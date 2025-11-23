/**
 * @file 234_smoke_detector.c
 * @brief MQ-2 smoke and gas detector
 */

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "SMOKE";

#define MQ2_ADC_CHANNEL  (ADC_CHANNEL_6)
#define BUZZER_PIN       (GPIO_NUM_25)
#define LED_PIN          (GPIO_NUM_26)
#define SMOKE_THRESHOLD (1500)

void smoke_detector_example(void)
{
  gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
  adc_oneshot_new_unit(&init_cfg, &adc);

  adc_oneshot_chan_cfg_t chan_cfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
  adc_oneshot_config_channel(adc, MQ2_ADC_CHANNEL, &chan_cfg);

  ESP_LOGI(s_tag, "Smoke detector started - warming up sensor...");
  vTaskDelay(pdMS_TO_TICKS(30000));

  for (int i = 0; i < 300; i++) {
    int raw;
    adc_oneshot_read(adc, MQ2_ADC_CHANNEL, &raw);

    ESP_LOGI(s_tag, "Gas level: %d", raw);

    if (raw > SMOKE_THRESHOLD) {
      ESP_LOGI(s_tag, "SMOKE/GAS DETECTED! Level: %d", raw);
      for (int j = 0; j < 5; j++) {
        gpio_set_level(BUZZER_PIN, 1);
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(BUZZER_PIN, 0);
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
      }
    } else {
      gpio_set_level(BUZZER_PIN, 0);
      gpio_set_level(LED_PIN, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  adc_oneshot_del_unit(adc);
}

void app_main(void) { smoke_detector_example(); }
