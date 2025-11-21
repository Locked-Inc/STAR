/* Test error handler component with Unity */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include <string.h>
#include <stdint.h>
#include "star_error_handler.h"
#include "star_error_interface.h"

/* Test helper - successful reset function */
static esp_err_t test_reset_success(void* context)
{
  int32_t* counter = (int32_t*)context;
  if (counter) {
    (*counter)++;
  }
  return ESP_OK;
}

void setUp(void) {}
void tearDown(void) {}

/* Initialization Tests */

void test_init_with_valid_params(void)
{
  error_handler_t handler;
  esp_err_t result = error_handler_init(&handler, 3, 100, 5000, NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_EQUAL(0, handler.error_count);
  TEST_ASSERT_EQUAL(3, handler.max_retries);
  TEST_ASSERT_FALSE(handler.in_error_state);
  error_handler_deinit(&handler);
}

void test_init_with_null_handler(void)
{
  esp_err_t result = error_handler_init(NULL, 3, 100, 5000, NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_init_with_reset_function(void)
{
  error_handler_t handler;
  int32_t reset_counter = 0;
  esp_err_t result = error_handler_init(&handler, 3, 100, 5000, test_reset_success, &reset_counter);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_NOT_NULL(handler.reset_fn);
  error_handler_deinit(&handler);
}

/* Error Recording Tests */

void test_record_error_increments_count(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test error", __FILE__, __LINE__, __func__);

  TEST_ASSERT_EQUAL(1, handler.error_count);
  TEST_ASSERT_EQUAL(ESP_FAIL, handler.last_error);
  TEST_ASSERT_TRUE(handler.in_error_state);

  error_handler_deinit(&handler);
}

void test_record_multiple_errors(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Error 1", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_ERR_TIMEOUT, "Error 2", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_ERR_NO_MEM, "Error 3", __FILE__, __LINE__, __func__);

  TEST_ASSERT_EQUAL(3, handler.error_count);
  TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, handler.last_error);

  error_handler_deinit(&handler);
}

void test_record_error_with_null_handler(void)
{
  esp_err_t result = error_handler_record_error(NULL, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  TEST_ASSERT_EQUAL(ESP_FAIL, result);
}

/* Retry Logic Tests */

void test_can_retry_returns_true_initially(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);

  bool can_retry = error_handler_can_retry(&handler);
  TEST_ASSERT_TRUE(can_retry);

  error_handler_deinit(&handler);
}

void test_can_retry_false_after_max_retries(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 2, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test 1", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_FAIL, "Test 2", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_FAIL, "Test 3", __FILE__, __LINE__, __func__);

  bool can_retry = error_handler_can_retry(&handler);
  TEST_ASSERT_FALSE(can_retry);

  error_handler_deinit(&handler);
}

void test_can_retry_with_null_handler(void)
{
  bool can_retry = error_handler_can_retry(NULL);
  TEST_ASSERT_FALSE(can_retry);
}

/* State Reset Tests */

void test_reset_state_clears_errors(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  TEST_ASSERT_EQUAL(1, handler.error_count);
  TEST_ASSERT_TRUE(handler.in_error_state);

  esp_err_t result = error_handler_reset_state(&handler);

  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_EQUAL(0, handler.error_count);
  TEST_ASSERT_FALSE(handler.in_error_state);

  error_handler_deinit(&handler);
}

void test_reset_state_with_null_handler(void)
{
  esp_err_t result = error_handler_reset_state(NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_reset_state_preserves_config(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 5, 200, 8000, test_reset_success, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  error_handler_reset_state(&handler);

  TEST_ASSERT_EQUAL(5, handler.max_retries);
  TEST_ASSERT_EQUAL(200, handler.base_retry_delay);
  TEST_ASSERT_NOT_NULL(handler.reset_fn);

  error_handler_deinit(&handler);
}

/* Additional Tests */

void test_init_zero_retries(void)
{
  error_handler_t handler;
  esp_err_t result = error_handler_init(&handler, 0, 100, 5000, NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_EQUAL(0, handler.max_retries);
  error_handler_deinit(&handler);
}

void test_init_large_retries(void)
{
  error_handler_t handler;
  esp_err_t result = error_handler_init(&handler, 100, 100, 5000, NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_OK, result);
  TEST_ASSERT_EQUAL(100, handler.max_retries);
  error_handler_deinit(&handler);
}

void test_exponential_backoff(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 5, 100, 5000, NULL, NULL);

  /* First error */
  error_handler_record_error(&handler, ESP_FAIL, "Test 1", __FILE__, __LINE__, __func__);
  uint32_t first_delay = handler.current_retry_delay;

  /* Second error should increase delay */
  error_handler_record_error(&handler, ESP_FAIL, "Test 2", __FILE__, __LINE__, __func__);
  uint32_t second_delay = handler.current_retry_delay;

  TEST_ASSERT_TRUE(second_delay >= first_delay);

  error_handler_deinit(&handler);
}

void test_max_retry_delay_cap(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 10, 1000, 2000, NULL, NULL);

  /* Record many errors to hit max delay */
  for (int i = 0; i < 10; i++) {
    error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  }

  /* Delay should not exceed max */
  TEST_ASSERT_TRUE(handler.current_retry_delay <= 2000);

  error_handler_deinit(&handler);
}

void test_deinit_null_handler(void)
{
  /* Should handle NULL gracefully without crash */
  error_handler_deinit(NULL);
  /* If we reach here without crash, test passes */
}

void test_double_init(void)
{
  error_handler_t handler;
  memset(&handler, 0, sizeof(handler)); /* Ensure mutex is NULL initially */
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  /* Second init should fail to prevent mutex leak */
  esp_err_t result = error_handler_init(&handler, 5, 200, 8000, NULL, NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);
  /* Original config should be preserved */
  TEST_ASSERT_EQUAL(3, handler.max_retries);

  error_handler_deinit(&handler);
}

void test_error_code_preserved(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  esp_err_t test_errors[] = {ESP_FAIL, ESP_ERR_TIMEOUT, ESP_ERR_NO_MEM, ESP_ERR_INVALID_ARG};

  for (int i = 0; i < 4; i++) {
    error_handler_record_error(&handler, test_errors[i], "Test", __FILE__, __LINE__, __func__);
    TEST_ASSERT_EQUAL(test_errors[i], handler.last_error);
  }

  error_handler_deinit(&handler);
}

void test_current_retry_increments(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 5, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test 1", __FILE__, __LINE__, __func__);
  TEST_ASSERT_EQUAL(1, handler.current_retry);

  error_handler_record_error(&handler, ESP_FAIL, "Test 2", __FILE__, __LINE__, __func__);
  TEST_ASSERT_EQUAL(2, handler.current_retry);

  error_handler_deinit(&handler);
}

void test_reset_resets_retry_count(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 5, 100, 5000, NULL, NULL);

  error_handler_record_error(&handler, ESP_FAIL, "Test 1", __FILE__, __LINE__, __func__);
  error_handler_record_error(&handler, ESP_FAIL, "Test 2", __FILE__, __LINE__, __func__);
  TEST_ASSERT_EQUAL(2, handler.current_retry);

  error_handler_reset_state(&handler);
  TEST_ASSERT_EQUAL(0, handler.current_retry);
  TEST_ASSERT_EQUAL(100, handler.current_retry_delay); /* Reset to base delay */

  error_handler_deinit(&handler);
}

void test_interface_adapter_complete_flow(void)
{
  error_handler_t handler;
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  star_error_interface_t iface;
  error_handler_get_interface(&iface, &handler);

  /* Full workflow through interface */
  iface.record_error(iface.ctx, ESP_ERR_TIMEOUT, "Test", __FILE__, __LINE__, __func__);
  TEST_ASSERT_TRUE(iface.can_retry(iface.ctx));

  iface.record_error(iface.ctx, ESP_ERR_TIMEOUT, "Test", __FILE__, __LINE__, __func__);
  iface.record_error(iface.ctx, ESP_ERR_TIMEOUT, "Test", __FILE__, __LINE__, __func__);
  iface.record_error(iface.ctx, ESP_ERR_TIMEOUT, "Test", __FILE__, __LINE__, __func__);

  TEST_ASSERT_FALSE(iface.can_retry(iface.ctx));

  iface.reset_state(iface.ctx);
  TEST_ASSERT_FALSE(iface.can_retry(iface.ctx)); /* No error recorded yet */

  error_handler_deinit(&handler);
}

/* Thread Safety Tests */

static error_handler_t* g_shared_handler = NULL;
static volatile int g_thread_errors = 0;

static void thread_record_error_task(void* arg)
{
  (void)arg; /* Suppress unused parameter warning */
  for (int i = 0; i < 10; i++) {
    esp_err_t result = error_handler_record_error(
      g_shared_handler, ESP_FAIL, "Concurrent test", __FILE__, __LINE__, __func__);
    if (result != ESP_FAIL && result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
      g_thread_errors++;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  vTaskDelete(NULL);
}

void test_concurrent_record_error(void)
{
  error_handler_t handler;
  memset(&handler, 0, sizeof(handler));
  error_handler_init(&handler, 100, 10, 1000, NULL, NULL);
  g_shared_handler = &handler;
  g_thread_errors = 0;

  /* Create multiple tasks that record errors concurrently */
  xTaskCreate(thread_record_error_task, "t1", 2048, (void*)1, 5, NULL);
  xTaskCreate(thread_record_error_task, "t2", 2048, (void*)2, 5, NULL);
  xTaskCreate(thread_record_error_task, "t3", 2048, (void*)3, 5, NULL);

  /* Wait for threads to complete */
  vTaskDelay(pdMS_TO_TICKS(200));

  /* Should have recorded errors from all threads without corruption */
  TEST_ASSERT_EQUAL(0, g_thread_errors);
  TEST_ASSERT_TRUE(handler.error_count >= 20); /* At least 20 errors from 3 tasks */

  error_handler_deinit(&handler);
  g_shared_handler = NULL;
}

void test_deinit_prevents_new_operations(void)
{
  error_handler_t handler;
  memset(&handler, 0, sizeof(handler));
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  /* Record an error first */
  error_handler_record_error(&handler, ESP_FAIL, "Test", __FILE__, __LINE__, __func__);
  TEST_ASSERT_TRUE(handler.in_error_state);

  /* Deinit should succeed */
  error_handler_deinit(&handler);

  /* Operations after deinit should fail gracefully */
  TEST_ASSERT_NULL(handler.mutex);

  /* can_retry should return false for deinitialized handler */
  bool can_retry = error_handler_can_retry(&handler);
  TEST_ASSERT_FALSE(can_retry);

  /* reset_state should return error for deinitialized handler */
  esp_err_t result = error_handler_reset_state(&handler);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);
}

void test_invalid_interface_from_null_handler(void)
{
  star_error_interface_t iface;

  /* Getting interface from NULL handler should set all function pointers to NULL */
  error_handler_get_interface(&iface, NULL);

  TEST_ASSERT_NULL(iface.record_error);
  TEST_ASSERT_NULL(iface.can_retry);
  TEST_ASSERT_NULL(iface.reset_state);
  TEST_ASSERT_NULL(iface.ctx);
}

void test_invalid_interface_from_uninitialized_handler(void)
{
  error_handler_t handler;
  memset(&handler, 0, sizeof(handler)); /* Uninitialized - mutex is NULL */

  star_error_interface_t iface;
  error_handler_get_interface(&iface, &handler);

  /* Should detect uninitialized handler and set NULL */
  TEST_ASSERT_NULL(iface.record_error);
  TEST_ASSERT_NULL(iface.can_retry);
  TEST_ASSERT_NULL(iface.reset_state);
  TEST_ASSERT_NULL(iface.ctx);
}

void test_toctou_protection_after_deinit(void)
{
  error_handler_t handler;
  memset(&handler, 0, sizeof(handler));
  error_handler_init(&handler, 3, 100, 5000, NULL, NULL);

  /* Capture mutex pointer (simulating what happens in record_error) */
  SemaphoreHandle_t captured_mutex = handler.mutex;
  TEST_ASSERT_NOT_NULL(captured_mutex);

  /* Deinit the handler */
  error_handler_deinit(&handler);

  /* The captured mutex pointer is now invalid but handler.mutex is NULL */
  TEST_ASSERT_NULL(handler.mutex);

  /* Operations should fail gracefully due to NULL mutex check */
  esp_err_t result = error_handler_record_error(
    &handler, ESP_FAIL, "After deinit", __FILE__, __LINE__, __func__);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);
}

int runUnityTests(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_init_with_valid_params);
  RUN_TEST(test_init_with_null_handler);
  RUN_TEST(test_init_with_reset_function);
  RUN_TEST(test_record_error_increments_count);
  RUN_TEST(test_record_multiple_errors);
  RUN_TEST(test_record_error_with_null_handler);
  RUN_TEST(test_can_retry_returns_true_initially);
  RUN_TEST(test_can_retry_false_after_max_retries);
  RUN_TEST(test_can_retry_with_null_handler);
  RUN_TEST(test_reset_state_clears_errors);
  RUN_TEST(test_reset_state_with_null_handler);
  RUN_TEST(test_reset_state_preserves_config);
  RUN_TEST(test_init_zero_retries);
  RUN_TEST(test_init_large_retries);
  RUN_TEST(test_exponential_backoff);
  RUN_TEST(test_max_retry_delay_cap);
  RUN_TEST(test_deinit_null_handler);
  RUN_TEST(test_double_init);
  RUN_TEST(test_error_code_preserved);
  RUN_TEST(test_current_retry_increments);
  RUN_TEST(test_reset_resets_retry_count);
  RUN_TEST(test_interface_adapter_complete_flow);

  /* Thread Safety Tests */
  RUN_TEST(test_concurrent_record_error);
  RUN_TEST(test_deinit_prevents_new_operations);
  RUN_TEST(test_invalid_interface_from_null_handler);
  RUN_TEST(test_invalid_interface_from_uninitialized_handler);
  RUN_TEST(test_toctou_protection_after_deinit);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
