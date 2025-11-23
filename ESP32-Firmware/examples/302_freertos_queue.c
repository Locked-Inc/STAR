/**
 * @file 302_freertos_queue.c
 * @brief FreeRTOS queue inter-task communication
 */

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

static const char *s_tag = "QUEUE";

typedef struct {
  int id;
  float value;
  int64_t timestamp;
} sensor_data_t;

static QueueHandle_t s_data_queue;

static void priv_producer_task(void *arg) {
  int id = (int)(intptr_t)arg;
  for (int i = 0; i < 10; i++) {
    sensor_data_t data = {
      .id = id,
      .value = id * 10.0f + i * 0.5f,
      .timestamp = esp_timer_get_time()
    };
    xQueueSend(s_data_queue, &data, portMAX_DELAY);
    ESP_LOGI(s_tag, "Producer %d sent: %.1f", id, data.value);
    vTaskDelay(pdMS_TO_TICKS(100 * id));
  }
  vTaskDelete(NULL);
}

static void priv_consumer_task(void *arg) {
  sensor_data_t data;
  int count = 0;
  while (count < 30) {
    if (xQueueReceive(s_data_queue, &data, pdMS_TO_TICKS(500))) {
      ESP_LOGI(s_tag, "Consumer got: id=%d val=%.1f ts=%lld",
               data.id, data.value, data.timestamp);
      count++;
    }
  }
  ESP_LOGI(s_tag, "Consumer done - processed %d items", count);
  vTaskDelete(NULL);
}

void freertos_queue_example(void)
{
  s_data_queue = xQueueCreate(10, sizeof(sensor_data_t));

  xTaskCreate(priv_consumer_task, "consumer", 4096, NULL, 5, NULL);
  xTaskCreate(priv_producer_task, "producer1", 2048, (void*)1, 4, NULL);
  xTaskCreate(priv_producer_task, "producer2", 2048, (void*)2, 4, NULL);
  xTaskCreate(priv_producer_task, "producer3", 2048, (void*)3, 4, NULL);

  vTaskDelay(pdMS_TO_TICKS(5000));
  vQueueDelete(s_data_queue);
}

void app_main(void) { freertos_queue_example(); }
