/**
 * @file 146_mpu6050_basic.c
 * @brief Basic MPU6050 6-axis IMU example
 *
 * Demonstrates:
 * - Initializing MPU6050 via I2C bus manager
 * - Reading accelerometer and gyroscope data
 * - Basic motion detection
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"
#include "star_error_handler.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char* TAG = "MPU6050_BASIC";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define MPU6050_ADDR (0x68)

void mpu6050_basic_example(void)
{
  esp_err_t ret;

  /* Initialize error handler for retry logic */
  error_handler_t error_handler;
  error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);

  star_error_interface_t error_iface;
  error_handler_get_interface(&error_iface, &error_handler);

  /* Initialize bus manager with error interface */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "IMU_Manager", &error_iface, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init manager");
    return;
  }

  /* Create I2C config for MPU6050 */
  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, MPU6050_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, 400000);

  if (i2c == NULL) {
    ESP_LOGE(TAG, "Failed to create I2C config");
    star_bus_manager_deinit(&manager);
    return;
  }

  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Initialize MPU6050 sensor */
  mpu6050_handle_t imu;
  mpu6050_config_t cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };

  ret = star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init MPU6050: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(TAG, "MPU6050 initialized successfully");

  /* Main reading loop */
  for (int i = 0; i < 100; i++) {
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    float temp;

    ret = star_sensor_mpu6050_read_all(&imu, &accel, &gyro, &temp);

    if (ret == ESP_OK) {
      float accel_magnitude = sqrtf(
        accel.x_g * accel.x_g +
        accel.y_g * accel.y_g +
        accel.z_g * accel.z_g);

      ESP_LOGI(TAG, "Accel: X=%.2f Y=%.2f Z=%.2f (mag=%.2f g)",
               accel.x_g, accel.y_g, accel.z_g, accel_magnitude);
      ESP_LOGI(TAG, "Gyro:  X=%.2f Y=%.2f Z=%.2f deg/s",
               gyro.x_dps, gyro.y_dps, gyro.z_dps);
      ESP_LOGI(TAG, "Temp:  %.1f C", temp);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  /* Cleanup */
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
  error_handler_deinit(&error_handler);

  ESP_LOGI(TAG, "Example complete");
}

void app_main(void)
{
  mpu6050_basic_example();
}
