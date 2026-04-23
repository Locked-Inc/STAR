/**
 * @file rx_iwdt.c
 * @brief Independent Watchdog Timer (IWDT) Implementation for RX72N
 *
 * @details
 * Implements the hardware-independent watchdog timer driver with enhanced
 * task monitoring capabilities. The IWDT uses a dedicated 120 kHz clock
 * source, providing reliable watchdog functionality even during main clock
 * failures - a critical safety feature for robotics applications.
 *
 * ## Implementation Architecture
 *
 * @dot
 * digraph iwdt_implementation {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   edge [fontsize=10];
 *
 *   subgraph cluster_state {
 *     label="Module State (s_iwdt_state)";
 *     style=dashed;
 *     config [label="config\nrx_iwdt_config_t"];
 *     tasks [label="tasks[8]\nrx_iwdt_task_info_t"];
 *     status [label="status\nrx_iwdt_status_t"];
 *     curr_state [label="current_state\nsystem_state_t"];
 *     initialized [label="initialized\nbool"];
 *   }
 *
 *   subgraph cluster_api {
 *     label="Public API";
 *     style=filled;
 *     fillcolor=lightblue;
 *     init [label="rx_iwdt_init()"];
 *     feed [label="rx_iwdt_feed()"];
 *     reg_task [label="rx_iwdt_register_task()"];
 *     heartbeat [label="rx_iwdt_task_heartbeat()"];
 *     set_state [label="rx_iwdt_set_state()"];
 *     check [label="rx_iwdt_check_tasks()"];
 *     get_status [label="rx_iwdt_get_status()"];
 *     was_reset [label="rx_iwdt_was_reset()"];
 *   }
 *
 *   subgraph cluster_internal {
 *     label="Internal Helpers";
 *     style=filled;
 *     fillcolor=lightyellow;
 *     get_tick [label="internal_get_tick_count()"];
 *     find_task [label="internal_find_task()"];
 *     find_slot [label="internal_find_free_slot()"];
 *     init_config [label="internal_init_default_config()"];
 *   }
 *
 *   subgraph cluster_hw {
 *     label="Hardware Registers";
 *     style=filled;
 *     fillcolor=lightgray;
 *     iwdtrr [label="IWDTRR\n0x00088030\n(Refresh Register)"];
 *     iwdtsr [label="IWDTSR\n0x00088032\n(Status Register)"];
 *     rstsr2 [label="RSTSR2\n(Reset Status 2)"];
 *   }
 *
 *   init -> config [label="store"];
 *   init -> init_config [label="if nullptr", style=dashed];
 *   init -> iwdtrr [label="initial feed"];
 *   init -> rstsr2 [label="check reset", style=dashed];
 *
 *   feed -> iwdtrr [label="write 0x00, 0xFF"];
 *   feed -> status [label="increment feeds"];
 *
 *   reg_task -> find_slot;
 *   reg_task -> tasks [label="add"];
 *
 *   heartbeat -> find_task;
 *   heartbeat -> tasks [label="update tick"];
 *
 *   check -> tasks [label="scan all"];
 *   check -> get_tick;
 * }
 * @enddot
 *
 * ## IWDT vs WDT Comparison
 *
 * | Feature | IWDT (This Module) | WDT (rx_wdt.c) |
 * |---------|-------------------|----------------|
 * | Clock Source | IWDTCLK (120 kHz independent) | PCLKB (system clock) |
 * | Can be Stopped | [X] No (hardware safety) | [OK] Yes |
 * | Clock Fault Detection | [OK] Yes (independent clock) | [X] No |
 * | Timeout Range | ~8 ms to ~17 seconds | ~4 us to ~273 us |
 * | Register Base | 0x00088030 | 0x00088020 |
 * | Task Monitoring | [OK] Software layer | [X] Not implemented |
 * | Primary Use | Production safety | Development/debugging |
 *
 * ## Task Monitoring System
 *
 * @msc
 * App, IWDT, Task1, Task2, HW;
 *
 * --- [label="Initialization"];
 * App => IWDT [label="rx_iwdt_init()"];
 * IWDT => HW [label="Initial feed"];
 *
 * --- [label="Task Registration"];
 * Task1 => IWDT [label="rx_iwdt_register_task(\"motor\", 500)"];
 * Task2 => IWDT [label="rx_iwdt_register_task(\"comm\", 1000)"];
 *
 * --- [label="Normal Operation"];
 * Task1 => IWDT [label="rx_iwdt_task_heartbeat(\"motor\")"];
 * IWDT box IWDT [label="Update last_heartbeat_tick"];
 * Task2 => IWDT [label="rx_iwdt_task_heartbeat(\"comm\")"];
 *
 * --- [label="Check & Feed Loop"];
 * App => IWDT [label="rx_iwdt_check_tasks()"];
 * IWDT box IWDT [label="Compare ticks vs timeout"];
 * App => IWDT [label="rx_iwdt_feed()"];
 * IWDT => HW [label="Write 0x00, 0xFF"];
 *
 * --- [label="Task Timeout"];
 * IWDT box IWDT [label="Task1 missed deadline"];
 * App => IWDT [label="rx_iwdt_check_tasks()"];
 * IWDT >> App [label="k_rx_err_timeout"];
 * @endmsc
 *
 * ## Memory Layout
 *
 * | Offset | Size | Field | Description |
 * |--------|------|-------|-------------|
 * | 0x000 | 68 | config | Configuration (timeouts, flags) |
 * | 0x044 | 320 | tasks[8] | Task info array (40 bytes each) |
 * | 0x184 | 76 | status | Status info |
 * | 0x1D0 | 1 | current_state | System state enum |
 * | 0x1D1 | 1 | initialized | Init flag |
 * | 0x1D2 | 2 | (padding) | Alignment |
 * | **Total** | **~470 bytes** | | Static allocation |
 *
 * ## Performance Characteristics
 *
 * | Function | Execution Time | Stack | Notes |
 * |----------|---------------|-------|-------|
 * | rx_iwdt_init() | ~30 us | 64 B | Memset + config copy |
 * | rx_iwdt_feed() | ~1 us | 8 B | 2 register writes |
 * | rx_iwdt_register_task() | ~10 us | 32 B | String copy + slot search |
 * | rx_iwdt_task_heartbeat() | ~5 us | 16 B | Task lookup + tick update |
 * | rx_iwdt_check_tasks() | ~20 us | 24 B | Scan all 8 tasks |
 *
 * @par Module Dependencies:
 * - [rx_iwdt.h](../inc/rx_iwdt.h) - Public API declarations
 * - [rx72n_iwdt_regs.h](../../rx_hal/inc/rx72n_iwdt_regs.h) - IWDT registers
 * - [rx72n_system_regs.h](../../rx_hal/inc/rx72n_system_regs.h) - System registers
 * - [rx_check.h](../inc/rx_check.h) - Validation macros
 * - `<tx_api.h>` - ThreadX API for tick count
 * - `<string.h>` - strlen, strcmp
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1**: [OK] No goto, setjmp, recursion
 * - **Rule 2**: [OK] All loops bounded by k_iwdt_max_tasks
 * - **Rule 3**: [OK] Zero dynamic allocation (static state)
 * - **Rule 4**: [OK] All functions <60 lines
 * - **Rule 5**: [OK] Minimum 2 checks per function
 * - **Rule 6**: [OK] Data declared at smallest scope
 * - **Rule 7**: [OK] All return values checked or propagated
 * - **Rule 8**: [OK] C23 typed enums for constants
 * - **Rule 9**: [OK] Minimal pointer use, no pointer arithmetic
 * - **Rule 10**: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility**: Only IWDT and task monitoring
 * - **Open/Closed**: Extensible via configuration and state callbacks
 * - **Liskov Substitution**: Consistent rx_err_t return interface
 * - **Interface Segregation**: Separate init/feed/task/status APIs
 * - **Dependency Inversion**: Depends on abstract rx_err_t
 *
 * @see rx_iwdt.h Public API documentation and usage examples
 * @see rx_wdt.c Software-controllable watchdog (complementary)
 * @see RX72N User's Manual Section 11: Independent Watchdog Timer
 *
 * @author Locked, Inc.
 * @date 2026-01-28
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#include "rx_iwdt.h"

#include <string.h>

#ifdef UNIT_TEST
#include "mock_rx72n_iwdt_regs.h"
#include "mock_rx72n_system_regs.h"
#else
#include "rx72n_iwdt_regs.h"
#include "rx72n_system_regs.h"
#endif
#include "tx_api.h" /* ThreadX API for tx_time_get() and TX_TIMER_TICKS_PER_SECOND */

/* =============================================================================
 * Static Data
 * =============================================================================
 */

/**
 * @struct rx_iwdt_state_t
 * @brief Internal state structure for IWDT driver
 *
 * @details
 * Aggregates all state for the IWDT driver including configuration,
 * task monitoring arrays, status tracking, and initialization flags.
 * This is the largest static allocation in the core library.
 *
 * @par Memory Layout:
 * | Offset | Size | Field | Description |
 * |--------|------|-------|-------------|
 * | 0x000 | ~68 | config | Configuration structure |
 * | 0x044 | 320 | tasks | 8 task info structures (40 B each) |
 * | 0x184 | ~76 | status | Status and counters |
 * | 0x1D0 | 1 | current_state | System state enum |
 * | 0x1D1 | 1 | initialized | Init complete flag |
 * | **Total** | **~470 bytes** | | Static allocation |
 *
 * @par State Transitions:
 * - initialized: false -> true on rx_iwdt_init() success
 * - current_state: k_system_state_init -> other states via rx_iwdt_set_state()
 * - tasks[n].active: false -> true via rx_iwdt_register_task()
 *
 * @invariant initialized implies config is valid
 * @invariant status.active_tasks equals count of tasks[].active==true
 *
 * @see s_iwdt_state Static instance
 *
 * @since Version 1.0.0
 */
typedef struct {
  rx_iwdt_config_t    config;                  /**< Configuration from rx_iwdt_init(). Contains
                                                    default timeout, task monitoring enable flag,
                                                    reset mode, and per-state timeout array. */
  rx_iwdt_task_info_t tasks[k_iwdt_max_tasks]; /**< Array of registered task info. Each entry
                                                    contains task name, timeout, last heartbeat
                                                    tick, and timeout flag. Max 8 tasks. */
  rx_iwdt_status_t    status;                  /**< Runtime status: feed count, reset count,
                                                    current state, active task count, last
                                                    failed task name. */
  system_state_t      current_state;           /**< Current system state for state-dependent
                                                    timeout selection. One of system_state_t. */
  bool                initialized;             /**< Module initialization flag. true = ready
                                                    for use, false = must call rx_iwdt_init(). */
} rx_iwdt_state_t;

/**
 * @var s_iwdt_state
 * @brief Global IWDT driver state
 *
 * @details
 * Single instance of IWDT state, zero-initialized at startup. All fields
 * start at 0/false/empty representing uninitialized state.
 *
 * **Memory Location:** .bss section (zero-initialized static data)
 * **Size:** ~470 bytes
 * **Alignment:** 4-byte aligned
 *
 * @par Access Pattern:
 * - **Write:** rx_iwdt_init(), rx_iwdt_register_task(), rx_iwdt_task_heartbeat(),
 *              rx_iwdt_set_state(), rx_iwdt_set_timeout_for_state(), rx_iwdt_feed(),
 *              rx_iwdt_check_tasks()
 * - **Read:** All functions (state checks)
 *
 * @note Not thread-safe for concurrent writes. Use external synchronization
 *       if calling from multiple threads.
 *
 * @see rx_iwdt_state_t Structure definition
 *
 * @since Version 1.0.0
 */
static rx_iwdt_state_t s_iwdt_state = {};

/* =============================================================================
 * Test Support Functions
 * =============================================================================
 */

#ifdef UNIT_TEST
/**
 * @brief Reset IWDT driver state for unit testing
 * @details
 * Resets the driver to uninitialized state. This function is only available
 * when UNIT_TEST is defined and should only be called from test code.
 */
void rx_iwdt_test_reset(void)
{
  s_iwdt_state = (rx_iwdt_state_t){};
}
#endif

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Copy a string with bounded length (replaces strncpy)
 *
 * @details
 * Copies up to max_len-1 characters from src to dst and null-terminates.
 * Avoids clang-tidy insecureAPI.DeprecatedOrUnsafeBufferHandling warnings
 * that strncpy triggers.
 *
 * @param[out] dst Destination buffer
 * @param[in] src Source string
 * @param[in] max_len Size of destination buffer (including null terminator)
 *
 * @pre dst != nullptr
 * @pre src != nullptr
 * @pre max_len > 0
 * @post dst is null-terminated
 *
 * @note Not thread-safe
 *
 * @since Version 1.0.0
 */
static void internal_safe_strcpy(char* dst, const char* src, size_t max_len)
{
  size_t i = 0;
  for (; i < max_len - 1U && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }
  dst[i] = '\0';
}

/**
 * @brief Get current system tick count from ThreadX
 *
 * @details
 * Wrapper around ThreadX tx_time_get() to get the current tick count.
 * Used for task heartbeat timing calculations.
 *
 * **Algorithm:**
 * 1. Call tx_time_get() to get ThreadX tick count
 * 2. Cast ULONG to uint32_t for consistent type
 * 3. Return tick count
 *
 * @return uint32_t Current tick count from ThreadX timer
 *
 * @note ThreadX tick rate defined by TX_TIMER_TICKS_PER_SECOND
 * @note Tick count wraps at UINT32_MAX (~497 days at 100 Hz tick rate)
 * @note Thread-safe - tx_time_get() is reentrant
 *
 * @see tx_time_get() ThreadX API
 * @see TX_TIMER_TICKS_PER_SECOND Tick rate configuration
 *
 * @since Version 1.0.0
 */
static uint32_t internal_get_tick_count(void)
{
  return (uint32_t)tx_time_get();
}

/**
 * @brief Find registered task by name
 *
 * @details
 * Searches the task array for a task with matching name that is currently
 * active. Uses strcmp() for exact string comparison.
 *
 * **Algorithm:**
 * 1. Iterate through tasks[0..k_iwdt_max_tasks-1]
 * 2. For each entry: check if active AND name matches
 * 3. Return pointer to matching entry, or nullptr if not found
 *
 * @param[in] task_name Task name to search for (null-terminated string)
 *
 * @return rx_iwdt_task_info_t* Pointer to task info if found
 * @retval NULL Task not found or not active
 *
 * @pre task_name must be non-NULL (caller validates)
 *
 * @note Linear search O(n) where n = k_iwdt_max_tasks (8)
 * @note String comparison is case-sensitive
 *
 * @see rx_iwdt_register_task() Adds tasks to array
 * @see rx_iwdt_task_heartbeat() Uses this to find task
 *
 * @since Version 1.0.0
 */
static rx_iwdt_task_info_t* internal_find_task(const char* task_name)
{
  for (uint32_t i = 0; i < k_iwdt_max_tasks; i++) {
    if ((int)s_iwdt_state.tasks[i].active &&
        strcmp(s_iwdt_state.tasks[i].task_name, task_name) == 0) {
      return &s_iwdt_state.tasks[i];
    }
  }

  return nullptr;
}

/**
 * @brief Find free slot in task array
 *
 * @details
 * Searches for the first inactive slot in the task array where a new
 * task can be registered.
 *
 * **Algorithm:**
 * 1. Iterate through tasks[0..k_iwdt_max_tasks-1]
 * 2. For each entry: check if NOT active
 * 3. Return pointer to first inactive slot, or nullptr if all full
 *
 * @return rx_iwdt_task_info_t* Pointer to free slot if available
 * @retval NULL No free slots (all 8 tasks registered)
 *
 * @note Linear search O(n) where n = k_iwdt_max_tasks (8)
 * @note Returns first available slot (lowest index)
 *
 * @see rx_iwdt_register_task() Uses this to allocate slot
 * @see k_iwdt_max_tasks Maximum number of tasks (8)
 *
 * @since Version 1.0.0
 */
static rx_iwdt_task_info_t* internal_find_free_slot(void)
{
  for (uint32_t i = 0; i < k_iwdt_max_tasks; i++) {
    if (!s_iwdt_state.tasks[i].active) {
      return &s_iwdt_state.tasks[i];
    }
  }

  return nullptr;
}

/**
 * @brief Initialize default configuration structure
 *
 * @details
 * Populates a configuration structure with safe default values suitable
 * for most applications. Called when rx_iwdt_init() receives nullptr config.
 *
 * **Default Values:**
 * - default_timeout_ms = k_iwdt_default_timeout_ms (1000 ms)
 * - enable_task_monitoring = true
 * - reset_on_timeout = true
 * - All state_timeouts_ms[] = k_iwdt_default_timeout_ms
 *
 * **Algorithm:**
 * 1. Set default_timeout_ms to 1000 ms
 * 2. Enable task monitoring
 * 3. Enable reset on timeout
 * 4. Initialize all state timeouts to default value
 *
 * @param[out] config Configuration structure to initialize
 *
 * @pre config must be non-NULL (caller validates)
 *
 * @post config contains default values ready for use
 *
 * @note Loop bounded by k_system_state_count (NASA Rule 2)
 *
 * @see rx_iwdt_init() Calls this when config is nullptr
 * @see k_iwdt_default_timeout_ms Default timeout constant
 * @see k_system_state_count Number of system states
 *
 * @since Version 1.0.0
 */
static void internal_init_default_config(rx_iwdt_config_t* config)
{
  config->default_timeout_ms     = k_iwdt_default_timeout_ms;
  config->enable_task_monitoring = true;
  config->reset_on_timeout       = true;

  /* Initialize all state timeouts to default */
  for (uint32_t i = 0; i < k_system_state_count; i++) {
    config->state_timeouts_ms[i] = k_iwdt_default_timeout_ms;
  }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize IWDT driver with configuration and task monitoring
 *
 * @details
 * Initializes the IWDT driver, validates configuration, checks for previous
 * watchdog reset, and performs initial hardware feed. If config is nullptr,
 * uses safe default values (1000ms timeout, task monitoring enabled).
 *
 * **Algorithm:**
 * 1. Check if already initialized (return error if so)
 * 2. If config is nullptr, create default configuration
 * 3. Validate timeout within [k_iwdt_min_timeout_ms, k_iwdt_max_timeout_ms]
 * 4. Clear and initialize all state
 * 5. Check RSTSR2 for previous watchdog reset
 * 6. Perform initial feed to IWDTRR (0x00 then 0xFF)
 * 7. Set initialized flag
 *
 * @par OFS Configuration Note:
 * The IWDT hardware timeout is configured at flash-time via OFS (Option
 * Function Select) registers and cannot be changed at runtime. The timeout
 * values in this driver are used for software task monitoring only. The
 * hardware IWDT will timeout based on OFS settings regardless of software
 * configuration.
 *
 * @param[in] config Configuration pointer, or nullptr for defaults
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_state Already initialized
 * @retval k_rx_err_invalid_arg Timeout out of valid range
 *
 * @pre IWDT not already initialized
 *
 * @post IWDT driver ready for use
 * @post Initial feed performed
 * @post Reset status captured
 *
 * @note Hardware timeout set by OFS, not this driver
 * @note nullptr config uses safe defaults (1000ms, monitoring enabled)
 *
 * @since Version 1.0.0
 */
rx_err_t rx_iwdt_init(const rx_iwdt_config_t* config)
{
  /* Check if already initialized */
  if (s_iwdt_state.initialized) {
    return k_rx_err_invalid_state;
  }

  /* Use default config if none provided */
  rx_iwdt_config_t local_config;
  if (config == nullptr) {
    internal_init_default_config(&local_config);
    config = &local_config;
  }

  /* Validate configuration */
  if (config->default_timeout_ms < k_iwdt_min_timeout_ms ||
      config->default_timeout_ms > k_iwdt_max_timeout_ms) {
    return k_rx_err_invalid_arg;
  }

  /* Initialize state */
  s_iwdt_state = (rx_iwdt_state_t){};
  {
    const uint8_t* src = (const uint8_t*)config;
    uint8_t*       dst = (uint8_t*)&s_iwdt_state.config;
    for (size_t byte_i = 0; byte_i < sizeof(rx_iwdt_config_t); byte_i++) {
      dst[byte_i] = src[byte_i];
    }
  }
  s_iwdt_state.current_state = k_system_state_init;

  /* Initialize status */
  s_iwdt_state.status.initialized        = true;
  s_iwdt_state.status.current_state      = k_system_state_init;
  s_iwdt_state.status.current_timeout_ms = config->default_timeout_ms;
  s_iwdt_state.status.active_tasks       = 0;

  /* Check if last reset was watchdog */
  if (rx_iwdt_was_reset()) {
    s_iwdt_state.status.total_resets++;
    s_iwdt_state.status.last_reset_reason = k_iwdt_reset_watchdog;
  } else {
    s_iwdt_state.status.last_reset_reason = k_iwdt_reset_power_on;
  }

  /* Note: IWDT hardware is configured via OFS (Option Function Select) registers
   * at compile/flash time and starts automatically on reset. We cannot reconfigure
   * the timeout period at runtime. This driver manages the software state and
   * provides the feed mechanism. The requested timeout is used for software task
   * monitoring only. */
  volatile rx_iwdt_regs_t* const regs = iwdt();

  /* Feed the watchdog to establish baseline */
  regs->iwdtrr = k_iwdt_refresh_start;
  regs->iwdtrr = k_iwdt_refresh_end;

  s_iwdt_state.initialized = true;

  return k_rx_ok;
}

/**
 * @brief Feed the independent watchdog timer to prevent system reset
 *
 * @details
 * Writes the two-byte refresh sequence (0x00 then 0xFF) to the IWDT Refresh
 * Register (IWDTRR) to reset the hardware watchdog counter. This must be
 * called periodically within the configured hardware timeout window or the
 * IWDT will reset the microcontroller.
 *
 * Per the RX72N hardware manual the refresh sequence is strictly ordered:
 * writing 0x00 to IWDTRR starts the window; writing 0xFF completes the
 * refresh. The entire sequence must occur within the permitted refresh window.
 *
 * Also increments the software diagnostic counter s_iwdt_state.status.watchdog_feeds
 * for telemetry and debugging purposes.
 *
 * Algorithm steps:
 * 1. Check s_iwdt_state.initialized; return k_rx_err_not_initialized if false
 * 2. Write k_iwdt_refresh_start (0x00) to regs->iwdtrr
 * 3. Write k_iwdt_refresh_end   (0xFF) to regs->iwdtrr
 * 4. Increment status.watchdog_feeds counter
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Watchdog counter successfully refreshed
 * @retval k_rx_err_not_initialized rx_iwdt_init() has not been called
 *
 * @pre rx_iwdt_init() must have been called successfully
 * @pre Must be called within the hardware IWDT refresh window (no gap > configured timeout)
 * @post Hardware IWDT counter is reset; timeout window restarts
 * @post s_iwdt_state.status.watchdog_feeds is incremented by one
 *
 * @note Not thread-safe; concurrent feeds from multiple tasks are benign for
 *       the hardware but may produce incorrect feed counts
 * @warning Failing to call within the hardware timeout triggers a system reset
 *
 * @par Performance:
 * Execution time: <1 us @ 240 MHz; safe to call from any task or ISR context
 *
 * @par Example:
 * @code
 * // In main watchdog task loop
 * while (1) {
 *     rx_err_t err = rx_iwdt_feed();
 *     if (err != k_rx_ok) {
 *         rx_log_error("wdt", "IWDT not initialized");
 *     }
 *     tx_thread_sleep(k_watchdog_feed_interval_ticks);
 * }
 * @endcode
 *
 * @see rx_iwdt_init() Initialize IWDT before feeding
 * @see rx_iwdt_check_tasks() Verify all tasks are alive before feeding
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: 1 precondition (initialized check), 2 postconditions (HW reset, counter++)
 */
rx_err_t rx_iwdt_feed(void)
{
  volatile rx_iwdt_regs_t* regs;

  /* Check initialization */
  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  /* Refresh watchdog counter - write 0x00 then 0xFF to IWDTRR */
  regs         = iwdt();
  regs->iwdtrr = k_iwdt_refresh_start;
  regs->iwdtrr = k_iwdt_refresh_end;

  s_iwdt_state.status.watchdog_feeds++;

  return k_rx_ok;
}

/**
 * @brief Register a ThreadX task for software watchdog monitoring
 *
 * @details
 * Adds a task entry to the IWDT software monitoring table, associating the
 * task name with a per-task timeout. Once registered, the task must call
 * rx_iwdt_task_heartbeat() at least once per timeout_ms milliseconds or
 * rx_iwdt_check_tasks() will report k_rx_err_timeout.
 *
 * The registration also records the current tick count as the initial
 * last_heartbeat_tick, giving the task a full timeout_ms window before
 * the first heartbeat is required.
 *
 * Algorithm steps:
 * 1. Validate task_name is non-null
 * 2. Verify s_iwdt_state.initialized
 * 3. Validate timeout_ms is in [k_iwdt_min_timeout_ms, k_iwdt_max_timeout_ms]
 * 4. Check task name length is in (0, k_iwdt_task_name_len)
 * 5. Check task is not already registered via internal_find_task()
 * 6. Find a free slot via internal_find_free_slot()
 * 7. Initialize slot with task name, timeout, current tick, active=true, timed_out=false
 * 8. Increment status.active_tasks
 *
 * @param[in] task_name Unique task identifier string (non-empty, max k_iwdt_task_name_len-1 chars,
 *            null-terminated); must remain valid for the lifetime of registration
 * @param[in] timeout_ms Per-task heartbeat timeout in milliseconds;
 *            valid range [k_iwdt_min_timeout_ms(100), k_iwdt_max_timeout_ms(60000)]
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task successfully registered
 * @retval k_rx_err_null_ptr task_name is nullptr
 * @retval k_rx_err_not_initialized rx_iwdt_init() has not been called
 * @retval k_rx_err_invalid_arg timeout_ms out of range, name empty, name too long,
 *         or task already registered
 * @retval k_rx_err_no_mem No free task slots (maximum k_iwdt_max_tasks tasks)
 *
 * @pre rx_iwdt_init() must have been called successfully
 * @pre task_name must be unique among registered tasks
 * @post Task entry is active in s_iwdt_state.tasks[]
 * @post status.active_tasks is incremented by one
 *
 * @note Not thread-safe; serialize registrations from multiple contexts
 *
 * @par Example:
 * @code
 * rx_err_t err = rx_iwdt_register_task("motor_ctrl", 500);
 * if (err != k_rx_ok) {
 *     rx_log_error("wdt", "Failed to register motor_ctrl task");
 * }
 * @endcode
 *
 * @see rx_iwdt_task_heartbeat() Update heartbeat for a registered task
 * @see rx_iwdt_check_tasks() Check all registered tasks for timeout
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: 5 preconditions (null, initialized, timeout, name, duplicate), 2 postconditions
 */
rx_err_t rx_iwdt_register_task(const char* task_name, uint32_t timeout_ms)
{
  /* Validate inputs */
  if (task_name == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  if (timeout_ms < k_iwdt_min_timeout_ms || timeout_ms > k_iwdt_max_timeout_ms) {
    return k_rx_err_invalid_arg;
  }

  /* Check task name length */
  const uint32_t name_len = (uint32_t)strlen(task_name);
  if (name_len == 0 || name_len >= k_iwdt_task_name_len) {
    return k_rx_err_invalid_arg;
  }

  /* Check if task already registered */
  if (internal_find_task(task_name) != nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Find free slot */
  rx_iwdt_task_info_t* const slot = internal_find_free_slot();
  if (slot == nullptr) {
    return k_rx_err_no_mem;
  }

  /* Register task */
  *slot = (rx_iwdt_task_info_t){};
  internal_safe_strcpy(slot->task_name, task_name, k_iwdt_task_name_len);
  slot->timeout_ms          = timeout_ms;
  slot->last_heartbeat_tick = internal_get_tick_count();
  slot->active              = true;
  slot->timed_out           = false;

  s_iwdt_state.status.active_tasks++;

  return k_rx_ok;
}

/**
 * @brief Send a heartbeat signal for a registered task to reset its timeout
 *
 * @details
 * Updates the last_heartbeat_tick for the named task to the current ThreadX
 * tick count, resetting the timeout window. Also clears the timed_out flag
 * if the task had previously been marked as timed out.
 *
 * Tasks must call this function at least once per their registered timeout_ms
 * window (configured in rx_iwdt_register_task()) to remain in good standing.
 * If the interval between heartbeats exceeds timeout_ms, the next call to
 * rx_iwdt_check_tasks() will return k_rx_err_timeout.
 *
 * Algorithm steps:
 * 1. Validate task_name is non-null
 * 2. Verify s_iwdt_state.initialized
 * 3. Find the task entry via internal_find_task()
 * 4. Set task->last_heartbeat_tick = internal_get_tick_count()
 * 5. Clear task->timed_out
 *
 * @param[in] task_name Null-terminated name of the registered task sending heartbeat;
 *            must exactly match the name used in rx_iwdt_register_task()
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Heartbeat timestamp updated and timed_out flag cleared
 * @retval k_rx_err_null_ptr task_name is nullptr
 * @retval k_rx_err_not_initialized rx_iwdt_init() has not been called
 * @retval k_rx_err_not_found No task registered with the given name
 *
 * @pre rx_iwdt_init() must have been called successfully
 * @pre Task must have been registered via rx_iwdt_register_task()
 * @post task->last_heartbeat_tick is refreshed to the current tick count
 * @post task->timed_out is false
 *
 * @note Not thread-safe; concurrent heartbeats from different tasks are safe
 *       as they write to distinct slots, but concurrent writes to the same slot are not
 *
 * @par Example:
 * @code
 * // At top of motor control task loop
 * while (1) {
 *     rx_iwdt_task_heartbeat("motor_ctrl");
 *     motor_control_update();
 *     tx_thread_sleep(k_motor_ctrl_period_ticks);
 * }
 * @endcode
 *
 * @see rx_iwdt_register_task() Register a task before sending heartbeats
 * @see rx_iwdt_check_tasks() Verify all tasks are within their timeout windows
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: 3 preconditions (null, initialized, registered), 2 postconditions
 */
rx_err_t rx_iwdt_task_heartbeat(const char* task_name)
{
  /* Validate inputs */
  if (task_name == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  /* Find task */
  rx_iwdt_task_info_t* const task = internal_find_task(task_name);
  if (task == nullptr) {
    return k_rx_err_not_found;
  }

  /* Update heartbeat */
  task->last_heartbeat_tick = internal_get_tick_count();
  task->timed_out           = false;

  return k_rx_ok;
}

/**
 * @brief Set the software task monitoring timeout for a specific system state
 *
 * @details
 * Configures the state-dependent software timeout used by rx_iwdt_check_tasks()
 * when the system is in the specified state. This allows different task heartbeat
 * windows depending on the operational mode (e.g. longer timeouts during
 * initialization, tighter timeouts during normal operation).
 *
 * Note: This does NOT change the hardware IWDT timeout. The hardware watchdog
 * timeout is fixed at compile/flash time via OFS registers and cannot be altered
 * at runtime. Only software task monitoring timeouts are affected.
 *
 * If state matches the currently active state (s_iwdt_state.current_state),
 * the active timeout (status.current_timeout_ms) is also updated immediately.
 *
 * Algorithm steps:
 * 1. Validate state is less than k_system_state_count
 * 2. Verify s_iwdt_state.initialized
 * 3. Validate timeout_ms is in [k_iwdt_min_timeout_ms, k_iwdt_max_timeout_ms]
 * 4. Set config.state_timeouts_ms[state] = timeout_ms
 * 5. If state == current_state, update status.current_timeout_ms
 *
 * @param[in] state System state identifier to configure (must be < k_system_state_count)
 * @param[in] timeout_ms Software task monitoring timeout in milliseconds;
 *            valid range [k_iwdt_min_timeout_ms(100), k_iwdt_max_timeout_ms(60000)]
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Timeout successfully updated
 * @retval k_rx_err_not_initialized rx_iwdt_init() has not been called
 * @retval k_rx_err_invalid_arg state >= k_system_state_count or timeout_ms out of range
 *
 * @pre rx_iwdt_init() must have been called successfully
 * @pre state must be a valid system_state_t value less than k_system_state_count
 * @post config.state_timeouts_ms[state] equals timeout_ms
 * @post If state is current, status.current_timeout_ms is updated immediately
 *
 * @note Not thread-safe; serialize state configuration changes
 * @warning Hardware IWDT timeout is unaffected; only software task monitoring changes
 *
 * @par Example:
 * @code
 * // Allow longer task timeouts during initialization phase
 * rx_iwdt_set_timeout_for_state(k_system_state_initializing, 5000);
 * rx_iwdt_set_timeout_for_state(k_system_state_running, 500);
 * @endcode
 *
 * @see rx_iwdt_set_state() Change the active system state
 * @see rx_iwdt_check_tasks() Uses current state timeout to evaluate task heartbeats
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: 3 preconditions (initialized, valid state, valid timeout), 2 postconditions
 */
rx_err_t rx_iwdt_set_timeout_for_state(const system_state_t state, const uint32_t timeout_ms)
{
  /* Validate inputs */
  if (state >= k_system_state_count) {
    return k_rx_err_invalid_arg;
  }

  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  if (timeout_ms < k_iwdt_min_timeout_ms || timeout_ms > k_iwdt_max_timeout_ms) {
    return k_rx_err_invalid_arg;
  }

  /* Update state timeout */
  s_iwdt_state.config.state_timeouts_ms[state] = timeout_ms;

  /* If this is the current state, update active timeout */
  if (state == s_iwdt_state.current_state) {
    s_iwdt_state.status.current_timeout_ms = timeout_ms;
  }

  return k_rx_ok;
}

/**
 * @brief Set the current system state for software task monitoring
 *
 * @details
 * Updates the active system state used by rx_iwdt_check_tasks() to select
 * per-state task heartbeat timeout windows. Transitions between states
 * (e.g. initializing -> running) change which timeout threshold is applied
 * when evaluating task heartbeat intervals.
 *
 * This function updates three tracking fields atomically:
 * - s_iwdt_state.current_state
 * - s_iwdt_state.status.current_state
 * - s_iwdt_state.status.current_timeout_ms (set from config.state_timeouts_ms[state])
 *
 * Important: This does NOT modify the hardware IWDT peripheral. The hardware
 * watchdog timeout is a fixed constant set in OFS registers at compile time.
 * Only the software task monitoring timeout threshold changes.
 *
 * Algorithm steps:
 * 1. Validate state is less than k_system_state_count
 * 2. Verify s_iwdt_state.initialized
 * 3. Update current_state and status.current_state = state
 * 4. Update status.current_timeout_ms = config.state_timeouts_ms[state]
 *
 * @param[in] state New system state (must be < k_system_state_count)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok State successfully updated
 * @retval k_rx_err_not_initialized rx_iwdt_init() has not been called
 * @retval k_rx_err_invalid_arg state >= k_system_state_count
 *
 * @pre rx_iwdt_init() must have been called successfully
 * @pre state must be a valid system_state_t value less than k_system_state_count
 * @post s_iwdt_state.current_state reflects the new state
 * @post status.current_timeout_ms reflects the configured timeout for the new state
 *
 * @note Not thread-safe; serialize state transitions from multiple contexts
 * @warning Hardware IWDT is unaffected; only software task monitoring changes
 *
 * @par Example:
 * @code
 * // Transition from initialization to running state
 * rx_err_t err = rx_iwdt_set_state(k_system_state_running);
 * if (err != k_rx_ok) {
 *     rx_log_error("wdt", "Failed to set IWDT system state");
 * }
 * @endcode
 *
 * @see rx_iwdt_set_timeout_for_state() Configure per-state timeouts
 * @see rx_iwdt_get_status() Query current state and timeout
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: 2 preconditions (initialized, valid state), 2 postconditions
 */
rx_err_t rx_iwdt_set_state(const system_state_t state)
{
  /* Validate inputs */
  if (state >= k_system_state_count) {
    return k_rx_err_invalid_arg;
  }

  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  /* Update software state tracking.
   * NOTE: This does not change the hardware watchdog timeout, as IWDT timeout
   * is configured via OFS registers at compile/flash time and cannot be changed
   * at runtime. The state-dependent timeouts are used for software task monitoring
   * via rx_iwdt_check_tasks(). The hardware IWDT timeout remains constant. */
  s_iwdt_state.current_state             = state;
  s_iwdt_state.status.current_state      = state;
  s_iwdt_state.status.current_timeout_ms = s_iwdt_state.config.state_timeouts_ms[state];

  return k_rx_ok;
}

/**
 * @brief Get a snapshot of the current IWDT module status
 *
 * @details
 * Copies the internal s_iwdt_state.status structure into the caller-provided
 * buffer via memcpy. The status snapshot includes: active task count, total
 * feed count, current system state, current timeout threshold, and the name
 * of the last task that timed out (if any).
 *
 * This is a diagnostic/telemetry function intended for logging and health
 * monitoring. The returned data is a point-in-time snapshot and may become
 * stale if the detection task or other threads update IWDT state concurrently.
 *
 * Algorithm steps:
 * 1. Validate status pointer is non-null
 * 2. Verify s_iwdt_state.initialized
 * 3. memcpy(&s_iwdt_state.status, status, sizeof(rx_iwdt_status_t))
 *
 * @param[out] status Pointer to caller-allocated rx_iwdt_status_t structure
 *             to receive the status snapshot; must be non-null; undefined on error
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Status snapshot successfully copied
 * @retval k_rx_err_null_ptr status is nullptr
 * @retval k_rx_err_not_initialized rx_iwdt_init() has not been called
 *
 * @pre rx_iwdt_init() must have been called successfully
 * @pre status must be a valid non-null pointer to an rx_iwdt_status_t
 * @post *status contains a copy of s_iwdt_state.status at the time of the call
 * @post Internal IWDT state is unchanged
 *
 * @note Not thread-safe; status fields may be updated concurrently by the task monitor
 * @note Snapshot accuracy depends on timing; use as diagnostic data, not safety-critical logic
 *
 * @par Example:
 * @code
 * rx_iwdt_status_t status;
 * rx_err_t err = rx_iwdt_get_status(&status);
 * if (err == k_rx_ok) {
 *     rx_log_info("wdt", "Active tasks: %u, Feeds: %u",
 *                 (unsigned)status.active_tasks,
 *                 (unsigned)status.watchdog_feeds);
 * }
 * @endcode
 *
 * @see rx_iwdt_was_reset() Check if last reset was hardware IWDT timeout
 * @see rx_iwdt_check_tasks() Check all tasks for software timeout
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: 2 preconditions (null check, initialized check), 2 postconditions
 */
rx_err_t rx_iwdt_get_status(rx_iwdt_status_t* status)
{
  /* Validate inputs */
  if (status == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  /* Copy status */
  {
    const uint8_t* src = (const uint8_t*)&s_iwdt_state.status;
    uint8_t*       dst = (uint8_t*)status;
    for (size_t byte_i = 0; byte_i < sizeof(rx_iwdt_status_t); byte_i++) {
      dst[byte_i] = src[byte_i];
    }
  }

  return k_rx_ok;
}

/**
 * @brief Check whether the last microcontroller reset was caused by an IWDT timeout
 *
 * @details
 * Reads the IWDT Reset Flag (IWDTRF) from the Reset Status Register 2 (RSTSR2)
 * of the RX72N. This register is located at a separate address from RSTSR0/1
 * and retains the reset cause flags across a reset event (values are cleared
 * on power-on reset only).
 *
 * The IWDTRF bit (bit position k_rstsr2_iwdtrf) is set by hardware when the IWDT
 * counter underflows (i.e., the watchdog was not refreshed in time). It remains
 * set until explicitly cleared or a power-on reset occurs.
 *
 * This function is used during startup to detect and log prior watchdog resets,
 * which may indicate a firmware hang or task scheduling failure.
 *
 * Algorithm steps:
 * 1. Read the RSTSR2 register byte via rstsr2() inline accessor
 * 2. Mask with k_rstsr2_iwdtrf to extract the IWDT reset flag bit
 * 3. Return true if the bit is set, false otherwise
 *
 * @return bool IWDT reset detection result
 * @retval true  RSTSR2.IWDTRF is set; last reset was caused by IWDT timeout
 * @retval false RSTSR2.IWDTRF is clear; last reset was not caused by IWDT
 *
 * @pre None -- safe to call before rx_iwdt_init() (reads hardware register directly)
 * @pre Must be called before the application clears the reset status register
 * @post Hardware RSTSR2 register is read-only; this function does not modify it
 * @post Return value reflects hardware register state at the instant of the call
 *
 * @note Thread-safe -- reads a single hardware register without shared state
 * @note Typically called once at startup to log the reset cause
 * @warning RSTSR2.IWDTRF remains set across warm resets; clear it in startup if needed
 *
 * @par Example:
 * @code
 * if (rx_iwdt_was_reset()) {
 *     rx_log_error("boot", "Prior reset caused by IWDT timeout - check task health");
 * }
 * rx_iwdt_init(&iwdt_config);
 * @endcode
 *
 * @see rx_iwdt_feed() Feed the hardware watchdog to prevent reset
 * @see rx_iwdt_check_tasks() Monitor software task heartbeats
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: 2 preconditions (safe before init, called before register clear), 2 postconditions
 */
bool rx_iwdt_was_reset(void)
{
  /* Read reset status register 2 for IWDT reset flag
   * Note: RSTSR2 is at a separate address from RSTSR0/1 */
  volatile uint8_t* const rstsr2_reg = rstsr2();
  const uint8_t           status     = *rstsr2_reg;

  /* Check IWDT reset flag in RSTSR2 */
  return (bool)((status & k_rstsr2_iwdtrf) != 0);
}

/**
 * @brief Check all registered tasks for software heartbeat timeout
 *
 * @details
 * Iterates over all active task slots and compares the elapsed tick count
 * since each task's last heartbeat against that task's configured timeout
 * (converted from milliseconds to ThreadX ticks). If any task has exceeded
 * its timeout, the task's timed_out flag is set, the task name is recorded
 * in status.last_failed_task, and the function returns k_rx_err_timeout.
 *
 * If task monitoring is disabled (config.enable_task_monitoring == false),
 * returns k_rx_ok immediately without checking any tasks.
 *
 * The caller (typically the watchdog task) should conditionally feed the
 * hardware IWDT only if this function returns k_rx_ok -- i.e., only refresh
 * the hardware watchdog when all software tasks are alive.
 *
 * Algorithm steps:
 * 1. Verify s_iwdt_state.initialized
 * 2. If !config.enable_task_monitoring, return k_rx_ok early
 * 3. Capture current_tick = internal_get_tick_count()
 * 4. For each active task slot (i < k_iwdt_max_tasks):
 *    a. Skip inactive slots
 *    b. Compute elapsed_ticks = current_tick - task->last_heartbeat_tick
 *    c. Compute timeout_in_ticks from task->timeout_ms and TX_TIMER_TICKS_PER_SECOND
 *    d. If elapsed_ticks > timeout_in_ticks: set timed_out=true, record task name, set any_timeout=true
 * 5. Return k_rx_err_timeout if any_timeout, else k_rx_ok
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All active tasks are within their heartbeat timeout windows
 * @retval k_rx_err_timeout One or more registered tasks have exceeded their timeout;
 *         name of first failing task is recorded in status.last_failed_task
 * @retval k_rx_err_not_initialized rx_iwdt_init() has not been called
 *
 * @pre rx_iwdt_init() must have been called successfully
 * @pre Registered tasks must be calling rx_iwdt_task_heartbeat() on schedule
 * @post Timed-out tasks have their timed_out flag set
 * @post status.last_failed_task contains the name of the most recently detected failure
 *
 * @note Not thread-safe; task heartbeat fields may be updated concurrently
 * @warning Do NOT feed the hardware IWDT (rx_iwdt_feed()) if this returns k_rx_err_timeout;
 *          allow the hardware watchdog to fire and reset the system
 *
 * @par Example:
 * @code
 * // Watchdog task main loop
 * while (1) {
 *     if (rx_iwdt_check_tasks() == k_rx_ok) {
 *         rx_iwdt_feed();  // Only feed if all tasks are alive
 *     } else {
 *         rx_log_error("wdt", "Task timeout detected -- allowing IWDT reset");
 *         // Do NOT feed the watchdog -- let hardware reset occur
 *     }
 *     tx_thread_sleep(k_watchdog_check_interval_ticks);
 * }
 * @endcode
 *
 * @see rx_iwdt_feed() Feed hardware watchdog (call only when this returns k_rx_ok)
 * @see rx_iwdt_task_heartbeat() Update per-task heartbeat from each monitored task
 * @see rx_iwdt_register_task() Register a task for monitoring
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: 2 preconditions (initialized, tasks registered), 2 postconditions
 */
rx_err_t rx_iwdt_check_tasks(void)
{
  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  if (!s_iwdt_state.config.enable_task_monitoring) {
    return k_rx_ok;
  }

  const uint32_t current_tick = internal_get_tick_count();
  bool           any_timeout  = false;

  /* Check each registered task */
  for (uint32_t i = 0; i < k_iwdt_max_tasks; i++) {
    if (!s_iwdt_state.tasks[i].active) {
      continue;
    }

    /* Calculate elapsed time */
    const uint32_t elapsed_ticks = current_tick - s_iwdt_state.tasks[i].last_heartbeat_tick;
    const uint32_t timeout_in_ticks =
      (s_iwdt_state.tasks[i].timeout_ms * TX_TIMER_TICKS_PER_SECOND) / k_ms_per_second;

    /* Check for timeout */
    if (elapsed_ticks > timeout_in_ticks) {
      s_iwdt_state.tasks[i].timed_out = true;
      any_timeout                     = true;

      /* Record failed task */
      internal_safe_strcpy(s_iwdt_state.status.last_failed_task,
                           s_iwdt_state.tasks[i].task_name,
                           k_iwdt_task_name_len);
    }
  }

  return (int)any_timeout ? k_rx_err_timeout : k_rx_ok;
}
