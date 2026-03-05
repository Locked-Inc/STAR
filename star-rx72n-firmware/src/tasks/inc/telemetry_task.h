/**
 * @file telemetry_task.h
 * @brief Telemetry Aggregation Task - System Status Collection and Reporting
 *
 * @details
 * Declares the telemetry task responsible for aggregating system status data
 * from all subsystems and sending it to the Raspberry Pi 5.
 *
 * **Responsibilities:**
 * - Collect data from shared memory (motor status, sensors)
 * - Aggregate into Protocol Buffer telemetry messages
 * - Send periodic status updates to RPi5 via SPI
 * - Monitor system health and diagnostic counters
 * - Generate system health reports
 *
 * **Telemetry Data:**
 * - Motor: Velocity, current, encoder position, faults
 * - Sensors: Temperature, obstacle distances
 * - System: CPU usage, task status, error counters
 *
 * @see telemetry_task.c Implementation
 * @see rx_nanopb.h Protocol Buffers encoder
 * @see shared_data.h Source of telemetry data
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "rx_err.h"

/**
 * @brief Create and start the telemetry aggregation task
 *
 * @details
 * Creates the TelemetryTask ThreadX task for system status reporting.
 * This task periodically sends telemetry data to the Raspberry Pi 5.
 *
 * **Task Configuration:**
 * - Priority: 14 (low priority - best-effort telemetry)
 * - Stack: 3072 bytes (Protocol Buffers encoding requires stack)
 * - Period: 50 ms (20 Hz telemetry rate)
 * - Auto-start: Enabled
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_* ThreadX task creation failed
 *
 * @pre ThreadX kernel running
 * @pre SPI communication initialized
 * @pre Protocol Buffers encoder initialized
 * @pre Shared data initialized
 *
 * @post TelemetryTask created and running at 20 Hz
 * @post RPi5 receives periodic status updates
 *
 * @note Call from tx_application_define() in main.c after communication initialization
 * @note Low priority - can be preempted by control tasks
 *
 * @see telemetry_task.c Implementation details
 * @see main.c Task creation in tx_application_define()
 *
 * @since Version 1.0.0
 */
rx_err_t telemetry_task_create(void);
