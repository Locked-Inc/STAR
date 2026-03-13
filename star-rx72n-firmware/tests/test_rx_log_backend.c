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
 * Test Constants
 * =============================================================================
 */

/**
 * @enum invalid_backend_test_value_t
 * @brief Named constants for invalid rx_log_backend_t values used in negative tests
 *
 * @details
 * Replaces raw hex literals in test_bit2_rejected, test_upper_nibble_rejected,
 * and test_all_bits_rejected so each pattern's intent is self-documenting.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_invalid_backend_bit2         = 0x04u, /**< Bit 2 -- first undefined bit above the valid mask */
  k_invalid_backend_upper_nibble = 0xF0u, /**< Upper nibble -- all high bits set */
  k_invalid_backend_all_bits     = 0xFFu, /**< All bits set -- includes undefined bits */
} invalid_backend_test_value_t;

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
 *
 * @pre  setUp() has been called, resetting s_log_backend to k_log_backend_uart
 * @pre  No other test has modified the backend since setUp()
 * @post rx_log_get_backend() returns k_log_backend_uart
 * @post No global state modified beyond what Unity tearDown() resets
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
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_log_set_backend((rx_log_backend_t)k_invalid_backend_bit2));
}

/**
 * @brief Upper nibble (0xF0) is outside the valid mask -- must be rejected
 */
void test_upper_nibble_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_log_set_backend((rx_log_backend_t)k_invalid_backend_upper_nibble));
}

/**
 * @brief All bits set (0xFF) contains invalid bits -- must be rejected
 */
void test_all_bits_rejected(void)
{
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg,
                    rx_log_set_backend((rx_log_backend_t)k_invalid_backend_all_bits));
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
  (void)rx_log_set_backend(
    (rx_log_backend_t)(k_invalid_backend_upper_nibble | k_invalid_backend_bit2));
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
 * Internal Log Function Coverage Tests
 *
 * These tests call the internal_rx_log_* static-inline functions directly
 * to achieve coverage. The functions write to the UART mock (mock_rx_log.c)
 * and are not observable beyond "no crash / no hang".
 * =============================================================================
 */

/**
 * @brief Exercise internal_rx_log_error_u8 directly
 */
void test_internal_rx_log_error_u8(void)
{
  uint8_t val = 42;
  internal_rx_log_error_u8("TEST", "error u8", val);
  TEST_PASS(); /* No crash = pass */
}

/**
 * @brief Exercise internal_rx_log_error_str directly
 */
void test_internal_rx_log_error_str(void)
{
  const char* s = "hello";
  internal_rx_log_error_str("TEST", "error str", s, 5);
  TEST_PASS();
}

/**
 * @brief Exercise internal_rx_log_warn_u8 directly
 */
void test_internal_rx_log_warn_u8(void)
{
  uint8_t val = 7;
  internal_rx_log_warn_u8("TEST", "warn u8", val);
  TEST_PASS();
}

/**
 * @brief Exercise internal_rx_log_warn_u32 directly
 */
void test_internal_rx_log_warn_u32(void)
{
  uint32_t val = 12345;
  internal_rx_log_warn_u32("TEST", "warn u32", val);
  TEST_PASS();
}

/**
 * @brief Exercise internal_rx_log_warn_str directly
 */
void test_internal_rx_log_warn_str(void)
{
  const char* s = "world";
  internal_rx_log_warn_str("TEST", "warn str", s, 5);
  TEST_PASS();
}

/**
 * @brief Exercise internal_rx_log_info_u8 directly
 */
void test_internal_rx_log_info_u8(void)
{
  uint8_t val = 255;
  internal_rx_log_info_u8("TEST", "info u8", val);
  TEST_PASS();
}

/**
 * @brief Exercise internal_rx_log_debug_u32 directly
 */
void test_internal_rx_log_debug_u32(void)
{
  uint32_t val = 99;
  internal_rx_log_debug_u32("TEST", "debug u32", val);
  TEST_PASS();
}

/**
 * @brief Exercise internal_rx_log_debug_err directly
 */
void test_internal_rx_log_debug_err(void)
{
  rx_err_t err = k_rx_ok;
  internal_rx_log_debug_err("TEST", "debug err", err);
  TEST_PASS();
}

/**
 * @brief Exercise warn/info/debug no-value variants and warn_u16/info_u16/info_u32/debug_u8
 *
 * @details
 * The internal_rx_log_warn(), internal_rx_log_info(), internal_rx_log_debug() (no-value
 * variants), internal_rx_log_warn_u16(), internal_rx_log_info_u16(),
 * internal_rx_log_info_u32(), and internal_rx_log_debug_u8() functions are not called
 * in the existing section 6 tests. Calling them here covers the LOG_PUTS dispatch
 * branches in those functions for the default UART backend (set by setUp).
 *
 * @pre setUp() has reset backend to k_log_backend_uart
 * @post All named functions execute without crash
 */
void test_additional_log_variants_uart_backend(void)
{
  /* No-value warning/info/debug */
  internal_rx_log_warn("TEST", "warn msg");
  internal_rx_log_info("TEST", "info msg");
  internal_rx_log_debug("TEST", "debug msg");

  /* Numeric variants not covered in section 6 */
  internal_rx_log_warn_u16("TEST", "warn u16", (uint16_t)500u);
  internal_rx_log_info_u16("TEST", "info u16", (uint16_t)600u);
  internal_rx_log_info_u32("TEST", "info u32", 700u);
  internal_rx_log_debug_u8("TEST", "debug u8", 42u);

  TEST_PASS();
}

/**
 * @brief Exercise warn/info/debug no-value and extra variants with USB-only backend
 *
 * @details
 * Covers the USB branch (true) and UART branch (false) for the same set of
 * functions exercised in test_additional_log_variants_uart_backend.
 *
 * @pre setUp() has reset backend to k_log_backend_uart (changed to USB here)
 * @post All named functions execute without crash
 */
void test_additional_log_variants_usb_backend(void)
{
  (void)rx_log_set_backend(k_log_backend_usb);

  internal_rx_log_warn("TEST", "warn msg");
  internal_rx_log_info("TEST", "info msg");
  internal_rx_log_debug("TEST", "debug msg");
  internal_rx_log_warn_u16("TEST", "warn u16", (uint16_t)500u);
  internal_rx_log_info_u16("TEST", "info u16", (uint16_t)600u);
  internal_rx_log_info_u32("TEST", "info u32", 700u);
  internal_rx_log_debug_u8("TEST", "debug u8", 42u);

  TEST_PASS();
}

/**
 * @brief Exercise null guards in internal_log_header, uart_debug_puts, uart_debug_puthex
 *
 * @details
 * Covers:
 * - uart_debug_puts(NULL) -- null guard branch (line 315 in rx_log.h)
 * - uart_debug_puthex with too-small and too-large digit counts (lines 407,410)
 * - internal_log_header with null level_str and null tag (line 896)
 * - internal_rx_log_warn null message guard (line 1208)
 * - internal_rx_log_info null message guard (line 1329)
 * - internal_rx_log_debug null message guard (line 1450)
 *
 * @pre setUp() has reset backend to k_log_backend_uart (default)
 * @post No crash; null inputs are silently rejected by the guards
 */
void test_log_null_and_edge_guards(void)
{
  /* uart_debug_puts NULL branch */
  uart_debug_puts(nullptr);

  /* uart_debug_puthex out-of-range digit counts (min=1, max=8) */
  uart_debug_puthex(0xABu, 0u); /* digits < min -- clamped up */
  uart_debug_puthex(0xABu, 9u); /* digits > max -- clamped down */

  /* internal_log_header null guards */
  internal_log_header(nullptr, "TAG");
  internal_log_header("ERROR", nullptr);

  /* No-value error/warn/info/debug null message guards */
  internal_rx_log_error("TEST", nullptr);
  internal_rx_log_warn("TEST", nullptr);
  internal_rx_log_info("TEST", nullptr);
  internal_rx_log_debug("TEST", nullptr);

  /* internal_rx_log_error_str null guards */
  internal_rx_log_error_str("TEST", nullptr, "val", 3u);
  internal_rx_log_error_str("TEST", "msg", nullptr, 3u);

  /* internal_rx_log_warn_str null guards */
  internal_rx_log_warn_str("TEST", nullptr, "val", 3u);
  internal_rx_log_warn_str("TEST", "msg", nullptr, 3u);

  TEST_PASS();
}

/* =============================================================================
 * 7. Backend dispatch branch coverage
 *
 * These tests activate each backend combination and call internal log
 * functions to exercise all four branch outcomes in LOG_PUTC/LOG_PUTS:
 *   UART=false/USB=false (none)
 *   UART=true/USB=false  (uart only) -- covered by setUp default in section 6
 *   UART=false/USB=true  (usb only)
 *   UART=true/USB=true   (both)
 * =============================================================================
 */

/**
 * @brief Exercise error_u16, error_u32, error_err with USB-only backend
 *
 * @details
 * Covers the missing branches at rx_log.h lines 1005-1008, 1017-1020, 1041-1044.
 * Those functions are only exercised with UART backend in the existing tests.
 * Setting USB-only here activates the usb-if=true / uart-if=false branches inside
 * the LOG_PUTS, LOG_PUTINT, and LOG_PUTHEX macros for these three variants.
 *
 * @pre  rx_log_set_backend() accepts k_log_backend_usb
 * @post All three functions execute without crash
 */
void test_error_u16_u32_err_usb_backend(void)
{
  (void)rx_log_set_backend(k_log_backend_usb);
  internal_rx_log_error_u16("T", "eu16", (uint16_t)1000u);
  internal_rx_log_error_u32("T", "eu32", 2000u);
  internal_rx_log_error_err("T", "eerr", k_rx_ok);
  TEST_PASS();
}

/**
 * @brief Exercise error_u16, error_u32, error_err with both backends
 *
 * @details
 * Further covers the LOG_PUTS/PUTINT/PUTHEX branches with UART=true and USB=true
 * for the three error-variant functions not previously exercised with both backends.
 *
 * @pre  rx_log_set_backend() accepts k_log_backend_both
 * @post All three functions execute without crash
 */
void test_error_u16_u32_err_both_backends(void)
{
  (void)rx_log_set_backend(k_log_backend_both);
  internal_rx_log_error_u16("T", "eu16", (uint16_t)500u);
  internal_rx_log_error_u32("T", "eu32", 600u);
  internal_rx_log_error_err("T", "eerr", k_rx_err_null_ptr);
  TEST_PASS();
}

/**
 * @brief Exercise str-log null terminator early exit branch
 *
 * @details
 * Covers the str_value[i] == '\\0' TRUE branch in the for-loops at
 * rx_log.h lines 1173 (error_str) and 1294 (warn_str).
 * Passing a string shorter than len causes the loop to hit '\\0' before
 * i >= len, exercising the second operand of the || condition.
 *
 * @pre  setUp() has reset backend to k_log_backend_uart
 * @post Functions execute without crash
 */
void test_str_log_null_terminator_early_exit(void)
{
  /* "hi" is 2 chars; len=10 forces the loop to hit '\\0' at index 2 */
  internal_rx_log_error_str("T", "early exit", "hi", 10u);
  internal_rx_log_warn_str("T", "early exit", "hi", 10u);
  TEST_PASS();
}

/**
 * @brief Exercise str-log loop exhaustion path (i >= k_log_str_max_len)
 *
 * @details
 * Covers the for-loop exit condition at rx_log.h lines 1192 and 1313.
 * These are the outer for-loop conditions becoming FALSE (i reaches
 * k_log_str_max_len=256 without an early break). Achieved by passing a
 * string of exactly 256 'A' characters with len >= 256, so neither
 * i >= len nor str_value[i] == '\\0' triggers before the loop ends.
 *
 * @pre  setUp() has reset backend to k_log_backend_uart
 * @post Functions execute without crash; exactly 256 characters output
 */
void test_str_log_loop_exhaustion(void)
{
  /* Fill a 256-byte buffer with 'A' -- no null terminator within bound */
  enum : uint32_t {
    k_max_len_val = 256u, /**< k_log_str_max_len value */
  };
  static char s_long_str[k_max_len_val + 1u]; /* +1 for safety null outside bound */
  for (uint32_t i = 0; i < k_max_len_val; i++) {
    s_long_str[i] = 'A';
  }
  s_long_str[k_max_len_val] = '\0'; /* null beyond the bounded region */

  /* len > 256 so i >= len never triggers; loop runs all 256 iterations */
  internal_rx_log_error_str("T", "full loop", s_long_str, k_max_len_val + 1u);
  internal_rx_log_warn_str("T", "full loop", s_long_str, k_max_len_val + 1u);
  TEST_PASS();
}

/**
 * @brief Log functions with k_log_backend_none -- both dispatch branches skipped
 *
 * @details
 * Sets backend to none before calling every internal_rx_log_* variant.
 * Exercises the false path of both LOG_PUTS/LOG_PUTC uart and usb checks.
 *
 * @pre  rx_log_set_backend() accepts k_log_backend_none (validated by section 2)
 * @pre  Internal log functions are callable with any active backend
 * @post No bytes written to any backend
 * @post Backend restored to k_log_backend_uart by next setUp() call
 */
void test_log_dispatch_none_backend(void)
{
  (void)rx_log_set_backend(k_log_backend_none);
  internal_rx_log_error_u8("T", "none u8", 1u);
  internal_rx_log_error_str("T", "none str", "x", 1u);
  internal_rx_log_warn_u8("T", "none wu8", 2u);
  internal_rx_log_warn_u32("T", "none wu32", 3u);
  internal_rx_log_warn_str("T", "none wstr", "y", 1u);
  internal_rx_log_info_u8("T", "none iu8", 4u);
  internal_rx_log_debug_u32("T", "none du32", 5u);
  internal_rx_log_debug_err("T", "none derr", k_rx_ok);
  TEST_PASS();
}

/**
 * @brief Log functions with k_log_backend_usb -- USB branch true, UART branch false
 *
 * @details
 * Sets backend to USB-only and calls every internal_rx_log_* variant.
 * Exercises: LOG_PUTC uart-if=false, LOG_PUTC usb-if=true,
 *            LOG_PUTS uart-if=false, LOG_PUTS usb-if=true,
 *            LOG_PUTINT uart-if=false, LOG_PUTINT usb-if=true, etc.
 *
 * @pre  rx_log_set_backend() accepts k_log_backend_usb (validated by section 2)
 * @pre  rx_log_usb_* stubs from mock_rx_log.c are no-ops (safe to call)
 * @post No bytes written (stubs discard output)
 * @post Backend restored to k_log_backend_uart by next setUp() call
 */
void test_log_dispatch_usb_only_backend(void)
{
  (void)rx_log_set_backend(k_log_backend_usb);
  internal_rx_log_error_u8("T", "usb u8", 10u);
  internal_rx_log_error_str("T", "usb str", "ab", 2u);
  internal_rx_log_warn_u8("T", "usb wu8", 20u);
  internal_rx_log_warn_u32("T", "usb wu32", 30u);
  internal_rx_log_warn_str("T", "usb wstr", "cd", 2u);
  internal_rx_log_info_u8("T", "usb iu8", 40u);
  internal_rx_log_debug_u32("T", "usb du32", 50u);
  internal_rx_log_debug_err("T", "usb derr", k_rx_ok);
  TEST_PASS();
}

/**
 * @brief Log functions with k_log_backend_both -- both dispatch branches true
 *
 * @details
 * Sets backend to both UART and USB and calls every internal_rx_log_* variant.
 * Exercises: LOG_PUTC uart-if=true, LOG_PUTC usb-if=true,
 *            LOG_PUTS uart-if=true, LOG_PUTS usb-if=true, etc.
 *
 * @pre  rx_log_set_backend() accepts k_log_backend_both (validated by section 2)
 * @pre  uart_debug_* and rx_log_usb_* stubs from mock_rx_log.c are no-ops
 * @post No bytes written (stubs discard output)
 * @post Backend restored to k_log_backend_uart by next setUp() call
 */
void test_log_dispatch_both_backends(void)
{
  (void)rx_log_set_backend(k_log_backend_both);
  internal_rx_log_error_u8("T", "both u8", 11u);
  internal_rx_log_error_str("T", "both str", "ef", 2u);
  internal_rx_log_warn_u8("T", "both wu8", 21u);
  internal_rx_log_warn_u32("T", "both wu32", 31u);
  internal_rx_log_warn_str("T", "both wstr", "gh", 2u);
  internal_rx_log_info_u8("T", "both iu8", 41u);
  internal_rx_log_debug_u32("T", "both du32", 51u);
  internal_rx_log_debug_err("T", "both derr", k_rx_ok);
  TEST_PASS();
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

  /* 6. Internal log function coverage */
  RUN_TEST(test_internal_rx_log_error_u8);
  RUN_TEST(test_internal_rx_log_error_str);
  RUN_TEST(test_internal_rx_log_warn_u8);
  RUN_TEST(test_internal_rx_log_warn_u32);
  RUN_TEST(test_internal_rx_log_warn_str);
  RUN_TEST(test_internal_rx_log_info_u8);
  RUN_TEST(test_internal_rx_log_debug_u32);
  RUN_TEST(test_internal_rx_log_debug_err);

  /* 7. Backend dispatch branch coverage */
  RUN_TEST(test_error_u16_u32_err_usb_backend);
  RUN_TEST(test_error_u16_u32_err_both_backends);
  RUN_TEST(test_str_log_null_terminator_early_exit);
  RUN_TEST(test_str_log_loop_exhaustion);
  RUN_TEST(test_log_dispatch_none_backend);
  RUN_TEST(test_log_dispatch_usb_only_backend);
  RUN_TEST(test_log_dispatch_both_backends);

  /* 8. Missing variant and null/edge guard coverage */
  RUN_TEST(test_additional_log_variants_uart_backend);
  RUN_TEST(test_additional_log_variants_usb_backend);
  RUN_TEST(test_log_null_and_edge_guards);

  return UNITY_END();
}
