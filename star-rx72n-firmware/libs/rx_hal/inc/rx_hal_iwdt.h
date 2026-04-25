/**
 * @file rx_hal_iwdt.h
 * @brief Independent Watchdog Timer (IWDT) HAL Driver for RX72N
 *
 * @details
 * **Hardware Abstraction Layer** for the RX72N Independent Watchdog Timer (IWDT)
 * peripheral. This driver provides low-level register access and task-level
 * monitoring for safety-critical system recovery.
 *
 * ## Purpose and Design Rationale
 *
 * The IWDT provides **true clock-independent** watchdog functionality:
 * - Dedicated 120 kHz on-chip oscillator (IWDTCLK)
 * - Operates even if main system clock fails
 * - Cannot be disabled once started (hardware safety feature)
 * - Triggers system reset on timeout
 *
 * This module is the **HAL layer** providing direct hardware access. For
 * higher-level watchdog features (state-dependent timeouts, comprehensive
 * task monitoring), see `rx_core/inc/rx_iwdt.h`.
 *
 * ## Key Features
 *
 * **Hardware Watchdog**:
 * - Internal 120 kHz clock source (IWDTCLK)
 * - Configurable timeout: 128ms to 16,384ms
 * - Window mode (optional, disabled by default)
 * - Reset or NMI on timeout
 * - Refresh error detection
 *
 * **Task Monitoring**:
 * - Register up to 8 tasks for heartbeat monitoring
 * - Per-task timeout configuration
 * - Failed task identification for diagnostics
 *
 * ## Hardware Architecture
 *
 * @par Clock and Timing:
 *
 * | Parameter | Value | Notes |
 * |-----------|-------|-------|
 * | Clock Source | IWDTCLK | Internal RC oscillator |
 * | Clock Frequency | 120 kHz | +/-1% accuracy |
 * | Clock Period | 8.33 us | 1/120kHz |
 * | Counter Width | 14-bit | Down-counter |
 * | Min Timeout | 128 ms | 2^14 / (120k / 1) |
 * | Max Timeout | 16,384 ms | 2^14 / (120k / 128) |
 *
 * @par Register Map:
 *
 * | Register | Address | Description |
 * |----------|---------|-------------|
 * | IWDTRR | 0x00088030 | Refresh register |
 * | IWDT\\r | 0x00088032 | Control register |
 * | IWDTSR | 0x00088034 | Status register |
 * | IWDTR\\r | 0x00088036 | Reset control register |
 * | IWDTCSTPR | 0x00088038 | Count stop register |
 *
 * @par Timeout Calculation:
 *
 * @f[
 *   T_{timeout} = \frac{2^{14} \times \text{TOPS}}{f_{IWDTCLK}}
 * @f]
 *
 * Where TOPS is the timeout period setting (1, 4, 16, 32, 64, or 128).
 *
 * ## Sub-clock Pin Handling
 *
 * The IWDT uses the internal IWDTCLK, **not** the sub-clock oscillator.
 * When sub-clock is unused:
 * - **XCIN**: Connect to VSS (ground) via pull-down resistor
 * - **XCOUT**: Leave open (unconnected)
 *
 * System initialization disables sub-clock via SOSC\\r.SOSTP = 1.
 *
 * ## Performance Characteristics
 *
 * | Operation | Execution Time | Notes |
 * |-----------|---------------|-------|
 * | rx_hal_iwdt_init() | ~20 us | One-time configuration |
 * | rx_hal_iwdt_feed() | ~1 us | Two register writes |
 * | rx_hal_iwdt_task_heartbeat() | ~3 us | Timestamp update |
 * | rx_hal_iwdt_check_tasks() | ~5 us x N tasks | Linear scan |
 *
 * **Memory Footprint**:
 * - Static variables: ~256 bytes (task tracking)
 * - Code size: ~1 KB
 * - Stack usage: < 32 bytes
 * - Heap usage: 0 bytes
 *
 * @par Hardware Requirements:
 *
 * | Component | Requirement | Notes |
 * |-----------|-------------|-------|
 * | MCU | Renesas RX72N | IWDT peripheral |
 * | Clock | IWDTCLK | 120 kHz internal |
 * | XCIN Pin | VSS via resistor | When sub-clock unused |
 * | XCOUT Pin | Open | When sub-clock unused |
 *
 * @par Module Dependencies:
 *
 * **This module depends on**:
 * - `rx_err.h` - Error code definitions
 * - `rx72n_iwdt_regs.h` - IWDT register definitions
 *
 * **This module is used by**:
 * - `main.c` - System initialization
 * - All RTOS tasks - Heartbeat reporting
 * - `rx_core/inc/rx_iwdt.h` - Higher-level watchdog features
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | No goto/setjmp/recursion |
 * | 2. Fixed loop bounds | [OK] | Loops bounded by k_iwdt_max_tasks (8) |
 * | 3. No dynamic allocation | [OK] | Static task array only |
 * | 4. Small functions | [OK] | All < 60 lines |
 * | 5. Assertions | [OK] | Minimum 2 checks per function |
 * | 6. Narrow scope | [OK] | File-scope statics |
 * | 7. Check return values | [OK] | All return rx_err_t |
 * | 8. Limited preprocessor | [OK] | C23 typed enums only |
 * | 9. Pointer restrictions | [OK] | Single-level pointers |
 * | 10. Compiler warnings | [OK] | -Wall -Wextra -Werror |
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility (S)**:
 * - HAL layer: Hardware register access only
 * - Task monitoring separate from hardware watchdog
 *
 * **Open/Closed (O)**:
 * - Timeout values configurable
 * - Task monitoring extensible
 *
 * **Dependency Inversion (D)**:
 * - Higher-level `rx_core/inc/rx_iwdt.h` depends on this HAL interface
 *
 * @see rx_core/inc/rx_iwdt.h Higher-level watchdog with state management
 * @see rx72n_iwdt_regs.h Register definitions
 * @see RX72N Hardware Manual Chapter 35 - Independent Watchdog Timer
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

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
 * @enum rx_iwdt_timeout_t
 * @brief IWDT Timeout Period Options
 *
 * @details
 * Pre-defined timeout periods for the IWDT. These values correspond to
 * hardware divider settings that produce specific timeout durations from
 * the 120 kHz IWDTCLK.
 *
 * ## Timeout Selection Guide
 *
 * | Timeout | Use Case | Notes |
 * |---------|----------|-------|
 * | 128ms | High-frequency control loops (> 50 Hz) | Very responsive |
 * | 512ms | Normal control loops (10-50 Hz) | Fast detection |
 * | 1000ms | **Default** - typical applications | Good balance |
 * | 2048ms | Low-frequency tasks (1-10 Hz) | Tolerant |
 * | 8192ms | Slow background operations | Long timeout |
 * | 16384ms | Initialization/maintenance | Maximum |
 *
 * @par Timeout Calculation:
 *
 * Timeout periods are derived from:
 * @f[
 *   T = \frac{\text{Cycle Count}}{f_{IWDTCLK}} = \frac{\text{Cycle Count}}{120\,\text{kHz}}
 * @f]
 *
 * @par Usage Example:
 * @code{.c}
 * // Use predefined timeout for 1 second
 * rx_hal_iwdt_init(k_iwdt_timeout_1000ms);
 *
 * // Or use custom value (rounded up to nearest supported)
 * rx_hal_iwdt_init(500);  // Rounds to 512ms
 * @endcode
 *
 * @invariant All values are in milliseconds
 * @invariant Values correspond to hardware divider settings
 *
 * @note C23 typed enum with uint16_t underlying type
 * @note Actual timeout may vary +/-1% due to oscillator tolerance
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_iwdt_timeout_128ms =
    128, /**< Minimum timeout (128ms). Use for high-frequency control loops requiring fast failure detection. TOPS=1 divider */
  k_iwdt_timeout_512ms =
    512, /**< Short timeout (512ms). Use for normal control loops (10-50 Hz). Provides good responsiveness. TOPS=4 divider */
  k_iwdt_timeout_1000ms =
    1000, /**< Default timeout (1 second). **Recommended** for typical applications. Good balance between detection speed and tolerance. TOPS~8 */
  k_iwdt_timeout_2048ms =
    2048, /**< Medium timeout (2 seconds). Use for low-frequency tasks that may have variable execution time. TOPS=16 divider */
  k_iwdt_timeout_8192ms =
    8192, /**< Long timeout (8 seconds). Use for slow background operations or maintenance tasks. TOPS=64 divider */
  k_iwdt_timeout_16384ms =
    16384, /**< Maximum timeout (~16 seconds). Use only for initialization phases or very slow operations. TOPS=128 divider */
} rx_iwdt_timeout_t;

/**
 * @enum rx_iwdt_reset_cause_t
 * @brief IWDT Reset Cause Information
 *
 * @details
 * Identifies the specific cause of an IWDT-triggered reset. This information
 * is read from the IWDTSR status register and can be used for post-mortem
 * diagnostics and logging.
 *
 * ## Reset Cause Detection
 *
 * At startup, read the reset cause before calling rx_hal_iwdt_init():
 *
 * @code{.c}
 * void startup_diagnostics(void)
 * {
 *     rx_iwdt_reset_cause_t cause = rx_hal_iwdt_get_reset_cause();
 *
 *     switch (cause) {
 *     case k_iwdt_reset_none:
 *         uart_debug_puts("Normal startup\\r\\n");
 *         break;
 *     case k_iwdt_reset_underflow:
 *         uart_debug_puts("[WARN] Watchdog timeout - task deadlock?\\r\\n");
 *         log_failed_task(rx_hal_iwdt_get_failed_task());
 *         break;
 *     case k_iwdt_reset_refresh_error:
 *         uart_debug_puts("[ERROR] Watchdog refresh error - code bug\\r\\n");
 *         break;
 *     }
 *
 *     rx_hal_iwdt_clear_status();
 * }
 * @endcode
 *
 * @par Reset Cause Table:
 *
 * | Cause | IWDTSR Bits | Meaning | Typical Issue |
 * |-------|-------------|---------|---------------|
 * | None | 0x0000 | Not a watchdog reset | Power-on, external reset |
 * | Underflow | UNDFF=1 | Counter reached zero | Task deadlock, infinite loop |
 * | Refresh Error | REFEF=1 | Invalid refresh | Window violation, wrong sequence |
 *
 * @invariant Values map to IWDTSR bit patterns
 * @invariant k_iwdt_reset_none (0) is the default for non-watchdog resets
 *
 * @note C23 typed enum with uint8_t underlying type
 * @see rx_hal_iwdt_get_reset_cause() Read the reset cause
 * @see rx_hal_iwdt_clear_status() Clear status flags after reading
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_iwdt_reset_none =
    0, /**< No watchdog reset occurred. System reset was from power-on, external reset pin, or other source. IWDTSR.UNDFF=0, REFEF=0 */
  k_iwdt_reset_underflow =
    1, /**< Reset due to counter underflow. Counter reached zero before rx_hal_iwdt_feed() was called. Indicates task deadlock or infinite loop. IWDTSR.UNDFF=1 */
  k_iwdt_reset_refresh_error =
    2, /**< Reset due to invalid refresh. Either wrong refresh sequence (not 0x00, 0xFF) or refresh during window-prohibited period. IWDTSR.REFEF=1 */
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
 * @note This function starts the watchdog - rx_hal_iwdt_feed() must be called
 *       within the timeout period to prevent reset.
 * @note On RX72N, IWDT starts counting after the first refresh operation.
 */
[[nodiscard]] rx_err_t rx_hal_iwdt_init(uint32_t timeout_ms);

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
 *         rx_hal_iwdt_feed();  // Feed at end of each iteration
 *         tx_thread_sleep(1);  // 10ms at 100Hz tick
 *     }
 * }
 * @endcode
 */
void rx_hal_iwdt_feed(void);

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
[[nodiscard]] bool rx_hal_iwdt_was_reset(void);

/**
 * @brief Get detailed reset cause from IWDT
 *
 * Provides more detailed information about what caused a watchdog reset:
 * - Underflow: Counter reached zero (normal timeout)
 * - Refresh error: Invalid refresh sequence or window violation
 *
 * @return Reset cause enumeration
 *
 * @note This provides more detail than rx_hal_iwdt_was_reset() for diagnostics.
 */
rx_iwdt_reset_cause_t rx_hal_iwdt_get_reset_cause(void);

/**
 * @brief Clear watchdog status flags
 *
 * Clears the underflow and refresh error flags in IWDTSR.
 * Call after reading the reset cause to prepare for next reset detection.
 *
 * @note Flags are typically cleared once at startup after logging the cause.
 */
void rx_hal_iwdt_clear_status(void);

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
 * Each task must call rx_hal_iwdt_task_heartbeat() periodically to indicate
 * it is still running.
 *
 * @param[in] task_name Name of the task (for logging)
 * @param[in] timeout_ms Maximum time between heartbeats before considering
 *                       the task dead (typically 3x the task period)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if task_name is nullptr
 * @return k_rx_err_no_mem if maximum tasks already registered
 *
 * @note Call during task initialization before entering main loop
 *
 * Example:
 * @code
 * // Motor_Controller runs at 250 Hz (4ms period)
 * // Set timeout to 3x period = 12ms
 * rx_hal_iwdt_register_task("Motor_Controller", 12);
 * @endcode
 */
[[nodiscard]] rx_err_t rx_hal_iwdt_register_task(const char* task_name, uint32_t timeout_ms);

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
 *     rx_hal_iwdt_register_task("Comm_Manager", 30);
 *     while (1) {
 *         process_ingress();
 *         process_egress();
 *         rx_hal_iwdt_task_heartbeat("Comm_Manager");
 *         tx_thread_sleep(1);
 *     }
 * }
 * @endcode
 */
void rx_hal_iwdt_task_heartbeat(const char* task_name);

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
[[nodiscard]] rx_err_t rx_hal_iwdt_check_tasks(void);

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
const char* rx_hal_iwdt_get_failed_task(void);

#ifdef __cplusplus
}

#endif /* STAR_RX_IWDT_H */
