/**
 * @file mock_rx_clock_power_init.h
 * @brief Mock clock and power initialization for system boot testing
 *
 * @details
 * Provides test double for clock/power initialization (rx_clock_power_init) to enable
 * unit testing of system boot sequence without RX72N clock generation circuit (CGC)
 * or low-power modes.
 *
 * Enables testing of: Boot sequence logic, Clock init error handling, Power mode
 * transitions, Initialization retry logic
 *
 * @par Mock Capabilities: Configurable return value, Call tracking
 * @par Usage: tests/test_main.c, tests/test_system_init.c
 * @see rx_clock_power_init.h Real clock/power initialization
 * @par NASA Power of 10: [OK] Static allocation
 * @par SOLID: S - Single responsibility
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Reset mock state to defaults
 *
 * @details
 * Resets call count and return value to k_rx_ok.
 * Call this in setUp() before each test.
 */
void mock_clock_power_init_reset(void);

/**
 * @brief Set the return value for rx_clock_power_init()
 *
 * @param[in] err Error code to return from rx_clock_power_init()
 */
void mock_clock_power_init_set_return(rx_err_t err);

/**
 * @brief Get number of times rx_clock_power_init() was called
 *
 * @return uint32_t Call count
 */
uint32_t mock_clock_power_init_get_call_count(void);

/**
 * @brief Check if rx_clock_power_init() was called
 *
 * @return true if called at least once, false otherwise
 */
bool mock_clock_power_init_was_called(void);

#ifdef __cplusplus
}
#endif
