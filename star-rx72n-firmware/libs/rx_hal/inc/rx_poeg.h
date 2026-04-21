/**
 * @file rx_poeg.h
 * @brief POEG Motor Fault Protection Driver API
 *
 * @details
 * Provides hardware-level emergency PWM output disable via the POEG module.
 * When a DRV8263H motor driver asserts its nFAULT output (active low), the
 * corresponding GTETRG pin triggers POEG to immediately disable GPTW PWM
 * outputs, placing the H-bridge in a safe Hi-Z state.
 *
 * @par System Architecture
 * @verbatim
 *  DRV8263H nFAULT (active low)
 *       |
 *       v
 *  GTETRG pin --> POEG (PIDF flag) --> GPTW output disable (Hi-Z)
 *       |                                      |
 *       v                                      v
 *  ICU interrupt --> ISR --> shared_data fault flags
 * @endverbatim
 *
 * @par Motor-to-POEG Group Mapping
 * | Motor | nFAULT Pin | GTETRG | POEG Group | Vector |
 * |-------|------------|--------|------------|--------|
 * | 0     | P15        | A      | A          | 188    |
 * | 1     | PA6        | B      | B          | 189    |
 * | 2     | PC4        | C      | C          | 190    |
 * | 3     | P14        | D      | D          | 191    |
 *
 * @par Two-Layer Protection
 * 1. **Hardware (automatic)**: POEG disables GPTW outputs within nanoseconds
 *    of nFAULT assertion. No firmware intervention required for the stop.
 * 2. **Firmware (ISR)**: POEG interrupt logs fault, sets shared_data flags,
 *    and prevents motor re-enable until fault is resolved.
 *
 * @see rx72n_poeg_regs.h POEG register definitions
 * @see rx72n_gptw_regs.h GPTW register definitions (GTINTAD)
 * @see shared_data.h Fault flag storage
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum rx_poeg_motor_count_t
 * @brief Number of motors with POEG fault protection
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_poeg_motor_count = 4, /**< 4 motors, one POEG group each (A-D) */
} rx_poeg_motor_count_t;

/**
 * @enum rx_poeg_isr_priority_t
 * @brief ICU priority for POEG fault interrupts
 *
 * @details
 * Priority 14 (near-maximum) ensures motor faults are handled immediately.
 * Only NMI (level 15) has higher priority.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_poeg_isr_priority = 14, /**< High priority for safety-critical fault ISR */
} rx_poeg_isr_priority_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize POEG fault protection for all 4 motor channels
 *
 * @details
 * Configures POEG Groups A-D for hardware motor fault protection:
 * 1. Enables external pin detection (PIDE) on each group
 * 2. Enables noise filter (NFEN) with PCLKB/8 sampling (~400ns)
 * 3. Inverts input (INV) since DRV8263H nFAULT is active-low
 * 4. Links each GPTW channel to its POEG group via GTINTAD
 * 5. Configures ICU interrupts for vectors 188-191 at priority 14
 *
 * After initialization, a DRV8263H fault will:
 * - Immediately disable GPTW PWM output (hardware, ~nanoseconds)
 * - Fire POEG ISR which sets fault flags in shared_data
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All 4 POEG groups initialized successfully
 * @retval k_rx_err_hw_init_failed POEG register write verification failed
 *
 * @pre GPTW module enabled (MSTPCRA.MSTPA7 = 0)
 * @pre GTETRG pins set as GPIO inputs (PDR=0); no MPC PSEL needed (dedicated POEG inputs)
 * @pre shared_data initialized
 *
 * @post POEG Groups A-D enabled with pin detection + noise filter
 * @post GPTW0-3 GTINTAD linked to POEG Groups A-D respectively
 * @post ICU vectors 188-191 enabled at priority 14
 * @post Any existing nFAULT assertion will trigger immediately
 *
 * @note Thread Safety: Call once during hardware_init, before motor tasks start
 * @warning PIDE enable bit is write-once after reset -- cannot be disabled
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto, setjmp, recursion
 * - Rule 2: Loop bounded by k_poeg_motor_count (4)
 * - Rule 3: No dynamic allocation
 * - Rule 5: 3 preconditions, 4 postconditions
 * - Rule 7: Return value checked
 * - Rule 8: All constants as C23 typed enums
 *
 * @see rx_poeg_clear_fault() Clear fault after resolution
 * @see rx_poeg_get_fault_status() Query fault state
 * @since Version 1.0.0
 */
rx_err_t rx_poeg_init(void);

/**
 * @brief Check if a motor has an active POEG fault
 *
 * @details
 * Reads the POEGGn register for the specified motor's POEG group and
 * returns whether the PIDF (port input detection flag) is set.
 *
 * @param[in] motor_index Motor index (0-3)
 * @param[out] fault_active true if POEG fault is active
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Status read successfully
 * @retval k_rx_err_null_ptr fault_active isnullptr
 * @retval k_rx_err_invalid_arg motor_index >= 4
 *
 * @pre POEG initialized via rx_poeg_init()
 * @post No side effects on hardware state
 *
 * @note Thread Safety: Safe to call from any context (read-only register access)
 *
 * @since Version 1.0.0
 */
rx_err_t rx_poeg_get_fault_status(uint8_t motor_index, bool* fault_active);

/**
 * @brief Clear POEG fault and re-enable motor output
 *
 * @details
 * Attempts to clear the PIDF flag for the specified motor's POEG group.
 * The flag can only be cleared when the underlying fault condition has
 * been resolved (i.e., DRV8263H nFAULT deasserted / GTETRG pin high).
 *
 * After clearing, GPTW output re-enables at the end of the next GPTW
 * counting cycle (delay of up to 1 PWM period, typically 50us at 20kHz).
 *
 * @param[in] motor_index Motor index (0-3)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Fault cleared successfully
 * @retval k_rx_err_invalid_arg motor_index >= 4
 * @retval k_rx_err_busy Fault condition still active (nFAULT still asserted)
 *
 * @pre Underlying fault resolved (DRV8263H nFAULT deasserted)
 * @post If successful, PIDF flag cleared and GPTW output will re-enable
 * @post shared_data fault_flags[motor_index] cleared
 *
 * @warning Returns k_rx_err_busy if fault source is still active
 * @note Motor control task should verify estop is cleared before resuming PWM
 *
 * @since Version 1.0.0
 */
rx_err_t rx_poeg_clear_fault(uint8_t motor_index);

/**
 * @brief Trigger software emergency stop on a specific motor
 *
 * @details
 * Sets the SSF (Software Stop Flag) in the POEGGn register, immediately
 * disabling GPTW PWM output for the specified motor. This provides a
 * firmware-controlled emergency stop independent of hardware faults.
 *
 * @param[in] motor_index Motor index (0-3)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Software stop activated
 * @retval k_rx_err_invalid_arg motor_index >= 4
 *
 * @post GPTW output immediately disabled for specified motor
 * @post SSF flag set in POEGGn register
 *
 * @see rx_poeg_clear_software_stop() Release software stop
 * @since Version 1.0.0
 */
rx_err_t rx_poeg_software_stop(uint8_t motor_index);

/**
 * @brief Release software emergency stop on a specific motor
 *
 * @details
 * Clears the SSF (Software Stop Flag) in the POEGGn register.
 * GPTW output re-enables at end of next counting cycle if no other
 * fault flags (PIDF, IOCF, OSTPF) are active.
 *
 * @param[in] motor_index Motor index (0-3)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Software stop released
 * @retval k_rx_err_invalid_arg motor_index >= 4
 *
 * @post SSF flag cleared in POEGGn register
 *
 * @see rx_poeg_software_stop() Activate software stop
 * @since Version 1.0.0
 */
rx_err_t rx_poeg_clear_software_stop(uint8_t motor_index);

#ifdef __cplusplus
}
#endif
