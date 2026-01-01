/* lib/rx_core/inc/rx_time_interface.h */

/**
 * @file rx_time_interface.h
 * @brief Time Interface (Dependency Inversion Principle)
 * @details
 * Defines the abstract interface for time operations in RX72N firmware.
 * Follows rx_error_interface_t pattern for architectural consistency.
 *
 * This is a pure interface with no implementation details.
 * Concrete implementations (ThreadX, mock) provide the actual timing logic.
 *
 * Benefits of this pattern:
 * - Testability: Mock implementations for unit tests
 * - Flexibility: Swap implementations without changing dependent code
 * - Decoupling: Communication layers depend on interface, not ThreadX
 *
 * Usage:
 * @code
 *   // 1. Create concrete implementation (ThreadX or mock)
 *   rx_time_interface_t time_iface;
 *   rx_time_threadx_get_interface(&time_iface);  // or mock_time_get_interface()
 *
 *   // 2. Inject into dependent component
 *   rx_usb_comm_config_t config = { .time_iface = &time_iface };
 *
 *   // 3. Use through interface
 *   time_iface.sleep_ms(time_iface.ctx, 10);
 * @endcode
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_TIME_INTERFACE_H
#define STAR_RX72N_TIME_INTERFACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of interface struct */
typedef struct rx_time_interface rx_time_interface_t;

/* =============================================================================
 * Time Interface Function Pointers
 * =============================================================================
 */

/**
 * @brief Sleep for a specified number of milliseconds
 *
 * On ThreadX: Yields CPU to other threads via tx_thread_sleep()
 * On Mock: Advances simulated time (if auto_advance enabled)
 *
 * @param[in] ctx Implementation context (opaque pointer)
 * @param[in] ms Number of milliseconds to sleep
 */
typedef void (*rx_time_sleep_ms_fn)(void* ctx, uint32_t ms);

/**
 * @brief Get current time in milliseconds (monotonic)
 *
 * Returns a monotonically increasing time value suitable for
 * elapsed time calculations. Does not represent wall clock time.
 *
 * @param[in] ctx Implementation context
 *
 * @return Current time in milliseconds
 */
typedef uint32_t (*rx_time_get_ms_fn)(void* ctx);

/**
 * @brief Check if timeout has elapsed
 *
 * Convenience function to check if (current_time - start_ms) >= timeout_ms.
 * Handles wrap-around correctly for 32-bit time values.
 *
 * @param[in] ctx Implementation context
 * @param[in] start_ms Start time from get_ms()
 * @param[in] timeout_ms Timeout duration in milliseconds
 *
 * @return true if timeout has elapsed, false otherwise
 */
typedef bool (*rx_time_is_elapsed_fn)(void* ctx, uint32_t start_ms, uint32_t timeout_ms);

/* =============================================================================
 * Time Interface Structure
 * =============================================================================
 */

/**
 * @brief Time interface
 *
 * This structure defines the contract that all time implementations must fulfill.
 * Dependent components use this interface without knowing the concrete
 * implementation details (ThreadX vs mock).
 *
 * The ctx field holds a pointer to the concrete implementation's private data.
 * All function pointers receive this context as their first parameter.
 */
struct rx_time_interface {
  void*                 ctx;        /**< Implementation context (opaque pointer to concrete handler) */
  rx_time_sleep_ms_fn   sleep_ms;   /**< Sleep for specified milliseconds */
  rx_time_get_ms_fn     get_ms;     /**< Get current time in milliseconds */
  rx_time_is_elapsed_fn is_elapsed; /**< Check if timeout has elapsed */
};

/* =============================================================================
 * Interface Validation
 * =============================================================================
 */

/**
 * @brief Validate that a time interface is properly initialized
 *
 * @param[in] iface Interface to validate
 *
 * @return k_rx_ok if valid, k_rx_err_null_pointer if NULL,
 *         k_rx_err_invalid_state if missing required function pointers
 */
static inline rx_err_t rx_time_interface_validate(const rx_time_interface_t* iface)
{
  if (iface == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Check required function pointers */
  if (iface->sleep_ms == NULL || iface->get_ms == NULL) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_TIME_INTERFACE_H */
