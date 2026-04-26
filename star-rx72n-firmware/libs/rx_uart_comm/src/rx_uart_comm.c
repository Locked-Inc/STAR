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
 * ... [label="TX Path"];
 * App => UART_Comm [label="send(type, payload)"];
 * UART_Comm => Frame [label="encode()"];
 * UART_Comm => UART_HAL [label="uart_write_channel()"];
 * ... [label="RX Path"];
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

static const char* const s_tag = "UART_COMM";

/* =============================================================================
 * Frame Header Constants
 *
 * Frame format: [SYNC(2B)][SEQ(2B)][LEN(2B)][TYPE(1B)][FLAGS(1B)]
 * =============================================================================
 */

/**
 * @enum rx_uart_comm_header_size_t
 * @brief Total encoded frame header size in bytes
 *
 * @details
 * Computed at compile time from the individual field size constants defined in
 * rx_frame.h. Used when determining how many bytes are needed to hold a complete
 * frame header before the payload begins.
 */
typedef enum : uint8_t {
  k_frame_header_total = k_frame_sync_size + k_frame_seq_size + k_frame_len_size +
                         k_frame_type_size + k_frame_flags_size, /**< Total header bytes */
} rx_uart_comm_header_size_t;

/**
 * @enum uart_crc32_seed_t
 * @brief Initial seed value for CRC-32 output variable
 *
 * @details
 * Used to zero-initialize the CRC output variable passed to rx_crc32_ieee()
 * before the calculation begins, avoiding use of a magic literal 0U.
 */
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
 *
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

  (void)(rx_frame_encoded_size(frame->header.length));
  /* Post-condition: data_len == total_size which includes the full payload.
   * internal_decode_frame is only called after total_size bytes are verified available. */

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
 *
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
RX_STATIC_TESTABLE rx_err_t internal_verify_crc(const uint8_t* data,
                                                uint32_t       offset,
                                                uint32_t*      crc_out)
{
  if (data == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (offset < (k_frame_min_size - k_frame_crc_size)) {
    return k_rx_err_invalid_arg;
  }

  const uint32_t received_crc   = rx_frame_read_le32(&data[offset]);
  uint32_t       calculated_crc = k_uart_crc32_seed_initial;
  (void)(rx_crc32_ieee(data, offset, &calculated_crc));
  /* Post-condition: rx_crc32_ieee only fails on null/zero-len; data is valid and
   * offset >= k_frame_min_size - k_frame_crc_size > 0 is guaranteed by RX_ASSERT above */

  if (received_crc != calculated_crc) {
    return k_rx_err_crc_mismatch;
  }

  if (crc_out != nullptr) {
    *crc_out = received_crc;
  }

  return k_rx_ok;
}

/**
 * @enum rx_uart_comm_sequence_constants_t
 * @brief Sequence number initial value constants
 *
 * @details
 * Provides a named constant for the sequence number starting value used when
 * initializing a TX sequence or building a reset-ack frame, avoiding magic 0.
 */
typedef enum : uint16_t {
  k_initial_sequence = 0, /**< Initial TX/RX sequence number */
} rx_uart_comm_sequence_constants_t;

/**
 * @enum rx_uart_comm_sync_constants_t
 * @brief Sync word search helper constants
 *
 * @details
 * Provides a named offset for the second byte of the two-byte sync word when
 * scanning the receive buffer for frame boundaries.
 */
typedef enum : uint8_t {
  k_sync_second_byte_offset = 1, /**< Offset from first sync byte to second sync byte */
} rx_uart_comm_sync_constants_t;

/**
 * @enum uart_frame_header_offset_t
 * @brief Byte offsets into the raw frame header buffer
 *
 * @details
 * Provides named constants for the fixed field positions within the encoded
 * frame header so that internal_parse_header() avoids magic number indexing.
 */
typedef enum : uint8_t {
  k_hdr_len_offset = 4, /**< Payload length field byte offset within header */
} uart_frame_header_offset_t;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @enum rx_uart_comm_sync_result_t
 * @brief Sentinel value returned by internal_find_sync() when no sync is found
 *
 * @details
 * Uses a signed int32_t so -1 is representable, distinguishing "not found"
 * from any valid non-negative buffer position.
 */
typedef enum : int32_t {
  k_sync_not_found = -1, /**< Sync word not found in buffer */
} rx_uart_comm_sync_result_t;

/**
 * @enum rx_receive_result_t
 * @brief Return codes for internal_receive_iteration()
 *
 * @details
 * Allows the receive loop in rx_uart_comm_receive() to distinguish between
 * "need more data", "frame ready", and "error occurred" without overloading
 * the rx_err_t return type.
 */
typedef enum : uint8_t {
  k_receive_continue = 0, /**< Continue to next iteration */
  k_receive_done     = 1, /**< Frame received successfully */
  k_receive_error    = 2, /**< Error occurred, check err output */
} rx_receive_result_t;

/**
 * @brief Read available data from UART into staging buffer
 *
 * @details
 * Reads any available data from the configured UART channel into the handle's
 * rx_buffer. Appends to existing data without overwriting. Returns immediately
 * if buffer is full or no data available.
 *
 *
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
RX_STATIC_TESTABLE rx_err_t internal_read_uart_data(rx_uart_comm_handle_t* handle)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Post-condition: rx_buffer_len is maintained <= k_uart_comm_rx_buffer_size by design.
   * All callers ensure rx_buffer_len starts valid; bytes_read <= space prevents overflow. */

  const uint32_t space = k_uart_comm_rx_buffer_size - handle->rx_buffer_len;
  if (space == 0) {
    return k_rx_ok; /* Buffer full */
  }

  /* Check if data is available before attempting to read */
  bool           available = false;
  const rx_err_t avail_err = uart_rx_available(handle->channel, &available);
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

  /* Post-condition: bytes_read <= space = k_uart_comm_rx_buffer_size - old_len,
   * so new rx_buffer_len = old_len + bytes_read <= k_uart_comm_rx_buffer_size */

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
 *
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
RX_STATIC_TESTABLE rx_err_t internal_compact_rx_buffer(rx_uart_comm_handle_t* handle)
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
    for (uint32_t i = 0; i < remaining; i++) {
      handle->rx_buffer[i] = handle->rx_buffer[handle->rx_buffer_pos + i];
    }
    handle->rx_buffer_len = remaining;
    handle->rx_buffer_pos = 0;
  }

  /* Post-condition: both branches above set rx_buffer_pos = 0 */

  return k_rx_ok;
}

/**
 * @brief Search for frame sync word in receive buffer
 *
 * @details
 * Scans the receive buffer starting from rx_buffer_pos for the frame sync
 * word (0x55AA) stored in little-endian format.
 *
 *
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
RX_STATIC_TESTABLE rx_err_t internal_find_sync(const rx_uart_comm_handle_t* handle,
                                               int32_t*                     sync_pos)
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
 * @details
 * When no sync word is present, advances the buffer position by one byte if
 * the buffer is full (to make room for new data), then compacts the buffer by
 * discarding consumed bytes from the front.
 *
 *
 *
 * @pre handle != nullptr
 * @pre handle->rx_buffer_pos <= handle->rx_buffer_len
 * @post Buffer compacted; rx_buffer_pos == 0 on success
 * @post No sync word present in remaining buffer data
 */
RX_STATIC_TESTABLE rx_err_t internal_handle_no_sync(rx_uart_comm_handle_t* handle)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (handle->rx_buffer_len >= k_uart_comm_rx_buffer_size) {
    handle->rx_buffer_pos++;
  }

  (void)(internal_compact_rx_buffer(handle));
  /* Post-condition: compact only fails on nullptr or pos > len; handle is valid
   * and pos <= len is maintained as an invariant by all callers */

  return k_rx_ok;
}

/**
 * @brief Decode a complete frame from buffer
 *
 * @details
 * Reads total_size bytes from the current buffer position, decodes the header,
 * copies the payload, and verifies the CRC-32. Advances rx_buffer_pos by
 * total_size regardless of success or failure, then compacts the buffer.
 *
 *
 *
 * @pre handle != nullptr and initialized
 * @pre frame != nullptr
 * @pre total_size bytes available starting at handle->rx_buffer_pos
 * @post rx_buffer_pos advanced by total_size; buffer compacted
 * @post *frame contains valid decoded frame on k_rx_ok
 */
RX_STATIC_TESTABLE rx_err_t internal_decode_frame(rx_uart_comm_handle_t* handle,
                                                  rx_frame_t*            frame,
                                                  const uint32_t         total_size)
{
  const uint8_t* hdr    = handle->rx_buffer + handle->rx_buffer_pos;
  uint32_t       offset = 0;

  (void)internal_decode_header(hdr, total_size, frame, &offset);
  /* Post-condition: header decode can only fail if sync word is wrong (impossible: buffer
   * was aligned to sync) or payload_len invalid (impossible: parse_header validated it) */

  if (frame->header.length > 0) {
    for (uint16_t i = 0; i < frame->header.length; i++) {
      frame->payload[i] = hdr[offset + i];
    }
    offset += frame->header.length;
  }

  rx_err_t err = internal_verify_crc(hdr, offset, &frame->crc);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame CRC check failed");
  }

  handle->rx_buffer_pos += total_size;
  (void)(internal_compact_rx_buffer(handle));
  /* Post-condition: compact only fails on nullptr or pos > len;
   * handle is valid and pos <= len is maintained by design */

  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Frame decode failed");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Align buffer position to sync word if found
 *
 * @details
 * Advances rx_buffer_pos to sync_pos when sync_pos is ahead of the current
 * position, discarding any bytes before the sync word.
 *
 *
 * @pre handle != nullptr
 * @pre sync_pos >= 0
 * @post handle->rx_buffer_pos >= (uint32_t)sync_pos
 * @post No bytes before sync_pos remain accessible
 */
RX_STATIC_TESTABLE void internal_align_to_sync(rx_uart_comm_handle_t* handle,
                                               const int32_t          sync_pos)
{
  if ((uint32_t)sync_pos > handle->rx_buffer_pos) {
    handle->rx_buffer_pos = (uint32_t)sync_pos;
  }
}

/**
 * @brief Parse frame header and validate payload length
 *
 * @details
 * Reads the payload length field from the frame header at the current buffer
 * position and validates it against k_frame_max_payload. Computes the total
 * expected frame size if the length is valid, or advances the buffer position
 * past the sync word if invalid.
 *
 *
 *
 * @pre handle != nullptr with at least k_frame_header_total bytes available
 * @pre payload_len != nullptr
 * @pre total_size != nullptr
 * @post On k_rx_ok: *total_size == k_frame_header_total + *payload_len + k_frame_crc_size
 * @post On k_rx_err_invalid_size: rx_buffer_pos advanced by k_frame_sync_size
 */
RX_STATIC_TESTABLE rx_err_t internal_parse_header(rx_uart_comm_handle_t* handle,
                                                  uint16_t*              payload_len,
                                                  uint32_t*              total_size)
{
  const uint8_t* hdr = handle->rx_buffer + handle->rx_buffer_pos;
  *payload_len       = rx_frame_read_le16(&hdr[k_hdr_len_offset]);

  if (*payload_len > k_frame_max_payload) {
    rx_log_warn(s_tag, "Invalid payload length, skipping");
    handle->rx_buffer_pos += k_frame_sync_size;
    return k_rx_err_invalid_size;
  }

  *total_size = k_frame_header_total + *payload_len + k_frame_crc_size;
  return k_rx_ok;
}

/**
 * @brief Process one iteration of the frame receive loop
 *
 * @details
 * Performs a single pass of the receive state machine. Attempts to read UART
 * data, find sync word, parse header, and decode complete frame. Returns
 * result code indicating whether to continue, success, or error.
 *
 *
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
RX_STATIC_TESTABLE rx_receive_result_t internal_receive_iteration(rx_uart_comm_handle_t* handle,
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
    /* Post-condition: handle_no_sync only fails if compact fails; compact only fails on
     * nullptr/pos>len -- both impossible for a valid, in-use handle */
    *err = k_rx_err_no_data;
    return k_receive_error;
  }
  /* Post-condition: find_sync only returns not_found (handled), ok, or error for null/invalid.
   * handle is validated before this call so only not_found or ok are reachable. */

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
  *err                 = internal_parse_header(handle, &payload_len, &total_size);
  if (*err == k_rx_err_invalid_size) {
    /* Invalid payload length; buffer advanced, continue to find next sync */
    *err = k_rx_ok;
    return k_receive_continue;
  }
  /* Post-condition: parse_header only returns invalid_size (handled above) or ok.
   * All other error codes are impossible from a valid, initialized handle. */

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

  (void)(rx_frame_encoder_init(&handle->encoder));
  /* Post-condition: encoder init only fails on nullptr; &handle->encoder is never null */

  (void)(rx_frame_decoder_init(&handle->decoder));
  /* Post-condition: decoder init only fails on nullptr; &handle->decoder is never null */

  handle->rx_buffer_len = 0;
  handle->rx_buffer_pos = 0;
  handle->initialized   = 1U;

  rx_log_debug(s_tag, "UART comm initialized");
  return k_rx_ok;
}

rx_err_t rx_uart_comm_deinit(rx_uart_comm_handle_t* handle)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (handle->initialized) {
    (void)(rx_frame_encoder_deinit(&handle->encoder));
    /* Post-condition: deinit only fails on nullptr; &handle->encoder is never null */

    (void)(rx_frame_decoder_deinit(&handle->decoder));
    /* Post-condition: deinit only fails on nullptr; &handle->decoder is never null */

    handle->initialized = 0U;
  }

  rx_log_debug(s_tag, "UART comm deinitialized");
  return k_rx_ok;
}

/* =============================================================================
 * Send API
 * =============================================================================
 */

/**
 * @brief Build frame structure from individual parameters
 *
 * @details
 * Populates all header fields of *frame and copies the payload bytes into
 * frame->payload. Used by rx_uart_comm_send() to construct outgoing frames
 * before encoding and transmission.
 *
 *
 *
 * @pre frame != nullptr
 * @pre payload != nullptr || payload_len == 0
 * @post frame->header.sequence, type, length, flags are set
 * @post frame->payload[0..payload_len-1] copied from payload on k_rx_ok
 */
RX_STATIC_TESTABLE rx_err_t internal_build_frame(rx_frame_t*           frame,
                                                 const uint16_t        sequence,
                                                 const rx_frame_type_t type,
                                                 const uint8_t         flags,
                                                 const uint8_t*        payload,
                                                 const uint32_t        payload_len)
{
  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

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
    for (uint32_t i = 0; i < payload_len; i++) {
      frame->payload[i] = payload[i];
    }
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

  rx_frame_t frame = {};
  /* build_frame only fails on nullptr frame (impossible: &frame), payload_len > max
   * (already validated above), or null payload with nonzero len (validated above) */
  (void)internal_build_frame(&frame, sequence, type, flags, payload, payload_len);

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

/**
 * @brief Handle a received PING frame by sending a PONG response
 *
 * @details
 * Sends a PONG frame echoing the received PING payload back over the UART
 * channel. Called automatically by rx_uart_comm_receive() when a PING frame
 * is decoded.
 *
 *
 *
 * @pre handle != nullptr and handle->initialized == true
 * @pre frame != nullptr and frame->header.type == k_frame_type_ping
 * @post PONG frame transmitted over UART channel
 * @post Session TX sequence number incremented
 */
RX_STATIC_TESTABLE rx_err_t internal_handle_ping(rx_uart_comm_handle_t* handle,
                                                 const rx_frame_t*      frame)
{
  if (handle == nullptr || frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  rx_err_t pong_err = rx_uart_comm_send(handle,
                                        k_frame_type_pong,
                                        k_frame_flag_none,
                                        frame->payload,
                                        frame->header.length);
  if (pong_err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send PONG response");
    return pong_err;
  }
  rx_log_debug(s_tag, "Auto-responded with PONG");
  return k_rx_ok;
}

/**
 * @brief Handle a received RESET frame by sending RESET_ACK and resetting session
 *
 * @details
 * Constructs and transmits a RESET_ACK frame over the UART channel, then resets
 * the session state machine. Called automatically by rx_uart_comm_receive() when
 * a RESET frame is decoded.
 *
 *
 *
 * @pre handle != nullptr and handle->initialized == true
 * @pre handle->session != nullptr and initialized
 * @post RESET_ACK frame transmitted over UART channel
 * @post handle->session reset to initial state on k_rx_ok
 */
RX_STATIC_TESTABLE rx_err_t internal_handle_reset(rx_uart_comm_handle_t* handle)
{
  if (handle == nullptr) {
    return k_rx_err_invalid_arg;
  }

  uint16_t       reset_ack_seq = k_initial_sequence;
  const rx_err_t seq_err       = rx_session_next_tx(handle->session, &reset_ack_seq);
  if (seq_err != k_rx_ok) {
    return seq_err;
  }

  rx_frame_t reset_ack_frame = {};
  (void)(rx_frame_create_reset_ack(&reset_ack_frame, reset_ack_seq));
  /* Post-condition: create_reset_ack only fails on nullptr; &reset_ack_frame is never null */

  uint32_t wire_len = 0;
  (void)(rx_frame_encode(&handle->encoder, &reset_ack_frame, handle->tx_buffer, &wire_len));
  /* Post-condition: encode only fails on nullptr/uninitialized; handle->encoder is initialized */

  rx_err_t write_err = uart_write_channel(handle->channel, handle->tx_buffer, (uint16_t)wire_len);
  if (write_err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send RESET_ACK");
    return write_err;
  }
  rx_log_debug(s_tag, "Auto-responded with RESET_ACK");

  (void)(rx_session_reset(handle->session));
  /* Post-condition: session_reset fails only on nullptr or uninitialized session.
   * handle->session was validated in rx_uart_comm_init and remains valid. */

  return k_rx_ok;
}

/**
 * @enum uart_dispatch_result_t
 * @brief Return codes for internal_dispatch_frame()
 *
 * @details
 * Distinguishes between "frame consumed internally, continue loop",
 * "frame dispatched to caller as data", and "error during dispatch".
 */
typedef enum : uint8_t {
  k_dispatch_continue = 0, /**< Control frame consumed; continue receive loop */
  k_dispatch_done     = 1, /**< Data frame validated; return to caller */
  k_dispatch_error    = 2, /**< Error during dispatch; check err output */
} uart_dispatch_result_t;

/**
 * @brief Dispatch a decoded frame by type (PING/RESET/PONG/data)
 *
 *
 *
 * @pre handle != nullptr and initialized
 * @pre frame != nullptr with valid decoded header
 * @pre err != nullptr
 * @post On k_dispatch_continue: PONG/RESET_ACK may have been sent
 * @post On k_dispatch_done: session RX sequence updated
 */
static uart_dispatch_result_t
internal_dispatch_frame(rx_uart_comm_handle_t* handle, rx_frame_t* frame, rx_err_t* err)
{
  /* PING: auto-send PONG, continue loop for next frame */
  if (frame->header.type == k_frame_type_ping) {
    *err = internal_handle_ping(handle, frame);
    return (*err != k_rx_ok) ? k_dispatch_error : k_dispatch_continue;
  }

  /* RESET: send RESET_ACK, reset session, continue loop */
  if (frame->header.type == k_frame_type_reset) {
    *err = internal_handle_reset(handle);
    return (*err != k_rx_ok) ? k_dispatch_error : k_dispatch_continue;
  }

  /* PONG / RESET_ACK: consume silently */
  if (frame->header.type == k_frame_type_pong || frame->header.type == k_frame_type_reset_ack) {
    return k_dispatch_continue;
  }

  /* Data frame: validate sequence via session */
  rx_session_validate_result_t validate_result = k_session_validate_fail;
  *err = rx_session_validate_rx(handle->session, frame->header.sequence, &validate_result);
  if (*err != k_rx_ok) {
    rx_log_error(s_tag, "Session validate_rx returned error");
    return k_dispatch_error;
  }
  return k_dispatch_done;
}

rx_err_t rx_uart_comm_receive(rx_uart_comm_handle_t* handle, rx_frame_t* frame, uint32_t timeout_ms)
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

  rx_err_t err = k_rx_ok;

  for (uint32_t iterations = 0; iterations < k_uart_comm_max_receive_iterations; iterations++) {
    rx_receive_result_t result = internal_receive_iteration(handle, frame, &err);
    if (result == k_receive_error) {
      return err;
    }
    if (result != k_receive_done) {
      continue;
    }

    uart_dispatch_result_t dispatch = internal_dispatch_frame(handle, frame, &err);
    if (dispatch == k_dispatch_error) {
      return err;
    }
    if (dispatch == k_dispatch_done) {
      return k_rx_ok;
    }
    /* k_dispatch_continue: loop for next frame */
  }

  return k_rx_err_timeout;
}
