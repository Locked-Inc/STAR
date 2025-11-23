/**
 * @file 237_current_sensor.c
 * @brief ACS712 current sensor
 */

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "CURRENT";

#define ACS712_ADC_CHANNEL  (ADC_CHANNEL_7)
#define ACS712_SENSITIVITY  0.066f  /* 66mV/A for 30A version */
#define VREF_MV             1650    /* Zero current voltage */
#define SAMPLES (100)

void current_sensor_example(void)
{
  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
  adc_oneshot_new_unit(&init_cfg, &adc);

  adc_oneshot_chan_cfg_t chan_cfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
  adc_oneshot_config_channel(adc, ACS712_ADC_CHANNEL, &chan_cfg);

  ESP_LOGI(s_tag, "ACS712 current sensor started");

  for (int i = 0; i < 200; i++) {
    int64_t sum = 0;
    int64_t sum_sq = 0;

    for (int s = 0; s < SAMPLES; s++) {
      int raw;
      adc_oneshot_read(adc, ACS712_ADC_CHANNEL, &raw);
      int mv = raw * 3300 / 4095;
      int offset_mv = mv - VREF_MV;
      sum += offset_mv;
      sum_sq += offset_mv * offset_mv;
      esp_rom_delay_us(200);
    }

    float avg_mv = sum / (float)SAMPLES;
    float rms_mv = sqrtf(sum_sq / (float)SAMPLES);

    float dc_current = avg_mv / 1000.0f / ACS712_SENSITIVITY;
    float ac_current = rms_mv / 1000.0f / ACS712_SENSITIVITY;

    float power_w = fabsf(ac_current) * 220.0f;  /* Assume 220V */

    ESP_LOGI(s_tag, "DC: %.2f A, AC RMS: %.2f A, Power: %.1f W", dc_current, ac_current, power_w);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  adc_oneshot_del_unit(adc);
}

void app_main(void) { current_sensor_example(); }
