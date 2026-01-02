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
 * Private Definitions
 * =============================================================================
 */

/** @brief UART configuration constants */
typedef enum {
  k_uart_baudrate  = 115200, /**< Baud rate: 115200 bps */
  k_uart_brr_value = 7,      /**< BRR register value for 115200 bps at 60MHz PCLKB */
} uart_config_t;

/** @brief UART timing constants */
typedef enum {
  k_uart_bit_time_delay_cycles =
    1000, /**< Bit time delay (~8.68us at 115200 bps, >520 cycles at 60MHz) */
} uart_timing_t;

/** @brief SCI register values */
typedef enum {
  k_sci_scr_disabled   = 0x00, /**< SCR: All functions disabled */
  k_sci_scr_tx_enabled = 0x20, /**< SCR: Transmit enabled (TE=1) */
  k_sci_smr_async_8n1  = 0x00, /**< SMR: Async mode, 8 data bits, no parity, 1 stop bit, PCLK/1 */
  k_sci_semr_default   = 0x00, /**< SEMR: Default extended mode */
  k_sci_ssr_tdre_flag  = 0x80, /**< SSR: Transmit data register empty flag */
} sci_register_values_t;

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
 * @return k_rx_ok on success
 */
rx_err_t uart_init(void)
{
  /* Disable SCI5 transmit/receive */
  SCI5.scr = k_sci_scr_disabled;

  /* Configure serial mode: Async, 8-bit, no parity, 1 stop, PCLK/1 */
  SCI5.smr = k_sci_smr_async_8n1;

  /* Set baud rate */
  SCI5.brr = k_uart_brr_value;

  /* Wait for at least 1 bit time (at least 8.68us at 115200 bps) */
  /* NOTE: Busy-wait required - may run before ThreadX initialization */
  for (volatile int32_t i = 0; i < k_uart_bit_time_delay_cycles; i++) {
    __asm__ volatile("nop");
  }

  /* Configure serial control: Enable transmit */
  SCI5.scr = k_sci_scr_tx_enabled;

  /* Configure serial extended mode */
  SCI5.semr = k_sci_semr_default;

  /* Note: GPIO pins for SCI5 TX/RX need to be configured in PMR/MPC
     * This would require MPC (Multi-Function Pin Controller) registers
     * which are not yet defined. For now, assume pins are configured
     * by default or by external code. */

  /* UART init complete - can't use RX_LOG yet since UART is just now ready */

  return k_rx_ok;
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
  while ((SCI5.ssr & k_sci_ssr_tdre_flag) == 0) {
    /* Wait */
  }

  /* Write data to transmit register */
  SCI5.tdr = (uint8_t)data;

  /* Clear TDRE flag by reading SSR then writing 0 */
  (void)SCI5.ssr;
  SCI5.ssr &= ~0x80;
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
