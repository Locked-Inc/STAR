/* lib/rx_bus/inc/rx_bus_config.h */

/**
 * @file rx_bus_config.h
 * @brief Bus configuration creation helpers for RX72N
 * @details
 * Provides helper functions to initialize bus configuration structures.
 * Uses static allocation pattern - user provides the config structure.
 *
 * Unlike ESP32 version which uses dynamic allocation, RX72N firmware
 * follows zero-allocation principle for safety-critical applications.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_BUS_CONFIG_H
#define STAR_RX72N_BUS_CONFIG_H

#include "rx_bus_types.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @example Basic usage with static allocation:
 * @code
 * // Declare static config
 * static rx_bus_config_t gpio_led_config;
 *
 * // Initialize config
 * rx_bus_config_init_gpio(&gpio_led_config, "led_gpio", 3, 0);
 *
 * // Add to bus manager
 * rx_bus_manager_add_bus(&bus_manager, &gpio_led_config);
 *
 * // Use through bus abstraction
 * rx_bus_gpio_init(&bus_manager, "led_gpio", true);  // Output
 * rx_bus_gpio_write(&bus_manager, "led_gpio", true); // LED on
 * @endcode
 */

/* =============================================================================
 * GPIO Bus Configuration
 * =============================================================================
 */

/**
 * @brief Initialize GPIO bus configuration
 *
 * Configures a single GPIO pin for bus manager control.
 *
 * @param[out] config Pointer to bus config structure to initialize
 * @param[in] name Unique bus name (must remain valid for lifetime)
 * @param[in] port GPIO port number (0-9, or 0xA-0x10 for A-G)
 * @param[in] pin GPIO pin number (0-7)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if config or name is NULL
 * @return k_rx_err_invalid_arg if port or pin is invalid
 *
 * @note The config structure must remain valid for the lifetime of bus usage
 * @note The name string must remain valid (use string literals or static storage)
 */
rx_err_t
rx_bus_config_init_gpio(rx_bus_config_t* config, const char* name, uint8_t port, uint8_t pin);

/* =============================================================================
 * ADC Bus Configuration
 * =============================================================================
 */

/**
 * @brief Initialize ADC bus configuration
 *
 * Configures an ADC channel for bus manager control.
 *
 * @param[out] config Pointer to bus config structure to initialize
 * @param[in] name Unique bus name (must remain valid for lifetime)
 * @param[in] unit ADC unit (0 or 1)
 * @param[in] channel ADC channel (0-7)
 * @param[in] bits Resolution (8, 10, or 12 bits)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if config or name is NULL
 * @return k_rx_err_invalid_arg if unit, channel, or bits is invalid
 *
 * @note The config structure must remain valid for the lifetime of bus usage
 * @note The name string must remain valid (use string literals or static storage)
 */
rx_err_t rx_bus_config_init_adc(rx_bus_config_t* config,
                                const char*      name,
                                uint8_t          unit,
                                uint8_t          channel,
                                uint8_t          bits);

/* =============================================================================
 * I2C Bus Configuration (Future)
 * =============================================================================
 */

/**
 * @brief Initialize I2C bus configuration
 *
 * Configures an I2C device for bus manager control.
 *
 * @param[out] config Pointer to bus config structure to initialize
 * @param[in] name Unique bus name (must remain valid for lifetime)
 * @param[in] channel RIIC channel (0-2)
 * @param[in] device_addr 7-bit I2C device address
 * @param[in] sda_port SDA pin port
 * @param[in] sda_pin SDA pin number
 * @param[in] scl_port SCL pin port
 * @param[in] scl_pin SCL pin number
 * @param[in] frequency_hz Clock frequency (100000, 400000, or 1000000)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if config or name is NULL
 * @return k_rx_err_invalid_arg if parameters are invalid
 *
 * @note Not yet implemented - Phase 1.4
 */
rx_err_t rx_bus_config_init_i2c(rx_bus_config_t* config,
                                const char*      name,
                                uint8_t          channel,
                                uint8_t          device_addr,
                                uint8_t          sda_port,
                                uint8_t          sda_pin,
                                uint8_t          scl_port,
                                uint8_t          scl_pin,
                                uint32_t         frequency_hz);

/* =============================================================================
 * SMBUS Bus Configuration (Future)
 * =============================================================================
 */

/**
 * @brief Initialize SMBUS bus configuration
 *
 * Configures an SMBUS device (I2C variant with CRC-8) for bus manager control.
 *
 * @param[out] config Pointer to bus config structure to initialize
 * @param[in] name Unique bus name (must remain valid for lifetime)
 * @param[in] channel RIIC channel (0-2)
 * @param[in] device_addr 7-bit SMBUS device address
 * @param[in] sda_port SDA pin port
 * @param[in] sda_pin SDA pin number
 * @param[in] scl_port SCL pin port
 * @param[in] scl_pin SCL pin number
 * @param[in] frequency_hz Clock frequency (typically 100000)
 * @param[in] use_pec Enable Packet Error Checking (CRC-8)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if config or name is NULL
 * @return k_rx_err_invalid_arg if parameters are invalid
 *
 * @note Not yet implemented - Phase 1.4
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
                                  bool             use_pec);

/* =============================================================================
 * OneWire Bus Configuration
 * =============================================================================
 */

/**
 * @brief Initialize OneWire (1-Wire) bus configuration
 *
 * Configures a single GPIO pin for OneWire protocol communication.
 * The pin should have an external 4.7k pullup resistor to VCC.
 *
 * OneWire uses bidirectional communication on a single wire:
 * - Controller pulls line low to drive
 * - Controller releases line (high-Z input) to allow peripheral to pull low
 * - External pullup resistor pulls line high when not driven
 *
 * @param[out] config Pointer to bus config structure to initialize
 * @param[in] name Unique bus name (must remain valid for lifetime)
 * @param[in] port GPIO port number (0-9, or 0xA-0x10 for A-G)
 * @param[in] pin GPIO pin number (0-7)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if config or name is NULL
 * @return k_rx_err_invalid_arg if port or pin is invalid
 *
 * @note The config structure must remain valid for the lifetime of bus usage
 * @note The name string must remain valid (use string literals or static storage)
 * @note Requires external 4.7k pullup resistor on the data line
 *
 * @example
 * @code
 * // Declare static config
 * static rx_bus_config_t temp_sensor_config;
 *
 * // Initialize OneWire on Port 3, Pin 2
 * rx_bus_config_init_onewire(&temp_sensor_config, "temp_sensor", 3, 2);
 *
 * // Add to bus manager
 * rx_bus_manager_add_bus(&bus_manager, &temp_sensor_config);
 *
 * // Use OneWire operations
 * bool presence;
 * rx_bus_onewire_init(&bus_manager, "temp_sensor");
 * rx_bus_onewire_reset(&bus_manager, "temp_sensor", &presence);
 * @endcode
 */
rx_err_t
rx_bus_config_init_onewire(rx_bus_config_t* config, const char* name, uint8_t port, uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_CONFIG_H */
