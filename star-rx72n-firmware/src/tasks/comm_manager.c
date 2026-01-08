/* star-rx72n-firmware/src/tasks/comm_manager.c */

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
        s_tag,
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
    /* Stub implementation - Issues 11, 18:
     * - Command processing via USB CDC/SPI (Issue 11)
     * - Manual emergency stop clearance (Issue 18) */

    return k_rx_ok;
}

static rx_err_t process_egress(void)
{
    /* Stub implementation - Issue 12:
     * Telemetry streaming via USB CDC/SPI */

    return k_rx_ok;
}

static rx_err_t check_comm_timeout(void)
{
    /* Stub implementation - Issue 17:
     * Communication timeout detection and E-STOP trigger */

    return k_rx_ok;
}
