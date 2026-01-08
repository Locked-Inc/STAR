/**
 * @file hardware_init.h
 * @brief Hardware initialization sequence for STAR RX72N firmware
 * @details
 * Provides a centralized hardware initialization function that configures
 * all peripherals in the correct order:
 *
 * 1. System clocks (240 MHz CPU, 120 MHz PCLKA, 60 MHz PCLKB)
 * 2. Debug UART (SCI9 at 115200 baud)
 * 3. GPIO ports (all ports configured for motor control)
 * 4. Watchdog timer (IWDT with 1000ms timeout)
 * 5. MTU encoders (4 channels for quadrature counting)
 * 6. GPTW PWM (4 channels for motor drivers)
 * 7. ADC (current sensing, 12-bit resolution)
 * 8. SPI (motor drivers + RPi5 communication)
 * 9. I2C (battery management system)
 * 10. 1-Wire (DS18B20 temperature sensor)
 * 11. USB CDC (primary communication with RPi5)
 *
 * Must be called before ThreadX initialization.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef HARDWARE_INIT_H
#define HARDWARE_INIT_H

#include "rx_err.h"

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize all hardware peripherals
 * @details
 * Initializes all peripherals in the correct order for the STAR motor
 * control system. Must be called in main() before tx_kernel_enter().
 *
 * Initialization Sequence:
 * 1. System clocks (system_init from HAL)
 * 2. Debug UART (uart_init for logging)
 * 3. GPIO ports (configure all pins)
 * 4. Watchdog timer (rx_iwdt_init)
 * 5. MTU encoders (rx_mtu3a_init for 4 channels)
 * 6. GPTW PWM (rx_gptw_init for 4 channels)
 * 7. ADC (adc_init for current sensing)
 * 8. SPI (rspi_init_peripheral for motor drivers + RPi5)
 * 9. I2C (riic_init for BQ4050 battery management)
 * 10. 1-Wire (bus manager init for DS18B20)
 * 11. USB CDC (rx_usb_init for RPi5 communication)
 *
 * Error Handling:
 * - Returns error code on first failure
 * - Logs error message via UART (if UART init succeeded)
 * - Does NOT de-initialize on failure (caller must handle)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if system init fails
 * @return k_rx_err_timeout if peripheral initialization times out
 * @return Other rx_err_t codes from peripheral drivers
 */
rx_err_t hardware_init_all(void);

#endif /* HARDWARE_INIT_H */
