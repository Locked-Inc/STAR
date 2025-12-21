/* include/rx_bus_smbus.h */

/**
 * @file rx_bus_smbus.h
 * @brief SMBUS bus abstraction for RX72N
 * @details
 * Provides bus manager integration for SMBUS operations.
 * SMBUS is I2C with additional features:
 * - Packet Error Checking (PEC) using CRC-8
 * - Timeout requirements
 * - Additional protocol commands
 *
 * Used for fuel gauge communication.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_BUS_SMBUS_H
#define STAR_RX72N_BUS_SMBUS_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * SMBUS Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize SMBUS bus through bus manager
 *
 * Configures RIIC channel for SMBUS communication with optional PEC.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_ARG if bus is not SMBUS type
 * @return RX_ERR_TIMEOUT if mutex timeout
 */
rx_err_t rx_bus_smbus_init(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Write byte to SMBUS device
 *
 * SMBUS Quick Command or Send Byte protocol.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[in] command Command byte to write
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if SMBUS timeout
 * @return RX_ERR_NACK if device NACK received
 * @return RX_ERR_CRC_MISMATCH if PEC check fails
 */
rx_err_t rx_bus_smbus_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t command);

/**
 * @brief Read byte from SMBUS device
 *
 * SMBUS Receive Byte protocol.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[out] data Pointer to store received byte
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager, bus_name, or data is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if SMBUS timeout
 * @return RX_ERR_NACK if device NACK received
 * @return RX_ERR_CRC_MISMATCH if PEC check fails
 */
rx_err_t rx_bus_smbus_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data);

/**
 * @brief Write byte data to SMBUS register
 *
 * SMBUS Write Byte protocol.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[in] command Register/command code
 * @param[in] data Data byte to write
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if SMBUS timeout
 * @return RX_ERR_NACK if device NACK received
 * @return RX_ERR_CRC_MISMATCH if PEC check fails
 */
rx_err_t rx_bus_smbus_write_byte_data(rx_bus_manager_t* manager,
                                       const char*        bus_name,
                                       uint8_t            command,
                                       uint8_t            data);

/**
 * @brief Read byte data from SMBUS register
 *
 * SMBUS Read Byte protocol.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[in] command Register/command code
 * @param[out] data Pointer to store received byte
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager, bus_name, or data is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if SMBUS timeout
 * @return RX_ERR_NACK if device NACK received
 * @return RX_ERR_CRC_MISMATCH if PEC check fails
 */
rx_err_t rx_bus_smbus_read_byte_data(rx_bus_manager_t* manager,
                                      const char*        bus_name,
                                      uint8_t            command,
                                      uint8_t*           data);

/**
 * @brief Write word data to SMBUS register
 *
 * SMBUS Write Word protocol (16-bit little-endian).
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[in] command Register/command code
 * @param[in] data Data word to write (little-endian)
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if SMBUS timeout
 * @return RX_ERR_NACK if device NACK received
 * @return RX_ERR_CRC_MISMATCH if PEC check fails
 */
rx_err_t rx_bus_smbus_write_word_data(rx_bus_manager_t* manager,
                                       const char*        bus_name,
                                       uint8_t            command,
                                       uint16_t           data);

/**
 * @brief Read word data from SMBUS register
 *
 * SMBUS Read Word protocol (16-bit little-endian).
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[in] command Register/command code
 * @param[out] data Pointer to store received word (little-endian)
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager, bus_name, or data is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if SMBUS timeout
 * @return RX_ERR_NACK if device NACK received
 * @return RX_ERR_CRC_MISMATCH if PEC check fails
 */
rx_err_t rx_bus_smbus_read_word_data(rx_bus_manager_t* manager,
                                      const char*        bus_name,
                                      uint8_t            command,
                                      uint16_t*          data);

/**
 * @brief Read block data from SMBUS device
 *
 * SMBUS Block Read protocol (up to 32 bytes).
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBUS bus name
 * @param[in] command Register/command code
 * @param[out] data Pointer to buffer for received data
 * @param[out] length Pointer to store number of bytes read
 * @param[in] max_length Maximum buffer size (typically 32)
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if any pointer parameter is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if SMBUS timeout
 * @return RX_ERR_NACK if device NACK received
 * @return RX_ERR_CRC_MISMATCH if PEC check fails
 */
rx_err_t rx_bus_smbus_read_block_data(rx_bus_manager_t* manager,
                                       const char*        bus_name,
                                       uint8_t            command,
                                       uint8_t*           data,
                                       uint8_t*           length,
                                       uint8_t            max_length);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_SMBUS_H */
