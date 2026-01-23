/* lib/rx_bus/inc/rx_bus_uart.h */

/**
 * @file rx_bus_uart.h
 * @brief UART bus abstraction for RX72N
 * @details
 * Provides bus manager integration for SCI UART operations.
 * Wraps the low-level UART HAL with bus abstraction pattern.
 *
 * Supports:
 * - Multi-channel operation (SCI0-12)
 * - TX + RX full duplex
 * - Character and buffer-based I/O
 * - Non-blocking RX with bytes_read count
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_BUS_UART_H
#define STAR_RX72N_BUS_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * UART Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize UART bus through bus manager
 *
 * Configures SCI channel and pins for UART communication.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name UART bus name
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_arg if bus is not UART type
 * @return k_rx_err_timeout if mutex timeout
 * @return k_rx_err_hw_init_failed if UART hardware init fails
 */
rx_err_t rx_bus_uart_init(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Write data buffer to UART through bus manager
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name UART bus name
 * @param[in] data Pointer to data to write
 * @param[in] length Number of bytes to write
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager, bus_name, or data is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if UART timeout or mutex timeout
 */
rx_err_t rx_bus_uart_write(rx_bus_manager_t* manager,
                           const char*       bus_name,
                           const uint8_t*    data,
                           uint16_t          length);

/**
 * @brief Read data from UART through bus manager
 *
 * Non-blocking read - returns immediately with bytes available.
 * Check bytes_read to determine how many bytes were actually received.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name UART bus name
 * @param[out] data Pointer to buffer for received data
 * @param[in] length Maximum bytes to read
 * @param[out] bytes_read Actual number of bytes read (can be 0 if no data)
 *
 * @return k_rx_ok on success (check bytes_read for actual count)
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_uart_read(rx_bus_manager_t* manager,
                          const char*       bus_name,
                          uint8_t*          data,
                          uint16_t          length,
                          uint16_t*         bytes_read);

/**
 * @brief Write single character to UART through bus manager
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name UART bus name
 * @param[in] c Character to write
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if UART timeout or mutex timeout
 */
rx_err_t rx_bus_uart_putc(rx_bus_manager_t* manager, const char* bus_name, char c);

/**
 * @brief Write null-terminated string to UART through bus manager
 *
 * Converts '\n' to '\r\n' for proper terminal output.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name UART bus name
 * @param[in] str Null-terminated string to write
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if UART timeout or mutex timeout
 */
rx_err_t rx_bus_uart_puts(rx_bus_manager_t* manager, const char* bus_name, const char* str);

/**
 * @brief Read single character from UART through bus manager
 *
 * Non-blocking - returns k_rx_err_empty if no data available.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name UART bus name
 * @param[out] c Character received
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_empty if no data available
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_uart_getc(rx_bus_manager_t* manager, const char* bus_name, char* c);

/**
 * @brief Check if RX data is available on UART
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name UART bus name
 * @param[out] available True if at least one byte is available
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer parameter is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if mutex timeout
 */
rx_err_t rx_bus_uart_rx_available(rx_bus_manager_t* manager, const char* bus_name, bool* available);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_UART_H */
