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
#include "rx_port_constants.h"

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

/**
 * @var g_pin_host_irq
 * @brief Canonical pin identifier for the HOST_IRQ output signal
 *
 * @details
 * Identifies GPIO pin P6.7 (RX72N package pin 98), which drives the active-low
 * HOST_IRQ line to the RPi5.  Assert LOW before sending telemetry; deassert HIGH
 * after the transfer completes (or on error).
 *
 * Using a single named constant ensures that every module (hardware_init,
 * telemetry_task, future modules) refers to the same pin without embedding the
 * raw port/bit value at multiple call sites.
 *
 * @note Read-only.  Do not modify from application code.
 * @warning Only hardware_init.c may initialise this pin direction; all other
 *          modules must use gpio_write_low() / gpio_write_high() exclusively.
 * @since Version 1.0.0
 * @see hardware_init() Configures this pin as a GPIO output, initially HIGH
 * @see gpio_write_low()  Assert HOST_IRQ (data ready)
 * @see gpio_write_high() Deassert HOST_IRQ (transfer complete)
 */
extern const rx_port_pin_t g_pin_host_irq;
