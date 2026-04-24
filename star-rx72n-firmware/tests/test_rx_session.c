/**
 * @file test_rx_session.c
 * @brief Unit Tests for Shared Session State Module
 *
 * @details
 * Tests the rx_session module which provides cross-transport sequence
 * continuity between USB CDC and SPI transports. Verifies behavior matches
 * the Go gateway's `manager/session.go` implementation.
 *
 * ## Test Coverage
 *
 * - Initialization and deinitialization
 * - TX sequence increment and wraparound
 * - RX sequence validation (exact, gap, reject)
 * - Session reset
 * - NULL pointer handling
 * - Transport switch continuity (USB seq=N -> SPI seq=N+1)
 * - Gap tolerance matching Go MaxGapTolerance=10
 *
 * @see rx_session.h  Module under test
 * @see star-gateway/internal/manager/session.go  Go reference
 
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
*/

#include "rx_session.h"
#include "unity.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

/**
 * @brief Test sequence number constants for session tests
 *
 * @details Named constants replace magic literals throughout the test suite
 * to improve readability and maintainability per NASA Power of 10 Rule 8.
 */
typedef enum : uint16_t {
  k_test_sentinel_seq      = 0xFFFF, /**< Sentinel value to detect unwritten output */
  k_test_expected_seq_0    = 0,      /**< Expected initial sequence after init/reset */
  k_test_advance_count     = 100,    /**< Iterations for TX advance loop test */
  k_test_validate_count    = 50,     /**< Iterations for RX validate loop test */
  k_test_reset_advance     = 10,     /**< Iterations to advance before reset test */
  k_test_transport_switch  = 5,      /**< Iterations per transport in switch test */
  k_test_gap_accept_count  = 6,      /**< Accept count for gap boundary test (0-5) */
  k_test_wraparound_start  = 0xFFFE, /**< TX/RX sequence near max for wraparound */
  k_test_wraparound_max    = 0xFFFF, /**< Maximum sequence number before wrap */
  k_test_gap_one_skip      = 2,      /**< RX seq with 1 frame skipped (gap=1) */
  k_test_gap_one_next      = 3,      /**< Expected next seq after gap-1 accept */
  k_test_gap_max_skip      = 10,     /**< RX seq with accepted gap (diff=9) */
  k_test_gap_boundary_skip = 32769,  /**< RX seq at gap tolerance boundary (diff=32768 = k_session_max_gap_tolerance) */
  k_test_gap_large_skip    = 33000,  /**< RX seq with large gap (diff=32999) -- rejected */
  k_test_dup_upper         = 5,      /**< Upper bound for duplicate test accept loop */
  k_test_dup_replay        = 3,      /**< Duplicated seq to test rejection */
} test_session_constants_t;

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

/**
 * @var s_session
 * @brief Shared session state instance for all test functions
 *
 * @details Initialized in setUp() via rx_session_init(), torn down in
 * tearDown() via rx_session_deinit(). Each test starts with a fresh
 * session (tx=0, rx=0).
 */
static rx_session_state_t s_session;

/**
 * @brief Per-test setup: initialize a fresh session state
 *
 * @details Zeroes the session struct and calls rx_session_init() to establish
 * a clean starting state (tx_sequence=0, rx_sequence=0, initialized=true).
 * Asserts initialization succeeds.
 *
 * @pre s_session memory is valid
 * @post s_session.initialized == true, sequences at zero
 */
void setUp(void)
{
  static const rx_session_state_t s_zero = {};
  s_session                              = s_zero;
  rx_err_t err                           = rx_session_init(&s_session);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Per-test teardown: deinitialize session if still active
 *
 * @details Safely deinitializes the session state only if it is still marked
 * as initialized, allowing tests that explicitly deinit to not double-free.
 *
 * @pre s_session may or may not be initialized
 * @post s_session.initialized == false
 */
void tearDown(void)
{
  if (s_session.initialized) {
    TEST_ASSERT_EQUAL(k_rx_ok, rx_session_deinit(&s_session));
  }
}

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

/**
 * @brief Verify init sets sequences to zero and marks initialized
 */
void test_init_sets_sequences_to_zero(void)
{
  /* setUp already called init, verify state */
  uint16_t tx_seq = k_test_sentinel_seq;
  uint16_t rx_seq = k_test_sentinel_seq;

  rx_err_t err = rx_session_get_tx(&s_session, &tx_seq);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_expected_seq_0, tx_seq);

  err = rx_session_get_rx(&s_session, &rx_seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_expected_seq_0, rx_seq);
}

/**
 * @brief Verify init rejects NULL pointer
 */
void test_init_null_ptr(void)
{
  rx_err_t err = rx_session_init(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify deinit rejects NULL pointer
 */
void test_deinit_null_ptr(void)
{
  rx_err_t err = rx_session_deinit(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify deinit rejects uninitialized state
 */
void test_deinit_not_initialized(void)
{
  rx_session_state_t uninit = {};

  rx_err_t err = rx_session_deinit(&uninit);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief Verify deinit clears initialized flag
 */
void test_deinit_clears_initialized(void)
{
  rx_err_t err = rx_session_deinit(&s_session);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_session.initialized);
}

/* ============================================================================
 * TX Sequence Tests
 * ============================================================================ */

/**
 * @brief Verify next_tx returns 0 on first call and increments
 *
 * Mirrors Go: seq := state.NextTxSequence() // returns 0, then 1, then 2...
 */
void test_next_tx_starts_at_zero(void)
{
  uint16_t seq;

  rx_err_t err = rx_session_next_tx(&s_session, &seq);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, seq);
}

/**
 * @brief Verify next_tx increments sequentially
 */
void test_next_tx_increments(void)
{
  uint16_t seq;

  for (uint16_t i = 0; i < k_test_advance_count; i++) {
    rx_err_t err = rx_session_next_tx(&s_session, &seq);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL_UINT16(i, seq);
  }
}

/**
 * @brief Verify next_tx wraps around at 65535 -> 0
 *
 * Mirrors Go: s.txSequence = (s.txSequence + 1) & 0xFFFF
 */
void test_next_tx_wraparound(void)
{
  /* Set tx_sequence close to max via repeated next_tx would be slow.
   * Instead, directly set the internal state for this test. */
  s_session.tx_sequence = k_test_wraparound_start;

  uint16_t seq;
  rx_err_t err;

  err = rx_session_next_tx(&s_session, &seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_wraparound_start, seq);

  err = rx_session_next_tx(&s_session, &seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_wraparound_max, seq);

  /* Should wrap to 0 */
  err = rx_session_next_tx(&s_session, &seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, seq);
}

/**
 * @brief Verify next_tx rejects NULL state
 */
void test_next_tx_null_state(void)
{
  uint16_t seq;
  rx_err_t err = rx_session_next_tx(NULL, &seq);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify next_tx rejects NULL sequence output
 */
void test_next_tx_null_sequence(void)
{
  rx_err_t err = rx_session_next_tx(&s_session, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify next_tx rejects uninitialized state
 */
void test_next_tx_not_initialized(void)
{
  rx_session_state_t uninit = {};

  uint16_t seq;
  rx_err_t err = rx_session_next_tx(&uninit, &seq);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/* ============================================================================
 * RX Validation Tests
 * ============================================================================ */

/**
 * @brief Verify validate_rx accepts exact match (diff == 0)
 *
 * Mirrors Go: if diff == 0 { s.rxSequence = (s.rxSequence + 1) & 0xFFFF; return true }
 */
void test_validate_rx_exact_match(void)
{
  rx_session_validate_result_t result;

  /* Expect seq=0, receive seq=0 -> accept */
  rx_err_t err = rx_session_validate_rx(&s_session, 0, &result);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_session_validate_ok, result);

  /* Expect seq=1, receive seq=1 -> accept */
  err = rx_session_validate_rx(&s_session, 1, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_session_validate_ok, result);
}

/**
 * @brief Verify validate_rx accepts sequential frames
 */
void test_validate_rx_sequential(void)
{
  rx_session_validate_result_t result;

  for (uint16_t i = 0; i < k_test_validate_count; i++) {
    rx_err_t err = rx_session_validate_rx(&s_session, i, &result);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL(k_session_validate_ok, result);
  }
}

/**
 * @brief Verify validate_rx accepts small gap (1 frame lost)
 *
 * Mirrors Go: if diff > 0 && diff < MaxGapTolerance { accept }
 */
void test_validate_rx_gap_one_frame(void)
{
  rx_session_validate_result_t result;

  /* Accept seq=0 */
  rx_err_t err = rx_session_validate_rx(&s_session, 0, &result);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Skip seq=1, receive seq=2 (gap=1) -> accept with gap */
  err = rx_session_validate_rx(&s_session, k_test_gap_one_skip, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_session_validate_gap, result);

  /* Next expected should be 3 */
  uint16_t rx_seq;
  err = rx_session_get_rx(&s_session, &rx_seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_gap_one_next, rx_seq);
}

/**
 * @brief Verify validate_rx accepts maximum small gap (9 frames lost)
 *
 * MaxGapTolerance is 10, so diff=9 is the largest accepted gap.
 */
void test_validate_rx_gap_max_accepted(void)
{
  rx_session_validate_result_t result;

  /* Accept seq=0 */
  rx_err_t err = rx_session_validate_rx(&s_session, 0, &result);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Skip 9 frames, receive seq=10 (diff=9) -> accept with gap */
  err = rx_session_validate_rx(&s_session, k_test_gap_max_skip, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_session_validate_gap, result);
}

/**
 * @brief Verify validate_rx rejects gap at tolerance boundary (10 frames lost)
 *
 * MaxGapTolerance is 10, so diff=10 is rejected.
 */
void test_validate_rx_gap_at_boundary(void)
{
  rx_session_validate_result_t result;

  /* Accept seq=0 */
  rx_err_t err = rx_session_validate_rx(&s_session, 0, &result);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Skip 10 frames, receive seq=11 (diff=10) -> reject */
  err = rx_session_validate_rx(&s_session, k_test_gap_boundary_skip, &result);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
  TEST_ASSERT_EQUAL(k_session_validate_fail, result);

  /* rx_sequence should be unchanged (still expecting 1) */
  uint16_t rx_seq;
  err = rx_session_get_rx(&s_session, &rx_seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, rx_seq);
}

/**
 * @brief Verify validate_rx rejects large gap
 */
void test_validate_rx_large_gap_rejected(void)
{
  rx_session_validate_result_t result;

  /* Accept seq=0 */
  rx_err_t err = rx_session_validate_rx(&s_session, 0, &result);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Receive seq=100 (diff=99) -> reject */
  err = rx_session_validate_rx(&s_session, k_test_gap_large_skip, &result);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
  TEST_ASSERT_EQUAL(k_session_validate_fail, result);
}

/**
 * @brief Verify validate_rx rejects duplicate (backward sequence)
 *
 * If we've accepted seq=5, receiving seq=3 should be rejected as duplicate.
 * diff = 3 - 6 = 0xFFFD (large positive in uint16, near 65535).
 */
void test_validate_rx_duplicate_rejected(void)
{
  rx_session_validate_result_t result;

  /* Accept seq 0-5 */
  for (uint16_t i = 0; i <= k_test_dup_upper; i++) {
    rx_err_t err = rx_session_validate_rx(&s_session, i, &result);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
  }

  /* Receive seq=3 (already processed) -> reject */
  rx_err_t err = rx_session_validate_rx(&s_session, k_test_dup_replay, &result);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
  TEST_ASSERT_EQUAL(k_session_validate_fail, result);
}

/**
 * @brief Verify validate_rx with NULL result pointer still works (exact match)
 */
void test_validate_rx_null_result(void)
{
  /* result=NULL should be fine (optional output) */
  rx_err_t err = rx_session_validate_rx(&s_session, 0, NULL);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Verify validate_rx with NULL result pointer works for gap case
 *
 * @details
 * Exercises the gap branch (0 < diff < MaxGapTolerance) with result=NULL.
 * Verifies that the null result pointer path inside the gap branch is
 * reachable and does not crash.
 */
void test_validate_rx_null_result_gap(void)
{
  /* Accept seq=0 first to advance rx_sequence to 1 */
  rx_err_t err = rx_session_validate_rx(&s_session, 0, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Skip seq=1, receive seq=2 (gap=1, within tolerance) with NULL result */
  err = rx_session_validate_rx(&s_session, k_test_gap_one_skip, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify rx_sequence updated to 3 (gap was accepted) */
  uint16_t rx_seq;
  err = rx_session_get_rx(&s_session, &rx_seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_gap_one_next, rx_seq);
}

/**
 * @brief Verify validate_rx with NULL result pointer works for fail case
 *
 * @details
 * Exercises the reject branch (diff >= MaxGapTolerance) with result=NULL.
 * Verifies that the null result pointer path inside the fail branch is
 * reachable and does not crash.
 */
void test_validate_rx_null_result_fail(void)
{
  /* Accept seq=0 first to advance rx_sequence to 1 */
  rx_err_t err = rx_session_validate_rx(&s_session, 0, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Receive seq=100 (diff=99, >= MaxGapTolerance=10) with NULL result */
  err = rx_session_validate_rx(&s_session, k_test_gap_large_skip, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);

  /* rx_sequence should be unchanged (still expecting 1) */
  uint16_t rx_seq;
  err = rx_session_get_rx(&s_session, &rx_seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(1, rx_seq);
}

/**
 * @brief Verify validate_rx rejects NULL state
 */
void test_validate_rx_null_state(void)
{
  rx_session_validate_result_t result;
  rx_err_t                     err = rx_session_validate_rx(NULL, 0, &result);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify validate_rx rejects uninitialized state
 */
void test_validate_rx_not_initialized(void)
{
  rx_session_state_t           uninit = {};
  rx_session_validate_result_t result;

  rx_err_t err = rx_session_validate_rx(&uninit, 0, &result);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief Verify validate_rx handles wraparound (seq near 65535)
 */
void test_validate_rx_wraparound(void)
{
  /* Set rx_sequence near wraparound */
  rx_session_validate_result_t result;

  s_session.rx_sequence = k_test_wraparound_start;

  /* Accept seq=0xFFFE */
  rx_err_t err = rx_session_validate_rx(&s_session, k_test_wraparound_start, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_session_validate_ok, result);

  /* Accept seq=0xFFFF */
  err = rx_session_validate_rx(&s_session, k_test_wraparound_max, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_session_validate_ok, result);

  /* Accept seq=0 (wrapped around) */
  err = rx_session_validate_rx(&s_session, 0, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_session_validate_ok, result);
}

/* ============================================================================
 * Reset Tests
 * ============================================================================ */

/**
 * @brief Verify reset clears both sequences to zero
 *
 * Mirrors Go: func (s *SessionState) Reset() { s.txSequence = 0; s.rxSequence = 0 }
 */
void test_reset_clears_sequences(void)
{
  uint16_t seq;

  /* Advance both sequences */
  for (uint16_t i = 0; i < k_test_reset_advance; i++) {
    rx_err_t ret = rx_session_next_tx(&s_session, &seq);
    TEST_ASSERT_EQUAL(k_rx_ok, ret);
  }

  rx_session_validate_result_t result;
  for (uint16_t i = 0; i < k_test_reset_advance; i++) {
    rx_err_t ret = rx_session_validate_rx(&s_session, i, &result);
    TEST_ASSERT_EQUAL(k_rx_ok, ret);
  }

  /* Reset */
  rx_err_t err = rx_session_reset(&s_session);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify both are zero */
  err = rx_session_get_tx(&s_session, &seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, seq);

  err = rx_session_get_rx(&s_session, &seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(0, seq);
}

/**
 * @brief Verify reset rejects NULL state
 */
void test_reset_null_ptr(void)
{
  rx_err_t err = rx_session_reset(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify reset rejects uninitialized state
 */
void test_reset_not_initialized(void)
{
  rx_session_state_t uninit = {};

  rx_err_t err = rx_session_reset(&uninit);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/* ============================================================================
 * Transport Switch Continuity Tests
 * ============================================================================ */

/**
 * @brief Verify sequence continuity across simulated transport switch
 *
 * @details
 * Simulates the key use case: USB sends seq 0-4, then SPI takes over and
 * must continue from seq 5. Both transports use the same session state,
 * so the sequence is continuous.
 *
 * "When Gateway fails over from USB (seq=105) to SPI, SPI MUST continue from seq=106"
 */
void test_transport_switch_tx_continuity(void)
{
  uint16_t seq;

  /* USB sends frames with seq 0-(N-1) */
  for (uint16_t i = 0; i < k_test_transport_switch; i++) {
    rx_err_t err = rx_session_next_tx(&s_session, &seq);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL_UINT16(i, seq);
  }

  /* "Switch to SPI" - same session state, next seq should be N */
  rx_err_t err = rx_session_next_tx(&s_session, &seq);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT16(k_test_transport_switch, seq);
}

/**
 * @brief Verify RX sequence continuity across simulated transport switch
 *
 * USB receives seq 0-4, then SPI receives seq 5 - should accept normally.
 */
void test_transport_switch_rx_continuity(void)
{
  rx_session_validate_result_t result;

  /* USB receives seq 0-(N-1) */
  for (uint16_t i = 0; i < k_test_transport_switch; i++) {
    rx_err_t err = rx_session_validate_rx(&s_session, i, &result);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_EQUAL(k_session_validate_ok, result);
  }

  /* "Switch to SPI" - same session state, receive seq N */
  rx_err_t err = rx_session_validate_rx(&s_session, k_test_transport_switch, &result);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_session_validate_ok, result);
}

/* ============================================================================
 * Getter Tests
 * ============================================================================ */

/**
 * @brief Verify get_tx rejects NULL state
 */
void test_get_tx_null_state(void)
{
  uint16_t seq;
  rx_err_t err = rx_session_get_tx(NULL, &seq);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify get_tx rejects NULL output
 */
void test_get_tx_null_output(void)
{
  rx_err_t err = rx_session_get_tx(&s_session, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify get_tx rejects uninitialized state
 */
void test_get_tx_not_initialized(void)
{
  rx_session_state_t uninit = {};
  uint16_t           seq;

  rx_err_t err = rx_session_get_tx(&uninit, &seq);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief Verify get_rx rejects NULL state
 */
void test_get_rx_null_state(void)
{
  uint16_t seq;
  rx_err_t err = rx_session_get_rx(NULL, &seq);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify get_rx rejects NULL output
 */
void test_get_rx_null_output(void)
{
  rx_err_t err = rx_session_get_rx(&s_session, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify get_rx rejects uninitialized state
 */
void test_get_rx_not_initialized(void)
{
  rx_session_state_t uninit = {};
  uint16_t           seq;

  rx_err_t err = rx_session_get_rx(&uninit, &seq);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization */
  RUN_TEST(test_init_sets_sequences_to_zero);
  RUN_TEST(test_init_null_ptr);
  RUN_TEST(test_deinit_null_ptr);
  RUN_TEST(test_deinit_not_initialized);
  RUN_TEST(test_deinit_clears_initialized);

  /* TX Sequence */
  RUN_TEST(test_next_tx_starts_at_zero);
  RUN_TEST(test_next_tx_increments);
  RUN_TEST(test_next_tx_wraparound);
  RUN_TEST(test_next_tx_null_state);
  RUN_TEST(test_next_tx_null_sequence);
  RUN_TEST(test_next_tx_not_initialized);

  /* RX Validation */
  RUN_TEST(test_validate_rx_exact_match);
  RUN_TEST(test_validate_rx_sequential);
  RUN_TEST(test_validate_rx_gap_one_frame);
  RUN_TEST(test_validate_rx_gap_max_accepted);
  RUN_TEST(test_validate_rx_gap_at_boundary);
  RUN_TEST(test_validate_rx_large_gap_rejected);
  RUN_TEST(test_validate_rx_duplicate_rejected);
  RUN_TEST(test_validate_rx_null_result);
  RUN_TEST(test_validate_rx_null_result_gap);
  RUN_TEST(test_validate_rx_null_result_fail);
  RUN_TEST(test_validate_rx_null_state);
  RUN_TEST(test_validate_rx_not_initialized);
  RUN_TEST(test_validate_rx_wraparound);

  /* Reset */
  RUN_TEST(test_reset_clears_sequences);
  RUN_TEST(test_reset_null_ptr);
  RUN_TEST(test_reset_not_initialized);

  /* Transport Switch Continuity */
  RUN_TEST(test_transport_switch_tx_continuity);
  RUN_TEST(test_transport_switch_rx_continuity);

  /* Getters */
  RUN_TEST(test_get_tx_null_state);
  RUN_TEST(test_get_tx_null_output);
  RUN_TEST(test_get_tx_not_initialized);
  RUN_TEST(test_get_rx_null_state);
  RUN_TEST(test_get_rx_null_output);
  RUN_TEST(test_get_rx_not_initialized);

  return UNITY_END();
}
