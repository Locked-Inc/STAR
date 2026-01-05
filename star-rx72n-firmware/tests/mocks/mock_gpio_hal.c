/* tests/mocks/mock_gpio_hal.c */

/**
 * @file mock_gpio_hal.c
 * @brief Mock GPIO HAL Function Implementation
 *
 * Provides mock implementations of GPIO HAL functions for unit testing.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_gpio_hal.h"

#include <string.h>

#include "rx_port_constants.h"

/* =============================================================================
 * Global State
 * =============================================================================
 */

mock_gpio_state_t g_mock_gpio;

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Record a call in the history
 */
static void internal_record_call(mock_gpio_call_type_t type, gpio_pin_t pin)
{
  if (g_mock_gpio.call_count < k_mock_gpio_call_history_size) {
    mock_gpio_call_t* call = &g_mock_gpio.call_history[g_mock_gpio.call_count];
    call->type             = type;
    call->pin              = pin;
    g_mock_gpio.call_count++;
  }
}

/**
 * @brief Check and consume error injection
 */
static rx_err_t internal_check_error(void)
{
  if (g_mock_gpio.error_set) {
    rx_err_t err           = g_mock_gpio.next_error;
    g_mock_gpio.error_set  = false;
    return err;
  }
  return k_rx_ok;
}

/**
 * @brief Validate port and pin
 */
static rx_err_t internal_validate_pin(gpio_pin_t pin, uint8_t* port, uint8_t* pin_num)
{
  *port    = gpio_pin_get_port(pin);
  *pin_num = gpio_pin_get_pin(pin);

  /* Validate port - check against known valid ports */
  bool valid_port = false;
  switch (*port) {
    case k_rx_port_0:
    case k_rx_port_1:
    case k_rx_port_2:
    case k_rx_port_3:
    case k_rx_port_4:
    case k_rx_port_5:
    case k_rx_port_a:
    case k_rx_port_b:
    case k_rx_port_c:
    case k_rx_port_d:
    case k_rx_port_e:
    case k_rx_port_j:
      valid_port = true;
      break;
    default:
      break;
  }

  if (!valid_port) {
    return k_rx_err_gpio_invalid_port;
  }

  /* Validate pin (0-7) */
  if (*pin_num > k_rx_pin_max) {
    return k_rx_err_gpio_invalid_pin;
  }

  return k_rx_ok;
}

/**
 * @brief Get pin state pointer
 */
static mock_gpio_pin_state_t* internal_get_pin_state(uint8_t port, uint8_t pin)
{
  if (port >= k_mock_gpio_max_ports || pin >= k_mock_gpio_pins_per_port) {
    return NULL;
  }
  return &g_mock_gpio.pins[port][pin];
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

void mock_gpio_init(void)
{
  memset(&g_mock_gpio, 0, sizeof(g_mock_gpio));
}

void mock_gpio_reset(void)
{
  mock_gpio_init();
}

/* =============================================================================
 * Test Setup Functions
 * =============================================================================
 */

void mock_gpio_set_input_value(gpio_pin_t pin, bool value)
{
  uint8_t port    = gpio_pin_get_port(pin);
  uint8_t pin_num = gpio_pin_get_pin(pin);

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != NULL) {
    state->input_value = value;
  }
}

void mock_gpio_set_next_error(rx_err_t err)
{
  g_mock_gpio.next_error = err;
  g_mock_gpio.error_set  = true;
}

void mock_gpio_clear_error(void)
{
  g_mock_gpio.error_set = false;
}

/* =============================================================================
 * State Inspection Functions
 * =============================================================================
 */

mock_gpio_direction_t mock_gpio_get_direction(gpio_pin_t pin)
{
  uint8_t port    = gpio_pin_get_port(pin);
  uint8_t pin_num = gpio_pin_get_pin(pin);

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != NULL) {
    return state->direction;
  }
  return k_mock_gpio_dir_undefined;
}

bool mock_gpio_get_output_value(gpio_pin_t pin)
{
  uint8_t port    = gpio_pin_get_port(pin);
  uint8_t pin_num = gpio_pin_get_pin(pin);

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != NULL) {
    return state->output_value;
  }
  return false;
}

/* =============================================================================
 * Call History Functions
 * =============================================================================
 */

const mock_gpio_call_t* mock_gpio_get_call(uint16_t index)
{
  if (index < g_mock_gpio.call_count) {
    return &g_mock_gpio.call_history[index];
  }
  return NULL;
}

uint16_t mock_gpio_get_call_count(void)
{
  return g_mock_gpio.call_count;
}

void mock_gpio_clear_history(void)
{
  g_mock_gpio.call_count = 0;
}

/* =============================================================================
 * Mock GPIO HAL Functions
 * =============================================================================
 */

rx_err_t gpio_set_output(gpio_pin_t pin)
{
  internal_record_call(k_mock_gpio_call_set_output, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port, pin_num;
  err = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != NULL) {
    state->direction = k_mock_gpio_dir_output;
  }

  return k_rx_ok;
}

rx_err_t gpio_set_input(gpio_pin_t pin)
{
  internal_record_call(k_mock_gpio_call_set_input, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port, pin_num;
  err = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != NULL) {
    state->direction = k_mock_gpio_dir_input;
  }

  return k_rx_ok;
}

rx_err_t gpio_write_high(gpio_pin_t pin)
{
  internal_record_call(k_mock_gpio_call_write_high, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port, pin_num;
  err = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != NULL) {
    state->output_value = true;
  }

  return k_rx_ok;
}

rx_err_t gpio_write_low(gpio_pin_t pin)
{
  internal_record_call(k_mock_gpio_call_write_low, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port, pin_num;
  err = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != NULL) {
    state->output_value = false;
  }

  return k_rx_ok;
}

rx_err_t gpio_toggle(gpio_pin_t pin)
{
  internal_record_call(k_mock_gpio_call_toggle, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port, pin_num;
  err = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != NULL) {
    state->output_value = !state->output_value;
  }

  return k_rx_ok;
}

rx_err_t gpio_read(gpio_pin_t pin, bool* value)
{
  internal_record_call(k_mock_gpio_call_read, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (value == NULL) {
    return k_rx_err_null_pointer;
  }

  uint8_t port, pin_num;
  err = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != NULL) {
    *value = state->input_value;
  }

  return k_rx_ok;
}
