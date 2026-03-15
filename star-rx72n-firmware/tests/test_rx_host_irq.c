/**
 * @file test_rx_host_irq.c
 * @brief Unit Tests for HOST_IRQ GPIO Output Driver (P67)
 *
 * @details
 * Self-contained test that reimplements the HOST_IRQ driver logic
 * against mock PORT registers. Follows the same pattern as test_rx_poeg.c.
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

/* =============================================================================
 * Mock Register Constants
 * =============================================================================
 */

typedef enum : uint8_t {
  k_port_reg_padding          = 31U,   /**< Padding bytes between PORT register fields */
  k_port_all_bits_set         = 0xFFU, /**< All bits set in an 8-bit register */
  k_port_low_nibble           = 0x0FU, /**< Lower nibble mask */
  k_port_lower_7_bits         = 0x7FU, /**< Lower 7 bits mask (bit 7 clear) */
  k_port_upper_nibble_or_mask = 0x8FU, /**< Upper bit + lower nibble combined result */
  k_test_cycle_count          = 5U,    /**< Number of assert/deassert cycles in loop test */
  k_test_info_count_1         = 1U,    /**< Expected info log count after one operation */
} test_host_irq_reg_constants_t;

/* =============================================================================
 * Mock Register Areas
 * =============================================================================
 */

typedef struct {
  volatile uint8_t pdr;
  uint8_t          pad0[k_port_reg_padding];
  volatile uint8_t podr;
  uint8_t          pad1[k_port_reg_padding];
  volatile uint8_t pidr;
  uint8_t          pad2[k_port_reg_padding];
  volatile uint8_t pmr;
} mock_port_regs_t;

static mock_port_regs_t s_mock_port6;

/* Mock log counts */
static uint32_t s_error_log_count;
static uint32_t s_info_log_count;

/* =============================================================================
 * Constants (match rx_host_irq.c)
 * =============================================================================
 */

typedef enum : uint8_t {
  k_host_irq_pin_bit  = 7,
  k_host_irq_pin_mask = 0x80,
  k_host_irq_active   = 0,
  k_host_irq_idle     = 1,
} test_host_irq_constants_t;

enum {
  k_rx_ok                = 0,
  k_rx_err_null_ptr      = 0x504,
  k_rx_err_invalid_state = 0x104,
};

typedef int32_t rx_err_t;

/* =============================================================================
 * Driver State (reimplemented for testing)
 * =============================================================================
 */

static bool s_initialized;

/* =============================================================================
 * Reimplemented Driver Functions (targeting mock registers)
 * =============================================================================
 */

static rx_err_t test_host_irq_init(void)
{
  if (s_initialized) {
    s_error_log_count++;
    return k_rx_err_invalid_state;
  }

  /* Step 1: GPIO mode (clear PMR bit 7) */
  s_mock_port6.pmr &= (uint8_t)~k_host_irq_pin_mask;

  /* Step 2: Set output HIGH (deasserted / idle) before enabling output */
  s_mock_port6.podr |= k_host_irq_pin_mask;

  /* Step 3: Set direction to output */
  s_mock_port6.pdr |= k_host_irq_pin_mask;

  s_initialized = true;

  s_info_log_count++;
  return k_rx_ok;
}

static rx_err_t test_host_irq_deinit(void)
{
  if (!s_initialized) {
    s_error_log_count++;
    return k_rx_err_invalid_state;
  }

  /* Deassert before reverting to input */
  s_mock_port6.podr |= k_host_irq_pin_mask;

  /* Revert to input (safe default) */
  s_mock_port6.pdr &= (uint8_t)~k_host_irq_pin_mask;

  s_initialized = false;

  s_info_log_count++;
  return k_rx_ok;
}

static rx_err_t test_host_irq_assert(void)
{
  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  s_mock_port6.podr &= (uint8_t)~k_host_irq_pin_mask;
  return k_rx_ok;
}

static rx_err_t test_host_irq_deassert(void)
{
  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  s_mock_port6.podr |= k_host_irq_pin_mask;
  return k_rx_ok;
}

static rx_err_t test_host_irq_is_asserted(bool* asserted)
{
  if (asserted == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Active-low: pin LOW means asserted */
  *asserted = (bool)((s_mock_port6.podr & k_host_irq_pin_mask) == k_host_irq_active);
  return k_rx_ok;
}

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

void setUp(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_port6, 0, sizeof(s_mock_port6));
  s_error_log_count = 0;
  s_info_log_count  = 0;
  s_initialized     = false;
}

void tearDown(void) {}

/* =============================================================================
 * Init Tests
 * =============================================================================
 */

void test_init_clears_pmr_bit(void)
{
  s_mock_port6.pmr = k_port_all_bits_set;
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_BITS(k_host_irq_pin_mask, k_host_irq_active, s_mock_port6.pmr);
}

void test_init_sets_podr_high(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_BITS(k_host_irq_pin_mask, k_host_irq_pin_mask, s_mock_port6.podr);
}

void test_init_sets_pdr_output(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_BITS(k_host_irq_pin_mask, k_host_irq_pin_mask, s_mock_port6.pdr);
}

void test_init_preserves_other_bits(void)
{
  s_mock_port6.pdr  = k_port_low_nibble;
  s_mock_port6.podr = k_port_low_nibble;
  s_mock_port6.pmr  = k_port_lower_7_bits;
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_EQUAL(k_port_upper_nibble_or_mask, s_mock_port6.pdr);
  TEST_ASSERT_EQUAL(k_port_upper_nibble_or_mask, s_mock_port6.podr);
  TEST_ASSERT_EQUAL(k_port_lower_7_bits, s_mock_port6.pmr);
}

void test_init_double_init_returns_error(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, test_host_irq_init());
}

void test_init_logs_info(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_EQUAL(k_test_info_count_1, s_info_log_count);
}

/* =============================================================================
 * Assert / Deassert Tests
 * =============================================================================
 */

void test_assert_drives_low(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_assert());
  TEST_ASSERT_BITS(k_host_irq_pin_mask, k_host_irq_active, s_mock_port6.podr);
}

void test_deassert_drives_high(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_assert());
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_deassert());
  TEST_ASSERT_BITS(k_host_irq_pin_mask, k_host_irq_pin_mask, s_mock_port6.podr);
}

void test_assert_without_init_returns_error(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, test_host_irq_assert());
}

void test_deassert_without_init_returns_error(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, test_host_irq_deassert());
}

void test_assert_preserves_other_bits(void)
{
  s_mock_port6.podr = k_port_lower_7_bits;
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  /* init sets bit 7 high: 0x7F | 0x80 = 0xFF */
  TEST_ASSERT_EQUAL(k_port_all_bits_set, s_mock_port6.podr);

  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_assert());
  /* assert clears bit 7: 0xFF & ~0x80 = 0x7F */
  TEST_ASSERT_EQUAL(k_port_lower_7_bits, s_mock_port6.podr);
}

void test_multiple_assert_deassert_cycles(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());

  for (uint8_t i = 0; i < k_test_cycle_count; i++) {
    TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_assert());
    TEST_ASSERT_BITS(k_host_irq_pin_mask, k_host_irq_active, s_mock_port6.podr);

    TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_deassert());
    TEST_ASSERT_BITS(k_host_irq_pin_mask, k_host_irq_pin_mask, s_mock_port6.podr);
  }
}

/* =============================================================================
 * Is Asserted Tests
 * =============================================================================
 */

void test_not_asserted_after_init(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  bool asserted = true;
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_is_asserted(&asserted));
  TEST_ASSERT_FALSE(asserted);
}

void test_asserted_after_assert(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_assert());
  bool asserted = false;
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_is_asserted(&asserted));
  TEST_ASSERT_TRUE(asserted);
}

void test_not_asserted_after_deassert(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_assert());
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_deassert());
  bool asserted = true;
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_is_asserted(&asserted));
  TEST_ASSERT_FALSE(asserted);
}

void test_is_asserted_null_returns_error(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, test_host_irq_is_asserted(NULL));
}

/* =============================================================================
 * Deinit Tests
 * =============================================================================
 */

void test_deinit_reverts_to_input(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_BITS(k_host_irq_pin_mask, k_host_irq_pin_mask, s_mock_port6.pdr);

  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_deinit());
  TEST_ASSERT_BITS(k_host_irq_pin_mask, k_host_irq_active, s_mock_port6.pdr);
}

void test_deinit_deasserts_output(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_assert());

  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_deinit());
  TEST_ASSERT_BITS(k_host_irq_pin_mask, k_host_irq_pin_mask, s_mock_port6.podr);
}

void test_deinit_without_init_returns_error(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, test_host_irq_deinit());
}

void test_reinit_after_deinit_succeeds(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_deinit());
  TEST_ASSERT_EQUAL(k_rx_ok, test_host_irq_init());
}

/* =============================================================================
 * GPIO Register Calculation Verification
 * =============================================================================
 */

void test_pin_mask_matches_bit_position(void)
{
  const uint8_t expected_mask = (uint8_t)(1U << k_host_irq_pin_bit);

  TEST_ASSERT_EQUAL(k_host_irq_pin_mask, expected_mask);
  TEST_ASSERT_EQUAL(k_host_irq_pin_mask, expected_mask);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Init tests */
  RUN_TEST(test_init_clears_pmr_bit);
  RUN_TEST(test_init_sets_podr_high);
  RUN_TEST(test_init_sets_pdr_output);
  RUN_TEST(test_init_preserves_other_bits);
  RUN_TEST(test_init_double_init_returns_error);
  RUN_TEST(test_init_logs_info);

  /* Assert/Deassert tests */
  RUN_TEST(test_assert_drives_low);
  RUN_TEST(test_deassert_drives_high);
  RUN_TEST(test_assert_without_init_returns_error);
  RUN_TEST(test_deassert_without_init_returns_error);
  RUN_TEST(test_assert_preserves_other_bits);
  RUN_TEST(test_multiple_assert_deassert_cycles);

  /* Is Asserted tests */
  RUN_TEST(test_not_asserted_after_init);
  RUN_TEST(test_asserted_after_assert);
  RUN_TEST(test_not_asserted_after_deassert);
  RUN_TEST(test_is_asserted_null_returns_error);

  /* Deinit tests */
  RUN_TEST(test_deinit_reverts_to_input);
  RUN_TEST(test_deinit_deasserts_output);
  RUN_TEST(test_deinit_without_init_returns_error);
  RUN_TEST(test_reinit_after_deinit_succeeds);

  /* Verification tests */
  RUN_TEST(test_pin_mask_matches_bit_position);

  return UNITY_END();
}
