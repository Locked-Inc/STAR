/**
 * @file 271_mag_hmc5883l.c
 * @brief HMC5883L magnetometer compass
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "MAG";

#define HMC5883L_ADDR (0x1E)

void mag_hmc5883l_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Mag_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "hmc5883l", I2C_NUM_0, HMC5883L_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Configure */
  uint8_t cfg[] = {0x70, 0x00, 0x00};  /* 8-avg, 15Hz, continuous */
  size_t bytes_written;
  star_bus_i2c_write(&manager, "hmc5883l", cfg, 3, 0x00, &bytes_written);

  ESP_LOGI(s_tag, "HMC5883L magnetometer started");

  for (int i = 0; i < 100; i++) {
    uint8_t data[6];
    size_t bytes_read;
    star_bus_i2c_read(&manager, "hmc5883l", data, 6, 0x03, &bytes_read);

    int16_t x = (data[0] << 8) | data[1];
    int16_t z = (data[2] << 8) | data[3];
    int16_t y = (data[4] << 8) | data[5];

    float heading = atan2f(y, x) * 180.0f / 3.14159f;
    if (heading < 0) heading += 360.0f;

    ESP_LOGI(s_tag, "X:%d Y:%d Z:%d  Heading:%.1f deg", x, y, z, heading);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { mag_hmc5883l_example(); }
