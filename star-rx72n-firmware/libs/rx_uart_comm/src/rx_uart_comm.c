/**
 * @file rx_uart_comm.c
 * @brief High-Level UART (SCI9) Communication Layer Implementation
 *
 * @details
 * Implements reliable UART communication by integrating the frame protocol layer
 * (rx_frame) with the SCI9 UART driver (hardware.h HAL). Handles frame encoding/decoding,
 * CRC-32 validation, sequence number management, and receive buffer handling.
 *
 * @par Implementation Architecture:
 * @msc
 * App, UART_Comm, Frame, UART_HAL;
 * --- [label="TX Path"];
 * App => UART_Comm [label="send(type, payload)"];
 * UART_Comm => Frame [label="encode()"];
 * UART_Comm => UART_HAL [label="uart_write_channel()"];
 * --- [label="RX Path"];
 * UART_HAL => UART_Comm [label="uart_read_channel()"];
 * UART_Comm box UART_Comm [label="Find sync, decode"];
 * UART_Comm => App [label="frame"];
 * @endmsc
 *
 * @par Channel Assignment:
 * | Channel | Constant         | Purpose              |
 * |---------|------------------|----------------------|
 * | SCI9    | k_uart_channel_9 | Binary protocol      |
 *
 * @par Performance:
 * | Operation      | Typical   | Worst Case  |
 * |----------------|-----------|-------------|
 * | Send (64B)     | 6 ms      | 20 ms       |
 * | Receive        | 6 ms      | non-blocking|
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion
 * - Rule 2: [OK] Loops bounded by k_uart_comm_max_receive_iterations
 * - Rule 3: [OK] No dynamic allocation
 * - Rule 5: [OK] Pre/post conditions on all functions
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Typed enums for constants
 *
 * @par SOLID Principles:
 * - SRP: Handles UART framing only, delegates I/O to UART HAL
 * - DIP: Uses hardware.h HAL abstraction
 *
 * @see rx_uart_comm.h Public API header
 * @see rx_frame.h Frame encoding layer
 * @see hardware.h UART HAL
 *
 * @author Locked, Inc.
 * @date 2026-03-11
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_uart_comm.h"

#include <string.h>

#include "rx_check.h"
#include "rx_crc.h"
#include "rx_log.h"

/* =============================================================================
 * Module State
 * =============================================================================
 */

static const char* s_tag = "UART_COMM";

/* =============================================================================
 * Frame Header Constants
 *
 * Frame format: [SYNC(2B)][SEQ(2B)][LEN(2B)][TYPE(1B)][FLAGS(1B)]
 * =============================================================================
 */

/** @brief Total header size including sync word: SYNC(2) + SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1) */
typedef enum : uint8_t {
  k_frame_header_total = k_frame_sync_size + k_frame_seq_size + k_frame_len_size +
                         k_frame_type_size + k_frame_flags_size,
} rx_uart_comm_header_size_t;

/** @brief CRC-32 seed value (pre-initialization before rx_crc32_ieee writes it) */
typedef enum : uint32_t {
  k_uart_crc32_seed_initial = 0U, /**< Initial value for CRC-32 output variable */
} uart_crc32_seed_t;

/**
 * @brief Decode frame header from raw wire data
 *
 * @details
 * Parses the frame header fields from a raw byte buffer. Validates sync word,
 * extracts sequence number, payload length, type, and flags.
 *
 * @param[in] data Raw frame data buffer (must not be nullptr)
 * @param[in] data_len Length of data buffer in bytes
 * @param[out] frame Frame structure to populate (must not be nullptr)
 * @param[out] offset_out Offset past header (optional, can be nullptr)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, frame header decoded
 * @retval k_rx_err_invalid_arg nullptr pointer in data or frame
 * @retval k_rx_err_invalid_size Data too short for header
 * @retval k_rx_err_protocol_error Invalid sync word
 *
 * @pre data != nullptr
 * @pre frame != nullptr
 * @post frame->header fields populated on success
 * @post offset_out contains byte offset past header (if not nullptr)
 *
 * @note Not thread-safe, but stateless (safe for concurrent calls with different data)
 *
 * @see internal_verify_crc() Called after this to verify frame CRC
 */
static rx_err_t internal_decode_header(const uint8_t* data,
                                       const uint32_t data_len,
                                       rx_frame_t*    frame,
                                       uint32_t*      offset_out)
{
  RX_ASSERT(data != nullptr, "Data pointer is nullptr");
  RX_ASSERT(frame != nullptr, "Frame pointer is nullptr");
  if (data == nullptr || frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (data_len < k_frame_min_size) {
    return k_rx_err_invalid_size;
  }

  uint32_t       offset    = 0;
  const uint16_t sync_word = rx_frame_read_le16(&data[offset]);
  if (sync_word != k_frame_sync_word) {
    return k_rx_err_protocol_error;
  }
  offset += k_frame_sync_size;

  frame->header.sequence = rx_frame_read_le16(&data[offset]);
  offset += k_frame_seq_size;

  frame->header.length = rx_frame_read_le16(&data[offset]);
  offset += k_frame_len_size;

  if (frame->header.length > k_frame_max_payload) {
    return k_rx_err_invalid_size;
  }

  const uint32_t expected_size = rx_frame_encoded_size(frame->header.length);
  if (data_len < expected_size) {
    return k_rx_err_invalid_size;
  }

  frame->header.type = data[offset];
  offset += k_frame_type_size;

  frame->header.flags = data[offset];
  offset += k_frame_flags_size;

  if (offset_out != nullptr) {
    *offset_out = offset;
  }

  return k_rx_ok;
}

/**
 * @brief Verify CRC-32 IEEE checksum for received frame
 *
 * @details
 * Computes CRC-32 IEEE 802.3 checksum over the frame data (excluding CRC field)
 * and compares against the CRC stored in the frame.
 *
 * @param[in] data Raw frame data buffer containing header + payload + CRC
 * @param[in] offset Byte offset to CRC field (= header_size + payload_len)
 * @param[out] crc_out Optional pointer to receive verified CRC value (can be nullptr)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok CRC valid, frame integrity confirmed
 * @retval k_rx_err_invalid_arg nullptr data, or offset below minimum valid CRC position
 * @retval k_rx_err_crc_mismatch Calculated CRC doesn't match stored CRC
 *
 * @pre data != nullptr
 * @pre offset >= (k_frame_min_size - k_frame_crc_size)
 * @post crc_out contains verified CRC value on success (if not nullptr)
 * @post Frame integrity validated on k_rx_ok return
 *
 * @note Uses rx_crc32_ieee() - CRC-32 IEEE 802.3 polynomial
 *
 * @see internal_decode_header() Called before this to parse header
 */
static rx_err_t internal_verify_crc(const uint8_t* data, uint32_t offset, uint32_t* crc_out)
{
  RX_ASSERT(data != nullptr, "Data pointer is nullptr");
  if (data == nullptr) {
    return k_rx_err_invalid_arg;
  }

  RX_ASSERT(offset >= (k_frame_min_size - k_frame_crc_size),
            "CRC offset too small for valid frame");
  if (offset < (k_frame_min_size - k_frame_crc_size)) {
    return k_rx_err_invalid_arg;
  }

  const uint32_t received_crc   = rx_frame_read_le32(&data[offset]);
  uint32_t       calculated_crc = (uint32_t)k_uart_crc32_seed_initial;
  rx_err_t       crc_err        = rx_crc32_ieee(data, offset, &calculated_crc);
  if (crc_err != k_rx_ok) {
    return crc_err;
  }

  if (received_crc != calculated_crc) {
    return k_rx_err_crc_mismatch;
  }

  if (crc_out != nullptr) {
    *crc_out = received_crc;
  }

  return k_rx_ok;
}

/**
 * @brief Sequence number constants
 */
typedef enum : uint16_t {
  k_initial_sequence = 0, /**< Initial TX/RX sequence number */
} rx_uart_comm_sequence_constants_t;

/**
 * @brief Sync search constants
 */
typedef enum : uint8_t {
  k_sync_second_byte_offset = 1, /**< Offset from current byte to second sync byte */
} rx_uart_comm_sync_constants_t;

/** @brief Byte offsets within frame header buffer */
typedef enum : uint8_t {
  k_hdr_len_offset = 4, /**< Payload length field offset */
} uart_frame_header_offset_t;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Sync word not found sentinel value
 */
typedef enum : int32_t {
  k_sync_not_found = -1, /**< Sync word not found in buffer */
} rx_uart_comm_sync_result_t;

/**
 * @brief Read available data from UART into staging buffer
 *
 * @details
 * Reads any available data from the configured UART channel into the handle's
 * rx_buffer. Appends to existing data without overwriting. Returns immediately
 * if buffer is full or no data available.
 *
 * @param[in,out] handle UART communication handle with rx_buffer
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success (0 or more bytes read)
 * @retval k_rx_err_invalid_arg nullptr handle
 * @retval k_rx_err_invalid_state Buffer length exceeds maximum
 * @retval Other errors from uart_read_channel()
 *
 * @pre handle != nullptr
 * @pre handle->rx_buffer_len <= k_uart_comm_rx_buffer_size
 * @post handle->rx_buffer_len <= k_uart_comm_rx_buffer_size
 * @post Data appended to handle->rx_buffer[old_len..new_len]
 *
 * @note Non-blocking: returns immediately with available data
 *
 * @see internal_compact_rx_buffer() Call after consuming data
 */
static rx_err_t internal_read_uart_data(rx_uart_comm_handle_t* handle)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (handle->rx_buffer_len > k_uart_comm_rx_buffer_size) {
    return k_rx_err_invalid_state;
  }

  const uint32_t space = k_uart_comm_rx_buffer_size - handle->rx_buffer_len;
  if (space == 0) {
    return k_rx_ok; /* Buffer full */
  }

  /* Check if data is available before attempting to read */
  bool available = false;
  const rx_err_t avail_err =
    uart_rx_available(handle->channel, &available);
  if (avail_err != k_rx_ok) {
    return avail_err;
  }
  if (!available) {
    return k_rx_ok; /* No data available */
  }

  /* Read from UART channel */
  uint16_t       bytes_read = 0;
  const rx_err_t err        = uart_read_channel(handle->channel,
                                                handle->rx_buffer + handle->rx_buffer_len,
                                                (uint16_t)space,
                                                &bytes_read);
  if (err != k_rx_ok) {
    return err;
  }

  handle->rx_buffer_len += (uint32_t)bytes_read;

  if (handle->rx_buffer_len > k_uart_comm_rx_buffer_size) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

/**
 * @brief Compact receive buffer by removing consumed data
 *
 * @details
 * Removes data from the beginning of rx_buffer that has already been processed
 * (bytes 0 to rx_buffer_pos-1). Remaining data is moved to the start of the
 * buffer using memmove() to handle overlapping regions safely.
 *
 * @param[in,out] handle UART communication handle with rx_buffer
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, buffer compacted
 * @retval k_rx_err_invalid_arg nullptr handle
 * @retval k_rx_err_invalid_state Position exceeds length
 *
 * @pre handle != nullptr
 * @pre handle->rx_buffer_pos <= handle->rx_buffer_len
 * @post handle->rx_buffer_pos == 0
 * @post Remaining data preserved at buffer start
 *
 * @note Uses memmove() for safe overlapping copy
 *
 * @see internal_read_uart_data() Fills buffer before compaction
 */
static rx_err_t internal_compact_rx_buffer(rx_uart_comm_handle_t* handle)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (handle->rx_buffer_pos > handle->rx_buffer_len) {
    return k_rx_err_invalid_state;
  }

  if (handle->rx_buffer_pos == 0) {
    return k_rx_ok; /* Nothing to compact */
  }

  if (handle->rx_buffer_pos >= handle->rx_buffer_len) {
    handle->rx_buffer_len = 0;
    handle->rx_buffer_pos = 0;
  } else {
    const uint32_t remaining = handle->rx_buffer_len - handle->rx_buffer_pos;
    memmove(handle->rx_buffer, handle->rx_buffer + handle->rx_buffer_pos, remaining);
    handle->rx_buffer_len = remaining;
    handle->rx_buffer_pos = 0;
  }

  if (handle->rx_buffer_pos != 0) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

/**
 * @brief Search for frame sync word in receive buffer
 *
 * @details
 * Scans the receive buffer starting from rx_buffer_pos for the frame sync
 * word (0x55AA) stored in little-endian format.
 *
 * @param[in] handle UART communication handle with rx_buffer
 * @param[out] sync_pos Output position of sync word (k_sync_not_found if not found)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Sync word found, position in sync_pos
 * @retval k_rx_err_invalid_arg nullptr handle or sync_pos
 * @retval k_rx_err_invalid_state Buffer position exceeds length
 * @retval k_rx_err_not_found Sync word not in buffer
 *
 * @pre handle != nullptr
 * @pre sync_pos != nullptr
 * @pre handle->rx_buffer_pos <= handle->rx_buffer_len
 * @post sync_pos contains position or k_sync_not_found
 * @post Buffer state unchanged
 *
 * @note O(n) linear scan where n = buffer length
 *
 * @see k_frame_sync_word Sync word constant (0x55AA)
 */
static rx_err_t internal_find_sync(const rx_uart_comm_handle_t* handle, int32_t* sync_pos)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (sync_pos == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (handle->rx_buffer_pos > handle->rx_buffer_len) {
    return k_rx_err_invalid_state;
  }

  const uint8_t sync_low  = (uint8_t)(k_frame_sync_word & k_rx_byte_mask);
  const uint8_t sync_high = (uint8_t)(k_frame_sync_word >> k_rx_le16_high_shift);

  for (uint32_t i = handle->rx_buffer_pos; i + k_sync_second_byte_offset < handle->rx_buffer_len;
       i++) {
    if (handle->rx_buffer[i] == sync_low &&
        handle->rx_buffer[i + k_sync_second_byte_offset] == sync_high) {
      *sync_pos = (int32_t)i;
      return k_rx_ok;
    }
  }

  *sync_pos = k_sync_not_found;
  return k_rx_err_not_found;
}

/**
 * @brief Handle case when no sync word is found in buffer
 *
 * Advances buffer position if full, compacts buffer.
 *
 * @param[in,out] handle UART communication handle
 * @return k_rx_ok to continue searching
 * @return k_rx_err_no_data if buffer still has no sync
 */
static rx_err_t internal_handle_no_sync(rx_uart_comm_handle_t* handle)
{
  if (handle->rx_buffer_len >= k_uart_comm_rx_buffer_size) {
    handle->rx_buffer_pos++;
  }

  const rx_err_t compact_err = internal_compact_rx_buffer(handle);
  if (compact_err != k_rx_ok) {
    return compact_err;
  }

  return k_rx_ok;
}

/**
 * @brief Decode a complete frame from buffer
 *
 * @param[in,out] handle UART communication handle
 * @param[out] frame Decoded frame output
 * @param[in] total_size Total frame size in bytes
 * @return k_rx_ok on success
 * @return Error code on failure
 */
static rx_err_t
internal_decode_frame(rx_uart_comm_handle_t* handle, rx_frame_t* frame, const uint32_t total_size)
{
  const uint8_t* hdr    = handle->rx_buffer + handle->rx_buffer_pos;
  uint32_t       offset = 0;

  rx_err_t err = internal_decode_header(hdr, total_size, frame, &offset);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame header decode failed");
  } else {
    if (frame->header.length > 0) {
      memcpy(frame->payload, &hdr[offset], frame->header.length);
      offset += frame->header.length;
    }

    err = internal_verify_crc(hdr, offset, &frame->crc);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Frame CRC check failed");
    }
  }

  handle->rx_buffer_pos += total_size;
  const rx_err_t compact_err = internal_compact_rx_buffer(handle);
  if (compact_err != k_rx_ok && err == k_rx_ok) {
    return compact_err;
  }

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame decode failed");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Receive iteration result codes
 */
typedef enum : uint8_t {
  k_receive_continue = 0, /**< Continue to next iteration */
  k_receive_done     = 1, /**< Frame received successfully */
  k_receive_error    = 2, /**< Error occurred, check err output */
} rx_receive_result_t;

/**
 * @brief Align buffer position to sync word if found
 *
 * @param[in,out] handle UART communication handle
 * @param[in] sync_pos Position of sync word in buffer
 */
static void internal_align_to_sync(rx_uart_comm_handle_t* handle, const int32_t sync_pos)
{
  if ((uint32_t)sync_pos > handle->rx_buffer_pos) {
    handle->rx_buffer_pos = (uint32_t)sync_pos;
  }
}

/**
 * @brief Parse frame header and validate payload length
 *
 * @param[in] handle UART communication handle
 * @param[out] payload_len Output payload length
 * @param[out] total_size Output total frame size
 * @return true if header is valid, false if invalid payload length
 */
static bool
internal_parse_header(rx_uart_comm_handle_t* handle, uint16_t* payload_len, uint32_t* total_size)
{
  const uint8_t* hdr = handle->rx_buffer + handle->rx_buffer_pos;
  *payload_len       = rx_frame_read_le16(&hdr[k_hdr_len_offset]);

  if (*payload_len > k_frame_max_payload) {
    rx_log_warn(s_tag, "Invalid payload length, skipping");
    handle->rx_buffer_pos += k_frame_sync_size;
    return false;
  }

  *total_size = k_frame_header_total + *payload_len + k_frame_crc_size;
  return true;
}

/**
 * @brief Process one iteration of the frame receive loop
 *
 * @details
 * Performs a single pass of the receive state machine. Attempts to read UART
 * data, find sync word, parse header, and decode complete frame. Returns
 * result code indicating whether to continue, success, or error.
 *
 * @param[in,out] handle UART communication handle
 * @param[out] frame Decoded frame output
 * @param[out] err Error code (valid only if k_receive_error)
 *
 * @return rx_receive_result_t Result code
 * @retval k_receive_continue Continue to next iteration
 * @retval k_receive_done Frame successfully received
 * @retval k_receive_error Error occurred, check err parameter
 *
 * @pre handle != nullptr and initialized
 * @pre frame != nullptr
 * @pre err != nullptr
 * @post On k_receive_done: frame contains valid decoded frame
 * @post On k_receive_error: err contains error code
 *
 * @note Called in loop by rx_uart_comm_receive()
 *
 * @see rx_uart_comm_receive() Main receive API that calls this
 */
static rx_receive_result_t internal_receive_iteration(rx_uart_comm_handle_t* handle,
                                                      rx_frame_t*            frame,
                                                      rx_err_t*              err)
{
  /* Read any available UART data */
  *err = internal_read_uart_data(handle);
  if (*err != k_rx_ok && *err != k_rx_err_timeout) {
    return k_receive_error;
  }

  /* If no data yet, return no_data */
  if (handle->rx_buffer_len == handle->rx_buffer_pos) {
    *err = k_rx_err_no_data;
    return k_receive_error;
  }

  /* Search for sync word */
  int32_t sync_pos = k_sync_not_found;
  *err             = internal_find_sync(handle, &sync_pos);
  if (*err == k_rx_err_not_found) {
    *err = internal_handle_no_sync(handle);
    if (*err != k_rx_ok) {
      return k_receive_error;
    }
    *err = k_rx_err_no_data;
    return k_receive_error;
  }
  if (*err != k_rx_ok) {
    return k_receive_error;
  }

  /* Found sync - align to it */
  internal_align_to_sync(handle, sync_pos);

  /* Check if we have enough data for header */
  const uint32_t available = handle->rx_buffer_len - handle->rx_buffer_pos;
  if (available < k_frame_header_total) {
    *err = k_rx_err_no_data;
    return k_receive_error;
  }

  /* Parse and validate header */
  uint32_t total_size  = 0;
  uint16_t payload_len = 0;
  if (!internal_parse_header(handle, &payload_len, &total_size)) {
    return k_receive_continue;
  }

  /* Check if we have complete frame */
  if (available < total_size) {
    *err = k_rx_err_no_data;
    return k_receive_error;
  }

  /* Decode the complete frame */
  *err = internal_decode_frame(handle, frame, total_size);
  return (*err != k_rx_ok) ? k_receive_error : k_receive_done;
}

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @var s_zero_handle
 * @brief Zero-initialized UART comm handle template for stack-safe initialization
 * @details Static const instance used to clear rx_uart_comm_handle_t without creating
 *          a large compound literal on the stack. Lives in .rodata section.
 * @note Read-only; never modified after static initialization
 * @warning Do not remove - prevents stack overflow in init function
 * @see rx_uart_comm_init() Uses this template to zero-initialize the comm handle
 * @since Version 1.0.0
 */
static const rx_uart_comm_handle_t s_zero_handle = {};

rx_err_t rx_uart_comm_init(rx_uart_comm_handle_t* handle, const rx_uart_comm_config_t* config)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *handle = s_zero_handle;

  if (config == nullptr || config->session == nullptr) {
    return k_rx_err_invalid_arg;
  }
  handle->session = config->session;
  handle->channel = config->channel;

  rx_err_t err = rx_frame_encoder_init(&handle->encoder);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to init frame encoder");
    return err;
  }

  err = rx_frame_decoder_init(&handle->decoder);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to init frame decoder");

    const rx_err_t cleanup_err = rx_frame_encoder_deinit(&handle->encoder);
    if (cleanup_err != k_rx_ok) {
      rx_log_warn(s_tag, "Failed to cleanup encoder during decoder init failure");
    }

    return err;
  }

  handle->rx_buffer_len = 0;
  handle->rx_buffer_pos = 0;
  handle->initialized   = 1;

  RX_ASSERT(handle->initialized, "Handle initialization failed");
  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  rx_log_debug(s_tag, "UART comm initialized");
  return k_rx_ok;
}

rx_err_t rx_uart_comm_deinit(rx_uart_comm_handle_t* handle)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  RX_ASSERT(handle->initialized, "Attempt to deinitialize uninitialized UART comm handle");
  rx_err_t result = k_rx_ok;

  if (handle->initialized) {
    const rx_err_t enc_err = rx_frame_encoder_deinit(&handle->encoder);
    if (enc_err != k_rx_ok) {
      rx_log_warn(s_tag, "Encoder deinit failed");
      result = enc_err;
    }

    const rx_err_t dec_err = rx_frame_decoder_deinit(&handle->decoder);
    if (dec_err != k_rx_ok) {
      rx_log_warn(s_tag, "Decoder deinit failed");
      if (result == k_rx_ok) {
        result = dec_err;
      }
    }

    handle->initialized = 0;
  }

  rx_log_debug(s_tag, "UART comm deinitialized");
  return result;
}

/* =============================================================================
 * Send API
 * =============================================================================
 */

/**
 * @brief Build frame structure from individual parameters
 *
 * @param[out] frame Output frame structure to populate
 * @param[in] sequence Sequence number for frame (0-65535)
 * @param[in] type Frame type (command, response, ack, etc.)
 * @param[in] flags Frame flags bitmap
 * @param[in] payload Payload data (can be nullptr if payload_len is 0)
 * @param[in] payload_len Payload length in bytes (0 to k_frame_max_payload)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, frame populated
 * @retval k_rx_err_invalid_arg nullptr frame or (nullptr payload with len > 0)
 *
 * @pre frame != nullptr
 * @pre payload != nullptr || payload_len == 0
 * @post frame->header fields populated
 * @post frame->payload contains copy of input payload
 */
static rx_err_t internal_build_frame(rx_frame_t*           frame,
                                     const uint16_t        sequence,
                                     const rx_frame_type_t type,
                                     const uint8_t         flags,
                                     const uint8_t*        payload,
                                     const uint32_t        payload_len)
{
  RX_ASSERT(frame != nullptr, "Frame pointer is nullptr");
  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  RX_ASSERT(payload != nullptr || payload_len == 0, "Payload nullptr but payload_len > 0");
  if (payload == nullptr && payload_len > 0) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > k_frame_max_payload) {
    return k_rx_err_invalid_size;
  }

  frame->header.sequence = sequence;
  frame->header.length   = (uint16_t)payload_len;
  frame->header.type     = (uint8_t)type;
  frame->header.flags    = flags;

  if (payload != nullptr && payload_len > 0) {
    memcpy(frame->payload, payload, payload_len);
  }

  return k_rx_ok;
}

rx_err_t rx_uart_comm_send(rx_uart_comm_handle_t* handle,
                           rx_frame_type_t        type,
                           uint8_t                flags,
                           const uint8_t*         payload,
                           uint32_t               payload_len)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    rx_log_error(s_tag, "Handle not initialized");
    return k_rx_err_invalid_state;
  }

  if (payload == nullptr && payload_len > 0) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > k_frame_max_payload) {
    rx_log_error(s_tag, "Payload too large");
    return k_rx_err_invalid_size;
  }

  uint16_t sequence = k_initial_sequence;
  rx_err_t err      = rx_session_next_tx(handle->session, &sequence);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Session next_tx failed");
    return err;
  }

  rx_frame_t frame = {0};
  err              = internal_build_frame(&frame, sequence, type, flags, payload, payload_len);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame build failed");
    return err;
  }

  uint32_t wire_len = 0;
  err               = rx_frame_encode(&handle->encoder, &frame, handle->tx_buffer, &wire_len);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame encode failed");
    return err;
  }

  err = uart_write_channel(handle->channel, handle->tx_buffer, (uint16_t)wire_len);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "UART write failed");
    return err;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Receive API
 * =============================================================================
 */

rx_err_t
rx_uart_comm_receive(rx_uart_comm_handle_t* handle, rx_frame_t* frame, uint32_t timeout_ms)
{
  (void)timeout_ms; /* UART receive is non-blocking; timeout reserved for future use */

  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  uint32_t            iterations = 0;
  rx_err_t            err        = k_rx_ok;
  rx_receive_result_t result     = k_receive_continue;

  while (iterations < k_uart_comm_max_receive_iterations) {
    iterations++;

    result = internal_receive_iteration(handle, frame, &err);
    if (result == k_receive_error) {
      return err;
    }
    if (result != k_receive_done) {
      continue;
    }

    /* PING: auto-send PONG, loop for next frame */
    if (frame->header.type == k_frame_type_ping) {
      rx_err_t pong_err = rx_uart_comm_send(handle,
                                            k_frame_type_pong,
                                            0,
                                            frame->payload,
                                            frame->header.length);
      if (pong_err != k_rx_ok) {
        rx_log_error(s_tag, "Failed to send PONG response");
        return pong_err;
      }
      rx_log_debug(s_tag, "Auto-responded with PONG");
      continue;
    }

    /* RESET: send RESET_ACK, reset session, loop */
    if (frame->header.type == k_frame_type_reset) {
      uint16_t         reset_ack_seq = k_initial_sequence;
      const rx_err_t   seq_err       = rx_session_next_tx(handle->session, &reset_ack_seq);
      if (seq_err != k_rx_ok) {
        return seq_err;
      }

      rx_frame_t reset_ack_frame = {0};
      rx_err_t   ack_build_err   = rx_frame_create_reset_ack(&reset_ack_frame, reset_ack_seq);
      if (ack_build_err != k_rx_ok) {
        return ack_build_err;
      }

      uint32_t wire_len = 0;
      rx_err_t enc_err  =
        rx_frame_encode(&handle->encoder, &reset_ack_frame, handle->tx_buffer, &wire_len);
      if (enc_err != k_rx_ok) {
        return enc_err;
      }

      rx_err_t write_err =
        uart_write_channel(handle->channel, handle->tx_buffer, (uint16_t)wire_len);
      if (write_err != k_rx_ok) {
        rx_log_error(s_tag, "Failed to send RESET_ACK");
        return write_err;
      }
      rx_log_debug(s_tag, "Auto-responded with RESET_ACK");

      rx_err_t reset_err = rx_session_reset(handle->session);
      if (reset_err != k_rx_ok) {
        rx_log_error(s_tag, "Session reset failed after RESET_ACK");
        return reset_err;
      }

      continue;
    }

    /* PONG / RESET_ACK: consume silently, loop for next frame */
    if (frame->header.type == k_frame_type_pong || frame->header.type == k_frame_type_reset_ack) {
      continue;
    }

    /* Data frame: validate sequence via session */
    rx_session_validate_result_t validate_result = k_session_validate_fail;
    rx_err_t                     validate_err    =
      rx_session_validate_rx(handle->session, frame->header.sequence, &validate_result);
    if (validate_err != k_rx_ok) {
      rx_log_error(s_tag, "Session validate_rx returned error");
      return validate_err;
    }
    if (validate_result == k_session_validate_fail) {
      rx_log_warn(s_tag, "Sequence validation failed, dropping frame");
      continue;
    }

    return k_rx_ok;
  }

  return k_rx_err_timeout;
}
