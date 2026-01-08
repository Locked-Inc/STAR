/* star-rx72n-firmware/include/diagnostics.h */

/**
 * @file diagnostics.h
 * @brief System diagnostics and logging module
 * @details
 * Provides diagnostic counters and logging for safety-critical events.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>

/* =============================================================================
 * Diagnostic Counters
 * =============================================================================
 */

typedef struct {
    uint32_t watchdog_resets;    /**< Total watchdog resets */
    uint32_t motor_faults;       /**< Total motor fault events */
    uint32_t comm_timeouts;      /**< Total communication timeouts */
    uint32_t obstacle_events;    /**< Total obstacle detection events */
    uint32_t thermal_warnings;   /**< Total thermal warnings */
    uint32_t register_corrections; /**< Register guard corrections */
} diagnostic_counters_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize diagnostics module
 */
void diagnostics_init(void);

/**
 * @brief Get diagnostic counters
 * @return Pointer to diagnostic counters structure
 */
const diagnostic_counters_t* diagnostics_get_counters(void);

/**
 * @brief Increment watchdog reset counter
 */
void diagnostics_increment_watchdog_reset(void);

/**
 * @brief Increment motor fault counter
 */
void diagnostics_increment_motor_fault(void);

/**
 * @brief Increment communication timeout counter
 */
void diagnostics_increment_comm_timeout(void);

/**
 * @brief Increment obstacle event counter
 */
void diagnostics_increment_obstacle_event(void);

/**
 * @brief Increment thermal warning counter
 */
void diagnostics_increment_thermal_warning(void);

/**
 * @brief Increment register correction counter
 */
void diagnostics_increment_register_correction(void);

#endif /* DIAGNOSTICS_H */
