/**
 * @file mock_rx_log.c
 * @brief Mock RX Log and UART Implementation for Unit Testing
 *
 * Provides stub implementations of UART functions used by rx_log.h
 * for host-side testing.
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include <stdint.h>

/* =============================================================================
 * UART Stub Implementations
 *
 * These are called by the inline logging functions in rx_log.h.
 * =============================================================================
 */

void uart_putc(char data)
{
  (void)data;
  /* No-op for testing */
}

void uart_puts(const char* str)
{
  (void)str;
  /* No-op for testing */
}

void uart_putint(int32_t value)
{
  (void)value;
  /* No-op for testing */
}

void uart_puthex(uint32_t value, uint8_t digits)
{
  (void)value;
  (void)digits;
  /* No-op for testing */
}
