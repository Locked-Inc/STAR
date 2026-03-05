/**
 * @file rx_frame_ascii.h
 * @brief Frame ASCII Formatter for Human-Readable Frame Dumps
 *
 * @details
 * Converts binary protocol frames into human-readable ASCII text for debugging
 * and diagnostic output. This module provides a debug visualization layer that
 * transforms the binary frame protocol (sync, headers, payload, CRC) into a
 * structured text format suitable for terminal viewing or log analysis.
 *
 * @par System Architecture
 * @dot
 * digraph frame_ascii_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_input {
 *     label="Input Sources";
 *     style=filled;
 *     color=lightblue;
 *     usb_rx [label="USB RX Frame"];
 *     usb_tx [label="USB TX Frame"];
 *     spi_rx [label="SPI RX Frame"];
 *     spi_tx [label="SPI TX Frame"];
 *   }
 *
 *   subgraph cluster_formatter {
 *     label="rx_frame_ascii (This Module)";
 *     style=filled;
 *     color=lightyellow;
 *     format [label="rx_frame_ascii_format()"];
 *     type_name [label="rx_frame_ascii_type_name()"];
 *     flags_str [label="rx_frame_ascii_flags_str()"];
 *   }
 *
 *   subgraph cluster_output {
 *     label="Output Destinations";
 *     style=filled;
 *     color=lightgreen;
 *     usb_debug [label="USB Debug Port\n/dev/ttyACM1"];
 *     uart_log [label="UART Debug Log"];
 *     buffer [label="RAM Buffer\nfor Analysis"];
 *   }
 *
 *   usb_rx -> format;
 *   usb_tx -> format;
 *   spi_rx -> format;
 *   spi_tx -> format;
 *   format -> usb_debug;
 *   format -> uart_log;
 *   format -> buffer;
 *   format -> type_name [style=dashed];
 *   format -> flags_str [style=dashed];
 * }
 * @enddot
 *
 * @par Output Format
 * The formatter produces structured text with three sections:
 * ```
 * [RX] seq=1234 len=10 type=COMMAND flags=ACK|PRI
 * [0000] 48 65 6C 6C 6F 20 55 53 42 21 00 00 00 00 00 00  Hello USB!......
 * [CRC] 12345678
 * ```
 *
 * @par Section Details
 * | Section | Description | Format |
 * |---------|-------------|--------|
 * | Header  | Direction, sequence, length, type, flags | `[TX/RX] seq=N len=N type=NAME flags=...` |
 * | Payload | Hex dump with ASCII sidebar | `[NNNN] XX XX XX...  ASCII...` |
 * | CRC     | 32-bit CRC value | `[CRC] XXXXXXXX` |
 *
 * @par Performance Characteristics
 * | Metric | Value | Notes |
 * |--------|-------|-------|
 * | Format time (64B payload) | ~50 us | At 240 MHz |
 * | Format time (2KB payload) | ~500 us | At 240 MHz |
 * | Stack usage | ~100 bytes | Fixed, no recursion |
 * | Output size ratio | ~4:1 | Output ~4x input size |
 *
 * @par Memory Usage
 * | Component | Size | Description |
 * |-----------|------|-------------|
 * | Code size | ~2 KB | Text section |
 * | Static data | ~32 B | Hex lookup table |
 * | Stack (format) | ~100 B | Per-call stack frame |
 * | Output buffer | User-provided | Typically 2048 bytes |
 *
 * @par Design Rationale
 * - **No dynamic allocation**: All buffers provided by caller (NASA Rule 3)
 * - **Bounded execution**: All loops have static upper bounds (NASA Rule 2)
 * - **No stdio dependency**: Custom formatters avoid printf overhead and non-reentrant issues
 * - **Hex + ASCII format**: Standard debugger-style output for easy payload inspection
 * - **Direction indicator**: TX/RX prefix distinguishes send vs receive for protocol tracing
 *
 * @par Thread Safety
 * All functions are **thread-safe** and **reentrant**:
 * - No static mutable state
 * - All state passed via parameters
 * - Pure functions (same inputs -> same outputs)
 *
 * @par Module Dependencies
 * - rx_frame.h: Frame structure definitions (rx_frame_t, rx_frame_header_t)
 * - rx_err.h: Error code definitions
 * - stdint.h: Fixed-width integer types
 * - stdbool.h: Boolean type
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] All loops bounded by payload length or constant
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] Functions are concise (<60 lines)
 * - Rule 5: [OK] Input validation on all public functions
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Constants use C23 typed enums
 * - Rule 9: [OK] No function pointers (pure computation)
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S (SRP):** ASCII formatting only, no I/O operations
 * - **O (OCP):** Flag/type tables easily extended without modifying logic
 * - **L (LSP):** N/A (no inheritance hierarchy)
 * - **I (ISP):** Three focused functions for different formatting needs
 * - **D (DIP):** Depends only on frame abstractions, not concrete drivers
 *
 * @see rx_frame.h Frame structure definitions
 * @see rx_usb.h USB CDC output for debug port
 * @see docs/sections/01_nanopb_protocol.tex Protocol specification
 *
 * @author Locked, Inc.
 * @date 2026-01-26
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @enum rx_frame_ascii_constants_t
 * @brief ASCII formatter configuration constants
 *
 * @details
 * Compile-time constants for buffer sizing, line formatting, and output
 * structure. These values are tuned for typical debugging scenarios with
 * protobuf message payloads up to 2KB.
 *
 * @par Buffer Sizing Rationale
 * - Output is approximately 4x input size due to hex expansion + ASCII sidebar
 * - 2048 byte max output handles ~500 byte payloads comfortably
 * - 16 bytes per line provides standard hex dump format (same as xxd, hexdump)
 *
 * @par Memory Impact
 * These constants only affect stack usage when output buffer is provided by
 * caller. No static buffers are allocated based on these values.
 *
 * @invariant k_frame_ascii_max_output >= 4 * k_frame_max_payload + headers
 *
 * @see rx_frame_ascii_format() Uses these constants for buffer validation
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Maximum recommended output buffer size in bytes
   * @details
   * Recommended minimum buffer size for rx_frame_ascii_format() to handle
   * maximum-size frames without truncation. Accounts for header line, hex
   * dump lines (4 chars per byte + ASCII sidebar), and CRC footer.
   * @par Value: 2048 bytes
   * @par Calculation: ~500 payload x 4 expansion + 64 header + 32 CRC
   */
  k_frame_ascii_max_output = 2048,

  /**
   * @brief Bytes per line in hex payload dump
   * @details
   * Standard hex dump format with 16 bytes per line. Produces output like:
   * `[0000] 00 01 02 ... 0F  ................`
   * @par Value: 16 bytes (standard format)
   * @note Matches xxd and hexdump default format
   */
  k_frame_ascii_hex_per_line = 16,

  /**
   * @brief Maximum header line length in characters
   * @details
   * Buffer size for the header metadata line containing direction, sequence,
   * length, type, and flags. Example: `[RX] seq=65535 len=2048 type=COMMAND flags=ACK|PRI|RETX`
   * @par Value: 64 characters
   */
  k_frame_ascii_header_len = 64,

  /**
   * @brief Maximum CRC line length in characters
   * @details
   * Buffer size for the CRC footer line. Format: `[CRC] XXXXXXXX`
   * @par Value: 32 characters
   */
  k_frame_ascii_crc_len = 32,
} rx_frame_ascii_constants_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Format binary frame as human-readable ASCII
 *
 * @details
 * Converts a complete frame structure into a formatted ASCII string suitable
 * for debugging output, logging, or terminal display. The output is structured
 * in three sections: header metadata, hex payload dump with ASCII sidebar, and
 * CRC footer.
 *
 * @par Algorithm Steps
 * 1. Validate all input pointers and buffer size
 * 2. Format header line: direction, sequence, length, type, flags
 * 3. Format payload as hex dump with 16 bytes per line + ASCII sidebar
 * 4. Format CRC footer with 8-digit hex value
 * 5. Null-terminate output and return length
 *
 * @par Output Structure
 * ```
 * [TX] seq=1234 len=10 type=COMMAND flags=ACK|PRI
 * [0000] 48 65 6C 6C 6F 20 55 53 42 21 00 00 00 00 00 00  Hello USB!......
 * [CRC] 12345678
 * ```
 *
 * @dot
 * digraph format_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="Start"];
 *   validate [label="Validate params"];
 *   header [label="Format header"];
 *   payload [label="Format payload\n(hex + ASCII)"];
 *   crc [label="Format CRC"];
 *   terminate [label="Null terminate"];
 *   done [label="Return k_rx_ok" style=filled, fillcolor=lightgreen];
 *   error [label="Return error" style=filled, fillcolor=lightyellow];
 *   start -> validate;
 *   validate -> header [label="valid"];
 *   validate -> error [label="invalid"];
 *   header -> payload;
 *   payload -> crc;
 *   crc -> terminate;
 *   terminate -> done;
 * }
 * @enddot
 *
 * @param[in] frame Frame to format
 *   - Must be valid non-nullptr
 *   - header.length must be <= k_frame_max_payload
 *   - payload data must be valid for header.length bytes
 * @param[in] is_tx Direction indicator
 *   - true: Prefixes output with "[TX] " (transmit direction)
 *   - false: Prefixes output with "[RX] " (receive direction)
 * @param[out] output Output buffer for ASCII text
 *   - Must be valid non-nullptr
 *   - Caller maintains ownership
 *   - Will be null-terminated on success
 * @param[in] output_max_len Maximum bytes to write to output buffer
 *   - Must be >= 64 (minimum for header + CRC with empty payload)
 *   - Recommended: k_frame_ascii_max_output (2048)
 *   - Larger payloads require proportionally larger buffers
 * @param[out] output_len Actual bytes written (excluding null terminator)
 *   - Must be valid non-nullptr
 *   - Set only on success
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Frame formatted successfully
 * @retval k_rx_err_invalid_arg frame, output, or output_len is nullptr,
 *         or frame->header.length > k_frame_max_payload
 * @retval k_rx_err_no_mem Output buffer too small (< 64 bytes or truncation occurred)
 *
 * @pre frame != nullptr
 * @pre output != nullptr
 * @pre output_len != nullptr
 * @pre output_max_len >= 64
 * @pre frame->header.length <= k_frame_max_payload
 *
 * @post output is null-terminated
 * @post *output_len contains actual output length (on success)
 * @post output buffer contains formatted ASCII text (on success)
 *
 * @invariant Output never exceeds output_max_len bytes
 *
 * @note Thread-safe: no static mutable state
 * @note Reentrant: all state passed via parameters
 *
 * @warning Truncation returns k_rx_err_no_mem; output may be partial
 * @warning Empty payload (length=0) produces `[PAYLOAD] (empty)` line
 *
 * @par Basic Usage Example
 * @code
 * rx_frame_t frame;
 * char output[2048];
 * uint32_t output_len;
 *
 * // Receive a frame from USB or SPI
 * rx_usb_comm_receive(&handle, &frame, 100);
 *
 * // Format for debug output
 * rx_err_t err = rx_frame_ascii_format(&frame, false, output, sizeof(output), &output_len);
 * if (err == k_rx_ok) {
 *     // Send to debug USB port
 *     rx_usb_write(k_usb_port_debug, (const uint8_t*)output, output_len);
 * }
 * @endcode
 *
 * @par Error Handling Example
 * @code
 * rx_err_t err = rx_frame_ascii_format(&frame, true, buf, sizeof(buf), &len);
 * switch (err) {
 *     case k_rx_ok:
 *         // Success - output contains formatted frame
 *         break;
 *     case k_rx_err_invalid_arg:
 *         rx_log_error("ASCII", "Invalid frame or buffer pointer");
 *         break;
 *     case k_rx_err_no_mem:
 *         rx_log_warn("ASCII", "Buffer too small, output truncated");
 *         break;
 *     default:
 *         rx_log_error("ASCII", "Unexpected error");
 *         break;
 * }
 * @endcode
 *
 * @par Performance
 * - Execution time: ~50 us for 64-byte payload @ 240 MHz
 * - Stack usage: ~100 bytes (fixed)
 * - No heap allocation
 *
 * @see rx_frame_ascii_type_name() Convert type enum to string
 * @see rx_frame_ascii_flags_str() Convert flags to string
 * @see rx_frame_t Frame structure definition
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_frame_ascii_format(const rx_frame_t* frame,
                                             bool              is_tx,
                                             char*             output,
                                             uint32_t          output_max_len,
                                             uint32_t*         output_len);

/**
 * @brief Get frame type as human-readable string
 *
 * @details
 * Converts a frame type enumeration value to its corresponding string name
 * for display or logging. Returns a static string literal that does not
 * require freeing.
 *
 * @par Type Mapping
 * | Type Value | String Output |
 * |------------|---------------|
 * | k_frame_type_ping (0x00) | "PING" |
 * | k_frame_type_pong (0x01) | "PONG" |
 * | k_frame_type_command (0x10) | "COMMAND" |
 * | k_frame_type_response (0x11) | "RESPONSE" |
 * | k_frame_type_ack (0x12) | "ACK" |
 * | k_frame_type_nack (0x13) | "NACK" |
 * | k_frame_type_reset_ack (0xFE) | "RESET_ACK" |
 * | k_frame_type_reset (0xFF) | "RESET" |
 * | (any other value) | "UNKNOWN" |
 *
 * @param[in] type Frame type enumeration value
 *   - Any rx_frame_type_t value
 *   - Unknown values return "UNKNOWN"
 *
 * @return Pointer to static string literal
 * @retval "PING" For k_frame_type_ping
 * @retval "PONG" For k_frame_type_pong
 * @retval "COMMAND" For k_frame_type_command
 * @retval "RESPONSE" For k_frame_type_response
 * @retval "ACK" For k_frame_type_ack
 * @retval "NACK" For k_frame_type_nack
 * @retval "RESET_ACK" For k_frame_type_reset_ack
 * @retval "RESET" For k_frame_type_reset
 * @retval "UNKNOWN" For unrecognized values
 *
 * @pre None (pure function, always valid)
 *
 * @post Return value is non-nullptr to null-terminated string
 * @post Return value points to static read-only memory
 *
 * @note Thread-safe: returns pointer to static const data
 * @note Reentrant: no mutable state
 * @note Do NOT free the returned pointer
 *
 * @par Example
 * @code
 * rx_frame_type_t type = k_frame_type_command;
 * const char* name = rx_frame_ascii_type_name(type);
 * // name == "COMMAND"
 *
 * // Safe with invalid values
 * const char* unknown = rx_frame_ascii_type_name((rx_frame_type_t)0xFF);
 * // unknown == "UNKNOWN"
 * @endcode
 *
 * @par Performance
 * - Execution time: O(1) - switch statement
 * - Stack usage: 0 bytes additional
 *
 * @see rx_frame_type_t Frame type enumeration
 * @see rx_frame_ascii_format() Uses this function internally
 *
 * @since Version 1.0.0
 */
const char* rx_frame_ascii_type_name(rx_frame_type_t type);

/**
 * @brief Get frame flags as human-readable string
 *
 * @details
 * Formats a flags bitmask into a compact pipe-separated string showing all
 * active flags. If no flags are set (value == 0), returns "NONE".
 *
 * @par Flag Mapping
 * | Flag Bit | String | Description |
 * |----------|--------|-------------|
 * | k_frame_flag_requires_ack (0x01) | "ACK" | Acknowledgment required |
 * | k_frame_flag_retransmit (0x02) | "RETX" | Retransmission |
 * | k_frame_flag_priority (0x04) | "PRI" | High priority |
 * | k_frame_flag_fec_enabled (0x08) | "FEC" | FEC encoding |
 * | k_frame_flag_soft_nack (0x10) | "SOFT" | Soft NACK |
 * | k_frame_flag_none (0x00) | "NONE" | No flags set |
 *
 * @par Output Examples
 * | Input Flags | Output String |
 * |-------------|---------------|
 * | 0x00 | "NONE" |
 * | 0x01 | "ACK" |
 * | 0x03 | "ACK|RETX" |
 * | 0x07 | "ACK|RETX|PRI" |
 * | 0x1F | "ACK|RETX|PRI|FEC|SOFT" |
 *
 * @param[in] flags Frame flags bitmask
 *   - Value from rx_frame_flags_t or combination thereof
 *   - 0 produces "NONE"
 * @param[out] buffer Output buffer for formatted string
 *   - Must be valid non-nullptr
 *   - Caller maintains ownership
 *   - Will be null-terminated
 * @param[in] buffer_len Size of output buffer in bytes
 *   - Minimum recommended: 32 bytes
 *   - Must be > 0
 *
 * @return Pointer to buffer with formatted flags string
 * @retval buffer On success (contains formatted string)
 * @retval "" (empty string) If buffer is nullptr or buffer_len is 0
 *
 * @pre buffer != nullptr (or returns empty string)
 * @pre buffer_len > 0 (or returns empty string)
 *
 * @post buffer is null-terminated (if valid)
 * @post Return value == buffer (if valid)
 *
 * @note Thread-safe: output written to caller-provided buffer
 * @note Reentrant: no static mutable state
 *
 * @warning Buffer must be large enough for all flags + separators
 * @warning Returns empty string literal "" if buffer is nullptr
 *
 * @par Basic Usage Example
 * @code
 * char flags_buf[32];
 * uint8_t flags = k_frame_flag_requires_ack | k_frame_flag_priority;
 *
 * const char* str = rx_frame_ascii_flags_str(flags, flags_buf, sizeof(flags_buf));
 * // str == "ACK|PRI"
 * // flags_buf contains "ACK|PRI\0"
 * @endcode
 *
 * @par Error Handling Example
 * @code
 * // NULL buffer returns empty string
 * const char* empty = rx_frame_ascii_flags_str(0x01, nullptr, 0);
 * // empty == ""
 *
 * // Zero-length buffer returns empty string
 * char tiny[1];
 * const char* also_empty = rx_frame_ascii_flags_str(0x01, tiny, 0);
 * // also_empty == ""
 * @endcode
 *
 * @par Performance
 * - Execution time: O(n) where n = number of flag bits checked
 * - Stack usage: ~10 bytes
 *
 * @see rx_frame_flags_t Flag definitions
 * @see rx_frame_ascii_format() Uses this function internally
 *
 * @since Version 1.0.0
 */
const char* rx_frame_ascii_flags_str(uint8_t flags, char* buffer, uint32_t buffer_len);

#ifdef __cplusplus
}
#endif
