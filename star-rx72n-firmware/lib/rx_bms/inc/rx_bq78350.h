/* lib/rx_bq78350/inc/rx_bq78350.h */

/**
 * @file rx_bq78350.h
 * @brief BQ78350-R1A Battery Fuel Gauge Driver for RX72N
 *
 * Driver for Texas Instruments BQ78350-R1A battery fuel gauge IC.
 * Implements Smart Battery System (SBS) 1.1 specification via SMBus protocol.
 *
 * Features:
 * - Standard SBS commands (voltage, current, SOC, temperature, etc.)
 * - TI-specific manufacturer access commands
 * - Cell voltage monitoring (supports up to 16 cells)
 * - Authentication and security
 * - Data flash configuration access
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_BQ78350_H
#define STAR_RX_BQ78350_H

#include "rx_bus_manager.h"
#include "rx_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief BQ78350 default SMBus address */
typedef enum {
  k_bq78350_default_addr = 0x0B, /**< Default 7-bit SMBus address (0x16 >> 1) */
} bq78350_addresses_t;

/** @brief BQ78350 constants */
typedef enum {
  k_bq78350_min_cells     = 1,   /**< Minimum number of series cells */
  k_bq78350_max_cells     = 16,  /**< Maximum number of series cells */
  k_bq78350_max_name_len  = 21,  /**< Maximum string length (20 + null) */
  k_bq78350_serial_len    = 2,   /**< Serial number length (16-bit) */
} bq78350_constants_t;

/** @brief Standard SBS command codes */
typedef enum {
  k_bq78350_cmd_manufacturer_access  = 0x00, /**< Manufacturer access commands */
  k_bq78350_cmd_remaining_alarm      = 0x01, /**< Remaining capacity alarm (mAh) */
  k_bq78350_cmd_remaining_time_alarm = 0x02, /**< Remaining time alarm (min) */
  k_bq78350_cmd_battery_mode         = 0x03, /**< Battery mode configuration */
  k_bq78350_cmd_at_rate              = 0x04, /**< AtRate value (mA) */
  k_bq78350_cmd_at_rate_time_to_full = 0x05, /**< Time to full at AtRate (min) */
  k_bq78350_cmd_at_rate_time_to_empty = 0x06, /**< Time to empty at AtRate (min) */
  k_bq78350_cmd_at_rate_ok           = 0x07, /**< AtRate OK flag */
  k_bq78350_cmd_temperature          = 0x08, /**< Temperature (0.1K) */
  k_bq78350_cmd_voltage              = 0x09, /**< Voltage (mV) */
  k_bq78350_cmd_current              = 0x0A, /**< Current (mA, signed) */
  k_bq78350_cmd_average_current      = 0x0B, /**< Average current (mA, signed) */
  k_bq78350_cmd_max_error            = 0x0C, /**< Max error (%) */
  k_bq78350_cmd_relative_soc         = 0x0D, /**< Relative state of charge (%) */
  k_bq78350_cmd_absolute_soc         = 0x0E, /**< Absolute state of charge (%) */
  k_bq78350_cmd_remaining_capacity   = 0x0F, /**< Remaining capacity (mAh) */
  k_bq78350_cmd_full_charge_capacity = 0x10, /**< Full charge capacity (mAh) */
  k_bq78350_cmd_run_time_to_empty    = 0x11, /**< Runtime to empty (min) */
  k_bq78350_cmd_avg_time_to_empty    = 0x12, /**< Average time to empty (min) */
  k_bq78350_cmd_avg_time_to_full     = 0x13, /**< Average time to full (min) */
  k_bq78350_cmd_charging_current     = 0x14, /**< Charging current (mA) */
  k_bq78350_cmd_charging_voltage     = 0x15, /**< Charging voltage (mV) */
  k_bq78350_cmd_battery_status       = 0x16, /**< Battery status flags */
  k_bq78350_cmd_cycle_count          = 0x17, /**< Cycle count */
  k_bq78350_cmd_design_capacity      = 0x18, /**< Design capacity (mAh) */
  k_bq78350_cmd_design_voltage       = 0x19, /**< Design voltage (mV) */
  k_bq78350_cmd_spec_info            = 0x1A, /**< Specification info */
  k_bq78350_cmd_mfg_date             = 0x1B, /**< Manufacture date */
  k_bq78350_cmd_serial_number        = 0x1C, /**< Serial number */
  k_bq78350_cmd_manufacturer_name    = 0x20, /**< Manufacturer name (block) */
  k_bq78350_cmd_device_name          = 0x21, /**< Device name (block) */
  k_bq78350_cmd_device_chemistry     = 0x22, /**< Device chemistry (block) */
  k_bq78350_cmd_manufacturer_data    = 0x23, /**< Manufacturer data (block) */
  k_bq78350_cmd_cell_voltage_16      = 0x30, /**< Cell 16 voltage (mV) */
  k_bq78350_cmd_cell_voltage_15      = 0x31, /**< Cell 15 voltage (mV) */
  k_bq78350_cmd_cell_voltage_14      = 0x32, /**< Cell 14 voltage (mV) */
  k_bq78350_cmd_cell_voltage_13      = 0x33, /**< Cell 13 voltage (mV) */
  k_bq78350_cmd_cell_voltage_12      = 0x34, /**< Cell 12 voltage (mV) */
  k_bq78350_cmd_cell_voltage_11      = 0x35, /**< Cell 11 voltage (mV) */
  k_bq78350_cmd_cell_voltage_10      = 0x36, /**< Cell 10 voltage (mV) */
  k_bq78350_cmd_cell_voltage_9       = 0x37, /**< Cell 9 voltage (mV) */
  k_bq78350_cmd_cell_voltage_8       = 0x38, /**< Cell 8 voltage (mV) */
  k_bq78350_cmd_cell_voltage_7       = 0x39, /**< Cell 7 voltage (mV) */
  k_bq78350_cmd_cell_voltage_6       = 0x3A, /**< Cell 6 voltage (mV) */
  k_bq78350_cmd_cell_voltage_5       = 0x3B, /**< Cell 5 voltage (mV) */
  k_bq78350_cmd_cell_voltage_4       = 0x3C, /**< Cell 4 voltage (mV) */
  k_bq78350_cmd_cell_voltage_3       = 0x3D, /**< Cell 3 voltage (mV) */
  k_bq78350_cmd_cell_voltage_2       = 0x3E, /**< Cell 2 voltage (mV) */
  k_bq78350_cmd_cell_voltage_1       = 0x3F, /**< Cell 1 voltage (mV) */
} bq78350_sbs_commands_t;

/** @brief Battery status flags (0x16) */
typedef enum {
  k_bq78350_status_over_charged_alarm = (1 << 15), /**< Over charged alarm */
  k_bq78350_status_terminate_charge_alarm = (1 << 14), /**< Terminate charge alarm */
  k_bq78350_status_over_temp_alarm    = (1 << 12), /**< Over temperature alarm */
  k_bq78350_status_terminate_discharge_alarm = (1 << 11), /**< Terminate discharge alarm */
  k_bq78350_status_remaining_capacity_alarm = (1 << 9), /**< Remaining capacity alarm */
  k_bq78350_status_remaining_time_alarm = (1 << 8), /**< Remaining time alarm */
  k_bq78350_status_initialized        = (1 << 7),  /**< Initialized */
  k_bq78350_status_discharging        = (1 << 6),  /**< Discharging */
  k_bq78350_status_fully_charged      = (1 << 5),  /**< Fully charged */
  k_bq78350_status_fully_discharged   = (1 << 4),  /**< Fully discharged */
} bq78350_battery_status_t;

/** @brief Manufacturer access subcommands (via 0x00/0x01) */
typedef enum {
  k_bq78350_mfg_device_type        = 0x0001, /**< Device type */
  k_bq78350_mfg_firmware_version   = 0x0002, /**< Firmware version */
  k_bq78350_mfg_hardware_version   = 0x0003, /**< Hardware version */
  k_bq78350_mfg_reset              = 0x0041, /**< Full reset */
  k_bq78350_mfg_safety_alert       = 0x0050, /**< Safety alert status */
  k_bq78350_mfg_safety_status      = 0x0051, /**< Safety status */
  k_bq78350_mfg_pf_alert           = 0x0052, /**< Permanent fail alert */
  k_bq78350_mfg_pf_status          = 0x0053, /**< Permanent fail status */
  k_bq78350_mfg_operation_status   = 0x0054, /**< Operation status */
  k_bq78350_mfg_charging_status    = 0x0055, /**< Charging status */
  k_bq78350_mfg_gauging_status     = 0x0056, /**< Gauging status */
} bq78350_mfg_access_commands_t;

/* =============================================================================
 * Data Structures
 * =============================================================================
 */

/**
 * @brief BQ78350 configuration structure
 */
typedef struct {
  const char* bus_name; /**< SMBus bus name (from bus manager config) */
} rx_bq78350_config_t;

/**
 * @brief BQ78350 device handle
 */
typedef struct {
  rx_bus_manager_t* bus_manager; /**< Bus manager instance */
  const char*       bus_name;     /**< SMBus bus name */
  bool              initialized;   /**< Initialization status */
} rx_bq78350_handle_t;

/**
 * @brief Battery telemetry data
 */
typedef struct {
  int16_t  temperature_01k;  /**< Temperature in 0.1 Kelvin */
  uint16_t voltage_mv;       /**< Voltage in millivolts */
  int16_t  current_ma;       /**< Current in milliamps (signed, + = charging) */
  int16_t  avg_current_ma;   /**< Average current in milliamps */
  uint16_t relative_soc_pct; /**< Relative state of charge (%) */
  uint16_t absolute_soc_pct; /**< Absolute state of charge (%) */
  uint16_t remaining_mah;    /**< Remaining capacity (mAh) */
  uint16_t full_capacity_mah; /**< Full charge capacity (mAh) */
  uint16_t battery_status;   /**< Battery status flags */
  uint16_t cycle_count;      /**< Charge/discharge cycle count */
} rx_bq78350_telemetry_t;

/**
 * @brief Cell voltage data (up to 16 cells)
 */
typedef struct {
  uint8_t  num_cells;                     /**< Number of cells in series */
  uint16_t cell_voltage_mv[k_bq78350_max_cells]; /**< Individual cell voltages (mV) */
} rx_bq78350_cell_voltages_t;

/**
 * @brief Device information
 */
typedef struct {
  char     manufacturer_name[k_bq78350_max_name_len]; /**< Manufacturer name string */
  char     device_name[k_bq78350_max_name_len];       /**< Device name string */
  char     chemistry[k_bq78350_max_name_len];         /**< Battery chemistry string */
  uint16_t serial_number;                             /**< Serial number */
  uint16_t firmware_version;                          /**< Firmware version */
  uint16_t hardware_version;                          /**< Hardware version */
  uint16_t design_capacity_mah;                       /**< Design capacity (mAh) */
  uint16_t design_voltage_mv;                         /**< Design voltage (mV) */
} rx_bq78350_device_info_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize BQ78350 device
 *
 * Initializes SMBus communication and verifies device presence.
 *
 * @param[in] manager Bus manager instance
 * @param[out] handle Pointer to device handle
 * @param[in] config Pointer to configuration
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handle or config is NULL,
 *         k_rx_err_invalid_arg if configuration is invalid,
 *         k_rx_err_timeout if device doesn't respond,
 *         k_rx_err_nack if device not found
 */
rx_err_t rx_bq78350_init(rx_bus_manager_t*           manager,
                         rx_bq78350_handle_t*        handle,
                         const rx_bq78350_config_t*  config);

/**
 * @brief Deinitialize BQ78350 device
 *
 * @param[in] handle Pointer to device handle
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handle is NULL
 */
rx_err_t rx_bq78350_deinit(rx_bq78350_handle_t* handle);

/**
 * @brief Read battery telemetry data
 *
 * Reads all standard SBS telemetry values in a single function.
 *
 * @param[in] handle Pointer to device handle
 * @param[out] telemetry Pointer to telemetry structure
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handle or telemetry is NULL,
 *         k_rx_err_invalid_state if device not initialized,
 *         k_rx_err_timeout if communication timeout,
 *         k_rx_err_crc if PEC check fails
 */
rx_err_t rx_bq78350_read_telemetry(const rx_bq78350_handle_t* handle,
                                    rx_bq78350_telemetry_t*    telemetry);

/**
 * @brief Read individual cell voltages
 *
 * Reads voltage for each cell in the battery pack.
 *
 * @param[in] handle Pointer to device handle
 * @param[out] cell_voltages Pointer to cell voltage structure
 * @param[in] num_cells Number of cells to read (1-16)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handle or cell_voltages is NULL,
 *         k_rx_err_invalid_arg if num_cells is invalid,
 *         k_rx_err_invalid_state if device not initialized,
 *         k_rx_err_timeout if communication timeout,
 *         k_rx_err_crc if PEC check fails
 */
rx_err_t rx_bq78350_read_cell_voltages(const rx_bq78350_handle_t* handle,
                                        rx_bq78350_cell_voltages_t* cell_voltages,
                                        uint8_t                     num_cells);

/**
 * @brief Read device information
 *
 * Reads manufacturer name, device name, chemistry, serial number, versions, etc.
 *
 * @param[in] handle Pointer to device handle
 * @param[out] info Pointer to device info structure
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handle or info is NULL,
 *         k_rx_err_invalid_state if device not initialized,
 *         k_rx_err_timeout if communication timeout,
 *         k_rx_err_crc if PEC check fails
 */
rx_err_t rx_bq78350_read_device_info(const rx_bq78350_handle_t* handle,
                                      rx_bq78350_device_info_t*  info);

/**
 * @brief Read 16-bit word from SBS command
 *
 * Generic function to read any standard SBS word command.
 *
 * @param[in] handle Pointer to device handle
 * @param[in] command SBS command code
 * @param[out] value Pointer to store 16-bit value
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handle or value is NULL,
 *         k_rx_err_invalid_state if device not initialized,
 *         k_rx_err_timeout if communication timeout,
 *         k_rx_err_crc if PEC check fails
 */
rx_err_t rx_bq78350_read_word(const rx_bq78350_handle_t* handle, uint8_t command, uint16_t* value);

/**
 * @brief Write 16-bit word to SBS command
 *
 * Generic function to write any standard SBS word command.
 *
 * @param[in] handle Pointer to device handle
 * @param[in] command SBS command code
 * @param[in] value 16-bit value to write
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handle is NULL,
 *         k_rx_err_invalid_state if device not initialized,
 *         k_rx_err_timeout if communication timeout
 */
rx_err_t rx_bq78350_write_word(const rx_bq78350_handle_t* handle, uint8_t command, uint16_t value);

/**
 * @brief Read block data from SBS command
 *
 * Generic function to read block data (e.g., strings, manufacturer data).
 *
 * @param[in] handle Pointer to device handle
 * @param[in] command SBS command code
 * @param[out] data Pointer to buffer for block data
 * @param[in] max_length Maximum buffer size
 * @param[out] actual_length Pointer to store actual bytes read
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if pointers are NULL,
 *         k_rx_err_invalid_arg if max_length is invalid,
 *         k_rx_err_invalid_state if device not initialized,
 *         k_rx_err_timeout if communication timeout,
 *         k_rx_err_crc if PEC check fails
 */
rx_err_t rx_bq78350_read_block(const rx_bq78350_handle_t* handle,
                               uint8_t                    command,
                               uint8_t*                   data,
                               uint8_t                    max_length,
                               uint8_t*                   actual_length);

/**
 * @brief Execute manufacturer access command
 *
 * Sends manufacturer access command via 0x00/0x01 interface.
 *
 * @param[in] handle Pointer to device handle
 * @param[in] mfg_command Manufacturer access subcommand
 * @param[out] response Pointer to store 16-bit response
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handle or response is NULL,
 *         k_rx_err_invalid_state if device not initialized,
 *         k_rx_err_timeout if communication timeout,
 *         k_rx_err_crc if PEC check fails
 */
rx_err_t rx_bq78350_manufacturer_access(const rx_bq78350_handle_t* handle,
                                         uint16_t                   mfg_command,
                                         uint16_t*                  response);

/**
 * @brief Reset BQ78350 device
 *
 * Performs full device reset via manufacturer access command.
 *
 * @param[in] handle Pointer to device handle
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if handle is NULL,
 *         k_rx_err_invalid_state if device not initialized,
 *         k_rx_err_timeout if communication timeout
 */
rx_err_t rx_bq78350_reset(const rx_bq78350_handle_t* handle);

/**
 * @brief Convert temperature from 0.1K to Celsius
 *
 * @param[in] temperature_01k Temperature in 0.1 Kelvin
 *
 * @return Temperature in Celsius (integer)
 */
int16_t rx_bq78350_temp_to_celsius(int16_t temperature_01k);

/**
 * @brief Convert temperature from Celsius to 0.1K
 *
 * @param[in] temperature_celsius Temperature in Celsius
 *
 * @return Temperature in 0.1 Kelvin
 */
int16_t rx_bq78350_celsius_to_temp(int16_t temperature_celsius);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_BQ78350_H */
