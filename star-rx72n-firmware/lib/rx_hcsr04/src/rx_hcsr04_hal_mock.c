/* lib/rx_hcsr04/src/rx_hcsr04_hal_mock.c */

/**
 * @file rx_hcsr04_hal_mock.c
 * @brief HC-SR04 HAL Implementation for Mock Testing
 *
 * @details
 * Mock implementation of the HC-SR04 HAL interface for host-side testing.
 * Delegates to mock hardware functions that simulate GPIO and timing behavior.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_hcsr04_hw.h"
#include "rx_hcsr04.h"
#include "rx_hcsr04_hal.h"

enum {
  k_hcsr04_delay_none   = 0,                        /**< No delay requested */
  k_hcsr04_delay_max_us = k_hcsr04_echo_timeout_us, /**< Maximum supported delay */
};

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @brief Validate pin parameter
 *
 * @param[in] pin GPIO pin to validate
 * @return true if pin is valid, false otherwise
 */
static inline bool internal_is_valid_pin(rx_port_pin_t pin)
{
  return (pin >= k_rx_p0_0 && pin <= k_rx_pj_7);
}

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

/**
 * @brief Set the specified pin as an output
 *
 * Validates pin and port ranges, then delegates to mock_gpio_set_output().
 */
rx_err_t hcsr04_hal_gpio_set_output(rx_port_pin_t pin)
{
  if (!internal_is_valid_pin(pin)) {
    return k_rx_err_invalid_arg;
  }

  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  if (port > k_rx_port_j || pin_num > k_rx_pin_max) {
    return k_rx_err_invalid_arg;
  }

  return mock_gpio_set_output(port, pin_num);
}

rx_err_t hcsr04_hal_gpio_set_input(rx_port_pin_t pin)
{
  if (!internal_is_valid_pin(pin)) {
    return k_rx_err_invalid_arg;
  }

  /* Extract port and pin for secondary validation */
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate the extracted port and pin are within expected ranges */
  if (port > k_rx_port_j || pin_num > k_rx_pin_max) {
    return k_rx_err_invalid_arg;
  }

  return mock_gpio_set_input(port, pin_num);
}

rx_err_t hcsr04_hal_gpio_write_high(rx_port_pin_t pin)
{
  if (!internal_is_valid_pin(pin)) {
    return k_rx_err_invalid_arg;
  }

  /* Extract port and pin for secondary validation */
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate the extracted port and pin are within expected ranges */
  if (port > k_rx_port_j || pin_num > k_rx_pin_max) {
    return k_rx_err_invalid_arg;
  }

  return mock_gpio_write_high(port, pin_num);
}

rx_err_t hcsr04_hal_gpio_write_low(rx_port_pin_t pin)
{
  if (!internal_is_valid_pin(pin)) {
    return k_rx_err_invalid_arg;
  }

  /* Extract port and pin for secondary validation */
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate the extracted port and pin are within expected ranges */
  if (port > k_rx_port_j || pin_num > k_rx_pin_max) {
    return k_rx_err_invalid_arg;
  }

  return mock_gpio_write_low(port, pin_num);
}

rx_err_t hcsr04_hal_gpio_read(rx_port_pin_t pin, bool* value)
{
  if (!internal_is_valid_pin(pin)) {
    return k_rx_err_invalid_arg;
  }
  if (value == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Extract port and pin for secondary validation */
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate the extracted port and pin are within expected ranges */
  if (port > k_rx_port_j || pin_num > k_rx_pin_max) {
    return k_rx_err_invalid_arg;
  }

  return mock_gpio_read(port, pin_num, value);
}

rx_err_t hcsr04_hal_gpio_deinit(rx_port_pin_t pin)
{
  if (!internal_is_valid_pin(pin)) {
    return k_rx_err_invalid_arg;
  }

  /* Extract port and pin for secondary validation */
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate the extracted port and pin are within expected ranges */
  if (port > k_rx_port_j || pin_num > k_rx_pin_max) {
    return k_rx_err_invalid_arg;
  }

  return mock_gpio_deinit(port, pin_num);
}

/* =============================================================================
 * Timing Functions
 * =============================================================================
 */

void hcsr04_hal_delay_us(uint32_t us)
{
  if (us == k_hcsr04_delay_none) {
    return;
  }

  if (us > k_hcsr04_delay_max_us) {
    return;
  }

  mock_delay_us(us);
}

uint32_t hcsr04_hal_get_time_us(void)
{
  return mock_get_time_us();
}
