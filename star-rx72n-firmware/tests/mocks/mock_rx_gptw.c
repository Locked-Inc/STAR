/**
 * @file mock_rx_gptw.c
 * @brief Mock GPTW Driver Implementation for Unit Testing
 *
 * Provides mock implementation of GPTW driver for host-side testing.
 * Records all operations for verification without actual hardware.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_gptw.h"

#include <string.h>

/* =============================================================================
 * Constants
 * =============================================================================
 */

#define MOCK_GPTW_MAX_CHANNELS   4
#define MOCK_GPTW_OUTPUTS_PER_CH 2

/* Simulated PCLKA for period calculation */
#define MOCK_PCLKA_HZ 120000000UL

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static bool     s_initialized[MOCK_GPTW_MAX_CHANNELS]                              = {false};
static bool     s_running[MOCK_GPTW_MAX_CHANNELS]                                  = {false};
static uint32_t s_period[MOCK_GPTW_MAX_CHANNELS]                                   = {0};
static uint32_t s_frequency[MOCK_GPTW_MAX_CHANNELS]                                = {0};
static float    s_duty[MOCK_GPTW_MAX_CHANNELS][MOCK_GPTW_OUTPUTS_PER_CH]           = {{0}};
static bool     s_output_enabled[MOCK_GPTW_MAX_CHANNELS][MOCK_GPTW_OUTPUTS_PER_CH] = {{false}};

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
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS) {
    return false;
  }
  return s_initialized[channel];
}

float mock_gptw_get_duty(rx_gptw_channel_t channel, rx_gptw_output_t output)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS || (int)output >= MOCK_GPTW_OUTPUTS_PER_CH) {
    return 0.0f;
  }
  return s_duty[channel][output];
}

uint32_t mock_gptw_get_period_value(rx_gptw_channel_t channel)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS) {
    return 0;
  }
  return s_period[channel];
}

uint32_t mock_gptw_get_frequency(rx_gptw_channel_t channel)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS) {
    return 0;
  }
  return s_frequency[channel];
}

bool mock_gptw_is_output_enabled(rx_gptw_channel_t channel, rx_gptw_output_t output)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS || (int)output >= MOCK_GPTW_OUTPUTS_PER_CH) {
    return false;
  }
  return s_output_enabled[channel][output];
}

bool mock_gptw_is_running(rx_gptw_channel_t channel)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS) {
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

  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS) {
    return k_rx_err_invalid_arg;
  }

  if (config->frequency_hz == 0) {
    return k_rx_err_invalid_arg;
  }

  /* Calculate period */
  s_period[channel]    = MOCK_PCLKA_HZ / config->frequency_hz;
  s_frequency[channel] = config->frequency_hz;

  /* Initialize outputs to 0% duty, enabled */
  s_duty[channel][0]           = 0.0f;
  s_duty[channel][1]           = 0.0f;
  s_output_enabled[channel][0] = true;
  s_output_enabled[channel][1] = true;

  s_initialized[channel] = true;
  s_running[channel]     = true;

  return k_rx_ok;
}

rx_err_t rx_gptw_set_duty(rx_gptw_channel_t channel, rx_gptw_output_t output, float duty_percent)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS || !s_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if ((int)output >= MOCK_GPTW_OUTPUTS_PER_CH) {
    return k_rx_err_invalid_arg;
  }

  if (duty_percent < 0.0f || duty_percent > 100.0f) {
    return k_rx_err_invalid_arg;
  }

  s_duty[channel][output] = duty_percent;
  return k_rx_ok;
}

rx_err_t rx_gptw_set_duty_raw(rx_gptw_channel_t channel, rx_gptw_output_t output, uint32_t duty_count)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS || !s_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if ((int)output >= MOCK_GPTW_OUTPUTS_PER_CH) {
    return k_rx_err_invalid_arg;
  }

  /* Convert to percentage */
  uint32_t period = s_period[channel];
  if (period > 0) {
    s_duty[channel][output] = (float)duty_count * 100.0f / (float)period;
  }

  return k_rx_ok;
}

rx_err_t rx_gptw_get_duty(rx_gptw_channel_t channel, rx_gptw_output_t output, float* duty_percent)
{
  if (duty_percent == NULL) {
    return k_rx_err_null_pointer;
  }

  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS || !s_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if ((int)output >= MOCK_GPTW_OUTPUTS_PER_CH) {
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

  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS || !s_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  *period_count = s_period[channel];
  return k_rx_ok;
}

rx_err_t rx_gptw_enable_output(rx_gptw_channel_t channel, rx_gptw_output_t output, bool enable)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS || !s_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if ((int)output >= MOCK_GPTW_OUTPUTS_PER_CH) {
    return k_rx_err_invalid_arg;
  }

  s_output_enabled[channel][output] = enable;
  return k_rx_ok;
}

rx_err_t rx_gptw_start(rx_gptw_channel_t channel)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS) {
    return k_rx_err_invalid_arg;
  }

  s_running[channel] = true;
  return k_rx_ok;
}

rx_err_t rx_gptw_stop(rx_gptw_channel_t channel)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS) {
    return k_rx_err_invalid_arg;
  }

  s_running[channel] = false;
  return k_rx_ok;
}

rx_err_t rx_gptw_deinit(rx_gptw_channel_t channel)
{
  if ((int)channel >= MOCK_GPTW_MAX_CHANNELS) {
    return k_rx_err_invalid_arg;
  }

  s_initialized[channel]       = false;
  s_running[channel]           = false;
  s_period[channel]            = 0;
  s_frequency[channel]         = 0;
  s_duty[channel][0]           = 0.0f;
  s_duty[channel][1]           = 0.0f;
  s_output_enabled[channel][0] = false;
  s_output_enabled[channel][1] = false;

  return k_rx_ok;
}
