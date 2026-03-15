/**
 * @file mock_gpio_hal.c
 * @brief Mock GPIO HAL function implementations for unit testing
 *
 * @details
 * Implements mock versions of all GPIO HAL functions (gpio_set_output,
 * gpio_write_high, gpio_read, etc.) for host-side unit testing without
 * RX72N hardware.
 *
 * Implementation features:
 * - Global state tracking in g_mock_gpio
 * - Per-pin direction and value state
 * - Automatic call history recording
 * - Single-shot error injection
 * - Pin validation matching real HAL behavior
 *
 * All public GPIO HAL functions follow this pattern:
 * 1. Record call in history (type + pin)
 * 2. Check for injected error (consume if set)
 * 3. Validate pin (port number, pin number)
 * 4. Update mock state
 * 5. Return k_rx_ok (or validation error)
 *
 * @par Implementation Notes:
 * - Uses byte-level for-loop for state reset (avoids banned memset)
 * - Uses nullptr (C23) for NULL checks
 * - Pin state stored as 2D array: pins[port][pin_num]
 * - Call history is circular buffer (oldest dropped when full)
 *
 * @see mock_gpio_hal.h Header with complete documentation
 * @see tests/test_gpio_hal.c Unit tests using this mock
 * @see gpio_hal.c Real GPIO HAL implementation
 *
 * @author Locked, Inc.
 * @date 2026-01-05
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_gpio_hal.h"

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
 * @brief Record a call in the history buffer
 *
 * @details
 * Appends a call record to the global call history. If history buffer
 * is full (64 calls), the call is dropped (oldest calls preserved).
 *
 * @param[in] type Type of GPIO function called
 * @param[in] pin Pin argument passed to function
 *
 * @note Called automatically by all public GPIO HAL functions
 * @note Does not check for buffer overflow beyond size limit
 */
static void internal_record_call(mock_gpio_call_type_t type, rx_port_pin_t pin)
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
 *
 * @details
 * Returns the injected error if one is set, then clears the injection
 * flag (single-shot behavior). If no error is injected, returns k_rx_ok.
 *
 * @return Injected error code, or k_rx_ok if no injection active
 *
 * @note Error injection is consumed after this call
 * @note Called by all public GPIO HAL functions before validation
 */
static rx_err_t internal_check_error(void)
{
  /* Check Nth-call injection first (before incrementing call_count) */
  if (g_mock_gpio.error_call_index != 0U &&
      g_mock_gpio.call_count + 1U == g_mock_gpio.error_call_index) {
    rx_err_t err                 = g_mock_gpio.nth_error;
    g_mock_gpio.error_call_index = 0U;
    return err;
  }
  if (g_mock_gpio.error_set) {
    rx_err_t err          = g_mock_gpio.next_error;
    g_mock_gpio.error_set = false;
    return err;
  }
  return k_rx_ok;
}

/**
 * @brief Validate port and pin numbers
 *
 * @details
 * Extracts port and pin from the combined rx_port_pin_t value and
 * validates both against RX72N hardware constraints. Matches validation
 * behavior of real GPIO HAL.
 *
 * Valid ports: 0-5, A-E, J (matches RX72N GPIO ports)
 * Valid pins: 0-7 (all ports have 8 pins)
 *
 * @param[in] pin Combined port/pin value
 * @param[out] port Extracted port number
 * @param[out] pin_num Extracted pin number (0-7)
 *
 * @return Error code
 * @retval k_rx_ok Pin is valid
 * @retval k_rx_err_gpio_invalid_port Port not in valid set
 * @retval k_rx_err_gpio_invalid_pin Pin number > 7
 */
static rx_err_t internal_validate_pin(rx_port_pin_t pin, uint8_t* port, uint8_t* pin_num)
{
  *port    = rx_port_from_pin(pin);
  *pin_num = rx_pin_from_pin(pin);

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
 * @brief Get pointer to pin state in global mock state
 *
 * @details
 * Returns pointer to the state structure for the specified port/pin.
 * Used by all GPIO HAL functions to access/modify pin state.
 *
 * @param[in] port Port number (0-19, unchecked)
 * @param[in] pin Pin number (0-7, unchecked)
 *
 * @return Pointer to pin state, or nullptr if out of bounds
 *
 * @note Does not validate port/pin (caller must validate first)
 */
static mock_gpio_pin_state_t* internal_get_pin_state(uint8_t port, uint8_t pin)
{
  if (port >= k_mock_gpio_max_ports || pin >= k_mock_gpio_pins_per_port) {
    return nullptr;
  }
  return &g_mock_gpio.pins[port][pin];
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

void mock_gpio_init(void)
{
  {
    uint8_t* raw = (uint8_t*)&g_mock_gpio;
    for (size_t i = 0; i < sizeof(g_mock_gpio); i++) {
      raw[i] = 0;
    }
  }
}

void mock_gpio_reset(void)
{
  mock_gpio_init();
}

/* =============================================================================
 * Test Setup Functions
 * =============================================================================
 */

void mock_gpio_set_input_value(rx_port_pin_t pin, bool value)
{
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != nullptr) {
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

void mock_gpio_set_error_on_nth_call(uint16_t call_index, rx_err_t err)
{
  g_mock_gpio.error_call_index = call_index;
  g_mock_gpio.nth_error        = err;
}

/* =============================================================================
 * State Inspection Functions
 * =============================================================================
 */

mock_gpio_direction_t mock_gpio_get_direction(rx_port_pin_t pin)
{
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != nullptr) {
    return state->direction;
  }
  return k_mock_gpio_dir_undefined;
}

bool mock_gpio_get_output_value(rx_port_pin_t pin)
{
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != nullptr) {
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
  return nullptr;
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

rx_err_t gpio_set_output(rx_port_pin_t pin)
{
  internal_record_call(k_mock_gpio_call_set_output, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port    = 0;
  uint8_t pin_num = 0;
  err             = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != nullptr) {
    state->direction = k_mock_gpio_dir_output;
  }

  return k_rx_ok;
}

rx_err_t gpio_set_input(rx_port_pin_t pin)
{
  internal_record_call(k_mock_gpio_call_set_input, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port    = 0;
  uint8_t pin_num = 0;
  err             = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != nullptr) {
    state->direction = k_mock_gpio_dir_input;
  }

  return k_rx_ok;
}

rx_err_t gpio_write_high(rx_port_pin_t pin)
{
  internal_record_call(k_mock_gpio_call_write_high, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port    = 0;
  uint8_t pin_num = 0;
  err             = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != nullptr) {
    state->output_value = true;
  }

  return k_rx_ok;
}

rx_err_t gpio_write_low(rx_port_pin_t pin)
{
  internal_record_call(k_mock_gpio_call_write_low, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port    = 0;
  uint8_t pin_num = 0;
  err             = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != nullptr) {
    state->output_value = false;
  }

  return k_rx_ok;
}

rx_err_t gpio_toggle(rx_port_pin_t pin)
{
  internal_record_call(k_mock_gpio_call_toggle, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t port    = 0;
  uint8_t pin_num = 0;
  err             = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != nullptr) {
    state->output_value = (bool)(!state->output_value);
    /* Keep input_value in sync so gpio_read reflects the toggled state */
    state->input_value = state->output_value;
  }

  return k_rx_ok;
}

rx_err_t gpio_read(rx_port_pin_t pin, bool* value)
{
  internal_record_call(k_mock_gpio_call_read, pin);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (value == nullptr) {
    return k_rx_err_null_ptr;
  }

  uint8_t port    = 0;
  uint8_t pin_num = 0;
  err             = internal_validate_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  mock_gpio_pin_state_t* state = internal_get_pin_state(port, pin_num);
  if (state != nullptr) {
    *value = state->input_value;
  }

  return k_rx_ok;
}
