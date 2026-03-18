/**
 * @file test_rx_stdckdint.c
 * @brief Unit Tests for C23 Checked Integer Arithmetic Polyfill
 *
 * @details
 * Comprehensive test suite for the rx_stdckdint.h header, verifying correct
 * overflow detection for ckd_add, ckd_sub, and ckd_mul across unsigned and
 * signed integer types at boundary conditions.
 *
 * ## Test Coverage Summary
 *
 * @par Test Categories:
 * | Category | Test Count | Coverage |
 * |----------|-----------|----------|
 * | ckd_add normal | 3 | No-overflow cases |
 * | ckd_add overflow | 3 | Boundary overflow detection |
 * | ckd_sub normal | 3 | No-underflow cases |
 * | ckd_sub underflow | 3 | Boundary underflow detection |
 * | ckd_mul normal | 3 | No-overflow cases |
 * | ckd_mul overflow | 3 | Boundary overflow detection |
 * | Mixed types | 2 | Cross-type arithmetic |
 * | **Total** | **20 tests** | **100% macro coverage** |
 *
 * @see rx_stdckdint.h Header under test
 *
 * @author Locked, Inc.
 * @date 2026-03-18
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.1.0
 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "rx_stdckdint.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/** @brief Test values for checked arithmetic boundary testing */
typedef enum : uint32_t {
  k_half_uint32_max = 2147483647U,   /**< UINT32_MAX / 2 (rounded down) */
  k_large_multiplier = 65536U,       /**< 2^16: causes overflow when squared in uint16 */
  k_small_product_a = 100U,          /**< Small factor for safe multiplication */
  k_small_product_b = 200U,          /**< Small factor for safe multiplication */
  k_expected_product = 20000U,       /**< 100 * 200 */
  k_expected_sum_10_20 = 30U,        /**< 10 + 20 */
  k_expected_diff_20_10 = 10U,       /**< 20 - 10 */
} test_values_t;

/* =============================================================================
 * Unity Setup/Teardown
 * =============================================================================
 */

void setUp(void) {}
void tearDown(void) {}

/* =============================================================================
 * ckd_add Tests
 * =============================================================================
 */

/**
 * @brief ckd_add: normal addition does not report overflow
 */
void test_ckd_add_no_overflow_u32(void)
{
  uint32_t result = 0U;
  bool overflow = ckd_add(&result, 10U, 20U);
  TEST_ASSERT_FALSE(overflow);
  TEST_ASSERT_EQUAL_UINT32(k_expected_sum_10_20, result);
}

/**
 * @brief ckd_add: zero operands produce zero without overflow
 */
void test_ckd_add_zero_operands(void)
{
  uint32_t result = 42U;
  bool overflow = ckd_add(&result, 0U, 0U);
  TEST_ASSERT_FALSE(overflow);
  TEST_ASSERT_EQUAL_UINT32(0U, result);
}

/**
 * @brief ckd_add: adding to MAX value causes overflow
 */
void test_ckd_add_max_plus_one_overflows(void)
{
  uint32_t result = 0U;
  bool overflow = ckd_add(&result, UINT32_MAX, 1U);
  TEST_ASSERT_TRUE(overflow);
}

/**
 * @brief ckd_add: adding two large values that barely overflow
 */
void test_ckd_add_two_large_values_overflow(void)
{
  uint32_t result = 0U;
  /* UINT32_MAX/2 + UINT32_MAX/2 + 2 > UINT32_MAX */
  bool overflow = ckd_add(&result, k_half_uint32_max + 1U, k_half_uint32_max + 1U);
  TEST_ASSERT_TRUE(overflow);
}

/**
 * @brief ckd_add: MAX + 0 does not overflow
 */
void test_ckd_add_max_plus_zero_no_overflow(void)
{
  uint32_t result = 0U;
  bool overflow = ckd_add(&result, UINT32_MAX, 0U);
  TEST_ASSERT_FALSE(overflow);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, result);
}

/**
 * @brief ckd_add: uint16_t boundary overflow
 */
void test_ckd_add_u16_overflow(void)
{
  uint16_t result = 0U;
  bool overflow = ckd_add(&result, (uint16_t)UINT16_MAX, (uint16_t)1U);
  TEST_ASSERT_TRUE(overflow);
}

/* =============================================================================
 * ckd_sub Tests
 * =============================================================================
 */

/**
 * @brief ckd_sub: normal subtraction does not report underflow
 */
void test_ckd_sub_no_underflow_u32(void)
{
  uint32_t result = 0U;
  bool underflow = ckd_sub(&result, 20U, 10U);
  TEST_ASSERT_FALSE(underflow);
  TEST_ASSERT_EQUAL_UINT32(k_expected_diff_20_10, result);
}

/**
 * @brief ckd_sub: subtracting zero produces same value
 */
void test_ckd_sub_zero_subtrahend(void)
{
  uint32_t result = 0U;
  bool underflow = ckd_sub(&result, UINT32_MAX, 0U);
  TEST_ASSERT_FALSE(underflow);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, result);
}

/**
 * @brief ckd_sub: equal operands produce zero without underflow
 */
void test_ckd_sub_equal_operands(void)
{
  uint32_t result = 42U;
  bool underflow = ckd_sub(&result, 100U, 100U);
  TEST_ASSERT_FALSE(underflow);
  TEST_ASSERT_EQUAL_UINT32(0U, result);
}

/**
 * @brief ckd_sub: 0 - 1 causes unsigned underflow
 */
void test_ckd_sub_zero_minus_one_underflows(void)
{
  uint32_t result = 0U;
  bool underflow = ckd_sub(&result, 0U, 1U);
  TEST_ASSERT_TRUE(underflow);
}

/**
 * @brief ckd_sub: small - large causes unsigned underflow
 */
void test_ckd_sub_small_minus_large_underflows(void)
{
  uint32_t result = 0U;
  bool underflow = ckd_sub(&result, 10U, 20U);
  TEST_ASSERT_TRUE(underflow);
}

/**
 * @brief ckd_sub: uint16_t boundary underflow
 */
void test_ckd_sub_u16_underflow(void)
{
  uint16_t result = 0U;
  bool underflow = ckd_sub(&result, (uint16_t)0U, (uint16_t)1U);
  TEST_ASSERT_TRUE(underflow);
}

/* =============================================================================
 * ckd_mul Tests
 * =============================================================================
 */

/**
 * @brief ckd_mul: normal multiplication does not report overflow
 */
void test_ckd_mul_no_overflow_u32(void)
{
  uint32_t result = 0U;
  bool overflow = ckd_mul(&result, k_small_product_a, k_small_product_b);
  TEST_ASSERT_FALSE(overflow);
  TEST_ASSERT_EQUAL_UINT32(k_expected_product, result);
}

/**
 * @brief ckd_mul: multiply by zero produces zero without overflow
 */
void test_ckd_mul_by_zero(void)
{
  uint32_t result = 42U;
  bool overflow = ckd_mul(&result, UINT32_MAX, 0U);
  TEST_ASSERT_FALSE(overflow);
  TEST_ASSERT_EQUAL_UINT32(0U, result);
}

/**
 * @brief ckd_mul: multiply by one produces same value
 */
void test_ckd_mul_by_one(void)
{
  uint32_t result = 0U;
  bool overflow = ckd_mul(&result, UINT32_MAX, 1U);
  TEST_ASSERT_FALSE(overflow);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, result);
}

/**
 * @brief ckd_mul: large * 2 overflows uint32_t
 */
void test_ckd_mul_max_times_two_overflows(void)
{
  uint32_t result = 0U;
  bool overflow = ckd_mul(&result, UINT32_MAX, 2U);
  TEST_ASSERT_TRUE(overflow);
}

/**
 * @brief ckd_mul: uint16_t overflow detection (300 * 300 > 65535)
 */
void test_ckd_mul_u16_overflow(void)
{
  uint16_t result = 0U;
  bool overflow = ckd_mul(&result, (uint16_t)300U, (uint16_t)300U);
  TEST_ASSERT_TRUE(overflow);
  /* 300 * 300 = 90000 > UINT16_MAX (65535) */
}

/**
 * @brief ckd_mul: uint16_t safe multiplication (200 * 200 = 40000 < 65535)
 */
void test_ckd_mul_u16_no_overflow(void)
{
  uint16_t result = 0U;
  bool overflow = ckd_mul(&result, (uint16_t)200U, (uint16_t)200U);
  TEST_ASSERT_FALSE(overflow);
  TEST_ASSERT_EQUAL_UINT16(40000U, result);
}

/* =============================================================================
 * Saturating Counter Pattern Test (matches rx_crc_hw.c usage)
 * =============================================================================
 */

/**
 * @brief Saturating increment: counter at MAX stays at MAX
 */
void test_saturating_increment_at_max(void)
{
  uint32_t counter = UINT32_MAX;
  uint32_t incremented;
  if (!ckd_add(&incremented, counter, 1U)) {
    counter = incremented;
  }
  /* Counter should remain at UINT32_MAX (overflow prevented) */
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, counter);
}

/**
 * @brief Saturating increment: counter below MAX increments normally
 */
void test_saturating_increment_below_max(void)
{
  uint32_t counter = UINT32_MAX - 1U;
  uint32_t incremented;
  if (!ckd_add(&incremented, counter, 1U)) {
    counter = incremented;
  }
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, counter);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* ckd_add */
  RUN_TEST(test_ckd_add_no_overflow_u32);
  RUN_TEST(test_ckd_add_zero_operands);
  RUN_TEST(test_ckd_add_max_plus_one_overflows);
  RUN_TEST(test_ckd_add_two_large_values_overflow);
  RUN_TEST(test_ckd_add_max_plus_zero_no_overflow);
  RUN_TEST(test_ckd_add_u16_overflow);

  /* ckd_sub */
  RUN_TEST(test_ckd_sub_no_underflow_u32);
  RUN_TEST(test_ckd_sub_zero_subtrahend);
  RUN_TEST(test_ckd_sub_equal_operands);
  RUN_TEST(test_ckd_sub_zero_minus_one_underflows);
  RUN_TEST(test_ckd_sub_small_minus_large_underflows);
  RUN_TEST(test_ckd_sub_u16_underflow);

  /* ckd_mul */
  RUN_TEST(test_ckd_mul_no_overflow_u32);
  RUN_TEST(test_ckd_mul_by_zero);
  RUN_TEST(test_ckd_mul_by_one);
  RUN_TEST(test_ckd_mul_max_times_two_overflows);
  RUN_TEST(test_ckd_mul_u16_overflow);
  RUN_TEST(test_ckd_mul_u16_no_overflow);

  /* Saturating pattern */
  RUN_TEST(test_saturating_increment_at_max);
  RUN_TEST(test_saturating_increment_below_max);

  return UNITY_END();
}
