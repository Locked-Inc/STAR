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

/** @brief Internal timing and buffer constants */
typedef enum {
  k_frame_header_total = 10, /**< Total header size including sync word (2 + 8) */
  k_sleep_interval_ms  = 10, /**< Sleep interval for receive polling (ms) */
} rx_usb_comm_internal_t;

/** @brief Byte offsets within frame header buffer */
typedef enum {
  k_hdr_sync_high  = 0, /**< SYNC word high byte */
  k_hdr_sync_low   = 1, /**< SYNC word low byte */
  k_hdr_len_offset = 4, /**< Payload length field offset */
} frame_header_offset_t;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Read more data from USB into staging buffer
 */
static rx_err_t internal_read_usb_data(rx_usb_comm_handle_t* handle)
{
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
  return k_rx_ok;
}

/**
 * @brief Compact receive buffer by removing consumed data
 */
static void internal_compact_rx_buffer(rx_usb_comm_handle_t* handle)
{
  if (handle->rx_buffer_pos == 0) {
    return; /* Nothing to compact */
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
}

/**
 * @brief Search for sync word in receive buffer
 *
 * @return Index of sync word, or -1 if not found
 */
static int32_t internal_find_sync(rx_usb_comm_handle_t* handle)
{
  uint8_t sync_high = (uint8_t)(k_frame_sync_word >> 8);
  uint8_t sync_low  = (uint8_t)(k_frame_sync_word & 0xFF);

  for (uint32_t i = handle->rx_buffer_pos; i + 1 < handle->rx_buffer_len; i++) {
    if (handle->rx_buffer[i] == sync_high && handle->rx_buffer[i + 1] == sync_low) {
      return (int32_t)i;
    }
  }

  return -1;
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
    rx_frame_encoder_deinit(&handle->encoder);
    return err;
  }

  /* Initialize sequence counters */
  handle->tx_sequence = 0;
  handle->rx_sequence = 0;

  /* Initialize buffer state */
  handle->rx_buffer_len = 0;
  handle->rx_buffer_pos = 0;

  handle->initialized = 1;

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
    handle->initialized = 0;
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
  if (handle == NULL || frame == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  if (!rx_usb_is_configured()) {
    return k_rx_err_invalid_state;
  }

  uint32_t elapsed_ms = 0;
  rx_err_t err;

  /* Try to receive a complete frame */
  while (1) {
    /* Read any available USB data */
    err = internal_read_usb_data(handle);
    if (err != k_rx_ok && err != k_rx_err_timeout) {
      return err;
    }

    /* Search for sync word */
    int32_t sync_pos = internal_find_sync(handle);

    if (sync_pos < 0) {
      /* No sync found */
      if (handle->rx_buffer_len >= k_usb_comm_rx_buffer_size) {
        /* Buffer full and no sync - discard one byte to advance sliding window */
        handle->rx_buffer_pos++;
      }

      /* Compact buffer (discard consumed/skipped data) */
      internal_compact_rx_buffer(handle);

      if (timeout_ms == 0 || elapsed_ms >= timeout_ms) {
        return k_rx_err_timeout;
      }

      /* Sleep and retry if time interface available */
      if (handle->time_iface != NULL && handle->time_iface->sleep_ms != NULL) {
        handle->time_iface->sleep_ms(handle->time_iface->ctx, k_sleep_interval_ms);
        elapsed_ms += k_sleep_interval_ms;
      } else {
        return k_rx_err_timeout;
      }
      continue;
    }

    /* Found sync - discard any data before it */
    if ((uint32_t)sync_pos > handle->rx_buffer_pos) {
      handle->rx_buffer_pos = (uint32_t)sync_pos;
    }

    /* Check if we have enough data for header */
    uint32_t available = handle->rx_buffer_len - handle->rx_buffer_pos;
    if (available < k_frame_header_total) {
      /* Need more data for header */
      if (timeout_ms == 0 || elapsed_ms >= timeout_ms) {
        return k_rx_err_timeout;
      }

      /* Sleep and retry if time interface available */
      if (handle->time_iface != NULL && handle->time_iface->sleep_ms != NULL) {
        handle->time_iface->sleep_ms(handle->time_iface->ctx, k_sleep_interval_ms);
        elapsed_ms += k_sleep_interval_ms;
      } else {
        return k_rx_err_timeout;
      }
      continue;
    }

    /* Parse header to get payload length */
    uint8_t* hdr         = handle->rx_buffer + handle->rx_buffer_pos;
    uint16_t payload_len = rx_read_be16(&hdr[k_hdr_len_offset]);

    if (payload_len > k_frame_max_payload) {
      /* Invalid payload length - skip this sync and search for next */
      rx_log_warn(s_tag, "Invalid payload length, skipping");
      handle->rx_buffer_pos += k_frame_sync_size;
      continue;
    }

    /* Calculate total frame size */
    uint32_t total_size = k_frame_header_total + payload_len + k_frame_crc_size;

    /* Check if we have complete frame */
    if (available < total_size) {
      /* Need more data */
      if (timeout_ms == 0 || elapsed_ms >= timeout_ms) {
        return k_rx_err_timeout;
      }

      /* Sleep and retry if time interface available */
      if (handle->time_iface != NULL && handle->time_iface->sleep_ms != NULL) {
        handle->time_iface->sleep_ms(handle->time_iface->ctx, k_sleep_interval_ms);
        elapsed_ms += k_sleep_interval_ms;
      } else {
        return k_rx_err_timeout;
      }
      continue;
    }

    /* We have a complete frame - decode it */
    err = rx_frame_decode(&handle->decoder, hdr, total_size, frame);

    /* Consume the frame data */
    handle->rx_buffer_pos += total_size;
    internal_compact_rx_buffer(handle);

    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Frame decode failed");
      return err;
    }

    /* Update expected RX sequence */
    handle->rx_sequence = frame->header.sequence + 1;

    return k_rx_ok;
  }
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
    handle->tx_sequence = 0;
    handle->rx_sequence = 0;
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
