/**
 * @file app_main_task.h
 * @brief Main Application Task - ThreadX Task Manager and System Coordinator
 *
 * @details
 * Declares the main application task that serves as the central coordinator
 * for the STAR firmware. This task is responsible for creating and managing
 * all other ThreadX tasks in the system.
 *
 * **Responsibilities:**
 * - Create and initialize all application tasks
 * - Monitor system health and status
 * - Coordinate inter-task communication
 * - Handle system-level events and errors
 *
 * **Task Hierarchy:**
 * ```
 * main() → tx_kernel_enter() → AppMainTask (this)
 *                                    ↓
 *                              [Creates all other tasks]
 *                                    ↓
 *          MotorTask, CommTask, BMSTask, ObstacleTask, TelemetryTask, TempTask
 *```
 *
 * @see app_main_task.c Implementation
 * @see main.c Application entry point
 *
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_APP_MAIN_TASK_H
#define STAR_APP_MAIN_TASK_H

#include "rx_err.h"

/**
 * @brief Create and start the main application task
 *
 * @details
 * Creates the AppMainTask ThreadX task which serves as the system coordinator.
 * This task must be created first before any other application tasks.
 *
 * **Task Configuration:**
 * - Priority: 5 (high priority for system coordination)
 * - Stack: 2048 bytes
 * - Auto-start: Enabled
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_* ThreadX task creation failed
 *
 * @pre ThreadX kernel entered (tx_kernel_enter() called)
 * @post AppMainTask created and running
 * @post All other application tasks will be created by this task
 *
 * @note Call this from tx_application_define() or main()
 * @note This task creates all other tasks - do not create tasks before calling this
 *
 * @see app_main_task.c Implementation details
 * @see tx_application_define() ThreadX application initialization
 *
 * @since STAR v1.0.0
 */
rx_err_t app_main_task_create(void);

#endif /* STAR_APP_MAIN_TASK_H */
