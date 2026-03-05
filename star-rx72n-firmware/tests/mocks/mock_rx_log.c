// SPDX-License-Identifier: MIT
/* tests/mocks/mock_rx_log.c */

/**
 * @file mock_rx_log.c
 * @brief Mock RX Log and UART Implementation for Unit Testing
 *
 * Provides stub implementations of UART functions used by rx_log.h
 * for host-side testing.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
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
