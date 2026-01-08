/**
 * @file comm_manager.c
 * @brief Communication Manager thread implementation
 * @details
 * Manages USB CDC/SPI communication with RPi5 at 100 Hz.
 * High priority to prevent command lag.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/comm_manager.h"
#include "motor_config.h"
#include "shared_state.h"
#include "rx_log.h"
#include "rx_usb.h"
#include <string.h>

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static const char* s_tag = "comm_mgr";

/* Thread control block and stack */
static TX_THREAD s_comm_manager_thread;
static uint8_t   s_comm_manager_stack[k_stack_comm_manager];

/* Communication state */
static uint32_t s_last_command_timestamp_ms = 0;

/* =============================================================================
 * Private Function Prototypes
 * =============================================================================
 */

static void     comm_manager_entry(ULONG input);
static rx_err_t process_ingress(void);
static rx_err_t process_egress(void);
static rx_err_t check_comm_timeout(void);

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

UINT comm_manager_create(void)
{
    UINT status = TX_SUCCESS;

    /* Create Comm_Manager thread */
    status = tx_thread_create(
        &s_comm_manager_thread,
        "CommMgr",
        comm_manager_entry,
        0,
        s_comm_manager_stack,
        k_stack_comm_manager,
        k_priority_comm_manager,
        k_priority_comm_manager,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Thread creation failed");
        return status;
    }

    rx_log_info(s_tag, "Comm_Manager thread created (priority 3, 100 Hz)");
    return TX_SUCCESS;
}

/* =============================================================================
 * Private Function Implementations
 * =============================================================================
 */

static void comm_manager_entry(ULONG input)
{
    rx_err_t ret = k_rx_ok;

    (void)input;

    rx_log_info(s_tag, "Comm_Manager thread started");

    /* Wait for USB CDC to be configured */
    while (!rx_usb_is_configured()) {
        tx_thread_sleep(10); /* Wait 100ms */
    }

    rx_log_info(s_tag, "USB CDC configured, entering main loop");

    /* Main communication loop (100 Hz = 10ms period) */
    while (1) {
        /* Process incoming commands (ingress) */
        ret = process_ingress();
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "Ingress processing error");
        }

        /* Send telemetry (egress) */
        ret = process_egress();
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "Egress processing error");
        }

        /* Check communication timeout */
        ret = check_comm_timeout();
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "Communication timeout");
        }

        /* Sleep for 10ms (100 Hz) */
        tx_thread_sleep(1); /* 1 tick = 10ms at 100 Hz ThreadX tick */
    }
}

static rx_err_t process_ingress(void)
{
    /* TODO: Issue 11 - Command processing (ingress)
     * 1. Receive frames via USB CDC (rx_usb_read with 100ms timeout)
     * 2. Decode SetVelocityRequest (nanopb deserialize)
     * 3. Validate command ranges (±2.0 m/s, clamp if needed)
     * 4. Update shared setpoint (with setpoint_mutex)
     * 5. Update s_last_command_timestamp_ms
     * 6. Send ACK response with sequence number
     *
     * TODO: Issue 18 - Manual emergency stop clearance
     * 1. Handle ClearEmergencyStop command
     * 2. Check safety conditions (no obstacles, no faults, no timeout)
     * 3. Clear safety.emergency_stop flag (with safety_mutex)
     * 4. Send ClearEmergencyStopResponse with status
     */

    return k_rx_ok;
}

static rx_err_t process_egress(void)
{
    /* TODO: Issue 12 - Telemetry streaming (egress)
     * 1. Read encoder feedback (4 motors) from shared state (encoder_mutex)
     * 2. Read battery/temperature from health state (health_mutex)
     * 3. Encode TelemetryData message (nanopb serialize)
     * 4. Transmit via USB CDC at 100 Hz
     * 5. Include system status (firmware version, uptime, fault flags)
     */

    return k_rx_ok;
}

static rx_err_t check_comm_timeout(void)
{
    /* TODO: Issue 17 - Communication timeout handling
     * 1. Calculate time since last command (current_time - s_last_command_timestamp_ms)
     * 2. If timeout > 500ms:
     *    a. Set safety.comm_timeout = true (with safety_mutex)
     *    b. Set safety.emergency_stop = true (with safety_mutex)
     *    c. Log timeout event
     * 3. If new command received:
     *    a. Clear safety.comm_timeout = false
     *    (Note: E-STOP remains until manual clearance)
     */

    return k_rx_ok;
}
