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
 *
 * Example Usage:
 * @code
 * // === Quick I2C Setup (Simplest Approach) ===
 *
 * #include "star_bus_helpers.h"
 * #include "star_bus_manager.h"
 *
 * // One-liner to setup I2C bus with automatic defaults
 * esp_err_t ret = star_bus_quick_setup_i2c(
 *     &bus_manager,
 *     "bmp280",           // Bus name
 *     0x76,               // Device address
 *     GPIO_NUM_21,        // SDA pin
 *     GPIO_NUM_22         // SCL pin
 * );
 * // Uses fast mode (400kHz) by default
 *
 *
 * // === I2C Speed Presets ===
 *
 * // Standard mode (100 kHz) - for older or long-wire devices
 * star_bus_config_t* std_bus = star_bus_config_i2c_standard(
 *     "eeprom", 0x50, GPIO_NUM_21, GPIO_NUM_22);
 * star_bus_manager_add_bus(&bus_manager, std_bus);
 *
 * // Fast mode (400 kHz) - most common for sensors
 * star_bus_config_t* fast_bus = star_bus_config_i2c_fast(
 *     "imu", 0x68, GPIO_NUM_21, GPIO_NUM_22);
 * star_bus_manager_add_bus(&bus_manager, fast_bus);
 *
 * // Fast mode plus (1 MHz) - high-speed sensors
 * star_bus_config_t* fast_plus_bus = star_bus_config_i2c_fast_plus(
 *     "oled", 0x3C, GPIO_NUM_21, GPIO_NUM_22);
 * star_bus_manager_add_bus(&bus_manager, fast_plus_bus);
 *
 *
 * // === Validation Before Configuration ===
 *
 * // Validate I2C address (7-bit, 0x08-0x77 valid range)
 * uint8_t addr = 0x68;
 * if (!star_bus_validate_i2c_address(addr)) {
 *     ESP_LOGE(TAG, "Invalid I2C address: 0x%02X", addr);
 *     return ESP_ERR_INVALID_ARG;
 * }
 *
 * // Validate I2C frequency
 * uint32_t freq = 400000;
 * if (!star_bus_validate_i2c_frequency(freq)) {
 *     ESP_LOGE(TAG, "Invalid I2C frequency: %lu Hz", freq);
 *     return ESP_ERR_INVALID_ARG;
 * }
 *
 * // Validate SPI frequency
 * uint32_t spi_freq = 10000000;  // 10 MHz
 * if (!star_bus_validate_spi_frequency(spi_freq)) {
 *     ESP_LOGE(TAG, "Invalid SPI frequency: %lu Hz", spi_freq);
 *     return ESP_ERR_INVALID_ARG;
 * }
 *
 * // Validate UART baud rate
 * uint32_t baud = 115200;
 * if (!star_bus_validate_uart_baudrate(baud)) {
 *     ESP_LOGE(TAG, "Invalid UART baud rate: %lu", baud);
 *     return ESP_ERR_INVALID_ARG;
 * }
 *
 *
 * // === GPIO Pin Validation ===
 *
 * // Check if GPIO is valid for I2C (must support open-drain)
 * gpio_num_t sda = GPIO_NUM_21;
 * gpio_num_t scl = GPIO_NUM_22;
 * if (!star_bus_validate_gpio_i2c(sda) || !star_bus_validate_gpio_i2c(scl)) {
 *     ESP_LOGE(TAG, "Invalid GPIO pins for I2C");
 *     return ESP_ERR_INVALID_ARG;
 * }
 *
 * // Check if GPIO is valid for SPI
 * gpio_num_t mosi = GPIO_NUM_23;
 * if (!star_bus_validate_gpio_spi(mosi)) {
 *     ESP_LOGE(TAG, "Invalid GPIO pin for SPI");
 *     return ESP_ERR_INVALID_ARG;
 * }
 *
 *
 * // === Using Default Pin Definitions ===
 *
 * // Use ESP32 default I2C pins
 * star_bus_config_t* bus = star_bus_config_i2c_fast(
 *     "default_i2c",
 *     0x68,
 *     STAR_BUS_I2C_DEFAULT_SDA,  // GPIO 21
 *     STAR_BUS_I2C_DEFAULT_SCL   // GPIO 22
 * );
 *
 * // Use default SPI pins (VSPI)
 * // MOSI: STAR_BUS_SPI_DEFAULT_MOSI (GPIO 23)
 * // MISO: STAR_BUS_SPI_DEFAULT_MISO (GPIO 19)
 * // SCLK: STAR_BUS_SPI_DEFAULT_SCLK (GPIO 18)
 *
 *
 * // === Using Frequency Constants ===
 *
 * // I2C frequencies
 * star_bus_config_t* cfg1 = star_bus_config_create_i2c(
 *     "sensor", I2C_NUM_0, 0x68,
 *     GPIO_NUM_21, GPIO_NUM_22,
 *     STAR_BUS_I2C_FREQ_FAST);      // 400 kHz
 *
 * star_bus_config_t* cfg2 = star_bus_config_create_i2c(
 *     "fast_sensor", I2C_NUM_0, 0x68,
 *     GPIO_NUM_21, GPIO_NUM_22,
 *     STAR_BUS_I2C_FREQ_FAST_PLUS); // 1 MHz
 *
 * // SPI frequencies
 * // STAR_BUS_SPI_FREQ_1MHZ   - 1 MHz
 * // STAR_BUS_SPI_FREQ_10MHZ  - 10 MHz
 * // STAR_BUS_SPI_FREQ_20MHZ  - 20 MHz (ESP32 max)
 *
 *
 * // === Complete Validation Example ===
 *
 * typedef struct {
 *     uint8_t    addr;
 *     gpio_num_t sda;
 *     gpio_num_t scl;
 *     uint32_t   freq;
 * } i2c_params_t;
 *
 * esp_err_t validate_i2c_params(const i2c_params_t* params) {
 *     if (!star_bus_validate_i2c_address(params->addr)) {
 *         return ESP_ERR_INVALID_ARG;
 *     }
 *     if (!star_bus_validate_gpio_i2c(params->sda)) {
 *         return ESP_ERR_INVALID_ARG;
 *     }
 *     if (!star_bus_validate_gpio_i2c(params->scl)) {
 *         return ESP_ERR_INVALID_ARG;
 *     }
 *     if (!star_bus_validate_i2c_frequency(params->freq)) {
 *         return ESP_ERR_INVALID_ARG;
 *     }
 *     return ESP_OK;
 * }
 *
 * i2c_params_t params = { .addr = 0x68, .sda = GPIO_NUM_21,
 *                         .scl = GPIO_NUM_22, .freq = 400000 };
 * if (validate_i2c_params(&params) == ESP_OK) {
 *     star_bus_quick_setup_i2c(&bus_manager, "imu",
 *                               params.addr, params.sda, params.scl);
 * }
 * @endcode
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
