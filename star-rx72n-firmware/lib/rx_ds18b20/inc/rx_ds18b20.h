/* lib/rx_ds18b20/inc/rx_ds18b20.h */

/**
 * @file rx_ds18b20.h
 * @brief DS18B20 digital temperature sensor driver
 * @details
 * Driver for Maxim DS18B20 1-Wire digital temperature sensor.
 *
 * Features:
 * - Temperature range: -55C to +125C
 * - Accuracy: +/-0.5C (-10C to +85C)
 * - Resolution: 9-12 bits (configurable)
 * - Single bus can support multiple sensors
 *
 * The DS18B20 communicates over the OneWire protocol. Each sensor has a unique
 * 64-bit ROM code that identifies it on the bus. This driver supports both
 * single-sensor operation (using Skip ROM) and multi-sensor operation (using
 * Match ROM with specific ROM addresses).
 *
 * Typical usage:
 * @code
 * // Single sensor on bus
 * rx_ds18b20_handle_t sensor;
 * rx_ds18b20_init(&sensor, &bus_manager, "temp_bus", NULL);
 * rx_ds18b20_start_conversion(&sensor);
 * tx_thread_sleep(75);  // Wait for conversion (750ms for 12-bit)
 * float temp_c;
 * rx_ds18b20_read_temperature(&sensor, &temp_c);
 *
 * // Multiple sensors - find all devices first
 * uint8_t roms[8 * 4];  // Up to 4 sensors
 * uint32_t count;
 * rx_ds18b20_search_devices(&bus_manager, "temp_bus", roms, 4, &count);
 *
 * // Initialize specific sensor by ROM
 * rx_ds18b20_handle_t sensor1;
 * rx_ds18b20_init(&sensor1, &bus_manager, "temp_bus", &roms[0]);
 * @endcode
 *
 * References:
 * - DS18B20 Datasheet (Maxim Integrated)
 * - 1-Wire Protocol Specification
 *
 * @date 2026-01-03
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_DS18B20_H
#define STAR_RX72N_DS18B20_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_bus_onewire.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * DS18B20 Constants
 * =============================================================================
 */

/**
 * @brief DS18B20 ROM and device constants
 */
typedef enum {
  k_ds18b20_rom_bytes        = 8,    /**< ROM code size in bytes (64 bits) */
  k_ds18b20_scratchpad_bytes = 9,    /**< Scratchpad size in bytes */
  k_ds18b20_family_code      = 0x28, /**< DS18B20 family code (first ROM byte) */
} ds18b20_device_constants_t;

/**
 * @brief DS18B20 function command codes
 */
typedef enum {
  k_ds18b20_cmd_convert_t        = 0x44, /**< Start temperature conversion */
  k_ds18b20_cmd_read_scratchpad  = 0xBE, /**< Read 9-byte scratchpad */
  k_ds18b20_cmd_write_scratchpad = 0x4E, /**< Write 3 bytes to scratchpad */
  k_ds18b20_cmd_copy_scratchpad  = 0x48, /**< Copy scratchpad to EEPROM */
  k_ds18b20_cmd_recall_e2        = 0xB8, /**< Recall EEPROM to scratchpad */
  k_ds18b20_cmd_read_power       = 0xB4, /**< Check power supply mode */
} ds18b20_command_t;

/**
 * @brief DS18B20 resolution settings (9-12 bits)
 */
typedef enum {
  k_ds18b20_resolution_9bit  = 9,  /**< 9-bit resolution (0.5C, 93.75ms) */
  k_ds18b20_resolution_10bit = 10, /**< 10-bit resolution (0.25C, 187.5ms) */
  k_ds18b20_resolution_11bit = 11, /**< 11-bit resolution (0.125C, 375ms) */
  k_ds18b20_resolution_12bit = 12, /**< 12-bit resolution (0.0625C, 750ms) */
} ds18b20_resolution_t;

/**
 * @brief DS18B20 conversion times in milliseconds
 */
typedef enum {
  k_ds18b20_conv_time_9bit_ms  = 94,  /**< 9-bit conversion time (93.75ms) */
  k_ds18b20_conv_time_10bit_ms = 188, /**< 10-bit conversion time (187.5ms) */
  k_ds18b20_conv_time_11bit_ms = 375, /**< 11-bit conversion time (375ms) */
  k_ds18b20_conv_time_12bit_ms = 750, /**< 12-bit conversion time (750ms) */
} ds18b20_conversion_time_t;

/**
 * @brief DS18B20 scratchpad byte indices
 */
typedef enum {
  k_ds18b20_scratch_temp_lsb   = 0, /**< Temperature LSB */
  k_ds18b20_scratch_temp_msb   = 1, /**< Temperature MSB */
  k_ds18b20_scratch_th_reg     = 2, /**< High alarm trigger (TH) / user byte 1 */
  k_ds18b20_scratch_tl_reg     = 3, /**< Low alarm trigger (TL) / user byte 2 */
  k_ds18b20_scratch_config     = 4, /**< Configuration register */
  k_ds18b20_scratch_reserved_1 = 5, /**< Reserved (0xFF) */
  k_ds18b20_scratch_reserved_2 = 6, /**< Reserved */
  k_ds18b20_scratch_reserved_3 = 7, /**< Reserved (0x10) */
  k_ds18b20_scratch_crc        = 8, /**< CRC-8 of bytes 0-7 */
} ds18b20_scratchpad_idx_t;

/**
 * @brief DS18B20 configuration register constants
 */
typedef enum {
  k_ds18b20_config_res_mask  = 0x60, /**< Resolution bits mask (bits 5-6) */
  k_ds18b20_config_res_shift = 5,    /**< Resolution bit shift */
  k_ds18b20_config_res_9bit  = 0x00, /**< 9-bit resolution (R0=0, R1=0) */
  k_ds18b20_config_res_10bit = 0x20, /**< 10-bit resolution (R0=1, R1=0) */
  k_ds18b20_config_res_11bit = 0x40, /**< 11-bit resolution (R0=0, R1=1) */
  k_ds18b20_config_res_12bit = 0x60, /**< 12-bit resolution (R0=1, R1=1) */
  k_ds18b20_config_reserved  = 0x1F, /**< Reserved bits (always 1) */
} ds18b20_config_constants_t;

/**
 * @brief DS18B20 temperature calculation constants
 */
typedef enum {
  k_ds18b20_temp_min_c = -55, /**< Minimum temperature in Celsius */
  k_ds18b20_temp_max_c = 125, /**< Maximum temperature in Celsius */
} ds18b20_temp_limits_t;

/**
 * @brief DS18B20 maximum devices per search
 */
typedef enum {
  k_ds18b20_max_devices = 16, /**< Maximum devices to search for */
} ds18b20_search_constants_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief DS18B20 sensor handle structure
 *
 * Contains all state needed to communicate with a specific DS18B20 sensor.
 * Must be initialized with rx_ds18b20_init() before use.
 */
typedef struct {
  rx_bus_manager_t* bus_manager;              /**< Bus manager instance */
  const char*       bus_name;                 /**< OneWire bus name */
  uint8_t           rom[k_ds18b20_rom_bytes]; /**< Device ROM address */
  uint8_t           resolution_bits;          /**< Current resolution (9-12 bits) */
  bool              parasitic_power;          /**< True if using parasitic power */
  bool              use_rom_match;            /**< True to use Match ROM (multi-sensor) */
  bool              initialized;              /**< True after successful init */
} rx_ds18b20_handle_t;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize DS18B20 sensor handle
 *
 * Initializes a DS18B20 handle for communication with a specific sensor.
 * If rom is NULL, the driver will use Skip ROM command (for single sensor
 * on bus). If rom is provided, Match ROM will be used to address the
 * specific device.
 *
 * This function:
 * 1. Initializes the OneWire bus if not already initialized
 * 2. Verifies device presence
 * 3. Reads the scratchpad to get current resolution
 * 4. Checks power supply mode (parasitic vs external)
 *
 * @param[out] handle DS18B20 handle to initialize
 * @param[in] bus_manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[in] rom 8-byte ROM address (NULL for Skip ROM mode)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle, bus_manager, or bus_name is NULL
 * @return k_rx_err_not_found if bus not found or no device present
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_crc_mismatch if scratchpad CRC fails
 * @return k_rx_err_hw_error if device does not respond
 */
rx_err_t rx_ds18b20_init(rx_ds18b20_handle_t* handle,
                         rx_bus_manager_t*    bus_manager,
                         const char*          bus_name,
                         const uint8_t*       rom);

/**
 * @brief Initialize DS18B20 by device index from search results
 *
 * Convenience function to initialize a sensor using an index into
 * the array of ROMs returned by rx_ds18b20_search_devices().
 *
 * @param[out] handle DS18B20 handle to initialize
 * @param[in] bus_manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[in] roms Array of ROM codes from search
 * @param[in] device_index Index of device to initialize (0-based)
 * @param[in] num_devices Total number of devices in roms array
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer parameter is NULL
 * @return k_rx_err_invalid_arg if device_index >= num_devices
 * @return Other errors from rx_ds18b20_init()
 */
rx_err_t rx_ds18b20_init_by_index(rx_ds18b20_handle_t* handle,
                                  rx_bus_manager_t*    bus_manager,
                                  const char*          bus_name,
                                  const uint8_t*       roms,
                                  uint32_t             device_index,
                                  uint32_t             num_devices);

/* =============================================================================
 * Temperature Conversion Functions
 * =============================================================================
 */

/**
 * @brief Start temperature conversion
 *
 * Initiates a temperature conversion. The conversion takes time depending
 * on resolution:
 * - 9-bit:  93.75 ms
 * - 10-bit: 187.5 ms
 * - 11-bit: 375 ms
 * - 12-bit: 750 ms
 *
 * After calling this function, wait for conversion to complete before
 * reading temperature. Use rx_ds18b20_get_conversion_time() to get the
 * required wait time for the current resolution.
 *
 * @param[in] handle Initialized DS18B20 handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if handle not initialized
 * @return k_rx_err_not_found if device not present
 * @return k_rx_err_hw_error if communication fails
 */
rx_err_t rx_ds18b20_start_conversion(rx_ds18b20_handle_t* handle);

/**
 * @brief Read temperature in Celsius
 *
 * Reads the most recent temperature conversion result from the sensor.
 * Must call rx_ds18b20_start_conversion() first and wait for conversion
 * to complete.
 *
 * Temperature is returned as a floating-point value in degrees Celsius.
 * Valid range is -55.0C to +125.0C.
 *
 * @param[in] handle Initialized DS18B20 handle
 * @param[out] temperature_c Pointer to store temperature in Celsius
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or temperature_c is NULL
 * @return k_rx_err_invalid_state if handle not initialized
 * @return k_rx_err_not_found if device not present
 * @return k_rx_err_crc_mismatch if scratchpad CRC fails
 * @return k_rx_err_range_check_failed if temperature out of valid range
 */
rx_err_t rx_ds18b20_read_temperature(rx_ds18b20_handle_t* handle, float* temperature_c);

/**
 * @brief Read temperature in Fahrenheit
 *
 * Convenience function that reads temperature and converts to Fahrenheit.
 * F = C * 9/5 + 32
 *
 * @param[in] handle Initialized DS18B20 handle
 * @param[out] temperature_f Pointer to store temperature in Fahrenheit
 *
 * @return Same as rx_ds18b20_read_temperature()
 */
rx_err_t rx_ds18b20_read_temperature_f(rx_ds18b20_handle_t* handle, float* temperature_f);

/**
 * @brief Read raw temperature value
 *
 * Returns the raw 16-bit temperature value from the scratchpad.
 * Each LSB represents 0.0625C (1/16 degree).
 *
 * @param[in] handle Initialized DS18B20 handle
 * @param[out] raw_temp Pointer to store raw 16-bit temperature
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or raw_temp is NULL
 * @return k_rx_err_invalid_state if handle not initialized
 * @return k_rx_err_crc_mismatch if scratchpad CRC fails
 */
rx_err_t rx_ds18b20_read_temperature_raw(rx_ds18b20_handle_t* handle, int16_t* raw_temp);

/* =============================================================================
 * Resolution Configuration
 * =============================================================================
 */

/**
 * @brief Set temperature resolution
 *
 * Configures the temperature resolution (9-12 bits). Higher resolution
 * provides more precision but takes longer to convert.
 *
 * Resolution | Precision | Conversion Time
 * -----------|-----------|----------------
 * 9-bit      | 0.5C      | 93.75 ms
 * 10-bit     | 0.25C     | 187.5 ms
 * 11-bit     | 0.125C    | 375 ms
 * 12-bit     | 0.0625C   | 750 ms
 *
 * @param[in,out] handle Initialized DS18B20 handle
 * @param[in] resolution_bits Resolution (9, 10, 11, or 12)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if handle not initialized
 * @return k_rx_err_invalid_arg if resolution_bits not 9-12
 * @return k_rx_err_not_found if device not present
 * @return k_rx_err_hw_error if communication fails
 */
rx_err_t rx_ds18b20_set_resolution(rx_ds18b20_handle_t* handle, uint8_t resolution_bits);

/**
 * @brief Get current temperature resolution
 *
 * Returns the currently configured resolution.
 *
 * @param[in] handle Initialized DS18B20 handle
 * @param[out] resolution_bits Pointer to store resolution (9-12)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or resolution_bits is NULL
 * @return k_rx_err_invalid_state if handle not initialized
 */
rx_err_t rx_ds18b20_get_resolution(const rx_ds18b20_handle_t* handle, uint8_t* resolution_bits);

/**
 * @brief Get conversion time for current resolution
 *
 * Returns the maximum conversion time in milliseconds for the current
 * resolution setting.
 *
 * @param[in] handle Initialized DS18B20 handle
 * @param[out] time_ms Pointer to store conversion time in milliseconds
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or time_ms is NULL
 * @return k_rx_err_invalid_state if handle not initialized
 */
rx_err_t rx_ds18b20_get_conversion_time(const rx_ds18b20_handle_t* handle, uint32_t* time_ms);

/* =============================================================================
 * Multi-Sensor Support
 * =============================================================================
 */

/**
 * @brief Search for all DS18B20 devices on the bus
 *
 * Performs a OneWire search to find all devices on the bus, filtering
 * for DS18B20 family code (0x28).
 *
 * @param[in] bus_manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[out] roms Array to store ROM codes (8 bytes each)
 * @param[in] max_devices Maximum number of devices to search for
 * @param[out] num_devices Pointer to store number of devices found
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_crc_mismatch if ROM CRC fails
 */
rx_err_t rx_ds18b20_search_devices(rx_bus_manager_t* bus_manager,
                                   const char*       bus_name,
                                   uint8_t*          roms,
                                   uint32_t          max_devices,
                                   uint32_t*         num_devices);

/* =============================================================================
 * Advanced Functions
 * =============================================================================
 */

/**
 * @brief Read sensor ROM code (single device only)
 *
 * Reads the 64-bit ROM code from the only device on the bus.
 * Only works when exactly one device is connected.
 *
 * @param[in] bus_manager Bus manager instance
 * @param[in] bus_name OneWire bus name
 * @param[out] rom 8-byte buffer for ROM code
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer is NULL
 * @return k_rx_err_not_found if bus not found or no device
 * @return k_rx_err_hw_error if multiple devices present
 * @return k_rx_err_crc_mismatch if ROM CRC fails
 */
rx_err_t rx_ds18b20_read_rom(rx_bus_manager_t* bus_manager, const char* bus_name, uint8_t* rom);

/**
 * @brief Check if sensor uses parasitic power
 *
 * The DS18B20 can be powered parasitically from the data line (2-wire mode)
 * or from an external supply (3-wire mode). This function detects the mode.
 *
 * @param[in] handle Initialized DS18B20 handle
 * @param[out] parasitic Pointer to store parasitic power flag
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or parasitic is NULL
 * @return k_rx_err_invalid_state if handle not initialized
 */
rx_err_t rx_ds18b20_is_parasitic_power(const rx_ds18b20_handle_t* handle, bool* parasitic);

/**
 * @brief Save current configuration to EEPROM
 *
 * Copies the current scratchpad configuration (TH, TL, config register)
 * to the sensor's EEPROM. Configuration will be restored on power-up.
 *
 * @param[in] handle Initialized DS18B20 handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if handle not initialized
 * @return k_rx_err_not_found if device not present
 */
rx_err_t rx_ds18b20_save_config(rx_ds18b20_handle_t* handle);

/**
 * @brief Recall configuration from EEPROM
 *
 * Reloads the TH, TL, and config register from EEPROM.
 *
 * @param[in] handle Initialized DS18B20 handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if handle not initialized
 * @return k_rx_err_not_found if device not present
 */
rx_err_t rx_ds18b20_recall_config(rx_ds18b20_handle_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_DS18B20_H */
