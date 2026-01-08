/* src/diagnostics.c */

/**
 * @file diagnostics.c
 * @brief Diagnostics module implementation
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "diagnostics.h"
#include <string.h>

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
