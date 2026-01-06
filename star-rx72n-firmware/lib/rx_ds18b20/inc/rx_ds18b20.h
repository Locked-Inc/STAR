/* lib/rx_ds18b20/inc/rx_ds18b20.h */

/**
 * @file rx_ds18b20.h
 * @brief DS18B20 1-Wire Digital Temperature Sensor Driver
 * @details
 * Driver for Maxim DS18B20 programmable resolution 1-Wire digital thermometer.
 *
 * Features:
 * - Temperature range: -55°C to +125°C
 * - Programmable resolution: 9, 10, 11, or 12 bits
 * - Unique 64-bit serial code (ROM)
 * - Parasitic or external power
 * - CRC-8 data integrity check
 *
 * Conversion times by resolution:
 * - 9-bit:  93.75ms (0.5°C resolution)
 * - 10-bit: 187.5ms (0.25°C resolution)
 * - 11-bit: 375ms (0.125°C resolution)
 * - 12-bit: 750ms (0.0625°C resolution)
 *
 * Dependency Injection:
 * Uses bus manager abstraction for hardware independence.
 * Fully testable with mock bus implementation.
 *
 * Example Usage:
 * @code
 * // Initialize DS18B20
 * rx_ds18b20_handle_t sensor;
 * rx_ds18b20_config_t config = {
 *     .bus_manager = &bus_mgr,
 *     .bus_name = "onewire0",
 *     .resolution = k_ds18b20_resolution_12bit,
 *     .use_rom_matching = false,  // Single device, use skip ROM
 * };
 * rx_ds18b20_init(&sensor, &config);
 *
 * // Trigger conversion and read temperature
 * rx_ds18b20_trigger_conversion(&sensor);
 * tx_thread_sleep(75);  // Wait 750ms for 12-bit conversion
 *
 * float temp_c;
 * rx_ds18b20_read_temperature(&sensor, &temp_c);
 * @endcode
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_DS18B20_H
#define STAR_RX_DS18B20_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_bus_onewire.h"
#include "rx_err.h"

/* =============================================================================
 * DS18B20 Constants
 * =============================================================================
 */

/**
 * @brief DS18B20 family code (first byte of ROM)
 */
typedef enum {
  k_ds18b20_family_code = 0x28, /**< DS18B20 family code in ROM */
} ds18b20_family_code_t;

/**
 * @brief DS18B20 function commands
 */
typedef enum {
  k_ds18b20_cmd_convert_t      = 0x44, /**< Trigger temperature conversion */
  k_ds18b20_cmd_write_scratch  = 0x4E, /**< Write to scratchpad memory */
  k_ds18b20_cmd_read_scratch   = 0xBE, /**< Read from scratchpad memory */
  k_ds18b20_cmd_copy_scratch   = 0x48, /**< Copy scratchpad to EEPROM */
  k_ds18b20_cmd_recall_eeprom  = 0xB8, /**< Recall EEPROM to scratchpad */
  k_ds18b20_cmd_read_power     = 0xB4, /**< Read power supply mode */
} ds18b20_command_t;

/**
 * @brief DS18B20 scratchpad memory layout
 */
typedef enum {
  k_ds18b20_scratch_temp_lsb  = 0, /**< Temperature LSB */
  k_ds18b20_scratch_temp_msb  = 1, /**< Temperature MSB */
  k_ds18b20_scratch_th_reg    = 2, /**< TH (high alarm) register */
  k_ds18b20_scratch_tl_reg    = 3, /**< TL (low alarm) register */
  k_ds18b20_scratch_config    = 4, /**< Configuration register */
  k_ds18b20_scratch_reserved1 = 5, /**< Reserved (0xFF) */
  k_ds18b20_scratch_reserved2 = 6, /**< Reserved (factory) */
  k_ds18b20_scratch_reserved3 = 7, /**< Reserved (factory) */
  k_ds18b20_scratch_crc       = 8, /**< CRC-8 of bytes 0-7 */
  k_ds18b20_scratchpad_bytes  = 9, /**< Total scratchpad size */
} ds18b20_scratchpad_index_t;

/**
 * @brief DS18B20 configuration register bit positions
 */
typedef enum {
  k_ds18b20_config_r0_bit = 5, /**< Resolution bit 0 */
  k_ds18b20_config_r1_bit = 6, /**< Resolution bit 1 */
} ds18b20_config_bits_t;

/**
 * @brief DS18B20 temperature resolution modes
 */
typedef enum {
  k_ds18b20_resolution_9bit  = 0, /**< 9-bit: 0.5°C, 93.75ms */
  k_ds18b20_resolution_10bit = 1, /**< 10-bit: 0.25°C, 187.5ms */
  k_ds18b20_resolution_11bit = 2, /**< 11-bit: 0.125°C, 375ms */
  k_ds18b20_resolution_12bit = 3, /**< 12-bit: 0.0625°C, 750ms */
} ds18b20_resolution_t;

/**
 * @brief DS18B20 conversion times (milliseconds)
 *
 * Maximum conversion time for each resolution.
 * Add margin for safety (spec values + 10%).
 */
typedef enum {
  k_ds18b20_conv_time_9bit_ms  = 100, /**< 9-bit conversion: 100ms */
  k_ds18b20_conv_time_10bit_ms = 200, /**< 10-bit conversion: 200ms */
  k_ds18b20_conv_time_11bit_ms = 400, /**< 11-bit conversion: 400ms */
  k_ds18b20_conv_time_12bit_ms = 800, /**< 12-bit conversion: 800ms */
} ds18b20_conversion_time_ms_t;

/**
 * @brief DS18B20 temperature masks for resolution
 *
 * Lower bits are undefined for resolutions < 12-bit and must be masked.
 * This prevents garbage bits from corrupting the temperature reading.
 */
typedef enum {
  k_ds18b20_temp_mask_9bit  = 0xFFF8, /**< Mask for 9-bit (discard bits 0-2) */
  k_ds18b20_temp_mask_10bit = 0xFFFC, /**< Mask for 10-bit (discard bits 0-1) */
  k_ds18b20_temp_mask_11bit = 0xFFFE, /**< Mask for 11-bit (discard bit 0) */
  k_ds18b20_temp_mask_12bit = 0xFFFF, /**< Mask for 12-bit (all bits valid) */
} ds18b20_temp_mask_t;

/**
 * @brief DS18B20 temperature conversion constants
 */
typedef enum {
  k_ds18b20_temp_shift       = 4,      /**< Shift to get integer temperature */
  k_ds18b20_sign_bit         = 0x8000, /**< Sign bit in 16-bit temperature */
  k_ds18b20_crc_bytes        = 8,      /**< Number of bytes for CRC calculation */
  k_ds18b20_shift_byte       = 8,      /**< Shift for byte positioning */
} ds18b20_conversion_constants_t;

/**
 * @brief DS18B20 power supply modes
 */
typedef enum {
  k_ds18b20_power_parasitic = false, /**< Parasitic power mode */
  k_ds18b20_power_external  = true,  /**< External power mode */
} ds18b20_power_mode_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief DS18B20 configuration structure
 */
typedef struct {
  rx_bus_manager_t*    bus_manager;       /**< Bus manager instance (required) */
  const char*          bus_name;          /**< OneWire bus name (required) */
  ds18b20_resolution_t resolution;        /**< Temperature resolution */
  bool                 use_rom_matching;  /**< Use ROM matching (true) or skip ROM (false) */
  uint8_t              rom[k_onewire_rom_bytes]; /**< Device ROM (if use_rom_matching=true) */
} rx_ds18b20_config_t;

/**
 * @brief DS18B20 driver handle structure
 */
typedef struct {
  rx_bus_manager_t*    bus_manager;   /**< Bus manager reference (not owned) */
  const char*          bus_name;      /**< OneWire bus name (not owned) */
  ds18b20_resolution_t resolution;    /**< Current temperature resolution */
  bool                 use_rom_matching; /**< ROM matching enabled */
  uint8_t              rom[k_onewire_rom_bytes]; /**< Device ROM code */
  bool                 initialized;   /**< True after successful init */
} rx_ds18b20_handle_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize DS18B20 sensor
 *
 * Initializes the DS18B20 sensor and configures resolution.
 * If ROM matching is disabled, uses Skip ROM (single device mode).
 *
 * @param[out] handle Pointer to DS18B20 handle. Must not be NULL.
 * @param[in] config Pointer to configuration. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or config is NULL
 * @return k_rx_err_invalid_arg if bus_manager or bus_name is NULL
 * @return k_rx_err_invalid_state if device not responding
 * @return k_rx_err_crc if scratchpad CRC check fails
 */
rx_err_t rx_ds18b20_init(rx_ds18b20_handle_t* handle, const rx_ds18b20_config_t* config);

/**
 * @brief Deinitialize DS18B20 sensor
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_ds18b20_deinit(rx_ds18b20_handle_t* handle);

/**
 * @brief Trigger temperature conversion
 *
 * Sends Convert T command to start temperature measurement.
 * Caller must wait for conversion time before reading temperature.
 *
 * Conversion times:
 * - 9-bit: 100ms
 * - 10-bit: 200ms
 * - 11-bit: 400ms
 * - 12-bit: 800ms
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized or device not present
 */
rx_err_t rx_ds18b20_trigger_conversion(rx_ds18b20_handle_t* handle);

/**
 * @brief Read temperature in Celsius
 *
 * Reads the scratchpad and converts raw temperature to Celsius.
 * Temperature conversion must complete before calling (see conversion times).
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 * @param[out] temperature_c Pointer to store temperature in °C. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer is NULL
 * @return k_rx_err_invalid_state if not initialized or device not present
 * @return k_rx_err_crc if scratchpad CRC check fails
 */
rx_err_t rx_ds18b20_read_temperature(rx_ds18b20_handle_t* handle, float* temperature_c);

/**
 * @brief Read raw temperature value
 *
 * Reads the raw 16-bit temperature from scratchpad without conversion.
 * Useful for debugging or custom temperature processing.
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 * @param[out] raw_temp Pointer to store raw temperature. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer is NULL
 * @return k_rx_err_invalid_state if not initialized or device not present
 * @return k_rx_err_crc if scratchpad CRC check fails
 */
rx_err_t rx_ds18b20_read_temperature_raw(rx_ds18b20_handle_t* handle, int16_t* raw_temp);

/**
 * @brief Set temperature resolution
 *
 * Changes the sensor resolution (9, 10, 11, or 12 bits).
 * Higher resolution provides more precision but takes longer to convert.
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 * @param[in] resolution Resolution mode (9-12 bits)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_arg if resolution out of range
 * @return k_rx_err_invalid_state if not initialized or device not present
 */
rx_err_t rx_ds18b20_set_resolution(rx_ds18b20_handle_t* handle, ds18b20_resolution_t resolution);

/**
 * @brief Get current temperature resolution
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 * @param[out] resolution Pointer to store resolution. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_ds18b20_get_resolution(const rx_ds18b20_handle_t* handle,
                                   ds18b20_resolution_t*      resolution);

/**
 * @brief Save configuration to EEPROM
 *
 * Copies scratchpad bytes 2-4 (TH, TL, Config) to non-volatile EEPROM.
 * Values persist across power cycles.
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized or device not present
 */
rx_err_t rx_ds18b20_save_config(rx_ds18b20_handle_t* handle);

/**
 * @brief Recall configuration from EEPROM
 *
 * Restores scratchpad bytes 2-4 (TH, TL, Config) from EEPROM.
 * Useful after power-up to restore saved settings.
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized or device not present
 */
rx_err_t rx_ds18b20_recall_config(rx_ds18b20_handle_t* handle);

/**
 * @brief Read power supply mode
 *
 * Checks if sensor is using parasitic power or external power.
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 * @param[out] external_power True if external power, false if parasitic. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer is NULL
 * @return k_rx_err_invalid_state if not initialized or device not present
 */
rx_err_t rx_ds18b20_read_power_mode(rx_ds18b20_handle_t* handle, bool* external_power);

/**
 * @brief Read scratchpad memory
 *
 * Reads the entire 9-byte scratchpad (8 data bytes + 1 CRC byte).
 * Performs CRC validation.
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 * @param[out] scratchpad Pointer to 9-byte buffer. Must not be NULL.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer is NULL
 * @return k_rx_err_invalid_state if not initialized or device not present
 * @return k_rx_err_crc if CRC check fails
 */
rx_err_t rx_ds18b20_read_scratchpad(rx_ds18b20_handle_t* handle,
                                    uint8_t              scratchpad[k_ds18b20_scratchpad_bytes]);

/**
 * @brief Get conversion time for current resolution
 *
 * Returns the maximum conversion time in milliseconds for the configured resolution.
 * Caller should wait this long after triggering conversion before reading temperature.
 *
 * @param[in] handle Pointer to initialized handle. Must not be NULL.
 *
 * @return Conversion time in milliseconds, or 0 if handle is NULL or not initialized
 */
uint32_t rx_ds18b20_get_conversion_time_ms(const rx_ds18b20_handle_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_DS18B20_H */
