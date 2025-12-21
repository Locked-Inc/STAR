/* src/hardware/uart.c */

/**
 * @file uart.c
 * @brief UART Driver for RX72N Debug Output
 *
 * Simple UART driver for SCI5 (Serial Communication Interface 5).
 * Provides basic transmit-only functionality for printf debugging.
 */

#include <stdbool.h>
#include <stdint.h>

#include "hardware.h"
#include "rx72n_regs.h"

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/* UART baud rate (115200 bps) */
#define UART_BAUDRATE 115200

/* Calculate BRR value for baud rate
 * BRR = (PCLKB / (32 * 2^(2n-1) * baudrate)) - 1
 * For SCI with n=0 (PCLK/1):
 * BRR = (60,000,000 / (64 * 115200)) - 1 = 7.13 ≈ 7
 */
#define UART_BRR_VALUE 7

/* =============================================================================
 * UART Initialization
 * =============================================================================
 */

/**
 * @brief Initialize SCI5 for UART communication (TX only)
 *
 * Configuration:
 * - Baud rate: 115200 bps
 * - 8 data bits, 1 stop bit, no parity
 * - TX only (no RX)
 * - Clock: PCLKB (60 MHz)
 *
 * @return RX_OK on success
 */
rx_err_t uart_init(void)
{
  /* Disable SCI5 transmit/receive */
  SCI5.SCR = 0x00;

  /* Configure serial mode:
     * SMR: Async, 8-bit, no parity, 1 stop, PCLK/1 */
  SCI5.SMR = 0x00;

  /* Set baud rate */
  SCI5.BRR = UART_BRR_VALUE;

  /* Wait for at least 1 bit time (at least 8.68 us at 115200 bps)
     * Simple delay loop - should be > 520 cycles at 60 MHz */
  for (volatile int32_t i = 0; i < 1000; i++) {
    __asm__ volatile("nop");
  }

  /* Configure serial control:
     * SCR: Enable transmit, enable transmit interrupt if needed */
  SCI5.SCR = 0x20; /* TE=1 (transmit enable), RE=0, interrupts disabled */

  /* Configure serial extended mode (default is fine) */
  SCI5.SEMR = 0x00;

  /* Note: GPIO pins for SCI5 TX/RX need to be configured in PMR/MPC
     * This would require MPC (Multi-Function Pin Controller) registers
     * which are not yet defined. For now, assume pins are configured
     * by default or by external code. */

  /* UART init complete - can't use RX_LOG yet since UART is just now ready */

  return RX_OK;
}

/* =============================================================================
 * UART Transmit
 * =============================================================================
 */

/**
 * @brief Transmit a single byte via UART
 *
 * @param[in] data Byte to transmit
 */
void uart_putc(char data)
{
  /* Wait for transmit buffer to be empty (TDRE flag) */
  while ((SCI5.SSR & 0x80) == 0) {
    /* Wait */
  }

  /* Write data to transmit register */
  SCI5.TDR = (uint8_t)data;

  /* Clear TDRE flag by reading SSR then writing 0 */
  (void)SCI5.SSR;
  SCI5.SSR &= ~0x80;
}

/**
 * @brief Transmit a null-terminated string via UART
 *
 * @param[in] str Pointer to string to transmit
 */
void uart_puts(const char* str)
{
  if (!str) {
    return;
  }

  while (*str) {
    /* Convert \n to \r\n for terminal compatibility */
    if (*str == '\n') {
      uart_putc('\r');
    }
    uart_putc(*str++);
  }
}

/**
 * @brief Simple integer to string conversion and transmit
 *
 * @param[in] value Integer value to print
 */
void uart_putint(int32_t value)
{
  char     buffer[12]; /* Enough for -2147483648 */
  char*    p = buffer + sizeof(buffer) - 1;
  uint32_t abs_value;
  bool     is_negative = false;

  /* Handle negative numbers */
  if (value < 0) {
    is_negative = true;
    abs_value   = (uint32_t)(-value);
  } else {
    abs_value = (uint32_t)value;
  }

  /* Null terminate */
  *p = '\0';

  /* Convert to string (reverse order) */
  do {
    *--p = '0' + (abs_value % 10);
    abs_value /= 10;
  } while (abs_value > 0);

  /* Add minus sign if negative */
  if (is_negative) {
    *--p = '-';
  }

  /* Transmit the string */
  uart_puts(p);
}

/**
 * @brief Print hexadecimal value
 *
 * @param[in] value Value to print in hex
 * @param[in] digits Number of hex digits to print (1-8)
 */
void uart_puthex(uint32_t value, uint8_t digits)
{
  static const char s_hex[] = "0123456789ABCDEF";

  uart_puts("0x");

  /* Clamp digits to valid range */
  if (digits > 8) {
    digits = 8;
  }
  if (digits == 0) {
    digits = 1;
  }

  /* Print hex digits from most significant */
  for (int32_t i = digits - 1; i >= 0; i--) {
    uint8_t nibble = (value >> (i * 4)) & 0x0F;
    uart_putc(s_hex[nibble]);
  }
}
