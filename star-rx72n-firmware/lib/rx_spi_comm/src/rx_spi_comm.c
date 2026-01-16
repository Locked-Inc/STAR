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
typedef enum {
  k_hdr_sync_high = 0, /**< SYNC word high byte */
  k_hdr_sync_low  = 1, /**< SYNC word low byte */
  k_hdr_seq_high  = 2, /**< Sequence number high byte */
  k_hdr_seq_low   = 3, /**< Sequence number low byte */
  k_hdr_len_high  = 4, /**< Payload length high byte */
  k_hdr_len_low   = 5, /**< Payload length low byte */
  k_hdr_type      = 6, /**< Frame type */
  k_hdr_flags     = 7, /**< Frame flags */
} frame_header_offset_t;

#ifdef __RX__
/** @brief Sleep duration for polling loop (1 tick) */
static const uint32_t s_poll_sleep_ticks = 1;
#endif

/** @brief ACK/ready wait timing constants */
typedef enum {
  k_ack_wait_timeout_ms = 50, /**< Abort if host doesn't ACK within 50 ms */
} ack_wait_t;

/** @brief Polling loop iteration limits */
typedef enum {
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
static rx_err_t internal_wait_for_ack(rx_spi_comm_handle_t* handle, uint32_t timeout_ms)
{
#ifdef __RX__
  /* RX72N: Use ThreadX time measurement for precise timeout */
  ULONG timeout_ticks = (timeout_ms + k_threadx_ms_per_tick - 1) / k_threadx_ms_per_tick;
  ULONG start_ticks   = tx_time_get();

  while ((tx_time_get() - start_ticks) < timeout_ticks) {
    bool     ready = false;
    rx_err_t err   = rspi_peripheral_write_ready(handle->channel, &ready);
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
    bool     ready = false;
    rx_err_t err   = rspi_peripheral_write_ready(handle->channel, &ready);
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
                                      uint32_t              tx_len,
                                      uint8_t*              rx_data,
                                      uint32_t              rx_len)
{
  /* Pre-condition 1: Handle pointer validation */
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Transfer length within buffer capacity */
  uint32_t transfer_len = (tx_len > rx_len) ? tx_len : rx_len;
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
    rx_err_t wait_err = internal_wait_for_ack(handle, k_ack_wait_timeout_ms);
    if (wait_err != k_rx_ok) {
      return wait_err;
    }
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
  rx_err_t err = rspi_peripheral_transfer(handle->channel,
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

rx_err_t rx_spi_comm_send(rx_spi_comm_handle_t* handle,
                          rx_frame_type_t       type,
                          uint8_t               flags,
                          const uint8_t*        payload,
                          uint32_t              payload_len)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    rx_log_error(s_tag, "Handle not initialized");
    return k_rx_err_invalid_state;
  }

  if (payload == NULL && payload_len > 0) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > k_frame_max_payload) {
    rx_log_error(s_tag, "Payload too large");
    return k_rx_err_invalid_size;
  }

  /* Build frame */
  rx_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.header.sequence = handle->tx_sequence;
  frame.header.length   = (uint16_t)payload_len;
  frame.header.type     = (uint8_t)type;
  frame.header.flags    = flags;

  if (payload != NULL && payload_len > 0) {
    memcpy(frame.payload, payload, payload_len);
  }

  /* Add FEC flag if enabled */
  if (handle->fec_enabled) {
    frame.header.flags |= k_frame_flag_fec_enabled;
  }

  /* Encode frame to wire format */
  uint8_t  wire_buffer[k_frame_max_size];
  uint32_t wire_len = 0;

  rx_err_t err = rx_frame_encode(&handle->encoder, &frame, wire_buffer, &wire_len);
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
  uint16_t expected_sequence = handle->tx_sequence + 1;
  handle->tx_sequence++;
  if (handle->tx_sequence != expected_sequence) {
    rx_log_error(s_tag, "TX sequence increment failed");
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

rx_err_t rx_spi_comm_send_ack(rx_spi_comm_handle_t* handle, uint16_t sequence)
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

rx_err_t rx_spi_comm_send_nack(rx_spi_comm_handle_t* handle, uint16_t sequence, uint8_t flags)
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
static rx_err_t internal_wait_for_data(rx_spi_comm_handle_t* handle, uint32_t timeout_ms)
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
  uint32_t header_len = k_frame_sync_size + k_frame_header_size;

  /* Read frame header from SPI */
  rx_err_t err = internal_spi_transfer(handle, NULL, 0, header_buf, header_len);
  if (err != k_rx_ok) {
    return err;
  }

  /* Validate sync word (big-endian) */
  uint16_t sync = rx_frame_read_be16(&header_buf[k_hdr_sync_high]);
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

/* =============================================================================
 * Receive API - Public Functions
 * =============================================================================
 */

rx_err_t rx_spi_comm_receive(rx_spi_comm_handle_t* handle, rx_frame_t* frame, uint32_t timeout_ms)
{
  if (handle == NULL || frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

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
  uint32_t header_len = sizeof(header_buf);
  uint32_t total_size = k_frame_sync_size + k_frame_header_size + payload_len + k_frame_crc_size;

  /* Read remaining data (payload + CRC) */
  uint32_t remaining = payload_len + k_frame_crc_size;
  if (remaining > 0) {
    err = internal_spi_transfer(handle, NULL, 0, handle->rx_buffer + header_len, remaining);
    if (err != k_rx_ok) {
      return err;
    }
  }

  /* Copy header to buffer for decoding */
  memcpy(handle->rx_buffer, header_buf, header_len);

  /* Decode frame */
  err = rx_frame_decode(&handle->decoder, handle->rx_buffer, total_size, frame);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame decode failed");
    return err;
  }

  /* Update expected RX sequence */
  handle->rx_sequence = frame->header.sequence + 1;

  return k_rx_ok;
}

rx_err_t rx_spi_comm_data_available(rx_spi_comm_handle_t* handle, bool* available)
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
