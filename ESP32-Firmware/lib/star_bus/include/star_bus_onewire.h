/* esp32-firmware/components/star_bus/include/star_bus_onewire.h */

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
 * @brief 1-Wire master protocol implementation
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

/** 1-Wire ROM code size (8 bytes: 1 family + 6 serial + 1 CRC) */
#define STAR_ONEWIRE_ROM_SIZE (8)

/** Maximum devices on a 1-Wire bus */
#define STAR_ONEWIRE_MAX_DEVICES (100)

/* 1-Wire ROM Commands */
#define STAR_ONEWIRE_CMD_READ_ROM (0x33)     /**< Read ROM (single device) */
#define STAR_ONEWIRE_CMD_MATCH_ROM (0x55)    /**< Match ROM (address device) */
#define STAR_ONEWIRE_CMD_SKIP_ROM (0xCC)     /**< Skip ROM (all devices) */
#define STAR_ONEWIRE_CMD_SEARCH_ROM (0xF0)   /**< Search ROM (find devices) */
#define STAR_ONEWIRE_CMD_ALARM_SEARCH (0xEC) /**< Alarm search */

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
 * @param[in] manager Pointer to initialized bus manager
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
 * @param[out] rom Pointer to store ROM code
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_read_rom(star_bus_manager_t* manager,
                                    const char*         bus_name,
                                    star_onewire_rom_t* rom);

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
void star_bus_onewire_print_stats(const char* bus_name, const star_onewire_stats_t* stats);

/* --- Helper Macros --- */

/**
 * @brief Create default 1-Wire configuration
 */
#define STAR_ONEWIRE_CONFIG_DEFAULT()                                                              \
  {                                                                                                \
    .gpio_pin            = GPIO_NUM_NC,                                                            \
    .speed               = STAR_ONEWIRE_SPEED_STANDARD,                                            \
    .use_parasitic_power = false,                                                                  \
    .use_strong_pullup   = false,                                                                  \
    .search_timeout_ms   = 5000,                                                                   \
  }

/**
 * @brief Common device family codes
 */
#define STAR_ONEWIRE_FAMILY_DS18S20 (0x10) /**< Temperature sensor */
#define STAR_ONEWIRE_FAMILY_DS18B20 (0x28) /**< Temperature sensor */
#define STAR_ONEWIRE_FAMILY_DS1822 (0x22)  /**< Temperature sensor */
#define STAR_ONEWIRE_FAMILY_DS2431 (0x2D)  /**< 1K EEPROM */
#define STAR_ONEWIRE_FAMILY_DS2433 (0x23)  /**< 4K EEPROM */
#define STAR_ONEWIRE_FAMILY_DS2401 (0x01)  /**< Silicon serial number */
#define STAR_ONEWIRE_FAMILY_DS2411 (0x01)  /**< Silicon serial number */
#define STAR_ONEWIRE_FAMILY_DS1990A (0x01) /**< iButton */

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_ONEWIRE_H */
