/* star-rx72n-firmware/tests/mocks/mock_time.c */
/**
 * @file mock_time.c
 * @brief Mock Time Implementation for Testing
 *
 * Mock implementation of rx_time_interface_t for testing.
 * Allows tests to control time progression without real delays.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_time.h"

#include <string.h>

#include "tx_api.h"

/* =============================================================================
 * ThreadX tx_time_get() Mock Implementation
 * =============================================================================
 */

static ULONG s_mock_tx_ticks = 0;

ULONG tx_time_get(void)
{
  return s_mock_tx_ticks;
}

void mock_tx_set_time(ULONG ticks)
{
  s_mock_tx_ticks = ticks;
}

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

static mock_time_t g_mock_time;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

static mock_time_t* internal_get_mock(mock_time_t* mock)
{
  return (mock != nullptr) ? mock : &g_mock_time;
}

/* =============================================================================
 * Interface Implementation Functions
 * =============================================================================
 */

/**
 * @brief Implementation of sleep_ms interface function
 *
 * @param[in,out] ctx Mock time context
 * @param[in] ms Milliseconds to sleep
 */
static void impl_sleep_ms(void* ctx, uint32_t ms)
{
  mock_time_t* m = (mock_time_t*)ctx;
  if (m == nullptr) {
    m = &g_mock_time;
  }

  m->sleep_call_count++;
  m->total_sleep_ms += ms;

  if (m->auto_advance) {
    m->current_time_ms += ms;
  }
}

/**
 * @brief Implementation of get_ms interface function
 *
 * @param[in] ctx Mock time context
 *
 * @return Current mock time in milliseconds
 */
static uint32_t impl_get_ms(void* ctx)
{
  mock_time_t* m = (mock_time_t*)ctx;
  if (m == nullptr) {
    m = &g_mock_time;
  }

  return m->current_time_ms;
}

/**
 * @brief Implementation of is_elapsed interface function
 *
 * @param[in] ctx Mock time context
 * @param[in] start_ms Start time in milliseconds
 * @param[in] timeout_ms Timeout duration in milliseconds
 *
 * @return true if timeout elapsed, false otherwise
 */
static bool impl_is_elapsed(void* ctx, uint32_t start_ms, uint32_t timeout_ms)
{
  mock_time_t* m = (mock_time_t*)ctx;
  if (m == nullptr) {
    m = &g_mock_time;
  }

  /*
   * Handle wrap-around correctly using unsigned integer subtraction.
   * Due to modular arithmetic (2^32), this works even when current_time_ms wraps.
   * Example: current=5, start=4294967295 -> elapsed = 5 - 4294967295 = 6 (correct)
   */
  uint32_t elapsed = m->current_time_ms - start_ms;

  return elapsed >= timeout_ms;
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock time instance
 *
 * @param[in,out] mock Mock time instance (NULL uses global instance)
 *
 * @return k_rx_ok on success
 */
rx_err_t mock_time_init(mock_time_t* mock)
{
  mock_time_t* m = internal_get_mock(mock);

  memset(m, 0, sizeof(mock_time_t));
  m->initialized  = true;
  m->auto_advance = false; /* Default: no auto-advance */

  return k_rx_ok;
}

/**
 * @brief Deinitialize mock time instance
 *
 * @param[in,out] mock Mock time instance (NULL uses global instance)
 *
 * @return k_rx_ok on success
 */
rx_err_t mock_time_deinit(mock_time_t* mock)
{
  mock_time_t* m = internal_get_mock(mock);

  memset(m, 0, sizeof(mock_time_t));

  return k_rx_ok;
}

/**
 * @brief Get time interface populated with mock implementation
 *
 * @param[out] iface Time interface structure to populate
 * @param[in] mock Mock time instance (NULL uses global instance)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if iface is NULL
 */
rx_err_t mock_time_get_interface(rx_time_interface_t* iface, mock_time_t* mock)
{
  if (iface == nullptr) {
    return k_rx_err_null_ptr;
  }

  mock_time_t* m = internal_get_mock(mock);

  iface->ctx        = m;
  iface->sleep_ms   = impl_sleep_ms;
  iface->get_ms     = impl_get_ms;
  iface->is_elapsed = impl_is_elapsed;

  return k_rx_ok;
}

/* =============================================================================
 * Time Control Functions
 * =============================================================================
 */

/**
 * @brief Advance mock time by specified milliseconds
 *
 * @param[in,out] mock Mock time instance (NULL uses global instance)
 * @param[in] ms Milliseconds to advance
 */
void mock_time_advance(mock_time_t* mock, uint32_t ms)
{
  mock_time_t* m = internal_get_mock(mock);

  m->current_time_ms += ms;
}

/**
 * @brief Set mock time to specific value
 *
 * @param[in,out] mock Mock time instance (NULL uses global instance)
 * @param[in] ms Time value in milliseconds
 */
void mock_time_set(mock_time_t* mock, uint32_t ms)
{
  mock_time_t* m = internal_get_mock(mock);

  m->current_time_ms = ms;
}

/**
 * @brief Enable or disable auto-advance mode
 *
 * When enabled, time automatically advances during sleep_ms calls.
 *
 * @param[in,out] mock Mock time instance (NULL uses global instance)
 * @param[in] enable True to enable auto-advance, false to disable
 */
void mock_time_set_auto_advance(mock_time_t* mock, bool enable)
{
  mock_time_t* m = internal_get_mock(mock);

  m->auto_advance = enable;
}

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

/**
 * @brief Get total number of sleep_ms calls
 *
 * @param[in] mock Mock time instance (NULL uses global instance)
 *
 * @return Number of sleep_ms calls
 */
uint32_t mock_time_get_sleep_count(mock_time_t* mock)
{
  mock_time_t* m = internal_get_mock(mock);

  return m->sleep_call_count;
}

/**
 * @brief Get total sleep time across all sleep_ms calls
 *
 * @param[in] mock Mock time instance (NULL uses global instance)
 *
 * @return Total sleep time in milliseconds
 */
uint32_t mock_time_get_total_sleep(mock_time_t* mock)
{
  mock_time_t* m = internal_get_mock(mock);

  return m->total_sleep_ms;
}

/**
 * @brief Reset sleep call and total sleep counters to zero
 *
 * @param[in,out] mock Mock time instance (NULL uses global instance)
 */
void mock_time_reset_counters(mock_time_t* mock)
{
  mock_time_t* m = internal_get_mock(mock);

  m->sleep_call_count = 0;
  m->total_sleep_ms   = 0;
}
