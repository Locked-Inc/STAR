/* lib/rx_bus/src/rx_bus_config.c */

/**
 * @file rx_bus_config.c
 * @brief Bus configuration creation helpers implementation
 * @details
 * Provides helper functions to initialize bus configuration structures
 * with static allocation pattern.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
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

rx_err_t
rx_bus_config_init_gpio(rx_bus_config_t* config, const char* name, uint8_t port, uint8_t pin)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");

  /* Validate port (0-9 or 0xA-0x10 for A-G) */
  if (port > k_gpio_port_max_alpha ||
      (port > k_gpio_port_max_numeric && port < k_gpio_port_min_alpha)) {
    RX_LOG_ERROR(s_tag, "Invalid GPIO port");
    return k_rx_err_invalid_arg;
  }

  /* Validate pin (0-7) */
  if (pin >= k_gpio_pin_count) {
    RX_LOG_ERROR(s_tag, "Invalid GPIO pin");
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
  config->proto.gpio.port = port;
  config->proto.gpio.pin  = pin;

  RX_LOG_DEBUG(s_tag, "GPIO bus config initialized");

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
    RX_LOG_ERROR(s_tag, "Invalid ADC unit");
    return k_rx_err_invalid_arg;
  }

  /* Validate channel (0-7) */
  if (channel > k_adc_channel_max) {
    RX_LOG_ERROR(s_tag, "Invalid ADC channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate resolution */
  if (bits != k_adc_resolution_8bit && bits != k_adc_resolution_10bit &&
      bits != k_adc_resolution_12bit) {
    RX_LOG_ERROR(s_tag, "Invalid ADC resolution (must be 8, 10, or 12)");
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

  RX_LOG_DEBUG(s_tag, "ADC bus config initialized");

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
                                uint8_t          sda_port,
                                uint8_t          sda_pin,
                                uint8_t          scl_port,
                                uint8_t          scl_pin,
                                uint32_t         frequency_hz)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");

  /* Validate channel (0-2) */
  if (channel >= k_riic_channel_count) {
    RX_LOG_ERROR(s_tag, "Invalid I2C channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate device address (7-bit) */
  if (device_addr > k_i2c_addr_max_7bit) {
    RX_LOG_ERROR(s_tag, "Invalid I2C device address");
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
  config->proto.i2c.sda_port     = sda_port;
  config->proto.i2c.sda_pin      = sda_pin;
  config->proto.i2c.scl_port     = scl_port;
  config->proto.i2c.scl_pin      = scl_pin;
  config->proto.i2c.frequency_hz = frequency_hz;
  config->proto.i2c.device_addr  = device_addr;

  RX_LOG_DEBUG(s_tag, "I2C bus config initialized");

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
                                  uint8_t          sda_port,
                                  uint8_t          sda_pin,
                                  uint8_t          scl_port,
                                  uint8_t          scl_pin,
                                  uint32_t         frequency_hz,
                                  bool             use_pec)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");
  RX_CHECK_NULL_PTR(name, s_tag, "name pointer is NULL");

  /* Validate channel (0-2) */
  if (channel >= k_riic_channel_count) {
    RX_LOG_ERROR(s_tag, "Invalid SMBUS channel");
    return k_rx_err_invalid_arg;
  }

  /* Validate device address (7-bit) */
  if (device_addr > k_i2c_addr_max_7bit) {
    RX_LOG_ERROR(s_tag, "Invalid SMBUS device address");
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
  config->proto.smbus.i2c_config.sda_port     = sda_port;
  config->proto.smbus.i2c_config.sda_pin      = sda_pin;
  config->proto.smbus.i2c_config.scl_port     = scl_port;
  config->proto.smbus.i2c_config.scl_pin      = scl_pin;
  config->proto.smbus.i2c_config.frequency_hz = frequency_hz;
  config->proto.smbus.i2c_config.device_addr  = device_addr;
  config->proto.smbus.use_pec                 = use_pec;

  RX_LOG_DEBUG(s_tag, "SMBUS bus config initialized");

  return k_rx_ok;
}
