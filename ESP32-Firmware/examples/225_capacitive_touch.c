/**
 * @file 225_capacitive_touch.c
 * @brief ESP32 built-in capacitive touch sensing
 */

#include <esp_log.h>
#include <driver/touch_pad.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "CAP_TOUCH";

#define TOUCH_THRESH_NO_USE (0)
#define TOUCH_THRESHOLD (400)

static bool s_touch_states[10] = {false};

void capacitive_touch_example(void)
{
  touch_pad_init();
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
  touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);

  /* Initialize touch pads 0-9 */
  for (int i = 0; i < 10; i++) {
    touch_pad_config(i, TOUCH_THRESH_NO_USE);
  }

  touch_pad_filter_start(10);

  ESP_LOGI(s_tag, "Capacitive touch initialized");

  /* Read baseline values */
  uint16_t baseline[10];
  for (int i = 0; i < 10; i++) {
    touch_pad_read_filtered(i, &baseline[i]);
    ESP_LOGI(s_tag, "Touch %d baseline: %d", i, baseline[i]);
  }

  for (int iter = 0; iter < 500; iter++) {
    for (int i = 0; i < 10; i++) {
      uint16_t value;
      touch_pad_read_filtered(i, &value);

      bool touched = (baseline[i] - value) > TOUCH_THRESHOLD;

      if (touched && !s_touch_states[i]) {
        ESP_LOGI(s_tag, "Touch %d PRESSED (value: %d, baseline: %d)", i, value, baseline[i]);
        s_touch_states[i] = true;
      } else if (!touched && s_touch_states[i]) {
        ESP_LOGI(s_tag, "Touch %d RELEASED", i);
        s_touch_states[i] = false;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }

  touch_pad_deinit();
}

void app_main(void) { capacitive_touch_example(); }
