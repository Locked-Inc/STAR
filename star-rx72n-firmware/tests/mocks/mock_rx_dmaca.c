/**
 * @file mock_rx_dmaca.c
 * @brief Mock DMAC Driver Implementation for Unit Testing
 *
 * @details
 * Provides mock implementations of rx_dmaca_init(), rx_dmaca_transfer_poll(),
 * rx_dmaca_abort(), and rx_dmaca_deinit() for host-side unit testing of the
 * rx_crc hw_dma backend without RX72N hardware.
 *
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_dmaca.h"

#include <stddef.h>
#include <string.h>

/* =============================================================================
 * Mock State
 * =============================================================================
 */

/** @brief Number of rx_dmaca_init() calls since last reset */
static uint32_t s_init_count;

/** @brief Number of rx_dmaca_deinit() calls since last reset */
static uint32_t s_deinit_count;

/** @brief Number of rx_dmaca_transfer_poll() calls since last reset */
static uint32_t s_transfer_count;

/** @brief Preset return value for rx_dmaca_transfer_poll() */
static rx_err_t s_transfer_result;

/** @brief Copy of the last config passed to rx_dmaca_transfer_poll() */
static rx_dmaca_config_t s_last_config;

/** @brief Whether transfer_poll has been called at least once */
static bool s_transfer_called;

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
 * @pre None
 * @post s_init_count = 0
 * @post s_deinit_count = 0
 * @post s_transfer_count = 0
 * @post s_transfer_result = k_rx_ok
 * @post s_last_config zeroed
 * @post s_transfer_called = false
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
void mock_rx_dmaca_reset(void)
{
  s_init_count      = 0;
  s_deinit_count    = 0;
  s_transfer_count  = 0;
  s_transfer_result = k_rx_ok;
  memset(&s_last_config, 0, sizeof(s_last_config));
  s_transfer_called = false;
}

/**
 * @brief Configure the return value for the next rx_dmaca_transfer_poll() call
 *
 * @details
 * Sets the preset result that will be returned by the mock
 * rx_dmaca_transfer_poll() implementation. Use this to simulate DMA success,
 * timeout, or other error conditions during testing.
 *
 * @param[in] result rx_err_t value to return from rx_dmaca_transfer_poll()
 *
 * @pre None
 * @post Subsequent rx_dmaca_transfer_poll() calls return result
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
void mock_rx_dmaca_set_transfer_result(rx_err_t result)
{
  s_transfer_result = result;
}

/**
 * @brief Get the number of rx_dmaca_init() calls recorded since last reset
 *
 * @details
 * Returns the cumulative count of rx_dmaca_init() invocations since the last
 * mock_rx_dmaca_reset() call. Used to verify that the CRC driver initializes
 * the DMA peripheral exactly once.
 *
 * @return uint32_t Number of rx_dmaca_init() calls
 *
 * @pre mock_rx_dmaca_reset() should have been called at test start
 * @post Return value is in [0, UINT32_MAX]
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
uint32_t mock_rx_dmaca_get_init_count(void)
{
  return s_init_count;
}

/**
 * @brief Get the number of rx_dmaca_deinit() calls recorded since last reset
 *
 * @details
 * Returns the cumulative count of rx_dmaca_deinit() invocations since the
 * last mock_rx_dmaca_reset() call. Used to verify teardown symmetry.
 *
 * @return uint32_t Number of rx_dmaca_deinit() calls
 *
 * @pre mock_rx_dmaca_reset() should have been called at test start
 * @post Return value is in [0, UINT32_MAX]
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
uint32_t mock_rx_dmaca_get_deinit_count(void)
{
  return s_deinit_count;
}

/**
 * @brief Get the number of rx_dmaca_transfer_poll() calls recorded since last reset
 *
 * @details
 * Returns the cumulative count of rx_dmaca_transfer_poll() invocations since
 * the last mock_rx_dmaca_reset() call. Used to verify the DMA path is taken
 * for large buffers and the CPU path for small ones.
 *
 * @return uint32_t Number of rx_dmaca_transfer_poll() calls
 *
 * @pre mock_rx_dmaca_reset() should have been called at test start
 * @post Return value is in [0, UINT32_MAX]
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
uint32_t mock_rx_dmaca_get_transfer_call_count(void)
{
  return s_transfer_count;
}

/**
 * @brief Get the last config passed to rx_dmaca_transfer_poll()
 *
 * @details
 * Returns a pointer to the internally stored copy of the most recent
 * rx_dmaca_config_t passed to rx_dmaca_transfer_poll(). Use this to verify
 * that the caller constructed the DMA config correctly (channel, src, len,
 * dst_addr, timeout_cycles).
 *
 * Returns NULL if rx_dmaca_transfer_poll() has not been called since the last
 * mock_rx_dmaca_reset().
 *
 * @return const rx_dmaca_config_t* Pointer to last config copy, or NULL
 *
 * @pre None
 * @post Returned pointer (if non-NULL) is valid until next mock_rx_dmaca_reset()
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
const rx_dmaca_config_t* mock_rx_dmaca_get_last_config(void)
{
  if (!s_transfer_called) {
    return nullptr;
  }
  return &s_last_config;
}

/* =============================================================================
 * Mock Driver Functions (replace real rx_dmaca_* in test builds)
 * =============================================================================
 */

/**
 * @brief Mock implementation of rx_dmaca_init()
 *
 * @details
 * Increments the init call counter and returns k_rx_ok. Does not touch any
 * hardware registers. Used by rx_crc tests to verify the hw_dma backend
 * calls rx_dmaca_init() during initialization.
 *
 * @return rx_err_t
 * @retval k_rx_ok Always
 *
 * @pre None
 * @post s_init_count incremented by 1
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_init(void)
{
  s_init_count++;
  return k_rx_ok;
}

/**
 * @brief Mock implementation of rx_dmaca_deinit()
 *
 * @details
 * Increments the deinit call counter and returns k_rx_ok. Does not touch any
 * hardware registers. Used by rx_crc tests to verify teardown symmetry.
 *
 * @return rx_err_t
 * @retval k_rx_ok Always
 *
 * @pre None
 * @post s_deinit_count incremented by 1
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_deinit(void)
{
  s_deinit_count++;
  return k_rx_ok;
}

/**
 * @brief Mock implementation of rx_dmaca_transfer_poll()
 *
 * @details
 * Increments the transfer call counter, saves a copy of config for later
 * inspection, and returns the preset result configured via
 * mock_rx_dmaca_set_transfer_result(). Does not perform any actual DMA
 * transfer or access hardware registers.
 *
 * @param[in] config Transfer configuration (may be NULL; NULL is recorded but not dereferenced)
 *
 * @return rx_err_t
 * @retval k_rx_ok Default (after mock_rx_dmaca_reset())
 * @retval other   Whatever was set by mock_rx_dmaca_set_transfer_result()
 *
 * @pre None
 * @post s_transfer_count incremented by 1
 * @post s_transfer_called = true
 * @post s_last_config updated if config != NULL
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_transfer_poll(const rx_dmaca_config_t* config)
{
  s_transfer_count++;
  s_transfer_called = true;
  if (config != nullptr) {
    s_last_config = *config;
  }
  return s_transfer_result;
}

/**
 * @brief Mock implementation of rx_dmaca_abort()
 *
 * @details
 * No-op stub. The channel parameter is ignored. Returns k_rx_ok immediately
 * without modifying any state. Provided for link compatibility with code that
 * calls rx_dmaca_abort() in error-recovery paths.
 *
 * @param[in] channel Channel to abort (ignored by mock)
 *
 * @return rx_err_t
 * @retval k_rx_ok Always
 *
 * @pre None
 * @post No mock state modified
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_abort(uint8_t channel)
{
  (void)channel;
  return k_rx_ok;
}
