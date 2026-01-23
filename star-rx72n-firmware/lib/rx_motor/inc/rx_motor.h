/* lib/rx_motor/inc/rx_motor.h */

/**
 * @file rx_motor.h
 * @brief Brushed DC motor control API using RX72N GPTW PWM
 * @details
 * Provides an interface for controlling brushed DC motors via H-bridge drivers using the
 * RX72N General PWM Timer (GPTW) peripheral. Supports bidirectional control with
 * configurable PWM frequency and dead-time insertion for preventing shoot-through.
 *
 * Port of star_motor from ESP32 to RX72N platform.
 *
 * Key Features:
 * - Bidirectional PWM control (-100% to +100% duty cycle)
 * - Automatic dead-time insertion for H-bridge safety
 * - Configurable PWM frequency (typically 20kHz)
 * - Brake and coast modes
 * - Hardware-based PWM (zero CPU overhead during operation)
 * - 32-bit resolution (GPTW vs MTU's 16-bit)
 *
 * Example Usage:
 * @code
 * // Initialize motor controller
 * rx_motor_handle_t motor;
 * rx_motor_config_t config = {
 *     .channel = k_gptw_channel_0,   // GPTW0 channel
 *     .output_a = k_gptw_output_a,   // GTIOC0A
 *     .output_b = k_gptw_output_b,   // GTIOC0B
 *     .pwm_freq_hz = 20000,          // 20kHz PWM
 *     .dead_time_ns = 1000,          // 1us dead-time
 *     .invert_pwm = false,
 * };
 * rx_motor_init(&motor, &config);
 *
 * // Control motor
 * rx_motor_set_duty(&motor, 50.0f);   // Forward at 50%
 * rx_motor_set_duty(&motor, -75.0f);  // Reverse at 75%
 * rx_motor_stop(&motor, true);        // Brake
 *
 * // Cleanup
 * rx_motor_deinit(&motor);
 * @endcode
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_MOTOR_H
#define STAR_RX_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_gptw.h"

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief Motor controller configuration structure
 */
typedef struct {
  rx_gptw_channel_t channel;      /**< GPTW channel (0-3) */
  rx_gptw_output_t  output_a;     /**< PWM output A (H-bridge IN1) */
  rx_gptw_output_t  output_b;     /**< PWM output B (H-bridge IN2) */
  uint32_t          pwm_freq_hz;  /**< PWM frequency in Hz (e.g., 20kHz) */
  uint32_t          dead_time_ns; /**< Dead-time in nanoseconds (e.g., 1000ns = 1us) */
  bool              invert_pwm;   /**< Invert PWM polarity */
} rx_motor_config_t;

/**
 * @brief Motor controller handle structure
 */
typedef struct {
  rx_gptw_channel_t channel;      /**< GPTW channel */
  rx_gptw_output_t  output_a;     /**< PWM output A */
  rx_gptw_output_t  output_b;     /**< PWM output B */
  uint32_t          pwm_freq_hz;  /**< PWM frequency */
  float             current_duty; /**< Current duty cycle (-100 to +100) */
  bool              invert_pwm;   /**< PWM inversion flag */
  bool              initialized;  /**< Initialization flag */
} rx_motor_handle_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize motor controller with GPTW PWM
 *
 * Configures the RX72N GPTW peripheral for H-bridge motor control.
 * Initializes PWM outputs with specified frequency and dead-time.
 *
 * @param[out] handle Pointer to motor handle structure. Must not be NULL.
 * @param[in]  config Pointer to motor configuration. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or config is NULL
 * @return k_rx_err_invalid_arg if configuration is invalid
 * @return k_rx_err_invalid_state if GPTW initialization fails
 */
rx_err_t rx_motor_init(rx_motor_handle_t* handle, const rx_motor_config_t* config);

/**
 * @brief Deinitialize motor controller and release resources
 *
 * Stops motor, disables PWM outputs, and releases GPTW peripheral.
 *
 * @param[in] handle Pointer to initialized motor handle. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is NULL
 * @return k_rx_err_invalid_state if motor not initialized
 */
rx_err_t rx_motor_deinit(rx_motor_handle_t* handle);

/**
 * @brief Set motor duty cycle
 *
 * Sets the motor PWM duty cycle. Positive values drive forward, negative
 * values drive reverse. The duty cycle is clamped to [-100.0, +100.0].
 *
 * @param[in] handle Pointer to initialized motor handle. Must not be NULL.
 * @param[in] duty   Duty cycle percentage (-100.0 to +100.0).
 *                   - Positive: Forward rotation (output_a active)
 *                   - Negative: Reverse rotation (output_b active)
 *                   - Zero: Coast (both outputs low)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is NULL
 * @return k_rx_err_invalid_state if motor not initialized
 */
rx_err_t rx_motor_set_duty(rx_motor_handle_t* handle, float duty);

/**
 * @brief Stop motor
 *
 * Stops the motor by either coasting (both outputs low) or braking
 * (both outputs high, creating a short circuit brake).
 *
 * @param[in] handle Pointer to initialized motor handle. Must not be NULL.
 * @param[in] brake  If true, apply brake (both outputs high).
 *                   If false, coast (both outputs low).
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is NULL
 * @return k_rx_err_invalid_state if motor not initialized
 */
rx_err_t rx_motor_stop(rx_motor_handle_t* handle, bool brake);

/**
 * @brief Get current motor duty cycle
 *
 * @param[in]  handle   Pointer to initialized motor handle. Must not be NULL.
 * @param[out] out_duty Pointer to store current duty cycle. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or out_duty is NULL
 * @return k_rx_err_invalid_state if motor not initialized
 */
rx_err_t rx_motor_get_duty(const rx_motor_handle_t* handle, float* out_duty);

/**
 * @brief Emergency stop - disable GPTW outputs immediately
 *
 * Disables GPTW peripheral outputs at hardware level for fail-safe operation.
 * Motor CANNOT be re-enabled without calling rx_motor_init() again.
 *
 * This function provides hardware-level safety cutoff for emergency situations
 * such as control loop failures, sensor faults, or safety violations.
 *
 * Safety features:
 * - Immediately sets duty to 0%
 * - Disables GPTW outputs at hardware level
 * - Stops timer to prevent glitches
 * - Marks handle as uninitialized (requires re-init to use)
 *
 * @param[in] handle Pointer to motor handle. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is NULL
 * @return k_rx_err_invalid_state if motor not initialized
 *
 * @note After emergency stop, motor requires rx_motor_init() to operate again
 * @warning This is a safety function - use for emergency conditions only
 */
rx_err_t rx_motor_emergency_stop(rx_motor_handle_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_MOTOR_H */
