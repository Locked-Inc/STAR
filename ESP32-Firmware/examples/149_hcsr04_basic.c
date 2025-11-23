/**
 * @file 149_hcsr04_basic.c
 * @brief Basic HC-SR04 ultrasonic distance sensor example
 *
 * Demonstrates:
 * - Initializing HC-SR04 with trigger/echo pins
 * - Distance measurement with temperature compensation
 * - Non-blocking measurement mode
 */

#include "star_sensor_hcsr04.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "HCSR04_BASIC";

#define TRIGGER_PIN     (GPIO_NUM_5)
#define ECHO_PIN        (GPIO_NUM_18)

void hcsr04_basic_example(void)
{
  esp_err_t ret;

  /* Initialize HC-SR04 */
  hcsr04_handle_t sensor;
  hcsr04_config_t cfg = {
    .trigger_pin = TRIGGER_PIN,
    .echo_pin = ECHO_PIN,
    .temperature_c = 20.0f  /* Initial temperature for speed of sound calc */
  };

  ret = star_sensor_hcsr04_init(&sensor, &cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init HC-SR04: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(TAG, "HC-SR04 initialized (trigger=%d, echo=%d)",
           TRIGGER_PIN, ECHO_PIN);

  /* Main measurement loop */
  for (int i = 0; i < 100; i++) {
    float distance_cm;

    /* Blocking read */
    ret = star_sensor_hcsr04_read_distance(&sensor, &distance_cm);

    if (ret == ESP_OK) {
      const char* proximity;
      if (distance_cm < 10.0f) {
        proximity = "Very Close";
      } else if (distance_cm < 30.0f) {
        proximity = "Close";
      } else if (distance_cm < 100.0f) {
        proximity = "Medium";
      } else if (distance_cm < 200.0f) {
        proximity = "Far";
      } else {
        proximity = "Very Far";
      }

      ESP_LOGI(TAG, "Distance: %.1f cm (%s)", distance_cm, proximity);
    } else if (ret == HCSR04_ERR_TIMEOUT) {
      ESP_LOGW(TAG, "Measurement timeout - no obstacle detected");
    } else if (ret == HCSR04_ERR_OUT_OF_RANGE_MIN) {
      ESP_LOGW(TAG, "Object too close (< 2cm)");
    } else if (ret == HCSR04_ERR_OUT_OF_RANGE_MAX) {
      ESP_LOGW(TAG, "Object too far (> 400cm)");
    } else {
      ESP_LOGE(TAG, "Measurement error: %d", ret);
    }

    vTaskDelay(pdMS_TO_TICKS(100));  /* HC-SR04 needs ~60ms between measurements */
  }

  /* Cleanup */
  star_sensor_hcsr04_deinit(&sensor);
  ESP_LOGI(TAG, "Example complete");
}

void app_main(void)
{
  hcsr04_basic_example();
}
