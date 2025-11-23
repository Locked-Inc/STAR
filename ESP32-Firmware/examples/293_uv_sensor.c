/**
 * @file 293_uv_sensor.c
 * @brief ML8511 UV index sensor
 */

#include <esp_log.h>
#include <driver/adc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "UV";

#define UV_CH ADC1_CHANNEL_7   /* GPIO35 */
#define REF_CH ADC1_CHANNEL_4  /* GPIO32 - 3.3V ref */

void uv_sensor_example(void)
{
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(UV_CH, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(REF_CH, ADC_ATTEN_DB_11);

  ESP_LOGI(s_tag, "ML8511 UV sensor started");

  for (int i = 0; i < 100; i++) {
    int uv_raw = adc1_get_raw(UV_CH);
    int ref_raw = adc1_get_raw(REF_CH);

    float uv_voltage = (uv_raw * 3.3f) / 4095.0f;
    float ref_voltage = (ref_raw * 3.3f) / 4095.0f;

    /* Ratiometric conversion */
    float corrected = (uv_voltage / ref_voltage) * 3.3f;

    /* ML8511: 1V = 0 UV, 2.8V = 15 UV index */
    float uv_index = (corrected - 1.0f) * 15.0f / 1.8f;
    if (uv_index < 0) uv_index = 0;

    const char *risk;
    if (uv_index < 3) risk = "Low";
    else if (uv_index < 6) risk = "Moderate";
    else if (uv_index < 8) risk = "High";
    else if (uv_index < 11) risk = "Very High";
    else risk = "Extreme";

    ESP_LOGI(s_tag, "UV Index: %.1f - %s risk", uv_index, risk);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void) { uv_sensor_example(); }
