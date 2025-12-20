/**
 * @file star_sensor_ds18b20.c
 * @brief DS18B20 temperature sensor driver implementation over 1-Wire
 * @details
 * Implements driver for Dallas/Maxim DS18B20 digital temperature sensor using 1-Wire protocol
 * via the bus manager. Supports configurable resolution (9-12 bit), temperature conversion, and
 * both single-device and multi-device configurations with ROM addressing.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "star_sensor_ds18b20.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "star_bus_onewire.h"

/* --- Constants --- */

static const char* s_tag = "STAR_DS18B20";

/**
 * @brief DS18B20 1-Wire command codes
 *
 * These are the standard DS18B20 ROM and function commands used for
 * communication over the 1-Wire bus (controller-to-peripheral protocol).
 */
typedef enum {
  k_ds18b20_cmd_skip_rom         = 0xCC, /**< Skip ROM - address all peripherals */
  k_ds18b20_cmd_match_rom        = 0x55, /**< Match ROM - address specific peripheral */
  k_ds18b20_cmd_read_rom         = 0x33, /**< Read ROM - get peripheral's 64-bit ID */
  k_ds18b20_cmd_convert_t        = 0x44, /**< Convert T - start temperature conversion */
  k_ds18b20_cmd_read_scratchpad  = 0xBE, /**< Read Scratchpad - read 9 bytes of data */
  k_ds18b20_cmd_write_scratchpad = 0x4E, /**< Write Scratchpad - write config data */
} ds18b20_cmd_t;

/**
 * @brief DS18B20 data sizes and constants
 */
typedef enum {
  k_ds18b20_scratchpad_size  = 9,    /**< Scratchpad size in bytes (data + CRC) */
  k_ds18b20_rom_size         = 8,    /**< ROM code size in bytes */
  k_ds18b20_scratchpad_bytes = 3,    /**< Bytes written to scratchpad (TH, TL, Config) */
  k_ds18b20_bits_per_byte    = 8,    /**< Bits per byte for CRC calculation */
} ds18b20_size_t;

/**
 * @brief DS18B20 scratchpad register configuration constants
 */
typedef enum {
  k_ds18b20_config_th_default      = 0x00, /**< TH register default (high alarm, unused) */
  k_ds18b20_config_tl_default      = 0x00, /**< TL register default (low alarm, unused) */
  k_ds18b20_config_resolution_mask = 0x1F, /**< Config register base mask */
  k_ds18b20_config_resolution_shift = 5,   /**< Resolution bits shift position */
} ds18b20_config_t;

/**
 * @brief DS18B20 CRC-8 calculation constants
 *
 * The DS18B20 uses a polynomial of x^8 + x^5 + x^4 + 1 (0x8C reflected).
 */
typedef enum {
  k_ds18b20_crc_polynomial = 0x8C, /**< CRC-8 polynomial (reflected form) */
  k_ds18b20_crc_lsb_mask   = 0x01, /**< Mask for LSB extraction */
} ds18b20_crc_t;

/* Conversion times (ms) for each resolution */
static const uint16_t s_conversion_times_ms[] = {
  94,  /* 9-bit: 93.75ms */
  188, /* 10-bit: 187.5ms */
  375, /* 11-bit: 375ms */
  750, /* 12-bit: 750ms */
};

/* --- Private Function Prototypes --- */

static esp_err_t internal_ds18b20_start_conversion(star_ds18b20_handle_t* handle);
static esp_err_t internal_ds18b20_read_scratchpad(star_ds18b20_handle_t* handle,
                                                  uint8_t*               scratchpad);
static esp_err_t internal_ds18b20_write_scratchpad(star_ds18b20_handle_t* handle);
static uint8_t   internal_ds18b20_crc8(const uint8_t* data, size_t len);
static esp_err_t internal_ds18b20_send_rom_command(star_ds18b20_handle_t* handle);

/* --- Public Functions --- */

esp_err_t star_sensor_ds18b20_init(star_ds18b20_handle_t*       handle,
                                   const star_ds18b20_config_t* config)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_tag, "Config is NULL");
  ESP_RETURN_ON_FALSE(config->bus_manager, ESP_ERR_INVALID_ARG, s_tag, "Bus manager is NULL");
  ESP_RETURN_ON_FALSE(config->bus_name, ESP_ERR_INVALID_ARG, s_tag, "Bus name is NULL");
  ESP_RETURN_ON_FALSE(!handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Already initialized");

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
      ESP_LOGE(s_tag, "Failed to read ROM code: %s", esp_err_to_name(ret));
      return ret;
    }
    ESP_LOGI(s_tag, "DS18B20 ROM code: 0x%016llX", handle->rom_code);
  }

  /* Configure resolution */
  esp_err_t ret = internal_ds18b20_write_scratchpad(handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to write scratchpad: %s", esp_err_to_name(ret));
    return ret;
  }

  handle->initialized = true;

  ESP_LOGI(s_tag,
           "DS18B20 initialized: bus=%s, resolution=%d-bit, rom=%s",
           config->bus_name,
           9 + config->resolution,
           config->use_rom ? "yes" : "skip");

  return ESP_OK;
}

esp_err_t star_sensor_ds18b20_deinit(star_ds18b20_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Clear handle */
  memset(handle, 0, sizeof(star_ds18b20_handle_t));

  ESP_LOGI(s_tag, "DS18B20 deinitialized");
  return ESP_OK;
}

esp_err_t star_sensor_ds18b20_read_temp(star_ds18b20_handle_t* handle, float* out_temperature)
{
  ESP_RETURN_ON_FALSE(handle && out_temperature,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Handle or out_temperature is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Start temperature conversion */
  esp_err_t ret = internal_ds18b20_start_conversion(handle);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to start conversion");

  /* Wait for conversion to complete */
  vTaskDelay(pdMS_TO_TICKS(s_conversion_times_ms[handle->resolution]));

  /* Read scratchpad */
  uint8_t scratchpad[k_ds18b20_scratchpad_size];
  ret = internal_ds18b20_read_scratchpad(handle, scratchpad);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to read scratchpad");

  /* Verify CRC (CRC is calculated over first 8 bytes, stored in byte 9) */
  uint8_t crc = internal_ds18b20_crc8(scratchpad, k_ds18b20_scratchpad_size - 1);
  if (crc != scratchpad[k_ds18b20_scratchpad_size - 1]) {
    ESP_LOGE(s_tag,
             "CRC mismatch: calculated=0x%02X, received=0x%02X",
             crc,
             scratchpad[k_ds18b20_scratchpad_size - 1]);
    return ESP_ERR_INVALID_CRC;
  }

  /* Extract temperature (16-bit value in scratchpad[0:1]) */
  int16_t raw_temp = (scratchpad[1] << 8) | scratchpad[0];

  /* Convert to Celsius based on resolution */
  *out_temperature = (float)raw_temp / 16.0f;

  ESP_LOGD(s_tag, "Temperature: %.2f °C (raw=0x%04X)", *out_temperature, raw_temp);
  return ESP_OK;
}

esp_err_t star_sensor_ds18b20_set_resolution(star_ds18b20_handle_t*    handle,
                                             star_ds18b20_resolution_t resolution)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");
  ESP_RETURN_ON_FALSE(resolution <= k_star_ds18b20_resolution_12_bit,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Invalid resolution");

  handle->resolution = resolution;

  /* Write new resolution to device */
  esp_err_t ret = internal_ds18b20_write_scratchpad(handle);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to write scratchpad");

  ESP_LOGI(s_tag, "Resolution updated to %d-bit", 9 + resolution);
  return ESP_OK;
}

esp_err_t star_sensor_ds18b20_read_rom(star_ds18b20_handle_t* handle, uint64_t* out_rom)
{
  ESP_RETURN_ON_FALSE(handle && out_rom, ESP_ERR_INVALID_ARG, s_tag, "Handle or out_rom is NULL");

  /* Send Read ROM command */
  uint8_t   rom_bytes[k_ds18b20_rom_size];
  esp_err_t ret = star_bus_onewire_read_rom(handle->bus_manager, handle->bus_name, rom_bytes);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to read ROM");

  /* Convert byte array to 64-bit value */
  *out_rom = 0;
  for (int32_t i = 0; i < k_ds18b20_rom_size; i++) {
    *out_rom |= ((uint64_t)rom_bytes[i]) << (i * k_ds18b20_bits_per_byte);
  }

  return ESP_OK;
}

/* --- Private Functions --- */

static esp_err_t internal_ds18b20_send_rom_command(star_ds18b20_handle_t* handle)
{
  if (handle->use_rom) {
    /* Match ROM - send ROM code to address specific peripheral */
    uint8_t rom_bytes[k_ds18b20_rom_size];
    for (int32_t i = 0; i < k_ds18b20_rom_size; i++) {
      rom_bytes[i] = (uint8_t)(handle->rom_code >> (i * k_ds18b20_bits_per_byte));
    }
    return star_bus_onewire_match_rom(handle->bus_manager, handle->bus_name, rom_bytes);
  } else {
    /* Skip ROM - address all peripherals on bus */
    return star_bus_onewire_skip_rom(handle->bus_manager, handle->bus_name);
  }
}

static esp_err_t internal_ds18b20_start_conversion(star_ds18b20_handle_t* handle)
{
  /* Reset bus */
  bool      presence;
  esp_err_t ret = star_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to reset bus");
  ESP_RETURN_ON_FALSE(presence, ESP_ERR_NOT_FOUND, s_tag, "No device present on bus");

  /* Send ROM command */
  ret = internal_ds18b20_send_rom_command(handle);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to send ROM command");

  /* Send Convert T command */
  uint8_t cmd = k_ds18b20_cmd_convert_t;
  ret         = star_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, cmd);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to send Convert T command");

  return ESP_OK;
}

static esp_err_t internal_ds18b20_read_scratchpad(star_ds18b20_handle_t* handle,
                                                  uint8_t*               scratchpad)
{
  /* Reset bus */
  bool      presence;
  esp_err_t ret = star_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to reset bus");
  ESP_RETURN_ON_FALSE(presence, ESP_ERR_NOT_FOUND, s_tag, "No device present on bus");

  /* Send ROM command */
  ret = internal_ds18b20_send_rom_command(handle);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to send ROM command");

  /* Send Read Scratchpad command */
  uint8_t cmd = k_ds18b20_cmd_read_scratchpad;
  ret         = star_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, cmd);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to send Read Scratchpad command");

  /* Read 9 bytes (scratchpad + CRC) */
  ret = star_bus_onewire_read_bytes(handle->bus_manager,
                                    handle->bus_name,
                                    scratchpad,
                                    k_ds18b20_scratchpad_size);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to read scratchpad");

  return ESP_OK;
}

static esp_err_t internal_ds18b20_write_scratchpad(star_ds18b20_handle_t* handle)
{
  /* Reset bus */
  bool      presence;
  esp_err_t ret = star_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to reset bus");
  ESP_RETURN_ON_FALSE(presence, ESP_ERR_NOT_FOUND, s_tag, "No device present on bus");

  /* Send ROM command */
  ret = internal_ds18b20_send_rom_command(handle);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to send ROM command");

  /* Send Write Scratchpad command */
  ret = star_bus_onewire_write_byte(handle->bus_manager,
                                    handle->bus_name,
                                    k_ds18b20_cmd_write_scratchpad);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to send Write Scratchpad command");

  /* Write 3 data bytes: [TH, TL, CONFIG] */
  uint8_t data[k_ds18b20_scratchpad_bytes];
  data[0] = k_ds18b20_config_th_default; /* TH (high alarm, not used) */
  data[1] = k_ds18b20_config_tl_default; /* TL (low alarm, not used) */
  data[2] = (uint8_t)((handle->resolution << k_ds18b20_config_resolution_shift) |
                      k_ds18b20_config_resolution_mask); /* Configuration register */

  for (int32_t i = 0; i < k_ds18b20_scratchpad_bytes; i++) {
    ret = star_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, data[i]);
    ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to write scratchpad byte");
  }

  return ESP_OK;
}

static uint8_t internal_ds18b20_crc8(const uint8_t* data, size_t len)
{
  uint8_t crc = 0;

  for (size_t i = 0; i < len; i++) {
    uint8_t byte = data[i];
    for (int32_t j = 0; j < k_ds18b20_bits_per_byte; j++) {
      uint8_t mix = (crc ^ byte) & k_ds18b20_crc_lsb_mask;
      crc >>= 1;
      if (mix) {
        crc ^= k_ds18b20_crc_polynomial;
      }
      byte >>= 1;
    }
  }

  return crc;
}
