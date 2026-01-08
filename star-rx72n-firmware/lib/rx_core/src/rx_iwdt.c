/* lib/rx_core/src/rx_iwdt.c */

/**
 * @file rx_iwdt.c
 * @brief Independent Watchdog Timer (IWDT) Implementation
 * @details
 * Implementation of enhanced IWDT driver with task monitoring and
 * system state-dependent timeouts for RX72N.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
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

/** @brief IWDT driver state */
typedef struct {
  rx_iwdt_config_t    config;                  /**< Configuration */
  rx_iwdt_task_info_t tasks[k_iwdt_max_tasks]; /**< Registered tasks */
  rx_iwdt_status_t    status;                  /**< Status info */
  system_state_t      current_state;           /**< Current system state */
  bool                initialized;             /**< Initialization flag */
} rx_iwdt_state_t;

/** @brief Global IWDT state (static allocation) */
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
 * @brief Get current system tick count
 *
 * @return uint32_t Current tick count from ThreadX
 */
static uint32_t internal_get_tick_count(void)
{
  return (uint32_t)tx_time_get();
}

/**
 * @brief Find task by name
 *
 * @param[in] task_name Task name to find
 * @return rx_iwdt_task_info_t* Pointer to task info, or NULL if not found
 */
static rx_iwdt_task_info_t* internal_find_task(const char* task_name)
{
  for (uint32_t i = 0; i < k_iwdt_max_tasks; i++) {
    if (s_iwdt_state.tasks[i].active && strcmp(s_iwdt_state.tasks[i].task_name, task_name) == 0) {
      return &s_iwdt_state.tasks[i];
    }
  }

  return NULL;
}

/**
 * @brief Find free task slot
 *
 * @return rx_iwdt_task_info_t* Pointer to free slot, or NULL if none available
 */
static rx_iwdt_task_info_t* internal_find_free_slot(void)
{
  for (uint32_t i = 0; i < k_iwdt_max_tasks; i++) {
    if (!s_iwdt_state.tasks[i].active) {
      return &s_iwdt_state.tasks[i];
    }
  }

  return NULL;
}

/**
 * @brief Initialize default configuration
 *
 * @param[out] config Configuration structure to initialize
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

rx_err_t rx_iwdt_init(const rx_iwdt_config_t* config)
{
  volatile rx_iwdt_regs_t* regs;
  rx_iwdt_config_t         local_config;

  /* Check if already initialized */
  if (s_iwdt_state.initialized) {
    return k_rx_err_invalid_state;
  }

  /* Use default config if none provided */
  if (config == NULL) {
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

rx_err_t rx_iwdt_register_task(const char* task_name, uint32_t timeout_ms)
{
  rx_iwdt_task_info_t* slot;
  uint32_t             name_len;

  /* Validate inputs */
  if (task_name == NULL) {
    return k_rx_err_null_pointer;
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
  if (internal_find_task(task_name) != NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Find free slot */
  slot = internal_find_free_slot();
  if (slot == NULL) {
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

rx_err_t rx_iwdt_task_heartbeat(const char* task_name)
{
  rx_iwdt_task_info_t* task;

  /* Validate inputs */
  if (task_name == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  /* Find task */
  task = internal_find_task(task_name);
  if (task == NULL) {
    return k_rx_err_not_found;
  }

  /* Update heartbeat */
  task->last_heartbeat_tick = internal_get_tick_count();
  task->timed_out           = false;

  return k_rx_ok;
}

rx_err_t rx_iwdt_set_timeout_for_state(system_state_t state, uint32_t timeout_ms)
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

rx_err_t rx_iwdt_set_state(system_state_t state)
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

rx_err_t rx_iwdt_get_status(rx_iwdt_status_t* status)
{
  /* Validate inputs */
  if (status == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  /* Copy status */
  memcpy(status, &s_iwdt_state.status, sizeof(rx_iwdt_status_t));

  return k_rx_ok;
}

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

rx_err_t rx_iwdt_check_tasks(void)
{
  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  if (!s_iwdt_state.config.enable_task_monitoring) {
    return k_rx_ok;
  }

  uint32_t current_tick = internal_get_tick_count();
  bool     any_timeout  = false;

  /* Check each registered task */
  for (uint32_t i = 0; i < k_iwdt_max_tasks; i++) {
    if (!s_iwdt_state.tasks[i].active) {
      continue;
    }

    /* Calculate elapsed time */
    uint32_t elapsed_ticks    = current_tick - s_iwdt_state.tasks[i].last_heartbeat_tick;
    uint32_t timeout_in_ticks = (s_iwdt_state.tasks[i].timeout_ms * TX_TIMER_TICKS_PER_SECOND) / k_ms_per_second;

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
