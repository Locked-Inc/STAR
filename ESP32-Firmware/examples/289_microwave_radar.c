/**
 * @file 289_microwave_radar.c
 * @brief RCWL-0516 microwave radar presence detector
 */

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "RADAR";

#define RADAR_PIN (GPIO_NUM_26)

void microwave_radar_example(void)
{
  gpio_set_direction(RADAR_PIN, GPIO_MODE_INPUT);

  ESP_LOGI(s_tag, "RCWL-0516 microwave radar started");

  bool last_state = false;
  int presence_duration = 0;

  for (int i = 0; i < 300; i++) {
    bool detected = gpio_get_level(RADAR_PIN);

    if (detected && !last_state) {
      ESP_LOGI(s_tag, "Motion DETECTED - presence started");
      presence_duration = 0;
    } else if (!detected && last_state) {
      ESP_LOGI(s_tag, "Motion ENDED - duration: %ds", presence_duration / 10);
    }

    if (detected) {
      presence_duration++;
    }

    last_state = detected;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void app_main(void) { microwave_radar_example(); }
