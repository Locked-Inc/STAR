// SPDX-License-Identifier: MIT
/* tests/mocks/mock_riic_hal.h */

/**
 * @file mock_riic_hal.h
 * @brief Mock RIIC (I2C) HAL implementation for unit testing without hardware
 *
 * @details
 * Provides test double for RIIC (Renesas I2C) Hardware Abstraction Layer to
 * enable unit testing of I2C-dependent modules without actual RX72N hardware.
 * Supports comprehensive state tracking, data buffer simulation, multiple
 * error injection modes, and call history verification.
 *
 * This mock enables testing of:
 * - I2C controller initialization (3 channels: RIIC0, RIIC1, RIIC2)
 * - I2C write operations (controller->peripheral)
 * - I2C read operations (peripheral->controller)
 * - Combined write-read operations (register read pattern)
 * - Error handling (NACK, timeout, bus busy)
 * - Multi-byte data transfer sequences
 *
 * @par Test Architecture:
 * @dot
 * digraph mock_riic_arch {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_test {
 *     label="Unit Test Environment";
 *     style=filled;
 *     color=lightyellow;
 *     test [label="test_riic_hal.c\nI2C Device Tests"];
 *   }
 *
 *   subgraph cluster_mock {
 *     label="Mock RIIC HAL (This Module)";
 *     style=filled;
 *     color=lightblue;
 *     state [label="g_mock_riic\n3 Channels\nTX/RX Buffers"];
 *     funcs [label="riic_init()\nriic_write()\nriic_read()\nriic_write_read()"];
 *   }
 *
 *   subgraph cluster_real {
 *     label="Real RIIC HAL (Production)";
 *     style=filled;
 *     color=lightgreen;
 *     hw [label="I2C Hardware\nRIIC0-2 Registers", style=dashed];
 *   }
 *
 *   test -> state [label="1. Setup buffers"];
 *   test -> funcs [label="2. Call RIIC HAL"];
 *   funcs -> state [label="3. Update state"];
 *   test -> state [label="4. Verify TX/RX"];
 *
 *   hw [style=dashed, label="(Not accessed\nin tests)"];
 * }
 * @enddot
 *
 * @par Mock Capabilities:
 * | Feature                  | Supported | Description |
 * |--------------------------|-----------|-------------|
 * | Multi-channel support    | Yes       | 3 independent RIIC channels (RIIC0-2) |
 * | TX data capture          | Yes       | Records all transmitted data |
 * | RX data simulation       | Yes       | Pre-configure data to return on reads |
 * | Error injection          | Yes       | NACK, timeout, bus busy, general errors |
 * | Call history             | Yes       | Records last 64 function calls |
 * | Device address tracking  | Yes       | Tracks last I2C device address used |
 * | Frequency configuration  | Yes       | Stores configured I2C frequency |
 *
 * @par Mock vs Real Implementation:
 * | Aspect              | Mock (This Module)          | Real (riic_hal.c) |
 * |---------------------|-----------------------------|--------------------|
 * | Hardware access     | None - buffers in RAM       | Direct RIIC register access |
 * | Timing              | Instant response            | I2C bus timing (us delays) |
 * | Error conditions    | Configurable injection      | Actual bus errors |
 * | Data transfer       | Buffer copy                 | DTC/interrupt-driven |
 * | NACK detection      | Simulated flag              | Actual NACK from peripheral |
 *
 * @par Usage in Tests:
 * - tests/test_riic_hal.c - RIIC HAL unit tests
 *
 * @par Example Usage:
 * @code
 * #include "mock_riic_hal.h"
 * #include "unity.h"
 *
 * void setUp(void) {
 *   mock_riic_init();  // Reset state before each test
 * }
 *
 * void test_riic_write_sends_data_to_device(void) {
 *   // Arrange: Configure channel
 *   riic_init(0, 400000);  // RIIC0 @ 400kHz
 *
 *   // Act: Write data to I2C device
 *   uint8_t data[] = {0x12, 0x34, 0x56};
 *   rx_err_t err = riic_write(0, 0x50, data, 3);
 *
 *   // Assert: Verify success
 *   TEST_ASSERT_EQUAL(k_rx_ok, err);
 *
 *   // Assert: Verify data was captured
 *   const uint8_t* tx_buf = mock_riic_get_tx_buffer(0);
 *   TEST_ASSERT_EQUAL_HEX8_ARRAY(data, tx_buf, 3);
 * }
 *
 * void test_riic_read_returns_simulated_data(void) {
 *   // Arrange: Pre-configure read data
 *   uint8_t expected[] = {0xAA, 0xBB, 0xCC};
 *   mock_riic_set_rx_buffer(0, expected, 3);
 *
 *   // Act: Read from device
 *   uint8_t actual[3];
 *   rx_err_t err = riic_read(0, 0x50, actual, 3);
 *
 *   // Assert: Verify data matches
 *   TEST_ASSERT_EQUAL(k_rx_ok, err);
 *   TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, 3);
 * }
 *
 * void test_riic_nack_error_handling(void) {
 *   // Arrange: Simulate NACK
 *   mock_riic_simulate_nack(true);
 *
 *   // Act: Attempt write
 *   uint8_t data[] = {0xFF};
 *   rx_err_t err = riic_write(0, 0x50, data, 1);
 *
 *   // Assert: Verify NACK error returned
 *   TEST_ASSERT_EQUAL(k_rx_err_i2c_nack, err);
 * }
 * @endcode
 *
 * @see riic_hal.h Real RIIC HAL interface
 * @see tests/test_riic_hal.c Test file using this mock
 * @see hardware.h Hardware peripheral definitions
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] All loops have fixed bounds
 * - Rule 3: [OK] Static allocation only (buffers in g_mock_riic)
 * - Rule 4: [OK] Functions kept under 60 lines
 * - Rule 5: [OK] Input validation in all functions
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] Return values checked
 * - Rule 8: [OK] Minimal preprocessor use
 * - Rule 9: [OK] Single-level pointers only
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - S: Single Responsibility - Only provides RIIC HAL mock functionality
 * - O: Open/Closed - Extensible via error injection
 * - L: Liskov Substitution - Drop-in replacement for real RIIC HAL
 * - I: Interface Segregation - Minimal focused interface
 * - D: Dependency Inversion - Tests depend on RIIC interface, not implementation
 *
 * @author STAR Team
 * @date 2026-01-05
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hardware.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Mock RIIC constants */
typedef enum : uint16_t {
  k_mock_riic_max_channels      = 3,   /**< RIIC channels (0-2) */
  k_mock_riic_buffer_size       = 256, /**< TX/RX buffer size */
  k_mock_riic_call_history_size = 64,  /**< Call history buffer size */
} mock_riic_constants_t;

/* =============================================================================
 * Types
 * =============================================================================
 */

/** @brief RIIC HAL function call types */
typedef enum : uint8_t {
  k_mock_riic_call_init,
  k_mock_riic_call_write,
  k_mock_riic_call_read,
  k_mock_riic_call_write_read,
} mock_riic_call_type_t;

/** @brief RIIC HAL function call record */
typedef struct {
  mock_riic_call_type_t type;         /**< Call type */
  uint8_t               channel;      /**< RIIC channel */
  uint8_t               device_addr;  /**< Device address */
  uint16_t              write_length; /**< Write data length */
  uint16_t              read_length;  /**< Read data length */
} mock_riic_call_t;

/** @brief Per-channel RIIC state */
typedef struct {
  bool     initialized;                        /**< Channel initialized */
  uint32_t frequency_hz;                       /**< Configured frequency */
  uint8_t  tx_buffer[k_mock_riic_buffer_size]; /**< Last transmitted data */
  uint16_t tx_length;                          /**< Last transmit length */
  uint8_t  rx_buffer[k_mock_riic_buffer_size]; /**< Data to return on read */
  uint16_t rx_length;                          /**< Available read data length */
  uint8_t  last_device_addr;                   /**< Last device address used */
} mock_riic_channel_state_t;

/** @brief Global mock RIIC state */
typedef struct {
  mock_riic_channel_state_t channels[k_mock_riic_max_channels];          /**< Per-channel state */
  mock_riic_call_t          call_history[k_mock_riic_call_history_size]; /**< Call history */
  uint16_t                  call_count;       /**< Number of calls recorded */
  rx_err_t                  next_error;       /**< Error to return on next call */
  bool                      error_set;        /**< Whether error injection is active */
  bool                      simulate_nack;    /**< Simulate NACK response */
  bool                      simulate_timeout; /**< Simulate timeout */
  bool                      simulate_busy;    /**< Simulate bus busy */
} mock_riic_state_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

/** @brief Global mock RIIC state instance */
extern mock_riic_state_t g_mock_riic;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock RIIC state
 *
 * Resets all channel states and clears call history.
 */
void mock_riic_init(void);

/**
 * @brief Reset mock RIIC state (alias for init)
 */
void mock_riic_reset(void);

/* =============================================================================
 * Test Setup Functions
 * =============================================================================
 */

/**
 * @brief Set data to be returned on read
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[in] data Data to return on read
 * @param[in] length Number of bytes
 */
void mock_riic_set_rx_data(uint8_t channel, const uint8_t* data, uint16_t length);

/**
 * @brief Enable NACK simulation
 *
 * @param[in] simulate True to simulate NACK
 */
void mock_riic_simulate_nack(bool simulate);

/**
 * @brief Enable timeout simulation
 *
 * @param[in] simulate True to simulate timeout
 */
void mock_riic_simulate_timeout(bool simulate);

/**
 * @brief Enable bus busy simulation
 *
 * @param[in] simulate True to simulate bus busy
 */
void mock_riic_simulate_busy(bool simulate);

/**
 * @brief Set error to return on next RIIC HAL call
 *
 * @param[in] err Error code to return
 */
void mock_riic_set_next_error(rx_err_t err);

/**
 * @brief Clear any pending error injection
 */
void mock_riic_clear_error(void);

/* =============================================================================
 * State Inspection Functions
 * =============================================================================
 */

/**
 * @brief Check if channel is initialized
 *
 * @param[in] channel RIIC channel (0-2)
 *
 * @return True if initialized
 */
bool mock_riic_is_initialized(uint8_t channel);

/**
 * @brief Get configured frequency for channel
 *
 * @param[in] channel RIIC channel (0-2)
 *
 * @return Frequency in Hz, or 0 if not initialized
 */
uint32_t mock_riic_get_frequency(uint8_t channel);

/**
 * @brief Get last transmitted data
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[out] data Buffer to copy data to
 * @param[in] max_length Maximum bytes to copy
 *
 * @return Number of bytes copied
 */
uint16_t mock_riic_get_tx_data(uint8_t channel, uint8_t* data, uint16_t max_length);

/**
 * @brief Get last device address used
 *
 * @param[in] channel RIIC channel (0-2)
 *
 * @return Device address
 */
uint8_t mock_riic_get_last_device_addr(uint8_t channel);

/* =============================================================================
 * Call History Functions
 * =============================================================================
 */

/**
 * @brief Get call history entry
 *
 * @param[in] index Index in call history
 *
 * @return Pointer to call record, or NULL if index out of range
 */
const mock_riic_call_t* mock_riic_get_call(uint16_t index);

/**
 * @brief Get total number of recorded calls
 *
 * @return Number of calls in history
 */
uint16_t mock_riic_get_call_count(void);

/**
 * @brief Clear call history
 */
void mock_riic_clear_history(void);

/* =============================================================================
 * RIIC HAL Function Declarations (for test linking)
 * =============================================================================
 */

rx_err_t riic_init(riic_channel_t channel, uint32_t frequency_hz);
rx_err_t riic_write(riic_channel_t    channel,
                    i2c_device_addr_t device_addr,
                    const uint8_t*    data,
                    const uint16_t    length);
rx_err_t riic_read(riic_channel_t    channel,
                   i2c_device_addr_t device_addr,
                   uint8_t*          data,
                   const uint16_t    length);
rx_err_t riic_write_read(riic_channel_t    channel,
                         i2c_device_addr_t device_addr,
                         const uint8_t*    write_data,
                         uint16_t          write_length,
                         uint8_t*          read_data,
                         uint16_t          read_length);

#ifdef __cplusplus
}
#endif
