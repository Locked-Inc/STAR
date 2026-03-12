/**
 * @file mock_rx_log.c
 * @brief Mock RX Log and UART Implementation for Unit Testing
 *
 * Provides stub implementations of UART functions used by rx_log.h
 * for host-side testing.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

/* =============================================================================
 * UART Debug Stub Implementations
 *
 * These are called by the inline logging functions in rx_log.h.
 * =============================================================================
 */

void uart_debug_putc(char data)
{
  (void)data;
  /* No-op for testing */
}

void uart_debug_puts(const char* str)
{
  (void)str;
  /* No-op for testing */
}

void uart_debug_putint(int32_t value)
{
  (void)value;
  /* No-op for testing */
}

void uart_debug_puthex(uint32_t value, uint8_t digits)
{
  (void)value;
  (void)digits;
  /* No-op for testing */
}

void rx_log_usb_putc(char c)
{
  (void)c;
  /* No-op for testing */
}

void rx_log_usb_puts(const char* str)
{
  (void)str;
  /* No-op for testing */
}

/**
 * @brief Stub for rx_log_usb_putint() -- no-op
 *
 * @details
 * Discards the integer value without writing to USB CDC Port 2.
 * This stub satisfies the linker for tests that pull in rx_log.h's
 * runtime LOG_PUTINT macro but do not exercise USB output.
 *
 * @param[in] value Integer value to print (ignored)
 *
 * @pre  None -- stub accepts any value
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to USB CDC
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void rx_log_usb_putint(int32_t value)
{
  (void)value;
}

/**
 * @brief Stub for rx_log_usb_puthex() -- no-op
 *
 * @details
 * Discards the hex value and digit count without writing to USB CDC Port 2.
 * This stub satisfies the linker for tests that pull in rx_log.h's
 * runtime LOG_PUTHEX macro but do not exercise USB output.
 *
 * @param[in] value  Unsigned value to print as hex (ignored)
 * @param[in] digits Number of hex digits to show (ignored)
 *
 * @pre  None -- stub accepts any value and digit count
 * @pre  Caller context is single-threaded test execution
 * @post No bytes written to USB CDC
 * @post Global state unchanged
 *
 * @since Version 1.0.0
 */
void rx_log_usb_puthex(uint32_t value, uint8_t digits)
{
  (void)value;
  (void)digits;
}
