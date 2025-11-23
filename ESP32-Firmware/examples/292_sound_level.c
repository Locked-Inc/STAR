/**
 * @file 292_sound_level.c
 * @brief Sound level meter / audio envelope
 */

#include <esp_log.h>
#include <driver/adc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "SOUND";

#define MIC_CH ADC1_CHANNEL_6  /* GPIO34 */

void sound_level_example(void)
{
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(MIC_CH, ADC_ATTEN_DB_11);

  ESP_LOGI(s_tag, "Sound level meter started");

  for (int i = 0; i < 200; i++) {
    /* Sample rapidly for envelope detection */
    int min_val = 4095, max_val = 0;
    for (int s = 0; s < 100; s++) {
      int val = adc1_get_raw(MIC_CH);
      if (val < min_val) min_val = val;
      if (val > max_val) max_val = val;
    }

    int amplitude = max_val - min_val;
    int db_approx = 20 + (amplitude * 60 / 4095);

    /* Visual meter */
    int bars = amplitude / 200;
    char meter[21] = "####################";
    if (bars < 20) meter[bars] = '\0';

    ESP_LOGI(s_tag, "[%-20s] ~%ddB", meter, db_approx);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void app_main(void) { sound_level_example(); }
