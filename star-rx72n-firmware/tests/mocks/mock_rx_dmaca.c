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

#include "mock_rx72n_dmac_regs.h"

/* =============================================================================
 * Private Constants (mirror rx_dmaca.c internal limits for validation parity)
 * =============================================================================
 */

/**
 * @brief Transfer-length and timeout limits (must match rx_dmaca.c values)
 *
 * @details
 * These constants duplicate the private constants in rx_dmaca.c so that the
 * mock can enforce the same input validation as the real driver without
 * exposing those constants in the public header.
 */
typedef enum : uint32_t {
  /** @brief Maximum DMA transfer length (DMCRA is 16-bit) */
  k_mock_dmaca_len_max = 65535U,

  /** @brief Maximum caller-supplied timeout_cycles (NASA Rule 2 loop bound) */
  k_mock_dmaca_timeout_cycles_max = 10000000U,
} mock_dmaca_limits_t;

/* =============================================================================
 * Mock State
 * =============================================================================
 */

/** @brief Number of rx_dmaca_init() calls since last reset */
static uint32_t s_init_count;

/** @brief Number of rx_dmaca_deinit() calls since last reset */
static uint32_t s_deinit_count;

/** @brief Whether the mock DMAC is currently initialized */
static bool s_is_initialized;

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
 * @pre Test harness is initialized; Unity setUp() has been entered
 * @pre No concurrent calls to any mock function (not thread-safe)
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
  s_is_initialized  = false;
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
 * @pre Called from the test thread; no concurrent calls to any mock function
 * @pre Mock state initialized (mock_rx_dmaca_reset() called at test start)
 * @post s_transfer_result is set to result
 * @post All subsequent rx_dmaca_transfer_poll() calls return result until changed
 *
 * @note Not thread-safe; call only from test thread
 * @note No allocation or blocking occurs
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
 * @pre mock_rx_dmaca_reset() called at test start to clear s_init_count
 * @pre Counting rx_dmaca_init() invocations; no concurrent mock modification
 * @post Returned value equals s_init_count at time of call
 * @post No mock state modified (pure read, no side effects)
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
 * @pre mock_rx_dmaca_reset() called at test start to clear s_deinit_count
 * @pre No concurrent threads modifying mock state (serialize access)
 * @post Returned value is a snapshot of s_deinit_count at time of call
 * @post No mock state modified (pure read, no side effects)
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
 * @pre mock_rx_dmaca_reset() called at test start to clear s_transfer_count
 * @pre Mock state fully initialized; no concurrent modification of s_transfer_count
 * @post Returned value equals s_transfer_count at time of call
 * @post No side effects on DMA state; call count remains unchanged by this call
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
 * @pre Called from test thread; no concurrent modification of s_last_config
 * @pre mock_rx_dmaca_reset() called at test start to initialize s_transfer_called
 * @post Returned pointer (if non-NULL) is valid until next mock_rx_dmaca_reset()
 * @post No mock state modified (pure read, no side effects)
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
 * @pre Called from test thread with test harness initialized
 * @pre s_init_count accessible and valid (reset via mock_rx_dmaca_reset() between tests)
 * @post s_init_count incremented by exactly 1
 * @post No hardware registers modified; callers responsible for resetting between tests
 *
 * @note Not thread-safe; not reentrant
 *
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_init(void)
{
  s_init_count++;
  if (s_is_initialized) {
    return k_rx_err_invalid_state;
  }
  s_is_initialized = true;
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
 * @pre Called from test thread with test harness initialized
 * @pre s_deinit_count valid (>= 0); reset via mock_rx_dmaca_reset() between tests
 * @post s_deinit_count incremented by exactly 1
 * @post No hardware registers modified; only s_deinit_count changed
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_deinit(void)
{
  s_deinit_count++;
  if (!s_is_initialized) {
    return k_rx_err_not_initialized;
  }
  s_is_initialized = false;
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
 * @param[in] config Transfer configuration; must not be NULL (mirrors real API null check)
 *
 * @return rx_err_t
 * @retval k_rx_err_null_ptr        config is NULL or config->src is NULL
 * @retval k_rx_err_not_initialized Mock not initialized (rx_dmaca_init() not called)
 * @retval k_rx_err_invalid_arg     channel out of range, len == 0 or > k_mock_dmaca_len_max,
 *                                  dst_addr == 0, or timeout_cycles == 0 or
 *                                  timeout_cycles > k_mock_dmaca_timeout_cycles_max
 * @retval k_rx_ok                  Default (after mock_rx_dmaca_reset())
 * @retval other                    Whatever was set by mock_rx_dmaca_set_transfer_result()
 *
 * @pre Mock initialized via mock_rx_dmaca_reset(); s_transfer_count and s_last_config valid
 * @pre Called only from the test thread; not thread-safe (no concurrent calls)
 * @pre mock_rx_dmaca_set_transfer_result() may be called beforehand to preset return value
 * @pre All config fields must pass the same validation as the real rx_dmaca_transfer_poll()
 * @post s_transfer_count incremented by 1 only on successful validation
 * @post s_transfer_called = true and s_last_config updated only on successful validation
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_transfer_poll(const rx_dmaca_config_t* config)
{
  if (config == nullptr) {
    return k_rx_err_null_ptr;
  }
  if (!s_is_initialized) {
    return k_rx_err_not_initialized;
  }
  if (config->channel >= (uint8_t)k_dmac_channel_count) {
    return k_rx_err_invalid_arg;
  }
  if (config->len == 0U || config->len > (uint32_t)k_mock_dmaca_len_max) {
    return k_rx_err_invalid_arg;
  }
  if (config->timeout_cycles == 0U ||
      config->timeout_cycles > (uint32_t)k_mock_dmaca_timeout_cycles_max) {
    return k_rx_err_invalid_arg;
  }
  if (config->dst_addr == 0U) {
    return k_rx_err_invalid_arg;
  }
  if (config->src == nullptr) {
    return k_rx_err_null_ptr;
  }
  s_transfer_count++;
  s_transfer_called = true;
  s_last_config     = *config;
  return s_transfer_result;
}

/**
 * @brief Mock implementation of rx_dmaca_abort()
 *
 * @details
 * Validates channel range and returns k_rx_ok for valid channels or
 * k_rx_err_invalid_arg for out-of-range channels. Does not modify any mock
 * state; provided for link compatibility with code that calls rx_dmaca_abort()
 * in error-recovery paths.
 *
 * @param[in] channel Channel to abort (0 to k_dmac_channel_count-1); out-of-range
 *                    values return k_rx_err_invalid_arg
 *
 * @return rx_err_t
 * @retval k_rx_ok             channel is in [0, k_dmac_channel_count - 1]
 * @retval k_rx_err_invalid_arg channel >= k_dmac_channel_count
 *
 * @pre Called from the test thread only; not thread-safe (no concurrent calls)
 * @pre channel must be in [0, k_dmac_channel_count - 1] for a successful return
 * @post Returns k_rx_ok with no state modified when channel is valid
 * @post Returns k_rx_err_invalid_arg with no state modified when channel is out of range
 *
 * @note Not thread-safe; call only from test thread
 *
 * @since Version 1.0.0
 */
rx_err_t rx_dmaca_abort(uint8_t channel)
{
  if (!s_is_initialized) {
    return k_rx_err_not_initialized;
  }
  if (channel >= (uint8_t)k_dmac_channel_count) {
    return k_rx_err_invalid_arg;
  }
  return k_rx_ok;
}
