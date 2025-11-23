/* lib/star_sensor_qmc5883l/src/star_sensor_qmc5883l.c */

#include "star_sensor_qmc5883l.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

#include "star_bus_i2c.h"

static const char* s_TAG = "qmc5883l";

static esp_err_t qmc5883l_write_reg(star_bus_manager_t* manager,
                                    const char*         bus_name,
                                    uint8_t             addr,
                                    uint8_t             reg,
                                    uint8_t             value)
{
  (void)addr;  // Address is stored in bus config
  return star_bus_i2c_write(manager, bus_name, &value, 1, reg, NULL);
}

static esp_err_t qmc5883l_read_reg(star_bus_manager_t* manager,
                                   const char*         bus_name,
                                   uint8_t             addr,
                                   uint8_t             reg,
                                   uint8_t*            value)
{
  (void)addr;  // Address is stored in bus config
  return star_bus_i2c_read(manager, bus_name, value, 1, reg, NULL);
}

static float get_sensitivity(qmc5883l_range_t range)
{
  return (range == QMC5883L_RNG_2G_VAL) ? 12000.0f : 3000.0f;
}

esp_err_t star_sensor_qmc5883l_init(qmc5883l_handle_t*       handle,
                                    star_bus_manager_t*      manager,
                                    const char*              bus_name,
                                    const qmc5883l_config_t* config)
{
  if (handle == NULL || manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(qmc5883l_handle_t));
  handle->manager  = manager;
  handle->bus_name = bus_name;
  handle->i2c_addr = QMC5883L_I2C_ADDR;
  memcpy(&handle->config, config, sizeof(qmc5883l_config_t));
  handle->sensitivity = get_sensitivity(config->range);

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG, "Initializing QMC5883L on bus '%s'", bus_name);

  uint8_t chip_id;
  ret = qmc5883l_read_reg(manager, bus_name, QMC5883L_I2C_ADDR, QMC5883L_REG_CHIP_ID, &chip_id);
  if (ret != ESP_OK || chip_id != QMC5883L_CHIP_ID) {
    ESP_LOGE(s_TAG, "Failed to detect QMC5883L (chip_id=0x%02X, expected=0x%02X)", chip_id, QMC5883L_CHIP_ID);
    error_handler_deinit(&handle->error_handler);
    return ESP_ERR_NOT_FOUND;
  }

  ret = star_sensor_qmc5883l_reset(handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to reset: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  ret = qmc5883l_write_reg(manager, bus_name, QMC5883L_I2C_ADDR, QMC5883L_REG_SET_RESET, 0x01);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure SET/RESET: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  uint8_t ctrl1 = (config->mode & 0x03) | (config->odr << 2) | (config->range << 4) | (config->osr << 6);
  ret = qmc5883l_write_reg(manager, bus_name, QMC5883L_I2C_ADDR, QMC5883L_REG_CONTROL1, ctrl1);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure CONTROL1: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "QMC5883L initialization successful");
  return ESP_OK;
}

esp_err_t star_sensor_qmc5883l_deinit(qmc5883l_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  star_sensor_qmc5883l_set_mode(handle, QMC5883L_MODE_STANDBY_VAL);
  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;
  return ESP_OK;
}

esp_err_t star_sensor_qmc5883l_reset(const qmc5883l_handle_t* handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  return qmc5883l_write_reg(handle->manager,
                            handle->bus_name,
                            handle->i2c_addr,
                            QMC5883L_REG_CONTROL2,
                            QMC5883L_SOFT_RST);
}

esp_err_t star_sensor_qmc5883l_read_raw(const qmc5883l_handle_t* handle, qmc5883l_raw_data_t* data)
{
  if (handle == NULL || data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t buf[6];
  esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, buf, 6, QMC5883L_REG_DATA_OUT_X_LSB, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  data->x = (int16_t)(buf[0] | (buf[1] << 8));
  data->y = (int16_t)(buf[2] | (buf[3] << 8));
  data->z = (int16_t)(buf[4] | (buf[5] << 8));

  ESP_LOGD(s_TAG, "Raw: X=%d, Y=%d, Z=%d", data->x, data->y, data->z);
  return ESP_OK;
}

esp_err_t star_sensor_qmc5883l_read_mag(const qmc5883l_handle_t* handle, qmc5883l_mag_data_t* data)
{
  if (handle == NULL || data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  qmc5883l_raw_data_t raw;
  esp_err_t ret = star_sensor_qmc5883l_read_raw(handle, &raw);
  if (ret != ESP_OK) {
    return ret;
  }

  data->x_gauss = (float)raw.x / handle->sensitivity;
  data->y_gauss = (float)raw.y / handle->sensitivity;
  data->z_gauss = (float)raw.z / handle->sensitivity;

  return ESP_OK;
}

esp_err_t star_sensor_qmc5883l_read_heading(const qmc5883l_handle_t* handle, float* heading)
{
  if (handle == NULL || heading == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  qmc5883l_mag_data_t mag;
  esp_err_t ret = star_sensor_qmc5883l_read_mag(handle, &mag);
  if (ret != ESP_OK) {
    return ret;
  }

  *heading = atan2f(mag.y_gauss, mag.x_gauss) * 180.0f / M_PI;
  if (*heading < 0) {
    *heading += 360.0f;
  }

  return ESP_OK;
}

esp_err_t star_sensor_qmc5883l_read_temperature(const qmc5883l_handle_t* handle, float* temp_c)
{
  if (handle == NULL || temp_c == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t buf[2];
  esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, buf, 2, QMC5883L_REG_TEMP_LSB, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  int16_t temp_raw = (int16_t)(buf[0] | (buf[1] << 8));
  *temp_c = (float)temp_raw / 100.0f;

  return ESP_OK;
}

esp_err_t star_sensor_qmc5883l_set_mode(qmc5883l_handle_t* handle, qmc5883l_mode_t mode)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t ctrl1;
  esp_err_t ret = qmc5883l_read_reg(handle->manager,
                                    handle->bus_name,
                                    handle->i2c_addr,
                                    QMC5883L_REG_CONTROL1,
                                    &ctrl1);
  if (ret != ESP_OK) {
    return ret;
  }

  ctrl1 = (ctrl1 & ~0x03) | (mode & 0x03);
  ret = qmc5883l_write_reg(handle->manager,
                           handle->bus_name,
                           handle->i2c_addr,
                           QMC5883L_REG_CONTROL1,
                           ctrl1);

  if (ret == ESP_OK) {
    handle->config.mode = mode;
  }

  return ret;
}

esp_err_t star_sensor_qmc5883l_set_odr(qmc5883l_handle_t* handle, qmc5883l_odr_t odr)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t ctrl1;
  esp_err_t ret = qmc5883l_read_reg(handle->manager,
                                    handle->bus_name,
                                    handle->i2c_addr,
                                    QMC5883L_REG_CONTROL1,
                                    &ctrl1);
  if (ret != ESP_OK) {
    return ret;
  }

  ctrl1 = (ctrl1 & ~0x0C) | ((odr & 0x03) << 2);
  ret = qmc5883l_write_reg(handle->manager,
                           handle->bus_name,
                           handle->i2c_addr,
                           QMC5883L_REG_CONTROL1,
                           ctrl1);

  if (ret == ESP_OK) {
    handle->config.odr = odr;
  }

  return ret;
}

esp_err_t star_sensor_qmc5883l_set_range(qmc5883l_handle_t* handle, qmc5883l_range_t range)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t ctrl1;
  esp_err_t ret = qmc5883l_read_reg(handle->manager,
                                    handle->bus_name,
                                    handle->i2c_addr,
                                    QMC5883L_REG_CONTROL1,
                                    &ctrl1);
  if (ret != ESP_OK) {
    return ret;
  }

  ctrl1 = (ctrl1 & ~0x30) | ((range & 0x03) << 4);
  ret = qmc5883l_write_reg(handle->manager,
                           handle->bus_name,
                           handle->i2c_addr,
                           QMC5883L_REG_CONTROL1,
                           ctrl1);

  if (ret == ESP_OK) {
    handle->config.range = range;
    handle->sensitivity = get_sensitivity(range);
  }

  return ret;
}

esp_err_t star_sensor_qmc5883l_is_data_ready(const qmc5883l_handle_t* handle, bool* ready)
{
  if (handle == NULL || ready == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t status;
  esp_err_t ret =
    qmc5883l_read_reg(handle->manager, handle->bus_name, handle->i2c_addr, QMC5883L_REG_STATUS, &status);
  if (ret != ESP_OK) {
    return ret;
  }

  *ready = (status & QMC5883L_STATUS_DRDY) != 0;
  return ESP_OK;
}

esp_err_t star_sensor_qmc5883l_check_overflow(const qmc5883l_handle_t* handle, bool* overflow)
{
  if (handle == NULL || overflow == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t status;
  esp_err_t ret =
    qmc5883l_read_reg(handle->manager, handle->bus_name, handle->i2c_addr, QMC5883L_REG_STATUS, &status);
  if (ret != ESP_OK) {
    return ret;
  }

  *overflow = (status & QMC5883L_STATUS_OVL) != 0;
  return ESP_OK;
}
