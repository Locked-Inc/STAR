/**
 * @file bms_monitor_task.h
 * @brief Battery Management System (BMS) Monitoring Task
 *
 * @details
 * Declares the BMS monitoring task responsible for reading battery status
 * from the BQ4050 fuel gauge IC and monitoring battery health.
 *
 * **Responsibilities:**
 * - Read battery voltage, current, temperature, state-of-charge (SOC)
 * - Monitor battery health and safety conditions
 * - Detect fault conditions (over-voltage, over-current, over-temperature)
 * - Update shared battery state for other tasks
 * - Trigger emergency stop on critical battery faults
 *
 * **Communication:**
 * - Hardware: BQ4050 via SMBus (I2C with PEC)
 * - Shared data: Updates g_shared_data.battery_* fields
 * - Fault handling: Sets emergency stop flag on critical faults
 *
 * @see bms_monitor_task.c Implementation
 * @see rx_bq4050.h BQ4050 fuel gauge driver
 * @see shared_data.h Shared battery state
 *
 * @copyright Copyright (c) 2026 STAR Project
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum bms_soc_range_t
 * @brief Valid input ranges for SoC threshold fields in bms_monitor_config_t
 *
 * @details
 * Defines the legal closed intervals for the configurable SoC thresholds.
 * bms_monitor_task_create() enforces these bounds before the task is started;
 * a value outside these ranges returns k_rx_err_invalid_arg.
 *
 * @invariant k_bms_soc_warning_min < k_bms_soc_warning_max
 * @invariant k_bms_soc_critical_min < k_bms_soc_critical_max
 *
 * @code
 * // Range check mirrored in bms_monitor_task_create() and mock_tasks.c:
 * if (config->soc_warning_pct  < k_bms_soc_warning_min  ||
 *     config->soc_warning_pct  > k_bms_soc_warning_max  ||
 *     config->soc_critical_pct < k_bms_soc_critical_min ||
 *     config->soc_critical_pct > k_bms_soc_critical_max) { return k_rx_err_invalid_arg; }
 * @endcode
 *
 * @see bms_monitor_task_create() Enforces these ranges on the config argument
 * @see bms_monitor_config_t Configuration structure using these bounds
 *
 * @since Version 1.1.0
 */
typedef enum : uint8_t {
  k_bms_soc_warning_min  = 1,   /**< Minimum legal soc_warning_pct  (%) */
  k_bms_soc_warning_max  = 100, /**< Maximum legal soc_warning_pct  (%) */
  k_bms_soc_critical_min = 1,   /**< Minimum legal soc_critical_pct (%) */
  k_bms_soc_critical_max = 99,  /**< Maximum legal soc_critical_pct (%) */
} bms_soc_range_t;

/**
 * @enum bms_soc_threshold_defaults_t
 * @brief Default SoC threshold percentages for BMS monitoring
 *
 * @details
 * Defines the default warning and critical thresholds for state-of-charge
 * monitoring. These values are used when bms_monitor_config_t is initialized
 * with default values.
 *
 * | Threshold | Value | Action |
 * |-----------|-------|--------|
 * | Warning   | 25%   | Log warning, set k_event_low_battery |
 * | Critical  | 5%    | Emergency stop (k_estop_reason_low_battery) |
 *
 * @invariant k_bms_soc_critical_pct_default < k_bms_soc_warning_pct_default
 *
 * @code
 * bms_monitor_config_t config = {
 *     .soc_warning_pct  = k_bms_soc_warning_pct_default,
 *     .soc_critical_pct = k_bms_soc_critical_pct_default,
 * };
 * @endcode
 *
 * @see bms_soc_range_t Valid input ranges for these threshold fields
 * @see bms_monitor_config_t Configuration structure using these defaults
 * @see bms_monitor_task_create() Task creation consuming this config
 *
 * @since Version 1.1.0
 */
typedef enum : uint8_t {
  k_bms_soc_warning_pct_default  = 25, /**< Default SoC warning threshold (%) - operator alert */
  k_bms_soc_critical_pct_default = 5,  /**< Default SoC critical threshold (%) - emergency stop */
} bms_soc_threshold_defaults_t;

/* =============================================================================
 * Data Structures
 * =============================================================================
 */

/**
 * @struct bms_monitor_config_t
 * @brief Configurable parameters for the BMS monitoring task
 *
 * @details
 * Provides runtime-configurable thresholds for BMS SoC monitoring. Pass this
 * struct to bms_monitor_task_create() to override the compiled-in defaults.
 *
 * **Field Constraints:**
 * - soc_warning_pct must be in [1, 100] (percent)
 * - soc_critical_pct must be in [1, 99] (percent)
 * - soc_critical_pct MUST be strictly less than soc_warning_pct
 *
 * @invariant soc_critical_pct < soc_warning_pct
 * @invariant soc_warning_pct <= 100
 * @invariant soc_critical_pct >= 1
 *
 * @code
 * // Using defaults (25% warning, 5% critical)
 * bms_monitor_config_t config = {
 *     .soc_warning_pct  = k_bms_soc_warning_pct_default,
 *     .soc_critical_pct = k_bms_soc_critical_pct_default,
 * };
 * rx_err_t err = bms_monitor_task_create(&config);
 *
 * // Custom thresholds (30% warning, 10% critical)
 * bms_monitor_config_t custom = {
 *     .soc_warning_pct  = 30,
 *     .soc_critical_pct = 10,
 * };
 * rx_err_t err = bms_monitor_task_create(&custom);
 * @endcode
 *
 * @see bms_soc_threshold_defaults_t Default values for these fields
 * @see bms_monitor_task_create() Consumes this configuration
 *
 * @since Version 1.1.0
 */
typedef struct {
  uint8_t soc_warning_pct;  /**< SoC % below which warning is logged and k_event_low_battery is
                                 set. Valid range: [1, 100]. Default: k_bms_soc_warning_pct_default
                                 (25%). */
  uint8_t soc_critical_pct; /**< SoC % below which emergency stop is triggered. Valid range: [1,
                                 99], must be < soc_warning_pct. Default:
                                 k_bms_soc_critical_pct_default (5%). */
} bms_monitor_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Create and start the BMS monitoring task with configurable thresholds
 *
 * @details
 * Creates the BMSTask ThreadX task for battery monitoring. This task
 * periodically reads the BQ4050 fuel gauge and updates battery state.
 * The caller supplies a bms_monitor_config_t to configure SoC warning
 * and critical thresholds.
 *
 * **Task Configuration:**
 * - Priority: 15 (low priority - not time-critical)
 * - Stack: 1024 bytes
 * - Period: 1 Hz (1 second monitoring rate)
 * - Auto-start: Enabled
 *
 * **SoC Threshold Behaviour:**
 * - If SoC falls below config->soc_warning_pct:  log warning, set k_event_low_battery
 * - If SoC falls below config->soc_critical_pct: log error, trigger
 *   shared_data_trigger_estop(k_estop_reason_low_battery)
 *
 * @param[in] config Monitoring configuration with SoC thresholds.
 *                   Must not be NULL.
 *                   config->soc_warning_pct must be in [1, 100].
 *                   config->soc_critical_pct must be in [1, 99] and strictly
 *                   less than soc_warning_pct.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_null_ptr config pointer is NULL
 * @retval k_rx_err_invalid_arg Threshold values violate constraints
 * @retval k_rx_err_invalid_state Task already created (call once only)
 * @retval k_rx_err_rtos_thread_create ThreadX task creation failed
 *
 * @pre ThreadX kernel running
 * @pre config != NULL
 * @pre config->soc_critical_pct < config->soc_warning_pct
 * @pre shared_data_init() called before this function
 * @pre This function has not been called before (single-shot)
 *
 * @post BMSTask created and running at 1 Hz
 * @post Battery state updated in shared memory every second
 * @post SoC monitoring active with supplied thresholds
 *
 * @note Call this from tx_application_define() after hardware initialization
 * @note Task auto-starts - no need to manually resume
 * @note This function is single-shot; subsequent calls return k_rx_err_invalid_state
 *
 * @warning Never call twice - assertion fires on second call in debug builds
 * @warning config must remain valid only for the duration of this call
 *          (thresholds are copied into the task's internal state)
 *
 * @par Example:
 * @code
 * // In tx_application_define():
 * bms_monitor_config_t bms_config = {
 *     .soc_warning_pct  = k_bms_soc_warning_pct_default,
 *     .soc_critical_pct = k_bms_soc_critical_pct_default,
 * };
 * rx_err_t err = bms_monitor_task_create(&bms_config);
 * if (err != k_rx_ok) {
 *     rx_log_error("INIT", "BMS task create failed");
 * }
 * @endcode
 *
 * @see bms_monitor_task.c Implementation details
 * @see bms_monitor_config_t Configuration structure
 * @see bms_soc_threshold_defaults_t Default threshold values
 * @see app_main_task.c Task creation coordinator
 *
 * @since Version 1.1.0
 */
rx_err_t bms_monitor_task_create(const bms_monitor_config_t* config);
