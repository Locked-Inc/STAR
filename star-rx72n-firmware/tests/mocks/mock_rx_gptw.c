/* tests/mocks/mock_rx_gptw.c */

/**
 * @file mock_rx_gptw.c
 * @brief Mock GPTW Driver Implementation for Unit Testing
 * @details
 * Provides mock implementation of GPTW driver for host-side testing.
 * Records all operations for verification without actual hardware.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_gptw.h"

#include <string.h>

#include "rx72n_clock.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Mock GPTW channel and output constants */
typedef enum {
  k_mock_gptw_max_channels   = 4, /**< Number of GPTW channels (0-3) */
  k_mock_gptw_outputs_per_ch = 2, /**< Outputs per channel (A and B) */
  k_mock_gptw_output_a       = 0, /**< Output A array index */
  k_mock_gptw_output_b       = 1, /**< Output B array index */
} mock_gptw_constants_t;

/** @brief Duty cycle percentage constants */
typedef enum {
  k_mock_duty_percent_max = 100, /**< Maximum duty cycle percentage */
} mock_duty_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static bool     s_initialized[k_mock_gptw_max_channels]                                = {false};
static bool     s_running[k_mock_gptw_max_channels]                                    = {false};
static uint32_t s_period[k_mock_gptw_max_channels]                                     = {0};
static uint32_t s_frequency[k_mock_gptw_max_channels]                                  = {0};
static float    s_duty[k_mock_gptw_max_channels][k_mock_gptw_outputs_per_ch]           = {{0}};
static bool     s_output_enabled[k_mock_gptw_max_channels][k_mock_gptw_outputs_per_ch] = {{false}};

/* =============================================================================
 * Mock Test Helpers
 * =============================================================================
 */

void mock_gptw_reset(void)
{
  memset(s_initialized, 0, sizeof(s_initialized));
  memset(s_running, 0, sizeof(s_running));
  memset(s_period, 0, sizeof(s_period));
  memset(s_frequency, 0, sizeof(s_frequency));
  memset(s_duty, 0, sizeof(s_duty));
  memset(s_output_enabled, 0, sizeof(s_output_enabled));
}

bool mock_gptw_is_initialized(rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels) {
    return false;
  }
  return s_initialized[channel];
}

float mock_gptw_get_duty(rx_gptw_channel_t channel, rx_gptw_output_t output)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels ||
      (int32_t)output >= k_mock_gptw_outputs_per_ch) {
    return 0.0f;
  }
  return s_duty[channel][output];
}

uint32_t mock_gptw_get_period_value(rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels) {
    return 0;
  }
  return s_period[channel];
}

uint32_t mock_gptw_get_frequency(rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels) {
    return 0;
  }
  return s_frequency[channel];
}

bool mock_gptw_is_output_enabled(rx_gptw_channel_t channel, rx_gptw_output_t output)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels ||
      (int32_t)output >= k_mock_gptw_outputs_per_ch) {
    return false;
  }
  return s_output_enabled[channel][output];
}

bool mock_gptw_is_running(rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels) {
    return false;
  }
  return s_running[channel];
}

/* =============================================================================
 * GPTW Driver Mock Implementation
 * =============================================================================
 */

rx_err_t rx_gptw_init_pwm(rx_gptw_channel_t channel, const rx_gptw_config_t* config)
{
  if (config == NULL) {
    return k_rx_err_null_pointer;
  }

  if ((int32_t)channel >= k_mock_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (config->frequency_hz == 0) {
    return k_rx_err_invalid_arg;
  }

  /* Calculate period */
  s_period[channel]    = k_pclka_hz / config->frequency_hz;
  s_frequency[channel] = config->frequency_hz;

  /* Initialize outputs to 0% duty, enabled */
  s_duty[channel][k_mock_gptw_output_a]           = 0.0f;
  s_duty[channel][k_mock_gptw_output_b]           = 0.0f;
  s_output_enabled[channel][k_mock_gptw_output_a] = true;
  s_output_enabled[channel][k_mock_gptw_output_b] = true;

  s_initialized[channel] = true;
  s_running[channel]     = true;

  return k_rx_ok;
}

rx_err_t rx_gptw_set_duty(rx_gptw_channel_t channel, rx_gptw_output_t output, float duty_percent)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels || !s_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if ((int32_t)output >= k_mock_gptw_outputs_per_ch) {
    return k_rx_err_invalid_arg;
  }

  if (duty_percent < 0.0f || duty_percent > (float)k_mock_duty_percent_max) {
    return k_rx_err_invalid_arg;
  }

  s_duty[channel][output] = duty_percent;
  return k_rx_ok;
}

rx_err_t
rx_gptw_set_duty_raw(rx_gptw_channel_t channel, rx_gptw_output_t output, uint32_t duty_count)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels || !s_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if ((int32_t)output >= k_mock_gptw_outputs_per_ch) {
    return k_rx_err_invalid_arg;
  }

  /* Convert to percentage */
  uint32_t period = s_period[channel];
  if (period > 0) {
    s_duty[channel][output] = (float)duty_count * (float)k_mock_duty_percent_max / (float)period;
  }

  return k_rx_ok;
}

rx_err_t rx_gptw_get_duty(rx_gptw_channel_t channel, rx_gptw_output_t output, float* duty_percent)
{
  if (duty_percent == NULL) {
    return k_rx_err_null_pointer;
  }

  if ((int32_t)channel >= k_mock_gptw_max_channels || !s_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if ((int32_t)output >= k_mock_gptw_outputs_per_ch) {
    return k_rx_err_invalid_arg;
  }

  *duty_percent = s_duty[channel][output];
  return k_rx_ok;
}

rx_err_t rx_gptw_get_period(rx_gptw_channel_t channel, uint32_t* period_count)
{
  if (period_count == NULL) {
    return k_rx_err_null_pointer;
  }

  if ((int32_t)channel >= k_mock_gptw_max_channels || !s_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  *period_count = s_period[channel];
  return k_rx_ok;
}

rx_err_t rx_gptw_enable_output(rx_gptw_channel_t channel, rx_gptw_output_t output, bool enable)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels || !s_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if ((int32_t)output >= k_mock_gptw_outputs_per_ch) {
    return k_rx_err_invalid_arg;
  }

  s_output_enabled[channel][output] = enable;
  return k_rx_ok;
}

rx_err_t rx_gptw_start(rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  s_running[channel] = true;
  return k_rx_ok;
}

rx_err_t rx_gptw_stop(rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  s_running[channel] = false;
  return k_rx_ok;
}

rx_err_t rx_gptw_deinit(rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  s_initialized[channel]                          = false;
  s_running[channel]                              = false;
  s_period[channel]                               = 0;
  s_frequency[channel]                            = 0;
  s_duty[channel][k_mock_gptw_output_a]           = 0.0f;
  s_duty[channel][k_mock_gptw_output_b]           = 0.0f;
  s_output_enabled[channel][k_mock_gptw_output_a] = false;
  s_output_enabled[channel][k_mock_gptw_output_b] = false;

  return k_rx_ok;
}
