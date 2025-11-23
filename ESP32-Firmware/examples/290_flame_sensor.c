/**
 * @file 290_flame_sensor.c
 * @brief IR flame detection sensor
 */

#include <esp_log.h>
#include <driver/adc.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "FLAME";

#define FLAME_ANALOG ADC1_CHANNEL_5  /* GPIO33 */
#define FLAME_DIGITAL (GPIO_NUM_34)
#define BUZZER_PIN (GPIO_NUM_25)

void flame_sensor_example(void)
{
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(FLAME_ANALOG, ADC_ATTEN_DB_11);
  gpio_set_direction(FLAME_DIGITAL, GPIO_MODE_INPUT);
  gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

  ESP_LOGI(s_tag, "Flame sensor started - monitoring...");

  for (int i = 0; i < 200; i++) {
    int analog = adc1_get_raw(FLAME_ANALOG);
    bool digital = !gpio_get_level(FLAME_DIGITAL);

    int intensity = (4095 - analog) * 100 / 4095;

    if (digital || intensity > 30) {
      ESP_LOGI(s_tag, "FIRE DETECTED! Intensity:%d%%", intensity);
      gpio_set_level(BUZZER_PIN, 1);
      vTaskDelay(pdMS_TO_TICKS(100));
      gpio_set_level(BUZZER_PIN, 0);
    } else {
      ESP_LOGI(s_tag, "OK - IR level:%d%%", intensity);
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }

  gpio_set_level(BUZZER_PIN, 0);
}

void app_main(void) { flame_sensor_example(); }
