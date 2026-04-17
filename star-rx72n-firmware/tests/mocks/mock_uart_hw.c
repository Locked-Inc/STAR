/**
 * @file mock_uart_hw.c
 * @brief Mock UART HAL Function Implementation
 *
 * Provides mock UART HAL functions with TX/RX FIFO buffers for testing.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_uart_hw.h"

#include "hardware.h"
#include "mock_sci_regs.h"

/* =============================================================================
 * Global State
 * =============================================================================
 */

mock_uart_hw_t g_mock_uart;

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Record a call in the history
 */
static void internal_record_call(mock_uart_call_type_t type, uint8_t channel, uint32_t param1)
{
  if (g_mock_uart.call_count < k_mock_uart_call_history_sz) {
    mock_uart_call_t* call = &g_mock_uart.call_history[g_mock_uart.call_count];
    call->type             = type;
    call->channel          = channel;
    call->param1           = param1;
    g_mock_uart.call_count++;
  }
}

/**
 * @brief Check and consume the general error injection
 */
static rx_err_t internal_check_error(void)
{
  if (g_mock_uart.error_set) {
    rx_err_t err          = g_mock_uart.next_error;
    g_mock_uart.error_set = false;
    return err;
  }
  return k_rx_ok;
}

/**
 * @brief Check and consume the write-only error injection
 */
static rx_err_t internal_check_write_error(void)
{
  if (g_mock_uart.write_error_set) {
    rx_err_t err                = g_mock_uart.next_write_error;
    g_mock_uart.write_error_set = false;
    return err;
  }
  return k_rx_ok;
}

/**
 * @brief Check and consume the read-only error injection
 */
static rx_err_t internal_check_read_error(void)
{
  if (g_mock_uart.read_error_set) {
    rx_err_t err               = g_mock_uart.next_read_error;
    g_mock_uart.read_error_set = false;
    return err;
  }
  return k_rx_ok;
}

/**
 * @brief Get number of bytes in a circular FIFO
 */
static uint16_t internal_fifo_count(uint16_t head, uint16_t tail)
{
  if (head >= tail) {
    return head - tail;
  }
  return k_mock_uart_fifo_size - tail + head;
}

/**
 * @brief Get available space in a circular FIFO
 */
static uint16_t internal_fifo_space(uint16_t head, uint16_t tail)
{
  return k_mock_uart_fifo_size - 1 - internal_fifo_count(head, tail);
}

typedef enum : uint32_t {
  k_mock_uart_tdre_timeout = 100000,
} mock_uart_timeout_t;

static rx_err_t internal_wait_for_tdre(uint8_t channel)
{
  uint32_t timeout = k_mock_uart_tdre_timeout;
  while ((g_mock_sci[channel].ssr & k_mock_sci_ssr_tdre) == 0 && timeout > 0) {
    timeout--;
  }
  if (timeout == 0) {
    return k_rx_err_timeout;
  }
  return k_rx_ok;
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

void mock_uart_hw_init(void)
{
  uint8_t* const p = (uint8_t*)&g_mock_uart;
  for (uint32_t i = 0U; i < sizeof(g_mock_uart); i++) {
    p[i] = 0U;
  }
}

void mock_uart_hw_deinit(void)
{
  mock_uart_hw_init();
}

/* =============================================================================
 * RX Data Injection
 * =============================================================================
 */

uint16_t mock_uart_hw_inject_rx_data(uint8_t channel, const uint8_t* data, uint16_t len)
{
  if (channel >= k_mock_uart_channel_count || data == nullptr) {
    return 0;
  }

  mock_uart_channel_t* ch       = &g_mock_uart.channels[channel];
  uint16_t             space    = internal_fifo_space(ch->rx_head, ch->rx_tail);
  uint16_t             to_write = (len < space) ? len : space;

  for (uint16_t i = 0; i < to_write; i++) {
    ch->rx_fifo[ch->rx_head] = data[i];
    ch->rx_head              = (ch->rx_head + 1) % k_mock_uart_fifo_size;
  }

  return to_write;
}

uint16_t mock_uart_hw_rx_available(uint8_t channel)
{
  if (channel >= k_mock_uart_channel_count) {
    return 0;
  }

  mock_uart_channel_t* ch = &g_mock_uart.channels[channel];
  return internal_fifo_count(ch->rx_head, ch->rx_tail);
}

/* =============================================================================
 * TX Data Retrieval
 * =============================================================================
 */

uint16_t mock_uart_hw_get_tx_data(uint8_t channel, uint8_t* data, uint16_t max_len)
{
  if (channel >= k_mock_uart_channel_count || data == nullptr) {
    return 0;
  }

  mock_uart_channel_t* ch      = &g_mock_uart.channels[channel];
  uint16_t             count   = internal_fifo_count(ch->tx_head, ch->tx_tail);
  uint16_t             to_read = (max_len < count) ? max_len : count;

  for (uint16_t i = 0; i < to_read; i++) {
    data[i]     = ch->tx_fifo[ch->tx_tail];
    ch->tx_tail = (ch->tx_tail + 1) % k_mock_uart_fifo_size;
  }

  return to_read;
}

uint16_t mock_uart_hw_tx_count(uint8_t channel)
{
  if (channel >= k_mock_uart_channel_count) {
    return 0;
  }

  mock_uart_channel_t* ch = &g_mock_uart.channels[channel];
  return internal_fifo_count(ch->tx_head, ch->tx_tail);
}

void mock_uart_hw_clear_tx(uint8_t channel)
{
  if (channel < k_mock_uart_channel_count) {
    mock_uart_channel_t* ch = &g_mock_uart.channels[channel];
    ch->tx_head             = 0;
    ch->tx_tail             = 0;
  }
}

/* =============================================================================
 * Error Injection
 * =============================================================================
 */

void mock_uart_hw_set_next_error(rx_err_t err)
{
  g_mock_uart.next_error = err;
  g_mock_uart.error_set  = true;
}

void mock_uart_hw_clear_error(void)
{
  g_mock_uart.error_set       = false;
  g_mock_uart.write_error_set = false;
  g_mock_uart.read_error_set  = false;
}

void mock_uart_hw_set_next_write_error(rx_err_t err)
{
  g_mock_uart.next_write_error = err;
  g_mock_uart.write_error_set  = true;
}

void mock_uart_hw_set_next_read_error(rx_err_t err)
{
  g_mock_uart.next_read_error = err;
  g_mock_uart.read_error_set  = true;
}

/* =============================================================================
 * Call History
 * =============================================================================
 */

const mock_uart_call_t* mock_uart_hw_get_call(uint16_t index)
{
  if (index < g_mock_uart.call_count) {
    return &g_mock_uart.call_history[index];
  }
  return nullptr;
}

uint16_t mock_uart_hw_get_call_count(void)
{
  return g_mock_uart.call_count;
}

void mock_uart_hw_clear_history(void)
{
  g_mock_uart.call_count = 0;
}

/* =============================================================================
 * State Inspection
 * =============================================================================
 */

bool mock_uart_hw_is_initialized(uint8_t channel)
{
  if (channel < k_mock_uart_channel_count) {
    return g_mock_uart.channels[channel].initialized;
  }
  return false;
}

uint32_t mock_uart_hw_get_baudrate(uint8_t channel)
{
  if (channel < k_mock_uart_channel_count) {
    return g_mock_uart.channels[channel].baudrate;
  }
  return 0;
}

/* =============================================================================
 * Mock UART HAL Functions
 *
 * These replace the real uart_*_channel functions during testing.
 * They use the mock SCI registers and track state.
 * =============================================================================
 */

rx_err_t uart_init_channel(const uart_channel_config_t* config)
{
  if (config == nullptr) {
    return k_rx_err_null_ptr;
  }

  uint8_t  channel  = config->channel;
  uint32_t baudrate = config->baudrate;

  /* Pin parameters are ignored in mock - no actual hardware config */
  (void)config->tx_gpio;
  (void)config->rx_gpio;

  internal_record_call(k_mock_uart_call_init, channel, baudrate);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (channel >= k_mock_uart_channel_count) {
    return k_rx_err_invalid_arg;
  }

  if (g_mock_uart.channels[channel].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Mark channel as initialized */
  g_mock_uart.channels[channel].initialized = true;
  g_mock_uart.channels[channel].baudrate    = baudrate;

  /* Configure mock SCI registers */
  g_mock_sci[channel].scr = k_mock_sci_scr_te | k_mock_sci_scr_re;
  g_mock_sci[channel].ssr = k_mock_sci_ssr_tdre;

  return k_rx_ok;
}

rx_err_t uart_deinit_channel(uart_channel_t channel)
{
  internal_record_call(k_mock_uart_call_deinit, (uint8_t)channel, 0);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if ((uint8_t)channel >= k_mock_uart_channel_count) {
    return k_rx_err_invalid_arg;
  }

  /* Mark channel as not initialized */
  g_mock_uart.channels[channel].initialized = false;
  g_mock_uart.channels[channel].baudrate    = 0;

  /* Clear mock SCI registers */
  g_mock_sci[channel].scr = 0;

  return k_rx_ok;
}

rx_err_t uart_putc_channel(uart_channel_t channel, char data)
{
  internal_record_call(k_mock_uart_call_putc, (uint8_t)channel, (uint8_t)data);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if ((uint8_t)channel >= k_mock_uart_channel_count) {
    return k_rx_err_invalid_arg;
  }

  if (!g_mock_uart.channels[channel].initialized) {
    return k_rx_err_invalid_state;
  }

  rx_err_t tdre_status = internal_wait_for_tdre(channel);
  if (tdre_status != k_rx_ok) {
    return tdre_status;
  }

  /* Write to TX FIFO */
  mock_uart_channel_t* ch    = &g_mock_uart.channels[channel];
  uint16_t             space = internal_fifo_space(ch->tx_head, ch->tx_tail);
  if (space > 0) {
    ch->tx_fifo[ch->tx_head] = (uint8_t)data;
    ch->tx_head              = (ch->tx_head + 1) % k_mock_uart_fifo_size;
  } else {
    return k_rx_err_busy;
  }

  g_mock_sci[channel].tdr = (uint8_t)data;
  mock_sci_set_tdre(channel, true);

  return k_rx_ok;
}

rx_err_t uart_puts_channel(uart_channel_t channel, const char* str)
{
  internal_record_call(k_mock_uart_call_puts, (uint8_t)channel, 0);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (str == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((uint8_t)channel >= k_mock_uart_channel_count) {
    return k_rx_err_invalid_arg;
  }

  if (!g_mock_uart.channels[channel].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Write string with \n to \r\n conversion */
  while (*str) {
    if (*str == '\n') {
      err = uart_putc_channel(channel, '\r');
      if (err != k_rx_ok) {
        return err;
      }
    }
    err = uart_putc_channel(channel, *str++);
    if (err != k_rx_ok) {
      return err;
    }
  }

  return k_rx_ok;
}

rx_err_t uart_write_channel(uart_channel_t channel, const uint8_t* data, uint16_t length)
{
  internal_record_call(k_mock_uart_call_write, (uint8_t)channel, length);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_check_write_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (data == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((uint8_t)channel >= k_mock_uart_channel_count) {
    return k_rx_err_invalid_arg;
  }

  if (!g_mock_uart.channels[channel].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Write each byte */
  for (uint16_t i = 0; i < length; i++) {
    err = uart_putc_channel(channel, (char)data[i]);
    if (err != k_rx_ok) {
      return err;
    }
  }

  return k_rx_ok;
}

rx_err_t uart_getc_channel(uart_channel_t channel, char* data)
{
  internal_record_call(k_mock_uart_call_getc, (uint8_t)channel, 0);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (data == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((uint8_t)channel >= k_mock_uart_channel_count) {
    return k_rx_err_invalid_arg;
  }

  if (!g_mock_uart.channels[channel].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Read from RX FIFO */
  mock_uart_channel_t* ch = &g_mock_uart.channels[channel];
  if (ch->rx_head == ch->rx_tail) {
    return k_rx_err_empty;
  }

  *data       = (char)ch->rx_fifo[ch->rx_tail];
  ch->rx_tail = (ch->rx_tail + 1) % k_mock_uart_fifo_size;

  return k_rx_ok;
}

rx_err_t
uart_read_channel(uart_channel_t channel, uint8_t* data, uint16_t length, uint16_t* bytes_read)
{
  internal_record_call(k_mock_uart_call_read, (uint8_t)channel, length);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_check_read_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (data == nullptr || bytes_read == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((uint8_t)channel >= k_mock_uart_channel_count) {
    return k_rx_err_invalid_arg;
  }

  if (!g_mock_uart.channels[channel].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Read available bytes from RX FIFO */
  *bytes_read = 0;
  for (uint16_t i = 0; i < length; i++) {
    char c;
    err = uart_getc_channel(channel, &c);
    if (err == k_rx_err_empty) {
      break;
    }
    if (err != k_rx_ok) {
      return err;
    }
    data[i] = (uint8_t)c;
    (*bytes_read)++;
  }

  return k_rx_ok;
}

rx_err_t uart_rx_available(uart_channel_t channel, bool* available)
{
  internal_record_call(k_mock_uart_call_rx_available, (uint8_t)channel, 0);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (available == nullptr) {
    return k_rx_err_null_ptr;
  }

  if ((uint8_t)channel >= k_mock_uart_channel_count) {
    return k_rx_err_invalid_arg;
  }

  if (!g_mock_uart.channels[channel].initialized) {
    return k_rx_err_invalid_state;
  }

  mock_uart_channel_t* ch = &g_mock_uart.channels[channel];
  *available              = (bool)(ch->rx_head != ch->rx_tail);

  return k_rx_ok;
}

/* =============================================================================
 * Legacy Debug UART Wrappers
 * =============================================================================
 */

/** @brief Debug UART configuration constants */
typedef enum : uint32_t {
  k_debug_uart_channel  = 9,      /**< SCI9 channel for debug UART */
  k_debug_uart_baudrate = 921600, /**< 921600 baud for debug + comm console (CY7C65213 bridge) */
} debug_uart_config_t;

/** @brief Constants for uart_debug_putint / uart_debug_puthex */
typedef enum : uint8_t {
  k_int32_max_digits = 12,   /**< Max chars for int32 + sign + NUL: "-2147483648\0" */
  k_decimal_base     = 10,   /**< Decimal (base-10) radix */
  k_hex_max_digits   = 8,    /**< Max hex digits for uint32 (32-bit = 8 nibbles) */
  k_bits_per_nibble  = 4,    /**< Bits per hexadecimal nibble */
  k_nibble_mask      = 0x0F, /**< Mask for lowest nibble */
} debug_format_constants_t;

rx_err_t uart_debug_init(void)
{
  const uart_channel_config_t config = {
    .channel  = (uart_channel_t)k_debug_uart_channel,
    .baudrate = k_debug_uart_baudrate,
    .tx_gpio  = k_rx_pb_7,
    .rx_gpio  = k_rx_pb_6,
  };

  return uart_init_channel(&config);
}

/*
 * uart_debug_* functions: only needed for hardware (non-simulator) builds.
 * Under RX_SIMULATOR_MODE, rx_log.h provides static inline versions that
 * output to stdout via putchar().
 */
#ifndef RX_SIMULATOR_MODE
void uart_debug_putc(char data)
{
  (void)uart_putc_channel((uart_channel_t)k_debug_uart_channel, data);
}

void uart_debug_puts(const char* str)
{
  if (str == nullptr) {
    return;
  }
  while (*str) {
    if (*str == '\n') {
      uart_debug_putc('\r');
    }
    uart_debug_putc(*str++);
  }
}

void uart_debug_putint(int32_t value)
{
  char buffer[k_int32_max_digits];

  char*    p = buffer + sizeof(buffer) - 1;
  uint32_t abs_value;
  bool     is_negative = false;

  if (value < 0) {
    is_negative = true;
    abs_value   = (uint32_t)(-value);
  } else {
    abs_value = (uint32_t)value;
  }

  *p = '\0';
  do {
    *--p = (char)('0' + (abs_value % (uint32_t)k_decimal_base));
    abs_value /= (uint32_t)k_decimal_base;
  } while (abs_value > 0U);

  if (is_negative) {
    *--p = '-';
  }
  uart_debug_puts(p);
}

void uart_debug_puthex(uint32_t value, uint8_t digits)
{
  static const char s_hex[] = "0123456789ABCDEF";

  uart_debug_puts("0x");
  if (digits > k_hex_max_digits) {
    digits = k_hex_max_digits;
  }
  if (digits == 0U) {
    digits = 1U;
  }

  for (int32_t i = (int32_t)digits - 1; i >= 0; i--) {
    uint8_t nibble =
      (uint8_t)((value >> ((uint32_t)i * (uint32_t)k_bits_per_nibble)) & (uint32_t)k_nibble_mask);
    uart_debug_putc(s_hex[nibble]);
  }
}
#endif /* !RX_SIMULATOR_MODE */
