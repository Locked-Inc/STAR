/**
 * @file mock_rx_bmp280.c
 * @brief Mock BMP280 Barometric Pressure Sensor Implementation for Unit Tests
 *
 * @author STAR Team
 * @date 2026-03-08
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_bmp280.h"

#include <string.h>

static rx_err_t s_init_return = k_rx_ok;
static rx_err_t s_read_return = k_rx_ok;
static uint32_t s_init_count  = 0;
static uint32_t s_read_count  = 0;

void mock_bmp280_reset(void)
{
  s_init_return = k_rx_ok;
  s_read_return = k_rx_ok;
  s_init_count  = 0;
  s_read_count  = 0;
}

void mock_bmp280_set_init_return(rx_err_t err)
{
  s_init_return = err;
}

void mock_bmp280_set_read_return(rx_err_t err)
{
  s_read_return = err;
}

uint32_t mock_bmp280_get_init_count(void)
{
  return s_init_count;
}

uint32_t mock_bmp280_get_read_count(void)
{
  return s_read_count;
}

rx_err_t rx_bmp280_init(rx_bus_manager_t* manager)
{
  (void)manager;
  s_init_count++;
  return s_init_return;
}

rx_err_t rx_bmp280_read(bmp280_data_t* out)
{
  if (out == NULL) {
    return k_rx_err_null_ptr;
  }
  s_read_count++;
  (void)memset(out, 0, sizeof(*out));
  return s_read_return;
}
