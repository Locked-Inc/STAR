/**
 * @file 207_kalman_filter.c
 * @brief Kalman filter for sensor fusion
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "KALMAN";

typedef struct {
  float q_angle;   /* Process noise variance for angle */
  float q_bias;    /* Process noise variance for gyro bias */
  float r_measure; /* Measurement noise variance */
  float angle;     /* Estimated angle */
  float bias;      /* Estimated gyro bias */
  float P[2][2];   /* Error covariance matrix */
} kalman_t;

static void priv_kalman_init(kalman_t *k)
{
  k->q_angle = 0.001f;
  k->q_bias = 0.003f;
  k->r_measure = 0.03f;
  k->angle = 0;
  k->bias = 0;
  k->P[0][0] = 0; k->P[0][1] = 0;
  k->P[1][0] = 0; k->P[1][1] = 0;
}

static float priv_kalman_update(kalman_t *k, float new_angle, float new_rate, float dt)
{
  /* Predict */
  k->angle += dt * (new_rate - k->bias);
  k->P[0][0] += dt * (dt * k->P[1][1] - k->P[0][1] - k->P[1][0] + k->q_angle);
  k->P[0][1] -= dt * k->P[1][1];
  k->P[1][0] -= dt * k->P[1][1];
  k->P[1][1] += k->q_bias * dt;

  /* Update */
  float S = k->P[0][0] + k->r_measure;
  float K[2] = { k->P[0][0] / S, k->P[1][0] / S };
  float y = new_angle - k->angle;

  k->angle += K[0] * y;
  k->bias += K[1] * y;

  float P00_temp = k->P[0][0];
  float P01_temp = k->P[0][1];
  k->P[0][0] -= K[0] * P00_temp;
  k->P[0][1] -= K[0] * P01_temp;
  k->P[1][0] -= K[1] * P00_temp;
  k->P[1][1] -= K[1] * P01_temp;

  return k->angle;
}

void kalman_filter_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Kalman_Manager", NULL, NULL);

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

  kalman_t k_roll, k_pitch;
  priv_kalman_init(&k_roll);
  priv_kalman_init(&k_pitch);

  float dt = 0.01f;

  for (int i = 0; i < 1000; i++) {
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    star_sensor_mpu6050_read_accel(&imu, &accel);
    star_sensor_mpu6050_read_gyro(&imu, &gyro);

    float accel_roll = atan2f(accel.y_g, accel.z_g) * 180.0f / M_PI;
    float accel_pitch = atan2f(-accel.x_g, sqrtf(accel.y_g * accel.y_g + accel.z_g * accel.z_g)) * 180.0f / M_PI;

    float roll = priv_kalman_update(&k_roll, accel_roll, gyro.x_dps, dt);
    float pitch = priv_kalman_update(&k_pitch, accel_pitch, gyro.y_dps, dt);

    if (i % 10 == 0) {
      ESP_LOGI(s_tag, "Kalman: Roll=%.1f Pitch=%.1f | Raw: Roll=%.1f Pitch=%.1f",
               roll, pitch, accel_roll, accel_pitch);
    }

    vTaskDelay(pdMS_TO_TICKS((int)(dt * 1000)));
  }

  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { kalman_filter_example(); }
