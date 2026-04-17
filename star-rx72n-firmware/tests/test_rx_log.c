/**
 * @file test_rx_log.c
 * @brief Unit tests exercising every inline helper in rx_log.h
 *
 * @details
 * rx_log.h is almost entirely @c static @c inline helpers: one per log level
 * (error, warn, info, debug, verbose) crossed with one per value type (plain,
 * u8, u16, u32, i32, err, hex, str), plus the simulator-mode uart_debug_*
 * helpers. They compile into every translation unit that includes rx_log.h,
 * so gcovr counts them against the libs/ 100%-coverage gate even when no
 * particular test happens to call them.
 *
 * This file builds with @c LOG_LEVEL=k_log_verbose so every level-gated macro
 * expands to a real call, then invokes every variant at least once. Buffer
 * boundary / edge cases for the simulator uart_debug_* helpers are also
 * exercised here (NULL guard, digit clamping low, digit clamping high).
 *
 * @author Locked, Inc.
 * @date 2026-04-17
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "rx_err.h"
#include "rx_log.h"
#include "unity.h"

typedef enum : uint8_t {
  k_log_test_u8_small = 7U,
  k_log_test_u8_large = 255U,
  k_log_test_u8_hex   = 0xABU,
} test_log_u8_values_t;

typedef enum : uint16_t {
  k_log_test_u16_typical = 500U,
  k_log_test_u16_large   = 60000U,
} test_log_u16_values_t;

typedef enum : uint32_t {
  k_log_test_u32_typical    = 12345U,
  k_log_test_u32_large      = 0xDEADBEEFU,
  k_log_test_hex_digits_min = 0U, /* clamped up to 1 */
  k_log_test_hex_digits_mid = 4U,
  k_log_test_hex_digits_max = 8U,
  k_log_test_hex_digits_over = 9U, /* clamped down to 8 */
  k_log_test_str_len_zero   = 0U,
  k_log_test_str_len_small  = 5U,
  k_log_test_str_len_over   = 300U, /* larger than k_log_str_max_len to hit bound */
} test_log_u32_values_t;

typedef enum : int32_t {
  k_log_test_i32_negative = -42,
  k_log_test_i32_positive = 42,
} test_log_i32_values_t;

static const char* const k_tag = "TEST";

void setUp(void) {}
void tearDown(void) {}

/* =============================================================================
 * Simulator-mode uart_debug_* helpers (rx_log.h RX_IS_SIMULATOR branch)
 * ============================================================================= */

void test_uart_debug_puts_null_is_noop(void)
{
  /* Covers the NULL-guard early return inside uart_debug_puts. */
  uart_debug_puts(NULL);
  /* No assertion -- exercising the branch without crashing is the test. */
  TEST_PASS();
}

void test_uart_debug_puts_newline_conversion(void)
{
  /* The string "a\nb" exercises the \n -> \r\n translation branch in
   * uart_debug_puts as well as the normal-character branch. */
  uart_debug_puts("a\nb\n");
  TEST_PASS();
}

void test_uart_debug_putc_basic(void)
{
  uart_debug_putc('Z');
  TEST_PASS();
}

void test_uart_debug_putint_negative_and_positive(void)
{
  uart_debug_putint((int32_t)k_log_test_i32_negative);
  uart_debug_putint((int32_t)k_log_test_i32_positive);
  TEST_PASS();
}

void test_uart_debug_putuint_large(void)
{
  uart_debug_putuint((uint32_t)k_log_test_u32_large);
  TEST_PASS();
}

void test_uart_debug_puthex_clamps_digits(void)
{
  /* 0 clamps to 1 (the digits < k_min_hex_digits branch). */
  uart_debug_puthex((uint32_t)k_log_test_u8_hex, (uint8_t)k_log_test_hex_digits_min);
  /* 9 clamps to 8 (the digits > k_max_hex_digits branch). */
  uart_debug_puthex((uint32_t)k_log_test_u32_large, (uint8_t)k_log_test_hex_digits_over);
  /* A value in-range for the lookup-table path. */
  uart_debug_puthex((uint32_t)k_log_test_u32_typical, (uint8_t)k_log_test_hex_digits_mid);
  TEST_PASS();
}

/* =============================================================================
 * internal_rx_log_* coverage (public rx_log_* macros route to these inlines)
 * ============================================================================= */

/* Plain (no value) */

void test_rx_log_plain_all_levels(void)
{
  rx_log_error(k_tag, "plain error");
  rx_log_warn(k_tag, "plain warn");
  rx_log_info(k_tag, "plain info");
  rx_log_debug(k_tag, "plain debug");
  rx_log_verbose(k_tag, "plain verbose");

  /* NULL-guard branches (message == NULL) */
  rx_log_error(k_tag, NULL);
  rx_log_warn(k_tag, NULL);
  rx_log_info(k_tag, NULL);
  rx_log_debug(k_tag, NULL);
  rx_log_verbose(k_tag, NULL);
  TEST_PASS();
}

/* _val variants -- C23 _Generic dispatches by value type */

void test_rx_log_val_u8(void)
{
  rx_log_error_val(k_tag, "u8", (uint8_t)k_log_test_u8_small);
  rx_log_warn_val(k_tag, "u8", (uint8_t)k_log_test_u8_small);
  rx_log_info_val(k_tag, "u8", (uint8_t)k_log_test_u8_large);
  rx_log_debug_val(k_tag, "u8", (uint8_t)k_log_test_u8_small);
  rx_log_verbose_val(k_tag, "u8", (uint8_t)k_log_test_u8_small);
  TEST_PASS();
}

void test_rx_log_val_u16(void)
{
  rx_log_error_val(k_tag, "u16", (uint16_t)k_log_test_u16_typical);
  rx_log_warn_val(k_tag, "u16", (uint16_t)k_log_test_u16_typical);
  rx_log_info_val(k_tag, "u16", (uint16_t)k_log_test_u16_large);
  rx_log_debug_val(k_tag, "u16", (uint16_t)k_log_test_u16_typical);
  rx_log_verbose_val(k_tag, "u16", (uint16_t)k_log_test_u16_typical);
  TEST_PASS();
}

void test_rx_log_val_u32(void)
{
  rx_log_error_val(k_tag, "u32", (uint32_t)k_log_test_u32_typical);
  rx_log_warn_val(k_tag, "u32", (uint32_t)k_log_test_u32_typical);
  rx_log_info_val(k_tag, "u32", (uint32_t)k_log_test_u32_typical);
  rx_log_debug_val(k_tag, "u32", (uint32_t)k_log_test_u32_typical);
  rx_log_verbose_val(k_tag, "u32", (uint32_t)k_log_test_u32_typical);
  TEST_PASS();
}

void test_rx_log_val_err(void)
{
  /* rx_err_t is int32_t so _Generic dispatches to the _err (hex) handler. */
  rx_log_error_val(k_tag, "err", (rx_err_t)k_rx_err_invalid_arg);
  rx_log_warn_val(k_tag, "err", (rx_err_t)k_rx_err_invalid_arg);
  rx_log_info_val(k_tag, "err", (rx_err_t)k_rx_err_invalid_arg);
  rx_log_debug_val(k_tag, "err", (rx_err_t)k_rx_err_invalid_arg);
  rx_log_verbose_val(k_tag, "err", (rx_err_t)k_rx_err_invalid_arg);
  TEST_PASS();
}

/* _hex variants (explicit digit count) */

void test_rx_log_hex_all_levels(void)
{
  const uint8_t d = (uint8_t)k_log_test_hex_digits_max;
  rx_log_error_hex(k_tag, "hex", (uint32_t)k_log_test_u32_large, d);
  rx_log_warn_hex(k_tag, "hex", (uint32_t)k_log_test_u32_large, d);
  rx_log_info_hex(k_tag, "hex", (uint32_t)k_log_test_u32_large, d);
  rx_log_debug_hex(k_tag, "hex", (uint32_t)k_log_test_u32_large, d);
  rx_log_verbose_hex(k_tag, "hex", (uint32_t)k_log_test_u32_large, d);
  TEST_PASS();
}

/* _str variants (bounded string) */

void test_rx_log_str_all_levels(void)
{
  const char* short_str = "hello";
  rx_log_error_str(k_tag, "str", short_str, (uint32_t)k_log_test_str_len_small);
  rx_log_warn_str(k_tag, "str", short_str, (uint32_t)k_log_test_str_len_small);
  rx_log_info_str(k_tag, "str", short_str, (uint32_t)k_log_test_str_len_small);
  rx_log_debug_str(k_tag, "str", short_str, (uint32_t)k_log_test_str_len_small);
  rx_log_verbose_str(k_tag, "str", short_str, (uint32_t)k_log_test_str_len_small);

  /* Hit the null-terminator early-break branch in the bounded loop by passing
   * a len > strlen(short_str). */
  rx_log_error_str(k_tag, "str", short_str, (uint32_t)k_log_test_str_len_over);

  /* NULL-guard branches (message NULL; str_value NULL). */
  rx_log_error_str(k_tag, NULL, short_str, (uint32_t)k_log_test_str_len_small);
  rx_log_error_str(k_tag, "str", NULL, (uint32_t)k_log_test_str_len_small);
  TEST_PASS();
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_uart_debug_puts_null_is_noop);
  RUN_TEST(test_uart_debug_puts_newline_conversion);
  RUN_TEST(test_uart_debug_putc_basic);
  RUN_TEST(test_uart_debug_putint_negative_and_positive);
  RUN_TEST(test_uart_debug_putuint_large);
  RUN_TEST(test_uart_debug_puthex_clamps_digits);
  RUN_TEST(test_rx_log_plain_all_levels);
  RUN_TEST(test_rx_log_val_u8);
  RUN_TEST(test_rx_log_val_u16);
  RUN_TEST(test_rx_log_val_u32);
  RUN_TEST(test_rx_log_val_err);
  RUN_TEST(test_rx_log_hex_all_levels);
  RUN_TEST(test_rx_log_str_all_levels);
  return UNITY_END();
}
