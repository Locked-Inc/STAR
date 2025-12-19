/* lib/star_motor/include/star_motor.h */

#ifndef STAR_MOTOR_H
#define STAR_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @file star_motor.h
 * @brief Brushed DC motor controller using ESP32 MCPWM peripheral
 *
 * This library provides H-bridge motor control using the ESP32's Motor Control PWM
 * (MCPWM) peripheral. It supports bidirectional control, dead-time insertion for
 * shoot-through protection, and optional fault detection.
 *
 * Key Features:
 * - Bidirectional PWM control (-100% to +100% duty cycle)
 * - Automatic dead-time insertion for H-bridge safety
 * - Configurable PWM frequency
 * - Optional fault input for overcurrent protection
 * - Brake and coast modes
 *
 * Example Usage:
 * @code
 * // === Initialize Motor Controller ===
 *
 * #include "star_motor.h"
 *
 * star_motor_handle_t motor;
 * star_motor_config_t config = {
 *     .group_id = 0,                 // MCPWM group 0
 *     .timer_resolution_hz = 10000000,  // 10MHz timer resolution
 *     .pwm_freq_hz = 20000,          // 20kHz PWM frequency
 *     .pin_pwm_a = GPIO_NUM_25,      // H-bridge IN1 pin
 *     .pin_pwm_b = GPIO_NUM_26,      // H-bridge IN2 pin
 *     .dead_time_ns = 1000,          // 1us dead-time
 *     .fault_pin = GPIO_NUM_27,      // Optional fault input
 * };
 *
 * esp_err_t ret = star_motor_init(&motor, &config);
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init motor: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === Control Motor ===
 *
 * // Forward at 50% duty cycle
 * star_motor_set_duty(&motor, 50.0f);
 *
 * // Reverse at 75% duty cycle
 * star_motor_set_duty(&motor, -75.0f);
 *
 * // Stop motor (coast)
 * star_motor_stop(&motor, false);
 *
 * // Stop motor (brake)
 * star_motor_stop(&motor, true);
 *
 *
 * // === Cleanup ===
 *
 * star_motor_deinit(&motor);
 * @endcode
 */

/* --- Type Definitions --- */

/**
 * @brief Motor controller configuration structure
 */
typedef struct {
  int32_t    group_id;            /**< MCPWM group ID (0 or 1) */
  uint32_t   timer_resolution_hz; /**< Timer resolution in Hz (e.g., 10MHz) */
  uint32_t   pwm_freq_hz;         /**< PWM frequency in Hz (e.g., 20kHz) */
  gpio_num_t pin_pwm_a;           /**< PWM output A (H-bridge IN1) */
  gpio_num_t pin_pwm_b;           /**< PWM output B (H-bridge IN2) */
  uint32_t   dead_time_ns;        /**< Dead-time in nanoseconds (e.g., 1000ns = 1us) */
  gpio_num_t fault_pin;           /**< Fault input pin (-1 if not used) */
  bool       invert_pwm;          /**< Invert PWM polarity */
} star_motor_config_t;

/**
 * @brief Motor controller handle structure
 */
typedef struct {
  mcpwm_timer_handle_t timer;        /**< MCPWM timer handle */
  mcpwm_oper_handle_t  operator;     /**< MCPWM operator handle */
  mcpwm_cmpr_handle_t  comparator;   /**< MCPWM comparator handle */
  mcpwm_gen_handle_t   gen_a;        /**< MCPWM generator A handle */
  mcpwm_gen_handle_t   gen_b;        /**< MCPWM generator B handle */
  mcpwm_fault_handle_t fault;        /**< MCPWM fault handle (optional) */
  int32_t              group_id;     /**< MCPWM group ID */
  uint32_t             pwm_freq_hz;  /**< PWM frequency */
  uint32_t             period_ticks; /**< PWM period in timer ticks */
  float                current_duty; /**< Current duty cycle (-100 to +100) */
  bool                 initialized;  /**< Initialization flag */
} star_motor_handle_t;

/* --- Public Functions --- */

/**
 * @brief Initialize motor controller with MCPWM
 *
 * Configures the ESP32 MCPWM peripheral for H-bridge motor control with
 * dead-time insertion and optional fault protection.
 *
 * @param[out] handle Pointer to motor handle structure. Must not be NULL.
 * @param[in]  config Pointer to motor configuration. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_motor_init(star_motor_handle_t* handle, const star_motor_config_t* config);

/**
 * @brief Deinitialize motor controller and release resources
 *
 * @param[in] handle Pointer to initialized motor handle. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_motor_deinit(star_motor_handle_t* handle);

/**
 * @brief Set motor duty cycle
 *
 * Sets the motor PWM duty cycle. Positive values drive forward, negative
 * values drive reverse. The duty cycle is clamped to [-100.0, +100.0].
 *
 * @param[in] handle Pointer to initialized motor handle. Must not be NULL.
 * @param[in] duty   Duty cycle percentage (-100.0 to +100.0).
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_motor_set_duty(star_motor_handle_t* handle, float duty);

/**
 * @brief Stop motor
 *
 * Stops the motor by either coasting (both outputs low) or braking
 * (both outputs high, creating a short circuit brake).
 *
 * @param[in] handle Pointer to initialized motor handle. Must not be NULL.
 * @param[in] brake  If true, apply brake. If false, coast.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_motor_stop(star_motor_handle_t* handle, bool brake);

/**
 * @brief Get current motor duty cycle
 *
 * @param[in]  handle   Pointer to initialized motor handle. Must not be NULL.
 * @param[out] out_duty Pointer to store current duty cycle. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t star_motor_get_duty(const star_motor_handle_t* handle, float* out_duty);

#ifdef __cplusplus
}
#endif

#endif /* STAR_MOTOR_H */
