/* src/diagnostics.c */

/**
 * @file diagnostics.c
 * @brief Diagnostics module implementation
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "diagnostics.h"

#include <stdio.h>
#include <string.h>

#include "rx_log.h"
#include "tx_api.h"

/* =============================================================================
 * Private Constants
 * =============================================================================
 */

/** @brief Log buffer size for formatted fault messages */
typedef enum {
  k_log_buffer_size = 128, /**< Buffer size for formatted log messages */
} diagnostics_buffer_constants_t;

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static diagnostic_counters_t s_counters;
static TX_MUTEX              s_diagnostics_mutex;

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

void diagnostics_init(void)
{
  /* Zero-initialize counters */
  memset(&s_counters, 0, sizeof(diagnostic_counters_t));

  /* Create mutex for thread-safe counter access */
  UINT status = tx_mutex_create(&s_diagnostics_mutex, "diagnostics", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    /* Fatal error - halt system */
    while (1) {
      /* Empty infinite loop - watchdog will reset system */
    }
  }
}

const diagnostic_counters_t* diagnostics_get_counters(void)
{
  /* Note: Returning pointer to static data protected by mutex.
     * Caller should copy the data if long-term storage is needed.
     * This is safe because counters are only written by increment functions
     * which use mutex protection. */
  return &s_counters;
}

void diagnostics_increment_watchdog_reset(void)
{
  tx_mutex_get(&s_diagnostics_mutex, TX_WAIT_FOREVER);
  s_counters.watchdog_resets++;
  tx_mutex_put(&s_diagnostics_mutex);
}

void diagnostics_increment_motor_fault(void)
{
  tx_mutex_get(&s_diagnostics_mutex, TX_WAIT_FOREVER);
  s_counters.motor_faults++;
  tx_mutex_put(&s_diagnostics_mutex);
}

void diagnostics_increment_comm_timeout(void)
{
  tx_mutex_get(&s_diagnostics_mutex, TX_WAIT_FOREVER);
  s_counters.comm_timeouts++;
  tx_mutex_put(&s_diagnostics_mutex);
}

void diagnostics_increment_obstacle_event(void)
{
  tx_mutex_get(&s_diagnostics_mutex, TX_WAIT_FOREVER);
  s_counters.obstacle_events++;
  tx_mutex_put(&s_diagnostics_mutex);
}

void diagnostics_increment_thermal_warning(void)
{
  tx_mutex_get(&s_diagnostics_mutex, TX_WAIT_FOREVER);
  s_counters.thermal_warnings++;
  tx_mutex_put(&s_diagnostics_mutex);
}

void diagnostics_increment_register_correction(void)
{
  tx_mutex_get(&s_diagnostics_mutex, TX_WAIT_FOREVER);
  s_counters.register_corrections++;
  tx_mutex_put(&s_diagnostics_mutex);
}

void diagnostics_log_fault(const char* tag, const char* message, int32_t error_code)
{
  /* -------------------------------------------------------------------------
     * Issue 19: Fault Logging with Timestamp
     * -------------------------------------------------------------------------
     * Log fault events with timestamp for post-mortem analysis
     */
  const uint32_t timestamp_ms = tx_time_get();

  /* Log error with timestamp and error code */
  char           log_buffer[k_log_buffer_size];
  const uint32_t written = (uint32_t)snprintf(log_buffer,
                                              sizeof(log_buffer),
                                              "[%lu ms] FAULT: %s - code 0x%lx",
                                              (uint32_t)timestamp_ms,
                                              message,
                                              (uint32_t)error_code);

  /* Ensure buffer wasn't truncated */
  if (written > 0 && written < sizeof(log_buffer)) {
    rx_log_error(tag, log_buffer);
  } else {
    /* Fallback if buffer too small */
    rx_log_error(tag, message);
  }

  /* NOTE: Future enhancement could write to non-volatile storage for
     * persistence across reboots using internal data flash for post-mortem
     * analysis of failures in the field. Not implemented in Phase 1. */
}
