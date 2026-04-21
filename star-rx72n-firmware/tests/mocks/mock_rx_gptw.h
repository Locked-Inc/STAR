/**
 * @file mock_rx_gptw.h
 * @brief Mock GPTW (General PWM Timer) driver for PWM unit testing
 *
 * @details
 * Provides test double for rx_gptw driver to enable unit testing of PWM-based
 * motor control without RX72N hardware. Supports duty cycle tracking, frequency
 * configuration, and call verification for comprehensive PWM driver testing.
 *
 * Enables testing of:
 * - GPTW initialization and configuration
 * - PWM duty cycle updates (0-100%)
 * - PWM frequency configuration
 * - Channel enable/disable logic
 * - Motor control PWM sequences
 *
 * @par Test Architecture:
 * @dot
 * digraph mock_gptw {
 *   rankdir=LR;
 *   test [label="Motor Control\nTests"];
 *   mock [label="Mock GPTW\n(duty cycle tracking)"];
 *   real [label="Real GPTW\nHardware Timer", style=dashed];
 *   test -> mock;
 *   mock -> real [style=dashed, label="(replaced)"];
 * }
 * @enddot
 *
 * @par Mock Capabilities:
 * | Feature | Supported | Description |
 * |---------|-----------|-------------|
 * | Duty cycle tracking | Yes | Records PWM duty cycles |
 * | Frequency config | Yes | Stores configured PWM frequency |
 * | Multi-channel | Yes | Multiple independent PWM channels |
 * | Call history | Yes | Verifies function call sequences |
 *
 * @par Usage: tests/test_rx_motor.c
 *
 * @see rx_gptw.h Real GPTW driver interface
 * @see tests/test_rx_motor.c Motor control tests
 *
 * @par NASA Power of 10: [OK] Static allocation, bounded loops
 * @par SOLID: D - Dependency Inversion (motor control depends on GPTW interface)
 *
 * @author Locked, Inc.
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "rx_gptw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Test Helpers
 * =============================================================================
 */

/**
 * @brief Reset all mock GPTW state to defaults
 *
 * @details
 * Clears all tracked duty cycles, frequencies, initialization flags, and
 * output enable states across every mock GPTW channel. Call before each
 * test to ensure a clean starting state.
 *
 * @see rx_gptw_deinit() Real driver cleanup function
 */
void mock_gptw_reset(void);

/**
 * @brief Check if a mock GPTW channel has been initialized
 *
 * @details
 * Returns whether rx_gptw_init_pwm() or rx_gptw_init_all_staggered() has
 * been called on the given channel since the last mock_gptw_reset().
 *
 * @param[in] channel GPTW channel to query
 *
 * @retval true  Channel has been initialized via rx_gptw_init_pwm() or rx_gptw_init_all_staggered()
 * @retval false Channel has not been initialized or was reset via mock_gptw_reset()
 *
 * @see rx_gptw_init_pwm() Real single-channel initialization
 * @see rx_gptw_init_all_staggered() Real multi-channel initialization
 */
bool mock_gptw_is_initialized(rx_gptw_channel_t channel);

/**
 * @brief Get the duty cycle last set on a mock GPTW output
 *
 * @details
 * Returns the most recent duty cycle percentage written to the specified
 * channel and output via rx_gptw_set_duty() or rx_gptw_set_duty_raw().
 *
 * @param[in] channel GPTW channel to query
 * @param[in] output  Output identifier (A or B)
 *
 * @return Duty cycle percentage (0.0 - 100.0)
 *
 * @see rx_gptw_set_duty() Real driver duty cycle setter
 * @see rx_gptw_get_duty() Real driver duty cycle getter
 */
float mock_gptw_get_duty(rx_gptw_channel_t channel, rx_gptw_output_t output);

/**
 * @brief Get the period count value recorded for a mock GPTW channel
 *
 * @details
 * Returns the period counter value that was configured during initialization
 * of the specified channel.
 *
 * @param[in] channel GPTW channel to query
 *
 * @return Period count value
 *
 * @see rx_gptw_get_period() Real driver period getter
 */
uint32_t mock_gptw_get_period_value(rx_gptw_channel_t channel);

/**
 * @brief Get the PWM frequency recorded for a mock GPTW channel
 *
 * @details
 * Returns the frequency in Hz that was passed in the configuration during
 * initialization of the specified channel.
 *
 * @param[in] channel GPTW channel to query
 *
 * @return Frequency in Hz
 *
 * @see rx_gptw_config_t::frequency_hz Real driver frequency configuration
 */
uint32_t mock_gptw_get_frequency(rx_gptw_channel_t channel);

/**
 * @brief Check if a mock GPTW output is enabled
 *
 * @details
 * Returns the enable state last set via rx_gptw_enable_output() on the
 * specified channel and output.
 *
 * @param[in] channel GPTW channel to query
 * @param[in] output  Output identifier (A or B)
 *
 * @retval true  Output is enabled on the specified channel
 * @retval false Output is disabled or channel has not been initialized
 *
 * @see rx_gptw_enable_output() Real driver output enable function
 */
bool mock_gptw_is_output_enabled(rx_gptw_channel_t channel, rx_gptw_output_t output);

/**
 * @brief Check if a mock GPTW timer is running
 *
 * @details
 * Returns whether the timer counter for the specified channel is currently
 * in the running state (started via rx_gptw_start() and not yet stopped).
 *
 * @param[in] channel GPTW channel to query
 *
 * @retval true  Timer counter is running (started via rx_gptw_start())
 * @retval false Timer counter is stopped or channel has not been initialized
 *
 * @see rx_gptw_start() Real driver timer start function
 * @see rx_gptw_stop() Real driver timer stop function
 */
bool mock_gptw_is_running(rx_gptw_channel_t channel);

/**
 * @brief Set the error return code for rx_gptw_init_pwm() calls
 *
 * @details
 * When set to a non-zero value, subsequent calls to rx_gptw_init_pwm() will
 * return this error code. Use mock_gptw_reset() to clear the error injection.
 *
 * @param[in] err Error code to return (k_rx_ok = no error)
 *
 * @see mock_gptw_reset() Clear all error injection
 */
void mock_gptw_set_init_error(rx_err_t err);

/**
 * @brief Set the error return code for rx_gptw_set_duty() calls
 *
 * @details
 * When set to a non-zero value, subsequent calls to rx_gptw_set_duty() will
 * return this error code. Use mock_gptw_reset() to clear the error injection.
 *
 * @param[in] err Error code to return (k_rx_ok = no error)
 *
 * @see mock_gptw_reset() Clear all error injection
 */
void mock_gptw_set_duty_error(rx_err_t err);

/**
 * @brief Inject a duty error only after N successful set_duty() calls
 *
 * @details
 * The first n calls to rx_gptw_set_duty() succeed normally; the (n+1)-th and
 * all subsequent calls return err. Use this to cover "second set_duty fails"
 * branches that cannot be reached with the global duty error flag alone.
 * Cleared by mock_gptw_reset().
 *
 * @param[in] n   Number of successful calls before injecting the error (0 = fail immediately)
 * @param[in] err Error code to return after n successful calls
 *
 * @see mock_gptw_reset() Clear all error injection
 */
void mock_gptw_set_duty_error_after_n(uint32_t n, rx_err_t err);

/**
 * @brief Set the error return code for rx_gptw_deinit() calls
 *
 * @details
 * When set to a non-zero value, subsequent calls to rx_gptw_deinit() will
 * return this error code. Use mock_gptw_reset() to clear the error injection.
 *
 * @param[in] err Error code to return (k_rx_ok = no error)
 *
 * @see mock_gptw_reset() Clear all error injection
 */
void mock_gptw_set_deinit_error(rx_err_t err);

/**
 * @brief Set the error return code for rx_gptw_stop() calls
 *
 * @details
 * When set to a non-zero value, subsequent calls to rx_gptw_stop() will
 * return this error code. Use mock_gptw_reset() to clear the error injection.
 *
 * @param[in] err Error code to return (k_rx_ok = no error)
 *
 * @see mock_gptw_reset() Clear all error injection
 */
void mock_gptw_set_stop_error(rx_err_t err);

/**
 * @brief Set the error return code for rx_gptw_enable_output() calls
 *
 * @details
 * When set to a non-zero value, subsequent calls to rx_gptw_enable_output() will
 * return this error code. Use mock_gptw_reset() to clear the error injection.
 *
 * @param[in] err Error code to return (k_rx_ok = no error)
 *
 * @see mock_gptw_reset() Clear all error injection
 */
void mock_gptw_set_enable_output_error(rx_err_t err);

/**
 * @brief Inject an enable_output error only after N successful calls
 *
 * @details
 * The first n calls to rx_gptw_enable_output() succeed normally; the (n+1)-th and
 * all subsequent calls return err. Use this to cover "second enable_output fails"
 * branches. Cleared by mock_gptw_reset().
 *
 * @param[in] n   Number of successful calls before injecting the error (0 = fail immediately)
 * @param[in] err Error code to return after n successful calls
 *
 * @see mock_gptw_reset() Clear all error injection
 */
void mock_gptw_set_enable_output_error_after_n(uint32_t n, rx_err_t err);

/**
 * @brief Initialize all 4 GPTW channels with 90 degree phase staggering (Mock)
 *
 * @details
 * Mock implementation for unit testing. Simulates initializing all 4 GPTW
 * channels (0-3) with the provided configuration. In the mock, each channel
 * is initialized via rx_gptw_init_pwm().
 *
 * @param[in] configs Array of per-channel configuration pointers, one entry
 *                    per GPTW channel (k_rx_gptw_channel_count). All entries
 *                    must share frequency_hz and wave_mode; pin map and other
 *                    fields may vary per channel.
 *
 * @retval k_rx_ok Success, all 4 channels initialized
 * @retval k_rx_err_null_ptr configs (or any configs[i]) is NULL
 * @retval k_rx_err_invalid_arg configs[0]->frequency_hz is zero
 *
 * @see rx_gptw_init_all_staggered() Real driver implementation in rx_gptw.h
 */
rx_err_t rx_gptw_init_all_staggered(const rx_gptw_config_t* configs[k_rx_gptw_channel_count]);

#ifdef __cplusplus
}
#endif
