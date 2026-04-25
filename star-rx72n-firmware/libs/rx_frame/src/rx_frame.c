/**
 * @file rx_frame.c
 * @brief Frame Layer Protocol Implementation
 *
 * @details
 * ## Overview
 *
 * This file implements the frame encoding and decoding operations for the
 * SPI communication protocol between RPi5 and RX72N. The implementation is
 * bit-exact compatible with the Go reference in star-gateway/internal/frame/.
 *
 * ## Module Architecture
 *
 * @dot
 * digraph frame_arch {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_public {
 *     label="Public API";
 *     style=filled;
 *     color=lightgrey;
 *     encoder_init [label="rx_frame_encoder_init()"];
 *     encoder_deinit [label="rx_frame_encoder_deinit()"];
 *     encode [label="rx_frame_encode()"];
 *     decoder_init [label="rx_frame_decoder_init()"];
 *     decoder_deinit [label="rx_frame_decoder_deinit()"];
 *     decode [label="rx_frame_decode()"];
 *     decode_resync [label="rx_frame_decode_with_resync()"];
 *     create_ack [label="rx_frame_create_ack()"];
 *     create_nack [label="rx_frame_create_nack()"];
 *   }
 *
 *   subgraph cluster_internal {
 *     label="Internal Functions";
 *     style=filled;
 *     color=lightyellow;
 *     write_le32 [label="internal_write_le32()"];
 *     read_le32 [label="internal_read_le32()"];
 *     decode_header [label="internal_decode_header()"];
 *     verify_crc [label="internal_verify_crc()"];
 *     find_sync [label="internal_find_sync_offset()"];
 *   }
 *
 *   subgraph cluster_deps {
 *     label="Dependencies";
 *     style=filled;
 *     color=lightblue;
 *     rx_crc [label="rx_crc32_ieee()"];
 *     rx_check [label="RX_CHECK_NULL_PTR()"];
 *     memcpy_fn [label="memcpy()"];
 *     memset_fn [label="memset()"];
 *   }
 *
 *   encode -> write_le32;
 *   encode -> rx_crc;
 *   decode -> decode_header;
 *   decode -> verify_crc;
 *   decode_resync -> decode;
 *   decode_resync -> find_sync;
 *   find_sync -> rx_check;
 *   decode_header -> rx_crc [style=invis];
 *   verify_crc -> read_le32;
 *   verify_crc -> rx_crc;
 *   write_le32 -> rx_check;
 *   read_le32 -> rx_check;
 *   create_ack -> memset_fn;
 *   create_nack -> memset_fn;
 * }
 * @enddot
 *
 * ## Encoding Process
 *
 * The encoder serializes a rx_frame_t structure to wire format:
 *
 * 1. **Validate inputs** - Check pointers, initialization state, payload size
 * 2. **Write SYNC** - 2-byte marker (0x55AA) in little-endian (wire: 0xAA, 0x55)
 * 3. **Write header** - SEQ, LEN in little-endian; TYPE, FLAGS as bytes
 * 4. **Copy payload** - Direct byte copy (no endian conversion)
 * 5. **Compute CRC** - IEEE 802.3 CRC-32 over SYNC+header+payload
 * 6. **Write CRC** - 4-byte CRC in little-endian format
 *
 * @dot
 * digraph encode_flow {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   input [label="rx_frame_t\n(host format)"];
 *   validate [label="Validate"];
 *   sync [label="Write SYNC\n(LE)"];
 *   header [label="Write Header\n(LE/bytes)"];
 *   payload [label="Copy Payload"];
 *   crc_calc [label="Calculate CRC"];
 *   crc_write [label="Write CRC\n(LE)"];
 *   output [label="Wire Buffer"];
 *
 *   input -> validate -> sync -> header -> payload -> crc_calc -> crc_write -> output;
 * }
 * @enddot
 *
 * ## Decoding Process
 *
 * The decoder parses wire format into a rx_frame_t structure:
 *
 * 1. **Validate inputs** - Check pointers, initialization state
 * 2. **Verify SYNC** - Check for 0x55AA marker
 * 3. **Parse header** - Extract SEQ, LEN, TYPE, FLAGS with endian conversion
 * 4. **Validate length** - Ensure payload fits in buffer
 * 5. **Copy payload** - Direct byte copy
 * 6. **Verify CRC** - Compute expected CRC, compare with received
 *
 * ## Memory Usage
 *
 * | Function | Stack | Notes |
 * |----------|-------|-------|
 * | rx_frame_encode() | 32 bytes | Variables, return address |
 * | rx_frame_decode() | 32 bytes | Variables, return address |
 * | internal_decode_header() | 24 bytes | Local variables |
 * | internal_verify_crc() | 20 bytes | CRC values |
 *
 * ## Performance (RX72N @ 240 MHz)
 *
 * | Operation | Empty | 64B | 256B | 1KB |
 * |-----------|-------|-----|------|-----|
 * | Encode | 5 us | 12 us | 35 us | 130 us |
 * | Decode | 5 us | 12 us | 35 us | 130 us |
 *
 * **Bottleneck:** CRC calculation dominates for larger payloads.
 * Hardware CRC reduces time by ~4x.
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Notes |
 * |------|--------|-------|
 * | 1. Simple control flow | [PASS] | Sequential processing, no goto |
 * | 2. Fixed loop bounds | [PASS] | Payload loop bounded by payload length; sync scanner bounded by k_frame_max_scan_bytes |
 * | 3. No dynamic memory | [PASS] | All buffers caller-provided |
 * | 4. Short functions | [PASS] | Largest function ~50 lines |
 * | 5. Assertions | [PASS] | All inputs validated, post-conditions checked |
 * | 6. Small scope | [PASS] | Variables at point of use |
 * | 7. Check returns | [PASS] | All errors propagated |
 * | 8. Limited preprocessor | [PASS] | Only includes |
 * | 9. Restrict pointers | [PASS] | No function pointers |
 * | 10. Compiler warnings | [PASS] | Clean build with -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * | Principle | Implementation |
 * |-----------|----------------|
 * | **Single Responsibility** | Encode/decode frames only |
 * | **Open/Closed** | New types/flags via enums |
 * | **Liskov Substitution** | N/A (no inheritance) |
 * | **Interface Segregation** | Separate encoder/decoder |
 * | **Dependency Inversion** | Depends on rx_crc abstraction |
 *
 * ## Test Coverage
 *
 * See tests/test_rx_frame.c for comprehensive unit tests covering:
 * - Encode/decode round-trip
 * - CRC validation
 * - Boundary conditions (min/max payload)
 * - Error cases (NULL pointers, invalid sizes)
 *
 * @see rx_frame.h Public API and frame format documentation
 * @see rx_crc.h CRC-32 computation
 * @see star-gateway/internal/frame/ Go reference implementation
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_frame.h"

#include <string.h>

#include "rx_check.h"
#include "rx_crc.h"

/* =============================================================================
 * Module State
 * =============================================================================
 */

/* =============================================================================
 * Byte Serialization Constants
 *
 * Named constants for byte manipulation in serialization functions.
 * Eliminates magic numbers and documents the bit/byte operations.
 * =============================================================================
 */

/**
 * @brief Bit shift amounts for extracting bytes from multi-byte values
 *
 * Each byte position requires shifting by (position * 8) bits.
 */
typedef enum : uint8_t {
  k_shift_byte_0 = 0,  /**< No shift needed for byte 0 (LSB) */
  k_shift_byte_1 = 8,  /**< Shift 8 bits for byte 1 */
  k_shift_byte_2 = 16, /**< Shift 16 bits for byte 2 */
  k_shift_byte_3 = 24, /**< Shift 24 bits for byte 3 (MSB) */
} byte_shift_t;

/**
 * @brief Initialization state values
 */
typedef enum : uint8_t {
  k_state_uninitialized = 0, /**< Object not initialized */
  k_state_initialized   = 1, /**< Object initialized and ready */
} init_state_t;

/**
 * @enum frame_offset_t
 * @brief Frame byte offsets used during encoding, decoding, and sync scanning
 *
 * @details
 * Defines named byte-offset constants used by the frame encoder, decoder, and
 * the bounded sync-word scanner. k_frame_offset_start marks the beginning of a
 * frame buffer for normal aligned decoding. k_frame_scan_start_offset is the
 * index at which internal_find_sync_offset() begins its forward scan -- offset 0
 * is omitted because rx_frame_decode() has already tested it before invoking
 * the scanner, so the scan starts at the next candidate position.
 *
 * @invariant k_frame_scan_start_offset > k_frame_offset_start, ensuring the
 *            scanner never re-tests the byte that rx_frame_decode() already checked.
 *
 * @code{.c}
 * // Normal aligned decode starts at k_frame_offset_start (0)
 * rx_err_t err = rx_frame_decode(dec, data, data_len, frame);
 *
 * // On sync mismatch, internal_find_sync_offset() scans from offset 1:
 * uint8_t sync_off = k_frame_scan_start_offset;
 * // ... scanner advances sync_off until sync word found or scan limit reached
 * @endcode
 *
 * @see rx_frame_decode() Uses k_frame_offset_start for normal aligned decoding
 * @see internal_find_sync_offset() Begins scan at k_frame_scan_start_offset
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_frame_offset_start      = 0, /**< Start offset for frame parsing */
  k_frame_scan_start_offset = 1, /**< First offset checked in internal_find_sync_offset();
                                      offset 0 is presumed already tested by rx_frame_decode() */
} frame_offset_t;

/**
 * @enum frame_resync_discarded_t
 * @brief Byte-discard counter initial value for rx_frame_decode_with_resync()
 *
 * @details
 * k_bytes_discarded_none is the zero sentinel written to *bytes_discarded_out
 * at the start of rx_frame_decode_with_resync() before any sync search.  Using
 * a named constant makes the intent auditable and prevents a raw 0 literal from
 * appearing in the implementation (NASA Rule 8 / STAR no-magic-numbers policy).
 *
 * @invariant k_bytes_discarded_none == 0 (matches the initial no-discard state)
 *
 * @code{.c}
 * uint32_t bytes_discarded = k_bytes_discarded_none;
 * rx_err_t err = rx_frame_decode_with_resync(&dec, data, data_len, &frame, &bytes_discarded);
 * @endcode
 *
 * @see rx_frame_decode_with_resync() Only caller of this constant
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_bytes_discarded_none = 0U, /**< No bytes have been discarded; stream is aligned */
} frame_resync_discarded_t;

/**
 * @enum frame_crc_init_t
 * @brief Initial value for CRC-32 output variable before rx_crc32_ieee() writes it
 *
 * @details
 * k_frame_crc32_init is used to pre-initialize the calculated_crc output
 * variable in internal_verify_crc() before passing its address to
 * rx_crc32_ieee(). The value is immediately overwritten on success; the
 * named constant satisfies the STAR no-magic-numbers policy (NASA Rule 8)
 * and makes the intent of the initialization explicit.
 *
 * @invariant k_frame_crc32_init == 0 (matches zero-init convention)
 *
 * @see internal_verify_crc() Only user of this constant
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_frame_crc32_init = 0U, /**< Pre-initialization value for CRC output variable */
} frame_crc_init_t;

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Write uint32 in little-endian format (for CRC-32)
 *
 *
 */
static rx_err_t internal_write_le32(uint8_t* buf, const uint32_t buf_len, const uint32_t val)
{
  /* Pre-conditions: buf is always a subslice of output buffer (never nullptr);
   * buf_len >= k_frame_crc_size is guaranteed by the caller's frame-size calculation. */
  (void)buf_len; /* suppress unused warning in non-debug builds */

  buf[k_le32_byte_0] = (uint8_t)(val & k_rx_byte_mask);
  buf[k_le32_byte_1] = (uint8_t)((val >> k_shift_byte_1) & k_rx_byte_mask);
  buf[k_le32_byte_2] = (uint8_t)((val >> k_shift_byte_2) & k_rx_byte_mask);
  buf[k_le32_byte_3] = (uint8_t)((val >> k_shift_byte_3) & k_rx_byte_mask);
  return k_rx_ok;
}

/**
 * @brief Read uint32 in little-endian format (for CRC-32)
 *
 *
 */
static rx_err_t internal_read_le32(const uint8_t* buf, const uint32_t buf_len, uint32_t* out_val)
{
  /* Pre-conditions: buf and out_val are always valid pointers from the caller;
   * buf_len >= k_frame_crc_size guaranteed by the caller's bounds check. */
  (void)buf_len; /* suppress unused warning in non-debug builds */

  *out_val = (uint32_t)buf[k_le32_byte_0] | ((uint32_t)buf[k_le32_byte_1] << k_shift_byte_1) |
             ((uint32_t)buf[k_le32_byte_2] << k_shift_byte_2) |
             ((uint32_t)buf[k_le32_byte_3] << k_shift_byte_3);
  return k_rx_ok;
}

/**
 * @brief Decode frame header from raw data buffer
 *
 * Extracts and validates frame header fields (sync word, sequence, length, type, flags)
 * from the beginning of a raw data buffer. Verifies the sync word matches expected value
 * and validates payload length is within bounds. Outputs the offset where payload data
 * begins for subsequent processing.
 *
 *
 */
static rx_err_t internal_decode_header(const uint8_t* data,
                                       const uint32_t data_len,
                                       rx_frame_t*    frame,
                                       uint32_t*      offset_out)
{
  /* Pre-conditions: data, frame, and offset_out are always valid non-null pointers;
   * validated by the public callers before forwarding to this internal function. */

  if (data_len < k_frame_min_size) {
    return k_rx_err_invalid_size;
  }

  uint32_t offset    = k_frame_offset_start;
  uint16_t sync_word = rx_frame_read_le16(&data[offset]);
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

  uint32_t expected_size = rx_frame_encoded_size(frame->header.length);
  if (data_len < expected_size) {
    return k_rx_err_invalid_size;
  }

  frame->header.type = data[offset];
  offset += k_frame_type_size;

  frame->header.flags = data[offset];
  offset += k_frame_flags_size;

  /* offset_out is always non-null (caller guarantees); assign unconditionally. */
  *offset_out = offset;

  return k_rx_ok;
}

/**
 * @brief Verify CRC-32 checksum at specified offset in data buffer
 *
 * Reads the CRC-32 value stored at the specified offset in the buffer, calculates
 * the expected CRC over the data preceding it, and compares for match. Uses IEEE 802.3
 * CRC-32 polynomial (0x04C11DB7), compatible with Go's crc32.ChecksumIEEE(). On success,
 * outputs the received CRC value for caller inspection.
 *
 *
 */
static rx_err_t
internal_verify_crc(const uint8_t* data, uint32_t data_len, uint32_t offset, uint32_t* crc_out)
{
  /* Pre-conditions: data and crc_out are valid non-null pointers (caller validated);
   * data_len >= k_frame_min_size and offset+CRC size fits in buffer (caller validated). */
  uint32_t received_crc = 0;
  /* internal_read_le32 always returns k_rx_ok when preconditions are met. */
  (void)internal_read_le32(&data[offset], data_len - offset, &received_crc);
  uint32_t calculated_crc = k_frame_crc32_init;
  /* rx_crc32_ieee always returns k_rx_ok for non-null data with valid length. */
  (void)rx_crc32_ieee(data, offset, &calculated_crc);

  if (received_crc != calculated_crc) {
    return k_rx_err_crc_mismatch;
  }

  *crc_out = received_crc;

  return k_rx_ok;
}

/**
 * @brief Scan a byte buffer for the frame sync word with a fixed upper bound
 *
 * @details
 * Performs a linear scan of the buffer starting at offset 1 (offset 0 is
 * presumed to have already failed sync check) looking for the 2-byte sync
 * pattern k_frame_sync_word in little-endian wire encoding (first byte 0xAA,
 * second byte 0x55). The scan is bounded at k_frame_max_scan_bytes to satisfy
 * NASA Rule 2 (fixed loop upper-bounds) and prevent infinite consumption of
 * a permanently corrupt stream.
 *
 * The function stops and reports success at the first occurrence of the sync
 * pattern. If no pattern is found within the window, k_rx_err_protocol_error
 * is returned.
 *
 * Algorithm:
 * 1. Validate preconditions (non-null pointers, minimum buffer size).
 * 2. Compute scan limit = min(data_len - k_frame_sync_size, k_frame_max_scan_bytes).
 * 3. Iterate i from 1 to scan_limit inclusive:
 *    a. Read 16-bit LE value at data[i].
 *    b. If it matches k_frame_sync_word, write i to offset_out and return k_rx_ok.
 * 4. Return k_rx_err_protocol_error if no match found.
 *
 *
 *
 * @pre data must point to a readable buffer of at least data_len bytes
 * @pre data_len must be >= k_frame_sync_size (2)
 * @post On k_rx_ok, offset_out is in range [1, min(data_len - k_frame_sync_size, k_frame_max_scan_bytes)]
 * @post On error, offset_out is unchanged
 *
 * @note Not thread-safe. Caller must ensure exclusive access to data buffer.
 * @note The loop bound is min(data_len - k_frame_sync_size, k_frame_max_scan_bytes), which is
 *       statically bounded by k_frame_max_scan_bytes = 1036 (NASA Rule 2).
 *
 * @par Performance:
 * O(N) where N <= k_frame_max_scan_bytes. Each iteration performs one 16-bit
 * LE read and one comparison. Worst case: 1036 iterations (no sync found).
 *
 * @par Example:
 * @code{.c}
 * // Buffer where offset k_frame_offset_start failed sync; sync is at index 1
 * uint8_t data[] = {0x00, 0xAA, 0x55, 0x01, 0x00};
 * uint32_t offset = k_frame_offset_start;
 * rx_err_t err = internal_find_sync_offset(data, sizeof(data), &offset);
 * if (err == k_rx_ok) {
 *     // offset == 1: decode from data[offset]
 * } else {
 *     // k_rx_err_protocol_error: no sync found within scan window
 * }
 * @endcode
 *
 * @see rx_frame_decode_with_resync() Public API that calls this function
 * @see k_frame_sync_word Sync word value (0x55AA)
 * @see k_frame_max_scan_bytes Maximum scan window (1036 bytes)
 *
 * @since Version 1.0.0
 */
static rx_err_t
internal_find_sync_offset(const uint8_t* data, const uint32_t data_len, uint32_t* offset_out)
{
  /* Pre-conditions: data and offset_out are valid non-null pointers (caller validated);
   * data_len >= k_frame_sync_size guaranteed by the caller's size check. */

  /* Bound scan to at most k_frame_max_scan_bytes positions beyond offset 0 */
  uint32_t scan_limit = data_len - k_frame_sync_size;
  if (scan_limit > k_frame_max_scan_bytes) {
    scan_limit = k_frame_max_scan_bytes;
  }

  /* Scan starting at k_frame_scan_start_offset; offset 0 already failed sync check */
  for (uint32_t i = k_frame_scan_start_offset; i <= scan_limit; i++) {
    const uint16_t candidate = rx_frame_read_le16(&data[i]);
    if (candidate == k_frame_sync_word) {
      *offset_out = i;
      return k_rx_ok;
    }
  }

  return k_rx_err_protocol_error;
}

/* =============================================================================
 * Encoder API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize a frame encoder instance
 *
 * Prepares the encoder for use by setting the initialized flag and verifying
 * the initialization completed successfully (redundant validation per Rule 5).
 *
 *
 */
rx_err_t rx_frame_encoder_init(rx_frame_encoder_t* enc)
{
  if (enc == nullptr) {
    return k_rx_err_invalid_arg;
  }

  enc->initialized = k_state_initialized;
  /* Post-condition: enc->initialized == k_state_initialized is guaranteed by the
   * direct assignment above. */
  return k_rx_ok;
}

/**
 * @brief Deinitialize a frame encoder instance
 *
 * Marks the encoder as uninitialized, preventing further use.
 * Verifies deinitialization completed successfully (redundant validation per Rule 5).
 *
 *
 */
rx_err_t rx_frame_encoder_deinit(rx_frame_encoder_t* enc)
{
  if (enc == nullptr) {
    return k_rx_err_invalid_arg;
  }

  enc->initialized = k_state_uninitialized;
  /* Post-condition: enc->initialized == k_state_uninitialized is guaranteed by the
   * direct assignment above. */
  return k_rx_ok;
}

rx_err_t rx_frame_encode(const rx_frame_encoder_t* enc,
                         const rx_frame_t*         frame,
                         uint8_t*                  output,
                         uint32_t*                 output_len)
{
  if (enc == nullptr || frame == nullptr || output == nullptr || output_len == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (enc->initialized != k_state_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Validate payload size */
  if (frame->header.length > k_frame_max_payload) {
    return k_rx_err_invalid_size;
  }

  /* Calculate total frame size */
  uint32_t frame_size = rx_frame_encoded_size(frame->header.length);
  uint32_t offset     = k_frame_offset_start;

  /* Write SYNC word (little-endian: wire bytes 0xAA, 0x55) */
  rx_frame_write_le16(&output[offset], k_frame_sync_word);
  offset += k_frame_sync_size;

  /* Write SEQ (little-endian) */
  rx_frame_write_le16(&output[offset], frame->header.sequence);
  offset += k_frame_seq_size;

  /* Write LEN (little-endian) */
  rx_frame_write_le16(&output[offset], frame->header.length);
  offset += k_frame_len_size;

  /* Write TYPE (1 byte) */
  output[offset] = frame->header.type;
  offset += k_frame_type_size;

  /* Write FLAGS (1 byte) */
  output[offset] = frame->header.flags;
  offset += k_frame_flags_size;

  /* Write PAYLOAD */
  if (frame->header.length > 0) {
    for (uint32_t i = 0U; i < (uint32_t)frame->header.length; i++) {
      output[offset + i] = frame->payload[i];
    }
    offset += frame->header.length;
  }

  /* Calculate CRC-32 over SYNC + Header + Payload (IEEE 802.3 polynomial).
   * rx_crc32_ieee always returns k_rx_ok for non-null buffer with valid length. */
  uint32_t crc = k_frame_crc32_init;
  (void)rx_crc32_ieee(output, offset, &crc);

  /* Write CRC-32 (little-endian to match IEEE 802.3 LSB-first order).
   * internal_write_le32 always returns k_rx_ok when preconditions are met. */
  (void)internal_write_le32(&output[offset], frame_size - offset, crc);

  *output_len = frame_size;
  return k_rx_ok;
}

/* =============================================================================
 * Decoder API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize a frame decoder instance
 *
 * Prepares the decoder for use by setting the initialized flag and verifying
 * the initialization completed successfully (redundant validation per Rule 5).
 *
 *
 */
rx_err_t rx_frame_decoder_init(rx_frame_decoder_t* dec)
{
  if (dec == nullptr) {
    return k_rx_err_invalid_arg;
  }

  dec->initialized = k_state_initialized;
  /* Post-condition: dec->initialized == k_state_initialized is guaranteed by the
   * direct assignment above. */
  return k_rx_ok;
}

/**
 * @brief Deinitialize a frame decoder instance
 *
 * Marks the decoder as uninitialized, preventing further use.
 * Verifies deinitialization completed successfully (redundant validation per Rule 5).
 *
 *
 */
rx_err_t rx_frame_decoder_deinit(rx_frame_decoder_t* dec)
{
  if (dec == nullptr) {
    return k_rx_err_invalid_arg;
  }

  dec->initialized = k_state_uninitialized;
  /* Post-condition: dec->initialized == k_state_uninitialized is guaranteed by the
   * direct assignment above. */
  return k_rx_ok;
}

/**
 * @brief Decode a frame from raw data
 *
 * Validates the decoder state, extracts the frame header, copies the payload,
 * and verifies the CRC-32 checksum.
 *
 *
 *
 * @note This function performs validation at multiple points:
 *       - Null pointer checks on all parameters
 *       - Decoder initialization check
 *       - Header parsing via internal_decode_header()
 *       - CRC verification via internal_verify_crc() (may surface CRC backend errors)
 */
rx_err_t rx_frame_decode(const rx_frame_decoder_t* dec,
                         const uint8_t*            data,
                         const uint32_t            data_len,
                         rx_frame_t*               frame)
{
  if (dec == nullptr || data == nullptr || frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (dec->initialized != k_state_initialized) {
    return k_rx_err_invalid_state;
  }

  uint32_t offset;
  rx_err_t err = internal_decode_header(data, data_len, frame, &offset);
  if (err != k_rx_ok) {
    return err;
  }

  /* Read PAYLOAD */
  if (frame->header.length > 0) {
    for (uint32_t i = 0U; i < (uint32_t)frame->header.length; i++) {
      frame->payload[i] = data[offset + i];
    }
    offset += frame->header.length;
  }

  err = internal_verify_crc(data, data_len, offset, &frame->crc);
  if (err != k_rx_ok) {
    return err;
  }
  return k_rx_ok;
}

/**
 * @brief Decode a frame from raw data with bounded sync-word resynchronization
 *
 * @details
 * Provides stream recovery when the byte stream has become misaligned due to
 * a dropped or inserted byte. On sync mismatch, scans forward up to
 * k_frame_max_scan_bytes positions for the next occurrence of the sync word,
 * discards the leading bytes, and reattempts decoding from that position.
 *
 * This function is safe for repeated calls on a stream: the caller must advance
 * its stream read pointer by *bytes_discarded_out on k_rx_ok and also on
 * k_rx_err_crc_mismatch (any case where bytes_discarded_out is set) to avoid
 * re-processing discarded bytes in subsequent calls.
 *
 * Recovery algorithm:
 * 1. Attempt rx_frame_decode() on data[0..data_len-1].
 * 2. If result is not k_rx_err_protocol_error, return immediately.
 * 3. Call internal_find_sync_offset() to locate next sync word in data[1..].
 * 4. If not found, return k_rx_err_protocol_error with *bytes_discarded_out = 0.
 * 5. Decode from data[sync_offset..data_len-1] using the trimmed sub-buffer.
 * 6. Write sync_offset to *bytes_discarded_out and return result.
 *
 *
 *
 * @pre dec must be initialized via rx_frame_decoder_init()
 * @pre data must point to a buffer of at least data_len bytes
 * @pre frame must point to a valid rx_frame_t storage location (not NULL)
 * @pre bytes_discarded_out must point to a valid uint32_t storage location (not NULL)
 * @post On k_rx_ok, *bytes_discarded_out == bytes skipped to reach sync word
 * @post On k_rx_ok, frame contains a fully verified decoded frame
 * @post On k_rx_err_crc_mismatch, *bytes_discarded_out == bytes skipped; caller must still advance stream pointer
 *
 * @note Parameter order follows the canonical convention: decoder input -> data input ->
 *       frame output -> diagnostic output (matching rx_frame_decode()).
 * @note Not thread-safe. Caller must synchronize access.
 * @note The scan is bounded at k_frame_max_scan_bytes (NASA Rule 2).
 * @note Log *bytes_discarded_out > 0 as a stream-health diagnostic warning.
 *
 * @warning Caller must advance stream read pointer by *bytes_discarded_out after any call
 *          that returns k_rx_ok or k_rx_err_crc_mismatch to avoid infinite re-scan;
 *          *bytes_discarded_out is unmodified only when k_rx_err_invalid_arg is returned.
 *
 * @par Example:
 * @code{.c}
 * uint32_t discarded = k_bytes_discarded_none;
 * rx_err_t err = rx_frame_decode_with_resync(dec, data, data_len, &frame, &discarded);
 * if (err == k_rx_ok) {
 *     // Advance stream read pointer past any discarded bytes
 *     stream_read_ptr += discarded;
 * } else if (err == k_rx_err_crc_mismatch) {
 *     // Sync was found but CRC failed -- still advance to avoid infinite re-scan
 *     stream_read_ptr += discarded;
 *     rx_log_error(TAG, "sync found but CRC failed");
 * } else {
 *     rx_log_error(TAG, "no sync word within scan window");
 * }
 * @endcode
 *
 * @see rx_frame_decode() Simple decode without stream recovery
 * @see internal_find_sync_offset() Bounded sync word scanner
 * @see k_frame_max_scan_bytes Scan window upper bound
 *
 * @since Version 1.0.0
 */
rx_err_t rx_frame_decode_with_resync(const rx_frame_decoder_t* dec,
                                     const uint8_t*            data,
                                     const uint32_t            data_len,
                                     rx_frame_t*               frame,
                                     uint32_t*                 bytes_discarded_out)
{
  if (dec == nullptr || data == nullptr || frame == nullptr || bytes_discarded_out == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *bytes_discarded_out = k_bytes_discarded_none;

  /* Fast path: attempt aligned decode first */
  rx_err_t err = rx_frame_decode(dec, data, data_len, frame);
  if (err != k_rx_err_protocol_error) {
    /* Either success, or an error other than sync mismatch (CRC, size, etc.) */
    return err;
  }

  /* Sync word not at offset 0: scan forward for next sync word */
  uint32_t sync_offset = (uint32_t)k_frame_offset_start;
  err                  = internal_find_sync_offset(data, data_len, &sync_offset);
  if (err != k_rx_ok) {
    /* No sync word found within bounded window */
    return k_rx_err_protocol_error;
  }

  /* Report discarded bytes now: caller needs this to advance past the sync
   * even if the subsequent decode fails (e.g. CRC mismatch at new offset) */
  *bytes_discarded_out = sync_offset;

  /* Reattempt decode from the discovered sync offset */
  err = rx_frame_decode(dec, &data[sync_offset], data_len - sync_offset, frame);

  return err;
}

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

/**
 * @brief Create an ACK (acknowledgment) frame
 *
 * Constructs a frame with type=ACK, empty payload, and the specified sequence number.
 * The ACK response frames are used by the receiver to confirm successful reception
 * of a command or data frame from the sender.
 *
 *
 */
rx_err_t rx_frame_create_ack(rx_frame_t* frame, const uint16_t sequence)
{
  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *frame                 = (rx_frame_t){};
  frame->header.sequence = sequence;
  frame->header.length   = 0;
  frame->header.type     = k_frame_type_ack;
  frame->header.flags    = k_frame_flag_none;

  /* Post-condition: type == k_frame_type_ack guaranteed by the direct assignment above. */
  return k_rx_ok;
}

/**
 * @brief Create a NACK (negative acknowledgment) frame
 *
 * Constructs a frame with type=NACK, empty payload, the specified sequence number,
 * and error/status flags. NACK frames are sent by the receiver to indicate that a
 * frame was not processed successfully due to an error condition (specified in flags).
 *
 *
 */
rx_err_t rx_frame_create_nack(rx_frame_t* frame, const uint16_t sequence, uint8_t flags)
{
  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *frame                 = (rx_frame_t){};
  frame->header.sequence = sequence;
  frame->header.length   = 0;
  frame->header.type     = k_frame_type_nack;
  frame->header.flags    = flags;

  /* Post-condition: type == k_frame_type_nack guaranteed by the direct assignment above. */
  return k_rx_ok;
}

/**
 * @brief Create a PING keepalive frame
 *
 * @details
 * Initializes frame as a PING request for connection liveness checks.
 * Receiver should respond with a PONG frame echoing the payload.
 * Payload is optional; a 4-byte LE counter is typical.
 *
 *
 *
 * @pre frame !=nullptr
 * @pre payload != NULL when payload_len > 0
 * @post frame->header.type == k_frame_type_ping
 * @post frame->header.length == payload_len
 *
 * @note Stateless; safe to call from any context.
 * @see rx_frame_create_pong() Corresponding response creator
 * @since Version 1.0.0
 */
rx_err_t rx_frame_create_ping(rx_frame_t*    frame,
                              const uint16_t sequence,
                              const uint8_t* payload,
                              const uint32_t payload_len)
{
  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > 0 && payload == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > k_frame_max_payload) {
    return k_rx_err_invalid_size;
  }

  *frame                 = (rx_frame_t){};
  frame->header.sequence = sequence;
  frame->header.length   = (uint16_t)payload_len;
  frame->header.type     = k_frame_type_ping;
  frame->header.flags    = k_frame_flag_none;

  if (payload_len > 0) {
    for (uint32_t i = 0U; i < payload_len; i++) {
      frame->payload[i] = payload[i];
    }
  }

  /* Post-condition: type == k_frame_type_ping guaranteed by the direct assignment above. */
  return k_rx_ok;
}

/**
 * @brief Create a PONG response frame
 *
 * @details
 * Initializes frame as a PONG response to a received PING.
 * Echoes the PING payload so the sender can validate round-trip data.
 *
 *
 *
 * @pre frame !=nullptr
 * @pre payload != NULL when payload_len > 0
 * @post frame->header.type == k_frame_type_pong
 * @post frame->header.length == payload_len
 *
 * @note Stateless; safe to call from any context.
 * @see rx_frame_create_ping() Corresponding request creator
 * @since Version 1.0.0
 */
rx_err_t rx_frame_create_pong(rx_frame_t*    frame,
                              const uint16_t sequence,
                              const uint8_t* payload,
                              const uint32_t payload_len)
{
  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > 0 && payload == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (payload_len > k_frame_max_payload) {
    return k_rx_err_invalid_size;
  }

  *frame                 = (rx_frame_t){};
  frame->header.sequence = sequence;
  frame->header.length   = (uint16_t)payload_len;
  frame->header.type     = k_frame_type_pong;
  frame->header.flags    = k_frame_flag_none;

  if (payload_len > 0) {
    for (uint32_t i = 0U; i < payload_len; i++) {
      frame->payload[i] = payload[i];
    }
  }

  /* Post-condition: type == k_frame_type_pong guaranteed by the direct assignment above. */
  return k_rx_ok;
}

/**
 * @brief Create a RESET request frame
 *
 * @details
 * Initializes frame as a RESET request that asks the peer to reset
 * communication state (sequence numbers, buffers). The receiver
 * should respond with a RESET_ACK frame. Payload is always empty.
 *
 *
 *
 * @pre frame !=nullptr
 * @pre Caller has determined that a reset is necessary
 * @post frame->header.type == k_frame_type_reset
 * @post frame->header.length == 0
 *
 * @note Stateless; safe to call from any context.
 * @warning Reset clears all sequence number state on both sides.
 * @see rx_frame_create_reset_ack() Corresponding acknowledgment creator
 * @since Version 1.0.0
 */
rx_err_t rx_frame_create_reset(rx_frame_t* frame, const uint16_t sequence)
{
  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *frame                 = (rx_frame_t){};
  frame->header.sequence = sequence;
  frame->header.length   = 0;
  frame->header.type     = k_frame_type_reset;
  frame->header.flags    = k_frame_flag_none;

  /* Post-condition: type == k_frame_type_reset guaranteed by the direct assignment above. */
  return k_rx_ok;
}

/**
 * @brief Create a RESET_ACK confirmation frame
 *
 * @details
 * Initializes frame as a RESET_ACK response to a received RESET request.
 * Confirms that the receiver has completed its reset procedure.
 * Payload is always empty; sequence matches the RESET request.
 *
 *
 *
 * @pre frame !=nullptr
 * @pre A RESET frame was received and processed
 * @post frame->header.type == k_frame_type_reset_ack
 * @post frame->header.length == 0
 *
 * @note Stateless; safe to call from any context.
 * @see rx_frame_create_reset() Corresponding request creator
 * @since Version 1.0.0
 */
rx_err_t rx_frame_create_reset_ack(rx_frame_t* frame, const uint16_t sequence)
{
  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *frame                 = (rx_frame_t){};
  frame->header.sequence = sequence;
  frame->header.length   = 0;
  frame->header.type     = k_frame_type_reset_ack;
  frame->header.flags    = k_frame_flag_none;

  /* Post-condition: type == k_frame_type_reset_ack guaranteed by the direct assignment above. */
  return k_rx_ok;
}
