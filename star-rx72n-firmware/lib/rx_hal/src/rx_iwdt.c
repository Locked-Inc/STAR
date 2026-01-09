/* lib/rx_hal/src/rx_iwdt.c */

/**
 * @file rx_iwdt.c
 * @brief Independent Watchdog Timer (IWDT) Driver Implementation for RX72N
 *
 * Implements the IWDT driver for system recovery from software hangs.
 * The IWDT uses a dedicated 120 kHz oscillator independent of the main clock.
 *
 * Hardware Details:
 * - IWDT clock: 120 kHz (IWDT-dedicated oscillator)
 * - Timeout calculation: cycles / (clock / divisor)
 * - Register base: 0x00088030
 *
 * Configuration Used:
 * - Window mode: Disabled (refresh allowed anytime)
 * - Reset action: Full chip reset (not NMI)
 * - Count in sleep: Enabled (continues counting during WAIT/STOP)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_iwdt.h"

#ifdef __RX__
#include "rx72n_regs.h"
#endif

/* =============================================================================
 * Internal Constants
 * =============================================================================
 */

/** @brief IWDT clock and size constants */
typedef enum {
  k_iwdt_clock_hz = 120000, /**< IWDT clock frequency in Hz */
} iwdt_clock_constants_t;

/** @brief Task monitoring string buffer sizes */
typedef enum {
  k_task_name_max_len = 16,     /**< Maximum task name length (including null) */
  k_task_name_cmp_len = 15,     /**< Length for strncmp (excluding null) */
  k_log_msg_buffer_size = 64,   /**< Log message buffer size */
} iwdt_buffer_size_constants_t;

/** @brief Timeout configuration lookup table entry */
typedef struct {
  uint32_t timeout_ms; /**< Timeout in milliseconds */
  uint16_t tops;       /**< TOPS bits (timeout period select) */
  uint16_t cks;        /**< CKS bits (clock divisor) */
} iwdt_timeout_entry_t;

/**
 * @brief Timeout configuration lookup table
 *
 * Maps requested timeouts to register values.
 * Entries sorted by increasing timeout for efficient lookup.
 *
 * Timeout = (cycles * divisor) / 120000
 *
 * Examples with TOPS=16384, varying CKS:
 * - CKS=1:   16384 * 1 / 120000 = 136.5ms  (~128ms)
 * - CKS=16:  16384 * 16 / 120000 = 2184ms  (~2048ms)
 * - CKS=128: 16384 * 128 / 120000 = 17476ms (~16384ms)
 */
static const iwdt_timeout_entry_t s_timeout_table[] = {
  /* timeout_ms, TOPS (cycles),        CKS (divisor)            */
  /* Actual timeout = (cycles * divisor) / 120kHz */
  {128, k_iwdt_tops_16384, k_iwdt_cks_div_1},     /* ~136ms actual  */
  {512, k_iwdt_tops_4096, k_iwdt_cks_div_16},     /* ~546ms actual  */
  {1000, k_iwdt_tops_8192, k_iwdt_cks_div_16},    /* ~1.09s actual  */
  {2048, k_iwdt_tops_16384, k_iwdt_cks_div_16},   /* ~2.18s actual  */
  {8192, k_iwdt_tops_16384, k_iwdt_cks_div_64},   /* ~8.74s actual  */
  {16384, k_iwdt_tops_16384, k_iwdt_cks_div_128}, /* ~17.48s actual */
};

/** @brief Number of entries in timeout table */
static const uint32_t k_iwdt_timeout_table_size =
  sizeof(s_timeout_table) / sizeof(s_timeout_table[0]);

/* =============================================================================
 * Module State
 * =============================================================================
 */

/** @brief Log tag for IWDT module */
static char* s_tag = "iwdt";

/** @brief Flag indicating IWDT has been initialized */
static uint8_t s_iwdt_initialized = 0;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Find best timeout configuration for requested timeout
 *
 * @param[in]  timeout_ms Requested timeout in milliseconds
 * @param[out] entry      Pointer to receive configuration entry
 *
 * @return k_rx_ok if valid configuration found
 * @return k_rx_err_invalid_arg if timeout out of range
 */
static rx_err_t internal_find_timeout_config(uint32_t                     timeout_ms,
                                             const iwdt_timeout_entry_t** entry)
{
  if (entry == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Find first entry with timeout >= requested */
  for (uint32_t i = 0; i < k_iwdt_timeout_table_size; i++) {
    if (s_timeout_table[i].timeout_ms >= timeout_ms) {
      *entry = &s_timeout_table[i];
      return k_rx_ok;
    }
  }

  /* Use maximum timeout if requested is too large */
  if (timeout_ms > s_timeout_table[k_iwdt_timeout_table_size - 1].timeout_ms) {
    *entry = &s_timeout_table[k_iwdt_timeout_table_size - 1];
    return k_rx_ok;
  }

  return k_rx_err_invalid_arg;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_iwdt_init(uint32_t timeout_ms)
{
#ifdef __RX__
  if (s_iwdt_initialized) {
    return k_rx_err_invalid_state;
  }

  if (timeout_ms == 0) {
    return k_rx_err_invalid_arg;
  }

  /* Find best timeout configuration */
  const iwdt_timeout_entry_t* config = NULL;
  rx_err_t                    err    = internal_find_timeout_config(timeout_ms, &config);
  if (err != k_rx_ok) {
    return err;
  }

  /*
   * Configure IWDT Control Register (IWDTCR)
   *
   * Bits [1:0]   TOPS  - Timeout Period Select
   * Bits [7:4]   CKS   - Clock Division Ratio Select
   * Bits [9:8]   RPES  - Window End Position (0x03 = 0%, window disabled)
   * Bits [13:12] RPSS  - Window Start Position (0x00 = 100%, full window)
   */
  uint16_t iwdtcr = 0;
  iwdtcr |= config->tops;    /* Timeout period */
  iwdtcr |= config->cks;     /* Clock divisor */
  iwdtcr |= k_iwdt_rpes_0;   /* Window end at 0% (disabled) */
  iwdtcr |= k_iwdt_rpss_100; /* Window start at 100% (full) */

  iwdt()->iwdtcr = iwdtcr;

  /*
   * Configure Reset Control Register (IWDTRCR)
   *
   * Bit 7 RSTIRQS - Reset Interrupt Request Select
   *   0 = Non-maskable interrupt request (NMI)
   *   1 = Reset
   *
   * We use reset (1) for safety - NMI could be masked or ignored.
   */
  iwdt()->iwdtrcr = k_iwdt_rstirqs_reset;

  /*
   * Configure Count Stop Control Register (IWDTCSTPR)
   *
   * Bit 7 SLCSTP - Sleep Mode Count Stop Control
   *   0 = Count continues during sleep
   *   1 = Count stops during sleep
   *
   * We continue counting during sleep for safety.
   */
  iwdt()->iwdtcstpr = k_iwdt_slcstp_continue;

  /*
   * Start the IWDT by performing first refresh
   *
   * The IWDT starts counting after the first refresh sequence.
   * After this, the watchdog CANNOT be stopped.
   */
  rx_iwdt_feed();

  s_iwdt_initialized = 1;
  return k_rx_ok;

#else
  /* Host-side stub for unit testing */
  (void)timeout_ms;
  s_iwdt_initialized = 1;
  return k_rx_ok;
#endif
}

void rx_iwdt_feed(void)
{
#ifdef __RX__
  /*
   * IWDT Refresh Sequence (Section 25.3.1)
   *
   * Write 0x00 followed by 0xFF to IWDTRR.
   * This sequence resets the down-counter to its initial value.
   *
   * CRITICAL: This sequence must not be interrupted.
   * An incomplete sequence will trigger a refresh error.
   *
   * Uses inline assembly to save/restore PSW (Program Status Word) because
   * there is no C library function to manipulate interrupt state on RX architecture.
   */

  /* Disable interrupts during refresh for atomicity */
  uint32_t psw;
  __asm__ volatile("mvfc psw, %0" : "=r"(psw));
  __asm__ volatile("clrpsw i");

  /* Perform refresh sequence */
  iwdt()->iwdtrr = k_iwdt_refresh_start; /* Write 0x00 */
  iwdt()->iwdtrr = k_iwdt_refresh_end;   /* Write 0xFF */

  /* Restore interrupt state */
  __asm__ volatile("mvtc %0, psw" : : "r"(psw));

#else
  /* Host-side stub - no operation */
#endif
}

bool rx_iwdt_was_reset(void)
{
#ifdef __RX__
  /*
   * Check IWDT Status Register for underflow or refresh error
   *
   * Bit 14 UNDFF - Underflow Flag
   *   1 = Counter underflow occurred
   *
   * Bit 15 REFEF - Refresh Error Flag
   *   1 = Refresh error occurred
   */
  uint16_t status = iwdt()->iwdtsr;
  return ((status & k_iwdt_sr_undff) != 0) || ((status & k_iwdt_sr_refef) != 0);

#else
  return false;
#endif
}

rx_iwdt_reset_cause_t rx_iwdt_get_reset_cause(void)
{
#ifdef __RX__
  uint16_t status = iwdt()->iwdtsr;

  if (status & k_iwdt_sr_refef) {
    return k_iwdt_reset_refresh_error;
  }

  if (status & k_iwdt_sr_undff) {
    return k_iwdt_reset_underflow;
  }

  return k_iwdt_reset_none;

#else
  return k_iwdt_reset_none;
#endif
}

void rx_iwdt_clear_status(void)
{
#ifdef __RX__
  /*
   * Clear status flags by writing 0 to the flag bits
   *
   * Bits 14-15 are write-0-to-clear
   */
  iwdt()->iwdtsr &= ~(k_iwdt_sr_undff | k_iwdt_sr_refef);

#else
  /* Host-side stub - no operation */
#endif
}

/* =============================================================================
 * Task-Level Monitoring (Issue 20: Enhanced Watchdog)
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>

#include "rx_log.h"
#include "tx_api.h"

/**
 * @brief Task monitoring state for a single task
 */
typedef struct {
  char     task_name[k_task_name_max_len]; /**< Task name (truncated if needed) */
  uint32_t timeout_ms;                     /**< Heartbeat timeout in milliseconds */
  uint32_t last_heartbeat_ms;              /**< Last heartbeat timestamp */
  uint8_t  registered;                     /**< 1 if task is registered */
} task_monitor_t;

/**
 * @brief Task monitoring module state
 */
typedef struct {
  task_monitor_t tasks[k_iwdt_max_tasks];       /**< Array of monitored tasks */
  uint8_t        task_count;                    /**< Number of registered tasks */
  char           failed_task[k_task_name_max_len]; /**< Name of last failed task */
} task_monitor_state_t;

/**
 * @brief Module state (static allocation, no dynamic memory)
 */
static task_monitor_state_t s_task_monitor = {0};

/**
 * @brief Find task index by name
 *
 * @param[in] task_name Task name to find
 * @return Task index (0-7), or -1 if not found
 */
static int32_t internal_find_task(const char* task_name)
{
  if (task_name == NULL) {
    return -1;
  }

  for (uint32_t i = 0; i < s_task_monitor.task_count; i++) {
    if (strncmp(s_task_monitor.tasks[i].task_name, task_name, k_task_name_cmp_len) == 0) {
      return (int32_t)i;
    }
  }

  return -1;
}

rx_err_t rx_iwdt_register_task(const char* task_name, uint32_t timeout_ms)
{
  /* Validate parameters */
  if (task_name == NULL) {
    return k_rx_err_invalid_arg;
  }

  if (s_task_monitor.task_count >= k_iwdt_max_tasks) {
    return k_rx_err_no_mem;
  }

  /* Check for duplicate registration */
  if (internal_find_task(task_name) >= 0) {
    return k_rx_err_exists;
  }

  /* Register new task */
  task_monitor_t* task = &s_task_monitor.tasks[s_task_monitor.task_count];

  strncpy(task->task_name, task_name, k_task_name_cmp_len);
  task->task_name[k_task_name_cmp_len] = '\0'; /* Ensure null termination */

  task->timeout_ms        = timeout_ms;
  task->last_heartbeat_ms = tx_time_get();
  task->registered        = 1;

  s_task_monitor.task_count++;

  rx_log_info(s_tag, "Registered task for monitoring");

  return k_rx_ok;
}

void rx_iwdt_task_heartbeat(const char* task_name)
{
  const int32_t idx = internal_find_task(task_name);

  if (idx < 0) {
    /* Task not registered - log warning but don't fail */
    rx_log_error(s_tag, "Heartbeat from unregistered task");
    return;
  }

  /* Update heartbeat timestamp */
  s_task_monitor.tasks[idx].last_heartbeat_ms = tx_time_get();
}

rx_err_t rx_iwdt_check_tasks(void)
{
  const uint32_t current_time_ms = tx_time_get();
  rx_err_t       result          = k_rx_ok;

  for (uint32_t i = 0; i < s_task_monitor.task_count; i++) {
    const task_monitor_t* task       = &s_task_monitor.tasks[i];
    const uint32_t        elapsed_ms = current_time_ms - task->last_heartbeat_ms;

    if (elapsed_ms > task->timeout_ms) {
      /* Task has exceeded heartbeat timeout - deadlock detected */
      char           log_msg[k_log_msg_buffer_size];
      const uint32_t written = (uint32_t)snprintf(log_msg,
                                                   sizeof(log_msg),
                                                   "Task deadlock: %s (timeout %lu ms)",
                                                   task->task_name,
                                                   (unsigned long)task->timeout_ms);

      if (written > 0 && written < sizeof(log_msg)) {
        rx_log_error(s_tag, log_msg);
      }

      /* Record failed task name for post-mortem */
      strncpy(s_task_monitor.failed_task, task->task_name, k_task_name_cmp_len);
      s_task_monitor.failed_task[k_task_name_cmp_len] = '\0';

      result = k_rx_err_timeout;
      /* Continue checking other tasks to log all failures */
    }
  }

  return result;
}

const char* rx_iwdt_get_failed_task(void)
{
  if (s_task_monitor.failed_task[0] != '\0') {
    return s_task_monitor.failed_task;
  }

  return NULL;
}
