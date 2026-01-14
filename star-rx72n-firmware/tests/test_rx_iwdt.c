/* tests/test_rx_iwdt.c */

/**
 * @file test_rx_iwdt.c
 * @brief Unit Tests for IWDT Driver
 * @details
 * Comprehensive tests for Independent Watchdog Timer driver including:
 * - Initialization and configuration
 * - Task registration and heartbeat monitoring
 * - System state management
 * - Status reporting
 * - Reset detection
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rx_err.h"
#include "rx_iwdt.h"
#include "unity.h"

/* =============================================================================
 * Test Helper Functions
 * =============================================================================
 */

static void test_setup(void)
{
  /* Reset test environment before each test */
  rx_iwdt_test_reset();
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

static void test_init_with_null_config(void)
{
  test_setup();

  /* Should succeed with default config */
  rx_err_t err = rx_iwdt_init(NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

static void test_init_with_custom_config(void)
{
  test_setup();

  rx_iwdt_config_t config = {.default_timeout_ms     = 1000,
                             .enable_task_monitoring = true,
                             .reset_on_timeout       = true};

  rx_err_t err = rx_iwdt_init(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

static void test_init_invalid_timeout_too_low(void)
{
  test_setup();

  rx_iwdt_config_t config = {.default_timeout_ms     = 50, /* Below minimum */
                             .enable_task_monitoring = true,
                             .reset_on_timeout       = true};

  rx_err_t err = rx_iwdt_init(&config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

static void test_init_invalid_timeout_too_high(void)
{
  test_setup();

  rx_iwdt_config_t config = {.default_timeout_ms     = 20000, /* Above maximum */
                             .enable_task_monitoring = true,
                             .reset_on_timeout       = true};

  rx_err_t err = rx_iwdt_init(&config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Feed Tests
 * =============================================================================
 */

static void test_feed_before_init(void)
{
  test_setup();

  rx_err_t err = rx_iwdt_feed();
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

static void test_feed_after_init(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_feed();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Task Registration Tests
 * =============================================================================
 */

static void test_register_task_null_name(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_register_task(NULL, 1000);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_register_task_success(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_register_task("TestTask", 1000);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

static void test_register_task_invalid_timeout(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_register_task("TestTask", 50); /* Too low */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

static void test_register_duplicate_task(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_iwdt_register_task("TestTask", 1000);

  /* Try to register same task again */
  rx_err_t err = rx_iwdt_register_task("TestTask", 1000);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Task Heartbeat Tests
 * =============================================================================
 */

static void test_heartbeat_null_name(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_task_heartbeat(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_heartbeat_unregistered_task(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_task_heartbeat("NonExistent");
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

static void test_heartbeat_registered_task(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_iwdt_register_task("TestTask", 1000);

  rx_err_t err = rx_iwdt_task_heartbeat("TestTask");
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * System State Tests
 * =============================================================================
 */

static void test_set_state_invalid(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_set_state((system_state_t)999);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

static void test_set_state_valid(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_set_state(k_system_state_running);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

static void test_set_timeout_for_state(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_set_timeout_for_state(k_system_state_motor_active, 500);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Status Tests
 * =============================================================================
 */

static void test_get_status_null_pointer(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_get_status(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

static void test_get_status_success(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_iwdt_status_t status;
  rx_err_t         err = rx_iwdt_get_status(&status);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(status.initialized);
}

/* =============================================================================
 * Task Monitoring Tests
 * =============================================================================
 */

static void test_check_tasks_before_init(void)
{
  test_setup();

  rx_err_t err = rx_iwdt_check_tasks();
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

static void test_check_tasks_no_registered(void)
{
  test_setup();

  rx_iwdt_init(NULL);
  rx_err_t err = rx_iwdt_check_tasks();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_init_with_null_config);
  RUN_TEST(test_init_with_custom_config);
  RUN_TEST(test_init_invalid_timeout_too_low);
  RUN_TEST(test_init_invalid_timeout_too_high);

  /* Feed tests */
  RUN_TEST(test_feed_before_init);
  RUN_TEST(test_feed_after_init);

  /* Task registration tests */
  RUN_TEST(test_register_task_null_name);
  RUN_TEST(test_register_task_success);
  RUN_TEST(test_register_task_invalid_timeout);
  RUN_TEST(test_register_duplicate_task);

  /* Task heartbeat tests */
  RUN_TEST(test_heartbeat_null_name);
  RUN_TEST(test_heartbeat_unregistered_task);
  RUN_TEST(test_heartbeat_registered_task);

  /* System state tests */
  RUN_TEST(test_set_state_invalid);
  RUN_TEST(test_set_state_valid);
  RUN_TEST(test_set_timeout_for_state);

  /* Status tests */
  RUN_TEST(test_get_status_null_pointer);
  RUN_TEST(test_get_status_success);

  /* Task monitoring tests */
  RUN_TEST(test_check_tasks_before_init);
  RUN_TEST(test_check_tasks_no_registered);

  return UNITY_END();
}
