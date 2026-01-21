/* lib/rx_core/inc/rx_threadx_config.h */

/**
 * @file rx_threadx_config.h
 * @brief ThreadX RTOS Configuration Constants
 *
 * Centralized ThreadX timing constants used for time conversions throughout
 * the firmware. These values must match the ThreadX configuration in tx_user.h.
 *
 * @date 2026-01-07
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_THREADX_CONFIG_H
#define STAR_RX_THREADX_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ThreadX Timing Constants
 * =============================================================================
 */

/**
 * @brief ThreadX timing configuration
 *
 * These constants define the ThreadX timer tick frequency and are used for
 * converting between milliseconds and ThreadX ticks.
 *
 * Configuration (from tx_user.h):
 * - TX_TIMER_TICKS_PER_SECOND = 100
 * - Tick period = 10 ms
 * - CMT0 configured for 100 Hz interrupt
 *
 * Example usage (with literal milliseconds per second):
 * @code
 * // Convert 500ms to ThreadX ticks
 * uint32_t ticks = (500 * s_rx_threadx_tick_rate_hz) / 1000;
 * tx_thread_sleep(ticks);  // Sleep for 500ms
 * @endcode
 *
 * @note In real code, use the symbolic constant @c k_rx_ms_per_second (defined
 * in @c rx_time_constants.h) instead of the literal @c 1000 for improved
 * maintainability and clarity. Example:
 * @code
 * #include "rx_time_constants.h"
 * uint32_t ticks = (500 * s_rx_threadx_tick_rate_hz) / k_rx_ms_per_second;
 * @endcode
 */
static const uint16_t s_rx_threadx_tick_rate_hz =
  100U; /**< ThreadX tick frequency: 100 Hz (10ms period) */

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_THREADX_CONFIG_H */
