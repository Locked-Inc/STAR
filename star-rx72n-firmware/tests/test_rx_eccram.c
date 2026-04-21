/**
 * @file test_rx_eccram.c
 * @brief Unit tests for the RX72N ECCRAM (ECC-protected RAM) driver
 *
 * @details
 * Exercises rx_eccram.c against mock register files defined in
 * mock_rx72n_eccram_regs.h and mock_rx72n_system_regs.h. Verifies:
 * - Module-stop clear under system PRCR unlock (MSTPCRC.MSTPC6 -> 0)
 * - ECCRAMMODE transitions via ECCRAMPRCR unlock sequence (0xF1 / 0xF0)
 * - Zero-fill of the 32 KB ECCRAM region through g_mock_eccram_region
 * - 1-bit latch enabled (ECCRAM1STSEN = 0x01) for non-disabled modes
 * - Error status readout and clearing
 * - ISR registration and dispatch for 1-bit and 2-bit events
 * - Failing-address capture for 2-bit events (saved before flag clear)
 * - Region introspection returns the hardware-manual-documented bounds
 *
 * All register manipulation sequences match the RX72N HW Manual
 * Chapter 60 (pages 2977-2990).
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <stdint.h>
#include <string.h>

#include "mock_rx72n_eccram_regs.h"
#include "mock_rx72n_system_regs.h"
#include "rx_eccram.h"
#include "rx_err.h"
#include "rx_register_protection.h"
#include "unity.h"

/* =============================================================================
 * External Mock Storage (defined below, declared in the mock headers)
 * =============================================================================
 */

/** @brief Backing storage for eccram_regs() */
rx_eccram_regs_t g_mock_eccram_regs;

/** @brief Backing storage for system_regs() */
rx_system_regs_t g_mock_eccram_system_regs;

/** @brief Backing storage for prcr_reg() (declared extern in mock_rx72n_system_regs.h) */
volatile uint16_t g_mock_prcr;

/** @brief Host-side stand-in for the real ECCRAM region (defined in rx_eccram.c) */
extern uint32_t g_mock_eccram_region[];

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum eccram_test_constants_t
 * @brief Named constants used across test cases
 */
typedef enum : uint32_t {
  k_test_region_base_addr  = 0x00FF8000, /**< Documented ECCRAM base (manual p.2977) */
  k_test_region_end_addr   = 0x00FFFFFF, /**< Documented ECCRAM last byte (p.2977) */
  k_test_region_words      = 0x2000,     /**< 32 KB / 4 bytes = 8192 words */
  k_test_region_first_word = 0,          /**< First word index */
  k_test_sentinel_word     = 0xDEADBEEF, /**< Pattern written to probe zero-fill */
} eccram_test_constants_t;

/**
 * @enum eccram_test_addr_constants_t
 * @brief uintptr_t-sized constants for address capture tests
 */
typedef enum : uintptr_t {
  k_test_failing_addr_1bit = 0x00FFA000, /**< Synthetic 1-bit failing address */
  k_test_failing_addr_2bit = 0x00FFC040, /**< Synthetic 2-bit failing address */
  k_test_zero_addr         = 0,          /**< Sentinel "no address captured" value */
} eccram_test_addr_constants_t;

/**
 * @enum eccram_test_counter_t
 * @brief Callback invocation counter expected values
 */
typedef enum : uint32_t {
  k_test_callback_not_called = 0,
  k_test_callback_once       = 1,
  k_test_callback_twice      = 2,
} eccram_test_counter_t;

/* =============================================================================
 * Callback Instrumentation
 * =============================================================================
 */

/** @brief Count of on_1bit dispatches observed by test callbacks */
static uint32_t s_test_1bit_count;

/** @brief Count of on_2bit dispatches observed by test callbacks */
static uint32_t s_test_2bit_count;

/** @brief Last address delivered to the on_1bit callback */
static uintptr_t s_test_1bit_addr;

/** @brief Last address delivered to the on_2bit callback */
static uintptr_t s_test_2bit_addr;

/** @brief Last ctx pointer delivered to either callback */
static void* s_test_ctx_seen;

static void test_on_1bit(uintptr_t addr, void* ctx)
{
  s_test_1bit_count++;
  s_test_1bit_addr = addr;
  s_test_ctx_seen  = ctx;
}

static void test_on_2bit(uintptr_t addr, void* ctx)
{
  s_test_2bit_count++;
  s_test_2bit_addr = addr;
  s_test_ctx_seen  = ctx;
}

/* =============================================================================
 * Forward declaration of the driver ISR body so tests can invoke it directly
 * =============================================================================
 */
void rx_eccram_ram_error_isr(void);

/**
 * @brief UNIT_TEST-only hook in rx_eccram.c that resets driver static state
 */
void rx_eccram_test_reset_state(void);

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

/**
 * @brief Reset all mock state, PRCR, and register areas before each test
 */
typedef enum : uint8_t {
  k_test_eccram_dirty_sentinel = 0xAAU, /**< pre-dirty fill byte for ECCRAM region */
} test_eccram_local_constants_t;

static void test_setup(void)
{
  g_mock_eccram_regs        = (rx_eccram_regs_t){0};
  g_mock_eccram_system_regs = (rx_system_regs_t){0};
  /* Pre-dirty the simulated ECCRAM region with a known sentinel so the
   * test can detect whether rx_eccram_init() actually zeros it. A manual
   * byte-fill loop sidesteps the cert-msc24-c / DeprecatedOrUnsafeBufferHandling
   * analyzers (which would have us reach for memset_s -- a function glibc
   * never shipped). */
  uint8_t* const region_bytes = (uint8_t*)g_mock_eccram_region;
  const size_t   region_size  = sizeof(uint32_t) * (size_t)k_test_region_words;
  for (size_t b = 0; b < region_size; b++) {
    region_bytes[b] = (uint8_t)k_test_eccram_dirty_sentinel;
  }
  g_mock_prcr = 0;

  /* Simulate hardware default: MSTPCRC.MSTPC6 = 1 (ECCRAM stopped) */
  g_mock_eccram_system_regs.mstpcrc = k_rx_eccram_mstpcrc_bit_mask;

  s_test_1bit_count = 0;
  s_test_2bit_count = 0;
  s_test_1bit_addr  = k_test_zero_addr;
  s_test_2bit_addr  = k_test_zero_addr;
  s_test_ctx_seen   = NULL;

  /* Reset driver static state so each test starts un-initialised */
  rx_eccram_test_reset_state();
}

void setUp(void)
{
  test_setup();
}

void tearDown(void) {}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @brief Rejects out-of-range mode with invalid_arg
 */
static void test_init_rejects_invalid_mode(void)
{
  /* Deliberately inject an out-of-range value to confirm rx_eccram_init
   * rejects it. clang-analyzer-optin.core.EnumCastOutOfRange flags the
   * cast; that's the whole point of the test. */
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const rx_eccram_mode_t bogus = (rx_eccram_mode_t)0xFFU;
  const rx_err_t         err   = rx_eccram_init(bogus);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Init in correct_and_detect clears MSTPCRC.MSTPC6
 */
static void test_init_clears_mstpcrc_bit(void)
{
  const rx_err_t err = rx_eccram_init(k_eccram_mode_correct_and_detect);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(0U, g_mock_eccram_system_regs.mstpcrc & k_rx_eccram_mstpcrc_bit_mask);
}

/**
 * @brief Init re-locks the system PRCR after touching MSTPCRC
 */
static void test_init_relocks_system_prcr(void)
{
  const rx_err_t err = rx_eccram_init(k_eccram_mode_correct_and_detect);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX16(k_rx_prcr_lock, g_mock_prcr);
}

/**
 * @brief Init leaves ECCRAMMODE = ecc_with_check for correct_and_detect
 */
static void test_init_programs_eccrammode_with_check(void)
{
  const rx_err_t err = rx_eccram_init(k_eccram_mode_correct_and_detect);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  const uint8_t mode = g_mock_eccram_regs.eccrammode & k_rx_eccrammode_rammod_mask;
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccrammode_ecc_with_check, mode);
}

/**
 * @brief Init for k_eccram_mode_disabled writes ecc_disabled into ECCRAMMODE
 */
static void test_init_disabled_mode_programs_disabled(void)
{
  const rx_err_t err = rx_eccram_init(k_eccram_mode_disabled);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  const uint8_t mode = g_mock_eccram_regs.eccrammode & k_rx_eccrammode_rammod_mask;
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccrammode_ecc_disabled, mode);
}

/**
 * @brief Init leaves ECCRAMPRCR re-locked (0xF0)
 */
static void test_init_relocks_eccramprcr(void)
{
  const rx_err_t err = rx_eccram_init(k_eccram_mode_correct_and_detect);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccramprcr_lock, g_mock_eccram_regs.eccramprcr);
}

/**
 * @brief Init enables the 1-bit status latch for non-disabled modes
 */
static void test_init_enables_1bit_latch(void)
{
  const rx_err_t err = rx_eccram_init(k_eccram_mode_correct_and_detect);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccram1stsen_enable, g_mock_eccram_regs.eccram1stsen);
}

/**
 * @brief Init in disabled mode skips the 1-bit latch enable
 */
static void test_init_disabled_mode_skips_1bit_latch(void)
{
  const rx_err_t err = rx_eccram_init(k_eccram_mode_disabled);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccram1stsen_disable, g_mock_eccram_regs.eccram1stsen);
}

/**
 * @brief Init zero-fills the entire ECCRAM region (8192 32-bit words)
 */
static void test_init_zero_fills_region(void)
{
  const rx_err_t err = rx_eccram_init(k_eccram_mode_correct_and_detect);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  for (uint32_t i = 0; i < k_test_region_words; i++) {
    TEST_ASSERT_EQUAL_UINT32(0U, g_mock_eccram_region[i]);
  }
}

/**
 * @brief Init in correct_only mode programs ECCRAMMODE to ecc_with_check
 *
 * @details
 * Documents that all "ECC active" public modes (correct_only,
 * correct_and_detect, detect_only) collapse to the same hardware encoding
 * (RAMMOD = 11b), per RX72N HW Manual Chapter 8 (RAM) (ECCRAMMODE,
 * page 2981). The distinction between behaviours is enforced at the ISR
 * dispatch layer, not by the hardware bits.
 *
 * @par Manual reference: RX72N HW Manual Chapter 8 (RAM), ECCRAMMODE
 *      register, RAMMOD[1:0] field, page 2981.
 *
 * @since Version 1.0.0
 */
static void test_init_correct_only_mode_programs_with_check(void)
{
  const rx_err_t err = rx_eccram_init(k_eccram_mode_correct_only);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  const uint8_t mode = g_mock_eccram_regs.eccrammode & k_rx_eccrammode_rammod_mask;
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccrammode_ecc_with_check, mode);
}

/**
 * @brief Init in detect_only mode programs ECCRAMMODE to ecc_with_check
 *
 * @details
 * The detect_only public mode is implemented by enabling the same hardware
 * RAMMOD = 11b encoding and treating any 1-bit event as a fault at the
 * application layer. This test pins the documented hardware-mode mapping.
 *
 * @par Manual reference: RX72N HW Manual Chapter 8 (RAM), ECCRAMMODE
 *      register, RAMMOD[1:0] field, page 2981.
 *
 * @since Version 1.0.0
 */
static void test_init_detect_only_mode_programs_with_check(void)
{
  const rx_err_t err = rx_eccram_init(k_eccram_mode_detect_only);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  const uint8_t mode = g_mock_eccram_regs.eccrammode & k_rx_eccrammode_rammod_mask;
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccrammode_ecc_with_check, mode);
}

/**
 * @brief Init clears any latched error flags so a fresh region is clean
 */
static void test_init_clears_latched_flags(void)
{
  /* Arrange: pretend old flags were latched from a previous session */
  g_mock_eccram_regs.eccram1sts = k_rx_eccram1sts_ecc1err_mask;
  g_mock_eccram_regs.eccram2sts = k_rx_eccram2sts_ecc2err_mask;

  const rx_err_t err = rx_eccram_init(k_eccram_mode_correct_and_detect);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccram1sts_clear, g_mock_eccram_regs.eccram1sts);
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccram2sts_clear, g_mock_eccram_regs.eccram2sts);
}

/* =============================================================================
 * Status Readout / Clear Tests
 * =============================================================================
 */

/**
 * @brief get_error_status returns null_ptr when passed nullptr
 */
static void test_get_error_status_null_ptr(void)
{
  const rx_err_t err = rx_eccram_get_error_status(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief get_error_status reports a 1-bit flag and captured address
 */
static void test_get_error_status_reports_1bit(void)
{
  g_mock_eccram_regs.eccram1sts  = k_rx_eccram1sts_ecc1err_mask;
  g_mock_eccram_regs.eccram1ecad = k_test_failing_addr_1bit;

  rx_eccram_status_t status = {.one_bit_error = false,
                               .two_bit_error = false,
                               .one_bit_addr  = 0U,
                               .two_bit_addr  = 0U};
  const rx_err_t     err    = rx_eccram_get_error_status(&status);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(status.one_bit_error);
  TEST_ASSERT_FALSE(status.two_bit_error);
  TEST_ASSERT_EQUAL_UINT((uintptr_t)k_test_failing_addr_1bit, status.one_bit_addr);
}

/**
 * @brief get_error_status reports a 2-bit flag and captured address
 */
static void test_get_error_status_reports_2bit(void)
{
  g_mock_eccram_regs.eccram2sts  = k_rx_eccram2sts_ecc2err_mask;
  g_mock_eccram_regs.eccram2ecad = k_test_failing_addr_2bit;

  rx_eccram_status_t status = {.one_bit_error = false,
                               .two_bit_error = false,
                               .one_bit_addr  = 0U,
                               .two_bit_addr  = 0U};
  const rx_err_t     err    = rx_eccram_get_error_status(&status);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(status.one_bit_error);
  TEST_ASSERT_TRUE(status.two_bit_error);
  TEST_ASSERT_EQUAL_UINT((uintptr_t)k_test_failing_addr_2bit, status.two_bit_addr);
}

/**
 * @brief clear_errors zeroes both flag registers
 */
static void test_clear_errors_zeroes_flags(void)
{
  g_mock_eccram_regs.eccram1sts = k_rx_eccram1sts_ecc1err_mask;
  g_mock_eccram_regs.eccram2sts = k_rx_eccram2sts_ecc2err_mask;

  const rx_err_t err = rx_eccram_clear_errors();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccram1sts_clear, g_mock_eccram_regs.eccram1sts);
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccram2sts_clear, g_mock_eccram_regs.eccram2sts);
}

/* =============================================================================
 * ISR Registration and Dispatch
 * =============================================================================
 */

/**
 * @brief register_error_isr before init fails with invalid_state
 */
static void test_register_isr_before_init_rejected(void)
{
  const rx_err_t err = rx_eccram_register_error_isr(test_on_1bit, test_on_2bit, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief register_error_isr after init succeeds
 */
static void test_register_isr_after_init_ok(void)
{
  (void)rx_eccram_init(k_eccram_mode_correct_and_detect);
  const rx_err_t err = rx_eccram_register_error_isr(test_on_1bit, test_on_2bit, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief register_error_isr with NULL on_1bit keeps previous 1-bit handler
 *
 * @details
 * Documents that passing nullptr for on_1bit leaves the previously
 * installed 1-bit callback in place while still updating the on_2bit
 * slot and the ctx pointer. Verified by registering both handlers, then
 * re-registering with on_1bit = NULL and a fresh ctx, and confirming
 * the original 1-bit handler still fires when a 1-bit event is latched.
 *
 * @par Manual reference: RX72N HW Manual Chapter 8 (RAM), ECCRAM1STS /
 *      ECCRAM1ECAD update behavior, pages 2984 and 2987.
 *
 * @since Version 1.0.0
 */
static void test_register_isr_null_1bit_keeps_previous_handler(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_eccram_init(k_eccram_mode_correct_and_detect));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_eccram_register_error_isr(test_on_1bit, test_on_2bit, NULL));

  int new_ctx = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_eccram_register_error_isr(NULL, test_on_2bit, &new_ctx));

  g_mock_eccram_regs.eccram1sts  = k_rx_eccram1sts_ecc1err_mask;
  g_mock_eccram_regs.eccram1ecad = (uint32_t)k_test_failing_addr_1bit;

  rx_eccram_ram_error_isr();

  TEST_ASSERT_EQUAL_UINT32((uint32_t)k_test_callback_once, s_test_1bit_count);
  TEST_ASSERT_EQUAL_UINT((uintptr_t)k_test_failing_addr_1bit, s_test_1bit_addr);
  TEST_ASSERT_EQUAL_PTR(&new_ctx, s_test_ctx_seen);
}

/**
 * @brief register_error_isr with NULL on_2bit keeps previous 2-bit handler
 *
 * @details
 * Mirror of test_register_isr_null_1bit_keeps_previous_handler. Confirms
 * that passing nullptr for on_2bit preserves the previously installed
 * 2-bit handler while updating on_1bit and ctx, matching the public
 * "register independently" contract documented on rx_eccram_register_error_isr().
 *
 * @par Manual reference: RX72N HW Manual Chapter 8 (RAM), ECCRAM2STS /
 *      ECCRAM2ECAD update behavior, pages 2982 and 2986.
 *
 * @since Version 1.0.0
 */
static void test_register_isr_null_2bit_keeps_previous_handler(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_eccram_init(k_eccram_mode_correct_and_detect));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_eccram_register_error_isr(test_on_1bit, test_on_2bit, NULL));

  int new_ctx = 0;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_eccram_register_error_isr(test_on_1bit, NULL, &new_ctx));

  g_mock_eccram_regs.eccram2sts  = k_rx_eccram2sts_ecc2err_mask;
  g_mock_eccram_regs.eccram2ecad = (uint32_t)k_test_failing_addr_2bit;

  rx_eccram_ram_error_isr();

  TEST_ASSERT_EQUAL_UINT32((uint32_t)k_test_callback_once, s_test_2bit_count);
  TEST_ASSERT_EQUAL_UINT((uintptr_t)k_test_failing_addr_2bit, s_test_2bit_addr);
  TEST_ASSERT_EQUAL_PTR(&new_ctx, s_test_ctx_seen);
}

/**
 * @brief ISR dispatches to the 1-bit handler and clears the flag
 */
static void test_isr_dispatches_1bit_event(void)
{
  (void)rx_eccram_init(k_eccram_mode_correct_and_detect);
  int ctx_marker = 0;
  (void)rx_eccram_register_error_isr(test_on_1bit, test_on_2bit, &ctx_marker);

  g_mock_eccram_regs.eccram1sts  = k_rx_eccram1sts_ecc1err_mask;
  g_mock_eccram_regs.eccram1ecad = k_test_failing_addr_1bit;

  rx_eccram_ram_error_isr();

  TEST_ASSERT_EQUAL_UINT32(k_test_callback_once, s_test_1bit_count);
  TEST_ASSERT_EQUAL_UINT((uintptr_t)k_test_failing_addr_1bit, s_test_1bit_addr);
  TEST_ASSERT_EQUAL_PTR(&ctx_marker, s_test_ctx_seen);
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccram1sts_clear, g_mock_eccram_regs.eccram1sts);
}

/**
 * @brief ISR captures the 2-bit failing address before clearing the flag
 */
static void test_isr_captures_2bit_address_before_clear(void)
{
  (void)rx_eccram_init(k_eccram_mode_correct_and_detect);
  (void)rx_eccram_register_error_isr(test_on_1bit, test_on_2bit, NULL);

  g_mock_eccram_regs.eccram2sts  = k_rx_eccram2sts_ecc2err_mask;
  g_mock_eccram_regs.eccram2ecad = k_test_failing_addr_2bit;

  rx_eccram_ram_error_isr();

  TEST_ASSERT_EQUAL_UINT32(k_test_callback_once, s_test_2bit_count);
  TEST_ASSERT_EQUAL_UINT((uintptr_t)k_test_failing_addr_2bit, s_test_2bit_addr);
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccram2sts_clear, g_mock_eccram_regs.eccram2sts);
}

/**
 * @brief ISR with no pending flags is a no-op and leaves counters at zero
 */
static void test_isr_no_flags_is_noop(void)
{
  (void)rx_eccram_init(k_eccram_mode_correct_and_detect);
  (void)rx_eccram_register_error_isr(test_on_1bit, test_on_2bit, NULL);

  rx_eccram_ram_error_isr();

  TEST_ASSERT_EQUAL_UINT32(k_test_callback_not_called, s_test_1bit_count);
  TEST_ASSERT_EQUAL_UINT32(k_test_callback_not_called, s_test_2bit_count);
}

/**
 * @brief ISR without registered callbacks still clears latched flags
 */
static void test_isr_without_callbacks_still_clears_flags(void)
{
  (void)rx_eccram_init(k_eccram_mode_correct_and_detect);
  /* Intentionally skip register_error_isr */

  g_mock_eccram_regs.eccram1sts = k_rx_eccram1sts_ecc1err_mask;
  g_mock_eccram_regs.eccram2sts = k_rx_eccram2sts_ecc2err_mask;

  rx_eccram_ram_error_isr();

  TEST_ASSERT_EQUAL_HEX8(k_rx_eccram1sts_clear, g_mock_eccram_regs.eccram1sts);
  TEST_ASSERT_EQUAL_HEX8(k_rx_eccram2sts_clear, g_mock_eccram_regs.eccram2sts);
}

/* =============================================================================
 * Region Introspection
 * =============================================================================
 */

/**
 * @brief region_start returns the manual-documented base address
 */
static void test_region_start_returns_base_addr(void)
{
  TEST_ASSERT_EQUAL_UINT((uintptr_t)k_test_region_base_addr, rx_eccram_region_start());
}

/**
 * @brief region_end returns the manual-documented last address
 */
static void test_region_end_returns_end_addr(void)
{
  TEST_ASSERT_EQUAL_UINT((uintptr_t)k_test_region_end_addr, rx_eccram_region_end());
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_init_rejects_invalid_mode);
  RUN_TEST(test_init_clears_mstpcrc_bit);
  RUN_TEST(test_init_relocks_system_prcr);
  RUN_TEST(test_init_programs_eccrammode_with_check);
  RUN_TEST(test_init_disabled_mode_programs_disabled);
  RUN_TEST(test_init_relocks_eccramprcr);
  RUN_TEST(test_init_enables_1bit_latch);
  RUN_TEST(test_init_disabled_mode_skips_1bit_latch);
  RUN_TEST(test_init_zero_fills_region);
  RUN_TEST(test_init_correct_only_mode_programs_with_check);
  RUN_TEST(test_init_detect_only_mode_programs_with_check);
  RUN_TEST(test_init_clears_latched_flags);

  RUN_TEST(test_get_error_status_null_ptr);
  RUN_TEST(test_get_error_status_reports_1bit);
  RUN_TEST(test_get_error_status_reports_2bit);
  RUN_TEST(test_clear_errors_zeroes_flags);

  RUN_TEST(test_register_isr_before_init_rejected);
  RUN_TEST(test_register_isr_after_init_ok);
  RUN_TEST(test_register_isr_null_1bit_keeps_previous_handler);
  RUN_TEST(test_register_isr_null_2bit_keeps_previous_handler);
  RUN_TEST(test_isr_dispatches_1bit_event);
  RUN_TEST(test_isr_captures_2bit_address_before_clear);
  RUN_TEST(test_isr_no_flags_is_noop);
  RUN_TEST(test_isr_without_callbacks_still_clears_flags);

  RUN_TEST(test_region_start_returns_base_addr);
  RUN_TEST(test_region_end_returns_end_addr);

  return UNITY_END();
}
