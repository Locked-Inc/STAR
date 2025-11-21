/* Test pin validator component with Unity */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "star_pin_validator.h"

#include <string.h>

/* Thread safety test globals */
static volatile int g_thread_errors = 0;
static volatile int g_tasks_completed = 0;

void setUp(void)
{
  star_free_pin_validator();
}

void tearDown(void)
{
  star_free_pin_validator();
}

/* Basic Registration Tests */

void test_register_single_pin(void)
{
  esp_err_t result = star_register_pin(GPIO_NUM_4, "Test Pin", false);
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_register_shared_pin(void)
{
  esp_err_t result = star_register_pin(GPIO_NUM_4, "Shared Pin", true);
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_register_multiple_shared_pins(void)
{
  esp_err_t result1 = star_register_pin(GPIO_NUM_4, "Shared 1", true);
  esp_err_t result2 = star_register_pin(GPIO_NUM_4, "Shared 2", true);

  TEST_ASSERT_EQUAL(ESP_OK, result1);
  TEST_ASSERT_EQUAL(ESP_OK, result2);
}

/* Conflict Detection Tests */

void test_exclusive_pin_conflict(void)
{
  star_register_pin(GPIO_NUM_4, "First", false);
  esp_err_t result = star_register_pin(GPIO_NUM_4, "Second", false);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);
}

void test_shared_then_exclusive_conflict(void)
{
  star_register_pin(GPIO_NUM_4, "Shared", true);
  esp_err_t result = star_register_pin(GPIO_NUM_4, "Exclusive", false);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);
}

void test_exclusive_then_shared_conflict(void)
{
  star_register_pin(GPIO_NUM_4, "Exclusive", false);
  esp_err_t result = star_register_pin(GPIO_NUM_4, "Shared", true);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);
}

/* Validation Tests */

void test_validate_no_conflicts(void)
{
  star_register_pin(GPIO_NUM_4, "Pin A", false);
  star_register_pin(GPIO_NUM_5, "Pin B", false);

  esp_err_t result = star_validate_pins();
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_validate_with_shared_pins(void)
{
  star_register_pin(GPIO_NUM_4, "Shared 1", true);
  star_register_pin(GPIO_NUM_4, "Shared 2", true);
  star_register_pin(GPIO_NUM_5, "Exclusive", false);

  esp_err_t result = star_validate_pins();
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

/* Clear Tests */

void test_clear_all_pins(void)
{
  star_register_pin(GPIO_NUM_4, "Pin", false);
  star_free_pin_validator();

  /* Should be able to register again after clear */
  esp_err_t result = star_register_pin(GPIO_NUM_4, "Pin Again", false);
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

/* NULL and Invalid Input Tests */

void test_register_null_description(void)
{
  esp_err_t result = star_register_pin(GPIO_NUM_4, NULL, false);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_register_invalid_pin(void)
{
  esp_err_t result = star_register_pin(GPIO_NUM_MAX, "Invalid", false);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

/* Unregister Tests */

void test_unregister_pin(void)
{
  star_register_pin(GPIO_NUM_4, "Test Pin", false);
  esp_err_t result = star_unregister_pin(GPIO_NUM_4, "Test Pin");
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Should be able to register same pin again */
  result = star_register_pin(GPIO_NUM_4, "New Test Pin", false);
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_unregister_nonexistent_pin(void)
{
  esp_err_t result = star_unregister_pin(GPIO_NUM_4, "Nonexistent");
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, result);
}

void test_unregister_wrong_description(void)
{
  star_register_pin(GPIO_NUM_4, "Correct Desc", false);
  esp_err_t result = star_unregister_pin(GPIO_NUM_4, "Wrong Desc");
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, result);
}

/* Thread Safety Tests */

static void concurrent_register_task(void* arg)
{
  int task_id = (int)(intptr_t)arg;
  char desc[32];

  for (int i = 0; i < 10; i++) {
    snprintf(desc, sizeof(desc), "Task%d_Pin%d", task_id, i);
    /* Use different pins for each task to avoid conflicts */
    gpio_num_t pin = (gpio_num_t)(GPIO_NUM_10 + task_id);
    esp_err_t result = star_register_pin(pin, desc, true);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
      g_thread_errors++;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  g_tasks_completed++;
  vTaskDelete(NULL);
}

void test_concurrent_register(void)
{
  g_thread_errors = 0;
  g_tasks_completed = 0;

  /* Spawn 3 tasks that register pins concurrently */
  xTaskCreate(concurrent_register_task, "t1", 2048, (void*)1, 5, NULL);
  xTaskCreate(concurrent_register_task, "t2", 2048, (void*)2, 5, NULL);
  xTaskCreate(concurrent_register_task, "t3", 2048, (void*)3, 5, NULL);

  /* Wait for tasks to complete */
  while (g_tasks_completed < 3) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  TEST_ASSERT_EQUAL(0, g_thread_errors);
}

static void concurrent_register_unregister_task(void* arg)
{
  int task_id = (int)(intptr_t)arg;
  char desc[32];
  gpio_num_t pin = (gpio_num_t)(GPIO_NUM_15 + task_id);

  for (int i = 0; i < 5; i++) {
    snprintf(desc, sizeof(desc), "Task%d_Iter%d", task_id, i);
    esp_err_t result = star_register_pin(pin, desc, true);
    if (result != ESP_OK) {
      g_thread_errors++;
    }
    vTaskDelay(pdMS_TO_TICKS(1));

    result = star_unregister_pin(pin, desc);
    if (result != ESP_OK) {
      g_thread_errors++;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  g_tasks_completed++;
  vTaskDelete(NULL);
}

void test_concurrent_register_unregister(void)
{
  g_thread_errors = 0;
  g_tasks_completed = 0;

  /* Spawn 3 tasks that register and unregister pins */
  xTaskCreate(concurrent_register_unregister_task, "t1", 2048, (void*)1, 5, NULL);
  xTaskCreate(concurrent_register_unregister_task, "t2", 2048, (void*)2, 5, NULL);
  xTaskCreate(concurrent_register_unregister_task, "t3", 2048, (void*)3, 5, NULL);

  /* Wait for tasks to complete */
  while (g_tasks_completed < 3) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  TEST_ASSERT_EQUAL(0, g_thread_errors);
}

static void concurrent_validate_task(void* arg)
{
  (void)arg;

  for (int i = 0; i < 10; i++) {
    esp_err_t result = star_validate_pins();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
      g_thread_errors++;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  g_tasks_completed++;
  vTaskDelete(NULL);
}

void test_concurrent_validate(void)
{
  g_thread_errors = 0;
  g_tasks_completed = 0;

  /* Register some pins first */
  star_register_pin(GPIO_NUM_4, "Pin1", true);
  star_register_pin(GPIO_NUM_5, "Pin2", true);

  /* Spawn 3 tasks that validate concurrently */
  xTaskCreate(concurrent_validate_task, "v1", 2048, NULL, 5, NULL);
  xTaskCreate(concurrent_validate_task, "v2", 2048, NULL, 5, NULL);
  xTaskCreate(concurrent_validate_task, "v3", 2048, NULL, 5, NULL);

  /* Wait for tasks to complete */
  while (g_tasks_completed < 3) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  TEST_ASSERT_EQUAL(0, g_thread_errors);
}

void test_freeing_prevents_operations(void)
{
  /* Register a pin */
  star_register_pin(GPIO_NUM_4, "Test", false);

  /* Free the validator */
  star_free_pin_validator();

  /* Operations should handle gracefully (either succeed after re-init or fail safely) */
  esp_err_t result = star_register_pin(GPIO_NUM_4, "New Test", false);
  /* Should succeed because free clears state and allows re-init */
  TEST_ASSERT_EQUAL(ESP_OK, result);
}

void test_double_free(void)
{
  star_register_pin(GPIO_NUM_4, "Test", false);

  /* First free should succeed */
  esp_err_t result = star_free_pin_validator();
  TEST_ASSERT_EQUAL(ESP_OK, result);

  /* Second free should be safe (returns OK or INVALID_STATE) */
  result = star_free_pin_validator();
  TEST_ASSERT_TRUE(result == ESP_OK || result == ESP_ERR_INVALID_STATE);
}

int runUnityTests(void)
{
  UNITY_BEGIN();

  /* Basic Registration Tests */
  RUN_TEST(test_register_single_pin);
  RUN_TEST(test_register_shared_pin);
  RUN_TEST(test_register_multiple_shared_pins);

  /* Conflict Detection Tests */
  RUN_TEST(test_exclusive_pin_conflict);
  RUN_TEST(test_shared_then_exclusive_conflict);
  RUN_TEST(test_exclusive_then_shared_conflict);

  /* Validation Tests */
  RUN_TEST(test_validate_no_conflicts);
  RUN_TEST(test_validate_with_shared_pins);

  /* Clear Tests */
  RUN_TEST(test_clear_all_pins);

  /* NULL and Invalid Input Tests */
  RUN_TEST(test_register_null_description);
  RUN_TEST(test_register_invalid_pin);

  /* Unregister Tests */
  RUN_TEST(test_unregister_pin);
  RUN_TEST(test_unregister_nonexistent_pin);
  RUN_TEST(test_unregister_wrong_description);

  /* Thread Safety Tests */
  RUN_TEST(test_concurrent_register);
  RUN_TEST(test_concurrent_register_unregister);
  RUN_TEST(test_concurrent_validate);
  RUN_TEST(test_freeing_prevents_operations);
  RUN_TEST(test_double_free);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
