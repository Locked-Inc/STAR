/**
 * @file system_health.c
 * @brief System Health thread implementation
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/system_health.h"
#include "motor_config.h"
#include "shared_state.h"
#include "rx_log.h"
#include <string.h>

static const char* s_tag = "sys_health";

static TX_THREAD s_system_health_thread;
static uint8_t   s_system_health_stack[k_stack_system_health];

static void     system_health_entry(ULONG input);
static rx_err_t poll_battery(void);
static rx_err_t update_diagnostics(void);

UINT system_health_create(void)
{
    UINT status = tx_thread_create(&s_system_health_thread, "SysHealth", system_health_entry, 0,
                                    s_system_health_stack, k_stack_system_health,
                                    k_priority_system_health, k_priority_system_health,
                                    TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Thread creation failed");
        return status;
    }

    rx_log_info(s_tag, "System_Health thread created (priority 9, 1 Hz)");
    return TX_SUCCESS;
}

static void system_health_entry(ULONG input)
{
    (void)input;

    rx_log_info(s_tag, "System_Health thread started");

    /* Main health monitoring loop (1 Hz = 1000ms period) */
    while (1) {
        poll_battery();
        update_diagnostics();

        /* TODO: Issue 21 - Register guard integration
         * Call rx_register_guard_refresh() every 1 second
         * Restores corrupted registers from ESD/EMI bit flips
         */

        tx_thread_sleep(100); /* 100 ticks = 1 second */
    }
}

static rx_err_t poll_battery(void)
{
    /* TODO: Issue 16 - Battery monitoring (BQ4050)
     * 1. Poll BQ4050 fuel gauge via I2C
     * 2. Update health.battery_voltage_v (with health_mutex)
     * 3. Update health.battery_soc_percent
     * 4. Update health.battery_current_ma
     * 5. Set health.battery_low if SOC < 20%
     */

    return k_rx_ok;
}

static rx_err_t update_diagnostics(void)
{
    /* TODO: Issue 19 - Diagnostics update
     * 1. Read CPU usage (ThreadX API)
     * 2. Update health.uptime_ms
     * 3. Read free heap (if any dynamic allocation)
     * 4. Update diagnostic counters
     */

    return k_rx_ok;
}
