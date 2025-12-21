/* include/rx_pid.h */

/**
 * @file rx_pid.h
 * @brief PID controller API for closed-loop motor control
 * @details
 * Provides a stateless PID (Proportional-Integral-Derivative) controller interface with
 * anti-windup, derivative filtering, and configurable output limits. Suitable for embedded
 * systems with deterministic behavior and tunable gains for velocity or position control loops.
 *
 * Direct port from ESP32 star_pid library - pure algorithm, no hardware dependencies.
 *
 * Key Features:
 * - Standard PID algorithm with Kp, Ki, Kd gains
 * - Configurable output limits
 * - Integral anti-windup
 * - Runtime gain tuning
 * - State reset capability
 *
 * Example Usage:
 * @code
 * // Initialize PID Controller
 * rx_pid_handle_t pid;
 * rx_pid_config_t config = {
 *     .kp = 1.0f,             // Proportional gain
 *     .ki = 0.5f,             // Integral gain
 *     .kd = 0.1f,             // Derivative gain
 *     .output_min = -100.0f,  // Minimum output (e.g., -100% duty cycle)
 *     .output_max = 100.0f,   // Maximum output (e.g., +100% duty cycle)
 *     .integral_min = -50.0f, // Anti-windup lower limit
 *     .integral_max = 50.0f,  // Anti-windup upper limit
 * };
 * rx_pid_init(&pid, &config);
 *
 * // PID Control Loop (typically 250Hz for RX72N motor control)
 * float setpoint = 100.0f;  // Target value (e.g., 100 RPM)
 * float measured = 95.0f;   // Current value (e.g., from encoder)
 * float dt = 0.004f;        // Time delta in seconds (4ms = 250Hz)
 *
 * float output;
 * rx_pid_compute(&pid, setpoint, measured, dt, &output);
 *
 * // Apply output to motor (e.g., PWM duty cycle)
 * rx_motor_set_duty(&motor, output);
 *
 * // Runtime Gain Tuning
 * rx_pid_set_gains(&pid, 1.5f, 0.6f, 0.15f);
 *
 * // Reset PID State (e.g., after setpoint change)
 * rx_pid_reset(&pid);
 *
 * // Cleanup
 * rx_pid_deinit(&pid);
 * @endcode
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX_PID_H
#define STAR_RX_PID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief PID controller configuration structure
 */
typedef struct {
  float kp;           /**< Proportional gain */
  float ki;           /**< Integral gain */
  float kd;           /**< Derivative gain */
  float output_min;   /**< Minimum output limit */
  float output_max;   /**< Maximum output limit */
  float integral_min; /**< Minimum integral value (anti-windup) */
  float integral_max; /**< Maximum integral value (anti-windup) */
} rx_pid_config_t;

/**
 * @brief PID controller handle structure
 */
typedef struct {
  float kp;           /**< Proportional gain */
  float ki;           /**< Integral gain */
  float kd;           /**< Derivative gain */
  float output_min;   /**< Minimum output limit */
  float output_max;   /**< Maximum output limit */
  float integral_min; /**< Minimum integral value (anti-windup) */
  float integral_max; /**< Maximum integral value (anti-windup) */
  float integral;     /**< Accumulated integral term */
  float prev_error;   /**< Previous error for derivative calculation */
  bool  initialized;  /**< Initialization flag */
} rx_pid_handle_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize a PID controller instance
 *
 * @param[out] handle Pointer to PID handle structure. Must not be NULL.
 * @param[in]  config Pointer to PID configuration. Must not be NULL.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if handle or config is NULL
 * @return RX_ERR_INVALID_ARG if output_max <= output_min or integral_max <= integral_min
 */
rx_err_t rx_pid_init(rx_pid_handle_t* handle, const rx_pid_config_t* config);

/**
 * @brief Deinitialize PID controller and clear state
 *
 * @param[in] handle Pointer to initialized PID handle. Must not be NULL.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if handle is NULL
 * @return RX_ERR_INVALID_STATE if not initialized
 */
rx_err_t rx_pid_deinit(rx_pid_handle_t* handle);

/**
 * @brief Compute PID output for current error
 *
 * Calculates the PID control output based on the error between setpoint
 * and measured value. This function implements the standard PID algorithm:
 *
 *   output = Kp*error + Ki*integral(error) + Kd*derivative(error)
 *
 * The output is clamped to [output_min, output_max] and the integral term
 * is clamped to [integral_min, integral_max] for anti-windup.
 *
 * @param[in]  handle    Pointer to initialized PID handle. Must not be NULL.
 * @param[in]  setpoint  Desired target value.
 * @param[in]  measured  Current measured value.
 * @param[in]  dt        Time delta in seconds since last call.
 * @param[out] output    Pointer to store PID output. Must not be NULL.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if handle or output is NULL
 * @return RX_ERR_INVALID_STATE if not initialized
 * @return RX_ERR_INVALID_ARG if dt <= 0
 */
rx_err_t rx_pid_compute(rx_pid_handle_t* handle,
                         float            setpoint,
                         float            measured,
                         float            dt,
                         float*           output);

/**
 * @brief Reset PID controller internal state
 *
 * Clears the integral accumulator and previous error. Useful when changing
 * setpoints or recovering from disturbances to prevent integral windup.
 *
 * @param[in] handle Pointer to initialized PID handle. Must not be NULL.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if handle is NULL
 * @return RX_ERR_INVALID_STATE if not initialized
 */
rx_err_t rx_pid_reset(rx_pid_handle_t* handle);

/**
 * @brief Update PID gains at runtime
 *
 * Allows tuning of PID gains without reinitializing the controller.
 * The internal state (integral, prev_error) is preserved.
 *
 * @param[in] handle Pointer to initialized PID handle. Must not be NULL.
 * @param[in] kp     New proportional gain.
 * @param[in] ki     New integral gain.
 * @param[in] kd     New derivative gain.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if handle is NULL
 * @return RX_ERR_INVALID_STATE if not initialized
 */
rx_err_t rx_pid_set_gains(rx_pid_handle_t* handle, float kp, float ki, float kd);

/**
 * @brief Update PID output limits at runtime
 *
 * Allows changing the output saturation limits without reinitializing.
 *
 * @param[in] handle     Pointer to initialized PID handle. Must not be NULL.
 * @param[in] output_min New minimum output limit.
 * @param[in] output_max New maximum output limit.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if handle is NULL
 * @return RX_ERR_INVALID_STATE if not initialized
 * @return RX_ERR_INVALID_ARG if output_max <= output_min
 */
rx_err_t rx_pid_set_output_limits(rx_pid_handle_t* handle, float output_min, float output_max);

/**
 * @brief Update PID integral limits at runtime (anti-windup)
 *
 * Allows changing the integral term saturation limits without reinitializing.
 *
 * @param[in] handle       Pointer to initialized PID handle. Must not be NULL.
 * @param[in] integral_min New minimum integral limit.
 * @param[in] integral_max New maximum integral limit.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if handle is NULL
 * @return RX_ERR_INVALID_STATE if not initialized
 * @return RX_ERR_INVALID_ARG if integral_max <= integral_min
 */
rx_err_t rx_pid_set_integral_limits(rx_pid_handle_t* handle, float integral_min, float integral_max);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_PID_H */
