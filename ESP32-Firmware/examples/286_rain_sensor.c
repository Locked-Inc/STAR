/**
 * @file 286_rain_sensor.c
 * @brief Rain detection sensor
 */

#include <esp_log.h>
#include <driver/adc.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "RAIN";

#define RAIN_ANALOG ADC1_CHANNEL_3  /* GPIO39 */
#define RAIN_DIGITAL (GPIO_NUM_34)

void rain_sensor_example(void)
{
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(RAIN_ANALOG, ADC_ATTEN_DB_11);
  gpio_set_direction(RAIN_DIGITAL, GPIO_MODE_INPUT);

  ESP_LOGI(s_tag, "Rain sensor started");

  for (int i = 0; i < 100; i++) {
    int analog = adc1_get_raw(RAIN_ANALOG);
    bool digital = !gpio_get_level(RAIN_DIGITAL);

    int intensity = (4095 - analog) * 100 / 4095;

    const char *status;
    if (!digital && intensity < 10) status = "No rain";
    else if (intensity < 30) status = "Light rain";
    else if (intensity < 60) status = "Moderate rain";
    else status = "Heavy rain";

    ESP_LOGI(s_tag, "Digital:%d Analog:%d Intensity:%d%% - %s",
             digital, analog, intensity, status);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void app_main(void) { rain_sensor_example(); }
