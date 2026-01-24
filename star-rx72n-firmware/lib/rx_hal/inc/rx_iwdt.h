/* lib/rx_hal/inc/rx_iwdt.h */

/**
 * @file rx_iwdt.h
 * @brief Independent Watchdog Timer (IWDT) Driver for RX72N
 *
 * The IWDT provides system recovery from infinite loops and software hangs.
 * It runs on the dedicated 120 kHz IWDT-dedicated on-chip RC oscillator (IWDTCLK),
 * which is independent of the main system clock. This internal oscillator
 * ensures the IWDT can reset the chip even if the main clock fails.
 * No external components are required for the IWDT clock.
 *
 * Sub-clock Pin Handling (XCIN/XCOUT):
 * The IWDT does not use the sub-clock oscillator pins (XCIN/XCOUT). When the
 * sub-clock oscillator is unused, the hardware manual specifies the
 * following connections for these pins:
 * - XCIN: Connect to VSS (ground) via a pull-down resistor.
 * - XCOUT: Leave open (unconnected).
 *
 * The system_init.c file disables the sub-clock oscillator by setting
 * the SOSCCR.SOSTP bit. It is also recommended to set RCR3.RTCEN = 0.
 *
 * Features:
 * - Internal 120 kHz clock source (IWDTCLK)
 * - Configurable timeout (128ms to 16,384ms)
 * - Window mode (optional - disabled by default)
 * - Reset or NMI on timeout
 * - Refresh error detection
 *
 * Usage:
 * @code
 * // Initialize with 1 second timeout
 * rx_iwdt_init(1000);
 *
 * // In main control loop (call at least once per timeout period)
 * while (1) {
 *     do_work();
 *     rx_iwdt_feed();  // Must be called within timeout period
 * }
 * @endcode
 *
 * @note IWDT is configured during startup and cannot be stopped once started.
 * @note Feed sequence (0x00, 0xFF) must complete atomically.
 *
 * @see RX72N Hardware Manual, Section 25 - Independent Watchdog Timer
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_IWDT_H
#define STAR_RX_IWDT_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief IWDT timeout configuration options
 *
 * Based on the 120 kHz internal IWDT clock (IWDTCLK) with various
 * cycle counts and divisors.
 */
typedef enum : uint16_t {
  k_iwdt_timeout_128ms   = 128,   /**< Minimum timeout */
  k_iwdt_timeout_512ms   = 512,   /**< Short timeout */
  k_iwdt_timeout_1000ms  = 1000,  /**< Default timeout (1 second) */
  k_iwdt_timeout_2048ms  = 2048,  /**< Medium timeout */
  k_iwdt_timeout_8192ms  = 8192,  /**< Long timeout */
  k_iwdt_timeout_16384ms = 16384, /**< Maximum timeout */
} rx_iwdt_timeout_t;

/**
 * @brief IWDT reset cause information
 */
typedef enum : uint8_t {
  k_iwdt_reset_none          = 0, /**< No watchdog reset occurred */
  k_iwdt_reset_underflow     = 1, /**< Reset due to counter underflow */
  k_iwdt_reset_refresh_error = 2, /**< Reset due to invalid refresh */
} rx_iwdt_reset_cause_t;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize the Independent Watchdog Timer
 *
 * Configures IWDT with the specified timeout period. Once started, the
 * watchdog cannot be stopped and must be fed periodically to prevent reset.
 *
 * Configuration:
 * - Clock: 120 kHz internal IWDT-dedicated oscillator (IWDTCLK)
 * - Window mode: Disabled (refresh allowed anytime)
 * - Action: System reset on timeout
 * - Count stop in sleep: Disabled (continues counting)
 *
 * @param[in] timeout_ms Desired timeout in milliseconds.
 *                       Actual timeout will be rounded up to nearest
 *                       supported value (128, 512, 1024, 2048, 4096,
 *                       8192, or 16384 ms).
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if timeout is out of supported range
 * @return k_rx_err_invalid_state if IWDT is already running
 *
 * @note This function starts the watchdog - rx_iwdt_feed() must be called
 *       within the timeout period to prevent reset.
 * @note On RX72N, IWDT starts counting after the first refresh operation.
 */
rx_err_t rx_iwdt_init(uint32_t timeout_ms);

/* =============================================================================
 * Runtime Operations
 * =============================================================================
 */

/**
 * @brief Feed (refresh) the watchdog timer
 *
 * Resets the watchdog counter to prevent timeout. This function must be
 * called periodically, within the configured timeout period.
 *
 * The refresh sequence writes 0x00 followed by 0xFF to the IWDTRR register.
 * This sequence must complete atomically without interruption.
 *
 * @note Call this function from your main control loop.
 * @note Calling more frequently than needed has no negative effect.
 * @note Do NOT call from interrupt handlers unless you understand the
 *       implications for your system's safety.
 *
 * Example placement:
 * @code
 * // Motor control task at 100 Hz (10ms period)
 * // With 1000ms timeout, we have 100 chances to feed
 * void motor_control_task_entry(ULONG input) {
 *     while (1) {
 *         read_sensors();
 *         update_pid();
 *         set_motor_output();
 *         rx_iwdt_feed();  // Feed at end of each iteration
 *         tx_thread_sleep(1);  // 10ms at 100Hz tick
 *     }
 * }
 * @endcode
 */
void rx_iwdt_feed(void);

/* =============================================================================
 * Diagnostics
 * =============================================================================
 */

/**
 * @brief Check if last reset was caused by watchdog timeout
 *
 * Reads the IWDT status register to determine if the previous system
 * reset was triggered by the watchdog timer.
 *
 * @return true if watchdog caused the last reset
 * @return false if reset was from another source (power-on, external, etc.)
 *
 * @note Call this early in startup (before clearing status flags) to
 *       log or handle watchdog-induced resets appropriately.
 */
bool rx_iwdt_was_reset(void);

/**
 * @brief Get detailed reset cause from IWDT
 *
 * Provides more detailed information about what caused a watchdog reset:
 * - Underflow: Counter reached zero (normal timeout)
 * - Refresh error: Invalid refresh sequence or window violation
 *
 * @return Reset cause enumeration
 *
 * @note This provides more detail than rx_iwdt_was_reset() for diagnostics.
 */
rx_iwdt_reset_cause_t rx_iwdt_get_reset_cause(void);

/**
 * @brief Clear watchdog status flags
 *
 * Clears the underflow and refresh error flags in IWDTSR.
 * Call after reading the reset cause to prepare for next reset detection.
 *
 * @note Flags are typically cleared once at startup after logging the cause.
 */
void rx_iwdt_clear_status(void);

/* =============================================================================
 * Task-Level Monitoring (Issue 20: Enhanced Watchdog)
 * =============================================================================
 */

/**
 * @brief Maximum number of tasks that can be monitored
 */
typedef enum : uint8_t {
  k_iwdt_max_tasks = 8, /**< Support up to 8 monitored tasks */
} rx_iwdt_limits_t;

/**
 * @brief Register a task for heartbeat monitoring
 *
 * Registers a task with the watchdog system to enable deadlock detection.
 * Each task must call rx_iwdt_task_heartbeat() periodically to indicate
 * it is still running.
 *
 * @param[in] task_name Name of the task (for logging)
 * @param[in] timeout_ms Maximum time between heartbeats before considering
 *                       the task dead (typically 3x the task period)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if task_name is NULL
 * @return k_rx_err_no_mem if maximum tasks already registered
 *
 * @note Call during task initialization before entering main loop
 *
 * Example:
 * @code
 * // Motor_Controller runs at 250 Hz (4ms period)
 * // Set timeout to 3x period = 12ms
 * rx_iwdt_register_task("Motor_Controller", 12);
 * @endcode
 */
rx_err_t rx_iwdt_register_task(const char* task_name, uint32_t timeout_ms);

/**
 * @brief Record a task heartbeat
 *
 * Updates the last heartbeat timestamp for the calling task. Tasks must
 * call this function periodically (within their registered timeout) to
 * prove they are still alive.
 *
 * @param[in] task_name Name of the task (must match registration)
 *
 * @note Call this at the end of each task iteration
 * @note If any task misses 3+ heartbeats, watchdog will eventually trigger
 *
 * Example:
 * @code
 * void comm_manager_entry(ULONG input) {
 *     rx_iwdt_register_task("Comm_Manager", 30);
 *     while (1) {
 *         process_ingress();
 *         process_egress();
 *         rx_iwdt_task_heartbeat("Comm_Manager");
 *         tx_thread_sleep(1);
 *     }
 * }
 * @endcode
 */
void rx_iwdt_task_heartbeat(const char* task_name);

/**
 * @brief Check for task deadlocks
 *
 * Examines all registered tasks to see if any have exceeded their heartbeat
 * timeout. Should be called periodically (e.g., from lowest priority task).
 *
 * @return k_rx_ok if all tasks healthy
 * @return k_rx_err_timeout if one or more tasks have deadlocked
 *
 * @note This function logs which task(s) failed before returning
 * @note The hardware IWDT will eventually reset the system if deadlock persists
 */
rx_err_t rx_iwdt_check_tasks(void);

/**
 * @brief Get the name of the last failed task (if any)
 *
 * Returns the name of the task that failed to heartbeat before the last
 * watchdog reset. Useful for post-mortem diagnostics.
 *
 * @return Task name if reset was due to task deadlock, NULL otherwise
 *
 * @note This information persists across reset in a special RAM region
 * @note Call early in startup to retrieve failure information
 */
const char* rx_iwdt_get_failed_task(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_IWDT_H */
