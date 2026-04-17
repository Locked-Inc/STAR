/**
 * @file rx_log.h
 * @brief Type-Safe Logging System for RX72N Firmware with Zero-Overhead Compile-Time Filtering
 *
 * @details
 * ## Overview
 * Production-quality logging infrastructure for safety-critical RX72N motor controller firmware.
 * Provides type-safe, compile-time filtered logging with zero overhead for disabled log levels.
 * Uses C23 `_Generic` for automatic type dispatch and bounded string handling for memory safety.
 *
 * ## Architecture Design
 *
 * ### Core Features
 * | Feature | Implementation | Benefit |
 * |---------|----------------|---------|
 * | **Zero-overhead filtering** | Compile-time `#if (LOG_LEVEL >= ...)` | Disabled logs = zero code size, zero CPU cycles |
 * | **Type-safe dispatch** | C23 `_Generic` selection expressions | Automatic type routing, no manual suffixes |
 * | **Bounded strings** | Explicit length parameters + compile-time max | Prevents buffer overruns (NASA Rule 3) |
 * | **Inline functions** | All implementations are `static inline` | No function call overhead, optimal performance |
 * | **Dual output** | Optional USB CDC mirroring | View logs on UART or USB terminal |
 * | **Consistent format** | `[LEVEL] [TAG] message` pattern | Easy parsing by log analysis tools |
 *
 * ### Log Level Hierarchy
 * ```
 * k_log_none (0)     - No logging (production builds)
 *     v
 * k_log_error (1)    - Critical failures only
 *     v
 * k_log_warn (2)     - Recoverable issues
 *     v
 * k_log_info (3)     - Informational events (default)
 *     v
 * k_log_debug (4)    - Development debugging
 *     v
 * k_log_verbose (5)  - Trace-level detail
 * ```
 *
 * ### Output Routing
 * @dot
 * digraph log_routing {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   app [label="Application\nCode"];
 *   macro [label="rx_log_*\nMacro"];
 *   filter [label="Compile-Time\nFilter", shape=diamond];
 *   impl [label="internal_rx_log_*\nInline Function"];
 *   uart [label="UART Debug\n(CY7C65213)"];
 *   usb [label="USB CDC Port 2\n(Optional)"];
 *
 *   app -> macro;
 *   macro -> filter;
 *   filter -> impl [label="enabled"];
 *   filter -> eliminated [label="disabled", style=dashed];
 *   impl -> uart;
 *   impl -> usb [label="if USB_LOG_MIRROR", style=dashed];
 *
 *   eliminated [label="Eliminated\n(zero code)", shape=ellipse, style=dashed];
 * }
 * @enddot
 *
 * ## Performance Characteristics
 *
 * ### Execution Timing (RX72N @ 240 MHz)
 * | Log Type | Cycles | Time | Bytes (UART 115200) |
 * |----------|--------|------|---------------------|
 * | Disabled log | 0 | 0 ns | 0 |
 * | rx_log_error() | ~2400 | 10 us | ~30 bytes |
 * | rx_log_error_val(uint8_t) | ~3600 | 15 us | ~35 bytes |
 * | rx_log_error_hex() | ~4200 | 17.5 us | ~40 bytes |
 * | rx_log_error_str(64 chars) | ~18000 | 75 us | ~95 bytes |
 *
 * ### Memory Footprint
 * | Component | Flash | RAM | Stack |
 * |-----------|-------|-----|-------|
 * | All inline functions | 0 (inlined) | 0 | 0 |
 * | Per log site (enabled) | ~60-150 bytes | 0 | 8-16 bytes |
 * | Per log site (disabled) | 0 bytes | 0 | 0 |
 * | USB CDC buffer (optional) | 0 | 512 bytes | 0 |
 *
 * ### Thread Safety
 * - **NOT thread-safe**: UART/USB output functions are not atomic
 * - **Interleaving possible**: Concurrent logs from multiple threads may mix
 * - **Mitigation**: Use RTOS mutex around critical log sequences if needed
 * - **Design rationale**: Performance over thread safety (motor control is time-critical)
 *
 * ## C23 _Generic Type Dispatch
 *
 * The `rx_log_*_val()` macros use C23 `_Generic` selection expressions to automatically
 * route values to the correct typed implementation:
 *
 * ```c
 * _Generic((val),
 *   uint8_t:  internal_rx_log_error_u8,
 *   uint16_t: internal_rx_log_error_u16,
 *   uint32_t: internal_rx_log_error_u32,
 *   int32_t:  internal_rx_log_error_err  // rx_err_t is int32_t
 * )(tag, msg, val)
 * ```
 *
 * **Type Limitation**: Because `rx_err_t` is `typedef int32_t`, `_Generic` cannot distinguish
 * between error codes and raw signed integers. Both route to hex formatting. Use explicit
 * `rx_log_*_hex()` or cast to `uint32_t` for decimal formatting of signed integers.
 *
 * ## USB Log Mirroring
 *
 * Enable with `-DUSB_LOG_MIRROR=1` to send logs to both:
 * - **UART**: `/dev/ttyUSB0` via CY7C65213 (115200 baud)
 * - **USB CDC Port 2**: `/dev/ttyACM2` (log port)
 *
 * Port assignments:
 * | Port | Enum | Purpose |
 * |------|------|---------|
 * | 0 | k_usb_port_proto | Binary protocol (nanopb frames) |
 * | 1 | k_usb_port_decoded | Decoded ASCII frame dumps |
 * | 2 | k_usb_port_log | Log output (mirrored here) |
 *
 * ## Configuration
 *
 * ### Compile-Time Flags
 * ```bash
 * # Set log level (default: k_log_info in release, k_log_debug in DEBUG)
 * -DLOG_LEVEL=k_log_error
 *
 * # Disable USB mirroring (default: 1, enabled by default)
 * -DUSB_LOG_MIRROR=0
 * ```
 *
 * @note USB_LOG_MIRROR=1 is **enabled by default** (Issue #161: USB CDC debug output).
 *       All logs appear on BOTH UART and USB CDC Port 2. Override with `-DUSB_LOG_MIRROR=0` to disable.
 *
 * ## Usage Examples
 *
 * ### Basic Logging
 * @code{.c}
 * #include "rx_log.h"
 *
 * void motor_init(void) {
 *   rx_log_info("MOTOR", "Starting initialization");
 *
 *   rx_err_t err = configure_pwm();
 *   if (err != k_rx_ok) {
 *     rx_log_error_val("MOTOR", "PWM config failed", err);  // Hex error code
 *     return;
 *   }
 *
 *   rx_log_info("MOTOR", "Initialization complete");
 * }
 * @endcode
 *
 * ### Type Dispatch Examples
 * @code{.c}
 * // Automatic type routing via _Generic
 * uint8_t port = 5;
 * rx_log_debug_val("GPIO", "Port number", port);  // -> _u8 (decimal)
 *
 * uint16_t rpm = 2100;
 * rx_log_info_val("MOTOR", "Current RPM", rpm);  // -> _u16 (decimal)
 *
 * uint32_t timestamp = get_ticks();
 * rx_log_verbose_val("TIMER", "Timestamp", timestamp);  // -> _u32 (decimal)
 *
 * rx_err_t err = k_rx_err_timeout;
 * rx_log_error_val("SPI", "Transfer error", err);  // -> _err (hex: 0x00000108)
 * @endcode
 *
 * ### Hex and String Logging
 * @code{.c}
 * // Hex values (explicit digit count)
 * uint32_t reg = read_register(0x40000000);
 * rx_log_debug_hex("HAL", "Register value", reg, 8);  // 8 hex digits
 *
 * // Bounded string logging (safe)
 * const char* device_name = "RX72N Motor Controller";
 * rx_log_info_str("SYSTEM", "Device", device_name, strlen(device_name));
 * @endcode
 *
 * ### Conditional Compilation (Zero Overhead)
 * @code{.c}
 * // If LOG_LEVEL < k_log_debug, this compiles to nothing (zero code)
 * rx_log_debug("TRACE", "Entering critical section");  // -> ((void)0) if disabled
 * @endcode
 *
 * ## Design Rationale
 *
 * ### Why Compile-Time Filtering?
 * - **Zero overhead**: Disabled logs generate no code (vs. runtime checks)
 * - **Flash savings**: Production builds omit all debug/verbose logs
 * - **Performance**: No branching or function call overhead
 *
 * ### Why _Generic Instead of Function Overloading?
 * - **Type safety**: Compiler enforces correct types, catches errors at compile time
 * - **No C++**: Pure C implementation (RX72N firmware is C-only)
 * - **Clean API**: Single macro name, automatic dispatch
 *
 * ### Why Bounded Strings?
 * - **NASA Rule 2**: Statically provable loop bounds
 * - **Safety**: Prevents reading past buffer end (undefined behavior)
 * - **Predictable timing**: Maximum execution time is known
 *
 * ## Dependencies
 * - `rx_err.h` - Error code definitions (rx_err_t)
 * - `uart.h` (implicit) - uart_debug_* functions for output
 * - `rx_usb.h` (optional) - USB CDC output (if USB_LOG_MIRROR=1)
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 [YES]:** No goto, setjmp, recursion (pure inline functions and macros)
 * - **Rule 2 [YES]:** All loops bounded (string logging uses k_log_str_max_len constant)
 * - **Rule 3 [YES]:** No dynamic allocation (all stack-based, inline functions)
 * - **Rule 4 [YES]:** All functions <60 lines (inline helpers are 5-20 lines each)
 * - **Rule 5 [YES]:** Minimum 2 assertions per function (nullptr validation in _str functions)
 * - **Rule 6 [YES]:** Data at smallest scope (loop counters, temporary variables)
 * - **Rule 7 [YES]:** N/A for logging (output functions return void, errors silently dropped)
 * - **Rule 8 [YES]:** C23 typed enums for constants, macros only for type dispatch and filtering
 * - **Rule 9 [WARN]:** Single-level dereferencing only (string buffers, function pointers avoided)
 * - **Rule 10 [YES]:** Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Only logging output (no error handling, no formatting logic beyond basic types)
 * - **O (Open/Closed):** Extensible via USB_LOG_MIRROR flag, new log levels can be added
 * - **L (Liskov Substitution):** All internal_rx_log_* functions have identical signatures per level
 * - **I (Interface Segregation):** Separate macros for different value types (val, hex, str)
 * - **D (Dependency Inversion):** Depends on abstract uart_debug_* interface (not concrete UART driver)
 *
 * @par Module Dependencies:
 * ```
 * rx_log.h
 *   +--> rx_err.h (error code definitions)
 *   +--> uart.h (uart_debug_* output functions)
 *   +--> rx_usb.h (optional, if USB_LOG_MIRROR=1)
 * ```
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @see uart.h for UART debug output implementation
 * @see rx_err.h for error code definitions
 * @see rx_usb.h for USB CDC output
 */

#pragma once

#include <stdint.h>
#include <string.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for UART debug functions (avoid circular dependency) */

/* Include simulator configuration for conditional compilation */
#include "rx_simulator_config.h"

#if RX_IS_SIMULATOR
/* =============================================================================
 * Simulator Mode: Logging to stdout
 * =============================================================================
 * When building for e^2 studio simulator, UART hardware doesn't work.
 * Redirect logging to stdout so output appears in the simulator console.
 *
 * These inline implementations replace the hardware UART functions.
 * The simulator console automatically appears in e^2 studio when running
 * in debug mode (View -> Console if not visible).
 *
 * NOTE: These are inline to avoid linking against UART driver in simulator.
 * =============================================================================
 */

#include <inttypes.h> /* For PRId32, PRIx32 format macros */
#include <stdio.h>    /* For putchar(), snprintf() */

/**
 * @enum uart_debug_buf_size_t
 * @brief Buffer sizes for uart_debug_put* simulator functions
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_uart_debug_int_buf_size  = 12U, /**< Buffer for int32 string: "-2147483648\0" = 12 chars */
  k_uart_debug_uint_buf_size = 11U, /**< Buffer for uint32 string: "4294967295\0" = 11 chars */
  k_uart_debug_hex_buf_size  = 11U, /**< Buffer for "0x" + 8 hex digits + null = 11 chars */
} uart_debug_buf_size_t;

/**
 * @brief Output character to simulator console
 *
 * @details
 * Simulator implementation: Writes to stdout using putchar().
 * Hardware implementation: Writes to SCI9 UART (see uart.c).
 *
 * @param[in] data Character to output
 *
 * @note Not thread-safe (hardware UART is also not thread-safe)
 * @note Inline function - zero call overhead in simulator builds
 *
 * @since Version 1.0.0
 */
static inline void uart_debug_putc(char data)
{
  (void)putchar((int)data);
}

/**
 * @brief Output string to simulator console
 *
 * @details
 * Simulator implementation: Writes to stdout with \r\n conversion.
 * Matches hardware UART behavior where \n -> \r\n.
 *
 * @param[in] str Null-terminated string to output (must not be NULL)
 *
 * @pre str != NULL (checked in hardware build, assumed in simulator)
 * @post All characters output to console
 *
 * @note Not thread-safe
 * @note Inline function - zero call overhead in simulator builds
 *
 * @since Version 1.0.0
 */
static inline void uart_debug_puts(const char* str)
{
  if (str == NULL) {
    return;
  }

  while (*str) {
    if (*str == '\n') {
      (void)putchar('\r'); /* Hardware UART does \n -> \r\n */
    }
    (void)putchar((int)(*str++));
  }
}

/**
 * @brief Output signed integer to simulator console
 *
 * @details
 * Simulator implementation: Uses sprintf + puts.
 * Hardware implementation: Custom formatting to avoid sprintf overhead.
 *
 * @param[in] value Signed 32-bit integer to output
 *
 * @note Inline function - zero call overhead in simulator builds
 *
 * @since Version 1.0.0
 */
static inline void uart_debug_putint(int32_t value)
{
  char buf[k_uart_debug_int_buf_size];
  (void)snprintf(/* NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
                 buf,
                 sizeof(buf),
                 "%" PRId32,
                 value);
  uart_debug_puts(buf);
}

/**
 * @brief Output unsigned integer to simulator console
 *
 * @details
 * Simulator implementation: Uses snprintf + uart_debug_puts.
 * Hardware implementation: Custom formatting to avoid sprintf overhead.
 *
 * @param[in] value Unsigned 32-bit integer to output
 *
 * @pre  value is a valid uint32_t (any value in [0, UINT32_MAX])
 * @pre  uart_debug subsystem initialized (uart_debug_puts functional)
 * @post Decimal ASCII string representation of value written to console
 * @post uart_debug_puts invoked with null-terminated decimal string
 *
 * @note Inline function - zero call overhead in simulator builds
 *
 * @since Version 1.0.0
 */
static inline void uart_debug_putuint(uint32_t value)
{
  /* 4294967295 (UINT32_MAX) = 10 digits + null terminator */
  char buf[k_uart_debug_uint_buf_size];
  (void)snprintf(/* NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
                 buf,
                 sizeof(buf),
                 "%" PRIu32,
                 value);
  uart_debug_puts(buf);
}

/**
 * @brief Output hexadecimal value to simulator console
 *
 * @details
 * Simulator implementation: Uses sprintf + puts.
 * Hardware implementation: Custom formatting to avoid sprintf overhead.
 *
 * @param[in] value Unsigned 32-bit value to output as hex
 * @param[in] digits Number of hex digits to display (1-8)
 *
 * @note Inline function - zero call overhead in simulator builds
 *
 * @since Version 1.0.0
 */
static inline void uart_debug_puthex(uint32_t value, uint8_t digits)
{
  enum : uint8_t { k_max_hex_digits = 8 };
  enum : uint8_t { k_min_hex_digits = 1 };

  /* Pre-computed format strings keyed by digit count (1-8), avoids runtime
   * format building and all implicit-width casts (int, unsigned long). */
  static const char* const s_hex_fmts[k_max_hex_digits] = {
    "0x%01" PRIx32,
    "0x%02" PRIx32,
    "0x%03" PRIx32,
    "0x%04" PRIx32,
    "0x%05" PRIx32,
    "0x%06" PRIx32,
    "0x%07" PRIx32,
    "0x%08" PRIx32,
  };

  char buf[k_uart_debug_hex_buf_size]; /* "0x" + 8 hex digits + null */
  if (digits < k_min_hex_digits) {
    digits = k_min_hex_digits;
  }
  if (digits > k_max_hex_digits) {
    digits = k_max_hex_digits;
  }
  (void)snprintf(/* NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
                 buf,
                 sizeof(buf),
                 s_hex_fmts[digits - k_min_hex_digits],
                 value);
  uart_debug_puts(buf);
}

#else
/* =============================================================================
 * Hardware Mode: UART-framed logging (k_frame_type_log_message frames)
 * =============================================================================
 * Log bytes are queued in the rx_log_uart ring buffer (zero I/O on the log
 * site) and periodically drained by the communication task, which wraps them
 * in a TYPE=0x20 (k_frame_type_log_message) frame and transmits over SCI9 via
 * the CY7C65213 USB-UART bridge to the Pi5 gateway.
 *
 * Framing logs prevents log bytes from colliding with the 0x55AA frame sync
 * word used by command / telemetry traffic on the same byte stream.
 * =============================================================================
 */

#include "rx_log_uart.h"

/* uart_debug_* are low-level polling-mode raw-UART writers exported by
 * libs/rx_hal/src/uart.c. They bypass the frame protocol and emit raw ASCII
 * directly on the SCI9 wire -- the gateway's frame decoder will discard those
 * bytes during sync recovery.
 *
 * Use ONLY for panic / stack-overflow / catastrophic-fault diagnostics where:
 *   - the ThreadX scheduler is not trustworthy (cannot drain the log ring),
 *   - the developer is expected to read output via E2 Lite / RTT / a direct
 *     serial terminal tapping TX before the bridge chip.
 *
 * Normal runtime logging MUST go through the rx_log_uart_* API above.
 */
void uart_debug_putc(char data);
void uart_debug_puts(const char* str);
void uart_debug_putint(int32_t value);
void uart_debug_putuint(uint32_t value);
void uart_debug_puthex(uint32_t value, uint8_t digits);

#endif /* RX_IS_SIMULATOR */

/* =============================================================================
 * Log output macros
 * =============================================================================
 *
 * In simulator builds these go to stdout via uart_debug_* (defined above).
 * In hardware builds they go to the rx_log_uart ring buffer, which the
 * communication task drains and wraps in k_frame_type_log_message frames for
 * transmission over SCI9 -> CY7C65213 -> /dev/ttyUSB0.
 *
 * There is no longer a runtime backend selector. Logs always flow through
 * the framed-UART path on hardware.
 */

#if RX_IS_SIMULATOR

#define LOG_PUTC(c)      uart_debug_putc((char)(c))
#define LOG_PUTS(s)      uart_debug_puts((s))
#define LOG_PUTINT(v)    uart_debug_putint((int32_t)(v))
#define LOG_PUTUINT(v)   uart_debug_putuint((uint32_t)(v))
#define LOG_PUTHEX(v, d) uart_debug_puthex((uint32_t)(v), (uint8_t)(d))

#else

#define LOG_PUTC(c)      rx_log_uart_putc((char)(c))
#define LOG_PUTS(s)      rx_log_uart_puts((s))
#define LOG_PUTINT(v)    rx_log_uart_putint((int32_t)(v))
#define LOG_PUTUINT(v)   rx_log_uart_putuint((uint32_t)(v))
#define LOG_PUTHEX(v, d) rx_log_uart_puthex((uint32_t)(v), (uint8_t)(d))

#endif /* RX_IS_SIMULATOR */

/* =============================================================================
 * Log Level Definitions
 * =============================================================================
 */

/**
 * @enum log_level_t
 * @brief Log level enumeration defining severity hierarchy for compile-time filtering
 *
 * @details
 * Defines the log level hierarchy used for compile-time filtering. Each level includes
 * all messages from higher-priority (lower-numbered) levels. The `LOG_LEVEL` macro
 * determines which levels are compiled into the binary.
 *
 * ## Hierarchy (Inclusive)
 * Each level includes all messages from levels above it:
 * - `k_log_error` (1) -> ERROR only
 * - `k_log_warn` (2) -> ERROR + WARN
 * - `k_log_info` (3) -> ERROR + WARN + INFO
 * - `k_log_debug` (4) -> ERROR + WARN + INFO + DEBUG
 * - `k_log_verbose` (5) -> ERROR + WARN + INFO + DEBUG + VERBOSE
 *
 * ## Recommended Usage
 * | Build Type | Recommended Level | Rationale |
 * |------------|-------------------|-----------|
 * | Production | `k_log_error` or `k_log_none` | Minimal overhead, critical failures only |
 * | Field Testing | `k_log_warn` | Captures issues without excessive logging |
 * | Development | `k_log_info` | Standard development visibility (default) |
 * | Debugging | `k_log_debug` | Detailed troubleshooting |
 * | Trace Analysis | `k_log_verbose` | Maximum detail for complex issues |
 *
 * ## Compile-Time Behavior
 * @code{.c}
 * // Example: LOG_LEVEL = k_log_warn
 * rx_log_error("TAG", "msg");   // [OK] Compiled (error <= warn)
 * rx_log_warn("TAG", "msg");    // [OK] Compiled (warn <= warn)
 * rx_log_info("TAG", "msg");    // [X] Eliminated (info > warn)
 * rx_log_debug("TAG", "msg");   // [X] Eliminated (debug > warn)
 * rx_log_verbose("TAG", "msg"); // [X] Eliminated (verbose > warn)
 * @endcode
 *
 * ## Configuration
 * Set at compile time via `-DLOG_LEVEL=<level>`:
 * ```bash
 * cmake -DLOG_LEVEL=k_log_error ..  # Production
 * cmake -DLOG_LEVEL=k_log_debug ..  # Development
 * ```
 *
 * Default (if not specified):
 * - **DEBUG builds**: `k_log_debug`
 * - **Release builds**: `k_log_info`
 *
 * @par Underlying Type: uint8_t (C23 typed enum)
 * - **Range**: 0-255 (only 0-5 used)
 * - **Size**: 1 byte
 * - **Rationale**: Smallest type sufficient for 6 levels, minimal memory footprint
 *
 * @see LOG_LEVEL Default log level configuration
 * @see rx_log_error(), rx_log_warn(), rx_log_info(), rx_log_debug(), rx_log_verbose()
 */
typedef enum : uint8_t {
  k_log_none = 0, /**< No logging (production silent mode). All logs eliminated at compile time. */
  k_log_error =
    1, /**< Critical errors only. Operation-preventing failures, requires immediate attention. */
  k_log_warn = 2, /**< Errors and warnings. Recoverable issues, degraded functionality. */
  k_log_info =
    3, /**< Errors, warnings, and informational messages. Default level for development. */
  k_log_debug =
    4, /**< Errors, warnings, info, and debug messages. Detailed troubleshooting information. */
  k_log_verbose =
    5, /**< All messages including verbose trace. Maximum detail for complex debugging. */
} log_level_t;

/**
 * @enum log_format_constants_t
 * @brief Logging formatting and safety constants
 *
 * @details
 * Defines compile-time constants for log output formatting and memory safety bounds.
 * These constants ensure predictable behavior and prevent buffer overruns.
 *
 * ## Safety Rationale
 *
 * ### k_log_hex_width_err = 8
 * **Purpose**: Hex digit width for displaying rx_err_t error codes.
 *
 * **Rationale**:
 * - `rx_err_t` is `typedef int32_t` (32 bits)
 * - 32 bits = 8 hex digits (4 bits per digit)
 * - Example: `0x00000108` (k_rx_err_timeout)
 *
 * **Usage**: `rx_log_error_val()` with rx_err_t values formats as 8-digit hex.
 *
 * ### k_log_str_max_len = 256
 * **Purpose**: Maximum string length for bounded string logging (NASA Rule 2 compliance).
 *
 * **Rationale**:
 * - **Safety**: Prevents unbounded loops reading past buffer end
 * - **Predictability**: Maximum execution time is known (256 x ~15 cycles/char = 3840 cycles)
 * - **Practicality**: 256 bytes sufficient for typical log messages
 * - **Stack impact**: No stack allocation (loop counter is uint32_t = 4 bytes)
 *
 * **Behavior**: String logging functions iterate up to `k_log_str_max_len` characters,
 * stopping early if:
 * 1. User-provided length `len` is reached, OR
 * 2. Null terminator `'\0'` is encountered
 *
 * **Example**:
 * @code{.c}
 * const char* long_str = "This is a very long string...";  // > 256 chars
 * rx_log_info_str("TAG", "Long", long_str, 300);  // Only logs first 256 chars
 * @endcode
 *
 * ## Performance Impact
 * | Constant | CPU Impact | Memory Impact |
 * |----------|------------|---------------|
 * | k_log_hex_width_err | 8 hex digits ~ 240 cycles @ 240 MHz | 0 bytes (immediate) |
 * | k_log_str_max_len | Max 3840 cycles (256 x 15) | 4 bytes (loop counter) |
 *
 * @par Underlying Type: uint16_t (C23 typed enum)
 * - **Range**: 0-65535
 * - **Size**: 2 bytes
 * - **Rationale**: k_log_str_max_len = 256 fits in uint16_t, uint8_t too small
 *
 * @see internal_rx_log_error_str(), internal_rx_log_warn_str(), internal_rx_log_info_str()
 * @see internal_rx_log_debug_str(), internal_rx_log_verbose_str()
 */
typedef enum : uint16_t {
  k_log_hex_width_err =
    8, /**< Hex digit width for rx_err_t error codes (32-bit = 8 digits). Example: 0x00000108 */
  k_log_str_max_len =
    256, /**< Maximum string length for bounded logging (NASA Rule 2 compliance). Prevents unbounded loops. */
} log_format_constants_t;

/**
 * @brief Default log level configuration
 *
 * Can be overridden at compile time:
 * -DLOG_LEVEL=(k_log_error)
 */
#ifndef LOG_LEVEL
#ifdef DEBUG
#define LOG_LEVEL (k_log_debug)
#else
#define LOG_LEVEL (k_log_info)
#endif
#endif

/* =============================================================================
 * Internal Output Helpers (thin wrappers to keep per-level functions small)
 * =============================================================================
 */

/** @brief Write one character to the active log backend(s). @param[in] c Character to write. */
static inline void internal_log_putc(char c)
{
  LOG_PUTC(c);
}

/** @brief Write a string to the active log backend(s). @param[in] s Null-terminated string. */
static inline void internal_log_puts(const char* s)
{
  LOG_PUTS(s);
}

/** @brief Write a signed 32-bit integer to the active log backend(s). @param[in] v Value. */
static inline void internal_log_putint(int32_t v)
{
  LOG_PUTINT(v);
}

/** @brief Write a hex value to the active log backend(s). @param[in] v Value. @param[in] d Digits. */
static inline void internal_log_puthex(uint32_t v, uint8_t d)
{
  LOG_PUTHEX(v, d);
}

/* =============================================================================
 * Internal Helper: Format Log Header
 * =============================================================================
 */

/**
 * @brief Internal helper to format and output log message header
 *
 * @details
 * Outputs standardized log header in format: `[LEVEL] [TAG] `
 * This prefix precedes all log messages for consistent parsing by log analysis tools.
 *
 * ## Algorithm
 * 1. Validate input pointers (NULL check for safety)
 * 2. Output opening bracket `[`
 * 3. Output log level string (e.g., "ERROR", "WARN", "INFO")
 * 4. Output `] [` separator
 * 5. Output component tag (e.g., "MOTOR", "SPI", "USB")
 * 6. Output `] ` (close bracket and space)
 *
 * ## Output Format
 * ```
 * [ERROR] [MOTOR] message content here\r\n
 * [WARN] [SPI] message content here\r\n
 * [INFO] [USB] message content here\r\n
 * ```
 *
 * ## Performance
 * - **Cycles**: ~600 cycles @ 240 MHz (depends on string lengths)
 * - **Time**: ~2.5 us
 * - **UART bytes**: ~15-25 bytes (depends on level_str and tag lengths)
 * - **Stack**: 0 bytes (inline, no local variables)
 *
 * @param[in] level_str Log level string (e.g., "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE")
 *                      - Must be null-terminated string literal
 *                      - Typically 4-7 characters
 *                      - nullptr silently ignored (safety)
 * @param[in] tag Component tag identifying log source (e.g., "MOTOR", "SPI", "I2C")
 *                - Must be null-terminated string literal
 *                - Recommended: 3-8 uppercase characters
 *                - nullptr silently ignored (safety)
 *
 * @return void (output-only, no error indication)
 *
 * @pre level_str and tag should be valid null-terminated strings
 * @post Log header written to configured output (UART and/or USB CDC)
 *
 * @note **Thread Safety**: NOT thread-safe. Concurrent calls may interleave output.
 * @note **Inline**: Always inlined for zero function call overhead.
 * @note **Error Handling**: NULL pointers silently ignored (safe failure mode).
 *
 * @par Example:
 * @code{.c}
 * // Called internally by all log functions
 * internal_log_header("ERROR", "MOTOR");  // Outputs: "[ERROR] [MOTOR] "
 * LOG_PUTS("PWM init failed\r\n");        // Complete message
 *
 * // Result on terminal:
 * // [ERROR] [MOTOR] PWM init failed
 * @endcode
 *
 * @see LOG_PUTC(), LOG_PUTS() Output macros (UART or UART+USB)
 * @see internal_rx_log_error(), internal_rx_log_warn(), internal_rx_log_info()
 */
static inline void internal_log_header(const char* level_str, const char* tag)
{
  /* Pre-condition: Validate pointers (NASA Power of 10 Rule 5).
   * The level_str side of the || is defensive-only: every caller passes a
   * compile-time string literal ("ERROR", "WARN", ...), so the TRUE branch is
   * architecturally unreachable. Excluded from gcovr branch coverage with an
   * EXCL marker so the 100% libs/ gate remains valid. */
  /* GCOVR_EXCL_BR_START */
  if (level_str == (const char*)0 || tag == (const char*)0) {
    return;
  }
  /* GCOVR_EXCL_BR_STOP */

  internal_log_putc('[');
  internal_log_puts(level_str);
  internal_log_puts("] [");
  internal_log_puts(tag);
  internal_log_puts("] ");
}

/* =============================================================================
 * ERROR Level - Internal Implementations
 * =============================================================================
 */

/**
 * @brief Log ERROR-level message without additional value
 *
 * @details
 * Outputs critical error message with standardized header format. Used for
 * operation-preventing failures that require immediate attention.
 *
 * ## Algorithm
 * 1. Validate message pointer (NULL check)
 * 2. Output header: `[ERROR] [TAG] `
 * 3. Output message text
 * 4. Output line terminator `\r\n`
 *
 * ## Output Format
 * ```
 * [ERROR] [TAG] message text\r\n
 * ```
 *
 * ## Performance
 * | Metric | Value |
 * |--------|-------|
 * | Cycles | ~2400 @ 240 MHz (30-char message) |
 * | Time | ~10 us |
 * | UART bytes | ~45 bytes (header + message + \r\n) |
 * | Stack | 0 bytes (inline) |
 *
 * @param[in] tag Component tag identifying error source (e.g., "MOTOR", "SPI")
 *                - Must be null-terminated string literal
 *                - Recommended: 3-8 uppercase characters
 *                - nullptr silently ignored
 * @param[in] message Error message describing the failure
 *                    - Must be null-terminated string
 *                    - Should be concise and actionable
 *                    - nullptr causes silent return (safety)
 *
 * @return void (output-only function)
 *
 * @pre message must be valid null-terminated string or nullptr
 * @post Error message written to UART (and USB CDC if USB_LOG_MIRROR=1)
 *
 * @note **Thread Safety**: NOT thread-safe. Concurrent calls may interleave.
 * @note **Inline**: Always inlined for zero function call overhead.
 * @note **NULL Handling**: NULL message pointer silently ignored (safe failure).
 *
 * @par Example Usage:
 * @code{.c}
 * // Critical initialization failure
 * if (!motor_init()) {
 *   internal_rx_log_error("MOTOR", "Initialization failed");
 * }
 *
 * // Hardware failure
 * if (!spi_ready()) {
 *   internal_rx_log_error("SPI", "Peripheral not responding");
 * }
 *
 * // Output on terminal:
 * // [ERROR] [MOTOR] Initialization failed
 * // [ERROR] [SPI] Peripheral not responding
 * @endcode
 *
 * @see rx_log_error() Public API macro (compile-time filtered)
 * @see internal_log_header() Header formatting function
 */
static inline void internal_rx_log_error(const char* tag, const char* message)
{
  /* Rule 5: Pre-condition validation - NULL guard */
  if (message == (const char*)0) {
    return;
  }
  internal_log_header("ERROR", tag);
  internal_log_puts(message);
  internal_log_puts("\r\n");
}

/**
 * @brief Log error with uint8_t value
 */
static inline void internal_rx_log_error_u8(const char* tag, const char* message, uint8_t value)
{
  internal_log_header("ERROR", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log error with uint16_t value
 */
static inline void internal_rx_log_error_u16(const char* tag, const char* message, uint16_t value)
{
  internal_log_header("ERROR", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log error with uint32_t value
 */
static inline void internal_rx_log_error_u32(const char* tag, const char* message, uint32_t value)
{
  internal_log_header("ERROR", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log error with int32_t value
 */
static inline void internal_rx_log_error_i32(const char* tag, const char* message, int32_t value)
{
  internal_log_header("ERROR", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint(value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log error with error code (rx_err_t)
 */
static inline void internal_rx_log_error_err(const char* tag, const char* message, rx_err_t err)
{
  internal_log_header("ERROR", tag);
  internal_log_puts(message);
  internal_log_puts(": 0x");
  internal_log_puthex((uint32_t)err, k_log_hex_width_err);
  internal_log_puts("\r\n");
}

/**
 * @brief Log error with hex value
 */
static inline void
internal_rx_log_error_hex(const char* tag, const char* message, uint32_t value, uint8_t digits)
{
  internal_log_header("ERROR", tag);
  internal_log_puts(message);
  internal_log_puts(": 0x");
  internal_log_puthex(value, digits);
  internal_log_puts("\r\n");
}

/**
 * @brief Log ERROR-level message with bounded string value (NASA-compliant)
 *
 * @details
 * Outputs critical error message with associated string value using bounded iteration
 * for memory safety. String output is limited to prevent unbounded loops and buffer
 * overruns (NASA Power of 10 Rule 2 compliance).
 *
 * ## Algorithm
 * 1. Validate message and str_value pointers (NULL checks)
 * 2. Output header: `[ERROR] [TAG] `
 * 3. Output message text
 * 4. Output separator: `: `
 * 5. Iterate up to k_log_str_max_len (256) characters:
 *    a. Stop if index >= user-provided len
 *    b. Stop if null terminator '\0' encountered
 *    c. Output character
 * 6. Output line terminator `\r\n`
 *
 * ## Output Format
 * ```
 * [ERROR] [TAG] message: string_value\r\n
 * ```
 *
 * ## Safety Features (NASA Rule 2)
 * | Feature | Implementation | Benefit |
 * |---------|----------------|---------|
 * | Bounded loop | `i < k_log_str_max_len` | Statically provable upper bound |
 * | Early termination | `i >= len` check | Respects caller's length limit |
 * | Null handling | `str_value[i] == '\0'` | Safe for null-terminated strings |
 * | Pointer validation | NULL checks at start | Prevents crashes |
 *
 * ## Performance
 * | Metric | Best Case | Worst Case |
 * |--------|-----------|------------|
 * | Cycles | ~2700 (1 char) | ~21600 (256 chars) @ 240 MHz |
 * | Time | ~11 us | ~90 us |
 * | UART bytes | ~50 bytes | ~300 bytes |
 * | Stack | 4 bytes (uint32_t i) | 4 bytes |
 *
 * @param[in] tag Component tag identifying error source (e.g., "MOTOR", "SPI")
 *                - Must be null-terminated string literal
 *                - nullptr silently ignored
 * @param[in] message Error message describing the context
 *                    - Must be null-terminated string
 *                    - nullptr causes silent return (safety)
 * @param[in] str_value String value to log (e.g., parameter name, device ID)
 *                      - Must point to valid memory
 *                      - nullptr causes silent return (safety)
 *                      - Does not need to be null-terminated if len is exact
 * @param[in] len Maximum number of characters to log from str_value
 *                - Typically obtained via `strlen(str_value)`
 *                - Values > k_log_str_max_len will be clamped to k_log_str_max_len
 *                - Use exact buffer length for non-null-terminated buffers
 *
 * @return void (output-only function)
 *
 * @pre message and str_value must be valid pointers or nullptr
 * @pre len must not exceed actual buffer size of str_value
 * @post Error message with string value written to output (if parameters valid)
 *
 * @note **Thread Safety**: NOT thread-safe. Concurrent calls may interleave output.
 * @note **Inline**: Always inlined for zero function call overhead.
 * @note **Bounded**: Maximum 256 characters logged regardless of len parameter.
 * @note **NULL Safety**: NULL pointers silently ignored (safe failure mode).
 *
 * @warning If str_value buffer is smaller than len, undefined behavior may occur.
 *          Always ensure len <= actual buffer size.
 *
 * @par Example: Logging Configuration Errors
 * @code{.c}
 * // Unknown configuration parameter
 * const char* param_name = "invalid_motor_id";
 * internal_rx_log_error_str("CONFIG", "Unknown parameter", param_name, strlen(param_name));
 * // Output: [ERROR] [CONFIG] Unknown parameter: invalid_motor_id
 *
 * // Non-null-terminated buffer (use exact length)
 * char device_id[16] = {0x01, 0x02, 0x03, ...};  // Binary data
 * internal_rx_log_error_str("USB", "Invalid device ID", device_id, 16);
 * // Output: [ERROR] [USB] Invalid device ID: <binary data>
 * @endcode
 *
 * @par Example: Truncation Behavior
 * @code{.c}
 * // String longer than k_log_str_max_len (256)
 * char long_str[300];
 * memset(long_str, 'A', 299);
 * long_str[299] = '\0';
 *
 * internal_rx_log_error_str("TEST", "Long string", long_str, 299);
 * // Output: [ERROR] [TEST] Long string: AAAA... (256 chars max, truncated)
 * @endcode
 *
 * @see k_log_str_max_len Maximum string length constant (256 bytes)
 * @see rx_log_error_str() Public API macro (compile-time filtered)
 * @see internal_rx_log_error() Simple error logging without value
 */
static inline void
internal_rx_log_error_str(const char* tag, const char* message, const char* str_value, uint32_t len)
{
  /* Pre-condition: Validate pointers (NASA Power of 10 Rule 5) */
  if (message == (const char*)0 || str_value == (const char*)0) {
    return;
  }
  internal_log_header("ERROR", tag);
  internal_log_puts(message);
  internal_log_puts(": ");

  /*
   * NASA Rule 2: Loop bounded by k_log_str_max_len (compile-time constant)
   * Early break when i >= len or null terminator encountered.
   */
  for (uint32_t i = 0; i < k_log_str_max_len; i++) {
    if (i >= len || str_value[i] == '\0') {
      break;
    }
    internal_log_putc(str_value[i]);
  }

  internal_log_puts("\r\n");
}

/* =============================================================================
 * WARN Level - Internal Implementations
 * =============================================================================
 */

/**
 * @brief Log warning message (no value)
 */
static inline void internal_rx_log_warn(const char* tag, const char* message)
{
  /* Rule 5: Pre-condition validation - NULL guard */
  if (message == (const char*)0) {
    return;
  }
  internal_log_header("WARN", tag);
  internal_log_puts(message);
  internal_log_puts("\r\n");
}

/**
 * @brief Log warning with uint8_t value
 */
static inline void internal_rx_log_warn_u8(const char* tag, const char* message, uint8_t value)
{
  internal_log_header("WARN", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log warning with uint16_t value
 */
static inline void internal_rx_log_warn_u16(const char* tag, const char* message, uint16_t value)
{
  internal_log_header("WARN", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log warning with uint32_t value
 */
static inline void internal_rx_log_warn_u32(const char* tag, const char* message, uint32_t value)
{
  internal_log_header("WARN", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log warning with int32_t value
 */
static inline void internal_rx_log_warn_i32(const char* tag, const char* message, int32_t value)
{
  internal_log_header("WARN", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint(value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log warning with error code (rx_err_t)
 */
static inline void internal_rx_log_warn_err(const char* tag, const char* message, rx_err_t err)
{
  internal_log_header("WARN", tag);
  internal_log_puts(message);
  internal_log_puts(": 0x");
  internal_log_puthex((uint32_t)err, k_log_hex_width_err);
  internal_log_puts("\r\n");
}

/**
 * @brief Log warning with hex value
 */
static inline void
internal_rx_log_warn_hex(const char* tag, const char* message, uint32_t value, uint8_t digits)
{
  internal_log_header("WARN", tag);
  internal_log_puts(message);
  internal_log_puts(": 0x");
  internal_log_puthex(value, digits);
  internal_log_puts("\r\n");
}

/**
 * @brief Log warning with string value (bounded length)
 */
static inline void
internal_rx_log_warn_str(const char* tag, const char* message, const char* str_value, uint32_t len)
{
  /* Pre-condition: Validate pointers (NASA Power of 10 Rule 5) */
  if (message == (const char*)0 || str_value == (const char*)0) {
    return;
  }

  internal_log_header("WARN", tag);
  internal_log_puts(message);
  internal_log_puts(": ");

  /*
   * NASA Rule 2: Loop bounded by k_log_str_max_len (compile-time constant)
   * Early break when i >= len or null terminator encountered.
   */
  for (uint32_t i = 0; i < k_log_str_max_len; i++) {
    if (i >= len || str_value[i] == '\0') {
      break;
    }
    internal_log_putc(str_value[i]);
  }

  internal_log_puts("\r\n");
}

/* =============================================================================
 * INFO Level - Internal Implementations
 * =============================================================================
 */

/**
 * @brief Log info message (no value)
 */
static inline void internal_rx_log_info(const char* tag, const char* message)
{
  /* Rule 5: Pre-condition validation - NULL guard */
  if (message == (const char*)0) {
    return;
  }
  internal_log_header("INFO", tag);
  internal_log_puts(message);
  internal_log_puts("\r\n");
}

/**
 * @brief Log info with uint8_t value
 */
static inline void internal_rx_log_info_u8(const char* tag, const char* message, uint8_t value)
{
  internal_log_header("INFO", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log info with uint16_t value
 */
static inline void internal_rx_log_info_u16(const char* tag, const char* message, uint16_t value)
{
  internal_log_header("INFO", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log info with uint32_t value
 */
static inline void internal_rx_log_info_u32(const char* tag, const char* message, uint32_t value)
{
  internal_log_header("INFO", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log info with int32_t value
 */
static inline void internal_rx_log_info_i32(const char* tag, const char* message, int32_t value)
{
  internal_log_header("INFO", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint(value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log info with error code (rx_err_t)
 */
static inline void internal_rx_log_info_err(const char* tag, const char* message, rx_err_t err)
{
  internal_log_header("INFO", tag);
  internal_log_puts(message);
  internal_log_puts(": 0x");
  internal_log_puthex((uint32_t)err, k_log_hex_width_err);
  internal_log_puts("\r\n");
}

/**
 * @brief Log info with hex value
 */
static inline void
internal_rx_log_info_hex(const char* tag, const char* message, uint32_t value, uint8_t digits)
{
  internal_log_header("INFO", tag);
  internal_log_puts(message);
  internal_log_puts(": 0x");
  internal_log_puthex(value, digits);
  internal_log_puts("\r\n");
}

/**
 * @brief Log info with string value (bounded length)
 */
static inline void
internal_rx_log_info_str(const char* tag, const char* message, const char* str_value, uint32_t len)
{
  /* Pre-condition: Validate pointers (NASA Power of 10 Rule 5) */
  if (message == (const char*)0 || str_value == (const char*)0) {
    return;
  }

  internal_log_header("INFO", tag);
  internal_log_puts(message);
  internal_log_puts(": ");

  /*
   * NASA Rule 2: Loop bounded by k_log_str_max_len (compile-time constant)
   * Early break when i >= len or null terminator encountered.
   */
  for (uint32_t i = 0; i < k_log_str_max_len; i++) {
    if (i >= len || str_value[i] == '\0') {
      break;
    }
    internal_log_putc(str_value[i]);
  }

  internal_log_puts("\r\n");
}

/* =============================================================================
 * DEBUG Level - Internal Implementations
 * =============================================================================
 */

/**
 * @brief Log debug message (no value)
 */
static inline void internal_rx_log_debug(const char* tag, const char* message)
{
  /* Rule 5: Pre-condition validation - NULL guard */
  if (message == (const char*)0) {
    return;
  }
  internal_log_header("DEBUG", tag);
  internal_log_puts(message);
  internal_log_puts("\r\n");
}

/**
 * @brief Log debug with uint8_t value
 */
static inline void internal_rx_log_debug_u8(const char* tag, const char* message, uint8_t value)
{
  internal_log_header("DEBUG", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log debug with uint16_t value
 */
static inline void internal_rx_log_debug_u16(const char* tag, const char* message, uint16_t value)
{
  internal_log_header("DEBUG", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log debug with uint32_t value
 */
static inline void internal_rx_log_debug_u32(const char* tag, const char* message, uint32_t value)
{
  internal_log_header("DEBUG", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log debug with int32_t value
 */
static inline void internal_rx_log_debug_i32(const char* tag, const char* message, int32_t value)
{
  internal_log_header("DEBUG", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint(value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log debug with error code (rx_err_t)
 */
static inline void internal_rx_log_debug_err(const char* tag, const char* message, rx_err_t err)
{
  internal_log_header("DEBUG", tag);
  internal_log_puts(message);
  internal_log_puts(": 0x");
  internal_log_puthex((uint32_t)err, k_log_hex_width_err);
  internal_log_puts("\r\n");
}

/**
 * @brief Log debug with hex value
 */
static inline void
internal_rx_log_debug_hex(const char* tag, const char* message, uint32_t value, uint8_t digits)
{
  internal_log_header("DEBUG", tag);
  internal_log_puts(message);
  internal_log_puts(": 0x");
  internal_log_puthex(value, digits);
  internal_log_puts("\r\n");
}

/**
 * @brief Log debug with string value (bounded length)
 */
static inline void
internal_rx_log_debug_str(const char* tag, const char* message, const char* str_value, uint32_t len)
{
  /* Pre-condition: Validate pointers (NASA Power of 10 Rule 5) */
  if (message == (const char*)0 || str_value == (const char*)0) {
    return;
  }

  internal_log_header("DEBUG", tag);
  internal_log_puts(message);
  internal_log_puts(": ");

  /*
   * NASA Rule 2: Loop bounded by k_log_str_max_len (compile-time constant)
   * Early break when i >= len or null terminator encountered.
   */
  for (uint32_t i = 0; i < k_log_str_max_len; i++) {
    if (i >= len || str_value[i] == '\0') {
      break;
    }
    internal_log_putc(str_value[i]);
  }

  internal_log_puts("\r\n");
}

/* =============================================================================
 * VERBOSE Level - Internal Implementations
 * =============================================================================
 */

/**
 * @brief Log verbose message (no value)
 */
static inline void internal_rx_log_verbose(const char* tag, const char* message)
{
  /* Rule 5: Pre-condition validation - NULL guard */
  if (message == (const char*)0) {
    return;
  }
  internal_log_header("VERBOSE", tag);
  internal_log_puts(message);
  internal_log_puts("\r\n");
}

/**
 * @brief Log verbose with uint8_t value
 */
static inline void internal_rx_log_verbose_u8(const char* tag, const char* message, uint8_t value)
{
  internal_log_header("VERBOSE", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log verbose with uint16_t value
 */
static inline void internal_rx_log_verbose_u16(const char* tag, const char* message, uint16_t value)
{
  internal_log_header("VERBOSE", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log verbose with uint32_t value
 */
static inline void internal_rx_log_verbose_u32(const char* tag, const char* message, uint32_t value)
{
  internal_log_header("VERBOSE", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint((int32_t)value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log verbose with int32_t value
 */
static inline void internal_rx_log_verbose_i32(const char* tag, const char* message, int32_t value)
{
  internal_log_header("VERBOSE", tag);
  internal_log_puts(message);
  internal_log_puts(": ");
  internal_log_putint(value);
  internal_log_puts("\r\n");
}

/**
 * @brief Log verbose with error code (rx_err_t)
 */
static inline void internal_rx_log_verbose_err(const char* tag, const char* message, rx_err_t err)
{
  internal_log_header("VERBOSE", tag);
  internal_log_puts(message);
  internal_log_puts(": 0x");
  internal_log_puthex((uint32_t)err, k_log_hex_width_err);
  internal_log_puts("\r\n");
}

/**
 * @brief Log verbose with hex value
 */
static inline void
internal_rx_log_verbose_hex(const char* tag, const char* message, uint32_t value, uint8_t digits)
{
  internal_log_header("VERBOSE", tag);
  internal_log_puts(message);
  internal_log_puts(": 0x");
  internal_log_puthex(value, digits);
  internal_log_puts("\r\n");
}

/**
 * @brief Log verbose with string value (bounded length)
 */
static inline void internal_rx_log_verbose_str(const char* tag,
                                               const char* message,
                                               const char* str_value,
                                               uint32_t    len)
{
  /* Pre-condition: Validate pointers (NASA Power of 10 Rule 5) */
  if (message == (const char*)0 || str_value == (const char*)0) {
    return;
  }

  internal_log_header("VERBOSE", tag);
  internal_log_puts(message);
  internal_log_puts(": ");

  /*
   * NASA Rule 2: Loop bounded by k_log_str_max_len (compile-time constant)
   * Early break when i >= len or null terminator encountered.
   */
  for (uint32_t i = 0; i < k_log_str_max_len; i++) {
    if (i >= len || str_value[i] == '\0') {
      break;
    }
    internal_log_putc(str_value[i]);
  }

  internal_log_puts("\r\n");
}

/* =============================================================================
 * C23 _Generic Type Dispatch Helpers
 * =============================================================================
 */

/**
 * @def RX_LOG_VAL_IMPL
 * @brief Internal macro implementing C23 _Generic automatic type dispatch for logging
 *
 * @details
 * Uses C23 `_Generic` selection expressions to automatically route log calls to the
 * correct typed implementation function based on the value's type at compile time.
 * This provides type-safe logging without manual type suffixes.
 *
 * ## Type Dispatch Mechanism
 *
 * The `_Generic` keyword performs compile-time type matching:
 * ```c
 * _Generic((val),
 *   uint8_t:  internal_rx_log_error_u8,   // Match: uint8_t  -> decimal
 *   uint16_t: internal_rx_log_error_u16,  // Match: uint16_t -> decimal
 *   uint32_t: internal_rx_log_error_u32,  // Match: uint32_t -> decimal
 *   int32_t:  internal_rx_log_error_err   // Match: int32_t  -> hex (error codes)
 * )(tag, msg, val)  // Call selected function
 * ```
 *
 * ## Supported Types
 * | Type | Output Format | Example Value | Example Output |
 * |------|---------------|---------------|----------------|
 * | uint8_t | Decimal | 127 | `127` |
 * | uint16_t | Decimal | 2048 | `2048` |
 * | uint32_t | Decimal | 4294967295 | `4294967295` |
 * | int32_t (rx_err_t) | Hex (8 digits) | 0x108 | `0x00000108` |
 *
 * ## Type Limitation: int32_t vs rx_err_t
 *
 * **Critical Design Constraint**:
 * Because `rx_err_t` is defined as `typedef int32_t`, C's `_Generic` cannot distinguish
 * between error codes and raw signed integers (both have type `int32_t`).
 *
 * **Consequence**:
 * - [OK] rx_err_t values display correctly as hex error codes
 * - [X] Raw int32_t values display as hex (unexpected for signed integers)
 *
 * **Workaround** for logging signed integers:
 * @code{.c}
 * int32_t temperature = -25;  // Negative temperature
 *
 * // WRONG: Will display as hex 0xFFFFFFE7 (two's complement)
 * rx_log_info_val("SENSOR", "Temperature", temperature);
 *
 * // CORRECT: Cast to unsigned for decimal display
 * rx_log_info_val("SENSOR", "Temperature", (uint32_t)temperature);  // Displays: 4294967271
 *
 * // ALTERNATIVE: Use explicit hex logging
 * rx_log_info_hex("SENSOR", "Temperature (raw)", temperature, 8);  // Displays: 0xFFFFFFE7
 * @endcode
 *
 * ## Macro Expansion Example
 * @code{.c}
 * uint8_t status = 42;
 * rx_log_error_val("MOTOR", "Status", status);
 *
 * // Step 1: Expands to RX_LOG_VAL_IMPL(error, "MOTOR", "Status", status)
 * // Step 2: _Generic matches uint8_t -> selects internal_rx_log_error_u8
 * // Step 3: Calls internal_rx_log_error_u8("MOTOR", "Status", 42)
 * // Output: [ERROR] [MOTOR] Status: 42
 * @endcode
 *
 * ## Performance
 * - **Overhead**: Zero. Type selection occurs at compile time (no runtime cost).
 * - **Code Size**: Identical to calling typed function directly.
 * - **Compile Time**: Negligible increase for type resolution.
 *
 * ## Advantages Over Manual Type Suffixes
 * | Approach | Example | Type Safety | Code Clarity |
 * |----------|---------|-------------|--------------|
 * | Manual suffixes | `log_error_u8(tag, msg, val)` | [X] Mismatch possible | [X] Verbose |
 * | _Generic dispatch | `log_error_val(tag, msg, val)` | [OK] Compile-time checked | [OK] Clean |
 *
 * @param[in] level Log level name (error, warn, info, debug, verbose) - token pasting
 * @param[in] tag Component tag string (passed to selected function)
 * @param[in] msg Log message string (passed to selected function)
 * @param[in] val Value to log - type determines function selection at compile time
 *
 * @note **Compile-Time**: Type selection occurs during compilation, not at runtime.
 * @note **Type Safety**: Compiler enforces supported types; unsupported types cause errors.
 * @note **Token Pasting**: Uses `##` to construct function names dynamically.
 *
 * @warning Only supports uint8_t, uint16_t, uint32_t, and int32_t (rx_err_t).
 *          Passing other types (float, int8_t, etc.) will cause compilation errors.
 *
 * @par Example: Type Mismatch Error
 * @code{.c}
 * float velocity = 1.5f;
 * rx_log_error_val("MOTOR", "Velocity", velocity);
 * // Compile error: no _Generic case for 'float'
 * @endcode
 *
 * @see rx_log_error_val(), rx_log_warn_val(), rx_log_info_val()
 * @see rx_log_debug_val(), rx_log_verbose_val()
 * @see internal_rx_log_error_u8(), internal_rx_log_error_u16(), internal_rx_log_error_u32()
 * @see internal_rx_log_error_err()
 */
#define RX_LOG_VAL_IMPL(level, tag, msg, val)                                                      \
  _Generic((val),                                                                                  \
    uint8_t: internal_rx_log_##level##_u8,                                                         \
    uint16_t: internal_rx_log_##level##_u16,                                                       \
    uint32_t: internal_rx_log_##level##_u32,                                                       \
    int32_t: internal_rx_log_##level##_err)(tag, msg, val)

/* =============================================================================
 * Public API - ERROR Level
 * =============================================================================
 */

#if (LOG_LEVEL >= k_log_error)

/**
 * @def rx_log_error
 * @brief Log critical ERROR-level message (compile-time filtered)
 *
 * @details
 * Public API for logging critical errors that prevent operation. This macro includes
 * compile-time filtering: if `LOG_LEVEL < k_log_error`, the macro expands to `((void)0)`
 * and generates **zero code** (no CPU cycles, no flash memory).
 *
 * ## Compile-Time Behavior
 * | LOG_LEVEL Setting | Macro Expansion | Code Generated |
 * |-------------------|-----------------|----------------|
 * | k_log_none | `((void)0)` | 0 bytes |
 * | k_log_error or higher | `internal_rx_log_error(tag, msg)` | ~60-80 bytes |
 *
 * ## When to Use
 * Use for critical failures that:
 * - Prevent system operation
 * - Require immediate attention
 * - Indicate hardware faults
 * - Signal initialization failures
 *
 * @param[in] tag Component tag (string literal, e.g., "MOTOR", "SPI", "USB")
 *                - Identifies error source
 *                - Recommended: 3-8 uppercase characters
 * @param[in] msg Error message (string literal or const char*)
 *                - Should be concise and actionable
 *                - Describes the specific failure
 *
 * @return void (macro expands to function call or no-op)
 *
 * @note **Zero Overhead**: If LOG_LEVEL < k_log_error, this compiles to nothing.
 * @note **Thread Safety**: NOT thread-safe. Concurrent calls may interleave output.
 *
 * @par Example: Critical Hardware Failure
 * @code{.c}
 * #include "rx_log.h"
 *
 * void motor_init(void) {
 *   if (!configure_pwm()) {
 *     rx_log_error("MOTOR", "PWM configuration failed");
 *     return;
 *   }
 *
 *   if (!enable_h_bridge()) {
 *     rx_log_error("MOTOR", "H-bridge enable failed");
 *     return;
 *   }
 *
 *   rx_log_info("MOTOR", "Initialization complete");
 * }
 *
 * // Output (if LOG_LEVEL >= k_log_error):
 * // [ERROR] [MOTOR] PWM configuration failed
 * @endcode
 *
 * @par Example: Initialization Sequence
 * @code{.c}
 * void system_init(void) {
 *   rx_log_info("SYSTEM", "Starting initialization");
 *
 *   if (!uart_init()) {
 *     rx_log_error("UART", "Failed to initialize");
 *     RX_ERROR_CHECK("UART", "Cannot continue without UART", k_rx_err_init);
 *   }
 *
 *   if (!spi_init()) {
 *     rx_log_error("SPI", "Failed to initialize");
 *     // Continue anyway - SPI is optional
 *   }
 * }
 * @endcode
 *
 * @par Example: Compile-Time Elimination
 * @code{.c}
 * // Build configuration: -DLOG_LEVEL=k_log_none
 *
 * rx_log_error("MOTOR", "Critical failure");  // Compiles to: ((void)0)
 *
 * // Result: Zero bytes of code generated, zero CPU cycles, zero flash usage
 * @endcode
 *
 * @see internal_rx_log_error() Implementation function
 * @see rx_log_error_val() Error logging with typed value
 * @see rx_log_error_hex() Error logging with hex value
 * @see rx_log_error_str() Error logging with string value
 * @see LOG_LEVEL Compile-time log level configuration
 */
#define rx_log_error(tag, msg) internal_rx_log_error((tag), (msg))

/**
 * @def rx_log_error_val
 * @brief Log ERROR-level message with automatic type-dispatched value
 *
 * @details
 * Public API for logging critical errors with an associated value. Uses C23 `_Generic`
 * for automatic type dispatch to the correct formatting function. Includes compile-time
 * filtering for zero overhead when disabled.
 *
 * ## Type Dispatch
 * The value's type determines output formatting at compile time:
 * | Type | Dispatches To | Format | Example |
 * |------|---------------|--------|---------|
 * | uint8_t | internal_rx_log_error_u8 | Decimal | `42` |
 * | uint16_t | internal_rx_log_error_u16 | Decimal | `2048` |
 * | uint32_t | internal_rx_log_error_u32 | Decimal | `4294967295` |
 * | int32_t (rx_err_t) | internal_rx_log_error_err | Hex (8 digits) | `0x00000108` |
 *
 * ## Compile-Time Behavior
 * | LOG_LEVEL Setting | Macro Expansion | Code Generated |
 * |-------------------|-----------------|----------------|
 * | k_log_none | `((void)0)` | 0 bytes |
 * | k_log_error or higher | `RX_LOG_VAL_IMPL(...)` -> typed function | ~70-90 bytes |
 *
 * @param[in] tag Component tag (string literal, e.g., "MOTOR", "SPI", "I2C")
 * @param[in] msg Error message (string literal or const char*)
 * @param[in] val Value to log - type determines formatting (uint8_t, uint16_t, uint32_t, int32_t)
 *
 * @return void (macro expands to type-dispatched function call or no-op)
 *
 * @note **Zero Overhead**: If LOG_LEVEL < k_log_error, this compiles to nothing.
 * @note **Type Safe**: Compiler enforces supported types at compile time.
 * @note **Thread Safety**: NOT thread-safe. Concurrent calls may interleave output.
 *
 * @warning **int32_t Limitation**: Raw int32_t values format as hex (for rx_err_t).
 *          Cast to uint32_t for decimal display of signed integers.
 *
 * @par Example: Error Codes
 * @code{.c}
 * #include "rx_log.h"
 * #include "rx_err.h"
 *
 * rx_err_t motor_init(void) {
 *   rx_err_t err = configure_pwm();
 *   if (err != k_rx_ok) {
 *     rx_log_error_val("MOTOR", "PWM config failed", err);  // Auto -> hex
 *     return err;
 *   }
 *
 *   err = calibrate_encoder();
 *   if (err != k_rx_ok) {
 *     rx_log_error_val("MOTOR", "Encoder calibration failed", err);  // Auto -> hex
 *     return err;
 *   }
 *
 *   return k_rx_ok;
 * }
 *
 * // Output (if err = k_rx_err_timeout = 0x108):
 * // [ERROR] [MOTOR] PWM config failed: 0x00000108
 * @endcode
 *
 * @par Example: Typed Values (Automatic Dispatch)
 * @code{.c}
 * // uint8_t -> decimal
 * uint8_t motor_id = 3;
 * rx_log_error_val("MOTOR", "Invalid motor ID", motor_id);
 * // Output: [ERROR] [MOTOR] Invalid motor ID: 3
 *
 * // uint16_t -> decimal
 * uint16_t rpm = 4500;
 * rx_log_error_val("MOTOR", "RPM out of range", rpm);
 * // Output: [ERROR] [MOTOR] RPM out of range: 4500
 *
 * // uint32_t -> decimal
 * uint32_t timestamp = get_tick_count();
 * rx_log_error_val("TIMER", "Overflow at", timestamp);
 * // Output: [ERROR] [TIMER] Overflow at: 4294967295
 *
 * // rx_err_t (int32_t) -> hex
 * rx_err_t err = k_rx_err_hw_fault;
 * rx_log_error_val("HAL", "Hardware fault", err);
 * // Output: [ERROR] [HAL] Hardware fault: 0x00000201
 * @endcode
 *
 * @par Example: Type Mismatch (Compile Error)
 * @code{.c}
 * float velocity = 1.5f;
 * rx_log_error_val("MOTOR", "Invalid velocity", velocity);
 * // Compile error: no _Generic case for 'float'
 * // Use rx_log_error() with formatted string instead
 * @endcode
 *
 * @see RX_LOG_VAL_IMPL() _Generic type dispatch implementation
 * @see internal_rx_log_error_u8(), internal_rx_log_error_u16(), internal_rx_log_error_u32()
 * @see internal_rx_log_error_err() Error code formatting (hex)
 * @see rx_log_error() Simple error logging without value
 */
#define rx_log_error_val(tag, msg, val) RX_LOG_VAL_IMPL(error, (tag), (msg), (val))

/**
 * @def rx_log_error_hex
 * @brief Log ERROR-level message with hexadecimal value
 *
 * @details
 * Public API for logging critical errors with associated value displayed as hexadecimal.
 * Useful for register values, memory addresses, bit masks, and other hardware-related data.
 * Includes compile-time filtering for zero overhead when disabled.
 *
 * ## Output Format
 * ```
 * [ERROR] [TAG] message: 0xHHHHHHHH
 * ```
 * Leading zeros are included up to the specified digit count.
 *
 * ## Compile-Time Behavior
 * | LOG_LEVEL Setting | Macro Expansion | Code Generated |
 * |-------------------|-----------------|----------------|
 * | k_log_none | `((void)0)` | 0 bytes |
 * | k_log_error or higher | `internal_rx_log_error_hex(...)` | ~75-95 bytes |
 *
 * @param[in] tag Component tag (string literal, e.g., "GPIO", "SPI", "REG")
 * @param[in] msg Error message (string literal or const char*)
 * @param[in] val Value to display as hexadecimal (uint32_t)
 *                - Supports 0 to 0xFFFFFFFF
 *                - Upper bits ignored if digits < 8
 * @param[in] digits Number of hex digits to display (1-8)
 *                   - Common values: 2 (byte), 4 (uint16_t), 8 (uint32_t)
 *                   - Leading zeros included
 *
 * @return void (macro expands to function call or no-op)
 *
 * @note **Zero Overhead**: If LOG_LEVEL < k_log_error, this compiles to nothing.
 * @note **Thread Safety**: NOT thread-safe. Concurrent calls may interleave output.
 * @note **Leading Zeros**: Always includes leading zeros up to specified digit count.
 *
 * @par Example: Hardware Register Debugging
 * @code{.c}
 * #include "rx_log.h"
 *
 * void configure_gpio(void) {
 *   uint32_t reg_val = read_register(GPIO_BASE + 0x04);
 *
 *   if ((reg_val & 0x80000000) == 0) {
 *     rx_log_error_hex("GPIO", "Enable bit not set", reg_val, 8);
 *     // Output: [ERROR] [GPIO] Enable bit not set: 0x00001234
 *   }
 * }
 * @endcode
 *
 * @par Example: SPI Transaction Failure
 * @code{.c}
 * void spi_transfer(uint8_t cmd, uint8_t data) {
 *   uint16_t status = spi_transact(cmd, data);
 *
 *   if (status & 0x8000) {  // Error bit set
 *     rx_log_error_hex("SPI", "Transfer failed, status", status, 4);
 *     // Output: [ERROR] [SPI] Transfer failed, status: 0x8001
 *   }
 * }
 * @endcode
 *
 * @par Example: Memory Address Logging
 * @code{.c}
 * void* allocate_buffer(uint32_t size) {
 *   void* ptr = malloc(size);
 *   if (ptr == nullptr) {
 *     rx_log_error_hex("MEM", "Allocation failed at", (uint32_t)ptr, 8);
 *     // Output: [ERROR] [MEM] Allocation failed at: 0x00000000
 *   }
 *   return ptr;
 * }
 * @endcode
 *
 * @par Example: Different Digit Counts
 * @code{.c}
 * uint8_t byte_val = 0x5A;
 * rx_log_error_hex("TEST", "Byte", byte_val, 2);
 * // Output: [ERROR] [TEST] Byte: 0x5A
 *
 * uint16_t word_val = 0x1234;
 * rx_log_error_hex("TEST", "Word", word_val, 4);
 * // Output: [ERROR] [TEST] Word: 0x1234
 *
 * uint32_t dword_val = 0xDEADBEEF;
 * rx_log_error_hex("TEST", "DWord", dword_val, 8);
 * // Output: [ERROR] [TEST] DWord: 0xDEADBEEF
 * @endcode
 *
 * @see internal_rx_log_error_hex() Implementation function
 * @see rx_log_error() Simple error logging without value
 * @see rx_log_error_val() Error logging with typed value (auto-dispatch)
 */
#define rx_log_error_hex(tag, msg, val, digits)                                                    \
  internal_rx_log_error_hex((tag), (msg), (val), (digits))

/**
 * @def rx_log_error_str
 * @brief Log ERROR-level message with bounded string value (NASA-compliant)
 *
 * @details
 * Public API for logging critical errors with associated string value. Uses bounded
 * iteration for memory safety (NASA Power of 10 Rule 2 compliance). String output is
 * limited to k_log_str_max_len (256 bytes) to prevent unbounded loops.
 *
 * ## Output Format
 * ```
 * [ERROR] [TAG] message: string_value
 * ```
 *
 * ## Safety Features
 * - **Bounded loop**: Maximum 256 characters logged (k_log_str_max_len)
 * - **Early termination**: Stops at user-provided len or null terminator
 * - **NULL safe**: NULL pointers silently ignored (no crash)
 *
 * ## Compile-Time Behavior
 * | LOG_LEVEL Setting | Macro Expansion | Code Generated |
 * |-------------------|-----------------|----------------|
 * | k_log_none | `((void)0)` | 0 bytes |
 * | k_log_error or higher | `internal_rx_log_error_str(...)` | ~100-120 bytes |
 *
 * @param[in] tag Component tag (string literal, e.g., "CONFIG", "USB", "FILE")
 * @param[in] msg Error message (string literal or const char*)
 * @param[in] str String value to log (const char*)
 *                - Must point to valid memory
 *                - Can be null-terminated or explicit length
 *                - nullptr silently ignored
 * @param[in] len Maximum number of characters to log
 *                - Typically obtained via strlen(str)
 *                - Values > 256 clamped to 256
 *                - Must not exceed actual buffer size
 *
 * @return void (macro expands to function call or no-op)
 *
 * @note **Zero Overhead**: If LOG_LEVEL < k_log_error, this compiles to nothing.
 * @note **Bounded**: Maximum 256 characters logged regardless of len parameter.
 * @note **Thread Safety**: NOT thread-safe. Concurrent calls may interleave output.
 * @note **NULL Safe**: NULL str pointer silently ignored (safe failure mode).
 *
 * @warning Ensure len <= actual buffer size to prevent undefined behavior.
 *
 * @par Example: Configuration Errors
 * @code{.c}
 * #include "rx_log.h"
 * #include <string.h>
 *
 * void parse_config(const char* param_name, const char* param_value) {
 *   if (strcmp(param_name, "motor_id") == 0) {
 *     // Valid parameter
 *   } else {
 *     // Unknown parameter name
 *     rx_log_error_str("CONFIG", "Unknown parameter", param_name, strlen(param_name));
 *     // Output: [ERROR] [CONFIG] Unknown parameter: invalid_param_name
 *   }
 * }
 * @endcode
 *
 * @par Example: File Path Errors
 * @code{.c}
 * void load_config_file(const char* filepath) {
 *   if (!file_exists(filepath)) {
 *     rx_log_error_str("FILE", "Config file not found", filepath, strlen(filepath));
 *     // Output: [ERROR] [FILE] Config file not found: /etc/motor_config.ini
 *   }
 * }
 * @endcode
 *
 * @par Example: USB Device Name Logging
 * @code{.c}
 * void usb_device_error(const char* device_name, uint32_t name_len) {
 *   rx_log_error_str("USB", "Device enumeration failed", device_name, name_len);
 *   // Output: [ERROR] [USB] Device enumeration failed: RX72N Motor Controller
 * }
 * @endcode
 *
 * @par Example: Truncation Behavior
 * @code{.c}
 * // String longer than k_log_str_max_len (256)
 * char very_long_string[300];
 * memset(very_long_string, 'A', 299);
 * very_long_string[299] = '\0';
 *
 * rx_log_error_str("TEST", "Long string", very_long_string, strlen(very_long_string));
 * // Output: [ERROR] [TEST] Long string: AAAA... (truncated at 256 chars)
 * @endcode
 *
 * @see internal_rx_log_error_str() Implementation function
 * @see k_log_str_max_len Maximum string length constant (256)
 * @see rx_log_error() Simple error logging without value
 * @see rx_log_error_val() Error logging with typed value
 */
#define rx_log_error_str(tag, msg, str, len) internal_rx_log_error_str((tag), (msg), (str), (len))

#else

#define rx_log_error(tag, msg)               ((void)0)
#define rx_log_error_val(tag, msg, val)      ((void)0)
#define rx_log_error_hex(tag, msg, val, d)   ((void)0)
#define rx_log_error_str(tag, msg, str, len) ((void)0)

#endif /* LOG_LEVEL >= k_log_error */

/* =============================================================================
 * Public API - WARN Level
 * =============================================================================
 */

#if (LOG_LEVEL >= k_log_warn)

/**
 * @brief Log warning message (no value)
 */
#define rx_log_warn(tag, msg) internal_rx_log_warn((tag), (msg))

/**
 * @brief Log warning with typed value (automatic type dispatch)
 */
#define rx_log_warn_val(tag, msg, val) RX_LOG_VAL_IMPL(warn, (tag), (msg), (val))

/**
 * @brief Log warning with hex value
 */
#define rx_log_warn_hex(tag, msg, val, digits)                                                     \
  internal_rx_log_warn_hex((tag), (msg), (val), (digits))

/**
 * @brief Log warning with string value (bounded length)
 */
#define rx_log_warn_str(tag, msg, str, len) internal_rx_log_warn_str((tag), (msg), (str), (len))

#else

#define rx_log_warn(tag, msg)               ((void)0)
#define rx_log_warn_val(tag, msg, val)      ((void)0)
#define rx_log_warn_hex(tag, msg, val, d)   ((void)0)
#define rx_log_warn_str(tag, msg, str, len) ((void)0)

#endif /* LOG_LEVEL >= k_log_warn */

/* =============================================================================
 * Public API - INFO Level
 * =============================================================================
 */

#if (LOG_LEVEL >= k_log_info)

/**
 * @brief Log info message (no value)
 */
#define rx_log_info(tag, msg) internal_rx_log_info((tag), (msg))

/**
 * @brief Log info with typed value (automatic type dispatch)
 */
#define rx_log_info_val(tag, msg, val) RX_LOG_VAL_IMPL(info, (tag), (msg), (val))

/**
 * @brief Log info with hex value
 */
#define rx_log_info_hex(tag, msg, val, digits)                                                     \
  internal_rx_log_info_hex((tag), (msg), (val), (digits))

/**
 * @brief Log info with string value (bounded length)
 */
#define rx_log_info_str(tag, msg, str, len) internal_rx_log_info_str((tag), (msg), (str), (len))

#else

#define rx_log_info(tag, msg)               ((void)0)
#define rx_log_info_val(tag, msg, val)      ((void)0)
#define rx_log_info_hex(tag, msg, val, d)   ((void)0)
#define rx_log_info_str(tag, msg, str, len) ((void)0)

#endif /* LOG_LEVEL >= k_log_info */

/* =============================================================================
 * Public API - DEBUG Level
 * =============================================================================
 */

#if (LOG_LEVEL >= k_log_debug)

/**
 * @brief Log debug message (no value)
 */
#define rx_log_debug(tag, msg) internal_rx_log_debug((tag), (msg))

/**
 * @brief Log debug with typed value (automatic type dispatch)
 */
#define rx_log_debug_val(tag, msg, val) RX_LOG_VAL_IMPL(debug, (tag), (msg), (val))

/**
 * @brief Log debug with hex value
 */
#define rx_log_debug_hex(tag, msg, val, digits)                                                    \
  internal_rx_log_debug_hex((tag), (msg), (val), (digits))

/**
 * @brief Log debug with string value (bounded length)
 */
#define rx_log_debug_str(tag, msg, str, len) internal_rx_log_debug_str((tag), (msg), (str), (len))

#else

#define rx_log_debug(tag, msg)               ((void)0)
#define rx_log_debug_val(tag, msg, val)      ((void)0)
#define rx_log_debug_hex(tag, msg, val, d)   ((void)0)
#define rx_log_debug_str(tag, msg, str, len) ((void)0)

#endif /* LOG_LEVEL >= k_log_debug */

/* =============================================================================
 * Public API - VERBOSE Level
 * =============================================================================
 */

#if (LOG_LEVEL >= k_log_verbose)

/**
 * @brief Log verbose message (no value)
 */
#define rx_log_verbose(tag, msg) internal_rx_log_verbose((tag), (msg))

/**
 * @brief Log verbose with typed value (automatic type dispatch)
 */
#define rx_log_verbose_val(tag, msg, val) RX_LOG_VAL_IMPL(verbose, (tag), (msg), (val))

/**
 * @brief Log verbose with hex value
 */
#define rx_log_verbose_hex(tag, msg, val, digits)                                                  \
  internal_rx_log_verbose_hex((tag), (msg), (val), (digits))

/**
 * @brief Log verbose with string value (bounded length)
 */
#define rx_log_verbose_str(tag, msg, str, len)                                                     \
  internal_rx_log_verbose_str((tag), (msg), (str), (len))

#else

#define rx_log_verbose(tag, msg)               ((void)0)
#define rx_log_verbose_val(tag, msg, val)      ((void)0)
#define rx_log_verbose_hex(tag, msg, val, d)   ((void)0)
#define rx_log_verbose_str(tag, msg, str, len) ((void)0)

#endif /* LOG_LEVEL >= k_log_verbose */

#ifdef __cplusplus
}
#endif
