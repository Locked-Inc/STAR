/**
 * @file mock_rx_bno055.c
 * @brief Mock BNO055 IMU Sensor Implementation for Unit Tests
 *
 * @author STAR Team
 * @date 2026-03-08
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_bno055.h"

#include <string.h>

static rx_err_t s_init_return = k_rx_ok;
static rx_err_t s_read_return = k_rx_ok;
static uint32_t s_init_count  = 0;
static uint32_t s_read_count  = 0;

void mock_bno055_reset(void)
{
  s_init_return = k_rx_ok;
  s_read_return = k_rx_ok;
  s_init_count  = 0;
  s_read_count  = 0;
}

void mock_bno055_set_init_return(rx_err_t err)
{
  s_init_return = err;
}

void mock_bno055_set_read_return(rx_err_t err)
{
  s_read_return = err;
}

uint32_t mock_bno055_get_init_count(void)
{
  return s_init_count;
}

uint32_t mock_bno055_get_read_count(void)
{
  return s_read_count;
}

rx_err_t rx_bno055_init(rx_bus_manager_t* manager)
{
  (void)manager;
  s_init_count++;
  return s_init_return;
}

rx_err_t rx_bno055_read(bno055_data_t* out)
{
  if (out == NULL) {
    return k_rx_err_null_ptr;
  }
  s_read_count++;
  (void)memset(out, 0, sizeof(*out));
  return s_read_return;
}
