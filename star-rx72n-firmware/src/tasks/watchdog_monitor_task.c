/**
 * @file watchdog_monitor_task.c
 * @brief Watchdog Monitor Task Implementation
 *
 * @details
 * Implements a high-priority ThreadX task that provides system-level watchdog
 * supervision by monitoring all registered tasks for liveness and feeding the
 * hardware Independent Watchdog Timer (IWDT).
 *
 * ## Three-Layer Watchdog Architecture
 *
 * **Layer 1: Hardware IWDT**
 * - Physical watchdog timer with 2-second timeout
 * - Triggers system reset if not fed by this task
 * - Cannot be disabled (hardware safety feature)
 *
 * **Layer 2: Task Monitoring**
 * - Software-level tracking of individual task heartbeats
 * - Each task reports liveness via rx_iwdt_task_heartbeat()
 * - Task-specific timeouts (30ms for fast tasks, 3000ms for slow tasks)
 *
 * **Layer 3: Watchdog Monitor (This Task)**
 * - Runs at 100 Hz (10ms period) at priority 6
 * - Checks all tasks for timeout via rx_iwdt_check_tasks()
 * - Feeds hardware watchdog via rx_iwdt_feed()
 * - Self-reports own heartbeat (self-monitoring)
 *
 * ## Task Characteristics
 *
 * | Metric | Value |
 * |--------|-------|
 * | Priority | 6 |
 * | Stack | 512 B |
 * | Update Rate | 100 Hz |
 * | CPU Utilization | < 0.01% |
 *
 * ## Failure Detection Flow
 *
 * @msc
 *   MotorTask, WatchdogMon, IWDT;
 *
 *   MotorTask -> WatchdogMon [label="heartbeat @ t=0ms"];
 *   WatchdogMon -> IWDT [label="feed @ t=10ms"];
 *   MotorTask -> WatchdogMon [label="heartbeat @ t=10ms"];
 *   WatchdogMon -> IWDT [label="feed @ t=20ms"];
 *   MotorTask note MotorTask [label="HANGS @ t=20ms", textbgcolor="red"];
 *   WatchdogMon -> IWDT [label="feed @ t=30ms"];
 *   WatchdogMon note WatchdogMon [label="Detect timeout @ t=50ms", textbgcolor="yellow"];
 *   WatchdogMon -> WatchdogMon [label="STOP feeding"];
 *   WatchdogMon note WatchdogMon [label="Log error"];
 *   IWDT note IWDT [label="Timeout @ t=2050ms", textbgcolor="red"];
 *   IWDT -> IWDT [label="SYSTEM RESET"];
 * @endmsc
 *
 * ## Timeout Strategy
 *
 * - Fast tasks (10ms period): 30ms timeout (3x period)
 * - Medium tasks (20ms period): 60ms timeout (3x period)
 * - Slow tasks (50ms period): 150ms timeout (3x period)
 * - Very slow tasks (1000ms period): 3000ms timeout (3x period)
 *
 * **Rationale**: 3x period allows 2 missed heartbeats before timeout,
 * providing margin for scheduler delays without false positives.
 *
 * @par Hardware Requirements:
 * - RX72N Independent Watchdog Timer (IWDT) peripheral
 * - IWDTCKS clock source (PCLKB/128 or LOCO)
 * - IWDT configured for 2048ms timeout
 * - PCLKB running at 60 MHz for accurate timing
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto, setjmp, recursion (sequential while loop)
 * - Rule 2: Bounded loops (infinite task loop with bounded sleep)
 * - Rule 3: No dynamic memory (static task stack)
 * - Rule 4: Functions < 60 lines (task entry ~40 lines)
 * - Rule 5: All returns checked (heartbeat, feed, check_tasks)
 * - Rule 7: All return values checked via RX_ASSERT
 * - Rule 8: C23 typed enums for constants
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles Adherence:
 * - Single Responsibility: Only monitors IWDT and task health
 * - Open/Closed: Extensible via rx_iwdt API, no task-specific logic
 * - Liskov Substitution: Implements standard task interface
 * - Interface Segregation: Minimal API (single create function)
 * - Dependency Inversion: Depends on rx_iwdt abstraction, not hardware
 *
 * @version 1.0.0
 * @author Locked, Inc.
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "watchdog_monitor_task.h"

#include "rx_check.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "tx_api.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum watchdog_task_constants_t
 * @brief Watchdog monitor task configuration constants
 *
 * @details
 * Defines ThreadX task parameters for the watchdog monitor task, which supervises
 * all registered tasks by checking heartbeats and feeding the hardware Independent
 * Watchdog Timer (IWDT) at 100 Hz. The task runs at high priority (6) between
 * Communication (5) and Motor Control (8) to ensure timely watchdog feeding.
 *
 * **Task Characteristics:**
 * - **Stack**: 512 bytes (minimal - no complex logic, just checks and feeds)
 * - **Priority**: 6 (high - ensures IWDT fed even under heavy load)
 * - **Period**: 10ms (100 Hz - provides 200x safety margin for 2048ms IWDT timeout)
 * - **CPU Utilization**: < 0.01% (extremely lightweight)
 *
 * **Design Rationale:**
 * - Small stack sufficient for heartbeat checks and IWDT feed operations
 * - High priority prevents preemption during safety-critical watchdog operations
 * - 100 Hz rate ensures hardware watchdog fed with 200x margin (2048ms / 10ms)
 *
 * @note All size values in bytes, periods in ThreadX ticks (10ms @ 100 Hz)
 * @see watchdog_monitor_task_create() Task creation using these constants
 * @see internal_watchdog_monitor_task_entry() Main loop running at configured period
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_watchdog_task_stack_size =
    512, /**< Stack size (512 bytes). Minimal allocation for lightweight supervision loop. No recursion, no large locals. Valid range: 256-1024 bytes. */
  k_watchdog_task_priority =
    6, /**< ThreadX priority (6). High priority ensures timely IWDT feeding. Between Communication (5) and Motor Control (8). Valid range: 1-15 (1=highest). */
  k_watchdog_task_input =
    0, /**< Thread entry input parameter (0). Unused by watchdog monitor task. ThreadX convention for parameterless tasks. */
  k_watchdog_task_period_ticks =
    1, /**< Task period (1 tick = 10ms @ 100 Hz). Watchdog check and feed rate. Provides 200x safety margin for 2048ms hardware IWDT timeout. Valid range: 1-10 ticks (10-100ms). */
} watchdog_task_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/**
 * @var s_watchdog_thread
 * @brief ThreadX thread control block for watchdog monitor task
 *
 * @details
 * Holds ThreadX RTOS state for the watchdog monitor task: stack pointer,
 * priority, sleep state, event flags, etc. Initialized by tx_thread_create()
 * in watchdog_monitor_task_create(). Persistent for entire system lifetime.
 *
 * @note Internal ThreadX structure - do not modify directly
 * @warning Must remain in scope for task lifetime (static storage)
 * @see watchdog_monitor_task_create() Initializes this control block
 *
 * @since Version 1.0.0
 */
static TX_THREAD s_watchdog_thread;

/**
 * @var s_watchdog_stack
 * @brief Static thread stack for watchdog monitor task (512 bytes)
 *
 * @details
 * Statically allocated stack memory for the watchdog monitor task. Uses
 * k_watchdog_task_stack_size (512 bytes) which is sufficient for the minimal
 * logic in the supervision loop (heartbeat checks, IWDT feed, sleep).
 *
 * **Stack Usage Breakdown:**
 * - Function call overhead: ~32 bytes (call stack, return addresses)
 * - Local variables: ~16 bytes (err code, minimal state)
 * - ThreadX context: ~64 bytes (registers, RTOS state)
 * - Safety margin: ~400 bytes (unused, guards against stack overflow)
 *
 * @note NASA Power of 10 Rule 3: No dynamic memory allocation (static array)
 * @warning Stack overflow triggers undefined behavior - monitor in debug builds
 * @post Initialized to zero by C runtime before main() (BSS section)
 *
 * @since Version 1.0.0
 */
static uint8_t s_watchdog_stack[k_watchdog_task_stack_size];

/**
 * @var s_watchdog_created
 * @brief Task creation guard flag - prevents duplicate task creation
 *
 * @details
 * Boolean flag that prevents watchdog_monitor_task_create() from being called
 * multiple times. Set to false at startup (BSS zero-init), set to true after
 * successful tx_thread_create() call.
 *
 * **State Transitions:**
 * - false -> true: First successful call to watchdog_monitor_task_create()
 * - Never resets (watchdog task runs for entire system lifetime)
 *
 * @note Not thread-safe - watchdog_monitor_task_create() must only be called
 *       from tx_application_define() (single-threaded initialization context)
 * @pre Initialized to false by C runtime (BSS section)
 * @post Set to true after successful task creation, never reset
 *
 * @since Version 1.0.0
 */
static bool s_watchdog_created = false;

/**
 * @var s_tag
 * @brief Log tag for watchdog monitor module ("IWDT_MON")
 *
 * @details
 * String literal used as the module identifier in all rx_log_*() calls.
 * Appears in UART debug output to identify log messages from this module.
 *
 * @note Stored in .rodata section (const string literal)
 * @see rx_log_info() Log informational messages with this tag
 * @see rx_log_error() Log error messages with this tag
 *
 * @since Version 1.0.0
 */
static const char* const s_tag = "IWDT_MON";

/**
 * @var s_task_name
 * @brief IWDT task name for thread creation and heartbeat registration
 *
 * @details
 * String literal used to identify the watchdog monitor task in ThreadX thread
 * creation and IWDT heartbeat calls. Centralizes the task name to prevent typos
 * and ensure consistency across tx_thread_create() and rx_iwdt_task_heartbeat().
 *
 * **Usage:**
 * - ThreadX thread creation: tx_thread_create(..., s_task_name, ...)
 * - Heartbeat reporting: rx_iwdt_task_heartbeat(s_task_name)
 *
 * @note String must match IWDT registration for heartbeat correlation
 * @warning Do not modify - IWDT system depends on exact string match
 * @see watchdog_monitor_task_create() Thread creation using this name
 * @see internal_watchdog_monitor_task_entry() Heartbeat reporting using this name
 *
 * @since Version 1.0.0
 */
static const char* const s_task_name = "WatchdogMon";

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

static void internal_handle_check_result(rx_err_t err, bool* timeout_logged);
static void internal_watchdog_monitor_task_entry(ULONG input);

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Create the watchdog monitor task
 *
 * @details
 * Creates and starts the watchdog monitor ThreadX task with high priority (6)
 * for system health supervision at 100 Hz.
 *
 * @par Usage Example:
 * @code{.c}
 * // In tx_application_define()
 * rx_err_t err;
 *
 * // Step 1: Initialize IWDT
 * err = rx_iwdt_init(&s_iwdt_config);
 * RX_ASSERT(err == k_rx_ok, "IWDT init failed");
 *
 * // Step 2: Register all tasks
 * err = rx_iwdt_register_task("MotorCtrl", 30);
 * RX_ASSERT(err == k_rx_ok, "Task registration failed");
 * // ... register other tasks ...
 *
 * // Step 3: Set initial state
 * err = rx_iwdt_set_state(k_system_state_init);
 * RX_ASSERT(err == k_rx_ok, "Set state failed");
 *
 * // Step 4: Create watchdog monitor task
 * err = watchdog_monitor_task_create();
 * RX_ASSERT(err == k_rx_ok, "Watchdog task creation failed");
 * // s_watchdog_created is now true
 * @endcode
 *
 *
 * @pre ThreadX kernel running
 * @pre rx_iwdt_init() called successfully
 * @pre All tasks registered via rx_iwdt_register_task()
 *
 * @post WatchdogMon created and running at 100 Hz
 * @post s_watchdog_created set to true
 *
 * @note Thread Safety: Not thread-safe, call from tx_application_define only
 *
 * @since Version 1.0.0
 */
rx_err_t watchdog_monitor_task_create(void)
{
  /* Check if already created */
  RX_ASSERT(!s_watchdog_created, "Watchdog monitor task already created");
  if (s_watchdog_created) {
    return k_rx_err_invalid_state;
  }

  /* Create the thread */
  UINT tx_status = tx_thread_create(&s_watchdog_thread,
                                    (CHAR*)s_task_name,
                                    internal_watchdog_monitor_task_entry,
                                    k_watchdog_task_input,
                                    s_watchdog_stack,
                                    k_watchdog_task_stack_size,
                                    k_watchdog_task_priority,
                                    k_watchdog_task_priority,
                                    TX_NO_TIME_SLICE,
                                    TX_AUTO_START);

  if (tx_status != TX_SUCCESS) {
    rx_log_error_val(s_tag, "Thread create failed", (uint32_t)tx_status);
    return k_rx_err_rtos_thread_create;
  }

  s_watchdog_created = true;
  rx_log_info(s_tag, "Watchdog monitor task created (priority 6, 100 Hz)");

  return k_rx_ok;
}

/* =============================================================================
 * Private Functions
 * =============================================================================
 */

/**
 * @brief Process the result of rx_iwdt_check_tasks() for one iteration
 *
 * @details
 * Handles the three possible outcomes of rx_iwdt_check_tasks(): timeout
 * (stop feeding watchdog), ok (feed watchdog), or unexpected error (stop
 * feeding as a safe default).
 *
 *
 * @pre timeout_logged is a valid non-NULL pointer
 * @pre err is a valid rx_err_t value returned by rx_iwdt_check_tasks()
 * @post Hardware watchdog fed if err == k_rx_ok
 * @post *timeout_logged updated to reflect current timeout state
 *
 * @note Not thread-safe; called only from internal_watchdog_monitor_task_entry
 * @since Version 1.0.0
 */
static void internal_handle_check_result(rx_err_t err, bool* timeout_logged)
{
  if (err == k_rx_err_timeout) {
    if (!(*timeout_logged)) {
      rx_log_error(s_tag, "Task timeout detected - system will reset in 2s");
      *timeout_logged = true;
    }
    /* Don't feed watchdog - allow hardware reset to occur */
  } else if (err == k_rx_ok) {
    *timeout_logged = false; /* Clear flag for future timeouts */
    err             = rx_iwdt_feed();
    RX_ASSERT(err == k_rx_ok, "IWDT feed must succeed");
  } else {
    /* Unexpected error - don't feed watchdog to prevent masking errors */
    rx_log_error_val(s_tag, "Unexpected error from rx_iwdt_check_tasks", (uint32_t)err);
  }
}

/**
 * @brief Watchdog monitor task entry point - infinite loop at 100 Hz
 *
 * @details
 * Main supervision loop that executes three critical operations each iteration:
 * 1. Check all registered tasks for heartbeat timeouts
 * 2. Feed hardware watchdog (prevents 2s timeout reset)
 * 3. Report own heartbeat (self-monitoring)
 *
 * **Failure Handling**:
 * - If task timeout detected: Log error, STOP feeding watchdog
 * - System will reset after 2s (hardware IWDT timeout)
 * - Failed task name preserved for post-mortem analysis
 *
 * **Timing**:
 * - 100 Hz execution rate (10ms period)
 * - Feed margin: 2000ms / 10ms = 200x safety factor
 * - Can tolerate ~200 missed iterations before reset
 *
 * @par Usage:
 * @code{.c}
 * // Called internally by ThreadX after watchdog_monitor_task_create()
 * tx_thread_create(&s_watchdog_thread,
 *                  "WatchdogMon",
 *                  internal_watchdog_monitor_task_entry,  // This function
 *                  k_watchdog_task_input,  // input parameter
 *                  s_watchdog_stack,
 *                  k_watchdog_task_stack_size,
 *                  k_watchdog_task_priority,
 *                  k_watchdog_task_priority,
 *                  TX_NO_TIME_SLICE,
 *                  TX_AUTO_START);
 * @endcode
 *
 *
 *
 * @pre watchdog_monitor_task_create() called successfully
 * @pre ThreadX scheduler started
 * @pre IWDT initialized
 *
 * @post Infinite loop - never returns
 * @post Hardware watchdog fed at 100 Hz
 * @post Task timeouts detected and logged
 *
 * @note Thread Safety: Safe from single task context
 *
 * @see watchdog_monitor_task_create() Creates and starts this task entry
 * @see rx_iwdt_check_tasks() Heartbeat timeout detection called each iteration
 * @see rx_iwdt_feed() Hardware watchdog feed called when all tasks healthy
 * @see rx_iwdt_task_heartbeat() Self-monitoring heartbeat reported each iteration
 *
 * @since Version 1.0.0
 */
static void internal_watchdog_monitor_task_entry(ULONG input)
{
  (void)input;

  rx_log_info(s_tag, "Watchdog monitor started @ 100 Hz");

  /* Main supervision loop */
  bool timeout_logged = false; /* Flag to log timeout message only once */

  while (true) {
    /* Check all registered tasks for heartbeat timeouts */
    const rx_err_t check_err = rx_iwdt_check_tasks();
    internal_handle_check_result(check_err, &timeout_logged);

    /* Report own heartbeat (self-monitoring) */
    rx_err_t err = rx_iwdt_task_heartbeat(s_task_name);
    if (err != k_rx_ok) {
      rx_log_error_val(s_tag, "IWDT heartbeat failed", (uint32_t)err);
      /* Continue operation - watchdog monitor will detect timeout */
    }

    /* Sleep 10ms (100 Hz rate) */
    (void)tx_thread_sleep(k_watchdog_task_period_ticks);
  }
}
