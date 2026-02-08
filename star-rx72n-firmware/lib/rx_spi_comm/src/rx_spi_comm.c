/* lib/rx_spi_comm/src/rx_spi_comm.c */

/**
 * @file rx_spi_comm.c
 * @brief High-Level SPI Communication Layer for RX72N
 * @details
 * Implements reliable SPI communication by integrating the frame layer
 * and SPI HAL. Handles frame encoding/decoding, CRC validation, and
 * sequence number management.
 *
 * @note RX72N operates as SPI peripheral, RPi5 as SPI controller.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_spi_comm.h"

#include <string.h>

#include "hardware.h"
#include "rx_crc.h"
#include "rx_threadx_config.h"
#include "rx_time_constants.h"

#ifdef __RX__
#include "tx_api.h" /* ThreadX for tx_thread_sleep */
#endif

/* =============================================================================
 * Module State
 * =============================================================================
 */

static const char* s_tag = "rx_spi_comm";

/* =============================================================================
 * Frame Header Byte Offsets
 *
 * Frame format: [SYNC(2B)][SEQ(2B)][LEN(2B)][TYPE(1B)][FLAGS(1B)]
 * These offsets are for parsing the raw header buffer.
 * =============================================================================
 */

/** @brief Byte offsets within frame header buffer (SYNC + header fields) */
typedef enum : uint8_t {
  k_hdr_sync_high = 0, /**< SYNC word high byte */
  k_hdr_sync_low  = 1, /**< SYNC word low byte */
  k_hdr_seq_high  = 2, /**< Sequence number high byte */
  k_hdr_seq_low   = 3, /**< Sequence number low byte */
  k_hdr_len_high  = 4, /**< Payload length high byte */
  k_hdr_len_low   = 5, /**< Payload length low byte */
  k_hdr_type      = 6, /**< Frame type */
  k_hdr_flags     = 7, /**< Frame flags */
} frame_header_offset_t;

/**
 * @brief Decode and validate a SPI frame header
 *
 * @param[in]  data       Raw frame data buffer
 * @param[in]  data_len   Length of data buffer in bytes
 * @param[out] frame      Frame to populate (header fields)
 * @param[out] offset_out Optional pointer to receive payload offset
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if data or frame is NULL
 * @return k_rx_err_invalid_size if data_len is too small or payload too large
 * @return k_rx_err_protocol_error if sync word is invalid
 */
static rx_err_t internal_decode_header(const uint8_t* data,
                                       const uint32_t data_len,
                                       rx_frame_t*    frame,
                                       uint32_t*      offset_out)
{
  uint32_t offset;
  uint16_t sync_word;
  uint32_t expected_size;

  if (data == NULL || frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (data_len < k_frame_min_size) {
    return k_rx_err_invalid_size;
  }

  offset = 0;

  sync_word = rx_frame_read_be16(&data[offset]);
  if (sync_word != k_frame_sync_word) {
    return k_rx_err_protocol_error;
  }
  offset += k_frame_sync_size;

  frame->header.sequence = rx_frame_read_be16(&data[offset]);
  offset += k_frame_seq_size;

  frame->header.length = rx_frame_read_be16(&data[offset]);
  offset += k_frame_len_size;

  if (frame->header.length > k_frame_max_payload) {
    return k_rx_err_invalid_size;
  }

  expected_size = rx_frame_encoded_size(frame->header.length);
  if (data_len < expected_size) {
    return k_rx_err_invalid_size;
  }

  frame->header.type = data[offset];
  offset += k_frame_type_size;

  frame->header.flags = data[offset];
  offset += k_frame_flags_size;

  if (offset_out != NULL) {
    *offset_out = offset;
  }

  return k_rx_ok;
}

/**
 * @brief Verify CRC for a received SPI frame
 *
 * @param[in]  data   Raw frame data buffer
 * @param[in]  offset Offset to CRC field in data
 * @param[out] crc_out Optional pointer to receive CRC (can be NULL)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if data is NULL
 * @return k_rx_err_crc_mismatch if CRC does not match
 *
 * @note Thread-safe if caller serializes access to the shared buffer.
 */
static rx_err_t internal_verify_crc(const uint8_t* data, uint32_t offset, uint32_t* crc_out)
{
  if (data == NULL) {
    return k_rx_err_invalid_arg;
  }

  const uint32_t received_crc   = rx_frame_read_le32(&data[offset]);
  const uint32_t calculated_crc = rx_crc32_ieee(data, offset);

  if (received_crc != calculated_crc) {
    return k_rx_err_crc_mismatch;
  }

  if (crc_out != NULL) {
    *crc_out = received_crc;
  }

  return k_rx_ok;
}

#ifdef __RX__
/** @brief Sleep duration for polling loop (1 tick) */
static const uint32_t s_poll_sleep_ticks = 1;
#endif

/** @brief ACK/ready wait timing constants */
typedef enum : uint16_t {
  k_ack_wait_timeout_ms = 50, /**< Abort if host doesn't ACK within 50 ms */
} ack_wait_t;

/** @brief Sequence number constants */
typedef enum : uint16_t {
  k_control_frame_sequence = 0, /**< Sequence number for control frames (PONG, RESET_ACK) */
} spi_comm_sequence_constants_t;

/** @brief Polling loop iteration limits */
typedef enum : uint16_t {
  k_max_poll_iterations = 1000, /**< Maximum polling iterations (safety bound) */
} polling_limits_t;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Wait for host ACK/ready signal before transmitting
 *
 * Prevents infinite wait states when host never signals readiness.
 * Uses precise time measurement on RX72N and iteration count for host tests.
 */
static rx_err_t internal_wait_for_ack(const rx_spi_comm_handle_t* handle, uint32_t timeout_ms)
{
#ifdef __RX__
  /* RX72N: Use ThreadX time measurement for precise timeout */
  ULONG timeout_ticks = (timeout_ms + k_threadx_ms_per_tick - 1) / k_threadx_ms_per_tick;
  ULONG start_ticks   = tx_time_get();

  while ((tx_time_get() - start_ticks) < timeout_ticks) {
    bool           ready = false;
    const rx_err_t err   = rspi_peripheral_write_ready(handle->channel, &ready);
    if (err != k_rx_ok) {
      return err;
    }

    if (ready) {
      return k_rx_ok;
    }

    /* Yield to other threads while waiting */
    tx_thread_sleep(s_poll_sleep_ticks);
  }
#else
  /* Host build (testing): Use iteration counter to simulate time */
  uint32_t elapsed_ms = 0;

  while (elapsed_ms < timeout_ms) {
    bool           ready = false;
    const rx_err_t err   = rspi_peripheral_write_ready(handle->channel, &ready);
    if (err != k_rx_ok) {
      return err;
    }

    if (ready) {
      return k_rx_ok;
    }

    /* Simulate time passing in tests */
    elapsed_ms += k_threadx_ms_per_tick;
  }
#endif

  rx_log_error(s_tag, "SPI ACK timeout waiting for host ready");
  return k_rx_err_timeout;
}

/**
 * @brief Perform raw SPI transfer using staging buffers
 *
 * For transmit operations (tx_data != NULL), waits for host ready signal
 * before transferring to prevent infinite wait states.
 */
static rx_err_t internal_spi_transfer(rx_spi_comm_handle_t* handle,
                                      const uint8_t*        tx_data,
                                      const uint32_t        tx_len,
                                      uint8_t*              rx_data,
                                      const uint32_t        rx_len)
{
  uint32_t transfer_len;
  rx_err_t wait_err;
  rx_err_t err;

  /* Pre-condition 1: Handle pointer validation */
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  /* Pre-condition 2: Transfer length within buffer capacity */
  transfer_len = (tx_len > rx_len) ? tx_len : rx_len;
  if (transfer_len > k_spi_comm_tx_buffer_size) {
    rx_log_error(s_tag, "Transfer length exceeds buffer capacity");
    return k_rx_err_invalid_size;
  }

  /* Pre-condition 3: TX data pointer consistent with length */
  if (tx_data == NULL && tx_len > 0) {
    rx_log_error(s_tag, "NULL TX data with non-zero length");
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 4: RX data pointer consistent with length */
  if (rx_data == NULL && rx_len > 0) {
    rx_log_error(s_tag, "NULL RX data with non-zero length");
    return k_rx_err_invalid_arg;
  }

  /* Wait for host ready signal before transmit operations */
  if (tx_data != NULL && tx_len > 0) {
    wait_err = internal_wait_for_ack(handle, k_ack_wait_timeout_ms);
    RX_RETURN_ON_ERROR(wait_err, s_tag, "Host ACK wait failed");
  }

  /* Prepare TX buffer (pad with zeros if RX is larger) */
  memset(handle->tx_buffer, 0, transfer_len);
  if (tx_data != NULL && tx_len > 0) {
    memcpy(handle->tx_buffer, tx_data, tx_len);
  }

  /*
   * Perform SPI transfer.
   * Cast to uint16_t: RSPI HAL uses 16-bit length (max 65535 bytes).
   * Safe because transfer_len is validated <= k_spi_comm_tx_buffer_size above.
   */
  err = rspi_peripheral_transfer(handle->channel,
                                 handle->tx_buffer,
                                 handle->rx_buffer,
                                 (uint16_t)transfer_len);

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "SPI peripheral transfer failed");
    return err;
  }

  /* Post-condition: Validate RX buffer populated if requested */
  if (rx_data != NULL && rx_len > 0) {
    memcpy(rx_data, handle->rx_buffer, rx_len);
  }

  return k_rx_ok;
}

/* =============================================================================
 * Initialization
 * =============================================================================
 */

rx_err_t rx_spi_comm_init(rx_spi_comm_handle_t* handle, const rx_spi_comm_config_t* config)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Clear handle */
  memset(handle, 0, sizeof(rx_spi_comm_handle_t));

  /* Apply configuration */
  if (config != NULL) {
    handle->channel     = config->channel;
    handle->fec_enabled = config->fec_enabled;
  } else {
    handle->channel     = k_spi_comm_default_channel;
    handle->fec_enabled = false;
  }

  /* Initialize frame encoder */
  rx_err_t err = rx_frame_encoder_init(&handle->encoder);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to init frame encoder");
    return err;
  }

  /* Initialize frame decoder */
  err = rx_frame_decoder_init(&handle->decoder);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to init frame decoder");
    rx_frame_encoder_deinit(&handle->encoder);
    return err;
  }

  /* Initialize sequence counters */
  handle->tx_sequence = 0;
  handle->rx_sequence = 0;

  handle->initialized = true;

  rx_log_debug(s_tag, "SPI comm initialized");
  return k_rx_ok;
}

rx_err_t rx_spi_comm_deinit(rx_spi_comm_handle_t* handle)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (handle->initialized) {
    rx_frame_encoder_deinit(&handle->encoder);
    rx_frame_decoder_deinit(&handle->decoder);
    handle->initialized = false;
  }

  rx_log_debug(s_tag, "SPI comm deinitialized");
  return k_rx_ok;
}

/* =============================================================================
 * Send API
 * =============================================================================
 */

static rx_err_t internal_build_frame(const rx_spi_comm_handle_t* handle,
                                     const rx_frame_type_t       type,
                                     const uint8_t               flags,
                                     const uint8_t*              payload,
                                     const uint32_t              payload_len,
                                     rx_frame_t*                 frame)
{
  if (handle == NULL || frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (payload == NULL && payload_len > 0) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > k_frame_max_payload) {
    rx_log_error(s_tag, "Payload too large");
    return k_rx_err_invalid_size;
  }

  memset(frame, 0, sizeof(*frame));
  frame->header.sequence = handle->tx_sequence;
  frame->header.length   = (uint16_t)payload_len;
  frame->header.type     = (uint8_t)type;
  frame->header.flags    = flags;

  if (payload != NULL && payload_len > 0) {
    memcpy(frame->payload, payload, payload_len);
  }

  if (handle->fec_enabled) {
    frame->header.flags |= k_frame_flag_fec_enabled;
  }

  return k_rx_ok;
}

rx_err_t rx_spi_comm_send(rx_spi_comm_handle_t* handle,
                          const rx_frame_type_t type,
                          const uint8_t         flags,
                          const uint8_t*        payload,
                          const uint32_t        payload_len)
{
  rx_frame_t frame;
  uint8_t    wire_buffer[k_frame_max_size];
  uint32_t   wire_len;
  uint16_t   expected_sequence;
  rx_err_t   err;

  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    rx_log_error(s_tag, "Handle not initialized");
    return k_rx_err_invalid_state;
  }

  /* Build frame */
  err = internal_build_frame(handle, type, flags, payload, payload_len, &frame);
  if (err != k_rx_ok) {
    return err;
  }

  /* Encode frame to wire format */
  wire_len = 0;
  err      = rx_frame_encode(&handle->encoder, &frame, wire_buffer, &wire_len);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame encode failed");
    return err;
  }

  /* Post-condition 1: Encoded length within valid bounds */
  if (wire_len == 0 || wire_len > k_frame_max_size) {
    rx_log_error(s_tag, "Invalid encoded frame length");
    return k_rx_err_invalid_size;
  }

  /* Transfer via SPI (waits for host ACK internally) */
  err = internal_spi_transfer(handle, wire_buffer, wire_len, NULL, 0);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "SPI transfer failed");
    return err;
  }

  /* Post-condition 2: TX sequence incremented correctly */
  expected_sequence = handle->tx_sequence + 1;
  handle->tx_sequence++;
  if (handle->tx_sequence != expected_sequence) {
    rx_log_error(s_tag, "TX sequence increment failed");
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

rx_err_t rx_spi_comm_send_ack(rx_spi_comm_handle_t* handle, const uint16_t sequence)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Create ACK frame */
  rx_frame_t ack_frame;
  rx_err_t   err = rx_frame_create_ack(&ack_frame, sequence);
  if (err != k_rx_ok) {
    return err;
  }

  /* Encode and send */
  uint8_t  wire_buffer[k_frame_min_size];
  uint32_t wire_len = 0;

  err = rx_frame_encode(&handle->encoder, &ack_frame, wire_buffer, &wire_len);
  if (err != k_rx_ok) {
    return err;
  }

  /* Post-condition: ACK frame encoded to minimum size */
  if (wire_len != k_frame_min_size) {
    rx_log_error(s_tag, "Invalid ACK frame length");
    return k_rx_err_invalid_size;
  }

  /* Transfer via SPI (waits for host ACK internally) */
  return internal_spi_transfer(handle, wire_buffer, wire_len, NULL, 0);
}

rx_err_t rx_spi_comm_send_nack(rx_spi_comm_handle_t* handle, const uint16_t sequence, uint8_t flags)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Create NACK frame */
  rx_frame_t nack_frame;
  rx_err_t   err = rx_frame_create_nack(&nack_frame, sequence, flags);
  if (err != k_rx_ok) {
    return err;
  }

  /* Encode and send */
  uint8_t  wire_buffer[k_frame_min_size];
  uint32_t wire_len = 0;

  err = rx_frame_encode(&handle->encoder, &nack_frame, wire_buffer, &wire_len);
  if (err != k_rx_ok) {
    return err;
  }

  /* Post-condition: NACK frame encoded to minimum size */
  if (wire_len != k_frame_min_size) {
    rx_log_error(s_tag, "Invalid NACK frame length");
    return k_rx_err_invalid_size;
  }

  /* Transfer via SPI (waits for host ACK internally) */
  return internal_spi_transfer(handle, wire_buffer, wire_len, NULL, 0);
}

rx_err_t rx_spi_comm_send_pong(rx_spi_comm_handle_t* handle,
                                const uint8_t*        payload,
                                const uint32_t        payload_len)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Create PONG frame echoing PING payload */
  rx_frame_t pong_frame;
  rx_err_t   err = rx_frame_create_pong(&pong_frame, k_control_frame_sequence, payload, payload_len);
  if (err != k_rx_ok) {
    return err;
  }

  /* Encode and send */
  uint8_t  wire_buffer[k_frame_max_size];
  uint32_t wire_len = 0;

  err = rx_frame_encode(&handle->encoder, &pong_frame, wire_buffer, &wire_len);
  if (err != k_rx_ok) {
    return err;
  }

  return internal_spi_transfer(handle, wire_buffer, wire_len, NULL, 0);
}

rx_err_t rx_spi_comm_send_reset_ack(rx_spi_comm_handle_t* handle)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Create RESET_ACK frame (sequence reset to initial after reset) */
  rx_frame_t reset_ack_frame;
  rx_err_t   err = rx_frame_create_reset_ack(&reset_ack_frame, k_control_frame_sequence);
  if (err != k_rx_ok) {
    return err;
  }

  /* Encode and send */
  uint8_t  wire_buffer[k_frame_min_size];
  uint32_t wire_len = 0;

  err = rx_frame_encode(&handle->encoder, &reset_ack_frame, wire_buffer, &wire_len);
  if (err != k_rx_ok) {
    return err;
  }

  return internal_spi_transfer(handle, wire_buffer, wire_len, NULL, 0);
}

rx_err_t rx_spi_comm_set_control_callbacks(rx_spi_comm_handle_t*    handle,
                                            rx_spi_comm_control_cb_t on_ping,
                                            rx_spi_comm_control_cb_t on_reset,
                                            void*                    ctx)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  handle->on_ping_cb  = on_ping;
  handle->on_reset_cb = on_reset;
  handle->cb_ctx      = ctx;

  return k_rx_ok;
}

/* =============================================================================
 * Receive API - Internal Helpers
 * =============================================================================
 */

/**
 * @brief Wait for data to be available on SPI channel
 *
 * @param handle SPI communication handle
 * @param timeout_ms Maximum time to wait in milliseconds (0 = no wait)
 * @return k_rx_ok if data available, k_rx_err_timeout if timeout expired
 */
static rx_err_t internal_wait_for_data(const rx_spi_comm_handle_t* handle,
                                       const uint32_t              timeout_ms)
{
  bool     available = false;
  rx_err_t err       = rspi_peripheral_read_available(handle->channel, &available);
  if (err != k_rx_ok) {
    return err;
  }

  if (!available) {
    if (timeout_ms == 0) {
      return k_rx_err_timeout;
    }

#ifdef __RX__
    /*
     * Wait for data using ThreadX sleep.
     * Yields CPU to other threads instead of busy-waiting.
     * Sleep duration and tick rate are defined by threadx_timing_t constants.
     */
    uint32_t elapsed_ms = 0;
    uint32_t iteration  = 0;
    while (!available && elapsed_ms < timeout_ms && iteration < k_max_poll_iterations) {
      iteration++;
      tx_thread_sleep(s_poll_sleep_ticks);
      elapsed_ms += k_threadx_ms_per_tick;

      err = rspi_peripheral_read_available(handle->channel, &available);
      if (err != k_rx_ok) {
        return err;
      }
    }
#else
    /* Host builds (testing): timeout not supported, return immediately */
    (void)timeout_ms;
#endif

    if (!available) {
      return k_rx_err_timeout;
    }
  }

  return k_rx_ok;
}

/**
 * @brief Read and validate frame header from SPI
 *
 * @param handle SPI communication handle
 * @param header_buf Output buffer for header (must be k_frame_sync_size + k_frame_header_size)
 * @param payload_len Output pointer for extracted payload length
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t
internal_read_frame_header(rx_spi_comm_handle_t* handle, uint8_t* header_buf, uint16_t* payload_len)
{
  const uint32_t header_len = k_frame_sync_size + k_frame_header_size;

  /* Read frame header from SPI */
  const rx_err_t err = internal_spi_transfer(handle, NULL, 0, header_buf, header_len);
  if (err != k_rx_ok) {
    return err;
  }

  /* Validate sync word (big-endian) */
  const uint16_t sync = rx_frame_read_be16(&header_buf[k_hdr_sync_high]);
  if (sync != k_frame_sync_word) {
    rx_log_error(s_tag, "Invalid sync word");
    return k_rx_err_protocol_error;
  }

  /* Extract payload length (big-endian) */
  *payload_len = rx_frame_read_be16(&header_buf[k_hdr_len_high]);
  if (*payload_len > k_frame_max_payload) {
    rx_log_error(s_tag, "Payload too large");
    return k_rx_err_invalid_size;
  }

  return k_rx_ok;
}

/**
 * @brief Decode and verify a received SPI frame
 *
 * @param[in]  handle     SPI communication handle
 * @param[out] frame      Frame to populate (header, payload, CRC)
 * @param[in]  total_size Total frame size in bytes (header + payload + CRC)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if inputs are NULL
 * @return k_rx_err_invalid_size or k_rx_err_protocol_error from internal_decode_header
 * @return k_rx_err_crc_mismatch from internal_verify_crc
 *
 * @note Delegates header validation to internal_decode_header and CRC validation
 *       to internal_verify_crc.
 */
static rx_err_t internal_decode_frame(const rx_spi_comm_handle_t* handle,
                                      rx_frame_t*                 frame,
                                      const uint32_t              total_size)
{
  uint32_t offset = 0;

  if (handle == NULL || frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  rx_err_t err = internal_decode_header(handle->rx_buffer, total_size, frame, &offset);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame header decode failed");
    return err;
  }

  if (frame->header.length > 0) {
    memcpy(frame->payload, &handle->rx_buffer[offset], frame->header.length);
    offset += frame->header.length;
  }

  err = internal_verify_crc(handle->rx_buffer, offset, &frame->crc);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame CRC check failed");
    return err;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Receive API - Control Frame Dispatch
 * =============================================================================
 */

/** @brief Control frame dispatch result */
typedef enum : uint8_t {
  k_dispatch_pass_through = 0, /**< Frame should be returned to caller */
  k_dispatch_consumed     = 1, /**< Frame was handled internally */
  k_dispatch_error        = 2, /**< Error during control frame handling */
} dispatch_result_t;

/**
 * @brief Handle control frames (PING, RESET) internally
 *
 * PING: Auto-sends PONG echoing the payload, invokes on_ping_cb.
 * RESET: Resets sequence counters, auto-sends RESET_ACK, invokes on_reset_cb.
 * All other frame types pass through to the caller.
 *
 * @param[in,out] handle SPI communication handle
 * @param[in]     frame  Decoded frame to inspect
 * @param[out]    err    Error code if dispatch_error returned
 *
 * @return k_dispatch_consumed if frame was handled internally
 * @return k_dispatch_pass_through if frame should be returned to caller
 * @return k_dispatch_error if an error occurred during handling
 */
static dispatch_result_t internal_dispatch_control(rx_spi_comm_handle_t* handle,
                                                    const rx_frame_t*     frame,
                                                    rx_err_t*             err)
{
  /* Pre-condition 1: Handle must be valid (NASA Rule 5) */
  if (handle == NULL) {
    if (err != NULL) {
      *err = k_rx_err_invalid_arg;
    }
    return k_dispatch_error;
  }

  /* Pre-condition 2: Frame must be valid (NASA Rule 5) */
  if (frame == NULL || err == NULL) {
    if (err != NULL) {
      *err = k_rx_err_invalid_arg;
    }
    return k_dispatch_error;
  }

  const rx_frame_type_t type = (rx_frame_type_t)frame->header.type;

  if (type == k_frame_type_ping) {
    /* Auto-respond with PONG echoing the PING payload */
    *err = rx_spi_comm_send_pong(handle, frame->payload, frame->header.length);
    if (*err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to send PONG response");
      return k_dispatch_error;
    }

    if (handle->on_ping_cb != NULL) {
      handle->on_ping_cb(frame, handle->cb_ctx);
    }

    return k_dispatch_consumed;
  }

  if (type == k_frame_type_reset) {
    /* Reset sequence counters */
    handle->tx_sequence = k_control_frame_sequence;
    handle->rx_sequence = k_control_frame_sequence;

    /* Auto-respond with RESET_ACK */
    *err = rx_spi_comm_send_reset_ack(handle);
    if (*err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to send RESET_ACK response");
      return k_dispatch_error;
    }

    if (handle->on_reset_cb != NULL) {
      handle->on_reset_cb(frame, handle->cb_ctx);
    }

    return k_dispatch_consumed;
  }

  return k_dispatch_pass_through;
}

/* =============================================================================
 * Receive API - Public Functions
 * =============================================================================
 */

/** @brief Maximum control frames to handle before returning timeout */
typedef enum : uint8_t {
  k_max_control_retries = 10, /**< Prevent infinite loop from control frame flood */
} receive_limits_t;

rx_err_t
rx_spi_comm_receive(rx_spi_comm_handle_t* handle, rx_frame_t* frame, const uint32_t timeout_ms)
{
  if (handle == NULL || frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  uint8_t retries = 0;

  while (retries < k_max_control_retries) {
    /* Wait for data to be available */
    rx_err_t err = internal_wait_for_data(handle, timeout_ms);
    if (err != k_rx_ok) {
      return err;
    }

    /* Read and validate frame header */
    uint8_t  header_buf[k_frame_sync_size + k_frame_header_size];
    uint16_t payload_len = 0;

    err = internal_read_frame_header(handle, header_buf, &payload_len);
    if (err != k_rx_ok) {
      return err;
    }

    /* Calculate frame size */
    const uint32_t header_len = sizeof(header_buf);
    const uint32_t total_size =
      k_frame_sync_size + k_frame_header_size + payload_len + k_frame_crc_size;

    /* Read remaining data (payload + CRC) into a staging buffer to avoid
     * overlapping memcpy UB. internal_spi_transfer writes into handle->rx_buffer
     * internally, so passing handle->rx_buffer + offset as rx_data would overlap. */
    const uint32_t remaining = payload_len + k_frame_crc_size;
    if (remaining > 0) {
      uint8_t staging[k_frame_max_payload + k_frame_crc_size];

      err = internal_spi_transfer(handle, NULL, 0, staging, remaining);
      if (err != k_rx_ok) {
        return err;
      }
      memcpy(handle->rx_buffer + header_len, staging, remaining);
    }

    /* Copy header to buffer for decoding */
    memcpy(handle->rx_buffer, header_buf, header_len);

    /* Decode frame */
    err = internal_decode_frame(handle, frame, total_size);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Frame decode failed");
      return err;
    }

    /* Update expected RX sequence */
    handle->rx_sequence = frame->header.sequence + 1;

    /* Dispatch control frames (PING, RESET) - consumed internally */
    const dispatch_result_t result = internal_dispatch_control(handle, frame, &err);
    if (result == k_dispatch_error) {
      return err;
    }
    if (result == k_dispatch_pass_through) {
      return k_rx_ok;
    }

    /* Control frame was consumed, retry to get a data frame */
    retries++;
  }

  return k_rx_err_timeout;
}

rx_err_t rx_spi_comm_data_available(const rx_spi_comm_handle_t* handle, bool* available)
{
  if (handle == NULL || available == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  return rspi_peripheral_read_available(handle->channel, available);
}

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

void rx_spi_comm_reset_sequence(rx_spi_comm_handle_t* handle)
{
  if (handle != NULL) {
    handle->tx_sequence = 0;
    handle->rx_sequence = 0;
  }
}

rx_err_t rx_spi_comm_get_tx_sequence(const rx_spi_comm_handle_t* handle, uint16_t* sequence)
{
  if (handle == NULL || sequence == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  *sequence = handle->tx_sequence;
  return k_rx_ok;
}

rx_err_t rx_spi_comm_get_rx_sequence(const rx_spi_comm_handle_t* handle, uint16_t* sequence)
{
  if (handle == NULL || sequence == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  *sequence = handle->rx_sequence;
  return k_rx_ok;
}
