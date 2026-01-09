/* lib/rx_bq78350/src/rx_bq78350.c */

/**
 * @file rx_bq78350.c
 * @brief BQ78350-R1A Battery Fuel Gauge Driver Implementation
 *
 * Implements Smart Battery System (SBS) 1.1 specification for BQ78350-R1A.
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_bq78350.h"

#include "rx_bus_smbus.h"
#include "rx_check.h"
#include "rx_log.h"
#include <string.h>

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static const char* s_tag = "BQ78350";

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @brief Temperature conversion constants
 */
typedef enum {
  k_temp_kelvin_offset = 2731, /**< Offset in 0.1K (273.15K = 0°C, scaled by 10) */
  k_temp_decimal_scale = 10,   /**< Scale factor for 0.1K units */
} bq78350_temp_constants_t;

/**
 * @brief Buffer size constants
 */
typedef enum {
  k_null_terminator_size = 1, /**< Size of null terminator for buffer calculations */
} buffer_constants_t;

int16_t rx_bq78350_temp_to_celsius(int16_t temperature_01k)
{
  return (temperature_01k - k_temp_kelvin_offset) / k_temp_decimal_scale;
}

int16_t rx_bq78350_celsius_to_temp(int16_t temperature_celsius)
{
  return (temperature_celsius * k_temp_decimal_scale) + k_temp_kelvin_offset;
}

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Read device information string with graceful error handling
 *
 * @param[in]  handle       BQ78350 device handle
 * @param[in]  command      SBS command to read
 * @param[out] dest         Destination string buffer
 * @param[in]  max_len      Maximum length including null terminator
 */
static void internal_read_info_string(const rx_bq78350_handle_t* handle,
                                       uint8_t                    command,
                                       char*                      dest,
                                       uint8_t                    max_len)
{
  uint8_t actual_length = 0;
  rx_err_t err = rx_bq78350_read_block(handle,
                                       command,
                                       (uint8_t*)dest,
                                       max_len - k_null_terminator_size,
                                       &actual_length);
  if (err == k_rx_ok) {
    dest[actual_length] = '\0'; /* Null terminate */
  } else {
    rx_log_warn(s_tag, "Failed to read device info string");
    dest[0] = '\0'; /* Empty string on failure */
  }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_bq78350_init(rx_bus_manager_t*          manager,
                         rx_bq78350_handle_t*       handle,
                         const rx_bq78350_config_t* config)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "Manager pointer is NULL");
  RX_CHECK_NULL_PTR(handle, s_tag, "Handle pointer is NULL");
  RX_CHECK_NULL_PTR(config, s_tag, "Config pointer is NULL");
  RX_CHECK_NULL_PTR(config->bus_name, s_tag, "Bus name pointer is NULL");

  /* Store bus manager in handle for subsequent operations */
  handle->bus_manager = manager;
  handle->bus_name    = config->bus_name;

  /* Initialize SMBus */
  rx_err_t err = rx_bus_smbus_init(manager, config->bus_name);
  RX_RETURN_ON_ERROR(err, s_tag, "SMBus initialization failed");

  /* Verify device presence by reading device type */
  uint16_t device_type = 0;
  err = rx_bq78350_manufacturer_access(handle, k_bq78350_mfg_device_type, &device_type);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read device type - device not responding");
    return err;
  }

  /* Mark as initialized */
  handle->initialized = true;

  rx_log_info(s_tag, "BQ78350 initialized successfully");

  return k_rx_ok;
}

rx_err_t rx_bq78350_deinit(rx_bq78350_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "Handle pointer is NULL");

  handle->initialized = false;
  handle->bus_name    = NULL;

  rx_log_debug(s_tag, "BQ78350 deinitialized");

  return k_rx_ok;
}

rx_err_t rx_bq78350_read_telemetry(const rx_bq78350_handle_t* handle,
                                    rx_bq78350_telemetry_t*    telemetry)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "Handle pointer is NULL");
  RX_CHECK_NULL_PTR(telemetry, s_tag, "Telemetry pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Device not initialized");
    return k_rx_err_invalid_state;
  }

  rx_err_t err = k_rx_ok;

  /* Read temperature (0.1K) */
  err = rx_bq78350_read_word(handle, k_bq78350_cmd_temperature, (uint16_t*)&telemetry->temperature_01k);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read temperature");

  /* Read voltage (mV) */
  err = rx_bq78350_read_word(handle, k_bq78350_cmd_voltage, &telemetry->voltage_mv);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read voltage");

  /* Read current (mA, signed) */
  err = rx_bq78350_read_word(handle, k_bq78350_cmd_current, (uint16_t*)&telemetry->current_ma);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read current");

  /* Read average current (mA, signed) */
  err = rx_bq78350_read_word(handle, k_bq78350_cmd_average_current, (uint16_t*)&telemetry->avg_current_ma);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read average current");

  /* Read relative state of charge (%) */
  err = rx_bq78350_read_word(handle, k_bq78350_cmd_relative_soc, &telemetry->relative_soc_pct);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read relative SOC");

  /* Read absolute state of charge (%) */
  err = rx_bq78350_read_word(handle, k_bq78350_cmd_absolute_soc, &telemetry->absolute_soc_pct);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read absolute SOC");

  /* Read remaining capacity (mAh) */
  err = rx_bq78350_read_word(handle, k_bq78350_cmd_remaining_capacity, &telemetry->remaining_mah);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read remaining capacity");

  /* Read full charge capacity (mAh) */
  err = rx_bq78350_read_word(handle, k_bq78350_cmd_full_charge_capacity, &telemetry->full_capacity_mah);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read full charge capacity");

  /* Read battery status */
  err = rx_bq78350_read_word(handle, k_bq78350_cmd_battery_status, &telemetry->battery_status);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read battery status");

  /* Read cycle count */
  err = rx_bq78350_read_word(handle, k_bq78350_cmd_cycle_count, &telemetry->cycle_count);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read cycle count");

  return k_rx_ok;
}

rx_err_t rx_bq78350_read_cell_voltages(const rx_bq78350_handle_t* handle,
                                        rx_bq78350_cell_voltages_t* cell_voltages,
                                        uint8_t                     num_cells)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "Handle pointer is NULL");
  RX_CHECK_NULL_PTR(cell_voltages, s_tag, "Cell voltages pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Device not initialized");
    return k_rx_err_invalid_state;
  }

  if (num_cells < k_bq78350_min_cells || num_cells > k_bq78350_max_cells) {
    rx_log_error(s_tag, "Invalid number of cells");
    return k_rx_err_invalid_arg;
  }

  /**
   * @brief Cell voltage register mapping array
   *
   * BQ78350 cell voltage registers are in reverse order (0x30-0x3F for cells 1-16):
   * - Cell N is at address (0x40 - N)
   * - Cell 1 at 0x3F, Cell 2 at 0x3E, ..., Cell 16 at 0x30
   */
  static const uint8_t s_cell_reg_map[k_bq78350_max_cells] = {
    k_bq78350_cmd_cell_voltage_1,  /* Cell 1 at 0x3F */
    k_bq78350_cmd_cell_voltage_2,  /* Cell 2 at 0x3E */
    k_bq78350_cmd_cell_voltage_3,  /* Cell 3 at 0x3D */
    k_bq78350_cmd_cell_voltage_4,  /* Cell 4 at 0x3C */
    k_bq78350_cmd_cell_voltage_5,  /* Cell 5 at 0x3B */
    k_bq78350_cmd_cell_voltage_6,  /* Cell 6 at 0x3A */
    k_bq78350_cmd_cell_voltage_7,  /* Cell 7 at 0x39 */
    k_bq78350_cmd_cell_voltage_8,  /* Cell 8 at 0x38 */
    k_bq78350_cmd_cell_voltage_9,  /* Cell 9 at 0x37 */
    k_bq78350_cmd_cell_voltage_10, /* Cell 10 at 0x36 */
    k_bq78350_cmd_cell_voltage_11, /* Cell 11 at 0x35 */
    k_bq78350_cmd_cell_voltage_12, /* Cell 12 at 0x34 */
    k_bq78350_cmd_cell_voltage_13, /* Cell 13 at 0x33 */
    k_bq78350_cmd_cell_voltage_14, /* Cell 14 at 0x32 */
    k_bq78350_cmd_cell_voltage_15, /* Cell 15 at 0x31 */
    k_bq78350_cmd_cell_voltage_16, /* Cell 16 at 0x30 */
  };

  cell_voltages->num_cells = num_cells;

  for (uint8_t i = 0; i < num_cells; i++) {
    rx_err_t err = rx_bq78350_read_word(handle, s_cell_reg_map[i], &cell_voltages->cell_voltage_mv[i]);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to read cell voltage");
      return err;
    }
  }

  return k_rx_ok;
}

rx_err_t rx_bq78350_read_device_info(const rx_bq78350_handle_t* handle,
                                      rx_bq78350_device_info_t*  info)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "Handle pointer is NULL");
  RX_CHECK_NULL_PTR(info, s_tag, "Info pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Device not initialized");
    return k_rx_err_invalid_state;
  }

  /* Read string fields (non-critical - graceful degradation) */
  internal_read_info_string(handle, k_bq78350_cmd_manufacturer_name,
                           info->manufacturer_name, k_bq78350_max_name_len);
  internal_read_info_string(handle, k_bq78350_cmd_device_name,
                           info->device_name, k_bq78350_max_name_len);
  internal_read_info_string(handle, k_bq78350_cmd_device_chemistry,
                           info->chemistry, k_bq78350_max_name_len);

  /* Read critical numeric fields (failures propagate) */
  rx_err_t err = k_rx_ok;

  err = rx_bq78350_read_word(handle, k_bq78350_cmd_serial_number, &info->serial_number);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read serial number");

  err = rx_bq78350_manufacturer_access(handle, k_bq78350_mfg_firmware_version, &info->firmware_version);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read firmware version");

  err = rx_bq78350_manufacturer_access(handle, k_bq78350_mfg_hardware_version, &info->hardware_version);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read hardware version");

  err = rx_bq78350_read_word(handle, k_bq78350_cmd_design_capacity, &info->design_capacity_mah);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read design capacity");

  err = rx_bq78350_read_word(handle, k_bq78350_cmd_design_voltage, &info->design_voltage_mv);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read design voltage");

  return k_rx_ok;
}

rx_err_t rx_bq78350_read_word(const rx_bq78350_handle_t* handle, uint8_t command, uint16_t* value)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "Handle pointer is NULL");
  RX_CHECK_NULL_PTR(value, s_tag, "Value pointer is NULL");
  RX_CHECK_NULL_PTR(handle->bus_manager, s_tag, "Bus manager not initialized");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Device not initialized");
    return k_rx_err_invalid_state;
  }

  return rx_bus_smbus_read_word_data(handle->bus_manager, handle->bus_name, command, value);
}

rx_err_t rx_bq78350_write_word(const rx_bq78350_handle_t* handle, uint8_t command, uint16_t value)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "Handle pointer is NULL");
  RX_CHECK_NULL_PTR(handle->bus_manager, s_tag, "Bus manager not initialized");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Device not initialized");
    return k_rx_err_invalid_state;
  }

  return rx_bus_smbus_write_word_data(handle->bus_manager, handle->bus_name, command, value);
}

rx_err_t rx_bq78350_read_block(const rx_bq78350_handle_t* handle,
                               uint8_t                    command,
                               uint8_t*                   data,
                               uint8_t                    max_length,
                               uint8_t*                   actual_length)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "Handle pointer is NULL");
  RX_CHECK_NULL_PTR(data, s_tag, "Data pointer is NULL");
  RX_CHECK_NULL_PTR(actual_length, s_tag, "Actual length pointer is NULL");
  RX_CHECK_NULL_PTR(handle->bus_manager, s_tag, "Bus manager not initialized");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Device not initialized");
    return k_rx_err_invalid_state;
  }

  return rx_bus_smbus_read_block_data(handle->bus_manager, handle->bus_name, command, data, actual_length, max_length);
}

rx_err_t rx_bq78350_manufacturer_access(const rx_bq78350_handle_t* handle,
                                         uint16_t                   mfg_command,
                                         uint16_t*                  response)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "Handle pointer is NULL");
  RX_CHECK_NULL_PTR(response, s_tag, "Response pointer is NULL");
  RX_CHECK_NULL_PTR(handle->bus_manager, s_tag, "Bus manager not initialized");

  /**
   * @note: We don't check handle->initialized here because this function
   * is also called during initialization (to verify device presence).
   * The handle->bus_manager check above ensures the bus is initialized.
   */

  /* Write manufacturer access command to 0x00 */
  rx_err_t err = rx_bus_smbus_write_word_data(handle->bus_manager,
                                              handle->bus_name,
                                              k_bq78350_cmd_manufacturer_access,
                                              mfg_command);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to write manufacturer access command");

  /* Read response from 0x00 */
  err = rx_bus_smbus_read_word_data(handle->bus_manager,
                                    handle->bus_name,
                                    k_bq78350_cmd_manufacturer_access,
                                    response);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to read manufacturer access response");

  return k_rx_ok;
}

rx_err_t rx_bq78350_reset(const rx_bq78350_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "Handle pointer is NULL");
  RX_CHECK_NULL_PTR(handle->bus_manager, s_tag, "Bus manager not initialized");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Device not initialized");
    return k_rx_err_invalid_state;
  }

  /* Write reset command to manufacturer access */
  rx_err_t err = rx_bus_smbus_write_word_data(handle->bus_manager,
                                              handle->bus_name,
                                              k_bq78350_cmd_manufacturer_access,
                                              k_bq78350_mfg_reset);
  RX_RETURN_ON_ERROR(err, s_tag, "Failed to send reset command");

  rx_log_info(s_tag, "BQ78350 reset command sent");

  return k_rx_ok;
}
