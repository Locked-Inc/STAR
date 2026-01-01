/**
 * @file rx_time_threadx.c
 * @brief ThreadX Time Implementation for RX72N
 *
 * Real implementation of rx_time_interface_t using ThreadX RTOS.
 * This file is only compiled when __RX__ is defined (firmware build).
 *
 * ThreadX tick rate: 100 Hz (10ms per tick)
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#ifdef __RX__

#include "rx_time_interface.h"
#include "tx_api.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief Milliseconds per ThreadX tick
 *
 * ThreadX is configured at 100 Hz (TX_TIMER_TICKS_PER_SECOND = 100)
 * So each tick is 10ms.
 */
#define MS_PER_TICK 10

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
  (void)ctx;

  /* Convert ms to ticks, rounding up */
  uint32_t ticks = (ms + MS_PER_TICK - 1) / MS_PER_TICK;

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
  (void)ctx;

  return tx_time_get() * MS_PER_TICK;
}

/**
 * @brief Check if timeout has elapsed
 *
 * Handles wrap-around correctly for 32-bit time values.
 */
static bool impl_is_elapsed(void* ctx, uint32_t start_ms, uint32_t timeout_ms)
{
  (void)ctx;

  uint32_t now     = tx_time_get() * MS_PER_TICK;
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
 * @return k_rx_ok on success, k_rx_err_null_pointer if iface is NULL
 */
rx_err_t rx_time_threadx_get_interface(rx_time_interface_t* iface)
{
  if (iface == NULL) {
    return k_rx_err_null_pointer;
  }

  iface->ctx        = NULL; /* No context needed for ThreadX */
  iface->sleep_ms   = impl_sleep_ms;
  iface->get_ms     = impl_get_ms;
  iface->is_elapsed = impl_is_elapsed;

  return k_rx_ok;
}

#endif /* __RX__ */
