/**
 * @file mock_rx_dmaca.h
 * @brief Mock DMAC Driver for CRC Unit Testing
 *
 * @details
 * Test double for the rx_dmaca polling-mode DMA driver. Used by rx_crc tests
 * to intercept DMA calls and control transfer outcomes without hardware.
 *
 * Records call counts, captures the last config passed to transfer_poll, and
 * returns a preset result. Callers reset state with mock_rx_dmaca_reset() in
 * test setUp and configure behaviour with mock_rx_dmaca_set_transfer_result().
 *
 * @par Usage: tests/test_rx_crc*.c (hw_dma backend tests)
 *
 * @see rx_dmaca.h Real DMAC driver API
 * @see mock_rx_dmaca.c Mock implementation
 *
 * @author Locked, Inc.
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "rx_dmaca.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Reset all mock state to defaults
 *
 * @details
 * Clears all call counts, resets last_config to zero, and sets the preset
 * transfer result back to k_rx_ok. Call this in test setUp.
 *
 * @post All counters = 0
 * @post transfer_result = k_rx_ok
 * @post last_config zeroed
 *
 * @since Version 1.0.0
 */
void mock_rx_dmaca_reset(void);

/**
 * @brief Set the result returned by the next rx_dmaca_transfer_poll() call
 *
 * @param[in] result Value to return from rx_dmaca_transfer_poll()
 *
 * @since Version 1.0.0
 */
void mock_rx_dmaca_set_transfer_result(rx_err_t result);

/**
 * @brief Get the number of rx_dmaca_init() calls since last reset
 *
 * @return Call count
 * @since Version 1.0.0
 */
uint32_t mock_rx_dmaca_get_init_count(void);

/**
 * @brief Get the number of rx_dmaca_deinit() calls since last reset
 *
 * @return Call count
 * @since Version 1.0.0
 */
uint32_t mock_rx_dmaca_get_deinit_count(void);

/**
 * @brief Get the number of rx_dmaca_transfer_poll() calls since last reset
 *
 * @return Call count
 * @since Version 1.0.0
 */
uint32_t mock_rx_dmaca_get_transfer_call_count(void);

/**
 * @brief Get the last config passed to rx_dmaca_transfer_poll()
 *
 * @return Pointer to internal copy of last config, or NULL if never called
 *
 * @note Pointer is valid until the next mock_rx_dmaca_reset() call
 * @since Version 1.0.0
 */
const rx_dmaca_config_t* mock_rx_dmaca_get_last_config(void);

#ifdef __cplusplus
}
#endif
