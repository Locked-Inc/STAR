/**
 * @file mock_rx_bno055.h
 * @brief Mock BNO055 9-DOF IMU sensor driver for IMU task unit tests
 *
 * @details
 * Provides test double for BNO055 9-DOF absolute orientation sensor.
 * Enables testing of IMU task initialization and interrupt-driven data
 * handling without actual BNO055 hardware.
 *
 * @author STAR Team
 * @date 2026-03-08
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Use the canonical production types to avoid struct divergence */
#include "rx_bno055.h"

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Reset all mock state to defaults (k_rx_ok returns, zero call counts)
 *
 * @details
 * Resets s_init_return and s_read_return to k_rx_ok, clears s_init_count
 * and s_read_count to 0, and resets s_last_mode to k_bno055_mode_poll.
 * Call from setUp() before each test.
 *
 * @pre None
 * @pre Mock module is linked into the test executable
 * @post s_init_return == k_rx_ok
 * @post s_read_return == k_rx_ok
 * @post s_init_count == 0 and s_read_count == 0
 * @post s_last_mode == k_bno055_mode_poll
 */
void mock_bno055_reset(void);

/**
 * @brief Set the return value that rx_bno055_init() will return
 *
 * @param[in] err Return value to inject (e.g. k_rx_ok or k_rx_err_nack)
 *
 * @pre err is a valid rx_err_t value (within the rx_err_t enum range)
 * @pre mock_bno055_reset() establishes the default (k_rx_ok) before overriding
 * @post Next call to rx_bno055_init() with non-NULL args returns err
 * @post No other mock state (s_init_count, s_read_return) is modified
 */
void mock_bno055_set_init_return(rx_err_t err);

/**
 * @brief Set the return value that rx_bno055_read() will return
 *
 * @param[in] err Return value to inject (e.g. k_rx_ok or k_rx_err_timeout)
 *
 * @pre err is a valid rx_err_t value (within the rx_err_t enum range)
 * @pre mock_bno055_reset() establishes the default (k_rx_ok) before overriding
 * @post Next call to rx_bno055_read() with non-NULL out returns err
 * @post When err != k_rx_ok, *out is NOT zeroed even if out is non-NULL
 */
void mock_bno055_set_read_return(rx_err_t err);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

/**
 * @brief Return the number of times rx_bno055_init() has been called
 *
 * @return uint32_t Call count since last mock_bno055_reset()
 *
 * @pre mock_bno055_reset() called at least once to establish a zero baseline
 * @pre s_init_count reflects only calls where both manager and config were non-NULL
 * @post Return value equals the number of rx_bno055_init() calls with non-NULL args since reset
 * @post Return value is >= 0 and non-decreasing since the last mock_bno055_reset()
 */
uint32_t mock_bno055_get_init_count(void);

/**
 * @brief Return the number of times rx_bno055_read() has been called
 *
 * @return uint32_t Call count since last mock_bno055_reset()
 *
 * @pre mock_bno055_reset() called at least once to establish a zero baseline
 * @pre s_read_count reflects only calls where out != NULL (NULL returns early)
 * @post Return value equals the number of rx_bno055_read() calls with out != NULL since reset
 * @post Return value is non-decreasing since the last mock_bno055_reset()
 */
uint32_t mock_bno055_get_read_count(void);

/**
 * @brief Return the bno055_mode_t value last passed to rx_bno055_init()
 *
 * @details
 * Returns the mode field captured from the config pointer on the most recent
 * call to rx_bno055_init(). Returns k_bno055_mode_poll if rx_bno055_init()
 * has not been called since the last mock_bno055_reset().
 *
 * @return bno055_mode_t Mode passed to the most recent rx_bno055_init() call
 *
 * @pre mock_bno055_reset() was called to set the default (k_bno055_mode_poll)
 * @pre If rx_bno055_init() has not been called since reset, returns k_bno055_mode_poll
 * @post Return value == k_bno055_mode_poll if rx_bno055_init() not called since reset
 * @post Return value == config->mode from the last rx_bno055_init() call otherwise
 */
bno055_mode_t mock_bno055_get_last_mode(void);

/* =============================================================================
 * Mock BNO055 API (matches rx_bno055.h signatures)
 * =============================================================================
 */

/**
 * @brief Mock implementation of rx_bno055_init()
 *
 * @details
 * Increments the init call counter and returns the value set by
 * mock_bno055_set_init_return(). Does not access hardware.
 *
 * @param[in] manager Bus manager pointer (validated; returns k_rx_err_null_ptr if NULL)
 * @param[in] config  Config pointer (validated; returns k_rx_err_null_ptr if NULL)
 *
 * @return rx_err_t Value set by mock_bno055_set_init_return() (default k_rx_ok)
 * @retval k_rx_err_null_ptr Either manager or config is NULL (matches real driver behavior)
 *
 * @pre manager non-NULL (mock validates like the real driver)
 * @pre config non-NULL (mock validates like the real driver)
 * @post s_init_count incremented by 1 (only when both args are non-NULL)
 * @post s_last_mode == config->mode (only when both args are non-NULL)
 */
rx_err_t rx_bno055_init(rx_bus_manager_t* manager, const bno055_config_t* config);

/**
 * @brief Mock implementation of rx_bno055_read()
 *
 * @details
 * Returns k_rx_err_null_ptr if out is NULL. Otherwise zeroes *out, increments
 * the read call counter, and returns the value set by mock_bno055_set_read_return().
 *
 * @param[out] out Destination for sensor data (zeroed on success)
 *
 * @return rx_err_t Value set by mock_bno055_set_read_return() (default k_rx_ok)
 * @retval k_rx_err_null_ptr out is NULL
 *
 * @pre out may be NULL (returns k_rx_err_null_ptr)
 * @pre mock_bno055_reset() called at least once to establish s_read_return baseline
 * @post *out zeroed when out != NULL and return value is k_rx_ok
 * @post s_read_count incremented when out != NULL
 */
rx_err_t rx_bno055_read(bno055_data_t* out);

#ifdef __cplusplus
}
#endif
