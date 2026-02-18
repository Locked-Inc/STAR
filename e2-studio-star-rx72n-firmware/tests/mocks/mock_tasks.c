/* tests/mocks/mock_tasks.c */

/**
 * @file mock_tasks.c
 * @brief Mock Task Create Functions for Unit Testing
 *
 * @details
 * Provides stub implementations of task creation functions that use
 * the ThreadX mock to simulate task creation behavior.
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include <stdbool.h>

#include "bms_monitor_task.h"
#include "rx_err.h"
#include "tx_api.h"

/* =============================================================================
 * Static State - Track if tasks are already created
 * =============================================================================
 */

static bool s_motor_task_created     = false;
static bool s_comm_task_created      = false;
static bool s_obstacle_task_created  = false;
static bool s_bms_task_created       = false;
static bool s_temp_task_created      = false;
static bool s_telemetry_task_created = false;

/* ThreadX thread structures (mocked) */
static TX_THREAD s_motor_thread;
static TX_THREAD s_comm_thread;
static TX_THREAD s_obstacle_thread;
static TX_THREAD s_bms_thread;
static TX_THREAD s_temp_thread;
static TX_THREAD s_telemetry_thread;

/* =============================================================================
 * Mock Reset Function (called by mock_tx_reset)
 * =============================================================================
 */

void mock_tasks_reset(void)
{
  s_motor_task_created     = false;
  s_comm_task_created      = false;
  s_obstacle_task_created  = false;
  s_bms_task_created       = false;
  s_temp_task_created      = false;
  s_telemetry_task_created = false;
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
 * @brief Mock BMS monitoring task create
 */
rx_err_t bms_monitor_task_create(const bms_monitor_config_t* config)
{
  tx_status status = TX_SUCCESS; /**< ThreadX thread-create return code; initialised to TX_SUCCESS */

  if (config == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((config->soc_warning_pct < k_bms_soc_warning_min) ||
      (config->soc_warning_pct > k_bms_soc_warning_max)) {
    return k_rx_err_invalid_arg;
  }

  if ((config->soc_critical_pct < k_bms_soc_critical_min) ||
      (config->soc_critical_pct > k_bms_soc_critical_max)) {
    return k_rx_err_invalid_arg;
  }

  if (config->soc_critical_pct >= config->soc_warning_pct) {
    return k_rx_err_invalid_arg;
  }

  if (s_bms_task_created) {
    return k_rx_err_invalid_state;
  }

  status = tx_thread_create(&s_bms_thread,
                            "BMS",
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

  s_bms_task_created = true;
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
