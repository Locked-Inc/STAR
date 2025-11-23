/**
 * @file 273_current_ina219.c
 * @brief INA219 current/power monitor
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "INA219";

#define INA219_ADDR (0x40)

void current_ina219_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Current_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "ina219", I2C_NUM_0, INA219_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Configure: 32V, 2A, 12-bit */
  uint8_t cfg[] = {0x39, 0x9F};
  size_t bytes_written;
  star_bus_i2c_write(&manager, "ina219", cfg, 2, 0x00, &bytes_written);

  /* Set calibration for 0.1 ohm shunt */
  uint8_t cal[] = {0x10, 0x00};
  star_bus_i2c_write(&manager, "ina219", cal, 2, 0x05, &bytes_written);

  ESP_LOGI(s_tag, "INA219 power monitor started");

  for (int i = 0; i < 100; i++) {
    /* Read bus voltage */
    uint8_t data[2];
    size_t bytes_read;
    star_bus_i2c_read(&manager, "ina219", data, 2, 0x02, &bytes_read);
    int16_t bus_raw = ((data[0] << 8) | data[1]) >> 3;
    float bus_v = bus_raw * 0.004f;

    /* Read shunt voltage */
    star_bus_i2c_read(&manager, "ina219", data, 2, 0x01, &bytes_read);
    int16_t shunt_raw = (data[0] << 8) | data[1];
    float shunt_mv = shunt_raw * 0.01f;

    float current_ma = shunt_mv / 0.1f;  /* 0.1 ohm shunt */
    float power_mw = bus_v * current_ma;

    ESP_LOGI(s_tag, "Bus:%.2fV Shunt:%.2fmV Current:%.1fmA Power:%.1fmW",
             bus_v, shunt_mv, current_ma, power_mw);
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { current_ina219_example(); }
