/* lib/rx_core/inc/rx_error_handler.h */

/**
 * @file rx_error_handler.h
 * @brief Error Handler Concrete Implementation
 * @details
 * Concrete implementation of the rx_error_interface_t.
 * Provides error tracking, retry logic, and exponential backoff.
 *
 * Features:
 * - Global error counter
 * - Per-component error tracking (up to k_error_handler_max_components)
 * - Retry counting with configurable limits
 * - Exponential backoff calculation
 * - Thread-safe operation (ThreadX mutex)
 *
 * Memory usage (static allocation):
 * - ~200 bytes for error_handler_t struct
 * - ~1KB for component tracking array
 * - ~100 bytes for ThreadX mutex
 * Total: ~1.3KB RAM
 *
 * Usage:
 * @code
 *   // 1. Declare handler
 *   static error_handler_t s_error_handler;
 *
 *   // 2. Initialize
 *   rx_err_t err = error_handler_init(&s_error_handler, 3, 100, 5000);
 *   RX_ERROR_CHECK(err);
 *
 *   // 3. Get interface
 *   rx_error_interface_t error_iface;
 *   error_handler_get_interface(&error_iface, &s_error_handler);
 *
 *   // 4. Use through interface
 *   error_iface.report_error(error_iface.ctx, k_rx_fail, "SPI", "Transfer timeout");
 *
 *   // 5. Check retry logic
 *   if (!error_iface.is_retry_limit_reached(error_iface.ctx, "SPI")) {
 *       uint32_t delay_ms = error_iface.get_backoff_delay(error_iface.ctx, "SPI");
 *       tx_thread_sleep(delay_ms / 10);  // Convert ms to ThreadX ticks
 *       // ... retry operation ...
 *   }
 * @endcode
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_ERROR_HANDLER_H
#define STAR_RX72N_ERROR_HANDLER_H

#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_error_interface.h"
#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/**
 * @brief Error handler configuration constants
 */
typedef enum {
  k_error_handler_max_components =
    16, /**< Maximum number of components to track (each gets error counter and retry state) */
  k_error_handler_component_name_max = 32, /**< Maximum component name length */
} error_handler_limits_t;

/* =============================================================================
 * Component Error Tracking
 * =============================================================================
 */

/**
 * @brief Per-component error tracking state
 */
typedef struct {
  char
    name[k_error_handler_component_name_max]; /**< Component name (e.g., "SPI", "GPIO", "UART") */
  uint32_t error_count;                       /**< Error count for this component */
  uint32_t retry_count;                       /**< Retry attempt count */
  bool     in_use;                            /**< Is this slot in use? */
} error_component_state_t;

/* =============================================================================
 * Error Handler Structure
 * =============================================================================
 */

/**
 * @brief Error handler concrete implementation
 *
 * This structure contains all state for error tracking and retry logic.
 * It implements the rx_error_interface_t.
 */
typedef struct {
  TX_MUTEX mutex;             /**< ThreadX mutex for thread-safe operation */
  uint32_t total_error_count; /**< Total error count across all components */
  error_component_state_t
           components[k_error_handler_max_components]; /**< Per-component error tracking */
  uint32_t max_retries;        /**< Maximum retry attempts before giving up */
  uint32_t initial_backoff_ms; /**< Initial backoff delay in milliseconds */
  uint32_t max_backoff_ms;     /**< Maximum backoff delay in milliseconds */
  bool     initialized;        /**< Is the handler initialized? */
} error_handler_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize error handler
 *
 * @param[in,out] handler Handler instance to initialize
 * @param[in] max_retries Maximum retry attempts (0 = no retry limit)
 * @param[in] initial_backoff_ms Initial backoff delay in milliseconds
 * @param[in] max_backoff_ms Maximum backoff delay in milliseconds
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handler is NULL,
 *         k_rx_err_rtos_mutex if mutex creation fails
 */
rx_err_t error_handler_init(error_handler_t* handler,
                            uint32_t         max_retries,
                            uint32_t         initial_backoff_ms,
                            uint32_t         max_backoff_ms);

/**
 * @brief Get interface from concrete handler
 *
 * @param[out] iface Interface to fill
 * @param[in,out] handler Concrete handler instance
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if either parameter is NULL,
 *         k_rx_err_invalid_state if handler not initialized
 */
rx_err_t error_handler_get_interface(rx_error_interface_t* iface, error_handler_t* handler);

/**
 * @brief Deinitialize error handler (cleanup resources)
 *
 * @param[in,out] handler Handler to deinitialize
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handler is NULL
 */
rx_err_t error_handler_deinit(error_handler_t* handler);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_ERROR_HANDLER_H */
