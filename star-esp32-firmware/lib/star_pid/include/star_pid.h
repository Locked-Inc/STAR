/* lib/star_pid/include/star_pid.h */

#ifndef STAR_PID_H
#define STAR_PID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @file star_pid.h
 * @brief Generic PID controller for motor control applications
 *
 * This library provides a standard PID (Proportional-Integral-Derivative)
 * controller implementation with anti-windup and output limiting.
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
 * // === Initialize PID Controller ===
 *
 * #include "star_pid.h"
 *
 * star_pid_handle_t pid;
 * star_pid_config_t config = {
 *     .kp = 1.0f,           // Proportional gain
 *     .ki = 0.5f,           // Integral gain
 *     .kd = 0.1f,           // Derivative gain
 *     .output_min = -100.0f,  // Minimum output (e.g., -100% duty cycle)
 *     .output_max = 100.0f,   // Maximum output (e.g., +100% duty cycle)
 *     .integral_min = -50.0f, // Anti-windup lower limit
 *     .integral_max = 50.0f,  // Anti-windup upper limit
 * };
 *
 * esp_err_t ret = star_pid_init(&pid, &config);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init PID: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === PID Control Loop (typically 1000Hz) ===
 *
 * float setpoint = 100.0f;  // Target value (e.g., 100 RPM)
 * float measured = 95.0f;   // Current value (e.g., from encoder)
 * float dt = 0.001f;        // Time delta in seconds (1ms = 1000Hz)
 *
 * float output;
 * star_pid_compute(&pid, setpoint, measured, dt, &output);
 *
 * // Apply output to motor (e.g., PWM duty cycle)
 * motor_set_duty_cycle(output);
 *
 *
 * // === Runtime Gain Tuning ===
 *
 * star_pid_set_gains(&pid, 1.5f, 0.6f, 0.15f);
 *
 *
 * // === Reset PID State ===
 *
 * // Reset integral and derivative terms (e.g., after setpoint change)
 * star_pid_reset(&pid);
 *
 *
 * // === Cleanup ===
 *
 * star_pid_deinit(&pid);
 * @endcode
 */

/* --- Type Definitions --- */

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
} star_pid_config_t;

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
} star_pid_handle_t;

/* --- Public Functions --- */

/**
 * @brief Initialize a PID controller instance
 *
 * @param[out] handle Pointer to PID handle structure. Must not be NULL.
 * @param[in]  config Pointer to PID configuration. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_pid_init(star_pid_handle_t* handle, const star_pid_config_t* config);

/**
 * @brief Deinitialize PID controller and clear state
 *
 * @param[in] handle Pointer to initialized PID handle. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_pid_deinit(star_pid_handle_t* handle);

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
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_pid_compute(star_pid_handle_t* handle,
                           float              setpoint,
                           float              measured,
                           float              dt,
                           float*             output);

/**
 * @brief Reset PID controller internal state
 *
 * Clears the integral accumulator and previous error. Useful when changing
 * setpoints or recovering from disturbances to prevent integral windup.
 *
 * @param[in] handle Pointer to initialized PID handle. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_pid_reset(star_pid_handle_t* handle);

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
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_pid_set_gains(star_pid_handle_t* handle, float kp, float ki, float kd);

/**
 * @brief Update PID output limits at runtime
 *
 * Allows changing the output saturation limits without reinitializing.
 *
 * @param[in] handle     Pointer to initialized PID handle. Must not be NULL.
 * @param[in] output_min New minimum output limit.
 * @param[in] output_max New maximum output limit.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_pid_set_output_limits(star_pid_handle_t* handle, float output_min, float output_max);

/**
 * @brief Update PID integral limits at runtime (anti-windup)
 *
 * Allows changing the integral term saturation limits without reinitializing.
 *
 * @param[in] handle       Pointer to initialized PID handle. Must not be NULL.
 * @param[in] integral_min New minimum integral limit.
 * @param[in] integral_max New maximum integral limit.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t
star_pid_set_integral_limits(star_pid_handle_t* handle, float integral_min, float integral_max);

#ifdef __cplusplus
}
#endif

#endif /* STAR_PID_H */
