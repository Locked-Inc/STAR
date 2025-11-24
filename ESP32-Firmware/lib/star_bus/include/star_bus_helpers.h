/* esp32-firmware/components/star_bus/include/star_bus_helpers.h */

#ifndef STAR_BUS_HELPERS_H
#define STAR_BUS_HELPERS_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_config.h"
#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_helpers.h
 * @brief Configuration helpers and validation utilities
 *
 * This module provides simplified configuration functions and validation
 * helpers for common use cases.
 */

/* --- I2C Presets --- */

/**
 * @brief Create I2C config for standard mode (100 kHz)
 */
star_bus_config_t* star_bus_config_i2c_standard(const char* name,
                                                uint8_t     address,
                                                gpio_num_t  sda_pin,
                                                gpio_num_t  scl_pin);

/**
 * @brief Create I2C config for fast mode (400 kHz)
 */
star_bus_config_t*
star_bus_config_i2c_fast(const char* name, uint8_t address, gpio_num_t sda_pin, gpio_num_t scl_pin);

/**
 * @brief Create I2C config for fast mode plus (1 MHz)
 */
star_bus_config_t* star_bus_config_i2c_fast_plus(const char* name,
                                                 uint8_t     address,
                                                 gpio_num_t  sda_pin,
                                                 gpio_num_t  scl_pin);

/* --- Validation Helpers --- */

/**
 * @brief Validate I2C address (7-bit)
 */
bool star_bus_validate_i2c_address(uint8_t address);

/**
 * @brief Validate I2C frequency
 */
bool star_bus_validate_i2c_frequency(uint32_t frequency);

/**
 * @brief Validate SPI frequency
 */
bool star_bus_validate_spi_frequency(uint32_t frequency);

/**
 * @brief Validate UART baud rate
 */
bool star_bus_validate_uart_baudrate(uint32_t baudrate);

/**
 * @brief Check if GPIO pin is valid for I2C
 */
bool star_bus_validate_gpio_i2c(gpio_num_t pin);

/**
 * @brief Check if GPIO pin is valid for SPI
 */
bool star_bus_validate_gpio_spi(gpio_num_t pin);

/* --- Quick Setup Helpers --- */

/**
 * @brief Quick setup: I2C master with automatic defaults
 *
 * @param manager Bus manager
 * @param bus_name Bus name (e.g., "bmp280")
 * @param address I2C device address
 * @param sda_pin SDA GPIO pin
 * @param scl_pin SCL GPIO pin
 *
 * @return ESP_OK on success
 */
esp_err_t star_bus_quick_setup_i2c(star_bus_manager_t* manager,
                                   const char*         bus_name,
                                   uint8_t             address,
                                   gpio_num_t          sda_pin,
                                   gpio_num_t          scl_pin);

/* --- Pin Mapping Helpers --- */

/**
 * @brief Default I2C pins for ESP32 boards
 */
#define STAR_BUS_I2C_DEFAULT_SDA (21)
#define STAR_BUS_I2C_DEFAULT_SCL (22)

/**
 * @brief Default SPI pins for ESP32 boards (VSPI)
 */
#define STAR_BUS_SPI_DEFAULT_MOSI (23)
#define STAR_BUS_SPI_DEFAULT_MISO (19)
#define STAR_BUS_SPI_DEFAULT_SCLK (18)

/* --- Common Frequencies --- */

#define STAR_BUS_I2C_FREQ_STANDARD (100000)   /**< I2C Standard mode (100 kHz) */
#define STAR_BUS_I2C_FREQ_FAST (400000)       /**< I2C Fast mode (400 kHz) */
#define STAR_BUS_I2C_FREQ_FAST_PLUS (1000000) /**< I2C Fast mode plus (1 MHz) */

#define STAR_BUS_SPI_FREQ_1MHZ (1000000)   /**< SPI 1 MHz */
#define STAR_BUS_SPI_FREQ_10MHZ (10000000) /**< SPI 10 MHz */
#define STAR_BUS_SPI_FREQ_20MHZ (20000000) /**< SPI 20 MHz (max for ESP32) */

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_HELPERS_H */
