/**
 * @file rx_clock_power_init.h
 * @brief RX72N System Clock and Power Management Initialization
 *
 * @details
 * Declares the clock and power initialization function that configures
 * the RX72N for 240 MHz operation with PLL.
 *
 * @see rx_clock_power_init.c Implementation
 */

#ifndef STAR_RX_CLOCK_POWER_INIT_H
#define STAR_RX_CLOCK_POWER_INIT_H

#include "rx_err.h"

/**
 * @brief Initialize system clocks and power management
 *
 * @details
 * Configures the RX72N clock tree for maximum performance:
 * - ICLK (CPU): 240 MHz
 * - PCLKA (Timers): 120 MHz
 * - PCLKB (UART/SPI): 60 MHz
 * - FCLK (Flash): 60 MHz
 *
 * Must be called early in main() before any peripheral initialization.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Clock configuration successful
 * @retval k_rx_err_timeout PLL failed to lock
 *
 * @pre CPU reset completed
 * @pre C runtime initialized
 * @post System running at 240 MHz
 * @post All clock domains configured
 *
 * @see main() Calls this function during boot
 * @see hardware_init() Called after this function
 */
rx_err_t rx_clock_power_init(void);

#endif /* STAR_RX_CLOCK_POWER_INIT_H */
