/* lib/rx_core/inc/rx_time_constants.h */

/**
 * @file rx_time_constants.h
 * @brief Time Conversion Constants
 *
 * Centralized time unit conversion constants used throughout the firmware
 * for timeout calculations and time conversions.
 *
 * @date 2026-01-07
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_TIME_CONSTANTS_H
#define STAR_RX_TIME_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Time Unit Conversion Constants
 * =============================================================================
 */

/**
 * @brief Time unit conversion factors
 *
 * Standard SI time unit conversions used for timeout calculations and
 * time conversions throughout the codebase.
 *
 * Example usage:
 * @code
 * // Convert 5 seconds to milliseconds
 * uint32_t timeout_ms = 5 * k_rx_ms_per_second;
 *
 * // Convert microseconds to nanoseconds
 * uint32_t delay_ns = delay_us * k_rx_ns_per_us;
 * @endcode
 */
typedef enum {
  k_rx_ms_per_second = 1000,    /**< Milliseconds per second */
  k_rx_us_per_ms     = 1000,    /**< Microseconds per millisecond */
  k_rx_ns_per_us     = 1000,    /**< Nanoseconds per microsecond */
  k_rx_us_per_second = 1000000, /**< Microseconds per second */
  k_rx_ns_per_ms     = 1000000, /**< Nanoseconds per millisecond */
} rx_time_conversion_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_TIME_CONSTANTS_H */
