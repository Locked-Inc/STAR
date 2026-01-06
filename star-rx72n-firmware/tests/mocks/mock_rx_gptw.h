/* tests/mocks/mock_rx_gptw.h */

/**
 * @file mock_rx_gptw.h
 * @brief Mock GPTW Driver for Unit Testing
 * @details
 * Provides mock implementation of GPTW driver for host-side testing.
 * Tracks initialization state and duty cycle values without hardware.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_GPTW_H
#define MOCK_RX_GPTW_H

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

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RX_GPTW_H */
