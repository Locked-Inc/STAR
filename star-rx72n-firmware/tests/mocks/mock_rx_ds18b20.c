/**
 * @file mock_rx_ds18b20.c
 * @brief Mock DS18B20 Temperature Sensor Implementation
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_ds18b20.h"

#include <string.h>

/* Static return values */
static rx_err_t s_init_return       = k_rx_ok;
static rx_err_t s_resolution_return = k_rx_ok;
static rx_err_t s_trigger_return    = k_rx_ok;
static rx_err_t s_read_return       = k_rx_ok;

/* Static call counts */
static uint32_t s_init_count    = 0;
static uint32_t s_trigger_count = 0;
static uint32_t s_read_count    = 0;

/* Configured temperature */
static float s_temperature = 25.0f;

void mock_ds18b20_reset(void)
{
  s_init_return       = k_rx_ok;
  s_resolution_return = k_rx_ok;
  s_trigger_return    = k_rx_ok;
  s_read_return       = k_rx_ok;
  s_init_count        = 0;
  s_trigger_count     = 0;
  s_read_count        = 0;
  s_temperature       = 25.0f;
}

void mock_ds18b20_set_init_return(rx_err_t err)
{
  s_init_return = err;
}
void mock_ds18b20_set_resolution_return(rx_err_t err)
{
  s_resolution_return = err;
}
void mock_ds18b20_set_trigger_return(rx_err_t err)
{
  s_trigger_return = err;
}
void mock_ds18b20_set_read_return(rx_err_t err)
{
  s_read_return = err;
}
void mock_ds18b20_set_temperature(float temp_celsius)
{
  s_temperature = temp_celsius;
}

uint32_t mock_ds18b20_get_init_count(void)
{
  return s_init_count;
}
uint32_t mock_ds18b20_get_trigger_count(void)
{
  return s_trigger_count;
}
uint32_t mock_ds18b20_get_read_count(void)
{
  return s_read_count;
}
bool mock_ds18b20_was_initialized(void)
{
  return s_init_count > 0;
}

rx_err_t rx_ds18b20_init(rx_ds18b20_handle_t* handle, const rx_ds18b20_config_t* config)
{
  s_init_count++;

  if (handle == nullptr || config == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_init_return == k_rx_ok) {
    handle->resolution  = config->resolution;
    handle->initialized = true;
  }

  return s_init_return;
}

rx_err_t rx_ds18b20_deinit(rx_ds18b20_handle_t* handle)
{
  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memset(handle, 0, sizeof(*handle));
  return k_rx_ok;
}

rx_err_t rx_ds18b20_set_resolution(rx_ds18b20_handle_t* handle, ds18b20_resolution_t resolution)
{
  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_resolution_return == k_rx_ok) {
    handle->resolution = resolution;
  }

  return s_resolution_return;
}

rx_err_t rx_ds18b20_trigger_conversion(rx_ds18b20_handle_t* handle)
{
  s_trigger_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  return s_trigger_return;
}

rx_err_t rx_ds18b20_read_temperature(rx_ds18b20_handle_t* handle, float* temp_celsius)
{
  s_read_count++;

  if (handle == nullptr || temp_celsius == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_read_return == k_rx_ok) {
    *temp_celsius = s_temperature;
  }

  return s_read_return;
}
