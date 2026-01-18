/* lib/rx_hal/src/uart.c */

/**
 * @file uart.c
 * @brief Multi-Channel UART Driver for RX72N
 *
 * UART driver for SCI peripherals (channels 0-12).
 * Provides TX and RX functionality for any SCI channel.
 *
 * Default debug channel: SCI9 (PB7/TXD9, PB6/RXD9) connected to
 * CY7C65213 USB-UART bridge.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdbool.h>
#include <stdint.h>

#include "hardware.h"
#include "rx72n_clock.h"
#include "rx72n_regs.h"
#include "rx_mpc.h"
#include "rx_port_utils.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Private Definitions
 * =============================================================================
 */

/** @brief UART configuration constants */
typedef enum {
  k_uart_default_baudrate = 115200, /**< Default baud rate: 115200 bps */
} uart_config_t;

/** @brief UART timing constants */
typedef enum {
  k_uart_bit_time_delay_cycles =
    1000, /**< Bit time delay (~8.68us at 115200 bps, >520 cycles at 60MHz) */
} uart_timing_t;

/** @brief SCI register values */
typedef enum {
  k_sci_scr_disabled     = 0x00, /**< SCR: All functions disabled */
  k_sci_scr_tx_enabled   = 0x20, /**< SCR: Transmit enabled (TE=1) */
  k_sci_scr_rx_enabled   = 0x10, /**< SCR: Receive enabled (RE=1) */
  k_sci_scr_txrx_enabled = 0x30, /**< SCR: TX + RX enabled (TE=1, RE=1) */
  k_sci_smr_async_8n1    = 0x00, /**< SMR: Async mode, 8 data bits, no parity, 1 stop bit, PCLK/1 */
  k_sci_semr_default     = 0x00, /**< SEMR: Default extended mode */
  k_sci_ssr_tdre_flag    = 0x80, /**< SSR: Transmit data register empty flag */
  k_sci_ssr_rdrf_flag    = 0x40, /**< SSR: Receive data register full flag */
  k_sci_ssr_orer_flag    = 0x20, /**< SSR: Overrun error flag */
  k_sci_ssr_fer_flag     = 0x10, /**< SSR: Framing error flag */
  k_sci_ssr_per_flag     = 0x08, /**< SSR: Parity error flag */
  k_sci_ssr_error_mask   = 0x38, /**< SSR: All error flags mask */
} sci_register_values_t;

/** @brief Integer to string buffer constants */
typedef enum {
  k_uart_int_buffer_size = 12, /**< Buffer size for int32 to string (enough for -2147483648) */
  k_uart_base_10         = 10, /**< Base 10 for decimal conversion */
} uart_int_constants_t;

/** @brief Hex digit constants */
typedef enum {
  k_uart_hex_max_digits  = 8,    /**< Maximum hex digits to print (32-bit value) */
  k_uart_hex_min_digits  = 1,    /**< Minimum hex digits to print */
  k_uart_hex_zero_digits = 0,    /**< Zero digits value */
  k_uart_hex_nibble_bits = 4,    /**< Bits per hex nibble */
  k_uart_hex_nibble_mask = 0x0F, /**< Mask for hex nibble */
} uart_hex_constants_t;

/** @brief BRR calculation constants */
typedef enum {
  k_brr_divisor_n0 = 32,  /**< Divisor for n=0 (CKS=00): 64 * 2^(2n-1) = 32 */
  k_brr_multiplier = 4,   /**< Multiplier per CKS increment (2^2) */
  k_brr_max_value  = 255, /**< Maximum BRR register value */
  k_brr_min_value  = 0,   /**< Minimum BRR register value */
} brr_constants_t;

/** @brief Maximum SCI channels */
typedef enum {
  k_uart_min_channel  = 0,  /**< Minimum SCI channel */
  k_uart_max_channels = 13, /**< SCI channels 0-12 */
} uart_channel_limits_t;

typedef enum {
  k_uart_baudrate_min = 1,
  k_uart_baudrate_max = (k_pclkb_hz / k_brr_divisor_n0),
} uart_validation_limits_t;

/** @brief UART timeout constants */
typedef enum {
  k_uart_tx_timeout        = 100000, /**< Transmit buffer wait timeout (prevents infinite loop) */
  k_uart_timeout_expired   = 0,      /**< Timeout counter expired value */
  k_uart_timeout_decrement = 1,      /**< Timeout counter decrement value */
} uart_timeout_t;

/** @brief SCI module stop bit positions in MSTPCRB */
typedef enum {
  k_sci_mstpb_sci0  = 31, /**< SCI0 module stop bit */
  k_sci_mstpb_sci1  = 30, /**< SCI1 module stop bit */
  k_sci_mstpb_sci2  = 29, /**< SCI2 module stop bit */
  k_sci_mstpb_sci3  = 28, /**< SCI3 module stop bit */
  k_sci_mstpb_sci4  = 27, /**< SCI4 module stop bit */
  k_sci_mstpb_sci5  = 26, /**< SCI5 module stop bit */
  k_sci_mstpb_sci6  = 25, /**< SCI6 module stop bit */
  k_sci_mstpb_sci7  = 24, /**< SCI7 module stop bit */
  k_sci_mstpb_sci8  = 23, /**< SCI8 module stop bit */
  k_sci_mstpb_sci9  = 22, /**< SCI9 module stop bit */
  k_sci_mstpb_sci10 = 21, /**< SCI10 module stop bit */
  k_sci_mstpb_sci11 = 20, /**< SCI11 module stop bit */
} sci_mstpb_bits_t;

/** @brief Debug UART pins (SCI9 on RX72N) */
typedef enum {
  k_uart_debug_tx_gpio = k_rx_pb_7, /**< PB7 = TXD9 (from rx_port_constants.h) */
  k_uart_debug_rx_gpio = k_rx_pb_6, /**< PB6 = RXD9 (from rx_port_constants.h) */
} uart_debug_pins_t;

/** @brief GPIO register bit manipulation constant */
static const uint8_t k_uart_gpio_bit_set = 1;

/* =============================================================================
 * Private State
 * =============================================================================
 */

/** @brief Per-channel initialization state */
static bool s_channel_initialized[k_uart_max_channels] = {false};

/* =============================================================================
 * Private Functions
 * =============================================================================
 */

/**
 * @brief Calculate BRR value for given baud rate
 *
 * BRR = (PCLKB / (64 * 2^(2n-1) * B)) - 1
 * For n=0 (CKS=00, PCLK/1): BRR = (PCLKB / (32 * B)) - 1
 *
 * @param[in] baudrate Target baud rate
 * @return BRR register value
 */
static uint8_t internal_calculate_brr(uint32_t baudrate)
{
  if (baudrate == 0) {
    return k_brr_max_value;
  }

  /* For n=0 (CKS=00): BRR = (PCLKB / (32 * B)) - 1 */
  uint32_t brr_value = (k_pclkb_hz / (k_brr_divisor_n0 * baudrate)) - 1;

  if (brr_value > k_brr_max_value) {
    return k_brr_max_value;
  }

  return (uint8_t)brr_value;
}

/**
 * @brief Clear error flags in SSR register
 *
 * @param[in] sci Pointer to SCI registers
 */
static void internal_clear_errors(volatile rx_sci_regs_t* sci)
{
  volatile uint8_t ssr = 0;

  /* Read SSR then clear error flags */
  ssr = sci->ssr;
  (void)ssr; /* Suppress unused variable warning */
  sci->ssr = (uint8_t)(ssr & ~k_sci_ssr_error_mask);
}

/**
 * @brief Get MSTPCRB bit position for SCI channel
 *
 * @param[in] channel SCI channel (0-11)
 *
 * @return Bit position in MSTPCRB, or -1 if invalid channel
 */
static int8_t internal_get_mstpb_bit(uint8_t channel)
{
  /* SCI12 is in MSTPCRC, not supported here */
  if (channel > 11) {
    return -1;
  }

  /* MSTPCRB bits: SCI0=31, SCI1=30, ..., SCI11=20 */
  return (int8_t)(k_sci_mstpb_sci0 - channel);
}

/**
 * @brief Enable SCI module clock (clear module stop)
 *
 * @param[in] channel SCI channel (0-11)
 *
 * @return k_rx_ok on success, k_rx_err_invalid_arg if channel invalid
 */
static rx_err_t internal_enable_sci_clock(uint8_t channel)
{
  int8_t mstpb_bit = internal_get_mstpb_bit(channel);
  if (mstpb_bit < 0) {
    return k_rx_err_invalid_arg;
  }

  /* Unlock protection */
  system_regs()->prcr = k_rx_prcr_unlock_all;

  /* Clear module stop bit to enable clock */
  system_regs()->mstpcrb &= ~(1UL << (uint8_t)mstpb_bit);

  /* Lock protection */
  system_regs()->prcr = k_rx_prcr_lock;

  return k_rx_ok;
}

/**
 * @brief Configure pins for SCI UART operation
 *
 * Sets up MPC (pin mux) and GPIO registers for TX/RX pins.
 *
 * @param[in] tx_gpio TX pin (rx_port_pin_t from rx_port_constants.h)
 * @param[in] rx_gpio RX pin (rx_port_pin_t from rx_port_constants.h)
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_configure_uart_pins(rx_port_pin_t tx_gpio, rx_port_pin_t rx_gpio)
{
  /* Extract port and pin numbers for hardware register access */
  uint8_t tx_port = rx_port_from_pin(tx_gpio);
  uint8_t tx_pin  = rx_pin_from_pin(tx_gpio);
  uint8_t rx_port = rx_port_from_pin(rx_gpio);
  uint8_t rx_pin  = rx_pin_from_pin(rx_gpio);

  /* Validate pin numbers */
  if (tx_pin > k_rx_pin_max || rx_pin > k_rx_pin_max) {
    return k_rx_err_invalid_arg;
  }

  /* Get port bases */
  volatile rx_port_regs_t* tx_port_base = rx_port_get_base(tx_port);
  volatile rx_port_regs_t* rx_port_base = rx_port_get_base(rx_port);

  if (tx_port_base == (volatile rx_port_regs_t*)0 || rx_port_base == (volatile rx_port_regs_t*)0) {
    return k_rx_err_invalid_arg;
  }

  /* Configure MPC for SCI function */
  rx_err_t err = rx_mpc_set_sci(tx_gpio, true);
  if (err != k_rx_ok) {
    return err;
  }

  err = rx_mpc_set_sci(rx_gpio, false);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure TX pin: output direction, peripheral mode */
  tx_port_base->pdr |= (k_uart_gpio_bit_set << tx_pin); /* Output */
  tx_port_base->pmr |= (k_uart_gpio_bit_set << tx_pin); /* Peripheral mode */

  /* Configure RX pin: input direction, peripheral mode */
  rx_port_base->pdr &= ~(k_uart_gpio_bit_set << rx_pin); /* Input */
  rx_port_base->pmr |= (k_uart_gpio_bit_set << rx_pin);  /* Peripheral mode */

  return k_rx_ok;
}

/* =============================================================================
 * Multi-Channel UART Functions
 * =============================================================================
 */

rx_err_t uart_init_channel(const uart_channel_config_t* config)
{
  /* Validate config pointer */
  if (config == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Validate channel */
  if ((config->channel < k_uart_min_channel) || (config->channel >= k_uart_max_channels)) {
    return k_rx_err_invalid_arg;
  }

  if ((config->baudrate < k_uart_baudrate_min) || (config->baudrate > k_uart_baudrate_max)) {
    return k_rx_err_invalid_arg;
  }

  /* Get SCI register base */
  volatile rx_sci_regs_t* sci = sci_get_channel(config->channel);
  if (sci == (volatile rx_sci_regs_t*)0) {
    return k_rx_err_invalid_arg;
  }

  /* Check if already initialized */
  if (s_channel_initialized[config->channel]) {
    return k_rx_err_invalid_state;
  }

  /* Enable SCI module clock (clear module stop bit) */
  rx_err_t err = internal_enable_sci_clock(config->channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure TX/RX pins (MPC and GPIO) */
  err = internal_configure_uart_pins(config->tx_gpio, config->rx_gpio);
  if (err != k_rx_ok) {
    return err;
  }

  /* Disable TX/RX */
  sci->scr = k_sci_scr_disabled;

  /* Configure serial mode: Async, 8-bit, no parity, 1 stop, PCLK/1 */
  sci->smr = k_sci_smr_async_8n1;

  /* Set baud rate */
  sci->brr = internal_calculate_brr(config->baudrate);

  /* Wait for at least 1 bit time */
  /* NOTE: Busy-wait required - may run before ThreadX initialization */
  for (volatile uint32_t i = 0; i < k_uart_bit_time_delay_cycles; i++) {
    __asm__ volatile("nop");
  }

  /* Configure serial control: Enable TX and RX */
  sci->scr = k_sci_scr_txrx_enabled;

  /* Configure serial extended mode */
  sci->semr = k_sci_semr_default;

  /* Mark channel as initialized */
  s_channel_initialized[config->channel] = true;

  return k_rx_ok;
}

rx_err_t uart_deinit_channel(uint8_t channel)
{
  /* Validate channel */
  if (channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Get SCI register base */
  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == (volatile rx_sci_regs_t*)0) {
    return k_rx_err_invalid_arg;
  }

  /* Disable TX/RX */
  sci->scr = k_sci_scr_disabled;

  /* Mark channel as not initialized */
  s_channel_initialized[channel] = false;

  return k_rx_ok;
}

rx_err_t uart_putc_channel(uint8_t channel, char data)
{
  /* Validate channel */
  if (channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Check initialization */
  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Get SCI register base */
  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == (volatile rx_sci_regs_t*)0) {
    return k_rx_err_invalid_arg;
  }

  /* Wait for transmit buffer to be empty (TDRE flag) with timeout */
  uint32_t timeout = k_uart_tx_timeout;
  while ((sci->ssr & k_sci_ssr_tdre_flag) == k_uart_timeout_expired &&
         timeout > k_uart_timeout_expired) {
    timeout -= k_uart_timeout_decrement;
  }

  /* Check if timeout occurred */
  if (timeout == k_uart_timeout_expired) {
    return k_rx_err_timeout;
  }

  /* Write data to transmit register */
  sci->tdr = (uint8_t)data;

  /* Clear TDRE flag by reading SSR then writing 0 */
  volatile uint8_t ssr = sci->ssr;
  sci->ssr             = (uint8_t)(ssr & ~k_sci_ssr_tdre_flag);

  return k_rx_ok;
}

rx_err_t uart_puts_channel(uint8_t channel, const char* str)
{
  /* Validate parameters */
  if (str == (const char*)0) {
    return k_rx_err_null_ptr;
  }

  if (channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Transmit string with \n to \r\n conversion */
  while (*str) {
    if (*str == '\n') {
      rx_err_t err = uart_putc_channel(channel, '\r');
      if (err != k_rx_ok) {
        return err;
      }
    }
    rx_err_t err = uart_putc_channel(channel, *str++);
    if (err != k_rx_ok) {
      return err;
    }
  }

  return k_rx_ok;
}

rx_err_t uart_write_channel(uint8_t channel, const uint8_t* data, uint16_t length)
{
  /* Validate parameters */
  if (data == (const uint8_t*)0) {
    return k_rx_err_null_ptr;
  }

  if (channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Write each byte */
  for (uint16_t i = 0; i < length; i++) {
    rx_err_t err = uart_putc_channel(channel, (char)data[i]);
    if (err != k_rx_ok) {
      return err;
    }
  }

  return k_rx_ok;
}

rx_err_t uart_getc_channel(uint8_t channel, char* data)
{
  /* Validate parameters */
  if (data == (char*)0) {
    return k_rx_err_null_ptr;
  }

  if (channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Get SCI register base */
  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == (volatile rx_sci_regs_t*)0) {
    return k_rx_err_invalid_arg;
  }

  /* Clear any error flags first */
  if ((sci->ssr & k_sci_ssr_error_mask) != 0) {
    internal_clear_errors(sci);
  }

  /* Check if receive data is available (RDRF flag) */
  if ((sci->ssr & k_sci_ssr_rdrf_flag) == 0) {
    return k_rx_err_empty;
  }

  /* Read received data */
  *data = (char)sci->rdr;

  /* Clear RDRF flag by reading SSR then writing 0 */
  /* Some RX MCUs require explicit clear after reading RDR */
  volatile uint8_t ssr = sci->ssr;
  sci->ssr             = (uint8_t)(ssr & ~k_sci_ssr_rdrf_flag);

  return k_rx_ok;
}

rx_err_t uart_read_channel(uint8_t channel, uint8_t* data, uint16_t length, uint16_t* bytes_read)
{
  /* Validate parameters */
  if (data == (uint8_t*)0 || bytes_read == (uint16_t*)0) {
    return k_rx_err_null_ptr;
  }

  if (channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Read available bytes */
  *bytes_read = 0;
  for (uint16_t i = 0; i < length; i++) {
    char     c;
    rx_err_t err = uart_getc_channel(channel, &c);
    if (err == k_rx_err_empty) {
      /* No more data available */
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

rx_err_t uart_rx_available(uint8_t channel, bool* available)
{
  /* Validate parameters */
  if (available == (bool*)0) {
    return k_rx_err_null_ptr;
  }

  if (channel >= k_uart_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (!s_channel_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Get SCI register base */
  volatile rx_sci_regs_t* sci = sci_get_channel(channel);
  if (sci == (volatile rx_sci_regs_t*)0) {
    return k_rx_err_invalid_arg;
  }

  /* Check RDRF flag */
  *available = ((sci->ssr & k_sci_ssr_rdrf_flag) != 0);

  return k_rx_ok;
}

/* =============================================================================
 * Legacy Debug UART Functions (SCI9)
 * =============================================================================
 */

rx_err_t uart_init(void)
{
  const uart_channel_config_t config = {
    .channel  = k_uart_debug_channel,
    .baudrate = k_uart_default_baudrate,
    .tx_gpio  = k_uart_debug_tx_gpio,
    .rx_gpio  = k_uart_debug_rx_gpio,
  };
  return uart_init_channel(&config);
}

void uart_putc(char data)
{
  /* For legacy function, ignore errors (used in early init before error handling) */
  (void)uart_putc_channel(k_uart_debug_channel, data);
}

void uart_puts(const char* str)
{
  if (str == (const char*)0) {
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

void uart_putint(int32_t value)
{
  char     buffer[k_uart_int_buffer_size]; /* Enough for -2147483648 */
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
    *--p = '0' + (abs_value % k_uart_base_10);
    abs_value /= k_uart_base_10;
  } while (abs_value > 0);

  /* Add minus sign if negative */
  if (is_negative) {
    *--p = '-';
  }

  /* Transmit the string */
  uart_puts(p);
}

void uart_puthex(uint32_t value, uint8_t digits)
{
  static const char s_hex[] = "0123456789ABCDEF";

  uart_puts("0x");

  /* Clamp digits to valid range */
  if (digits > k_uart_hex_max_digits) {
    digits = k_uart_hex_max_digits;
  }
  if (digits == k_uart_hex_zero_digits) {
    digits = k_uart_hex_min_digits;
  }

  /* Print hex digits from most significant */
  for (int32_t i = digits - 1; i >= 0; i--) {
    uint8_t nibble = (value >> (i * k_uart_hex_nibble_bits)) & k_uart_hex_nibble_mask;
    uart_putc(s_hex[nibble]);
  }
}
