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

#include "rx_check.h"
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
 * @brief GPTW PWM waveform mode
 * @see RX72N Hardware Manual Section 26.2.12 (GTCR.MD bits)
 */
typedef enum : uint8_t {
  k_gptw_wave_saw_pwm  = 0, /**< Sawtooth-wave PWM (edge-aligned) */
  k_gptw_wave_tri_pwm1 = 1, /**< Triangle-wave PWM mode 1: transfer at trough */
  k_gptw_wave_tri_pwm2 = 2, /**< Triangle-wave PWM mode 2: transfer at crest and trough */
  k_gptw_wave_tri_pwm3 = 3, /**< Triangle-wave PWM mode 3: 64-bit transfer at trough */
} rx_gptw_wave_mode_t;

/**
 * @brief GPTW channel wrapper type (prevents accidental argument swaps)
 */
typedef struct {
  rx_gptw_channel_t value; /**< GPTW channel */
} rx_gptw_channel_id_t;

/**
 * @brief GPTW output wrapper type (prevents accidental argument swaps)
 */
typedef struct {
  rx_gptw_output_t value; /**< GPTW output */
} rx_gptw_output_id_t;

/**
 * @brief Helper to construct a GPTW channel wrapper
 *
 * @details Provides type-safe wrapper construction with NASA Power of 10 Rule 5
 * validation (minimum 2 checks per function):
 * - Pre-condition: Input value is within valid channel range [0-3]
 * - Post-condition: Wrapper correctly stores the input value
 *
 * @param[in] value GPTW channel enum value (k_gptw_channel_0 to k_gptw_channel_3)
 *
 * @return Wrapped channel ID for type-safe function calls
 *
 * @note Asserts on invalid input - use only with known-valid enum values
 */
static inline rx_gptw_channel_id_t rx_gptw_channel_id(rx_gptw_channel_t value)
{
  /* Pre-condition: channel must be in valid range */
  /* Note: value >= k_gptw_channel_0 is always true for uint8_t since k_gptw_channel_0 == 0 */
  RX_ASSERT(value <= k_gptw_channel_3, "GPTW channel out of range");

  rx_gptw_channel_id_t id = {.value = value};

  /* Post-condition: wrapper must store value correctly */
  RX_ASSERT(id.value == value, "GPTW channel wrapper mismatch");

  return id;
}

/**
 * @brief Helper to construct a GPTW output wrapper
 *
 * @details Provides type-safe wrapper construction with NASA Power of 10 Rule 5
 * validation (minimum 2 checks per function):
 * - Pre-condition: Input value is within valid output range [A-B]
 * - Post-condition: Wrapper correctly stores the input value
 *
 * @param[in] value GPTW output enum value (k_gptw_output_a or k_gptw_output_b)
 *
 * @return Wrapped output ID for type-safe function calls
 *
 * @note Asserts on invalid input - use only with known-valid enum values
 */
static inline rx_gptw_output_id_t rx_gptw_output_id(rx_gptw_output_t value)
{
  /* Pre-condition: output must be in valid range */
  /* Note: value >= k_gptw_output_a is always true for uint8_t since k_gptw_output_a == 0 */
  RX_ASSERT(value <= k_gptw_output_b, "GPTW output out of range");

  rx_gptw_output_id_t id = {.value = value};

  /* Post-condition: wrapper must store value correctly */
  RX_ASSERT(id.value == value, "GPTW output wrapper mismatch");

  return id;
}

/**
 * @brief GPTW PWM configuration
 */
typedef struct {
  uint32_t            frequency_hz;         /**< PWM frequency in Hz (e.g., 20000 for 20kHz) */
  uint16_t            deadtime_ns;          /**< Deadtime in nanoseconds (e.g., 1000 for 1us) */
  rx_gptw_wave_mode_t wave_mode;            /**< Waveform mode (sawtooth or triangle) */
  bool                enable_complementary; /**< Enable complementary outputs */
  bool                invert_polarity;      /**< Invert PWM polarity */
} rx_gptw_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize all 4 GPTW channels with 90 degree phase staggering
 *
 * Configures GPTW0-3 for synchronized PWM with phase offsets:
 * - Channel 0: 0 deg
 * - Channel 1: 90 deg
 * - Channel 2: 180 deg
 * - Channel 3: 270 deg
 *
 * Reduces peak current draw and EMI. Used instead of rx_gptw_init_pwm().
 *
 * Parameter validation performed:
 * - config must be non-NULL
 * - config->frequency_hz must be > 0
 * - config->wave_mode must be a valid rx_gptw_wave_mode_t value
 *
 * @param[in] config Common configuration for all channels (frequency, wave mode, etc.)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if config is NULL
 * @return k_rx_err_invalid_arg if config->frequency_hz is zero or config->wave_mode is invalid
 * @return k_rx_err_hw_error if hardware initialization fails (e.g., gptw_common() returns NULL)
 *
 * @note Propagates rx_err_t codes from internal_calculate_period(),
 *       internal_configure_gptw_hardware(), and internal_configure_mpc().
 */
rx_err_t rx_gptw_init_all_staggered(const rx_gptw_config_t* config);

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
rx_err_t
rx_gptw_set_duty(rx_gptw_channel_id_t channel, rx_gptw_output_id_t output, float duty_percent);

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
