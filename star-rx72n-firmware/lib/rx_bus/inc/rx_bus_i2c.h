/* lib/rx_bus/inc/rx_bus_i2c.h */

/**
 * @file rx_bus_i2c.h
 * @brief I2C bus abstraction for RX72N
 * @details
 * Provides bus manager integration for RIIC I2C operations.
 * Wraps the low-level RIIC HAL with bus abstraction pattern.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_BUS_I2C_H
#define STAR_RX72N_BUS_I2C_H

#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * I2C Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize I2C bus through bus manager
 *
 * Configures RIIC channel and pins for I2C communication.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_arg if bus is not I2C type
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_i2c_init(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Write data to I2C device through bus manager
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[in] data Pointer to data to write
 * @param[in] length Number of bytes to write
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or data is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if I2C timeout or mutex timeout
 * @return k_rx_err_nack if device NACK received
 */
rx_err_t rx_bus_i2c_write(rx_bus_manager_t* manager,
                          const char*       bus_name,
                          const uint8_t*    data,
                          uint16_t          length);

/**
 * @brief Read data from I2C device through bus manager
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[out] data Pointer to buffer for received data
 * @param[in] length Number of bytes to read
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or data is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if I2C timeout or mutex timeout
 * @return k_rx_err_nack if device NACK received
 */
rx_err_t
rx_bus_i2c_read(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data, uint16_t length);

/**
 * @brief Write then read from I2C device through bus manager
 *
 * Common pattern for reading registers: write register address, then read data.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[in] write_data Pointer to data to write
 * @param[in] write_length Number of bytes to write
 * @param[out] read_data Pointer to buffer for received data
 * @param[in] read_length Number of bytes to read
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if I2C timeout or mutex timeout
 * @return k_rx_err_nack if device NACK received
 */
rx_err_t rx_bus_i2c_write_read(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               const uint8_t*    write_data,
                               uint16_t          write_length,
                               uint8_t*          read_data,
                               uint16_t          read_length);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_I2C_H */
