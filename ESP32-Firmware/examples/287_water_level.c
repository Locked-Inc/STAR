/**
 * @file 287_water_level.c
 * @brief Water level sensor
 */

#include <esp_log.h>
#include <driver/adc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "WATER_LEVEL";

#define LEVEL_CH ADC1_CHANNEL_4  /* GPIO32 */

void water_level_example(void)
{
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(LEVEL_CH, ADC_ATTEN_DB_11);

  ESP_LOGI(s_tag, "Water level sensor started");

  for (int i = 0; i < 100; i++) {
    int raw = adc1_get_raw(LEVEL_CH);
    int level = raw * 100 / 4095;

    const char *status;
    if (level < 10) status = "EMPTY";
    else if (level < 25) status = "LOW";
    else if (level < 50) status = "QUARTER";
    else if (level < 75) status = "HALF";
    else if (level < 90) status = "HIGH";
    else status = "FULL";

    ESP_LOGI(s_tag, "Raw:%d Level:%d%% - %s", raw, level, status);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void app_main(void) { water_level_example(); }
