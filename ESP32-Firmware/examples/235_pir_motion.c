/**
 * @file 235_pir_motion.c
 * @brief PIR motion sensor with debouncing
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "PIR";

#define PIR_PIN (GPIO_NUM_27)
#define LED_PIN (GPIO_NUM_25)

void pir_motion_example(void)
{
  gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
  gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

  ESP_LOGI(s_tag, "PIR motion sensor started");

  bool motion_detected = false;
  int motion_count = 0;
  uint32_t last_motion_time = 0;

  for (int i = 0; i < 600; i++) {
    bool current = gpio_get_level(PIR_PIN);

    if (current && !motion_detected) {
      motion_detected = true;
      motion_count++;
      last_motion_time = xTaskGetTickCount();
      ESP_LOGI(s_tag, "Motion detected! Count: %d", motion_count);
      gpio_set_level(LED_PIN, 1);
    } else if (!current && motion_detected) {
      if (xTaskGetTickCount() - last_motion_time > pdMS_TO_TICKS(2000)) {
        motion_detected = false;
        ESP_LOGI(s_tag, "Motion ended");
        gpio_set_level(LED_PIN, 0);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ESP_LOGI(s_tag, "Total motions detected: %d", motion_count);
}

void app_main(void) { pir_motion_example(); }
