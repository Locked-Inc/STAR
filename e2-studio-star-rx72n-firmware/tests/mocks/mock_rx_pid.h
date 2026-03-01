/* tests/mocks/mock_rx_pid.h */

/**
 * @file mock_rx_pid.h
 * @brief Mock PID controller for motor control algorithm testing
 *
 * @details
 * Provides test double for PID controller (Proportional-Integral-Derivative)
 * to enable unit testing of motor velocity control without actual PID
 * computation or motor feedback loops.
 *
 * Enables testing of: Motor control task integration, PID configuration and
 * tuning, Control loop timing, Setpoint tracking, Anti-windup behavior
 *
 * @par Mock Capabilities: Configurable PID output values, Error injection (init
 * failures, compute errors), Call count tracking (init, compute, reset, set_gains),
 * State inspection (last setpoint, measured value)
 *
 * @par Usage: tests/test_rx_motor.c, tests/test_motor_control_task.c
 * @see rx_pid.h Real PID controller
 * @see rx_motor.h Motor control using PID
 * @par NASA Power of 10: [OK] Static allocation
 * @par SOLID: D - Motor control depends on PID interface
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum mock_pid_constants_t
 * @brief Mock PID controller constants
 */
typedef enum : uint8_t {
  k_mock_pid_max_instances = 8, /**< Maximum tracked PID instances */
} mock_pid_constants_t;

/* =============================================================================
 * Type Definitions (mirrors rx_pid.h)
 * =============================================================================
 */

/**
 * @struct rx_pid_config_t
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
 * @struct rx_pid_handle_t
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
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Reset all mock PID state
 *
 * @details
 * Clears all call counts, resets return values to defaults (k_rx_ok),
 * clears compute output configuration. Call in test setUp().
 */
void mock_pid_reset(void);

/**
 * @brief Set return value for rx_pid_init()
 *
 * @param[in] err Error code to return from rx_pid_init()
 */
void mock_pid_set_init_return(rx_err_t err);

/**
 * @brief Set return value for rx_pid_deinit()
 *
 * @param[in] err Error code to return from rx_pid_deinit()
 */
void mock_pid_set_deinit_return(rx_err_t err);

/**
 * @brief Set return value for rx_pid_compute()
 *
 * @param[in] err Error code to return from rx_pid_compute()
 */
void mock_pid_set_compute_return(rx_err_t err);

/**
 * @brief Set the output value that rx_pid_compute() will return
 *
 * @param[in] output Output value to write to *output parameter
 */
void mock_pid_set_compute_output(float output);

/**
 * @brief Set return value for rx_pid_reset()
 *
 * @param[in] err Error code to return from rx_pid_reset()
 */
void mock_pid_set_reset_return(rx_err_t err);

/**
 * @brief Set return value for rx_pid_set_gains()
 *
 * @param[in] err Error code to return from rx_pid_set_gains()
 */
void mock_pid_set_gains_return(rx_err_t err);

/**
 * @brief Set return value for rx_pid_set_output_limits()
 *
 * @param[in] err Error code to return from rx_pid_set_output_limits()
 */
void mock_pid_set_output_limits_return(rx_err_t err);

/**
 * @brief Set return value for rx_pid_set_integral_limits()
 *
 * @param[in] err Error code to return from rx_pid_set_integral_limits()
 */
void mock_pid_set_integral_limits_return(rx_err_t err);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

/**
 * @brief Get total number of rx_pid_init() calls
 *
 * @return uint32_t Number of times rx_pid_init() was called
 */
uint32_t mock_pid_get_init_count(void);

/**
 * @brief Get total number of rx_pid_deinit() calls
 *
 * @return uint32_t Number of times rx_pid_deinit() was called
 */
uint32_t mock_pid_get_deinit_count(void);

/**
 * @brief Get total number of rx_pid_compute() calls
 *
 * @return uint32_t Number of times rx_pid_compute() was called
 */
uint32_t mock_pid_get_compute_count(void);

/**
 * @brief Get total number of rx_pid_reset() calls
 *
 * @return uint32_t Number of times rx_pid_reset() was called
 */
uint32_t mock_pid_get_reset_count(void);

/**
 * @brief Get total number of rx_pid_set_gains() calls
 *
 * @return uint32_t Number of times rx_pid_set_gains() was called
 */
uint32_t mock_pid_get_set_gains_count(void);

/**
 * @brief Get the last setpoint passed to rx_pid_compute()
 *
 * @return float Last setpoint value
 */
float mock_pid_get_last_setpoint(void);

/**
 * @brief Get the last measured value passed to rx_pid_compute()
 *
 * @return float Last measured value
 */
float mock_pid_get_last_measured(void);

/**
 * @brief Get the last dt value passed to rx_pid_compute()
 *
 * @return float Last dt value
 */
float mock_pid_get_last_dt(void);

/**
 * @brief Check if any PID instance was initialized
 *
 * @return bool True if rx_pid_init() was called at least once
 */
bool mock_pid_was_initialized(void);

/**
 * @brief Get the last parameters passed to rx_pid_compute()
 *
 * @param[out] setpoint Pointer to store last setpoint (can be NULL)
 * @param[out] measured Pointer to store last measured value (can be NULL)
 * @param[out] dt       Pointer to store last dt value (can be NULL)
 */
void mock_pid_get_last_compute_params(float* setpoint, float* measured, float* dt);

/* =============================================================================
 * Mock PID API (replaces real implementation)
 * =============================================================================
 */

/**
 * @brief Mock rx_pid_init() - Initialize a PID controller instance
 *
 * @param[out] handle Pointer to PID handle structure
 * @param[in]  config Pointer to PID configuration
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_pid_init(rx_pid_handle_t* handle, const rx_pid_config_t* config);

/**
 * @brief Mock rx_pid_deinit() - Deinitialize PID controller
 *
 * @param[in] handle Pointer to initialized PID handle
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_pid_deinit(rx_pid_handle_t* handle);

/**
 * @brief Mock rx_pid_compute() - Compute PID control output
 *
 * @param[in]  handle   Pointer to initialized PID controller handle
 * @param[in]  setpoint Desired target value
 * @param[in]  measured Current measured feedback value
 * @param[in]  dt       Time delta in seconds since last call
 * @param[out] output   Pointer to store computed control output
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t
rx_pid_compute(rx_pid_handle_t* handle, float setpoint, float measured, float dt, float* output);

/**
 * @brief Mock rx_pid_reset() - Reset PID controller internal state
 *
 * @param[in] handle Pointer to initialized PID handle
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_pid_reset(rx_pid_handle_t* handle);

/**
 * @brief Mock rx_pid_set_gains() - Update PID gains at runtime
 *
 * @param[in] handle Pointer to initialized PID handle
 * @param[in] kp     New proportional gain
 * @param[in] ki     New integral gain
 * @param[in] kd     New derivative gain
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_pid_set_gains(rx_pid_handle_t* handle, const float kp, const float ki, const float kd);

/**
 * @brief Mock rx_pid_set_output_limits() - Update PID output limits
 *
 * @param[in] handle     Pointer to initialized PID handle
 * @param[in] output_min New minimum output limit
 * @param[in] output_max New maximum output limit
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t rx_pid_set_output_limits(rx_pid_handle_t* handle, float output_min, float output_max);

/**
 * @brief Mock rx_pid_set_integral_limits() - Update PID integral limits
 *
 * @param[in] handle       Pointer to initialized PID handle
 * @param[in] integral_min New minimum integral limit
 * @param[in] integral_max New maximum integral limit
 *
 * @return rx_err_t Configured return value (default k_rx_ok)
 */
rx_err_t
rx_pid_set_integral_limits(rx_pid_handle_t* handle, float integral_min, float integral_max);

#ifdef __cplusplus
}
#endif
