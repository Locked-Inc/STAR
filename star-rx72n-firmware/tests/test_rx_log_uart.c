/**
 * @file test_rx_log_uart.c
 * @brief Unit tests for the UART-framed log backend (rx_log_uart.c)
 *
 * @details
 * Exercises every public function and branch of rx_log_uart.c so that the
 * 100%-coverage CI gate on libs/ stays green. The backend is a producer /
 * single-consumer ring buffer; the tests drive the producer API (putc, puts,
 * putint, putuint, puthex), then drain back out via rx_log_uart_drain() and
 * assert the contents round-trip exactly.
 *
 * Branches covered:
 * - ring buffer overflow drop path (producer)
 * - wrap-around split drain (consumer gets pre-wrap chunk first)
 * - NULL-pointer guards (puts, get_stats, drain)
 * - max_len == 0 invalid-arg guard on drain
 * - negative int formatting, zero formatting, hex digit clamping
 *
 * @author Locked, Inc.
 * @date 2026-04-17
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <string.h>

#include "rx_log_uart.h"
#include "unity.h"

typedef enum : uint32_t {
  k_test_small_buf   = 64U,
  k_test_large_buf   = 4096U,
  k_test_drain_chunk = 256U,
} test_rx_log_uart_sizes_t;

typedef enum : uint32_t {
  k_test_putint_positive       = 42U,
  k_test_putint_negative_abs   = 123U,
  k_test_int32_max             = 2147483647U,
  k_test_uint32_max            = 4294967295U,
  k_test_puthex_small          = 0xAU,
  k_test_puthex_large          = 0xDEADBEEFU,
  k_test_puthex_padded_value   = 0x7U,
  k_test_digits_nine_clamped   = 9U,
  k_test_digits_four           = 4U,
  k_test_sentinel_out_len      = 42U,
  k_test_drain_max             = 16U,
  k_test_prefill_trailing      = 16U,
  k_test_wrap_x_count          = 32U,
  k_test_wrap_y_count          = 32U,
  k_test_wrap_first_chunk_len  = 16U,
  k_test_wrap_second_chunk_len = 48U,
} test_rx_log_uart_values_t;

typedef enum : uint8_t {
  k_test_fill_byte = (uint8_t)'X',
} test_rx_log_uart_bytes_t;

void setUp(void)
{
  rx_log_uart_test_reset_state();
}

void tearDown(void) {}

/* =============================================================================
 * Basic producer coverage
 * ============================================================================= */

void test_putc_then_drain_round_trip(void)
{
  rx_log_uart_putc('A');
  rx_log_uart_putc('B');
  rx_log_uart_putc('C');

  TEST_ASSERT_EQUAL_UINT32(3U, rx_log_uart_pending_len());

  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_log_uart_drain(out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL_UINT32(3U, out_len);
  TEST_ASSERT_EQUAL_MEMORY("ABC", out, 3U);
  TEST_ASSERT_EQUAL_UINT32(0U, rx_log_uart_pending_len());
}

void test_puts_appends_full_string(void)
{
  rx_log_uart_puts("hello");
  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_log_uart_drain(out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL_UINT32(5U, out_len);
  TEST_ASSERT_EQUAL_MEMORY("hello", out, 5U);
}

void test_puts_null_is_silent(void)
{
  rx_log_uart_puts(NULL);
  TEST_ASSERT_EQUAL_UINT32(0U, rx_log_uart_pending_len());
}

void test_putint_positive(void)
{
  rx_log_uart_putint((int32_t)k_test_putint_positive);
  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  (void)rx_log_uart_drain(out, sizeof(out), &out_len);
  TEST_ASSERT_EQUAL_UINT32(2U, out_len);
  TEST_ASSERT_EQUAL_MEMORY("42", out, 2U);
}

void test_putint_negative(void)
{
  rx_log_uart_putint(-(int32_t)k_test_putint_negative_abs);
  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  (void)rx_log_uart_drain(out, sizeof(out), &out_len);
  TEST_ASSERT_EQUAL_UINT32(4U, out_len);
  TEST_ASSERT_EQUAL_MEMORY("-123", out, 4U);
}

void test_putint_zero(void)
{
  rx_log_uart_putint(0);
  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  (void)rx_log_uart_drain(out, sizeof(out), &out_len);
  TEST_ASSERT_EQUAL_UINT32(1U, out_len);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)'0', out[0]);
}

void test_putint_int_min(void)
{
  /* INT32_MIN = -2147483648; exercises the |INT32_MIN| edge case in the
   * unsigned-magnitude conversion. Computed as -INT32_MAX - 1 so the literal
   * stays within the signed int32 range the lint checks enforce. */
  rx_log_uart_putint(-(int32_t)k_test_int32_max - 1);
  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  (void)rx_log_uart_drain(out, sizeof(out), &out_len);
  TEST_ASSERT_EQUAL_UINT32(11U, out_len);
  TEST_ASSERT_EQUAL_MEMORY("-2147483648", out, 11U);
}

void test_putuint_zero(void)
{
  rx_log_uart_putuint(0U);
  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  (void)rx_log_uart_drain(out, sizeof(out), &out_len);
  TEST_ASSERT_EQUAL_UINT32(1U, out_len);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)'0', out[0]);
}

void test_putuint_max(void)
{
  rx_log_uart_putuint(k_test_uint32_max);
  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  (void)rx_log_uart_drain(out, sizeof(out), &out_len);
  TEST_ASSERT_EQUAL_UINT32(10U, out_len);
  TEST_ASSERT_EQUAL_MEMORY("4294967295", out, 10U);
}

void test_puthex_clamps_zero_digits_to_one(void)
{
  rx_log_uart_puthex(k_test_puthex_small, 0U);
  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  (void)rx_log_uart_drain(out, sizeof(out), &out_len);
  TEST_ASSERT_EQUAL_UINT32(1U, out_len);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)'A', out[0]);
}

void test_puthex_clamps_nine_digits_to_eight(void)
{
  rx_log_uart_puthex(k_test_puthex_large, (uint8_t)k_test_digits_nine_clamped);
  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  (void)rx_log_uart_drain(out, sizeof(out), &out_len);
  TEST_ASSERT_EQUAL_UINT32(8U, out_len);
  TEST_ASSERT_EQUAL_MEMORY("DEADBEEF", out, 8U);
}

void test_puthex_pads_with_zeros(void)
{
  rx_log_uart_puthex(k_test_puthex_padded_value, (uint8_t)k_test_digits_four);
  uint8_t  out[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  (void)rx_log_uart_drain(out, sizeof(out), &out_len);
  TEST_ASSERT_EQUAL_UINT32(4U, out_len);
  TEST_ASSERT_EQUAL_MEMORY("0007", out, 4U);
}

/* =============================================================================
 * Ring buffer mechanics
 * ============================================================================= */

void test_ring_overflow_drops_and_counts(void)
{
  /* Fill the ring exactly, then push one more byte that must be dropped. */
  uint8_t filler[k_test_large_buf] = {0};
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  (void)memset(filler, k_test_fill_byte, sizeof(filler));
  for (uint32_t i = 0U; i < (uint32_t)k_rx_log_uart_ring_size; ++i) {
    rx_log_uart_putc((char)filler[i]);
  }
  TEST_ASSERT_EQUAL_UINT32((uint32_t)k_rx_log_uart_ring_size, rx_log_uart_pending_len());

  rx_log_uart_putc('!'); /* should be dropped */

  rx_log_uart_stats_t stats = {0, 0, 0, 0};
  rx_log_uart_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(1U, stats.dropped_bytes);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)k_rx_log_uart_ring_size, stats.high_water);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)k_rx_log_uart_ring_size, stats.total_bytes);
}

void test_drain_returns_only_pre_wrap_portion_when_split(void)
{
  /* Advance the tail far enough into the ring that a subsequent write wraps. */
  const uint32_t advance = (uint32_t)k_rx_log_uart_ring_size - k_test_prefill_trailing;
  for (uint32_t i = 0U; i < advance; ++i) {
    rx_log_uart_putc('a');
  }

  uint8_t  throwaway[k_test_large_buf] = {0};
  uint32_t discard                     = 0U;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_log_uart_drain(throwaway, sizeof(throwaway), &discard));
  TEST_ASSERT_EQUAL_UINT32(advance, discard);

  /* Now head=tail=advance. Write k_test_wrap_x_count bytes of 'X' followed by
   * k_test_wrap_y_count of 'Y'. This wraps the head. The first drain should
   * return up to the end-of-ring only (k_test_wrap_first_chunk_len bytes of
   * 'X'); a second drain picks up the remaining 'X' plus all 'Y' from the
   * start of the ring. */
  for (uint32_t i = 0U; i < k_test_wrap_x_count; ++i) {
    rx_log_uart_putc('X');
  }
  for (uint32_t i = 0U; i < k_test_wrap_y_count; ++i) {
    rx_log_uart_putc('Y');
  }

  uint8_t  chunk1[k_test_drain_chunk] = {0};
  uint32_t len1                       = 0U;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_log_uart_drain(chunk1, sizeof(chunk1), &len1));
  TEST_ASSERT_EQUAL_UINT32(k_test_wrap_first_chunk_len, len1);
  for (uint32_t i = 0U; i < len1; ++i) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'X', chunk1[i]);
  }

  uint8_t  chunk2[k_test_drain_chunk] = {0};
  uint32_t len2                       = 0U;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_log_uart_drain(chunk2, sizeof(chunk2), &len2));
  TEST_ASSERT_EQUAL_UINT32(k_test_wrap_second_chunk_len, len2);
  for (uint32_t i = 0U; i < k_test_wrap_first_chunk_len; ++i) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'X', chunk2[i]);
  }
  for (uint32_t i = k_test_wrap_first_chunk_len; i < k_test_wrap_second_chunk_len; ++i) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'Y', chunk2[i]);
  }
  TEST_ASSERT_EQUAL_UINT32(0U, rx_log_uart_pending_len());
}

/* =============================================================================
 * Guard clauses
 * ============================================================================= */

void test_drain_null_buf_is_invalid_arg(void)
{
  uint32_t out_len = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_log_uart_drain(NULL, k_test_drain_max, &out_len));
}

void test_drain_null_out_len_is_invalid_arg(void)
{
  uint8_t buf[k_test_small_buf] = {0};
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_log_uart_drain(buf, k_test_drain_max, NULL));
}

void test_drain_zero_max_len_is_invalid_arg(void)
{
  uint8_t  buf[k_test_small_buf] = {0};
  uint32_t out_len               = 0U;
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_log_uart_drain(buf, 0U, &out_len));
}

void test_get_stats_null_is_noop(void)
{
  /* Must not crash; previous state preserved. */
  rx_log_uart_get_stats(NULL);
  rx_log_uart_putc('Z');
  rx_log_uart_stats_t stats = {0, 0, 0, 0};
  rx_log_uart_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(1U, stats.total_bytes);
}

void test_drain_on_empty_ring_returns_zero(void)
{
  uint8_t  buf[k_test_small_buf] = {0};
  uint32_t out_len               = k_test_sentinel_out_len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_log_uart_drain(buf, sizeof(buf), &out_len));
  TEST_ASSERT_EQUAL_UINT32(0U, out_len);
}

/* Hit the `s_count < max_len` FALSE branch of the ternary at the top of
 * rx_log_uart_drain by enqueueing more bytes than the caller's buffer can
 * hold in one call. The ring-overflow and wrap-around tests both happen to
 * drain with max_len > pending bytes, so they only exercise the TRUE branch. */
void test_drain_respects_max_len_cap(void)
{
  enum : uint32_t { k_drain_cap_pending = 100U, k_drain_cap_max_len = 32U };
  for (uint32_t i = 0U; i < k_drain_cap_pending; ++i) {
    rx_log_uart_putc('P');
  }
  TEST_ASSERT_EQUAL_UINT32(k_drain_cap_pending, rx_log_uart_pending_len());

  uint8_t  chunk[k_drain_cap_max_len] = {0};
  uint32_t len                        = 0U;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_log_uart_drain(chunk, k_drain_cap_max_len, &len));
  TEST_ASSERT_EQUAL_UINT32(k_drain_cap_max_len, len);
  TEST_ASSERT_EQUAL_UINT32(k_drain_cap_pending - k_drain_cap_max_len, rx_log_uart_pending_len());
}

/* =============================================================================
 * Unity runner
 * ============================================================================= */

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_putc_then_drain_round_trip);
  RUN_TEST(test_puts_appends_full_string);
  RUN_TEST(test_puts_null_is_silent);
  RUN_TEST(test_putint_positive);
  RUN_TEST(test_putint_negative);
  RUN_TEST(test_putint_zero);
  RUN_TEST(test_putint_int_min);
  RUN_TEST(test_putuint_zero);
  RUN_TEST(test_putuint_max);
  RUN_TEST(test_puthex_clamps_zero_digits_to_one);
  RUN_TEST(test_puthex_clamps_nine_digits_to_eight);
  RUN_TEST(test_puthex_pads_with_zeros);
  RUN_TEST(test_ring_overflow_drops_and_counts);
  RUN_TEST(test_drain_returns_only_pre_wrap_portion_when_split);
  RUN_TEST(test_drain_null_buf_is_invalid_arg);
  RUN_TEST(test_drain_null_out_len_is_invalid_arg);
  RUN_TEST(test_drain_zero_max_len_is_invalid_arg);
  RUN_TEST(test_get_stats_null_is_noop);
  RUN_TEST(test_drain_on_empty_ring_returns_zero);
  RUN_TEST(test_drain_respects_max_len_cap);
  return UNITY_END();
}
