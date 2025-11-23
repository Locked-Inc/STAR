/**
 * @file 248_sound_level.c
 * @brief Sound level meter with microphone
 */

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "SOUND";

#define MIC_ADC_CHANNEL (ADC_CHANNEL_4)
#define SAMPLES (500)

void sound_level_example(void)
{
  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
  adc_oneshot_new_unit(&init_cfg, &adc);

  adc_oneshot_chan_cfg_t cfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
  adc_oneshot_config_channel(adc, MIC_ADC_CHANNEL, &cfg);

  ESP_LOGI(s_tag, "Sound level meter started");

  for (int iter = 0; iter < 100; iter++) {
    int64_t sum_sq = 0;
    int min_val = 4095, max_val = 0;

    for (int i = 0; i < SAMPLES; i++) {
      int raw;
      adc_oneshot_read(adc, MIC_ADC_CHANNEL, &raw);

      int centered = raw - 2048;
      sum_sq += centered * centered;

      if (raw < min_val) min_val = raw;
      if (raw > max_val) max_val = raw;

      esp_rom_delay_us(100);
    }

    float rms = sqrtf(sum_sq / (float)SAMPLES);
    float db = 20.0f * log10f(rms + 1);
    int peak_to_peak = max_val - min_val;

    ESP_LOGI(s_tag, "dB: %.1f, RMS: %.0f, P2P: %d", db, rms, peak_to_peak);

    if (db > 50) ESP_LOGI(s_tag, "LOUD!");

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  adc_oneshot_del_unit(adc);
}

void app_main(void) { sound_level_example(); }
