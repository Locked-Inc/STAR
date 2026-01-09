/* tests/mocks/mock_rx_bus_smbus.h */

/**
 * @file mock_rx_bus_smbus.h
 * @brief Mock SMBus Driver for Unit Testing BQ4050
 * @details
 * Provides mock implementation of SMBus driver for host-side testing.
 * Allows configuring expected responses and error injection.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_BUS_SMBUS_H
#define MOCK_RX_BUS_SMBUS_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief Mock SMBus buffer and limit constants
 */
typedef enum {
  k_mock_smbus_max_regs      = 256, /**< Maximum number of registers */
  k_mock_smbus_max_block_len = 32,  /**< Maximum block read length */
} mock_smbus_constants_t;

/* =============================================================================
 * Mock Configuration Functions
 * =============================================================================
 */

/**
 * @brief Reset all mock state
 *
 * Call before each test to ensure clean state.
 */
void mock_smbus_reset(void);

/**
 * @brief Set expected word value for a register read
 *
 * @param[in] command Register/command code
 * @param[in] value Word value to return
 */
void mock_smbus_set_word_response(uint8_t command, uint16_t value);

/**
 * @brief Set expected byte value for a register read
 *
 * @param[in] command Register/command code
 * @param[in] value Byte value to return
 */
void mock_smbus_set_byte_response(uint8_t command, uint8_t value);

/**
 * @brief Set error to return for next read operation
 *
 * @param[in] err Error code to return
 */
void mock_smbus_set_next_error(rx_err_t err);

/**
 * @brief Set error to return for specific command
 *
 * @param[in] command Register/command code
 * @param[in] err Error code to return for this command
 */
void mock_smbus_set_command_error(uint8_t command, rx_err_t err);

/**
 * @brief Clear error injection for specific command
 *
 * @param[in] command Register/command code
 */
void mock_smbus_clear_command_error(uint8_t command);

/**
 * @brief Set initialized state
 *
 * @param[in] initialized True if bus should appear initialized
 */
void mock_smbus_set_initialized(bool initialized);

/**
 * @brief Set expected block data for a block read
 *
 * @param[in] command Register/command code
 * @param[in] data Pointer to block data
 * @param[in] length Length of block data
 */
void mock_smbus_set_block_response(uint8_t command, const uint8_t* data, uint8_t length);

/**
 * @brief Get number of read operations performed
 *
 * @return Number of read operations
 */
uint32_t mock_smbus_get_read_count(void);

/**
 * @brief Get last command code that was read
 *
 * @return Last command code
 */
uint8_t mock_smbus_get_last_command(void);

/**
 * @brief Check if init was called
 *
 * @return True if init was called
 */
bool mock_smbus_was_init_called(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RX_BUS_SMBUS_H */
