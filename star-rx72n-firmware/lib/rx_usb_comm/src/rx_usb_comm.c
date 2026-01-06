/* lib/rx_usb_comm/src/rx_usb_comm.c */

/**
 * @file rx_usb_comm.c
 * @brief High-Level USB CDC Communication Layer for RX72N
 * @details
 * Implements reliable USB communication by integrating the frame layer
 * and USB CDC driver. Handles frame encoding/decoding, CRC validation,
 * and sequence number management.
 *
 * @note USB CDC appears as /dev/ttyACM0 on RPi5 host.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_usb_comm.h"

#include <string.h>

#include "rx_log.h"
#include "rx_usb.h"

/* =============================================================================
 * Module State
 * =============================================================================
 */

static const char* s_tag = "USB_COMM";

/* =============================================================================
 * Frame Header Constants
 *
 * Frame format: [SYNC(2B)][SEQ(2B)][LEN(2B)][TYPE(1B)][FLAGS(1B)]
 * =============================================================================
 */

/** @brief Total header size including sync word: SYNC(2) + SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1) */
static const uint32_t s_frame_header_total = 8;

/** @brief Sleep interval for receive polling (ms) */
static const uint32_t s_sleep_interval_ms = 10;

/**
 * @brief Sequence number constants
 */
typedef enum {
  k_initial_sequence = 0, /**< Initial TX/RX sequence number */
} rx_usb_comm_sequence_constants_t;

/**
 * @brief Receive loop bounds for NASA Power of 10 Rule 2 compliance
 */
typedef enum {
  k_max_receive_iterations = 100, /**< Max receive loop attempts */
} rx_usb_comm_loop_limits_t;

/** @brief Byte offsets within frame header buffer */
typedef enum {
  k_hdr_len_offset = 4, /**< Payload length field offset */
} frame_header_offset_t;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Sync word not found sentinel value
 */
typedef enum {
  k_sync_not_found = -1, /**< Sync word not found in buffer */
} rx_usb_comm_sync_result_t;

/**
 * @brief Read more data from USB into staging buffer
 *
 * @param[in,out] handle USB communication handle
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle is NULL
 * @return k_rx_err_invalid_state if buffer state is inconsistent
 */
static rx_err_t internal_read_usb_data(rx_usb_comm_handle_t* handle)
{
  /* Pre-condition 1: Handle must be valid */
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Buffer length must be within bounds */
  if (handle->rx_buffer_len > k_usb_comm_rx_buffer_size) {
    return k_rx_err_invalid_state;
  }

  /* Calculate available space in buffer */
  uint32_t space = k_usb_comm_rx_buffer_size - handle->rx_buffer_len;
  if (space == 0) {
    return k_rx_ok; /* Buffer full */
  }

  /* Read from USB */
  uint32_t bytes_read = 0;
  rx_err_t err        = rx_usb_read(handle->rx_buffer + handle->rx_buffer_len, space, &bytes_read);

  if (err != k_rx_ok) {
    return err;
  }

  handle->rx_buffer_len += bytes_read;

  /* Post-condition: Buffer length must not exceed maximum */
  if (handle->rx_buffer_len > k_usb_comm_rx_buffer_size) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

/**
 * @brief Compact receive buffer by removing consumed data
 *
 * @param[in,out] handle USB communication handle
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle is NULL
 * @return k_rx_err_invalid_state if buffer state is inconsistent
 */
static rx_err_t internal_compact_rx_buffer(rx_usb_comm_handle_t* handle)
{
  /* Pre-condition 1: Handle must be valid */
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Position must not exceed length */
  if (handle->rx_buffer_pos > handle->rx_buffer_len) {
    return k_rx_err_invalid_state;
  }

  if (handle->rx_buffer_pos == 0) {
    return k_rx_ok; /* Nothing to compact */
  }

  if (handle->rx_buffer_pos >= handle->rx_buffer_len) {
    /* All data consumed, reset buffer */
    handle->rx_buffer_len = 0;
    handle->rx_buffer_pos = 0;
  } else {
    /* Move remaining data to beginning */
    uint32_t remaining = handle->rx_buffer_len - handle->rx_buffer_pos;
    memmove(handle->rx_buffer, handle->rx_buffer + handle->rx_buffer_pos, remaining);
    handle->rx_buffer_len = remaining;
    handle->rx_buffer_pos = 0;
  }

  /* Post-condition: Position should be reset to 0 */
  if (handle->rx_buffer_pos != 0) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

/**
 * @brief Search for sync word in receive buffer
 *
 * Scans the receive buffer for the frame sync word (0x55AA) stored in
 * big-endian format. Uses the shared byte index constants from rx_frame.h.
 *
 * @param[in] handle USB communication handle
 * @param[out] sync_pos Output position of sync word if found
 * @return k_rx_ok if sync found (position in sync_pos)
 * @return k_rx_err_invalid_arg if handle or sync_pos is NULL
 * @return k_rx_err_invalid_state if buffer state is inconsistent
 * @return k_rx_err_not_found if sync word not found
 */
static rx_err_t internal_find_sync(rx_usb_comm_handle_t* handle, int32_t* sync_pos)
{
  /* Pre-condition 1: Handle must be valid */
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Output pointer must be valid */
  if (sync_pos == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 3: Buffer position must be within length */
  if (handle->rx_buffer_pos > handle->rx_buffer_len) {
    return k_rx_err_invalid_state;
  }

  /* Extract sync word bytes using shared constants from rx_frame.h */
  uint8_t sync_high = (uint8_t)(k_frame_sync_word >> k_rx_be16_high_shift);
  uint8_t sync_low  = (uint8_t)(k_frame_sync_word & k_rx_byte_mask);

  for (uint32_t i = handle->rx_buffer_pos; i + 1 < handle->rx_buffer_len; i++) {
    if (handle->rx_buffer[i] == sync_high && handle->rx_buffer[i + 1] == sync_low) {
      *sync_pos = (int32_t)i;
      return k_rx_ok;
    }
  }

  /* Post-condition: Set output to not-found sentinel */
  *sync_pos = k_sync_not_found;
  return k_rx_err_not_found;
}

/**
 * @brief Sleep and advance elapsed time if time interface available
 *
 * @param[in] handle USB communication handle
 * @param[in,out] elapsed_ms Pointer to elapsed time counter
 * @return true if sleep was performed, false if no time interface
 */
static bool internal_sleep_and_advance(rx_usb_comm_handle_t* handle, uint32_t* elapsed_ms)
{
  if (handle->time_iface != NULL && handle->time_iface->sleep_ms != NULL) {
    handle->time_iface->sleep_ms(handle->time_iface->ctx, s_sleep_interval_ms);
    *elapsed_ms += s_sleep_interval_ms;
    return true;
  }
  return false;
}

/**
 * @brief Check if timeout has been reached
 *
 * @param[in] timeout_ms Timeout value in milliseconds (0 = immediate)
 * @param[in] elapsed_ms Elapsed time in milliseconds
 * @return true if timed out, false otherwise
 */
static bool internal_is_timed_out(uint32_t timeout_ms, uint32_t elapsed_ms)
{
  return (timeout_ms == 0) || (elapsed_ms >= timeout_ms);
}

/**
 * @brief Handle case when no sync word is found in buffer
 *
 * Advances buffer position if full, compacts buffer, and handles timeout/sleep.
 *
 * @param[in,out] handle USB communication handle
 * @param[in] timeout_ms Timeout value in milliseconds
 * @param[in,out] elapsed_ms Pointer to elapsed time counter
 * @return k_rx_ok to continue searching
 * @return k_rx_err_timeout if timed out or no time interface
 */
static rx_err_t internal_handle_no_sync(rx_usb_comm_handle_t* handle,
                                        uint32_t              timeout_ms,
                                        uint32_t*             elapsed_ms)
{
  /* Buffer full and no sync - discard one byte to advance sliding window */
  if (handle->rx_buffer_len >= k_usb_comm_rx_buffer_size) {
    handle->rx_buffer_pos++;
  }

  /* Compact buffer (discard consumed/skipped data) */
  rx_err_t compact_err = internal_compact_rx_buffer(handle);
  if (compact_err != k_rx_ok) {
    return compact_err;
  }

  if (internal_is_timed_out(timeout_ms, *elapsed_ms)) {
    return k_rx_err_timeout;
  }

  /* Sleep and retry if time interface available */
  if (!internal_sleep_and_advance(handle, elapsed_ms)) {
    return k_rx_err_timeout;
  }

  return k_rx_ok;
}

/**
 * @brief Wait for more data with timeout handling
 *
 * @param[in,out] handle USB communication handle
 * @param[in] timeout_ms Timeout value in milliseconds
 * @param[in,out] elapsed_ms Pointer to elapsed time counter
 * @return k_rx_ok to continue waiting
 * @return k_rx_err_timeout if timed out or no time interface
 */
static rx_err_t internal_wait_for_data(rx_usb_comm_handle_t* handle,
                                       uint32_t              timeout_ms,
                                       uint32_t*             elapsed_ms)
{
  if (internal_is_timed_out(timeout_ms, *elapsed_ms)) {
    return k_rx_err_timeout;
  }

  /* Sleep and retry if time interface available */
  if (!internal_sleep_and_advance(handle, elapsed_ms)) {
    return k_rx_err_timeout;
  }

  return k_rx_ok;
}

/**
 * @brief Decode a complete frame from buffer
 *
 * @param[in,out] handle USB communication handle
 * @param[out] frame Decoded frame output
 * @param[in] total_size Total frame size in bytes
 * @return k_rx_ok on success
 * @return Error code from rx_frame_decode on failure
 */
static rx_err_t internal_decode_frame(rx_usb_comm_handle_t* handle,
                                      rx_frame_t*           frame,
                                      uint32_t              total_size)
{
  uint8_t* hdr = handle->rx_buffer + handle->rx_buffer_pos;

  /* Decode the frame */
  rx_err_t err = rx_frame_decode(&handle->decoder, hdr, total_size, frame);

  /* Consume the frame data */
  handle->rx_buffer_pos += total_size;
  rx_err_t compact_err = internal_compact_rx_buffer(handle);
  if (compact_err != k_rx_ok && err == k_rx_ok) {
    return compact_err;
  }

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame decode failed");
    return err;
  }

  /* Update expected RX sequence */
  handle->rx_sequence = frame->header.sequence + 1;

  return k_rx_ok;
}

/**
 * @brief Receive iteration result codes
 */
typedef enum {
  k_receive_continue = 0, /**< Continue to next iteration */
  k_receive_done     = 1, /**< Frame received successfully */
  k_receive_error    = 2, /**< Error occurred, check err output */
} rx_receive_result_t;

/**
 * @brief Align buffer position to sync word if found
 *
 * @param[in,out] handle USB communication handle
 * @param[in] sync_pos Position of sync word in buffer
 */
static void internal_align_to_sync(rx_usb_comm_handle_t* handle, int32_t sync_pos)
{
  if ((uint32_t)sync_pos > handle->rx_buffer_pos) {
    handle->rx_buffer_pos = (uint32_t)sync_pos;
  }
}

/**
 * @brief Parse frame header and validate payload length
 *
 * @param[in] handle USB communication handle
 * @param[out] payload_len Output payload length
 * @param[out] total_size Output total frame size
 * @return true if header is valid, false if invalid payload length
 */
static bool internal_parse_header(rx_usb_comm_handle_t* handle,
                                  uint16_t*             payload_len,
                                  uint32_t*             total_size)
{
  uint8_t* hdr = handle->rx_buffer + handle->rx_buffer_pos;
  *payload_len = rx_frame_read_be16(&hdr[k_hdr_len_offset]);

  if (*payload_len > k_frame_max_payload) {
    rx_log_warn(s_tag, "Invalid payload length, skipping");
    handle->rx_buffer_pos += k_frame_sync_size;
    return false;
  }

  *total_size = s_frame_header_total + *payload_len + k_frame_crc_size;
  return true;
}

/**
 * @brief Process one iteration of the receive loop
 *
 * @param[in,out] handle USB communication handle
 * @param[out] frame Decoded frame output
 * @param[in] timeout_ms Timeout value in milliseconds
 * @param[in,out] elapsed_ms Pointer to elapsed time counter
 * @param[out] err Error code if result is k_receive_error
 * @return k_receive_continue to continue loop
 * @return k_receive_done if frame was received
 * @return k_receive_error if an error occurred
 */
static rx_receive_result_t internal_receive_iteration(rx_usb_comm_handle_t* handle,
                                                      rx_frame_t*           frame,
                                                      uint32_t              timeout_ms,
                                                      uint32_t*             elapsed_ms,
                                                      rx_err_t*             err)
{
  int32_t  sync_pos    = k_sync_not_found;
  uint32_t available   = 0;
  uint32_t total_size  = 0;
  uint16_t payload_len = 0;

  /* Read any available USB data */
  *err = internal_read_usb_data(handle);
  if (*err != k_rx_ok && *err != k_rx_err_timeout) {
    return k_receive_error;
  }

  /* Search for sync word */
  *err = internal_find_sync(handle, &sync_pos);
  if (*err == k_rx_err_not_found) {
    *err = internal_handle_no_sync(handle, timeout_ms, elapsed_ms);
    return (*err != k_rx_ok) ? k_receive_error : k_receive_continue;
  } else if (*err != k_rx_ok) {
    return k_receive_error;
  }

  /* Found sync - align to it */
  internal_align_to_sync(handle, sync_pos);

  /* Check if we have enough data for header */
  available = handle->rx_buffer_len - handle->rx_buffer_pos;
  if (available < s_frame_header_total) {
    *err = internal_wait_for_data(handle, timeout_ms, elapsed_ms);
    return (*err != k_rx_ok) ? k_receive_error : k_receive_continue;
  }

  /* Parse and validate header */
  if (!internal_parse_header(handle, &payload_len, &total_size)) {
    return k_receive_continue;
  }

  /* Check if we have complete frame */
  if (available < total_size) {
    *err = internal_wait_for_data(handle, timeout_ms, elapsed_ms);
    return (*err != k_rx_ok) ? k_receive_error : k_receive_continue;
  }

  /* Decode the complete frame */
  *err = internal_decode_frame(handle, frame, total_size);
  return (*err != k_rx_ok) ? k_receive_error : k_receive_done;
}

/* =============================================================================
 * Initialization
 * =============================================================================
 */

rx_err_t rx_usb_comm_init(rx_usb_comm_handle_t* handle, const rx_usb_comm_config_t* config)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Clear handle */
  memset(handle, 0, sizeof(rx_usb_comm_handle_t));

  /* Apply configuration */
  if (config != NULL) {
    handle->fec_enabled = config->fec_enabled;
    handle->time_iface  = config->time_iface;
  } else {
    handle->fec_enabled = 0;
    handle->time_iface  = NULL;
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

    /* Attempt cleanup, but propagate decoder init error */
    rx_err_t cleanup_err = rx_frame_encoder_deinit(&handle->encoder);
    if (cleanup_err != k_rx_ok) {
      rx_log_warn(s_tag, "Failed to cleanup encoder during decoder init failure");
    }

    return err;
  }

  /* Initialize sequence counters */
  handle->tx_sequence = k_initial_sequence;
  handle->rx_sequence = k_initial_sequence;

  /* Initialize buffer state */
  handle->rx_buffer_len = 0;
  handle->rx_buffer_pos = 0;

  handle->initialized = true;

  rx_log_debug(s_tag, "USB comm initialized");
  return k_rx_ok;
}

rx_err_t rx_usb_comm_deinit(rx_usb_comm_handle_t* handle)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (handle->initialized) {
    rx_frame_encoder_deinit(&handle->encoder);
    rx_frame_decoder_deinit(&handle->decoder);
    handle->initialized = false;
  }

  rx_log_debug(s_tag, "USB comm deinitialized");
  return k_rx_ok;
}

/* =============================================================================
 * Send API
 * =============================================================================
 */

rx_err_t rx_usb_comm_send(rx_usb_comm_handle_t* handle,
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

  /* Check USB is ready */
  if (!rx_usb_is_configured()) {
    rx_log_error(s_tag, "USB not configured");
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
  uint32_t wire_len = 0;

  rx_err_t err = rx_frame_encode(&handle->encoder, &frame, handle->tx_buffer, &wire_len);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame encode failed");
    return err;
  }

  /* Send via USB */
  err = rx_usb_write(handle->tx_buffer, wire_len);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "USB write failed");
    return err;
  }

  /* Increment TX sequence */
  handle->tx_sequence++;

  return k_rx_ok;
}

rx_err_t rx_usb_comm_send_ack(rx_usb_comm_handle_t* handle, uint16_t sequence)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  if (!rx_usb_is_configured()) {
    return k_rx_err_invalid_state;
  }

  /* Create ACK frame */
  rx_frame_t ack_frame;
  rx_err_t   err = rx_frame_create_ack(&ack_frame, sequence);
  if (err != k_rx_ok) {
    return err;
  }

  /* Encode and send */
  uint32_t wire_len = 0;

  err = rx_frame_encode(&handle->encoder, &ack_frame, handle->tx_buffer, &wire_len);
  if (err != k_rx_ok) {
    return err;
  }

  return rx_usb_write(handle->tx_buffer, wire_len);
}

rx_err_t rx_usb_comm_send_nack(rx_usb_comm_handle_t* handle, uint16_t sequence, uint8_t flags)
{
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  if (!rx_usb_is_configured()) {
    return k_rx_err_invalid_state;
  }

  /* Create NACK frame */
  rx_frame_t nack_frame;
  rx_err_t   err = rx_frame_create_nack(&nack_frame, sequence, flags);
  if (err != k_rx_ok) {
    return err;
  }

  /* Encode and send */
  uint32_t wire_len = 0;

  err = rx_frame_encode(&handle->encoder, &nack_frame, handle->tx_buffer, &wire_len);
  if (err != k_rx_ok) {
    return err;
  }

  return rx_usb_write(handle->tx_buffer, wire_len);
}

/* =============================================================================
 * Receive API
 * =============================================================================
 */

rx_err_t rx_usb_comm_receive(rx_usb_comm_handle_t* handle, rx_frame_t* frame, uint32_t timeout_ms)
{
  /* Pre-condition 1: Handle must be valid */
  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Frame output must be valid */
  if (frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 3: Handle must be initialized */
  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Pre-condition 4: USB must be configured */
  if (!rx_usb_is_configured()) {
    return k_rx_err_invalid_state;
  }

  uint32_t             elapsed_ms = 0;
  uint32_t             iterations = 0;
  rx_err_t             err        = k_rx_ok;
  rx_receive_result_t  result     = k_receive_continue;

  /* Try to receive a complete frame with bounded iterations */
  while (iterations < k_max_receive_iterations) {
    iterations++;

    result = internal_receive_iteration(handle, frame, timeout_ms, &elapsed_ms, &err);
    if (result == k_receive_done) {
      return k_rx_ok;
    } else if (result == k_receive_error) {
      return err;
    }
    /* k_receive_continue: loop continues */
  }

  /* Exceeded maximum iterations */
  return k_rx_err_timeout;
}

rx_err_t rx_usb_comm_data_available(rx_usb_comm_handle_t* handle, bool* available)
{
  if (handle == NULL || available == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check USB CDC buffer */
  uint32_t usb_available = 0;
  rx_err_t err           = rx_usb_rx_available(&usb_available);
  if (err != k_rx_ok) {
    return err;
  }

  /* Also check our staging buffer */
  uint32_t buffered = handle->rx_buffer_len - handle->rx_buffer_pos;

  *available = (usb_available > 0) || (buffered > 0);
  return k_rx_ok;
}

rx_err_t rx_usb_comm_is_ready(rx_usb_comm_handle_t* handle, bool* ready)
{
  if (handle == NULL || ready == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    *ready = false;
    return k_rx_err_invalid_state;
  }

  *ready = rx_usb_is_configured();
  return k_rx_ok;
}

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

void rx_usb_comm_reset_sequence(rx_usb_comm_handle_t* handle)
{
  if (handle != NULL) {
    handle->tx_sequence = k_initial_sequence;
    handle->rx_sequence = k_initial_sequence;
  }
}

uint16_t rx_usb_comm_get_tx_sequence(const rx_usb_comm_handle_t* handle)
{
  if (handle == NULL) {
    return 0;
  }
  return handle->tx_sequence;
}

uint16_t rx_usb_comm_get_rx_sequence(const rx_usb_comm_handle_t* handle)
{
  if (handle == NULL) {
    return 0;
  }
  return handle->rx_sequence;
}

void rx_usb_comm_flush_rx(rx_usb_comm_handle_t* handle)
{
  if (handle != NULL) {
    handle->rx_buffer_len = 0;
    handle->rx_buffer_pos = 0;
  }
}
