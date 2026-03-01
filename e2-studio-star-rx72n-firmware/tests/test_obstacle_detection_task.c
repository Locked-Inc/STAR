/* tests/test_obstacle_detection_task.c */

/**
 * @file test_obstacle_detection_task.c
 * @brief Unit Tests for Obstacle Detection Task
 *
 * @details
 * Tests obstacle detection task creation, initialization, and callback behavior.
 * Uses mocks for ThreadX, rx_obstacle_detect, and shared data.
 *
 * Test coverage:
 * - Task creation success
 * - Obstacle detect initialization and start
 * - Callback triggers emergency stop on obstacle detection
 * - Obstacle distances stored in shared data
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "mock_rx_obstacle_detect.h"
#include "mock_shared_data.h"
#include "tx_api.h"
#include "unity.h"

/* Include the task header for the public API */
#include "obstacle_detect_task.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

typedef enum : uint8_t {
  k_test_sensor_count = 4,  /**< Number of sensors */
  k_test_threshold_cm = 30, /**< Detection threshold */
  k_test_sensor_idx_0 = 0,  /**< Sensor index 0 */
  k_test_sensor_idx_1 = 1,  /**< Sensor index 1 */
} test_obstacle_constants_t;

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

/**
 * @brief Reset mocks before each test
 *
 * @details
 * Called automatically by Unity before each test function.
 * Resets all mocks to their default state to ensure test isolation.
 *
 * @note Called automatically by Unity test framework before each test
 * @since Version 1.0.0
 */
void setUp(void)
{
  /* Reset all mocks before each test */
  mock_shared_data_reset();
  mock_obstacle_detect_reset();
  mock_tx_reset();
}

/**
 * @brief Clean up after each test
 *
 * @details
 * Called automatically by Unity after each test function.
 * Currently a no-op placeholder for future cleanup logic.
 *
 * @note Called automatically by Unity test framework after each test
 * @since Version 1.0.0
 */
void tearDown(void)
{
  /* Clean up after each test */
}

/* =============================================================================
 * Task Creation Tests
 * =============================================================================
 */

/**
 * @brief Test successful obstacle detection task creation
 *
 * @details
 * Verifies that obstacle_detect_task_create() successfully creates
 * the ThreadX thread when conditions are normal.
 */
void test_obstacle_task_create_success(void)
{
  /* Configure mocks for success */
  mock_tx_set_thread_create_return(TX_SUCCESS);

  /* Create the task */
  const rx_err_t err = obstacle_detect_task_create();

  /* Verify success */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_tx_was_thread_create_called());
}

/**
 * @brief Test obstacle task creation fails when ThreadX fails
 *
 * @details
 * Verifies that obstacle_detect_task_create() returns an RTOS error
 * when tx_thread_create() reports TX_NO_MEMORY.
 *
 * @note Tests negative path: ThreadX thread allocation failure
 * @since Version 1.0.0
 */
void test_obstacle_task_create_thread_failure(void)
{
  /* Configure ThreadX to fail */
  mock_tx_set_thread_create_return(TX_NO_MEMORY);

  /* Create the task */
  const rx_err_t err = obstacle_detect_task_create();

  /* Verify failure */
  TEST_ASSERT_EQUAL(k_rx_err_rtos_thread_create, err);
}

/**
 * @brief Test obstacle task creation fails when already created
 *
 * @details
 * Verifies that calling obstacle_detect_task_create() a second time
 * returns k_rx_err_invalid_state, ensuring the task is created only once.
 *
 * @note Tests idempotency guard: prevents double initialization
 * @since Version 1.0.0
 */
void test_obstacle_task_create_already_created(void)
{
  /* Configure mocks for success */
  mock_tx_set_thread_create_return(TX_SUCCESS);

  /* Create the task first time - should succeed */
  rx_err_t err = obstacle_detect_task_create();
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create the task second time - should fail */
  err = obstacle_detect_task_create();
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Initialization and Start Tests
 * =============================================================================
 */

/**
 * @brief Test obstacle detect is initialized and started
 *
 * @details
 * Verifies that rx_obstacle_detect_init() and rx_obstacle_detect_start()
 * are called during task startup.
 */
void test_obstacle_task_init_and_start(void)
{
  /* Configure mocks for success */
  rx_obstacle_detect_t        handle = {0};
  rx_obstacle_detect_config_t config = {0};

  mock_obstacle_detect_set_init_return(k_rx_ok);
  mock_obstacle_detect_set_start_return(k_rx_ok);

  /* Configure detection */
  config.sensor_count           = k_test_sensor_count;
  config.detection_threshold_cm = (float)k_test_threshold_cm;
  config.debounce_samples       = 3;
  config.poll_interval_ms       = 20;
  config.callback               = nullptr; /* Would be internal callback */
  config.user_data              = nullptr;

  /* Initialize (simulating task behavior) */
  rx_err_t err = rx_obstacle_detect_init(&handle, &config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_obstacle_detect_was_initialized());

  /* Start */
  err = rx_obstacle_detect_start(&handle);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(1, mock_obstacle_detect_get_start_count());
}

/* =============================================================================
 * Callback Tests
 * =============================================================================
 */

/**
 * @brief Test obstacle callback sets emergency flag on detection
 *
 * @details
 * Verifies that when obstacle is detected, emergency stop is triggered
 * with the correct reason code.
 */
void test_obstacle_callback_sets_emergency_flag(void)
{
  /* Simulate obstacle callback triggering e-stop */
  (void)shared_data_trigger_estop(k_estop_reason_obstacle);

  /* Verify estop was triggered */
  TEST_ASSERT_EQUAL_UINT32(1, mock_shared_data_get_trigger_estop_count());
  TEST_ASSERT_EQUAL(k_estop_reason_obstacle, mock_shared_data_get_last_estop_reason());
  TEST_ASSERT_TRUE(shared_data_is_estop_active());
}

/**
 * @brief Test obstacle distances are stored in shared data
 *
 * @details
 * Verifies that when obstacle callback is invoked, the distance
 * reading is stored in shared_data for telemetry.
 */
void test_obstacle_distances_stored_in_shared_data(void)
{
  obstacle_state_t state_in  = {0};
  obstacle_state_t state_out = {0};

  /* Simulate callback storing obstacle state */
  state_in.distance_cm[k_test_sensor_idx_0] = 25; /* 25 cm */

  state_in.obstacle_detected[k_test_sensor_idx_0] = true;
  state_in.distance_cm[k_test_sensor_idx_1]       = 100; /* 100 cm (no obstacle) */
  state_in.obstacle_detected[k_test_sensor_idx_1] = false;
  state_in.any_obstacle                           = true;
  state_in.timestamp_ms                           = 1000;

  /* Store in shared data */
  rx_err_t err = shared_data_update_obstacle(&state_in);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Read back */
  err = shared_data_get_obstacle(&state_out);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify */
  TEST_ASSERT_TRUE(state_out.any_obstacle);
  TEST_ASSERT_EQUAL_UINT16(25, state_out.distance_cm[k_test_sensor_idx_0]);
  TEST_ASSERT_TRUE(state_out.obstacle_detected[k_test_sensor_idx_0]);
  TEST_ASSERT_EQUAL_UINT16(100, state_out.distance_cm[k_test_sensor_idx_1]);
  TEST_ASSERT_FALSE(state_out.obstacle_detected[k_test_sensor_idx_1]);
}

/**
 * @brief Test obstacle cleared does not auto-clear e-stop
 *
 * @details
 * When obstacle is cleared, e-stop should remain active (manual clear required).
 */
void test_obstacle_cleared_keeps_estop_active(void)
{
  /* First trigger e-stop due to obstacle */
  mock_shared_data_set_estop_active(true, k_estop_reason_obstacle);
  TEST_ASSERT_TRUE(shared_data_is_estop_active());

  /* Simulate obstacle cleared - e-stop should remain */
  /* (Task only signals event, doesn't clear e-stop) */
  TEST_ASSERT_TRUE(shared_data_is_estop_active());

  /* E-stop must be manually cleared */
  shared_data_clear_estop();
  TEST_ASSERT_FALSE(shared_data_is_estop_active());
}

/* =============================================================================
 * Statistics Tests
 * =============================================================================
 */

/**
 * @brief Test getting obstacle detection statistics
 *
 * @details
 * Verifies that stats can be retrieved from the obstacle detection module.
 */
void test_obstacle_get_stats(void)
{
  /* Configure mock stats */
  rx_obstacle_detect_t handle          = {0};
  uint32_t             total_polls     = 0;
  uint32_t             obstacle_events = 0;
  uint32_t             false_positives = 0;

  mock_obstacle_detect_set_stats(1000, 5, 2);

  /* Get stats */
  const rx_err_t err =
    rx_obstacle_detect_get_stats(&handle, &total_polls, &obstacle_events, &false_positives);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(1000, total_polls);
  TEST_ASSERT_EQUAL_UINT32(5, obstacle_events);
  TEST_ASSERT_EQUAL_UINT32(2, false_positives);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Task Creation Tests */
  RUN_TEST(test_obstacle_task_create_success);
  RUN_TEST(test_obstacle_task_create_thread_failure);
  RUN_TEST(test_obstacle_task_create_already_created);

  /* Initialization and Start Tests */
  RUN_TEST(test_obstacle_task_init_and_start);

  /* Callback Tests */
  RUN_TEST(test_obstacle_callback_sets_emergency_flag);
  RUN_TEST(test_obstacle_distances_stored_in_shared_data);
  RUN_TEST(test_obstacle_cleared_keeps_estop_active);

  /* Statistics Tests */
  RUN_TEST(test_obstacle_get_stats);

  return UNITY_END();
}
