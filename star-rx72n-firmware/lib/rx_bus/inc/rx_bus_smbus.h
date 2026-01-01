/* lib/rx_bus/inc/rx_bus_smbus.h */

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
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_arg if bus is not SMBUS type
 * @return k_rx_err_timeout if mutex timeout
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
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_nack if device NACK received
 * @return k_rx_err_crc_mismatch if PEC check fails
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
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or data is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_nack if device NACK received
 * @return k_rx_err_crc_mismatch if PEC check fails
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
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_nack if device NACK received
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_bus_smbus_write_byte_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint8_t           data);

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
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or data is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_nack if device NACK received
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_bus_smbus_read_byte_data(rx_bus_manager_t* manager,
                                     const char*       bus_name,
                                     uint8_t           command,
                                     uint8_t*          data);

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
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_nack if device NACK received
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_bus_smbus_write_word_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint16_t          data);

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
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or data is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_nack if device NACK received
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_bus_smbus_read_word_data(rx_bus_manager_t* manager,
                                     const char*       bus_name,
                                     uint8_t           command,
                                     uint16_t*         data);

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
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBUS timeout
 * @return k_rx_err_nack if device NACK received
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_bus_smbus_read_block_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint8_t*          data,
                                      uint8_t*          length,
                                      uint8_t           max_length);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_SMBUS_H */
