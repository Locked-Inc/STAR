/**
 * @file 233_rain_detector.c
 * @brief Rain sensor with intensity measurement
 */

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "RAIN";

#define RAIN_ADC_CHANNEL  (ADC_CHANNEL_5)
#define RAIN_DIGITAL_PIN  (GPIO_NUM_34)

void rain_detector_example(void)
{
  gpio_set_direction(RAIN_DIGITAL_PIN, GPIO_MODE_INPUT);

  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
  adc_oneshot_new_unit(&init_cfg, &adc);

  adc_oneshot_chan_cfg_t chan_cfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
  adc_oneshot_config_channel(adc, RAIN_ADC_CHANNEL, &chan_cfg);

  ESP_LOGI(s_tag, "Rain detector started");

  for (int i = 0; i < 200; i++) {
    int raw;
    adc_oneshot_read(adc, RAIN_ADC_CHANNEL, &raw);
    bool digital = !gpio_get_level(RAIN_DIGITAL_PIN);

    int intensity = 100 - (raw * 100 / 4095);

    if (digital) {
      if (intensity > 80) ESP_LOGI(s_tag, "HEAVY RAIN! Intensity: %d%%", intensity);
      else if (intensity > 50) ESP_LOGI(s_tag, "Moderate rain. Intensity: %d%%", intensity);
      else ESP_LOGI(s_tag, "Light rain. Intensity: %d%%", intensity);
    } else {
      ESP_LOGI(s_tag, "No rain detected");
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  adc_oneshot_del_unit(adc);
}

void app_main(void) { rain_detector_example(); }
