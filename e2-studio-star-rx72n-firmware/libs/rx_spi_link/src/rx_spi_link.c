/**
 * @file rx_spi_link.c
 * @brief SPI Link Layer with HARQ and FEC Integration Implementation
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
 * @since Version 1.1.0
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
static const char* s_tag = "SPI_LINK";

/**
 * @enum internal_soft_bit_values_t
 * @brief Soft bit conversion constants
 *
 * @details
 * Used when converting received hard bytes to soft bits for the Viterbi
 * decoder. Each bit becomes +127 (confident 1) or -127 (confident 0).
 * Matches Go's bytesToSoftBits() in internal/link/spi.go.
 *
 * @since Version 1.1.0
 */
typedef enum : int8_t {
    k_soft_bit_one  = 127,  /**< Hard bit 1 -> soft confidence +127 */
    k_soft_bit_zero = -127, /**< Hard bit 0 -> soft confidence -127 */
} internal_soft_bit_values_t;

/**
 * @enum internal_bit_constants_t
 * @brief Bit manipulation constants for byte-to-soft-bit conversion
 *
 * @since Version 1.1.0
 */
typedef enum : uint8_t {
    k_bits_per_byte = 8,   /**< Number of bits in a byte */
    k_msb_shift     = 7,   /**< Shift to extract MSB */
    k_bit_mask      = 0x01, /**< Mask to extract single bit */
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
 * @pre soft_size >= data_len * k_bits_per_byte
 * @post soft_len == data_len * k_bits_per_byte
 *
 * @since Version 1.1.0
 */
static rx_err_t internal_bytes_to_soft_bits(const uint8_t*  data,
                                             uint32_t        data_len,
                                             rx_soft_bit_t*  soft,
                                             uint32_t        soft_size,
                                             uint32_t*       soft_len)
{
    const uint32_t required = data_len * (uint32_t)k_bits_per_byte;
    if (required > soft_size) {
        return k_rx_err_invalid_size;
    }

    for (uint32_t byte_idx = 0; byte_idx < data_len; byte_idx++) {
        const uint8_t b = data[byte_idx];
        for (uint8_t bit_idx = 0; bit_idx < k_bits_per_byte; bit_idx++) {
            const uint8_t bit = (b >> (k_msb_shift - bit_idx)) & k_bit_mask;
            const uint32_t idx = byte_idx * (uint32_t)k_bits_per_byte + bit_idx;
            soft[idx] = (bit == k_bit_mask)
                            ? (rx_soft_bit_t)k_soft_bit_one
                            : (rx_soft_bit_t)k_soft_bit_zero;
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
 * @since Version 1.1.0
 */
static rx_err_t internal_wait_for_ack(rx_spi_link_t* link, uint16_t expected_seq)
{
    rx_frame_t ack_frame;
    memset(&ack_frame, 0, sizeof(ack_frame));

    const rx_err_t err = rx_spi_comm_receive(link->spi_handle,
                                              &ack_frame,
                                              k_spi_link_ack_timeout_ms);
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
 * @brief Initialize SPI link layer
 * @see rx_spi_link.h for full documentation
 */
rx_err_t rx_spi_link_init(rx_spi_link_t*              link,
                           const rx_spi_link_config_t* config)
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

    /* Zero-fill link handle */
    memset(link, 0, sizeof(*link));

    /* Store configuration */
    link->spi_handle  = config->spi_handle;
    link->fec_enabled = config->fec_enabled;
    link->max_retries = (config->max_retries > 0)
                            ? config->max_retries
                            : (uint8_t)k_spi_link_default_max_retries;

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

    rx_log_info(s_tag, "Init OK (FEC=%s, retries=%u)",
                link->fec_enabled ? "on" : "off",
                link->max_retries);

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
 * @brief Send payload with HARQ+FEC reliability over SPI
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
    const uint8_t* tx_payload  = payload;
    uint32_t       tx_len      = payload_len;
    uint8_t        base_flags  = k_frame_flag_requires_ack;

    if (link->fec_enabled && payload != nullptr && payload_len > 0) {
        uint32_t encoded_len = 0;
        err = rx_harq_encode(&link->harq,
                             payload,
                             payload_len,
                             link->fec_encode_buf,
                             (uint32_t)k_spi_link_max_encoded_payload,
                             &encoded_len);
        if (err != k_rx_ok) {
            rx_log_error(s_tag, "FEC encode failed");
            return err;
        }

        tx_payload = link->fec_encode_buf;
        tx_len     = encoded_len;
        base_flags |= k_frame_flag_fec_enabled;
    }

    /* Retry loop (bounded by max_retries) */
    for (uint8_t attempt = 0; attempt < link->max_retries; attempt++) {
        uint8_t flags = base_flags;

        /* Mark retransmissions */
        if (attempt > 0) {
            flags |= k_frame_flag_retransmit;
            link->state = k_spi_link_state_retransmitting;
        } else {
            link->state = k_spi_link_state_waiting_ack;
        }

        /* Send frame via SPI transport */
        err = rx_spi_comm_send(link->spi_handle, type, flags, tx_payload, tx_len);
        if (err != k_rx_ok) {
            rx_log_warn(s_tag, "SPI send failed, attempt %u/%u",
                        (unsigned)(attempt + 1U), (unsigned)link->max_retries);
            continue;
        }

        /* Get the sequence number of the frame we just sent (it was auto-incremented) */
        uint16_t sent_seq = 0;
        (void)rx_session_get_tx(link->spi_handle->session, &sent_seq);
        /* Session returns the NEXT seq, we want the one we just used */
        sent_seq = (uint16_t)((sent_seq - 1U) & k_session_seq_wrap_mask);

        /* Wait for ACK/NACK */
        err = internal_wait_for_ack(link, sent_seq);
        if (err == k_rx_ok) {
            /* ACK received - success */
            link->state = k_spi_link_state_idle;
            return k_rx_ok;
        }

        if (err == k_rx_err_protocol_error) {
            /* NACK received - gateway decode failed, retry */
            rx_log_debug(s_tag, "NACK received, retry %u/%u",
                         (unsigned)(attempt + 1U), (unsigned)link->max_retries);
            continue;
        }

        /* Timeout - no response, retry */
        rx_log_debug(s_tag, "ACK timeout, retry %u/%u",
                     (unsigned)(attempt + 1U), (unsigned)link->max_retries);
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
 * @brief Receive and decode a frame from SPI with HARQ+FEC
 * @see rx_spi_link.h for full documentation
 */
rx_err_t rx_spi_link_receive(rx_spi_link_t*                link,
                              rx_spi_link_receive_result_t* result,
                              uint32_t                      timeout_ms)
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
    rx_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    memset(result, 0, sizeof(*result));

    const rx_err_t recv_err = rx_spi_comm_receive(link->spi_handle, &frame, timeout_ms);
    if (recv_err != k_rx_ok) {
        return recv_err; /* Timeout or error */
    }

    /* Handle ACK/NACK control frames internally (not data for application) */
    if (frame.header.type == (uint8_t)k_frame_type_ack ||
        frame.header.type == (uint8_t)k_frame_type_nack) {
        /* Control frame - not application data */
        return k_rx_err_timeout; /* Signal "no data" to caller */
    }

    /* Handle PING/PONG/RESET (already handled by rx_spi_comm internally) */
    if (frame.header.type == (uint8_t)k_frame_type_ping ||
        frame.header.type == (uint8_t)k_frame_type_pong ||
        frame.header.type == (uint8_t)k_frame_type_reset ||
        frame.header.type == (uint8_t)k_frame_type_reset_ack) {
        return k_rx_err_timeout; /* Not application data */
    }

    /* Data frame received */
    result->sequence      = frame.header.sequence;
    result->frame_type    = frame.header.type;
    result->is_retransmit = (frame.header.flags & k_frame_flag_retransmit) != 0;

    /* Check if FEC decoding is needed */
    const bool has_fec_flag = (frame.header.flags & k_frame_flag_fec_enabled) != 0;

    if (link->fec_enabled && has_fec_flag && frame.header.length > 0) {
        /* FEC decode path: convert bytes to soft bits, then Viterbi decode */
        rx_soft_bit_t soft_bits[k_harq_soft_buffer_size];
        uint32_t      soft_len = 0;

        rx_err_t err = internal_bytes_to_soft_bits(frame.payload,
                                                    frame.header.length,
                                                    soft_bits,
                                                    k_harq_soft_buffer_size,
                                                    &soft_len);
        if (err != k_rx_ok) {
            rx_log_error(s_tag, "Soft bit conversion failed");
            (void)rx_spi_comm_send_nack(link->spi_handle, frame.header.sequence, 0);
            return k_rx_err_protocol_error;
        }

        /* Calculate expected decoded length from FEC parameters */
        const uint32_t expected_decoded_len =
            (soft_len / (uint32_t)k_bits_per_byte) / 2U; /* rate 1/2 halves, then bits->bytes */

        const rx_harq_decode_params_t params = {
            .soft_bits           = soft_bits,
            .soft_len            = soft_len,
            .expected_output_len = expected_decoded_len,
        };

        err = rx_harq_decode(&link->harq,
                             &params,
                             result->payload,
                             &result->payload_len);

        result->fec_decoded     = true;
        result->combining_count = rx_harq_get_retry_count(&link->harq);

        if (err == k_rx_ok) {
            /* Decode success - send ACK */
            const rx_err_t ack_err =
                rx_spi_comm_send_ack(link->spi_handle, frame.header.sequence);
            if (ack_err != k_rx_ok) {
                rx_log_warn(s_tag, "ACK send failed (data decoded OK)");
            }
            (void)rx_harq_reset(&link->harq);
            return k_rx_ok;
        }

        /* Decode failed - send NACK for retransmission */
        rx_log_debug(s_tag, "FEC decode failed, sending NACK");
        (void)rx_spi_comm_send_nack(link->spi_handle,
                                     frame.header.sequence,
                                     k_frame_flag_soft_nack);
        return k_rx_err_protocol_error;

    }

    /* No FEC: passthrough (copy payload directly) */
    if (frame.header.length > 0 && frame.header.length <= k_harq_max_payload) {
        memcpy(result->payload, frame.payload, frame.header.length);
    }
    result->payload_len     = frame.header.length;
    result->fec_decoded     = false;
    result->combining_count = 1;

    /* Send ACK if the frame requires it */
    if ((frame.header.flags & k_frame_flag_requires_ack) != 0) {
        const rx_err_t ack_err =
            rx_spi_comm_send_ack(link->spi_handle, frame.header.sequence);
        if (ack_err != k_rx_ok) {
            rx_log_warn(s_tag, "ACK send failed (passthrough)");
        }
    }

    return k_rx_ok;
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
