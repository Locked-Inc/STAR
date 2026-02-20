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
 * @brief Mock implementation of bms_monitor_task_create for unit tests
 *
 * @details
 * Stub implementation that validates the same preconditions as the production
 * function (NULL check, SoC range checks, threshold relationship check, and
 * singleton guard), then calls the ThreadX mock's tx_thread_create() to
 * exercise thread-creation error paths. No real task is spawned; the thread
 * structure exists only in mock memory.
 *
 * @param[in] config Monitoring configuration. Must not be NULL.
 *                   config->soc_warning_pct must be in [k_bms_soc_warning_min,
 *                   k_bms_soc_warning_max] and config->soc_critical_pct must
 *                   be in [k_bms_soc_critical_min, k_bms_soc_critical_max] and
 *                   strictly less than config->soc_warning_pct.
 *
 * @return rx_err_t Mock task creation status
 * @retval k_rx_ok              Mock thread created successfully
 * @retval k_rx_err_null_ptr    config is NULL
 * @retval k_rx_err_invalid_arg soc_warning_pct outside [k_bms_soc_warning_min,
 *                              k_bms_soc_warning_max], or soc_critical_pct
 *                              outside [k_bms_soc_critical_min,
 *                              k_bms_soc_critical_max], or soc_critical_pct
 *                              >= soc_warning_pct
 * @retval k_rx_err_invalid_state Mock task already created (singleton guard)
 * @retval k_rx_err_rtos_thread_create tx_thread_create() returned error
 *                                     (injected via mock_tx_set_thread_create_return())
 *
 * @pre config != NULL
 * @pre config->soc_warning_pct in [k_bms_soc_warning_min, k_bms_soc_warning_max]
 * @pre config->soc_critical_pct in [k_bms_soc_critical_min, k_bms_soc_critical_max]
 * @pre config->soc_critical_pct < config->soc_warning_pct
 * @pre mock_tasks_reset() called via mock_tx_reset() before each test
 *
 * @post On success: s_bms_task_created == true, s_bms_thread populated
 * @post On failure: s_bms_task_created unchanged (remains false)
 * @post No real ThreadX thread is created; task entry point is nullptr
 *
 * @note This is a test/mock implementation. It does NOT start a real BMS
 *       monitoring loop, initialize the BQ4050, or write to shared data.
 * @note Thread-unsafe; intended for single-threaded test use only.
 *
 * @code
 * // Inject thread-create failure and verify error propagation:
 * mock_tx_set_thread_create_return(TX_NO_MEMORY);
 * const bms_monitor_config_t config = {
 *     .soc_warning_pct  = k_bms_soc_warning_pct_default,
 *     .soc_critical_pct = k_bms_soc_critical_pct_default,
 * };
 * rx_err_t err = bms_monitor_task_create(&config);
 * TEST_ASSERT_EQUAL(k_rx_err_rtos_thread_create, err);
 * @endcode
 *
 * @see bms_monitor_config_t         Configuration structure
 * @see bms_soc_threshold_defaults_t Default threshold constants
 * @see bms_soc_range_t              Valid input ranges (k_bms_soc_warning_min/max,
 *                                   k_bms_soc_critical_min/max)
 * @see mock_tx_set_thread_create_return() Inject tx_thread_create() result
 * @see mock_tasks_reset()           Reset singleton guard between tests
 *
 * @since Version 1.1.0
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
 * @brief Reset BMS mock singleton guard for unit test isolation
 *
 * @details
 * Clears the s_bms_task_created guard so that bms_monitor_task_create() may
 * be called again in a subsequent test. Called from tearDown() to ensure each
 * test starts with a clean creation state.
 *
 * @return void
 * @retval None
 *
 * @pre Called from tearDown() after a test that invoked bms_monitor_task_create()
 * @pre No real thread is running (mock environment is single-threaded)
 *
 * @post s_bms_task_created == false
 * @post bms_monitor_task_create() may be called again in the next test
 *
 * @note NOT thread-safe; intended for single-threaded test context only.
 * @warning Do not call from production code or interrupt context.
 *
 * @see bms_monitor_task_create() The mock function whose guard this resets
 * @see mock_tasks_reset()        Also resets this guard (called via mock_tx_reset)
 *
 * @since Version 1.1.0
 */
void bms_monitor_task_reset(void)
{
  s_bms_task_created = false;
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
