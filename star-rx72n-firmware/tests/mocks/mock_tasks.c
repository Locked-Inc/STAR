/**
 * @file mock_tasks.c
 * @brief Mock Task Create Functions for Unit Testing
 *
 * @details
 * Provides stub implementations of task creation functions that use
 * the ThreadX mock to simulate task creation behavior.
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_err.h"
#include "tx_api.h"

/* =============================================================================
 * Static State - Track if tasks are already created
 * =============================================================================
 */

static bool s_motor_task_created     = false;
static bool s_comm_task_created      = false;
static bool s_obstacle_task_created  = false;
static bool s_temp_task_created      = false;
static bool s_telemetry_task_created = false;
static bool s_imu_task_created       = false;

/* ThreadX thread structures (mocked) */
static TX_THREAD s_motor_thread;
static TX_THREAD s_comm_thread;
static TX_THREAD s_obstacle_thread;
static TX_THREAD s_temp_thread;
static TX_THREAD s_telemetry_thread;
static TX_THREAD s_imu_thread;

/* ThreadX event flags structures (mocked) */
static TX_EVENT_FLAGS_GROUP s_imu_event_flags;

/* =============================================================================
 * Mock Reset Function (called by mock_tx_reset)
 * =============================================================================
 */

void mock_tasks_reset(void)
{
  s_motor_task_created     = false;
  s_comm_task_created      = false;
  s_obstacle_task_created  = false;
  s_temp_task_created      = false;
  s_telemetry_task_created = false;
  s_imu_task_created       = false;
}

/* =============================================================================
 * Task Create Function Stubs
 * =============================================================================
 */

/**
 * @brief Mock motor control task create
 */
rx_err_t motor_control_task_create(void)
{
  tx_status status;

  if (s_motor_task_created) {
    return k_rx_err_invalid_state;
  }

  status = tx_thread_create(&s_motor_thread,
                            "Motor",
                            nullptr,
                            0,
                            NULL,
                            0,
                            0,
                            0,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);

  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_thread_create;
  }

  s_motor_task_created = true;
  return k_rx_ok;
}

/**
 * @brief Mock communication task create
 */
rx_err_t comm_task_create(void)
{
  tx_status status;

  if (s_comm_task_created) {
    return k_rx_err_invalid_state;
  }

  status = tx_thread_create(&s_comm_thread,
                            "Comm",
                            nullptr,
                            0,
                            NULL,
                            0,
                            0,
                            0,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);

  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_thread_create;
  }

  s_comm_task_created = true;
  return k_rx_ok;
}

/**
 * @brief Mock obstacle detection task create
 */
rx_err_t obstacle_detect_task_create(void)
{
  tx_status status;

  if (s_obstacle_task_created) {
    return k_rx_err_invalid_state;
  }

  status = tx_thread_create(&s_obstacle_thread,
                            "Obstacle",
                            nullptr,
                            0,
                            NULL,
                            0,
                            0,
                            0,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);

  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_thread_create;
  }

  s_obstacle_task_created = true;
  return k_rx_ok;
}

/**
 * @brief Mock temperature sensor task create
 */
rx_err_t temp_sensor_task_create(void)
{
  tx_status status;

  if (s_temp_task_created) {
    return k_rx_err_invalid_state;
  }

  status = tx_thread_create(&s_temp_thread,
                            "Temp",
                            nullptr,
                            0,
                            NULL,
                            0,
                            0,
                            0,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);

  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_thread_create;
  }

  s_temp_task_created = true;
  return k_rx_ok;
}

/**
 * @brief Mock IMU task create
 *
 * @details
 * Stub for imu_task_create() used by tests that test other tasks
 * (motor, comm, obstacle, temp, telemetry) that reference imu_task.
 * The real imu_task_create() is tested in test_imu_task.c.
 */
rx_err_t imu_task_create(void)
{
  tx_status status;

  if (s_imu_task_created) {
    return k_rx_err_invalid_state;
  }

  /* Create event flags group (matches real imu_task_create behavior) */
  const tx_status ef_status = tx_event_flags_create(&s_imu_event_flags, "imu_int_flags");
  if (ef_status != TX_SUCCESS) {
    return k_rx_err_rtos_thread_create;
  }

  status = tx_thread_create(&s_imu_thread,
                            "ImuTask",
                            nullptr,
                            0,
                            NULL,
                            0,
                            0,
                            0,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);

  if (status != TX_SUCCESS) {
    (void)tx_event_flags_delete(&s_imu_event_flags);
    return k_rx_err_rtos_thread_create;
  }

  s_imu_task_created = true;
  return k_rx_ok;
}

/**
 * @brief Mock telemetry task create
 */
rx_err_t telemetry_task_create(void)
{
  tx_status status;

  if (s_telemetry_task_created) {
    return k_rx_err_invalid_state;
  }

  status = tx_thread_create(&s_telemetry_thread,
                            "Telemetry",
                            nullptr,
                            0,
                            NULL,
                            0,
                            0,
                            0,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);

  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_thread_create;
  }

  s_telemetry_task_created = true;
  return k_rx_ok;
}
