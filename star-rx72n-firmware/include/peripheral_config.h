/* include/peripheral_config.h */

/**
 * @file peripheral_config.h
 * @brief Peripheral hardware configuration constants
 * @details
 * Defines UART, I2C, SPI, and ADC configuration parameters.
 * Used by various hardware drivers and tasks.
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_PERIPHERAL_CONFIG_H
#define STAR_RX72N_PERIPHERAL_CONFIG_H

/* =============================================================================
 * UART Configuration
 * =============================================================================
 */

/**
 * @brief UART configuration
 * @details Debug UART (SCI9) settings
 */
typedef enum {
  k_uart_baudrate = 115200, /**< Debug UART baud rate */
} uart_config_t;

/* =============================================================================
 * I2C Configuration
 * =============================================================================
 */

/**
 * @brief I2C configuration
 * @details RIIC settings for battery management (BQ4050)
 */
typedef enum {
  k_i2c_frequency_hz = 400000, /**< I2C clock frequency (400 kHz) */
  k_riic_channel     = 0,      /**< RIIC channel for BQ4050 */
} i2c_config_t;

/* =============================================================================
 * SPI Configuration
 * =============================================================================
 */

/**
 * @brief SPI configuration
 * @details RSPI settings for motor drivers and RPi5 communication
 */
typedef enum {
  k_rspi_channel   = 0, /**< RSPI channel for motor drivers */
  k_rspi_mode      = 0, /**< SPI mode 0 (CPOL=0, CPHA=0) */
  k_rspi_use_16bit = 0, /**< Use 8-bit frames (false) */
} spi_config_t;

/* =============================================================================
 * ADC Configuration
 * =============================================================================
 */

/**
 * @brief ADC configuration
 * @details S12ADFa settings for current sensing
 */
typedef enum {
  k_adc_unit       = 0,  /**< ADC unit 0 */
  k_adc_resolution = 12, /**< 12-bit ADC resolution */
} adc_config_t;

#endif /* STAR_RX72N_PERIPHERAL_CONFIG_H */
