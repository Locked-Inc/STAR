/**
 * @file test_rx_stdbit.c
 * @brief Unit Tests for C23 Bit Manipulation Polyfill
 *
 * @details
 * Comprehensive test suite for the rx_stdbit.h header, verifying correct
 * behavior of bit manipulation functions including leading/trailing zeros,
 * population count, power-of-2 testing, bit width, and bit ceiling.
 *
 * ## Test Coverage Summary
 *
 * @par Test Categories:
 * | Category | Test Count | Coverage |
 * |----------|-----------|----------|
 * | stdc_leading_zeros_ui | 5 | Zero, one, max, powers, arbitrary |
 * | stdc_trailing_zeros_ui | 5 | Zero, one, max, powers, arbitrary |
 * | stdc_count_ones_ui | 5 | Zero, one, max, pattern, arbitrary |
 * | stdc_has_single_bit_ui | 5 | Zero, powers, non-powers |
 * | stdc_bit_width_ui | 5 | Zero, one, max, powers, arbitrary |
 * | stdc_bit_ceil_ui | 5 | Zero, one, powers, non-powers |
 * | **Total** | **30 tests** | **100% function coverage** |
 *
 * @see rx_stdbit.h Header under test
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

#include "rx_stdbit.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/** @brief Bit width of unsigned int on this platform */
typedef enum : uint8_t {
  k_uint_bits = sizeof(unsigned int) * CHAR_BIT, /**< Total bits in unsigned int (32) */
} test_bit_constants_t;

/* =============================================================================
 * Unity Setup/Teardown
 * =============================================================================
 */

void setUp(void) {}
void tearDown(void) {}

/* =============================================================================
 * stdc_leading_zeros_ui Tests
 * =============================================================================
 */

/**
 * @brief Leading zeros of 0 returns total bit width
 */
void test_leading_zeros_zero(void)
{
  TEST_ASSERT_EQUAL_UINT(k_uint_bits, stdc_leading_zeros_ui(0U));
}

/**
 * @brief Leading zeros of 1 returns bit_width - 1
 */
void test_leading_zeros_one(void)
{
  TEST_ASSERT_EQUAL_UINT(k_uint_bits - 1U, stdc_leading_zeros_ui(1U));
}

/**
 * @brief Leading zeros of UINT_MAX returns 0
 */
void test_leading_zeros_max(void)
{
  TEST_ASSERT_EQUAL_UINT(0U, stdc_leading_zeros_ui(UINT_MAX));
}

/**
 * @brief Leading zeros of 0x00FF0000 returns 8
 */
void test_leading_zeros_byte_pattern(void)
{
  TEST_ASSERT_EQUAL_UINT(8U, stdc_leading_zeros_ui(0x00FF0000U));
}

/**
 * @brief Leading zeros of powers of 2
 */
void test_leading_zeros_power_of_2(void)
{
  /* 0x80000000 = 2^31, should have 0 leading zeros */
  TEST_ASSERT_EQUAL_UINT(0U, stdc_leading_zeros_ui(0x80000000U));
  /* 0x00000100 = 2^8, should have 23 leading zeros */
  TEST_ASSERT_EQUAL_UINT(23U, stdc_leading_zeros_ui(0x00000100U));
}

/* =============================================================================
 * stdc_trailing_zeros_ui Tests
 * =============================================================================
 */

/**
 * @brief Trailing zeros of 0 returns total bit width
 */
void test_trailing_zeros_zero(void)
{
  TEST_ASSERT_EQUAL_UINT(k_uint_bits, stdc_trailing_zeros_ui(0U));
}

/**
 * @brief Trailing zeros of 1 returns 0
 */
void test_trailing_zeros_one(void)
{
  TEST_ASSERT_EQUAL_UINT(0U, stdc_trailing_zeros_ui(1U));
}

/**
 * @brief Trailing zeros of 0x100 returns 8
 */
void test_trailing_zeros_power_of_2(void)
{
  TEST_ASSERT_EQUAL_UINT(8U, stdc_trailing_zeros_ui(0x00000100U));
}

/**
 * @brief Trailing zeros of odd number returns 0
 */
void test_trailing_zeros_odd(void)
{
  TEST_ASSERT_EQUAL_UINT(0U, stdc_trailing_zeros_ui(0xFFU));
}

/**
 * @brief Trailing zeros of MSB-only returns bit_width - 1
 */
void test_trailing_zeros_msb_only(void)
{
  TEST_ASSERT_EQUAL_UINT(k_uint_bits - 1U, stdc_trailing_zeros_ui(0x80000000U));
}

/* =============================================================================
 * stdc_count_ones_ui Tests
 * =============================================================================
 */

/**
 * @brief Population count of 0 returns 0
 */
void test_count_ones_zero(void)
{
  TEST_ASSERT_EQUAL_UINT(0U, stdc_count_ones_ui(0U));
}

/**
 * @brief Population count of 1 returns 1
 */
void test_count_ones_one(void)
{
  TEST_ASSERT_EQUAL_UINT(1U, stdc_count_ones_ui(1U));
}

/**
 * @brief Population count of UINT_MAX returns bit width
 */
void test_count_ones_max(void)
{
  TEST_ASSERT_EQUAL_UINT(k_uint_bits, stdc_count_ones_ui(UINT_MAX));
}

/**
 * @brief Population count of 0x0F (4 bits set)
 */
void test_count_ones_nibble(void)
{
  TEST_ASSERT_EQUAL_UINT(4U, stdc_count_ones_ui(0x0FU));
}

/**
 * @brief Population count of alternating bits (0x55555555 = 16 ones in 32 bits)
 */
void test_count_ones_alternating(void)
{
  TEST_ASSERT_EQUAL_UINT(16U, stdc_count_ones_ui(0x55555555U));
}

/* =============================================================================
 * stdc_has_single_bit_ui Tests
 * =============================================================================
 */

/**
 * @brief Zero is not a power of 2
 */
void test_has_single_bit_zero(void)
{
  TEST_ASSERT_FALSE(stdc_has_single_bit_ui(0U));
}

/**
 * @brief 1 is a power of 2 (2^0)
 */
void test_has_single_bit_one(void)
{
  TEST_ASSERT_TRUE(stdc_has_single_bit_ui(1U));
}

/**
 * @brief Various powers of 2
 */
void test_has_single_bit_powers(void)
{
  TEST_ASSERT_TRUE(stdc_has_single_bit_ui(2U));
  TEST_ASSERT_TRUE(stdc_has_single_bit_ui(256U));
  TEST_ASSERT_TRUE(stdc_has_single_bit_ui(0x80000000U));
}

/**
 * @brief Non-powers of 2
 */
void test_has_single_bit_non_powers(void)
{
  TEST_ASSERT_FALSE(stdc_has_single_bit_ui(3U));
  TEST_ASSERT_FALSE(stdc_has_single_bit_ui(255U));
  TEST_ASSERT_FALSE(stdc_has_single_bit_ui(UINT_MAX));
}

/**
 * @brief Adjacent to power of 2 (power +/- 1)
 */
void test_has_single_bit_adjacent(void)
{
  TEST_ASSERT_FALSE(stdc_has_single_bit_ui(127U));  /* 128 - 1 */
  TEST_ASSERT_TRUE(stdc_has_single_bit_ui(128U));   /* 2^7 */
  TEST_ASSERT_FALSE(stdc_has_single_bit_ui(129U));  /* 128 + 1 */
}

/* =============================================================================
 * stdc_bit_width_ui Tests
 * =============================================================================
 */

/**
 * @brief Bit width of 0 is 0
 */
void test_bit_width_zero(void)
{
  TEST_ASSERT_EQUAL_UINT(0U, stdc_bit_width_ui(0U));
}

/**
 * @brief Bit width of 1 is 1
 */
void test_bit_width_one(void)
{
  TEST_ASSERT_EQUAL_UINT(1U, stdc_bit_width_ui(1U));
}

/**
 * @brief Bit width of 255 is 8
 */
void test_bit_width_byte_max(void)
{
  TEST_ASSERT_EQUAL_UINT(8U, stdc_bit_width_ui(255U));
}

/**
 * @brief Bit width of 256 is 9
 */
void test_bit_width_byte_max_plus_one(void)
{
  TEST_ASSERT_EQUAL_UINT(9U, stdc_bit_width_ui(256U));
}

/**
 * @brief Bit width of UINT_MAX is full bit width
 */
void test_bit_width_max(void)
{
  TEST_ASSERT_EQUAL_UINT(k_uint_bits, stdc_bit_width_ui(UINT_MAX));
}

/* =============================================================================
 * stdc_bit_ceil_ui Tests
 * =============================================================================
 */

/**
 * @brief Bit ceil of 0 is 1
 */
void test_bit_ceil_zero(void)
{
  TEST_ASSERT_EQUAL_UINT(1U, stdc_bit_ceil_ui(0U));
}

/**
 * @brief Bit ceil of 1 is 1
 */
void test_bit_ceil_one(void)
{
  TEST_ASSERT_EQUAL_UINT(1U, stdc_bit_ceil_ui(1U));
}

/**
 * @brief Bit ceil of power of 2 returns same value
 */
void test_bit_ceil_power_of_2(void)
{
  TEST_ASSERT_EQUAL_UINT(256U, stdc_bit_ceil_ui(256U));
  TEST_ASSERT_EQUAL_UINT(1024U, stdc_bit_ceil_ui(1024U));
}

/**
 * @brief Bit ceil of non-power rounds up to next power
 */
void test_bit_ceil_non_power(void)
{
  TEST_ASSERT_EQUAL_UINT(256U, stdc_bit_ceil_ui(200U));
  TEST_ASSERT_EQUAL_UINT(256U, stdc_bit_ceil_ui(129U));
  TEST_ASSERT_EQUAL_UINT(128U, stdc_bit_ceil_ui(65U));
}

/**
 * @brief Bit ceil of 2 is 2
 */
void test_bit_ceil_two(void)
{
  TEST_ASSERT_EQUAL_UINT(2U, stdc_bit_ceil_ui(2U));
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* stdc_leading_zeros_ui */
  RUN_TEST(test_leading_zeros_zero);
  RUN_TEST(test_leading_zeros_one);
  RUN_TEST(test_leading_zeros_max);
  RUN_TEST(test_leading_zeros_byte_pattern);
  RUN_TEST(test_leading_zeros_power_of_2);

  /* stdc_trailing_zeros_ui */
  RUN_TEST(test_trailing_zeros_zero);
  RUN_TEST(test_trailing_zeros_one);
  RUN_TEST(test_trailing_zeros_power_of_2);
  RUN_TEST(test_trailing_zeros_odd);
  RUN_TEST(test_trailing_zeros_msb_only);

  /* stdc_count_ones_ui */
  RUN_TEST(test_count_ones_zero);
  RUN_TEST(test_count_ones_one);
  RUN_TEST(test_count_ones_max);
  RUN_TEST(test_count_ones_nibble);
  RUN_TEST(test_count_ones_alternating);

  /* stdc_has_single_bit_ui */
  RUN_TEST(test_has_single_bit_zero);
  RUN_TEST(test_has_single_bit_one);
  RUN_TEST(test_has_single_bit_powers);
  RUN_TEST(test_has_single_bit_non_powers);
  RUN_TEST(test_has_single_bit_adjacent);

  /* stdc_bit_width_ui */
  RUN_TEST(test_bit_width_zero);
  RUN_TEST(test_bit_width_one);
  RUN_TEST(test_bit_width_byte_max);
  RUN_TEST(test_bit_width_byte_max_plus_one);
  RUN_TEST(test_bit_width_max);

  /* stdc_bit_ceil_ui */
  RUN_TEST(test_bit_ceil_zero);
  RUN_TEST(test_bit_ceil_one);
  RUN_TEST(test_bit_ceil_power_of_2);
  RUN_TEST(test_bit_ceil_non_power);
  RUN_TEST(test_bit_ceil_two);

  return UNITY_END();
}
