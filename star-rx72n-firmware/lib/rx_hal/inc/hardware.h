/* include/hardware.h */

/**
 * @file hardware.h
 * @brief Hardware Abstraction Layer
 *
 * Public interface for RX72N hardware drivers.
 * Includes core infrastructure (error codes, logging, interfaces).
 */

#ifndef STAR_RX72N_HARDWARE_H
#define STAR_RX72N_HARDWARE_H

#include <stdbool.h>
#include <stdint.h>

/* Core infrastructure */
#include "rx_check.h"
#include "rx_err.h"
#include "rx_infrastructure.h"
#include "rx_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * System Initialization
 * =============================================================================
 */

/**
 * @brief Initialize RX72N system (clocks, peripherals)
 *
 * Configures:
 * - PLL to 240 MHz
 * - Peripheral clocks (PCLKA=120MHz, PCLKB/C/D=60MHz)
 * - Module stop control
 *
 * Call this before ThreadX initialization.
 *
 * @return RX_OK on success, error code on failure
 */
rx_err_t system_init(void);

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

/**
 * @brief Configure GPIO pin as output
 *
 * @param[in] port Port number (0-9, or 0xA-0x10 for A-G)
 * @param[in] pin Pin number (0-7)
 *
 * @return RX_OK on success,
 *         RX_ERR_GPIO_INVALID_PORT if port is invalid,
 *         RX_ERR_GPIO_INVALID_PIN if pin is invalid,
 *         RX_ERR_GPIO_CONFLICT if pin already reserved
 */
rx_err_t gpio_set_output(uint8_t port, uint8_t pin);

/**
 * @brief Configure GPIO pin as input
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 *
 * @return RX_OK on success,
 *         RX_ERR_GPIO_INVALID_PORT if port is invalid,
 *         RX_ERR_GPIO_INVALID_PIN if pin is invalid,
 *         RX_ERR_GPIO_CONFLICT if pin already reserved
 */
rx_err_t gpio_set_input(uint8_t port, uint8_t pin);

/**
 * @brief Set GPIO pin high
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 *
 * @return RX_OK on success,
 *         RX_ERR_GPIO_INVALID_PORT if port is invalid,
 *         RX_ERR_GPIO_INVALID_PIN if pin is invalid
 */
rx_err_t gpio_write_high(uint8_t port, uint8_t pin);

/**
 * @brief Set GPIO pin low
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 *
 * @return RX_OK on success,
 *         RX_ERR_GPIO_INVALID_PORT if port is invalid,
 *         RX_ERR_GPIO_INVALID_PIN if pin is invalid
 */
rx_err_t gpio_write_low(uint8_t port, uint8_t pin);

/**
 * @brief Toggle GPIO pin
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 *
 * @return RX_OK on success,
 *         RX_ERR_GPIO_INVALID_PORT if port is invalid,
 *         RX_ERR_GPIO_INVALID_PIN if pin is invalid
 */
rx_err_t gpio_toggle(uint8_t port, uint8_t pin);

/**
 * @brief Read GPIO pin
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 * @param[out] value Pointer to store pin value (true=high, false=low)
 *
 * @return RX_OK on success,
 *         RX_ERR_GPIO_INVALID_PORT if port is invalid,
 *         RX_ERR_GPIO_INVALID_PIN if pin is invalid,
 *         RX_ERR_NULL_POINTER if value is NULL
 */
rx_err_t gpio_read(uint8_t port, uint8_t pin, bool* value);

/* =============================================================================
 * Timer Functions
 * =============================================================================
 */

/**
 * @brief Initialize CMT0 for ThreadX system tick (100 Hz)
 *
 * @return RX_OK on success, error code on failure
 */
rx_err_t timer_init(void);

/**
 * @brief Stop CMT0 timer
 *
 * @return RX_OK on success
 */
rx_err_t timer_stop(void);

/**
 * @brief Get current CMT0 counter value
 *
 * @param[out] count Pointer to store counter value
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if count is NULL
 */
rx_err_t timer_get_count(uint16_t* count);

/* =============================================================================
 * ADC Functions
 * =============================================================================
 */

/**
 * @brief Initialize ADC unit and channel
 *
 * @param[in] unit ADC unit (0 or 1)
 * @param[in] channel ADC channel (0-7)
 * @param[in] bits Resolution (8, 10, or 12 bits)
 *
 * @return RX_OK on success,
 *         RX_ERR_INVALID_ARG if unit, channel, or bits is invalid,
 *         RX_ERR_GPIO_CONFLICT if pin already reserved
 */
rx_err_t adc_init(uint8_t unit, uint8_t channel, uint8_t bits);

/**
 * @brief Read ADC value
 *
 * @param[in] unit ADC unit (0 or 1)
 * @param[in] channel ADC channel (0-7)
 * @param[out] value Pointer to store ADC value
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if value is NULL,
 *         RX_ERR_INVALID_ARG if unit or channel is invalid,
 *         RX_ERR_INVALID_STATE if ADC unit not initialized,
 *         RX_ERR_TIMEOUT if conversion times out
 */
rx_err_t adc_read(uint8_t unit, uint8_t channel, uint16_t* value);

/**
 * @brief Read ADC value and convert to millivolts
 *
 * @param[in] unit ADC unit (0 or 1)
 * @param[in] channel ADC channel (0-7)
 * @param[in] bits Resolution used during init (8, 10, or 12)
 * @param[out] voltage_mv Pointer to store voltage in millivolts
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if voltage_mv is NULL,
 *         RX_ERR_INVALID_ARG if unit or channel is invalid,
 *         RX_ERR_INVALID_STATE if ADC unit not initialized,
 *         RX_ERR_TIMEOUT if conversion times out
 */
rx_err_t adc_read_voltage_mv(uint8_t unit, uint8_t channel, uint8_t bits, uint32_t* voltage_mv);

/* =============================================================================
 * RIIC (I2C) Functions
 * =============================================================================
 */

/**
 * @brief Initialize RIIC channel for I2C communication
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[in] frequency_hz Clock frequency (100000, 400000, or 1000000)
 *
 * @return RX_OK on success,
 *         RX_ERR_INVALID_ARG if channel or frequency is invalid
 */
rx_err_t riic_init(uint8_t channel, uint32_t frequency_hz);

/**
 * @brief Write data to I2C device
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[in] device_addr 7-bit I2C device address
 * @param[in] data Pointer to data to write
 * @param[in] length Number of bytes to write
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if data is NULL,
 *         RX_ERR_INVALID_STATE if channel not initialized,
 *         RX_ERR_TIMEOUT if bus timeout,
 *         RX_ERR_NACK if device NACK received
 */
rx_err_t riic_write(uint8_t channel, uint8_t device_addr, const uint8_t* data, uint16_t length);

/**
 * @brief Read data from I2C device
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[in] device_addr 7-bit I2C device address
 * @param[out] data Pointer to buffer for received data
 * @param[in] length Number of bytes to read
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if data is NULL,
 *         RX_ERR_INVALID_STATE if channel not initialized,
 *         RX_ERR_TIMEOUT if bus timeout,
 *         RX_ERR_NACK if device NACK received
 */
rx_err_t riic_read(uint8_t channel, uint8_t device_addr, uint8_t* data, uint16_t length);

/**
 * @brief Write then read from I2C device (combined transaction)
 *
 * Common pattern for reading registers: write register address, then read data.
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[in] device_addr 7-bit I2C device address
 * @param[in] write_data Pointer to data to write
 * @param[in] write_length Number of bytes to write
 * @param[out] read_data Pointer to buffer for received data
 * @param[in] read_length Number of bytes to read
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if write_data or read_data is NULL,
 *         RX_ERR_INVALID_STATE if channel not initialized,
 *         RX_ERR_TIMEOUT if bus timeout,
 *         RX_ERR_NACK if device NACK received
 */
rx_err_t riic_write_read(uint8_t        channel,
                         uint8_t        device_addr,
                         const uint8_t* write_data,
                         uint16_t       write_length,
                         uint8_t*       read_data,
                         uint16_t       read_length);

/* =============================================================================
 * RSPI (SPI) Functions - Peripheral Mode
 * =============================================================================
 */

/**
 * @brief Initialize RSPI in peripheral mode for RPi5 communication
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[in] mode SPI mode (0-3): CPOL and CPHA configuration
 * @param[in] use_16bit True for 16-bit frames, false for 8-bit frames
 *
 * @return RX_OK on success,
 *         RX_ERR_INVALID_ARG if channel or mode is invalid
 */
rx_err_t rspi_init_peripheral(uint8_t channel, uint8_t mode, bool use_16bit);

/**
 * @brief Full-duplex SPI transfer in peripheral mode
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[in] tx_data Pointer to transmit data
 * @param[out] rx_data Pointer to receive buffer
 * @param[in] length Number of bytes to transfer
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if tx_data or rx_data is NULL,
 *         RX_ERR_INVALID_STATE if channel not initialized,
 *         RX_ERR_TIMEOUT if transfer timeout
 */
rx_err_t rspi_peripheral_transfer(uint8_t        channel,
                                  const uint8_t* tx_data,
                                  uint8_t*       rx_data,
                                  uint16_t       length);

/**
 * @brief Check if receive data is available
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[out] available Pointer to store availability status
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if available is NULL,
 *         RX_ERR_INVALID_STATE if channel not initialized
 */
rx_err_t rspi_peripheral_read_available(uint8_t channel, bool* available);

/**
 * @brief Check if transmit buffer is ready
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[out] ready Pointer to store ready status
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if ready is NULL,
 *         RX_ERR_INVALID_STATE if channel not initialized
 */
rx_err_t rspi_peripheral_write_ready(uint8_t channel, bool* ready);

/**
 * @brief Deinitialize RSPI channel
 *
 * @param[in] channel RSPI channel (0-2)
 *
 * @return RX_OK on success,
 *         RX_ERR_INVALID_ARG if channel is invalid
 */
rx_err_t rspi_deinit(uint8_t channel);

/* =============================================================================
 * UART Functions (Debug Output)
 * =============================================================================
 */

/**
 * @brief Initialize SCI5 UART (115200 bps, 8N1, TX only)
 *
 * @return RX_OK on success, error code on failure
 */
rx_err_t uart_init(void);

/**
 * @brief Transmit a single character
 *
 * @param[in] data Character to transmit
 */
void uart_putc(char data);

/**
 * @brief Transmit a null-terminated string
 *
 * @param[in] str Pointer to string
 */
void uart_puts(const char* str);

/**
 * @brief Print a signed integer
 *
 * @param[in] value Integer value to print
 */
void uart_putint(int32_t value);

/**
 * @brief Print a hexadecimal value
 *
 * @param[in] value Value to print
 * @param[in] digits Number of hex digits (1-8)
 */
void uart_puthex(uint32_t value, uint8_t digits);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_HARDWARE_H */
