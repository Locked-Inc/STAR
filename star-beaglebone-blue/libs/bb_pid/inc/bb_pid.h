/**
 * @file bb_pid.h
 * @brief Discrete PID controller for BeagleBone Blue motor control.
 *
 * @details
 * Parallel-form discrete PID with anti-windup clamping and output saturation.
 * Ported from star-rx72n-firmware/libs/rx_pid/ with bb_ prefix. Pure algorithm
 * with zero hardware dependencies.
 *
 * Algorithm:
 *   e[k] = setpoint - measured
 *   integral += e[k] * dt, clamped to [integral_min, integral_max]
 *   derivative = (e[k] - e[k-1]) / dt
 *   output = clamp(Kp*e + Ki*integral + Kd*derivative, output_min, output_max)
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>

#include "bb_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct bb_pid_config_t
 * @brief PID controller configuration parameters.
 *
 * @details
 * Passed to bb_pid_init(). Can be stack-allocated (copied to handle).
 * output_max must be > output_min. integral_max must be > integral_min.
 *
 * @since Version 1.0.0
 */
typedef struct {
    float kp;           /**< Proportional gain */
    float ki;           /**< Integral gain */
    float kd;           /**< Derivative gain */
    float output_min;   /**< Output saturation minimum */
    float output_max;   /**< Output saturation maximum */
    float integral_min; /**< Anti-windup integral minimum */
    float integral_max; /**< Anti-windup integral maximum */
} bb_pid_config_t;

/**
 * @struct bb_pid_handle_t
 * @brief PID controller runtime state.
 *
 * @details
 * Contains both configuration (copied from bb_pid_config_t) and runtime
 * state (integral accumulator, previous error). 40 bytes per instance.
 *
 * @invariant integral in [integral_min, integral_max] after each compute
 * @invariant output in [output_min, output_max] after each compute
 *
 * @since Version 1.0.0
 */
typedef struct {
    float kp;           /**< Proportional gain */
    float ki;           /**< Integral gain */
    float kd;           /**< Derivative gain */
    float output_min;   /**< Output saturation minimum */
    float output_max;   /**< Output saturation maximum */
    float integral_min; /**< Anti-windup integral minimum */
    float integral_max; /**< Anti-windup integral maximum */
    float integral;     /**< Accumulated integral term (state) */
    float prev_error;   /**< Previous error for derivative (state) */
    bool  initialized;  /**< Initialization flag */
} bb_pid_handle_t;

/**
 * @brief Initialize a PID controller with configuration parameters.
 *
 * @param[out] handle PID controller handle (must not be NULL)
 * @param[in]  config Configuration (must not be NULL, copied to handle)
 *
 * @return bb_err_t
 * @retval k_bb_err_ok            Success
 * @retval k_bb_err_null_ptr      handle or config is NULL
 * @retval k_bb_err_already_initialized Already initialized
 * @retval k_bb_err_invalid_arg   output_max <= output_min or integral_max <= integral_min
 *
 * @pre handle->initialized == false
 * @pre config->output_max > config->output_min
 * @pre config->integral_max > config->integral_min
 * @post handle->initialized == true, integral = 0, prev_error = 0
 *
 * @since Version 1.0.0
 */
bb_err_t bb_pid_init(bb_pid_handle_t* handle, const bb_pid_config_t* config);

/**
 * @brief Compute PID control output for one control cycle.
 *
 * @param[in,out] handle PID controller handle (must be initialized)
 * @param[in]     setpoint Desired value
 * @param[in]     measured Current measured value (same units as setpoint)
 * @param[in]     dt Time step in seconds (must be > 0)
 * @param[out]    output Computed control output (clamped)
 *
 * @return bb_err_t
 * @retval k_bb_err_ok              Success
 * @retval k_bb_err_null_ptr        handle or output is NULL
 * @retval k_bb_err_not_initialized Not initialized
 * @retval k_bb_err_invalid_arg     dt <= 0
 *
 * @pre handle must be initialized via bb_pid_init()
 * @pre dt > 0
 * @post integral state updated and clamped
 * @post *output clamped to [output_min, output_max]
 *
 * @since Version 1.0.0
 */
bb_err_t bb_pid_compute(bb_pid_handle_t* handle, float setpoint, float measured,
                        float dt, float* output);

/**
 * @brief Reset PID state (integral and prev_error) without clearing config.
 *
 * @param[in,out] handle PID controller handle
 *
 * @return bb_err_t
 * @retval k_bb_err_ok              Success
 * @retval k_bb_err_null_ptr        handle is NULL
 * @retval k_bb_err_not_initialized Not initialized
 *
 * @post integral = 0, prev_error = 0; gains and limits unchanged
 *
 * @since Version 1.0.0
 */
bb_err_t bb_pid_reset(bb_pid_handle_t* handle);

/**
 * @brief Update PID gains at runtime.
 *
 * @param[in,out] handle PID controller handle
 * @param[in]     kp Proportional gain
 * @param[in]     ki Integral gain
 * @param[in]     kd Derivative gain
 *
 * @return bb_err_t
 * @retval k_bb_err_ok              Success
 * @retval k_bb_err_null_ptr        handle is NULL
 * @retval k_bb_err_not_initialized Not initialized
 *
 * @since Version 1.0.0
 */
bb_err_t bb_pid_set_gains(bb_pid_handle_t* handle, float kp, float ki, float kd);

/**
 * @brief Update output saturation limits at runtime.
 *
 * @param[in,out] handle PID controller handle
 * @param[in]     min Output minimum
 * @param[in]     max Output maximum (must be > min)
 *
 * @return bb_err_t
 * @retval k_bb_err_ok              Success
 * @retval k_bb_err_null_ptr        handle is NULL
 * @retval k_bb_err_not_initialized Not initialized
 * @retval k_bb_err_invalid_arg     max <= min
 *
 * @since Version 1.0.0
 */
bb_err_t bb_pid_set_output_limits(bb_pid_handle_t* handle, float min, float max);

/**
 * @brief Update integral anti-windup limits at runtime.
 *
 * @param[in,out] handle PID controller handle
 * @param[in]     min Integral minimum
 * @param[in]     max Integral maximum (must be > min)
 *
 * @return bb_err_t
 * @retval k_bb_err_ok              Success
 * @retval k_bb_err_null_ptr        handle is NULL
 * @retval k_bb_err_not_initialized Not initialized
 * @retval k_bb_err_invalid_arg     max <= min
 *
 * @post integral immediately clamped to new limits
 *
 * @since Version 1.0.0
 */
bb_err_t bb_pid_set_integral_limits(bb_pid_handle_t* handle, float min, float max);

#ifdef __cplusplus
}
#endif
