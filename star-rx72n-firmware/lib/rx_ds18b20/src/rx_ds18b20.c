/* lib/rx_ds18b20/src/rx_ds18b20.c */

/**
 * @file rx_ds18b20.c
 * @brief DS18B20 1-Wire Digital Temperature Sensor Driver Implementation
 * @details
 * Implementation of DS18B20 driver using dependency injection pattern.
 * Uses bus manager for hardware abstraction, enabling unit testing with mocks.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_ds18b20.h"

#include <math.h>
#include <string.h>

#include "rx_bus_onewire.h"
#include "rx_check.h"
#include "rx_crc.h"
#include "rx_log.h"

static const char*   s_tag                 = "DS18B20";
static const uint8_t s_ds18b20_family_code = 0x28U; /**< DS18B20 family code in ROM */

/* Temperature conversion constants (floating-point) */
static const float s_temp_conversion_divisor =
  16.0f; /**< Temperature raw-to-Celsius divisor (1/16°C units) */

/* =============================================================================
 * Internal Validation Macros
 * =============================================================================
 */

/**
 * @brief Check DS18B20 handle validity
 *
 * Validates handle pointer and initialization state.
 */
#define CHECK_DS18B20_HANDLE(handle, tag)                                                          \
  do {                                                                                             \
    RX_CHECK_NULL_PTR(handle, tag, "handle is NULL");                                              \
    if (!(handle)->initialized) {                                                                  \
      rx_log_error(tag, "DS18B20 not initialized");                                                \
      return k_rx_err_invalid_state;                                                               \
    }                                                                                              \
  } while (0)

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Scratchpad write parameters for TH, TL, and config registers
 */
typedef struct {
  uint8_t th;     /**< High alarm register */
  uint8_t tl;     /**< Low alarm register */
  uint8_t config; /**< Configuration register */
} ds18b20_scratchpad_write_t;

static rx_err_t internal_ds18b20_validate_config(const rx_ds18b20_config_t* config,
                                                 const rx_ds18b20_handle_t* handle);
static rx_err_t internal_ds18b20_verify_device_presence(rx_ds18b20_handle_t* handle);
static rx_err_t internal_ds18b20_select_device(const rx_ds18b20_handle_t* handle);
static rx_err_t
                internal_ds18b20_read_scratchpad_raw(const rx_ds18b20_handle_t* handle,
                                                     uint8_t scratchpad[k_ds18b20_scratchpad_bytes]);
static rx_err_t internal_ds18b20_write_scratchpad(const rx_ds18b20_handle_t*        handle,
                                                  const ds18b20_scratchpad_write_t* scratchpad);
static uint8_t  internal_ds18b20_resolution_to_config(const ds18b20_resolution_t resolution);
static uint16_t internal_ds18b20_get_temp_mask(ds18b20_resolution_t resolution);
static float    internal_ds18b20_raw_to_celsius(const int16_t raw_temp);

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_ds18b20_init(rx_ds18b20_handle_t* handle, const rx_ds18b20_config_t* config)
{
  rx_err_t err = k_rx_ok;

  /* Validate inputs */
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is NULL");

  err = internal_ds18b20_validate_config(config, handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Initialize handle */
  memset(handle, 0, sizeof(rx_ds18b20_handle_t));
  handle->bus_manager      = config->bus_manager;
  handle->bus_name         = config->bus_name;
  handle->resolution       = config->resolution;
  handle->use_rom_matching = config->use_rom_matching;

  if (config->use_rom_matching) {
    memcpy(handle->rom, config->rom, k_onewire_rom_bytes);
  }

  /* Verify device presence */
  err = internal_ds18b20_verify_device_presence(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure resolution */
  handle->initialized = true;
  err                 = rx_ds18b20_set_resolution(handle, config->resolution);
  if (err != k_rx_ok) {
    handle->initialized = false;
    return err;
  }

  rx_log_info(s_tag, "DS18B20 initialized successfully");
  return k_rx_ok;
}

rx_err_t rx_ds18b20_deinit(rx_ds18b20_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is NULL");

  if (!handle->initialized) {
    rx_log_warn(s_tag, "DS18B20 not initialized");
    return k_rx_err_invalid_state;
  }

  /* Clear handle */
  memset(handle, 0, sizeof(rx_ds18b20_handle_t));

  rx_log_info(s_tag, "DS18B20 deinitialized");
  return k_rx_ok;
}

rx_err_t rx_ds18b20_trigger_conversion(const rx_ds18b20_handle_t* handle)
{
  bool     presence = false;
  rx_err_t err      = k_rx_ok;

  CHECK_DS18B20_HANDLE(handle, s_tag);

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for conversion trigger");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Convert T command */
  err = rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_convert_t);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send conversion command");
    return err;
  }

  return k_rx_ok;
}

rx_err_t rx_ds18b20_read_temperature(rx_ds18b20_handle_t* handle, float* temperature_celsius)
{
  int16_t  raw_temp = 0;
  rx_err_t err      = k_rx_ok;

  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(temperature_celsius, s_tag, "temperature_celsius is NULL");

  /* Read raw temperature */
  err = rx_ds18b20_read_temperature_raw(handle, &raw_temp);
  if (err != k_rx_ok) {
    return err;
  }

  /* Convert to Celsius */
  *temperature_celsius = internal_ds18b20_raw_to_celsius(raw_temp);

  return k_rx_ok;
}

rx_err_t rx_ds18b20_read_temperature_raw(rx_ds18b20_handle_t* handle, int16_t* raw_temp)
{
  uint8_t  scratchpad[k_ds18b20_scratchpad_bytes];
  rx_err_t err  = k_rx_ok;
  uint16_t temp = 0;
  uint16_t mask = 0;

  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(raw_temp, s_tag, "raw_temp is NULL");

  /* Read scratchpad */
  err = internal_ds18b20_read_scratchpad_raw(handle, scratchpad);
  if (err != k_rx_ok) {
    return err;
  }

  /* Extract temperature (LSB + MSB) */
  temp = (uint16_t)scratchpad[k_ds18b20_scratch_temp_lsb];
  temp |= ((uint16_t)scratchpad[k_ds18b20_scratch_temp_msb]) << k_ds18b20_shift_byte;

  /* Apply resolution mask to clear undefined bits */
  mask = internal_ds18b20_get_temp_mask(handle->resolution);
  temp &= mask;

  /* Post-condition: Validate temperature is within sensor range */
  *raw_temp = (int16_t)temp;
  if (*raw_temp < k_ds18b20_temp_min_raw || *raw_temp > k_ds18b20_temp_max_raw) {
    rx_log_error(s_tag, "Temperature out of sensor range (-55°C to +125°C)");
    return k_rx_err_out_of_range;
  }

  return k_rx_ok;
}

rx_err_t rx_ds18b20_set_resolution(rx_ds18b20_handle_t*       handle,
                                   const ds18b20_resolution_t resolution)
{
  uint8_t                    scratchpad[k_ds18b20_scratchpad_bytes];
  rx_err_t                   err    = k_rx_ok;
  uint8_t                    config = 0;
  ds18b20_scratchpad_write_t write_cfg;

  CHECK_DS18B20_HANDLE(handle, s_tag);

  if (resolution != k_ds18b20_resolution_9bit && resolution != k_ds18b20_resolution_10bit &&
      resolution != k_ds18b20_resolution_11bit && resolution != k_ds18b20_resolution_12bit) {
    rx_log_error(s_tag, "Invalid resolution value");
    return k_rx_err_invalid_arg;
  }

  /* Read current scratchpad */
  err = internal_ds18b20_read_scratchpad_raw(handle, scratchpad);
  if (err != k_rx_ok) {
    return err;
  }

  /* Generate new config byte */
  config = internal_ds18b20_resolution_to_config(resolution);

  /* Write scratchpad with new config */
  write_cfg.th     = scratchpad[k_ds18b20_scratch_th_reg];
  write_cfg.tl     = scratchpad[k_ds18b20_scratch_tl_reg];
  write_cfg.config = config;
  err              = internal_ds18b20_write_scratchpad(handle, &write_cfg);
  if (err != k_rx_ok) {
    return err;
  }

  /* Update handle */
  handle->resolution = resolution;

  return k_rx_ok;
}

rx_err_t rx_ds18b20_get_resolution(const rx_ds18b20_handle_t* handle,
                                   ds18b20_resolution_t*      resolution)
{
  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(resolution, s_tag, "resolution is NULL");

  *resolution = handle->resolution;
  return k_rx_ok;
}

rx_err_t rx_ds18b20_save_config(const rx_ds18b20_handle_t* handle)
{
  bool     presence = false;
  rx_err_t err      = k_rx_ok;

  CHECK_DS18B20_HANDLE(handle, s_tag);

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for EEPROM save");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Copy Scratchpad command */
  err =
    rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_copy_scratch);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send copy scratchpad command");
    return err;
  }

  return k_rx_ok;
}

rx_err_t rx_ds18b20_recall_config(const rx_ds18b20_handle_t* handle)
{
  bool     presence = false;
  rx_err_t err      = k_rx_ok;

  CHECK_DS18B20_HANDLE(handle, s_tag);

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for EEPROM recall");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Recall E2 command */
  err =
    rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_recall_eeprom);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send recall EEPROM command");
    return err;
  }

  return k_rx_ok;
}

rx_err_t rx_ds18b20_read_power_mode(const rx_ds18b20_handle_t* handle, bool* external_power)
{
  bool     presence  = false;
  bool     power_bit = false;
  rx_err_t err       = k_rx_ok;

  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(external_power, s_tag, "external_power is NULL");

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for power mode read");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Read Power Supply command */
  err = rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_read_power);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send read power command");
    return err;
  }

  /* Read power bit (1 = external, 0 = parasitic) */
  err = rx_bus_onewire_read_bit(handle->bus_manager, handle->bus_name, &power_bit);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read power bit");
    return err;
  }

  *external_power = power_bit;
  return k_rx_ok;
}

rx_err_t rx_ds18b20_read_scratchpad(rx_ds18b20_handle_t* handle,
                                    uint8_t              scratchpad[k_ds18b20_scratchpad_bytes])
{
  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(scratchpad, s_tag, "scratchpad is NULL");

  return internal_ds18b20_read_scratchpad_raw(handle, scratchpad);
}

uint32_t rx_ds18b20_get_conversion_time_ms(const rx_ds18b20_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return s_ds18b20_conversion_time_invalid_u32;
  }

  switch (handle->resolution) {
    case k_ds18b20_resolution_9bit:
      return k_ds18b20_conv_time_9bit_ms;
    case k_ds18b20_resolution_10bit:
      return k_ds18b20_conv_time_10bit_ms;
    case k_ds18b20_resolution_11bit:
      return k_ds18b20_conv_time_11bit_ms;
    case k_ds18b20_resolution_12bit:
      return k_ds18b20_conv_time_12bit_ms;
    default:
      return s_ds18b20_conversion_time_invalid_u32;
  }
}

/* =============================================================================
 * Internal Helper Functions Implementation
 * =============================================================================
 */

/**
 * @brief Validate DS18B20 configuration parameters
 *
 * @param[in] config Configuration to validate
 * @param[in] handle Handle to check initialization state
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any required pointer is NULL
 * @return k_rx_err_invalid_arg if resolution is invalid
 * @return k_rx_err_invalid_state if already initialized
 */
static rx_err_t internal_ds18b20_validate_config(const rx_ds18b20_config_t* config,
                                                 const rx_ds18b20_handle_t* handle)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config is NULL");
  RX_CHECK_NULL_PTR(config->bus_manager, s_tag, "bus_manager is NULL");
  RX_CHECK_NULL_PTR(config->bus_name, s_tag, "bus_name is NULL");

  if (handle->initialized) {
    rx_log_warn(s_tag, "DS18B20 already initialized");
    return k_rx_err_invalid_state;
  }

  if (config->resolution > k_ds18b20_resolution_12bit) {
    rx_log_error(s_tag, "Invalid resolution setting");
    return k_rx_err_invalid_arg;
  }

  if (config->use_rom_matching && config->rom[0] != s_ds18b20_family_code) {
    rx_log_error(s_tag, "Invalid DS18B20 family code");
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Verify DS18B20 device presence and communication
 *
 * Initializes the OneWire bus, checks for device presence,
 * and verifies communication by reading the scratchpad.
 *
 * @param[in] handle DS18B20 handle with bus configuration
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if device not present
 * @return Error code on bus or communication failure
 */
static rx_err_t internal_ds18b20_verify_device_presence(rx_ds18b20_handle_t* handle)
{
  bool     presence = false;
  rx_err_t err      = k_rx_ok;
  uint8_t  scratchpad[k_ds18b20_scratchpad_bytes];

  /* Initialize OneWire bus */
  err = rx_bus_onewire_init(handle->bus_manager, handle->bus_name);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to initialize OneWire bus");
    return err;
  }

  /* Check device presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "OneWire reset failed");
    return err;
  }

  if (!presence) {
    rx_log_error(s_tag, "DS18B20 device not present on bus");
    return k_rx_err_invalid_state;
  }

  /* Read scratchpad to verify communication */
  err = internal_ds18b20_read_scratchpad_raw(handle, scratchpad);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read scratchpad during init");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Select DS18B20 device using ROM matching or Skip ROM
 *
 * @param[in] handle DS18B20 handle
 *
 * @return k_rx_ok on success
 * @return Error code on failure
 */
static rx_err_t internal_ds18b20_select_device(const rx_ds18b20_handle_t* handle)
{
  rx_err_t err = k_rx_ok;

  if (handle->use_rom_matching) {
    /* Use Match ROM to address specific device */
    err = rx_bus_onewire_match_rom(handle->bus_manager, handle->bus_name, handle->rom);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to send Match ROM");
      return err;
    }
  } else {
    /* Use Skip ROM for single device or broadcast */
    err = rx_bus_onewire_skip_rom(handle->bus_manager, handle->bus_name);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to send Skip ROM");
      return err;
    }
  }

  return k_rx_ok;
}

/**
 * @brief Read scratchpad with CRC validation
 *
 * @param[in] handle DS18B20 handle
 * @param[out] scratchpad 9-byte scratchpad buffer
 *
 * @return k_rx_ok on success
 * @return k_rx_err_crc if CRC check fails
 * @return Error code on failure
 */
static rx_err_t internal_ds18b20_read_scratchpad_raw(const rx_ds18b20_handle_t* handle,
                                                     uint8_t scratchpad[k_ds18b20_scratchpad_bytes])
{
  bool     presence   = false;
  rx_err_t err        = k_rx_ok;
  uint8_t  crc_calc   = 0;
  uint8_t  crc_device = 0;

  RX_CHECK_NULL_PTR(handle, s_tag, "handle is NULL");
  RX_CHECK_NULL_PTR(scratchpad, s_tag, "scratchpad is NULL");

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for scratchpad read");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Read Scratchpad command */
  err =
    rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_read_scratch);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send read scratchpad command");
    return err;
  }

  /* Read 9 bytes (8 data + 1 CRC) */
  err = rx_bus_onewire_read(handle->bus_manager,
                            handle->bus_name,
                            scratchpad,
                            k_ds18b20_scratchpad_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read scratchpad data");
    return err;
  }

  /* Validate CRC */
  crc_calc   = rx_crc8_maxim(scratchpad, k_ds18b20_crc_bytes);
  crc_device = scratchpad[k_ds18b20_scratch_crc];

  if (crc_calc != crc_device) {
    rx_log_error(s_tag, "Scratchpad CRC mismatch");
    return k_rx_err_crc_mismatch;
  }

  /* Post-condition: Validate scratchpad structure (reserved byte should be 0xFF) */
  if (scratchpad[k_ds18b20_scratch_reserved1] != s_ds18b20_reserved_byte_value) {
    rx_log_warn(s_tag, "Scratchpad reserved byte unexpected value");
  }

  return k_rx_ok;
}

/**
 * @brief Write scratchpad (TH, TL, Config registers)
 *
 * @param[in] handle DS18B20 handle
 * @param[in] scratchpad Scratchpad write values (TH, TL, config)
 *
 * @return k_rx_ok on success
 * @return Error code on failure
 */
static rx_err_t internal_ds18b20_write_scratchpad(const rx_ds18b20_handle_t*        handle,
                                                  const ds18b20_scratchpad_write_t* scratchpad)
{
  bool     presence = false;
  rx_err_t err      = k_rx_ok;
  uint8_t  write_buf[k_ds18b20_scratchpad_write_bytes];

  RX_CHECK_NULL_PTR(handle, s_tag, "handle is NULL");
  RX_CHECK_NULL_PTR(scratchpad, s_tag, "scratchpad is NULL");

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for scratchpad write");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Write Scratchpad command */
  err =
    rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_write_scratch);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send write scratchpad command");
    return err;
  }

  /* Write 3 bytes (TH, TL, Config) */
  write_buf[k_ds18b20_write_idx_th]     = scratchpad->th;
  write_buf[k_ds18b20_write_idx_tl]     = scratchpad->tl;
  write_buf[k_ds18b20_write_idx_config] = scratchpad->config;

  err = rx_bus_onewire_write(handle->bus_manager,
                             handle->bus_name,
                             write_buf,
                             k_ds18b20_scratchpad_write_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to write scratchpad data");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Convert resolution enum to config register value
 *
 * @param[in] resolution Resolution mode
 *
 * @return Configuration register value
 */
static uint8_t internal_ds18b20_resolution_to_config(const ds18b20_resolution_t resolution)
{
  uint8_t config = k_ds18b20_config_register_cleared;

  /* Resolution bits are R1:R0 at bits 6:5 */
  config = (uint8_t)resolution << k_ds18b20_config_r0_bit;

  return config;
}

/**
 * @brief Get temperature mask for resolution
 *
 * Lower bits are undefined for resolutions < 12-bit and must be masked.
 *
 * @param[in] resolution Resolution mode
 *
 * @return Temperature mask
 */
static uint16_t internal_ds18b20_get_temp_mask(const ds18b20_resolution_t resolution)
{
  switch (resolution) {
    case k_ds18b20_resolution_9bit:
      return k_ds18b20_temp_mask_9bit;
    case k_ds18b20_resolution_10bit:
      return k_ds18b20_temp_mask_10bit;
    case k_ds18b20_resolution_11bit:
      return k_ds18b20_temp_mask_11bit;
    case k_ds18b20_resolution_12bit:
      return k_ds18b20_temp_mask_12bit;
    default:
      return k_ds18b20_temp_mask_12bit;
  }
}

/**
 * @brief Convert raw temperature to Celsius
 *
 * DS18B20 stores temperature as 16-bit signed value in 1/16°C units.
 * Divide by 16 to get Celsius.
 *
 * @param[in] raw_temp Raw 16-bit temperature value
 *
 * @return Temperature in degrees Celsius
 */
static float internal_ds18b20_raw_to_celsius(const int16_t raw_temp)
{
  float result = NAN;

  if (raw_temp < k_ds18b20_temp_min_raw || raw_temp > k_ds18b20_temp_max_raw) {
    rx_log_error(s_tag, "Raw temperature out of range - returning NaN sentinel");
    return NAN;
  }

  /* Convert from 1/16°C to °C */
  result = (float)raw_temp / s_temp_conversion_divisor;

  /* Post-condition: Validate result is finite (not NaN or Inf) */
  if (!isfinite(result)) {
    rx_log_error(s_tag, "Computed Celsius not finite - returning NaN sentinel");
    return NAN;
  }

  return result;
}
