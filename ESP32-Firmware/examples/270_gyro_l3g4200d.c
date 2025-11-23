/**
 * @file 270_gyro_l3g4200d.c
 * @brief L3G4200D triple-axis gyroscope
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "GYRO";

#define L3G4200D_ADDR (0x69)

void gyro_l3g4200d_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Gyro_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "l3g4200d", I2C_NUM_0, L3G4200D_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Configure: normal mode, 250dps */
  uint8_t cfg1[] = {0x0F};  /* CTRL_REG1: all axes, normal mode */
  uint8_t cfg2[] = {0x00};  /* CTRL_REG4: 250dps */
  size_t bytes_written;
  star_bus_i2c_write(&manager, "l3g4200d", cfg1, 1, 0x20, &bytes_written);
  star_bus_i2c_write(&manager, "l3g4200d", cfg2, 1, 0x23, &bytes_written);

  ESP_LOGI(s_tag, "L3G4200D gyroscope started");

  for (int i = 0; i < 100; i++) {
    uint8_t data[6];
    size_t bytes_read;
    star_bus_i2c_read(&manager, "l3g4200d", data, 6, 0x28 | 0x80, &bytes_read);  /* Auto-increment */

    int16_t x = (data[1] << 8) | data[0];
    int16_t y = (data[3] << 8) | data[2];
    int16_t z = (data[5] << 8) | data[4];

    float gx = x * 0.00875f;  /* 8.75mdps/LSB at 250dps */
    float gy = y * 0.00875f;
    float gz = z * 0.00875f;

    ESP_LOGI(s_tag, "X:%.2f Y:%.2f Z:%.2f dps", gx, gy, gz);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { gyro_l3g4200d_example(); }
