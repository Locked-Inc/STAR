/**
 * @file 303_freertos_semaphore.c
 * @brief FreeRTOS semaphore synchronization
 */

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static const char *s_tag = "SEMAPHORE";

static SemaphoreHandle_t s_mutex;
static SemaphoreHandle_t s_counting_sem;
static int s_shared_counter = 0;

static void priv_mutex_task(void *arg) {
  int id = (int)(intptr_t)arg;
  for (int i = 0; i < 5; i++) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int local = s_shared_counter;
    vTaskDelay(pdMS_TO_TICKS(10));
    s_shared_counter = local + 1;
    ESP_LOGI(s_tag, "Task %d: counter = %d", id, s_shared_counter);
    xSemaphoreGive(s_mutex);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  vTaskDelete(NULL);
}

static void priv_producer(void *arg) {
  for (int i = 0; i < 10; i++) {
    vTaskDelay(pdMS_TO_TICKS(100));
    xSemaphoreGive(s_counting_sem);
    ESP_LOGI(s_tag, "Produced item %d", i);
  }
  vTaskDelete(NULL);
}

static void priv_consumer(void *arg) {
  int id = (int)(intptr_t)arg;
  for (int i = 0; i < 5; i++) {
    xSemaphoreTake(s_counting_sem, portMAX_DELAY);
    ESP_LOGI(s_tag, "Consumer %d got item", id);
  }
  vTaskDelete(NULL);
}

void freertos_semaphore_example(void)
{
  ESP_LOGI(s_tag, "=== Mutex Demo ===");
  s_mutex = xSemaphoreCreateMutex();

  xTaskCreate(priv_mutex_task, "task1", 2048, (void*)1, 5, NULL);
  xTaskCreate(priv_mutex_task, "task2", 2048, (void*)2, 5, NULL);
  vTaskDelay(pdMS_TO_TICKS(1000));

  ESP_LOGI(s_tag, "Final counter: %d (expected: 10)", s_shared_counter);
  vSemaphoreDelete(s_mutex);

  ESP_LOGI(s_tag, "=== Counting Semaphore Demo ===");
  s_counting_sem = xSemaphoreCreateCounting(10, 0);

  xTaskCreate(priv_producer, "producer", 2048, NULL, 5, NULL);
  xTaskCreate(priv_consumer, "consumer1", 2048, (void*)1, 4, NULL);
  xTaskCreate(priv_consumer, "consumer2", 2048, (void*)2, 4, NULL);
  vTaskDelay(pdMS_TO_TICKS(2000));

  vSemaphoreDelete(s_counting_sem);
}

void app_main(void) { freertos_semaphore_example(); }
