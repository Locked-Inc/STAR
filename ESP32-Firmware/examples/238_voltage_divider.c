/**
 * @file 238_voltage_divider.c
 * @brief Battery voltage monitoring with divider
 */

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "VOLTAGE";

#define VBAT_ADC_CHANNEL  (ADC_CHANNEL_0)
#define R1                100000.0f  /* Upper resistor */
#define R2                10000.0f   /* Lower resistor */
#define DIVIDER_RATIO     ((R1 + R2) / R2)

void voltage_divider_example(void)
{
  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
  adc_oneshot_new_unit(&init_cfg, &adc);

  adc_oneshot_chan_cfg_t chan_cfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
  adc_oneshot_config_channel(adc, VBAT_ADC_CHANNEL, &chan_cfg);

  adc_cali_handle_t cali;
  adc_cali_line_fitting_config_t cali_cfg = {
    .unit_id = ADC_UNIT_1, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12
  };
  adc_cali_create_scheme_line_fitting(&cali_cfg, &cali);

  ESP_LOGI(s_tag, "Voltage monitor started (divider ratio: %.1f)", DIVIDER_RATIO);

  for (int i = 0; i < 100; i++) {
    int raw, mv;
    adc_oneshot_read(adc, VBAT_ADC_CHANNEL, &raw);
    adc_cali_raw_to_voltage(cali, raw, &mv);

    float actual_voltage = (mv / 1000.0f) * DIVIDER_RATIO;
    int percent = (int)((actual_voltage - 3.0f) / (4.2f - 3.0f) * 100);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    ESP_LOGI(s_tag, "Battery: %.2f V (%d%%)", actual_voltage, percent);

    if (actual_voltage < 3.3f) ESP_LOGI(s_tag, "LOW BATTERY!");

    vTaskDelay(pdMS_TO_TICKS(5000));
  }

  adc_cali_delete_scheme_line_fitting(cali);
  adc_oneshot_del_unit(adc);
}

void app_main(void) { voltage_divider_example(); }
