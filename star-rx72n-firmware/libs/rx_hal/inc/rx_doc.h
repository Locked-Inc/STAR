/**
 * @file rx_doc.h
 * @brief RX72N DOC (Data Operation Circuit) HAL Public API
 *
 * @details
 * Higher-level driver API for the RX72N Data Operation Circuit. The DOC is a
 * fixed-function 16-bit compute accelerator supporting four operations:
 *
 * - **Compare equal** (k_rx_doc_mode_compare):     flag set when DODIR == DODSR
 * - **Compare not-equal** (k_rx_doc_mode_compare_neq): flag set when DODIR != DODSR
 * - **Add** (k_rx_doc_mode_add):                   DODSR += DODIR, flag on carry past 0xFFFF
 * - **Subtract** (k_rx_doc_mode_subtract):         DODSR -= DODIR, flag on borrow below 0x0000
 *
 * ## STAR Use Case
 *
 * Low-cost integrity check on small safety-critical values (PID gains, shared
 * telemetry words, firmware identifiers). Significantly cheaper than a CRC
 * when the payload is only 16 bits.
 *
 * ## Usage Pattern
 *
 * @code{.c}
 * // 1. Initialize in compare-equal mode
 * rx_err_t err = rx_doc_init(k_rx_doc_mode_compare);
 *
 * // 2. Load the expected reference value
 * err = rx_doc_set_reference(0xCAFE);
 *
 * // 3. Run the operation
 * bool matched = false;
 * err = rx_doc_compare(0xCAFE, &matched);   // matched = true
 *
 * // 4. Later, switch modes for a running-sum check
 * rx_doc_init(k_rx_doc_mode_add);
 * rx_doc_set_reference(0x0000);
 * uint16_t sum = 0;
 * bool overflow = false;
 * rx_doc_add(0x1234, &sum, &overflow);      // sum = 0x1234, overflow = false
 *
 * // 5. Power down
 * rx_doc_deinit();
 * @endcode
 *
 * ## Thread Safety
 *
 * The DOC is a single-instance peripheral. All functions in this API are
 * **not** thread-safe; callers must serialize access via an RTOS mutex or by
 * confining DOC use to a single task.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto / setjmp / recursion
 * - Rule 2: No unbounded loops
 * - Rule 3: No dynamic allocation
 * - Rule 4: All functions <= 60 lines
 * - Rule 5: Minimum 2 validation checks per function
 * - Rule 6: File-scope state is static, narrowest possible
 * - Rule 7: All return values checked or cast to void
 * - Rule 8: C23 typed enums for all constants
 * - Rule 9: Single-level pointers
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @see rx72n_doc_regs.h Register-layer definitions
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * DOC Operating Mode
 * =============================================================================
 */

/**
 * @enum rx_doc_mode_t
 * @brief Selects the DOC operating mode
 *
 * @details
 * Determines which DOC hardware mode (OMS + DCSEL) is configured by
 * rx_doc_init().
 *
 * | Mode value                        | OMS | DCSEL | Hardware behavior               |
 * |-----------------------------------|-----|-------|---------------------------------|
 * | k_rx_doc_mode_compare             | 00  | 1     | DOPCF set when DODIR == DODSR   |
 * | k_rx_doc_mode_compare_neq         | 00  | 0     | DOPCF set when DODIR != DODSR   |
 * | k_rx_doc_mode_add                 | 01  | x     | DODSR += DODIR; DOPCF on carry  |
 * | k_rx_doc_mode_subtract            | 10  | x     | DODSR -= DODIR; DOPCF on borrow |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rx_doc_mode_compare     = 0, /**< Compare equal (flag on match)     */
  k_rx_doc_mode_compare_neq = 1, /**< Compare not-equal (flag on mismatch) */
  k_rx_doc_mode_add         = 2, /**< 16-bit addition                   */
  k_rx_doc_mode_subtract    = 3, /**< 16-bit subtraction                */
} rx_doc_mode_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize DOC in the selected operating mode
 *
 * @details
 * Powers on the DOC (clears MSTPCRB.MSTPB6 under PRCR.PRC1 unlock), then
 * configures DOCR with the requested operation mode, clears any stale DOPCF,
 * and marks the driver initialized.
 *
 * Subsequent operations expect the driver to remain in this mode until
 * rx_doc_init() is called again with a different mode (re-init is allowed
 * and is the supported way to switch modes).
 *
 * @param[in] mode Operating mode to configure
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg mode is not a valid rx_doc_mode_t value
 *
 * @pre PRCR accessor (prcr_reg()) is addressable
 * @pre MSTPCRB is writable once PRCR.PRC1 is unlocked
 * @post DOC module stop bit cleared (MSTPCRB.MSTPB6 = 0)
 * @post DOCR configured for the requested mode, DOPCIE = 0, DOPCF cleared
 * @post Driver is marked initialized
 *
 * @note Not thread-safe. Caller must serialize DOC access.
 *
 * @see rx_doc_deinit()
 * @since Version 1.0.0
 */
rx_err_t rx_doc_init(rx_doc_mode_t mode);

/**
 * @brief Load the DOC reference value (DODIR) used for the next operation
 *
 * @param[in] reference 16-bit reference / operand value
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_not_initialized rx_doc_init() has not been called
 *
 * @pre Driver must be initialized via rx_doc_init()
 * @post DODIR == reference
 *
 * @note Writing DODIR does not trigger the operation -- only writing DODSR does.
 * @since Version 1.0.0
 */
rx_err_t rx_doc_set_reference(uint16_t reference);

/**
 * @brief Execute a compare operation against the loaded reference
 *
 * @details
 * Writes @p value to DODSR, which triggers the DOC to evaluate the configured
 * compare condition. Reads DOPCF to report whether the condition fired, then
 * clears DOPCF.
 *
 * Interpretation depends on the mode selected at init time:
 * - k_rx_doc_mode_compare:      *matched = true when DODIR == value
 * - k_rx_doc_mode_compare_neq:  *matched = true when DODIR != value
 *
 * @param[in]  value 16-bit value to compare against the DODIR reference
 * @param[out] matched Set to true if the compare condition fired, false otherwise
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr matched is nullptr
 * @retval k_rx_err_not_initialized rx_doc_init() has not been called
 * @retval k_rx_err_invalid_state Driver is initialized but not in a compare mode
 *
 * @pre Driver initialized in a compare mode (compare or compare_neq)
 * @pre DODIR loaded via rx_doc_set_reference()
 * @post *matched reflects the configured condition
 * @post DOPCF is cleared
 *
 * @since Version 1.0.0
 */
rx_err_t rx_doc_compare(uint16_t value, bool* matched);

/**
 * @brief Execute a 16-bit add using the DOC
 *
 * @details
 * Writes @p addend to DODSR, which triggers DODSR = DODSR_before + DODIR.
 * Reads back DODSR for the sum, reads DOPCF to detect carry (sum overflowed
 * past 0xFFFF), then clears DOPCF.
 *
 * To chain adds (running sum), call rx_doc_set_reference() once with the
 * initial accumulator and then call rx_doc_add() repeatedly; each call uses
 * DODIR as the first operand and @p addend as the second.
 *
 * @param[in]  addend 16-bit value to add to the reference
 * @param[out] sum 16-bit result (DODSR after the operation)
 * @param[out] overflow true if the add carried past 0xFFFF, false otherwise
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr sum or overflow is nullptr
 * @retval k_rx_err_not_initialized rx_doc_init() has not been called
 * @retval k_rx_err_invalid_state Driver is initialized but not in add mode
 *
 * @pre Driver initialized in k_rx_doc_mode_add
 * @post *sum == (DODIR + addend) mod 65536
 * @post *overflow reflects DOPCF after the operation
 * @post DOPCF is cleared
 *
 * @since Version 1.0.0
 */
rx_err_t rx_doc_add(uint16_t addend, uint16_t* sum, bool* overflow);

/**
 * @brief Execute a 16-bit subtract using the DOC
 *
 * @details
 * Writes @p subtrahend to DODSR, which triggers DODSR = DODSR_before - DODIR.
 * Reads back DODSR for the difference, reads DOPCF to detect borrow (difference
 * would have gone below 0x0000), then clears DOPCF.
 *
 * @param[in]  subtrahend 16-bit value to subtract from the reference
 * @param[out] difference 16-bit result (DODSR after the operation)
 * @param[out] borrow true if the subtract borrowed below 0x0000, false otherwise
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr difference or borrow is nullptr
 * @retval k_rx_err_not_initialized rx_doc_init() has not been called
 * @retval k_rx_err_invalid_state Driver is initialized but not in subtract mode
 *
 * @pre Driver initialized in k_rx_doc_mode_subtract
 * @post *difference == (DODIR - subtrahend) mod 65536
 * @post *borrow reflects DOPCF after the operation
 * @post DOPCF is cleared
 *
 * @since Version 1.0.0
 */
rx_err_t rx_doc_subtract(uint16_t subtrahend, uint16_t* difference, bool* borrow);

/**
 * @brief Deinitialize DOC and gate its clock
 *
 * @details
 * Clears DOCR (disables DOPCIE, deselects the mode), sets MSTPCRB.MSTPB6 under
 * PRCR.PRC1 unlock to stop the DOC module, and marks the driver uninitialized.
 *
 * Safe to call without a prior rx_doc_init() (returns k_rx_ok and does nothing).
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Always
 *
 * @post DOCR == 0
 * @post MSTPCRB.MSTPB6 == 1 (DOC module stopped)
 * @post Driver is marked uninitialized
 *
 * @since Version 1.0.0
 */
rx_err_t rx_doc_deinit(void);

#ifdef __cplusplus
}
#endif
