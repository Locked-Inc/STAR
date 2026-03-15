/**
 * @file mock_hcsr04_hw.c
 * @brief Mock HC-SR04 Hardware Layer Implementation
 *
 * @details
 * Implementation of mock GPIO and timing functions for HC-SR04 driver testing.
 * Simulates hardware behavior including trigger pulse detection, echo timing,
 * and error injection for comprehensive unit testing without real hardware.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_hcsr04_hw.h"

#include <string.h>

#include "rx_hcsr04.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief Microseconds per centimeter (roundtrip) at 20C
 */
typedef enum : uint8_t {
  k_us_per_cm = 58, /**< 58us per cm roundtrip */
} echo_constants_t;

/**
 * @brief Default simulated distance for testing
 */
typedef enum : uint8_t {
  k_default_test_distance_cm = 10, /**< 10cm default distance */
  k_default_time_step_us     = 1,  /**< 1us default time step */
} mock_defaults_t;

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

mock_hcsr04_hw_t g_mock_hcsr04_hw;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Get mock instance (global if NULL)
 *
 * @param[in] mock Mock instance pointer (NULL = use global)
 *
 * @return Pointer to mock instance (either provided or global)
 */
static mock_hcsr04_hw_t* internal_get_mock(mock_hcsr04_hw_t* mock)
{
  return (mock != nullptr) ? mock : &g_mock_hcsr04_hw;
}

/**
 * @brief Record a function call in history
 *
 * @param[in] mock     Mock instance pointer (NULL = use global)
 * @param[in] function Function name to record
 * @param[in] port     Port number argument
 * @param[in] pin      Pin number argument
 * @param[in] value    Boolean value argument (for read/write)
 * @param[in] ret      Return value to record
 */
static void internal_record_call(mock_hcsr04_hw_t* mock,
                                 const char*       function,
                                 uint8_t           port,
                                 uint8_t           pin,
                                 bool              value,
                                 rx_err_t          ret)
{
  mock_hcsr04_hw_t* m = internal_get_mock(mock);

  if (m->call_count < k_mock_hcsr04_max_calls) {
    mock_hcsr04_call_t* call = &m->call_history[m->call_count];
    {
      uint8_t ci = 0;
      for (; ci < k_mock_hcsr04_func_name_max - 1 && function[ci] != '\0'; ci++) {
        call->function[ci] = function[ci];
      }
      call->function[ci] = '\0';
    }
    call->port         = port;
    call->pin          = pin;
    call->value        = value;
    call->return_value = ret;
    m->call_count++;
  }
}

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_hcsr04_hw_init(mock_hcsr04_hw_t* mock)
{
  mock_hcsr04_hw_t* m = internal_get_mock(mock);
  {
    uint8_t* raw = (uint8_t*)m;
    for (size_t bi = 0; bi < sizeof(mock_hcsr04_hw_t); bi++) {
      raw[bi] = 0;
    }
  }
  m->initialized       = true;
  m->simulated_echo_us = k_default_test_distance_cm * k_us_per_cm;
  m->auto_advance_time = true;
  m->time_step_us      = k_default_time_step_us;
}

void mock_hcsr04_hw_deinit(mock_hcsr04_hw_t* mock)
{
  mock_hcsr04_hw_t* m = internal_get_mock(mock);
  m->initialized      = false;
}

void mock_hcsr04_hw_reset(mock_hcsr04_hw_t* mock)
{
  mock_hcsr04_hw_t* m = internal_get_mock(mock);

  /* Keep error injection settings, reset everything else */
  bool     inject_timeout      = m->inject_timeout;
  bool     inject_gpio_error   = m->inject_gpio_error;
  bool     inject_pin_conflict = m->inject_pin_conflict;
  uint32_t echo_us             = m->simulated_echo_us;

  {
    uint8_t* raw = (uint8_t*)m;
    for (size_t bi = 0; bi < sizeof(mock_hcsr04_hw_t); bi++) {
      raw[bi] = 0;
    }
  }

  m->initialized         = true;
  m->inject_timeout      = inject_timeout;
  m->inject_gpio_error   = inject_gpio_error;
  m->inject_pin_conflict = inject_pin_conflict;
  m->simulated_echo_us   = echo_us;
  m->auto_advance_time   = true;
  m->time_step_us        = k_default_time_step_us;
}

/* =============================================================================
 * Echo Simulation Functions
 * =============================================================================
 */

void mock_hcsr04_hw_set_echo_time(mock_hcsr04_hw_t* mock, uint32_t echo_us)
{
  mock_hcsr04_hw_t* m  = internal_get_mock(mock);
  m->simulated_echo_us = echo_us;
}

void mock_hcsr04_hw_set_distance(mock_hcsr04_hw_t* mock, float distance_cm)
{
  mock_hcsr04_hw_t* m  = internal_get_mock(mock);
  m->simulated_echo_us = (uint32_t)(distance_cm * (float)k_us_per_cm);
}

void mock_hcsr04_hw_set_timeout(mock_hcsr04_hw_t* mock, bool timeout)
{
  mock_hcsr04_hw_t* m = internal_get_mock(mock);
  m->inject_timeout   = timeout;
}

/* =============================================================================
 * Error Injection Functions
 * =============================================================================
 */

void mock_hcsr04_hw_set_gpio_error(mock_hcsr04_hw_t* mock, bool error)
{
  mock_hcsr04_hw_t* m  = internal_get_mock(mock);
  m->inject_gpio_error = error;
}

void mock_hcsr04_hw_set_gpio_input_error(mock_hcsr04_hw_t* mock, bool error)
{
  mock_hcsr04_hw_t* m        = internal_get_mock(mock);
  m->inject_gpio_input_error = error;
}

void mock_hcsr04_hw_set_gpio_read_error(mock_hcsr04_hw_t* mock, bool error)
{
  mock_hcsr04_hw_t* m       = internal_get_mock(mock);
  m->inject_gpio_read_error = error;
}

void mock_hcsr04_hw_set_gpio_write_error(mock_hcsr04_hw_t* mock, bool error)
{
  mock_hcsr04_hw_t* m        = internal_get_mock(mock);
  m->inject_gpio_write_error = error;
}

void mock_hcsr04_hw_set_gpio_write_high_error(mock_hcsr04_hw_t* mock, bool error)
{
  mock_hcsr04_hw_t* m             = internal_get_mock(mock);
  m->inject_gpio_write_high_error = error;
}

void mock_hcsr04_hw_set_gpio_write_low_fail_after(mock_hcsr04_hw_t* mock, uint32_t count)
{
  mock_hcsr04_hw_t* m                = internal_get_mock(mock);
  m->gpio_write_low_fail_after_count = count;
}

void mock_hcsr04_hw_set_gpio_deinit_error(mock_hcsr04_hw_t* mock, bool error)
{
  mock_hcsr04_hw_t* m         = internal_get_mock(mock);
  m->inject_gpio_deinit_error = error;
}

void mock_hcsr04_hw_set_gpio_deinit_fail_after(mock_hcsr04_hw_t* mock, uint32_t count)
{
  mock_hcsr04_hw_t* m             = internal_get_mock(mock);
  m->gpio_deinit_fail_after_count = count;
}

void mock_hcsr04_hw_set_pin_conflict(mock_hcsr04_hw_t* mock, bool conflict)
{
  mock_hcsr04_hw_t* m    = internal_get_mock(mock);
  m->inject_pin_conflict = conflict;
}

/* =============================================================================
 * Timing Simulation Functions
 * =============================================================================
 */

void mock_hcsr04_hw_set_time(mock_hcsr04_hw_t* mock, uint32_t time_us)
{
  mock_hcsr04_hw_t* m = internal_get_mock(mock);
  m->current_time_us  = time_us;
}

void mock_hcsr04_hw_advance_time(mock_hcsr04_hw_t* mock, uint32_t delta_us)
{
  mock_hcsr04_hw_t* m = internal_get_mock(mock);
  m->current_time_us += delta_us;
}

void mock_hcsr04_hw_set_auto_advance(mock_hcsr04_hw_t* mock, bool enable, uint32_t step_us)
{
  mock_hcsr04_hw_t* m  = internal_get_mock(mock);
  m->auto_advance_time = enable;
  m->time_step_us      = step_us;
}

/* =============================================================================
 * Call Tracking Functions
 * =============================================================================
 */

bool mock_hcsr04_hw_was_called(mock_hcsr04_hw_t* mock, const char* function_name)
{
  mock_hcsr04_hw_t* m = internal_get_mock(mock);

  for (uint32_t i = 0; i < m->call_count; i++) {
    if (strcmp(m->call_history[i].function, function_name) == 0) {
      return true;
    }
  }
  return false;
}

uint32_t mock_hcsr04_hw_get_call_count(mock_hcsr04_hw_t* mock, const char* function_name)
{
  mock_hcsr04_hw_t* m     = internal_get_mock(mock);
  uint32_t          count = 0;

  for (uint32_t i = 0; i < m->call_count; i++) {
    if (strcmp(m->call_history[i].function, function_name) == 0) {
      count++;
    }
  }
  return count;
}

uint32_t mock_hcsr04_hw_get_trigger_count(mock_hcsr04_hw_t* mock)
{
  mock_hcsr04_hw_t* m = internal_get_mock(mock);
  return m->trigger_pulse_count;
}

/* =============================================================================
 * Mock GPIO Functions
 * =============================================================================
 */

rx_err_t mock_gpio_set_output(uint8_t port, uint8_t pin)
{
  mock_hcsr04_hw_t* m   = internal_get_mock(nullptr);
  rx_err_t          ret = k_rx_ok;

  if (m->inject_gpio_error) {
    ret = k_rx_err_hw_init_failed;
  } else if (m->inject_pin_conflict) {
    ret = k_rx_err_gpio_conflict;
  }

  internal_record_call(nullptr, "gpio_set_output", port, pin, false, ret);
  m->gpio_set_output_count++;

  return ret;
}

rx_err_t mock_gpio_set_input(uint8_t port, uint8_t pin)
{
  mock_hcsr04_hw_t* m   = internal_get_mock(nullptr);
  rx_err_t          ret = k_rx_ok;

  if (m->inject_gpio_input_error) {
    ret = k_rx_err_hw_init_failed;
  } else if (m->inject_pin_conflict) {
    ret = k_rx_err_gpio_conflict;
  }

  internal_record_call(nullptr, "gpio_set_input", port, pin, false, ret);
  m->gpio_set_input_count++;

  return ret;
}

rx_err_t mock_gpio_write_high(uint8_t port, uint8_t pin)
{
  mock_hcsr04_hw_t* m   = internal_get_mock(nullptr);
  rx_err_t          ret = k_rx_ok;

  if (m->inject_gpio_write_high_error) {
    internal_record_call(nullptr, "gpio_write_high", port, pin, true, k_rx_err_hw_error);
    m->gpio_write_high_count++;
    return k_rx_err_hw_error;
  }

  m->trigger_pin_state = true;
  internal_record_call(nullptr, "gpio_write_high", port, pin, true, ret);
  m->gpio_write_high_count++;

  return ret;
}

rx_err_t mock_gpio_write_low(uint8_t port, uint8_t pin)
{
  mock_hcsr04_hw_t* m   = internal_get_mock(nullptr);
  rx_err_t          ret = k_rx_ok;

  if (m->inject_gpio_write_error) {
    ret = k_rx_err_hw_error;
  } else if (m->gpio_write_low_fail_after_count > 0) {
    m->gpio_write_low_fail_after_count--;
    if (m->gpio_write_low_fail_after_count == 0) {
      ret = k_rx_err_hw_error;
    }
  }

  /* Detect trigger pulse completion (high->low transition) */
  if (ret == k_rx_ok && m->trigger_pin_state) { // NOLINT(readability-implicit-bool-conversion)
    m->trigger_pulse_count++;
  }

  if (ret == k_rx_ok) {
    m->trigger_pin_state = false;
  }
  internal_record_call(nullptr, "gpio_write_low", port, pin, false, ret);
  m->gpio_write_low_count++;

  return ret;
}

rx_err_t mock_gpio_read(uint8_t port, uint8_t pin, bool* value)
{
  mock_hcsr04_hw_t* m = internal_get_mock(nullptr);

  if (value == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (m->inject_gpio_read_error) {
    internal_record_call(nullptr, "gpio_read", port, pin, false, k_rx_err_hw_error);
    m->gpio_read_count++;
    return k_rx_err_hw_error;
  }

  /* Simulate echo behavior based on timing */
  if (!m->inject_timeout && m->trigger_pulse_count > 0) {
    /*
     * After trigger pulse, simulate echo timing:
     * - Echo goes high shortly after trigger (~10us)
     * - Echo stays high for simulated_echo_us
     * - Echo goes low after that
     *
     * We simplify this for testing by using current_time_us
     * as a relative position in the echo sequence.
     */
    uint32_t       elapsed = m->current_time_us - m->last_trigger_time_us;
    const uint32_t echo_start =
      k_hcsr04_trigger_settle_us + k_hcsr04_trigger_pulse_us; /* Wait for trigger pulse to finish */
    const uint32_t echo_end = echo_start + m->simulated_echo_us;

    *value =
      (elapsed >= echo_start && elapsed < echo_end); // NOLINT(readability-implicit-bool-conversion)
  } else {
    /* Timeout injected or no trigger sent yet: echo is low */
    *value = false;
  }

  m->echo_pin_state = *value;
  internal_record_call(nullptr, "gpio_read", port, pin, *value, k_rx_ok);
  m->gpio_read_count++;

  /* Auto-advance time if enabled */
  if (m->auto_advance_time) {
    m->current_time_us += m->time_step_us;
  }

  return k_rx_ok;
}

rx_err_t mock_gpio_deinit(uint8_t port, uint8_t pin)
{
  mock_hcsr04_hw_t* m   = internal_get_mock(nullptr);
  rx_err_t          ret = k_rx_ok;

  if (m->inject_gpio_deinit_error || // NOLINT(readability-implicit-bool-conversion)
      (m->gpio_deinit_fail_after_count > 0U &&
       m->gpio_deinit_count >= m->gpio_deinit_fail_after_count)) {
    ret = k_rx_err_hw_error;
  }

  internal_record_call(nullptr, "gpio_deinit", port, pin, false, ret);
  m->gpio_deinit_count++;
  return ret;
}

/* =============================================================================
 * Mock Timing Functions
 * =============================================================================
 */

void mock_delay_us(uint32_t us)
{
  mock_hcsr04_hw_t* m = internal_get_mock(nullptr);
  m->current_time_us += us;
  m->total_delay_us += us;
}

uint32_t mock_get_time_us(void)
{
  mock_hcsr04_hw_t* m = internal_get_mock(nullptr);
  return m->current_time_us;
}
