/**
 * @file 265_pressure_bmp280.c
 * @brief BMP280 barometric pressure sensor
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "BMP280";

#define BMP280_ADDR (0x76)

void pressure_bmp280_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Pressure_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "bmp280", I2C_NUM_0, BMP280_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Configure: normal mode, 16x oversampling */
  uint8_t cfg[] = {0x57};
  size_t bytes_written;
  star_bus_i2c_write(&manager, "bmp280", cfg, 1, 0xF4, &bytes_written);

  ESP_LOGI(s_tag, "BMP280 pressure sensor started");

  for (int i = 0; i < 100; i++) {
    uint8_t data[6];
    size_t bytes_read;
    star_bus_i2c_read(&manager, "bmp280", data, 6, 0xF7, &bytes_read);

    int32_t press_raw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    int32_t temp_raw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);

    /* Simplified conversion (needs calibration for accuracy) */
    float temp = temp_raw / 5120.0f;
    float press = press_raw / 256.0f;

    ESP_LOGI(s_tag, "Temp: %.1fC, Pressure: %.1f Pa", temp, press);
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { pressure_bmp280_example(); }
