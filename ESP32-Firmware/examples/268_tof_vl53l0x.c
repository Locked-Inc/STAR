/**
 * @file 268_tof_vl53l0x.c
 * @brief VL53L0X time-of-flight distance sensor
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "TOF";

#define VL53L0X_ADDR (0x29)

static star_bus_config_t *tof_bus;
static star_bus_manager_t *tof_manager;

static void priv_tof_write_reg(uint8_t reg, uint8_t val) {
  uint8_t buf[1] = {val};
  size_t bytes_written;
  star_bus_i2c_write(tof_manager, "vl53l0x", buf, 1, reg, &bytes_written);
}

static uint8_t priv_tof_read_reg(uint8_t reg) {
  uint8_t val;
  size_t bytes_read;
  star_bus_i2c_read(tof_manager, "vl53l0x", &val, 1, reg, &bytes_read);
  return val;
}

void tof_vl53l0x_example(void)
{
  static star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TOF_Manager", NULL, NULL);
  tof_manager = &manager;

  tof_bus = star_bus_config_create_i2c(
    "vl53l0x", I2C_NUM_0, VL53L0X_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, tof_bus);
  star_bus_config_init(tof_bus, &manager);

  /* Basic initialization */
  priv_tof_write_reg(0x80, 0x01);
  priv_tof_write_reg(0xFF, 0x01);
  priv_tof_write_reg(0x00, 0x00);
  priv_tof_write_reg(0xFF, 0x00);
  priv_tof_write_reg(0x80, 0x00);

  ESP_LOGI(s_tag, "VL53L0X ToF sensor started");

  for (int i = 0; i < 100; i++) {
    /* Start single measurement */
    priv_tof_write_reg(0x00, 0x01);
    vTaskDelay(pdMS_TO_TICKS(30));

    /* Read result */
    uint8_t data[2];
    size_t bytes_read;
    star_bus_i2c_read(&manager, "vl53l0x", data, 2, 0x14 + 10, &bytes_read);

    uint16_t distance_mm = (data[0] << 8) | data[1];
    ESP_LOGI(s_tag, "Distance: %d mm", distance_mm);

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { tof_vl53l0x_example(); }
