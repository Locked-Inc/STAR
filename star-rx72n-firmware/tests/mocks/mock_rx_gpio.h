/* tests/mocks/mock_rx_gpio.h */

/**
 * @file mock_rx_gpio.h
 * @brief Mock GPIO Implementation for Host-Side Testing
 *
 * Provides mock GPIO functions for testing OneWire and other GPIO-dependent
 * bus protocols. Tracks GPIO pin state and direction for verification.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_GPIO_H
#define MOCK_RX_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Constants
 * =============================================================================
 */

/**
 * @brief Mock GPIO constants
 */
typedef enum {
  k_mock_gpio_max_pins = 256, /**< Maximum trackable pins (16 ports x 16 pins) */
} mock_gpio_constants_t;

/* =============================================================================
 * Mock State Control
 * =============================================================================
 */

/**
 * @brief Initialize mock GPIO subsystem
 *
 * Clears all pin states and directions.
 */
void mock_gpio_init(void);

/**
 * @brief Deinitialize mock GPIO subsystem
 */
void mock_gpio_deinit(void);

/**
 * @brief Set the value to be returned by the next gpio_read call
 *
 * @param[in] pin GPIO pin enum
 * @param[in] high True for high, false for low
 */
void mock_gpio_set_read_value(rx_port_pin_t pin, bool high);

/**
 * @brief Get the last written value for a pin
 *
 * @param[in] pin GPIO pin enum
 *
 * @return Last written value (true = high, false = low)
 */
bool mock_gpio_get_written_value(rx_port_pin_t pin);

/**
 * @brief Check if pin is configured as output
 *
 * @param[in] pin GPIO pin enum
 *
 * @return True if pin is output, false if input
 */
bool mock_gpio_is_output(rx_port_pin_t pin);

/**
 * @brief Set next error to return from GPIO operations
 *
 * @param[in] err Error to return on next GPIO call
 */
void mock_gpio_set_next_error(rx_err_t err);

/**
 * @brief Get number of times gpio_write_low was called
 *
 * @return Call count
 */
uint32_t mock_gpio_get_write_low_count(void);

/**
 * @brief Get number of times gpio_write_high was called
 *
 * @return Call count
 */
uint32_t mock_gpio_get_write_high_count(void);

/**
 * @brief Get number of times gpio_set_output was called
 *
 * @return Call count
 */
uint32_t mock_gpio_get_set_output_count(void);

/**
 * @brief Get number of times gpio_set_input was called
 *
 * @return Call count
 */
uint32_t mock_gpio_get_set_input_count(void);

/**
 * @brief Get number of times gpio_read was called
 *
 * @return Call count
 */
uint32_t mock_gpio_get_read_count(void);

/**
 * @brief Reset all call counters
 */
void mock_gpio_reset_counters(void);

/* =============================================================================
 * GPIO Functions (Mock Implementations)
 * =============================================================================
 */

/**
 * @brief Set GPIO pin as output
 *
 * @param[in] pin GPIO pin enum
 *
 * @return k_rx_ok on success, or injected error
 */
rx_err_t gpio_set_output(rx_port_pin_t pin);

/**
 * @brief Set GPIO pin as input
 *
 * @param[in] pin GPIO pin enum
 *
 * @return k_rx_ok on success, or injected error
 */
rx_err_t gpio_set_input(rx_port_pin_t pin);

/**
 * @brief Write GPIO pin low
 *
 * @param[in] pin GPIO pin enum
 *
 * @return k_rx_ok on success, or injected error
 */
rx_err_t gpio_write_low(rx_port_pin_t pin);

/**
 * @brief Write GPIO pin high
 *
 * @param[in] pin GPIO pin enum
 *
 * @return k_rx_ok on success, or injected error
 */
rx_err_t gpio_write_high(rx_port_pin_t pin);

/**
 * @brief Read GPIO pin state
 *
 * @param[in] pin GPIO pin enum
 * @param[out] high Pin state (true = high, false = low)
 *
 * @return k_rx_ok on success, or injected error
 */
rx_err_t gpio_read(rx_port_pin_t pin, bool* high);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RX_GPIO_H */
