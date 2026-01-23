/* lib/rx_core/inc/rx_log.h */

/**
 * @file rx_log.h
 * @brief Type-Safe Logging System for RX72N Firmware
 * @details
 * Modern logging system designed specifically for RX72N bare-metal firmware.
 * Uses C23 _Generic for automatic type dispatch and compile-time filtering
 * for zero overhead when logs are disabled.
 *
 * Key Features:
 * - Compile-time log level filtering (zero overhead for disabled logs)
 * - C23 _Generic automatic type dispatch (no manual type suffixes needed)
 * - Type-safe with explicit-width integer types (uint8_t, uint16_t, uint32_t, int32_t)
 * - Bounded string safety (explicit lengths required, NASA Power of 10 compliant)
 * - UART-only output (no printf, suitable for bare-metal)
 * - Clean rx_ namespace prefix (clear project ownership)
 *
 * Log Levels (from highest to lowest priority):
 * - ERROR: Critical errors that prevent operation
 * - WARN: Warnings about recoverable issues
 * - INFO: Important informational messages
 * - DEBUG: Detailed debugging information
 * - VERBOSE: Very detailed trace information
 *
 * Usage Examples:
 * @code
 *   // Basic message (no value)
 *   rx_log_error("MOTOR", "Initialization failed");
 *
 *   // With typed value (automatic dispatch via _Generic)
 *   uint8_t status = get_status();
 *   rx_log_error_val("MOTOR", "Invalid status", status);  // Auto-dispatches to _u8
 *
 *   // With error code
 *   rx_err_t err = init_motor();
 *   rx_log_error_val("MOTOR", "Init failed", err);  // Auto-dispatches to _err
 *
 *   // With hex value
 *   rx_log_error_hex("GPIO", "Register value", reg_val, 8);
 *
 *   // With string (explicit length for safety)
 *   rx_log_error_str("CONFIG", "Unknown parameter", param_name, strlen(param_name));
 * @endcode
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_LOG_H
#define STAR_RX72N_LOG_H

#include <stdint.h>
#include <string.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for UART functions (avoid circular dependency) */
void uart_putc(char data);
void uart_puts(const char* str);
void uart_putint(int32_t value);
void uart_puthex(uint32_t value, uint8_t digits);

/* =============================================================================
 * Log Level Definitions
 * =============================================================================
 */

/**
 * @brief Log levels (priority from low to high)
 */
typedef enum : uint8_t {
  k_log_none    = 0, /**< No logging */
  k_log_error   = 1, /**< Critical errors only */
  k_log_warn    = 2, /**< Errors and warnings */
  k_log_info    = 3, /**< Errors, warnings, and info */
  k_log_debug   = 4, /**< Errors, warnings, info, and debug */
  k_log_verbose = 5, /**< All messages */
} log_level_t;

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
 * Internal Helper: Format Log Header
 * =============================================================================
 */

/**
 * @brief Internal helper to write log header [LEVEL] [TAG]
 *
 * @param[in] level_str Log level string (e.g., "ERROR", "WARN")
 * @param[in] tag Component tag string
 */
static inline void internal_log_header(const char* level_str, const char* tag)
{
  uart_putc('[');
  uart_puts(level_str);
  uart_puts("] [");
  uart_puts(tag);
  uart_puts("] ");
}

/* =============================================================================
 * ERROR Level - Internal Implementations
 * =============================================================================
 */

/**
 * @brief Log error message (no value)
 */
static inline void internal_rx_log_error(const char* tag, const char* message)
{
  internal_log_header("ERROR", tag);
  uart_puts(message);
  uart_puts("\r\n");
}

/**
 * @brief Log error with uint8_t value
 */
static inline void internal_rx_log_error_u8(const char* tag, const char* message, uint8_t value)
{
  internal_log_header("ERROR", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log error with uint16_t value
 */
static inline void internal_rx_log_error_u16(const char* tag, const char* message, uint16_t value)
{
  internal_log_header("ERROR", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log error with uint32_t value
 */
static inline void internal_rx_log_error_u32(const char* tag, const char* message, uint32_t value)
{
  internal_log_header("ERROR", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log error with int32_t value
 */
static inline void internal_rx_log_error_i32(const char* tag, const char* message, int32_t value)
{
  internal_log_header("ERROR", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint(value);
  uart_puts("\r\n");
}

/**
 * @brief Log error with error code (rx_err_t)
 */
static inline void internal_rx_log_error_err(const char* tag, const char* message, rx_err_t err)
{
  internal_log_header("ERROR", tag);
  uart_puts(message);
  uart_puts(": 0x");
  uart_puthex((uint32_t)err, 8);
  uart_puts("\r\n");
}

/**
 * @brief Log error with hex value
 */
static inline void
internal_rx_log_error_hex(const char* tag, const char* message, uint32_t value, uint8_t digits)
{
  internal_log_header("ERROR", tag);
  uart_puts(message);
  uart_puts(": 0x");
  uart_puthex(value, digits);
  uart_puts("\r\n");
}

/**
 * @brief Log error with string value (bounded length)
 */
static inline void
internal_rx_log_error_str(const char* tag, const char* message, const char* str_value, uint32_t len)
{
  internal_log_header("ERROR", tag);
  uart_puts(message);
  uart_puts(": ");

  /* Write string with explicit length bound */
  for (uint32_t i = 0; i < len && str_value[i] != '\0'; i++) {
    uart_putc(str_value[i]);
  }

  uart_puts("\r\n");
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
  internal_log_header("WARN", tag);
  uart_puts(message);
  uart_puts("\r\n");
}

/**
 * @brief Log warning with uint8_t value
 */
static inline void internal_rx_log_warn_u8(const char* tag, const char* message, uint8_t value)
{
  internal_log_header("WARN", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log warning with uint16_t value
 */
static inline void internal_rx_log_warn_u16(const char* tag, const char* message, uint16_t value)
{
  internal_log_header("WARN", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log warning with uint32_t value
 */
static inline void internal_rx_log_warn_u32(const char* tag, const char* message, uint32_t value)
{
  internal_log_header("WARN", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log warning with int32_t value
 */
static inline void internal_rx_log_warn_i32(const char* tag, const char* message, int32_t value)
{
  internal_log_header("WARN", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint(value);
  uart_puts("\r\n");
}

/**
 * @brief Log warning with error code (rx_err_t)
 */
static inline void internal_rx_log_warn_err(const char* tag, const char* message, rx_err_t err)
{
  internal_log_header("WARN", tag);
  uart_puts(message);
  uart_puts(": 0x");
  uart_puthex((uint32_t)err, 8);
  uart_puts("\r\n");
}

/**
 * @brief Log warning with hex value
 */
static inline void
internal_rx_log_warn_hex(const char* tag, const char* message, uint32_t value, uint8_t digits)
{
  internal_log_header("WARN", tag);
  uart_puts(message);
  uart_puts(": 0x");
  uart_puthex(value, digits);
  uart_puts("\r\n");
}

/**
 * @brief Log warning with string value (bounded length)
 */
static inline void
internal_rx_log_warn_str(const char* tag, const char* message, const char* str_value, uint32_t len)
{
  internal_log_header("WARN", tag);
  uart_puts(message);
  uart_puts(": ");

  /* Write string with explicit length bound */
  for (uint32_t i = 0; i < len && str_value[i] != '\0'; i++) {
    uart_putc(str_value[i]);
  }

  uart_puts("\r\n");
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
  internal_log_header("INFO", tag);
  uart_puts(message);
  uart_puts("\r\n");
}

/**
 * @brief Log info with uint8_t value
 */
static inline void internal_rx_log_info_u8(const char* tag, const char* message, uint8_t value)
{
  internal_log_header("INFO", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log info with uint16_t value
 */
static inline void internal_rx_log_info_u16(const char* tag, const char* message, uint16_t value)
{
  internal_log_header("INFO", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log info with uint32_t value
 */
static inline void internal_rx_log_info_u32(const char* tag, const char* message, uint32_t value)
{
  internal_log_header("INFO", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log info with int32_t value
 */
static inline void internal_rx_log_info_i32(const char* tag, const char* message, int32_t value)
{
  internal_log_header("INFO", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint(value);
  uart_puts("\r\n");
}

/**
 * @brief Log info with error code (rx_err_t)
 */
static inline void internal_rx_log_info_err(const char* tag, const char* message, rx_err_t err)
{
  internal_log_header("INFO", tag);
  uart_puts(message);
  uart_puts(": 0x");
  uart_puthex((uint32_t)err, 8);
  uart_puts("\r\n");
}

/**
 * @brief Log info with hex value
 */
static inline void
internal_rx_log_info_hex(const char* tag, const char* message, uint32_t value, uint8_t digits)
{
  internal_log_header("INFO", tag);
  uart_puts(message);
  uart_puts(": 0x");
  uart_puthex(value, digits);
  uart_puts("\r\n");
}

/**
 * @brief Log info with string value (bounded length)
 */
static inline void
internal_rx_log_info_str(const char* tag, const char* message, const char* str_value, uint32_t len)
{
  internal_log_header("INFO", tag);
  uart_puts(message);
  uart_puts(": ");

  /* Write string with explicit length bound */
  for (uint32_t i = 0; i < len && str_value[i] != '\0'; i++) {
    uart_putc(str_value[i]);
  }

  uart_puts("\r\n");
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
  internal_log_header("DEBUG", tag);
  uart_puts(message);
  uart_puts("\r\n");
}

/**
 * @brief Log debug with uint8_t value
 */
static inline void internal_rx_log_debug_u8(const char* tag, const char* message, uint8_t value)
{
  internal_log_header("DEBUG", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log debug with uint16_t value
 */
static inline void internal_rx_log_debug_u16(const char* tag, const char* message, uint16_t value)
{
  internal_log_header("DEBUG", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log debug with uint32_t value
 */
static inline void internal_rx_log_debug_u32(const char* tag, const char* message, uint32_t value)
{
  internal_log_header("DEBUG", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log debug with int32_t value
 */
static inline void internal_rx_log_debug_i32(const char* tag, const char* message, int32_t value)
{
  internal_log_header("DEBUG", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint(value);
  uart_puts("\r\n");
}

/**
 * @brief Log debug with error code (rx_err_t)
 */
static inline void internal_rx_log_debug_err(const char* tag, const char* message, rx_err_t err)
{
  internal_log_header("DEBUG", tag);
  uart_puts(message);
  uart_puts(": 0x");
  uart_puthex((uint32_t)err, 8);
  uart_puts("\r\n");
}

/**
 * @brief Log debug with hex value
 */
static inline void
internal_rx_log_debug_hex(const char* tag, const char* message, uint32_t value, uint8_t digits)
{
  internal_log_header("DEBUG", tag);
  uart_puts(message);
  uart_puts(": 0x");
  uart_puthex(value, digits);
  uart_puts("\r\n");
}

/**
 * @brief Log debug with string value (bounded length)
 */
static inline void
internal_rx_log_debug_str(const char* tag, const char* message, const char* str_value, uint32_t len)
{
  internal_log_header("DEBUG", tag);
  uart_puts(message);
  uart_puts(": ");

  /* Write string with explicit length bound */
  for (uint32_t i = 0; i < len && str_value[i] != '\0'; i++) {
    uart_putc(str_value[i]);
  }

  uart_puts("\r\n");
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
  internal_log_header("VERBOSE", tag);
  uart_puts(message);
  uart_puts("\r\n");
}

/**
 * @brief Log verbose with uint8_t value
 */
static inline void internal_rx_log_verbose_u8(const char* tag, const char* message, uint8_t value)
{
  internal_log_header("VERBOSE", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log verbose with uint16_t value
 */
static inline void internal_rx_log_verbose_u16(const char* tag, const char* message, uint16_t value)
{
  internal_log_header("VERBOSE", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log verbose with uint32_t value
 */
static inline void internal_rx_log_verbose_u32(const char* tag, const char* message, uint32_t value)
{
  internal_log_header("VERBOSE", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint((int32_t)value);
  uart_puts("\r\n");
}

/**
 * @brief Log verbose with int32_t value
 */
static inline void internal_rx_log_verbose_i32(const char* tag, const char* message, int32_t value)
{
  internal_log_header("VERBOSE", tag);
  uart_puts(message);
  uart_puts(": ");
  uart_putint(value);
  uart_puts("\r\n");
}

/**
 * @brief Log verbose with error code (rx_err_t)
 */
static inline void internal_rx_log_verbose_err(const char* tag, const char* message, rx_err_t err)
{
  internal_log_header("VERBOSE", tag);
  uart_puts(message);
  uart_puts(": 0x");
  uart_puthex((uint32_t)err, 8);
  uart_puts("\r\n");
}

/**
 * @brief Log verbose with hex value
 */
static inline void
internal_rx_log_verbose_hex(const char* tag, const char* message, uint32_t value, uint8_t digits)
{
  internal_log_header("VERBOSE", tag);
  uart_puts(message);
  uart_puts(": 0x");
  uart_puthex(value, digits);
  uart_puts("\r\n");
}

/**
 * @brief Log verbose with string value (bounded length)
 */
static inline void internal_rx_log_verbose_str(const char* tag,
                                               const char* message,
                                               const char* str_value,
                                               uint32_t    len)
{
  internal_log_header("VERBOSE", tag);
  uart_puts(message);
  uart_puts(": ");

  /* Write string with explicit length bound */
  for (uint32_t i = 0; i < len && str_value[i] != '\0'; i++) {
    uart_putc(str_value[i]);
  }

  uart_puts("\r\n");
}

/* =============================================================================
 * C23 _Generic Type Dispatch Helpers
 * =============================================================================
 */

/**
 * @brief Internal macro for C23 _Generic type dispatch
 *
 * Automatically selects the correct typed logging function based on the
 * value's type at compile time. Supported types:
 * - uint8_t, uint16_t, uint32_t (unsigned integers)
 * - int32_t → formatted as hex (error codes)
 * - rx_err_t → formatted as hex (error codes)
 *
 * **IMPORTANT - Type Limitation:**
 * Because `rx_err_t` is defined as `typedef int32_t`, C's `_Generic` cannot
 * distinguish between `rx_err_t` and raw `int32_t` values. Both are routed
 * to hex formatting (error code format). This means:
 * - rx_err_t values display correctly as hex error codes
 * - Raw signed integers should NOT be passed to rx_log_*_val() macros
 *   (they will display as hex instead of decimal)
 *
 * **Workaround for signed integers:**
 * Cast to uint32_t or use explicit decimal formatting:
 * ```c
 * int32_t value = -42;
 * rx_log_info(tag, "Value as unsigned");
 * rx_log_val(info, tag, "Value", (uint32_t)value);
 * ```
 *
 * @param[in] level Log level name (error, warn, info, debug, verbose)
 * @param[in] tag Component tag string
 * @param[in] msg Log message string
 * @param[in] val Value to log (type dispatch based on this)
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
 * @brief Log error message (no value)
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] msg Log message string
 *
 * Example:
 * @code
 *   rx_log_error("MOTOR", "Initialization failed");
 * @endcode
 */
#define rx_log_error(tag, msg) internal_rx_log_error((tag), (msg))

/**
 * @brief Log error with typed value (automatic type dispatch)
 *
 * Uses C23 _Generic to automatically select the correct logging function
 * based on the value's type. Supported types: uint8_t, uint16_t, uint32_t,
 * int32_t, rx_err_t.
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] msg Log message string
 * @param[in] val Value to log (type dispatched automatically)
 *
 * Examples:
 * @code
 *   uint8_t status = get_status();
 *   rx_log_error_val("MOTOR", "Invalid status", status);  // Dispatches to _u8
 *
 *   rx_err_t err = init();
 *   rx_log_error_val("MOTOR", "Init failed", err);  // Dispatches to _err
 * @endcode
 */
#define rx_log_error_val(tag, msg, val) RX_LOG_VAL_IMPL(error, (tag), (msg), (val))

/**
 * @brief Log error with hex value
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] msg Log message string
 * @param[in] val Value to log as hex
 * @param[in] digits Number of hex digits (1-8)
 *
 * Example:
 * @code
 *   rx_log_error_hex("GPIO", "Register value", reg_val, 8);
 * @endcode
 */
#define rx_log_error_hex(tag, msg, val, digits)                                                    \
  internal_rx_log_error_hex((tag), (msg), (val), (digits))

/**
 * @brief Log error with string value (bounded length)
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] msg Log message string
 * @param[in] str String value to log
 * @param[in] len Maximum length to log (for safety)
 *
 * Example:
 * @code
 *   rx_log_error_str("CONFIG", "Unknown param", param_name, strlen(param_name));
 * @endcode
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

#endif /* STAR_RX72N_LOG_H */
