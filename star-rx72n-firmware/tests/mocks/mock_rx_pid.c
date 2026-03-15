/**
 * @file mock_rx_pid.c
 * @brief Mock PID Controller Implementation
 *
 * @details
 * Implements mock rx_pid functions for unit testing.
 * Tracks calls, stores parameters, and allows configurable return values.
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_pid.h"

#include <stddef.h>
#include <stdint.h>

/* =============================================================================
 * Static Variables - Return Values
 * =============================================================================
 */

/** @brief Return value for rx_pid_init() */
static rx_err_t s_init_return = k_rx_ok;

/** @brief Return value for rx_pid_deinit() */
static rx_err_t s_deinit_return = k_rx_ok;

/** @brief Return value for rx_pid_compute() */
static rx_err_t s_compute_return = k_rx_ok;

/** @brief Return value for rx_pid_reset() */
static rx_err_t s_reset_return = k_rx_ok;

/** @brief Return value for rx_pid_set_gains() */
static rx_err_t s_set_gains_return = k_rx_ok;

/** @brief Return value for rx_pid_set_output_limits() */
static rx_err_t s_set_output_limits_return = k_rx_ok;

/** @brief Return value for rx_pid_set_integral_limits() */
static rx_err_t s_set_integral_limits_return = k_rx_ok;

/* =============================================================================
 * Static Variables - Call Counts
 * =============================================================================
 */

/** @brief Call count for rx_pid_init() */
static uint32_t s_init_count = 0;

/** @brief Call count for rx_pid_deinit() */
static uint32_t s_deinit_count = 0;

/** @brief Call count for rx_pid_compute() */
static uint32_t s_compute_count = 0;

/** @brief Call count for rx_pid_reset() */
static uint32_t s_reset_count = 0;

/** @brief Call count for rx_pid_set_gains() */
static uint32_t s_set_gains_count = 0;

/** @brief Call count for rx_pid_set_output_limits() */
static uint32_t s_set_output_limits_count = 0;

/** @brief Call count for rx_pid_set_integral_limits() */
static uint32_t s_set_integral_limits_count = 0;

/* =============================================================================
 * Static Variables - Compute Parameters
 * =============================================================================
 */

/** @brief Configured output value for rx_pid_compute() */
static float s_compute_output = 0.0f;

/** @brief Last setpoint passed to rx_pid_compute() */
static float s_last_setpoint = 0.0f;

/** @brief Last measured value passed to rx_pid_compute() */
static float s_last_measured = 0.0f;

/** @brief Last dt passed to rx_pid_compute() */
static float s_last_dt = 0.0f;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_pid_reset(void)
{
  /* Reset return values to defaults */
  s_init_return                = k_rx_ok;
  s_deinit_return              = k_rx_ok;
  s_compute_return             = k_rx_ok;
  s_reset_return               = k_rx_ok;
  s_set_gains_return           = k_rx_ok;
  s_set_output_limits_return   = k_rx_ok;
  s_set_integral_limits_return = k_rx_ok;

  /* Reset call counts */
  s_init_count                = 0;
  s_deinit_count              = 0;
  s_compute_count             = 0;
  s_reset_count               = 0;
  s_set_gains_count           = 0;
  s_set_output_limits_count   = 0;
  s_set_integral_limits_count = 0;

  /* Reset compute parameters */
  s_compute_output = 0.0f;
  s_last_setpoint  = 0.0f;
  s_last_measured  = 0.0f;
  s_last_dt        = 0.0f;
}

void mock_pid_set_init_return(rx_err_t err)
{
  s_init_return = err;
}

void mock_pid_set_deinit_return(rx_err_t err)
{
  s_deinit_return = err;
}

void mock_pid_set_compute_return(rx_err_t err)
{
  s_compute_return = err;
}

void mock_pid_set_compute_output(float output)
{
  s_compute_output = output;
}

void mock_pid_set_reset_return(rx_err_t err)
{
  s_reset_return = err;
}

void mock_pid_set_gains_return(rx_err_t err)
{
  s_set_gains_return = err;
}

void mock_pid_set_output_limits_return(rx_err_t err)
{
  s_set_output_limits_return = err;
}

void mock_pid_set_integral_limits_return(rx_err_t err)
{
  s_set_integral_limits_return = err;
}

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

uint32_t mock_pid_get_init_count(void)
{
  return s_init_count;
}

uint32_t mock_pid_get_deinit_count(void)
{
  return s_deinit_count;
}

uint32_t mock_pid_get_compute_count(void)
{
  return s_compute_count;
}

uint32_t mock_pid_get_reset_count(void)
{
  return s_reset_count;
}

uint32_t mock_pid_get_set_gains_count(void)
{
  return s_set_gains_count;
}

float mock_pid_get_last_setpoint(void)
{
  return s_last_setpoint;
}

float mock_pid_get_last_measured(void)
{
  return s_last_measured;
}

float mock_pid_get_last_dt(void)
{
  return s_last_dt;
}

bool mock_pid_was_initialized(void)
{
  return s_init_count > 0;
}

void mock_pid_get_last_compute_params(float* setpoint, float* measured, float* dt)
{
  if (setpoint != nullptr) {
    *setpoint = s_last_setpoint;
  }
  if (measured != nullptr) {
    *measured = s_last_measured;
  }
  if (dt != nullptr) {
    *dt = s_last_dt;
  }
}

/* =============================================================================
 * Mock Implementation
 * =============================================================================
 */

rx_err_t rx_pid_init(rx_pid_handle_t* handle, const rx_pid_config_t* config)
{
  s_init_count++;

  if (handle == nullptr || config == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_init_return == k_rx_ok) {
    /* Copy configuration to handle */
    handle->kp           = config->kp;
    handle->ki           = config->ki;
    handle->kd           = config->kd;
    handle->output_min   = config->output_min;
    handle->output_max   = config->output_max;
    handle->integral_min = config->integral_min;
    handle->integral_max = config->integral_max;
    handle->integral     = 0.0f;
    handle->prev_error   = 0.0f;
    handle->initialized  = true;
  }

  return s_init_return;
}

rx_err_t rx_pid_deinit(rx_pid_handle_t* handle)
{
  s_deinit_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_deinit_return == k_rx_ok) {
    {
      uint8_t* p = (uint8_t*)handle;
      for (size_t i = 0; i < sizeof(*handle); i++) {
        p[i] = 0;
      }
    }
    handle->initialized = false;
  }

  return s_deinit_return;
}

rx_err_t
rx_pid_compute(rx_pid_handle_t* handle, float setpoint, float measured, float dt, float* output)
{
  s_compute_count++;

  /* Store parameters for test verification */
  s_last_setpoint = setpoint;
  s_last_measured = measured;
  s_last_dt       = dt;

  if (handle == nullptr || output == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_compute_return == k_rx_ok) {
    *output = s_compute_output;
  }

  return s_compute_return;
}

rx_err_t rx_pid_reset(rx_pid_handle_t* handle)
{
  s_reset_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_reset_return == k_rx_ok) {
    handle->integral   = 0.0f;
    handle->prev_error = 0.0f;
  }

  return s_reset_return;
}

rx_err_t rx_pid_set_gains(rx_pid_handle_t* handle, const float kp, const float ki, const float kd)
{
  s_set_gains_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_set_gains_return == k_rx_ok) {
    handle->kp = kp;
    handle->ki = ki;
    handle->kd = kd;
  }

  return s_set_gains_return;
}

rx_err_t rx_pid_set_output_limits(rx_pid_handle_t* handle, float output_min, float output_max)
{
  s_set_output_limits_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_set_output_limits_return == k_rx_ok) {
    handle->output_min = output_min;
    handle->output_max = output_max;
  }

  return s_set_output_limits_return;
}

rx_err_t rx_pid_set_integral_limits(rx_pid_handle_t* handle, float integral_min, float integral_max)
{
  s_set_integral_limits_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_set_integral_limits_return == k_rx_ok) {
    handle->integral_min = integral_min;
    handle->integral_max = integral_max;
  }

  return s_set_integral_limits_return;
}
