/* include/hardware_init.h */

/**
 * @file hardware_init.h
 * @brief Hardware initialization setup for RX72N STAR Firmware
 * @details
 * Provides a centralized hardware initialization function that configures
 * all application-specific peripherals in the correct order.
 *
 * This is separate from rx_clock_power_init() which handles low-level clock
 * and power initialization. hardware_init() focuses on application peripherals.
 *
 * @date 2026-01-14
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_HARDWARE_INIT_H
#define STAR_RX72N_HARDWARE_INIT_H

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize application-specific hardware
 *
 * Configures peripherals required by the STAR robot:
 * - GPIO pins
 * - Communication interfaces (SPI, UART, I2C)
 * - Timers and PWM
 * - ADC channels
 *
 * Call this after rx_clock_power_init() completes.
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t hardware_init(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_HARDWARE_INIT_H */
