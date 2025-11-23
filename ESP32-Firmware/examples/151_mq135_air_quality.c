/**
 * @file 151_mq135_air_quality.c
 * @brief MQ135 air quality gas sensor example
 *
 * Demonstrates:
 * - Sensor calibration
 * - PPM calculation for various gases
 * - Air quality index estimation
 */

#include "star_sensor_mq135.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "MQ135_EXAMPLE";

#define MQ135_ADC_CHANNEL   (ADC1_CHANNEL_0)  /* GPIO36 on ESP32 */
#define MQ135_RL_OHMS       (10000.0f)        /* Load resistor value (10kΩ) */

void mq135_example(void)
{
  esp_err_t ret;

  mq135_handle_t sensor;
  mq135_config_t cfg = {
    .adc_channel = MQ135_ADC_CHANNEL,
    .attenuation = ADC_ATTEN_DB_11,
    .r0 = 0.0f,  /* Will be set during calibration */
    .rl = MQ135_RL_OHMS
  };

  ret = star_sensor_mq135_init(&sensor, &cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init MQ135");
    return;
  }

  ESP_LOGI(s_tag, "MQ135 initialized - warming up (20 seconds recommended)");
  vTaskDelay(pdMS_TO_TICKS(20000));

  /* Calibrate in clean air */
  ESP_LOGI(s_tag, "Calibrating in clean air...");
  float r0_value;
  ret = star_sensor_mq135_calibrate(&sensor, 100, &r0_value);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Calibration complete, R0 = %.2f ohms (%.2f kohms)",
             sensor.r0, sensor.r0 / 1000.0f);
  }

  /* Main reading loop */
  for (int i = 0; i < 100; i++) {
    mq135_data_t data;
    ret = star_sensor_mq135_read(&sensor, &data);

    if (ret == ESP_OK) {
      const char *quality;
      if (data.ppm_co2 < 400) {
        quality = "Excellent";
      } else if (data.ppm_co2 < 600) {
        quality = "Good";
      } else if (data.ppm_co2 < 1000) {
        quality = "Moderate";
      } else if (data.ppm_co2 < 2000) {
        quality = "Poor";
      } else {
        quality = "Dangerous";
      }

      ESP_LOGI(s_tag, "CO2: %.0f ppm, NH3: %.1f ppm, Alcohol: %.1f ppm (%s)",
               data.ppm_co2, data.ppm_nh3, data.ppm_alcohol, quality);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  star_sensor_mq135_deinit(&sensor);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { mq135_example(); }
