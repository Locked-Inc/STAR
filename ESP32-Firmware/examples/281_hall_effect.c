/**
 * @file 281_hall_effect.c
 * @brief Digital hall effect sensor (external sensor on GPIO)
 * 
 * Note: ESP32 built-in hall sensor was deprecated in ESP-IDF 5.x
 * This example demonstrates using an external digital hall effect sensor
 * connected to a GPIO pin (e.g., A3144, OH137, SS441A)
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "HALL";

#define HALL_SENSOR_PIN GPIO_NUM_34  /* Digital hall sensor output */

void hall_effect_example(void)
{
  /* Configure GPIO for hall sensor input */
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << HALL_SENSOR_PIN),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&io_conf);

  ESP_LOGI(s_tag, "Digital hall effect sensor started on GPIO %d", HALL_SENSOR_PIN);
  ESP_LOGI(s_tag, "Bring a magnet close to detect magnetic field");

  int detections = 0;
  int last_state = -1;

  for (int i = 0; i < 200; i++) {
    int state = gpio_get_level(HALL_SENSOR_PIN);

    if (state != last_state) {
      if (state == 0) {
        detections++;
        ESP_LOGI(s_tag, "Magnetic field detected! (Count: %d)", detections);
      } else {
        ESP_LOGI(s_tag, "Magnetic field removed");
      }
      last_state = state;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ESP_LOGI(s_tag, "Total detections: %d", detections);
}

void app_main(void) { hall_effect_example(); }
