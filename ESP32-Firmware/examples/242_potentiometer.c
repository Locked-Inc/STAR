/**
 * @file 242_potentiometer.c
 * @brief Multiple potentiometer inputs
 */

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "POTS";

#define POT1_CH (ADC_CHANNEL_0)
#define POT2_CH (ADC_CHANNEL_3)
#define POT3_CH (ADC_CHANNEL_6)

void potentiometer_example(void)
{
  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
  adc_oneshot_new_unit(&init_cfg, &adc);

  adc_oneshot_chan_cfg_t cfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
  adc_oneshot_config_channel(adc, POT1_CH, &cfg);
  adc_oneshot_config_channel(adc, POT2_CH, &cfg);
  adc_oneshot_config_channel(adc, POT3_CH, &cfg);

  ESP_LOGI(s_tag, "Potentiometers started");

  for (int i = 0; i < 200; i++) {
    int p1, p2, p3;
    adc_oneshot_read(adc, POT1_CH, &p1);
    adc_oneshot_read(adc, POT2_CH, &p2);
    adc_oneshot_read(adc, POT3_CH, &p3);

    int pct1 = p1 * 100 / 4095;
    int pct2 = p2 * 100 / 4095;
    int pct3 = p3 * 100 / 4095;

    ESP_LOGI(s_tag, "Pot1: %3d%% | Pot2: %3d%% | Pot3: %3d%%", pct1, pct2, pct3);

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  adc_oneshot_del_unit(adc);
}

void app_main(void) { potentiometer_example(); }
