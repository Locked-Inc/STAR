/**
 * @file 280_touch_sensor.c
 * @brief ESP32 capacitive touch sensing
 */

#include <esp_log.h>
#include <driver/touch_pad.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "TOUCH";

#define TOUCH_THRESH (400)

void touch_sensor_example(void)
{
  touch_pad_init();
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
  touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);

  /* Configure touch pads */
  touch_pad_config(TOUCH_PAD_NUM0, TOUCH_THRESH);
  touch_pad_config(TOUCH_PAD_NUM2, TOUCH_THRESH);
  touch_pad_config(TOUCH_PAD_NUM4, TOUCH_THRESH);
  touch_pad_config(TOUCH_PAD_NUM7, TOUCH_THRESH);

  touch_pad_filter_start(10);

  ESP_LOGI(s_tag, "Touch sensor started (GPIO 4, 2, 13, 27)");

  for (int i = 0; i < 200; i++) {
    uint16_t t0, t2, t4, t7;
    touch_pad_read_filtered(TOUCH_PAD_NUM0, &t0);
    touch_pad_read_filtered(TOUCH_PAD_NUM2, &t2);
    touch_pad_read_filtered(TOUCH_PAD_NUM4, &t4);
    touch_pad_read_filtered(TOUCH_PAD_NUM7, &t7);

    bool touched[4] = {
      t0 < TOUCH_THRESH,
      t2 < TOUCH_THRESH,
      t4 < TOUCH_THRESH,
      t7 < TOUCH_THRESH
    };

    if (touched[0] || touched[1] || touched[2] || touched[3]) {
      ESP_LOGI(s_tag, "Touched: T0=%d T2=%d T4=%d T7=%d",
               touched[0], touched[1], touched[2], touched[3]);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  touch_pad_deinit();
}

void app_main(void) { touch_sensor_example(); }
