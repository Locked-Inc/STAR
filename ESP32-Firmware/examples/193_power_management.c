/**
 * @file 193_power_management.c
 * @brief Low power sensor reading with deep sleep
 */

#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "POWER_MGMT";

#define SLEEP_DURATION_US   (60 * 1000000)  /* 60 seconds */

RTC_DATA_ATTR static int boot_count = 0;
RTC_DATA_ATTR static float last_temp = 0;
RTC_DATA_ATTR static float last_humidity = 0;

void power_management_example(void)
{
  boot_count++;
  ESP_LOGI(s_tag, "Boot count: %d", boot_count);
  ESP_LOGI(s_tag, "Previous readings: Temp=%.1f, Hum=%.1f", last_temp, last_humidity);

  /* Quick sensor read */
  dht22_handle_t dht;
  dht22_config_t cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };
  star_sensor_dht22_init(&dht, &cfg);

  dht22_data_t data;
  if (star_sensor_dht22_read(&dht, &data) == ESP_OK) {
    ESP_LOGI(s_tag, "Current: Temp=%.1f C, Humidity=%.1f%%", data.temperature_c, data.humidity_rh);
    last_temp = data.temperature_c;
    last_humidity = data.humidity_rh;

    /* Check for significant change */
    if (boot_count > 1) {
      float temp_diff = data.temperature_c - last_temp;
      if (temp_diff > 2.0f || temp_diff < -2.0f) {
        ESP_LOGW(s_tag, "Significant temperature change detected!");
      }
    }
  }

  star_sensor_dht22_deinit(&dht);

  ESP_LOGI(s_tag, "Entering deep sleep for %d seconds...", SLEEP_DURATION_US / 1000000);
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
  esp_deep_sleep_start();
}

void app_main(void) { power_management_example(); }
