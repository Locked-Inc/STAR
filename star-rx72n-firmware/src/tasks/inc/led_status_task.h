/* star-rx72n-firmware/src/tasks/inc/led_status_task.h */

/**
 * @file led_status_task.h
 * @brief LED Status Indicator Task - Visual System Health Feedback
 *
 * @details
 * Declares the LED status task responsible for driving 6 status LEDs
 * based on system state read from shared_data. Provides visual feedback
 * for heartbeat, errors, motor status, communication activity, obstacle
 * detection, and boot/debug indicators.
 *
 * **LED Assignments:**
 * | LED | Pin | Function                    |
 * |-----|-----|-----------------------------|
 * | 0   | P32 | System Heartbeat (1 Hz)     |
 * | 1   | P87 | Error Indicator (blink code)|
 * | 2   | P56 | Motor Active                |
 * | 3   | P55 | Communication Activity      |
 * | 4   | P54 | Obstacle Detected           |
 * | 5   | P52 | E-Stop Active               |
 *
 * @see led_status_task.c Implementation
 * @see shared_data.h Source of system state data
 * @see hardware_config.h LED pin assignments
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include "rx_err.h"

/**
 * @brief Create and start the LED status indicator task
 *
 * @details
 * Creates the LEDTask ThreadX task for visual system health feedback.
 * This task reads system state from shared_data and drives 6 status
 * LEDs at 20 Hz to provide real-time visual indication of system health.
 *
 * **Task Configuration:**
 * - Priority: 17 (low priority - visual feedback only)
 * - Stack: 512 bytes (minimal - GPIO writes only)
 * - Period: 50 ms (20 Hz LED update rate)
 * - Auto-start: Enabled
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_invalid_state Task already created
 * @retval k_rx_err_rtos_thread_create ThreadX task creation failed
 *
 * @pre ThreadX kernel running
 * @pre shared_data initialized
 * @pre LED GPIO pins configured as outputs (hardware_init)
 *
 * @post LEDTask created and running at 20 Hz
 * @post 6 status LEDs actively driven based on system state
 *
 * @note Call from tx_application_define after shared_data_init
 * @note Low priority - visual feedback only, never preempts control tasks
 *
 * @see led_status_task.c Implementation details
 * @see tx_application_define() Task creation coordinator
 *
 * @since Version 1.0.0
 */
rx_err_t led_status_task_create(void);
