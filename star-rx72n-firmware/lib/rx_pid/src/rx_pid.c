/* src/rx_pid.c */

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
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
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
    RX_LOG_WARN(s_tag, "PID already initialized");
    return RX_ERR_INVALID_STATE;
  }

  /* Validate configuration */
  if (config->output_max <= config->output_min) {
    RX_LOG_ERROR(s_tag, "output_max must be > output_min");
    return RX_ERR_INVALID_ARG;
  }

  if (config->integral_max <= config->integral_min) {
    RX_LOG_ERROR(s_tag, "integral_max must be > integral_min");
    return RX_ERR_INVALID_ARG;
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

  RX_LOG_INFO(s_tag, "PID initialized");

  return RX_OK;
}

rx_err_t rx_pid_deinit(rx_pid_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    RX_LOG_WARN(s_tag, "PID not initialized");
    return RX_ERR_INVALID_STATE;
  }

  /* Clear handle */
  memset(handle, 0, sizeof(rx_pid_handle_t));

  RX_LOG_INFO(s_tag, "PID deinitialized");

  return RX_OK;
}

rx_err_t
rx_pid_compute(rx_pid_handle_t* handle, float setpoint, float measured, float dt, float* output)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(output, s_tag, "output pointer is NULL");

  if (!handle->initialized) {
    RX_LOG_ERROR(s_tag, "PID not initialized");
    return RX_ERR_INVALID_STATE;
  }

  if (dt <= 0.0f) {
    RX_LOG_ERROR(s_tag, "dt must be > 0");
    return RX_ERR_INVALID_ARG;
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

  RX_LOG_DEBUG(s_tag, "PID computation");

  return RX_OK;
}

rx_err_t rx_pid_reset(rx_pid_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    RX_LOG_ERROR(s_tag, "PID not initialized");
    return RX_ERR_INVALID_STATE;
  }

  /* Clear internal state */
  handle->integral   = 0.0f;
  handle->prev_error = 0.0f;

  RX_LOG_DEBUG(s_tag, "PID state reset");

  return RX_OK;
}

rx_err_t rx_pid_set_gains(rx_pid_handle_t* handle, float kp, float ki, float kd)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    RX_LOG_ERROR(s_tag, "PID not initialized");
    return RX_ERR_INVALID_STATE;
  }

  handle->kp = kp;
  handle->ki = ki;
  handle->kd = kd;

  RX_LOG_INFO(s_tag, "PID gains updated");

  return RX_OK;
}

rx_err_t rx_pid_set_output_limits(rx_pid_handle_t* handle, float output_min, float output_max)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    RX_LOG_ERROR(s_tag, "PID not initialized");
    return RX_ERR_INVALID_STATE;
  }

  if (output_max <= output_min) {
    RX_LOG_ERROR(s_tag, "output_max must be > output_min");
    return RX_ERR_INVALID_ARG;
  }

  handle->output_min = output_min;
  handle->output_max = output_max;

  RX_LOG_INFO(s_tag, "PID output limits updated");

  return RX_OK;
}

rx_err_t rx_pid_set_integral_limits(rx_pid_handle_t* handle, float integral_min, float integral_max)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    RX_LOG_ERROR(s_tag, "PID not initialized");
    return RX_ERR_INVALID_STATE;
  }

  if (integral_max <= integral_min) {
    RX_LOG_ERROR(s_tag, "integral_max must be > integral_min");
    return RX_ERR_INVALID_ARG;
  }

  handle->integral_min = integral_min;
  handle->integral_max = integral_max;

  /* Clamp current integral to new limits */
  handle->integral = internal_clamp(handle->integral, integral_min, integral_max);

  RX_LOG_INFO(s_tag, "PID integral limits updated");

  return RX_OK;
}
