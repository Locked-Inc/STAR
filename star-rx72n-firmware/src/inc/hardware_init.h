/* star-rx72n-firmware/src/inc/hardware_init.h */
/**
 * @file hardware_init.h
 * @brief Application-Specific Hardware Initialization
 *
 * @details
 * Declares the hardware initialization function that configures all
 * application peripherals after system clock setup.
 *
 * @see hardware_init.c Implementation
 
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
*/

#pragma once

#include "rx_err.h"

/**
 * @brief Initialize all application hardware peripherals
 *
 * @details
 * Initializes GPIO, timers, UART, SPI, I2C, ADC in the correct order.
 * Must be called after rx_clock_power_init() and before tx_kernel_enter().
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All peripherals initialized successfully
 * @retval k_rx_err_* Peripheral initialization failed
 *
 * @pre System clocks configured (rx_clock_power_init called)
 * @post All peripherals ready for use
 *
 * @see rx_clock_power_init() Must be called first
 * @see main() Calls this function during boot
 */
rx_err_t hardware_init(void);
