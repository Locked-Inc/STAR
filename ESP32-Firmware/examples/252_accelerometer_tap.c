/**
 * @file 252_accelerometer_tap.c
 * @brief Tap detection using accelerometer
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "TAP";

void accelerometer_tap_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Tap_Manager", NULL, NULL);

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

  ESP_LOGI(s_tag, "Tap detection started");

  float prev_mag = 1.0f;
  int tap_count = 0;
  uint32_t last_tap = 0;

  for (int i = 0; i < 1000; i++) {
    mpu6050_accel_t accel;
    star_sensor_mpu6050_read_accel(&imu, &accel);

    float mag = sqrtf(accel.x_g * accel.x_g +
                     accel.y_g * accel.y_g +
                     accel.z_g * accel.z_g);

    float delta = fabsf(mag - prev_mag);
    uint32_t now = xTaskGetTickCount();

    if (delta > 1.5f && (now - last_tap) > pdMS_TO_TICKS(300)) {
      tap_count++;
      last_tap = now;
      ESP_LOGI(s_tag, "TAP detected! Count: %d, Strength: %.2f", tap_count, delta);
    }

    prev_mag = mag;
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  ESP_LOGI(s_tag, "Total taps: %d", tap_count);
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { accelerometer_tap_example(); }
