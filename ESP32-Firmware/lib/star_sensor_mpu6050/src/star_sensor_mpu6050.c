/* lib/star_sensor_mpu6050/src/star_sensor_mpu6050.c */

#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "star_bus_i2c.h"

static const char* s_TAG = "mpu6050";

static esp_err_t mpu6050_write_reg(star_bus_manager_t* manager,
                                   const char*         bus_name,
                                   uint8_t             addr,
                                   uint8_t             reg,
                                   uint8_t             value)
{
  return star_bus_i2c_write(manager, bus_name, &value, 1, reg, NULL);
}

static esp_err_t mpu6050_read_reg(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  uint8_t             addr,
                                  uint8_t             reg,
                                  uint8_t*            value)
{
  return star_bus_i2c_read(manager, bus_name, value, 1, reg, NULL);
}

static void get_accel_sensitivity(mpu6050_accel_range_t range, float* sensitivity)
{
  switch (range) {
    case MPU6050_ACCEL_RANGE_2G:  *sensitivity = 16384.0f; break;
    case MPU6050_ACCEL_RANGE_4G:  *sensitivity = 8192.0f; break;
    case MPU6050_ACCEL_RANGE_8G:  *sensitivity = 4096.0f; break;
    case MPU6050_ACCEL_RANGE_16G: *sensitivity = 2048.0f; break;
    default: *sensitivity = 16384.0f;
  }
}

static void get_gyro_sensitivity(mpu6050_gyro_range_t range, float* sensitivity)
{
  switch (range) {
    case MPU6050_GYRO_RANGE_250:  *sensitivity = 131.0f; break;
    case MPU6050_GYRO_RANGE_500:  *sensitivity = 65.5f; break;
    case MPU6050_GYRO_RANGE_1000: *sensitivity = 32.8f; break;
    case MPU6050_GYRO_RANGE_2000: *sensitivity = 16.4f; break;
    default: *sensitivity = 131.0f;
  }
}

esp_err_t star_sensor_mpu6050_init(mpu6050_handle_t*       handle,
                                   star_bus_manager_t*     manager,
                                   const char*             bus_name,
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

  get_accel_sensitivity(config->accel_range, &handle->accel_sensitivity);
  get_gyro_sensitivity(config->gyro_range, &handle->gyro_sensitivity);

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG, "Initializing MPU6050 on bus '%s' at address 0x%02X", bus_name, config->i2c_addr);

  uint8_t who_am_i;
  ret = mpu6050_read_reg(manager, bus_name, config->i2c_addr, MPU6050_REG_WHO_AM_I, &who_am_i);
  if (ret != ESP_OK || who_am_i != MPU6050_WHO_AM_I_VAL) {
    ESP_LOGE(s_TAG, "Failed to detect MPU6050 (who_am_i=0x%02X)", who_am_i);
    error_handler_deinit(&handle->error_handler);
    return ESP_ERR_NOT_FOUND;
  }

  ret = star_sensor_mpu6050_reset(handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to reset: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  ret = mpu6050_write_reg(manager, bus_name, config->i2c_addr, MPU6050_REG_PWR_MGMT_1, 0x01);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set clock source: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  uint8_t gyro_config = (config->gyro_range & 0x03) << 3;
  ret = mpu6050_write_reg(manager, bus_name, config->i2c_addr, MPU6050_REG_GYRO_CONFIG, gyro_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure gyro: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  uint8_t accel_config = (config->accel_range & 0x03) << 3;
  ret = mpu6050_write_reg(manager, bus_name, config->i2c_addr, MPU6050_REG_ACCEL_CONFIG, accel_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure accel: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  ret = mpu6050_write_reg(manager, bus_name, config->i2c_addr, MPU6050_REG_CONFIG, config->dlpf & 0x07);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure DLPF: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  ret = mpu6050_write_reg(manager, bus_name, config->i2c_addr, MPU6050_REG_SMPLRT_DIV, config->sample_rate_div);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure sample rate: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  if (config->enable_fifo) {
    ret = star_sensor_mpu6050_fifo_enable(handle, true);
    if (ret != ESP_OK) {
      ESP_LOGW(s_TAG, "Failed to enable FIFO: %s", esp_err_to_name(ret));
    }
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "MPU6050 initialization successful");
  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_deinit(mpu6050_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  star_sensor_mpu6050_set_sleep(handle, true);
  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;
  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_reset(const mpu6050_handle_t* handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  return mpu6050_write_reg(handle->manager,
                           handle->bus_name,
                           handle->i2c_addr,
                           MPU6050_REG_PWR_MGMT_1,
                           MPU6050_PWR1_DEVICE_RESET);
}

esp_err_t star_sensor_mpu6050_read_accel_raw(const mpu6050_handle_t* handle,
                                             mpu6050_raw_accel_t*    accel)
{
  if (handle == NULL || accel == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
  uint8_t data[6];
  esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 6, reg, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  accel->x = (int16_t)((data[0] << 8) | data[1]);
  accel->y = (int16_t)((data[2] << 8) | data[3]);
  accel->z = (int16_t)((data[4] << 8) | data[5]);

  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_read_gyro_raw(const mpu6050_handle_t* handle,
                                            mpu6050_raw_gyro_t*     gyro)
{
  if (handle == NULL || gyro == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t reg = MPU6050_REG_GYRO_XOUT_H;
  uint8_t data[6];
  esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 6, reg, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  gyro->x = (int16_t)((data[0] << 8) | data[1]);
  gyro->y = (int16_t)((data[2] << 8) | data[3]);
  gyro->z = (int16_t)((data[4] << 8) | data[5]);

  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_read_accel(const mpu6050_handle_t* handle, mpu6050_accel_t* accel)
{
  if (handle == NULL || accel == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  mpu6050_raw_accel_t raw;
  esp_err_t ret = star_sensor_mpu6050_read_accel_raw(handle, &raw);
  if (ret != ESP_OK) return ret;

  accel->x_g = (float)raw.x / handle->accel_sensitivity;
  accel->y_g = (float)raw.y / handle->accel_sensitivity;
  accel->z_g = (float)raw.z / handle->accel_sensitivity;

  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_read_gyro(const mpu6050_handle_t* handle, mpu6050_gyro_t* gyro)
{
  if (handle == NULL || gyro == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  mpu6050_raw_gyro_t raw;
  esp_err_t ret = star_sensor_mpu6050_read_gyro_raw(handle, &raw);
  if (ret != ESP_OK) return ret;

  gyro->x_dps = (float)raw.x / handle->gyro_sensitivity;
  gyro->y_dps = (float)raw.y / handle->gyro_sensitivity;
  gyro->z_dps = (float)raw.z / handle->gyro_sensitivity;

  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_read_temperature(const mpu6050_handle_t* handle, float* temp_c)
{
  if (handle == NULL || temp_c == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t reg = MPU6050_REG_TEMP_OUT_H;
  uint8_t data[2];
  esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 2, reg, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  int16_t temp_raw = (int16_t)((data[0] << 8) | data[1]);
  *temp_c = (float)temp_raw / 340.0f + 36.53f;

  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_read_all(const mpu6050_handle_t* handle,
                                       mpu6050_accel_t*        accel,
                                       mpu6050_gyro_t*         gyro,
                                       float*                  temp_c)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
  uint8_t data[14];
  esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 14, reg, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  if (accel != NULL) {
    int16_t ax = (int16_t)((data[0] << 8) | data[1]);
    int16_t ay = (int16_t)((data[2] << 8) | data[3]);
    int16_t az = (int16_t)((data[4] << 8) | data[5]);
    accel->x_g = (float)ax / handle->accel_sensitivity;
    accel->y_g = (float)ay / handle->accel_sensitivity;
    accel->z_g = (float)az / handle->accel_sensitivity;
  }

  if (temp_c != NULL) {
    int16_t temp_raw = (int16_t)((data[6] << 8) | data[7]);
    *temp_c = (float)temp_raw / 340.0f + 36.53f;
  }

  if (gyro != NULL) {
    int16_t gx = (int16_t)((data[8] << 8) | data[9]);
    int16_t gy = (int16_t)((data[10] << 8) | data[11]);
    int16_t gz = (int16_t)((data[12] << 8) | data[13]);
    gyro->x_dps = (float)gx / handle->gyro_sensitivity;
    gyro->y_dps = (float)gy / handle->gyro_sensitivity;
    gyro->z_dps = (float)gz / handle->gyro_sensitivity;
  }

  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_fifo_enable(mpu6050_handle_t* handle, bool enable)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  if (enable) {
    uint8_t fifo_en = MPU6050_FIFO_EN_ACCEL | MPU6050_FIFO_EN_XG | MPU6050_FIFO_EN_YG | MPU6050_FIFO_EN_ZG;
    esp_err_t ret = mpu6050_write_reg(handle->manager, handle->bus_name, handle->i2c_addr, MPU6050_REG_FIFO_EN, fifo_en);
    if (ret != ESP_OK) return ret;

    ret = mpu6050_write_reg(handle->manager, handle->bus_name, handle->i2c_addr, MPU6050_REG_USER_CTRL, MPU6050_USERCTRL_FIFO_EN);
    if (ret != ESP_OK) return ret;
  } else {
    esp_err_t ret = mpu6050_write_reg(handle->manager, handle->bus_name, handle->i2c_addr, MPU6050_REG_USER_CTRL, 0);
    if (ret != ESP_OK) return ret;

    ret = mpu6050_write_reg(handle->manager, handle->bus_name, handle->i2c_addr, MPU6050_REG_FIFO_EN, 0);
    if (ret != ESP_OK) return ret;
  }

  handle->config.enable_fifo = enable;
  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_fifo_reset(const mpu6050_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t user_ctrl;
  esp_err_t ret = mpu6050_read_reg(handle->manager, handle->bus_name, handle->i2c_addr, MPU6050_REG_USER_CTRL, &user_ctrl);
  if (ret != ESP_OK) return ret;

  user_ctrl |= MPU6050_USERCTRL_FIFO_RESET;
  return mpu6050_write_reg(handle->manager, handle->bus_name, handle->i2c_addr, MPU6050_REG_USER_CTRL, user_ctrl);
}

esp_err_t star_sensor_mpu6050_fifo_get_count(const mpu6050_handle_t* handle, uint16_t* count)
{
  if (handle == NULL || count == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t reg = MPU6050_REG_FIFO_COUNT_H;
  uint8_t data[2];
  esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 2, reg, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  *count = ((uint16_t)data[0] << 8) | data[1];

  if (*count > MPU6050_FIFO_SIZE) {
    ESP_LOGW(s_TAG, "FIFO overflow detected: count=%d", *count);
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

esp_err_t star_sensor_mpu6050_fifo_read(const mpu6050_handle_t* handle, uint8_t* data, uint16_t len)
{
  if (handle == NULL || data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (len > MPU6050_FIFO_SIZE) {
    ESP_LOGE(s_TAG, "Read length %d exceeds FIFO size %d", len, MPU6050_FIFO_SIZE);
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t reg = MPU6050_REG_FIFO_R_W;
  return star_bus_i2c_read(handle->manager, handle->bus_name, data, len, reg, NULL);
}

esp_err_t star_sensor_mpu6050_set_sleep(mpu6050_handle_t* handle, bool sleep)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t pwr_mgmt;
  esp_err_t ret = mpu6050_read_reg(handle->manager, handle->bus_name, handle->i2c_addr, MPU6050_REG_PWR_MGMT_1, &pwr_mgmt);
  if (ret != ESP_OK) return ret;

  if (sleep) {
    pwr_mgmt |= MPU6050_PWR1_SLEEP;
  } else {
    pwr_mgmt &= ~MPU6050_PWR1_SLEEP;
  }

  return mpu6050_write_reg(handle->manager, handle->bus_name, handle->i2c_addr, MPU6050_REG_PWR_MGMT_1, pwr_mgmt);
}
