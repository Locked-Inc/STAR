/**
 * @file 176_hcsr04_parking_sensor.c
 * @brief HC-SR04 parking distance sensor with buzzer
 */

#include "star_sensor_hcsr04.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

static const char *s_tag = "PARKING";

#define BUZZER_PIN (GPIO_NUM_25)
#define TRIGGER_PIN (GPIO_NUM_5)
#define ECHO_PIN (GPIO_NUM_18)

void parking_sensor_example(void)
{
  gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

  hcsr04_handle_t sensor;
  hcsr04_config_t cfg = {
    .trigger_pin = TRIGGER_PIN,
    .echo_pin = ECHO_PIN,
    .temperature_c = 20.0f
  };
  star_sensor_hcsr04_init(&sensor, &cfg);

  for (int i = 0; i < 200; i++) {
    float distance_cm;
    if (star_sensor_hcsr04_read_distance(&sensor, &distance_cm) == ESP_OK) {
      int beep_delay_ms;

      if (distance_cm < 20) {
        beep_delay_ms = 50;  /* Continuous-like */
        ESP_LOGW(s_tag, "STOP! %.1f cm", distance_cm);
      } else if (distance_cm < 50) {
        beep_delay_ms = 100;
        ESP_LOGI(s_tag, "Close: %.1f cm", distance_cm);
      } else if (distance_cm < 100) {
        beep_delay_ms = 300;
        ESP_LOGI(s_tag, "Near: %.1f cm", distance_cm);
      } else if (distance_cm < 200) {
        beep_delay_ms = 500;
        ESP_LOGI(s_tag, "Far: %.1f cm", distance_cm);
      } else {
        beep_delay_ms = 0;  /* No beep */
        ESP_LOGI(s_tag, "Clear: %.1f cm", distance_cm);
      }

      if (beep_delay_ms > 0) {
        gpio_set_level(BUZZER_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(BUZZER_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(beep_delay_ms - 20));
      } else {
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
  }

  star_sensor_hcsr04_deinit(&sensor);
}

void app_main(void) { parking_sensor_example(); }
