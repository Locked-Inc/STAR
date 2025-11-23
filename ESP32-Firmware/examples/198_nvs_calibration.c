/**
 * @file 198_nvs_calibration.c
 * @brief NVS storage for sensor calibration
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "NVS_CAL";

typedef struct {
  float accel_offset_x;
  float accel_offset_y;
  float accel_offset_z;
  float gyro_offset_x;
  float gyro_offset_y;
  float gyro_offset_z;
} imu_calibration_t;

static esp_err_t save_calibration(const imu_calibration_t* cal)
{
  nvs_handle_t handle;
  esp_err_t err = nvs_open("calibration", NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;

  nvs_set_blob(handle, "imu_cal", cal, sizeof(imu_calibration_t));
  nvs_commit(handle);
  nvs_close(handle);
  return ESP_OK;
}

static esp_err_t load_calibration(imu_calibration_t* cal)
{
  nvs_handle_t handle;
  esp_err_t err = nvs_open("calibration", NVS_READONLY, &handle);
  if (err != ESP_OK) return err;

  size_t size = sizeof(imu_calibration_t);
  err = nvs_get_blob(handle, "imu_cal", cal, &size);
  nvs_close(handle);
  return err;
}

static void calibrate_imu(mpu6050_handle_t* imu, imu_calibration_t* cal)
{
  ESP_LOGI(s_tag, "Calibrating IMU - keep sensor still...");

  float ax_sum = 0, ay_sum = 0, az_sum = 0;
  float gx_sum = 0, gy_sum = 0, gz_sum = 0;
  int samples = 100;

  for (int i = 0; i < samples; i++) {
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    star_sensor_mpu6050_read_accel(imu, &accel);
    star_sensor_mpu6050_read_gyro(imu, &gyro);

    ax_sum += accel.x_g;
    ay_sum += accel.y_g;
    az_sum += accel.z_g - 1.0f;  /* Remove gravity on Z */
    gx_sum += gyro.x_dps;
    gy_sum += gyro.y_dps;
    gz_sum += gyro.z_dps;

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  cal->accel_offset_x = ax_sum / samples;
  cal->accel_offset_y = ay_sum / samples;
  cal->accel_offset_z = az_sum / samples;
  cal->gyro_offset_x = gx_sum / samples;
  cal->gyro_offset_y = gy_sum / samples;
  cal->gyro_offset_z = gz_sum / samples;

  ESP_LOGI(s_tag, "Calibration complete");
  ESP_LOGI(s_tag, "Accel offsets: %.4f, %.4f, %.4f",
           cal->accel_offset_x, cal->accel_offset_y, cal->accel_offset_z);
  ESP_LOGI(s_tag, "Gyro offsets: %.2f, %.2f, %.2f",
           cal->gyro_offset_x, cal->gyro_offset_y, cal->gyro_offset_z);
}

void nvs_calibration_example(void)
{
  nvs_flash_init();

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "IMU_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
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

  imu_calibration_t cal = {0};

  /* Try to load existing calibration */
  if (load_calibration(&cal) != ESP_OK) {
    ESP_LOGI(s_tag, "No calibration found, performing calibration...");
    calibrate_imu(&imu, &cal);
    save_calibration(&cal);
    ESP_LOGI(s_tag, "Calibration saved to NVS");
  } else {
    ESP_LOGI(s_tag, "Loaded calibration from NVS");
  }

  /* Use calibrated readings */
  for (int i = 0; i < 100; i++) {
    mpu6050_accel_t accel;
    star_sensor_mpu6050_read_accel(&imu, &accel);

    float ax = accel.x_g - cal.accel_offset_x;
    float ay = accel.y_g - cal.accel_offset_y;
    float az = accel.z_g - cal.accel_offset_z;

    ESP_LOGI(s_tag, "Calibrated Accel: %.3f, %.3f, %.3f", ax, ay, az);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { nvs_calibration_example(); }
