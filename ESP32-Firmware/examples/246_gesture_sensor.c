/**
 * @file 246_gesture_sensor.c
 * @brief APDS9960 gesture sensor
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "GESTURE";

#define APDS9960_ADDR (0x39)

void gesture_sensor_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Gesture_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "apds9960", I2C_NUM_0, APDS9960_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Init sensor (simplified) */
  size_t bytes_written;
  uint8_t val;

  val = 0x45;
  star_bus_i2c_write(&manager, "apds9960", &val, 1, 0x80, &bytes_written);
  val = 0x00;
  star_bus_i2c_write(&manager, "apds9960", &val, 1, 0xA3, &bytes_written);

  ESP_LOGI(s_tag, "APDS9960 gesture sensor started");

  size_t bytes_read;

  for (int i = 0; i < 200; i++) {
    uint8_t status;
    star_bus_i2c_read(&manager, "apds9960", &status, 1, 0xAF, &bytes_read);

    if (status & 0x01) {
      uint8_t data[4];
      star_bus_i2c_read(&manager, "apds9960", data, 4, 0xFC, &bytes_read);

      int up = data[0], down = data[1], left = data[2], right = data[3];

      if (up > down + 20) ESP_LOGI(s_tag, "Gesture: DOWN");
      else if (down > up + 20) ESP_LOGI(s_tag, "Gesture: UP");
      else if (left > right + 20) ESP_LOGI(s_tag, "Gesture: RIGHT");
      else if (right > left + 20) ESP_LOGI(s_tag, "Gesture: LEFT");
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { gesture_sensor_example(); }
