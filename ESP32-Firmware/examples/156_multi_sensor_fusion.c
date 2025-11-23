/**
 * @file 156_multi_sensor_fusion.c
 * @brief Multi-sensor data fusion example
 *
 * Demonstrates:
 * - Combining MPU6050 + QMC5883L for 9-DOF orientation
 * - Complementary filter for sensor fusion
 * - Real-time attitude estimation
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"
#include "star_sensor_qmc5883l.h"
#include "star_error_handler.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "SENSOR_FUSION";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

typedef struct {
  float roll;
  float pitch;
  float yaw;
} attitude_t;

static attitude_t s_attitude = {0};
static const float ALPHA = (0.98f);  /* Complementary filter coefficient */

void sensor_fusion_example(void)
{
  esp_err_t ret;

  /* Setup error handler */
  error_handler_t error_handler;
  error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);
  star_error_interface_t error_iface;
  error_handler_get_interface(&error_iface, &error_handler);

  /* Initialize bus manager */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Fusion_Manager", &error_iface, NULL);

  /* Create I2C configs for both sensors */
  star_bus_config_t* mpu_bus = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  star_bus_config_t* mag_bus = star_bus_config_create_i2c(
    "qmc5883l", I2C_NUM_0, 0x0D, I2C_SDA_PIN, I2C_SCL_PIN, 400000);

  star_bus_manager_add_bus(&manager, mpu_bus);
  star_bus_manager_add_bus(&manager, mag_bus);
  star_bus_config_init(mpu_bus, &manager);
  star_bus_config_init(mag_bus, &manager);

  /* Initialize MPU6050 */
  mpu6050_handle_t imu;
  mpu6050_config_t imu_cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  ret = star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &imu_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "MPU6050 init failed");
    return;
  }

  /* Initialize QMC5883L */
  qmc5883l_handle_t mag;
  qmc5883l_config_t mag_cfg = {
    .mode = QMC5883L_MODE_CONTINUOUS_VAL,
    .odr = QMC5883L_ODR_100HZ_VAL,
    .range = QMC5883L_RNG_8G_VAL,
    .osr = QMC5883L_OSR_512_VAL
  };
  ret = star_sensor_qmc5883l_init(&mag, &manager, "qmc5883l", &mag_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "QMC5883L init failed");
    return;
  }

  ESP_LOGI(s_tag, "9-DOF sensor fusion initialized");

  uint64_t last_time = esp_timer_get_time();

  /* Main fusion loop */
  for (int i = 0; i < 500; i++) {
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    qmc5883l_mag_data_t mag_data;

    ret = star_sensor_mpu6050_read_accel(&imu, &accel);
    if (ret != ESP_OK) continue;

    ret = star_sensor_mpu6050_read_gyro(&imu, &gyro);
    if (ret != ESP_OK) continue;

    ret = star_sensor_qmc5883l_read_mag(&mag, &mag_data);
    if (ret != ESP_OK) continue;

    /* Calculate time delta */
    uint64_t now = esp_timer_get_time();
    float dt = (now - last_time) / 1000000.0f;
    last_time = now;

    /* Calculate roll/pitch from accelerometer */
    float accel_roll = atan2f(accel.y_g, accel.z_g) * 180.0f / M_PI;
    float accel_pitch = atan2f(-accel.x_g,
      sqrtf(accel.y_g * accel.y_g + accel.z_g * accel.z_g)) * 180.0f / M_PI;

    /* Integrate gyroscope data */
    float gyro_roll = s_attitude.roll + gyro.x_dps * dt;
    float gyro_pitch = s_attitude.pitch + gyro.y_dps * dt;

    /* Complementary filter */
    s_attitude.roll = ALPHA * gyro_roll + (1.0f - ALPHA) * accel_roll;
    s_attitude.pitch = ALPHA * gyro_pitch + (1.0f - ALPHA) * accel_pitch;

    /* Calculate yaw from magnetometer (tilt-compensated) */
    float cos_roll = cosf(s_attitude.roll * M_PI / 180.0f);
    float sin_roll = sinf(s_attitude.roll * M_PI / 180.0f);
    float cos_pitch = cosf(s_attitude.pitch * M_PI / 180.0f);
    float sin_pitch = sinf(s_attitude.pitch * M_PI / 180.0f);

    float mag_x = mag_data.x_gauss * cos_pitch + mag_data.z_gauss * sin_pitch;
    float mag_y = mag_data.x_gauss * sin_roll * sin_pitch + mag_data.y_gauss * cos_roll -
                  mag_data.z_gauss * sin_roll * cos_pitch;

    s_attitude.yaw = atan2f(-mag_y, mag_x) * 180.0f / M_PI;
    if (s_attitude.yaw < 0) s_attitude.yaw += 360.0f;

    ESP_LOGI(s_tag, "Attitude: Roll=%.1f, Pitch=%.1f, Yaw=%.1f",
             s_attitude.roll, s_attitude.pitch, s_attitude.yaw);

    vTaskDelay(pdMS_TO_TICKS(20));  /* 50 Hz update rate */
  }

  star_sensor_mpu6050_deinit(&imu);
  star_sensor_qmc5883l_deinit(&mag);
  star_bus_manager_deinit(&manager);
  error_handler_deinit(&error_handler);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { sensor_fusion_example(); }
