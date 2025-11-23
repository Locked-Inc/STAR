/**
 * @file 253_fall_detection.c
 * @brief Fall detection with IMU
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "FALL";

#define FREEFALL_THRESHOLD (0.3f) 
#define IMPACT_THRESHOLD   2.5f

void fall_detection_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Fall_Manager", NULL, NULL);

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

  ESP_LOGI(s_tag, "Fall detection started");

  bool freefall_detected = false;
  int fall_events = 0;

  for (int i = 0; i < 2000; i++) {
    mpu6050_accel_t accel;
    star_sensor_mpu6050_read_accel(&imu, &accel);

    float mag = sqrtf(accel.x_g * accel.x_g +
                     accel.y_g * accel.y_g +
                     accel.z_g * accel.z_g);

    if (mag < FREEFALL_THRESHOLD) {
      freefall_detected = true;
      ESP_LOGI(s_tag, "FREEFALL detected! mag=%.2f", mag);
    }

    if (freefall_detected && mag > IMPACT_THRESHOLD) {
      fall_events++;
      ESP_LOGI(s_tag, "*** FALL DETECTED! *** Impact=%.2f, Event #%d", mag, fall_events);
      freefall_detected = false;
    }

    if (freefall_detected && mag > 0.8f) {
      freefall_detected = false;  /* False alarm */
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  ESP_LOGI(s_tag, "Total falls detected: %d", fall_events);
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { fall_detection_example(); }
