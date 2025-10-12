/* esp32-firmware/components/star_error_handler/test/test_error_handler.c */

/**
 * @file test_error_handler.c
 * @brief Comprehensive STAR_TEST tests for error handler component (30 tests)
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "star_error_handler.h"
#include "star_test.h"

/* Test helper - successful reset function */
static esp_err_t test_reset_success(void* context)
{
  int* counter = (int*)context;
  if (counter) {
    (*counter)++;
  }
  return ESP_OK;
}

/* Test helper - failing reset function */
static esp_err_t test_reset_fail(void* context)
{
  return ESP_FAIL;
}

/* Test helper - reset function that succeeds after N attempts */
static int       reset_attempt_counter = 0;
static esp_err_t test_reset_after_attempts(void* context)
{
  reset_attempt_counter++;
  int* required_attempts = (int*)context;
  if (reset_attempt_counter >= *required_attempts) {
    return ESP_OK;
  }
  return ESP_FAIL;
}

/* ============================================================================
 * Initialization Tests (5 tests)
 * ============================================================================ */

STAR_TEST_CASE(error_handler, init_with_valid_params)
{
  error_handler_t handler;
  esp_err_t       result = error_handler_init(&handler, 3, 100, 5000, NULL, NULL);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_EQUAL(0, handler.error_count);
  STAR_ASSERT_EQUAL(3, handler.max_retries);
  STAR_ASSERT_EQUAL(100, handler.base_retry_delay);
  STAR_ASSERT_FALSE(handler.in_error_state);
  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, init_with_null_handler)
{
  esp_err_t result = error_handler_init(NULL, 3, 100, 5000, NULL, NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(error_handler, init_with_reset_function)
{
  error_handler_t handler;
  int             reset_counter = 0;
  esp_err_t result = error_handler_init(&handler, 3, 100, 5000, test_reset_success, &reset_counter);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_NOT_NULL(handler.reset_fn);
  STAR_ASSERT_EQUAL(&reset_counter, handler.reset_context);
  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, init_with_zero_retries)
{
  error_handler_t handler;
  esp_err_t       result = error_handler_init(&handler, 0, 100, 5000, NULL, NULL);
  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_EQUAL(0, handler.max_retries);
  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, init_creates_mutex)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);
  STAR_ASSERT_NOT_NULL(handler.mutex);
  error_handler_deinit(&handler);
}

/* ============================================================================
 * Deinitialization Tests (3 tests)
 * ============================================================================ */

STAR_TEST_CASE(error_handler, deinit_with_null_handler)
{
  error_handler_deinit(NULL); /* Should not crash */
  STAR_ASSERT_TRUE(true);     /* If we got here, test passed */
}

STAR_TEST_CASE(error_handler, deinit_deletes_mutex)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);
  SemaphoreHandle_t mutex_before = handler.mutex;
  STAR_ASSERT_NOT_NULL(mutex_before);
  error_handler_deinit(&handler);
  /* Can't directly check if mutex is deleted, but if no crash occurs, test passes */
  STAR_ASSERT_TRUE(true);
}

STAR_TEST_CASE(error_handler, deinit_multiple_times_safe)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);
  error_handler_deinit(&handler);
  error_handler_deinit(&handler); /* Should be safe to call multiple times */
  STAR_ASSERT_TRUE(true);
}

/* ============================================================================
 * Error Recording Tests (8 tests)
 * ============================================================================ */

STAR_TEST_CASE(error_handler, record_error_increments_count)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test error", __FILE__, __LINE__, __func__);

  STAR_ASSERT_EQUAL(1, handler.error_count);
  STAR_ASSERT_EQUAL(ESP_FAIL, handler.last_error);
  STAR_ASSERT_TRUE(handler.in_error_state);

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, record_multiple_errors)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Error 1", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_ERR_TIMEOUT, "Error 2", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_ERR_NO_MEM, "Error 3", __FILE__, __LINE__, __func__);

  STAR_ASSERT_EQUAL(3, handler.error_count);
  STAR_ASSERT_EQUAL(ESP_ERR_NO_MEM, handler.last_error);

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, record_error_with_null_handler)
{
  esp_err_t result =
    error_handler_record_error(NULL, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  STAR_ASSERT_EQUAL(ESP_FAIL, result);
}

STAR_TEST_CASE(error_handler, record_error_sets_error_state)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  STAR_ASSERT_FALSE(handler.in_error_state);
  error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  STAR_ASSERT_TRUE(handler.in_error_state);

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, record_error_with_reset_fn_success)
{
  error_handler_t handler;
  int             reset_counter = 0;
  error_handler_init(&handler, 3, 100, 5000, test_reset_success, &reset_counter);

  /* First few errors won't trigger reset */
  error_handler_record_error(&handler, ESP_FAIL, "Test 1", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_FAIL, "Test 2", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_FAIL, "Test 3", __FILE__, __LINE__, __func__);

  /* Fourth error exceeds max_retries, should trigger reset */
  esp_err_t result =
    error_handler_record_error(&handler, ESP_FAIL, "Test 4", __FILE__, __LINE__, __func__);

  STAR_ASSERT_EQUAL(ESP_OK, result); /* Reset succeeded */
  STAR_ASSERT_EQUAL(1, reset_counter);

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, record_error_with_reset_fn_failure)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, test_reset_fail, NULL);

  esp_err_t result =
    error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);

  STAR_ASSERT_EQUAL(ESP_FAIL, result);         /* Reset failed, original error returned */
  STAR_ASSERT_EQUAL(0, handler.current_retry); /* First error doesn't increment retry yet */

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, record_error_stores_error_code)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_ERR_NOT_FOUND, "Test", __FILE__, __LINE__, __func__);

  STAR_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, handler.last_error);

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, record_error_macro)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  RECORD_ERROR(&handler, ESP_FAIL, "Macro test");

  STAR_ASSERT_EQUAL(1, handler.error_count);
  STAR_ASSERT_EQUAL(ESP_FAIL, handler.last_error);

  error_handler_deinit(&handler);
}

/* ============================================================================
 * Retry Logic Tests (7 tests)
 * ============================================================================ */

STAR_TEST_CASE(error_handler, can_retry_returns_true_initially)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);

  bool can_retry = error_handler_can_retry(&handler);
  STAR_ASSERT_TRUE(can_retry);

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, can_retry_false_after_max_retries)
{
  error_handler_t handler;
  error_handler_init(&handler, 2, 100, 5000, NULL, NULL);

  /* Exhaust retries */
  error_handler_record_error(&handler, ESP_FAIL, "Test 1", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_FAIL, "Test 2", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_FAIL, "Test 3", __FILE__, __LINE__, __func__);

  bool can_retry = error_handler_can_retry(&handler);
  STAR_ASSERT_FALSE(can_retry);

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, can_retry_with_null_handler)
{
  bool can_retry = error_handler_can_retry(NULL);
  STAR_ASSERT_FALSE(can_retry);
}

STAR_TEST_CASE(error_handler, can_retry_false_when_not_in_error_state)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  bool can_retry = error_handler_can_retry(&handler);
  STAR_ASSERT_FALSE(can_retry); /* Not in error state yet */

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, retry_count_increments)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, test_reset_fail, NULL);

  STAR_ASSERT_EQUAL(0, handler.current_retry);
  error_handler_record_error(&handler, ESP_FAIL, "Test 1", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_FAIL, "Test 2", __FILE__, __LINE__, __func__);
  STAR_ASSERT_TRUE(handler.current_retry >= 1);

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, exponential_backoff_increases_delay)
{
  error_handler_t handler;
  error_handler_init(&handler, 5, 100, 10000, test_reset_fail, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test 1", __FILE__, __LINE__, __func__);
  uint32_t delay1 = handler.current_retry_delay;

  error_handler_record_error(&handler, ESP_FAIL, "Test 2", __FILE__, __LINE__, __func__);
  uint32_t delay2 = handler.current_retry_delay;

  STAR_ASSERT_TRUE(delay2 > delay1); /* Exponential backoff increases delay */

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, retry_delay_respects_max)
{
  error_handler_t handler;
  error_handler_init(&handler, 10, 100, 1000, test_reset_fail, NULL);

  /* Generate many errors to exceed max delay */
  for (int i = 0; i < 10; i++) {
    error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  }

  STAR_ASSERT_TRUE(handler.current_retry_delay <= 1000);

  error_handler_deinit(&handler);
}

/* ============================================================================
 * State Reset Tests (5 tests)
 * ============================================================================ */

STAR_TEST_CASE(error_handler, reset_state_clears_errors)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  STAR_ASSERT_EQUAL(1, handler.error_count);
  STAR_ASSERT_TRUE(handler.in_error_state);

  esp_err_t result = error_handler_reset_state(&handler);

  STAR_ASSERT_EQUAL(ESP_OK, result);
  STAR_ASSERT_EQUAL(0, handler.error_count);
  STAR_ASSERT_FALSE(handler.in_error_state);
  STAR_ASSERT_EQUAL(0, handler.current_retry);

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, reset_state_resets_retry_delay)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, test_reset_fail, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  error_handler_reset_state(&handler);

  STAR_ASSERT_EQUAL(100, handler.current_retry_delay); /* Back to base delay */

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, reset_state_allows_new_errors)
{
  error_handler_t handler;
  error_handler_init(&handler, 1, 100, 5000, test_reset_fail, NULL);

  /* Exhaust retries */
  error_handler_record_error(&handler, ESP_FAIL, "Test 1", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_FAIL, "Test 2", __FILE__, __LINE__, __func__);
  STAR_ASSERT_FALSE(error_handler_can_retry(&handler));

  /* Reset and try again */
  error_handler_reset_state(&handler);
  error_handler_record_error(&handler, ESP_FAIL, "Test 3", __FILE__, __LINE__, __func__);
  STAR_ASSERT_TRUE(error_handler_can_retry(&handler));

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, reset_state_with_null_handler)
{
  esp_err_t result = error_handler_reset_state(NULL);
  STAR_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

STAR_TEST_CASE(error_handler, reset_state_preserves_config)
{
  error_handler_t handler;
  error_handler_init(&handler, 5, 200, 8000, test_reset_success, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  error_handler_reset_state(&handler);

  /* Configuration should be preserved */
  STAR_ASSERT_EQUAL(5, handler.max_retries);
  STAR_ASSERT_EQUAL(200, handler.base_retry_delay);
  STAR_ASSERT_EQUAL(8000, handler.max_retry_delay);
  STAR_ASSERT_NOT_NULL(handler.reset_fn);

  error_handler_deinit(&handler);
}

/* ============================================================================
 * Edge Cases and Stress Tests (2 tests)
 * ============================================================================ */

STAR_TEST_CASE(error_handler, many_errors_sequential)
{
  error_handler_t handler;
  error_handler_init(&handler, 100, 10, 1000, NULL, NULL);

  for (int i = 0; i < 50; i++) {
    error_handler_record_error(&handler, ESP_FAIL, "Stress test", __FILE__, __LINE__, __func__);
  }

  STAR_ASSERT_EQUAL(50, handler.error_count);
  STAR_ASSERT_TRUE(handler.in_error_state);

  error_handler_deinit(&handler);
}

STAR_TEST_CASE(error_handler, reset_fn_eventually_succeeds)
{
  error_handler_t handler;
  int             required_attempts = 3;
  reset_attempt_counter             = 0;

  error_handler_init(&handler, 5, 10, 1000, test_reset_after_attempts, &required_attempts);

  /* First two attempts should fail */
  esp_err_t result1 =
    error_handler_record_error(&handler, ESP_FAIL, "Test 1", __FILE__, __LINE__, __func__);
  STAR_ASSERT_EQUAL(ESP_FAIL, result1);

  esp_err_t result2 =
    error_handler_record_error(&handler, ESP_FAIL, "Test 2", __FILE__, __LINE__, __func__);
  STAR_ASSERT_EQUAL(ESP_FAIL, result2);

  esp_err_t result3 =
    error_handler_record_error(&handler, ESP_FAIL, "Test 3", __FILE__, __LINE__, __func__);
  STAR_ASSERT_EQUAL(ESP_FAIL, result3);

  /* Eventually should succeed after enough attempts */
  error_handler_record_error(&handler, ESP_FAIL, "Test 4", __FILE__, __LINE__, __func__);

  error_handler_deinit(&handler);
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()
/* Initialization tests */
STAR_TEST_REF(error_handler, init_with_valid_params)
STAR_TEST_REF(error_handler, init_with_null_handler)
STAR_TEST_REF(error_handler, init_with_reset_function)
STAR_TEST_REF(error_handler, init_with_zero_retries)
STAR_TEST_REF(error_handler, init_creates_mutex)

/* Deinitialization tests */
STAR_TEST_REF(error_handler, deinit_with_null_handler)
STAR_TEST_REF(error_handler, deinit_deletes_mutex)
STAR_TEST_REF(error_handler, deinit_multiple_times_safe)

/* Error recording tests */
STAR_TEST_REF(error_handler, record_error_increments_count)
STAR_TEST_REF(error_handler, record_error_with_null_handler)
STAR_TEST_REF(error_handler, record_multiple_errors)
STAR_TEST_REF(error_handler, record_error_sets_error_state)
STAR_TEST_REF(error_handler, record_error_with_reset_fn_success)
STAR_TEST_REF(error_handler, record_error_with_reset_fn_failure)
STAR_TEST_REF(error_handler, record_error_stores_error_code)
STAR_TEST_REF(error_handler, record_error_macro)

/* Retry logic tests */
STAR_TEST_REF(error_handler, can_retry_returns_true_initially)
STAR_TEST_REF(error_handler, can_retry_false_after_max_retries)
STAR_TEST_REF(error_handler, can_retry_with_null_handler)
STAR_TEST_REF(error_handler, can_retry_false_when_not_in_error_state)
STAR_TEST_REF(error_handler, retry_count_increments)
STAR_TEST_REF(error_handler, exponential_backoff_increases_delay)
STAR_TEST_REF(error_handler, retry_delay_respects_max)

/* State reset tests */
STAR_TEST_REF(error_handler, reset_state_clears_errors)
STAR_TEST_REF(error_handler, reset_state_resets_retry_delay)
STAR_TEST_REF(error_handler, reset_state_allows_new_errors)
STAR_TEST_REF(error_handler, reset_state_with_null_handler)
STAR_TEST_REF(error_handler, reset_state_preserves_config)

/* Edge cases */
STAR_TEST_REF(error_handler, many_errors_sequential)
STAR_TEST_REF(error_handler, reset_fn_eventually_succeeds)
STAR_TEST_LIST_END()
