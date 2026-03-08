/**
 * @file test_imu_task.c
 * @brief Unit Tests for IMU Task - Interrupt-Driven BNO055 + BMP280 Reading
 *
 * @details
 * Tests IMU task creation and the interrupt-driven timeout constants.
 * Uses mocks for ThreadX, BNO055, BMP280, and shared data.
 *
 * Test coverage:
 * - Task creation success
 * - Task creation failure when ThreadX fails
 * - Timeout constant correctness (k_imu_int_timeout_ms >= 200 ms)
 * - Reference period constant (k_imu_task_period_ms == 50 ms)
 *
 * @author STAR Team
 * @date 2026-03-08
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "mock_shared_data.h"
#include "tx_api.h"
#include "unity.h"

/* Include the task header for the public API */
#include "imu_task.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

typedef enum : uint8_t {
  k_test_event_data_ready = 0x01U, /**< Mirror of k_imu_event_data_ready for tests */
} test_imu_constants_t;

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

void setUp(void)
{
  mock_shared_data_reset();
  mock_tx_reset();
}

void tearDown(void) {}

/* =============================================================================
 * Task Creation Tests
 * =============================================================================
 */

/**
 * @brief Test successful IMU task creation
 *
 * @details
 * Verifies that imu_task_create() returns k_rx_ok when ThreadX succeeds.
 */
void test_imu_task_create_success(void)
{
  mock_tx_set_thread_create_return(TX_SUCCESS);

  const rx_err_t err = imu_task_create();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test IMU task creation failure when thread create fails
 *
 * @details
 * Verifies that imu_task_create() propagates a ThreadX thread creation
 * failure as k_rx_err_rtos_thread_create.
 */
void test_imu_task_create_thread_fail(void)
{
  mock_tx_set_thread_create_return(TX_NO_MEMORY);

  const rx_err_t err = imu_task_create();
  TEST_ASSERT_EQUAL(k_rx_err_rtos_thread_create, err);
}

/* =============================================================================
 * Timing Constant Tests
 * =============================================================================
 */

/**
 * @brief Test that k_imu_int_timeout_ms is 200 ms
 *
 * @details
 * Verifies the BNO055 INT watchdog timeout constant is correctly set to
 * 200 ms, which is the fault recovery window.
 */
void test_imu_int_timeout_ms_value(void)
{
  TEST_ASSERT_EQUAL(200U, (uint8_t)k_imu_int_timeout_ms);
}

/**
 * @brief Test that k_imu_task_period_ms is the 20 Hz reference period
 *
 * @details
 * Verifies the reference period constant is still 50 ms (20 Hz reference),
 * matching the BNO055 output data rate in NDOF mode.
 */
void test_imu_task_period_ms_value(void)
{
  TEST_ASSERT_EQUAL(50U, (uint8_t)k_imu_task_period_ms);
}

/**
 * @brief Test that k_imu_int_timeout_ms exceeds the reference period
 *
 * @details
 * Verifies the timeout (200 ms) is at least 3x the reference period (50 ms),
 * satisfying the IWDT safety margin invariant.
 */
void test_imu_int_timeout_exceeds_period(void)
{
  /* k_imu_int_timeout_ms must be >= 3 * k_imu_task_period_ms */
  TEST_ASSERT_GREATER_OR_EQUAL((uint8_t)(3U * (uint8_t)k_imu_task_period_ms),
                               (uint8_t)k_imu_int_timeout_ms);
}

/**
 * @brief Test that k_imu_task_period_margin_ms is 3x the reference period
 *
 * @details
 * Verifies the safety margin constant (150 ms) equals exactly 3x the
 * reference period (50 ms).
 */
void test_imu_task_period_margin_is_3x(void)
{
  TEST_ASSERT_EQUAL(3U * (uint8_t)k_imu_task_period_ms, (uint8_t)k_imu_task_period_margin_ms);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_imu_task_create_success);
  RUN_TEST(test_imu_task_create_thread_fail);
  RUN_TEST(test_imu_int_timeout_ms_value);
  RUN_TEST(test_imu_task_period_ms_value);
  RUN_TEST(test_imu_int_timeout_exceeds_period);
  RUN_TEST(test_imu_task_period_margin_is_3x);

  return UNITY_END();
}
