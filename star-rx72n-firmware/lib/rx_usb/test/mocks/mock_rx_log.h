/**
 * @file mock_rx_log.h
 * @brief Mock Logging Functions for Host-Side Testing
 *
 * Provides mock implementations of rx_log_* functions that do nothing
 * but can optionally track log messages for test verification.
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_LOG_H
#define MOCK_RX_LOG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/** @brief Maximum log message length to store */
typedef enum {
  k_mock_log_max_msg_len  = 128, /**< Maximum message length */
  k_mock_log_max_tag_len  = 32,  /**< Maximum tag length */
  k_mock_log_history_size = 32,  /**< Maximum number of logged messages */
} mock_log_constants_t;

/* =============================================================================
 * Log Level Enum
 * =============================================================================
 */

typedef enum {
  k_mock_log_level_error = 0, /**< Error level */
  k_mock_log_level_warn  = 1, /**< Warning level */
  k_mock_log_level_info  = 2, /**< Info level */
  k_mock_log_level_debug = 3, /**< Debug level */
} mock_log_level_t;

/* =============================================================================
 * Log Entry Structure
 * =============================================================================
 */

/**
 * @brief Record of a single log message
 */
typedef struct {
  mock_log_level_t level;                       /**< Log level */
  char             tag[k_mock_log_max_tag_len]; /**< Log tag */
  char             msg[k_mock_log_max_msg_len]; /**< Log message */
} mock_log_entry_t;

/* =============================================================================
 * Mock Log State Structure
 * =============================================================================
 */

/**
 * @brief Mock log state
 */
typedef struct {
  mock_log_entry_t entries[k_mock_log_history_size]; /**< Log history */
  uint32_t         write_index;                      /**< Next write index */
  uint32_t         count;                            /**< Total messages logged */
  uint32_t         error_count;                      /**< Number of error logs */
  uint32_t         warn_count;                       /**< Number of warning logs */
  uint32_t         info_count;                       /**< Number of info logs */
  uint32_t         debug_count;                      /**< Number of debug logs */
  bool             print_enabled;                    /**< Print to stdout if true */
} mock_log_state_t;

/* =============================================================================
 * Global Mock State
 * =============================================================================
 */

extern mock_log_state_t g_mock_log;

/* =============================================================================
 * Mock Log Functions (replace real rx_log_* functions)
 * =============================================================================
 */

/**
 * @brief Log an error message
 * @param tag Module tag
 * @param msg Error message
 */
void rx_log_error(const char* tag, const char* msg);

/**
 * @brief Log a warning message
 * @param tag Module tag
 * @param msg Warning message
 */
void rx_log_warn(const char* tag, const char* msg);

/**
 * @brief Log an info message
 * @param tag Module tag
 * @param msg Info message
 */
void rx_log_info(const char* tag, const char* msg);

/**
 * @brief Log a debug message
 * @param tag Module tag
 * @param msg Debug message
 */
void rx_log_debug(const char* tag, const char* msg);

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Initialize/reset mock log state
 */
void mock_log_init(void);

/**
 * @brief Clear all logged messages
 */
void mock_log_clear(void);

/**
 * @brief Enable/disable printing to stdout
 * @param enable true to print, false to suppress
 */
void mock_log_set_print(bool enable);

/**
 * @brief Get count of error logs
 * @return Number of rx_log_error calls
 */
uint32_t mock_log_get_error_count(void);

/**
 * @brief Get count of warning logs
 * @return Number of rx_log_warn calls
 */
uint32_t mock_log_get_warn_count(void);

/**
 * @brief Check if a specific message was logged
 * @param level Log level to search
 * @param tag Tag to match (NULL to match any)
 * @param msg Substring to search for in message (NULL to match any)
 * @return true if matching log entry found
 */
bool mock_log_contains(mock_log_level_t level, const char* tag, const char* msg);

/**
 * @brief Get the last log entry at a specific level
 * @param level Log level to search
 * @param out_entry Output entry (can be NULL to just check existence)
 * @return true if entry found
 */
bool mock_log_get_last(mock_log_level_t level, mock_log_entry_t* out_entry);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RX_LOG_H */
