/**
 * @file 147_dht22_basic.c
 * @brief Basic DHT22 temperature/humidity sensor example
 *
 * Demonstrates:
 * - Initializing DHT22 single-wire sensor
 * - Reading temperature and humidity
 * - Respecting 2-second sample interval
 */

#include "star_sensor_dht22.h"
#include "star_error_handler.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "DHT22_BASIC";

#define DHT22_DATA_PIN  (GPIO_NUM_4)

void dht22_basic_example(void)
{
  esp_err_t ret;

  /* Initialize DHT22 */
  dht22_handle_t sensor;
  dht22_config_t cfg = {
    .data_pin = DHT22_DATA_PIN,
    .enable_pullup = false
  };

  ret = star_sensor_dht22_init(&sensor, &cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init DHT22: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(TAG, "DHT22 initialized on GPIO %d", DHT22_DATA_PIN);

  /* Main reading loop - DHT22 requires 2 second minimum between reads */
  for (int i = 0; i < 30; i++) {
    dht22_data_t data;

    /* Check if enough time has passed since last read */
    bool can_read = false;
    star_sensor_dht22_can_read(&sensor, &can_read);
    if (can_read) {
      ret = star_sensor_dht22_read(&sensor, &data);

      if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Temperature: %.1f C, Humidity: %.1f %%",
                 data.temperature_c, data.humidity_rh);

        /* Calculate heat index (approximate) */
        if (data.temperature_c > 26.0f) {
          float hi = data.temperature_c + 0.5f * (data.humidity_rh - 50.0f) * 0.1f;
          ESP_LOGI(TAG, "Heat Index: %.1f C", hi);
        }

        /* Comfort level assessment */
        if (data.humidity_rh >= 30.0f && data.humidity_rh <= 60.0f &&
            data.temperature_c >= 20.0f && data.temperature_c <= 26.0f) {
          ESP_LOGI(TAG, "Comfort: Ideal");
        } else if (data.humidity_rh > 70.0f) {
          ESP_LOGI(TAG, "Comfort: Too humid");
        } else if (data.humidity_rh < 30.0f) {
          ESP_LOGI(TAG, "Comfort: Too dry");
        }
      } else {
        ESP_LOGW(TAG, "Read failed: %s", esp_err_to_name(ret));
      }
    } else {
      ESP_LOGD(TAG, "Waiting for sensor cooldown...");
    }

    vTaskDelay(pdMS_TO_TICKS(2500));  /* Wait 2.5 seconds between attempts */
  }

  /* Cleanup */
  star_sensor_dht22_deinit(&sensor);
  ESP_LOGI(TAG, "Example complete");
}

void app_main(void)
{
  dht22_basic_example();
}
