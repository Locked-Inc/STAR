/**
 * @file 285_soil_moisture.c
 * @brief Capacitive soil moisture sensor
 */

#include <esp_log.h>
#include <driver/adc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "SOIL";

#define SOIL_CH ADC1_CHANNEL_0  /* GPIO36 */
#define AIR_VALUE (3500)
#define WATER_VALUE (1500)

void soil_moisture_example(void)
{
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(SOIL_CH, ADC_ATTEN_DB_11);

  ESP_LOGI(s_tag, "Soil moisture sensor started");

  for (int i = 0; i < 100; i++) {
    int raw = adc1_get_raw(SOIL_CH);

    /* Convert to percentage */
    int moisture = (AIR_VALUE - raw) * 100 / (AIR_VALUE - WATER_VALUE);
    if (moisture < 0) moisture = 0;
    if (moisture > 100) moisture = 100;

    const char *status;
    if (moisture < 20) status = "DRY - Water needed!";
    else if (moisture < 40) status = "Slightly dry";
    else if (moisture < 60) status = "Optimal";
    else if (moisture < 80) status = "Moist";
    else status = "Very wet";

    ESP_LOGI(s_tag, "Raw:%d Moisture:%d%% - %s", raw, moisture, status);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void) { soil_moisture_example(); }
