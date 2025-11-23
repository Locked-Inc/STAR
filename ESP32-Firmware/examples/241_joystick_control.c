/**
 * @file 241_joystick_control.c
 * @brief Dual axis joystick input
 */

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "JOYSTICK";

#define JOY_X_CH  (ADC_CHANNEL_0)
#define JOY_Y_CH  (ADC_CHANNEL_3)
#define JOY_BTN   (GPIO_NUM_34)
#define DEADZONE (200)

void joystick_example(void)
{
  gpio_set_direction(JOY_BTN, GPIO_MODE_INPUT);
  gpio_set_pull_mode(JOY_BTN, GPIO_PULLUP_ONLY);

  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
  adc_oneshot_new_unit(&init_cfg, &adc);

  adc_oneshot_chan_cfg_t chan_cfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
  adc_oneshot_config_channel(adc, JOY_X_CH, &chan_cfg);
  adc_oneshot_config_channel(adc, JOY_Y_CH, &chan_cfg);

  ESP_LOGI(s_tag, "Joystick started");

  for (int i = 0; i < 500; i++) {
    int x_raw, y_raw;
    adc_oneshot_read(adc, JOY_X_CH, &x_raw);
    adc_oneshot_read(adc, JOY_Y_CH, &y_raw);
    bool btn = !gpio_get_level(JOY_BTN);

    int x = x_raw - 2048;
    int y = y_raw - 2048;

    if (abs(x) < DEADZONE) x = 0;
    if (abs(y) < DEADZONE) y = 0;

    const char *dir = "CENTER";
    if (y > 500) dir = "UP";
    else if (y < -500) dir = "DOWN";
    else if (x > 500) dir = "RIGHT";
    else if (x < -500) dir = "LEFT";

    ESP_LOGI(s_tag, "X:%4d Y:%4d Dir:%s Btn:%s", x, y, dir, btn ? "PRESSED" : "");

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  adc_oneshot_del_unit(adc);
}

void app_main(void) { joystick_example(); }
