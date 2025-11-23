/**
 * @file 251_heart_rate.c
 * @brief MAX30102 heart rate and SpO2 sensor
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "HEART_RATE";

#define MAX30102_ADDR (0x57)

void heart_rate_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "HR_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "max30102", I2C_NUM_0, MAX30102_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  size_t bytes_written;
  uint8_t val;

  /* Reset and configure */
  val = 0x40;
  star_bus_i2c_write(&manager, "max30102", &val, 1, 0x09, &bytes_written);
  vTaskDelay(pdMS_TO_TICKS(100));

  val = 0x03;
  star_bus_i2c_write(&manager, "max30102", &val, 1, 0x09, &bytes_written);
  val = 0x27;
  star_bus_i2c_write(&manager, "max30102", &val, 1, 0x0A, &bytes_written);
  val = 0x24;
  star_bus_i2c_write(&manager, "max30102", &val, 1, 0x0C, &bytes_written);
  val = 0x24;
  star_bus_i2c_write(&manager, "max30102", &val, 1, 0x0D, &bytes_written);

  ESP_LOGI(s_tag, "MAX30102 heart rate sensor started");
  ESP_LOGI(s_tag, "Place finger on sensor...");

  uint32_t last_beat_time = 0;
  int bpm = 0;
  size_t bytes_read;

  for (int i = 0; i < 300; i++) {
    uint8_t data[6];
    star_bus_i2c_read(&manager, "max30102", data, 6, 0x07, &bytes_read);

    uint32_t red = (data[0] << 16) | (data[1] << 8) | data[2];
    uint32_t ir = (data[3] << 16) | (data[4] << 8) | data[5];

    /* Simple beat detection */
    static uint32_t prev_ir = 0;
    if (ir > 50000 && prev_ir > 0) {
      if (ir > prev_ir * 1.05f) {  /* Rising edge */
        uint32_t now = xTaskGetTickCount();
        if (last_beat_time > 0) {
          uint32_t interval = now - last_beat_time;
          bpm = 60000 / (interval * portTICK_PERIOD_MS);
          if (bpm > 40 && bpm < 200) {
            ESP_LOGI(s_tag, "Heart rate: %d BPM", bpm);
          }
        }
        last_beat_time = now;
      }
    }
    prev_ir = ir;

    vTaskDelay(pdMS_TO_TICKS(20));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { heart_rate_example(); }
