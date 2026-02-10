/* tests/mocks/mock_rx_gptw.h */

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
 * @par Usage: tests/test_rx_motor.c, tests/test_rx_drv8243.c
 *
 * @see rx_gptw.h Real GPTW driver interface
 * @see tests/test_rx_motor.c Motor control tests
 *
 * @par NASA Power of 10: ✓ Static allocation, bounded loops
 * @par SOLID: D - Dependency Inversion (motor control depends on GPTW interface)
 *
 * @author STAR Team
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdbool.h>
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
 * @brief Reset all mock state
 *
 * Call before each test to ensure clean state.
 */
void mock_gptw_reset(void);

/**
 * @brief Check if channel is initialized
 *
 * @param[in] channel GPTW channel
 *
 * @return true if initialized, false otherwise
 */
bool mock_gptw_is_initialized(rx_gptw_channel_t channel);

/**
 * @brief Get recorded duty cycle for output
 *
 * @param[in] channel GPTW channel
 * @param[in] output Output channel (A/B)
 *
 * @return Duty cycle percentage (0.0 - 100.0)
 */
float mock_gptw_get_duty(rx_gptw_channel_t channel, rx_gptw_output_t output);

/**
 * @brief Get recorded period for channel
 *
 * @param[in] channel GPTW channel
 *
 * @return Period count value
 */
uint32_t mock_gptw_get_period_value(rx_gptw_channel_t channel);

/**
 * @brief Get recorded frequency for channel
 *
 * @param[in] channel GPTW channel
 *
 * @return Frequency in Hz
 */
uint32_t mock_gptw_get_frequency(rx_gptw_channel_t channel);

/**
 * @brief Check if output is enabled
 *
 * @param[in] channel GPTW channel
 * @param[in] output Output channel (A/B)
 *
 * @return true if enabled, false otherwise
 */
bool mock_gptw_is_output_enabled(rx_gptw_channel_t channel, rx_gptw_output_t output);

/**
 * @brief Check if timer is running
 *
 * @param[in] channel GPTW channel
 *
 * @return true if running, false otherwise
 */
bool mock_gptw_is_running(rx_gptw_channel_t channel);

/**
 * @brief Initialize all 4 GPTW channels with 90 degree phase staggering (Mock)
 *
 * Mock implementation for unit testing. Simulates initializing all 4 GPTW
 * channels (0-3) with the provided configuration. In the mock, each channel
 * is initialized via rx_gptw_init_pwm().
 *
 * @param[in] config Pointer to configuration structure containing frequency,
 *                   wave mode, and other PWM settings for all 4 channels.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if config is NULL
 * @return k_rx_err_invalid_arg if config->frequency_hz is zero
 * @return Propagated error codes from rx_gptw_init_pwm() on failure
 */
rx_err_t rx_gptw_init_all_staggered(const rx_gptw_config_t* config);

#ifdef __cplusplus
}
#endif
