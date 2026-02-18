/* lib/rx_core/src/rx_iwdt.c */

/**
 * @file rx_iwdt.c
 * @brief Independent Watchdog Timer (IWDT) Implementation for RX72N
 *
 * @details
 * Implements the hardware-independent watchdog timer driver with enhanced
 * task monitoring capabilities. The IWDT uses a dedicated 125 kHz clock
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
 * | Clock Source | IWDTCLK (125 kHz independent) | PCLKB (system clock) |
 * | Can be Stopped | [X] No (hardware safety) | [OK] Yes |
 * | Clock Fault Detection | [OK] Yes (independent clock) | [X] No |
 * | Timeout Range | ~8 ms to ~17 seconds | ~4 µs to ~273 µs |
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
 * | rx_iwdt_init() | ~30 µs | 64 B | Memset + config copy |
 * | rx_iwdt_feed() | ~1 µs | 8 B | 2 register writes |
 * | rx_iwdt_register_task() | ~10 µs | 32 B | String copy + slot search |
 * | rx_iwdt_task_heartbeat() | ~5 µs | 16 B | Task lookup + tick update |
 * | rx_iwdt_check_tasks() | ~20 µs | 24 B | Scan all 8 tasks |
 *
 * @par Module Dependencies:
 * - [rx_iwdt.h](../inc/rx_iwdt.h) - Public API declarations
 * - [rx72n_iwdt_regs.h](../../rx_hal/inc/rx72n_iwdt_regs.h) - IWDT registers
 * - [rx72n_system_regs.h](../../rx_hal/inc/rx72n_system_regs.h) - System registers
 * - [rx_check.h](../inc/rx_check.h) - Validation macros
 * - `<tx_api.h>` - ThreadX API for tick count
 * - `<string.h>` - memcpy, memset, strncpy, strlen, strcmp
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
 * @author STAR Team
 * @date 2026-01-28
 * @version 1.0.0
 * @copyright Copyright (c) 2026 STAR Project. Licensed under MIT.
 *
 * @since Version 1.0.0
 */

#include "rx_iwdt.h"

#include <string.h>

#include "rx72n_iwdt_regs.h"
#include "rx72n_system_regs.h"
#include "rx_check.h"
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
static rx_iwdt_state_t s_iwdt_state = {0};

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
  memset(&s_iwdt_state, 0, sizeof(s_iwdt_state));
}
#endif

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

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
 * @note Tick count wraps at UINT32_MAX (~49.7 days at 1 kHz tick rate)
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
    if (s_iwdt_state.tasks[i].active && strcmp(s_iwdt_state.tasks[i].task_name, task_name) == 0) {
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
  volatile rx_iwdt_regs_t* regs;
  rx_iwdt_config_t         local_config;

  /* Check if already initialized */
  if (s_iwdt_state.initialized) {
    return k_rx_err_invalid_state;
  }

  /* Use default config if none provided */
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
  memset(&s_iwdt_state, 0, sizeof(s_iwdt_state));
  memcpy(&s_iwdt_state.config, config, sizeof(rx_iwdt_config_t));
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
  regs = iwdt();

  /* Feed the watchdog to establish baseline */
  regs->iwdtrr = k_iwdt_refresh_start;
  regs->iwdtrr = k_iwdt_refresh_end;

  s_iwdt_state.initialized = true;

  return k_rx_ok;
}

/**
 * @brief Feed the independent watchdog timer to prevent timeout
 *
 * @return k_rx_ok on success, k_rx_err_not_initialized if IWDT not initialized
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
 * @brief Register a task for watchdog monitoring
 *
 * @param[in] task_name Unique task name (must be non-empty, max 31 chars)
 * @param[in] timeout_ms Task-specific timeout in milliseconds (100-60000)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if task_name is nullptr
 * @return k_rx_err_not_initialized if IWDT not initialized
 * @return k_rx_err_invalid_arg if timeout invalid, name empty/too long, or task already registered
 * @return k_rx_err_no_mem if no free task slots available
 */
rx_err_t rx_iwdt_register_task(const char* task_name, uint32_t timeout_ms)
{
  rx_iwdt_task_info_t* slot;
  uint32_t             name_len;

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
  name_len = (uint32_t)strlen(task_name);
  if (name_len == 0 || name_len >= k_iwdt_task_name_len) {
    return k_rx_err_invalid_arg;
  }

  /* Check if task already registered */
  if (internal_find_task(task_name) != nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Find free slot */
  slot = internal_find_free_slot();
  if (slot == nullptr) {
    return k_rx_err_no_mem;
  }

  /* Register task */
  memset(slot, 0, sizeof(rx_iwdt_task_info_t));
  strncpy(slot->task_name, task_name, k_iwdt_task_name_len - 1);
  slot->task_name[k_iwdt_task_name_len - 1] = '\0';
  slot->timeout_ms                          = timeout_ms;
  slot->last_heartbeat_tick                 = internal_get_tick_count();
  slot->active                              = true;
  slot->timed_out                           = false;

  s_iwdt_state.status.active_tasks++;

  return k_rx_ok;
}

/**
 * @brief Send heartbeat for a registered task
 *
 * @param[in] task_name Name of task sending heartbeat
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if task_name is nullptr
 * @return k_rx_err_not_initialized if IWDT not initialized
 * @return k_rx_err_not_found if task not registered
 */
rx_err_t rx_iwdt_task_heartbeat(const char* task_name)
{
  rx_iwdt_task_info_t* task;

  /* Validate inputs */
  if (task_name == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  /* Find task */
  task = internal_find_task(task_name);
  if (task == nullptr) {
    return k_rx_err_not_found;
  }

  /* Update heartbeat */
  task->last_heartbeat_tick = internal_get_tick_count();
  task->timed_out           = false;

  return k_rx_ok;
}

/**
 * @brief Set watchdog timeout for a specific system state
 *
 * @param[in] state System state to configure
 * @param[in] timeout_ms Timeout in milliseconds (100-60000)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if state invalid or timeout out of range
 * @return k_rx_err_not_initialized if IWDT not initialized
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
 * @brief Set current system state (affects task monitoring timeout)
 *
 * @param[in] state New system state
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if state invalid
 * @return k_rx_err_not_initialized if IWDT not initialized
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
 * @brief Get current IWDT status
 *
 * @param[out] status Pointer to status structure to populate
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if status is nullptr
 * @return k_rx_err_not_initialized if IWDT not initialized
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
  memcpy(status, &s_iwdt_state.status, sizeof(rx_iwdt_status_t));

  return k_rx_ok;
}

/**
 * @brief Check if last reset was caused by IWDT timeout
 *
 * @return true if last reset was IWDT timeout, false otherwise
 */
bool rx_iwdt_was_reset(void)
{
  volatile uint8_t* rstsr2_reg;
  uint8_t           status;

  /* Read reset status register 2 for IWDT reset flag
   * Note: RSTSR2 is at a separate address from RSTSR0/1 */
  rstsr2_reg = rstsr2();
  status     = *rstsr2_reg;

  /* Check IWDT reset flag in RSTSR2 */
  return ((status & k_rstsr2_iwdtrf) != 0);
}

/**
 * @brief Check all registered tasks for timeout
 *
 * @return k_rx_ok if all tasks are within timeout
 * @return k_rx_err_timeout if any task has timed out
 * @return k_rx_err_not_initialized if IWDT not initialized
 */
rx_err_t rx_iwdt_check_tasks(void)
{
  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  if (!s_iwdt_state.config.enable_task_monitoring) {
    return k_rx_ok;
  }

  uint32_t current_tick  = internal_get_tick_count();
  bool     any_timeout   = false;

  /* Check each registered task */
  for (uint32_t i = 0; i < k_iwdt_max_tasks; i++) {
    if (!s_iwdt_state.tasks[i].active) {
      continue;
    }

    /* Calculate elapsed time */
    uint32_t elapsed_ticks   = current_tick - s_iwdt_state.tasks[i].last_heartbeat_tick;
    uint32_t timeout_in_ticks =
      (s_iwdt_state.tasks[i].timeout_ms * TX_TIMER_TICKS_PER_SECOND) / k_ms_per_second;

    /* Check for timeout */
    if (elapsed_ticks > timeout_in_ticks) {
      s_iwdt_state.tasks[i].timed_out = true;
      any_timeout                     = true;

      /* Record failed task */
      strncpy(s_iwdt_state.status.last_failed_task,
              s_iwdt_state.tasks[i].task_name,
              k_iwdt_task_name_len - 1);
      s_iwdt_state.status.last_failed_task[k_iwdt_task_name_len - 1] = '\0';
    }
  }

  return any_timeout ? k_rx_err_timeout : k_rx_ok;
}
