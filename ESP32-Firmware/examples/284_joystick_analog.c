/**
 * @file 284_joystick_analog.c
 * @brief Analog joystick with button
 */

#include <esp_log.h>
#include <driver/adc.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "JOYSTICK";

#define JOY_X_CH ADC1_CHANNEL_6  /* GPIO34 */
#define JOY_Y_CH ADC1_CHANNEL_7  /* GPIO35 */
#define JOY_BTN  (GPIO_NUM_32)

void joystick_analog_example(void)
{
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(JOY_X_CH, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(JOY_Y_CH, ADC_ATTEN_DB_11);

  gpio_set_direction(JOY_BTN, GPIO_MODE_INPUT);
  gpio_pullup_en(JOY_BTN);

  ESP_LOGI(s_tag, "Analog joystick started");

  for (int i = 0; i < 200; i++) {
    int x_raw = adc1_get_raw(JOY_X_CH);
    int y_raw = adc1_get_raw(JOY_Y_CH);
    bool btn = !gpio_get_level(JOY_BTN);

    /* Normalize to -100 to +100 */
    int x = ((x_raw - 2048) * 100) / 2048;
    int y = ((y_raw - 2048) * 100) / 2048;

    /* Dead zone */
    if (x > -10 && x < 10) x = 0;
    if (y > -10 && y < 10) y = 0;

    const char *dir = "CENTER";
    if (y < -50) dir = "UP";
    else if (y > 50) dir = "DOWN";
    else if (x < -50) dir = "LEFT";
    else if (x > 50) dir = "RIGHT";

    ESP_LOGI(s_tag, "X:%+4d Y:%+4d %s %s", x, y, dir, btn ? "[PRESSED]" : "");
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void app_main(void) { joystick_analog_example(); }
