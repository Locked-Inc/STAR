/* lib/rx_core/inc/mock_time.h */

/**
 * @file mock_time.h
 * @brief Mock Time Implementation for Testing
 * @details
 * Mock implementation of rx_time_interface_t for testing.
 * Allows tests to control time progression without real delays.
 *
 * Features:
 * - Simulated time that advances when sleep_ms is called (if auto_advance enabled)
 * - Manual time advancement for testing timeouts
 * - Call tracking for verification
 *
 * Usage:
 * @code
 *   mock_time_t mock;
 *   mock_time_init(&mock);
 *   mock_time_set_auto_advance(&mock, true);  // Auto-advance on sleep
 *
 *   rx_time_interface_t iface;
 *   mock_time_get_interface(&iface, &mock);
 *
 *   iface.sleep_ms(iface.ctx, 50);  // Simulates 50ms passing
 *   uint32_t now = iface.get_ms(iface.ctx);  // Returns 50
 * @endcode
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_MOCK_TIME_H
#define STAR_RX72N_MOCK_TIME_H

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
 * @return RX_OK on success
 */
rx_err_t mock_time_init(mock_time_t* mock);

/**
 * @brief Deinitialize mock time
 *
 * @param[in] mock Mock time instance (NULL for global)
 *
 * @return RX_OK on success
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
 * @return RX_OK on success, RX_ERR_NULL_POINTER if iface is NULL
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

#endif /* STAR_RX72N_MOCK_TIME_H */
