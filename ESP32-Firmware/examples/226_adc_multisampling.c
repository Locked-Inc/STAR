/**
 * @file 226_adc_multisampling.c
 * @brief ADC with oversampling and averaging
 */

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "ADC_MULTI";

#define ADC_CHANNEL   (ADC_CHANNEL_0)
#define OVERSAMPLE (64)

void adc_multisampling_example(void)
{
  adc_oneshot_unit_handle_t adc_handle;
  adc_oneshot_unit_init_cfg_t init_cfg = {
    .unit_id = ADC_UNIT_1,
  };
  adc_oneshot_new_unit(&init_cfg, &adc_handle);

  adc_oneshot_chan_cfg_t chan_cfg = {
    .bitwidth = ADC_BITWIDTH_12,
    .atten = ADC_ATTEN_DB_12,
  };
  adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg);

  /* Calibration */
  adc_cali_handle_t cali_handle = NULL;
  adc_cali_line_fitting_config_t cali_cfg = {
    .unit_id = ADC_UNIT_1,
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
  };
  adc_cali_create_scheme_line_fitting(&cali_cfg, &cali_handle);

  ESP_LOGI(s_tag, "ADC multisampling started (oversample: %d)", OVERSAMPLE);

  for (int i = 0; i < 100; i++) {
    int64_t sum = 0;
    int min_val = 4095, max_val = 0;

    /* Collect oversampled readings */
    for (int s = 0; s < OVERSAMPLE; s++) {
      int raw;
      adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);
      sum += raw;
      if (raw < min_val) min_val = raw;
      if (raw > max_val) max_val = raw;
    }

    int avg_raw = sum / OVERSAMPLE;
    int voltage_mv;
    adc_cali_raw_to_voltage(cali_handle, avg_raw, &voltage_mv);

    /* Calculate standard deviation */
    int64_t sq_diff_sum = 0;
    for (int s = 0; s < OVERSAMPLE; s++) {
      int raw;
      adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);
      sq_diff_sum += (raw - avg_raw) * (raw - avg_raw);
    }
    float std_dev = sqrtf((float)sq_diff_sum / OVERSAMPLE);

    ESP_LOGI(s_tag, "Raw: %d, Voltage: %d mV, Range: %d-%d, StdDev: %.1f",
             avg_raw, voltage_mv, min_val, max_val, std_dev);

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  adc_cali_delete_scheme_line_fitting(cali_handle);
  adc_oneshot_del_unit(adc_handle);
}

void app_main(void) { adc_multisampling_example(); }
