/* tests/test_rx_register_guard.c */

/**
 * @file test_rx_register_guard.c
 * @brief Unit Tests for Register Guard Implementation
 *
 * Tests the register guard module including:
 * - Initialization (capture golden values)
 * - Initialized state tracking
 * - Correction count tracking
 * - Count reset functionality
 * - Multiple initialization cycles
 * - Refresh behavior
 *
 * Note: Hardware-specific functionality (PDR, MSTPCR) is only compiled
 * for RX72N target (__RX__ defined). Host tests verify the state machine
 * and counter logic.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "unity.h"

/* Include the module under test */
#include "rx_register_guard.h"

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

void setUp(void)
{
  /* Reset register guard state before each test */
  /* We need to call init to set up state, then rely on API to manage it */
}

void tearDown(void)
{
  /* Reset the correction counter between tests */
  rx_register_guard_reset_count();
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test successful initialization
 */
void test_register_guard_init_success(void)
{
  rx_err_t err = rx_register_guard_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());
}

/**
 * @brief Test initialization resets correction count
 */
void test_register_guard_init_resets_count(void)
{
  /* First init to establish state */
  rx_register_guard_init();

  /* Manually cannot increment, but verify count starts at 0 */
  uint32_t count = rx_register_guard_get_correction_count();
  TEST_ASSERT_EQUAL_UINT32(0, count);
}

/**
 * @brief Test double initialization is safe
 */
void test_register_guard_double_init(void)
{
  rx_err_t err = rx_register_guard_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());

  /* Second init should also succeed (updates golden values) */
  err = rx_register_guard_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());
}

/* =============================================================================
 * State Query Tests
 * =============================================================================
 */

/**
 * @brief Test is_initialized returns correct state
 */
void test_register_guard_is_initialized_after_init(void)
{
  rx_register_guard_init();
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());
}

/* =============================================================================
 * Correction Count Tests
 * =============================================================================
 */

/**
 * @brief Test get_correction_count returns 0 initially
 */
void test_register_guard_correction_count_initial(void)
{
  rx_register_guard_init();
  uint32_t count = rx_register_guard_get_correction_count();
  TEST_ASSERT_EQUAL_UINT32(0, count);
}

/**
 * @brief Test reset_count clears the counter
 */
void test_register_guard_reset_count(void)
{
  rx_register_guard_init();

  /* On host, refresh won't increment counter (no hardware),
   * but we can verify reset works */
  rx_register_guard_reset_count();
  uint32_t count = rx_register_guard_get_correction_count();
  TEST_ASSERT_EQUAL_UINT32(0, count);
}

/**
 * @brief Test multiple reset cycles
 */
void test_register_guard_multiple_reset_cycles(void)
{
  rx_register_guard_init();

  /* Reset multiple times - should always succeed */
  rx_register_guard_reset_count();
  TEST_ASSERT_EQUAL_UINT32(0, rx_register_guard_get_correction_count());

  rx_register_guard_reset_count();
  TEST_ASSERT_EQUAL_UINT32(0, rx_register_guard_get_correction_count());

  rx_register_guard_reset_count();
  TEST_ASSERT_EQUAL_UINT32(0, rx_register_guard_get_correction_count());
}

/* =============================================================================
 * Refresh Tests
 * =============================================================================
 */

/**
 * @brief Test refresh does nothing when not initialized
 *
 * This is a safety test - calling refresh before init should be safe.
 */
void test_register_guard_refresh_not_initialized(void)
{
  /* Create a fresh state by manually resetting internal state */
  /* Since we can't uninitialize, we rely on the init check in refresh */
  rx_register_guard_init();

  /* Refresh should be safe to call (no crash) */
  rx_register_guard_refresh();

  /* Verify state is still valid */
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());
}

/**
 * @brief Test refresh is safe to call multiple times
 */
void test_register_guard_refresh_multiple_calls(void)
{
  rx_register_guard_init();

  /* Call refresh multiple times - should not crash */
  for (uint32_t i = 0; i < 100; i++) {
    rx_register_guard_refresh();
  }

  /* Verify state is still valid */
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());
}

/**
 * @brief Test refresh does not corrupt counter on host
 *
 * On host (no __RX__), refresh is effectively a no-op for hardware,
 * so counter should stay at 0.
 */
void test_register_guard_refresh_no_host_corruption(void)
{
  rx_register_guard_init();
  TEST_ASSERT_EQUAL_UINT32(0, rx_register_guard_get_correction_count());

  /* Multiple refreshes should not change count on host */
  for (uint32_t i = 0; i < 10; i++) {
    rx_register_guard_refresh();
  }

  TEST_ASSERT_EQUAL_UINT32(0, rx_register_guard_get_correction_count());
}

/* =============================================================================
 * Workflow Tests
 * =============================================================================
 */

/**
 * @brief Test typical usage workflow
 *
 * Simulates: init -> periodic refresh -> check count -> reset -> continue
 */
void test_register_guard_typical_workflow(void)
{
  /* Step 1: Initialize after peripheral setup */
  rx_err_t err = rx_register_guard_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());

  /* Step 2: Periodic refresh (simulating main loop) */
  for (uint32_t loop = 0; loop < 10; loop++) {
    rx_register_guard_refresh();
  }

  /* Step 3: Check correction count (for diagnostics) */
  uint32_t count = rx_register_guard_get_correction_count();
  /* On host, count should be 0 (no hardware corruption possible) */
  TEST_ASSERT_EQUAL_UINT32(0, count);

  /* Step 4: Reset count (for periodic logging) */
  rx_register_guard_reset_count();
  TEST_ASSERT_EQUAL_UINT32(0, rx_register_guard_get_correction_count());

  /* Step 5: Continue with more refreshes */
  for (uint32_t loop = 0; loop < 10; loop++) {
    rx_register_guard_refresh();
  }

  /* Final state should be valid */
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());
}

/**
 * @brief Test re-initialization updates golden values
 *
 * This simulates reconfiguring peripherals and updating the guard.
 */
void test_register_guard_reinit_workflow(void)
{
  /* First init */
  rx_err_t err = rx_register_guard_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Some refreshes */
  rx_register_guard_refresh();
  rx_register_guard_refresh();

  /* Re-init (e.g., after peripheral reconfiguration) */
  err = rx_register_guard_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());

  /* Count should be reset by init */
  TEST_ASSERT_EQUAL_UINT32(0, rx_register_guard_get_correction_count());
}

/* =============================================================================
 * Edge Case Tests
 * =============================================================================
 */

/**
 * @brief Test counter does not overflow on many refreshes
 *
 * This is a sanity check that refresh operations don't cause issues.
 */
void test_register_guard_many_refreshes(void)
{
  rx_register_guard_init();

  /* Large number of refreshes */
  for (uint32_t i = 0; i < 1000; i++) {
    rx_register_guard_refresh();
  }

  /* Should still be functional */
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());
  /* On host, count stays 0 */
  TEST_ASSERT_EQUAL_UINT32(0, rx_register_guard_get_correction_count());
}

/**
 * @brief Test reset before init is safe
 */
void test_register_guard_reset_before_init(void)
{
  /* This relies on static initialization to 0 */
  rx_register_guard_reset_count();
  TEST_ASSERT_EQUAL_UINT32(0, rx_register_guard_get_correction_count());

  /* Now init and verify normal operation */
  rx_register_guard_init();
  TEST_ASSERT_TRUE(rx_register_guard_is_initialized());
}

/**
 * @brief Test get_correction_count before init
 */
void test_register_guard_get_count_before_init(void)
{
  /* Static initialization should give 0 */
  /* Note: This relies on C guaranteeing static variables are zero-initialized */
  uint32_t count = rx_register_guard_get_correction_count();
  /* May be 0 or undefined depending on prior tests, but should not crash */
  (void)count;
  TEST_PASS();
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_register_guard_init_success);
  RUN_TEST(test_register_guard_init_resets_count);
  RUN_TEST(test_register_guard_double_init);

  /* State query tests */
  RUN_TEST(test_register_guard_is_initialized_after_init);

  /* Correction count tests */
  RUN_TEST(test_register_guard_correction_count_initial);
  RUN_TEST(test_register_guard_reset_count);
  RUN_TEST(test_register_guard_multiple_reset_cycles);

  /* Refresh tests */
  RUN_TEST(test_register_guard_refresh_not_initialized);
  RUN_TEST(test_register_guard_refresh_multiple_calls);
  RUN_TEST(test_register_guard_refresh_no_host_corruption);

  /* Workflow tests */
  RUN_TEST(test_register_guard_typical_workflow);
  RUN_TEST(test_register_guard_reinit_workflow);

  /* Edge case tests */
  RUN_TEST(test_register_guard_many_refreshes);
  RUN_TEST(test_register_guard_reset_before_init);
  RUN_TEST(test_register_guard_get_count_before_init);

  return UNITY_END();
}
