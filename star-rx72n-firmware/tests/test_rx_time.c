/* tests/test_rx_time.c */

/**
 * @file test_rx_time.c
 * @brief Unit Tests for Time Interface and Mock Implementation
 *
 * Tests mock time functionality including:
 * - Initialization
 * - Time advancement
 * - Sleep behavior
 * - Elapsed time checking
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "unity.h"

#include "mock_time.h"
#include "rx_time_interface.h"

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static mock_time_t         s_mock;
static rx_time_interface_t s_iface;

void setUp(void)
{
  mock_time_init(&s_mock);
  mock_time_get_interface(&s_iface, &s_mock);
}

void tearDown(void)
{
  mock_time_deinit(&s_mock);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

void test_mock_time_init_zeros_state(void)
{
  mock_time_t mock;
  mock_time_init(&mock);

  TEST_ASSERT_TRUE(mock.initialized);
  TEST_ASSERT_EQUAL_UINT32(0, mock.current_time_ms);
  TEST_ASSERT_EQUAL_UINT32(0, mock.sleep_call_count);
  TEST_ASSERT_EQUAL_UINT32(0, mock.total_sleep_ms);
  TEST_ASSERT_FALSE(mock.auto_advance);
}

void test_mock_time_deinit_clears_state(void)
{
  mock_time_t mock;
  mock_time_init(&mock);
  mock_time_advance(&mock, 100);

  mock_time_deinit(&mock);

  TEST_ASSERT_FALSE(mock.initialized);
  TEST_ASSERT_EQUAL_UINT32(0, mock.current_time_ms);
}

void test_mock_time_get_interface_null_fails(void)
{
  rx_err_t err = mock_time_get_interface(NULL, &s_mock);

  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_mock_time_get_interface_populates_iface(void)
{
  rx_time_interface_t iface;
  rx_err_t            err = mock_time_get_interface(&iface, &s_mock);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_NULL(iface.sleep_ms);
  TEST_ASSERT_NOT_NULL(iface.get_ms);
  TEST_ASSERT_NOT_NULL(iface.is_elapsed);
  TEST_ASSERT_EQUAL_PTR(&s_mock, iface.ctx);
}

/* =============================================================================
 * Time Advancement Tests
 * =============================================================================
 */

void test_mock_time_advance_increments_time(void)
{
  mock_time_advance(&s_mock, 100);

  TEST_ASSERT_EQUAL_UINT32(100, s_iface.get_ms(s_iface.ctx));
}

void test_mock_time_advance_accumulates(void)
{
  mock_time_advance(&s_mock, 50);
  mock_time_advance(&s_mock, 30);
  mock_time_advance(&s_mock, 20);

  TEST_ASSERT_EQUAL_UINT32(100, s_iface.get_ms(s_iface.ctx));
}

void test_mock_time_set_overrides_time(void)
{
  mock_time_advance(&s_mock, 100);
  mock_time_set(&s_mock, 500);

  TEST_ASSERT_EQUAL_UINT32(500, s_iface.get_ms(s_iface.ctx));
}

/* =============================================================================
 * Sleep Tests
 * =============================================================================
 */

void test_mock_time_sleep_increments_count(void)
{
  s_iface.sleep_ms(s_iface.ctx, 10);

  TEST_ASSERT_EQUAL_UINT32(1, mock_time_get_sleep_count(&s_mock));
}

void test_mock_time_sleep_accumulates_total(void)
{
  s_iface.sleep_ms(s_iface.ctx, 10);
  s_iface.sleep_ms(s_iface.ctx, 20);
  s_iface.sleep_ms(s_iface.ctx, 30);

  TEST_ASSERT_EQUAL_UINT32(60, mock_time_get_total_sleep(&s_mock));
  TEST_ASSERT_EQUAL_UINT32(3, mock_time_get_sleep_count(&s_mock));
}

void test_mock_time_sleep_no_auto_advance_by_default(void)
{
  s_iface.sleep_ms(s_iface.ctx, 100);

  /* Time should NOT advance because auto_advance is off */
  TEST_ASSERT_EQUAL_UINT32(0, s_iface.get_ms(s_iface.ctx));
}

void test_mock_time_sleep_with_auto_advance(void)
{
  mock_time_set_auto_advance(&s_mock, true);

  s_iface.sleep_ms(s_iface.ctx, 50);

  /* Time should advance by sleep amount */
  TEST_ASSERT_EQUAL_UINT32(50, s_iface.get_ms(s_iface.ctx));
}

void test_mock_time_sleep_auto_advance_accumulates(void)
{
  mock_time_set_auto_advance(&s_mock, true);

  s_iface.sleep_ms(s_iface.ctx, 10);
  s_iface.sleep_ms(s_iface.ctx, 20);
  s_iface.sleep_ms(s_iface.ctx, 30);

  TEST_ASSERT_EQUAL_UINT32(60, s_iface.get_ms(s_iface.ctx));
}

/* =============================================================================
 * Elapsed Time Tests
 * =============================================================================
 */

void test_mock_time_is_elapsed_false_before_timeout(void)
{
  uint32_t start = s_iface.get_ms(s_iface.ctx);

  mock_time_advance(&s_mock, 50);

  TEST_ASSERT_FALSE(s_iface.is_elapsed(s_iface.ctx, start, 100));
}

void test_mock_time_is_elapsed_true_at_timeout(void)
{
  uint32_t start = s_iface.get_ms(s_iface.ctx);

  mock_time_advance(&s_mock, 100);

  TEST_ASSERT_TRUE(s_iface.is_elapsed(s_iface.ctx, start, 100));
}

void test_mock_time_is_elapsed_true_after_timeout(void)
{
  uint32_t start = s_iface.get_ms(s_iface.ctx);

  mock_time_advance(&s_mock, 150);

  TEST_ASSERT_TRUE(s_iface.is_elapsed(s_iface.ctx, start, 100));
}

void test_mock_time_is_elapsed_handles_wraparound(void)
{
  /* Set time near max value */
  mock_time_set(&s_mock, 0xFFFFFFF0);

  uint32_t start = s_iface.get_ms(s_iface.ctx);

  /* Advance past wraparound */
  mock_time_advance(&s_mock, 0x20);

  /* Should correctly detect elapsed time across wraparound */
  TEST_ASSERT_TRUE(s_iface.is_elapsed(s_iface.ctx, start, 0x10));
}

/* =============================================================================
 * Reset Counter Tests
 * =============================================================================
 */

void test_mock_time_reset_counters_clears_counts(void)
{
  s_iface.sleep_ms(s_iface.ctx, 100);
  mock_time_advance(&s_mock, 50);

  mock_time_reset_counters(&s_mock);

  TEST_ASSERT_EQUAL_UINT32(0, mock_time_get_sleep_count(&s_mock));
  TEST_ASSERT_EQUAL_UINT32(0, mock_time_get_total_sleep(&s_mock));
}

void test_mock_time_reset_counters_preserves_time(void)
{
  mock_time_advance(&s_mock, 500);
  s_iface.sleep_ms(s_iface.ctx, 100);

  mock_time_reset_counters(&s_mock);

  /* Time should be preserved */
  TEST_ASSERT_EQUAL_UINT32(500, s_iface.get_ms(s_iface.ctx));
}

/* =============================================================================
 * Interface Validation Tests
 * =============================================================================
 */

void test_time_interface_validate_null_fails(void)
{
  rx_err_t err = rx_time_interface_validate(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_time_interface_validate_success(void)
{
  rx_err_t err = rx_time_interface_validate(&s_iface);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_time_interface_validate_missing_sleep_fails(void)
{
  rx_time_interface_t bad_iface = {0};
  bad_iface.get_ms              = s_iface.get_ms;
  bad_iface.is_elapsed          = s_iface.is_elapsed;
  /* sleep_ms is NULL */

  rx_err_t err = rx_time_interface_validate(&bad_iface);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_time_interface_validate_missing_get_ms_fails(void)
{
  rx_time_interface_t bad_iface = {0};
  bad_iface.sleep_ms            = s_iface.sleep_ms;
  bad_iface.is_elapsed          = s_iface.is_elapsed;
  /* get_ms is NULL */

  rx_err_t err = rx_time_interface_validate(&bad_iface);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Global Instance Tests (NULL mock parameter)
 * =============================================================================
 */

void test_mock_time_null_uses_global(void)
{
  /* Init global */
  mock_time_init(NULL);

  rx_time_interface_t global_iface;
  mock_time_get_interface(&global_iface, NULL);

  /* Should work with global instance */
  mock_time_advance(NULL, 100);
  TEST_ASSERT_EQUAL_UINT32(100, global_iface.get_ms(global_iface.ctx));

  mock_time_deinit(NULL);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_mock_time_init_zeros_state);
  RUN_TEST(test_mock_time_deinit_clears_state);
  RUN_TEST(test_mock_time_get_interface_null_fails);
  RUN_TEST(test_mock_time_get_interface_populates_iface);

  /* Time advancement tests */
  RUN_TEST(test_mock_time_advance_increments_time);
  RUN_TEST(test_mock_time_advance_accumulates);
  RUN_TEST(test_mock_time_set_overrides_time);

  /* Sleep tests */
  RUN_TEST(test_mock_time_sleep_increments_count);
  RUN_TEST(test_mock_time_sleep_accumulates_total);
  RUN_TEST(test_mock_time_sleep_no_auto_advance_by_default);
  RUN_TEST(test_mock_time_sleep_with_auto_advance);
  RUN_TEST(test_mock_time_sleep_auto_advance_accumulates);

  /* Elapsed time tests */
  RUN_TEST(test_mock_time_is_elapsed_false_before_timeout);
  RUN_TEST(test_mock_time_is_elapsed_true_at_timeout);
  RUN_TEST(test_mock_time_is_elapsed_true_after_timeout);
  RUN_TEST(test_mock_time_is_elapsed_handles_wraparound);

  /* Reset counter tests */
  RUN_TEST(test_mock_time_reset_counters_clears_counts);
  RUN_TEST(test_mock_time_reset_counters_preserves_time);

  /* Interface validation tests */
  RUN_TEST(test_time_interface_validate_null_fails);
  RUN_TEST(test_time_interface_validate_success);
  RUN_TEST(test_time_interface_validate_missing_sleep_fails);
  RUN_TEST(test_time_interface_validate_missing_get_ms_fails);

  /* Global instance tests */
  RUN_TEST(test_mock_time_null_uses_global);

  return UNITY_END();
}
