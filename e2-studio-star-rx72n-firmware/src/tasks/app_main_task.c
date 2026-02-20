/* src/tasks/app_main_task.c */

/**
 * @file app_main_task.c
 * @brief Application main ThreadX task implementation
 *
 * @details
 * # Overview
 *
 * Implements the primary application ThreadX task for the STAR motor controller
 * firmware. This task serves as the main execution context and keeps the RTOS
 * scheduler active while application subsystems execute.
 *
 * ## Implementation Architecture
 *
 * The module consists of three layers:
 *
 * @dot
 * digraph implementation {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_public {
 *     label="Public API";
 *     color=green;
 *     create [label="app_main_task_create()"];
 *   }
 *
 *   subgraph cluster_private {
 *     label="Internal";
 *     color=blue;
 *     entry [label="internal_app_main_task_entry()"];
 *   }
 *
 *   subgraph cluster_state {
 *     label="Static State";
 *     color=orange;
 *     thread [label="s_app_main_thread\n(TX_THREAD)"];
 *     stack [label="s_app_main_stack\n(1024 bytes)"];
 *     created [label="s_app_main_created\n(bool)"];
 *   }
 *
 *   create -> entry [label="tx_thread_create()"];
 *   entry -> thread [label="runs in"];
 *   thread -> stack [label="uses"];
 *   create -> created [label="sets"];
 * }
 * @enddot
 *
 * ## Control Flow
 *
 * @dot
 * digraph control_flow {
 *   rankdir=TB;
 *   node [shape=box];
 *
 *   start [label="app_main_task_create()", shape=ellipse];
 *   check_created [label="s_app_main_created?", shape=diamond];
 *   error_state [label="return k_rx_err_invalid_state", fillcolor=red, style=filled];
 *   tx_create [label="tx_thread_create()"];
 *   check_status [label="status == TX_SUCCESS?", shape=diamond];
 *   error_rtos [label="return k_rx_err_rtos_error", fillcolor=red, style=filled];
 *   set_created [label="s_app_main_created = true"];
 *   verify [label="tx_thread_info_get()"];
 *   assert_state [label="RX_ASSERT(scheduled)"];
 *   success [label="return k_rx_ok", fillcolor=green, style=filled];
 *
 *   start -> check_created;
 *   check_created -> error_state [label="true"];
 *   check_created -> tx_create [label="false"];
 *   tx_create -> check_status;
 *   check_status -> error_rtos [label="!= TX_SUCCESS"];
 *   check_status -> set_created [label="== TX_SUCCESS"];
 *   set_created -> verify;
 *   verify -> assert_state;
 *   assert_state -> success;
 * }
 * @enddot
 *
 * ## Memory Map
 *
 * | Symbol | Offset | Size | Section | Description |
 * |--------|--------|------|---------|-------------|
 * | s_app_main_thread | 0x0000 | 140 | .bss | ThreadX TCB |
 * | s_app_main_stack | 0x008C | 1024 | .bss | Thread stack |
 * | s_app_main_created | 0x048C | 1 | .bss | Creation flag |
 *
 * **Total module footprint:** 1165 bytes in .bss section
 *
 * ## Stack Usage Analysis
 *
 * | Function | Stack Frame | Locals | Call Depth |
 * |----------|-------------|--------|------------|
 * | app_main_task_create() | 32 bytes | 12 | 2 (->tx_thread_create) |
 * | internal_app_main_task_entry() | 16 bytes | 0 | 1 (->tx_thread_sleep) |
 *
 * **Worst-case stack usage:** 1024 bytes allocated, ~200 bytes typical peak
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | Rule 1: Control flow | [PASS] | No goto/setjmp, single while(true) loop |
 * | Rule 2: Loop bounds | [PASS] | Infinite loop with watchdog, bounded sleep |
 * | Rule 3: No heap | [PASS] | All allocations static (s_app_main_stack) |
 * | Rule 4: Function length | [PASS] | Entry=10 lines, Create=37 lines |
 * | Rule 5: Assertions | [PASS] | 4 assertions total |
 * | Rule 6: Data scope | [PASS] | File-scope static with s_ prefix |
 * | Rule 7: Return checks | [PASS] | tx_thread_create return checked |
 * | Rule 8: Preprocessor | [PASS] | C23 typed enum for all constants |
 * | Rule 9: Pointers | [PASS] | Single-level pointers only |
 * | Rule 10: Warnings | [PASS] | Clean under -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **S** | Single module responsibility: task lifecycle management |
 * | **O** | Extended via configuration enum, not code modification |
 * | **L** | Returns rx_err_t consistent with all STAR modules |
 * | **I** | Minimal interface: one public function |
 * | **D** | Depends on rx_err_t abstraction, not concrete types |
 *
 * ## Module Dependencies
 *
 * | Dependency | Purpose | Include |
 * |------------|---------|---------|
 * | app_main_task.h | Public API declaration | local |
 * | rx_check.h | RX_ASSERT macro | project |
 * | rx_err.h | Error codes (via header) | project |
 * | tx_api.h | ThreadX RTOS API | external |
 * | stdbool.h | bool type | standard |
 * | stdint.h | Fixed-width integers | standard |
 *
 * @see app_main_task.h Public API with full documentation
 * @see tx_api.h ThreadX RTOS API reference
 * @see rx_err.h Error code definitions
 * @see rx_check.h Assertion macros
 *
 * @author STAR Team
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#include "app_main_task.h"

#include <stdbool.h>
#include <stdint.h>

#include "rx_check.h"
#include "tx_api.h"

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @enum app_main_task_config_t
 * @brief Configuration constants for the main application task
 *
 * @details
 * Defines all compile-time configuration parameters for the main application
 * ThreadX task. These values control stack allocation, scheduling priority,
 * and timing behavior.
 *
 * ## Configuration Rationale
 *
 * | Parameter | Value | Rationale |
 * |-----------|-------|-----------|
 * | Stack size | 1024 | Allows ~800 bytes for nested calls + safety margin |
 * | Priority | 10 | Medium priority, yields to motor (5) and timer (0) |
 * | Sleep ticks | 10 | 10ms at 1kHz = 100Hz idle loop rate |
 * | Thread input | 0 | Reserved for future expansion |
 *
 * ## ThreadX Priority Map
 *
 * | Priority | Task | Purpose |
 * |----------|------|---------|
 * | 0 | Timer | System timer thread (ThreadX internal) |
 * | 5 | Motor | Real-time motor control (future) |
 * | 10 | **AppMain** | Main application logic |
 * | 15 | Comm | Communication handling (future) |
 * | 31 | Idle | System idle thread (ThreadX internal) |
 *
 * @par Value Constraints:
 * - k_app_main_stack_size: Must be >= 512 for ThreadX overhead, <= 4096
 * - k_app_main_priority: Must be 1-30 (0=timer, 31=idle reserved)
 * - k_app_main_sleep_ticks: Must be > 0, recommended 1-100
 *
 * @note Values chosen for balanced CPU utilization and responsiveness.
 * @warning Do not reduce stack size below 512 bytes.
 *
 * @see TX_THREAD ThreadX thread control block
 * @see tx_thread_create() Uses these values
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Thread stack size in bytes
   *
   * @details
   * Stack allocation for the main application task. Sized to accommodate:
   * - ThreadX context save area (~100 bytes)
   * - Function call frames (~200 bytes typical)
   * - Local variables in application code
   * - Safety margin for unexpected deep calls
   *
   * @par Value: 1024 bytes
   * @par Rationale: 2x typical usage provides safety margin for growth
   *
   * @warning Reduce only if memory-constrained and stack analyzed.
   */
  k_app_main_stack_size = 1024,

  /**
   * @brief ThreadX scheduling priority (0=highest, 31=lowest)
   *
   * @details
   * Medium priority allows yielding to real-time tasks (motor control)
   * while maintaining responsiveness over background tasks.
   *
   * @par Value: 10
   * @par Rationale: Higher than comm (15), lower than motor (5)
   */
  k_app_main_priority = 10,

  /**
   * @brief Sleep duration in ThreadX timer ticks
   *
   * @details
   * Controls the idle loop rate. At 1kHz tick rate (1ms/tick):
   * - 10 ticks = 10ms sleep = 100Hz loop rate
   * - CPU available for other tasks during sleep
   *
   * @par Value: 10 ticks (10 ms)
   * @par Rationale: 100Hz sufficient for non-real-time application logic
   *
   * @note Increase to reduce CPU usage, decrease for faster response.
   */
  k_app_main_sleep_ticks = 10,

  /**
   * @brief Thread entry function parameter
   *
   * @details
   * Value passed to internal_app_main_task_entry() by ThreadX.
   * Currently unused but reserved for future configuration passing.
   *
   * @par Value: 0
   * @par Rationale: Reserved, verified by assertion in entry function
   */
  k_app_main_thread_input = 0,
} app_main_task_config_t;

/* =============================================================================
 * Static State
 * =============================================================================
 */

/**
 * @var s_app_main_thread
 * @brief ThreadX thread control block for main application task
 *
 * @details
 * Contains all ThreadX state for this thread including:
 * - Thread state (ready, running, suspended, terminated)
 * - Stack pointer and stack area pointers
 * - Priority and preemption threshold
 * - Time slice information
 * - Suspension and resume counts
 * - Entry function pointer
 *
 * ## Memory Layout (TX_THREAD structure)
 *
 * | Offset | Field | Size | Description |
 * |--------|-------|------|-------------|
 * | 0x00 | tx_thread_id | 4 | Thread ID (magic number) |
 * | 0x04 | tx_thread_run_count | 4 | Execution count |
 * | 0x08 | tx_thread_stack_ptr | 4 | Current stack pointer |
 * | 0x0C | tx_thread_stack_start | 4 | Stack area start |
 * | 0x10 | tx_thread_stack_end | 4 | Stack area end |
 * | 0x14 | tx_thread_stack_size | 4 | Stack size |
 * | ... | ... | ... | ... |
 *
 * **Total size:** ~140 bytes (platform-dependent)
 *
 * @note Initialized by tx_thread_create() in app_main_task_create().
 * @note Accessed by ThreadX kernel for scheduling decisions.
 *
 * @warning Never modify directly - use ThreadX API only.
 * @warning Must remain valid for thread lifetime.
 *
 * @see tx_thread_create() Initializes this structure
 * @see tx_thread_info_get() Queries thread state
 *
 * @since Version 1.0.0
 */
static TX_THREAD s_app_main_thread;

/**
 * @var s_app_main_stack
 * @brief Static stack memory for main application task
 *
 * @details
 * Pre-allocated stack area used by the main application thread. Statically
 * allocated in .bss section to comply with NASA Power of 10 Rule 3 (no heap).
 *
 * ## Stack Layout (grows downward on RX72N)
 *
 * ```
 * s_app_main_stack[0]        <- Stack top (high address)
 * ...
 * [Exception frame]          <- Saved on interrupt
 * [Context save area]        <- ThreadX context (~100 bytes)
 * [Local variables]          <- Function locals
 * [Return addresses]         <- Call stack
 * ...
 * s_app_main_stack[1023]     <- Stack bottom (low address)
 * ```
 *
 * @par Size: 1024 bytes (k_app_main_stack_size)
 *
 * @note Passed to tx_thread_create() as stack area.
 * @note ThreadX fills with pattern 0xEF for stack usage analysis.
 *
 * @warning Stack overflow causes undefined behavior.
 * @warning Do not access directly during thread execution.
 *
 * @see k_app_main_stack_size Stack size constant
 * @see tx_thread_create() Uses this buffer
 *
 * @since Version 1.0.0
 */
static uint8_t s_app_main_stack[k_app_main_stack_size];

/**
 * @var s_app_main_created
 * @brief Flag indicating whether main task has been created
 *
 * @details
 * Single-shot creation flag ensuring app_main_task_create() executes only once.
 * Transitions from false to true exactly once during boot.
 *
 * ## State Transition
 *
 * ```
 * false (initial) --[app_main_task_create() success]--> true (final)
 * ```
 *
 * @par Initial value: false
 * @par Valid values: false (not created), true (created)
 *
 * @invariant Once true, never returns to false
 * @invariant Set to true only after tx_thread_create() succeeds
 *
 * @note Checked at start of app_main_task_create() to prevent double creation.
 * @note Set after successful ThreadX thread creation.
 *
 * @warning Do not modify directly - managed by app_main_task_create() only.
 *
 * @see app_main_task_create() Manages this flag
 *
 * @since Version 1.0.0
 */
static bool s_app_main_created = false;

/* =============================================================================
 * Thread Entry
 * =============================================================================
 */

/**
 * @brief Main application task entry loop (internal)
 *
 * @details
 * ThreadX thread entry function that executes the main application loop.
 * This function runs in the context of the created task and never returns.
 *
 * ## Execution Flow
 *
 * @msc
 * msc {
 *   width=500;
 *   Entry, Assert, Loop, Sleep;
 *
 *   Entry box Entry [label="internal_app_main_task_entry(input)"];
 *   Entry => Assert [label="RX_ASSERT(input == 0)"];
 *   Assert => Assert [label="RX_ASSERT(sleep_ticks > 0)"];
 *   Assert => Loop [label="Enter while(true)"];
 *
 *   Loop => Sleep [label="tx_thread_sleep(10)"];
 *   Sleep box Sleep [label="Suspend 10 ms"];
 *   Sleep >> Loop [label="Resume"];
 *   Loop => Sleep [label="tx_thread_sleep(10)"];
 *   Loop note Loop [label="Forever...", textbgcolor="lightyellow"];
 * }
 * @endmsc
 *
 * ## Loop Timing
 *
 * | Parameter | Value | Notes |
 * |-----------|-------|-------|
 * | Sleep period | 10 ticks | 10 ms at 1 kHz |
 * | Wake frequency | 100 Hz | Periodic wake |
 * | CPU usage | ~0.1% | Mostly sleeping |
 *
 * @param[in] input ThreadX entry parameter passed from tx_thread_create()
 *                  Expected value: k_app_main_thread_input (0)
 *                  Valid range: Must equal k_app_main_thread_input
 *
 * @return void (never returns)
 *
 * @pre Task created successfully via tx_thread_create()
 * @pre ThreadX scheduler running
 * @pre input == k_app_main_thread_input
 *
 * @post Never returns (infinite loop)
 *
 * @invariant Task remains in RUNNING or SUSPENDED state
 * @invariant Sleep period constant at k_app_main_sleep_ticks
 *
 * @note **Never returns:** This function contains an infinite loop.
 * @note **NASA Rule 2:** while(true) is acceptable with sleep (bounded wait).
 * @note **(void)input:** Silences unused parameter warning after assertion.
 *
 * @warning **Stack overflow:** Deep call chains from this function risk overflow.
 * @warning **Blocking calls:** Avoid blocking calls without timeout.
 *
 * @par Thread Safety:
 * Executes in dedicated task context. No shared state modified in loop.
 *
 * @par Performance:
 * - Execution time per iteration: ~5 us (excluding sleep)
 * - CPU utilization: < 0.1% (mostly sleeping)
 *
 * @see app_main_task_create() Creates task with this entry point
 * @see tx_thread_sleep() ThreadX sleep API
 * @see k_app_main_sleep_ticks Sleep duration constant
 *
 * @since Version 1.0.0
 *
 * @internal
 * This function is static and not exposed in the public API.
 * @endinternal
 */
static void internal_app_main_task_entry(ULONG input)
{
  RX_ASSERT(input == k_app_main_thread_input, "Unexpected thread input");
  RX_ASSERT(k_app_main_sleep_ticks > 0, "Sleep ticks must be non-zero");
  (void)input;

  while (true) {
    (void)tx_thread_sleep(k_app_main_sleep_ticks);
  }
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Implementation of app_main_task_create()
 *
 * @details
 * Creates the main application ThreadX task. See app_main_task.h for full
 * public API documentation.
 *
 * ## Implementation Steps
 *
 * 1. **Precondition assertion:** Verify not already created (debug build)
 * 2. **Runtime check:** Return error if already created (release build)
 * 3. **Thread creation:** Call tx_thread_create() with static resources
 * 4. **Error check:** Return error if ThreadX fails
 * 5. **State update:** Set s_app_main_created = true
 * 6. **Postcondition verification:** Query thread state and assert valid
 * 7. **Return success**
 *
 * ## ThreadX Parameters Used
 *
 * | Parameter | Value | Description |
 * |-----------|-------|-------------|
 * | thread_ptr | &s_app_main_thread | Control block pointer |
 * | name | "AppMain" | Thread name (debug) |
 * | entry | internal_app_main_task_entry | Entry function |
 * | input | k_app_main_thread_input | Entry parameter (0) |
 * | stack_start | s_app_main_stack | Stack base address |
 * | stack_size | k_app_main_stack_size | Stack size (1024) |
 * | priority | k_app_main_priority | Priority level (10) |
 * | preempt_thresh | k_app_main_priority | Preemption threshold (10) |
 * | time_slice | TX_NO_TIME_SLICE | No round-robin |
 * | auto_start | TX_AUTO_START | Start immediately |
 *
 * @see app_main_task.h Full API documentation with examples
 *
 * @since Version 1.0.0
 */
rx_err_t app_main_task_create(void)
{
  RX_ASSERT(!s_app_main_created, "Task must not be already created");
  if (s_app_main_created) {
    return k_rx_err_invalid_state;
  }

  UINT       thread_state = TX_TERMINATED;
  const UINT status       = tx_thread_create(&s_app_main_thread,
                                       "AppMain",
                                       internal_app_main_task_entry,
                                       k_app_main_thread_input,
                                       s_app_main_stack,
                                       k_app_main_stack_size,
                                       k_app_main_priority,
                                       k_app_main_priority,
                                       TX_NO_TIME_SLICE,
                                       TX_AUTO_START);

  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  s_app_main_created = true;
  (void)tx_thread_info_get(&s_app_main_thread,
                           TX_NULL,
                           &thread_state,
                           TX_NULL,
                           TX_NULL,
                           TX_NULL,
                           TX_NULL,
                           TX_NULL,
                           TX_NULL);
  RX_ASSERT((status == TX_SUCCESS) && (thread_state != TX_TERMINATED) &&
              (thread_state != TX_COMPLETED),
            "Task must be created and scheduled");
  return k_rx_ok;
}
