/* lib/star_sensor_ds18b20/src/star_sensor_ds18b20.c */

#include "star_sensor_ds18b20.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#include "star_bus_onewire.h"

/* --- Constants --- */

static const char* s_TAG = "STAR_DS18B20";

/* DS18B20 Commands */
#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_MATCH_ROM       0x55
#define DS18B20_CMD_READ_ROM        0x33
#define DS18B20_CMD_CONVERT_T       0x44
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE
#define DS18B20_CMD_WRITE_SCRATCHPAD 0x4E

/* Scratchpad size */
#define DS18B20_SCRATCHPAD_SIZE 9

/* Conversion times (ms) for each resolution */
static const uint16_t s_conversion_times_ms[] = {
  94,   /* 9-bit: 93.75ms */
  188,  /* 10-bit: 187.5ms */
  375,  /* 11-bit: 375ms */
  750,  /* 12-bit: 750ms */
};

/* --- Private Function Prototypes --- */

static esp_err_t internal_ds18b20_start_conversion(star_ds18b20_handle_t* handle);
static esp_err_t internal_ds18b20_read_scratchpad(star_ds18b20_handle_t* handle, uint8_t* scratchpad);
static esp_err_t internal_ds18b20_write_scratchpad(star_ds18b20_handle_t* handle);
static uint8_t   internal_ds18b20_crc8(const uint8_t* data, size_t len);
static esp_err_t internal_ds18b20_send_rom_command(star_ds18b20_handle_t* handle);

/* --- Public Functions --- */

esp_err_t star_sensor_ds18b20_init(star_ds18b20_handle_t*       handle,
                                    const star_ds18b20_config_t* config)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config is NULL");
  ESP_RETURN_ON_FALSE(config->bus_manager, ESP_ERR_INVALID_ARG, s_TAG, "Bus manager is NULL");
  ESP_RETURN_ON_FALSE(config->bus_name, ESP_ERR_INVALID_ARG, s_TAG, "Bus name is NULL");
  ESP_RETURN_ON_FALSE(!handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Already initialized");

  /* Zero out handle */
  memset(handle, 0, sizeof(star_ds18b20_handle_t));

  /* Store configuration */
  handle->bus_manager = config->bus_manager;
  handle->bus_name    = config->bus_name;
  handle->resolution  = config->resolution;
  handle->use_rom     = config->use_rom;
  handle->rom_code    = config->rom_code;

  /* If ROM code not provided and use_rom is true, try to read it */
  if (handle->use_rom && handle->rom_code == 0) {
    esp_err_t ret = star_sensor_ds18b20_read_rom(handle, &handle->rom_code);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to read ROM code: %s", esp_err_to_name(ret));
      return ret;
    }
    ESP_LOGI(s_TAG, "DS18B20 ROM code: 0x%016llX", handle->rom_code);
  }

  /* Configure resolution */
  esp_err_t ret = internal_ds18b20_write_scratchpad(handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to write scratchpad: %s", esp_err_to_name(ret));
    return ret;
  }

  handle->initialized = true;

  ESP_LOGI(s_TAG,
           "DS18B20 initialized: bus=%s, resolution=%d-bit, rom=%s",
           config->bus_name,
           9 + config->resolution,
           config->use_rom ? "yes" : "skip");

  return ESP_OK;
}

esp_err_t star_sensor_ds18b20_deinit(star_ds18b20_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Clear handle */
  memset(handle, 0, sizeof(star_ds18b20_handle_t));

  ESP_LOGI(s_TAG, "DS18B20 deinitialized");
  return ESP_OK;
}

esp_err_t star_sensor_ds18b20_read_temp(star_ds18b20_handle_t* handle, float* out_temperature)
{
  ESP_RETURN_ON_FALSE(handle && out_temperature,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Handle or out_temperature is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Start temperature conversion */
  esp_err_t ret = internal_ds18b20_start_conversion(handle);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to start conversion");

  /* Wait for conversion to complete */
  vTaskDelay(pdMS_TO_TICKS(s_conversion_times_ms[handle->resolution]));

  /* Read scratchpad */
  uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE];
  ret = internal_ds18b20_read_scratchpad(handle, scratchpad);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to read scratchpad");

  /* Verify CRC */
  uint8_t crc = internal_ds18b20_crc8(scratchpad, 8);
  if (crc != scratchpad[8]) {
    ESP_LOGE(s_TAG, "CRC mismatch: calculated=0x%02X, received=0x%02X", crc, scratchpad[8]);
    return ESP_ERR_INVALID_CRC;
  }

  /* Extract temperature (16-bit value in scratchpad[0:1]) */
  int16_t raw_temp = (scratchpad[1] << 8) | scratchpad[0];

  /* Convert to Celsius based on resolution */
  *out_temperature = (float)raw_temp / 16.0f;

  ESP_LOGD(s_TAG, "Temperature: %.2f °C (raw=0x%04X)", *out_temperature, raw_temp);
  return ESP_OK;
}

esp_err_t star_sensor_ds18b20_set_resolution(star_ds18b20_handle_t*    handle,
                                              star_ds18b20_resolution_t resolution)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");
  ESP_RETURN_ON_FALSE(resolution <= k_star_ds18b20_resolution_12_bit,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Invalid resolution");

  handle->resolution = resolution;

  /* Write new resolution to device */
  esp_err_t ret = internal_ds18b20_write_scratchpad(handle);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to write scratchpad");

  ESP_LOGI(s_TAG, "Resolution updated to %d-bit", 9 + resolution);
  return ESP_OK;
}

esp_err_t star_sensor_ds18b20_read_rom(star_ds18b20_handle_t* handle, uint64_t* out_rom)
{
  ESP_RETURN_ON_FALSE(handle && out_rom, ESP_ERR_INVALID_ARG, s_TAG, "Handle or out_rom is NULL");

  /* Send Read ROM command */
  uint8_t rom_bytes[8];
  esp_err_t ret = star_bus_onewire_read_rom(handle->bus_manager, handle->bus_name, rom_bytes);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to read ROM");

  /* Convert byte array to 64-bit value */
  *out_rom = 0;
  for (int i = 0; i < 8; i++) {
    *out_rom |= ((uint64_t)rom_bytes[i]) << (i * 8);
  }

  return ESP_OK;
}

/* --- Private Functions --- */

static esp_err_t internal_ds18b20_send_rom_command(star_ds18b20_handle_t* handle)
{
  if (handle->use_rom) {
    /* Match ROM - send ROM code */
    uint8_t rom_bytes[8];
    for (int i = 0; i < 8; i++) {
      rom_bytes[i] = (handle->rom_code >> (i * 8)) & 0xFF;
    }
    return star_bus_onewire_match_rom(handle->bus_manager, handle->bus_name, rom_bytes);
  } else {
    /* Skip ROM - address all devices */
    return star_bus_onewire_skip_rom(handle->bus_manager, handle->bus_name);
  }
}

static esp_err_t internal_ds18b20_start_conversion(star_ds18b20_handle_t* handle)
{
  /* Reset bus */
  bool presence;
  esp_err_t ret = star_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to reset bus");
  ESP_RETURN_ON_FALSE(presence, ESP_ERR_NOT_FOUND, s_TAG, "No device present on bus");

  /* Send ROM command */
  ret = internal_ds18b20_send_rom_command(handle);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to send ROM command");

  /* Send Convert T command */
  uint8_t cmd = DS18B20_CMD_CONVERT_T;
  ret = star_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, cmd);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to send Convert T command");

  return ESP_OK;
}

static esp_err_t internal_ds18b20_read_scratchpad(star_ds18b20_handle_t* handle, uint8_t* scratchpad)
{
  /* Reset bus */
  bool presence;
  esp_err_t ret = star_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to reset bus");
  ESP_RETURN_ON_FALSE(presence, ESP_ERR_NOT_FOUND, s_TAG, "No device present on bus");

  /* Send ROM command */
  ret = internal_ds18b20_send_rom_command(handle);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to send ROM command");

  /* Send Read Scratchpad command */
  uint8_t cmd = DS18B20_CMD_READ_SCRATCHPAD;
  ret = star_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, cmd);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to send Read Scratchpad command");

  /* Read 9 bytes (scratchpad + CRC) */
  ret = star_bus_onewire_read_bytes(handle->bus_manager, handle->bus_name, scratchpad, DS18B20_SCRATCHPAD_SIZE);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to read scratchpad");

  return ESP_OK;
}

static esp_err_t internal_ds18b20_write_scratchpad(star_ds18b20_handle_t* handle)
{
  /* Reset bus */
  bool presence;
  esp_err_t ret = star_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to reset bus");
  ESP_RETURN_ON_FALSE(presence, ESP_ERR_NOT_FOUND, s_TAG, "No device present on bus");

  /* Send ROM command */
  ret = internal_ds18b20_send_rom_command(handle);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to send ROM command");

  /* Send Write Scratchpad command */
  ret = star_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, DS18B20_CMD_WRITE_SCRATCHPAD);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to send Write Scratchpad command");

  /* Write 3 data bytes: [TH, TL, CONFIG] */
  uint8_t data[3];
  data[0] = 0x00;  /* TH (high alarm, not used) */
  data[1] = 0x00;  /* TL (low alarm, not used) */
  data[2] = (handle->resolution << 5) | 0x1F;  /* Configuration register */

  for (int i = 0; i < 3; i++) {
    ret = star_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, data[i]);
    ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to write scratchpad byte");
  }

  return ESP_OK;
}

static uint8_t internal_ds18b20_crc8(const uint8_t* data, size_t len)
{
  uint8_t crc = 0;

  for (size_t i = 0; i < len; i++) {
    uint8_t byte = data[i];
    for (int j = 0; j < 8; j++) {
      uint8_t mix = (crc ^ byte) & 0x01;
      crc >>= 1;
      if (mix) {
        crc ^= 0x8C;
      }
      byte >>= 1;
    }
  }

  return crc;
}
