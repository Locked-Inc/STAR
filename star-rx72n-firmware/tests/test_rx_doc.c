/**
 * @file test_rx_doc.c
 * @brief Unity Test Suite for the RX72N DOC (Data Operation Circuit) Driver
 *
 * @details
 * Host-side unit tests exercising every public entry point of rx_doc.h and
 * every internal branch of rx_doc.c. Uses mock_rx_doc.c to back the DOC
 * register window, MSTPCRB, and PRCR in ordinary host memory so the driver's
 * register writes become observable struct-member writes.
 *
 * ## Test Coverage
 *
 * - Initialization
 *   - Valid modes (compare, compare_neq, add, subtract)
 *   - Invalid mode (> k_rx_doc_mode_subtract)
 *   - MSTPCRB.MSTPB6 cleared by rx_doc_init()
 *   - DOCR set to mode-appropriate bit pattern + DOPCFCL
 * - Reference loading
 *   - Sets DODIR on success
 *   - Rejects calls before init with k_rx_err_invalid_state
 * - Compare
 *   - Match (compare mode) sets *matched = true, clears DOPCF
 *   - Mismatch (compare mode) sets *matched = false
 *   - Match (compare_neq mode) sets *matched = false
 *   - Mismatch (compare_neq mode) sets *matched = true
 *   - Rejects null matched pointer
 *   - Rejects call before init
 *   - Rejects call when driver is in a non-compare mode
 * - Add
 *   - No overflow: *sum, *overflow correct; DOPCF cleared
 *   - Carry overflow: *overflow = true
 *   - Rejects null sum / null overflow
 *   - Rejects before init and in wrong mode
 * - Subtract
 *   - No borrow: *difference, *borrow correct; DOPCF cleared
 *   - Borrow: *borrow = true
 *   - Rejects null difference / null borrow
 *   - Rejects before init and in wrong mode
 * - Deinit
 *   - Safe to call without prior init (returns ok)
 *   - Clears DOCR
 *   - Sets MSTPCRB.MSTPB6 (module stopped)
 *   - Allows subsequent re-init
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mock_rx_doc.h"
#include "rx_doc.h"
#include "rx_err.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum doc_test_values_t
 * @brief Numeric test vectors used across the suite
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_doc_test_ref_cafe     = 0xCAFEU, /**< arbitrary 16-bit reference value        */
  k_doc_test_ref_1234     = 0x1234U, /**< small reference (well below overflow)   */
  k_doc_test_ref_ffff     = 0xFFFFU, /**< max 16-bit value                        */
  k_doc_test_value_cafe   = 0xCAFEU, /**< compare-equal probe value               */
  k_doc_test_value_beef   = 0xBEEFU, /**< compare-neq probe value                 */
  k_doc_test_addend_0001  = 0x0001U, /**< adds by 1 (forces carry when ref=FFFF)  */
  k_doc_test_addend_0042  = 0x0042U, /**< normal no-overflow addend               */
  k_doc_test_sub_0010     = 0x0010U, /**< small subtrahend                        */
  k_doc_test_sub_0100     = 0x0100U, /**< forces borrow when ref < subtrahend     */
  k_doc_test_mstpb6_mask  = (1U << 6),
  k_doc_test_mstpcrb_init = 0xFFFFFFFFU,
  k_doc_test_invalid_mode = 42,
} doc_test_values_t;

/* =============================================================================
 * Test Harness Hooks
 * =============================================================================
 */

/** @brief Driver-internal reset hook exposed only in UNIT_TEST builds. */
void rx_doc_test_reset(void);

void setUp(void)
{
  mock_rx_doc_reset();
  rx_doc_test_reset();
}

void tearDown(void) {}

/* =============================================================================
 * rx_doc_init()
 * =============================================================================
 */

static void test_init_compare_mode_clears_mstpb6(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare));
  TEST_ASSERT_EQUAL_UINT32(0U,
                           g_mock_doc_system_regs.mstpcrb & (uint32_t)k_doc_test_mstpb6_mask);
}

static void test_init_compare_mode_writes_docr(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare));
  const uint8_t expected =
    (uint8_t)(k_doc_oms_compare | k_doc_dcsel_match | k_doc_dopcie_disabled | k_doc_dopcfcl_clear);
  TEST_ASSERT_EQUAL_UINT8(expected, g_mock_doc_regs.docr);
}

static void test_init_compare_neq_mode_writes_docr(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare_neq));
  const uint8_t expected = (uint8_t)(k_doc_oms_compare | k_doc_dcsel_mismatch
                                     | k_doc_dopcie_disabled | k_doc_dopcfcl_clear);
  TEST_ASSERT_EQUAL_UINT8(expected, g_mock_doc_regs.docr);
}

static void test_init_add_mode_writes_docr(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_add));
  const uint8_t expected =
    (uint8_t)(k_doc_oms_add | k_doc_dopcie_disabled | k_doc_dopcfcl_clear);
  TEST_ASSERT_EQUAL_UINT8(expected, g_mock_doc_regs.docr);
}

static void test_init_subtract_mode_writes_docr(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_subtract));
  const uint8_t expected =
    (uint8_t)(k_doc_oms_subtract | k_doc_dopcie_disabled | k_doc_dopcfcl_clear);
  TEST_ASSERT_EQUAL_UINT8(expected, g_mock_doc_regs.docr);
}

static void test_init_rejects_invalid_mode(void)
{
  const rx_doc_mode_t invalid = (rx_doc_mode_t)k_doc_test_invalid_mode;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_doc_init(invalid));
}

static void test_reinit_switches_mode(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_add));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_subtract));
  const uint8_t expected =
    (uint8_t)(k_doc_oms_subtract | k_doc_dopcie_disabled | k_doc_dopcfcl_clear);
  TEST_ASSERT_EQUAL_UINT8(expected, g_mock_doc_regs.docr);
}

/* =============================================================================
 * rx_doc_set_reference()
 * =============================================================================
 */

static void test_set_reference_before_init_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
                    rx_doc_set_reference((uint16_t)k_doc_test_ref_cafe));
}

static void test_set_reference_writes_dodir(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_set_reference((uint16_t)k_doc_test_ref_cafe));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_doc_test_ref_cafe, g_mock_doc_regs.dodir);
}

/* =============================================================================
 * rx_doc_compare()
 * =============================================================================
 */

static void test_compare_null_output_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare));
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_doc_compare((uint16_t)k_doc_test_value_cafe, NULL));
}

static void test_compare_before_init_rejected(void)
{
  bool matched = false;
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized,
                    rx_doc_compare((uint16_t)k_doc_test_value_cafe, &matched));
}

static void test_compare_wrong_mode_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_add));
  bool matched = false;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
                    rx_doc_compare((uint16_t)k_doc_test_value_cafe, &matched));
}

static void test_compare_match_reports_true_and_clears_flag(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_set_reference((uint16_t)k_doc_test_ref_cafe));

  /* Arm the hardware emulator: when driver writes DODSR we want DOPCF set
     iff DODIR == written value. */
  mock_rx_doc_set_auto_flag(true);
  mock_rx_doc_set_dopcf(true); /* pre-arm so the read-path has something to see */

  bool matched = false;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_compare((uint16_t)k_doc_test_value_cafe, &matched));
  TEST_ASSERT_TRUE(matched);
  /* Driver must have written the DOPCFCL bit back, leaving DOPCF clear. */
  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_doc_regs.docr & (uint8_t)k_doc_docr_dopcf_mask);
}

static void test_compare_mismatch_reports_false(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_set_reference((uint16_t)k_doc_test_ref_cafe));

  /* Leave DOPCF clear so the read-back returns "not matched". */
  bool matched = true;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_compare((uint16_t)k_doc_test_value_beef, &matched));
  TEST_ASSERT_FALSE(matched);
}

static void test_compare_neq_match_reports_false(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare_neq));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_set_reference((uint16_t)k_doc_test_ref_cafe));

  /* Same values: DOPCF (flag-on-mismatch) should stay clear; report false. */
  bool matched = true;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_compare((uint16_t)k_doc_test_value_cafe, &matched));
  TEST_ASSERT_FALSE(matched);
}

static void test_compare_neq_mismatch_reports_true(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare_neq));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_set_reference((uint16_t)k_doc_test_ref_cafe));

  /* Arm DOPCF to simulate hardware flagging a mismatch. */
  mock_rx_doc_set_dopcf(true);

  bool matched = false;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_compare((uint16_t)k_doc_test_value_beef, &matched));
  TEST_ASSERT_TRUE(matched);
}

/* =============================================================================
 * rx_doc_add()
 * =============================================================================
 */

static void test_add_null_sum_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_add));
  bool overflow = false;
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr,
                    rx_doc_add((uint16_t)k_doc_test_addend_0042, NULL, &overflow));
}

static void test_add_null_overflow_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_add));
  uint16_t sum = 0;
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr,
                    rx_doc_add((uint16_t)k_doc_test_addend_0042, &sum, NULL));
}

static void test_add_before_init_rejected(void)
{
  uint16_t sum      = 0;
  bool     overflow = false;
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized,
                    rx_doc_add((uint16_t)k_doc_test_addend_0042, &sum, &overflow));
}

static void test_add_wrong_mode_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare));
  uint16_t sum      = 0;
  bool     overflow = false;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
                    rx_doc_add((uint16_t)k_doc_test_addend_0042, &sum, &overflow));
}

/* NOTE: On real hardware, writing DODSR triggers the DOC to atomically replace
 * DODSR with the arithmetic result before the driver can read it back. The
 * host mock has no way to hook a struct write, so we test the driver's
 * contract with the hardware interface instead of simulated arithmetic:
 *   - driver writes DODSR (mock stores the written value)
 *   - driver reads DODSR into *sum (so *sum == value written)
 *   - driver reads DOPCF (which the test pre-arms to model the overflow flag)
 *   - driver clears DOPCF via DOPCFCL (observable as the flag bit gone)
 */

static void test_add_without_carry(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_add));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_set_reference((uint16_t)k_doc_test_ref_1234));

  /* Leave DOPCF clear: simulates the "no carry past 0xFFFF" hardware outcome. */
  uint16_t sum      = 0;
  bool     overflow = true;
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_doc_add((uint16_t)k_doc_test_addend_0042, &sum, &overflow));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_doc_test_addend_0042, sum);
  TEST_ASSERT_FALSE(overflow);
  /* Driver must have written DOPCFCL to clear the flag (it was 0 anyway, but
     the post-state must still have DOPCF clear). */
  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_doc_regs.docr & (uint8_t)k_doc_docr_dopcf_mask);
  /* DODSR now holds the addend the driver wrote. */
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_doc_test_addend_0042, g_mock_doc_regs.dodsr);
}

static void test_add_with_carry(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_add));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_set_reference((uint16_t)k_doc_test_ref_ffff));

  /* Pre-arm DOPCF: simulates hardware flagging carry after the add. */
  mock_rx_doc_set_dopcf(true);

  uint16_t sum      = 0xDEADU;
  bool     overflow = false;
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_doc_add((uint16_t)k_doc_test_addend_0001, &sum, &overflow));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_doc_test_addend_0001, sum);
  TEST_ASSERT_TRUE(overflow);
  /* After the call, driver must have cleared DOPCF via DOPCFCL write. */
  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_doc_regs.docr & (uint8_t)k_doc_docr_dopcf_mask);
}

/* =============================================================================
 * rx_doc_subtract()
 * =============================================================================
 */

static void test_subtract_null_difference_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_subtract));
  bool borrow = false;
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr,
                    rx_doc_subtract((uint16_t)k_doc_test_sub_0010, NULL, &borrow));
}

static void test_subtract_null_borrow_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_subtract));
  uint16_t diff = 0;
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr,
                    rx_doc_subtract((uint16_t)k_doc_test_sub_0010, &diff, NULL));
}

static void test_subtract_before_init_rejected(void)
{
  uint16_t diff   = 0;
  bool     borrow = false;
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized,
                    rx_doc_subtract((uint16_t)k_doc_test_sub_0010, &diff, &borrow));
}

static void test_subtract_wrong_mode_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_add));
  uint16_t diff   = 0;
  bool     borrow = false;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
                    rx_doc_subtract((uint16_t)k_doc_test_sub_0010, &diff, &borrow));
}

static void test_subtract_without_borrow(void)
{
  /* Driver contract: write subtrahend to DODSR, read DODSR back into *diff,
     read DOPCF into *borrow, and clear DOPCF via DOPCFCL. The mock cannot
     hook struct writes, so *diff equals what the driver wrote rather than
     the HW-computed result. */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_subtract));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_set_reference((uint16_t)k_doc_test_ref_1234));

  uint16_t diff   = 0;
  bool     borrow = true;
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_doc_subtract((uint16_t)k_doc_test_sub_0010, &diff, &borrow));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_doc_test_sub_0010, diff);
  TEST_ASSERT_FALSE(borrow);
  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_doc_regs.docr & (uint8_t)k_doc_docr_dopcf_mask);
}

static void test_subtract_with_borrow(void)
{
  /* Pre-arm DOPCF so the driver reads borrow=true, then verify it clears
     DOPCF via DOPCFCL after reading. */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_subtract));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_set_reference((uint16_t)k_doc_test_sub_0010));
  mock_rx_doc_set_dopcf(true);

  uint16_t diff   = 0;
  bool     borrow = false;
  TEST_ASSERT_EQUAL(k_rx_ok,
                    rx_doc_subtract((uint16_t)k_doc_test_sub_0100, &diff, &borrow));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)k_doc_test_sub_0100, diff);
  TEST_ASSERT_TRUE(borrow);
  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_doc_regs.docr & (uint8_t)k_doc_docr_dopcf_mask);
}

/* =============================================================================
 * rx_doc_deinit()
 * =============================================================================
 */

static void test_deinit_without_init_is_noop(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_deinit());
  /* No flag changes expected; mstpcrb stays in its reset-default "all stopped"
     pattern, and DOCR is still zero. */
  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_doc_regs.docr);
}

static void test_deinit_clears_docr_and_stops_module(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_deinit());

  TEST_ASSERT_EQUAL_UINT8(0U, g_mock_doc_regs.docr);
  /* MSTPB6 back to "stopped" (bit set). */
  TEST_ASSERT_NOT_EQUAL(0U,
                        g_mock_doc_system_regs.mstpcrb & (uint32_t)k_doc_test_mstpb6_mask);
}

static void test_deinit_allows_reinit(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_compare));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_deinit());

  /* Post-deinit, compare should be rejected (driver no longer initialized). */
  bool matched = false;
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized,
                    rx_doc_compare((uint16_t)k_doc_test_value_cafe, &matched));

  /* And re-init must succeed. */
  TEST_ASSERT_EQUAL(k_rx_ok, rx_doc_init(k_rx_doc_mode_add));
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* init */
  RUN_TEST(test_init_compare_mode_clears_mstpb6);
  RUN_TEST(test_init_compare_mode_writes_docr);
  RUN_TEST(test_init_compare_neq_mode_writes_docr);
  RUN_TEST(test_init_add_mode_writes_docr);
  RUN_TEST(test_init_subtract_mode_writes_docr);
  RUN_TEST(test_init_rejects_invalid_mode);
  RUN_TEST(test_reinit_switches_mode);

  /* set_reference */
  RUN_TEST(test_set_reference_before_init_rejected);
  RUN_TEST(test_set_reference_writes_dodir);

  /* compare */
  RUN_TEST(test_compare_null_output_rejected);
  RUN_TEST(test_compare_before_init_rejected);
  RUN_TEST(test_compare_wrong_mode_rejected);
  RUN_TEST(test_compare_match_reports_true_and_clears_flag);
  RUN_TEST(test_compare_mismatch_reports_false);
  RUN_TEST(test_compare_neq_match_reports_false);
  RUN_TEST(test_compare_neq_mismatch_reports_true);

  /* add */
  RUN_TEST(test_add_null_sum_rejected);
  RUN_TEST(test_add_null_overflow_rejected);
  RUN_TEST(test_add_before_init_rejected);
  RUN_TEST(test_add_wrong_mode_rejected);
  RUN_TEST(test_add_without_carry);
  RUN_TEST(test_add_with_carry);

  /* subtract */
  RUN_TEST(test_subtract_null_difference_rejected);
  RUN_TEST(test_subtract_null_borrow_rejected);
  RUN_TEST(test_subtract_before_init_rejected);
  RUN_TEST(test_subtract_wrong_mode_rejected);
  RUN_TEST(test_subtract_without_borrow);
  RUN_TEST(test_subtract_with_borrow);

  /* deinit */
  RUN_TEST(test_deinit_without_init_is_noop);
  RUN_TEST(test_deinit_clears_docr_and_stops_module);
  RUN_TEST(test_deinit_allows_reinit);

  return UNITY_END();
}
