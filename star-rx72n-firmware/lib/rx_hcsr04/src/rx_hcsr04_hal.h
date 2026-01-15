/* lib/rx_hcsr04/src/rx_hcsr04_hal.h */

/**
 * @file rx_hcsr04_hal.h
 * @brief HC-SR04 Hardware Abstraction Layer Interface
 *
 * @details
 * Defines the hardware abstraction layer for HC-SR04 driver. This interface
 * allows the driver to work with either real hardware or mock implementations
 * for testing. The build system selects which implementation to link.
 *
 * Two implementations:
 * - rx_hcsr04_hal_hw.c: Real RX72N hardware (GPIO, timers)
 * - rx_hcsr04_hal_mock.c: Mock for host-side unit testing
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef RX_HCSR04_HAL_H
#define RX_HCSR04_HAL_H

#include <stdint.h>

#include "rx_err.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

/**
 * @brief Configure GPIO pin as output
 *
 * @param[in] pin GPIO pin (type-safe enum)
 *
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t hcsr04_hal_gpio_set_output(rx_port_pin_t pin);

/**
 * @brief Configure GPIO pin as input
 *
 * @param[in] pin GPIO pin (type-safe enum)
 *
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t hcsr04_hal_gpio_set_input(rx_port_pin_t pin);

/**
 * @brief Write GPIO pin high
 *
 * @param[in] pin GPIO pin (type-safe enum)
 *
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t hcsr04_hal_gpio_write_high(rx_port_pin_t pin);

/**
 * @brief Write GPIO pin low
 *
 * @param[in] pin GPIO pin (type-safe enum)
 *
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t hcsr04_hal_gpio_write_low(rx_port_pin_t pin);

/**
 * @brief Read GPIO pin state
 *
 * @param[in]  pin   GPIO pin (type-safe enum)
 * @param[out] value Pointer to store pin state (true=high, false=low)
 *
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t hcsr04_hal_gpio_read(rx_port_pin_t pin, bool* value);

/**
 * @brief Deinitialize GPIO pin
 *
 * @param[in] pin GPIO pin (type-safe enum)
 *
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t hcsr04_hal_gpio_deinit(rx_port_pin_t pin);

/* =============================================================================
 * Timing Functions
 * =============================================================================
 */

/**
 * @brief Delay for specified microseconds
 *
 * @param[in] us Microseconds to delay
 */
void hcsr04_hal_delay_us(uint32_t us);

/**
 * @brief Get current time in microseconds
 *
 * @return Current time in microseconds
 */
uint32_t hcsr04_hal_get_time_us(void);

#ifdef __cplusplus
}
#endif

#endif /* RX_HCSR04_HAL_H */
