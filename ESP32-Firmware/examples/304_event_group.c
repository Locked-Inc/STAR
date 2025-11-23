/**
 * @file 304_event_group.c
 * @brief FreeRTOS event group synchronization
 */

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

static const char *s_tag = "EVENTS";

static EventGroupHandle_t s_event_group;

#define SENSOR_READY_BIT   (1 << 0)
#define NETWORK_READY_BIT  (1 << 1)
#define DATA_READY_BIT     (1 << 2)
#define ALL_READY_BITS     (SENSOR_READY_BIT | NETWORK_READY_BIT | DATA_READY_BIT)

static void priv_sensor_task(void *arg) {
  ESP_LOGI(s_tag, "Sensor initializing...");
  vTaskDelay(pdMS_TO_TICKS(500));
  ESP_LOGI(s_tag, "Sensor ready!");
  xEventGroupSetBits(s_event_group, SENSOR_READY_BIT);
  vTaskDelete(NULL);
}

static void priv_network_task(void *arg) {
  ESP_LOGI(s_tag, "Network connecting...");
  vTaskDelay(pdMS_TO_TICKS(800));
  ESP_LOGI(s_tag, "Network ready!");
  xEventGroupSetBits(s_event_group, NETWORK_READY_BIT);
  vTaskDelete(NULL);
}

static void priv_data_task(void *arg) {
  ESP_LOGI(s_tag, "Loading data...");
  vTaskDelay(pdMS_TO_TICKS(600));
  ESP_LOGI(s_tag, "Data ready!");
  xEventGroupSetBits(s_event_group, DATA_READY_BIT);
  vTaskDelete(NULL);
}

static void priv_main_task(void *arg) {
  ESP_LOGI(s_tag, "Waiting for all subsystems...");

  EventBits_t bits = xEventGroupWaitBits(
    s_event_group, ALL_READY_BITS, pdTRUE, pdTRUE, pdMS_TO_TICKS(5000));

  if ((bits & ALL_READY_BITS) == ALL_READY_BITS) {
    ESP_LOGI(s_tag, "All systems GO! Starting main operation.");
  } else {
    ESP_LOGI(s_tag, "Timeout waiting for subsystems (bits=0x%lX)", bits);
  }
  vTaskDelete(NULL);
}

void s_event_group_example(void)
{
  s_event_group = xEventGroupCreate();

  xTaskCreate(priv_main_task, "main", 2048, NULL, 5, NULL);
  xTaskCreate(priv_sensor_task, "sensor", 2048, NULL, 4, NULL);
  xTaskCreate(priv_network_task, "network", 2048, NULL, 4, NULL);
  xTaskCreate(priv_data_task, "data", 2048, NULL, 4, NULL);

  vTaskDelay(pdMS_TO_TICKS(3000));
  vEventGroupDelete(s_event_group);
}

void app_main(void) { s_event_group_example(); }
