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
 * @brief Reset all mock state to initial values
 *
 * @details
 * Clears all call counters, zeroes the last captured config, and resets the
 * preset transfer result to k_rx_ok. Call this in test setUp() to ensure
 * each test starts from a clean state.
 *
 * @pre Called from the test thread; no concurrent calls to any mock function
 * @pre No concurrent mock modification in progress
 * @post All counters (init, deinit, transfer) = 0
 * @post s_transfer_result = k_rx_ok; s_last_config zeroed; s_transfer_called = false
 *
 * @note Not thread-safe; call only from test thread
 * @since Version 1.0.0
 */
void mock_rx_dmaca_reset(void);

/**
 * @brief Configure the return value for the next rx_dmaca_transfer_poll() call
 *
 * @details
 * Sets the preset result returned by all subsequent rx_dmaca_transfer_poll()
 * calls until changed or reset. Use to simulate DMA success, timeout, or error.
 *
 * @param[in] result rx_err_t value to return from rx_dmaca_transfer_poll()
 *
 * @pre Called from the test thread; not thread-safe
 * @pre mock_rx_dmaca_reset() called at test start to initialize state
 * @post s_transfer_result set to result
 * @post All subsequent rx_dmaca_transfer_poll() calls return result until changed
 *
 * @note No allocation or blocking; safe to call from setUp()
 * @since Version 1.0.0
 */
void mock_rx_dmaca_set_transfer_result(rx_err_t result);

/**
 * @brief Get the number of rx_dmaca_init() calls recorded since last reset
 *
 * @details
 * Returns the cumulative count of rx_dmaca_init() invocations since the last
 * mock_rx_dmaca_reset() call. Used to verify the CRC driver initializes DMAC once.
 *
 * @return uint32_t Number of rx_dmaca_init() calls
 *
 * @pre mock_rx_dmaca_reset() called at test start to clear s_init_count
 * @pre No concurrent threads modifying mock state
 * @post Returned value equals s_init_count at time of call
 * @post No mock state modified (pure read)
 *
 * @note Not thread-safe; call only from test thread
 * @since Version 1.0.0
 */
uint32_t mock_rx_dmaca_get_init_count(void);

/**
 * @brief Get the number of rx_dmaca_deinit() calls recorded since last reset
 *
 * @details
 * Returns the cumulative count of rx_dmaca_deinit() invocations since the
 * last mock_rx_dmaca_reset() call. Used to verify teardown symmetry.
 *
 * @return uint32_t Number of rx_dmaca_deinit() calls
 *
 * @pre mock_rx_dmaca_reset() called at test start to clear s_deinit_count
 * @pre No concurrent threads modifying mock state
 * @post Returned value is a snapshot of s_deinit_count at time of call
 * @post No mock state modified (pure read)
 *
 * @note Not thread-safe; call only from test thread
 * @since Version 1.0.0
 */
uint32_t mock_rx_dmaca_get_deinit_count(void);

/**
 * @brief Get the number of rx_dmaca_transfer_poll() calls recorded since last reset
 *
 * @details
 * Returns the cumulative count of rx_dmaca_transfer_poll() invocations since
 * the last mock_rx_dmaca_reset() call. Used to verify DMA path is taken for
 * large buffers and CPU path for small ones.
 *
 * @return uint32_t Number of rx_dmaca_transfer_poll() calls
 *
 * @pre mock_rx_dmaca_reset() called at test start to clear s_transfer_count
 * @pre No concurrent modification of s_transfer_count
 * @post Returned value equals s_transfer_count at time of call
 * @post No side effects; call count remains unchanged by this call
 *
 * @note Not thread-safe; call only from test thread
 * @since Version 1.0.0
 */
uint32_t mock_rx_dmaca_get_transfer_call_count(void);

/**
 * @brief Get the last config passed to rx_dmaca_transfer_poll()
 *
 * @details
 * Returns a pointer to the internally stored copy of the most recent
 * rx_dmaca_config_t passed to rx_dmaca_transfer_poll(). Returns NULL if
 * rx_dmaca_transfer_poll() has not been called since the last reset.
 * Use this to verify the caller constructed the DMA config correctly.
 *
 * @return const rx_dmaca_config_t* Pointer to last config copy, or NULL
 *
 * @pre mock_rx_dmaca_reset() called at test start to initialize s_transfer_called
 * @pre No concurrent modification of s_last_config
 * @post Returned pointer (if non-NULL) is valid until next mock_rx_dmaca_reset()
 * @post No mock state modified (pure read)
 *
 * @note Pointer is valid until the next mock_rx_dmaca_reset() call
 * @note Not thread-safe; call only from test thread
 * @since Version 1.0.0
 */
const rx_dmaca_config_t* mock_rx_dmaca_get_last_config(void);

#ifdef __cplusplus
}
#endif
