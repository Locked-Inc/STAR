/**
 * @file 269_accel_adxl345.c
 * @brief ADXL345 triple-axis accelerometer
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "ACCEL";

#define ADXL345_ADDR (0x53)

void accel_adxl345_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Accel_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "adxl345", I2C_NUM_0, ADXL345_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Configure: measurement mode, full resolution, 100Hz */
  uint8_t cfg1[] = {0x08};  /* Power control */
  uint8_t cfg2[] = {0x0B};  /* Data format: full res, +/-16g */
  size_t bytes_written;
  star_bus_i2c_write(&manager, "adxl345", cfg1, 1, 0x2D, &bytes_written);
  star_bus_i2c_write(&manager, "adxl345", cfg2, 1, 0x31, &bytes_written);

  ESP_LOGI(s_tag, "ADXL345 accelerometer started");

  for (int i = 0; i < 100; i++) {
    uint8_t data[6];
    size_t bytes_read;
    star_bus_i2c_read(&manager, "adxl345", data, 6, 0x32, &bytes_read);

    int16_t x = (data[1] << 8) | data[0];
    int16_t y = (data[3] << 8) | data[2];
    int16_t z = (data[5] << 8) | data[4];

    float ax = x * 0.0039f;  /* 3.9mg/LSB */
    float ay = y * 0.0039f;
    float az = z * 0.0039f;

    ESP_LOGI(s_tag, "X:%.2fg Y:%.2fg Z:%.2fg", ax, ay, az);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { accel_adxl345_example(); }
