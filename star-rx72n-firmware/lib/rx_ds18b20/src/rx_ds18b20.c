/* lib/rx_ds18b20/src/rx_ds18b20.c */

/**
 * @file rx_ds18b20.c
 * @brief DS18B20 digital temperature sensor driver implementation
 *
 * Implements the DS18B20 driver using the OneWire bus abstraction layer.
 * Supports single and multi-sensor configurations with configurable resolution.
 *
 * @date 2026-01-03
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_ds18b20.h"

#include <string.h>

#include "rx_bus_onewire.h"
#include "rx_check.h"
#include "rx_crc.h"
#include "rx_log.h"

static const char* s_tag = "DS18B20";

/* =============================================================================
 * Internal Constants
 * =============================================================================
 */

/**
 * @brief Temperature calculation constants
 */
typedef enum {
  k_temp_lsb_resolution    = 16, /**< LSB represents 1/16 degree C */
  k_temp_c_to_f_mult_num   = 9,  /**< C to F multiplier numerator */
  k_temp_c_to_f_mult_denom = 5,  /**< C to F multiplier denominator */
  k_temp_c_to_f_offset     = 32, /**< C to F offset */
} temp_calc_constants_t;

/**
 * @brief Scratchpad configuration bytes for write operation
 */
typedef enum {
  k_scratch_write_size = 3, /**< TH + TL + Config = 3 bytes */
} scratchpad_constants_t;

/**
 * @brief Default alarm values (unused by most applications)
 */
typedef enum {
  k_default_th = 0x00, /**< Default high alarm (0C) */
  k_default_tl = 0x00, /**< Default low alarm (0C) */
} alarm_defaults_t;

/* =============================================================================
 * Internal Helper Macros
 * =============================================================================
 */

/**
 * @brief Validate DS18B20 handle is initialized
 *
 * @param[in] handle Handle to validate
 * @param[in] tag Logging tag
 */
#define CHECK_DS18B20_HANDLE(handle, tag)                                                          \
  do {                                                                                             \
    RX_CHECK_NULL_PTR(handle, tag, "handle is NULL");                                              \
    if (!(handle)->initialized) {                                                                  \
      rx_log_error(tag, "Handle not initialized");                                                 \
      return k_rx_err_invalid_state;                                                               \
    }                                                                                              \
  } while (0)

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Convert resolution bits (9-12) to config register value
 *
 * @param[in] resolution_bits Resolution (9, 10, 11, or 12)
 * @return Config register value with resolution bits set
 */
static uint8_t internal_resolution_to_config(uint8_t resolution_bits)
{
  uint8_t config = k_ds18b20_config_reserved;

  switch (resolution_bits) {
    case k_ds18b20_resolution_9bit:
      config |= k_ds18b20_config_res_9bit;
      break;
    case k_ds18b20_resolution_10bit:
      config |= k_ds18b20_config_res_10bit;
      break;
    case k_ds18b20_resolution_11bit:
      config |= k_ds18b20_config_res_11bit;
      break;
    case k_ds18b20_resolution_12bit:
    default:
      config |= k_ds18b20_config_res_12bit;
      break;
  }

  return config;
}

/**
 * @brief Convert config register value to resolution bits
 *
 * @param[in] config Config register value
 * @return Resolution in bits (9-12)
 */
static uint8_t internal_config_to_resolution(uint8_t config)
{
  uint8_t res_bits = (config & k_ds18b20_config_res_mask) >> k_ds18b20_config_res_shift;

  switch (res_bits) {
    case 0:
      return k_ds18b20_resolution_9bit;
    case 1:
      return k_ds18b20_resolution_10bit;
    case 2:
      return k_ds18b20_resolution_11bit;
    case 3:
    default:
      return k_ds18b20_resolution_12bit;
  }
}

/**
 * @brief Get conversion time for a given resolution
 *
 * @param[in] resolution_bits Resolution (9-12)
 * @return Conversion time in milliseconds
 */
static uint32_t internal_get_conv_time_ms(uint8_t resolution_bits)
{
  switch (resolution_bits) {
    case k_ds18b20_resolution_9bit:
      return k_ds18b20_conv_time_9bit_ms;
    case k_ds18b20_resolution_10bit:
      return k_ds18b20_conv_time_10bit_ms;
    case k_ds18b20_resolution_11bit:
      return k_ds18b20_conv_time_11bit_ms;
    case k_ds18b20_resolution_12bit:
    default:
      return k_ds18b20_conv_time_12bit_ms;
  }
}

/**
 * @brief Address the sensor (Skip ROM or Match ROM based on handle config)
 *
 * @param[in] handle DS18B20 handle
 * @return k_rx_ok on success
 */
static rx_err_t internal_address_device(rx_ds18b20_handle_t* handle)
{
  if (handle->use_rom_match) {
    return rx_bus_onewire_match_rom(handle->bus_manager, handle->bus_name, handle->rom);
  } else {
    return rx_bus_onewire_skip_rom(handle->bus_manager, handle->bus_name);
  }
}

/**
 * @brief Read the scratchpad and validate CRC
 *
 * @param[in] handle DS18B20 handle
 * @param[out] scratchpad 9-byte buffer for scratchpad data
 * @return k_rx_ok on success
 */
static rx_err_t internal_read_scratchpad(rx_ds18b20_handle_t* handle, uint8_t* scratchpad)
{
  rx_err_t err = internal_address_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  err =
    rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_read_scratchpad);
  if (err != k_rx_ok) {
    return err;
  }

  err = rx_bus_onewire_read(handle->bus_manager,
                            handle->bus_name,
                            scratchpad,
                            k_ds18b20_scratchpad_bytes);
  if (err != k_rx_ok) {
    return err;
  }

  /* Validate CRC-8 (CRC of bytes 0-7 should equal byte 8) */
  uint8_t crc = rx_crc8_maxim(scratchpad, k_ds18b20_scratchpad_bytes - 1U);
  if (crc != scratchpad[k_ds18b20_scratch_crc]) {
    rx_log_error(s_tag, "Scratchpad CRC mismatch");
    return k_rx_err_crc_mismatch;
  }

  return k_rx_ok;
}

/**
 * @brief Write 3 bytes to scratchpad (TH, TL, Config)
 *
 * @param[in] handle DS18B20 handle
 * @param[in] th High alarm byte
 * @param[in] tl Low alarm byte
 * @param[in] config Configuration register
 * @return k_rx_ok on success
 */
static rx_err_t
internal_write_scratchpad(rx_ds18b20_handle_t* handle, uint8_t th, uint8_t tl, uint8_t config)
{
  rx_err_t err = internal_address_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  err = rx_bus_onewire_write_byte(handle->bus_manager,
                                  handle->bus_name,
                                  k_ds18b20_cmd_write_scratchpad);
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t data[k_scratch_write_size] = {th, tl, config};
  return rx_bus_onewire_write(handle->bus_manager, handle->bus_name, data, k_scratch_write_size);
}

/**
 * @brief Check power supply mode (parasitic vs external)
 *
 * @param[in] handle DS18B20 handle
 * @param[out] parasitic True if parasitic power
 * @return k_rx_ok on success
 */
static rx_err_t internal_check_power_mode(rx_ds18b20_handle_t* handle, bool* parasitic)
{
  rx_err_t err = internal_address_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  err = rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_read_power);
  if (err != k_rx_ok) {
    return err;
  }

  bool bit = false;
  err      = rx_bus_onewire_read_bit(handle->bus_manager, handle->bus_name, &bit);
  if (err != k_rx_ok) {
    return err;
  }

  /* If device pulls line low (bit = 0), it uses parasitic power */
  *parasitic = !bit;
  return k_rx_ok;
}

/**
 * @brief Convert raw temperature to Celsius
 *
 * @param[in] raw_temp Raw 16-bit temperature value
 * @return Temperature in Celsius
 */
static float internal_raw_to_celsius(int16_t raw_temp)
{
  return (float)raw_temp / (float)k_temp_lsb_resolution;
}

/**
 * @brief Convert Celsius to Fahrenheit
 *
 * @param[in] celsius Temperature in Celsius
 * @return Temperature in Fahrenheit
 */
static float internal_celsius_to_fahrenheit(float celsius)
{
  return (celsius * (float)k_temp_c_to_f_mult_num / (float)k_temp_c_to_f_mult_denom) +
         (float)k_temp_c_to_f_offset;
}

/**
 * @brief Validate temperature is within DS18B20 range
 *
 * @param[in] temp_c Temperature in Celsius
 * @return true if valid, false if out of range
 */
static bool internal_validate_temperature(float temp_c)
{
  return (temp_c >= (float)k_ds18b20_temp_min_c) && (temp_c <= (float)k_ds18b20_temp_max_c);
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

rx_err_t rx_ds18b20_init(rx_ds18b20_handle_t* handle,
                         rx_bus_manager_t*    bus_manager,
                         const char*          bus_name,
                         const uint8_t*       rom)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is NULL");
  RX_CHECK_NULL_PTR(bus_manager, s_tag, "bus_manager is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name is NULL");

  /* Initialize handle structure */
  memset(handle, 0, sizeof(rx_ds18b20_handle_t));
  handle->bus_manager     = bus_manager;
  handle->bus_name        = bus_name;
  handle->resolution_bits = k_ds18b20_resolution_12bit; /* Default 12-bit */
  handle->use_rom_match   = (rom != NULL);

  if (rom != NULL) {
    memcpy(handle->rom, rom, k_ds18b20_rom_bytes);

    /* Verify family code is DS18B20 */
    if (handle->rom[0] != k_ds18b20_family_code) {
      rx_log_error(s_tag, "Invalid family code - not a DS18B20");
      return k_rx_err_invalid_arg;
    }
  }

  /* Initialize OneWire bus if needed */
  rx_err_t err = rx_bus_onewire_init(bus_manager, bus_name);
  if (err != k_rx_ok) {
    return err;
  }

  /* Check device presence */
  bool presence = false;
  err           = rx_bus_onewire_reset(bus_manager, bus_name, &presence);
  if (err != k_rx_ok) {
    return err;
  }

  if (!presence) {
    rx_log_error(s_tag, "No device present on bus");
    return k_rx_err_not_found;
  }

  /* Read scratchpad to get current resolution */
  uint8_t scratchpad[k_ds18b20_scratchpad_bytes];
  err = internal_read_scratchpad(handle, scratchpad);
  if (err != k_rx_ok) {
    return err;
  }

  handle->resolution_bits = internal_config_to_resolution(scratchpad[k_ds18b20_scratch_config]);

  /* Check power supply mode */
  err = internal_check_power_mode(handle, &handle->parasitic_power);
  if (err != k_rx_ok) {
    /* Non-fatal - assume external power */
    handle->parasitic_power = false;
  }

  handle->initialized = true;

  rx_log_info(s_tag, "DS18B20 initialized");

  return k_rx_ok;
}

rx_err_t rx_ds18b20_init_by_index(rx_ds18b20_handle_t* handle,
                                  rx_bus_manager_t*    bus_manager,
                                  const char*          bus_name,
                                  const uint8_t*       roms,
                                  uint32_t             device_index,
                                  uint32_t             num_devices)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is NULL");
  RX_CHECK_NULL_PTR(bus_manager, s_tag, "bus_manager is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name is NULL");
  RX_CHECK_NULL_PTR(roms, s_tag, "roms is NULL");

  if (device_index >= num_devices) {
    rx_log_error(s_tag, "Device index out of range");
    return k_rx_err_invalid_arg;
  }

  const uint8_t* rom = &roms[device_index * k_ds18b20_rom_bytes];
  return rx_ds18b20_init(handle, bus_manager, bus_name, rom);
}

/* =============================================================================
 * Temperature Conversion Functions
 * =============================================================================
 */

rx_err_t rx_ds18b20_start_conversion(rx_ds18b20_handle_t* handle)
{
  CHECK_DS18B20_HANDLE(handle, s_tag);

  rx_err_t err = internal_address_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  return rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_convert_t);
}

rx_err_t rx_ds18b20_read_temperature(rx_ds18b20_handle_t* handle, float* temperature_c)
{
  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(temperature_c, s_tag, "temperature_c is NULL");

  int16_t  raw_temp = 0;
  rx_err_t err      = rx_ds18b20_read_temperature_raw(handle, &raw_temp);
  if (err != k_rx_ok) {
    return err;
  }

  *temperature_c = internal_raw_to_celsius(raw_temp);

  if (!internal_validate_temperature(*temperature_c)) {
    rx_log_error(s_tag, "Temperature out of valid range");
    return k_rx_err_range_check_failed;
  }

  return k_rx_ok;
}

rx_err_t rx_ds18b20_read_temperature_f(rx_ds18b20_handle_t* handle, float* temperature_f)
{
  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(temperature_f, s_tag, "temperature_f is NULL");

  float    temp_c = 0.0f;
  rx_err_t err    = rx_ds18b20_read_temperature(handle, &temp_c);
  if (err != k_rx_ok) {
    return err;
  }

  *temperature_f = internal_celsius_to_fahrenheit(temp_c);
  return k_rx_ok;
}

rx_err_t rx_ds18b20_read_temperature_raw(rx_ds18b20_handle_t* handle, int16_t* raw_temp)
{
  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(raw_temp, s_tag, "raw_temp is NULL");

  uint8_t  scratchpad[k_ds18b20_scratchpad_bytes];
  rx_err_t err = internal_read_scratchpad(handle, scratchpad);
  if (err != k_rx_ok) {
    return err;
  }

  /* Temperature is 16-bit signed, LSB first */
  *raw_temp = (int16_t)((uint16_t)scratchpad[k_ds18b20_scratch_temp_msb] << 8U) |
              (uint16_t)scratchpad[k_ds18b20_scratch_temp_lsb];

  return k_rx_ok;
}

/* =============================================================================
 * Resolution Configuration
 * =============================================================================
 */

rx_err_t rx_ds18b20_set_resolution(rx_ds18b20_handle_t* handle, uint8_t resolution_bits)
{
  CHECK_DS18B20_HANDLE(handle, s_tag);

  if (resolution_bits < k_ds18b20_resolution_9bit || resolution_bits > k_ds18b20_resolution_12bit) {
    rx_log_error(s_tag, "Invalid resolution (must be 9-12)");
    return k_rx_err_invalid_arg;
  }

  /* Read current scratchpad to preserve TH/TL */
  uint8_t  scratchpad[k_ds18b20_scratchpad_bytes];
  rx_err_t err = internal_read_scratchpad(handle, scratchpad);
  if (err != k_rx_ok) {
    return err;
  }

  uint8_t new_config = internal_resolution_to_config(resolution_bits);

  /* Write new config with existing TH/TL */
  err = internal_write_scratchpad(handle,
                                  scratchpad[k_ds18b20_scratch_th_reg],
                                  scratchpad[k_ds18b20_scratch_tl_reg],
                                  new_config);
  if (err != k_rx_ok) {
    return err;
  }

  handle->resolution_bits = resolution_bits;
  return k_rx_ok;
}

rx_err_t rx_ds18b20_get_resolution(const rx_ds18b20_handle_t* handle, uint8_t* resolution_bits)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is NULL");
  RX_CHECK_NULL_PTR(resolution_bits, s_tag, "resolution_bits is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Handle not initialized");
    return k_rx_err_invalid_state;
  }

  *resolution_bits = handle->resolution_bits;
  return k_rx_ok;
}

rx_err_t rx_ds18b20_get_conversion_time(const rx_ds18b20_handle_t* handle, uint32_t* time_ms)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is NULL");
  RX_CHECK_NULL_PTR(time_ms, s_tag, "time_ms is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Handle not initialized");
    return k_rx_err_invalid_state;
  }

  *time_ms = internal_get_conv_time_ms(handle->resolution_bits);
  return k_rx_ok;
}

/* =============================================================================
 * Multi-Sensor Support
 * =============================================================================
 */

rx_err_t rx_ds18b20_search_devices(rx_bus_manager_t* bus_manager,
                                   const char*       bus_name,
                                   uint8_t*          roms,
                                   uint32_t          max_devices,
                                   uint32_t*         num_devices)
{
  RX_CHECK_NULL_PTR(bus_manager, s_tag, "bus_manager is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name is NULL");
  RX_CHECK_NULL_PTR(roms, s_tag, "roms is NULL");
  RX_CHECK_NULL_PTR(num_devices, s_tag, "num_devices is NULL");

  *num_devices = 0;

  if (max_devices == 0) {
    return k_rx_ok;
  }

  /* Initialize bus if needed */
  rx_err_t err = rx_bus_onewire_init(bus_manager, bus_name);
  if (err != k_rx_ok) {
    return err;
  }

  /* Use temporary buffer for all devices (DS18B20 and others) */
  uint8_t  all_roms[k_ds18b20_max_devices * k_ds18b20_rom_bytes];
  uint32_t total_devices = 0;

  uint32_t search_max = (max_devices > k_ds18b20_max_devices) ? k_ds18b20_max_devices : max_devices;

  err = rx_bus_onewire_search(bus_manager, bus_name, all_roms, search_max, &total_devices);
  if (err != k_rx_ok) {
    return err;
  }

  /* Filter for DS18B20 family code */
  uint32_t ds18b20_count = 0;
  for (uint32_t i = 0; i < total_devices && ds18b20_count < max_devices; ++i) {
    uint8_t* rom = &all_roms[i * k_ds18b20_rom_bytes];

    if (rom[0] == k_ds18b20_family_code) {
      memcpy(&roms[ds18b20_count * k_ds18b20_rom_bytes], rom, k_ds18b20_rom_bytes);
      ds18b20_count++;
    }
  }

  *num_devices = ds18b20_count;
  return k_rx_ok;
}

/* =============================================================================
 * Advanced Functions
 * =============================================================================
 */

rx_err_t rx_ds18b20_read_rom(rx_bus_manager_t* bus_manager, const char* bus_name, uint8_t* rom)
{
  RX_CHECK_NULL_PTR(bus_manager, s_tag, "bus_manager is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name is NULL");
  RX_CHECK_NULL_PTR(rom, s_tag, "rom is NULL");

  rx_err_t err = rx_bus_onewire_init(bus_manager, bus_name);
  if (err != k_rx_ok) {
    return err;
  }

  return rx_bus_onewire_read_rom(bus_manager, bus_name, rom);
}

rx_err_t rx_ds18b20_is_parasitic_power(const rx_ds18b20_handle_t* handle, bool* parasitic)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is NULL");
  RX_CHECK_NULL_PTR(parasitic, s_tag, "parasitic is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Handle not initialized");
    return k_rx_err_invalid_state;
  }

  *parasitic = handle->parasitic_power;
  return k_rx_ok;
}

rx_err_t rx_ds18b20_save_config(rx_ds18b20_handle_t* handle)
{
  CHECK_DS18B20_HANDLE(handle, s_tag);

  rx_err_t err = internal_address_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  return rx_bus_onewire_write_byte(handle->bus_manager,
                                   handle->bus_name,
                                   k_ds18b20_cmd_copy_scratchpad);
}

rx_err_t rx_ds18b20_recall_config(rx_ds18b20_handle_t* handle)
{
  CHECK_DS18B20_HANDLE(handle, s_tag);

  rx_err_t err = internal_address_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  err = rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_recall_e2);
  if (err != k_rx_ok) {
    return err;
  }

  /* Re-read the configuration from scratchpad */
  uint8_t scratchpad[k_ds18b20_scratchpad_bytes];
  err = internal_read_scratchpad(handle, scratchpad);
  if (err != k_rx_ok) {
    return err;
  }

  handle->resolution_bits = internal_config_to_resolution(scratchpad[k_ds18b20_scratch_config]);
  return k_rx_ok;
}
