/**
 * @file bb_pid.c
 * @brief PID controller implementation for BeagleBone Blue motor control.
 *
 * @details
 * Parallel-form discrete PID with anti-windup clamping. Ported from
 * star-rx72n-firmware/libs/rx_pid/ with bb_ prefix and bb_err_t return types.
 * Pure algorithm -- zero hardware dependencies, zero dynamic allocation.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#include "bb_pid.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/**
 * @brief Clamp a float to [min, max].
 *
 * @param[in] value Value to clamp
 * @param[in] min   Lower bound
 * @param[in] max   Upper bound
 *
 * @return Clamped value
 *
 * @pre min <= max
 * @post Return value is in the range [min, max]
 *
 * @since Version 1.0.0
 */
static inline float internal_clamp(const float value, const float min,
                                   const float max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

bb_err_t bb_pid_init(bb_pid_handle_t* handle, const bb_pid_config_t* config)
{
    if (handle == NULL || config == NULL) {
        return k_bb_err_null_ptr;
    }

    if (handle->initialized) {
        return k_bb_err_already_initialized;
    }

    if (config->output_max <= config->output_min) {
        return k_bb_err_invalid_arg;
    }

    if (config->integral_max <= config->integral_min) {
        return k_bb_err_invalid_arg;
    }

    handle->kp           = config->kp;
    handle->ki           = config->ki;
    handle->kd           = config->kd;
    handle->output_min   = config->output_min;
    handle->output_max   = config->output_max;
    handle->integral_min = config->integral_min;
    handle->integral_max = config->integral_max;
    handle->integral     = 0.0F;
    handle->prev_error   = 0.0F;
    handle->initialized  = true;

    return k_bb_err_ok;
}

bb_err_t bb_pid_compute(bb_pid_handle_t* handle, const float setpoint,
                        const float measured, const float dt, float* output)
{
    if (handle == NULL || output == NULL) {
        return k_bb_err_null_ptr;
    }

    if (!handle->initialized) {
        return k_bb_err_not_initialized;
    }

    if (dt <= 0.0F) {
        return k_bb_err_invalid_arg;
    }

    /* Error */
    const float error = setpoint - measured;

    /* Proportional */
    const float p_term = handle->kp * error;

    /* Integral with anti-windup */
    handle->integral += error * dt;
    handle->integral = internal_clamp(handle->integral, handle->integral_min,
                                      handle->integral_max);
    const float i_term = handle->ki * handle->integral;

    /* Derivative */
    const float derivative = (error - handle->prev_error) / dt;
    const float d_term = handle->kd * derivative;

    /* Total output, clamped */
    const float raw_output = p_term + i_term + d_term;
    *output = internal_clamp(raw_output, handle->output_min, handle->output_max);

    /* Store for next iteration */
    handle->prev_error = error;

    return k_bb_err_ok;
}

bb_err_t bb_pid_reset(bb_pid_handle_t* handle)
{
    if (handle == NULL) {
        return k_bb_err_null_ptr;
    }

    if (!handle->initialized) {
        return k_bb_err_not_initialized;
    }

    handle->integral   = 0.0F;
    handle->prev_error = 0.0F;

    return k_bb_err_ok;
}

bb_err_t bb_pid_set_gains(bb_pid_handle_t* handle, const float kp,
                          const float ki, const float kd)
{
    if (handle == NULL) {
        return k_bb_err_null_ptr;
    }

    if (!handle->initialized) {
        return k_bb_err_not_initialized;
    }

    handle->kp = kp;
    handle->ki = ki;
    handle->kd = kd;

    return k_bb_err_ok;
}

bb_err_t bb_pid_set_output_limits(bb_pid_handle_t* handle, const float min,
                                  const float max)
{
    if (handle == NULL) {
        return k_bb_err_null_ptr;
    }

    if (!handle->initialized) {
        return k_bb_err_not_initialized;
    }

    if (max <= min) {
        return k_bb_err_invalid_arg;
    }

    handle->output_min = min;
    handle->output_max = max;

    return k_bb_err_ok;
}

bb_err_t bb_pid_set_integral_limits(bb_pid_handle_t* handle, const float min,
                                    const float max)
{
    if (handle == NULL) {
        return k_bb_err_null_ptr;
    }

    if (!handle->initialized) {
        return k_bb_err_not_initialized;
    }

    if (max <= min) {
        return k_bb_err_invalid_arg;
    }

    handle->integral_min = min;
    handle->integral_max = max;

    /* Immediately clamp current integral to new limits */
    handle->integral = internal_clamp(handle->integral, min, max);

    return k_bb_err_ok;
}
