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

rx_err_t hcsr04_hal_gpio_set_output(uint8_t port, uint8_t pin)
{
  return mock_gpio_set_output(port, pin);
}

rx_err_t hcsr04_hal_gpio_set_input(uint8_t port, uint8_t pin)
{
  return mock_gpio_set_input(port, pin);
}

rx_err_t hcsr04_hal_gpio_write_high(uint8_t port, uint8_t pin)
{
  return mock_gpio_write_high(port, pin);
}

rx_err_t hcsr04_hal_gpio_write_low(uint8_t port, uint8_t pin)
{
  return mock_gpio_write_low(port, pin);
}

rx_err_t hcsr04_hal_gpio_read(uint8_t port, uint8_t pin, bool* value)
{
  return mock_gpio_read(port, pin, value);
}

rx_err_t hcsr04_hal_gpio_deinit(uint8_t port, uint8_t pin)
{
  return mock_gpio_deinit(port, pin);
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
