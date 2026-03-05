/* star-rx72n-firmware/tests/mocks/mock_time.h */

/**
 * @file mock_time.h
 * @brief Mock time interface for deterministic timing tests without delays
 *
 * @details
 * Provides test double for time interface (rx_time_interface_t) to enable unit
 * testing of timeout logic, periodic tasks, and time-dependent algorithms without
 * actual delays or ThreadX tick timer.
 *
 * **Key Feature:** Simulated time - tests run instantly, no waiting!
 *
 * Enables testing of: Timeout detection, Periodic task timing, Timestamp
 * generation, Time-based state machines, Delay-dependent logic
 *
 * @par Mock Capabilities: Simulated time counter (manual or auto-advance),
 * Auto-advance mode (time advances on sleep_ms calls), Manual advance (set
 * arbitrary time jumps), Zero actual delay (tests run at full speed), Call
 * tracking (sleep_ms, get_ms counts)
 *
 * @par Example:
 * @code
 * mock_time_t mock;
 * mock_time_init(&mock);
 * mock_time_set_auto_advance(&mock, true);  // Time auto-advances
 *
 * rx_time_interface_t iface;
 * mock_time_get_interface(&iface, &mock);
 *
 * iface.sleep_ms(iface.ctx, 50);  // Simulates 50ms (instant!)
 * uint32_t now = iface.get_ms(iface.ctx);  // Returns 50
 * @endcode
 *
 * @par Usage: All tests with time-dependent behavior
 * @see rx_time.h Real time interface (ThreadX tick timer)
 * @par NASA Power of 10: [OK] Static allocation
 * @par SOLID: D - Dependency Inversion, L - Liskov Substitution
 *
 * @author STAR Team
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "rx_time_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Time Structure
 * =============================================================================
 */

/**
 * @brief Mock time state
 */
typedef struct {
  uint32_t current_time_ms;  /**< Simulated current time */
  uint32_t sleep_call_count; /**< Number of times sleep_ms was called */
  uint32_t total_sleep_ms;   /**< Total sleep time requested */
  bool     auto_advance;     /**< Auto-advance time on sleep */
  bool     initialized;      /**< Initialization flag */
} mock_time_t;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock time
 *
 * @param[in] mock Mock time instance (NULL for global)
 *
 * @return k_rx_ok on success
 */
rx_err_t mock_time_init(mock_time_t* mock);

/**
 * @brief Deinitialize mock time
 *
 * @param[in] mock Mock time instance (NULL for global)
 *
 * @return k_rx_ok on success
 */
rx_err_t mock_time_deinit(mock_time_t* mock);

/**
 * @brief Get time interface for mock
 *
 * Populates an rx_time_interface_t structure with function pointers
 * to the mock implementation.
 *
 * @param[out] iface Interface to populate
 * @param[in]  mock  Mock time instance (NULL for global)
 *
 * @return k_rx_ok on success, k_rx_err_null_ptr if iface is NULL
 */
rx_err_t mock_time_get_interface(rx_time_interface_t* iface, mock_time_t* mock);

/* =============================================================================
 * Time Control Functions
 * =============================================================================
 */

/**
 * @brief Manually advance simulated time
 *
 * @param[in] mock Mock time instance (NULL for global)
 * @param[in] ms   Milliseconds to advance
 */
void mock_time_advance(mock_time_t* mock, uint32_t ms);

/**
 * @brief Set simulated time to specific value
 *
 * @param[in] mock Mock time instance (NULL for global)
 * @param[in] ms   Time value to set
 */
void mock_time_set(mock_time_t* mock, uint32_t ms);

/**
 * @brief Enable/disable auto-advance on sleep
 *
 * When enabled, calls to sleep_ms will automatically advance the
 * simulated time by the sleep amount. This is useful for testing
 * code that uses sleep for timing without actual delays.
 *
 * @param[in] mock   Mock time instance (NULL for global)
 * @param[in] enable True to enable auto-advance, false to disable
 */
void mock_time_set_auto_advance(mock_time_t* mock, bool enable);

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

/**
 * @brief Get number of times sleep_ms was called
 *
 * @param[in] mock Mock time instance (NULL for global)
 *
 * @return Number of sleep_ms calls
 */
uint32_t mock_time_get_sleep_count(mock_time_t* mock);

/**
 * @brief Get total sleep time requested
 *
 * @param[in] mock Mock time instance (NULL for global)
 *
 * @return Total milliseconds requested via sleep_ms
 */
uint32_t mock_time_get_total_sleep(mock_time_t* mock);

/**
 * @brief Reset all counters
 *
 * Resets sleep_call_count and total_sleep_ms but preserves current_time_ms
 *
 * @param[in] mock Mock time instance (NULL for global)
 */
void mock_time_reset_counters(mock_time_t* mock);

#ifdef __cplusplus
}
#endif
