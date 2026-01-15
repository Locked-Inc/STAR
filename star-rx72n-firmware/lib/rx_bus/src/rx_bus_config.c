/* lib/rx_bus/src/rx_bus_config.c */

/**
 * @file rx_bus_config.c
 * @brief Bus configuration creation helpers implementation
 * @details
 * Provides helper functions to initialize bus configuration structures
 * with static allocation pattern.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_bus_config.h"

#include <string.h>

#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BUS_CFG";

/* =============================================================================
 * GPIO Bus Configuration
 * =============================================================================
 */

rx_err_t rx_bus_config_init_gpio(rx_bus_config_t* config, const char* name, rx_port_pin_t pin)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");

  /* Extract port and pin from type-safe enum */
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate port (0-9 or 0xA-0x10 for A-G) */
  if (port > k_hex_port_end || (port > k_max_decimal_port && port < k_hex_port_start)) {
    rx_log_error(s_tag, "Invalid GPIO port");
    return k_rx_err_invalid_arg;
  }

  /* Validate pin (0-7) */
  if (pin_num >= k_pins_per_port) {
    rx_log_error(s_tag, "Invalid GPIO pin");
    return k_rx_err_invalid_arg;
  }

  /* Zero out config structure */
  memset(config, 0, sizeof(rx_bus_config_t));

  /* Set common fields */
  config->name        = name;
  config->type        = k_bus_type_gpio;
  config->initialized = false;
  config->handle      = NULL;
  config->user_ctx    = NULL;
  config->next        = NULL;

  /* Set GPIO-specific fields */
  config->proto.gpio.pin = pin;

  rx_log_debug(s_tag, "GPIO bus config initialized");

  return k_rx_ok;
}

/* =============================================================================
 * ADC Bus Configuration
 * =============================================================================
 */

rx_err_t rx_bus_config_init_adc(rx_bus_config_t* config,
                                const char*      name,
                                uint8_t          unit,
                                uint8_t          channel,
                                uint8_t          bits)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");

  /* Validate unit (0 or 1) */
  if (unit >= k_adc_unit_count) {
    rx_log_error(s_tag, "Invalid ADC unit");
    return k_rx_err_invalid_arg;
  }

  /* Validate channel (0-7) */
  if (channel > k_adc_channel_max) {
    rx_log_error(s_tag, "Invalid ADC channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate resolution */
  if (bits != k_adc_resolution_8bit && bits != k_adc_resolution_10bit &&
      bits != k_adc_resolution_12bit) {
    rx_log_error(s_tag, "Invalid ADC resolution (must be 8, 10, or 12)");
    return k_rx_err_invalid_arg;
  }

  /* Zero out config structure */
  memset(config, 0, sizeof(rx_bus_config_t));

  /* Set common fields */
  config->name        = name;
  config->type        = k_bus_type_adc;
  config->initialized = false;
  config->handle      = NULL;
  config->user_ctx    = NULL;
  config->next        = NULL;

  /* Set ADC-specific fields */
  config->proto.adc.unit    = unit;
  config->proto.adc.channel = channel;
  config->proto.adc.bits    = bits;

  rx_log_debug(s_tag, "ADC bus config initialized");

  return k_rx_ok;
}

/* =============================================================================
 * I2C Bus Configuration (Stub for Future Implementation)
 * =============================================================================
 */

rx_err_t rx_bus_config_init_i2c(rx_bus_config_t* config,
                                const char*      name,
                                uint8_t          channel,
                                uint8_t          device_addr,
                                rx_port_pin_t    sda_pin,
                                rx_port_pin_t    scl_pin,
                                uint32_t         frequency_hz)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");

  /* Validate channel (0-2) */
  if (channel >= k_riic_channel_count) {
    rx_log_error(s_tag, "Invalid I2C channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate device address (7-bit) */
  if (device_addr > k_i2c_addr_max_7bit) {
    rx_log_error(s_tag, "Invalid I2C device address");
    return k_rx_err_invalid_arg;
  }

  /* Zero out config structure */
  memset(config, 0, sizeof(rx_bus_config_t));

  /* Set common fields */
  config->name        = name;
  config->type        = k_bus_type_i2c;
  config->initialized = false;
  config->handle      = NULL;
  config->user_ctx    = NULL;
  config->next        = NULL;

  /* Set I2C-specific fields */
  config->proto.i2c.channel      = channel;
  config->proto.i2c.sda_pin      = sda_pin;
  config->proto.i2c.scl_pin      = scl_pin;
  config->proto.i2c.frequency_hz = frequency_hz;
  config->proto.i2c.device_addr  = device_addr;

  rx_log_debug(s_tag, "I2C bus config initialized");

  return k_rx_ok;
}

/* =============================================================================
 * SMBUS Bus Configuration (Stub for Future Implementation)
 * =============================================================================
 */

rx_err_t rx_bus_config_init_smbus(rx_bus_config_t* config,
                                  const char*      name,
                                  uint8_t          channel,
                                  uint8_t          device_addr,
                                  rx_port_pin_t    sda_pin,
                                  rx_port_pin_t    scl_pin,
                                  uint32_t         frequency_hz,
                                  bool             use_pec)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");

  /* Validate channel (0-2) */
  if (channel >= k_riic_channel_count) {
    rx_log_error(s_tag, "Invalid SMBUS channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate device address (7-bit) */
  if (device_addr > k_i2c_addr_max_7bit) {
    rx_log_error(s_tag, "Invalid SMBUS device address");
    return k_rx_err_invalid_arg;
  }

  /* Zero out config structure */
  memset(config, 0, sizeof(rx_bus_config_t));

  /* Set common fields */
  config->name        = name;
  config->type        = k_bus_type_smbus;
  config->initialized = false;
  config->handle      = NULL;
  config->user_ctx    = NULL;
  config->next        = NULL;

  /* Set SMBUS-specific fields (extends I2C) */
  config->proto.smbus.i2c_config.channel      = channel;
  config->proto.smbus.i2c_config.sda_pin      = sda_pin;
  config->proto.smbus.i2c_config.scl_pin      = scl_pin;
  config->proto.smbus.i2c_config.frequency_hz = frequency_hz;
  config->proto.smbus.i2c_config.device_addr  = device_addr;
  config->proto.smbus.use_pec                 = use_pec;

  rx_log_debug(s_tag, "SMBUS bus config initialized");

  return k_rx_ok;
}

/* =============================================================================
 * UART Bus Configuration
 * =============================================================================
 */

rx_err_t rx_bus_config_init_uart(rx_bus_config_t* config,
                                 const char*      name,
                                 uint8_t          channel,
                                 rx_port_pin_t    tx_pin,
                                 rx_port_pin_t    rx_pin,
                                 uint32_t         baudrate)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");

  /* Extract port and pin from type-safe enums for validation */
  uint8_t tx_port    = rx_port_from_pin(tx_pin);
  uint8_t tx_pin_num = rx_pin_from_pin(tx_pin);
  uint8_t rx_port    = rx_port_from_pin(rx_pin);
  uint8_t rx_pin_num = rx_pin_from_pin(rx_pin);

  /* Validate SCI channel (0-12) */
  if (channel >= k_sci_channel_count) {
    rx_log_error(s_tag, "Invalid UART channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate TX port (0-9 or 0xA-0x10 for A-G) */
  if (tx_port > k_hex_port_end || (tx_port > k_max_decimal_port && tx_port < k_hex_port_start)) {
    rx_log_error(s_tag, "Invalid UART TX port");
    return k_rx_err_invalid_arg;
  }

  /* Validate TX pin (0-7) */
  if (tx_pin_num >= k_pins_per_port) {
    rx_log_error(s_tag, "Invalid UART TX pin");
    return k_rx_err_invalid_arg;
  }

  /* Validate RX port (0-9 or 0xA-0x10 for A-G) */
  if (rx_port > k_hex_port_end || (rx_port > k_max_decimal_port && rx_port < k_hex_port_start)) {
    rx_log_error(s_tag, "Invalid UART RX port");
    return k_rx_err_invalid_arg;
  }

  /* Validate RX pin (0-7) */
  if (rx_pin_num >= k_pins_per_port) {
    rx_log_error(s_tag, "Invalid UART RX pin");
    return k_rx_err_invalid_arg;
  }

  /* Validate baud rate */
  if (baudrate == 0) {
    rx_log_error(s_tag, "Invalid UART baud rate (cannot be 0)");
    return k_rx_err_invalid_arg;
  }

  /* Zero out config structure */
  memset(config, 0, sizeof(rx_bus_config_t));

  /* Set common fields */
  config->name        = name;
  config->type        = k_bus_type_uart;
  config->initialized = false;
  config->handle      = NULL;
  config->user_ctx    = NULL;
  config->next        = NULL;

  /* Set UART-specific fields */
  config->proto.uart.channel  = channel;
  config->proto.uart.tx_pin   = tx_pin;
  config->proto.uart.rx_pin   = rx_pin;
  config->proto.uart.baudrate = baudrate;

  rx_log_debug(s_tag, "UART bus config initialized");

  return k_rx_ok;
}

/* =============================================================================
 * OneWire Bus Configuration
 * =============================================================================
 */

rx_err_t rx_bus_config_init_onewire(rx_bus_config_t* config, const char* name, rx_port_pin_t pin)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");

  /* Extract port and pin from type-safe enum */
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate port (0-9 or 0xA-0x10 for A-G) */
  if (port > k_hex_port_end || (port > k_max_decimal_port && port < k_hex_port_start)) {
    rx_log_error(s_tag, "Invalid OneWire GPIO port");
    return k_rx_err_invalid_arg;
  }

  /* Validate pin (0-7) */
  if (pin_num >= k_pins_per_port) {
    rx_log_error(s_tag, "Invalid OneWire GPIO pin");
    return k_rx_err_invalid_arg;
  }

  /* Zero out config structure */
  memset(config, 0, sizeof(rx_bus_config_t));

  /* Set common fields */
  config->name        = name;
  config->type        = k_bus_type_onewire;
  config->initialized = false;
  config->handle      = NULL;
  config->user_ctx    = NULL;
  config->next        = NULL;

  /* Set OneWire-specific fields */
  config->proto.onewire.pin = pin;

  rx_log_debug(s_tag, "OneWire bus config initialized");

  return k_rx_ok;
}
