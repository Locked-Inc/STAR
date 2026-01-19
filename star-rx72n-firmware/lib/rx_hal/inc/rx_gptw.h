/* lib/rx_hal/inc/rx_gptw.h */

/**
 * @file rx_gptw.h
 * @brief GPTW PWM Driver for RX72N Motor Control
 * @details
 * General PWM Timer (GPTW) driver for brushed DC motor control.
 *
 * The GPTW provides:
 * - 4 timer channels (GPTW0-GPTW3)
 * - 32-bit resolution (vs MTU's 16-bit)
 * - Optimized for PWM motor control
 * - Complementary outputs for H-bridge control
 * - Configurable deadtime for shoot-through protection
 * - Up to 120MHz operation (PCLKA)
 *
 * For 4-motor control (as per hardware_pinout.h):
 * - Motor 0: GPTW0 channel (PE5/GTIOC0A, PE2/GTIOC0B)
 * - Motor 1: GPTW1 channel (PE4/GTIOC1A, PE1/GTIOC1B)
 * - Motor 2: GPTW2 channel (PE3/GTIOC2A, PE0/GTIOC2B)
 * - Motor 3: GPTW3 channel (PE7/GTIOC3A, PE6/GTIOC3B)
 *
 * PWM Configuration:
 * - Frequency: 20 kHz (typical for motor control)
 * - Resolution: ~6000 counts at 20kHz (PCLKA=120MHz)
 * - Duty cycle: 0-100% (0-period counts)
 * - Deadtime: ~1 us (configurable)
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_GPTW_H
#define STAR_RX72N_GPTW_H

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
 * @brief GPTW channel identifiers
 */
typedef enum : uint8_t {
  k_gptw_channel_0 = 0, /**< GPTW0 - Motor 0 */
  k_gptw_channel_1 = 1, /**< GPTW1 - Motor 1 */
  k_gptw_channel_2 = 2, /**< GPTW2 - Motor 2 */
  k_gptw_channel_3 = 3, /**< GPTW3 - Motor 3 */
} rx_gptw_channel_t;

/**
 * @brief GPTW output channel (for complementary PWM)
 */
typedef enum : uint8_t {
  k_gptw_output_a = 0, /**< GTIOCA output */
  k_gptw_output_b = 1, /**< GTIOCB output */
} rx_gptw_output_t;

/**
 * @brief GPTW PWM configuration
 */
typedef struct {
  uint32_t frequency_hz;         /**< PWM frequency in Hz (e.g., 20000 for 20kHz) */
  uint16_t deadtime_ns;          /**< Deadtime in nanoseconds (e.g., 1000 for 1us) */
  bool     enable_complementary; /**< Enable complementary outputs */
  bool     invert_polarity;      /**< Invert PWM polarity */
} rx_gptw_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize GPTW channel for PWM output
 *
 * Configures GPTW channel for PWM mode (sawtooth or triangle wave).
 * Automatically configures MPC for Port E pin alternate functions.
 * Enables timer and starts PWM generation at 0% duty cycle.
 *
 * @param[in] channel GPTW channel (0-3)
 * @param[in] config PWM configuration
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel or config is invalid
 * @return k_rx_err_invalid_state if GPTW module not enabled
 */
rx_err_t rx_gptw_init_pwm(rx_gptw_channel_t channel, const rx_gptw_config_t* config);

/**
 * @brief Set PWM duty cycle
 *
 * Updates duty cycle for specified output channel.
 * Change takes effect on next PWM period for glitch-free operation.
 *
 * @param[in] channel GPTW channel (0-3)
 * @param[in] output Output channel (A/B)
 * @param[in] duty_percent Duty cycle in percent (0.0 - 100.0)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel, output, or duty is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_gptw_set_duty(rx_gptw_channel_t channel, rx_gptw_output_t output, float duty_percent);

/**
 * @brief Set PWM duty cycle (raw count value)
 *
 * Updates duty cycle using raw counter value for efficiency.
 * Useful in tight control loops where floating-point calculation overhead
 * should be avoided.
 *
 * @param[in] channel GPTW channel (0-3)
 * @param[in] output Output channel (A/B)
 * @param[in] duty_count Duty cycle count (0 - period_count)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel, output, or count is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t
rx_gptw_set_duty_raw(rx_gptw_channel_t channel, rx_gptw_output_t output, uint32_t duty_count);

/**
 * @brief Get current duty cycle
 *
 * @param[in] channel GPTW channel (0-3)
 * @param[in] output Output channel (A/B)
 * @param[out] duty_percent Pointer to store duty cycle in percent
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if duty_percent is NULL
 * @return k_rx_err_invalid_arg if channel or output is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_gptw_get_duty(rx_gptw_channel_t channel, rx_gptw_output_t output, float* duty_percent);

/**
 * @brief Get PWM period count
 *
 * Returns the period register value (useful for raw duty cycle calculations).
 *
 * @param[in] channel GPTW channel (0-3)
 * @param[out] period_count Pointer to store period count
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if period_count is NULL
 * @return k_rx_err_invalid_arg if channel is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_gptw_get_period(rx_gptw_channel_t channel, uint32_t* period_count);

/**
 * @brief Enable or disable PWM output
 *
 * @param[in] channel GPTW channel (0-3)
 * @param[in] output Output channel (A/B)
 * @param[in] enable True to enable output, false to disable
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel or output is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_gptw_enable_output(rx_gptw_channel_t channel, rx_gptw_output_t output, bool enable);

/**
 * @brief Start GPTW timer
 *
 * Starts the timer counter for PWM generation.
 * Timer is automatically started during init, but can be stopped/restarted.
 *
 * @param[in] channel GPTW channel (0-3)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t rx_gptw_start(rx_gptw_channel_t channel);

/**
 * @brief Stop GPTW timer
 *
 * Stops the timer counter. PWM outputs will hold their last state.
 *
 * @param[in] channel GPTW channel (0-3)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t rx_gptw_stop(rx_gptw_channel_t channel);

/**
 * @brief Deinitialize GPTW channel
 *
 * Stops timer, disables outputs, and releases resources.
 *
 * @param[in] channel GPTW channel (0-3)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t rx_gptw_deinit(rx_gptw_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_GPTW_H */
