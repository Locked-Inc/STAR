/* lib/rx_core/inc/rx_log.h */

/**
 * @file rx_log.h
 * @brief Logging Macros for RX72N Firmware
 * @details
 * Provides ESP-IDF-style logging macros adapted for RX72N UART output.
 * Follows ESP_LOG* pattern for architectural consistency.
 *
 * Log levels (from highest to lowest priority):
 * - ERROR: Critical errors that prevent operation
 * - WARN: Warnings about recoverable issues
 * - INFO: Important informational messages
 * - DEBUG: Detailed debugging information
 * - VERBOSE: Very detailed trace information
 *
 * Compile-time log level control via RX_LOG_LEVEL define.
 * Logs at or above the configured level will be compiled in.
 * Logs below the level are optimized out at compile time (zero overhead).
 *
 * Usage:
 * @code
 *   RX_LOG_ERROR("MOTOR", "Failed to initialize: err=0x%X", err);
 *   RX_LOG_INFO("SYSTEM", "Starting firmware version %d.%d.%d", 1, 0, 0);
 * @endcode
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_LOG_H
#define STAR_RX72N_LOG_H

#include <stdint.h>

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
 * @brief Log level: None (disable all logging)
 */
#define RX_LOG_LEVEL_NONE 0

/**
 * @brief Log level: Error (critical errors only)
 */
#define RX_LOG_LEVEL_ERROR 1

/**
 * @brief Log level: Warning (errors and warnings)
 */
#define RX_LOG_LEVEL_WARN 2

/**
 * @brief Log level: Info (errors, warnings, and info)
 */
#define RX_LOG_LEVEL_INFO 3

/**
 * @brief Log level: Debug (errors, warnings, info, and debug)
 */
#define RX_LOG_LEVEL_DEBUG 4

/**
 * @brief Log level: Verbose (all messages)
 */
#define RX_LOG_LEVEL_VERBOSE 5

/* =============================================================================
 * Default Log Level Configuration
 * =============================================================================
 */

#ifndef RX_LOG_LEVEL
#ifdef DEBUG
#define RX_LOG_LEVEL RX_LOG_LEVEL_DEBUG
#else
#define RX_LOG_LEVEL RX_LOG_LEVEL_INFO
#endif
#endif

/* =============================================================================
 * Internal Logging Functions
 * =============================================================================
 */

/**
 * @brief Internal log function
 *
 * @param[in] level_str Log level string (e.g., "ERROR", "INFO")
 * @param[in] tag Component tag string
 * @param[in] message Log message string
 */
static inline void internal_rx_log(const char* level_str, const char* tag, const char* message)
{
  uart_putc('[');
  uart_puts(level_str);
  uart_puts("] [");
  uart_puts(tag);
  uart_puts("] ");
  uart_puts(message);
  uart_puts("\r\n");
}

/**
 * @brief Internal log function with file/line info
 *
 * @param[in] level_str Log level string
 * @param[in] tag Component tag string
 * @param[in] file Source file name
 * @param[in] line Line number
 * @param[in] message Log message string
 */
static inline void internal_rx_log_detailed(const char* level_str,
                                            const char* tag,
                                            const char* file,
                                            int32_t     line,
                                            const char* message)
{
  uart_putc('[');
  uart_puts(level_str);
  uart_puts("] [");
  uart_puts(tag);
  uart_puts("] ");
  uart_puts(file);
  uart_putc(':');
  uart_putint(line);
  uart_puts(": ");
  uart_puts(message);
  uart_puts("\r\n");
}

/* =============================================================================
 * Basic Logging Macros
 * =============================================================================
 */

/**
 * @brief Log an error message
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] message Message string (no formatting support)
 *
 * Example: RX_LOG_ERROR("MOTOR", "Initialization failed");
 */
#if (RX_LOG_LEVEL >= RX_LOG_LEVEL_ERROR)
#define RX_LOG_ERROR(tag, message) internal_rx_log("ERROR", tag, message)
#else
#define RX_LOG_ERROR(tag, message) ((void)0)
#endif

/**
 * @brief Log a warning message
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] message Message string
 *
 * Example: RX_LOG_WARN("SPI", "Retrying communication");
 */
#if (RX_LOG_LEVEL >= RX_LOG_LEVEL_WARN)
#define RX_LOG_WARN(tag, message) internal_rx_log("WARN", tag, message)
#else
#define RX_LOG_WARN(tag, message) ((void)0)
#endif

/**
 * @brief Log an informational message
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] message Message string
 *
 * Example: RX_LOG_INFO("SYSTEM", "Firmware started");
 */
#if (RX_LOG_LEVEL >= RX_LOG_LEVEL_INFO)
#define RX_LOG_INFO(tag, message) internal_rx_log("INFO", tag, message)
#else
#define RX_LOG_INFO(tag, message) ((void)0)
#endif

/**
 * @brief Log a debug message
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] message Message string
 *
 * Example: RX_LOG_DEBUG("GPIO", "Pin configured");
 */
#if (RX_LOG_LEVEL >= RX_LOG_LEVEL_DEBUG)
#define RX_LOG_DEBUG(tag, message) internal_rx_log("DEBUG", tag, message)
#else
#define RX_LOG_DEBUG(tag, message) ((void)0)
#endif

/**
 * @brief Log a verbose message
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] message Message string
 *
 * Example: RX_LOG_VERBOSE("TIMER", "Tick processed");
 */
#if (RX_LOG_LEVEL >= RX_LOG_LEVEL_VERBOSE)
#define RX_LOG_VERBOSE(tag, message) internal_rx_log("VERBOSE", tag, message)
#else
#define RX_LOG_VERBOSE(tag, message) ((void)0)
#endif

/* =============================================================================
 * Detailed Logging Macros (with file/line)
 * =============================================================================
 */

/**
 * @brief Log an error message with file and line number
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] message Message string
 *
 * Example: RX_LOG_ERROR_DETAILED("MOTOR", "Init failed");
 * Output: [ERROR] [MOTOR] motor.c:123: Init failed
 */
#if (RX_LOG_LEVEL >= RX_LOG_LEVEL_ERROR)
#define RX_LOG_ERROR_DETAILED(tag, message)                                                        \
  internal_rx_log_detailed("ERROR", tag, __FILE__, __LINE__, message)
#else
#define RX_LOG_ERROR_DETAILED(tag, message) ((void)0)
#endif

/**
 * @brief Log a warning message with file and line number
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] message Message string
 */
#if (RX_LOG_LEVEL >= RX_LOG_LEVEL_WARN)
#define RX_LOG_WARN_DETAILED(tag, message)                                                         \
  internal_rx_log_detailed("WARN", tag, __FILE__, __LINE__, message)
#else
#define RX_LOG_WARN_DETAILED(tag, message) ((void)0)
#endif

/**
 * @brief Log an info message with file and line number
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] message Message string
 */
#if (RX_LOG_LEVEL >= RX_LOG_LEVEL_INFO)
#define RX_LOG_INFO_DETAILED(tag, message)                                                         \
  internal_rx_log_detailed("INFO", tag, __FILE__, __LINE__, message)
#else
#define RX_LOG_INFO_DETAILED(tag, message) ((void)0)
#endif

/**
 * @brief Log a debug message with file and line number
 *
 * @param[in] tag Component tag (string literal)
 * @param[in] message Message string
 */
#if (RX_LOG_LEVEL >= RX_LOG_LEVEL_DEBUG)
#define RX_LOG_DEBUG_DETAILED(tag, message)                                                        \
  internal_rx_log_detailed("DEBUG", tag, __FILE__, __LINE__, message)
#else
#define RX_LOG_DEBUG_DETAILED(tag, message) ((void)0)
#endif

/* =============================================================================
 * Error Code Logging Helpers
 * =============================================================================
 */

/**
 * @brief Log an error code as hex
 *
 * @param[in] tag Component tag
 * @param[in] err Error code to log
 *
 * Note: This is a simple helper. For formatted output, manually construct
 * the log message.
 */
static inline void rx_log_error_code(const char* tag, rx_err_t err)
{
  uart_putc('[');
  uart_puts("ERROR");
  uart_puts("] [");
  uart_puts(tag);
  uart_puts("] Error code: ");
  uart_puthex((uint32_t)err, 8);
  uart_puts("\r\n");
}

/**
 * @brief Log error with message and integer value
 *
 * @param[in] tag Component tag
 * @param[in] message Error message
 * @param[in] value Integer value to log
 */
static inline void rx_log_error_int(const char* tag, const char* message, int32_t value)
{
  uart_putc('[');
  uart_puts("ERROR");
  uart_puts("] [");
  uart_puts(tag);
  uart_puts("] ");
  uart_puts(message);
  uart_puts(": ");
  uart_putint(value);
  uart_puts("\r\n");
}

/**
 * @brief Log warning with message and integer value
 *
 * @param[in] tag Component tag
 * @param[in] message Warning message
 * @param[in] value Integer value to log
 */
static inline void rx_log_warn_int(const char* tag, const char* message, int32_t value)
{
  uart_putc('[');
  uart_puts("WARN");
  uart_puts("] [");
  uart_puts(tag);
  uart_puts("] ");
  uart_puts(message);
  uart_puts(": ");
  uart_putint(value);
  uart_puts("\r\n");
}

/**
 * @brief Log info with message and string value
 *
 * @param[in] tag Component tag
 * @param[in] message Info message
 * @param[in] str_value String value to log
 */
static inline void rx_log_info_str(const char* tag, const char* message, const char* str_value)
{
  uart_putc('[');
  uart_puts("INFO");
  uart_puts("] [");
  uart_puts(tag);
  uart_puts("] ");
  uart_puts(message);
  uart_puts(": ");
  uart_puts(str_value);
  uart_puts("\r\n");
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_LOG_H */
