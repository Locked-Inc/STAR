/**
 * @file mock_error_handler.h
 * @brief Mock error handler for testing error reporting and recovery logic
 *
 * @details
 * Provides test double for error handler interface (rx_error_interface_t) to
 * enable unit testing of error reporting, recovery, and diagnostic logging
 * without actual error handler or ThreadX dependencies.
 *
 * **Dependency Inversion Principle (SOLID-D) Example:**
 * - Same interface as real error_handler_t
 * - Different implementation (records errors in RAM vs. logs to UART)
 * - Drop-in replacement for any code using rx_error_interface_t
 *
 * Enables testing of: Error reporting workflows, Error recovery logic, Fatal
 * error handling, Component error isolation, Diagnostic message generation
 *
 * @par Mock Capabilities: Error recording buffer (32 errors max), Error code +
 * component + message storage, No ThreadX dependencies (no mutex needed),
 * Query functions for test verification
 *
 * @par Memory Usage: ~2.6KB RAM (static allocation: 32 errors x 80 bytes + state)
 *
 * @par Usage: All test files requiring error handling
 * @see rx_error_handler.h Real error handler interface
 * @par NASA Power of 10: [OK] Static allocation (circular buffer), bounded capacity
 * @par SOLID: D - Dependency Inversion, L - Liskov Substitution
 *
 * @author Locked, Inc.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */
*Usage Example(Swapping Implementations)
    : *@code * // Production code uses real error handler:
  *error_handler_t real_handler;
*const error_handler_config_t config = {*.max_retries = k_error_handler_default_max_retries,
                                        *.initial_backoff_ms =
                                          k_error_handler_default_initial_backoff_ms,
                                        *.max_backoff_ms = k_error_handler_default_max_backoff_ms,
                                        *};
*error_handler_init(&real_handler, &config);
*rx_error_interface_t error_iface;
*error_handler_get_interface(&error_iface, &real_handler);
** // Test code uses mock handler (same interface!):
 *mock_error_handler_t mock_handler;
*mock_error_handler_init(&mock_handler, 32);
*rx_error_interface_t test_iface;
*mock_error_handler_get_interface(&test_iface, &mock_handler);
**                                          // Both can be used identically by dependent code:
 *bus_manager_init(&bus_mgr, &error_iface); // or &test_iface
*@endcode** Usage Example(Verifying Errors in Tests)
    : *@code * // 1. Initialize mock
  *mock_error_handler_t mock;
*mock_error_handler_init(&mock, 32);
*rx_error_interface_t iface;
*mock_error_handler_get_interface(&iface, &mock);
** // 2. Pass to code under test
 *bus_manager_init(&bus_mgr, &iface, nullptr);
**                                     // 3. Trigger error condition
 *bus_manager_transfer(&bus_mgr, ...); // Causes error
**                                     // 4. Verify error was reported
 *rx_err_t   last_err;
*const char* component;
*const char* message;
*mock_error_handler_get_last_error(&mock, &last_err, &component, &message);
*assert(last_err == k_rx_err_timeout);
*assert(strcmp(component, "SPI") == 0);
** // 5. Check error count
 *uint32_t count = iface.get_error_count(iface.ctx);
*assert(count == 1);
*@endcode** @date 2026 - 01 -
  01 * @copyright Copyright(c) 2026 STAR Project* /

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_error_interface.h"
#include "rx_gpio_constants.h"

#ifdef __cplusplus
    extern "C"
{
#endif

  /* =============================================================================
 * Configuration
 * =============================================================================
 */

  /**
 * @brief Mock error handler configuration constants
 */
  typedef enum : uint8_t {
    k_mock_error_handler_default_max_errors = 32,  /**< Default maximum errors to record */
    k_mock_error_handler_component_name_max = 32,  /**< Maximum component name length */
    k_mock_error_handler_message_max        = 128, /**< Maximum error message length */
  } mock_error_handler_limits_t;

  /* =============================================================================
 * Error Record Structure
 * =============================================================================
 */

  /**
 * @brief Single error record
 */
  typedef struct {
    rx_err_t error_code;                                         /**< Error code */
    char     component[k_mock_error_handler_component_name_max]; /**< Component name */
    char     message[k_mock_error_handler_message_max];          /**< Error message */
  } mock_error_record_t;

  /* =============================================================================
 * Mock Error Handler Structure
 * =============================================================================
 */

  /**
 * @brief Mock error handler implementation
 *
 * Records errors in memory for testing. Implements rx_error_interface_t.
 */
  typedef struct {
    mock_error_record_t
             errors[k_mock_error_handler_default_max_errors]; /**< Array of recorded errors */
    uint32_t total_error_count;  /**< Total number of errors recorded (may exceed max_errors) */
    uint32_t stored_error_count; /**< Number of errors currently stored (capped at max_errors) */
    uint32_t max_errors;         /**< Maximum number of errors to store */
    uint32_t write_index;        /**< Next index to write (circular buffer) */
    bool     initialized;        /**< Is the handler initialized? */
  } mock_error_handler_t;

  /* =============================================================================
 * Public API
 * =============================================================================
 */

  /**
 * @brief Initialize mock error handler
 *
 * @param[in,out] handler Handler instance to initialize
 * @param[in] max_errors Maximum number of errors to record (1-32)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if handler is NULL,
 *         k_rx_err_invalid_arg if max_errors is 0 or > 32
 */
  rx_err_t mock_error_handler_init(mock_error_handler_t * handler, uint32_t max_errors);

  /**
 * @brief Get interface from mock handler
 *
 * @param[out] iface Interface to fill
 * @param[in,out] handler Mock handler instance
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if either parameter is NULL,
 *         k_rx_err_invalid_state if handler not initialized
 */
  rx_err_t mock_error_handler_get_interface(rx_error_interface_t * iface,
                                            mock_error_handler_t * handler);

  /* =============================================================================
 * Testing Helper Functions
 * =============================================================================
 */

  /**
 * @brief Get the last error recorded
 *
 * @param[in] handler Handler instance
 * @param[out] out_error Error code (can be NULL)
 * @param[out] out_component Component name pointer (can be NULL)
 * @param[out] out_message Error message pointer (can be NULL)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if handler is NULL,
 *         k_rx_err_not_found if no errors recorded
 *
 * @note out_component and out_message point to internal buffers.
 *       Do not modify or free them. Copy if needed.
 */
  rx_err_t mock_error_handler_get_last_error(mock_error_handler_t * handler,
                                             rx_err_t * out_error,
                                             const char** out_component,
                                             const char** out_message);

  /**
 * @brief Get error at specific index (0 = oldest, count-1 = newest)
 *
 * @param[in] handler Handler instance
 * @param[in] index Error index (0 to stored_error_count-1)
 * @param[out] out_error Error code (can be NULL)
 * @param[out] out_component Component name pointer (can be NULL)
 * @param[out] out_message Error message pointer (can be NULL)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if handler is NULL,
 *         k_rx_err_invalid_arg if index out of range
 */
  rx_err_t mock_error_handler_get_error_at(mock_error_handler_t * handler,
                                           uint32_t     index,
                                           rx_err_t*    out_error,
                                           const char** out_component,
                                           const char** out_message);

  /**
 * @brief Check if a specific error was recorded
 *
 * @param[in] handler Handler instance
 * @param[in] error_code Error code to search for
 * @param[in] component Component name to match (NULL = any component)
 *
 * @return true if error was recorded, false otherwise
 */
  bool mock_error_handler_has_error(mock_error_handler_t * handler,
                                    rx_err_t    error_code,
                                    const char* component);

  /**
 * @brief Clear all recorded errors
 *
 * Resets error count and clears all stored errors.
 *
 * @param[in,out] handler Handler to clear
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if handler is NULL
 */
  rx_err_t mock_error_handler_clear(mock_error_handler_t * handler);

  /**
 * @brief Get number of stored errors
 *
 * @param[in] handler Handler instance
 *
 * @return Number of errors currently stored (may be less than total if circular buffer wrapped)
 */
  uint32_t mock_error_handler_get_stored_count(mock_error_handler_t * handler);

  /**
 * @brief Deinitialize mock error handler (cleanup resources)
 *
 * @param[in,out] handler Handler to deinitialize
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if handler is NULL
 */
  rx_err_t mock_error_handler_deinit(mock_error_handler_t * handler);

#ifdef __cplusplus
}
#endif
