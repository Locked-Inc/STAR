/* esp32-firmware/components/star_bus/include/star_bus_smbus_peripheral.h */

#ifndef STAR_BUS_SMBUS_PERIPHERAL_H
#define STAR_BUS_SMBUS_PERIPHERAL_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_smbus_peripheral.h
 * @brief SMBus peripheral (slave) mode implementation
 *
 * This module enables the ESP32 to act as an SMBus peripheral device,
 * responding to master requests. Useful for emulating SMBus devices
 * like batteries, fans, temperature sensors, etc.
 *
 * Features:
 * - 7-bit SMBus addressing
 * - Callback-based command handling
 * - Support for all SMBus commands
 * - PEC (Packet Error Code) support
 * - Alert response address support
 * - Statistics tracking
 *
 * Supported Commands:
 * - Quick Command
 * - Send/Receive Byte
 * - Read/Write Byte
 * - Read/Write Word
 * - Read/Write Block
 * - Process Call
 * - Block Process Call
 *
 * Use Cases:
 * - Emulating smart batteries (SBS)
 * - Fan controllers
 * - Temperature sensors
 * - PMBus power supplies (peripheral side)
 * - Custom SMBus devices
 *
 * Example:
 * @code
 * // Read callback - master reading register
 * static esp_err_t on_read(uint8_t cmd, uint8_t* data, size_t* length, void* ctx) {
 *   if (cmd == 0x00) { // Temperature
 *     uint16_t temp = read_temperature();
 *     data[0] = temp & 0xFF;
 *     data[1] = (temp >> 8) & 0xFF;
 *     *length = 2;
 *   }
 *   return ESP_OK;
 * }
 *
 * star_smbus_peripheral_config_t config = {0};
 * config.address = 0x48;
 * config.on_read_byte = on_read;
 * star_bus_smbus_peripheral_enable(manager, "i2c0", &config);
 * @endcode
 */

/* --- Types --- */

/**
 * @brief SMBus peripheral quick command callback
 *
 * @param[in] value Quick command value (0 or 1)
 * @param[in] context User context
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*star_smbus_peripheral_quick_cb_t)(uint8_t value, void* context);

/**
 * @brief SMBus peripheral receive byte callback
 *
 * @param[out] byte Pointer to store received byte
 * @param[in]  context User context
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*star_smbus_peripheral_receive_byte_cb_t)(uint8_t* byte, void* context);

/**
 * @brief SMBus peripheral send byte callback
 *
 * @param[in] byte Byte to send
 * @param[in] context User context
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*star_smbus_peripheral_send_byte_cb_t)(uint8_t byte, void* context);

/**
 * @brief SMBus peripheral read byte/word/block callback
 *
 * @param[in]     command Command code
 * @param[out]    data Buffer to store data
 * @param[in,out] length On entry: buffer size. On exit: bytes written
 * @param[in]     context User context
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*star_smbus_peripheral_read_cb_t)(uint8_t  command,
                                                     uint8_t* data,
                                                     size_t*  length,
                                                     void*    context);

/**
 * @brief SMBus peripheral write byte/word/block callback
 *
 * @param[in] command Command code
 * @param[in] data Data written
 * @param[in] length Number of bytes
 * @param[in] context User context
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*star_smbus_peripheral_write_cb_t)(uint8_t        command,
                                                      const uint8_t* data,
                                                      size_t         length,
                                                      void*          context);

/**
 * @brief SMBus peripheral process call callback
 *
 * @param[in]     command Command code
 * @param[in]     in_data Input data (2 bytes)
 * @param[out]    out_data Output data buffer
 * @param[in,out] out_length On entry: buffer size. On exit: bytes written
 * @param[in]     context User context
 *
 * @return ESP_OK on success, error code otherwise
 */
typedef esp_err_t (*star_smbus_peripheral_process_cb_t)(uint8_t        command,
                                                        const uint8_t* in_data,
                                                        uint8_t*       out_data,
                                                        size_t*        out_length,
                                                        void*          context);

/**
 * @brief SMBus peripheral configuration
 */
typedef struct {
  uint8_t address; /**< SMBus peripheral address (7-bit) */

  /* Callbacks */
  star_smbus_peripheral_quick_cb_t        on_quick;         /**< Quick command */
  star_smbus_peripheral_receive_byte_cb_t on_receive_byte;  /**< Receive byte */
  star_smbus_peripheral_send_byte_cb_t    on_send_byte;     /**< Send byte */
  star_smbus_peripheral_read_cb_t         on_read_byte;     /**< Read byte */
  star_smbus_peripheral_write_cb_t        on_write_byte;    /**< Write byte */
  star_smbus_peripheral_read_cb_t         on_read_word;     /**< Read word */
  star_smbus_peripheral_write_cb_t        on_write_word;    /**< Write word */
  star_smbus_peripheral_read_cb_t         on_read_block;    /**< Read block */
  star_smbus_peripheral_write_cb_t        on_write_block;   /**< Write block */
  star_smbus_peripheral_process_cb_t      on_process_call;  /**< Process call */
  star_smbus_peripheral_process_cb_t      on_block_process; /**< Block process call */

  void* context; /**< User context for callbacks */

  /* Options */
  bool    use_pec;       /**< Enable PEC (Packet Error Code) */
  bool    support_alert; /**< Support SMBus Alert */
  uint8_t alert_address; /**< Alert response address */
} star_smbus_peripheral_config_t;

/**
 * @brief SMBus peripheral statistics
 */
typedef struct {
  uint64_t  quick_commands;         /**< Quick command count */
  uint64_t  send_byte_commands;     /**< Send byte count */
  uint64_t  receive_byte_commands;  /**< Receive byte count */
  uint64_t  read_byte_commands;     /**< Read byte count */
  uint64_t  write_byte_commands;    /**< Write byte count */
  uint64_t  read_word_commands;     /**< Read word count */
  uint64_t  write_word_commands;    /**< Write word count */
  uint64_t  read_block_commands;    /**< Read block count */
  uint64_t  write_block_commands;   /**< Write block count */
  uint64_t  process_call_commands;  /**< Process call count */
  uint64_t  block_process_commands; /**< Block process call count */
  uint64_t  pec_errors;             /**< PEC errors */
  uint64_t  callback_errors;        /**< Callback errors */
  uint64_t  alert_requests;         /**< Alert requests sent */
  esp_err_t last_error;             /**< Last error */
} star_smbus_peripheral_stats_t;

/* --- Peripheral Management --- */

/**
 * @brief Enable SMBus peripheral mode
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of I2C/SMBus bus
 * @param[in] config Peripheral configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note At least one callback should be provided
 */
esp_err_t star_bus_smbus_peripheral_enable(star_bus_manager_t*                   manager,
                                           const char*                           bus_name,
                                           const star_smbus_peripheral_config_t* config);

/**
 * @brief Disable SMBus peripheral mode
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of I2C/SMBus bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_smbus_peripheral_disable(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Check if SMBus peripheral mode is enabled
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of I2C/SMBus bus
 * @param[out] enabled Pointer to store enabled status
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_smbus_peripheral_is_enabled(const star_bus_manager_t* manager,
                                               const char*               bus_name,
                                               bool*                     enabled);

/* --- Alert Management --- */

/**
 * @brief Send SMBus Alert
 *
 * Pulls SMBALERT# line low to signal master.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of I2C/SMBus bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_smbus_peripheral_send_alert(star_bus_manager_t* manager, const char* bus_name);

/* --- Statistics --- */

/**
 * @brief Get SMBus peripheral statistics
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of I2C/SMBus bus
 * @param[out] stats Pointer to store statistics
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_smbus_peripheral_get_stats(const star_bus_manager_t*      manager,
                                              const char*                    bus_name,
                                              star_smbus_peripheral_stats_t* stats);

/**
 * @brief Reset SMBus peripheral statistics
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of I2C/SMBus bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_smbus_peripheral_reset_stats(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Print SMBus peripheral statistics
 *
 * @param[in] bus_name Name of I2C/SMBus bus
 * @param[in] stats Pointer to statistics
 */
void star_bus_smbus_peripheral_print_stats(const char*                          bus_name,
                                           const star_smbus_peripheral_stats_t* stats);

/* --- Helper Macros --- */

/**
 * @brief Default SMBus Alert address
 */
#define STAR_SMBUS_ALERT_ADDRESS (0x0C)

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_SMBUS_PERIPHERAL_H */
