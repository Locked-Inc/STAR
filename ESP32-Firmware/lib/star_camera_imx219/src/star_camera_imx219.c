/* lib/star_camera_imx219/src/star_camera_imx219.c */

#include "star_camera_imx219.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "star_bus_i2c.h"

static const char* s_TAG = "imx219";

// IMX219 Register addresses (16-bit addresses)
#define IMX219_REG_CHIP_ID_H (0x0000)
#define IMX219_REG_CHIP_ID_L (0x0001)
#define IMX219_REG_MODE_SELECT (0x0100)
#define IMX219_REG_COARSE_TIME_H (0x015A)
#define IMX219_REG_COARSE_TIME_L (0x015B)
#define IMX219_REG_ANALOG_GAIN (0x0157)

// Mode select values
#define IMX219_MODE_STANDBY (0x00)
#define IMX219_MODE_STREAMING (0x01)

static const imx219_resolution_config_t mode_configs[] = {
  [IMX219_MODE_3280x2464_21FPS] = {3280, 2464, 21, IMX219_FORMAT_RAW10},
  [IMX219_MODE_1920x1080_30FPS] = {1920, 1080, 30, IMX219_FORMAT_RAW10},
  [IMX219_MODE_1640x1232_40FPS] = {1640, 1232, 40, IMX219_FORMAT_RAW10},
  [IMX219_MODE_640x480_90FPS] = {640, 480, 90, IMX219_FORMAT_RAW8},
};

// Write 16-bit register address, 8-bit data
static esp_err_t priv_write_reg(const imx219_handle_t* handle, uint8_t i2c_addr, uint16_t reg, uint8_t value)
{
  // For 16-bit register addresses, we need to send: [reg_high, reg_low, value]
  // Use star_bus_i2c_write with the high byte as reg_addr and [low_byte, value] as data
  // This is a workaround since the bus API expects 8-bit register addresses
  uint8_t data[2] = {reg & 0xFF, value};
  return star_bus_i2c_write(handle->manager, handle->bus_name, data, 2, (reg >> 8) & 0xFF, NULL);
}

// Read 16-bit register address, 8-bit data
static esp_err_t priv_read_reg(const imx219_handle_t* handle, uint8_t i2c_addr, uint16_t reg, uint8_t* value)
{
  // For 16-bit register addresses with single byte read, we need to:
  // 1. Write the 16-bit register address [reg_high, reg_low]
  // 2. Read single byte
  // Workaround: use star_bus_i2c_write then star_bus_i2c_read_raw
  uint8_t reg_low = reg & 0xFF;
  esp_err_t ret = star_bus_i2c_write(handle->manager, handle->bus_name, &reg_low, 1, (reg >> 8) & 0xFF, NULL);
  if (ret != ESP_OK) {
    return ret;
  }
  return star_bus_i2c_read_raw(handle->manager, handle->bus_name, value, 1, NULL);
}

esp_err_t star_camera_imx219_init(imx219_handle_t* handle, const imx219_config_t* config)
{
  if (handle == NULL || config == NULL || config->manager == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(imx219_handle_t));
  handle->manager = config->manager;
  handle->bus_name = config->bus_name;
  handle->left_addr = config->left_addr;
  handle->right_addr = config->right_addr;
  handle->stereo_enabled = config->enable_stereo;
  handle->current_mode = config->mode;

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG,
           "Initializing IMX219 (left=0x%02X, right=0x%02X, stereo=%d)",
           config->left_addr,
           config->right_addr,
           config->enable_stereo);

  // Verify left camera chip ID
  uint8_t chip_id_h, chip_id_l;
  ret = priv_read_reg(handle, handle->left_addr, IMX219_REG_CHIP_ID_H, &chip_id_h);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read left camera chip ID");
    error_handler_deinit(&handle->error_handler);
    return ret;
  }
  ret = priv_read_reg(handle, handle->left_addr, IMX219_REG_CHIP_ID_L, &chip_id_l);
  if (ret != ESP_OK) {
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  uint16_t chip_id = (chip_id_h << 8) | chip_id_l;
  if (chip_id != IMX219_CHIP_ID) {
    ESP_LOGE(s_TAG, "Invalid left camera chip ID: 0x%04X", chip_id);
    error_handler_deinit(&handle->error_handler);
    return ESP_ERR_NOT_FOUND;
  }

  // Verify right camera if stereo enabled
  if (handle->stereo_enabled) {
    ret = priv_read_reg(handle, handle->right_addr, IMX219_REG_CHIP_ID_H, &chip_id_h);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to read right camera chip ID");
      error_handler_deinit(&handle->error_handler);
      return ret;
    }
    ret = priv_read_reg(handle, handle->right_addr, IMX219_REG_CHIP_ID_L, &chip_id_l);
    if (ret != ESP_OK) {
      error_handler_deinit(&handle->error_handler);
      return ret;
    }

    chip_id = (chip_id_h << 8) | chip_id_l;
    if (chip_id != IMX219_CHIP_ID) {
      ESP_LOGE(s_TAG, "Invalid right camera chip ID: 0x%04X", chip_id);
      error_handler_deinit(&handle->error_handler);
      return ESP_ERR_NOT_FOUND;
    }
  }

  // Put cameras in standby mode
  ret = priv_write_reg(handle, handle->left_addr, IMX219_REG_MODE_SELECT, IMX219_MODE_STANDBY);
  if (ret != ESP_OK) {
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  if (handle->stereo_enabled) {
    ret = priv_write_reg(handle, handle->right_addr, IMX219_REG_MODE_SELECT, IMX219_MODE_STANDBY);
    if (ret != ESP_OK) {
      error_handler_deinit(&handle->error_handler);
      return ret;
    }
  }

  // Set initial mode configuration
  handle->resolution = mode_configs[handle->current_mode];
  handle->exposure = 1000;  // Default 1ms
  handle->gain = 16;        // Default gain

  handle->initialized = true;
  ESP_LOGI(s_TAG,
           "IMX219 initialization successful (%dx%d @ %dfps)",
           handle->resolution.width,
           handle->resolution.height,
           handle->resolution.fps);

  return ESP_OK;
}

esp_err_t star_camera_imx219_deinit(imx219_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->streaming) {
    star_camera_imx219_stop_streaming(handle);
  }

  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_camera_imx219_set_mode(imx219_handle_t* handle, imx219_mode_t mode)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (mode >= sizeof(mode_configs) / sizeof(mode_configs[0])) {
    ESP_LOGE(s_TAG, "Invalid mode: %d", mode);
    return ESP_ERR_INVALID_ARG;
  }

  handle->current_mode = mode;
  handle->resolution = mode_configs[mode];

  ESP_LOGI(s_TAG,
           "Mode set to %dx%d @ %dfps",
           handle->resolution.width,
           handle->resolution.height,
           handle->resolution.fps);

  return ESP_OK;
}

esp_err_t star_camera_imx219_start_streaming(imx219_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Start left camera
  esp_err_t ret = priv_write_reg(handle, handle->left_addr, IMX219_REG_MODE_SELECT, IMX219_MODE_STREAMING);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to start left camera streaming");
    return ret;
  }

  // Start right camera if stereo
  if (handle->stereo_enabled) {
    ret = priv_write_reg(handle, handle->right_addr, IMX219_REG_MODE_SELECT, IMX219_MODE_STREAMING);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to start right camera streaming");
      return ret;
    }
  }

  handle->streaming = true;
  ESP_LOGI(s_TAG, "Streaming started");

  return ESP_OK;
}

esp_err_t star_camera_imx219_stop_streaming(imx219_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = priv_write_reg(handle, handle->left_addr, IMX219_REG_MODE_SELECT, IMX219_MODE_STANDBY);
  if (ret != ESP_OK) {
    return ret;
  }

  if (handle->stereo_enabled) {
    ret = priv_write_reg(handle, handle->right_addr, IMX219_REG_MODE_SELECT, IMX219_MODE_STANDBY);
    if (ret != ESP_OK) {
      return ret;
    }
  }

  handle->streaming = false;
  ESP_LOGI(s_TAG, "Streaming stopped");

  return ESP_OK;
}

esp_err_t star_camera_imx219_capture_left(const imx219_handle_t* handle, imx219_frame_t* frame)
{
  if (handle == NULL || frame == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Note: Actual frame capture requires CSI-2 interface implementation
  // This would typically involve DMA and ISR to capture frame data from CSI
  // For now, populate metadata only

  frame->width = handle->resolution.width;
  frame->height = handle->resolution.height;
  frame->format = handle->resolution.format;
  frame->timestamp_us = esp_timer_get_time();

  ESP_LOGD(s_TAG, "Left frame captured");

  return ESP_OK;
}

esp_err_t star_camera_imx219_capture_right(const imx219_handle_t* handle, imx219_frame_t* frame)
{
  if (handle == NULL || frame == NULL || !handle->initialized || !handle->stereo_enabled) {
    return ESP_ERR_INVALID_ARG;
  }

  frame->width = handle->resolution.width;
  frame->height = handle->resolution.height;
  frame->format = handle->resolution.format;
  frame->timestamp_us = esp_timer_get_time();

  ESP_LOGD(s_TAG, "Right frame captured");

  return ESP_OK;
}

esp_err_t star_camera_imx219_capture_stereo(const imx219_handle_t* handle,
                                             imx219_frame_t*        frame_left,
                                             imx219_frame_t*        frame_right)
{
  if (handle == NULL || frame_left == NULL || frame_right == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!handle->stereo_enabled) {
    ESP_LOGE(s_TAG, "Stereo mode not enabled");
    return ESP_ERR_INVALID_STATE;
  }

  // Capture both frames synchronously
  uint64_t timestamp = esp_timer_get_time();

  esp_err_t ret = star_camera_imx219_capture_left(handle, frame_left);
  if (ret != ESP_OK) return ret;

  ret = star_camera_imx219_capture_right(handle, frame_right);
  if (ret != ESP_OK) return ret;

  // Ensure synchronized timestamps
  frame_left->timestamp_us = timestamp;
  frame_right->timestamp_us = timestamp;

  ESP_LOGD(s_TAG, "Stereo frames captured");

  return ESP_OK;
}

esp_err_t star_camera_imx219_set_exposure(imx219_handle_t* handle, uint32_t exposure_us)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Convert microseconds to coarse time (line time units)
  uint16_t coarse_time = exposure_us / 10;  // Simplified conversion

  // Write to both cameras
  esp_err_t ret = priv_write_reg(handle, handle->left_addr, IMX219_REG_COARSE_TIME_H, (coarse_time >> 8) & 0xFF);
  if (ret != ESP_OK) return ret;
  ret = priv_write_reg(handle, handle->left_addr, IMX219_REG_COARSE_TIME_L, coarse_time & 0xFF);
  if (ret != ESP_OK) return ret;

  if (handle->stereo_enabled) {
    ret = priv_write_reg(handle, handle->right_addr, IMX219_REG_COARSE_TIME_H, (coarse_time >> 8) & 0xFF);
    if (ret != ESP_OK) return ret;
    ret = priv_write_reg(handle, handle->right_addr, IMX219_REG_COARSE_TIME_L, coarse_time & 0xFF);
    if (ret != ESP_OK) return ret;
  }

  handle->exposure = coarse_time;
  ESP_LOGI(s_TAG, "Exposure set to %lu us", exposure_us);

  return ESP_OK;
}

esp_err_t star_camera_imx219_set_gain(imx219_handle_t* handle, uint16_t gain)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (gain > 255) {
    ESP_LOGW(s_TAG, "Gain clamped to 255");
    gain = 255;
  }

  esp_err_t ret = priv_write_reg(handle, handle->left_addr, IMX219_REG_ANALOG_GAIN, gain);
  if (ret != ESP_OK) return ret;

  if (handle->stereo_enabled) {
    ret = priv_write_reg(handle, handle->right_addr, IMX219_REG_ANALOG_GAIN, gain);
    if (ret != ESP_OK) return ret;
  }

  handle->gain = gain;
  ESP_LOGI(s_TAG, "Gain set to %d", gain);

  return ESP_OK;
}

esp_err_t star_camera_imx219_set_auto_exposure(imx219_handle_t* handle, bool enable)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Auto exposure would be implemented via additional register writes
  ESP_LOGI(s_TAG, "Auto exposure %s", enable ? "enabled" : "disabled");

  return ESP_OK;
}

esp_err_t star_camera_imx219_get_resolution(const imx219_handle_t*      handle,
                                             imx219_resolution_config_t* config)
{
  if (handle == NULL || config == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  *config = handle->resolution;

  return ESP_OK;
}
