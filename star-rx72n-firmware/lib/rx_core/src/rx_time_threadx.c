/**
 * @file rx_time_threadx.c
 * @brief ThreadX Time Implementation for RX72N
 *
 * Real implementation of rx_time_interface_t using ThreadX RTOS.
 * This file is only compiled when __RX__ is defined (firmware build).
 *
 * ThreadX tick rate: 100 Hz (10ms per tick)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifdef __RX__

#include "rx_check.h"
#include "rx_time_constants.h"
#include "rx_time_interface.h"
#include "tx_api.h"

/* =============================================================================
 * Interface Implementation Functions
 * =============================================================================
 */

/**
 * @brief Sleep for specified milliseconds
 *
 * Yields CPU to other threads via tx_thread_sleep().
 * Rounds up to nearest tick (minimum 1 tick).
 */
static void impl_sleep_ms(void* ctx, uint32_t ms)
{
  (void)ctx; /* ThreadX uses global state - no context needed */

  /* Convert ms to ticks, rounding up */
  uint32_t ticks = (ms + k_threadx_ms_per_tick - 1) / k_threadx_ms_per_tick;

  if (ticks > 0) {
    tx_thread_sleep(ticks);
  }
}

/**
 * @brief Get current time in milliseconds
 *
 * Uses tx_time_get() which returns the ThreadX tick count.
 */
static uint32_t impl_get_ms(void* ctx)
{
  (void)ctx; /* ThreadX uses global state - no context needed */

  return tx_time_get() * k_threadx_ms_per_tick;
}

/**
 * @brief Check if timeout has elapsed
 *
 * Handles wrap-around correctly for 32-bit time values.
 */
static bool impl_is_elapsed(void* ctx, uint32_t start_ms, uint32_t timeout_ms)
{
  (void)ctx; /* ThreadX uses global state - no context needed */

  uint32_t now     = tx_time_get() * k_threadx_ms_per_tick;
  uint32_t elapsed = now - start_ms;

  return elapsed >= timeout_ms;
}

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Get time interface for ThreadX
 *
 * Populates an rx_time_interface_t structure with function pointers
 * to the ThreadX implementation.
 *
 * @param[out] iface Interface to populate
 *
 * @return k_rx_ok on success, k_rx_err_null_ptr if iface is NULL
 */
rx_err_t rx_time_threadx_get_interface(rx_time_interface_t* iface)
{
  RX_CHECK_NULL_PTR(iface, "TIME", "Interface pointer is NULL");

  iface->ctx        = NULL; /* No context needed for ThreadX */
  iface->sleep_ms   = impl_sleep_ms;
  iface->get_ms     = impl_get_ms;
  iface->is_elapsed = impl_is_elapsed;

  RX_CHECK_NULL_PTR(iface->sleep_ms, "TIME", "sleep_ms is NULL");
  RX_CHECK_NULL_PTR(iface->get_ms, "TIME", "get_ms is NULL");
  RX_CHECK_NULL_PTR(iface->is_elapsed, "TIME", "is_elapsed is NULL");

  return k_rx_ok;
}

#endif /* __RX__ */
