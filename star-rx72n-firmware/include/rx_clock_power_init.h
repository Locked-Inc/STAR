/* include/rx_clock_power_init.h */

/**
 * @file rx_clock_power_init.h
 * @brief RX72N clock and power initialization interface
 *
 * Provides low-level clock and power initialization for early boot.
 *
 * @note
 * PLL itself stands for Phase Locked Loop.
 * It is an internal circuit used to multiply the frequency of a clock signal from an oscillator
 *
 * @date 2026-01-14
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_CLOCK_POWER_INIT_H
#define STAR_RX72N_CLOCK_POWER_INIT_H

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize RX72N clocks and power management
 *
 * Configures:
 * - PLL to 240 MHz
 * - Peripheral clocks (PCLKA=120MHz, PCLKB/C/D=60MHz)
 * - Module stop control
 *
 * Call this before hardware_init() and ThreadX initialization.
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_clock_power_init(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_CLOCK_POWER_INIT_H */
