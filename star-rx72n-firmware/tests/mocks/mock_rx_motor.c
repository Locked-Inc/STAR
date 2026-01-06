/* tests/mocks/mock_rx_motor.c */

/**
 * @file mock_rx_motor.c
 * @brief Mock Motor Driver Implementation for Unit Testing
 * @details
 * Provides mock implementation of rx_motor driver for DRV8243 testing.
 * Records all operations for verification without actual hardware.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_motor.h"

#include <string.h>

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Motor speed limits */
typedef enum {
  k_mock_motor_duty_min = -100, /**< Minimum duty cycle (-100%) */
  k_mock_motor_duty_max = 100,  /**< Maximum duty cycle (+100%) */
} mock_motor_duty_limits_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static bool     s_initialized = false;
static float    s_duty        = 0.0f;
static bool     s_brake_mode  = false;
static bool     s_stopped     = false;
static rx_err_t s_init_error  = k_rx_ok;
static rx_err_t s_duty_error  = k_rx_ok;
static rx_err_t s_stop_error  = k_rx_ok;

/* =============================================================================
 * Mock Test Helpers
 * =============================================================================
 */

void mock_motor_reset(void)
{
  s_initialized = false;
  s_duty        = 0.0f;
  s_brake_mode  = false;
  s_stopped     = false;
  s_init_error  = k_rx_ok;
  s_duty_error  = k_rx_ok;
  s_stop_error  = k_rx_ok;
}

bool mock_motor_is_initialized(void)
{
  return s_initialized;
}

float mock_motor_get_duty(void)
{
  return s_duty;
}

bool mock_motor_get_brake_mode(void)
{
  return s_brake_mode;
}

bool mock_motor_is_stopped(void)
{
  return s_stopped;
}

void mock_motor_set_init_error(rx_err_t err)
{
  s_init_error = err;
}

void mock_motor_set_duty_error(rx_err_t err)
{
  s_duty_error = err;
}

void mock_motor_set_stop_error(rx_err_t err)
{
  s_stop_error = err;
}

/* =============================================================================
 * Motor Driver Mock Implementation
 * =============================================================================
 */

rx_err_t rx_motor_init(rx_motor_handle_t* handle, const rx_motor_config_t* config)
{
  if (handle == NULL || config == NULL) {
    return k_rx_err_null_pointer;
  }

  if (s_init_error != k_rx_ok) {
    return s_init_error;
  }

  memset(handle, 0, sizeof(rx_motor_handle_t));
  handle->channel      = config->channel;
  handle->output_a     = config->output_a;
  handle->output_b     = config->output_b;
  handle->pwm_freq_hz  = config->pwm_freq_hz;
  handle->invert_pwm   = config->invert_pwm;
  handle->current_duty = 0.0f;
  handle->initialized  = true;

  s_initialized = true;
  s_stopped     = false;

  return k_rx_ok;
}

rx_err_t rx_motor_deinit(rx_motor_handle_t* handle)
{
  if (handle == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  handle->initialized = false;
  s_initialized       = false;

  return k_rx_ok;
}

rx_err_t rx_motor_set_duty(rx_motor_handle_t* handle, float duty)
{
  if (handle == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_duty_error != k_rx_ok) {
    return s_duty_error;
  }

  /* Clamp duty cycle to valid range */
  if (duty > (float)k_mock_motor_duty_max) {
    duty = (float)k_mock_motor_duty_max;
  } else if (duty < (float)k_mock_motor_duty_min) {
    duty = (float)k_mock_motor_duty_min;
  }

  handle->current_duty = duty;
  s_duty               = duty;
  s_stopped            = false;

  return k_rx_ok;
}

rx_err_t rx_motor_stop(rx_motor_handle_t* handle, bool brake)
{
  if (handle == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_stop_error != k_rx_ok) {
    return s_stop_error;
  }

  handle->current_duty = 0.0f;
  s_duty               = 0.0f;
  s_brake_mode         = brake;
  s_stopped            = true;

  return k_rx_ok;
}

rx_err_t rx_motor_get_duty(const rx_motor_handle_t* handle, float* out_duty)
{
  if (handle == NULL || out_duty == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  *out_duty = handle->current_duty;
  return k_rx_ok;
}
