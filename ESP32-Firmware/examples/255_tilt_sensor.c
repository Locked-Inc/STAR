/**
 * @file 255_tilt_sensor.c
 * @brief Tilt angle measurement
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "TILT";

void tilt_sensor_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Tilt_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_handle_t imu;
  mpu6050_config_t cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &cfg);

  ESP_LOGI(s_tag, "Tilt sensor started");

  for (int i = 0; i < 500; i++) {
    mpu6050_accel_t accel;
    star_sensor_mpu6050_read_accel(&imu, &accel);

    float pitch = atan2f(-accel.x_g, sqrtf(accel.y_g * accel.y_g + accel.z_g * accel.z_g)) * 180.0f / M_PI;
    float roll = atan2f(accel.y_g, accel.z_g) * 180.0f / M_PI;

    ESP_LOGI(s_tag, "Pitch: %6.1f°  Roll: %6.1f°", pitch, roll);

    if (fabsf(pitch) > 45 || fabsf(roll) > 45) {
      ESP_LOGI(s_tag, "TILTED!");
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { tilt_sensor_example(); }
