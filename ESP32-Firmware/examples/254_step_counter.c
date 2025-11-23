/**
 * @file 254_step_counter.c
 * @brief Pedometer using accelerometer
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "PEDOMETER";

#define STEP_THRESHOLD (1.2f) 
#define MIN_STEP_INTERVAL_MS (300)

void step_counter_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Step_Manager", NULL, NULL);

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

  ESP_LOGI(s_tag, "Step counter started - walk around!");

  int step_count = 0;
  uint32_t last_step_time = 0;
  float prev_mag = 1.0f;
  bool above_threshold = false;
  float distance_m = 0;  /* Assume 0.7m stride */

  for (int i = 0; i < 3000; i++) {
    mpu6050_accel_t accel;
    star_sensor_mpu6050_read_accel(&imu, &accel);

    float mag = sqrtf(accel.x_g * accel.x_g +
                     accel.y_g * accel.y_g +
                     accel.z_g * accel.z_g);

    uint32_t now = xTaskGetTickCount();

    if (mag > STEP_THRESHOLD && !above_threshold) {
      above_threshold = true;
      if ((now - last_step_time) > pdMS_TO_TICKS(MIN_STEP_INTERVAL_MS)) {
        step_count++;
        distance_m = step_count * 0.7f;
        last_step_time = now;
        ESP_LOGI(s_tag, "Step! Count: %d, Distance: %.1f m", step_count, distance_m);
      }
    } else if (mag < STEP_THRESHOLD - 0.1f) {
      above_threshold = false;
    }

    prev_mag = mag;
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  ESP_LOGI(s_tag, "Final: %d steps, %.1f meters", step_count, distance_m);
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { step_counter_example(); }
