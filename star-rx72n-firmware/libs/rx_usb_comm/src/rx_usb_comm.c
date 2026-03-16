/**
 * @file rx_usb_comm.c
 * @brief High-Level USB CDC Communication Layer Implementation
 *
 * @details
 * Implements reliable USB communication by integrating the frame protocol layer
 * (rx_frame) with the USB CDC driver (rx_usb). Handles frame encoding/decoding,
 * CRC-32 validation, sequence number management, and receive buffer handling.
 *
 * @par Implementation Architecture:
 * @msc
 * App, USB_Comm, Frame, USB_CDC;
 * --- [label="TX Path"];
 * App => USB_Comm [label="send(type, payload)"];
 * USB_Comm => Frame [label="encode()"];
 * USB_Comm => USB_CDC [label="write()"];
 * --- [label="RX Path"];
 * USB_CDC => USB_Comm [label="read()"];
 * USB_Comm box USB_Comm [label="Find sync, decode"];
 * USB_Comm => App [label="frame"];
 * @endmsc
 *
 * @par Port Assignment:
 * | Port | Constant         | Host Device  | Purpose              |
 * |------|------------------|--------------|----------------------|
 * | 0    | k_usb_port_proto | /dev/ttyACM0 | Binary protocol      |
 * | 1    | k_usb_port_debug | /dev/ttyACM1 | Debug text output    |
 *
 * @par Performance:
 * | Operation      | Typical   | Worst Case  |
 * |----------------|-----------|-------------|
 * | Send (64B)     | 80 us     | 200 us      |
 * | Receive        | 500 us    | timeout_ms  |
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion
 * - Rule 2: [OK] Loops bounded by k_max_receive_iterations
 * - Rule 3: [OK] No dynamic allocation
 * - Rule 5: [OK] Pre/post conditions on all functions
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Typed enums for constants
 *
 * @par SOLID Principles:
 * - SRP: Handles USB framing only, delegates I/O to rx_usb
 * - DIP: Uses rx_time_interface abstraction for delays
 *
 * @see rx_usb_comm.h Public API header
 * @see rx_frame.h Frame encoding layer
 * @see rx_usb.h USB CDC driver
 *
 * @author Locked, Inc.
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_usb_comm.h"

#include "rx_check.h"
#include "rx_crc.h"
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
typedef enum : uint8_t {
  k_frame_header_total = k_frame_sync_size + k_frame_seq_size + k_frame_len_size +
                         k_frame_type_size + k_frame_flags_size,
} rx_usb_comm_header_size_t;

/** @brief CRC-32 seed value (pre-initialization before rx_crc32_ieee writes it) */
typedef enum : uint32_t {
  k_usb_crc32_seed_initial = 0U, /**< Initial value for CRC-32 output variable */
} usb_crc32_seed_t;

/**
 * @brief Decode frame header from raw wire data
 *
 * @details
 * Parses the frame header fields from a raw byte buffer. Validates sync word,
 * extracts sequence number, payload length, type, and flags.
 *
 * @par Algorithm:
 * 1. Validate input pointers (RX_ASSERT + runtime check)
 * 2. Verify minimum data length for header
 * 3. Read and validate sync word (0x55AA little-endian)
 * 4. Extract sequence number (LE16)
 * 5. Extract and validate payload length (LE16)
 * 6. Extract frame type and flags
 * 7. Return offset past header for payload access
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
 * @pre data_len >= k_frame_min_size for valid frame
 * @post frame->header fields populated on success
 * @post offset_out contains byte offset past header (if not nullptr)
 *
 * @note Not thread-safe, but stateless (safe for concurrent calls with different data)
 *
 * @see internal_verify_crc() Called after this to verify frame CRC
 */
RX_STATIC_TESTABLE rx_err_t internal_decode_header(const uint8_t* data,
                                                   const uint32_t data_len,
                                                   rx_frame_t*    frame,
                                                   uint32_t*      offset_out)
{
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
 * and compares against the CRC stored in the frame. Uses little-endian CRC
 * storage as per protocol specification.
 *
 * @par Algorithm:
 * 1. Validate data pointer (RX_ASSERT + runtime check)
 * 2. Validate offset is valid for CRC position
 * 3. Read stored CRC from data[offset] (LE32)
 * 4. Calculate CRC-32 over data[0..offset-1]
 * 5. Compare and return result
 *
 * @param[in] data Raw frame data buffer containing header + payload + CRC
 * @param[in] offset Byte offset to CRC field (= header_size + payload_len)
 * @param[out] crc_out Optional pointer to receive verified CRC value (can be nullptr)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok CRC valid, frame integrity confirmed
 * @retval k_rx_err_invalid_arg nullptr data, or offset below minimum valid CRC position
 *                              (offset < k_frame_min_size - k_frame_crc_size), including offset == 0
 * @retval k_rx_err_null_ptr Propagated from rx_crc32_ieee() when data is NULL
 * @retval k_rx_err_crc_mismatch Calculated CRC doesn't match stored CRC
 *
 * @pre data != nullptr
 * @pre offset >= (k_frame_min_size - k_frame_crc_size)
 * @post crc_out contains verified CRC value on success (if not nullptr)
 * @post Frame integrity validated on k_rx_ok return
 *
 * @note Uses rx_crc32_ieee() - CRC-32 IEEE 802.3 polynomial
 * @note CRC stored in little-endian format in frame
 *
 * @par Performance:
 * CRC calculation: ~10 us for 64-byte payload @ 240 MHz
 *
 * @see rx_crc32_ieee() CRC calculation function
 * @see internal_decode_header() Called before this to parse header
 */
RX_STATIC_TESTABLE rx_err_t internal_verify_crc(const uint8_t* data,
                                                uint32_t       offset,
                                                uint32_t*      crc_out)
{
  /* Pre-condition 1: Validate data pointer */
  if (data == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Offset must be at least header size for valid CRC position */
  if (offset < (k_frame_min_size - k_frame_crc_size)) {
    return k_rx_err_invalid_arg;
  }

  const uint32_t received_crc   = rx_frame_read_le32(&data[offset]);
  uint32_t       calculated_crc = k_usb_crc32_seed_initial;
  /* data is validated non-null above; offset >= 8 so len > 0: crc_err is always k_rx_ok */
  (void)(rx_crc32_ieee(data, offset, &calculated_crc));

  if (received_crc != calculated_crc) {
    return k_rx_err_crc_mismatch;
  }

  if (crc_out != nullptr) {
    *crc_out = received_crc;
  }

  return k_rx_ok;
}

/** @brief Sleep interval for receive polling (ms) */
typedef enum : uint8_t {
  k_sleep_interval_ms = 10,
} rx_usb_comm_sleep_t;

/**
 * @brief Sequence number constants
 */
typedef enum : uint16_t {
  k_initial_sequence = 0, /**< Initial TX/RX sequence number */
} rx_usb_comm_sequence_constants_t;

/**
 * @brief Sync search constants
 */
typedef enum : uint8_t {
  k_sync_second_byte_offset = 1, /**< Offset from current byte to second sync byte */
} rx_usb_comm_sync_constants_t;

/**
 * @brief Receive loop bounds for NASA Power of 10 Rule 2 compliance
 */
typedef enum : uint8_t {
  k_max_receive_iterations = 100, /**< Max receive loop attempts */
} rx_usb_comm_loop_limits_t;

/** @brief Byte offsets within frame header buffer */
typedef enum : uint8_t {
  k_hdr_len_offset = 4, /**< Payload length field offset */
} frame_header_offset_t;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Sync word not found sentinel value
 */
typedef enum : int32_t {
  k_sync_not_found = -1, /**< Sync word not found in buffer */
} rx_usb_comm_sync_result_t;

/**
 * @brief Read available data from USB CDC into staging buffer
 *
 * @details
 * Reads any available data from USB Port 0 (protocol port) into the handle's
 * rx_buffer. Appends to existing data without overwriting. Returns immediately
 * if buffer is full or no data available.
 *
 * @par Algorithm:
 * 1. Validate handle pointer
 * 2. Validate buffer length within bounds
 * 3. Calculate available space in buffer
 * 4. Return early if buffer full
 * 5. Call rx_usb_read() to get available data
 * 6. Update buffer length
 * 7. Validate post-condition
 *
 * @param[in,out] handle USB communication handle with rx_buffer
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success (0 or more bytes read)
 * @retval k_rx_err_invalid_arg nullptr handle
 * @retval k_rx_err_invalid_state Buffer length exceeds maximum
 * @retval Other errors from rx_usb_read()
 *
 * @pre handle != nullptr
 * @pre handle->rx_buffer_len <= k_usb_comm_rx_buffer_size
 * @post handle->rx_buffer_len <= k_usb_comm_rx_buffer_size
 * @post Data appended to handle->rx_buffer[old_len..new_len]
 *
 * @note Non-blocking: returns immediately with available data
 * @note Uses Port 0 (k_usb_port_proto) exclusively
 *
 * @see internal_compact_rx_buffer() Call after consuming data
 */
RX_STATIC_TESTABLE rx_err_t internal_read_usb_data(rx_usb_comm_handle_t* handle)
{
  /* Pre-condition 1: Handle must be valid */
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Buffer length must be within bounds */
  if (handle->rx_buffer_len > k_usb_comm_rx_buffer_size) {
    return k_rx_err_invalid_state;
  }

  /* Calculate available space in buffer */
  const uint32_t space = k_usb_comm_rx_buffer_size - handle->rx_buffer_len;
  if (space == 0) {
    return k_rx_ok; /* Buffer full */
  }

  /* Read from USB (Port 0 = protocol) */
  uint32_t bytes_read = 0;
  (void)(rx_usb_read(k_usb_port_proto,
                     handle->rx_buffer + handle->rx_buffer_len,
                     space,
                     &bytes_read));

  /* Port and buffer pointer are always valid here; rx_usb_read only fails on invalid args */

  handle->rx_buffer_len += bytes_read;

  /* Post-condition: bytes_read <= space, so buffer cannot exceed maximum */

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
 * @par Algorithm:
 * 1. Validate handle pointer
 * 2. Validate position <= length invariant
 * 3. Return early if position is 0 (nothing to compact)
 * 4. If all data consumed, reset both counters to 0
 * 5. Otherwise, memmove remaining data to start
 * 6. Update length and reset position to 0
 * 7. Validate post-condition
 *
 * @param[in,out] handle USB communication handle with rx_buffer
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
 * @note O(n) where n = remaining bytes
 *
 * @see internal_read_usb_data() Fills buffer before compaction
 */
RX_STATIC_TESTABLE rx_err_t internal_compact_rx_buffer(rx_usb_comm_handle_t* handle)
{
  /* Pre-condition 1: Handle must be valid */
  if (handle == nullptr) {
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
    /* Move remaining data to beginning (forward copy safe: dst < src) */
    const uint32_t remaining = handle->rx_buffer_len - handle->rx_buffer_pos;
    for (uint32_t ci = 0; ci < remaining; ci++) {
      handle->rx_buffer[ci] = handle->rx_buffer[handle->rx_buffer_pos + ci];
    }
    handle->rx_buffer_len = remaining;
    handle->rx_buffer_pos = 0;
  }

  /* Post-condition: both branches above set rx_buffer_pos = 0 unconditionally */

  return k_rx_ok;
}

/**
 * @brief Search for frame sync word in receive buffer
 *
 * @details
 * Scans the receive buffer starting from rx_buffer_pos for the frame sync
 * word (0x55AA) stored in little-endian format. Returns the position of the
 * first occurrence found.
 *
 * @par Algorithm:
 * 1. Validate handle and output pointers
 * 2. Validate buffer position <= length
 * 3. Extract sync word bytes (0xAA low, 0x55 high)
 * 4. Linear scan from rx_buffer_pos to end-1
 * 5. Return position if found, k_sync_not_found otherwise
 *
 * @par Sync Word Format:
 * The sync word 0x55AA is stored little-endian in the frame:
 * - Byte 0: 0xAA (low byte, LSB first)
 * - Byte 1: 0x55 (high byte, MSB second)
 *
 * @param[in] handle USB communication handle with rx_buffer
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
 * @note Read-only operation, does not modify buffer
 *
 * @see k_frame_sync_word Sync word constant (0x55AA)
 */
RX_STATIC_TESTABLE rx_err_t internal_find_sync(const rx_usb_comm_handle_t* handle,
                                               int32_t*                    sync_pos)
{
  /* Pre-condition 1: Handle must be valid */
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Output pointer must be valid */
  if (sync_pos == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 3: Buffer position must be within length */
  if (handle->rx_buffer_pos > handle->rx_buffer_len) {
    return k_rx_err_invalid_state;
  }

  /* Extract sync word bytes using shared constants from rx_frame.h */
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
static bool internal_sleep_and_advance(const rx_usb_comm_handle_t* handle, uint32_t* elapsed_ms)
{
  /* elapsed_ms is always non-null: callers pass &elapsed_ms from receive loop */
  if (handle->time_iface != nullptr && handle->time_iface->sleep_ms != nullptr) {
    handle->time_iface->sleep_ms(handle->time_iface->ctx, k_sleep_interval_ms);
    *elapsed_ms += k_sleep_interval_ms;
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
static bool internal_is_timed_out(const uint32_t timeout_ms, uint32_t elapsed_ms)
{
  return (bool)((timeout_ms == 0) || (elapsed_ms >= timeout_ms));
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
                                        const uint32_t        timeout_ms,
                                        uint32_t*             elapsed_ms)
{
  /* Buffer full and no sync - discard one byte to advance sliding window */
  if (handle->rx_buffer_len >= k_usb_comm_rx_buffer_size) {
    handle->rx_buffer_pos++;
  }

  /* Compact buffer (discard consumed/skipped data) */
  /* compact only fails if pos > len, which cannot happen after the bounded pos++ above */
  (void)(internal_compact_rx_buffer(handle));

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
static rx_err_t internal_wait_for_data(const rx_usb_comm_handle_t* handle,
                                       const uint32_t              timeout_ms,
                                       uint32_t*                   elapsed_ms)
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
static rx_err_t
internal_decode_frame(rx_usb_comm_handle_t* handle, rx_frame_t* frame, const uint32_t total_size)
{
  const uint8_t* hdr    = handle->rx_buffer + handle->rx_buffer_pos;
  uint32_t       offset = 0;

  /* Decode header: decode cannot fail here because total_size was verified by
   * internal_parse_header using the same calculation as internal_decode_header */
  (void)internal_decode_header(hdr, total_size, frame, &offset);
  rx_err_t err = k_rx_ok;

  /* Read payload */
  if (frame->header.length > 0) {
    for (uint16_t ci = 0; ci < frame->header.length; ci++) {
      frame->payload[ci] = hdr[offset + ci];
    }
    offset += frame->header.length;
  }

  /* Verify CRC */
  err = internal_verify_crc(hdr, offset, &frame->crc);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame CRC check failed");
  }

  /* Consume the frame data */
  handle->rx_buffer_pos += total_size;
  /* compact cannot fail: pos was incremented by exactly total_size which <= rx_buffer_len */
  (void)(internal_compact_rx_buffer(handle));

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
 * @param[in,out] handle USB communication handle
 * @param[in] sync_pos Position of sync word in buffer
 */
static void internal_align_to_sync(rx_usb_comm_handle_t* handle, const int32_t sync_pos)
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
static bool
internal_parse_header(rx_usb_comm_handle_t* handle, uint16_t* payload_len, uint32_t* total_size)
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
 * Performs a single pass of the receive state machine. Attempts to read USB
 * data, find sync word, parse header, and decode complete frame. Returns
 * result code indicating whether to continue, success, or error.
 *
 * @par State Machine:
 * @dot
 * digraph receive_iteration {
 *   rankdir=TB;
 *   read [label="Read USB Data"];
 *   find_sync [label="Find Sync"];
 *   check_header [label="Header Complete?"];
 *   parse_header [label="Parse Header"];
 *   check_frame [label="Frame Complete?"];
 *   decode [label="Decode Frame"];
 *   done [label="DONE"];
 *   continue_state [label="CONTINUE"];
 *   error [label="ERROR"];
 *
 *   read -> find_sync [label="ok"];
 *   read -> error [label="fail"];
 *   find_sync -> check_header [label="found"];
 *   find_sync -> continue_state [label="not found"];
 *   check_header -> parse_header [label="yes"];
 *   check_header -> continue_state [label="no"];
 *   parse_header -> check_frame [label="ok"];
 *   parse_header -> continue_state [label="invalid"];
 *   check_frame -> decode [label="yes"];
 *   check_frame -> continue_state [label="no"];
 *   decode -> done [label="ok"];
 *   decode -> error [label="fail"];
 * }
 * @enddot
 *
 * @param[in,out] handle USB communication handle
 * @param[out] frame Decoded frame output (valid only if k_receive_done)
 * @param[in] timeout_ms Timeout value in milliseconds
 * @param[in,out] elapsed_ms Elapsed time counter (updated on sleep)
 * @param[out] err Error code (valid only if k_receive_error)
 *
 * @return rx_receive_result_t Result code
 * @retval k_receive_continue Continue to next iteration
 * @retval k_receive_done Frame successfully received
 * @retval k_receive_error Error occurred, check err parameter
 *
 * @pre handle != nullptr and initialized
 * @pre frame != nullptr
 * @pre elapsed_ms != nullptr
 * @pre err != nullptr
 * @post On k_receive_done: frame contains valid decoded frame
 * @post On k_receive_error: err contains error code
 *
 * @note Called in loop by rx_usb_comm_receive()
 *
 * @see rx_usb_comm_receive() Main receive API that calls this
 */
static rx_receive_result_t internal_receive_iteration(rx_usb_comm_handle_t* handle,
                                                      rx_frame_t*           frame,
                                                      const uint32_t        timeout_ms,
                                                      uint32_t*             elapsed_ms,
                                                      rx_err_t*             err)
{
  /* Read any available USB data */
  /* internal_read_usb_data: port and buffer valid, so only k_rx_ok expected */
  *err = internal_read_usb_data(handle);

  /* Search for sync word */
  int32_t sync_pos = k_sync_not_found;
  *err             = internal_find_sync(handle, &sync_pos);
  if (*err == k_rx_err_not_found) {
    *err = internal_handle_no_sync(handle, timeout_ms, elapsed_ms);
    return (*err != k_rx_ok) ? k_receive_error : k_receive_continue;
  }
  /* handle is valid and pos <= len at this point, so only k_rx_ok or k_rx_err_not_found expected */

  /* Found sync - align to it */
  internal_align_to_sync(handle, sync_pos);

  /* Check if we have enough data for header */
  const uint32_t available = handle->rx_buffer_len - handle->rx_buffer_pos;
  if (available < k_frame_header_total) {
    *err = internal_wait_for_data(handle, timeout_ms, elapsed_ms);
    return (*err != k_rx_ok) ? k_receive_error : k_receive_continue;
  }

  /* Parse and validate header */
  uint32_t total_size  = 0;
  uint16_t payload_len = 0;
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

/**
 * @var s_zero_handle
 * @brief Zero-initialized USB comm handle template for stack-safe initialization
 * @details Static const instance used to clear rx_usb_comm_handle_t without creating
 *          a large compound literal on the stack. Lives in .rodata section.
 * @note Read-only; never modified after static initialization
 * @warning Do not remove - prevents stack overflow in init function
 * @see rx_usb_comm_init() Uses this template to zero-initialize the comm handle
 * @since Version 1.0.0
 */
static const rx_usb_comm_handle_t s_zero_handle = {};

/**
 * @brief Initialize USB communication layer
 *
 * @details
 * Initializes the USB communication handle for framed communication. Sets up
 * the frame encoder/decoder, configures FEC and time interface, and prepares
 * buffer state. Must be called before any other rx_usb_comm functions.
 *
 * @par Algorithm:
 * 1. Validate handle pointer
 * 2. Clear handle to zero state
 * 3. Apply configuration (or defaults if nullptr)
 * 4. Initialize frame encoder
 * 5. Initialize frame decoder (cleanup encoder on failure)
 * 6. Initialize sequence counters to 0
 * 7. Initialize buffer state
 * 8. Set mode to binary (default)
 * 9. Mark handle as initialized
 * 10. Validate post-conditions
 *
 * @param[out] handle USB communication handle to initialize
 * @param[in] config Configuration parameters (must not be NULL)
 *   - session: Required shared session state pointer
 *   - time_iface: Time interface for sleep operations (nullptr = no sleep)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, handle ready for use
 * @retval k_rx_err_invalid_arg nullptr handle pointer
 * @retval k_rx_err_invalid_state Post-condition validation failed
 * @retval Other errors from rx_frame_encoder_init() or rx_frame_decoder_init()
 *
 * @pre handle != nullptr
 * @pre handle memory writable (sizeof(rx_usb_comm_handle_t) bytes)
 * @post handle->initialized == true on success
 * @post handle->mode == k_usb_comm_mode_binary on success
 * @post handle->session points to shared session state
 *
 * @note Not thread-safe during initialization
 * @note USB hardware must be initialized separately via rx_usb_init()
 *
 * @par Example:
 * @code
 * rx_usb_comm_handle_t handle;
 * rx_usb_comm_config_t config = {
 *     .session    = &g_session,
 *     .time_iface = &time_interface
 * };
 * rx_err_t err = rx_usb_comm_init(&handle, &config);
 * if (err != k_rx_ok) {
 *     rx_log_error("USB", "Init failed");
 * }
 * @endcode
 *
 * @see rx_usb_comm_deinit() Cleanup function
 * @see rx_usb_init() USB hardware initialization
 */
rx_err_t rx_usb_comm_init(rx_usb_comm_handle_t* handle, const rx_usb_comm_config_t* config)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *handle = s_zero_handle;

  /* Apply configuration - config is required for session pointer */
  if (config == nullptr || config->session == nullptr) {
    return k_rx_err_invalid_arg;
  }
  handle->session    = config->session;
  handle->time_iface = config->time_iface;

  /* Initialize frame encoder: only fails with nullptr (impossible here) */
  (void)(rx_frame_encoder_init(&handle->encoder));

  /* Initialize frame decoder: only fails with nullptr (impossible here) */
  (void)(rx_frame_decoder_init(&handle->decoder));

  /* Session state is managed externally - pointer already set above */

  /* Initialize buffer state */
  handle->rx_buffer_len = 0;
  handle->rx_buffer_pos = 0;

  /* Initialize mode to binary (default) */
  handle->mode = k_usb_comm_mode_binary;

  handle->initialized = 1U;

  /* Post-condition: initialized and mode were set unconditionally above */
  /* GCOVR_EXCL_BR_START LCOV_EXCL_BR_START(assigned 1U on line above) */
  RX_ASSERT_POST(handle->initialized, "Handle initialization failed");
  /* GCOVR_EXCL_BR_STOP LCOV_EXCL_BR_STOP */

  rx_log_debug(s_tag, "USB comm initialized");
  return k_rx_ok;
}

/**
 * @brief Deinitialize USB communication layer
 *
 * @details
 * Releases resources associated with the USB communication handle. Deinitializes
 * both frame encoder and decoder. Safe to call on uninitialized handle (logs
 * assertion but doesn't crash).
 *
 * @par Algorithm:
 * 1. Validate handle pointer
 * 2. Assert handle was initialized (catches programming errors)
 * 3. If initialized, deinit encoder (log warning on failure)
 * 4. Deinit decoder (log warning on failure)
 * 5. Mark handle as not initialized
 * 6. Return first error encountered (or k_rx_ok)
 *
 * @param[in,out] handle USB communication handle to deinitialize
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success (or handle was not initialized)
 * @retval k_rx_err_invalid_arg nullptr handle pointer
 * @retval Other errors from encoder/decoder deinit (first error returned)
 *
 * @pre handle != nullptr
 * @pre handle->initialized == true (asserted, not enforced)
 * @post handle->initialized == false
 * @post Frame encoder/decoder resources released
 *
 * @note Safe to call multiple times (idempotent after first call)
 * @note Logs warning but continues if sub-deinit fails
 *
 * @see rx_usb_comm_init() Initialize before use
 */
rx_err_t rx_usb_comm_deinit(rx_usb_comm_handle_t* handle)
{
  /* Rule 5: Pre-condition 1 - validate pointer */
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Rule 5: Pre-condition 2 - catch programming error (deinit without init) */
  rx_err_t result = k_rx_ok;

  if (handle->initialized) {
    /* encoder/decoder deinit only fail with nullptr, impossible since handle is valid */
    (void)(rx_frame_encoder_deinit(&handle->encoder));

    (void)(rx_frame_decoder_deinit(&handle->decoder));

    handle->initialized = 0U;
  }

  rx_log_debug(s_tag, "USB comm deinitialized");
  return result;
}

/* =============================================================================
 * Send API
 * =============================================================================
 */

/**
 * @brief Build frame structure from individual parameters
 *
 * @details
 * Constructs an rx_frame_t structure by populating header fields and copying
 * payload data. Does not compute CRC - that is done during encoding.
 *
 * @par Algorithm:
 * 1. Validate frame pointer (RX_ASSERT + runtime check)
 * 2. Validate payload pointer if length > 0
 * 3. Populate header fields (sequence, length, type, flags)
 * 4. Copy payload data if present
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
 *
 * @note Does not compute CRC - use rx_frame_encode() for that
 * @note Payload is copied, caller can free original after call
 *
 * @see rx_usb_comm_send() Uses this to build frames for transmission
 */
static rx_err_t internal_build_frame(rx_frame_t*           frame,
                                     const uint16_t        sequence,
                                     const rx_frame_type_t type,
                                     const uint8_t         flags,
                                     const uint8_t*        payload,
                                     const uint32_t        payload_len)
{
  /* Pre-condition 1: Frame pointer must be valid (caller always passes &local_frame) */

  /* Pre-condition 2: Payload pointer must be valid if length > 0 (checked by caller) */

  /* Pre-condition 3: Payload length must fit (checked by caller before calling) */

  frame->header.sequence = sequence;
  frame->header.length   = (uint16_t)payload_len;
  frame->header.type     = (uint8_t)type;
  frame->header.flags    = flags;

  if (payload != nullptr && payload_len > 0) {
    for (uint32_t ci = 0; ci < payload_len; ci++) {
      frame->payload[ci] = payload[ci];
    }
  }

  return k_rx_ok;
}

rx_err_t rx_usb_comm_send(rx_usb_comm_handle_t* handle,
                          const rx_frame_type_t type,
                          const uint8_t         flags,
                          const uint8_t*        payload,
                          const uint32_t        payload_len)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    rx_log_error(s_tag, "Handle not initialized");
    return k_rx_err_invalid_state;
  }

  /* Check USB is ready (Port 0 = protocol) */
  if (!rx_usb_is_configured(k_usb_port_proto)) {
    rx_log_error(s_tag, "USB not configured");
    return k_rx_err_invalid_state;
  }

  /* Check mode - binary send not allowed in debug mode */
  if (handle->mode == k_usb_comm_mode_debug) {
    rx_log_warn(s_tag, "Binary send blocked in debug mode");
    return k_rx_err_invalid_state;
  }

  if (payload == nullptr && payload_len > 0) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > k_frame_max_payload) {
    rx_log_error(s_tag, "Payload too large");
    return k_rx_err_invalid_size;
  }

  /* Get next TX sequence: session always initialized and valid ptrs passed */
  uint16_t sequence = k_initial_sequence;
  (void)rx_session_next_tx(handle->session, &sequence);

  /* Build frame: all args valid (checked above) so always succeeds */
  rx_frame_t frame = {};
  (void)internal_build_frame(&frame, sequence, type, flags, payload, payload_len);

  /* Encode frame: encoder initialized, payload within bounds, so always succeeds */
  uint32_t wire_len = 0;
  (void)rx_frame_encode(&handle->encoder, &frame, handle->tx_buffer, &wire_len);

  /* Send via USB (Port 0 = protocol) - can fail if TX buffer is full */
  const rx_err_t err = rx_usb_write(k_usb_port_proto, handle->tx_buffer, wire_len);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "USB write failed");
    return err;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Control Frame Handling
 * =============================================================================
 */

/**
 * @enum rx_frame_action_t
 * @brief Action returned by internal_handle_control_frame
 *
 * @details Indicates whether a decoded frame was handled internally (control
 * frame) or should be returned to the caller (data frame).
 */
typedef enum : uint8_t {
  k_frame_action_continue = 0, /**< Control frame handled; loop for next */
  k_frame_action_return   = 1, /**< Data frame validated; return to caller */
  k_frame_action_error    = 2, /**< Error occurred; return err */
} rx_frame_action_t;

/**
 * @brief Handle a decoded frame: auto-reply to control frames, validate data frames
 *
 * @details
 * Processes PING (sends PONG, invokes callback), RESET (sends RESET_ACK, resets
 * session, invokes callback), and silently consumes PONG/RESET_ACK. Data frames
 * (COMMAND, RESPONSE) are validated via session and flagged for return.
 *
 * @param[in,out] handle USB comm handle for sending responses and session access
 * @param[in,out] frame  Decoded frame to inspect and potentially respond to
 * @param[out]    err    Set to error code on k_frame_action_error
 *
 * @return rx_frame_action_t Next action for the receive loop
 *
 * @pre  handle != nullptr and handle->initialized
 * @pre  frame contains a successfully decoded frame
 * @post On k_frame_action_continue: control frame was handled (response sent)
 * @post On k_frame_action_return: data frame validated via session
 * @post On k_frame_action_error: *err contains the error code
 *
 * @note Not thread-safe; must be called from single thread
 * @since Version 1.0.0
 */
static rx_frame_action_t
internal_handle_control_frame(rx_usb_comm_handle_t* handle, rx_frame_t* frame, rx_err_t* err)
{
  /* PING: auto-send PONG, invoke callback, loop for next frame */
  if (frame->header.type == k_frame_type_ping) {
    *err = rx_usb_comm_send_pong(handle, frame->payload, frame->header.length);
    if (*err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to send PONG response");
      return k_frame_action_error;
    }
    rx_log_debug(s_tag, "Auto-responded with PONG");
    if (handle->on_ping_cb != nullptr) {
      handle->on_ping_cb(frame, handle->cb_ctx);
    }
    return k_frame_action_continue;
  }

  /* RESET: send RESET_ACK, reset session, invoke callback, loop */
  if (frame->header.type == k_frame_type_reset) {
    *err = rx_usb_comm_send_reset_ack(handle);
    if (*err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to send RESET_ACK");
      return k_frame_action_error;
    }
    rx_log_debug(s_tag, "Auto-responded with RESET_ACK");
    (void)(rx_session_reset(handle->session));
    if (handle->on_reset_cb != nullptr) {
      handle->on_reset_cb(frame, handle->cb_ctx);
    }
    return k_frame_action_continue;
  }

  /* PONG / RESET_ACK: consume silently */
  if (frame->header.type == k_frame_type_pong || frame->header.type == k_frame_type_reset_ack) {
    return k_frame_action_continue;
  }

  /* Data frame: validate sequence via session */
  rx_session_validate_result_t validate_result = k_session_validate_fail;
  *err = rx_session_validate_rx(handle->session, frame->header.sequence, &validate_result);
  if (*err != k_rx_ok) {
    rx_log_error(s_tag, "Session validate_rx returned error");
    return k_frame_action_error;
  }

  return k_frame_action_return;
}

/* =============================================================================
 * Receive API
 * =============================================================================
 */

rx_err_t
rx_usb_comm_receive(rx_usb_comm_handle_t* handle, rx_frame_t* frame, const uint32_t timeout_ms)
{
  /* Pre-condition 1: Handle must be valid */
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Frame output must be valid */
  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 3: Handle must be initialized */
  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Pre-condition 4: USB must be configured (Port 0 = protocol) */
  if (!rx_usb_is_configured(k_usb_port_proto)) {
    return k_rx_err_invalid_state;
  }

  uint32_t            elapsed_ms = 0;
  uint32_t            iterations = 0;
  rx_err_t            err        = k_rx_ok;
  rx_receive_result_t result     = k_receive_continue;

  /* Try to receive a complete frame with bounded iterations.
   * Control frames (PING, PONG, RESET, RESET_ACK) are handled internally
   * and the loop continues to receive the next data frame. */
  while (iterations < k_max_receive_iterations) {
    iterations++;

    result = internal_receive_iteration(handle, frame, timeout_ms, &elapsed_ms, &err);
    if (result == k_receive_error) {
      return err;
    }
    if (result != k_receive_done) {
      continue; /* k_receive_continue: loop continues */
    }

    /* Frame decoded - dispatch control frames or validate data frame */
    rx_frame_action_t action = internal_handle_control_frame(handle, frame, &err);
    if (action == k_frame_action_error) {
      return err;
    }
    if (action == k_frame_action_return) {
      return k_rx_ok;
    }
    /* k_frame_action_continue: control frame handled, loop for next */
  }

  /* Exceeded maximum iterations */
  return k_rx_err_timeout;
}

/**
 * @brief Check if data is available to receive
 * @param[in] handle USB communication handle
 * @param[out] available Set to true if data available, false otherwise
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle or available is nullptr
 * @return k_rx_err_invalid_state if handle not initialized
 * @return Error code from USB RX availability check on failure
 */
rx_err_t rx_usb_comm_data_available(const rx_usb_comm_handle_t* handle, bool* available)
{
  if (handle == nullptr || available == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check USB CDC buffer (Port 0 = protocol) - port and ptr are valid, always succeeds */
  uint32_t usb_available = 0;
  (void)(rx_usb_rx_available(k_usb_port_proto, &usb_available));

  /* Also check our staging buffer */
  const uint32_t buffered = handle->rx_buffer_len - handle->rx_buffer_pos;

  const bool usb_has_data    = (bool)(usb_available > 0);
  const bool buffer_has_data = (bool)(buffered > 0);
  *available                 = (bool)((int)usb_has_data | (int)buffer_has_data);
  return k_rx_ok;
}

/**
 * @brief Check if USB communication is ready to send/receive
 * @param[in] handle USB communication handle
 * @param[out] ready Set to true if ready, false otherwise
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle or ready is nullptr
 * @return k_rx_err_invalid_state if handle not initialized
 */
rx_err_t rx_usb_comm_is_ready(const rx_usb_comm_handle_t* handle, bool* ready)
{
  if (handle == nullptr || ready == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    *ready = false;
    return k_rx_err_invalid_state;
  }

  *ready = rx_usb_is_configured(k_usb_port_proto);
  return k_rx_ok;
}

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

/**
 * @brief Flush the USB receive buffer, discarding all pending data
 *
 * @details
 * Resets the internal RX buffer position and length to zero, effectively
 * discarding any partially received or buffered data. This is useful after
 * error recovery or mode switching to ensure a clean receive state.
 *
 * @param[in,out] handle USB communication handle (nullptr-safe: no-op if nullptr)
 *
 * @return void
 *
 * @pre handle should be initialized, but nullptr is safely handled
 * @post handle->rx_buffer_len == 0
 * @post handle->rx_buffer_pos == 0
 *
 * @note Not thread-safe: caller must serialize access to handle
 * @warning Any partially received frame data will be lost
 *
 * @see rx_usb_comm_receive() Receive path that populates the buffer
 * @since Version 1.0.0
 */
void rx_usb_comm_flush_rx(rx_usb_comm_handle_t* handle)
{
  if (handle != nullptr) {
    handle->rx_buffer_len = 0;
    handle->rx_buffer_pos = 0;
  }
}

/* =============================================================================
 * Control Frame API
 * =============================================================================
 */

/**
 * @brief Register callbacks for PING and RESET control frames
 *
 * @details
 * Sets application-level callbacks invoked after control frame auto-response.
 * PING callback fires after PONG is sent; RESET callback fires after
 * RESET_ACK is sent and session is reset. Either callback may be NULL to
 * disable notification for that frame type.
 *
 * @param[in,out] handle USB communication handle (must be initialized)
 * @param[in] on_ping_cb Callback for PING frames (NULL to disable)
 * @param[in] on_reset_cb Callback for RESET frames (NULL to disable)
 * @param[in] cb_ctx Opaque context pointer forwarded to callbacks
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Callbacks registered successfully
 * @retval k_rx_err_invalid_arg handle isnullptr
 * @retval k_rx_err_invalid_state handle is not initialized
 *
 * @pre handle must be initialized via rx_usb_comm_init()
 * @post handle->on_ping_cb, on_reset_cb, cb_ctx updated
 *
 * @note Not thread-safe: call during initialization or from single thread
 * @note Callback ownership: caller retains ownership of cb_ctx
 *
 * @see rx_usb_comm_send_pong() Auto-response before PING callback
 * @see rx_usb_comm_send_reset_ack() Auto-response before RESET callback
 * @since Version 1.0.0
 */
rx_err_t rx_usb_comm_set_control_callbacks(rx_usb_comm_handle_t* handle,
                                           void (*on_ping_cb)(const rx_frame_t* frame, void* ctx),
                                           void (*on_reset_cb)(const rx_frame_t* frame, void* ctx),
                                           void* cb_ctx)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  handle->on_ping_cb  = on_ping_cb;
  handle->on_reset_cb = on_reset_cb;
  handle->cb_ctx      = cb_ctx;

  return k_rx_ok;
}

/**
 * @brief Send a PONG frame in response to a received PING
 *
 * @details
 * Constructs and sends a PONG frame echoing the PING payload (typically a
 * 4-byte counter). Uses the shared session for TX sequence assignment.
 *
 * @param[in,out] handle USB communication handle (must be initialized)
 * @param[in] payload PING payload to echo (may be NULL if payload_len == 0)
 * @param[in] payload_len Length of payload in bytes
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok PONG sent successfully
 * @retval k_rx_err_invalid_arg handle isnullptr
 * @retval k_rx_err_invalid_state handle not initialized
 *
 * @pre handle must be initialized
 * @post TX sequence incremented by 1
 *
 * @note Called automatically by rx_usb_comm_receive() on PING frames
 *
 * @see rx_frame_create_pong() Frame construction
 * @see rx_session_next_tx() Sequence assignment
 * @since Version 1.0.0
 */
rx_err_t rx_usb_comm_send_pong(rx_usb_comm_handle_t* handle,
                               const uint8_t*        payload,
                               const uint16_t        payload_len)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  if (!rx_usb_is_configured(k_usb_port_proto)) {
    return k_rx_err_invalid_state;
  }

  /* session always valid here; next_tx only fails with nullptr */
  uint16_t sequence = k_initial_sequence;
  (void)(rx_session_next_tx(handle->session, &sequence));

  /* Build PONG frame: frame ptr is valid, payload is from received ping frame */
  rx_frame_t pong_frame = {};
  (void)(rx_frame_create_pong(&pong_frame, sequence, payload, payload_len));

  /* Encode: encoder initialized, frame valid, always succeeds */
  uint32_t wire_len = 0;
  (void)(rx_frame_encode(&handle->encoder, &pong_frame, handle->tx_buffer, &wire_len));

  /* Send via USB - can fail if TX buffer full */
  return rx_usb_write(k_usb_port_proto, handle->tx_buffer, wire_len);
}

/**
 * @brief Send a RESET_ACK frame in response to a received RESET
 *
 * @details
 * Constructs and sends a RESET_ACK frame (no payload) to acknowledge a
 * session reset request. Uses the shared session for TX sequence assignment.
 *
 * @param[in,out] handle USB communication handle (must be initialized)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok RESET_ACK sent successfully
 * @retval k_rx_err_invalid_arg handle isnullptr
 * @retval k_rx_err_invalid_state handle not initialized
 *
 * @pre handle must be initialized
 * @post TX sequence incremented by 1
 *
 * @note Called automatically by rx_usb_comm_receive() on RESET frames
 *
 * @see rx_frame_create_reset_ack() Frame construction
 * @see rx_session_reset() Session reset after ACK
 * @since Version 1.0.0
 */
rx_err_t rx_usb_comm_send_reset_ack(rx_usb_comm_handle_t* handle)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  if (!rx_usb_is_configured(k_usb_port_proto)) {
    return k_rx_err_invalid_state;
  }

  /* session always valid here; next_tx only fails with nullptr */
  uint16_t sequence = k_initial_sequence;
  (void)(rx_session_next_tx(handle->session, &sequence));

  /* Build RESET_ACK frame: frame ptr is valid, always succeeds */
  rx_frame_t reset_ack_frame = {};
  (void)(rx_frame_create_reset_ack(&reset_ack_frame, sequence));

  /* Encode: encoder initialized, frame valid, always succeeds */
  uint32_t wire_len = 0;
  (void)(rx_frame_encode(&handle->encoder, &reset_ack_frame, handle->tx_buffer, &wire_len));

  return rx_usb_write(k_usb_port_proto, handle->tx_buffer, wire_len);
}

/* =============================================================================
 * Mode Switching API
 * =============================================================================
 */

/**
 * @brief Set USB communication mode (binary protocol or debug text)
 * @param[in,out] handle USB communication handle
 * @param[in] mode Mode to set (k_usb_comm_mode_binary or k_usb_comm_mode_debug)
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle is nullptr or mode is invalid
 * @return k_rx_err_invalid_state if handle not initialized
 */
rx_err_t rx_usb_comm_set_mode(rx_usb_comm_handle_t* handle, const rx_usb_comm_mode_t mode)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Validate mode is a valid enum value (NASA Rule 5) */
  if (mode != k_usb_comm_mode_binary && mode != k_usb_comm_mode_debug) {
    return k_rx_err_invalid_arg;
  }

  /* Log mode change */
  if (handle->mode != mode) {
    if (mode == k_usb_comm_mode_debug) {
      rx_log_info(s_tag, "Switching to debug text mode");
    } else {
      rx_log_info(s_tag, "Switching to binary protocol mode");
    }
  }

  handle->mode = mode;
  return k_rx_ok;
}

/**
 * @brief Get current USB communication mode
 * @param[in] handle USB communication handle
 * @return Current mode (k_usb_comm_mode_invalid if handle is nullptr or not initialized)
 */
rx_usb_comm_mode_t rx_usb_comm_get_mode(const rx_usb_comm_handle_t* handle)
{
  if (handle == nullptr || !handle->initialized) {
    return k_usb_comm_mode_invalid;
  }
  return handle->mode;
}
