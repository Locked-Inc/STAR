/* tests/mocks/mock_hardware_init.h */

/**
 * @file mock_hardware_init.h
 * @brief Mock hardware initialization for system startup testing
 *
 * @details
 * Provides test double for hardware initialization function (hardware_init) to
 * enable unit testing of system startup, initialization sequences, and error
 * recovery without actual RX72N hardware peripherals.
 *
 * Enables testing of: Startup sequence logic, Initialization error handling,
 * System state after failed init, Retry/recovery mechanisms, Boot-time diagnostics
 *
 * @par Mock Capabilities: Configurable return value (success/failure), Error
 * injection (init failure scenarios), Call count tracking, State inspection
 *
 * @par Usage: tests/test_main.c, tests/test_system_init.c
 * @see hardware_init.h Real hardware initialization
 * @par NASA Power of 10: ✓ Static allocation
 * @par SOLID: S - Single responsibility (init only)
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
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
void mock_hardware_init_reset(void);

/**
 * @brief Set the return value for hardware_init()
 *
 * @param[in] err Error code to return from hardware_init()
 */
void mock_hardware_init_set_return(rx_err_t err);

/**
 * @brief Get number of times hardware_init() was called
 *
 * @return uint32_t Call count
 */
uint32_t mock_hardware_init_get_call_count(void);

/**
 * @brief Check if hardware_init() was called
 *
 * @return true if called at least once, false otherwise
 */
bool mock_hardware_init_was_called(void);

#ifdef __cplusplus
}
#endif
