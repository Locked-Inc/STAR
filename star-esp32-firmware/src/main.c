/* src/main.c - Minimal stub for STAR ESP32 Firmware */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* s_TAG = "STAR_MAIN";

void app_main(void)
{
  ESP_LOGI(s_TAG, "STAR ESP32 Firmware - Ready for application code");

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
