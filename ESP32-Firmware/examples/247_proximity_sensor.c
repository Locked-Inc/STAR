/**
 * @file 247_proximity_sensor.c
 * @brief VL53L0X laser proximity sensor
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "PROXIMITY";

#define VL53L0X_ADDR (0x29)

void proximity_sensor_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Proximity_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "vl53l0x", I2C_NUM_0, VL53L0X_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  ESP_LOGI(s_tag, "VL53L0X proximity sensor started");

  size_t bytes_written;
  size_t bytes_read;

  for (int i = 0; i < 200; i++) {
    /* Start measurement */
    uint8_t start = 0x01;
    star_bus_i2c_write(&manager, "vl53l0x", &start, 1, 0x00, &bytes_written);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Read result */
    uint8_t data[2];
    star_bus_i2c_read(&manager, "vl53l0x", data, 2, 0x1E, &bytes_read);

    uint16_t distance_mm = (data[0] << 8) | data[1];

    if (distance_mm > 20 && distance_mm < 2000) {
      ESP_LOGI(s_tag, "Distance: %d mm", distance_mm);

      if (distance_mm < 100) ESP_LOGI(s_tag, "Object very close!");
    } else {
      ESP_LOGI(s_tag, "Out of range");
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { proximity_sensor_example(); }
