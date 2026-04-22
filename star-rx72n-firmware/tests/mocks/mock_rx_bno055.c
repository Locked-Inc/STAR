/**
 * @file mock_rx_bno055.c
 * @brief Mock BNO055 IMU Sensor Implementation for Unit Tests
 *
 * @details
 * Test double for the BNO055 9-DOF absolute orientation sensor driver.
 * Records call counts and injects configurable return values so that unit
 * tests can exercise IMU task logic (initialization retry, read path, error
 * handling) without requiring physical BNO055 hardware or RIIC1 I2C bus.
 *
 * @author STAR Team
 * @date 2026-03-08
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_bno055.h"

#include <stddef.h>
#include <stdint.h>

/** @brief Injected return value for rx_bno055_init() (default k_rx_ok) */
static rx_err_t s_init_return = k_rx_ok;
/** @brief Injected return value for rx_bno055_read() (default k_rx_ok) */
static rx_err_t s_read_return = k_rx_ok;
/** @brief Number of rx_bno055_init() calls since last mock_bno055_reset() */
static uint32_t s_init_count = 0;
/** @brief Number of rx_bno055_read() calls (out != NULL) since last mock_bno055_reset() */
static uint32_t s_read_count = 0;
/** @brief Mode captured from config->mode on last rx_bno055_init() call */
static bno055_mode_t s_last_mode = k_bno055_mode_poll;

/**
 * @brief Reset all mock state to defaults (k_rx_ok returns, zero call counts)
 *
 * @details
 * Resets s_init_return and s_read_return to k_rx_ok and clears s_init_count
 * and s_read_count to 0. Call from setUp() before each test.
 *
 * @pre None
 * @pre Mock module is linked into the test executable
 * @post s_init_return == k_rx_ok
 * @post s_read_return == k_rx_ok
 * @post s_init_count == 0 and s_read_count == 0
 */
void mock_bno055_reset(void)
{
  s_init_return = k_rx_ok;
  s_read_return = k_rx_ok;
  s_init_count  = 0;
  s_read_count  = 0;
  s_last_mode   = k_bno055_mode_poll;
}

/**
 * @brief Set the return value that rx_bno055_init() will return
 *
 * @param[in] err Return value to inject (e.g. k_rx_ok or k_rx_err_nack)
 *
 * @pre err is a valid rx_err_t value
 * @post Next call to rx_bno055_init() returns err
 */
void mock_bno055_set_init_return(rx_err_t err)
{
  s_init_return = err;
}

/**
 * @brief Set the return value that rx_bno055_read() will return
 *
 * @param[in] err Return value to inject (e.g. k_rx_ok or k_rx_err_timeout)
 *
 * @pre err is a valid rx_err_t value
 * @post Next call to rx_bno055_read() returns err (unless out is NULL)
 */
void mock_bno055_set_read_return(rx_err_t err)
{
  s_read_return = err;
}

/**
 * @brief Return the number of times rx_bno055_init() has been called
 *
 * @return uint32_t Call count since last mock_bno055_reset()
 *
 * @pre mock_bno055_reset() called at least once to establish baseline
 * @post Return value equals number of rx_bno055_init() calls since reset
 */
uint32_t mock_bno055_get_init_count(void)
{
  return s_init_count;
}

/**
 * @brief Return the number of times rx_bno055_read() has been called
 *
 * @return uint32_t Call count since last mock_bno055_reset()
 *
 * @pre mock_bno055_reset() called at least once to establish baseline
 * @post Return value equals number of successful rx_bno055_read() calls (out != NULL)
 */
uint32_t mock_bno055_get_read_count(void)
{
  return s_read_count;
}

/**
 * @brief Return the bno055_mode_t value last passed to rx_bno055_init()
 *
 * @return bno055_mode_t Mode passed to most recent rx_bno055_init() call
 *
 * @pre mock_bno055_reset() called at least once to establish baseline
 * @post Return value reflects the config->mode of the last rx_bno055_init() call
 */
bno055_mode_t mock_bno055_get_last_mode(void)
{
  return s_last_mode;
}

/**
 * @brief Mock implementation of rx_bno055_init()
 *
 * @details
 * Increments the init call counter and returns the value set by
 * mock_bno055_set_init_return(). Does not access hardware.
 *
 * @param[in] manager Bus manager pointer (ignored in mock)
 * @param[in] config  Config pointer; mode field captured when non-NULL
 *
 * @return rx_err_t Value set by mock_bno055_set_init_return() (default k_rx_ok)
 *
 * @pre manager may be NULL (ignored by mock)
 * @pre config may be NULL (mode not captured when config is NULL)
 * @post s_init_count incremented by 1
 * @post s_last_mode == config->mode when config != NULL
 */
rx_err_t rx_bno055_init(rx_bus_manager_t* manager, const bno055_config_t* config)
{
  if (manager == NULL || config == NULL) {
    return k_rx_err_null_ptr;
  }
  s_init_count++;
  s_last_mode = config->mode;
  return s_init_return;
}

/**
 * @brief Mock implementation of rx_bno055_read()
 *
 * @details
 * Returns k_rx_err_null_ptr if out is NULL. Otherwise zeroes *out, increments
 * the read call counter, and returns the value set by mock_bno055_set_read_return().
 *
 * @param[out] out Destination for sensor data (zeroed on success)
 *
 * @return rx_err_t Value set by mock_bno055_set_read_return() (default k_rx_ok)
 * @retval k_rx_err_null_ptr out is NULL
 *
 * @pre out may be NULL (returns k_rx_err_null_ptr)
 * @post *out zeroed only when out != NULL and injected return value is k_rx_ok
 * @post s_read_count incremented when out != NULL
 */
rx_err_t rx_bno055_read(bno055_data_t* out)
{
  if (out == NULL) {
    return k_rx_err_null_ptr;
  }
  s_read_count++;
  const rx_err_t ret = s_read_return;
  if (ret == k_rx_ok) {
    {
      uint8_t* p = (uint8_t*)out;
      for (size_t i = 0; i < sizeof(*out); i++) {
        p[i] = 0;
      }
    }
  }
  return ret;
}

/**
 * @brief Mock implementation of rx_bno055_clear_int()
 *
 * @details
 * No-op stub for tests -- always returns k_rx_ok. The real driver reads
 * BNO055 INT_STA (0x37) over I2C to deassert the INT pin; under unit
 * tests there is no real I2C traffic to simulate, so the mock just
 * succeeds. Tests that care about the INT-clear path should inject
 * behavior via the bus-manager mock, not here.
 *
 * @return Always k_rx_ok.
 */
rx_err_t rx_bno055_clear_int(void)
{
  return k_rx_ok;
}
