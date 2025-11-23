/**
 * @file 166_bno055_bmp280_full.c
 * @brief Full 10-DOF IMU with barometric altitude
 *
 * Demonstrates:
 * - BNO055 sensor fusion modes
 * - BMP280 pressure/altitude
 * - Quaternion orientation
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_bno055_bmp280.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "BNO055_BMP280";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)

void bno055_bmp280_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "10DOF_Manager", NULL, NULL);

  star_bus_config_t* bno_bus = star_bus_config_create_i2c(
    "bno055", I2C_NUM_0, 0x28, I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  star_bus_config_t* bmp_bus = star_bus_config_create_i2c(
    "bmp280", I2C_NUM_0, 0x76, I2C_SDA_PIN, I2C_SCL_PIN, 400000);

  star_bus_manager_add_bus(&manager, bno_bus);
  star_bus_manager_add_bus(&manager, bmp_bus);
  star_bus_config_init(bno_bus, &manager);
  star_bus_config_init(bmp_bus, &manager);

  imu_10dof_handle_t imu;
  imu_10dof_config_t cfg = {
    .manager = &manager,
    .bus_name = "bno055",
    .bno055_addr = BNO055_I2C_ADDR,
    .bmp280_addr = BMP280_I2C_ADDR,
    .operation_mode = BNO055_OP_MODE_NDOF
  };

  if (star_sensor_imu_10dof_init(&imu, &cfg) != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init 10-DOF IMU");
    return;
  }

  ESP_LOGI(s_tag, "10-DOF IMU initialized in NDOF fusion mode");

  /* Wait for calibration */
  ESP_LOGI(s_tag, "Calibrating - rotate sensor in all directions...");
  calibration_status_t cal;
  do {
    star_sensor_imu_10dof_get_calibration(&imu, &cal);
    ESP_LOGI(s_tag, "Cal: Sys=%d Gyro=%d Accel=%d Mag=%d",
             cal.sys, cal.gyro, cal.accel, cal.mag);
    vTaskDelay(pdMS_TO_TICKS(500));
  } while (cal.sys < 3);

  ESP_LOGI(s_tag, "Calibration complete!");

  /* Main reading loop */
  for (int i = 0; i < 200; i++) {
    imu_10dof_data_t data;
    star_sensor_imu_10dof_read(&imu, &data);

    ESP_LOGI(s_tag, "Euler: H=%.1f R=%.1f P=%.1f",
             data.euler.heading, data.euler.roll, data.euler.pitch);
    ESP_LOGI(s_tag, "Quat: w=%d x=%d y=%d z=%d",
             data.quaternion.w, data.quaternion.x, data.quaternion.y, data.quaternion.z);
    ESP_LOGI(s_tag, "Pressure: %.1f hPa, Altitude: %.1f m, Temp: %.1f C",
             data.pressure_pa / 100.0f, data.altitude_m, data.bmp_temp_c);

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  star_sensor_imu_10dof_deinit(&imu);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { bno055_bmp280_example(); }
