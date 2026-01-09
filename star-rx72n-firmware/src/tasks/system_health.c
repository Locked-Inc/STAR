/* src/tasks/system_health.c */

/**
 * @file system_health.c
 * @brief System Health thread implementation
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/system_health.h"
#include "diagnostics.h"
#include "motor_config.h"
#include "shared_state.h"
#include "rx_bq4050.h"
#include "rx_bus_manager.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_register_guard.h"
#include <string.h>

static char s_tag[] = "sys_health";

static TX_THREAD s_system_health_thread;
static uint8_t   s_system_health_stack[k_stack_system_health];

/* Bus manager for I2C battery communication */
static rx_bus_manager_t s_bus_manager;

/* System uptime tracking */
static uint32_t s_startup_time_ms = 0;

static void     system_health_entry(ULONG input);
static rx_err_t init_battery_comm(void);
static rx_err_t poll_battery(void);
static rx_err_t update_diagnostics(void);

UINT system_health_create(void)
{
    UINT status = tx_thread_create(&s_system_health_thread, s_tag, system_health_entry, 0,
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

    /* Register with watchdog for task-level monitoring
     * Timeout = 3x period = 3 * 1000ms = 3000ms */
    rx_err_t ret = rx_iwdt_register_task("System_Health", 3000);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Failed to register with watchdog");
    }

    /* Initialize battery communication */
    ret = init_battery_comm();
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Battery comm init failed (continuing without battery)");
        /* Continue even if battery init fails - not critical */
    }

    /* Record startup time for uptime calculation */
    s_startup_time_ms = tx_time_get();

    rx_log_info(s_tag, "Entering main health monitoring loop");

    /* Main health monitoring loop (1 Hz = 1000ms period) */
    while (1) {
        poll_battery();
        update_diagnostics();

        /* Register guard: Restore corrupted registers from ESD/EMI bit flips
         * Called every 1 second as recommended by rx_register_guard.h */
        rx_register_guard_refresh();

        /* Track register corrections in diagnostics */
        const uint32_t corrections = rx_register_guard_get_correction_count();
        if (corrections > 0) {
            diagnostics_increment_register_correction();
            rx_register_guard_reset_count();
        }

        /* Check for task deadlocks (Issue 20: Enhanced Watchdog)
         * This runs at low priority to detect if higher priority tasks are stuck */
        ret = rx_iwdt_check_tasks();
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "Task deadlock detected");
            diagnostics_log_fault(s_tag, "Task deadlock detected", ret);
        }

        /* Feed watchdog (critical - prevents reset) */
        rx_iwdt_feed();

        /* Record task heartbeat for deadlock detection */
        rx_iwdt_task_heartbeat("System_Health");

        tx_thread_sleep(k_threadx_ticks_1s); /* 1 second = 1 Hz polling rate */
    }
}

static rx_err_t init_battery_comm(void)
{
    /* -------------------------------------------------------------------------
     * Initialize Bus Manager for I2C Battery Communication
     * -------------------------------------------------------------------------
     * BQ4050 fuel gauge uses SMBus (I2C variant) at 0x0B address
     * RIIC0 at 1 MHz on P12/P13
     */
    rx_log_info(s_tag, "Initializing SMBus for BQ4050 battery");

    /* Initialize bus manager */
    rx_err_t ret = rx_bus_manager_init(&s_bus_manager, s_tag, NULL, NULL);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Bus manager init failed");
        return ret;
    }

    /* Create bus configuration node with SMBus config in proto union */
    static rx_bus_config_t bus_config = {
        .name        = "bms",
        .type        = k_bus_type_smbus,
        .initialized = false,
        .handle      = NULL,
        .user_ctx    = NULL,
        .proto.smbus = {
            .i2c_config = {
                .channel      = 0,                /* RIIC0 */
                .sda_pin      = k_pin_bms_sda,    /* P13 (pin 33) */
                .scl_pin      = k_pin_bms_scl,    /* P12 (pin 34) */
                .frequency_hz = 1000000,          /* 1 MHz Fast Mode Plus */
                .device_addr  = 0x0B,             /* BQ4050 7-bit address */
            },
            .use_pec = true, /* Use Packet Error Checking (CRC-8) for SMBus */
        },
        .next        = NULL,
    };

    /* Register bus with manager */
    ret = rx_bus_manager_add_bus(&s_bus_manager, &bus_config);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Failed to add SMBus to manager");
        return ret;
    }

    /* Initialize BQ4050 fuel gauge */
    ret = rx_bq4050_init(&s_bus_manager, "bms", NULL);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "BQ4050 init failed");
        return ret;
    }

    rx_log_info(s_tag, "SMBus for BQ4050 initialized");

    return k_rx_ok;
}

static rx_err_t poll_battery(void)
{
    /* -------------------------------------------------------------------------
     * Issue 16: Battery Polling Integration
     * -------------------------------------------------------------------------
     * Read BQ4050 fuel gauge via SMBus (I2C variant)
     * Update health state with voltage, SOC, current
     * Set battery_low flag if SOC < 20%
     */

    shared_state_t* state = shared_state_get();

    /* Read battery voltage (returns millivolts) */
    uint16_t voltage_mv = 0;
    rx_err_t ret = rx_bq4050_read_voltage(&s_bus_manager, "bms", &voltage_mv);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Battery voltage read failed");
        return ret;
    }

    /* Read relative state of charge (SOC) */
    uint8_t soc_percent = 0;
    ret = rx_bq4050_read_relative_soc(&s_bus_manager, "bms", &soc_percent);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Battery SOC read failed");
        return ret;
    }

    /* Read current (positive = charging, negative = discharging) */
    int16_t current_ma = 0;
    ret = rx_bq4050_read_current(&s_bus_manager, "bms", &current_ma);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Battery current read failed");
        return ret;
    }

    /* Update health state (mutex-protected) */
    UINT status = tx_mutex_get(&state->health_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Failed to acquire health mutex");
        return k_rx_err_threadx;
    }

    /* Convert voltage from millivolts to volts */
    state->health.battery_voltage_v = (float)voltage_mv / 1000.0f;
    state->health.battery_soc_percent = soc_percent;
    state->health.battery_current_ma = (float)current_ma;

    /* Check for low battery warning */
    if (soc_percent < k_battery_low_soc_percent) {
        state->health.battery_low = true;
        rx_log_error(s_tag, "Low battery warning");
    } else {
        state->health.battery_low = false;
    }

    tx_mutex_put(&state->health_mutex);

    return k_rx_ok;
}

static rx_err_t update_diagnostics(void)
{
    /* -------------------------------------------------------------------------
     * Issue 19: System Diagnostics Update
     * -------------------------------------------------------------------------
     * Calculate system uptime from startup timestamp
     * Update health state with uptime
     * Aggregate diagnostic counters (will be implemented in Phase 5)
     */

    shared_state_t* state = shared_state_get();

    /* Calculate uptime */
    const uint32_t current_time_ms = tx_time_get();
    const uint32_t uptime_ms = current_time_ms - s_startup_time_ms;

    /* Update health state (mutex-protected) */
    UINT status = tx_mutex_get(&state->health_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Failed to acquire health mutex");
        return k_rx_err_threadx;
    }

    state->health.uptime_ms = uptime_ms;

    /* TODO: Aggregate diagnostic counters (Issue 19 - Phase 5)
     * This will include:
     * - Watchdog resets
     * - Motor faults
     * - Communication timeouts
     * - Obstacle events
     * - Register corrections
     * For now, just track uptime */

    tx_mutex_put(&state->health_mutex);

    return k_rx_ok;
}
