/* src/tasks/app_main_task.c */

/**
 * @file app_main_task.c
 * @brief Application main ThreadX task implementation
 *
 * @details
 * Provides a minimal application thread that keeps the scheduler active.
 * Application logic will be added as subsystems are integrated.
 *
 * @date 2026-01-14
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "app_main_task.h"

#include <stdbool.h>
#include <stdint.h>

#include "tx_api.h"

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

typedef enum : uint16_t {
  k_app_main_stack_size  = 1024, /**< Stack size in bytes */
  k_app_main_priority    = 10,   /**< ThreadX priority (lower is higher) */
  k_app_main_sleep_ticks = 10,   /**< Sleep period in ThreadX ticks */
} app_main_task_config_t;

/* =============================================================================
 * Static State
 * =============================================================================
 */

static TX_THREAD s_app_main_thread;
static uint8_t   s_app_main_stack[k_app_main_stack_size];
static bool      s_app_main_created = false;

/* =============================================================================
 * Thread Entry
 * =============================================================================
 */

static void internal_app_main_task_entry(ULONG input)
{
  (void)input;

  while (true) {
    tx_thread_sleep(k_app_main_sleep_ticks);
  }
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

rx_err_t app_main_task_create(void)
{
  if (s_app_main_created) {
    return k_rx_err_invalid_state;
  }

  const UINT status = tx_thread_create(&s_app_main_thread,
                                       "AppMain",
                                       internal_app_main_task_entry,
                                       0,
                                       s_app_main_stack,
                                       k_app_main_stack_size,
                                       k_app_main_priority,
                                       k_app_main_priority,
                                       TX_NO_TIME_SLICE,
                                       TX_AUTO_START);

  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  s_app_main_created = true;
  return k_rx_ok;
}
