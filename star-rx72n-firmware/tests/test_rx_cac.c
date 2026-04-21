/**
 * @file test_rx_cac.c
 * @brief Unit Tests for Clock Frequency Accuracy Measurement Circuit (CAC) Driver
 *
 * @details
 * Unity-framework tests covering the five-function rx_cac public API. Each
 * test runs against the host-side CAC mock defined in mock_rx_cac.c so no
 * RX72N hardware is required.
 *
 * @par Test Strategy
 * - Error paths: nullptr, callvr >= caulvr, out-of-range clocks, double init,
 *   uninitialized start/stop/check/deinit.
 * - Happy paths: verify MSTPCRC.MSTPC19 toggling, CACR1/CACR2/CAULVR/CALLVR
 *   programming, CFME start/stop, CASTR ack via CAICR write-1-clear.
 *
 * @see rx_cac.h Driver API under test
 * @see mock_rx_cac.h Host-side mock CAC register bank
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

#include "mock_rx72n_regs.h"
#include "mock_rx_cac.h"
#include "mock_rx_system_regs.h"
#include "rx_cac.h"
#include "rx_err.h"
#include "unity.h"

/* =============================================================================
 * Access to PRCR mock storage
 * =============================================================================
 */

extern volatile uint16_t g_mock_prcr;

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum cac_test_constants_t
 * @brief Representative CAC measurement window values used throughout tests
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_cac_test_caulvr_nominal = 1560, /**< Upper bound for MOSC/HOCO nominal window */
  k_cac_test_callvr_nominal = 1500, /**< Lower bound for MOSC/HOCO nominal window */
  k_cac_test_caulvr_swapped = 1000, /**< Invalid swap: upper < lower */
  k_cac_test_callvr_swapped = 2000, /**< Invalid swap: lower > upper */
} cac_test_constants_t;

/* =============================================================================
 * Fixtures
 * =============================================================================
 */

void setUp(void)
{
  mock_rx_cac_reset();
  mock_system_regs_reset();
  rx_cac_test_reset();
  g_mock_prcr = 0;
}

void tearDown(void) {}

/**
 * @brief Build a representative config for positive-path tests
 * @return rx_cac_config_t Ready-to-use config (MOSC against HOCO, ferrie on)
 * @since Version 1.0.0
 */
static rx_cac_config_t make_valid_config(void)
{
  rx_cac_config_t cfg = {
    .measured_clock  = k_cac_clock_main,
    .reference_clock = k_cac_clock_hoco,
    .target_div      = k_cac_target_div_1,
    .reference_div   = k_cac_ref_div_1024,
    .caulvr          = k_cac_test_caulvr_nominal,
    .callvr          = k_cac_test_callvr_nominal,
    .enable_ferrie   = true,
    .enable_mendie   = false,
    .enable_ovfie    = false,
  };
  return cfg;
}

/* =============================================================================
 * rx_cac_init() Tests
 * =============================================================================
 */

static void test_init_null_config_returns_null_ptr(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_cac_init(NULL));
}

static void test_init_callvr_ge_caulvr_returns_invalid_arg(void)
{
  rx_cac_config_t cfg = make_valid_config();
  cfg.caulvr          = k_cac_test_caulvr_swapped;
  cfg.callvr          = k_cac_test_callvr_swapped;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_cac_init(&cfg));
}

static void test_init_reference_uclk_returns_invalid_arg(void)
{
  rx_cac_config_t cfg = make_valid_config();
  cfg.reference_clock = k_cac_clock_uclk; /* UCLK is measured-only */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_cac_init(&cfg));
}

static void test_init_reference_clkout25_returns_invalid_arg(void)
{
  rx_cac_config_t cfg = make_valid_config();
  cfg.reference_clock = k_cac_clock_clkout25; /* CLKOUT25M is measured-only */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_cac_init(&cfg));
}

static void test_init_valid_config_clears_module_stop_bit(void)
{
  /* Set MSTPC19 = 1 (stopped) to verify init clears it. */
  system_regs()->mstpcrc |= ((uint32_t)1U << (uint32_t)k_cac_mstpc_bit);
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(0U, system_regs()->mstpcrc & ((uint32_t)1U << (uint32_t)k_cac_mstpc_bit));
}

static void test_init_leaves_cfme_cleared(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(0U, cac()->cacr0 & (uint8_t)k_cac_cacr0_cfme_mask);
}

static void test_init_programs_limits_and_ferrie(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(k_cac_test_caulvr_nominal, cac()->caulvr);
  TEST_ASSERT_EQUAL(k_cac_test_callvr_nominal, cac()->callvr);
  TEST_ASSERT_TRUE((cac()->caicr & (uint8_t)k_cac_caicr_ferrie_mask) != 0U);
  TEST_ASSERT_EQUAL(0U, cac()->caicr & (uint8_t)k_cac_caicr_mendie_mask);
}

static void test_init_programs_cacr1_cacr2(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL((uint8_t)k_cac_cacr1_fmcs_main, cac()->cacr1 & (uint8_t)k_cac_cacr1_fmcs_mask);
  TEST_ASSERT_EQUAL((uint8_t)k_cac_cacr2_rscs_hoco, cac()->cacr2 & (uint8_t)k_cac_cacr2_rscs_mask);
  TEST_ASSERT_EQUAL((uint8_t)k_cac_cacr2_rcds_div_1024,
                    cac()->cacr2 & (uint8_t)k_cac_cacr2_rcds_mask);
  TEST_ASSERT_EQUAL((uint8_t)k_cac_cacr2_rps_internal,
                    cac()->cacr2 & (uint8_t)k_cac_cacr2_rps_mask);
}

static void test_init_twice_returns_invalid_state(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_cac_init(&cfg));
}

static void test_init_relocks_prcr(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL((uint16_t)0xA500, g_mock_prcr);
}

/* =============================================================================
 * rx_cac_start() / rx_cac_stop() Tests
 * =============================================================================
 */

static void test_start_without_init_returns_not_initialized(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, rx_cac_start());
}

static void test_stop_without_init_returns_not_initialized(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, rx_cac_stop());
}

static void test_start_sets_cfme(void)
{
  rx_cac_config_t cfg = make_valid_config();
  (void)rx_cac_init(&cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_start());
  TEST_ASSERT_EQUAL((uint8_t)k_cac_cacr0_cfme_start, cac()->cacr0 & (uint8_t)k_cac_cacr0_cfme_mask);
}

static void test_stop_clears_cfme(void)
{
  rx_cac_config_t cfg = make_valid_config();
  (void)rx_cac_init(&cfg);
  (void)rx_cac_start();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_stop());
  TEST_ASSERT_EQUAL(0U, cac()->cacr0 & (uint8_t)k_cac_cacr0_cfme_mask);
}

/* =============================================================================
 * rx_cac_check() Tests
 * =============================================================================
 */

static void test_check_before_init_returns_false(void)
{
  TEST_ASSERT_FALSE(rx_cac_check(NULL));
}

static void test_check_returns_true_when_ferrf_set(void)
{
  rx_cac_config_t cfg = make_valid_config();
  (void)rx_cac_init(&cfg);
  /* Simulate hardware setting FERRF and loading a counter snapshot. */
  cac()->castr   = (uint8_t)k_cac_castr_ferrf_mask;
  cac()->cacntbr = (uint16_t)1234;
  uint32_t count = 0;
  TEST_ASSERT_TRUE(rx_cac_check(&count));
  TEST_ASSERT_EQUAL(1234U, count);
}

static void test_check_returns_false_when_ferrf_clear(void)
{
  rx_cac_config_t cfg = make_valid_config();
  (void)rx_cac_init(&cfg);
  cac()->castr = (uint8_t)k_cac_castr_mendf_mask; /* MENDF only, no FERRF */
  TEST_ASSERT_FALSE(rx_cac_check(NULL));
}

static void test_check_writes_clear_bits_and_keeps_ferrie(void)
{
  rx_cac_config_t cfg = make_valid_config();
  (void)rx_cac_init(&cfg);
  cac()->castr = (uint8_t)k_cac_castr_ferrf_mask;
  (void)rx_cac_check(NULL);
  /* FERRIE must remain enabled; clear bits must have been asserted. */
  TEST_ASSERT_TRUE((cac()->caicr & (uint8_t)k_cac_caicr_ferrie_mask) != 0U);
  TEST_ASSERT_TRUE((cac()->caicr & (uint8_t)k_cac_caicr_ferrfcl_mask) != 0U);
  TEST_ASSERT_TRUE((cac()->caicr & (uint8_t)k_cac_caicr_mendfcl_mask) != 0U);
  TEST_ASSERT_TRUE((cac()->caicr & (uint8_t)k_cac_caicr_ovffcl_mask) != 0U);
}

static void test_check_tolerates_null_out_count(void)
{
  rx_cac_config_t cfg = make_valid_config();
  (void)rx_cac_init(&cfg);
  cac()->castr   = (uint8_t)k_cac_castr_ferrf_mask;
  cac()->cacntbr = (uint16_t)999;
  TEST_ASSERT_TRUE(rx_cac_check(NULL));
}

/* =============================================================================
 * rx_cac_deinit() Tests
 * =============================================================================
 */

static void test_deinit_without_init_returns_not_initialized(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, rx_cac_deinit());
}

static void test_deinit_sets_module_stop(void)
{
  rx_cac_config_t cfg = make_valid_config();
  (void)rx_cac_init(&cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_deinit());
  TEST_ASSERT_NOT_EQUAL(0U, system_regs()->mstpcrc & ((uint32_t)1U << (uint32_t)k_cac_mstpc_bit));
}

static void test_deinit_allows_reinit(void)
{
  rx_cac_config_t cfg = make_valid_config();
  (void)rx_cac_init(&cfg);
  (void)rx_cac_deinit();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
}

static void test_deinit_clears_cfme_and_enables(void)
{
  rx_cac_config_t cfg = make_valid_config();
  (void)rx_cac_init(&cfg);
  (void)rx_cac_start();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_deinit());
  TEST_ASSERT_EQUAL(0U, cac()->cacr0 & (uint8_t)k_cac_cacr0_cfme_mask);
  TEST_ASSERT_EQUAL(0U, cac()->cacr1);
  TEST_ASSERT_EQUAL(0U, cac()->cacr2);
}

/* =============================================================================
 * Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_init_null_config_returns_null_ptr);
  RUN_TEST(test_init_callvr_ge_caulvr_returns_invalid_arg);
  RUN_TEST(test_init_reference_uclk_returns_invalid_arg);
  RUN_TEST(test_init_reference_clkout25_returns_invalid_arg);
  RUN_TEST(test_init_valid_config_clears_module_stop_bit);
  RUN_TEST(test_init_leaves_cfme_cleared);
  RUN_TEST(test_init_programs_limits_and_ferrie);
  RUN_TEST(test_init_programs_cacr1_cacr2);
  RUN_TEST(test_init_twice_returns_invalid_state);
  RUN_TEST(test_init_relocks_prcr);

  RUN_TEST(test_start_without_init_returns_not_initialized);
  RUN_TEST(test_stop_without_init_returns_not_initialized);
  RUN_TEST(test_start_sets_cfme);
  RUN_TEST(test_stop_clears_cfme);

  RUN_TEST(test_check_before_init_returns_false);
  RUN_TEST(test_check_returns_true_when_ferrf_set);
  RUN_TEST(test_check_returns_false_when_ferrf_clear);
  RUN_TEST(test_check_writes_clear_bits_and_keeps_ferrie);
  RUN_TEST(test_check_tolerates_null_out_count);

  RUN_TEST(test_deinit_without_init_returns_not_initialized);
  RUN_TEST(test_deinit_sets_module_stop);
  RUN_TEST(test_deinit_allows_reinit);
  RUN_TEST(test_deinit_clears_cfme_and_enables);

  return UNITY_END();
}
