/* include/rx_mtu3a.h */

/**
 * @file rx_mtu3a.h
 * @brief MTU3a PWM Driver for RX72N Motor Control
 * @details
 * Multi-Function Timer Unit (MTU3a) PWM driver for brushed DC motor control.
 *
 * The MTU3a provides:
 * - 8 timer channels (MTU0-MTU4, MTU6-MTU7)
 * - PWM mode 1 (triangle wave, center-aligned)
 * - Complementary outputs for H-bridge control
 * - Configurable deadtime for shoot-through protection
 * - Up to 240MHz operation (PCLKA)
 *
 * For 4-motor control:
 * - Motor 1: MTU3 channel (MTIOC3A/3B)
 * - Motor 2: MTU3 channel (MTIOC3C/3D)
 * - Motor 3: MTU4 channel (MTIOC4A/4B)
 * - Motor 4: MTU4 channel (MTIOC4C/4D)
 *
 * PWM Configuration:
 * - Frequency: 20 kHz (typical for motor control)
 * - Resolution: 12-bit (PCLKA/20kHz = 6000 counts)
 * - Duty cycle: 0-100% (0-6000 counts)
 * - Deadtime: ~1 us (configurable)
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_MTU3A_H
#define STAR_RX72N_MTU3A_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief MTU channel identifiers
 */
typedef enum {
  k_mtu_channel_0 = 0,
  k_mtu_channel_1 = 1,
  k_mtu_channel_2 = 2,
  k_mtu_channel_3 = 3,
  k_mtu_channel_4 = 4,
  k_mtu_channel_6 = 6,
  k_mtu_channel_7 = 7,
} rx_mtu_channel_t;

/**
 * @brief MTU output channel (for complementary PWM)
 */
typedef enum {
  k_mtu_output_a = 0, /**< MTIOCA output */
  k_mtu_output_b = 1, /**< MTIOCB output */
  k_mtu_output_c = 2, /**< MTIOCC output */
  k_mtu_output_d = 3, /**< MTIOCD output */
} rx_mtu_output_t;

/**
 * @brief MTU PWM configuration
 */
typedef struct {
  uint32_t frequency_hz;         /**< PWM frequency in Hz (e.g., 20000 for 20kHz) */
  uint16_t deadtime_ns;          /**< Deadtime in nanoseconds (e.g., 1000 for 1us) */
  bool     enable_complementary; /**< Enable complementary outputs */
  bool     invert_polarity;      /**< Invert PWM polarity */
} rx_mtu_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize MTU channel for PWM output
 *
 * Configures MTU channel for PWM mode 1 (center-aligned, triangle wave).
 * Enables timer and starts PWM generation at 0% duty cycle.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[in] config PWM configuration
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel or config is invalid
 * @return k_rx_err_invalid_state if MTU module not enabled
 */
rx_err_t rx_mtu_init_pwm(rx_mtu_channel_t channel, const rx_mtu_config_t* config);

/**
 * @brief Set PWM duty cycle
 *
 * Updates duty cycle for specified output channel.
 * Change takes effect on next PWM period for glitch-free operation.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[in] output Output channel (A/B/C/D)
 * @param[in] duty_percent Duty cycle in percent (0.0 - 100.0)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel, output, or duty is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_mtu_set_duty(rx_mtu_channel_t channel, rx_mtu_output_t output, float duty_percent);

/**
 * @brief Set PWM duty cycle (raw count value)
 *
 * Updates duty cycle using raw counter value for efficiency.
 * Useful in tight control loops where floating-point calculation overhead
 * should be avoided.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[in] output Output channel (A/B/C/D)
 * @param[in] duty_count Duty cycle count (0 - period_count)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel, output, or count is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_mtu_set_duty_raw(rx_mtu_channel_t channel, rx_mtu_output_t output, uint16_t duty_count);

/**
 * @brief Get current duty cycle
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[in] output Output channel (A/B/C/D)
 * @param[out] duty_percent Pointer to store duty cycle in percent
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if duty_percent is NULL
 * @return k_rx_err_invalid_arg if channel or output is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_mtu_get_duty(rx_mtu_channel_t channel, rx_mtu_output_t output, float* duty_percent);

/**
 * @brief Get PWM period count
 *
 * Returns the period register value (useful for raw duty cycle calculations).
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[out] period_count Pointer to store period count
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if period_count is NULL
 * @return k_rx_err_invalid_arg if channel is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_mtu_get_period(rx_mtu_channel_t channel, uint16_t* period_count);

/**
 * @brief Enable or disable PWM output
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[in] output Output channel (A/B/C/D)
 * @param[in] enable True to enable output, false to disable
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel or output is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_mtu_enable_output(rx_mtu_channel_t channel, rx_mtu_output_t output, bool enable);

/**
 * @brief Start MTU timer
 *
 * Starts the timer counter for PWM generation.
 * Timer is automatically started during init, but can be stopped/restarted.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t rx_mtu_start(rx_mtu_channel_t channel);

/**
 * @brief Stop MTU timer
 *
 * Stops the timer counter. PWM outputs will hold their last state.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t rx_mtu_stop(rx_mtu_channel_t channel);

/**
 * @brief Deinitialize MTU channel
 *
 * Stops timer, disables outputs, and releases resources.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t rx_mtu_deinit(rx_mtu_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MTU3A_H */
