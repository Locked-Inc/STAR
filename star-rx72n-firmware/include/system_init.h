/* include/system_init.h */

/**
 * @file system_init.h
 * @brief RX72N system initialization interface
 *
 * Provides the early boot system initialization entry point.
 *
 * @note
 * PLL itself stands for Phase Locked Loop.
 * It is an internal circuit used to multiply the frequency of a clock signal from an oscillator
 */

#ifndef STAR_RX72N_SYSTEM_INIT_H
#define STAR_RX72N_SYSTEM_INIT_H

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize RX72N system (clocks, peripherals)
 *
 * Configures:
 * - PLL to 240 MHz
 * - Peripheral clocks (PCLKA=120MHz, PCLKB/C/D=60MHz)
 * - Module stop control
 *
 * Call this before ThreadX initialization.
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t system_init(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_SYSTEM_INIT_H */
