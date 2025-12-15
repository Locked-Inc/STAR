/* esp32-firmware/components/star_bus/include/star_bus_onewire_peripheral.h */

#ifndef STAR_BUS_ONEWIRE_PERIPHERAL_H
#define STAR_BUS_ONEWIRE_PERIPHERAL_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"
#include "star_bus_onewire.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_onewire_peripheral.h
 * @brief 1-Wire peripheral (slave) mode implementation
 *
 * This module enables the ESP32 to act as a 1-Wire peripheral device,
 * responding to controller requests. Useful for emulating sensors or creating
 * custom 1-Wire devices.
 *
 * Features:
 * - Configurable ROM code (family + serial number)
 * - Automatic CRC calculation
 * - Callback-based data handling
 * - Function command support
 * - Parasite power detection
 * - Statistics tracking
 *
 * Use Cases:
 * - Emulating DS18B20 temperature sensors
 * - Creating custom 1-Wire sensors
 * - 1-Wire protocol testing
 * - Building 1-Wire authentication devices
 *
 * Example:
 * @code
 * // Read callback - controller reading from us
 * static esp_err_t on_read(uint8_t function_cmd, uint8_t* data,
 *                          size_t* length, void* ctx) {
 *   if (function_cmd == 0x44) { // Convert T
 *     // Return temperature data
 *     data[0] = 0x50; data[1] = 0x05; // 85C
 *     *length = 2;
 *   }
 *   return ESP_OK;
 * }
 *
 * // Setup peripheral
 * star_onewire_peripheral_config_t config = {0};
 * config.rom_code = 0x1234567890ABCD28ULL; // DS18B20 family
 * config.on_read = on_read;
 * star_bus_onewire_peripheral_enable(manager, "onewire0", &config);
 * @endcode
 */

/* --- Types --- */

/**
 * @brief 1-Wire peripheral read callback
 *
 * Called when controller reads data from peripheral after function command.
 *
 * @param[in]     function_cmd Function command received
 * @param[out]    data Buffer to write response data
 * @param[in,out] length On entry: buffer size. On exit: bytes written
 * @param[in]     context User context
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*star_onewire_peripheral_read_cb_t)(uint8_t  function_cmd,
                                                       uint8_t* data,
                                                       size_t*  length,
                                                       void*    context);

/**
 * @brief 1-Wire peripheral write callback
 *
 * Called when controller writes data to peripheral after function command.
 *
 * @param[in] function_cmd Function command received
 * @param[in] data Data received from controller
 * @param[in] length Number of bytes received
 * @param[in] context User context
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*star_onewire_peripheral_write_cb_t)(uint8_t        function_cmd,
                                                        const uint8_t* data,
                                                        size_t         length,
                                                        void*          context);

/**
 * @brief 1-Wire peripheral configuration
 */
typedef struct {
  star_onewire_rom_t rom_code;           /**< ROM code (family + serial + CRC) */
  bool               auto_calculate_crc; /**< Auto-calculate ROM CRC */

  /* Callbacks */
  star_onewire_peripheral_read_cb_t  on_read;  /**< Read callback (required) */
  star_onewire_peripheral_write_cb_t on_write; /**< Write callback (required) */
  void*                              context;  /**< User context for callbacks */

  /* Options */
  bool    support_overdrive; /**< Support overdrive speed */
  bool    support_alarm;     /**< Support alarm search */
  uint8_t alarm_flags;       /**< Alarm flags (device-specific) */
} star_onewire_peripheral_config_t;

/**
 * @brief 1-Wire peripheral statistics
 */
typedef struct {
  uint64_t  reset_pulses_detected; /**< Reset pulses detected */
  uint64_t  rom_reads;             /**< ROM read operations */
  uint64_t  match_rom_commands;    /**< Match ROM commands */
  uint64_t  skip_rom_commands;     /**< Skip ROM commands */
  uint64_t  search_rom_commands;   /**< Search ROM commands */
  uint64_t  alarm_search_commands; /**< Alarm search commands */
  uint64_t  function_commands;     /**< Function commands received */
  uint64_t  bytes_sent;            /**< Total bytes sent to controller */
  uint64_t  bytes_received;        /**< Total bytes received from controller */
  uint64_t  callback_errors;       /**< Errors from callbacks */
  esp_err_t last_error;            /**< Last error code */
} star_onewire_peripheral_stats_t;

/* --- Peripheral Management --- */

/**
 * @brief Enable 1-Wire peripheral mode
 *
 * Configures the bus to operate in peripheral mode, responding to controller requests.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 * @param[in] config Peripheral configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Peripheral mode is mutually exclusive with controller mode
 * @note on_read and on_write callbacks are required
 */
esp_err_t star_bus_onewire_peripheral_enable(star_bus_manager_t*                     manager,
                                             const char*                             bus_name,
                                             const star_onewire_peripheral_config_t* config);

/**
 * @brief Disable 1-Wire peripheral mode
 *
 * Returns the bus to controller mode.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_peripheral_disable(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Check if 1-Wire peripheral mode is enabled
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of 1-Wire bus
 * @param[out] enabled Pointer to store enabled status
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_peripheral_is_enabled(const star_bus_manager_t* manager,
                                                 const char*               bus_name,
                                                 bool*                     enabled);

/* --- ROM Management --- */

/**
 * @brief Update peripheral ROM code
 *
 * Allows changing the ROM code at runtime.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 * @param[in] rom_code New ROM code
 * @param[in] auto_calculate_crc Auto-calculate CRC byte
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_peripheral_set_rom(star_bus_manager_t* manager,
                                              const char*         bus_name,
                                              star_onewire_rom_t  rom_code,
                                              bool                auto_calculate_crc);

/**
 * @brief Get current peripheral ROM code
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of 1-Wire bus
 * @param[out] rom_code Pointer to store ROM code
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_peripheral_get_rom(const star_bus_manager_t* manager,
                                              const char*               bus_name,
                                              star_onewire_rom_t*       rom_code);

/* --- Alarm Management --- */

/**
 * @brief Set alarm condition
 *
 * Sets the alarm flag that will be reported during alarm search.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 * @param[in] alarm_active True to activate alarm
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_peripheral_set_alarm(star_bus_manager_t* manager,
                                                const char*         bus_name,
                                                bool                alarm_active);

/* --- Statistics --- */

/**
 * @brief Get peripheral statistics
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of 1-Wire bus
 * @param[out] stats Pointer to store statistics
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_peripheral_get_stats(const star_bus_manager_t*        manager,
                                                const char*                      bus_name,
                                                star_onewire_peripheral_stats_t* stats);

/**
 * @brief Reset peripheral statistics
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of 1-Wire bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_onewire_peripheral_reset_stats(star_bus_manager_t* manager,
                                                  const char*         bus_name);

/**
 * @brief Print peripheral statistics
 *
 * @param[in] bus_name Name of 1-Wire bus
 * @param[in] stats Pointer to statistics
 */
void star_bus_onewire_peripheral_print_stats(const char*                            bus_name,
                                             const star_onewire_peripheral_stats_t* stats);

/* --- Helper Macros --- */

/**
 * @brief Create ROM code with auto-calculated CRC
 *
 * @param family Family code (8 bits)
 * @param serial Serial number (48 bits)
 */
#define STAR_ONEWIRE_MAKE_ROM(family, serial)                                                      \
  ((star_onewire_rom_t)(((uint64_t)(family) & 0xFF) |                                              \
                        (((uint64_t)(serial) & 0xFFFFFFFFFFFFULL) << 8)))

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_ONEWIRE_PERIPHERAL_H */
