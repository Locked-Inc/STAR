/**
 * @file mock_rspi.h
 * @brief Mock RSPI (SPI) hardware layer for unit testing without hardware
 *
 * @details
 * Provides test double for RSPI (Renesas SPI) hardware layer to enable
 * unit testing of SPI-dependent modules without actual RX72N hardware.
 * Supports TX/RX data capture, error injection, and call verification.
 *
 * This mock enables testing of:
 * - SPI controller initialization (3 channels: RSPI0, RSPI1, RSPI2)
 * - SPI data transfer (full-duplex TX/RX)
 * - Chip select control
 * - Error handling (timeout, overrun, mode fault)
 * - Multi-byte transfer sequences
 *
 * @par Test Architecture:
 * @dot
 * digraph mock_rspi_arch {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   test [label="Unit Tests"];
 *   mock [label="Mock RSPI\n(this file)"];
 *   real [label="Real RSPI\nHardware", style=dashed];
 *
 *   test -> mock [label="SPI calls"];
 *   mock -> real [style=dashed, label="(replaced in tests)"];
 * }
 * @enddot
 *
 * @par Mock Capabilities:
 * | Feature              | Supported | Description |
 * |----------------------|-----------|-------------|
 * | Multi-channel        | Yes       | 3 RSPI channels (RSPI0-2) |
 * | TX data capture      | Yes       | Records transmitted data |
 * | RX data injection    | Yes       | Pre-configure received data |
 * | Error injection      | Yes       | Timeout, overrun, mode fault |
 * | Call history         | Yes       | Last 64 function calls |
 * | Transfer size        | Yes       | Up to 2048 bytes per transfer |
 *
 * @par Mock vs Real:
 * | Aspect       | Mock              | Real |
 * |--------------|-------------------|------|
 * | Hardware     | None - RAM buffers| RSPI registers |
 * | Timing       | Instant           | SPI clock delays |
 * | Errors       | Injected          | Actual bus errors |
 *
 * @par Usage in Tests:
 * - tests/test_rspi.c - RSPI HAL tests
 *
 * @see rspi.h Real RSPI hardware layer
 * @see tests/test_rspi.c Test file
 *
 * @par NASA Power of 10 Compliance:
 * - [OK] Static allocation, no recursion, bounded loops
 *
 * @par SOLID Principles:
 * - D: Dependency Inversion - Tests depend on interface, not implementation
 *
 * @author Locked, Inc.
 * @date 2026-01-05
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
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
 * Configuration Constants
 * =============================================================================
 */

/** @brief Mock RSPI buffer and tracking constants */
typedef enum : uint16_t {
  k_mock_rspi_max_channels     = 3,    /**< RSPI0, RSPI1, RSPI2 */
  k_mock_rspi_buffer_size      = 2048, /**< Max transfer buffer size */
  k_mock_rspi_call_history_max = 64,   /**< Max recorded calls */
  k_mock_rspi_func_name_max    = 48,   /**< Max function name length */
} mock_rspi_constants_t;

/* =============================================================================
 * Call Record Structure
 * =============================================================================
 */

/**
 * @brief Record of a single function call
 */
typedef struct {
  char     function[k_mock_rspi_func_name_max]; /**< Function name */
  uint8_t  channel;                             /**< RSPI channel */
  uint32_t arg1;                                /**< First argument */
  uint32_t arg2;                                /**< Second argument */
  rx_err_t return_value;                        /**< Return value */
} mock_rspi_call_t;

/* =============================================================================
 * Channel State Structure
 * =============================================================================
 */

/**
 * @brief State of a single RSPI channel
 */
typedef struct {
  bool     initialized;                      /**< Channel initialized */
  uint8_t  spi_mode;                         /**< Configured SPI mode */
  bool     use_16bit;                        /**< 16-bit frame mode */
  bool     data_available;                   /**< RX data available */
  bool     write_ready;                      /**< TX ready */
  uint8_t  rx_data[k_mock_rspi_buffer_size]; /**< RX buffer for injection */
  uint32_t rx_len;                           /**< RX data length */
  uint32_t rx_pos;                           /**< RX read position */
  uint8_t  tx_data[k_mock_rspi_buffer_size]; /**< TX buffer for capture */
  uint32_t tx_len;                           /**< TX data length */
} mock_rspi_channel_t;

/* =============================================================================
 * Mock RSPI State Structure
 * =============================================================================
 */

/**
 * @brief Controller mode channel state
 */
typedef struct {
  bool     initialized;                         /**< Controller mode initialized */
  uint8_t  spi_mode;                            /**< SPI mode (0-3) */
  uint32_t freq_hz;                             /**< Clock frequency */
  uint8_t  cs_port;                             /**< Chip select port */
  uint8_t  cs_pin;                              /**< Chip select pin */
  bool     cs_active;                           /**< CS currently active */
  uint16_t last_tx_data;                        /**< Last transmitted data */
  uint16_t next_rx_data;                        /**< Next data to receive */
  uint8_t  tx_history[k_mock_rspi_buffer_size]; /**< TX history buffer */
  uint32_t tx_history_len;                      /**< TX history length */
} mock_rspi_controller_t;

/**
 * @brief Mock RSPI state structure
 */
typedef struct {
  /* Channel states */
  mock_rspi_channel_t channels[k_mock_rspi_max_channels]; /**< Channel states */

  /* Controller mode states */
  mock_rspi_controller_t controller[k_mock_rspi_max_channels]; /**< Controller states */

  /* Call tracking */
  mock_rspi_call_t call_history[k_mock_rspi_call_history_max]; /**< Call history */
  uint32_t         call_count;                                 /**< Total calls */
  uint32_t         call_write_index;                           /**< Next index */

  /* Configurable return values for error injection */
  rx_err_t next_init_return;      /**< Return for next init call */
  rx_err_t next_transfer_return;  /**< Return for next transfer call */
  rx_err_t next_available_return; /**< Return for next available call */
  rx_err_t next_ready_return;     /**< Return for next ready call */
  rx_err_t next_deinit_return;    /**< Return for next deinit call */

  /* Controller mode return values */
  rx_err_t next_controller_init_return;     /**< Return for controller init */
  rx_err_t next_controller_transfer_return; /**< Return for controller transfer */
  rx_err_t next_controller_cs_return;       /**< Return for CS control */
  rx_err_t next_controller_deinit_return;   /**< Return for controller deinit */

  /* Statistics */
  uint32_t init_calls;      /**< Number of init calls */
  uint32_t deinit_calls;    /**< Number of deinit calls */
  uint32_t transfer_calls;  /**< Number of transfer calls */
  uint32_t available_calls; /**< Number of available check calls */
  uint32_t ready_calls;     /**< Number of ready check calls */

  /* Controller mode statistics */
  uint32_t controller_init_calls;     /**< Controller init calls */
  uint32_t controller_transfer_calls; /**< Controller transfer calls */
  uint32_t controller_cs_calls;       /**< Controller CS calls */
  uint32_t controller_deinit_calls;   /**< Controller deinit calls */
} mock_rspi_t;

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

extern mock_rspi_t g_mock_rspi;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize the mock RSPI
 *
 * Clears all state and sets default return values.
 *
 * @param mock Mock instance (use NULL for global g_mock_rspi)
 * @return k_rx_ok on success
 */
rx_err_t mock_rspi_init(mock_rspi_t* mock);

/**
 * @brief Deinitialize the mock RSPI
 *
 * @param mock Mock instance (use NULL for global g_mock_rspi)
 * @return k_rx_ok on success
 */
rx_err_t mock_rspi_deinit(mock_rspi_t* mock);

/**
 * @brief Clear all mock state without full reinitialization
 *
 * @param mock Mock instance (use NULL for global g_mock_rspi)
 * @return k_rx_ok on success
 */
rx_err_t mock_rspi_clear(mock_rspi_t* mock);

/* =============================================================================
 * Error Injection Functions
 * =============================================================================
 */

/**
 * @brief Set return value for next rspi_init_peripheral call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_init_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_peripheral_transfer call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_transfer_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_peripheral_read_available call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_available_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_peripheral_write_ready call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_ready_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_deinit call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_deinit_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_init_controller call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_controller_init_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_controller_transfer_16bit call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_controller_transfer_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_controller_set_cs call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_controller_cs_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_controller_deinit call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_controller_deinit_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set the data that will be returned by the next controller transfer
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel
 * @param rx_data Data to return
 */
void mock_rspi_set_controller_rx_data(mock_rspi_t* mock, rspi_channel_t channel, uint16_t rx_data);

/**
 * @brief Get the last data transmitted by controller transfer
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel
 * @return Last transmitted 16-bit data
 */
uint16_t mock_rspi_get_controller_last_tx(mock_rspi_t* mock, rspi_channel_t channel);

/**
 * @brief Clear controller mode state for a channel
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel
 */
void mock_rspi_clear_controller_channel(mock_rspi_t* mock, rspi_channel_t channel);

/* =============================================================================
 * Data Injection/Extraction Functions
 * =============================================================================
 */

/**
 * @brief Inject data for the next SPI receive
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel (0-2)
 * @param data Data to inject
 * @param len Data length
 * @return k_rx_ok on success
 */
rx_err_t mock_rspi_inject_rx_data(mock_rspi_t*   mock,
                                  rspi_channel_t channel,
                                  const uint8_t* data,
                                  uint32_t       len);

/**
 * @brief Get data that was transmitted via SPI
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel (0-2)
 * @param data Output buffer
 * @param max_len Maximum bytes to read
 * @param actual_len Actual bytes read
 * @return k_rx_ok on success
 */
rx_err_t mock_rspi_get_tx_data(mock_rspi_t*   mock,
                               rspi_channel_t channel,
                               uint8_t*       data,
                               uint32_t       max_len,
                               uint32_t*      actual_len);

/**
 * @brief Set data availability for a channel
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel (0-2)
 * @param available True if data is available
 */
void mock_rspi_set_data_available(mock_rspi_t* mock, rspi_channel_t channel, bool available);

/**
 * @brief Set write ready status for a channel
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel (0-2)
 * @param ready True if write ready
 */
void mock_rspi_set_write_ready(mock_rspi_t* mock, rspi_channel_t channel, bool ready);

/**
 * @brief Clear a channel's buffers
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel (0-2)
 */
void mock_rspi_clear_channel(mock_rspi_t* mock, rspi_channel_t channel);

/* =============================================================================
 * Call Tracking Functions
 * =============================================================================
 */

/**
 * @brief Check if a function was called
 *
 * @param mock Mock instance (use NULL for global)
 * @param func Function name to check
 * @return true if function was called at least once
 */
bool mock_rspi_was_called(mock_rspi_t* mock, const char* func);

/**
 * @brief Get number of times a function was called
 *
 * @param mock Mock instance (use NULL for global)
 * @param func Function name to check
 * @return Number of calls
 */
uint32_t mock_rspi_get_call_count(mock_rspi_t* mock, const char* func);

/**
 * @brief Get the last call record for a function
 *
 * @param mock Mock instance (use NULL for global)
 * @param func Function name
 * @param out_call Output call record
 * @return k_rx_ok on success, k_rx_err_not_found if never called
 */
rx_err_t mock_rspi_get_last_call(mock_rspi_t* mock, const char* func, mock_rspi_call_t* out_call);

/**
 * @brief Clear call history
 *
 * @param mock Mock instance (use NULL for global)
 */
void mock_rspi_clear_calls(mock_rspi_t* mock);

/**
 * @brief Record a function call (internal use)
 *
 * @param mock Mock instance
 * @param func Function name
 * @param channel RSPI channel
 * @param arg1 First argument
 * @param arg2 Second argument
 * @param ret Return value
 */
void mock_rspi_record_call(mock_rspi_t* mock,
                           const char*  func,
                           uint8_t      channel,
                           uint32_t     arg1,
                           uint32_t     arg2,
                           rx_err_t     ret);

#ifdef __cplusplus
}
#endif
