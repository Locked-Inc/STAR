/**
 * @file 178_mq135_co2_monitor.c
 * @brief MQ135 CO2 concentration monitor
 */

#include "star_sensor_mq135.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "CO2_MONITOR";

void co2_monitor_example(void)
{
  mq135_handle_t sensor;
  mq135_config_t cfg = {
    .adc_channel = ADC1_CHANNEL_0,
    .attenuation = ADC_ATTEN_DB_11,
    .r0 = 76.63f,   /* Calibrated in clean air */
    .rl = 10000.0f  /* 10k load resistor */
  };
  star_sensor_mq135_init(&sensor, &cfg);

  ESP_LOGI(s_tag, "Warming up sensor (2 min recommended)...");
  vTaskDelay(pdMS_TO_TICKS(30000));  /* 30 sec warmup for demo */

  for (int i = 0; i < 100; i++) {
    mq135_data_t data;
    if (star_sensor_mq135_read(&sensor, &data) == ESP_OK) {
      ESP_LOGI(s_tag, "CO2: %.0f ppm, NH3: %.2f ppm, Alcohol: %.3f ppm",
               data.ppm_co2, data.ppm_nh3, data.ppm_alcohol);

      if (data.ppm_co2 > 1000) {
        ESP_LOGW(s_tag, "High CO2 - ventilate!");
      } else if (data.ppm_co2 > 800) {
        ESP_LOGI(s_tag, "CO2 elevated");
      } else {
        ESP_LOGI(s_tag, "Air quality: Good");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  star_sensor_mq135_deinit(&sensor);
}

void app_main(void) { co2_monitor_example(); }
