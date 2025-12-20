/**
 * @file star_bus_onewire.h
 * @brief 1-Wire protocol API for the bus manager
 * @details
 * Provides interface for Dallas/Maxim 1-Wire protocol operations through the bus manager.
 * Supports single-wire bidirectional communication with devices like DS18B20 temperature
 * sensors, ROM commands, device enumeration, and CRC-8 validation for data integrity.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_BUS_ONEWIRE_H
#define STAR_BUS_ONEWIRE_H

#include <driver/gpio.h>
#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_onewire.h
 * @brief 1-Wire controller protocol implementation
 *
 * This module implements the 1-Wire protocol for communication with
 * Dallas/Maxim 1-Wire devices. The protocol uses a single data line
 * for bidirectional communication.
 *
 * Features:
 * - Standard and overdrive speed modes
 * - ROM search algorithm for device discovery
 * - CRC-8 calculation and verification
 * - Parasite power support
 * - Strong pull-up for power-hungry devices
 * - Multi-drop bus support (up to 100+ devices)
 *
 * Supported Commands:
 * - Read ROM (0x33) - Read single device ROM
 * - Match ROM (0x55) - Address specific device
 * - Skip ROM (0xCC) - Address all devices
 * - Search ROM (0xF0) - Search for devices
 * - Alarm Search (0xEC) - Search for alarming devices
 *
 * Common Use Cases:
 * - DS18B20 temperature sensors
 * - DS2431/DS2433 EEPROM
 * - DS2401/DS2411 silicon serial numbers
 * - DS2890 digital potentiometers
 * - DS1990A iButton authentication
 *
 * Example:
 * @code
 * // Initialize 1-Wire bus
 * star_onewire_config_t config = STAR_ONEWIRE_CONFIG_DEFAULT();
 * config.gpio_pin = GPIO_NUM_4;
 * star_bus_onewire_init(manager, "onewire0", &config);
 *
 * // Search for devices
 * uint64_t rom_codes[10];
 * size_t count = 10;
 * star_bus_onewire_search(manager, "onewire0", rom_codes, &count);
 *
 * // Read temperature from DS18B20
 * uint8_t temp_cmd[] = {0x44}; // Convert T
 * star_bus_onewire_write_bytes(manager, "onewire0", rom_codes[0], temp_cmd, 1);
 * vTaskDelay(pdMS_TO_TICKS(750)); // Wait for conversion
 *
 * uint8_t read_cmd[] = {0xBE}; // Read scratchpad
 * star_bus_onewire_write_bytes(manager, "onewire0", rom_codes[0], read_cmd, 1);
 *
 * uint8_t scratchpad[9];
 * star_bus_onewire_read_bytes(manager, "onewire0", scratchpad, 9);
 * @endcode
 */

/* --- Constants --- */

/**
 * @brief OneWire size constants
 *
 * Using enum for type safety while maintaining compile-time constant behavior
 * required for array sizes in C.
 */
enum {
  /** 1-Wire ROM code size (8 bytes: 1 family + 6 serial + 1 CRC) */
  k_star_onewire_rom_size = 8,

  /** Maximum devices on a 1-Wire bus */
  k_star_onewire_max_devices = 100,
};

/**
 * @brief OneWire ROM commands
 */
typedef enum {
  k_star_onewire_cmd_read_rom     = 0x33, /**< Read ROM (single device) */
  k_star_onewire_cmd_match_rom    = 0x55, /**< Match ROM (address device) */
  k_star_onewire_cmd_skip_rom     = 0xCC, /**< Skip ROM (all devices) */
  k_star_onewire_cmd_search_rom   = 0xF0, /**< Search ROM (find devices) */
  k_star_onewire_cmd_alarm_search = 0xEC, /**< Alarm search */
} star_onewire_cmd_t;

/* --- Configuration Constants --- */

/**
 * @brief Timing parameters for 1-Wire standard speed (microseconds)
 *
 * These values define the critical timing requirements for 1-Wire
 * communication at standard speed. Values are based on the Dallas/Maxim
 * 1-Wire specification.
 */
typedef enum {
  k_timing_reset_pulse     = 480, /**< Reset pulse duration (us) */
  k_timing_presence_wait   = 70,  /**< Wait before sampling presence (us) */
  k_timing_presence_sample = 410, /**< Presence pulse sample time (us) */
  k_timing_write_0_low     = 60,  /**< Write 0 low pulse duration (us) */
  k_timing_write_1_low     = 6,   /**< Write 1 low pulse duration (us) */
  k_timing_write_recovery  = 10,  /**< Recovery time after write (us) */
  k_timing_read_low        = 6,   /**< Read initiation low pulse (us) */
  k_timing_read_sample     = 9,   /**< Read sample time after low (us) */
  k_timing_read_recovery   = 55,  /**< Recovery time after read (us) */
} onewire_timing_config_t;

/* --- Types --- */

/**
 * @brief 1-Wire speed mode
 */
typedef enum {
  k_star_onewire_speed_standard  = 0, /**< Standard speed (default) */
  k_star_onewire_speed_overdrive = 1, /**< Overdrive speed (faster) */
} star_onewire_speed_t;

/**
 * @brief 1-Wire ROM code (64-bit unique address)
 */
typedef uint64_t star_onewire_rom_t;

/**
 * @brief 1-Wire bus configuration
 */
typedef struct {
  gpio_num_t           gpio_pin;            /**< GPIO pin for 1-Wire data */
  star_onewire_speed_t speed;               /**< Speed mode */
  bool                 use_parasitic_power; /**< Enable parasite power */
  bool                 use_strong_pullup;   /**< Enable strong pull-up */
  uint32_t             search_timeout_ms;   /**< Search timeout */
} star_onewire_config_t;

/**
 * @brief 1-Wire bus statistics
 */
typedef struct {
  uint64_t total_resets;           /**< Total reset pulses sent */
  uint64_t successful_resets;      /**< Successful resets (device present) */
  uint64_t failed_resets;          /**< Failed resets (no device) */
  uint64_t bytes_written;          /**< Total bytes written */
  uint64_t bytes_read;             /**< Total bytes read */
  uint64_t crc_errors;             /**< CRC verification errors */
  uint64_t search_operations;      /**< Number of search operations */
  uint32_t devices_found;          /**< Devices found in last search */
  uint32_t last_operation_time_us; /**< Last operation time */
} star_onewire_stats_t;

/* --- Bus Initialization --- */

/**
 * @brief Initialize 1-Wire bus
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name for this 1-Wire bus
 * @param[in] config 1-Wire configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_init(star_bus_manager_t*          manager,
                                const char*                  bus_name,
                                const star_onewire_config_t* config);

/**
 * @brief Deinitialize 1-Wire bus
 *
 * @param[in] manager Pointer to bus manager (can be NULL, parameter kept for API consistency)
 * @param[in] bus_name Name of 1-Wire bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_deinit(star_bus_manager_t* manager, const char* bus_name);

/* --- Basic Operations --- */

/**
 * @brief Send reset pulse and detect device presence
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of 1-Wire bus
 * @param[out] present True if device detected
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_reset(star_bus_manager_t* manager, const char* bus_name, bool* present);

/**
 * @brief Write a single bit
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 * @param[in] bit Bit value (0 or 1)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bus_onewire_write_bit(star_bus_manager_t* manager, const char* bus_name, uint8_t bit);

/**
 * @brief Read a single bit
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of 1-Wire bus
 * @param[out] bit Pointer to store bit value
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bus_onewire_read_bit(star_bus_manager_t* manager, const char* bus_name, uint8_t* bit);

/**
 * @brief Write a byte
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 * @param[in] byte Byte to write
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bus_onewire_write_byte(star_bus_manager_t* manager, const char* bus_name, uint8_t byte);

/**
 * @brief Read a byte
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of 1-Wire bus
 * @param[out] byte Pointer to store byte
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bus_onewire_read_byte(star_bus_manager_t* manager, const char* bus_name, uint8_t* byte);

/**
 * @brief Write multiple bytes
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 * @param[in] rom ROM code to address (0 to skip ROM command)
 * @param[in] data Data to write
 * @param[in] length Number of bytes to write
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_write_bytes(star_bus_manager_t* manager,
                                       const char*         bus_name,
                                       star_onewire_rom_t  rom,
                                       const uint8_t*      data,
                                       size_t              length);

/**
 * @brief Read multiple bytes
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of 1-Wire bus
 * @param[out] data Buffer to store data
 * @param[in]  length Number of bytes to read
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_read_bytes(star_bus_manager_t* manager,
                                      const char*         bus_name,
                                      uint8_t*            data,
                                      size_t              length);

/* --- Device Discovery --- */

/**
 * @brief Search for all devices on the bus
 *
 * Implements the 1-Wire search algorithm to find all devices.
 *
 * @param[in]     manager Pointer to initialized bus manager
 * @param[in]     bus_name Name of 1-Wire bus
 * @param[out]    rom_codes Array to store ROM codes
 * @param[in,out] count On entry: array size. On exit: devices found
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_search(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  star_onewire_rom_t* rom_codes,
                                  size_t*             count);

/**
 * @brief Search for devices with active alarms
 *
 * @param[in]     manager Pointer to initialized bus manager
 * @param[in]     bus_name Name of 1-Wire bus
 * @param[out]    rom_codes Array to store ROM codes
 * @param[in,out] count On entry: array size. On exit: devices found
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_search_alarms(star_bus_manager_t* manager,
                                         const char*         bus_name,
                                         star_onewire_rom_t* rom_codes,
                                         size_t*             count);

/**
 * @brief Read ROM from single device on bus
 *
 * Only works when exactly one device is present.
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of 1-Wire bus
 * @param[out] rom Pointer to store ROM code (8-byte array)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bus_onewire_read_rom(star_bus_manager_t* manager, const char* bus_name, uint8_t rom[8]);

/**
 * @brief Send MATCH ROM command to address a specific device
 *
 * Sends the MATCH ROM command (0x55) followed by the 8-byte ROM code
 * to select a specific device on a multi-drop bus.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 * @param[in] rom 8-byte ROM code of device to address
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bus_onewire_match_rom(star_bus_manager_t* manager, const char* bus_name, const uint8_t rom[8]);

/**
 * @brief Send SKIP ROM command to address all devices
 *
 * Sends the SKIP ROM command (0xCC) to address all devices on the bus.
 * Only use when a single device is present or when broadcasting to all devices.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_skip_rom(star_bus_manager_t* manager, const char* bus_name);

/* --- CRC Operations --- */

/**
 * @brief Calculate CRC-8 for data
 *
 * Uses Dallas/Maxim CRC-8 polynomial: x^8 + x^5 + x^4 + 1
 *
 * @param[in] data Data to calculate CRC for
 * @param[in] length Data length
 *
 * @return CRC-8 value
 */
uint8_t star_bus_onewire_crc8(const uint8_t* data, size_t length);

/**
 * @brief Verify ROM code CRC
 *
 * @param[in] rom ROM code to verify
 *
 * @return true if CRC is valid, false otherwise
 */
bool star_bus_onewire_verify_rom(star_onewire_rom_t rom);

/**
 * @brief Calculate CRC-16 for data
 *
 * Used for extended operations (EEPROM, etc.)
 *
 * @param[in] data Data to calculate CRC for
 * @param[in] length Data length
 *
 * @return CRC-16 value
 */
uint16_t star_bus_onewire_crc16(const uint8_t* data, size_t length);

/* --- ROM Utilities --- */

/**
 * @brief Extract family code from ROM
 *
 * @param[in] rom ROM code
 *
 * @return Family code (first byte)
 */
static inline uint8_t star_bus_onewire_get_family(star_onewire_rom_t rom)
{
  return (uint8_t)(rom & 0xFF);
}

/**
 * @brief Extract CRC from ROM
 *
 * @param[in] rom ROM code
 *
 * @return CRC (last byte)
 */
static inline uint8_t star_bus_onewire_get_crc(star_onewire_rom_t rom)
{
  return (uint8_t)((rom >> 56) & 0xFF);
}

/**
 * @brief Convert ROM to byte array
 *
 * @param[in]  rom ROM code
 * @param[out] bytes 8-byte array
 */
void star_bus_onewire_rom_to_bytes(star_onewire_rom_t rom, uint8_t bytes[8]);

/**
 * @brief Convert byte array to ROM
 *
 * @param[in] bytes 8-byte array
 *
 * @return ROM code
 */
star_onewire_rom_t star_bus_onewire_bytes_to_rom(const uint8_t bytes[8]);

/* --- Statistics --- */

/**
 * @brief Get 1-Wire bus statistics
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of 1-Wire bus
 * @param[out] stats Pointer to store statistics
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_get_stats(const star_bus_manager_t* manager,
                                     const char*               bus_name,
                                     star_onewire_stats_t*     stats);

/**
 * @brief Reset 1-Wire bus statistics
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_reset_stats(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Print 1-Wire statistics
 *
 * @param[in] bus_name Name of 1-Wire bus
 * @param[in] stats Pointer to statistics
 */

/* --- Helper Macros --- */

/**
 * @brief Create default 1-Wire configuration
 */
#define STAR_ONEWIRE_CONFIG_DEFAULT()                                                              \
  {                                                                                                \
    .gpio_pin            = GPIO_NUM_NC,                                                            \
    .speed               = k_star_onewire_speed_standard,                                          \
    .use_parasitic_power = false,                                                                  \
    .use_strong_pullup   = false,                                                                  \
    .search_timeout_ms   = 5000,                                                                   \
  }

/**
 * @brief Common device family codes
 */
typedef enum {
  k_star_onewire_family_ds18s20 = 0x10, /**< Temperature sensor */
  k_star_onewire_family_ds1822  = 0x22, /**< Temperature sensor */
  k_star_onewire_family_ds2433  = 0x23, /**< 4K EEPROM */
  k_star_onewire_family_ds2438  = 0x26, /**< Smart battery monitor */
  k_star_onewire_family_ds18b20 = 0x28, /**< Temperature sensor */
  k_star_onewire_family_ds2431  = 0x2D, /**< 1K EEPROM */
  k_star_onewire_family_ds2401  = 0x01, /**< Silicon serial number */
  k_star_onewire_family_ds2411  = 0x01, /**< Silicon serial number */
  k_star_onewire_family_ds1990a = 0x01, /**< iButton */
} star_onewire_family_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_ONEWIRE_H */
