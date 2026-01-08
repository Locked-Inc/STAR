/* star-rx72n-firmware/src/tasks/motor_controller.c */

/**
 * @file motor_controller.c
 * @brief Motor Controller thread implementation
 * @details
 * Deterministic 250 Hz control loop for 4-motor velocity control.
 * Highest priority thread to prevent jitter from sensor delays.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/motor_controller.h"
#include "motor_config.h"
#include "shared_state.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include <string.h>

/* =============================================================================
 * Private Types
 * =============================================================================
 */

/**
 * @brief Motor subsystem state
 * @details Will be populated with driver/encoder/PID handles in Issue 6
 */
typedef struct {
    bool initialized; /**< True if subsystem initialized */
} motor_subsystem_t;

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static const char* s_tag = "motor_ctrl";

/* Thread control block and stack (statically allocated) */
static TX_THREAD s_motor_controller_thread;
static uint8_t   s_motor_controller_stack[k_stack_motor_controller];

/* Motor subsystem state */
static motor_subsystem_t s_motor_subsystem;

/* =============================================================================
 * Private Function Prototypes
 * =============================================================================
 */

/**
 * @brief Motor Controller thread entry point
 * @param input Thread input parameter (unused)
 */
static void motor_controller_entry(ULONG input);

/**
 * @brief Initialize motor subsystem (motors, encoders, PIDs)
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t motor_subsystem_init(void);

/**
 * @brief Execute one control loop iteration
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t control_loop_iteration(void);

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

UINT motor_controller_create(void)
{
    UINT status = TX_SUCCESS;

    /* Zero-initialize motor subsystem */
    memset(&s_motor_subsystem, 0, sizeof(motor_subsystem_t));

    /* Create Motor_Controller thread */
    status = tx_thread_create(
        &s_motor_controller_thread,
        s_tag,
        motor_controller_entry,
        0,
        s_motor_controller_stack,
        k_stack_motor_controller,
        k_priority_motor_controller,
        k_priority_motor_controller,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Thread creation failed");
        return status;
    }

    rx_log_info(s_tag, "Motor_Controller thread created (priority 2, 250 Hz)");
    return TX_SUCCESS;
}

/* =============================================================================
 * Private Function Implementations
 * =============================================================================
 */

static void motor_controller_entry(ULONG input)
{
    rx_err_t ret = k_rx_ok;

    (void)input; /* Suppress unused parameter warning */

    rx_log_info(s_tag, "Motor_Controller thread started");

    /* Initialize motor subsystem */
    ret = motor_subsystem_init();
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Motor subsystem init failed");
        /* Cannot continue - halt this thread */
        while (1) {
            tx_thread_sleep(100); /* Sleep forever */
        }
    }

    rx_log_info(s_tag, "Entering 250 Hz control loop");

    /* Main control loop (250 Hz = 4ms period) */
    while (1) {
        /* Execute control loop iteration */
        ret = control_loop_iteration();
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "Control loop error");
        }

        /* Feed watchdog (critical - prevents reset) */
        rx_iwdt_feed();

        /* Sleep for 4ms (250 Hz control rate)
         * Note: k_control_loop_ticks = 0 because 4ms < 10ms ThreadX tick
         * We use minimal sleep to maintain timing precision */
        tx_thread_sleep(1); /* Sleep for 1 tick (10ms) - TODO: Use timer for precise 4ms */
    }
}

static rx_err_t motor_subsystem_init(void)
{
    /* Stub implementation - Issue 6: Motor subsystem initialization
     * Will initialize: bus manager, DRV8243S drivers, MTU encoders, PIDs */

    rx_log_info(s_tag, "Motor subsystem init (stub - Issue 6)");
    s_motor_subsystem.initialized = true;
    return k_rx_ok;
}

static rx_err_t control_loop_iteration(void)
{
    /* Stub implementation - Issues 7-9:
     * - Control loop with encoder feedback and PID (Issue 7)
     * - Current sensing and fault detection (Issue 8)
     * - Emergency stop integration (Issue 9) */

    return k_rx_ok;
}
