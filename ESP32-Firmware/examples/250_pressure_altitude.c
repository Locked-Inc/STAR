/**
 * @file 250_pressure_altitude.c
 * @brief BMP388 high precision barometer
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "BARO";

#define BMP388_ADDR (0x77)
#define SEA_LEVEL_PRESSURE (101325.0f) 

void pressure_altitude_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Baro_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "bmp388", I2C_NUM_0, BMP388_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Init sensor */
  size_t bytes_written;
  uint8_t val;

  val = 0x33;
  star_bus_i2c_write(&manager, "bmp388", &val, 1, 0x1B, &bytes_written);
  val = 0x00;
  star_bus_i2c_write(&manager, "bmp388", &val, 1, 0x1C, &bytes_written);

  ESP_LOGI(s_tag, "BMP388 barometer started");

  float baseline_alt = 0;
  bool baseline_set = false;
  size_t bytes_read;

  for (int i = 0; i < 100; i++) {
    uint8_t data[6];
    star_bus_i2c_read(&manager, "bmp388", data, 6, 0x04, &bytes_read);

    uint32_t press_raw = data[0] | (data[1] << 8) | (data[2] << 16);
    uint32_t temp_raw = data[3] | (data[4] << 8) | (data[5] << 16);

    float pressure = press_raw / 64.0f;  /* Simplified */
    float temperature = temp_raw / 5120.0f - 40.0f;

    /* Calculate altitude */
    float altitude = 44330.0f * (1.0f - powf(pressure / SEA_LEVEL_PRESSURE, 0.1903f));

    if (!baseline_set) {
      baseline_alt = altitude;
      baseline_set = true;
    }

    float relative_alt = altitude - baseline_alt;

    ESP_LOGI(s_tag, "P: %.1f Pa, T: %.1f C, Alt: %.1f m (rel: %.2f m)",
             pressure, temperature, altitude, relative_alt);

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { pressure_altitude_example(); }
