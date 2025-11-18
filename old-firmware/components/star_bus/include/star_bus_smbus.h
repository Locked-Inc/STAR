/* esp32-firmware/components/star_bus/include/star_bus_smbus.h */

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
 */

/* --- Constants --- */

/** Maximum block transfer size per SMBus 2.0 specification */
#define STAR_SMBUS_MAX_BLOCK_SIZE (32)

/** SMBus timeout in milliseconds (spec: 25-35ms) */
#define STAR_SMBUS_TIMEOUT_MS (30)

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
