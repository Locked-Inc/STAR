/* lib/rx_core/inc/rx_error_interface.h */

/**
 * @file rx_error_interface.h
 * @brief Error Handler Interface (Dependency Inversion Principle)
 * @details
 * Defines the abstract interface for error handling in RX72N firmware.
 * Follows ESP32 star_error_interface_t pattern for architectural consistency.
 *
 * This is a pure interface with no implementation details.
 * Concrete implementations (like rx_error_handler.c) will provide the actual
 * error handling logic while conforming to this interface.
 *
 * Benefits of this pattern:
 * - Testability: Mock implementations for unit tests
 * - Flexibility: Swap implementations without changing dependent code
 * - Decoupling: Bus manager depends on interface, not concrete handler
 *
 * Usage:
 * @code
 *   // 1. Create concrete implementation
 *   error_handler_t handler;
 *   const error_handler_config_t config = {
 *     .max_retries        = k_error_handler_default_max_retries,
 *     .initial_backoff_ms = k_error_handler_default_initial_backoff_ms,
 *     .max_backoff_ms     = k_error_handler_default_max_backoff_ms,
 *   };
 *   error_handler_init(&handler, &config);
 *
 *   // 2. Get interface
 *   rx_error_interface_t iface;
 *   error_handler_get_interface(&iface, &handler);
 *
 *   // 3. Inject into dependent component
 *   bus_manager_init(&bus_mgr, &iface);
 *
 *   // 4. Use through interface
 *   iface.report_error(iface.ctx, k_rx_fail, "BUS", "Transfer failed");
 * @endcode
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_ERROR_INTERFACE_H
#define STAR_RX72N_ERROR_INTERFACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of interface struct */
typedef struct rx_error_interface rx_error_interface_t;

/* =============================================================================
 * Error Handler Interface Function Pointers
 * =============================================================================
 */

/**
 * @brief Report an error through the error handler
 *
 * @param[in] ctx Implementation context (opaque pointer)
 * @param[in] err Error code
 * @param[in] component Component name (e.g., "SPI", "GPIO")
 * @param[in] message Error message
 *
 * @return k_rx_ok on success, error code on failure
 */
typedef rx_err_t (*rx_error_report_fn)(void*       ctx,
                                       rx_err_t    err,
                                       const char* component,
                                       const char* message);

/**
 * @brief Get the total number of errors reported
 *
 * @param[in] ctx Implementation context
 *
 * @return Total error count
 */
typedef uint32_t (*rx_error_get_count_fn)(void* ctx);

/**
 * @brief Get the number of errors for a specific component
 *
 * @param[in] ctx Implementation context
 * @param[in] component Component name
 *
 * @return Error count for the component
 */
typedef uint32_t (*rx_error_get_component_count_fn)(void* ctx, const char* component);

/**
 * @brief Clear all error counters
 *
 * @param[in] ctx Implementation context
 *
 * @return k_rx_ok on success, error code on failure
 */
typedef rx_err_t (*rx_error_clear_fn)(void* ctx);

/**
 * @brief Check if retry limit has been reached
 *
 * @param[in] ctx Implementation context
 * @param[in] component Component name
 *
 * @return true if retry limit reached, false otherwise
 */
typedef bool (*rx_error_is_retry_limit_reached_fn)(void* ctx, const char* component);

/**
 * @brief Reset retry counter for a component
 *
 * @param[in] ctx Implementation context
 * @param[in] component Component name
 *
 * @return k_rx_ok on success, error code on failure
 */
typedef rx_err_t (*rx_error_reset_retry_fn)(void* ctx, const char* component);

/**
 * @brief Get recommended backoff delay (exponential backoff)
 *
 * @param[in] ctx Implementation context
 * @param[in] component Component name
 *
 * @return Delay in milliseconds (0 if no backoff needed)
 */
typedef uint32_t (*rx_error_get_backoff_delay_fn)(void* ctx, const char* component);

/* =============================================================================
 * Error Handler Interface Structure
 * =============================================================================
 */

/**
 * @brief Error handler interface
 *
 * This structure defines the contract that all error handler implementations
 * must fulfill. Dependent components use this interface without knowing the
 * concrete implementation details.
 *
 * The ctx field holds a pointer to the concrete implementation's private data.
 * All function pointers receive this context as their first parameter.
 */
struct rx_error_interface {
  void*                 ctx; /**< Implementation context (opaque pointer to concrete handler) */
  rx_error_report_fn    report_error;    /**< Report an error */
  rx_error_get_count_fn get_error_count; /**< Get total error count */
  rx_error_get_component_count_fn
                    get_component_error_count; /**< Get component-specific error count */
  rx_error_clear_fn clear_errors;              /**< Clear all error counters */
  rx_error_is_retry_limit_reached_fn is_retry_limit_reached; /**< Check if retry limit reached */
  rx_error_reset_retry_fn            reset_retry_counter;    /**< Reset retry counter */
  rx_error_get_backoff_delay_fn      get_backoff_delay;      /**< Get backoff delay */
};

/* =============================================================================
 * Interface Validation
 * =============================================================================
 */

/**
 * @brief Validate that an error interface is properly initialized
 *
 * @param[in] iface Interface to validate
 *
 * @return k_rx_ok if valid, k_rx_err_null_ptr if NULL,
 *         k_rx_err_invalid_state if missing function pointers
 */
static inline rx_err_t rx_error_interface_validate(const rx_error_interface_t* iface)
{
  if (iface == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Check required function pointers */
  if (iface->report_error == NULL || iface->get_error_count == NULL ||
      iface->clear_errors == NULL) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_ERROR_INTERFACE_H */
