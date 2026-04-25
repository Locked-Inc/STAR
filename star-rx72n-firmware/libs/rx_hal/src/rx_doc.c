/**
 * @file rx_doc.c
 * @brief RX72N DOC (Data Operation Circuit) Driver Implementation
 *
 * @details
 * Driver for the Renesas RX72N Data Operation Circuit -- a small fixed-function
 * 16-bit compute accelerator supporting four operating modes:
 *
 * | rx_doc_mode_t              | OMS | DCSEL | Flag (DOPCF) Triggered On  |
 * |----------------------------|-----|-------|----------------------------|
 * | k_rx_doc_mode_compare      | 00  | 1     | DODIR == DODSR             |
 * | k_rx_doc_mode_compare_neq  | 00  | 0     | DODIR != DODSR             |
 * | k_rx_doc_mode_add          | 01  | x     | Carry past 0xFFFF          |
 * | k_rx_doc_mode_subtract     | 10  | x     | Borrow below 0x0000        |
 *
 * ## Initialization Sequence
 *
 * 1. Validate the requested mode against rx_doc_mode_t.
 * 2. Unlock PRCR (PRC1) and clear MSTPCRB.MSTPB6 to enable the DOC module clock.
 * 3. Re-lock PRCR.
 * 4. Write DOCR with the mode + DCSEL + DOPCFCL (clears any stale DOPCF).
 * 5. Record the mode and flag the driver as initialized.
 *
 * ## DOPCF Clearing
 *
 * DOPCF (DOCR bit 5) is read-only; the peripheral clears it only when 1 is
 * written to DOPCFCL (bit 6). Every operation writes DODSR to trigger the
 * computation, reads DOPCF, then writes DOPCFCL | (current mode bits) back to
 * DOCR to leave the mode untouched while clearing the flag.
 *
 * ## Mode Switching
 *
 * Re-calling rx_doc_init() with a different mode is the supported path to
 * switch; mode mismatches between the configured operation and the API call
 * return k_rx_err_invalid_state.
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0111),
 * Chapter 59 "Data Operation Circuit", pages 2969-2978.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto / setjmp / recursion
 * - Rule 2: No unbounded loops
 * - Rule 3: No dynamic allocation
 * - Rule 4: Every function <= 60 lines
 * - Rule 5: Minimum 2 validation checks per function
 * - Rule 6: File-scope state is static and private
 * - Rule 7: All returns checked or cast to void
 * - Rule 8: C23 typed enums for all constants
 * - Rule 9: Single-level pointers
 * - Rule 10: Compiles clean with -Wall -Wextra -Werror
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_doc.h"

#ifdef UNIT_TEST
#include "mock_rx72n_system_regs.h"
#include "mock_rx_doc.h"
#else
#include "rx72n_doc_regs.h"
#include "rx72n_system_regs.h"
#endif

#include "rx_check.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Internal Constants
 * =============================================================================
 */

/**
 * @enum doc_init_state_t
 * @brief DOC driver initialization state tracking
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_doc_not_initialized = 0, /**< Driver not initialized; API calls will fail */
  k_doc_initialized     = 1, /**< Driver initialized; operations permitted    */
} doc_init_state_t;

/**
 * @enum doc_bit_widths_t
 * @brief Bit-width constants for shifting the module-stop bit into MSTPCRB
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_doc_mstpb_bit_one = 1U, /**< Single-bit value used for the MSTPCRB shift */
} doc_bit_widths_t;

/* =============================================================================
 * Module State
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Log tag for this driver
 * @since Version 1.0.0
 */
static const char* const s_tag = "DOC";

/**
 * @var s_doc_init_state
 * @brief Tracks whether the DOC driver has been initialized
 * @since Version 1.0.0
 */
static doc_init_state_t s_doc_init_state = k_doc_not_initialized;

/**
 * @var s_doc_mode
 * @brief Current DOC operating mode (valid only when s_doc_init_state == initialized)
 * @since Version 1.0.0
 */
static rx_doc_mode_t s_doc_mode = k_rx_doc_mode_compare;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Translate an rx_doc_mode_t into the corresponding DOCR bit pattern
 *
 * @details
 * Validates @p mode and, on success, writes the DOCR bit pattern (OMS + DCSEL
 * + DOPCIE disabled) for the requested mode into @p docr_out. DOPCFCL is left
 * clear; the caller OR's it in when a flag clear is needed.
 *
 * Combining the validation and the encoding in a single step eliminates the
 * dead "unknown mode" branch that would otherwise exist in a pure encoder.
 *
 *
 *
 * @pre  docr_out != NULL
 * @post On success, @p *docr_out has DOPCFCL == 0
 *
 * @par Manual Reference
 * RX72N HW Manual Section 59.2 "DOCR" (page 2970): OMS[1:0], DCSEL, DOPCIE
 * layout.
 *
 * @note Pure function -- no side effects, safe to call anywhere.
 * @since Version 1.0.0
 */
static rx_err_t internal_docr_for_mode(rx_doc_mode_t mode, uint8_t* docr_out)
{
  switch (mode) {
    case k_rx_doc_mode_compare:
      *docr_out = (uint8_t)(k_doc_oms_compare | k_doc_dcsel_match | k_doc_dopcie_disabled);
      return k_rx_ok;
    case k_rx_doc_mode_compare_neq:
      *docr_out = (uint8_t)(k_doc_oms_compare | k_doc_dcsel_mismatch | k_doc_dopcie_disabled);
      return k_rx_ok;
    case k_rx_doc_mode_add:
      *docr_out = (uint8_t)(k_doc_oms_add | k_doc_dopcie_disabled);
      return k_rx_ok;
    case k_rx_doc_mode_subtract:
      *docr_out = (uint8_t)(k_doc_oms_subtract | k_doc_dopcie_disabled);
      return k_rx_ok;
    default:
      return k_rx_err_invalid_arg;
  }
}

/**
 * @brief Enable the DOC module clock by clearing MSTPCRB.MSTPB6 under PRCR.PRC1
 *
 * @details
 * Sequence:
 *   1. Unlock PRCR (PRC1) via k_rx_prcr_unlock_prc1.
 *   2. Clear bit k_doc_mstpb_bit (bit 6) in MSTPCRB.
 *   3. Re-lock PRCR via k_rx_prcr_lock.
 *
 * @pre prcr_reg() is writable
 * @post MSTPCRB.MSTPB6 == 0 (DOC module clock running)
 * @post PRCR is re-locked (k_rx_prcr_lock)
 *
 * @note system_regs() is an inline accessor for a fixed memory-mapped address
 *       (k_rx_system_regs_base), so it is guaranteed non-NULL and no runtime
 *       check is performed (would be dead code per NASA Power of 10).
 * @note Not thread-safe; call during single-threaded init.
 * @since Version 1.0.0
 */
static void internal_enable_doc_clock(void)
{
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcrb &= ~(k_doc_mstpb_bit_one << k_doc_mstpb_bit);
  *prcr_reg() = k_rx_prcr_lock;
}

/**
 * @brief Disable the DOC module clock by setting MSTPCRB.MSTPB6 under PRCR.PRC1
 *
 * @pre prcr_reg() is writable
 * @post MSTPCRB.MSTPB6 == 1 (DOC module clock stopped)
 * @post PRCR is re-locked
 *
 * @note See internal_enable_doc_clock() for the rationale on the absence of a
 *       system_regs() null check.
 * @since Version 1.0.0
 */
static void internal_disable_doc_clock(void)
{
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcrb |= (k_doc_mstpb_bit_one << k_doc_mstpb_bit);
  *prcr_reg() = k_rx_prcr_lock;
}

/**
 * @brief Clear the DOPCF flag while preserving the configured operating mode
 *
 * @details
 * Writes DOCR = (mode bits) | DOPCFCL. DOPCFCL is write-1-to-clear and leaves
 * the mode fields (OMS, DCSEL) unchanged. s_doc_mode is guaranteed to be a
 * valid enumerator whenever s_doc_init_state == k_doc_initialized, so the
 * internal_docr_for_mode() call cannot fail here.
 *
 * @pre Driver must be initialized (s_doc_init_state == k_doc_initialized)
 * @post DOPCF is cleared
 * @post OMS / DCSEL / DOPCIE unchanged from their configured values
 * @since Version 1.0.0
 */
static void internal_clear_dopcf(void)
{
  uint8_t mode_bits = 0U;
  (void)internal_docr_for_mode(s_doc_mode, &mode_bits);
  doc()->docr = (uint8_t)(mode_bits | k_doc_dopcfcl_clear);
}

/**
 * @brief Read and return the current DOPCF (operation completion flag)
 * @since Version 1.0.0
 */
static bool internal_read_dopcf(void)
{
  return (bool)((doc()->docr & k_doc_docr_dopcf_mask) != 0U);
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

rx_err_t rx_doc_init(rx_doc_mode_t mode)
{
  uint8_t        mode_bits = 0U;
  const rx_err_t err       = internal_docr_for_mode(mode, &mode_bits);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "rx_doc_init: invalid mode");
    return err;
  }

  internal_enable_doc_clock();
  doc()->docr = (uint8_t)(mode_bits | k_doc_dopcfcl_clear);

  s_doc_mode       = mode;
  s_doc_init_state = k_doc_initialized;
  return k_rx_ok;
}

/**
 * @brief Set the DOC compare reference (DODIR) register
 *
 * @details
 * Writes the supplied 16-bit reference value into the Data Operation
 * Circuit Input register (DODIR) that subsequent rx_doc_compare() /
 * rx_doc_add() / rx_doc_subtract() calls operate against.  Validates
 * driver init state but does not validate or constrain the value range
 * (any 16-bit value is meaningful in compare/add/subtract modes).
 *
 * @param[in] reference 16-bit value to load into DODIR for subsequent
 *                      compare/add/subtract operations.
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok                  Reference written.
 * @retval k_rx_err_not_initialized rx_doc_init() has not been called.
 *
 * @pre rx_doc_init() previously returned k_rx_ok.
 * @pre Caller has ensured no other thread is racing with this DOC channel
 *      (driver is single-instance global).
 *
 * @post DOC->DODIR == reference.
 * @post No other DOC register is mutated.
 *
 * @note Not thread-safe.  The DOC peripheral is global; serialize access
 *       at the application layer.
 *
 * @see rx_doc_compare()  Use the reference for compare operations.
 * @see rx_doc_add()      Use the reference for add operations.
 * @see rx_doc_subtract() Use the reference for subtract operations.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_doc_set_reference(uint16_t reference)
{
  RX_VALIDATE_INIT(s_doc_init_state == k_doc_initialized,
                   s_tag,
                   "rx_doc_set_reference: driver not initialized");

  doc()->dodir = reference;
  return k_rx_ok;
}

/**
 * @brief Compare value against the previously set DOC reference
 *
 * @details
 * Writes value into DODSR, latches the comparator flag (DOPCF), and
 * reports whether the configured compare condition is satisfied.  In
 * k_rx_doc_mode_compare the flag is set on equality; in
 * k_rx_doc_mode_compare_neq the flag is set on inequality.  The
 * comparator flag is cleared internally before return so successive
 * compares are independent.
 *
 * @param[in]  value   16-bit input written into DODSR to trigger the
 *                     comparison.
 * @param[out] matched On k_rx_ok holds the latched DOPCF result for the
 *                     configured compare mode (true if condition satisfied).
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok                  Compare succeeded; *matched populated.
 * @retval k_rx_err_null_ptr        matched is NULL.
 * @retval k_rx_err_not_initialized rx_doc_init() has not been called.
 * @retval k_rx_err_invalid_state   Driver not configured in a compare mode.
 *
 * @pre matched != NULL.
 * @pre rx_doc_init() was called with one of the compare modes.
 * @pre Caller previously set a reference via rx_doc_set_reference() if
 *      the comparison should be against anything other than the reset
 *      value.
 *
 * @post On k_rx_ok: *matched holds the result; DOC->DOPCF is cleared.
 * @post DOC->DODSR == value (samples register).
 *
 * @note Not thread-safe.  Serialize access to the global DOC peripheral.
 *
 * @see rx_doc_set_reference() Set the comparison reference.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_doc_compare(uint16_t value, bool* matched)
{
  RX_CHECK_NULL_PTR(matched, s_tag, "rx_doc_compare: matched is NULL");

  if (s_doc_init_state != k_doc_initialized) {
    rx_log_error(s_tag, "rx_doc_compare: driver not initialized");
    return k_rx_err_not_initialized;
  }
  if ((s_doc_mode != k_rx_doc_mode_compare) && (s_doc_mode != k_rx_doc_mode_compare_neq)) {
    rx_log_error(s_tag, "rx_doc_compare: driver not in a compare mode");
    return k_rx_err_invalid_state;
  }

  doc()->dodsr = value;
  *matched     = internal_read_dopcf();
  internal_clear_dopcf();
  return k_rx_ok;
}

/**
 * @brief Perform a hardware-accelerated 16-bit add with overflow detection
 *
 * @details
 * Writes the addend into DODSR; the DOC then computes
 * sum = DODIR + addend with overflow flagged in DOPCF.  Reads the result
 * back through DODSR (post-operation register) and reports the overflow
 * flag, then clears DOPCF before return.
 *
 * @param[in]  addend   16-bit value to add to the DODIR reference.
 * @param[out] sum      Destination for the 16-bit DODIR + addend result
 *                      (mod 2^16).
 * @param[out] overflow Destination for the carry-out flag (true on overflow
 *                      past 0xFFFF).
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok                  Add succeeded; *sum and *overflow
 *                                  populated.
 * @retval k_rx_err_null_ptr        sum or overflow is NULL.
 * @retval k_rx_err_not_initialized rx_doc_init() has not been called.
 * @retval k_rx_err_invalid_state   Driver not configured for add mode.
 *
 * @pre sum != NULL and overflow != NULL.
 * @pre rx_doc_init() was called with k_rx_doc_mode_add.
 * @pre rx_doc_set_reference() previously set the addition operand DODIR.
 *
 * @post On k_rx_ok: *sum holds DODIR + addend (mod 2^16); *overflow holds
 *       the carry-out flag; DOC->DOPCF is cleared.
 *
 * @note Not thread-safe.  Serialize access to the global DOC peripheral.
 *
 * @see rx_doc_subtract() Hardware subtract counterpart.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_doc_add(uint16_t addend, uint16_t* sum, bool* overflow)
{
  RX_CHECK_NULL_PTR(sum, s_tag, "rx_doc_add: sum is NULL");
  RX_CHECK_NULL_PTR(overflow, s_tag, "rx_doc_add: overflow is NULL");

  if (s_doc_init_state != k_doc_initialized) {
    rx_log_error(s_tag, "rx_doc_add: driver not initialized");
    return k_rx_err_not_initialized;
  }
  if (s_doc_mode != k_rx_doc_mode_add) {
    rx_log_error(s_tag, "rx_doc_add: driver not in add mode");
    return k_rx_err_invalid_state;
  }

  doc()->dodsr = addend;
  *sum         = doc()->dodsr;
  *overflow    = internal_read_dopcf();
  internal_clear_dopcf();
  return k_rx_ok;
}

/**
 * @brief Perform a hardware-accelerated 16-bit subtract with borrow
 *        detection
 *
 * @details
 * Writes subtrahend into DODSR; the DOC computes
 * difference = DODIR - subtrahend with borrow flagged in DOPCF.  Reads
 * the result back through DODSR and reports the borrow flag, then clears
 * DOPCF before return.
 *
 * @param[in]  subtrahend 16-bit value to subtract from the DODIR minuend.
 * @param[out] difference Destination for the 16-bit DODIR - subtrahend
 *                        result.
 * @param[out] borrow     Destination for the borrow flag (true if result
 *                        underflowed below 0).
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok                  Subtract succeeded; *difference and
 *                                  *borrow populated.
 * @retval k_rx_err_null_ptr        difference or borrow is NULL.
 * @retval k_rx_err_not_initialized rx_doc_init() has not been called.
 * @retval k_rx_err_invalid_state   Driver not configured for subtract mode.
 *
 * @pre difference != NULL and borrow != NULL.
 * @pre rx_doc_init() was called with k_rx_doc_mode_subtract.
 * @pre rx_doc_set_reference() previously set DODIR (the minuend).
 *
 * @post On k_rx_ok: *difference holds DODIR - subtrahend; *borrow holds
 *       the borrow flag; DOC->DOPCF is cleared.
 *
 * @note Not thread-safe.  Serialize access to the global DOC peripheral.
 *
 * @see rx_doc_add() Hardware add counterpart.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_doc_subtract(uint16_t subtrahend, uint16_t* difference, bool* borrow)
{
  RX_CHECK_NULL_PTR(difference, s_tag, "rx_doc_subtract: difference is NULL");
  RX_CHECK_NULL_PTR(borrow, s_tag, "rx_doc_subtract: borrow is NULL");

  if (s_doc_init_state != k_doc_initialized) {
    rx_log_error(s_tag, "rx_doc_subtract: driver not initialized");
    return k_rx_err_not_initialized;
  }
  if (s_doc_mode != k_rx_doc_mode_subtract) {
    rx_log_error(s_tag, "rx_doc_subtract: driver not in subtract mode");
    return k_rx_err_invalid_state;
  }

  doc()->dodsr = subtrahend;
  *difference  = doc()->dodsr;
  *borrow      = internal_read_dopcf();
  internal_clear_dopcf();
  return k_rx_ok;
}

/**
 * @brief Deinitialize the DOC peripheral and gate its clock
 *
 * @details
 * Idempotent: returns k_rx_ok immediately if the driver was not
 * initialized.  Otherwise clears DOC->DOCR (mode register), disables the
 * DOC module clock through internal_disable_doc_clock(), and marks the
 * driver as uninitialized.
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok Always returned (idempotent: ok if uninitialized too).
 *
 * @pre None (function is safe to call without prior init).
 *
 * @post DOC->DOCR == 0 and DOC module clock is gated.
 * @post Subsequent rx_doc_* operations (other than rx_doc_init) will
 *       return k_rx_err_not_initialized.
 *
 * @note Not thread-safe.  Serialize access to the global DOC peripheral.
 *
 * @see rx_doc_init() Required to re-arm after deinit.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_doc_deinit(void)
{
  if (s_doc_init_state != k_doc_initialized) {
    return k_rx_ok;
  }

  doc()->docr = 0U;
  internal_disable_doc_clock();

  s_doc_init_state = k_doc_not_initialized;
  return k_rx_ok;
}

/* =============================================================================
 * Test Support
 * =============================================================================
 */

#ifdef UNIT_TEST
/**
 * @brief Reset the DOC driver module state for unit testing
 *
 * @details
 * Available only in UNIT_TEST builds. Clears the initialized flag and resets
 * the tracked mode so individual tests can start from a known state without
 * relying on static initialization order.
 *
 * @post s_doc_init_state == k_doc_not_initialized
 * @post s_doc_mode == k_rx_doc_mode_compare
 * @since Version 1.0.0
 */
void rx_doc_test_reset(void)
{
  s_doc_init_state = k_doc_not_initialized;
  s_doc_mode       = k_rx_doc_mode_compare;
}
#endif
