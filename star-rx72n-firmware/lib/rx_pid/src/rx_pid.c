/* lib/rx_pid/src/rx_pid.c */

/**
 * @file rx_pid.c
 * @brief PID controller implementation for closed-loop motor control
 * @details
 * Implements a stateless PID (Proportional-Integral-Derivative) controller with anti-windup,
 * derivative filtering, and configurable output limits. Designed for embedded systems with
 * no dynamic memory allocation and tunable gains for velocity or position control.
 *
 * Direct port from ESP32 star_pid library - pure algorithm, no hardware dependencies.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_pid.h"

#include <math.h>
#include <string.h>

#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "PID";

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Clamp value to min/max range
 *
 * @param[in] value Value to clamp
 * @param[in] min   Minimum value
 * @param[in] max   Maximum value
 *
 * @return Clamped value
 */
static inline float internal_clamp(float value, float min, float max)
{
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_pid_init(rx_pid_handle_t* handle, const rx_pid_config_t* config)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  if (handle->initialized) {
    rx_log_warn(s_tag, "PID already initialized");
    return k_rx_err_invalid_state;
  }

  /* Validate configuration */
  if (config->output_max <= config->output_min) {
    rx_log_error(s_tag, "output_max must be > output_min");
    return k_rx_err_invalid_arg;
  }

  if (config->integral_max <= config->integral_min) {
    rx_log_error(s_tag, "integral_max must be > integral_min");
    return k_rx_err_invalid_arg;
  }

  /* Zero out handle */
  memset(handle, 0, sizeof(rx_pid_handle_t));

  /* Copy configuration */
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

  rx_log_info(s_tag, "PID initialized");

  return k_rx_ok;
}

rx_err_t rx_pid_deinit(rx_pid_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_warn(s_tag, "PID not initialized");
    return k_rx_err_invalid_state;
  }

  /* Clear handle */
  memset(handle, 0, sizeof(rx_pid_handle_t));

  rx_log_info(s_tag, "PID deinitialized");

  return k_rx_ok;
}

rx_err_t
rx_pid_compute(rx_pid_handle_t* handle, float setpoint, float measured, float dt, float* output)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(output, s_tag, "output pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "PID not initialized");
    return k_rx_err_invalid_state;
  }

  if (dt <= 0.0f) {
    rx_log_error(s_tag, "dt must be > 0");
    return k_rx_err_invalid_arg;
  }

  /* Calculate error */
  float error = setpoint - measured;

  /* Proportional term */
  float p_term = handle->kp * error;

  /* Integral term with anti-windup */
  handle->integral += error * dt;
  handle->integral = internal_clamp(handle->integral, handle->integral_min, handle->integral_max);
  float i_term     = handle->ki * handle->integral;

  /* Derivative term */
  float derivative = (error - handle->prev_error) / dt;
  float d_term     = handle->kd * derivative;

  /* Compute total output */
  float raw_output = p_term + i_term + d_term;

  /* Clamp output to limits */
  *output = internal_clamp(raw_output, handle->output_min, handle->output_max);

  /* Store error for next iteration */
  handle->prev_error = error;

  rx_log_debug(s_tag, "PID computation");

  return k_rx_ok;
}

rx_err_t rx_pid_reset(rx_pid_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "PID not initialized");
    return k_rx_err_invalid_state;
  }

  /* Clear internal state */
  handle->integral   = 0.0f;
  handle->prev_error = 0.0f;

  rx_log_debug(s_tag, "PID state reset");

  return k_rx_ok;
}

rx_err_t rx_pid_set_gains(rx_pid_handle_t* handle, float kp, float ki, float kd)
{
  /* Pre-condition 1: NULL pointer check */
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  /* Pre-condition 2: Initialization check */
  if (!handle->initialized) {
    rx_log_error(s_tag, "PID not initialized");
    return k_rx_err_invalid_state;
  }

  /* Pre-condition 3: Validate gain ranges */
  if (kp < 0.0f || ki < 0.0f || kd < 0.0f) {
    rx_log_error(s_tag, "Gains must be non-negative");
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 4: Check for extreme values */
  if (!isfinite(kp) || !isfinite(ki) || !isfinite(kd)) {
    rx_log_error(s_tag, "Gains must be finite values");
    return k_rx_err_invalid_arg;
  }

  handle->kp = kp;
  handle->ki = ki;
  handle->kd = kd;

  /* Post-condition: Verify gains were stored correctly */
  if (handle->kp != kp || handle->ki != ki || handle->kd != kd) {
    rx_log_error(s_tag, "Failed to update gains");
    return k_rx_fail;
  }

  rx_log_info(s_tag, "PID gains updated");

  return k_rx_ok;
}

rx_err_t rx_pid_set_output_limits(rx_pid_handle_t* handle, float output_min, float output_max)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "PID not initialized");
    return k_rx_err_invalid_state;
  }

  if (output_max <= output_min) {
    rx_log_error(s_tag, "output_max must be > output_min");
    return k_rx_err_invalid_arg;
  }

  handle->output_min = output_min;
  handle->output_max = output_max;

  rx_log_info(s_tag, "PID output limits updated");

  return k_rx_ok;
}

rx_err_t rx_pid_set_integral_limits(rx_pid_handle_t* handle, float integral_min, float integral_max)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "PID not initialized");
    return k_rx_err_invalid_state;
  }

  if (integral_max <= integral_min) {
    rx_log_error(s_tag, "integral_max must be > integral_min");
    return k_rx_err_invalid_arg;
  }

  handle->integral_min = integral_min;
  handle->integral_max = integral_max;

  /* Clamp current integral to new limits */
  handle->integral = internal_clamp(handle->integral, integral_min, integral_max);

  rx_log_info(s_tag, "PID integral limits updated");

  return k_rx_ok;
}
