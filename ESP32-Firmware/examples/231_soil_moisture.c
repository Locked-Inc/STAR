/**
 * @file 231_soil_moisture.c
 * @brief Capacitive soil moisture sensor
 */

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "SOIL";

#define SOIL_ADC_CHANNEL  (ADC_CHANNEL_4)
#define PUMP_RELAY_PIN    (GPIO_NUM_25)

#define MOISTURE_DRY (3000)
#define MOISTURE_WET (1500)
#define WATER_THRESHOLD   40  /* % */

void soil_moisture_example(void)
{
  gpio_set_direction(PUMP_RELAY_PIN, GPIO_MODE_OUTPUT);

  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
  adc_oneshot_new_unit(&init_cfg, &adc);

  adc_oneshot_chan_cfg_t chan_cfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
  adc_oneshot_config_channel(adc, SOIL_ADC_CHANNEL, &chan_cfg);

  ESP_LOGI(s_tag, "Soil moisture monitor started");

  for (int i = 0; i < 200; i++) {
    int raw;
    adc_oneshot_read(adc, SOIL_ADC_CHANNEL, &raw);

    int moisture_percent = 100 - (100 * (raw - MOISTURE_WET) / (MOISTURE_DRY - MOISTURE_WET));
    if (moisture_percent < 0) moisture_percent = 0;
    if (moisture_percent > 100) moisture_percent = 100;

    ESP_LOGI(s_tag, "Soil Moisture: %d%% (raw: %d)", moisture_percent, raw);

    if (moisture_percent < WATER_THRESHOLD) {
      ESP_LOGI(s_tag, "Soil dry - activating pump");
      gpio_set_level(PUMP_RELAY_PIN, 1);
      vTaskDelay(pdMS_TO_TICKS(2000));
      gpio_set_level(PUMP_RELAY_PIN, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }

  adc_oneshot_del_unit(adc);
}

void app_main(void) { soil_moisture_example(); }
