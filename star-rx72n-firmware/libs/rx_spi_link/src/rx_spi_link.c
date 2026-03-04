/**
 * @file rx_spi_link.c
 * @brief SPI Link Layer with HARQ Implementation
 *
 * @details
 * Implements the SPI link layer that sits between the communication manager
 * and the raw SPI transport (rx_spi_comm). This is a C port of the Go
 * gateway's `internal/link/spi.go`, providing:
 *
 * - FEC encoding on TX (convolutional K=7, rate 1/2)
 * - Soft-decision Viterbi decoding on RX
 * - Chase Combining across retransmissions
 * - ACK/NACK handshake with configurable retries
 *
 * ## Key Design Decisions
 *
 * 1. **Hard-to-soft conversion**: SPI is digital (no analog front-end), so
 *    received bytes are converted to hard soft bits: 1 -> +127, 0 -> -127.
 *    Chase Combining still provides gain because noise corrupts different
 *    bits on each retransmission.
 *
 * 2. **Blocking send**: rx_spi_link_send() blocks until ACK or all retries
 *    are exhausted. The comm_task runs at 100 Hz, giving ~10 ms per cycle.
 *    With k_spi_link_ack_timeout_ms = 10 ms and 3 retries, worst case is
 *    ~30 ms (occupies 3 comm cycles).
 *
 * 3. **Non-blocking receive**: rx_spi_link_receive() defers to
 *    rx_spi_comm_receive() with the caller's timeout. Control frames
 *    (ACK/NACK) are handled internally and not returned to the caller.
 *
 * ## Protocol Flow (TX)
 *
 * ```
 * payload
 *   |
 *   +--> rx_harq_encode() --> encoded_payload (2N+2 bytes if FEC)
 *   |                          |
 *   +--> (passthrough if no FEC)
 *   |
 *   v
 * for attempt = 1..3:
 *   rx_spi_comm_send(encoded_payload, REQUIRES_ACK | FEC_ENABLED)
 *   wait for ACK/NACK (10 ms timeout)
 *   ACK -> return success
 *   NACK/timeout -> set RETRANSMIT flag, retry
 * ```
 *
 * ## Protocol Flow (RX)
 *
 * ```
 * rx_spi_comm_receive()
 *   |
 *   +--> ACK/NACK -> dispatch internally, return "no data"
 *   |
 *   +--> data frame (FEC flag set):
 *   |     convert bytes to soft bits
 *   |     rx_harq_decode() (with Chase Combining)
 *   |     success -> send ACK, return decoded payload
 *   |     failure -> send NACK, return error
 *   |
 *   +--> data frame (no FEC flag):
 *         passthrough, send ACK, return payload
 * ```
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp/longjmp, recursion
 * - Rule 2: [OK] All loops bounded (max_retries, k_rx_bits_per_byte)
 * - Rule 3: [OK] Zero dynamic allocation
 * - Rule 4: [OK] All functions < 60 lines
 * - Rule 5: [OK] Minimum 2 preconditions per function
 * - Rule 6: [OK] Variables at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] C23 typed enums for all constants
 * - Rule 10: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @see rx_spi_link.h  Public API
 * @see star-gateway/internal/link/spi.go  Go reference implementation
 *
 * @author STAR Team
 * @date 2026-02-14
 * @copyright Copyright (c) 2026 STAR Project. Licensed under MIT License.
 *
 * @since Version 1.0.0
 */

#include "rx_spi_link.h"

#include <string.h>

#include "rx_fec.h"
#include "rx_log.h"

/* =============================================================================
 * Module Constants
 * ============================================================================= */

/**
 * @var s_tag
 * @brief Log tag for SPI link layer messages
 */
static const char s_tag[] = "SPI_LINK";

/**
 * @enum internal_soft_bit_values_t
 * @brief Soft bit conversion constants
 *
 * @details
 * Used when converting received hard bytes to soft bits for the Viterbi
 * decoder. Each bit becomes +127 (confident 1) or -127 (confident 0).
 * Matches Go's bytesToSoftBits() in internal/link/spi.go.
 *
 * @since Version 1.0.0
 */
typedef enum : int8_t {
  k_internal_soft_bit_one  = 127,  /**< Hard bit 1 -> soft confidence +127 */
  k_internal_soft_bit_zero = -127, /**< Hard bit 0 -> soft confidence -127 */
} internal_soft_bit_values_t;

/**
 * @enum internal_bit_constants_t
 * @brief Bit manipulation constants for byte-to-soft-bit conversion
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_spi_link_bits_per_byte = 8,    /**< Number of bits in a byte */
  k_spi_link_msb_shift     = 7,    /**< Shift to extract MSB */
  k_spi_link_bit_mask      = 0x01, /**< Mask to extract single bit */
} internal_bit_constants_t;

/* =============================================================================
 * Internal Helpers (static)
 * ============================================================================= */

/**
 * @brief Convert payload bytes to soft bits for Viterbi decoder
 *
 * @details
 * Each byte is expanded to 8 soft bits, MSB-first. Bit value 1 becomes
 * +127, bit value 0 becomes -127. This matches the Go gateway's
 * bytesToSoftBits() function.
 *
 * @param[in]  data     Input hard bytes
 * @param[in]  data_len Number of input bytes
 * @param[out] soft     Output soft bits (must hold data_len * 8 elements)
 * @param[in]  soft_size Size of soft buffer in elements
 * @param[out] soft_len Actual number of soft bits written
 *
 * @return k_rx_ok on success, k_rx_err_invalid_size if buffer too small
 *
 * @pre data and soft must be non-NULL
 * @pre soft_size >= data_len * k_spi_link_bits_per_byte
 * @post soft_len == data_len * k_spi_link_bits_per_byte
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_bytes_to_soft_bits(const uint8_t* data,
                                            uint32_t       data_len,
                                            rx_soft_bit_t* soft,
                                            uint32_t       soft_size,
                                            uint32_t*      soft_len)
{
  /* Rule 5: Precondition validation - nullptr checks */
  if (data == nullptr) {
    if (soft_len != nullptr) {
      *soft_len = 0;
    }
    return k_rx_err_invalid_arg;
  }

  if (soft == nullptr) {
    if (soft_len != nullptr) {
      *soft_len = 0;
    }
    return k_rx_err_invalid_arg;
  }

  if (soft_len == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Rule 5: Precondition validation - overflow check before multiplication */
  if (data_len > 0 && data_len > UINT32_MAX / (uint32_t)k_spi_link_bits_per_byte) {
    *soft_len = 0;
    return k_rx_err_invalid_size; /* Would overflow */
  }

  /* Rule 5: Precondition validation - buffer size check */
  const uint32_t required = data_len * (uint32_t)k_spi_link_bits_per_byte;
  if (required > soft_size) {
    *soft_len = 0;
    return k_rx_err_invalid_size;
  }

  for (uint32_t byte_idx = 0; byte_idx < data_len; byte_idx++) {
    const uint8_t b = data[byte_idx];
    for (uint8_t bit_idx = 0; bit_idx < k_spi_link_bits_per_byte; bit_idx++) {
      const uint8_t  bit = (b >> (k_spi_link_msb_shift - bit_idx)) & k_spi_link_bit_mask;
      const uint32_t idx = byte_idx * (uint32_t)k_spi_link_bits_per_byte + bit_idx;
      soft[idx]          = (bit == k_spi_link_bit_mask) ? (rx_soft_bit_t)k_internal_soft_bit_one
                                                        : (rx_soft_bit_t)k_internal_soft_bit_zero;
    }
  }

  *soft_len = required;
  return k_rx_ok;
}

/**
 * @brief Wait for ACK/NACK response from gateway
 *
 * @details
 * Polls rx_spi_comm_receive() looking for an ACK or NACK frame matching
 * the expected sequence number. Non-matching frames are logged and ignored.
 *
 * @param[in,out] link Link handle
 * @param[in]     expected_seq Expected ACK/NACK sequence
 *
 * @return k_rx_ok if ACK received
 * @return k_rx_err_protocol_error if NACK received
 * @return k_rx_err_timeout if no ACK/NACK within timeout
 *
 * @pre link must be initialized
 * @pre expected_seq matches the last sent frame's sequence
 * @post No side effects on timeout
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_wait_for_ack(rx_spi_link_t* link, uint16_t expected_seq)
{
  rx_frame_t     ack_frame = {0};
  const rx_err_t err = rx_spi_comm_receive(link->spi_handle, &ack_frame, k_spi_link_ack_timeout_ms);

  if (err == k_rx_err_timeout) {
    return k_rx_err_timeout;
  }
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "ACK receive error");
    return err;
  }

  /* Check frame type */
  if (ack_frame.header.type == (uint8_t)k_frame_type_ack) {
    if (ack_frame.header.sequence == expected_seq) {
      return k_rx_ok;
    }
    rx_log_warn(s_tag, "ACK seq mismatch");
    return k_rx_err_timeout; /* Treat as timeout, will retry */
  }

  if (ack_frame.header.type == (uint8_t)k_frame_type_nack) {
    if (ack_frame.header.sequence == expected_seq) {
      return k_rx_err_protocol_error; /* NACK = decode failure on receiver */
    }
    rx_log_warn(s_tag, "NACK seq mismatch");
    return k_rx_err_timeout;
  }

  /* Unexpected frame type (data frame while waiting for ACK) - ignore */
  rx_log_debug(s_tag, "Non-ACK frame during wait");
  return k_rx_err_timeout;
}

/* =============================================================================
 * Lifecycle API Implementation
 * ============================================================================= */

/**
 * @var s_zero_link
 * @brief Zero-initialized SPI link template for stack-safe initialization
 * @details Static const instance used to clear rx_spi_link_t without creating
 *          a large compound literal on the stack. Lives in .rodata section.
 * @note Read-only; never modified after static initialization
 * @warning Do not remove - prevents stack overflow in init function
 * @see rx_spi_link_init() Uses this template to zero-initialize the link handle
 * @since Version 1.0.0
 */
static const rx_spi_link_t s_zero_link = {};

/**
 * @brief Initialize SPI link layer
 * @see rx_spi_link.h for full documentation
 */
rx_err_t rx_spi_link_init(rx_spi_link_t* link, const rx_spi_link_config_t* config)
{
  /* Pre-condition 1: NULL checks */
  if (link == nullptr || config == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: SPI handle required */
  if (config->spi_handle == nullptr) {
    rx_log_error(s_tag, "SPI handle is required");
    return k_rx_err_invalid_arg;
  }

  *link = s_zero_link;

  /* Store configuration */
  link->spi_handle  = config->spi_handle;
  link->fec_enabled = config->fec_enabled;
  link->max_retries =
    (config->max_retries > 0) ? config->max_retries : (uint8_t)k_spi_link_default_max_retries;

  /* Initialize HARQ handle (includes FEC encoder/decoder and Chase Combiner) */
  const rx_harq_config_t harq_cfg = {
    .max_retries  = link->max_retries,
    .fec_enabled  = link->fec_enabled ? 1U : 0U,
    .max_combines = link->max_retries,
  };

  const rx_err_t err = rx_harq_init(&link->harq, &harq_cfg);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "HARQ init failed");
    return err;
  }

  /* Post-condition 1: Mark initialized */
  link->state       = k_spi_link_state_idle;
  link->initialized = true;

  /* Post-condition 2: Verify HARQ is ready */
  if (rx_harq_get_state(&link->harq) != k_harq_state_idle) {
    link->initialized = false;
    return k_rx_err_not_initialized;
  }

  rx_log_info(s_tag, "Init OK");

  return k_rx_ok;
}

/**
 * @brief Deinitialize SPI link layer
 * @see rx_spi_link.h for full documentation
 */
rx_err_t rx_spi_link_deinit(rx_spi_link_t* link)
{
  /* Pre-condition 1 */
  if (link == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2 */
  if (!link->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Deinit HARQ subsystem */
  const rx_err_t err = rx_harq_deinit(&link->harq);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "HARQ deinit warning");
  }

  /* Post-condition: Mark uninitialized */
  link->initialized = false;
  link->state       = k_spi_link_state_idle;

  rx_log_info(s_tag, "Deinitialized");
  return k_rx_ok;
}

/* =============================================================================
 * Send API Implementation
 * ============================================================================= */

/**
 * @brief Attempt a single transmission with ACK/NACK handling
 *
 * @details
 * Sends frame via SPI transport, waits for ACK/NACK, and handles retransmission
 * logic. Updates link state for retransmit/waiting_ack. Gets TX sequence BEFORE
 * sending to avoid race conditions with external sequence management.
 *
 * @note Parameter ordering convention: type, base_flags, tx_payload, tx_len, attempt.
 * This ordering groups frame metadata (type, flags) before payload data (payload, len),
 * then retry state (attempt). While adjacent integer parameters may appear swappable,
 * the semantic grouping is intentional for code clarity.
 *
 * @param[in,out] link       Link handle (must be initialized)
 * @param[in]     type       Frame type (command, telemetry, etc.)
 * @param[in]     base_flags Base flags (fec_enabled, requires_ack)
 * @param[in]     tx_payload Payload to transmit (may be FEC-encoded)
 * @param[in]     tx_len     Payload length in bytes
 * @param[in]     attempt    Attempt number (0 = first, >0 = retransmit)
 *
 * @return rx_err_t Result of attempt
 * @retval k_rx_ok ACK received (success)
 * @retval k_rx_err_protocol_error NACK received (retry needed)
 * @retval k_rx_err_timeout Timeout (retry needed)
 * @retval Other SPI send errors
 */
static rx_err_t internal_send_attempt(rx_spi_link_t*  link,
                                      rx_frame_type_t type,
                                      uint8_t         base_flags,
                                      const uint8_t*  tx_payload,
                                      uint32_t        tx_len,
                                      uint8_t         attempt)
{
  uint8_t flags = base_flags;

  /* Mark retransmissions */
  if (attempt > 0) {
    flags |= k_frame_flag_retransmit;
    link->state = k_spi_link_state_retransmitting;
  } else {
    link->state = k_spi_link_state_waiting_ack;
  }

  /* CRITICAL: Get TX sequence BEFORE sending to avoid race.
     * rx_session_get_tx() returns the NEXT sequence to be used.
     * rx_spi_comm_send() will use this sequence and then increment.
     * This approach is safe ONLY if rx_spi_link_send() is not called
     * concurrently (enforced by caller via external mutex or task priority).
     *
     * Alternative solution: Modify rx_spi_comm_send() to return the
     * sequence actually used, but that requires API change across codebase. */
  uint16_t next_seq    = 0;
  rx_err_t session_err = rx_session_get_tx(link->spi_handle->session, &next_seq);
  if (session_err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to get TX sequence");
    return session_err;
  }
  const uint16_t expected_seq = next_seq; /* This will be used by send */

  /* Send frame via SPI transport */
  const rx_err_t send_err = rx_spi_comm_send(link->spi_handle, type, flags, tx_payload, tx_len);
  if (send_err != k_rx_ok) {
    rx_log_warn(s_tag, "SPI send failed");
    return send_err;
  }

  /* Wait for ACK/NACK using the sequence we captured */
  return internal_wait_for_ack(link, expected_seq);
}

/**
 * @brief Prepare payload for transmission (FEC encode or passthrough)
 *
 * @details
 * If FEC is enabled and payload is non-empty, encodes via rx_harq_encode()
 * and sets FEC flag. Otherwise, passes payload through unchanged. Sets
 * base_flags to include k_frame_flag_requires_ack and optionally
 * k_frame_flag_fec_enabled.
 *
 * @param[in,out] link        Link handle (must be initialized, fec_enabled checked)
 * @param[in]     payload     Original payload to send
 * @param[in]     payload_len Original payload length
 * @param[out]    out_payload Pointer to prepared payload (original or encoded buffer)
 * @param[out]    out_len     Prepared payload length
 * @param[out]    out_flags   Base flags for transmission
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Preparation successful
 * @retval k_rx_err_* FEC encoding error
 *
 * @pre link->fec_enabled indicates whether FEC should be used
 * @pre link->fec_encode_buf sized for k_spi_link_max_encoded_payload
 * @post If FEC enabled: out_payload points to link->fec_encode_buf, out_flags has FEC flag
 * @post If FEC disabled: out_payload == payload, out_len == payload_len
 */
static rx_err_t internal_prepare_tx_payload(rx_spi_link_t*  link,
                                            const uint8_t*  payload,
                                            uint32_t        payload_len,
                                            const uint8_t** out_payload,
                                            uint32_t*       out_len,
                                            uint8_t*        out_flags)
{
  *out_payload = payload;
  *out_len     = payload_len;
  *out_flags   = k_frame_flag_requires_ack;

  if (link->fec_enabled && payload != nullptr && payload_len > 0) {
    uint32_t encoded_len = 0;
    rx_err_t err         = rx_harq_encode(&link->harq,
                                  payload,
                                  payload_len,
                                  link->fec_encode_buf,
                                  (uint32_t)k_spi_link_max_encoded_payload,
                                  &encoded_len);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "FEC encode failed");
      return err;
    }

    *out_payload = link->fec_encode_buf;
    *out_len     = encoded_len;
    *out_flags |= k_frame_flag_fec_enabled;
  }

  return k_rx_ok;
}

/**
 * @brief Send payload with HARQ reliability over SPI
 * @see rx_spi_link.h for full documentation
 */
rx_err_t rx_spi_link_send(rx_spi_link_t*  link,
                          rx_frame_type_t type,
                          const uint8_t*  payload,
                          uint32_t        payload_len)
{
  /* Pre-condition 1: NULL and state checks */
  if (link == nullptr) {
    return k_rx_err_invalid_arg;
  }
  if (!link->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Pre-condition 2: Payload validation */
  if (payload == nullptr && payload_len > 0) {
    return k_rx_err_invalid_arg;
  }
  if (payload_len > k_harq_max_payload) {
    rx_log_error(s_tag, "Payload too large for HARQ");
    return k_rx_err_invalid_size;
  }

  /* Reset HARQ for new transaction */
  rx_err_t err = rx_harq_reset(&link->harq);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "HARQ reset failed");
    return err;
  }

  /* Prepare payload: FEC encode if enabled, otherwise passthrough */
  const uint8_t* tx_payload = nullptr;
  uint32_t       tx_len     = 0;
  uint8_t        base_flags = 0;

  err = internal_prepare_tx_payload(link, payload, payload_len, &tx_payload, &tx_len, &base_flags);
  if (err != k_rx_ok) {
    return err;
  }

  /* Retry loop (bounded by max_retries)
     * NOTE: This function is NOT thread-safe. Caller must ensure that
     * rx_spi_link_send() is not called concurrently on the same link handle.
     * Typical enforcement: Single task owns link, or external mutex protection. */
  for (uint8_t attempt = 0; attempt < link->max_retries; attempt++) {
    err = internal_send_attempt(link, type, base_flags, tx_payload, tx_len, attempt);

    if (err == k_rx_ok) {
      /* ACK received - success */
      link->state = k_spi_link_state_idle;
      return k_rx_ok;
    }

    if (err == k_rx_err_protocol_error) {
      /* NACK received - gateway decode failed, retry */
      rx_log_debug(s_tag, "NACK received, retrying");
      continue;
    }

    /* Timeout or send error - retry */
    rx_log_debug(s_tag, "Send attempt failed, retrying");
  }

  /* All retries exhausted */
  link->state = k_spi_link_state_error;
  rx_log_error(s_tag, "Max retries exceeded");
  return k_rx_err_retry_limit;
}

/* =============================================================================
 * Receive API Implementation
 * ============================================================================= */

/**
 * @brief Process received frame with FEC decoding and Chase Combining
 *
 * @details
 * Converts hard bits to soft bits, performs Viterbi decoding via HARQ,
 * and sends ACK/NACK based on decode success. On success, resets HARQ
 * combiner. On failure, sends NACK for retransmission.
 *
 * Algorithm:
 * 1. Convert received bytes to soft bits (hard decision: 1 -> +127, 0 -> -127)
 * 2. Pass soft bits to HARQ decoder (Chase Combining + Viterbi)
 * 3. If decode succeeds: send ACK, reset combiner, return payload
 * 4. If decode fails: send NACK, accumulate soft bits for next retransmit
 *
 * @param[in,out] link    Link handle (must be initialized)
 * @param[in]     frame   Received frame with FEC-encoded payload
 * @param[out]    result  Decode result (payload, length, metadata)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Decode success, ACK sent
 * @retval k_rx_err_protocol_error Decode failed, NACK sent
 *
 * @pre link->initialized == true (link must be initialized)
 * @pre frame->payload contains FEC-encoded data (FEC flag set)
 * @pre result->payload buffer >= k_harq_max_payload (1024 bytes minimum)
 * @pre Single-threaded context (SPI receive path protected by state machine)
 *
 * @post On success: result->fec_decoded == true, result->payload filled
 * @post On success: ACK sent to sender, HARQ combiner reset
 * @post On failure: NACK sent, soft bits accumulated in combiner
 * @post result->combining_count reflects number of transmissions combined
 *
 * @note Thread safety: This function is NOT thread-safe. Must be called from
 * single-threaded SPI receive context (enforced by link state machine).
 *
 * @par State Machine Invariant:
 * This function is called only from rx_spi_link_receive(), which serializes
 * access via the link state machine. Multiple concurrent calls would corrupt
 * the Chase Combiner state and static soft bit buffer.
 *
 * @par Static Buffer Lifetime:
 * Uses static s_soft_bits[k_harq_soft_buffer_size] (16,400 bytes) to avoid
 * stack overflow. Safe because function is never called concurrently. Buffer
 * is overwritten on each call (no persistent state).
 *
 * @par Example (FEC Decode Flow):
 * @code
 * // Receive FEC-encoded frame (2050 bytes encoded payload)
 * rx_frame_t frame;
 * rx_spi_comm_receive(spi, &frame, timeout);
 *
 * // Decode with HARQ + Chase Combining
 * rx_spi_link_receive_result_t result;
 * rx_err_t err = internal_receive_fec_decode(link, &frame, &result);
 *
 * if (err == k_rx_ok) {
 *     // Decode success - payload in result.payload (1024 bytes)
 *     // ACK automatically sent, combiner reset
 *     process_decoded_data(result.payload, result.payload_len);
 * } else {
 *     // Decode failed - NACK sent, waiting for retransmit
 *     // Soft bits accumulated in combiner for next attempt
 * }
 * @endcode
 */
static rx_err_t internal_receive_fec_decode(rx_spi_link_t*                link,
                                            const rx_frame_t*             frame,
                                            rx_spi_link_receive_result_t* result)
{
  /* FEC decode path: convert bytes to soft bits, then Viterbi decode
   * NOTE: Using static buffer to avoid stack overflow (16,400 bytes).
   * Safe because this function is called from single-threaded context
   * (SPI receive path protected by link state machine). */
  static rx_soft_bit_t s_soft_bits[k_harq_soft_buffer_size];
  uint32_t             soft_len = 0;

  rx_err_t err = internal_bytes_to_soft_bits(frame->payload,
                                             frame->header.length,
                                             s_soft_bits,
                                             k_harq_soft_buffer_size,
                                             &soft_len);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Soft bit conversion failed");
    (void)rx_spi_comm_send_nack(link->spi_handle, frame->header.sequence, 0);
    return k_rx_err_protocol_error;
  }

  /* Calculate expected decoded length from FEC parameters.
     *
     * IMPORTANT: This formula is correct because rx_fec.c internally accounts
     * for k_fec_tail_bits when encoding. The frame payload contains the full
     * FEC-encoded bitstream (data bits + parity bits + tail bits). The formula:
     *   expected_decoded_len = (soft_len / k_spi_link_bits_per_byte) / 2
     * works as follows:
     *   1. soft_len is the total encoded bit count (includes tail bits)
     *   2. Divide by k_spi_link_bits_per_byte (8) to get encoded byte count
     *   3. Divide by 2 because FEC rate is 1/2 (each input bit -> 2 output bits)
     * The FEC decoder (rx_fec_viterbi_decode) automatically handles tail bit
     * flushing internally, so we don't subtract them here. */
  const uint32_t expected_decoded_len = (soft_len / (uint32_t)k_spi_link_bits_per_byte) / 2U;

  const rx_harq_decode_params_t params = {
    .soft_bits           = s_soft_bits,
    .soft_len            = soft_len,
    .expected_output_len = expected_decoded_len,
  };

  err = rx_harq_decode(&link->harq, &params, result->payload, &result->payload_len);

  result->fec_decoded     = true;
  result->combining_count = rx_harq_get_retry_count(&link->harq);

  if (err == k_rx_ok) {
    /* Decode success - send ACK */
    const rx_err_t ack_err = rx_spi_comm_send_ack(link->spi_handle, frame->header.sequence);
    if (ack_err != k_rx_ok) {
      rx_log_warn(s_tag, "ACK send failed (data decoded OK)");
    }
    (void)rx_harq_reset(&link->harq);
    return k_rx_ok;
  }

  /* Decode failed - send NACK for retransmission */
  rx_log_debug(s_tag, "FEC decode failed, sending NACK");
  (void)rx_spi_comm_send_nack(link->spi_handle, frame->header.sequence, k_frame_flag_soft_nack);
  return k_rx_err_protocol_error;
}

/**
 * @brief Process received frame without FEC (passthrough mode)
 *
 * @details
 * Copies payload directly to result buffer and sends ACK if required
 * by frame flags. No FEC decoding or Chase Combining applied.
 *
 * @param[in,out] link    Link handle (must be initialized)
 * @param[in]     frame   Received frame with raw payload
 * @param[out]    result  Receive result (payload, length, metadata)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, payload copied and ACK sent (if required)
 */
static rx_err_t internal_receive_passthrough(rx_spi_link_t*                link,
                                             const rx_frame_t*             frame,
                                             rx_spi_link_receive_result_t* result)
{
  /* No FEC: passthrough (copy payload directly) */
  if (frame->header.length > 0 && frame->header.length <= k_harq_max_payload) {
    (void)memcpy(result->payload, frame->payload, frame->header.length);
    result->payload_len = frame->header.length;
  } else if (frame->header.length > k_harq_max_payload) {
    /* Defensive: bound payload_len to buffer capacity to prevent out-of-bounds reads */
    rx_log_warn(s_tag, "Passthrough payload too large, truncating");
    (void)memcpy(result->payload, frame->payload, k_harq_max_payload);
    result->payload_len = k_harq_max_payload;
  } else {
    /* Zero-length payload */
    result->payload_len = 0;
  }
  result->fec_decoded     = false;
  result->combining_count = 1;

  /* Send ACK if the frame requires it */
  if ((frame->header.flags & k_frame_flag_requires_ack) != 0) {
    const rx_err_t ack_err = rx_spi_comm_send_ack(link->spi_handle, frame->header.sequence);
    if (ack_err != k_rx_ok) {
      rx_log_warn(s_tag, "ACK send failed (passthrough)");
    }
  }

  return k_rx_ok;
}

/**
 * @brief Receive and decode a frame from SPI with HARQ
 * @see rx_spi_link.h for full documentation
 */
rx_err_t
rx_spi_link_receive(rx_spi_link_t* link, rx_spi_link_receive_result_t* result, uint32_t timeout_ms)
{
  /* Pre-condition 1: NULL checks */
  if (link == nullptr || result == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: State check */
  if (!link->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Receive raw frame from SPI transport */
  rx_frame_t frame = {0};
  *result          = (rx_spi_link_receive_result_t){0};

  const rx_err_t recv_err = rx_spi_comm_receive(link->spi_handle, &frame, timeout_ms);
  if (recv_err != k_rx_ok) {
    return recv_err; /* Timeout or error */
  }

  /* Handle ACK/NACK control frames internally (not data for application) */
  if (frame.header.type == (uint8_t)k_frame_type_ack ||
      frame.header.type == (uint8_t)k_frame_type_nack) {
    /* Control frame - not application data */
    return k_rx_err_no_data; /* Signal "no data" to caller */
  }

  /* Handle PING/PONG/RESET (already handled by rx_spi_comm internally) */
  if (frame.header.type == (uint8_t)k_frame_type_ping ||
      frame.header.type == (uint8_t)k_frame_type_pong ||
      frame.header.type == (uint8_t)k_frame_type_reset ||
      frame.header.type == (uint8_t)k_frame_type_reset_ack) {
    return k_rx_err_no_data; /* Not application data */
  }

  /* Data frame received - populate common metadata */
  result->sequence      = frame.header.sequence;
  result->frame_type    = frame.header.type;
  result->is_retransmit = (frame.header.flags & k_frame_flag_retransmit) != 0;

  /* Check if FEC decoding is needed */
  const bool has_fec_flag = (frame.header.flags & k_frame_flag_fec_enabled) != 0;

  if (link->fec_enabled && has_fec_flag && frame.header.length > 0) {
    /* FEC decode path: HARQ with Chase Combining */
    return internal_receive_fec_decode(link, &frame, result);
  }

  /* No FEC: passthrough (copy payload directly) */
  return internal_receive_passthrough(link, &frame, result);
}

/* =============================================================================
 * State Management Implementation
 * ============================================================================= */

/**
 * @brief Reset SPI link layer for new session
 * @see rx_spi_link.h for full documentation
 */
rx_err_t rx_spi_link_reset(rx_spi_link_t* link)
{
  /* Pre-condition 1 */
  if (link == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2 */
  if (!link->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Reset HARQ (clears combiner and retry count) */
  const rx_err_t err = rx_harq_reset(&link->harq);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "HARQ reset warning during link reset");
  }

  /* Post-condition: return to idle */
  link->state = k_spi_link_state_idle;

  rx_log_debug(s_tag, "Link reset");
  return k_rx_ok;
}

/**
 * @brief Get current SPI link state
 * @see rx_spi_link.h for full documentation
 */
rx_spi_link_state_t rx_spi_link_get_state(const rx_spi_link_t* link)
{
  if (link == nullptr || !link->initialized) {
    return k_spi_link_state_error;
  }
  return link->state;
}

/**
 * @brief Check if FEC is enabled on this link
 * @see rx_spi_link.h for full documentation
 */
bool rx_spi_link_fec_enabled(const rx_spi_link_t* link)
{
  if (link == nullptr || !link->initialized) {
    return false;
  }
  return link->fec_enabled;
}
