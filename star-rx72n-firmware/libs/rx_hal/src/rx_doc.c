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
 * @param[in]  mode      Operating mode to encode
 * @param[out] docr_out  Receives the encoded DOCR value on success (untouched on error)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok              Mode valid; @p docr_out written
 * @retval k_rx_err_invalid_arg @p mode is not a valid rx_doc_mode_t value
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
 * @return true if DOPCF is set (condition occurred), false otherwise
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

rx_err_t rx_doc_set_reference(uint16_t reference)
{
  RX_VALIDATE_INIT(s_doc_init_state == k_doc_initialized,
                   s_tag,
                   "rx_doc_set_reference: driver not initialized");

  doc()->dodir = reference;
  return k_rx_ok;
}

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
