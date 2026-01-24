/* include/tasks/app_main_task.h */

/**
 * @file app_main_task.h
 * @brief Application main ThreadX task definition
 *
 * Provides the primary application thread creation entry point.
 *
 * @date 2026-01-14
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_INCLUDE_TASKS_APP_MAIN_TASK_H
#define STAR_RX72N_INCLUDE_TASKS_APP_MAIN_TASK_H

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the main application thread
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if the task was already created
 * @return k_rx_err_rtos_error if ThreadX thread creation fails
 */
rx_err_t app_main_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_INCLUDE_TASKS_APP_MAIN_TASK_H */
