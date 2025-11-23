/* lib/star_sensor_bno055_bmp280/src/star_sensor_bno055_bmp280.c */

#include "star_sensor_bno055_bmp280.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

#include "star_bus_i2c.h"

static const char* s_TAG = "imu10dof";

// BNO055 Register addresses
#define BNO055_REG_CHIP_ID (0x00)
#define BNO055_REG_OPR_MODE (0x3D)
#define BNO055_REG_PWR_MODE (0x3E)
#define BNO055_REG_SYS_TRIGGER (0x3F)
#define BNO055_REG_CALIB_STAT (0x35)
#define BNO055_REG_QUA_DATA_W_LSB (0x20)
#define BNO055_REG_EUL_HEADING_LSB (0x1A)
#define BNO055_REG_ACC_DATA_X_LSB (0x08)
#define BNO055_REG_GYR_DATA_X_LSB (0x14)
#define BNO055_REG_MAG_DATA_X_LSB (0x0E)
#define BNO055_REG_LIA_DATA_X_LSB (0x28)
#define BNO055_REG_GRV_DATA_X_LSB (0x2E)
#define BNO055_REG_TEMP (0x34)

// BMP280 Register addresses
#define BMP280_REG_CHIP_ID (0xD0)
#define BMP280_REG_CTRL_MEAS (0xF4)
#define BMP280_REG_CONFIG (0xF5)
#define BMP280_REG_PRESS_MSB (0xF7)
#define BMP280_REG_TEMP_MSB (0xFA)
#define BMP280_REG_CALIB00 (0x88)

#define BNO055_CHIP_ID_VALUE (0xA0)
#define BMP280_CHIP_ID_VALUE (0x58)

#define SEA_LEVEL_PRESSURE_PA (101325.0f)

esp_err_t star_sensor_imu_10dof_init(imu_10dof_handle_t* handle, const imu_10dof_config_t* config)
{
  if (handle == NULL || config == NULL || config->manager == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(imu_10dof_handle_t));
  handle->manager = config->manager;
  handle->bus_name = config->bus_name;
  handle->bno055_addr = config->bno055_addr;
  handle->bmp280_addr = config->bmp280_addr;
  handle->operation_mode = config->operation_mode;

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG, "Initializing 10-DOF IMU (BNO055=0x%02X, BMP280=0x%02X)", config->bno055_addr, config->bmp280_addr);

  // Verify BNO055 chip ID
  uint8_t chip_id;
  ret = star_bus_i2c_read(handle->manager, handle->bus_name, &chip_id, 1, BNO055_REG_CHIP_ID, NULL);
  if (ret != ESP_OK || chip_id != BNO055_CHIP_ID_VALUE) {
    ESP_LOGE(s_TAG, "BNO055 not found (ID=0x%02X)", chip_id);
    error_handler_deinit(&handle->error_handler);
    return ESP_ERR_NOT_FOUND;
  }

  // Reset BNO055
  uint8_t reset = 0x20;
  ret = star_bus_i2c_write(handle->manager, handle->bus_name, &reset, 1, BNO055_REG_SYS_TRIGGER, NULL);
  if (ret != ESP_OK) {
    error_handler_deinit(&handle->error_handler);
    return ret;
  }
  vTaskDelay(pdMS_TO_TICKS(650));  // Wait for reset

  // Set to config mode
  uint8_t mode = BNO055_OP_MODE_CONFIG;
  ret = star_bus_i2c_write(handle->manager, handle->bus_name, &mode, 1, BNO055_REG_OPR_MODE, NULL);
  if (ret != ESP_OK) {
    error_handler_deinit(&handle->error_handler);
    return ret;
  }
  vTaskDelay(pdMS_TO_TICKS(25));

  // Set power mode to normal
  uint8_t power = BNO055_POWER_MODE_NORMAL;
  ret = star_bus_i2c_write(handle->manager, handle->bus_name, &power, 1, BNO055_REG_PWR_MODE, NULL);
  if (ret != ESP_OK) {
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  // Set operation mode
  mode = config->operation_mode;
  ret = star_bus_i2c_write(handle->manager, handle->bus_name, &mode, 1, BNO055_REG_OPR_MODE, NULL);
  if (ret != ESP_OK) {
    error_handler_deinit(&handle->error_handler);
    return ret;
  }
  vTaskDelay(pdMS_TO_TICKS(20));

  // Initialize BMP280
  ret = star_bus_i2c_read(handle->manager, handle->bus_name, &chip_id, 1, BMP280_REG_CHIP_ID, NULL);
  if (ret != ESP_OK || chip_id != BMP280_CHIP_ID_VALUE) {
    ESP_LOGE(s_TAG, "BMP280 not found (ID=0x%02X)", chip_id);
    error_handler_deinit(&handle->error_handler);
    return ESP_ERR_NOT_FOUND;
  }

  // Read BMP280 calibration data
  uint8_t calib_data[24];
  ret = star_bus_i2c_read(handle->manager, handle->bus_name, calib_data, 24, BMP280_REG_CALIB00, NULL);
  if (ret != ESP_OK) {
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  handle->dig_T1 = (calib_data[1] << 8) | calib_data[0];
  handle->dig_T2 = (calib_data[3] << 8) | calib_data[2];
  handle->dig_T3 = (calib_data[5] << 8) | calib_data[4];
  handle->dig_P1 = (calib_data[7] << 8) | calib_data[6];
  handle->dig_P2 = (calib_data[9] << 8) | calib_data[8];
  handle->dig_P3 = (calib_data[11] << 8) | calib_data[10];
  handle->dig_P4 = (calib_data[13] << 8) | calib_data[12];
  handle->dig_P5 = (calib_data[15] << 8) | calib_data[14];
  handle->dig_P6 = (calib_data[17] << 8) | calib_data[16];
  handle->dig_P7 = (calib_data[19] << 8) | calib_data[18];
  handle->dig_P8 = (calib_data[21] << 8) | calib_data[20];
  handle->dig_P9 = (calib_data[23] << 8) | calib_data[22];

  // Configure BMP280: normal mode, 16x oversampling
  uint8_t ctrl = 0xB7;  // osrs_t=16, osrs_p=16, mode=normal
  ret = star_bus_i2c_write(handle->manager, handle->bus_name, &ctrl, 1, BMP280_REG_CTRL_MEAS, NULL);
  if (ret != ESP_OK) {
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "10-DOF IMU initialization successful");

  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_deinit(imu_10dof_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_read(const imu_10dof_handle_t* handle, imu_10dof_data_t* data)
{
  if (handle == NULL || data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(data, 0, sizeof(imu_10dof_data_t));
  esp_err_t ret;

  // Read quaternion (8 bytes)
  uint8_t quat_data[8];
  ret = star_bus_i2c_read(handle->manager, handle->bus_name, quat_data, 8, BNO055_REG_QUA_DATA_W_LSB, NULL);
  if (ret == ESP_OK) {
    data->quaternion.w = (int16_t)((quat_data[1] << 8) | quat_data[0]);
    data->quaternion.x = (int16_t)((quat_data[3] << 8) | quat_data[2]);
    data->quaternion.y = (int16_t)((quat_data[5] << 8) | quat_data[4]);
    data->quaternion.z = (int16_t)((quat_data[7] << 8) | quat_data[6]);
  }

  // Read Euler angles (6 bytes)
  uint8_t euler_data[6];
  ret = star_bus_i2c_read(handle->manager, handle->bus_name, euler_data, 6, BNO055_REG_EUL_HEADING_LSB, NULL);
  if (ret == ESP_OK) {
    data->euler.heading = ((int16_t)((euler_data[1] << 8) | euler_data[0])) / 16.0f;
    data->euler.roll = ((int16_t)((euler_data[3] << 8) | euler_data[2])) / 16.0f;
    data->euler.pitch = ((int16_t)((euler_data[5] << 8) | euler_data[4])) / 16.0f;
  }

  // Read accelerometer (6 bytes)
  uint8_t acc_data[6];
  ret = star_bus_i2c_read(handle->manager, handle->bus_name, acc_data, 6, BNO055_REG_ACC_DATA_X_LSB, NULL);
  if (ret == ESP_OK) {
    data->accelerometer.x = (int16_t)((acc_data[1] << 8) | acc_data[0]);
    data->accelerometer.y = (int16_t)((acc_data[3] << 8) | acc_data[2]);
    data->accelerometer.z = (int16_t)((acc_data[5] << 8) | acc_data[4]);
  }

  // Read BMP280 pressure and temperature
  uint8_t bmp_data[6];
  ret = star_bus_i2c_read(handle->manager, handle->bus_name, bmp_data, 6, BMP280_REG_PRESS_MSB, NULL);
  if (ret == ESP_OK) {
    int32_t adc_P = (bmp_data[0] << 12) | (bmp_data[1] << 4) | (bmp_data[2] >> 4);
    int32_t adc_T = (bmp_data[3] << 12) | (bmp_data[4] << 4) | (bmp_data[5] >> 4);

    // Temperature compensation
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)handle->dig_T1 << 1))) * ((int32_t)handle->dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)handle->dig_T1)) * ((adc_T >> 4) - ((int32_t)handle->dig_T1))) >> 12) * ((int32_t)handle->dig_T3)) >> 14;
    int32_t t_fine = var1 + var2;
    data->bmp_temp_c = (t_fine * 5 + 128) >> 8;
    data->bmp_temp_c /= 100.0f;

    // Pressure compensation
    int64_t var1_64 = ((int64_t)t_fine) - 128000;
    int64_t var2_64 = var1_64 * var1_64 * (int64_t)handle->dig_P6;
    var2_64 = var2_64 + ((var1_64 * (int64_t)handle->dig_P5) << 17);
    var2_64 = var2_64 + (((int64_t)handle->dig_P4) << 35);
    var1_64 = ((var1_64 * var1_64 * (int64_t)handle->dig_P3) >> 8) + ((var1_64 * (int64_t)handle->dig_P2) << 12);
    var1_64 = (((((int64_t)1) << 47) + var1_64)) * ((int64_t)handle->dig_P1) >> 33;

    if (var1_64 != 0) {
      int64_t p = 1048576 - adc_P;
      p = (((p << 31) - var2_64) * 3125) / var1_64;
      var1_64 = (((int64_t)handle->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
      var2_64 = (((int64_t)handle->dig_P8) * p) >> 19;
      p = ((p + var1_64 + var2_64) >> 8) + (((int64_t)handle->dig_P7) << 4);
      data->pressure_pa = (float)p / 256.0f;

      // Calculate altitude
      data->altitude_m = 44330.0f * (1.0f - powf(data->pressure_pa / SEA_LEVEL_PRESSURE_PA, 0.1903f));
    }
  }

  // Read calibration status
  star_sensor_imu_10dof_get_calibration(handle, &data->calibration);

  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_get_calibration(const imu_10dof_handle_t* handle,
                                                 calibration_status_t*     status)
{
  if (handle == NULL || status == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t calib_stat;
  esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, &calib_stat, 1, BNO055_REG_CALIB_STAT, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  status->sys = (calib_stat >> 6) & 0x03;
  status->gyro = (calib_stat >> 4) & 0x03;
  status->accel = (calib_stat >> 2) & 0x03;
  status->mag = calib_stat & 0x03;

  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_is_calibrated(const imu_10dof_handle_t* handle, bool* calibrated)
{
  if (handle == NULL || calibrated == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  calibration_status_t status;
  esp_err_t ret = star_sensor_imu_10dof_get_calibration(handle, &status);
  if (ret != ESP_OK) {
    return ret;
  }

  *calibrated = (status.sys == 3 && status.gyro == 3 && status.accel == 3 && status.mag == 3);

  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_set_mode(imu_10dof_handle_t* handle, bno055_op_mode_t mode)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t mode_val = mode;
  esp_err_t ret = star_bus_i2c_write(handle->manager, handle->bus_name, &mode_val, 1, BNO055_REG_OPR_MODE, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  handle->operation_mode = mode;
  vTaskDelay(pdMS_TO_TICKS(20));

  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_reset_bno055(imu_10dof_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t reset = 0x20;
  esp_err_t ret = star_bus_i2c_write(handle->manager, handle->bus_name, &reset, 1, BNO055_REG_SYS_TRIGGER, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(650));
  ESP_LOGI(s_TAG, "BNO055 reset");

  return ESP_OK;
}
