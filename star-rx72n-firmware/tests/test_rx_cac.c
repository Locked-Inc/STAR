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
 * - Error paths: nullptr, callvr >= caulvr, out-of-range clocks/dividers,
 *   double init, uninitialized start/stop/check/deinit.
 * - Happy paths: verify MSTPCRC.MSTPC19 toggling, CACR1/CACR2/CAULVR/CALLVR
 *   programming for every legal enum value, CFME start/stop, CASTR ack via
 *   CAICR write-1-clear, and optional IRQ-enable bits (mendie, ovfie).
 *
 * @par Manual Reference
 * RX72N Group User's Manual R01UH0824EJ0111 Chapter 11 "Clock Frequency
 * Accuracy Measurement Circuit" - register semantics for CACR0 (p.393),
 * CACR1/FMCS/TCSS (p.393), CACR2/RSCS/RCDS (p.394), CAICR (p.395), CASTR
 * (p.395), CAULVR (p.396), CALLVR (p.397), CACNTBR (p.397).
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

/* g_mock_prcr is declared by mock_rx72n_system_regs.h (included transitively
 * via the mocks). Re-declaring extern here would trip clang-tidy
 * readability-redundant-declaration. */

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum cac_test_window_t
 * @brief Representative CAC measurement window values used throughout tests
 * @details Window bounds are CAULVR/CALLVR register values per RX72N HW
 * Manual p.396/p.397. The "swapped" pair is an intentionally invalid
 * configuration used to exercise rx_cac_init()'s callvr >= caulvr guard.
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_cac_test_caulvr_nominal = 1560U, /**< Upper bound for MOSC/HOCO nominal window */
  k_cac_test_callvr_nominal = 1500U, /**< Lower bound for MOSC/HOCO nominal window */
  k_cac_test_caulvr_swapped = 1000U, /**< Invalid swap: upper < lower */
  k_cac_test_callvr_swapped = 2000U, /**< Invalid swap: lower > upper */
} cac_test_window_t;

/**
 * @enum cac_test_count_t
 * @brief Arbitrary non-zero CACNTBR values injected by rx_cac_check tests
 * @details CACNTBR is the 16-bit read-only counter snapshot register
 * (RX72N HW Manual p.397); tests write these values directly into the mock
 * register bank to verify that rx_cac_check() forwards them unchanged via
 * its out_count argument.
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_cac_test_count_first  = 1234U, /**< Count returned to the out_count pointer */
  k_cac_test_count_second = 999U,  /**< Count discarded via nullptr out_count */
} cac_test_count_t;

/**
 * @enum cac_test_invalid_enum_t
 * @brief Out-of-range enum indices used to exercise rx_cac_init() validation
 * @details Each value is deliberately larger than the highest legal index for
 * the corresponding rx_cac_* enum so that rx_cac_init() returns
 * k_rx_err_invalid_arg. Uses uint8_t storage to match the underlying type of
 * every rx_cac_* enum declared in rx_cac.h.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_test_clock_out_of_range = 8U, /**< > k_cac_clock_clkout25 (7) */
  k_cac_test_div_out_of_range   = 4U, /**< > k_cac_target_div_32 / k_cac_ref_div_8192 (3) */
} cac_test_invalid_enum_t;

/**
 * @enum cac_test_prcr_t
 * @brief Expected PRCR mock value after a successful init cycle
 * @details After rx_cac_init()'s final module-start sequence re-locks PRCR,
 * the last write is the lock key 0xA500 (RX72N HW Manual p.427). Tests
 * observe g_mock_prcr directly because the mock's prcr_reg() accessor
 * backs to a uint16_t.
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_cac_test_prcr_locked = 0xA500U,
} cac_test_prcr_t;

/* =============================================================================
 * Fixtures
 * =============================================================================
 */

/**
 * @brief Per-test reset hook run by Unity before every RUN_TEST
 * @details Clears the mock CAC register bank, the mock system-register bank,
 * the driver's s_initialized flag, and the PRCR mock. Guarantees that each
 * test runs in the identical power-on state described in RX72N HW Manual
 * Ch11.3.
 * @post Driver and mock banks are equivalent to cold boot
 * @since Version 1.0.0
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
 * rx_cac_init() Tests - Error Paths
 * =============================================================================
 */

/**
 * @brief nullptr config returns k_rx_err_null_ptr
 * @details rx_cac.h documents config != nullptr as a precondition.
 * @par Manual reference: RX72N HW Manual Ch11.3 (bring-up sequence requires
 * caller-owned configuration data).
 * @since Version 1.0.0
 */
static void test_init_null_config_returns_null_ptr(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, rx_cac_init(NULL));
  TEST_ASSERT_EQUAL(0U, cac()->cacr1); /* Driver must not have programmed CACR1. */
}

/**
 * @brief callvr >= caulvr returns k_rx_err_invalid_arg
 * @par Manual reference: RX72N HW Manual p.396/p.397 require CAULVR > CALLVR
 * or FERRF fires on every measurement.
 * @since Version 1.0.0
 */
static void test_init_callvr_ge_caulvr_returns_invalid_arg(void)
{
  rx_cac_config_t cfg = make_valid_config();
  cfg.caulvr          = k_cac_test_caulvr_swapped;
  cfg.callvr          = k_cac_test_callvr_swapped;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(0U, cac()->caulvr);
}

/**
 * @brief UCLK is not a legal RSCS reference clock
 * @par Manual reference: RX72N HW Manual p.394 RSCS[2:0] only accepts
 * Main/Sub/HOCO/LOCO/PCLKB/IWDTCLK (000..101).
 * @since Version 1.0.0
 */
static void test_init_reference_uclk_returns_invalid_arg(void)
{
  rx_cac_config_t cfg = make_valid_config();
  cfg.reference_clock = k_cac_clock_uclk;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(0U, cac()->cacr2);
}

/**
 * @brief CLKOUT25M is not a legal RSCS reference clock
 * @par Manual reference: RX72N HW Manual p.394 RSCS rejects 110/111.
 * @since Version 1.0.0
 */
static void test_init_reference_clkout25_returns_invalid_arg(void)
{
  rx_cac_config_t cfg = make_valid_config();
  cfg.reference_clock = k_cac_clock_clkout25;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(0U, cac()->cacr2);
}

/**
 * @brief measured_clock beyond k_cac_clock_clkout25 returns k_rx_err_invalid_arg
 * @details CACR1.FMCS is a 3-bit field (RX72N HW Manual p.393); values > 7
 * are not representable. The driver validates against the typed-enum bound.
 * @par Manual reference: RX72N HW Manual p.393 FMCS field.
 * @since Version 1.0.0
 */
static void test_init_measured_clock_out_of_range_returns_invalid_arg(void)
{
  rx_cac_config_t cfg = make_valid_config();
  /* Write the raw byte through unsigned-char-pointer aliasing -- the standard
   * permits this (C17 6.5p7 unsigned-char exception) and it avoids both the
   * typed-enum cast (EnumCastOutOfRange) and memcpy (cert-msc24-c) that would
   * otherwise be flagged. The whole point of the test is to feed an
   * out-of-range value past the language-level check. */
  ((unsigned char*)&cfg.measured_clock)[0] = k_cac_test_clock_out_of_range;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(0U, cac()->cacr1);
}

/**
 * @brief target_div beyond k_cac_target_div_32 returns k_rx_err_invalid_arg
 * @par Manual reference: RX72N HW Manual p.393 CACR1.TCSS[1:0] (values 00..11).
 * @since Version 1.0.0
 */
static void test_init_target_div_out_of_range_returns_invalid_arg(void)
{
  rx_cac_config_t cfg                  = make_valid_config();
  ((unsigned char*)&cfg.target_div)[0] = k_cac_test_div_out_of_range;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(0U, cac()->cacr1);
}

/**
 * @brief reference_div beyond k_cac_ref_div_8192 returns k_rx_err_invalid_arg
 * @par Manual reference: RX72N HW Manual p.394 CACR2.RCDS[1:0] (values 00..11).
 * @since Version 1.0.0
 */
static void test_init_reference_div_out_of_range_returns_invalid_arg(void)
{
  rx_cac_config_t cfg                     = make_valid_config();
  ((unsigned char*)&cfg.reference_div)[0] = k_cac_test_div_out_of_range;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(0U, cac()->cacr2);
}

/**
 * @brief Double init returns k_rx_err_invalid_state
 * @par Manual reference: RX72N HW Manual Ch11.3 requires MSTP clear only
 * once per configuration campaign.
 * @since Version 1.0.0
 */
static void test_init_twice_returns_invalid_state(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, rx_cac_init(&cfg));
}

/* =============================================================================
 * rx_cac_init() Tests - Happy Paths
 * =============================================================================
 */

/**
 * @brief Successful init clears MSTPCRC.MSTPC19 (module-stop bit)
 * @par Manual reference: RX72N HW Manual pp.410-411 MSTPCRC.MSTPC19.
 * @since Version 1.0.0
 */
static void test_init_valid_config_clears_module_stop_bit(void)
{
  system_regs()->mstpcrc |= ((uint32_t)1U << (uint32_t)k_cac_mstpc_bit);
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(0U, system_regs()->mstpcrc & ((uint32_t)1U << (uint32_t)k_cac_mstpc_bit));
}

/**
 * @brief rx_cac_init leaves CACR0.CFME cleared (measurement not started yet)
 * @par Manual reference: RX72N HW Manual Ch11.3 step 4 (caller starts measurement).
 * @since Version 1.0.0
 */
static void test_init_leaves_cfme_cleared(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(0U, cac()->cacr0 & k_cac_cacr0_cfme_mask);
}

/**
 * @brief Init programs CAULVR / CALLVR and sets CAICR.FERRIE when requested
 * @par Manual reference: RX72N HW Manual p.395 FERRIE; p.396 CAULVR; p.397 CALLVR.
 * @since Version 1.0.0
 */
static void test_init_programs_limits_and_ferrie(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(k_cac_test_caulvr_nominal, cac()->caulvr);
  TEST_ASSERT_EQUAL(k_cac_test_callvr_nominal, cac()->callvr);
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_ferrie_mask) != 0U);
  TEST_ASSERT_EQUAL(0U, cac()->caicr & k_cac_caicr_mendie_mask);
}

/**
 * @brief Init encodes MOSC measured / HOCO reference correctly in CACR1/CACR2
 * @par Manual reference: RX72N HW Manual p.393 FMCS=000 (Main);
 * p.394 RSCS=010 (HOCO), RCDS=10 (/1024), RPS=0 (internal).
 * @since Version 1.0.0
 */
static void test_init_programs_cacr1_cacr2(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(k_cac_cacr1_fmcs_main, cac()->cacr1 & k_cac_cacr1_fmcs_mask);
  TEST_ASSERT_EQUAL(k_cac_cacr2_rscs_hoco, cac()->cacr2 & k_cac_cacr2_rscs_mask);
  TEST_ASSERT_EQUAL(k_cac_cacr2_rcds_div_1024, cac()->cacr2 & k_cac_cacr2_rcds_mask);
  TEST_ASSERT_EQUAL(k_cac_cacr2_rps_internal, cac()->cacr2 & k_cac_cacr2_rps_mask);
}

/**
 * @brief Init re-locks PRCR after toggling MSTPCRC
 * @par Manual reference: RX72N HW Manual p.427 PRCR key 0xA500 = locked.
 * @since Version 1.0.0
 */
static void test_init_relocks_prcr(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL((uint16_t)k_cac_test_prcr_locked, g_mock_prcr);
}

/**
 * @brief enable_mendie sets CAICR.MENDIE
 * @par Manual reference: RX72N HW Manual p.395 CAICR.MENDIE.
 * @since Version 1.0.0
 */
static void test_init_enable_mendie_sets_mendie_bit(void)
{
  rx_cac_config_t cfg = make_valid_config();
  cfg.enable_mendie   = true;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_mendie_mask) != 0U);
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_ferrie_mask) != 0U);
}

/**
 * @brief enable_ovfie sets CAICR.OVFIE
 * @par Manual reference: RX72N HW Manual p.395 CAICR.OVFIE.
 * @since Version 1.0.0
 */
static void test_init_enable_ovfie_sets_ovfie_bit(void)
{
  rx_cac_config_t cfg = make_valid_config();
  cfg.enable_ferrie   = false;
  cfg.enable_ovfie    = true;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_ovfie_mask) != 0U);
  TEST_ASSERT_EQUAL(0U, cac()->caicr & k_cac_caicr_ferrie_mask);
}

/* =============================================================================
 * rx_cac_init() Tests - Enum Coverage (FMCS / RSCS / TCSS / RCDS)
 * =============================================================================
 */

/**
 * @struct measured_clock_case_t
 * @brief Pairs a measured-clock enum with its expected CACR1.FMCS bits
 * @details Used by test_init_all_measured_clocks_program_fmcs() to walk the
 * full legal range of rx_cac_clock_t and assert the CACR1 encoding.
 * @since Version 1.0.0
 */
typedef struct {
  rx_cac_clock_t input;    /**< Caller-supplied measured clock */
  uint8_t        expected; /**< Expected CACR1.FMCS field value */
} measured_clock_case_t;

/**
 * @enum measured_clock_case_count_t
 * @brief Length of the measured-clock coverage table
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_measured_clock_case_count = 8U, /**< FMCS field covers 000..111 */
} measured_clock_case_count_t;

/**
 * @brief Every legal measured_clock programs the right CACR1.FMCS bits
 * @par Manual reference: RX72N HW Manual p.393 FMCS[2:0] encoding table.
 * @since Version 1.0.0
 */
static void test_init_all_measured_clocks_program_fmcs(void)
{
  const measured_clock_case_t cases[k_measured_clock_case_count] = {
    {k_cac_clock_main, k_cac_cacr1_fmcs_main},
    {k_cac_clock_sub, k_cac_cacr1_fmcs_sub},
    {k_cac_clock_hoco, k_cac_cacr1_fmcs_hoco},
    {k_cac_clock_loco, k_cac_cacr1_fmcs_loco},
    {k_cac_clock_pclkb, k_cac_cacr1_fmcs_pclkb},
    {k_cac_clock_iwdtclk, k_cac_cacr1_fmcs_iwdtclk},
    {k_cac_clock_uclk, k_cac_cacr1_fmcs_uclk},
    {k_cac_clock_clkout25, k_cac_cacr1_fmcs_clkout25},
  };
  for (uint8_t i = 0; i < k_measured_clock_case_count; ++i) {
    setUp();
    rx_cac_config_t cfg = make_valid_config();
    cfg.measured_clock  = cases[i].input;
    TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
    TEST_ASSERT_EQUAL(cases[i].expected, cac()->cacr1 & k_cac_cacr1_fmcs_mask);
  }
}

/**
 * @struct reference_clock_case_t
 * @brief Pairs a reference-clock enum with its expected CACR2.RSCS bits
 * @since Version 1.0.0
 */
typedef struct {
  rx_cac_clock_t input;
  uint8_t        expected;
} reference_clock_case_t;

/**
 * @enum reference_clock_case_count_t
 * @brief Length of the reference-clock coverage table
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_reference_clock_case_count = 6U, /**< RSCS[2:0] legal encoding is 000..101 */
} reference_clock_case_count_t;

/**
 * @brief Every legal reference_clock programs the right CACR2.RSCS bits
 * @par Manual reference: RX72N HW Manual p.394 RSCS[2:0] encoding table.
 * @since Version 1.0.0
 */
static void test_init_all_reference_clocks_program_rscs(void)
{
  const reference_clock_case_t cases[k_reference_clock_case_count] = {
    {k_cac_clock_main, k_cac_cacr2_rscs_main},
    {k_cac_clock_sub, k_cac_cacr2_rscs_sub},
    {k_cac_clock_hoco, k_cac_cacr2_rscs_hoco},
    {k_cac_clock_loco, k_cac_cacr2_rscs_loco},
    {k_cac_clock_pclkb, k_cac_cacr2_rscs_pclkb},
    {k_cac_clock_iwdtclk, k_cac_cacr2_rscs_iwdtclk},
  };
  for (uint8_t i = 0; i < k_reference_clock_case_count; ++i) {
    setUp();
    rx_cac_config_t cfg = make_valid_config();
    cfg.reference_clock = cases[i].input;
    TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
    TEST_ASSERT_EQUAL(cases[i].expected, cac()->cacr2 & k_cac_cacr2_rscs_mask);
  }
}

/**
 * @struct target_div_case_t
 * @brief Pairs a target divider enum with its expected CACR1.TCSS bits
 * @since Version 1.0.0
 */
typedef struct {
  rx_cac_target_div_t input;
  uint8_t             expected;
} target_div_case_t;

/**
 * @enum target_div_case_count_t
 * @brief Length of the target-divider coverage table
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_target_div_case_count = 4U, /**< TCSS[1:0] covers 00..11 */
} target_div_case_count_t;

/**
 * @brief Every legal target_div programs the right CACR1.TCSS bits
 * @par Manual reference: RX72N HW Manual p.393 TCSS[1:0] encoding (/1, /4, /8, /32).
 * @since Version 1.0.0
 */
static void test_init_all_target_divs_program_tcss(void)
{
  const target_div_case_t cases[k_target_div_case_count] = {
    {k_cac_target_div_1, k_cac_cacr1_tcss_div_1},
    {k_cac_target_div_4, k_cac_cacr1_tcss_div_4},
    {k_cac_target_div_8, k_cac_cacr1_tcss_div_8},
    {k_cac_target_div_32, k_cac_cacr1_tcss_div_32},
  };
  for (uint8_t i = 0; i < k_target_div_case_count; ++i) {
    setUp();
    rx_cac_config_t cfg = make_valid_config();
    cfg.target_div      = cases[i].input;
    TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
    TEST_ASSERT_EQUAL(cases[i].expected, cac()->cacr1 & k_cac_cacr1_tcss_mask);
  }
}

/**
 * @struct ref_div_case_t
 * @brief Pairs a reference divider enum with its expected CACR2.RCDS bits
 * @since Version 1.0.0
 */
typedef struct {
  rx_cac_ref_div_t input;
  uint8_t          expected;
} ref_div_case_t;

/**
 * @enum ref_div_case_count_t
 * @brief Length of the reference-divider coverage table
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_ref_div_case_count = 4U, /**< RCDS[1:0] covers 00..11 */
} ref_div_case_count_t;

/**
 * @brief Every legal reference_div programs the right CACR2.RCDS bits
 * @par Manual reference: RX72N HW Manual p.394 RCDS[1:0] encoding (/32, /128,
 * /1024, /8192).
 * @since Version 1.0.0
 */
static void test_init_all_ref_divs_program_rcds(void)
{
  const ref_div_case_t cases[k_ref_div_case_count] = {
    {k_cac_ref_div_32, k_cac_cacr2_rcds_div_32},
    {k_cac_ref_div_128, k_cac_cacr2_rcds_div_128},
    {k_cac_ref_div_1024, k_cac_cacr2_rcds_div_1024},
    {k_cac_ref_div_8192, k_cac_cacr2_rcds_div_8192},
  };
  for (uint8_t i = 0; i < k_ref_div_case_count; ++i) {
    setUp();
    rx_cac_config_t cfg = make_valid_config();
    cfg.reference_div   = cases[i].input;
    TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
    TEST_ASSERT_EQUAL(cases[i].expected, cac()->cacr2 & k_cac_cacr2_rcds_mask);
  }
}

/* =============================================================================
 * rx_cac_start() / rx_cac_stop() Tests
 * =============================================================================
 */

/**
 * @brief rx_cac_start before init returns k_rx_err_not_initialized
 * @par Manual reference: RX72N HW Manual Ch11.3 forbids CFME=1 before bring-up.
 * @since Version 1.0.0
 */
static void test_start_without_init_returns_not_initialized(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, rx_cac_start());
  TEST_ASSERT_EQUAL(0U, cac()->cacr0);
}

/**
 * @brief rx_cac_stop before init returns k_rx_err_not_initialized
 * @since Version 1.0.0
 */
static void test_stop_without_init_returns_not_initialized(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, rx_cac_stop());
  TEST_ASSERT_EQUAL(0U, cac()->cacr0);
}

/**
 * @brief rx_cac_start sets CACR0.CFME = 1
 * @par Manual reference: RX72N HW Manual p.393 CACR0.CFME.
 * @since Version 1.0.0
 */
static void test_start_sets_cfme(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_start());
  TEST_ASSERT_EQUAL(k_cac_cacr0_cfme_start, cac()->cacr0 & k_cac_cacr0_cfme_mask);
}

/**
 * @brief rx_cac_stop clears CACR0.CFME
 * @par Manual reference: RX72N HW Manual p.393 CACR0.CFME.
 * @since Version 1.0.0
 */
static void test_stop_clears_cfme(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_start());
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_stop());
  TEST_ASSERT_EQUAL(0U, cac()->cacr0 & k_cac_cacr0_cfme_mask);
}

/* =============================================================================
 * rx_cac_check() Tests
 * =============================================================================
 */

/**
 * @brief Checking before init returns false and does not read CASTR
 * @since Version 1.0.0
 */
static void test_check_before_init_returns_false(void)
{
  uint32_t count = k_cac_test_count_first;
  TEST_ASSERT_FALSE(rx_cac_check(&count));
  TEST_ASSERT_EQUAL(k_cac_test_count_first, count); /* out_count untouched */
}

/**
 * @brief FERRF=1 => rx_cac_check returns true and copies CACNTBR to out_count
 * @par Manual reference: RX72N HW Manual p.395 CASTR.FERRF; p.397 CACNTBR.
 * @since Version 1.0.0
 */
static void test_check_returns_true_when_ferrf_set(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  cac()->castr   = k_cac_castr_ferrf_mask;
  cac()->cacntbr = k_cac_test_count_first;
  uint32_t count = 0U;
  TEST_ASSERT_TRUE(rx_cac_check(&count));
  TEST_ASSERT_EQUAL(k_cac_test_count_first, count);
}

/**
 * @brief MENDF-only CASTR (no FERRF) => rx_cac_check returns false
 * @par Manual reference: RX72N HW Manual p.395 CASTR.MENDF / FERRF.
 * @since Version 1.0.0
 */
static void test_check_returns_false_when_ferrf_clear(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  cac()->castr = k_cac_castr_mendf_mask;
  TEST_ASSERT_FALSE(rx_cac_check(NULL));
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_mendfcl_mask) != 0U);
}

/**
 * @brief rx_cac_check writes all three clear bits while preserving IRQ enables
 * @par Manual reference: RX72N HW Manual p.395 CAICR write-1-clear semantics
 * for FERRFCL/MENDFCL/OVFFCL.
 * @since Version 1.0.0
 */
static void test_check_writes_clear_bits_and_keeps_ferrie(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  cac()->castr = k_cac_castr_ferrf_mask;
  (void)rx_cac_check(NULL);
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_ferrie_mask) != 0U);
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_ferrfcl_mask) != 0U);
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_mendfcl_mask) != 0U);
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_ovffcl_mask) != 0U);
}

/**
 * @brief rx_cac_check with every IRQ enable set preserves all three across ack
 * @details Forces the FERRIE / MENDIE / OVFIE branch of the CAICR read-modify-
 * write so each enable bit is walked on the preserved path.
 * @par Manual reference: RX72N HW Manual p.395 CAICR enables.
 * @since Version 1.0.0
 */
static void test_check_preserves_all_irq_enables(void)
{
  rx_cac_config_t cfg = make_valid_config();
  cfg.enable_mendie   = true;
  cfg.enable_ovfie    = true;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  cac()->castr = k_cac_castr_ferrf_mask;
  (void)rx_cac_check(NULL);
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_ferrie_mask) != 0U);
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_mendie_mask) != 0U);
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_ovfie_mask) != 0U);
}

/**
 * @brief rx_cac_check with out_count == nullptr still reads status and acks
 * @par Manual reference: RX72N HW Manual p.397 CACNTBR is read-only; skipping
 * it must not disturb CASTR ack behavior.
 * @since Version 1.0.0
 */
static void test_check_tolerates_null_out_count(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  cac()->castr   = k_cac_castr_ferrf_mask;
  cac()->cacntbr = k_cac_test_count_second;
  TEST_ASSERT_TRUE(rx_cac_check(NULL));
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_ferrfcl_mask) != 0U);
}

/* =============================================================================
 * rx_cac_deinit() Tests
 * =============================================================================
 */

/**
 * @brief Deinit without init returns k_rx_err_not_initialized
 * @since Version 1.0.0
 */
static void test_deinit_without_init_returns_not_initialized(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, rx_cac_deinit());
  TEST_ASSERT_EQUAL(0U, cac()->cacr0);
}

/**
 * @brief Deinit asserts MSTPCRC.MSTPC19 (module-stop)
 * @par Manual reference: RX72N HW Manual pp.410-411 MSTPCRC.MSTPC19.
 * @since Version 1.0.0
 */
static void test_deinit_sets_module_stop(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_deinit());
  TEST_ASSERT_NOT_EQUAL(0U, system_regs()->mstpcrc & ((uint32_t)1U << (uint32_t)k_cac_mstpc_bit));
}

/**
 * @brief rx_cac_init -> rx_cac_deinit -> rx_cac_init round-trip is permitted
 * @since Version 1.0.0
 */
static void test_deinit_allows_reinit(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_deinit());
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
}

/**
 * @brief Deinit clears CFME, zeros CACR1/CACR2, and acks pending CASTR flags
 * @par Manual reference: RX72N HW Manual Ch11.3 teardown is the reverse of
 * the bring-up: disable measurement (p.393), then re-stop the module
 * (pp.410-411).
 * @since Version 1.0.0
 */
static void test_deinit_clears_cfme_and_enables(void)
{
  rx_cac_config_t cfg = make_valid_config();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_init(&cfg));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_start());
  TEST_ASSERT_EQUAL(k_rx_ok, rx_cac_deinit());
  TEST_ASSERT_EQUAL(0U, cac()->cacr0 & k_cac_cacr0_cfme_mask);
  TEST_ASSERT_EQUAL(0U, cac()->cacr1);
  TEST_ASSERT_EQUAL(0U, cac()->cacr2);
  TEST_ASSERT_TRUE((cac()->caicr & k_cac_caicr_clear_all_flags) != 0U);
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
  RUN_TEST(test_init_measured_clock_out_of_range_returns_invalid_arg);
  RUN_TEST(test_init_target_div_out_of_range_returns_invalid_arg);
  RUN_TEST(test_init_reference_div_out_of_range_returns_invalid_arg);
  RUN_TEST(test_init_twice_returns_invalid_state);

  RUN_TEST(test_init_valid_config_clears_module_stop_bit);
  RUN_TEST(test_init_leaves_cfme_cleared);
  RUN_TEST(test_init_programs_limits_and_ferrie);
  RUN_TEST(test_init_programs_cacr1_cacr2);
  RUN_TEST(test_init_relocks_prcr);
  RUN_TEST(test_init_enable_mendie_sets_mendie_bit);
  RUN_TEST(test_init_enable_ovfie_sets_ovfie_bit);

  RUN_TEST(test_init_all_measured_clocks_program_fmcs);
  RUN_TEST(test_init_all_reference_clocks_program_rscs);
  RUN_TEST(test_init_all_target_divs_program_tcss);
  RUN_TEST(test_init_all_ref_divs_program_rcds);

  RUN_TEST(test_start_without_init_returns_not_initialized);
  RUN_TEST(test_stop_without_init_returns_not_initialized);
  RUN_TEST(test_start_sets_cfme);
  RUN_TEST(test_stop_clears_cfme);

  RUN_TEST(test_check_before_init_returns_false);
  RUN_TEST(test_check_returns_true_when_ferrf_set);
  RUN_TEST(test_check_returns_false_when_ferrf_clear);
  RUN_TEST(test_check_writes_clear_bits_and_keeps_ferrie);
  RUN_TEST(test_check_preserves_all_irq_enables);
  RUN_TEST(test_check_tolerates_null_out_count);

  RUN_TEST(test_deinit_without_init_returns_not_initialized);
  RUN_TEST(test_deinit_sets_module_stop);
  RUN_TEST(test_deinit_allows_reinit);
  RUN_TEST(test_deinit_clears_cfme_and_enables);

  return UNITY_END();
}
