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

#include "rx_check.h"

/* =============================================================================
 * Hardware Register Definitions
 * =============================================================================
 */

/** @brief IWDT register addresses */
typedef enum {
  k_iwdt_base_addr = 0x00088030, /**< IWDT base address */
  k_rstsr1_addr    = 0x000C0010, /**< Reset Status Register 1 */
} iwdt_hw_addr_t;

/** @brief IWDT register structure */
typedef struct {
  volatile uint8_t  iwdtcr; /**< Control Register */
  volatile uint8_t  _pad0[1];
  volatile uint16_t iwdtsr;  /**< Status Register */
  volatile uint16_t iwdtrcr; /**< Refresh Register */
  volatile uint8_t  _pad1[2];
  volatile uint16_t iwdtcstpr; /**< Count Stop Control */
} rx_iwdt_regs_t;

/** @brief System register for reset status */
typedef struct {
  volatile uint8_t rstsr1; /**< Reset Status Register 1 */
} rx_system_rstsr_t;

/** @brief IWDT hardware constants */
typedef enum {
  k_iwdt_refresh_key  = 0x00, /**< Refresh key value */
  k_iwdt_start_key    = 0x00, /**< Start key value */
  k_iwdt_wdtrstf_mask = 0x80, /**< Watchdog reset flag */
  k_iwdt_enable_bit   = 0x80, /**< Enable bit in IWDTCR */
  k_iwdt_divider_mask = 0x03, /**< Clock divider mask */
  k_ticks_per_second  = 100,  /**< ThreadX ticks per second */
} iwdt_hw_constants_t;

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
 * Hardware Access Functions
 * =============================================================================
 */

/**
 * @brief Get IWDT register base
 * @return Pointer to IWDT registers
 */
static inline volatile rx_iwdt_regs_t* iwdt_regs(void)
{
  return (volatile rx_iwdt_regs_t*)k_iwdt_base_addr;
}

/**
 * @brief Get reset status register
 * @return Pointer to reset status register
 */
static inline volatile rx_system_rstsr_t* rstsr_regs(void)
{
  return (volatile rx_system_rstsr_t*)k_rstsr1_addr;
}

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Convert timeout in ms to hardware divider setting
 *
 * @param[in] timeout_ms Timeout in milliseconds
 * @return iwdt_timeout_period_t Hardware divider setting
 */
static iwdt_timeout_period_t internal_timeout_to_divider(uint32_t timeout_ms)
{
  /* Select smallest divider that gives timeout >= requested */
  if (timeout_ms <= 512) {
    return k_iwdt_timeout_512ms;
  } else if (timeout_ms <= 2048) {
    return k_iwdt_timeout_2048ms;
  } else {
    return k_iwdt_timeout_8192ms;
  }
}

/**
 * @brief Get current system tick count
 *
 * @return uint32_t Current tick count
 */
static uint32_t internal_get_tick_count(void)
{
  /* In real implementation, would use ThreadX tx_time_get() */
  /* For now, return stub value */
  return 0;
}

/**
 * @brief Find task by name
 *
 * @param[in] task_name Task name to find
 * @return rx_iwdt_task_info_t* Pointer to task info, or NULL if not found
 */
static rx_iwdt_task_info_t* internal_find_task(const char* task_name)
{
  uint32_t i;

  for (i = 0; i < k_iwdt_max_tasks; i++) {
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
  uint32_t i;

  for (i = 0; i < k_iwdt_max_tasks; i++) {
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
  uint32_t i;

  config->default_timeout_ms     = k_iwdt_default_timeout_ms;
  config->enable_task_monitoring = true;
  config->reset_on_timeout       = true;

  /* Initialize all state timeouts to default */
  for (i = 0; i < k_system_state_count; i++) {
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
  iwdt_timeout_period_t    divider;

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

  /* Configure hardware */
  regs    = iwdt_regs();
  divider = internal_timeout_to_divider(config->default_timeout_ms);

  /* Set clock divider and enable IWDT */
  regs->iwdtcr = (uint8_t)(k_iwdt_enable_bit | divider);

  /* Start IWDT */
  regs->iwdtrcr = k_iwdt_start_key;

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

  /* Refresh watchdog counter */
  regs          = iwdt_regs();
  regs->iwdtrcr = k_iwdt_refresh_key;

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

  /* Update state */
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
  volatile rx_system_rstsr_t* rstsr;
  uint8_t                     status;

  /* Read reset status register */
  rstsr  = rstsr_regs();
  status = rstsr->rstsr1;

  /* Check watchdog reset flag */
  return ((status & k_iwdt_wdtrstf_mask) != 0);
}

rx_err_t rx_iwdt_check_tasks(void)
{
  uint32_t current_tick;
  uint32_t i;
  uint32_t elapsed_ticks;
  uint32_t timeout_ticks;
  bool     any_timeout;

  if (!s_iwdt_state.initialized) {
    return k_rx_err_not_initialized;
  }

  if (!s_iwdt_state.config.enable_task_monitoring) {
    return k_rx_ok;
  }

  current_tick = internal_get_tick_count();
  any_timeout  = false;

  /* Check each registered task */
  for (i = 0; i < k_iwdt_max_tasks; i++) {
    if (!s_iwdt_state.tasks[i].active) {
      continue;
    }

    /* Calculate elapsed time */
    elapsed_ticks = current_tick - s_iwdt_state.tasks[i].last_heartbeat_tick;
    timeout_ticks = (s_iwdt_state.tasks[i].timeout_ms * k_ticks_per_second) / 1000;

    /* Check for timeout */
    if (elapsed_ticks > timeout_ticks) {
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
