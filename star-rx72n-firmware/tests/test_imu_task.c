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
 * - Timeout constant correctness (k_imu_int_timeout_ms == 20 ms)
 * - Reference period constant (k_imu_task_period_ms == 20 ms)
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

/**
 * @brief Test IMU task creation failure when event flags create fails
 *
 * @details
 * Verifies that imu_task_create() returns k_rx_err_rtos_thread_create when
 * tx_event_flags_create() fails. The event flags group must be created before
 * the thread; a failure here must propagate and prevent task creation.
 *
 * @pre setUp() has reset mock TX state
 * @pre s_imu_created == false at start (reset by mock_tx_reset())
 * @post Returns k_rx_err_rtos_thread_create
 * @post No thread was created (event flags creation failed first)
 */
void test_imu_task_create_event_flags_fail(void)
{
  mock_tx_set_event_flags_create_return(TX_NO_MEMORY);

  const rx_err_t err = imu_task_create();
  TEST_ASSERT_EQUAL(k_rx_err_rtos_thread_create, err);
}

/* =============================================================================
 * Timing Constant Tests
 * =============================================================================
 */

/**
 * @brief Test that k_imu_int_timeout_ms is 20 ms (50 Hz polling fallback)
 *
 * @details
 * Verifies the BNO055 INT watchdog timeout. With the BNO055 INT pin not
 * pulsing on this PCB rev (see imu_task.h comment), this value is the
 * effective IMU sample period.
 */
void test_imu_int_timeout_ms_value(void)
{
  TEST_ASSERT_EQUAL(20U, (uint32_t)k_imu_int_timeout_ms);
}

/**
 * @brief Test that k_imu_task_period_ms is the 50 Hz reference period
 */
void test_imu_task_period_ms_value(void)
{
  TEST_ASSERT_EQUAL(20U, (uint32_t)k_imu_task_period_ms);
}

/**
 * @brief Test that k_imu_int_timeout_ms is at least the reference period
 *
 * @details
 * With the polling-fallback rate matching the reference period, the
 * timeout must be >= the period to avoid double-reads inside one period.
 */
void test_imu_int_timeout_exceeds_period(void)
{
  TEST_ASSERT_GREATER_OR_EQUAL((uint32_t)k_imu_task_period_ms, (uint32_t)k_imu_int_timeout_ms);
}

/**
 * @brief Test that k_imu_task_period_margin_ms is 3x the reference period
 *
 * @details
 * Verifies the safety margin constant equals exactly 3x the reference
 * period.
 */
void test_imu_task_period_margin_is_3x(void)
{
  TEST_ASSERT_EQUAL(3U * (uint32_t)k_imu_task_period_ms, (uint32_t)k_imu_task_period_margin_ms);
}

/**
 * @brief Test that a failed first imu_task_create() does not block a second attempt
 *
 * @details
 * Verifies that after a first imu_task_create() call fails (TX_NO_MEMORY injected
 * into tx_thread_create()), a subsequent call with TX_SUCCESS succeeds. Confirms
 * that the task can be retried after a creation failure (no permanent state corruption).
 *
 * @pre setUp() has reset mock TX state
 * @pre s_imu_created == false at start (reset by mock_tx_reset())
 * @post First call returns k_rx_err_rtos_thread_create
 * @post Second call returns k_rx_ok after TX_SUCCESS injected
 */
void test_imu_task_create_retry_succeeds(void)
{
  mock_tx_set_thread_create_return(TX_NO_MEMORY);

  const rx_err_t first_err = imu_task_create();
  TEST_ASSERT_EQUAL(k_rx_err_rtos_thread_create, first_err);

  mock_tx_reset();
  mock_tx_set_thread_create_return(TX_SUCCESS);

  const rx_err_t second_err = imu_task_create();
  TEST_ASSERT_EQUAL(k_rx_ok, second_err);
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
  RUN_TEST(test_imu_task_create_event_flags_fail);
  RUN_TEST(test_imu_task_create_retry_succeeds);
  RUN_TEST(test_imu_int_timeout_ms_value);
  RUN_TEST(test_imu_task_period_ms_value);
  RUN_TEST(test_imu_int_timeout_exceeds_period);
  RUN_TEST(test_imu_task_period_margin_is_3x);

  return UNITY_END();
}
