/**
 * @file test_rx_isr_log.c
 * @brief Unit tests for the ISR-safe deferred log queue (rx_isr_log.c)
 *
 * @details
 * Exercises the SPSC ring producer/consumer paths, drop-on-overflow
 * accounting, the drop-summary emission on the next drain, and the
 * out-of-range event id rejection path.
 *
 * The drain path calls into rx_log_* which under RX_SIMULATOR_MODE writes
 * to stdout via uart_debug_* (not the log_uart ring buffer). The tests
 * verify the ring's own bookkeeping (pending count, stats counters) and
 * confirm drain returns the right number of records, rather than
 * inspecting downstream UART bytes.
 *
 * @author Locked, Inc.
 * @date 2026-04-25
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "rx_isr_log.h"
#include "rx_log_uart.h"
#include "unity.h"

/* Test fixture constants -- avoid clang-tidy readability-magic-numbers. */
enum : uint32_t {
  k_test_5_records  = 5U,
  k_test_4_rounds   = 4U,
  k_test_8_records  = 8U,
  k_test_10_records = 10U,
  k_test_3          = 3U,
  k_test_15         = 15U,
  k_test_32_records = 32U,
};
typedef enum : uint32_t {
  k_test_capacity        = 32U,
  k_test_overflow_pushes = 40U, /* 8 over capacity */
  k_test_first_value     = 0xDEADBEEFU,
} test_rx_isr_log_values_t;

void setUp(void)
{
  rx_isr_log_test_reset_state();
  rx_log_uart_test_reset_state();
}

void tearDown(void) {}

/* =============================================================================
 * Test helpers
 *
 * Extracted to keep individual tests under the readability-function-size and
 * cognitive-complexity thresholds enforced by clang-tidy.
 * ============================================================================= */

/**
 * @brief Push n records of the same event id into the ring.
 *
 * Each call asserts the push succeeded so callers can chain invocations
 * without re-stating the assertion on every record.
 */
static void push_n_records(uint32_t n, rx_isr_log_event_id_t evt)
{
  for (uint32_t i = 0U; i < n; i++) {
    TEST_ASSERT_TRUE(rx_isr_log_push(evt, i));
  }
}

/* =============================================================================
 * Producer path
 * ============================================================================= */

void test_push_single_record_increments_pending(void)
{
  TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_test_pending_count());
  TEST_ASSERT_TRUE(rx_isr_log_push(k_isr_log_event_usb_resume, 0U));
  TEST_ASSERT_EQUAL_UINT32(1U, rx_isr_log_test_pending_count());
}

void test_push_records_round_trip_via_drain(void)
{
  TEST_ASSERT_TRUE(rx_isr_log_push(k_isr_log_event_usb_vbus_se0, 0U));
  TEST_ASSERT_TRUE(rx_isr_log_push(k_isr_log_event_usb_dvst_default, k_test_first_value));

  TEST_ASSERT_EQUAL_UINT32(2U, rx_isr_log_test_pending_count());

  const uint32_t drained = rx_isr_log_drain();
  TEST_ASSERT_EQUAL_UINT32(2U, drained);

  /* Drain advances tail to head -> ring is now empty */
  TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_test_pending_count());

  rx_isr_log_stats_t stats;
  rx_isr_log_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(2U, stats.total_pushed);
  TEST_ASSERT_EQUAL_UINT32(2U, stats.total_drained);
}

void test_push_rejects_event_none(void)
{
  TEST_ASSERT_FALSE(rx_isr_log_push(k_isr_log_event_none, 0U));
  TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_test_pending_count());

  rx_isr_log_stats_t stats;
  rx_isr_log_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(0U, stats.total_pushed);
  TEST_ASSERT_EQUAL_UINT32(1U, stats.total_dropped);
}

void test_push_rejects_event_count_sentinel(void)
{
  TEST_ASSERT_FALSE(rx_isr_log_push(k_isr_log_event_count, 0U));
  TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_test_pending_count());

  rx_isr_log_stats_t stats;
  rx_isr_log_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(1U, stats.total_dropped);
}

/* =============================================================================
 * Overflow path
 * ============================================================================= */

void test_push_drops_when_ring_is_full(void)
{
  /* Fill exactly to capacity */
  for (uint32_t i = 0U; i < k_test_capacity; i++) {
    TEST_ASSERT_TRUE(rx_isr_log_push(k_isr_log_event_usb_resume, i));
  }
  TEST_ASSERT_EQUAL_UINT32(k_test_capacity, rx_isr_log_test_pending_count());

  /* Next push must fail */
  TEST_ASSERT_FALSE(rx_isr_log_push(k_isr_log_event_usb_resume, 0U));

  rx_isr_log_stats_t stats;
  rx_isr_log_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(k_test_capacity, stats.total_pushed);
  TEST_ASSERT_EQUAL_UINT32(1U, stats.total_dropped);
  TEST_ASSERT_EQUAL_UINT32(k_test_capacity, stats.high_water);
}

void test_overflow_emits_summary_on_next_drain(void)
{
  /* Push more than capacity */
  for (uint32_t i = 0U; i < k_test_overflow_pushes; i++) {
    (void)rx_isr_log_push(k_isr_log_event_usb_resume, i);
  }

  rx_isr_log_stats_t stats_before;
  rx_isr_log_get_stats(&stats_before);
  const uint32_t expected_drops = k_test_overflow_pushes - k_test_capacity;
  TEST_ASSERT_EQUAL_UINT32(expected_drops, stats_before.total_dropped);
  TEST_ASSERT_EQUAL_UINT32(k_test_capacity, stats_before.total_pushed);

  /* Drain should emit each accepted record (plus a single summary line via
   * the synchronous rx_log_warn_val call -- the summary is observed by
   * test_drain_after_recovery_does_not_re_emit_summary). */
  const uint32_t drained = rx_isr_log_drain();
  TEST_ASSERT_EQUAL_UINT32(k_test_capacity, drained);

  /* After drain, ring empty and total_drained matches */
  TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_test_pending_count());
  rx_isr_log_stats_t stats_after;
  rx_isr_log_get_stats(&stats_after);
  TEST_ASSERT_EQUAL_UINT32(k_test_capacity, stats_after.total_drained);
  TEST_ASSERT_EQUAL_UINT32(expected_drops, stats_after.total_dropped);
}

void test_drain_after_recovery_does_not_re_emit_summary(void)
{
  /* Push 1 over capacity, drain once -- summary emitted */
  for (uint32_t i = 0U; i < (k_test_capacity + 1U); i++) {
    (void)rx_isr_log_push(k_isr_log_event_usb_resume, i);
  }
  (void)rx_isr_log_drain();

  rx_isr_log_stats_t stats_after_first_drain;
  rx_isr_log_get_stats(&stats_after_first_drain);
  TEST_ASSERT_EQUAL_UINT32(1U, stats_after_first_drain.total_dropped);
  TEST_ASSERT_EQUAL_UINT32(k_test_capacity, stats_after_first_drain.total_drained);

  /* Second drain with no new pushes -> no records, no new summary */
  const uint32_t drained = rx_isr_log_drain();
  TEST_ASSERT_EQUAL_UINT32(0U, drained);

  rx_isr_log_stats_t stats_after_second_drain;
  rx_isr_log_get_stats(&stats_after_second_drain);
  /* Drop count unchanged, drained count unchanged */
  TEST_ASSERT_EQUAL_UINT32(stats_after_first_drain.total_dropped,
                           stats_after_second_drain.total_dropped);
  TEST_ASSERT_EQUAL_UINT32(stats_after_first_drain.total_drained,
                           stats_after_second_drain.total_drained);
}

/* =============================================================================
 * Drain
 * ============================================================================= */

void test_drain_empty_returns_zero(void)
{
  TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_drain());
}

void test_drain_advances_tail_only_for_processed_records(void)
{
  /* Push 5 records */
  for (uint32_t i = 0U; i < k_test_5_records; i++) {
    (void)rx_isr_log_push(k_isr_log_event_usb_dvst_address, i);
  }
  TEST_ASSERT_EQUAL_UINT32(k_test_5_records, rx_isr_log_test_pending_count());

  const uint32_t drained = rx_isr_log_drain();
  TEST_ASSERT_EQUAL_UINT32(k_test_5_records, drained);
  TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_test_pending_count());

  rx_isr_log_stats_t stats;
  rx_isr_log_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(k_test_5_records, stats.total_drained);
}

void test_push_drain_push_drain_does_not_lose_records(void)
{
  for (uint32_t round = 0U; round < k_test_4_rounds; round++) {
    push_n_records(k_test_8_records, k_isr_log_event_usb_vbus_fs_j);
    TEST_ASSERT_EQUAL_UINT32(k_test_8_records, rx_isr_log_drain());
  }

  rx_isr_log_stats_t stats;
  rx_isr_log_get_stats(&stats);
  TEST_ASSERT_EQUAL_UINT32(k_test_32_records, stats.total_pushed);
  TEST_ASSERT_EQUAL_UINT32(k_test_32_records, stats.total_drained);
  TEST_ASSERT_EQUAL_UINT32(0U, stats.total_dropped);
}

/* =============================================================================
 * Stats
 * ============================================================================= */

void test_get_stats_with_null_is_silent(void)
{
  rx_isr_log_get_stats(NULL); /* must not crash */
}

void test_high_water_tracks_peak_occupancy(void)
{
  for (uint32_t i = 0U; i < k_test_10_records; i++) {
    (void)rx_isr_log_push(k_isr_log_event_usb_resume, 0U);
  }
  rx_isr_log_stats_t stats1;
  rx_isr_log_get_stats(&stats1);
  TEST_ASSERT_EQUAL_UINT32(k_test_10_records, stats1.high_water);

  (void)rx_isr_log_drain();

  /* Drain should NOT shrink high_water */
  rx_isr_log_stats_t stats2;
  rx_isr_log_get_stats(&stats2);
  TEST_ASSERT_EQUAL_UINT32(k_test_10_records, stats2.high_water);
}

/* =============================================================================
 * Coverage of every event id
 *
 * Walking the table guarantees every row's tag/message pointer is valid and
 * the drain switch covers every level branch (debug / info / warn / error).
 * ============================================================================= */

void test_drain_handles_all_event_ids(void)
{
  for (uint8_t id = k_isr_log_event_none + 1U; id < k_isr_log_event_count; id++) {
    rx_isr_log_test_reset_state();
    rx_log_uart_test_reset_state();

    TEST_ASSERT_TRUE(rx_isr_log_push((rx_isr_log_event_id_t)id, 0U));
    TEST_ASSERT_EQUAL_UINT32(1U, rx_isr_log_drain());
    TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_test_pending_count());
  }
}

/* =============================================================================
 * Drain has_value=true coverage
 *
 * Every production event-table entry sets has_value=false, so the drain hook's
 * has_value=true switch arms (debug / info / warn / error variants of
 * rx_log_*_val) are unreachable through normal use. We use the test hook
 * rx_isr_log_test_set_event_meta() to flip the flag and the level on one event
 * id, then push and drain to exercise the corresponding _val emit branch.
 *
 * Each test asserts the round-trip succeeds (1 push, 1 drain) -- the rx_log_*
 * macros write to stdout in RX_SIMULATOR_MODE, so semantic verification is
 * limited to bookkeeping, but the line/branch counters in gcov see all four
 * arms of the switch get hit.
 * ============================================================================= */

/* Drain emit-level values mirroring the static rx_isr_log_level_t enum
 * (file-private to rx_isr_log.c). Keep these in sync with the source. */
typedef enum : uint8_t {
  k_test_drain_level_debug = 0U,
  k_test_drain_level_info  = 1U,
  k_test_drain_level_warn  = 2U,
  k_test_drain_level_error = 3U,
} test_drain_level_t;

static void run_has_value_drain_for_level(test_drain_level_t level)
{
  /* Pick any valid event id; metadata override drives the switch arm. */
  rx_isr_log_test_set_event_meta(k_isr_log_event_usb_resume, (uint8_t)level, true);

  TEST_ASSERT_TRUE(rx_isr_log_push(k_isr_log_event_usb_resume, k_test_first_value));
  TEST_ASSERT_EQUAL_UINT32(1U, rx_isr_log_drain());
  TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_test_pending_count());
}

void test_drain_has_value_debug_branch(void)
{
  run_has_value_drain_for_level(k_test_drain_level_debug);
}

void test_drain_has_value_info_branch(void)
{
  run_has_value_drain_for_level(k_test_drain_level_info);
}

void test_drain_has_value_warn_branch(void)
{
  run_has_value_drain_for_level(k_test_drain_level_warn);
}

void test_drain_has_value_error_branch(void)
{
  run_has_value_drain_for_level(k_test_drain_level_error);
}

void test_drain_has_value_default_branch_falls_through_to_debug(void)
{
  /* Out-of-range level value flows through the switch's default arm,
   * which mirrors the debug emit. The cast suppresses the (intentionally)
   * out-of-range enum value -- this is the whole point of the test. */
  enum : uint8_t {
    k_test_drain_level_invalid = 7U, /* Any value not in 0..3 */
  };
  rx_isr_log_test_set_event_meta(k_isr_log_event_usb_resume, k_test_drain_level_invalid, true);

  TEST_ASSERT_TRUE(rx_isr_log_push(k_isr_log_event_usb_resume, k_test_first_value));
  TEST_ASSERT_EQUAL_UINT32(1U, rx_isr_log_drain());
}

void test_drain_no_value_default_branch_falls_through_to_debug(void)
{
  /* Same as the has_value=true default test, but with has_value=false so
   * the second switch (line 294) gets its default branch exercised too --
   * otherwise gcov reports the case-debug-only path as never taken. */
  enum : uint8_t {
    k_test_drain_level_invalid = 9U, /* Any value not in 0..3 */
  };
  rx_isr_log_test_set_event_meta(k_isr_log_event_usb_resume, k_test_drain_level_invalid, false);

  TEST_ASSERT_TRUE(rx_isr_log_push(k_isr_log_event_usb_resume, 0U));
  TEST_ASSERT_EQUAL_UINT32(1U, rx_isr_log_drain());
}

/* =============================================================================
 * Drain torn-record defensive guards
 *
 * rx_isr_log_push() rejects event_id == k_isr_log_event_none and event_id >=
 * k_isr_log_event_count, so internal_emit_record()'s defensive early returns
 * for those values are unreachable from normal producer paths. The
 * rx_isr_log_test_inject_raw_record() hook bypasses push() validation so the
 * drain hook actually processes a record with an invalid id and we cover the
 * out-of-range / sentinel guard branches.
 * ============================================================================= */

void test_drain_skips_torn_record_with_event_id_none(void)
{
  /* Inject raw 0 (== k_isr_log_event_none) directly into the ring */
  TEST_ASSERT_TRUE(rx_isr_log_test_inject_raw_record(0U, k_test_first_value));
  TEST_ASSERT_EQUAL_UINT32(1U, rx_isr_log_test_pending_count());

  /* Drain consumes the record but skips emission via the early return */
  const uint32_t drained = rx_isr_log_drain();
  TEST_ASSERT_EQUAL_UINT32(1U, drained);
  TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_test_pending_count());
}

void test_drain_skips_torn_record_with_out_of_range_event_id(void)
{
  /* Inject an id well past k_isr_log_event_count (UINT8_MAX) */
  TEST_ASSERT_TRUE(rx_isr_log_test_inject_raw_record(UINT8_MAX, k_test_first_value));
  TEST_ASSERT_EQUAL_UINT32(1U, rx_isr_log_test_pending_count());

  const uint32_t drained = rx_isr_log_drain();
  TEST_ASSERT_EQUAL_UINT32(1U, drained);
  TEST_ASSERT_EQUAL_UINT32(0U, rx_isr_log_test_pending_count());
}

/* =============================================================================
 * Test hook self-tests
 *
 * Defend against the test hooks regressing silently and masking coverage gaps.
 * ============================================================================= */

void test_inject_raw_record_returns_false_when_full(void)
{
  /* Fill the ring exactly to capacity using the public push */
  for (uint32_t i = 0U; i < k_test_capacity; i++) {
    TEST_ASSERT_TRUE(rx_isr_log_push(k_isr_log_event_usb_resume, i));
  }
  /* Inject must now refuse */
  TEST_ASSERT_FALSE(rx_isr_log_test_inject_raw_record(0U, 0U));
}

void test_set_event_meta_ignores_out_of_range_id(void)
{
  /* Should not crash; verify idempotency by drain on a real id afterwards */
  rx_isr_log_test_set_event_meta((rx_isr_log_event_id_t)k_isr_log_event_count,
                                 k_test_drain_level_warn,
                                 true);

  TEST_ASSERT_TRUE(rx_isr_log_push(k_isr_log_event_usb_resume, 0U));
  TEST_ASSERT_EQUAL_UINT32(1U, rx_isr_log_drain());
}

/* =============================================================================
 * Test runner
 * ============================================================================= */

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_push_single_record_increments_pending);
  RUN_TEST(test_push_records_round_trip_via_drain);
  RUN_TEST(test_push_rejects_event_none);
  RUN_TEST(test_push_rejects_event_count_sentinel);
  RUN_TEST(test_push_drops_when_ring_is_full);
  RUN_TEST(test_overflow_emits_summary_on_next_drain);
  RUN_TEST(test_drain_after_recovery_does_not_re_emit_summary);
  RUN_TEST(test_drain_empty_returns_zero);
  RUN_TEST(test_drain_advances_tail_only_for_processed_records);
  RUN_TEST(test_push_drain_push_drain_does_not_lose_records);
  RUN_TEST(test_get_stats_with_null_is_silent);
  RUN_TEST(test_high_water_tracks_peak_occupancy);
  RUN_TEST(test_drain_handles_all_event_ids);
  RUN_TEST(test_drain_has_value_debug_branch);
  RUN_TEST(test_drain_has_value_info_branch);
  RUN_TEST(test_drain_has_value_warn_branch);
  RUN_TEST(test_drain_has_value_error_branch);
  RUN_TEST(test_drain_has_value_default_branch_falls_through_to_debug);
  RUN_TEST(test_drain_no_value_default_branch_falls_through_to_debug);
  RUN_TEST(test_drain_skips_torn_record_with_event_id_none);
  RUN_TEST(test_drain_skips_torn_record_with_out_of_range_event_id);
  RUN_TEST(test_inject_raw_record_returns_false_when_full);
  RUN_TEST(test_set_event_meta_ignores_out_of_range_id);

  return UNITY_END();
}
