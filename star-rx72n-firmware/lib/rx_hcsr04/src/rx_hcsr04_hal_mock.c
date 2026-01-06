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
#include "rx_hcsr04_hal.h"

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

rx_err_t hcsr04_hal_gpio_set_output(gpio_pin_t pin)
{
  return mock_gpio_set_output(gpio_pin_get_port(pin), gpio_pin_get_pin(pin));
}

rx_err_t hcsr04_hal_gpio_set_input(gpio_pin_t pin)
{
  return mock_gpio_set_input(gpio_pin_get_port(pin), gpio_pin_get_pin(pin));
}

rx_err_t hcsr04_hal_gpio_write_high(gpio_pin_t pin)
{
  return mock_gpio_write_high(gpio_pin_get_port(pin), gpio_pin_get_pin(pin));
}

rx_err_t hcsr04_hal_gpio_write_low(gpio_pin_t pin)
{
  return mock_gpio_write_low(gpio_pin_get_port(pin), gpio_pin_get_pin(pin));
}

rx_err_t hcsr04_hal_gpio_read(gpio_pin_t pin, bool* value)
{
  return mock_gpio_read(gpio_pin_get_port(pin), gpio_pin_get_pin(pin), value);
}

rx_err_t hcsr04_hal_gpio_deinit(gpio_pin_t pin)
{
  return mock_gpio_deinit(gpio_pin_get_port(pin), gpio_pin_get_pin(pin));
}

/* =============================================================================
 * Timing Functions
 * =============================================================================
 */

void hcsr04_hal_delay_us(uint32_t us)
{
  mock_delay_us(us);
}

uint32_t hcsr04_hal_get_time_us(void)
{
  return mock_get_time_us();
}
