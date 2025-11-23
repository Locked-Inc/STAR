/**
 * @file 173_mpu6050_motion_detect.c
 * @brief MPU6050 motion detection and interrupts
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "MPU6050_MOTION";

void mpu6050_motion_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "IMU_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_handle_t imu;
  mpu6050_config_t cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_4G,
    .gyro_range = MPU6050_GYRO_RANGE_500,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };

  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &cfg);

  ESP_LOGI(s_tag, "Software motion detection enabled (delta threshold)");

  float prev_magnitude = 0;
  for (int i = 0; i < 200; i++) {
    mpu6050_accel_t accel;
    star_sensor_mpu6050_read_accel(&imu, &accel);

    float magnitude = sqrtf(accel.x_g * accel.x_g +
                           accel.y_g * accel.y_g +
                           accel.z_g * accel.z_g);

    float delta = fabsf(magnitude - prev_magnitude);
    if (delta > 0.5f) {
      ESP_LOGW(s_tag, "Motion detected! Delta: %.2f g", delta);
    }

    prev_magnitude = magnitude;
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { mpu6050_motion_example(); }
