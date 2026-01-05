/* tests/mocks/mock_rx_motor.h */

/**
 * @file mock_rx_motor.h
 * @brief Mock Motor Driver for Unit Testing
 * @details
 * Provides mock implementation of rx_motor driver for DRV8243 testing.
 * Tracks motor state and duty cycle values without actual hardware.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_MOTOR_H
#define MOCK_RX_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_gptw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Motor Configuration Types (Copied from rx_motor.h)
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
 * Mock Test Helpers
 * =============================================================================
 */

/**
 * @brief Reset all mock motor state
 *
 * Call before each test to ensure clean state.
 */
void mock_motor_reset(void);

/**
 * @brief Check if motor is initialized
 *
 * @return true if initialized, false otherwise
 */
bool mock_motor_is_initialized(void);

/**
 * @brief Get last recorded duty cycle
 *
 * @return Duty cycle percentage (-100.0 to +100.0)
 */
float mock_motor_get_duty(void);

/**
 * @brief Get last stop mode
 *
 * @return true if brake, false if coast
 */
bool mock_motor_get_brake_mode(void);

/**
 * @brief Check if motor stop was called
 *
 * @return true if stopped, false otherwise
 */
bool mock_motor_is_stopped(void);

/**
 * @brief Set error to return from init
 *
 * @param[in] err Error code to return
 */
void mock_motor_set_init_error(rx_err_t err);

/**
 * @brief Set error to return from set_duty
 *
 * @param[in] err Error code to return
 */
void mock_motor_set_duty_error(rx_err_t err);

/**
 * @brief Set error to return from stop
 *
 * @param[in] err Error code to return
 */
void mock_motor_set_stop_error(rx_err_t err);

/* =============================================================================
 * Motor Driver API (Mock Implementation)
 * =============================================================================
 */

/**
 * @brief Initialize motor controller with GPTW PWM (mock)
 */
rx_err_t rx_motor_init(rx_motor_handle_t* handle, const rx_motor_config_t* config);

/**
 * @brief Deinitialize motor controller (mock)
 */
rx_err_t rx_motor_deinit(rx_motor_handle_t* handle);

/**
 * @brief Set motor duty cycle (mock)
 */
rx_err_t rx_motor_set_duty(rx_motor_handle_t* handle, float duty);

/**
 * @brief Stop motor (mock)
 */
rx_err_t rx_motor_stop(rx_motor_handle_t* handle, bool brake);

/**
 * @brief Get current motor duty cycle (mock)
 */
rx_err_t rx_motor_get_duty(const rx_motor_handle_t* handle, float* out_duty);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RX_MOTOR_H */
