/* lib/star_sensor_bno055_bmp280/src/star_sensor_bno055_bmp280.c */

#include "star_sensor_bno055_bmp280.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bno055_bmp280_constants.h"
#include "star_bus_i2c.h"
#include "star_error_handler.h"

static const char* s_TAG = "imu10dof";

/* Use constant instead of macro for type safety */
static const uint32_t s_imu_10dof_mutex_timeout_ms = 1000;

/* Register addresses, chip ID values, and other constants are now provided
 * by type-safe constants in bno055_bmp280_constants.h (included via the header).
 * See k_bno055_reg_*, k_bmp280_reg_*, k_bno055_chip_id_value, k_bmp280_chip_id_value, etc. */

/* Sea level pressure constant */
static const float s_sea_level_pressure_pa = 101325.0f;

esp_err_t star_sensor_imu_10dof_init(imu_10dof_handle_t*       handle,
                                     star_bus_manager_t*       manager,
                                     const char*               bus_name,
                                     star_error_interface_t*   error_iface,
                                     const imu_10dof_config_t* config)
{
  if (handle == NULL || manager == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(imu_10dof_handle_t));
  handle->manager        = manager;
  handle->bus_name       = bus_name;
  handle->bno055_addr    = config->bno055_addr;
  handle->bmp280_addr    = config->bmp280_addr;
  handle->operation_mode = config->operation_mode;

  // Create mutex first
  handle->mutex = xSemaphoreCreateMutex();
  if (handle->mutex == NULL) {
    ESP_LOGE(s_TAG, "Failed to create mutex");
    return ESP_ERR_NO_MEM;
  }

  // Handle error interface injection
  if (error_iface == NULL) {
    // Create default error handler internally
    error_handler_t* default_handler = error_handler_create_default();
    if (default_handler == NULL) {
      ESP_LOGE(s_TAG, "Failed to create default error handler");
      vSemaphoreDelete(handle->mutex);
      handle->mutex = NULL;
      return ESP_ERR_NO_MEM;
    }

    // Allocate and store the interface
    handle->error_iface = (star_error_interface_t*)malloc(sizeof(star_error_interface_t));
    if (handle->error_iface == NULL) {
      ESP_LOGE(s_TAG, "Failed to allocate error interface");
      error_handler_destroy_default(default_handler);
      vSemaphoreDelete(handle->mutex);
      handle->mutex = NULL;
      return ESP_ERR_NO_MEM;
    }

    error_handler_get_interface(handle->error_iface, default_handler);
    handle->owns_error_handler = true;
    ESP_LOGI(s_TAG, "Created default error handler internally");
  } else {
    handle->error_iface        = error_iface;
    handle->owns_error_handler = false;
    ESP_LOGI(s_TAG, "Using injected error interface");
  }

  ESP_LOGI(s_TAG,
           "Initializing 10-DOF IMU (BNO055=0x%02X, BMP280=0x%02X)",
           config->bno055_addr,
           config->bmp280_addr);

  // Verify BNO055 chip ID
  uint8_t   chip_id;
  esp_err_t ret = star_bus_i2c_read(handle->manager,
                                    handle->bus_name,
                                    &chip_id,
                                    1,
                                    (uint8_t)k_bno055_reg_chip_id,
                                    NULL);
  if (ret != ESP_OK || chip_id != k_bno055_chip_id_value) {
    ESP_LOGE(s_TAG, "BNO055 not found (ID=0x%02X)", chip_id);
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ESP_ERR_NOT_FOUND;
  }

  // Reset BNO055
  uint8_t reset = 0x20;
  ret           = star_bus_i2c_write(handle->manager,
                           handle->bus_name,
                           &reset,
                           1,
                           (uint8_t)k_bno055_reg_sys_trigger,
                           NULL);
  if (ret != ESP_OK) {
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }
  vTaskDelay(pdMS_TO_TICKS(650)); // Wait for reset

  // Set to config mode
  uint8_t mode = k_bno055_op_mode_config;
  ret          = star_bus_i2c_write(handle->manager,
                           handle->bus_name,
                           &mode,
                           1,
                           (uint8_t)k_bno055_reg_opr_mode,
                           NULL);
  if (ret != ESP_OK) {
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }
  vTaskDelay(pdMS_TO_TICKS(25));

  // Set power mode to normal
  uint8_t power = k_bno055_power_mode_normal;
  ret           = star_bus_i2c_write(handle->manager,
                           handle->bus_name,
                           &power,
                           1,
                           (uint8_t)k_bno055_reg_pwr_mode,
                           NULL);
  if (ret != ESP_OK) {
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  // Set operation mode
  mode = config->operation_mode;
  ret  = star_bus_i2c_write(handle->manager,
                           handle->bus_name,
                           &mode,
                           1,
                           (uint8_t)k_bno055_reg_opr_mode,
                           NULL);
  if (ret != ESP_OK) {
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }
  vTaskDelay(pdMS_TO_TICKS(20));

  // Initialize BMP280
  ret =
    star_bus_i2c_read(handle->manager, handle->bus_name, &chip_id, 1, k_bmp280_reg_chip_id, NULL);
  if (ret != ESP_OK || chip_id != k_bmp280_chip_id_value) {
    ESP_LOGE(s_TAG, "BMP280 not found (ID=0x%02X)", chip_id);
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ESP_ERR_NOT_FOUND;
  }

  // Read BMP280 calibration data
  uint8_t calib_data[24];
  ret = star_bus_i2c_read(handle->manager,
                          handle->bus_name,
                          calib_data,
                          24,
                          k_bmp280_reg_calib00,
                          NULL);
  if (ret != ESP_OK) {
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
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

  // Validate BMP280 calibration data
  if (handle->dig_T1 == 0 || handle->dig_P1 == 0) {
    ESP_LOGE(s_TAG,
             "Invalid BMP280 calibration: dig_T1=%u, dig_P1=%u",
             handle->dig_T1,
             handle->dig_P1);
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ESP_ERR_INVALID_RESPONSE;
  }

  // Check for all 0xFF (read failure indicator)
  bool all_ff = true;
  for (int i = 0; i < 24; i++) {
    if (calib_data[i] != 0xFF) {
      all_ff = false;
      break;
    }
  }
  if (all_ff) {
    ESP_LOGE(s_TAG, "BMP280 calibration read failure: all bytes 0xFF");
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ESP_ERR_INVALID_RESPONSE;
  }

  // Configure BMP280: normal mode, 16x oversampling
  uint8_t ctrl = 0xB7; // osrs_t=16, osrs_p=16, mode=normal
  ret =
    star_bus_i2c_write(handle->manager, handle->bus_name, &ctrl, 1, k_bmp280_reg_ctrl_meas, NULL);
  if (ret != ESP_OK) {
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "10-DOF IMU initialization successful (thread-safe with mutex)");

  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_deinit(imu_10dof_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Acquire mutex before cleanup
  SemaphoreHandle_t mutex = handle->mutex;
  if (mutex != NULL &&
      xSemaphoreTake(mutex, pdMS_TO_TICKS(s_imu_10dof_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGW(s_TAG, "Failed to acquire mutex for deinit, continuing anyway");
  }

  // Clean up error handler if we own it
  star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
  handle->error_iface = NULL;
  if (handle->owns_error_handler) {
    ESP_LOGI(s_TAG, "Destroyed internally-owned error handler");
  }

  // Clear handle fields
  handle->initialized = false;
  handle->mutex       = NULL;

  // Release and delete mutex
  if (mutex != NULL) {
    xSemaphoreGive(mutex);
    vSemaphoreDelete(mutex);
  }

  ESP_LOGI(s_TAG, "10-DOF IMU deinitialized");
  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_read(const imu_10dof_handle_t* handle, imu_10dof_data_t* data)
{
  if (handle == NULL || data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Acquire mutex for thread-safe read
  SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL ||
      xSemaphoreTake(mutex, pdMS_TO_TICKS(s_imu_10dof_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for read");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  memset(data, 0, sizeof(imu_10dof_data_t));
  esp_err_t ret;

  // Read quaternion (8 bytes)
  uint8_t quat_data[8];
  ret = star_bus_i2c_read(handle->manager,
                          handle->bus_name,
                          quat_data,
                          8,
                          (uint8_t)k_bno055_reg_qua_data_w_lsb,
                          NULL);
  if (ret == ESP_OK) {
    data->quaternion.w = (int16_t)((quat_data[1] << 8) | quat_data[0]);
    data->quaternion.x = (int16_t)((quat_data[3] << 8) | quat_data[2]);
    data->quaternion.y = (int16_t)((quat_data[5] << 8) | quat_data[4]);
    data->quaternion.z = (int16_t)((quat_data[7] << 8) | quat_data[6]);
  }

  // Read Euler angles (6 bytes)
  uint8_t euler_data[6];
  ret = star_bus_i2c_read(handle->manager,
                          handle->bus_name,
                          euler_data,
                          6,
                          (uint8_t)k_bno055_reg_eul_heading_lsb,
                          NULL);
  if (ret == ESP_OK) {
    data->euler.heading = ((int16_t)((euler_data[1] << 8) | euler_data[0])) / 16.0f;
    data->euler.roll    = ((int16_t)((euler_data[3] << 8) | euler_data[2])) / 16.0f;
    data->euler.pitch   = ((int16_t)((euler_data[5] << 8) | euler_data[4])) / 16.0f;
  }

  // Read accelerometer (6 bytes)
  uint8_t acc_data[6];
  ret = star_bus_i2c_read(handle->manager,
                          handle->bus_name,
                          acc_data,
                          6,
                          (uint8_t)k_bno055_reg_acc_data_x_lsb,
                          NULL);
  if (ret == ESP_OK) {
    data->accelerometer.x = (int16_t)((acc_data[1] << 8) | acc_data[0]);
    data->accelerometer.y = (int16_t)((acc_data[3] << 8) | acc_data[2]);
    data->accelerometer.z = (int16_t)((acc_data[5] << 8) | acc_data[4]);
  }

  // Read BMP280 pressure and temperature
  uint8_t bmp_data[6];
  ret =
    star_bus_i2c_read(handle->manager, handle->bus_name, bmp_data, 6, k_bmp280_reg_press_msb, NULL);
  if (ret == ESP_OK) {
    int32_t adc_P = (bmp_data[0] << 12) | (bmp_data[1] << 4) | (bmp_data[2] >> 4);
    int32_t adc_T = (bmp_data[3] << 12) | (bmp_data[4] << 4) | (bmp_data[5] >> 4);

    // Temperature compensation
    int32_t var1 =
      ((((adc_T >> 3) - ((int32_t)handle->dig_T1 << 1))) * ((int32_t)handle->dig_T2)) >> 11;
    int32_t var2 =
      (((((adc_T >> 4) - ((int32_t)handle->dig_T1)) * ((adc_T >> 4) - ((int32_t)handle->dig_T1))) >>
        12) *
       ((int32_t)handle->dig_T3)) >>
      14;
    int32_t t_fine   = var1 + var2;
    data->bmp_temp_c = (t_fine * 5 + 128) >> 8;
    data->bmp_temp_c /= 100.0f;

    // Pressure compensation
    int64_t var1_64 = ((int64_t)t_fine) - 128000;
    int64_t var2_64 = var1_64 * var1_64 * (int64_t)handle->dig_P6;
    var2_64         = var2_64 + ((var1_64 * (int64_t)handle->dig_P5) << 17);
    var2_64         = var2_64 + (((int64_t)handle->dig_P4) << 35);
    var1_64         = ((var1_64 * var1_64 * (int64_t)handle->dig_P3) >> 8) +
              ((var1_64 * (int64_t)handle->dig_P2) << 12);
    var1_64 = (((((int64_t)1) << 47) + var1_64)) * ((int64_t)handle->dig_P1) >> 33;

    if (var1_64 != 0) {
      int64_t p         = 1048576 - adc_P;
      p                 = (((p << 31) - var2_64) * 3125) / var1_64;
      var1_64           = (((int64_t)handle->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
      var2_64           = (((int64_t)handle->dig_P8) * p) >> 19;
      p                 = ((p + var1_64 + var2_64) >> 8) + (((int64_t)handle->dig_P7) << 4);
      data->pressure_pa = (float)p / 256.0f;

      // Calculate altitude
      data->altitude_m =
        44330.0f * (1.0f - powf(data->pressure_pa / s_sea_level_pressure_pa, 0.1903f));
    }
  }

  // Read calibration status
  star_sensor_imu_10dof_get_calibration(handle, &data->calibration);

  xSemaphoreGive(mutex);
  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_get_calibration(const imu_10dof_handle_t* handle,
                                                calibration_status_t*     status)
{
  if (handle == NULL || status == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t   calib_stat;
  esp_err_t ret = star_bus_i2c_read(handle->manager,
                                    handle->bus_name,
                                    &calib_stat,
                                    1,
                                    (uint8_t)k_bno055_reg_calib_stat,
                                    NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  status->sys   = (calib_stat >> 6) & 0x03;
  status->gyro  = (calib_stat >> 4) & 0x03;
  status->accel = (calib_stat >> 2) & 0x03;
  status->mag   = calib_stat & 0x03;

  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_is_calibrated(const imu_10dof_handle_t* handle, bool* calibrated)
{
  if (handle == NULL || calibrated == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  calibration_status_t status;
  esp_err_t            ret = star_sensor_imu_10dof_get_calibration(handle, &status);
  if (ret != ESP_OK) {
    return ret;
  }

  *calibrated = (status.sys == 3 && status.gyro == 3 && status.accel == 3 && status.mag == 3);

  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_set_mode(imu_10dof_handle_t* handle, bno055_op_mode_t mode)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Acquire mutex
  SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL ||
      xSemaphoreTake(mutex, pdMS_TO_TICKS(s_imu_10dof_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for set_mode");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t   mode_val = mode;
  esp_err_t ret      = star_bus_i2c_write(handle->manager,
                                     handle->bus_name,
                                     &mode_val,
                                     1,
                                     (uint8_t)k_bno055_reg_opr_mode,
                                     NULL);
  if (ret != ESP_OK) {
    xSemaphoreGive(mutex);
    return ret;
  }

  handle->operation_mode = mode;
  vTaskDelay(pdMS_TO_TICKS(20));

  xSemaphoreGive(mutex);
  return ESP_OK;
}

esp_err_t star_sensor_imu_10dof_reset_bno055(imu_10dof_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t   reset = 0x20;
  esp_err_t ret   = star_bus_i2c_write(handle->manager,
                                     handle->bus_name,
                                     &reset,
                                     1,
                                     (uint8_t)k_bno055_reg_sys_trigger,
                                     NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(650));
  ESP_LOGI(s_TAG, "BNO055 reset");

  return ESP_OK;
}
