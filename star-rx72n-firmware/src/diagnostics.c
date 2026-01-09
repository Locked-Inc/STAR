/* src/diagnostics.c */

/**
 * @file diagnostics.c
 * @brief Diagnostics module implementation
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "diagnostics.h"
#include "rx_log.h"
#include "tx_api.h"
#include <string.h>
#include <stdio.h>

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static diagnostic_counters_t s_counters;

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

void diagnostics_init(void)
{
    memset(&s_counters, 0, sizeof(diagnostic_counters_t));
}

const diagnostic_counters_t* diagnostics_get_counters(void)
{
    return &s_counters;
}

void diagnostics_increment_watchdog_reset(void)
{
    s_counters.watchdog_resets++;
}

void diagnostics_increment_motor_fault(void)
{
    s_counters.motor_faults++;
}

void diagnostics_increment_comm_timeout(void)
{
    s_counters.comm_timeouts++;
}

void diagnostics_increment_obstacle_event(void)
{
    s_counters.obstacle_events++;
}

void diagnostics_increment_thermal_warning(void)
{
    s_counters.thermal_warnings++;
}

void diagnostics_increment_register_correction(void)
{
    s_counters.register_corrections++;
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
    char log_buffer[128];
    const int written = snprintf(log_buffer, sizeof(log_buffer),
                                 "[%lu ms] FAULT: %s - code 0x%lx",
                                 (unsigned long)timestamp_ms,
                                 message,
                                 (unsigned long)error_code);

    /* Ensure buffer wasn't truncated */
    if (written > 0 && (size_t)written < sizeof(log_buffer)) {
        rx_log_error(tag, log_buffer);
    } else {
        /* Fallback if buffer too small */
        rx_log_error(tag, message);
    }

    /* TODO (Phase 5+): Write to non-volatile storage for persistence
     * across reboots. This would use internal data flash for post-mortem
     * analysis of failures in the field. */
}
