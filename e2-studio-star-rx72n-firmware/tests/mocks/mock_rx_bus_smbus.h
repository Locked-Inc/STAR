/* tests/mocks/mock_rx_bus_smbus.h */

/**
 * @file mock_rx_bus_smbus.h
 * @brief Mock SMBus bus abstraction for BQ4050 battery monitor testing
 *
 * @details
 * Provides test double for SMBus (System Management Bus over I2C) driver to
 * enable unit testing of BQ4050 battery monitor without actual hardware.
 * Supports SMBus block read/write, PEC (Packet Error Code), and error injection.
 *
 * Enables testing of:
 * - SMBus read/write word/block operations
 * - BQ4050 command sequences (voltage, current, capacity)
 * - PEC (CRC-8) validation
 * - SMBus timeout and NACK handling
 * - Battery status decoding
 *
 * @par Test Architecture:
 * @dot
 * digraph mock_smbus {
 *   rankdir=LR;
 *   test [label="BQ4050\nTests"];
 *   mock [label="Mock SMBus\n(simulated replies)"];
 *   real [label="Real SMBus\nI2C Bus", style=dashed];
 *   test -> mock [label="Configure responses"];
 *   mock -> real [style=dashed, label="(replaced)"];
 * }
 * @enddot
 *
 * @par Mock Capabilities:
 * | Feature | Supported | Description |
 * |---------|-----------|-------------|
 * | Block read/write | Yes | SMBus block transactions |
 * | PEC validation | Yes | Packet Error Code checking |
 * | Response config | Yes | Pre-configure register values |
 * | Error injection | Yes | NACK, timeout, PEC errors |
 *
 * @par Usage: tests/test_rx_bq4050.c
 *
 * @see rx_bus_smbus.h Real SMBus driver interface
 * @see rx_bq4050.h BQ4050 battery monitor driver
 * @see tests/test_rx_bq4050.c Battery monitor tests
 *
 * @par NASA Power of 10: [OK] Static allocation, bounded loops
 * @par SOLID: D - BQ4050 driver depends on SMBus interface
 *
 * @author STAR Team
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

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
typedef enum : uint16_t {
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
