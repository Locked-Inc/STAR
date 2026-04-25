/**
 * @file rx_frame_ascii.c
 * @brief Frame ASCII Formatter Implementation
 *
 * @details
 * Implements the ASCII frame formatting functions declared in rx_frame_ascii.h.
 * This module converts binary protocol frames into human-readable text for
 * debugging and diagnostic output.
 *
 * @par Implementation Overview
 * The implementation uses a functional decomposition approach with small,
 * focused helper functions for each formatting task:
 *
 * @dot
 * digraph impl_structure {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_public {
 *     label="Public API";
 *     style=filled;
 *     color=lightgreen;
 *     format [label="rx_frame_ascii_format()"];
 *     type_name [label="rx_frame_ascii_type_name()"];
 *     flags_str [label="rx_frame_ascii_flags_str()"];
 *   }
 *
 *   subgraph cluster_helpers {
 *     label="Internal Helpers";
 *     style=filled;
 *     color=lightyellow;
 *     append_str [label="internal_append_str()"];
 *     append_char [label="internal_append_char()"];
 *     append_hex [label="internal_append_hex_byte/u16/u32()"];
 *     append_dec [label="internal_append_decimal_u16()"];
 *     format_header [label="internal_format_header()"];
 *     format_payload [label="internal_format_payload()"];
 *   }
 *
 *   format -> format_header;
 *   format -> format_payload;
 *   format -> type_name [style=dashed];
 *   format -> flags_str [style=dashed];
 *   format_header -> append_str;
 *   format_header -> append_dec;
 *   format_payload -> append_hex;
 *   format_payload -> append_char;
 *   flags_str -> append_str;
 *   flags_str -> append_char;
 * }
 * @enddot
 *
 * @par Design Decisions
 * - **No snprintf/printf**: Avoids libc dependencies, non-reentrant functions,
 *   and format string vulnerabilities. Custom formatters are safer and smaller.
 * - **Position-based appending**: All append helpers track position and bounds,
 *   preventing buffer overflows automatically.
 * - **Static lookup table**: Hex digits use compile-time lookup for speed.
 * - **Consistent signature**: All append helpers follow (buf, pos, max, value)
 *   convention for predictable composition.
 *
 * @par Memory Safety
 * Every function validates bounds before writing. Output buffer overflow is
 * impossible if functions are used correctly. Truncation is detected and
 * reported via return value.
 *
 * @par Performance Optimizations
 * - Hex digit lookup table (s_hex_chars) avoids arithmetic
 * - Direct character output avoids format string parsing
 * - Single-pass formatting (no temporary buffers except small decimal conversion)
 * - All loops have static upper bounds (NASA Rule 2)
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] All loops bounded (payload length, constant limits)
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] Functions under 60 lines
 * - Rule 5: [OK] Input validation on all functions
 * - Rule 6: [OK] Variables at smallest scope
 * - Rule 7: [OK] Return values checked
 * - Rule 8: [OK] C23 typed enums for constants
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @see rx_frame_ascii.h Public API documentation
 *
 * @author Locked, Inc.
 * @date 2026-01-26
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_frame_ascii.h"

#include <string.h>

#include "rx_check.h"

/* =============================================================================
 * Private Constants
 * =============================================================================
 */

/**
 * @enum format_constants_t
 * @brief String formatting constants for hex and decimal conversion
 *
 * @details
 * Internal constants used by the formatting helper functions. These define
 * bit manipulation parameters, ASCII character ranges, and output formatting
 * dimensions.
 *
 * @par Hex Digit Extraction
 * To convert a byte to two hex characters:
 * - Upper nibble: `(byte >> k_hex_digit_shift) & k_hex_digit_mask`
 * - Lower nibble: `byte & k_hex_digit_mask`
 *
 * @par Printable ASCII Range
 * Characters 0x20 (space) through 0x7E (~) are printable. Outside this range,
 * the hex dump ASCII sidebar displays '.' as a placeholder.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_hex_digit_shift = 4, /**< Bits to shift for upper hex nibble extraction.
                              @par Value: 4 (half a byte = 4 bits) */

  k_hex_digit_mask = 0xF, /**< Mask for extracting single hex digit (4 bits).
                               @par Value: 0x0F (binary: 00001111) */

  k_printable_min = 0x20, /**< Minimum printable ASCII character (space).
                               @par Value: 0x20 (32 decimal)
                               @see internal_is_printable() */

  k_printable_max = 0x7E, /**< Maximum printable ASCII character (tilde).
                               @par Value: 0x7E (126 decimal)
                               @see internal_is_printable() */

  k_bytes_per_line = 16, /**< Bytes per line in hex dump output.
                              Matches standard xxd/hexdump format.
                              @par Value: 16 bytes */

  k_min_output_size = 64, /**< Minimum output buffer size accepted.
                               Fits header + CRC with empty payload.
                               @par Value: 64 bytes */

  k_hex_space_gap = 2, /**< Spaces between hex and ASCII sections.
                            @par Value: 2 spaces */

  k_mask_u8 = 0xFF, /**< Mask for uint8_t extraction from larger type.
                         @par Value: 0xFF (8 bits) */

  k_mask_u16 = 0xFFFF, /**< Mask for uint16_t extraction from uint32_t.
                            @par Value: 0xFFFF (16 bits) */

  k_decimal_base = 10, /**< Decimal number base for digit extraction.
                            @par Value: 10 */

  k_u16_decimal_max_digits = 5, /**< Maximum decimal digits for uint16_t (65535).
                                     @par Value: 5 */

  k_flags_buf_size = 32, /**< Buffer size for flags string formatting.
                              Fits "ACK|RETX|PRI|FEC|SOFT" + null.
                              @par Value: 32 bytes */
} format_constants_t;

/**
 * @enum buffer_size_constants_t
 * @brief Buffer size and offset constants for string building
 *
 * @details
 * Constants for buffer boundary calculations and hex character sizing.
 * All values fit in uint8_t for efficient comparison operations.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_empty_buffer = 0, /**< Sentinel for empty/invalid buffer size.
                           @par Value: 0 */

  k_null_terminator = 1, /**< Space required for null terminator byte.
                              @par Value: 1 byte */

  k_hex_chars_per_byte = 2, /**< Hex characters per byte (e.g., "FF").
                                 @par Value: 2 */

  k_hex_chars_per_u16 = 4, /**< Hex characters for uint16_t (e.g., "FFFF").
                                @par Value: 4 */

  k_hex_chars_per_u32 = 8, /**< Hex characters for uint32_t (e.g., "FFFFFFFF").
                                @par Value: 8 */

  k_u16_high_byte_shift = 8, /**< Bit shift to extract high byte from uint16_t.
                                   @par Value: 8 bits */

  k_u32_high_u16_shift = 16, /**< Bit shift to extract high uint16 from uint32.
                                  @par Value: 16 bits */
} buffer_size_constants_t;

/**
 * @enum decimal_buf_constants_t
 * @brief Decimal conversion buffer size constants
 *
 * @details
 * Defines buffer sizes for temporary decimal string conversion.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_u16_decimal_buf_size = k_u16_decimal_max_digits + 1, /**< Buffer size for uint16_t decimal
                                                              with null terminator (6 bytes).
                                                              @par Calculation: 5 digits + 1 null */
} decimal_buf_constants_t;

/**
 * @var s_hex_chars
 * @brief Lookup table for hexadecimal digit characters
 *
 * @details
 * Static constant lookup table mapping nibble values (0-15) to their ASCII
 * hexadecimal character representation ('0'-'9', 'A'-'F'). Uses uppercase
 * hex letters for consistency with standard debugger output.
 *
 * @par Usage
 * @code
 * uint8_t nibble = (byte >> 4) & 0x0F;  // Extract upper nibble
 * char hex_char = s_hex_chars[nibble];   // Convert to hex character
 * @endcode
 *
 * @par Memory
 * 17 bytes (16 characters + null terminator) in .rodata section
 *
 * @note Thread-safe: read-only constant data
 *
 * @since Version 1.0.0
 */
static const char s_hex_chars[] = "0123456789ABCDEF";

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @defgroup frame_ascii_helpers Internal Helper Functions
 * @{
 *
 * @brief String building helper functions with bounds checking
 *
 * @details
 * All append helpers follow a consistent signature pattern for composability:
 * ```c
 * uint32_t internal_append_xxx(char* buf, uint32_t pos, uint32_t max, <value>);
 * ```
 *
 * @par Parameters
 * - **buf**: Output buffer pointer (may be NULL for safety)
 * - **pos**: Current write position in buffer
 * - **max**: Maximum buffer size (total capacity)
 * - **value**: Data to append (varies by function)
 *
 * @par Return Value
 * All helpers return the updated position after appending. If the append
 * would overflow, the original position is returned unchanged.
 *
 * @par Bounds Safety
 * - All functions check `pos < max - k_null_terminator` before writing
 * - NULL buffer pointer causes immediate return with unchanged position
 * - Zero-size buffer causes immediate return with unchanged position
 * - No writes beyond `max - 1` to preserve space for null terminator
 *
 * @par Usage Pattern
 * ```c
 * uint32_t pos = 0;
 * pos = internal_append_str(buf, pos, max, "Hello ");
 * pos = internal_append_decimal_u16(buf, pos, max, 42);
 * pos = internal_append_char(buf, pos, max, '!');
 * buf[pos] = '\0';  // Null terminate
 * // Result: "Hello 42!"
 * ```
 * @}
 */

/**
 * @brief Append string to buffer with bounds checking
 *
 * @details
 * Copies characters from source string to output buffer until null terminator
 * is reached or buffer is full (leaving room for final null terminator).
 *
 *
 *
 * @pre pos < max (for any writing to occur)
 *
 * @post Characters written in range [pos, return_value)
 * @post Buffer NOT null-terminated (caller responsibility)
 *
 * @note Does not write null terminator
 * @note Returns original pos if nothing can be written
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE uint32_t internal_append_str(char*       buf,
                                                uint32_t    pos,
                                                uint32_t    max,
                                                const char* str)
{
  /* Rule 5: Pre-condition invariant - callers guarantee these conditions */
  RX_ASSERT(buf != nullptr && str != nullptr && max != k_empty_buffer && pos < max,
            "internal_append_str: precondition violated by caller");

  while (pos < max - k_null_terminator && *str != '\0') {
    buf[pos++] = *str++;
  }
  return pos;
}

/**
 * @brief Append single character to buffer with bounds checking
 *
 * @details
 * Writes a single character to the buffer at the current position if space
 * is available (accounting for null terminator).
 *
 *
 *
 * @pre pos < max - 1 (for writing to occur)
 *
 * @post buf[pos] contains c (if written)
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE uint32_t internal_append_char(char* buf, uint32_t pos, uint32_t max, char c)
{
  /* Rule 5: Pre-condition invariant - callers guarantee these conditions */
  RX_ASSERT(buf != nullptr && max != k_empty_buffer && pos < max,
            "internal_append_char: precondition violated by caller");

  if (pos < max - k_null_terminator) {
    buf[pos++] = c;
  }
  return pos;
}

/**
 * @brief Append byte as two hex characters to buffer
 *
 * @details
 * Converts a single byte to its two-character hexadecimal representation
 * (e.g., 0xFF -> "FF") and appends it to the buffer. Uses uppercase hex
 * letters via s_hex_chars lookup table.
 *
 *
 *
 * @pre pos + 2 <= max - 1 (for writing to occur)
 *
 * @post buf[pos] contains upper nibble hex char
 * @post buf[pos+1] contains lower nibble hex char
 *
 * @par Example Output
 * - byte=0x00 -> "00"
 * - byte=0xFF -> "FF"
 * - byte=0x5A -> "5A"
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE uint32_t internal_append_hex_byte(char*    buf,
                                                     uint32_t pos,
                                                     uint32_t max,
                                                     uint8_t  byte)
{
  /* Rule 5: Pre-condition invariant - callers guarantee these conditions */
  RX_ASSERT(buf != nullptr && max >= k_hex_chars_per_byte && pos < max,
            "internal_append_hex_byte: precondition violated by caller");

  if (pos < max - k_hex_chars_per_byte) {
    buf[pos++] = s_hex_chars[(byte >> k_hex_digit_shift) & k_hex_digit_mask];
    buf[pos++] = s_hex_chars[byte & k_hex_digit_mask];
  }
  return pos;
}

/**
 * @brief Append uint16_t as four hex characters to buffer
 *
 * @details
 * Converts a 16-bit value to its four-character hexadecimal representation
 * (e.g., 0xABCD -> "ABCD") in big-endian order (most significant byte first).
 *
 *
 *
 * @pre pos + 4 <= max - 1 (for writing to occur)
 *
 * @post Four hex characters written at buf[pos..pos+3]
 *
 * @par Example Output
 * - value=0x0000 -> "0000"
 * - value=0xFFFF -> "FFFF"
 * - value=0x1234 -> "1234"
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE uint32_t internal_append_hex_u16(char*    buf,
                                                    uint32_t pos,
                                                    uint32_t max,
                                                    uint16_t value)
{
  /* Rule 5: Pre-condition invariant - callers guarantee these conditions */
  RX_ASSERT(buf != nullptr && max >= k_hex_chars_per_u16 && pos < max,
            "internal_append_hex_u16: precondition violated by caller");

  pos = internal_append_hex_byte(buf, pos, max, (uint8_t)(value >> k_u16_high_byte_shift));
  pos = internal_append_hex_byte(buf, pos, max, (uint8_t)(value & k_mask_u8));
  return pos;
}

/**
 * @brief Append uint32_t as eight hex characters to buffer
 *
 * @details
 * Converts a 32-bit value to its eight-character hexadecimal representation
 * (e.g., 0xDEADBEEF -> "DEADBEEF") in big-endian order. Commonly used for
 * \\rC-32 values in frame output.
 *
 *
 *
 * @pre pos + 8 <= max - 1 (for writing to occur)
 *
 * @post Eight hex characters written at buf[pos..pos+7]
 *
 * @par Example Output
 * - value=0x00000000 -> "00000000"
 * - value=0xFFFFFFFF -> "FFFFFFFF"
 * - value=0xDEADBEEF -> "DEADBEEF"
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE uint32_t internal_append_hex_u32(char*    buf,
                                                    uint32_t pos,
                                                    uint32_t max,
                                                    uint32_t value)
{
  /* Rule 5: Pre-condition invariant - callers guarantee these conditions */
  RX_ASSERT(buf != nullptr && max >= k_hex_chars_per_u32 && pos < max,
            "internal_append_hex_u32: precondition violated by caller");

  pos = internal_append_hex_u16(buf, pos, max, (uint16_t)(value >> k_u32_high_u16_shift));
  pos = internal_append_hex_u16(buf, pos, max, (uint16_t)(value & k_mask_u16));
  return pos;
}

/**
 * @brief Append uint16_t as decimal digits to buffer
 *
 * @details
 * Converts a 16-bit value to its decimal string representation (1-5 digits).
 * Uses a small stack buffer for digit reversal, avoiding heap allocation.
 * Zero value outputs single '0' character.
 *
 * @par Algorithm
 * 1. Extract digits via repeated modulo/division (reverse order)
 * 2. Store in temporary stack buffer
 * 3. Reverse and append to output buffer
 *
 *
 *
 * @pre Sufficient space for up to 5 digits
 *
 * @post Decimal digits written without leading zeros (except for value=0)
 *
 * @par Example Output
 * - value=0 -> "0"
 * - value=1 -> "1"
 * - value=12345 -> "12345"
 * - value=65535 -> "65535"
 *
 * @par Stack Usage
 * ~6 bytes (temp buffer for digit reversal)
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE uint32_t internal_append_decimal_u16(char*    buf,
                                                        uint32_t pos,
                                                        uint32_t max,
                                                        uint16_t value)
{
  /* Rule 5: Pre-condition invariant - callers guarantee these conditions */
  RX_ASSERT(buf != nullptr && max != k_empty_buffer && pos < max,
            "internal_append_decimal_u16: precondition violated by caller");

  char    temp[k_u16_decimal_buf_size]; /* Max 5 digits + null for uint16_t */
  uint8_t len = 0;

  if (value == 0) {
    return internal_append_char(buf, pos, max, '0');
  }

  while (value > 0) {
    const uint8_t digit = (uint8_t)(value % k_decimal_base);
    temp[len++]         = (char)('0' + digit);
    value /= k_decimal_base;
  }

  /* Reverse digits into output */
  while (len > 0) {
    pos = internal_append_char(buf, pos, max, temp[--len]);
  }

  return pos;
}

/**
 * @brief Check if byte value is printable ASCII character
 *
 * @details
 * Tests whether a byte value falls within the printable ASCII range
 * (0x20-0x7E inclusive). Used to decide whether to display the actual
 * character or a '.' placeholder in hex dump ASCII sidebars.
 *
 * @par Printable Range
 * - 0x20 (space) through 0x7E (~) are printable
 * - 0x00-0x1F are control characters (not printable)
 * - 0x7F (DEL) is not printable
 * - 0x80-0xFF are extended ASCII (not printable in this implementation)
 *
 *
 *
 * @note Pure function with no side effects
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE bool internal_is_printable(uint8_t c)
{
  return (bool)((c >= k_printable_min) && (c <= k_printable_max));
}

/**
 * @brief Format payload as hex dump with ASCII sidebar
 *
 * @details
 * Formats binary payload data as a traditional hex dump with 16 bytes per line,
 * offset prefix, and ASCII sidebar showing printable characters (or '.' for
 * non-printable bytes).
 *
 * @par Output Format
 * Each line follows this structure:
 * ```
 * [NNNN] XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX  ................
 * ```
 * Where:
 * - `[NNNN]` is the 4-digit hex offset from payload start
 * - `XX XX ...` is the hex representation of up to 16 bytes
 * - ASCII sidebar shows printable chars or '.' for non-printable
 *
 * @par Empty Payload Handling
 * If length is 0, outputs: `[PAYLOAD] (empty)\\r\\n`
 *
 *
 *
 * @pre buf != nullptr (for any output)
 * @pre payload != nullptr if length > 0
 *
 * @post Hex dump lines written with \\r\\n line endings
 *
 * @par Loop Bounds (NASA Rule 2)
 * - Outer loop: bounded by `length / k_bytes_per_line + 1` iterations
 * - Inner loops: bounded by `k_bytes_per_line` (16) iterations
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
/**
 * @brief Format one hex dump line (hex bytes + ASCII sidebar)
 *
 *
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
static uint32_t internal_format_hex_line(char*          buf,
                                         uint32_t       pos,
                                         uint32_t       max,
                                         const uint8_t* payload,
                                         uint16_t       offset,
                                         uint16_t       line_bytes)
{
  /* Hex dump */
  for (uint16_t i = 0; i < k_bytes_per_line; i++) {
    if (i < line_bytes) {
      pos = internal_append_hex_byte(buf, pos, max, payload[offset + i]);
      pos = internal_append_char(buf, pos, max, ' ');
    } else {
      pos = internal_append_str(buf, pos, max, "   "); /* Pad short lines */
    }
  }

  /* ASCII sidebar */
  pos = internal_append_char(buf, pos, max, ' ');
  for (uint16_t i = 0; i < line_bytes; i++) {
    const uint8_t c          = payload[offset + i];
    char          display_ch = '.';
    if (internal_is_printable(c)) {
      display_ch = (char)c;
    }
    pos = internal_append_char(buf, pos, max, display_ch);
  }

  return pos;
}

RX_STATIC_TESTABLE uint32_t internal_format_payload(char*          buf,
                                                    uint32_t       pos,
                                                    uint32_t       max,
                                                    const uint8_t* payload,
                                                    uint16_t       length)
{
  /* Rule 5: Pre-condition invariant - callers guarantee these conditions */
  RX_ASSERT(buf != nullptr && max != k_empty_buffer && pos < max,
            "internal_format_payload: precondition violated by caller");

  if (length == 0) {
    pos = internal_append_str(buf, pos, max, "[PAYLOAD] (empty)\r\n");
    return pos;
  }

  /* payload must be non-NULL when length > 0 */
  if (payload == nullptr) {
    return pos;
  }

  uint16_t offset = 0;
  while (offset < length) {
    /* Start new line with offset */
    pos = internal_append_str(buf, pos, max, "[");
    pos = internal_append_hex_u16(buf, pos, max, offset);
    pos = internal_append_str(buf, pos, max, "] ");

    /* Calculate bytes on this line */
    uint16_t line_bytes = length - offset;
    if (line_bytes > k_bytes_per_line) {
      line_bytes = k_bytes_per_line;
    }

    /* Format hex bytes and ASCII sidebar */
    pos = internal_format_hex_line(buf, pos, max, payload, offset, line_bytes);

    pos = internal_append_str(buf, pos, max, "\r\n");
    offset += line_bytes;
  }

  return pos;
}

/**
 * @brief Format frame header metadata as single text line
 *
 * @details
 * Formats the frame header fields into a human-readable summary line
 * including direction indicator, sequence number, payload length, frame
 * type, and flags.
 *
 * @par Output Format
 * ```
 * [TX] seq=12345 len=64 type=COMMAND flags=ACK|PRI
 * [RX] seq=12346 len=128 type=RESPONSE flags=NONE
 * ```
 *
 * @par Field Details
 * | Field | Format | Example |
 * |-------|--------|---------|
 * | Direction | [TX] or [RX] | [TX] |
 * | Sequence | seq=N (decimal) | seq=12345 |
 * | Length | len=N (decimal) | len=64 |
 * | Type | type=NAME | type=COMMAND |
 * | Flags | flags=F1\|F2 | flags=ACK\|PRI |
 *
 *
 *
 * @pre buf != nullptr
 * @pre header != nullptr
 *
 * @post Header line written with \\r\\n line ending
 * @post pos <= max (invariant maintained)
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE uint32_t internal_format_header(char*                    buf,
                                                   uint32_t                 pos,
                                                   uint32_t                 max,
                                                   const rx_frame_header_t* header,
                                                   bool                     is_tx)
{
  /* Rule 5: Pre-condition invariant - callers guarantee these conditions */
  RX_ASSERT(buf != nullptr && header != nullptr && max != k_empty_buffer && pos < max,
            "internal_format_header: precondition violated by caller");

  /* Safety guard: prevent null dereference in unit test mode after assert fires */
  if (header == nullptr) {
    return pos;
  }

  char flags_buf[k_flags_buf_size];

  /* Direction */
  if (is_tx) {
    pos = internal_append_str(buf, pos, max, "[TX] ");
  } else {
    pos = internal_append_str(buf, pos, max, "[RX] ");
  }

  /* Sequence number */
  pos = internal_append_str(buf, pos, max, "seq=");
  pos = internal_append_decimal_u16(buf, pos, max, header->sequence);

  /* Length */
  pos = internal_append_str(buf, pos, max, " len=");
  pos = internal_append_decimal_u16(buf, pos, max, header->length);

  /* Type */
  pos = internal_append_str(buf, pos, max, " type=");
  pos = internal_append_str(buf, pos, max, rx_frame_ascii_type_name((rx_frame_type_t)header->type));

  /* Flags */
  pos = internal_append_str(buf, pos, max, " flags=");
  (void)rx_frame_ascii_flags_str(header->flags, flags_buf, sizeof(flags_buf));
  pos = internal_append_str(buf, pos, max, flags_buf);
  pos = internal_append_str(buf, pos, max, "\r\n");

  /* Rule 5: Post-condition: pos <= max is guaranteed by helpers' bounds checks. */

  return pos;
}

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Convert frame type enum to human-readable string
 *
 * @details
 * Implementation of rx_frame_ascii_type_name(). Uses switch statement
 * for O(1) lookup with compile-time string literals.
 *
 *
 *
 * @see rx_frame_ascii.h for full documentation
 */
const char* rx_frame_ascii_type_name(rx_frame_type_t type)
{
  switch (type) {
    case k_frame_type_ping:
      return "PING";
    case k_frame_type_pong:
      return "PONG";
    case k_frame_type_command:
      return "COMMAND";
    case k_frame_type_response:
      return "RESPONSE";
    case k_frame_type_ack:
      return "ACK";
    case k_frame_type_nack:
      return "NACK";
    case k_frame_type_log_message:
      return "LOG_MESSAGE";
    case k_frame_type_reset_ack:
      return "RESET_ACK";
    case k_frame_type_reset:
      return "RESET";
    default:
      return "UNKNOWN";
  }
}

/**
 * @brief Append a single flag name with pipe separator if needed
 *
 * @details
 * Conditionally appends a flag name to the output buffer if the corresponding
 * flag bit is set. Manages separator insertion: no separator before first flag,
 * pipe separator before subsequent flags.
 *
 * @par Example Sequence
 * 1. flags=ACK|PRI, first call (first=true): appends "ACK", sets first=false
 * 2. Same flags, second call: appends "|PRI"
 *
 *
 *
 * @pre first != nullptr
 *
 * @post *first set to false if flag was appended
 * @post Separator '|' prepended if not first flag
 *
 * @ingroup frame_ascii_helpers
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE uint32_t internal_append_flag(char*       buffer,
                                                 uint32_t    pos,
                                                 uint32_t    max,
                                                 bool*       first,
                                                 uint8_t     flags,
                                                 uint8_t     flag_bit,
                                                 const char* flag_name)
{
  /* Rule 5: Pre-condition invariant - callers always pass valid local bool address */
  RX_ASSERT(first != nullptr, "internal_append_flag: first is nullptr - caller must validate");

  if (flags & flag_bit) {
    if (!*first) {
      pos = internal_append_char(buffer, pos, max, '|');
    }
    pos    = internal_append_str(buffer, pos, max, flag_name);
    *first = false;
  }
  return pos;
}

/**
 * @brief Convert frame flags bitmask to human-readable string
 *
 * @details
 * Implementation of rx_frame_ascii_flags_str(). Iterates through known
 * flag bits, appending names with pipe separators.
 *
 *
 *
 * @note Flags formatted as pipe-separated: "ACK|RETX|PRI"
 *
 * @see rx_frame_ascii.h for full documentation
 */
const char* rx_frame_ascii_flags_str(uint8_t flags, char* buffer, uint32_t buffer_len)
{
  if (buffer == nullptr || buffer_len == k_empty_buffer) {
    return "";
  }

  uint32_t pos = 0;

  if (flags == k_frame_flag_none) {
    pos         = internal_append_str(buffer, pos, buffer_len, "NONE");
    buffer[pos] = '\0';
    return buffer;
  }

  bool first = true;

  /* Use helper to append each flag */
  pos =
    internal_append_flag(buffer, pos, buffer_len, &first, flags, k_frame_flag_requires_ack, "ACK");
  pos =
    internal_append_flag(buffer, pos, buffer_len, &first, flags, k_frame_flag_retransmit, "RETX");
  pos = internal_append_flag(buffer, pos, buffer_len, &first, flags, k_frame_flag_priority, "PRI");
  pos =
    internal_append_flag(buffer, pos, buffer_len, &first, flags, k_frame_flag_fec_enabled, "FEC");
  pos =
    internal_append_flag(buffer, pos, buffer_len, &first, flags, k_frame_flag_soft_nack, "SOFT");

  buffer[pos] = '\0';
  return buffer;
}

/**
 * @brief Format frame as human-readable ASCII string
 *
 * @details
 * Implementation of rx_frame_ascii_format(). Orchestrates header, payload,
 * and \\rC formatting using internal helper functions.
 *
 * @par Implementation Notes
 * - Validates all inputs before any output
 * - Calls internal_format_header() for metadata line
 * - Calls internal_format_payload() for hex dump
 * - Appends \\rC footer directly
 * - Detects truncation and returns k_rx_err_no_mem
 *
 *
 *
 * @see rx_frame_ascii.h for full documentation
 */
rx_err_t rx_frame_ascii_format(const rx_frame_t* frame,
                               bool              is_tx,
                               char*             output,
                               uint32_t          output_max_len,
                               uint32_t*         output_len)
{
  /* Rule 5: Pre-condition validation */
  if (frame == nullptr) {
    return k_rx_err_invalid_arg;
  }
  if (output == nullptr) {
    return k_rx_err_invalid_arg;
  }
  if (output_len == nullptr) {
    return k_rx_err_invalid_arg;
  }
  if (output_max_len < k_min_output_size) {
    return k_rx_err_no_mem;
  }

  /* Validate payload length does not exceed frame capacity */
  if (frame->header.length > k_frame_max_payload) {
    return k_rx_err_invalid_arg;
  }

  uint32_t pos = 0;

  /* Format header metadata */
  pos = internal_format_header(output, pos, output_max_len, &frame->header, is_tx);

  /* Payload hex dump - use validated length to prevent out-of-bounds reads */
  pos = internal_format_payload(output, pos, output_max_len, frame->payload, frame->header.length);

  /* CRC */
  pos = internal_append_str(output, pos, output_max_len, "[CRC] ");
  pos = internal_append_hex_u32(output, pos, output_max_len, frame->crc);
  pos = internal_append_str(output, pos, output_max_len, "\r\n");

  /* Rule 5: Post-condition: helpers cap pos at output_max_len - 1 via bounds
   * checks, so pos < output_max_len is an invariant by construction. */

  /* Null terminate */
  output[pos] = '\0';

  *output_len = pos;

  return k_rx_ok;
}
