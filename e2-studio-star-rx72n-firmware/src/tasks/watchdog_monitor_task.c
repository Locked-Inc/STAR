/* src/tasks/watchdog_monitor_task.c */

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
 * msc {
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
 * }
 * @endmsc
 *
 * ## Timeout Strategy
 *
 * - Fast tasks (10ms period): 30ms timeout (3× period)
 * - Medium tasks (20ms period): 60ms timeout (3× period)
 * - Slow tasks (50ms period): 150ms timeout (3× period)
 * - Very slow tasks (1000ms period): 3000ms timeout (3× period)
 *
 * **Rationale**: 3× period allows 2 missed heartbeats before timeout,
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
 * @author STAR Team
 * @date 2026-02-16
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
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
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_watchdog_task_stack_size = 512, /**< Stack size in bytes (minimal logic) */
  k_watchdog_task_priority   = 6,   /**< ThreadX priority (high - safety critical) */
  k_watchdog_task_input      = 0,   /**< Thread entry input parameter */
  k_watchdog_task_period_ticks =
    1, /**< 10ms period = 100 Hz (1 tick @ 100 Hz tick rate) */
} watchdog_task_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief ThreadX thread control block */
static TX_THREAD s_watchdog_thread;

/** @brief Static thread stack (no dynamic allocation) */
static uint8_t s_watchdog_stack[k_watchdog_task_stack_size];

/** @brief Task creation guard flag */
static bool s_watchdog_created = false;

/** @brief Log tag for this module */
static const char* const s_tag = "IWDT_MON";

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

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
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_invalid_state Already created
 * @retval k_rx_err_rtos_thread_create ThreadX error
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
  UINT tx_status;

  /* Check if already created */
  RX_ASSERT(!s_watchdog_created, "Watchdog monitor task already created");
  if (s_watchdog_created) {
    return k_rx_err_invalid_state;
  }

  /* Create the thread */
  tx_status = tx_thread_create(&s_watchdog_thread,
                               "WatchdogMon",
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
 * - Feed margin: 2000ms / 10ms = 200× safety factor
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
 * @param[in] input Thread input parameter (unused, always 0)
 *
 * @return void Function never returns (infinite loop)
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
 * @since Version 1.0.0
 */
static void internal_watchdog_monitor_task_entry(ULONG input)
{
  (void)input;

  rx_log_info(s_tag, "Watchdog monitor started @ 100 Hz");

  /* Main supervision loop */
  while (true) {
    /* Check all registered tasks for heartbeat timeouts */
    rx_err_t err = rx_iwdt_check_tasks();
    if (err == k_rx_err_timeout) {
      /* Task timeout detected - system will reset in ~2s */
      rx_log_error(s_tag, "Task timeout detected - system will reset in 2s");
      /* Don't feed watchdog - allow hardware reset to occur */
    }

    /* Feed hardware watchdog (prevents 2s timeout reset) */
    if (err != k_rx_err_timeout) {
      err = rx_iwdt_feed();
      RX_ASSERT(err == k_rx_ok, "IWDT feed must succeed");
    }

    /* Report own heartbeat (self-monitoring) */
    err = rx_iwdt_task_heartbeat("WatchdogMon");
    if (err != k_rx_ok) {
      rx_log_error_val(s_tag, "IWDT heartbeat failed", (uint32_t)err);
      /* Continue operation - watchdog monitor will detect timeout */
    }

    /* Sleep 10ms (100 Hz rate) */
    (void)tx_thread_sleep(k_watchdog_task_period_ticks);
  }
}
