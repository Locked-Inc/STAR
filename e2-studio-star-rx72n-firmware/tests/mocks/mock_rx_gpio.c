/* tests/mocks/mock_rx_gpio.c */

/**
 * @file mock_rx_gpio.c
 * @brief Mock GPIO Implementation for Host-Side Testing
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_gpio.h"

#include <string.h>

/* =============================================================================
 * Mock State
 * =============================================================================
 */

/**
 * @brief Mock GPIO read sequence state
 */
typedef struct {
  bool     values[k_mock_gpio_max_read_sequence]; /**< Programmed read values */
  uint32_t count;                                 /**< Total values in sequence */
  uint32_t index;                                 /**< Next value to return */
  bool     active;                                /**< True if sequence is active */
} mock_read_sequence_t;

/**
 * @brief Mock GPIO pin state
 */
typedef struct {
  bool is_output;    /**< True if configured as output */
  bool output_value; /**< Current output value (if output) */
  bool read_value;   /**< Value to return on read */
} mock_pin_state_t;

/**
 * @brief Mock GPIO global state
 */
typedef struct {
  mock_pin_state_t     pins[k_mock_gpio_max_pins];
  mock_read_sequence_t read_seq;     /**< Single read sequence (one pin at a time) */
  rx_port_pin_t        read_seq_pin; /**< Pin the read sequence is assigned to */
  rx_err_t             next_error;
  uint32_t             write_low_count;
  uint32_t             write_high_count;
  uint32_t             set_output_count;
  uint32_t             set_input_count;
  uint32_t             read_count;
  bool                 initialized;
} mock_gpio_state_t;

static mock_gpio_state_t s_mock_gpio;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Convert rx_port_pin_t to array index
 */
static uint32_t internal_pin_to_index(rx_port_pin_t pin)
{
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  return (uint32_t)((port << k_port_shift) | pin_num);
}

/* =============================================================================
 * Mock State Control
 * =============================================================================
 */

void mock_gpio_init(void)
{
  memset(&s_mock_gpio, 0, sizeof(s_mock_gpio));
  s_mock_gpio.initialized = true;

  /* Default all pins to high (external pull-up for OneWire) */
  for (uint32_t pin_idx = 0; pin_idx < k_mock_gpio_max_pins; ++pin_idx) {
    s_mock_gpio.pins[pin_idx].read_value = true;
  }
}

void mock_gpio_deinit(void)
{
  memset(&s_mock_gpio, 0, sizeof(s_mock_gpio));
}

void mock_gpio_set_read_value(rx_port_pin_t pin, bool high)
{
  uint32_t idx = internal_pin_to_index(pin);

  if (idx < k_mock_gpio_max_pins) {
    s_mock_gpio.pins[idx].read_value = high;
  }
}

bool mock_gpio_get_written_value(rx_port_pin_t pin)
{
  uint32_t idx = internal_pin_to_index(pin);

  if (idx < k_mock_gpio_max_pins) {
    return s_mock_gpio.pins[idx].output_value;
  }
  return false;
}

bool mock_gpio_is_output(rx_port_pin_t pin)
{
  uint32_t idx = internal_pin_to_index(pin);

  if (idx < k_mock_gpio_max_pins) {
    return s_mock_gpio.pins[idx].is_output;
  }
  return false;
}

void mock_gpio_set_next_error(rx_err_t err)
{
  s_mock_gpio.next_error = err;
}

uint32_t mock_gpio_get_write_low_count(void)
{
  return s_mock_gpio.write_low_count;
}

uint32_t mock_gpio_get_write_high_count(void)
{
  return s_mock_gpio.write_high_count;
}

uint32_t mock_gpio_get_set_output_count(void)
{
  return s_mock_gpio.set_output_count;
}

uint32_t mock_gpio_get_set_input_count(void)
{
  return s_mock_gpio.set_input_count;
}

uint32_t mock_gpio_get_read_count(void)
{
  return s_mock_gpio.read_count;
}

void mock_gpio_reset_counters(void)
{
  s_mock_gpio.write_low_count  = 0;
  s_mock_gpio.write_high_count = 0;
  s_mock_gpio.set_output_count = 0;
  s_mock_gpio.set_input_count  = 0;
  s_mock_gpio.read_count       = 0;
}

void mock_gpio_set_read_sequence(rx_port_pin_t pin, const bool* values, uint32_t count)
{
  if (values == nullptr || count == 0) {
    s_mock_gpio.read_seq.active = false;
    return;
  }

  uint32_t copy_count =
    (count < k_mock_gpio_max_read_sequence) ? count : k_mock_gpio_max_read_sequence;
  for (uint32_t i = 0; i < copy_count; ++i) {
    s_mock_gpio.read_seq.values[i] = values[i];
  }

  s_mock_gpio.read_seq.count  = copy_count;
  s_mock_gpio.read_seq.index  = 0;
  s_mock_gpio.read_seq.active = true;
  s_mock_gpio.read_seq_pin    = pin;
}

void mock_gpio_clear_read_sequence(rx_port_pin_t pin)
{
  (void)pin;
  s_mock_gpio.read_seq.active = false;
  s_mock_gpio.read_seq.index  = 0;
  s_mock_gpio.read_seq.count  = 0;
}

/* =============================================================================
 * GPIO Function Implementations
 * =============================================================================
 */

rx_err_t gpio_set_output(rx_port_pin_t pin)
{
  s_mock_gpio.set_output_count++;

  if (s_mock_gpio.next_error != k_rx_ok) {
    rx_err_t err           = s_mock_gpio.next_error;
    s_mock_gpio.next_error = k_rx_ok;
    return err;
  }

  uint32_t idx = internal_pin_to_index(pin);
  if (idx >= k_mock_gpio_max_pins) {
    return k_rx_err_out_of_range;
  }

  s_mock_gpio.pins[idx].is_output = true;

  return k_rx_ok;
}

rx_err_t gpio_set_input(rx_port_pin_t pin)
{
  s_mock_gpio.set_input_count++;

  if (s_mock_gpio.next_error != k_rx_ok) {
    rx_err_t err           = s_mock_gpio.next_error;
    s_mock_gpio.next_error = k_rx_ok;
    return err;
  }

  uint32_t idx = internal_pin_to_index(pin);
  if (idx >= k_mock_gpio_max_pins) {
    return k_rx_err_out_of_range;
  }

  s_mock_gpio.pins[idx].is_output = false;

  return k_rx_ok;
}

rx_err_t gpio_write_low(rx_port_pin_t pin)
{
  s_mock_gpio.write_low_count++;

  if (s_mock_gpio.next_error != k_rx_ok) {
    rx_err_t err           = s_mock_gpio.next_error;
    s_mock_gpio.next_error = k_rx_ok;
    return err;
  }

  uint32_t idx = internal_pin_to_index(pin);
  if (idx >= k_mock_gpio_max_pins) {
    return k_rx_err_out_of_range;
  }

  s_mock_gpio.pins[idx].output_value = false;

  return k_rx_ok;
}

rx_err_t gpio_write_high(rx_port_pin_t pin)
{
  s_mock_gpio.write_high_count++;

  if (s_mock_gpio.next_error != k_rx_ok) {
    rx_err_t err           = s_mock_gpio.next_error;
    s_mock_gpio.next_error = k_rx_ok;
    return err;
  }

  uint32_t idx = internal_pin_to_index(pin);
  if (idx >= k_mock_gpio_max_pins) {
    return k_rx_err_out_of_range;
  }

  s_mock_gpio.pins[idx].output_value = true;

  return k_rx_ok;
}

rx_err_t gpio_read(rx_port_pin_t pin, bool* high)
{
  s_mock_gpio.read_count++;

  /* Pre-condition: Validate output pointer */
  if (high == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Check for injected error */
  if (s_mock_gpio.next_error != k_rx_ok) {
    rx_err_t err           = s_mock_gpio.next_error;
    s_mock_gpio.next_error = k_rx_ok;
    return err;
  }

  /* Bounds validation: Verify pin index is within valid range */
  uint32_t idx = internal_pin_to_index(pin);
  if (idx >= k_mock_gpio_max_pins) {
    return k_rx_err_out_of_range;
  }

  /* Check for active read sequence on this pin */
  if (s_mock_gpio.read_seq.active && pin == s_mock_gpio.read_seq_pin &&
      s_mock_gpio.read_seq.index < s_mock_gpio.read_seq.count) {
    *high = s_mock_gpio.read_seq.values[s_mock_gpio.read_seq.index];
    s_mock_gpio.read_seq.index++;
    return k_rx_ok;
  }

  *high = s_mock_gpio.pins[idx].read_value;
  return k_rx_ok;
}
