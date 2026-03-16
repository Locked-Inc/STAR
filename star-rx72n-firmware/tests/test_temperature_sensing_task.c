/**
 * @file test_temperature_sensing_task.c
 * @brief Unit Tests for Temperature Sensing Task
 *
 * @details
 * Tests temperature sensing task creation and DS18B20 data handling.
 * Uses mocks for ThreadX, DS18B20 driver, and shared data.
 *
 * Test coverage:
 * - Task creation success
 * - Conversion trigger and read
 * - Temperature data storage for telemetry
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "mock_rx_ds18b20.h"
#include "mock_shared_data.h"
#include "tx_api.h"
#include "unity.h"

/* Include the task header for the public API */
#include "temp_sensor_task.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

typedef enum : uint8_t {
  k_test_sensor_idx   = 0, /**< Primary sensor index */
  k_test_sensor_count = 1, /**< Number of sensors */
} test_temp_constants_t;

/** @brief Named constants for temperature test values */
typedef enum : uint16_t {
  k_test_timestamp_ms   = 1000, /**< Test timestamp in ms */
  k_test_expected_cdegc = 2575, /**< Expected 25.75 degC in centi-degrees */
} test_temp_u16_constants_t;

/** @brief Floating-point temperature test values (cannot be in enum) */
static const float s_test_temp_reading_celsius = 25.5F;
static const float s_test_temp_convert_celsius = 25.75F;
static const float s_test_cdegc_per_degree     = 100.0F;

/** @brief Tolerance for float comparisons (replaces Unity UNITY_FLOAT_PRECISION literal) */
static const float s_float_tolerance = 0.00001F;

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

void setUp(void)
{
  /* Reset all mocks before each test */
  mock_shared_data_reset();
  mock_ds18b20_reset();
  mock_tx_reset();
}

void tearDown(void)
{
  /* Clean up after each test */
}

/* =============================================================================
 * Task Creation Tests
 * =============================================================================
 */

/**
 * @brief Test successful temperature sensing task creation
 *
 * @details
 * Verifies that temp_sensor_task_create() successfully creates
 * the ThreadX thread when conditions are normal.
 */
void test_temp_task_create_success(void)
{
  /* Configure mocks for success */
  rx_err_t err;

  mock_tx_set_thread_create_return(TX_SUCCESS);

  /* Create the task */
  err = temp_sensor_task_create();

  /* Verify success */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_tx_was_thread_create_called());
}

/**
 * @brief Test temp task creation fails when ThreadX fails
 */
void test_temp_task_create_thread_failure(void)
{
  /* Configure ThreadX to fail */
  rx_err_t err;

  mock_tx_set_thread_create_return(TX_NO_MEMORY);

  /* Create the task */
  err = temp_sensor_task_create();

  /* Verify failure */
  TEST_ASSERT_EQUAL(k_rx_err_rtos_thread_create, err);
}

/**
 * @brief Test temp task creation fails when already created
 */
void test_temp_task_create_already_created(void)
{
  /* Configure mocks for success */
  rx_err_t err;

  mock_tx_set_thread_create_return(TX_SUCCESS);

  /* Create the task first time - should succeed */
  err = temp_sensor_task_create();
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create the task second time - should fail */
  err = temp_sensor_task_create();
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Sensor Operation Tests
 * =============================================================================
 */

/**
 * @brief Test DS18B20 initialization
 *
 * @details
 * Verifies that rx_ds18b20_init() is called during task startup.
 */
void test_temp_task_initializes_ds18b20(void)
{
  /* Configure mock */
  rx_ds18b20_handle_t handle = {};
  rx_ds18b20_config_t config = {};
  rx_err_t            err;

  mock_ds18b20_set_init_return(k_rx_ok);

  /* Configure sensor */
  config.bus_manager      = nullptr;
  config.bus_name         = "onewire0";
  config.resolution       = k_ds18b20_resolution_12bit;
  config.use_rom_matching = false;

  /* Initialize */
  err = rx_ds18b20_init(&handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_ds18b20_was_initialized());
}

/**
 * @brief Test conversion trigger
 *
 * @details
 * Verifies that rx_ds18b20_trigger_conversion() is called
 * to start a temperature reading.
 */
void test_temp_task_triggers_conversion(void)
{
  /* Configure mock */
  rx_ds18b20_handle_t handle = {};
  rx_err_t            err;

  mock_ds18b20_set_trigger_return(k_rx_ok);

  /* Trigger conversion */
  err = rx_ds18b20_trigger_conversion(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(1, mock_ds18b20_get_trigger_count());
}

/**
 * @brief Test temperature reading
 *
 * @details
 * Verifies that temperature can be read after conversion.
 */
void test_temp_task_reads_temperature(void)
{
  /* Configure mock with known temperature */
  rx_ds18b20_handle_t handle = {};
  float               temp_celsius;
  rx_err_t            err;

  mock_ds18b20_set_temperature(s_test_temp_reading_celsius);
  mock_ds18b20_set_read_return(k_rx_ok);

  /* Read temperature */
  err = rx_ds18b20_read_temperature(&handle, &temp_celsius);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, s_test_temp_reading_celsius, temp_celsius);
  TEST_ASSERT_EQUAL_UINT32(1, mock_ds18b20_get_read_count());
}

/**
 * @brief Test temperature data stored in shared data for telemetry
 *
 * @details
 * Verifies that temperature readings are stored in shared_data
 * for telemetry task to access.
 */
void test_temp_data_stored_in_telemetry(void)
{
  /* Build state as task does */
  temp_sensor_state_t state_in  = {};
  temp_sensor_state_t state_out = {};
  rx_err_t            err;

  /* Configure temperature reading (25.75degC) */
  float temp_celsius = s_test_temp_convert_celsius;

  state_in.temperature_cdegc[k_test_sensor_idx] = (int16_t)(temp_celsius * s_test_cdegc_per_degree);
  state_in.sensor_valid[k_test_sensor_idx]      = true;
  state_in.sensor_count                         = k_test_sensor_count;
  state_in.timestamp_ms                         = k_test_timestamp_ms;

  /* Store in shared data */
  err = shared_data_update_temp(&state_in);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Read back */
  err = shared_data_get_temp(&state_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify */
  TEST_ASSERT_TRUE(state_out.sensor_valid[k_test_sensor_idx]);
  TEST_ASSERT_EQUAL_INT16(k_test_expected_cdegc, state_out.temperature_cdegc[k_test_sensor_idx]);
  TEST_ASSERT_EQUAL_UINT8(k_test_sensor_count, state_out.sensor_count);
}

/**
 * @brief Test invalid temperature marked correctly
 *
 * @details
 * When temperature read fails, the state should be marked invalid.
 */
void test_temp_task_handles_read_failure(void)
{
  /* Configure mock to fail */
  temp_sensor_state_t state_in  = {};
  temp_sensor_state_t state_out = {};
  rx_err_t            err;

  mock_ds18b20_set_read_return(k_rx_err_timeout);

  /* Try to read (will fail) */
  rx_ds18b20_handle_t handle = {};
  float               temp_celsius;
  err = rx_ds18b20_read_temperature(&handle, &temp_celsius);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  /* Build state with invalid data as task does */
  state_in.sensor_valid[k_test_sensor_idx] = false;
  state_in.sensor_count                    = k_test_sensor_count;
  state_in.timestamp_ms                    = k_test_timestamp_ms;

  /* Store in shared data */
  err = shared_data_update_temp(&state_in);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Read back and verify invalid */
  err = shared_data_get_temp(&state_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(state_out.sensor_valid[k_test_sensor_idx]);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Task Creation Tests */
  RUN_TEST(test_temp_task_create_success);
  RUN_TEST(test_temp_task_create_thread_failure);
  RUN_TEST(test_temp_task_create_already_created);

  /* Sensor Operation Tests */
  RUN_TEST(test_temp_task_initializes_ds18b20);
  RUN_TEST(test_temp_task_triggers_conversion);
  RUN_TEST(test_temp_task_reads_temperature);
  RUN_TEST(test_temp_data_stored_in_telemetry);
  RUN_TEST(test_temp_task_handles_read_failure);

  return UNITY_END();
}
