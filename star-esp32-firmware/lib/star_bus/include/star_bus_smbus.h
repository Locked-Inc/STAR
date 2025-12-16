/* lib/star_bus/include/star_bus_smbus.h */

#ifndef STAR_BUS_SMBUS_H
#define STAR_BUS_SMBUS_H

#include <esp_err.h>
#include <stdint.h>

#include "star_bus_i2c.h"
#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_smbus.h
 * @brief SMBus (System Management Bus) protocol implementation over I2C
 *
 * SMBus is a subset of I2C with additional timing and protocol requirements.
 * This module provides standard SMBus commands built on top of the star_bus I2C
 * implementation.
 *
 * SMBus Protocol Features:
 * - Standardized command set
 * - Packet Error Checking (PEC) support
 * - Timeout requirements (25-35ms)
 * - Clock stretching limits
 *
 * Example Usage:
 * @code
 * // === Setup (see star_bus_manager.h for full setup) ===
 *
 * #include "star_bus_smbus.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // Create I2C bus for SMBus device (e.g., battery management IC)
 * star_bus_config_t* bms_bus = star_bus_config_create_i2c(
 *     "bms_smbus", I2C_NUM_0, 0x0B, GPIO_NUM_21, GPIO_NUM_22, 100000);
 * star_bus_manager_add_bus(&bus_manager, bms_bus);
 *
 *
 * // === Quick Command (Device Polling) ===
 *
 * // Check if device is present
 * esp_err_t ret = star_smbus_quick_command(&bus_manager, "bms_smbus", 0x0B, true);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "BMS device present");
 * }
 *
 *
 * // === Send/Receive Byte (No Command Code) ===
 *
 * // Send a control byte
 * star_smbus_send_byte(&bus_manager, "bms_smbus", 0x0B, 0x01);
 *
 * // Receive a status byte
 * uint8_t status;
 * star_smbus_receive_byte(&bus_manager, "bms_smbus", 0x0B, &status);
 * ESP_LOGI(TAG, "Device status: 0x%02X", status);
 *
 *
 * // === Read/Write Byte (With Command Code) ===
 *
 * // Read battery state of charge (command 0x0D)
 * uint8_t soc;
 * ret = star_smbus_read_byte(&bus_manager, "bms_smbus", 0x0B, 0x0D, &soc);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "State of Charge: %d%%", soc);
 * }
 *
 * // Write configuration register
 * star_smbus_write_byte(&bus_manager, "bms_smbus", 0x0B, 0x00, 0x80);
 *
 *
 * // === Read/Write Word (16-bit values) ===
 *
 * // Read battery voltage (command 0x09, returns millivolts)
 * uint16_t voltage_mv;
 * ret = star_smbus_read_word(&bus_manager, "bms_smbus", 0x0B, 0x09, &voltage_mv);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Battery voltage: %d mV", voltage_mv);
 * }
 *
 * // Read battery current (command 0x0A, returns milliamps, signed)
 * uint16_t current_ma;
 * star_smbus_read_word(&bus_manager, "bms_smbus", 0x0B, 0x0A, &current_ma);
 * int16_t signed_current = (int16_t)current_ma;
 * ESP_LOGI(TAG, "Current: %d mA", signed_current);
 *
 * // Write a 16-bit value
 * star_smbus_write_word(&bus_manager, "bms_smbus", 0x0B, 0x15, 0x1234);
 *
 *
 * // === Process Call (Atomic Write-Read) ===
 *
 * // Send a command and get processed response atomically
 * uint16_t request = 0x1234;
 * uint16_t response;
 * ret = star_smbus_process_call(
 *     &bus_manager,
 *     "bms_smbus",
 *     0x0B,
 *     0x44,       // Process call command
 *     request,
 *     &response
 * );
 * ESP_LOGI(TAG, "Process call: 0x%04X -> 0x%04X", request, response);
 *
 *
 * // === Block Write (Multi-Byte Write) ===
 *
 * // Write configuration block
 * uint8_t config_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
 * ret = star_smbus_block_write(
 *     &bus_manager,
 *     "bms_smbus",
 *     0x0B,
 *     0x20,           // Config command
 *     config_data,
 *     sizeof(config_data)  // Max 32 bytes
 * );
 *
 *
 * // === Block Read (Multi-Byte Read) ===
 *
 * // Read manufacturer name
 * uint8_t name_buffer[32];
 * uint8_t name_length;
 * ret = star_smbus_block_read(
 *     &bus_manager,
 *     "bms_smbus",
 *     0x0B,
 *     0x20,           // Manufacturer name command
 *     name_buffer,
 *     sizeof(name_buffer),
 *     &name_length
 * );
 * if (ret == ESP_OK) {
 *     name_buffer[name_length] = '\0';
 *     ESP_LOGI(TAG, "Manufacturer: %s", name_buffer);
 * }
 *
 *
 * // === Block Process Call (Atomic Block Write-Read) ===
 *
 * uint8_t cmd_data[] = {0xAA, 0xBB};
 * uint8_t response_data[32];
 * uint8_t response_len;
 *
 * ret = star_smbus_block_process_call(
 *     &bus_manager,
 *     "bms_smbus",
 *     0x0B,
 *     0x50,           // Block process command
 *     cmd_data,
 *     sizeof(cmd_data),
 *     response_data,
 *     sizeof(response_data),
 *     &response_len
 * );
 *
 *
 * // === PEC (Packet Error Checking) ===
 *
 * // Calculate CRC for packet validation
 * uint8_t packet[] = {0x16, 0x09, 0x17, 0x0C, 0x80};  // Addr, Cmd, Data
 * uint8_t pec = star_smbus_calculate_pec(packet, sizeof(packet), 0);
 * ESP_LOGI(TAG, "Calculated PEC: 0x%02X", pec);
 *
 *
 * // === Reading Battery Management Data ===
 *
 * // Common SMBus battery commands (SBS specification)
 * uint16_t temp_k, full_cap, remain_cap, run_time;
 *
 * star_smbus_read_word(&bus_manager, "bms_smbus", 0x0B, 0x08, &temp_k);       // Temperature (0.1K)
 * star_smbus_read_word(&bus_manager, "bms_smbus", 0x0B, 0x10, &full_cap);     // Full capacity (mAh)
 * star_smbus_read_word(&bus_manager, "bms_smbus", 0x0B, 0x0F, &remain_cap);   // Remaining (mAh)
 * star_smbus_read_word(&bus_manager, "bms_smbus", 0x0B, 0x11, &run_time);     // Run time (minutes)
 *
 * float temp_c = (temp_k / 10.0f) - 273.15f;
 * ESP_LOGI(TAG, "Battery: %.1f°C, %d/%d mAh, %d min remaining",
 *          temp_c, remain_cap, full_cap, run_time);
 *
 *
 * // === Cleanup ===
 *
 * star_bus_manager_remove_bus(&bus_manager, "bms_smbus");
 * star_bus_config_destroy(bms_bus);
 * @endcode
 */

/* --- Constants --- */

/**
 * @brief SMBus constants
 *
 * Using enum for type safety while maintaining compile-time constant behavior
 * required for array sizes in C.
 */
enum {
  /** Maximum block transfer size per SMBus 2.0 specification */
  STAR_SMBUS_MAX_BLOCK_SIZE = 32,

  /** SMBus timeout in milliseconds (spec: 25-35ms) */
  STAR_SMBUS_TIMEOUT_MS = 30,
};

/* --- Types --- */

/**
 * @brief SMBus Packet Error Code (PEC) mode
 */
typedef enum {
  k_star_smbus_pec_disabled = 0, /**< PEC disabled (default) */
  k_star_smbus_pec_enabled  = 1, /**< PEC enabled for all transactions */
} star_smbus_pec_mode_t;

/* --- SMBus Protocol Functions --- */

/**
 * @brief SMBus Quick Command
 *
 * Sends a single bit (R/W bit) to the target device with no data.
 * Used for simple on/off control or device polling.
 *
 * @param[in] manager   Pointer to initialized bus manager
 * @param[in] bus_name  Name of I2C bus to use
 * @param[in] addr      7-bit SMBus target address
 * @param[in] write     true for write command, false for read command
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_smbus_quick_command(star_bus_manager_t* manager,
                                   const char*         bus_name,
                                   uint8_t             addr,
                                   bool                write);

/**
 * @brief SMBus Send Byte
 *
 * Sends a single byte to the target device (no command code).
 *
 * @param[in] manager   Pointer to initialized bus manager
 * @param[in] bus_name  Name of I2C bus to use
 * @param[in] addr      7-bit SMBus target address
 * @param[in] data      Byte to send
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_smbus_send_byte(star_bus_manager_t* manager, const char* bus_name, uint8_t addr, uint8_t data);

/**
 * @brief SMBus Receive Byte
 *
 * Reads a single byte from the target device (no command code).
 *
 * @param[in]  manager   Pointer to initialized bus manager
 * @param[in]  bus_name  Name of I2C bus to use
 * @param[in]  addr      7-bit SMBus target address
 * @param[out] data      Pointer to store received byte
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_smbus_receive_byte(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  uint8_t             addr,
                                  uint8_t*            data);

/**
 * @brief SMBus Write Byte
 *
 * Writes a command code followed by a single data byte.
 *
 * @param[in] manager   Pointer to initialized bus manager
 * @param[in] bus_name  Name of I2C bus to use
 * @param[in] addr      7-bit SMBus target address
 * @param[in] command   Command code (register address)
 * @param[in] data      Data byte to write
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_smbus_write_byte(star_bus_manager_t* manager,
                                const char*         bus_name,
                                uint8_t             addr,
                                uint8_t             command,
                                uint8_t             data);

/**
 * @brief SMBus Read Byte
 *
 * Writes a command code and reads a single data byte response.
 *
 * @param[in]  manager   Pointer to initialized bus manager
 * @param[in]  bus_name  Name of I2C bus to use
 * @param[in]  addr      7-bit SMBus target address
 * @param[in]  command   Command code (register address)
 * @param[out] data      Pointer to store received byte
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_smbus_read_byte(star_bus_manager_t* manager,
                               const char*         bus_name,
                               uint8_t             addr,
                               uint8_t             command,
                               uint8_t*            data);

/**
 * @brief SMBus Write Word
 *
 * Writes a command code followed by a 16-bit word (little-endian).
 *
 * @param[in] manager   Pointer to initialized bus manager
 * @param[in] bus_name  Name of I2C bus to use
 * @param[in] addr      7-bit SMBus target address
 * @param[in] command   Command code (register address)
 * @param[in] data      16-bit data word to write (little-endian)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_smbus_write_word(star_bus_manager_t* manager,
                                const char*         bus_name,
                                uint8_t             addr,
                                uint8_t             command,
                                uint16_t            data);

/**
 * @brief SMBus Read Word
 *
 * Writes a command code and reads a 16-bit word response (little-endian).
 *
 * @param[in]  manager   Pointer to initialized bus manager
 * @param[in]  bus_name  Name of I2C bus to use
 * @param[in]  addr      7-bit SMBus target address
 * @param[in]  command   Command code (register address)
 * @param[out] data      Pointer to store received 16-bit word (little-endian)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_smbus_read_word(star_bus_manager_t* manager,
                               const char*         bus_name,
                               uint8_t             addr,
                               uint8_t             command,
                               uint16_t*           data);

/**
 * @brief SMBus Process Call
 *
 * Writes a command code and 16-bit word, then reads a 16-bit word response.
 * This is an atomic operation (no STOP condition between write and read).
 *
 * @param[in]  manager    Pointer to initialized bus manager
 * @param[in]  bus_name   Name of I2C bus to use
 * @param[in]  addr       7-bit SMBus target address
 * @param[in]  command    Command code
 * @param[in]  write_data 16-bit data word to write (little-endian)
 * @param[out] read_data  Pointer to store received 16-bit word (little-endian)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_smbus_process_call(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  uint8_t             addr,
                                  uint8_t             command,
                                  uint16_t            write_data,
                                  uint16_t*           read_data);

/**
 * @brief SMBus Block Write
 *
 * Writes a command code followed by a block of up to 32 bytes.
 * The first byte sent is the block length.
 *
 * @param[in] manager   Pointer to initialized bus manager
 * @param[in] bus_name  Name of I2C bus to use
 * @param[in] addr      7-bit SMBus target address
 * @param[in] command   Command code
 * @param[in] data      Pointer to data buffer to write
 * @param[in] length    Number of bytes to write (1-32)
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if length > 32
 */
esp_err_t star_smbus_block_write(star_bus_manager_t* manager,
                                 const char*         bus_name,
                                 uint8_t             addr,
                                 uint8_t             command,
                                 const uint8_t*      data,
                                 uint8_t             length);

/**
 * @brief SMBus Block Read
 *
 * Writes a command code and reads a block of up to 32 bytes.
 * The first byte received is the block length.
 *
 * @param[in]  manager    Pointer to initialized bus manager
 * @param[in]  bus_name   Name of I2C bus to use
 * @param[in]  addr       7-bit SMBus target address
 * @param[in]  command    Command code
 * @param[out] data       Pointer to buffer to store received data
 * @param[in]  max_length Maximum buffer size (should be >= 32)
 * @param[out] length     Pointer to store actual number of bytes read
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_smbus_block_read(star_bus_manager_t* manager,
                                const char*         bus_name,
                                uint8_t             addr,
                                uint8_t             command,
                                uint8_t*            data,
                                uint8_t             max_length,
                                uint8_t*            length);

/**
 * @brief SMBus Block Write-Block Read Process Call
 *
 * Writes a command code and block of data, then reads a block response.
 * This is an atomic operation (no STOP condition between write and read).
 *
 * @param[in]  manager      Pointer to initialized bus manager
 * @param[in]  bus_name     Name of I2C bus to use
 * @param[in]  addr         7-bit SMBus target address
 * @param[in]  command      Command code
 * @param[in]  write_data   Pointer to data buffer to write
 * @param[in]  write_length Number of bytes to write (1-32)
 * @param[out] read_data    Pointer to buffer to store received data
 * @param[in]  max_read_len Maximum read buffer size
 * @param[out] read_length  Pointer to store actual number of bytes read
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_smbus_block_process_call(star_bus_manager_t* manager,
                                        const char*         bus_name,
                                        uint8_t             addr,
                                        uint8_t             command,
                                        const uint8_t*      write_data,
                                        uint8_t             write_length,
                                        uint8_t*            read_data,
                                        uint8_t             max_read_len,
                                        uint8_t*            read_length);

/* --- PEC (Packet Error Checking) Functions --- */

/**
 * @brief Calculate CRC-8 for SMBus Packet Error Checking (PEC)
 *
 * Uses polynomial 0x07 (x^8 + x^2 + x + 1) per SMBus specification.
 *
 * @param[in] data   Pointer to data buffer
 * @param[in] length Number of bytes in buffer
 * @param[in] crc    Initial CRC value (usually 0)
 *
 * @return Calculated CRC-8 value
 */
uint8_t star_smbus_calculate_pec(const uint8_t* data, size_t length, uint8_t crc);

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_SMBUS_H */
