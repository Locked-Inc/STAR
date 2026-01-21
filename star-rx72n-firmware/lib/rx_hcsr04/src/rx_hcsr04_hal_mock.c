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

#include <stdio.h>
#include <string.h>

#include "mock_hcsr04_hw.h"
#include "rx_check.h"
#include "rx_hcsr04.h"
#include "rx_hcsr04_hal.h"
#include "rx_log.h"

enum : uint16_t {
  k_hcsr04_delay_none          = 0,                        /**< No delay requested */
  k_hcsr04_delay_max_us        = k_hcsr04_echo_timeout_us, /**< Maximum supported delay */
  k_hcsr04_log_msg_max         = 96,                       /**< Max log message buffer size */
  k_hcsr04_log_null_terminator = 1,                        /**< Null terminator size */
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
  RX_ASSERT(((uint16_t)pin & k_port_mask) <= k_rx_pin_max,
            "Pin portion must be <= k_rx_pin_max");
  RX_ASSERT(((uint16_t)pin >> k_port_shift) <= k_rx_port_j,
            "Port portion must be <= k_rx_port_j");
  return (pin >= k_rx_p0_0 && pin <= k_rx_pj_7);
}

/**
 * @brief Validate and extract port/pin numbers
 *
 * @param[in]  pin     GPIO pin to validate
 * @param[out] port    Pointer to receive port number
 * @param[out] pin_num Pointer to receive pin number
 * @return k_rx_ok on success, k_rx_err_invalid_arg on invalid pin/port
 */
static rx_err_t internal_validate_and_extract_pin(rx_port_pin_t pin,
                                                  uint8_t*      port,
                                                  uint8_t*      pin_num)
{
  if (!internal_is_valid_pin(pin)) {
    return k_rx_err_invalid_arg;
  }

  if (port == NULL || pin_num == NULL) {
    return k_rx_err_invalid_arg;
  }

  *port    = rx_port_from_pin(pin);
  *pin_num = rx_pin_from_pin(pin);

  if (*port > k_rx_port_j || *pin_num > k_rx_pin_max) {
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

/**
 * @brief Set the specified pin as an output
 *
 * Validates pin and port ranges, then delegates to mock_gpio_set_output().
 *
 * @param[in] pin GPIO pin to configure as output
 * @return k_rx_ok on success, k_rx_err_invalid_arg on invalid pin/port
 */
rx_err_t hcsr04_hal_gpio_set_output(rx_port_pin_t pin)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err     = internal_validate_and_extract_pin(pin, &port, &pin_num);

  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_set_output(port, pin_num);
}

/**
 * @brief Set the specified pin as an input
 *
 * Validates pin and port ranges, then delegates to mock_gpio_set_input().
 *
 * @param[in] pin GPIO pin to configure as input
 * @return k_rx_ok on success, k_rx_err_invalid_arg on invalid pin/port
 */
rx_err_t hcsr04_hal_gpio_set_input(rx_port_pin_t pin)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err     = internal_validate_and_extract_pin(pin, &port, &pin_num);

  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_set_input(port, pin_num);
}

/**
 * @brief Write a high value to the specified pin
 *
 * Validates pin and port ranges, then delegates to mock_gpio_write_high().
 *
 * @param[in] pin GPIO pin to write high
 * @return k_rx_ok on success, k_rx_err_invalid_arg on invalid pin/port
 */
rx_err_t hcsr04_hal_gpio_write_high(rx_port_pin_t pin)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err     = internal_validate_and_extract_pin(pin, &port, &pin_num);

  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_write_high(port, pin_num);
}

/**
 * @brief Write a low value to the specified pin
 *
 * Validates pin and port ranges, then delegates to mock_gpio_write_low().
 *
 * @param[in] pin GPIO pin to write low
 * @return k_rx_ok on success, k_rx_err_invalid_arg on invalid pin/port
 */
rx_err_t hcsr04_hal_gpio_write_low(rx_port_pin_t pin)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err     = internal_validate_and_extract_pin(pin, &port, &pin_num);

  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_write_low(port, pin_num);
}

/**
 * @brief Read the current value of the specified pin
 *
 * Validates pin and port ranges, then delegates to mock_gpio_read().
 *
 * @param[in]  pin   GPIO pin to read
 * @param[out] value Pointer to receive the pin state (true=high, false=low)
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg on invalid pin/port
 * @return k_rx_err_null_ptr if value pointer is NULL
 */
rx_err_t hcsr04_hal_gpio_read(rx_port_pin_t pin, bool* value)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err;

  if (value == NULL) {
    return k_rx_err_null_ptr;
  }

  err = internal_validate_and_extract_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_read(port, pin_num, value);
}

/**
 * @brief Deinitialize the specified GPIO pin
 *
 * Validates pin and port ranges, then delegates to mock_gpio_deinit().
 *
 * @param[in] pin GPIO pin to deinitialize
 * @return k_rx_ok on success, k_rx_err_invalid_arg on invalid pin/port
 */
rx_err_t hcsr04_hal_gpio_deinit(rx_port_pin_t pin)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err     = internal_validate_and_extract_pin(pin, &port, &pin_num);

  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_deinit(port, pin_num);
}

/* =============================================================================
 * Timing Functions
 * =============================================================================
 */

/**
 * @brief Delay execution by the specified number of microseconds
 *
 * @param[in] us Delay in microseconds. k_hcsr04_delay_none performs a no-op.
 *               Values above k_hcsr04_delay_max_us are clamped with a warning.
 *
 * @details Uses snprintf for warning formatting when clamping occurs.
 *          This mock provides no real-time guarantees beyond invoking mock_delay_us().
 */
void hcsr04_hal_delay_us(uint32_t us)
{
  char     message[k_hcsr04_log_msg_max];
  uint32_t message_len  = 0U;
  int      snprintf_result = 0;

  if (us == k_hcsr04_delay_none) {
    return;
  }

  if (us > k_hcsr04_delay_max_us) {
    snprintf_result = snprintf(message,
                  sizeof(message),
                  "Delay request %lu exceeds max %lu us",
                  (unsigned long)us,
                  (unsigned long)k_hcsr04_delay_max_us);
    if (snprintf_result < 0) {
      rx_log_error_str("HCSR04", "Delay request formatting error", "", 0U);
      message_len = (uint32_t)(k_hcsr04_log_msg_max - k_hcsr04_log_null_terminator);
      (void)memset(message, 0, sizeof(message));
    } else {
      message_len = (uint32_t)snprintf_result;
      if (message_len >= k_hcsr04_log_msg_max) {
        message_len = (uint32_t)(k_hcsr04_log_msg_max - k_hcsr04_log_null_terminator);
      }
    }
    rx_log_warn_str("HCSR04", "Delay request warning", message, message_len);
    return;
  }

  mock_delay_us(us);
}

uint32_t hcsr04_hal_get_time_us(void)
{
  return mock_get_time_us();
}
