/**
 * @file 174_dht22_humidity_alert.c
 * @brief DHT22 with humidity threshold alerts
 */

#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "DHT22_ALERT";

#define HUMIDITY_LOW_THRESHOLD (30.0f)
#define HUMIDITY_HIGH_THRESHOLD (70.0f)
void dht22_alert_example(void)
{
  dht22_handle_t sensor;
  dht22_config_t cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };
  star_sensor_dht22_init(&sensor, &cfg);

  for (int i = 0; i < 100; i++) {
    dht22_data_t data;
    if (star_sensor_dht22_read(&sensor, &data) == ESP_OK) {
      ESP_LOGI(s_tag, "Temp: %.1f C, Humidity: %.1f%%", data.temperature_c, data.humidity_rh);

      if (data.humidity_rh < HUMIDITY_LOW_THRESHOLD) {
        ESP_LOGW(s_tag, "LOW HUMIDITY ALERT!");
      } else if (data.humidity_rh > HUMIDITY_HIGH_THRESHOLD) {
        ESP_LOGW(s_tag, "HIGH HUMIDITY ALERT!");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  star_sensor_dht22_deinit(&sensor);
}

void app_main(void) { dht22_alert_example(); }
