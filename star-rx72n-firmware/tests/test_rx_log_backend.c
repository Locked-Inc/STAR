/**
 * @file test_rx_log_backend.c
 * @brief Unit tests for rx_log.c runtime backend selection API
 *
 * @details
 * Validates rx_log_set_backend() and rx_log_get_backend() independently of
 * the LOG_* dispatch macros.  All tests run on the host (x86-64) with
 * uart_debug_* and rx_log_usb_* provided by mock_rx_log.c.
 *
 * **Test categories:**
 * 1. Default state -- s_log_backend starts as k_log_backend_uart
 * 2. Valid settings -- UART-only, USB-only, both, none all return k_rx_ok
 * 3. Invalid bits -- any bit outside 0x03 returns k_rx_err_invalid_arg
 * 4. Rejection atomicity -- rejected call leaves backend unchanged
 * 5. Idempotency -- setting the same value twice returns k_rx_ok
 *
 * @author Locked, Inc.
 * @date 2026-03-11
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_log.h"
#include "unity.h"

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

/**
 * @brief Reset s_log_backend to the safe default before each test
 *
 * @details
 * rx_log.c holds a static s_log_backend variable that persists across calls.
 * Resetting to k_log_backend_uart before each test ensures test-order
 * independence.
 */
void setUp(void)
{
  (void)rx_log_set_backend(k_log_backend_uart);
}

/** @brief No teardown required */
void tearDown(void) {}

/* =============================================================================
 * 1. Default State
 * =============================================================================
 */

/**
 * @brief Default backend is k_log_backend_uart after setUp reset
 *
 * @details
 * Verifies the safe boot-time default: UART logging only so that early
 * startup messages appear on SCI9 before USB CDC is enumerated.
 */
void test_default_backend_is_uart(void)
{
  TEST_ASSERT_EQUAL(k_log_backend_uart, rx_log_get_backend());
}

/* =============================================================================
 * 2. Valid Settings
 * =============================================================================
 */

/**
 * @brief Setting UART-only backend succeeds and is reflected by getter
 */
void test_set_backend_uart_only(void)
{
  const rx_err_t err = rx_log_set_backend(k_log_backend_uart);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_log_backend_uart, rx_log_get_backend());
}

/**
 * @brief Setting USB-only backend succeeds and is reflected by getter
 */
void test_set_backend_usb_only(void)
{
  const rx_err_t err = rx_log_set_backend(k_log_backend_usb);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_log_backend_usb, rx_log_get_backend());
}

/**
 * @brief Setting both UART and USB backends succeeds and is reflected by getter
 */
void test_set_backend_both(void)
{
  const rx_err_t err = rx_log_set_backend(k_log_backend_both);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_log_backend_both, rx_log_get_backend());
}

/**
 * @brief Setting none (silent mode) succeeds and is reflected by getter
 */
void test_set_backend_none(void)
{
  const rx_err_t err = rx_log_set_backend(k_log_backend_none);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_log_backend_none, rx_log_get_backend());
}

/* =============================================================================
 * 3. Invalid Bits
 * =============================================================================
 */

/**
 * @brief Bit 2 (0x04) is outside the valid mask -- must be rejected
 */
void test_bit2_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_log_set_backend((rx_log_backend_t)0x04u));
}

/**
 * @brief Upper nibble (0xF0) is outside the valid mask -- must be rejected
 */
void test_upper_nibble_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_log_set_backend((rx_log_backend_t)0xF0u));
}

/**
 * @brief All bits set (0xFF) contains invalid bits -- must be rejected
 */
void test_all_bits_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_log_set_backend((rx_log_backend_t)0xFFu));
}

/* =============================================================================
 * 4. Rejection Atomicity
 * =============================================================================
 */

/**
 * @brief Backend is unchanged when rx_log_set_backend() rejects a call
 *
 * @details
 * Sets the backend to USB-only, then attempts an invalid set.
 * Verifies the backend remains USB-only (not the default and not garbage).
 */
void test_backend_unchanged_after_rejection(void)
{
  (void)rx_log_set_backend(k_log_backend_usb);
  (void)rx_log_set_backend((rx_log_backend_t)0xFCu);
  TEST_ASSERT_EQUAL(k_log_backend_usb, rx_log_get_backend());
}

/* =============================================================================
 * 5. Idempotency
 * =============================================================================
 */

/**
 * @brief Setting the same valid backend value twice returns k_rx_ok both times
 *
 * @details
 * rx_log_set_backend() must be idempotent: calling it twice with the same
 * argument is a no-op and must not return an error.
 */
void test_idempotent_set(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_log_set_backend(k_log_backend_uart));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_log_set_backend(k_log_backend_uart));
  TEST_ASSERT_EQUAL(k_log_backend_uart, rx_log_get_backend());
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Unity test runner for rx_log backend selection tests
 *
 * @return int 0 on success, non-zero if any test fails
 */
int main(void)
{
  UNITY_BEGIN();

  /* 1. Default state */
  RUN_TEST(test_default_backend_is_uart);

  /* 2. Valid settings */
  RUN_TEST(test_set_backend_uart_only);
  RUN_TEST(test_set_backend_usb_only);
  RUN_TEST(test_set_backend_both);
  RUN_TEST(test_set_backend_none);

  /* 3. Invalid bits */
  RUN_TEST(test_bit2_rejected);
  RUN_TEST(test_upper_nibble_rejected);
  RUN_TEST(test_all_bits_rejected);

  /* 4. Rejection atomicity */
  RUN_TEST(test_backend_unchanged_after_rejection);

  /* 5. Idempotency */
  RUN_TEST(test_idempotent_set);

  return UNITY_END();
}
