/* lib/star_sensor_mpu6050/src/star_sensor_mpu6050.c */

#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

#include "star_bus_i2c.h"
#include "star_error_handler.h"

static const char* const s_TAG = "mpu6050";

/* Use constant instead of macro for type safety */
static const uint32_t s_mpu6050_mutex_timeout_ms = 1000;

static esp_err_t internal_mpu6050_write_reg(star_bus_manager_t* const manager,
                                            const char* const         bus_name,
                                            const uint8_t             reg,
                                            const uint8_t             value)
{
  return star_bus_i2c_write(manager, bus_name, &value, 1, reg, NULL);
}

static esp_err_t internal_mpu6050_read_reg(star_bus_manager_t* const manager,
                                           const char* const         bus_name,
                                           const uint8_t             reg,
                                           uint8_t* const            value)
{
  return star_bus_i2c_read(manager, bus_name, value, 1, reg, NULL);
}

static void internal_get_accel_sensitivity(const mpu6050_accel_range_t range,
                                           float* const                sensitivity)
{
  switch (range) {
    case k_mpu6050_accel_range_2g:
      *sensitivity = MPU6050_ACCEL_SENS_2G;
      break;
    case k_mpu6050_accel_range_4g:
      *sensitivity = MPU6050_ACCEL_SENS_4G;
      break;
    case k_mpu6050_accel_range_8g:
      *sensitivity = MPU6050_ACCEL_SENS_8G;
      break;
    case k_mpu6050_accel_range_16g:
      *sensitivity = MPU6050_ACCEL_SENS_16G;
      break;
    default:
      *sensitivity = MPU6050_ACCEL_SENS_2G;
  }
}

static void internal_get_gyro_sensitivity(const mpu6050_gyro_range_t range,
                                          float* const               sensitivity)
{
  switch (range) {
    case k_mpu6050_gyro_range_250:
      *sensitivity = MPU6050_GYRO_SENS_250;
      break;
    case k_mpu6050_gyro_range_500:
      *sensitivity = MPU6050_GYRO_SENS_500;
      break;
    case k_mpu6050_gyro_range_1000:
      *sensitivity = MPU6050_GYRO_SENS_1000;
      break;
    case k_mpu6050_gyro_range_2000:
      *sensitivity = MPU6050_GYRO_SENS_2000;
      break;
    default:
      *sensitivity = MPU6050_GYRO_SENS_250;
  }
}

esp_err_t star_sensor_mpu6050_init(mpu6050_handle_t*       handle,
                                   star_bus_manager_t*     manager,
                                   const char*             bus_name,
                                   star_error_interface_t* error_iface,
                                   const mpu6050_config_t* config)
{
  if (handle == NULL || manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(mpu6050_handle_t));
  handle->manager  = manager;
  handle->bus_name = bus_name;
  handle->i2c_addr = config->i2c_addr;
  memcpy(&handle->config, config, sizeof(mpu6050_config_t));

  internal_get_accel_sensitivity(config->accel_range, &handle->accel_sensitivity);
  internal_get_gyro_sensitivity(config->gyro_range, &handle->gyro_sensitivity);

  /* Create mutex first */
  handle->mutex = xSemaphoreCreateMutex();
  if (handle->mutex == NULL) {
    ESP_LOGE(s_TAG, "Failed to create mutex");
    return ESP_ERR_NO_MEM;
  }

  // Handle error interface injection
  if (error_iface == NULL) {
    handle->error_iface = star_error_interface_create_default();
    if (handle->error_iface == NULL) {
      ESP_LOGE(s_TAG, "Failed to create default error interface");
      vSemaphoreDelete(handle->mutex);
      handle->mutex = NULL;
      return ESP_ERR_NO_MEM;
    }
    handle->owns_error_handler = true;
    ESP_LOGI(s_TAG, "Created default error handler internally");
  } else {
    handle->error_iface        = error_iface;
    handle->owns_error_handler = false;
    ESP_LOGI(s_TAG, "Using injected error interface");
  }

  ESP_LOGI(s_TAG, "Initializing MPU6050 on bus '%s' at address 0x%02X", bus_name, config->i2c_addr);

  uint8_t         who_am_i;
  const esp_err_t ret = internal_mpu6050_read_reg(manager, bus_name, k_mpu6050_reg_who_am_i, &who_am_i);
  if (ret != ESP_OK || who_am_i != MPU6050_WHO_AM_I_VAL) {
    ESP_LOGE(s_TAG, "Failed to detect MPU6050 (who_am_i=0x%02X)", who_am_i);
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ESP_ERR_NOT_FOUND;
  }

  ret = star_sensor_mpu6050_reset(handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to reset: %s", esp_err_to_name(ret));
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  ret = internal_mpu6050_write_reg(manager, bus_name, k_mpu6050_reg_pwr_mgmt_1, 0x01);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set clock source: %s", esp_err_to_name(ret));
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  const uint8_t gyro_config = (config->gyro_range & 0x03) << 3;
  ret = internal_mpu6050_write_reg(manager, bus_name, k_mpu6050_reg_gyro_config, gyro_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure gyro: %s", esp_err_to_name(ret));
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  const uint8_t accel_config = (config->accel_range & 0x03) << 3;
  ret = internal_mpu6050_write_reg(manager, bus_name, k_mpu6050_reg_accel_config, accel_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure accel: %s", esp_err_to_name(ret));
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  ret = internal_mpu6050_write_reg(manager, bus_name, k_mpu6050_reg_config, config->dlpf & 0x07);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure DLPF: %s", esp_err_to_name(ret));
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  ret = internal_mpu6050_write_reg(manager, bus_name, k_mpu6050_reg_smplrt_div, config->sample_rate_div);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure sample rate: %s", esp_err_to_name(ret));
    star_error_interface_cleanup(handle->error_iface, handle->owns_error_handler);
    handle->error_iface = NULL;
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  if (config->enable_fifo) {
    ret = star_sensor_mpu6050_fifo_enable(handle, true);
    if (ret != ESP_OK) {
      ESP_LOGW(s_TAG, "Failed to enable FIFO: %s", esp_err_to_name(ret));
    }
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "MPU6050 initialization successful (thread-safe with mutex)");
  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_deinit(mpu6050_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Acquire mutex before cleanup
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(s_mpu6050_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGW(s_TAG, "Failed to acquire mutex for deinit, continuing anyway");
  }

  // Put device to sleep (no separate mutex needed, we already hold it)
  uint8_t pwr_mgmt;
  if (internal_mpu6050_read_reg(handle->manager, handle->bus_name, k_mpu6050_reg_pwr_mgmt_1, &pwr_mgmt) ==
      ESP_OK) {
    pwr_mgmt |= MPU6050_PWR1_SLEEP;
    internal_mpu6050_write_reg(handle->manager, handle->bus_name, k_mpu6050_reg_pwr_mgmt_1, pwr_mgmt);
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

  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_reset(mpu6050_handle_t* handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  return internal_mpu6050_write_reg(handle->manager,
                           handle->bus_name,
                           k_mpu6050_reg_pwr_mgmt_1,
                           MPU6050_PWR1_DEVICE_RESET);
}

esp_err_t star_sensor_mpu6050_read_accel_raw(const mpu6050_handle_t* handle,
                                             mpu6050_raw_accel_t*    accel)
{
  if (handle == NULL || accel == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Acquire mutex for thread-safe read
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(s_mpu6050_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for read_accel_raw");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  const uint8_t   reg = k_mpu6050_reg_accel_xout_h;
  uint8_t         data[6];
  const esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 6, reg, NULL);
  if (ret != ESP_OK) {
    xSemaphoreGive(mutex);
    return ret;
  }

  accel->x = (int16_t)((data[0] << 8) | data[1]);
  accel->y = (int16_t)((data[2] << 8) | data[3]);
  accel->z = (int16_t)((data[4] << 8) | data[5]);

  xSemaphoreGive(mutex);
  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_read_gyro_raw(const mpu6050_handle_t* handle,
                                            mpu6050_raw_gyro_t*     gyro)
{
  if (handle == NULL || gyro == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Acquire mutex for thread-safe read
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(s_mpu6050_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for read_gyro_raw");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t         reg = k_mpu6050_reg_gyro_xout_h;
  uint8_t         data[6];
  const esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 6, reg, NULL);
  if (ret != ESP_OK) {
    xSemaphoreGive(mutex);
    return ret;
  }

  gyro->x = (int16_t)((data[0] << 8) | data[1]);
  gyro->y = (int16_t)((data[2] << 8) | data[3]);
  gyro->z = (int16_t)((data[4] << 8) | data[5]);

  xSemaphoreGive(mutex);
  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_read_accel(const mpu6050_handle_t* handle,
                                         mpu6050_accel_t* const  accel)
{
  if (handle == NULL || accel == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Guard against division by zero from corrupted sensitivity */
  if (handle->accel_sensitivity <= 0.0f) {
    ESP_LOGE(s_TAG, "Invalid accel sensitivity: %f", handle->accel_sensitivity);
    return ESP_ERR_INVALID_STATE;
  }

  mpu6050_raw_accel_t raw;
  const esp_err_t     ret = star_sensor_mpu6050_read_accel_raw(handle, &raw);
  if (ret != ESP_OK) {
    return ret;
  }

  accel->x_g = (float)raw.x / handle->accel_sensitivity;
  accel->y_g = (float)raw.y / handle->accel_sensitivity;
  accel->z_g = (float)raw.z / handle->accel_sensitivity;

  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_read_gyro(const mpu6050_handle_t* handle, mpu6050_gyro_t* const gyro)
{
  if (handle == NULL || gyro == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Guard against division by zero from corrupted sensitivity */
  if (handle->gyro_sensitivity <= 0.0f) {
    ESP_LOGE(s_TAG, "Invalid gyro sensitivity: %f", handle->gyro_sensitivity);
    return ESP_ERR_INVALID_STATE;
  }

  mpu6050_raw_gyro_t raw;
  const esp_err_t    ret = star_sensor_mpu6050_read_gyro_raw(handle, &raw);
  if (ret != ESP_OK) {
    return ret;
  }

  gyro->x_dps = (float)raw.x / handle->gyro_sensitivity;
  gyro->y_dps = (float)raw.y / handle->gyro_sensitivity;
  gyro->z_dps = (float)raw.z / handle->gyro_sensitivity;

  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_read_temperature(const mpu6050_handle_t* handle, float* const temp_c)
{
  if (handle == NULL || temp_c == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Acquire mutex for thread-safe read
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(s_mpu6050_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for read_temperature");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  const uint8_t reg = k_mpu6050_reg_temp_out_h;
  uint8_t       data[2];
  esp_err_t     ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 2, reg, NULL);
  if (ret != ESP_OK) {
    xSemaphoreGive(mutex);
    return ret;
  }

  const int16_t temp_raw = (int16_t)((data[0] << 8) | data[1]);
  *temp_c                = (float)temp_raw / MPU6050_TEMP_SENSITIVITY + MPU6050_TEMP_OFFSET;

  xSemaphoreGive(mutex);
  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_read_all(const mpu6050_handle_t* handle,
                                       mpu6050_accel_t* const  accel,
                                       mpu6050_gyro_t* const   gyro,
                                       float* const            temp_c)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Guard against division by zero from corrupted sensitivity */
  if ((accel != NULL && handle->accel_sensitivity <= 0.0f) ||
      (gyro != NULL && handle->gyro_sensitivity <= 0.0f)) {
    ESP_LOGE(s_TAG, "Invalid sensitivity values");
    return ESP_ERR_INVALID_STATE;
  }

  // Acquire mutex for thread-safe read
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(s_mpu6050_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for read_all");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  const uint8_t reg = k_mpu6050_reg_accel_xout_h;
  uint8_t       data[14];
  esp_err_t     ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 14, reg, NULL);
  if (ret != ESP_OK) {
    xSemaphoreGive(mutex);
    return ret;
  }

  if (accel != NULL) {
    const int16_t ax = (int16_t)((data[0] << 8) | data[1]);
    const int16_t ay = (int16_t)((data[2] << 8) | data[3]);
    const int16_t az = (int16_t)((data[4] << 8) | data[5]);
    accel->x_g       = (float)ax / handle->accel_sensitivity;
    accel->y_g       = (float)ay / handle->accel_sensitivity;
    accel->z_g       = (float)az / handle->accel_sensitivity;
  }

  if (temp_c != NULL) {
    const int16_t temp_raw = (int16_t)((data[6] << 8) | data[7]);
    *temp_c                = (float)temp_raw / MPU6050_TEMP_SENSITIVITY + MPU6050_TEMP_OFFSET;
  }

  if (gyro != NULL) {
    const int16_t gx = (int16_t)((data[8] << 8) | data[9]);
    const int16_t gy = (int16_t)((data[10] << 8) | data[11]);
    const int16_t gz = (int16_t)((data[12] << 8) | data[13]);
    gyro->x_dps      = (float)gx / handle->gyro_sensitivity;
    gyro->y_dps      = (float)gy / handle->gyro_sensitivity;
    gyro->z_dps      = (float)gz / handle->gyro_sensitivity;
  }

  xSemaphoreGive(mutex);
  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_fifo_enable(mpu6050_handle_t* const handle, const bool enable)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Acquire mutex
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(s_mpu6050_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for fifo_enable");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t ret;
  if (enable) {
    const uint8_t fifo_en =
      MPU6050_FIFO_EN_ACCEL | MPU6050_FIFO_EN_XG | MPU6050_FIFO_EN_YG | MPU6050_FIFO_EN_ZG;
    ret = internal_mpu6050_write_reg(handle->manager, handle->bus_name, k_mpu6050_reg_fifo_en, fifo_en);
    if (ret != ESP_OK) {
      xSemaphoreGive(mutex);
      return ret;
    }

    ret = internal_mpu6050_write_reg(handle->manager,
                            handle->bus_name,
                            k_mpu6050_reg_user_ctrl,
                            MPU6050_USERCTRL_FIFO_EN);
    if (ret != ESP_OK) {
      xSemaphoreGive(mutex);
      return ret;
    }
  } else {
    ret = internal_mpu6050_write_reg(handle->manager, handle->bus_name, k_mpu6050_reg_user_ctrl, 0);
    if (ret != ESP_OK) {
      xSemaphoreGive(mutex);
      return ret;
    }

    ret = internal_mpu6050_write_reg(handle->manager, handle->bus_name, k_mpu6050_reg_fifo_en, 0);
    if (ret != ESP_OK) {
      xSemaphoreGive(mutex);
      return ret;
    }
  }

  handle->config.enable_fifo = enable;
  xSemaphoreGive(mutex);
  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_fifo_reset(mpu6050_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Acquire mutex for thread-safe FIFO reset
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(s_mpu6050_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for fifo_reset");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t   user_ctrl;
  esp_err_t ret =
    internal_mpu6050_read_reg(handle->manager, handle->bus_name, k_mpu6050_reg_user_ctrl, &user_ctrl);
  if (ret != ESP_OK) {
    xSemaphoreGive(mutex);
    return ret;
  }

  user_ctrl |= MPU6050_USERCTRL_FIFO_RESET;
  ret = internal_mpu6050_write_reg(handle->manager, handle->bus_name, k_mpu6050_reg_user_ctrl, user_ctrl);

  xSemaphoreGive(mutex);
  return ret;
}

esp_err_t star_sensor_mpu6050_fifo_get_count(const mpu6050_handle_t* handle, uint16_t* const count)
{
  if (handle == NULL || count == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Acquire mutex for thread-safe FIFO count read
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(s_mpu6050_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for fifo_get_count");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  const uint8_t reg = k_mpu6050_reg_fifo_count_h;
  uint8_t       data[2];
  esp_err_t     ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 2, reg, NULL);
  if (ret != ESP_OK) {
    xSemaphoreGive(mutex);
    return ret;
  }

  *count = ((uint16_t)data[0] << 8) | data[1];

  if (*count > MPU6050_FIFO_SIZE) {
    ESP_LOGW(s_TAG, "FIFO overflow detected: count=%d, auto-resetting FIFO", *count);
    /* Auto-reset FIFO on overflow to recover from corrupted state */
    uint8_t user_ctrl;
    ret = internal_mpu6050_read_reg(handle->manager, handle->bus_name, k_mpu6050_reg_user_ctrl, &user_ctrl);
    if (ret == ESP_OK) {
      user_ctrl |= MPU6050_USERCTRL_FIFO_RESET;
      internal_mpu6050_write_reg(handle->manager, handle->bus_name, k_mpu6050_reg_user_ctrl, user_ctrl);
    }
    *count = 0;
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  xSemaphoreGive(mutex);
  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_fifo_read(const mpu6050_handle_t* handle,
                                        uint8_t* const          data,
                                        const uint16_t          len)
{
  if (handle == NULL || data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (len > MPU6050_FIFO_SIZE) {
    ESP_LOGE(s_TAG, "Read length %d exceeds FIFO size %d", len, MPU6050_FIFO_SIZE);
    return ESP_ERR_INVALID_ARG;
  }

  // Acquire mutex for thread-safe FIFO read
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(s_mpu6050_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for fifo_read");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  const uint8_t   reg = k_mpu6050_reg_fifo_r_w;
  const esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, len, reg, NULL);

  xSemaphoreGive(mutex);
  return ret;
}

esp_err_t star_sensor_mpu6050_set_sleep(mpu6050_handle_t* const handle, const bool sleep)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Acquire mutex
  const SemaphoreHandle_t mutex = handle->mutex;
  if (mutex == NULL || xSemaphoreTake(mutex, pdMS_TO_TICKS(s_mpu6050_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for set_sleep");
    return ESP_ERR_TIMEOUT;
  }

  // Re-check after mutex acquisition
  if (!handle->initialized || handle->mutex == NULL) {
    xSemaphoreGive(mutex);
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t   pwr_mgmt;
  esp_err_t ret =
    internal_mpu6050_read_reg(handle->manager, handle->bus_name, k_mpu6050_reg_pwr_mgmt_1, &pwr_mgmt);
  if (ret != ESP_OK) {
    xSemaphoreGive(mutex);
    return ret;
  }

  if (sleep) {
    pwr_mgmt |= MPU6050_PWR1_SLEEP;
  } else {
    pwr_mgmt &= ~MPU6050_PWR1_SLEEP;
  }

  ret = internal_mpu6050_write_reg(handle->manager, handle->bus_name, k_mpu6050_reg_pwr_mgmt_1, pwr_mgmt);

  xSemaphoreGive(mutex);
  return ret;
}
