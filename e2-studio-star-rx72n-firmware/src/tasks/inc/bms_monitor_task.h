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
 */

#pragma once

#include "rx_err.h"

/**
 * @brief Create and start the BMS monitoring task
 *
 * @details
 * Creates the BMSTask ThreadX task for battery monitoring. This task
 * periodically reads the BQ4050 fuel gauge and updates battery state.
 *
 * **Task Configuration:**
 * - Priority: 10 (medium priority - not time-critical)
 * - Stack: 2048 bytes
 * - Period: 100 ms (10 Hz monitoring rate)
 * - Auto-start: Enabled
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_* ThreadX task creation failed
 *
 * @pre ThreadX kernel running
 * @pre BQ4050 initialized via rx_bq4050_init()
 * @pre Shared data initialized
 *
 * @post BMSTask created and running at 10 Hz
 * @post Battery state updated in shared memory
 *
 * @note Call this from AppMainTask after hardware initialization
 * @note Task auto-starts - no need to manually resume
 *
 * @see bms_monitor_task.c Implementation details
 * @see app_main_task.c Task creation coordinator
 *
 * @since STAR v1.0.0
 */
rx_err_t bms_monitor_task_create(void);
