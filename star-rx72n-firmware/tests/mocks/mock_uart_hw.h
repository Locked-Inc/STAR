/**
 * @file mock_uart_hw.h
 * @brief Mock UART hardware layer for serial communication testing
 *
 * @details
 * Provides test double for UART hardware layer (low-level SCI interface) to enable
 * unit testing of UART bus drivers (rx_bus_uart) without actual serial hardware.
 *
 * Enables testing of: UART TX/RX operations, Baud rate configuration, Frame error
 * handling, FIFO buffer management, Bus initialization
 *
 * @par Mock Capabilities: TX FIFO (captures transmitted data), RX FIFO (inject
 * test data), Error injection (frame error, overrun, etc.), Init state tracking
 *
 * @par Usage Example:
 * @code
 * setUp() {
 *   mock_uart_hw_init();
 *   mock_uart_hw_inject_rx_data(0, data, len);  // Simulate RX
 * }
 * test() {
 *   uart_send(0, data, len);
 *   const uint8_t* tx = mock_uart_hw_get_tx_data(0);  // Verify TX
 * }
 * @endcode
 *
 * @par Usage: tests/test_rx_bus_uart.c
 * @see uart_hal.h Real UART HAL
 * @see mock_sci_regs.h SCI register mock
 * @par NASA Power of 10: [OK] Static FIFO buffers
 * @par SOLID: D - UART bus depends on hardware interface
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

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

/** @brief Mock UART constants */
typedef enum : uint16_t {
  k_mock_uart_fifo_size       = 256, /**< Size of TX/RX FIFO per channel */
  k_mock_uart_channel_count   = 13,  /**< Number of UART channels (0-12) */
  k_mock_uart_call_history_sz = 64,  /**< Size of call history buffer */
} mock_uart_constants_t;

/* =============================================================================
 * Types
 * =============================================================================
 */

/** @brief UART HAL function call types (for call history) */
typedef enum : uint8_t {
  k_mock_uart_call_init,
  k_mock_uart_call_deinit,
  k_mock_uart_call_putc,
  k_mock_uart_call_puts,
  k_mock_uart_call_write,
  k_mock_uart_call_getc,
  k_mock_uart_call_read,
  k_mock_uart_call_rx_available,
  k_mock_uart_call_clear_rx_errors,
} mock_uart_call_type_t;

/** @brief UART HAL function call record */
typedef struct {
  mock_uart_call_type_t type;    /**< Call type */
  uint8_t               channel; /**< Channel number */
  uint32_t              param1;  /**< First parameter (baudrate, length, etc.) */
} mock_uart_call_t;

/** @brief Per-channel UART state */
typedef struct {
  uint8_t  tx_fifo[k_mock_uart_fifo_size]; /**< TX data buffer */
  uint16_t tx_head;                        /**< TX write position */
  uint16_t tx_tail;                        /**< TX read position */
  uint8_t  rx_fifo[k_mock_uart_fifo_size]; /**< RX data buffer */
  uint16_t rx_head;                        /**< RX write position */
  uint16_t rx_tail;                        /**< RX read position */
  bool     initialized;                    /**< Channel initialized? */
  uint32_t baudrate;                       /**< Configured baud rate */
} mock_uart_channel_t;

/** @brief Global mock UART state */
typedef struct {
  mock_uart_channel_t channels[k_mock_uart_channel_count];       /**< Per-channel state */
  mock_uart_call_t    call_history[k_mock_uart_call_history_sz]; /**< Call history */
  uint16_t            call_count;                                /**< Number of calls recorded */
  rx_err_t            next_error;       /**< Error to return on next call (all functions) */
  bool                error_set;        /**< Whether general error injection is active */
  rx_err_t            next_write_error; /**< Error to return on next write call only */
  bool                write_error_set;  /**< Whether write-only error injection is active */
  rx_err_t            next_read_error;  /**< Error to return on next read call only */
  bool                read_error_set;   /**< Whether read-only error injection is active */
} mock_uart_hw_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

/** @brief Global mock UART state instance */
extern mock_uart_hw_t g_mock_uart;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock UART state
 *
 * Clears all FIFOs, marks all channels as uninitialized,
 * and resets call history.
 */
void mock_uart_hw_init(void);

/**
 * @brief Deinitialize mock UART state
 *
 * Same as init - clears all state.
 */
void mock_uart_hw_deinit(void);

/* =============================================================================
 * RX Data Injection
 * =============================================================================
 */

/**
 * @brief Inject data into RX FIFO for a channel
 *
 * Simulates received data that will be returned by uart_getc_channel()
 * and uart_read_channel() calls.
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] data Pointer to data to inject
 * @param[in] len Number of bytes to inject
 *
 * @return Number of bytes actually injected (may be less if FIFO full)
 */
uint16_t mock_uart_hw_inject_rx_data(uint8_t channel, const uint8_t* data, uint16_t len);

/**
 * @brief Get number of bytes available in RX FIFO
 *
 * @param[in] channel SCI channel (0-12)
 *
 * @return Number of bytes available
 */
uint16_t mock_uart_hw_rx_available(uint8_t channel);

/* =============================================================================
 * TX Data Retrieval
 * =============================================================================
 */

/**
 * @brief Get transmitted data from TX FIFO
 *
 * Retrieves data that was "transmitted" by uart_putc_channel(),
 * uart_puts_channel(), or uart_write_channel() calls.
 *
 * @param[in] channel SCI channel (0-12)
 * @param[out] data Buffer to store transmitted data
 * @param[in] max_len Maximum bytes to retrieve
 *
 * @return Number of bytes retrieved
 */
uint16_t mock_uart_hw_get_tx_data(uint8_t channel, uint8_t* data, uint16_t max_len);

/**
 * @brief Get number of bytes in TX FIFO
 *
 * @param[in] channel SCI channel (0-12)
 *
 * @return Number of bytes transmitted
 */
uint16_t mock_uart_hw_tx_count(uint8_t channel);

/**
 * @brief Clear TX FIFO for a channel
 *
 * @param[in] channel SCI channel (0-12)
 */
void mock_uart_hw_clear_tx(uint8_t channel);

/* =============================================================================
 * Error Injection
 * =============================================================================
 */

/**
 * @brief Set error to return on next UART HAL call
 *
 * After the next HAL function call, the error injection is cleared.
 *
 * @param[in] err Error code to return
 */
void mock_uart_hw_set_next_error(rx_err_t err);

/**
 * @brief Set error to return on next UART write call only
 *
 * Only consumed by uart_write_channel(); read and availability calls are
 * not affected. After the next write call the injection is cleared.
 *
 * @param[in] err Error code to return
 */
void mock_uart_hw_set_next_write_error(rx_err_t err);

/**
 * @brief Set error to return on next UART read call only
 *
 * Only consumed by uart_read_channel(); uart_rx_available() and write calls
 * are not affected. After the next read call the injection is cleared.
 *
 * @param[in] err Error code to return
 */
void mock_uart_hw_set_next_read_error(rx_err_t err);

/**
 * @brief Clear any pending error injection (all error slots)
 */
void mock_uart_hw_clear_error(void);

/* =============================================================================
 * Call History
 * =============================================================================
 */

/**
 * @brief Get call history entry
 *
 * @param[in] index Index in call history (0 = first call)
 *
 * @return Pointer to call record, or NULL if index out of range
 */
const mock_uart_call_t* mock_uart_hw_get_call(uint16_t index);

/**
 * @brief Get total number of recorded calls
 *
 * @return Number of calls in history
 */
uint16_t mock_uart_hw_get_call_count(void);

/**
 * @brief Clear call history
 */
void mock_uart_hw_clear_history(void);

/* =============================================================================
 * State Inspection
 * =============================================================================
 */

/**
 * @brief Check if channel is initialized
 *
 * @param[in] channel SCI channel (0-12)
 *
 * @return True if initialized
 */
bool mock_uart_hw_is_initialized(uint8_t channel);

/**
 * @brief Get configured baud rate for channel
 *
 * @param[in] channel SCI channel (0-12)
 *
 * @return Baud rate, or 0 if not initialized
 */
uint32_t mock_uart_hw_get_baudrate(uint8_t channel);

/* UART HAL functions are declared in hardware.h (included above). */

#ifdef __cplusplus
}
#endif
