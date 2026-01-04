/* lib/rx_hcsr04/src/rx_hcsr04_hal_hw.c */

/**
 * @file rx_hcsr04_hal_hw.c
 * @brief HC-SR04 HAL Implementation for RX72N Hardware
 *
 * @details
 * Real hardware implementation of the HC-SR04 HAL interface for RX72N.
 * Uses actual GPIO and timer peripherals for production firmware.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "hardware.h"
#include "rx_hcsr04_hal.h"
#include "rx_time_interface.h"

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

rx_err_t hcsr04_hal_gpio_set_output(uint8_t port, uint8_t pin)
{
  return gpio_set_output(port, pin);
}

rx_err_t hcsr04_hal_gpio_set_input(uint8_t port, uint8_t pin)
{
  return gpio_set_input(port, pin);
}

rx_err_t hcsr04_hal_gpio_write_high(uint8_t port, uint8_t pin)
{
  return gpio_write_high(port, pin);
}

rx_err_t hcsr04_hal_gpio_write_low(uint8_t port, uint8_t pin)
{
  return gpio_write_low(port, pin);
}

rx_err_t hcsr04_hal_gpio_read(uint8_t port, uint8_t pin, bool* value)
{
  return gpio_read(port, pin, value);
}

rx_err_t hcsr04_hal_gpio_deinit(uint8_t port, uint8_t pin)
{
  /*
   * GPIO_DEINIT is intentionally a no-op: the RX72N GPIO HAL does not provide
   * pin deallocation. GPIO pins are static resources allocated at init time.
   */
  (void)port;
  (void)pin;
  return k_rx_ok;
}

/* =============================================================================
 * Timing Functions
 * =============================================================================
 */

void hcsr04_hal_delay_us(uint32_t us)
{
  delay_us(us);
}

uint32_t hcsr04_hal_get_time_us(void)
{
  return get_time_us();
}
