/* lib/star_sensor_bh1750/src/star_sensor_bh1750.c */

#include "star_sensor_bh1750.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "star_bus_i2c.h"

static const char* s_TAG = "bh1750";

/* --- Internal Helper Functions --- */

static esp_err_t bh1750_send_command(star_bus_manager_t* manager,
                                     const char*         bus_name,
                                     uint8_t             addr,
                                     uint8_t             cmd)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  return star_bus_i2c_write_command(manager, bus_name, cmd);
}

static esp_err_t bh1750_read_data(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  uint8_t             addr,
                                  uint16_t*           value)
{
  if (manager == NULL || bus_name == NULL || value == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t data[2];
  esp_err_t ret = star_bus_i2c_read_raw(manager, bus_name, data, 2, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  *value = (data[0] << 8) | data[1];
  return ESP_OK;
}

static uint8_t get_mode_command(bh1750_mode_t mode, bh1750_resolution_t resolution)
{
  if (mode == BH1750_MODE_CONTINUOUS) {
    switch (resolution) {
      case BH1750_RES_LOW: return BH1750_CMD_CONT_L_RES_MODE;
      case BH1750_RES_HIGH: return BH1750_CMD_CONT_H_RES_MODE;
      case BH1750_RES_HIGH2: return BH1750_CMD_CONT_H_RES_MODE2;
      default: return BH1750_CMD_CONT_H_RES_MODE;
    }
  } else {
    switch (resolution) {
      case BH1750_RES_LOW: return BH1750_CMD_ONE_TIME_L_RES;
      case BH1750_RES_HIGH: return BH1750_CMD_ONE_TIME_H_RES;
      case BH1750_RES_HIGH2: return BH1750_CMD_ONE_TIME_H_RES2;
      default: return BH1750_CMD_ONE_TIME_H_RES;
    }
  }
}

/* --- Core Functions --- */

esp_err_t star_sensor_bh1750_init(bh1750_handle_t*       handle,
                                  star_bus_manager_t*    manager,
                                  const char*            bus_name,
                                  const bh1750_config_t* config)
{
  if (handle == NULL || manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  if (config->mtreg < BH1750_MTREG_MIN || config->mtreg > BH1750_MTREG_MAX) {
    ESP_LOGE(s_TAG,
             "Invalid MTreg: %d (valid: %d-%d)",
             config->mtreg,
             BH1750_MTREG_MIN,
             BH1750_MTREG_MAX);
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(bh1750_handle_t));
  handle->manager  = manager;
  handle->bus_name = bus_name;
  handle->i2c_addr = config->i2c_addr;
  memcpy(&handle->config, config, sizeof(bh1750_config_t));

  esp_err_t ret = error_handler_init(&handle->error_handler,
                                     3,
                                     10,
                                     100,
                                     NULL,
                                     NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG, "Initializing BH1750 on bus '%s' at address 0x%02X", bus_name, config->i2c_addr);

  ret = star_sensor_bh1750_power(handle, true);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to power on: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  ret = star_sensor_bh1750_reset(handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to reset: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  if (config->mtreg != BH1750_DEFAULT_MTREG) {
    ret = star_sensor_bh1750_set_mtreg(handle, config->mtreg);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to set MTreg: %s", esp_err_to_name(ret));
      error_handler_deinit(&handle->error_handler);
      return ret;
    }
  }

  ret = star_sensor_bh1750_set_mode(handle, config->mode, config->resolution);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set mode: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "BH1750 initialization successful");

  return ESP_OK;
}

esp_err_t star_sensor_bh1750_deinit(bh1750_handle_t* handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!handle->initialized) {
    return ESP_OK;
  }

  ESP_LOGI(s_TAG, "Deinitializing BH1750");

  star_sensor_bh1750_power(handle, false);
  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_sensor_bh1750_power(const bh1750_handle_t* handle, bool power)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t cmd = power ? BH1750_CMD_POWER_ON : BH1750_CMD_POWER_DOWN;
  return bh1750_send_command(handle->manager, handle->bus_name, handle->i2c_addr, cmd);
}

esp_err_t star_sensor_bh1750_reset(const bh1750_handle_t* handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  return bh1750_send_command(handle->manager, handle->bus_name, handle->i2c_addr, BH1750_CMD_RESET);
}

/* --- Measurement Functions --- */

esp_err_t star_sensor_bh1750_read_light(const bh1750_handle_t* handle, float* lux)
{
  if (handle == NULL || lux == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret;

  if (handle->config.mode == BH1750_MODE_ONE_TIME) {
    ret = star_sensor_bh1750_trigger_measurement(handle);
    if (ret != ESP_OK) {
      return ret;
    }

    uint32_t wait_time;
    ret = star_sensor_bh1750_get_measurement_time(handle, &wait_time);
    if (ret != ESP_OK) {
      return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(wait_time + 10));
  }

  return star_sensor_bh1750_get_result(handle, lux);
}

esp_err_t star_sensor_bh1750_trigger_measurement(const bh1750_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t cmd = get_mode_command(BH1750_MODE_ONE_TIME, handle->config.resolution);
  return bh1750_send_command(handle->manager, handle->bus_name, handle->i2c_addr, cmd);
}

esp_err_t star_sensor_bh1750_get_result(const bh1750_handle_t* handle, float* lux)
{
  if (handle == NULL || lux == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t raw_value;
  esp_err_t ret = bh1750_read_data(handle->manager, handle->bus_name, handle->i2c_addr, &raw_value);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read data: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = star_sensor_bh1750_raw_to_lux(raw_value,
                                      handle->config.resolution,
                                      handle->config.mtreg,
                                      lux);
  if (ret == ESP_OK) {
    ESP_LOGD(s_TAG, "Light level: %.2f lux (raw: %d)", *lux, raw_value);
  }

  return ret;
}

/* --- Configuration Functions --- */

esp_err_t star_sensor_bh1750_set_mode(bh1750_handle_t*    handle,
                                      bh1750_mode_t       mode,
                                      bh1750_resolution_t resolution)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t cmd = get_mode_command(mode, resolution);
  esp_err_t ret = bh1750_send_command(handle->manager, handle->bus_name, handle->i2c_addr, cmd);

  if (ret == ESP_OK) {
    handle->config.mode = mode;
    handle->config.resolution = resolution;
    handle->last_cmd = cmd;
  }

  return ret;
}

esp_err_t star_sensor_bh1750_set_mtreg(bh1750_handle_t* handle, uint8_t mtreg)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (mtreg < BH1750_MTREG_MIN || mtreg > BH1750_MTREG_MAX) {
    ESP_LOGE(s_TAG, "MTreg %d out of range (%d-%d)", mtreg, BH1750_MTREG_MIN, BH1750_MTREG_MAX);
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t high_bits = (mtreg >> 5) & 0x07;
  uint8_t low_bits = mtreg & 0x1F;

  uint8_t cmd_high = BH1750_CMD_CHANGE_MEAS_TIME_H | high_bits;
  esp_err_t ret =
    bh1750_send_command(handle->manager, handle->bus_name, handle->i2c_addr, cmd_high);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set MTreg high bits: %s", esp_err_to_name(ret));
    return ret;
  }

  uint8_t cmd_low = BH1750_CMD_CHANGE_MEAS_TIME_L | low_bits;
  ret = bh1750_send_command(handle->manager, handle->bus_name, handle->i2c_addr, cmd_low);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set MTreg low bits: %s", esp_err_to_name(ret));
    return ret;
  }

  handle->config.mtreg = mtreg;
  ESP_LOGI(s_TAG, "Set MTreg to %d", mtreg);

  return ESP_OK;
}

/* --- Helper Functions --- */

esp_err_t star_sensor_bh1750_get_measurement_time(const bh1750_handle_t* handle, uint32_t* time_ms)
{
  if (handle == NULL || time_ms == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uint32_t base_time;
  switch (handle->config.resolution) {
    case BH1750_RES_LOW:
      base_time = BH1750_MEAS_TIME_L_RES;
      break;
    case BH1750_RES_HIGH:
    case BH1750_RES_HIGH2:
      base_time = BH1750_MEAS_TIME_H_RES;
      break;
    default:
      return ESP_ERR_INVALID_STATE;
  }

  *time_ms = (base_time * handle->config.mtreg) / BH1750_DEFAULT_MTREG;
  return ESP_OK;
}

esp_err_t star_sensor_bh1750_raw_to_lux(uint16_t            raw_value,
                                        bh1750_resolution_t resolution,
                                        uint8_t             mtreg,
                                        float*              lux)
{
  if (lux == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (mtreg == 0) {
    ESP_LOGE(s_TAG, "Division by zero: mtreg is 0");
    return ESP_ERR_INVALID_ARG;
  }

  float divisor = 1.2f;
  if (resolution == BH1750_RES_HIGH2) {
    divisor = 0.6f;
  }

  float sensitivity_correction = (float)BH1750_DEFAULT_MTREG / (float)mtreg;
  *lux = ((float)raw_value / divisor) * sensitivity_correction;

  return ESP_OK;
}
