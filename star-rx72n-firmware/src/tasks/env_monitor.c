/* star-rx72n-firmware/src/tasks/env_monitor.c */

/**
 * @file env_monitor.c
 * @brief Environment Monitor thread implementation
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/env_monitor.h"
#include "motor_config.h"
#include "shared_state.h"
#include "rx_log.h"
#include <string.h>

static const char* s_tag = "env_mon";

static TX_THREAD s_env_monitor_thread;
static uint8_t   s_env_monitor_stack[k_stack_env_monitor];

static void     env_monitor_entry(ULONG input);
static rx_err_t scan_obstacles(void);
static rx_err_t monitor_temperature(void);

UINT env_monitor_create(void)
{
    UINT status = tx_thread_create(&s_env_monitor_thread, s_tag, env_monitor_entry, 0,
                                    s_env_monitor_stack, k_stack_env_monitor,
                                    k_priority_env_monitor, k_priority_env_monitor,
                                    TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Thread creation failed");
        return status;
    }

    rx_log_info(s_tag, "Env_Monitor thread created (priority 5, 50 Hz)");
    return TX_SUCCESS;
}

static void env_monitor_entry(ULONG input)
{
    (void)input;

    rx_log_info(s_tag, "Env_Monitor thread started");

    /* Main monitoring loop (50 Hz = 20ms period) */
    while (1) {
        scan_obstacles();
        monitor_temperature();
        tx_thread_sleep(2); /* 2 ticks = 20ms */
    }
}

static rx_err_t scan_obstacles(void)
{
    /* Stub implementation - Issue 14:
     * HC-SR04 obstacle detection with E-STOP trigger */

    return k_rx_ok;
}

static rx_err_t monitor_temperature(void)
{
    /* Stub implementation - Issue 15:
     * DS18B20 temperature monitoring with thermal protection */

    return k_rx_ok;
}
