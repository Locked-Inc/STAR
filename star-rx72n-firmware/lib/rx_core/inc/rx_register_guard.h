/* lib/rx_core/inc/rx_register_guard.h */

/**
 * @file rx_register_guard.h
 * @brief Register Guard - Periodic Refresh for ESD/EMI Protection
 * @details
 * Provides periodic refresh of critical hardware registers to recover from
 * ESD or EMI-induced bit flips that could corrupt system configuration.
 *
 * In electrically noisy environments (motors, high currents, long cables),
 * transient voltage spikes can flip bits in configuration registers. This
 * module maintains known-good values for critical registers and periodically
 * rewrites them to ensure correct operation.
 *
 * Protected Registers:
 * - GPIO Port Direction Registers (PDR) - Ensure pins stay in correct mode
 * - Interrupt Priority Registers (IPR) - Prevent priority inversions
 * - Module Stop Control (MSTPCRA/B/C/D) - Ensure peripherals stay enabled
 *
 * Usage:
 * @code
 * // Initialize at startup (after all peripherals configured)
 * rx_register_guard_init();
 *
 * // Call periodically from main control loop (~1 second interval)
 * static uint32_t refresh_counter = 0;
 * if (++refresh_counter >= 250) {  // Every 1s at 250Hz
 *     rx_register_guard_refresh();
 *     refresh_counter = 0;
 * }
 * @endcode
 *
 * @note This module captures register values during init, so call after
 *       all peripheral initialization is complete.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_REGISTER_GUARD_H
#define STAR_RX_REGISTER_GUARD_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/**
 * @brief Register guard configuration constants
 */
typedef enum {
  k_register_guard_max_ports = 10, /**< Maximum PORT registers to guard */
  k_register_guard_max_ipr   = 16, /**< Maximum IPR registers to guard */
} rx_register_guard_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize register guard module
 *
 * Captures current values of critical registers as the "golden" reference.
 * These values will be restored during periodic refresh.
 *
 * @return k_rx_ok on success
 *
 * @note Call AFTER all peripheral initialization is complete.
 * @note Can be called multiple times to update golden values.
 */
rx_err_t rx_register_guard_init(void);

/**
 * @brief Refresh all critical registers
 *
 * Compares current register values against golden reference and restores
 * any that have changed. Also counts corrections for diagnostics.
 *
 * Recommended call frequency: Every 100-1000ms
 *
 * @note Call from main control loop, NOT from interrupt handlers.
 * @note Brief interrupt disable during PRCR unlock sequence.
 */
void rx_register_guard_refresh(void);

/**
 * @brief Get count of register corrections since last reset
 *
 * Returns the number of times a register value was corrected during
 * refresh. Non-zero values indicate ESD/EMI events occurred.
 *
 * @return Correction count (0 = no corrections needed)
 *
 * @note Counter wraps at UINT32_MAX.
 */
uint32_t rx_register_guard_get_correction_count(void);

/**
 * @brief Reset the correction counter
 *
 * Clears the correction count. Useful for periodic logging.
 */
void rx_register_guard_reset_count(void);

/**
 * @brief Check if guard module is initialized
 *
 * @return true if initialized, false otherwise
 */
bool rx_register_guard_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_REGISTER_GUARD_H */
