/**
 * @file mock_rx_gptw.c
 * @brief Mock GPTW Driver Implementation for Unit Testing
 * @details
 * Provides mock implementation of GPTW driver for host-side testing.
 * Records all operations for verification without actual hardware.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_gptw.h"

#include <limits.h>

#include "rx72n_clock.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Mock GPTW channel and output constants */
typedef enum : uint8_t {
  k_mock_gptw_max_channels   = 4, /**< Number of GPTW channels (0-3) */
  k_mock_gptw_outputs_per_ch = 2, /**< Outputs per channel (A and B) */
  k_mock_gptw_output_a       = 0, /**< Output A array index */
  k_mock_gptw_output_b       = 1, /**< Output B array index */
} mock_gptw_constants_t;

/** @brief Duty cycle percentage constants */
typedef enum : uint8_t {
  k_mock_duty_percent_max = 100, /**< Maximum duty cycle percentage */
} mock_duty_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static bool     s_initialized[k_mock_gptw_max_channels]                                = {false};
static bool     s_running[k_mock_gptw_max_channels]                                    = {false};
static uint32_t s_period[k_mock_gptw_max_channels]                                     = {};
static uint32_t s_frequency[k_mock_gptw_max_channels]                                  = {};
static float    s_duty[k_mock_gptw_max_channels][k_mock_gptw_outputs_per_ch]           = {};
static bool     s_output_enabled[k_mock_gptw_max_channels][k_mock_gptw_outputs_per_ch] = {};

/** @brief Error injection: force rx_gptw_init_pwm() to return this error (k_rx_ok = no error) */
static rx_err_t s_init_error = k_rx_ok;
/** @brief Error injection: force rx_gptw_set_duty() to return this error (k_rx_ok = no error) */
static rx_err_t s_set_duty_error = k_rx_ok;
/** @brief Error injection: force rx_gptw_deinit() to return this error (k_rx_ok = no error) */
static rx_err_t s_deinit_error = k_rx_ok;
/** @brief Error injection: force rx_gptw_stop() to return this error (k_rx_ok = no error) */
static rx_err_t s_stop_error = k_rx_ok;
/** @brief Error injection: force rx_gptw_enable_output() to return this error (k_rx_ok = no error) */
static rx_err_t s_enable_output_error = k_rx_ok;
/** @brief Number of successful set_duty() calls remaining before injecting s_set_duty_error_after */
static uint32_t s_set_duty_success_remaining = UINT32_MAX;
/** @brief Error to inject after s_set_duty_success_remaining calls succeed (k_rx_ok = disabled) */
static rx_err_t s_set_duty_error_after = k_rx_ok;
/** @brief Number of successful enable_output() calls remaining before injecting error */
static uint32_t s_enable_output_success_remaining = UINT32_MAX;
/** @brief Error to inject after s_enable_output_success_remaining calls succeed (k_rx_ok = disabled) */
static rx_err_t s_enable_output_error_after = k_rx_ok;

/* =============================================================================
 * Mock Test Helpers
 * =============================================================================
 */

void mock_gptw_reset(void)
{
  for (uint8_t ch = 0; ch < k_mock_gptw_max_channels; ch++) {
    s_initialized[ch] = false;
    s_running[ch]     = false;
    s_period[ch]      = 0;
    s_frequency[ch]   = 0;
    for (uint8_t out = 0; out < k_mock_gptw_outputs_per_ch; out++) {
      s_duty[ch][out]           = 0.0F;
      s_output_enabled[ch][out] = false;
    }
  }
  s_init_error                      = k_rx_ok;
  s_set_duty_error                  = k_rx_ok;
  s_deinit_error                    = k_rx_ok;
  s_stop_error                      = k_rx_ok;
  s_enable_output_error             = k_rx_ok;
  s_set_duty_success_remaining      = UINT32_MAX;
  s_set_duty_error_after            = k_rx_ok;
  s_enable_output_success_remaining = UINT32_MAX;
  s_enable_output_error_after       = k_rx_ok;
}

void mock_gptw_set_init_error(rx_err_t err)
{
  s_init_error = err;
}

void mock_gptw_set_duty_error(rx_err_t err)
{
  s_set_duty_error = err;
}

void mock_gptw_set_duty_error_after_n(uint32_t n, rx_err_t err)
{
  s_set_duty_success_remaining = n;
  s_set_duty_error_after       = err;
}

void mock_gptw_set_deinit_error(rx_err_t err)
{
  s_deinit_error = err;
}

void mock_gptw_set_stop_error(rx_err_t err)
{
  s_stop_error = err;
}

void mock_gptw_set_enable_output_error(rx_err_t err)
{
  s_enable_output_error = err;
}

void mock_gptw_set_enable_output_error_after_n(uint32_t n, rx_err_t err)
{
  s_enable_output_success_remaining = n;
  s_enable_output_error_after       = err;
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
    return 0.0F;
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
  if (config == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((int32_t)channel >= k_mock_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (config->frequency_hz == 0) {
    return k_rx_err_invalid_arg;
  }

  if (s_init_error != k_rx_ok) {
    return s_init_error;
  }

  /* Calculate period */
  s_period[channel]    = k_pclka_hz / config->frequency_hz;
  s_frequency[channel] = config->frequency_hz;

  /* Initialize outputs to 0% duty, enabled */
  s_duty[channel][k_mock_gptw_output_a]           = 0.0F;
  s_duty[channel][k_mock_gptw_output_b]           = 0.0F;
  s_output_enabled[channel][k_mock_gptw_output_a] = true;
  s_output_enabled[channel][k_mock_gptw_output_b] = true;

  s_initialized[channel] = true;
  s_running[channel]     = true;

  return k_rx_ok;
}

rx_err_t rx_gptw_init_all_staggered(const rx_gptw_config_t* configs[k_rx_gptw_channel_count])
{
  rx_err_t          err;
  const uint32_t    max_channels = k_mock_gptw_max_channels;
  rx_gptw_channel_t channel;

  /* Pre-condition: validate array pointer (NASA Power of 10 Rule 5) */
  if (configs == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Pre-condition: validate every per-channel pointer */
  for (uint32_t i = 0; i < max_channels; i++) {
    if (configs[i] == nullptr) {
      return k_rx_err_null_ptr;
    }
  }

  /* Pre-condition: validate frequency is non-zero (shared across channels) */
  if (configs[0]->frequency_hz == 0) {
    return k_rx_err_invalid_arg;
  }

  /* Simulate initializing all channels with per-channel configs */
  for (uint32_t i = 0; i < max_channels; i++) {
    channel = (rx_gptw_channel_t)i;
    err     = rx_gptw_init_pwm(channel, configs[i]);
    if (err != k_rx_ok) {
      return err;
    }
  }

  return k_rx_ok;
}

rx_err_t
rx_gptw_set_duty(rx_gptw_channel_id_t channel, rx_gptw_output_id_t output, float duty_percent)
{
  const rx_gptw_channel_t channel_value = channel.value;
  const rx_gptw_output_t  output_value  = output.value;

  if ((int32_t)channel_value >= k_mock_gptw_max_channels || !s_initialized[channel_value]) {
    return k_rx_err_invalid_state;
  }

  if ((int32_t)output_value >= k_mock_gptw_outputs_per_ch) {
    return k_rx_err_invalid_arg;
  }

  if (duty_percent < 0.0F || duty_percent > (float)k_mock_duty_percent_max) {
    return k_rx_err_invalid_arg;
  }

  if (s_set_duty_error != k_rx_ok) {
    return s_set_duty_error;
  }

  if (s_set_duty_error_after != k_rx_ok) {
    if (s_set_duty_success_remaining == 0) {
      return s_set_duty_error_after;
    }
    s_set_duty_success_remaining--;
  }

  s_duty[channel_value][output_value] = duty_percent;
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
  if (duty_percent == nullptr) {
    return k_rx_err_null_ptr;
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
  if (period_count == nullptr) {
    return k_rx_err_null_ptr;
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

  if (s_enable_output_error != k_rx_ok) {
    return s_enable_output_error;
  }

  if (s_enable_output_error_after != k_rx_ok) {
    if (s_enable_output_success_remaining == 0) {
      return s_enable_output_error_after;
    }
    s_enable_output_success_remaining--;
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

  if (s_stop_error != k_rx_ok) {
    return s_stop_error;
  }

  s_running[channel] = false;
  return k_rx_ok;
}

rx_err_t rx_gptw_deinit(rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= k_mock_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (s_deinit_error != k_rx_ok) {
    return s_deinit_error;
  }

  s_initialized[channel]                          = false;
  s_running[channel]                              = false;
  s_period[channel]                               = 0;
  s_frequency[channel]                            = 0;
  s_duty[channel][k_mock_gptw_output_a]           = 0.0F;
  s_duty[channel][k_mock_gptw_output_b]           = 0.0F;
  s_output_enabled[channel][k_mock_gptw_output_a] = false;
  s_output_enabled[channel][k_mock_gptw_output_b] = false;

  return k_rx_ok;
}
