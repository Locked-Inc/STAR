/* lib/rx_core/inc/rx_wdt.h */

/**
 * @file rx_wdt.h
 * @brief Watchdog Timer (WDT) Driver for RX72N
 * @details
 * WDT driver providing software watchdog functionality. Works in conjunction
 * with IWDT (Independent Watchdog) for comprehensive system monitoring.
 *
 * Differences from IWDT:
 * - Can be stopped/restarted in software
 * - Uses system clock (not independent clock)
 * - Provides additional flexibility for development/debugging
 * - Complementary to IWDT for production systems
 *
 * Hardware: RX72N Watchdog Timer (WDT)
 * - Register Base: 0x00088020
 * - Clock: PCLKB (system peripheral clock)
 * - Timeout Range: Configurable via clock divider
 * - Can be disabled in software (unlike IWDT)
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_WDT_H
#define STAR_RX72N_WDT_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief WDT timeout periods (clock divider settings) */
typedef enum {
  k_wdt_timeout_256_cycles   = 0, /**< 256 PCLKB cycles */
  k_wdt_timeout_1024_cycles  = 1, /**< 1024 PCLKB cycles */
  k_wdt_timeout_4096_cycles  = 2, /**< 4096 PCLKB cycles */
  k_wdt_timeout_16384_cycles = 3, /**< 16384 PCLKB cycles */
} wdt_timeout_period_t;

/* =============================================================================
 * Data Structures
 * =============================================================================
 */

/**
 * @brief WDT configuration structure
 */
typedef struct {
  wdt_timeout_period_t timeout_period;   /**< Timeout period */
  bool                 enable_on_init;   /**< Start immediately */
  bool                 reset_on_timeout; /**< Reset vs interrupt */
} rx_wdt_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize the Watchdog Timer
 *
 * @param[in] config Configuration structure (NULL for defaults)
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg Invalid configuration
 * @retval k_rx_err_invalid_state Already initialized
 */
rx_err_t rx_wdt_init(const rx_wdt_config_t* config);

/**
 * @brief Start the WDT
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_not_initialized WDT not initialized
 */
rx_err_t rx_wdt_start(void);

/**
 * @brief Stop the WDT
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_not_initialized WDT not initialized
 */
rx_err_t rx_wdt_stop(void);

/**
 * @brief Feed the watchdog timer
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_not_initialized WDT not initialized
 */
rx_err_t rx_wdt_feed(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_WDT_H */
