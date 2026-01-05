/* lib/rx_hal/inc/hardware.h */

/**
 * @file hardware.h
 * @brief Hardware Abstraction Layer
 *
 * Public interface for RX72N hardware drivers.
 * Includes core infrastructure (error codes, logging, interfaces).
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_HARDWARE_H
#define STAR_RX72N_HARDWARE_H

#include <stdbool.h>
#include <stdint.h>

/* Core infrastructure */
#include "hardware_pinout.h"
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
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t system_init(void);

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

/**
 * @brief Configure GPIO pin as output
 *
 * @param[in] pin GPIO pin (type-safe enum from hardware_pinout.h)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid,
 *         k_rx_err_gpio_conflict if pin already reserved
 */
rx_err_t gpio_set_output(gpio_pin_t pin);

/**
 * @brief Configure GPIO pin as input
 *
 * @param[in] pin GPIO pin (type-safe enum from hardware_pinout.h)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid,
 *         k_rx_err_gpio_conflict if pin already reserved
 */
rx_err_t gpio_set_input(gpio_pin_t pin);

/**
 * @brief Set GPIO pin high
 *
 * @param[in] pin GPIO pin (type-safe enum from hardware_pinout.h)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid
 */
rx_err_t gpio_write_high(gpio_pin_t pin);

/**
 * @brief Set GPIO pin low
 *
 * @param[in] pin GPIO pin (type-safe enum from hardware_pinout.h)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid
 */
rx_err_t gpio_write_low(gpio_pin_t pin);

/**
 * @brief Toggle GPIO pin
 *
 * @param[in] pin GPIO pin (type-safe enum from hardware_pinout.h)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid
 */
rx_err_t gpio_toggle(gpio_pin_t pin);

/**
 * @brief Read GPIO pin
 *
 * @param[in] pin GPIO pin (type-safe enum from hardware_pinout.h)
 * @param[out] value Pointer to store pin value (true=high, false=low)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_gpio_invalid_port if port is invalid,
 *         k_rx_err_gpio_invalid_pin if pin is invalid,
 *         k_rx_err_null_pointer if value is NULL
 */
rx_err_t gpio_read(gpio_pin_t pin, bool* value);

/* =============================================================================
 * Timer Functions
 * =============================================================================
 */

/**
 * @brief Initialize CMT0 for ThreadX system tick (100 Hz)
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t timer_init(void);

/**
 * @brief Stop CMT0 timer
 *
 * @return k_rx_ok on success
 */
rx_err_t timer_stop(void);

/**
 * @brief Get current CMT0 counter value
 *
 * @param[out] count Pointer to store counter value
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if count is NULL
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
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if unit, channel, or bits is invalid,
 *         k_rx_err_gpio_conflict if pin already reserved
 */
rx_err_t adc_init(uint8_t unit, uint8_t channel, uint8_t bits);

/**
 * @brief Read ADC value
 *
 * @param[in] unit ADC unit (0 or 1)
 * @param[in] channel ADC channel (0-7)
 * @param[out] value Pointer to store ADC value
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if value is NULL,
 *         k_rx_err_invalid_arg if unit or channel is invalid,
 *         k_rx_err_invalid_state if ADC unit not initialized,
 *         k_rx_err_timeout if conversion times out
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
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if voltage_mv is NULL,
 *         k_rx_err_invalid_arg if unit or channel is invalid,
 *         k_rx_err_invalid_state if ADC unit not initialized,
 *         k_rx_err_timeout if conversion times out
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
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel or frequency is invalid
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
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if data is NULL,
 *         k_rx_err_invalid_state if channel not initialized,
 *         k_rx_err_timeout if bus timeout,
 *         k_rx_err_nack if device NACK received
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
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if data is NULL,
 *         k_rx_err_invalid_state if channel not initialized,
 *         k_rx_err_timeout if bus timeout,
 *         k_rx_err_nack if device NACK received
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
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if write_data or read_data is NULL,
 *         k_rx_err_invalid_state if channel not initialized,
 *         k_rx_err_timeout if bus timeout,
 *         k_rx_err_nack if device NACK received
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
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel or mode is invalid
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
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if tx_data or rx_data is NULL,
 *         k_rx_err_invalid_state if channel not initialized,
 *         k_rx_err_timeout if transfer timeout
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
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if available is NULL,
 *         k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rspi_peripheral_read_available(uint8_t channel, bool* available);

/**
 * @brief Check if transmit buffer is ready
 *
 * @param[in] channel RSPI channel (0-2)
 * @param[out] ready Pointer to store ready status
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if ready is NULL,
 *         k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rspi_peripheral_write_ready(uint8_t channel, bool* ready);

/**
 * @brief Deinitialize RSPI channel
 *
 * @param[in] channel RSPI channel (0-2)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t rspi_deinit(uint8_t channel);

/* =============================================================================
 * UART Functions (Multi-Channel)
 * =============================================================================
 */

/**
 * @brief Default debug UART channel (SCI9 - CY7C65213 USB bridge)
 */
typedef enum {
  k_uart_debug_channel = 9, /**< Debug UART on SCI9 (PB7/TXD9, PB6/RXD9) */
} uart_defaults_t;

/**
 * @brief Initialize SCI channel for UART communication
 *
 * Performs full hardware initialization including:
 * - Module stop control (MSTPCRB)
 * - Pin function configuration (MPC)
 * - GPIO direction setup (PDR/PMR)
 * - SCI register configuration
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] baudrate Baud rate (e.g., 9600, 115200)
 * @param[in] tx_port TX pin port number
 * @param[in] tx_pin TX pin number (0-7)
 * @param[in] rx_port RX pin port number
 * @param[in] rx_pin RX pin number (0-7)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel or pins are invalid,
 *         k_rx_err_invalid_state if channel already initialized
 */
rx_err_t uart_init_channel(uint8_t  channel,
                           uint32_t baudrate,
                           uint8_t  tx_port,
                           uint8_t  tx_pin,
                           uint8_t  rx_port,
                           uint8_t  rx_pin);

/**
 * @brief Deinitialize SCI channel
 *
 * @param[in] channel SCI channel (0-12)
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t uart_deinit_channel(uint8_t channel);

/**
 * @brief Transmit a single character on specified channel
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] data Character to transmit
 *
 * @return k_rx_ok on success,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized
 */
rx_err_t uart_putc_channel(uint8_t channel, char data);

/**
 * @brief Transmit a null-terminated string on specified channel
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] str Pointer to string
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if str is NULL,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized
 */
rx_err_t uart_puts_channel(uint8_t channel, const char* str);

/**
 * @brief Write buffer to specified channel
 *
 * @param[in] channel SCI channel (0-12)
 * @param[in] data Pointer to data buffer
 * @param[in] length Number of bytes to write
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if data is NULL,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized
 */
rx_err_t uart_write_channel(uint8_t channel, const uint8_t* data, uint16_t length);

/**
 * @brief Receive a single character from specified channel (non-blocking)
 *
 * @param[in] channel SCI channel (0-12)
 * @param[out] data Pointer to store received character
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if data is NULL,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized,
 *         k_rx_err_empty if no data available
 */
rx_err_t uart_getc_channel(uint8_t channel, char* data);

/**
 * @brief Read available data from specified channel (non-blocking)
 *
 * Reads up to length bytes that are currently available.
 *
 * @param[in] channel SCI channel (0-12)
 * @param[out] data Pointer to buffer for received data
 * @param[in] length Maximum number of bytes to read
 * @param[out] bytes_read Pointer to store actual bytes read
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if data or bytes_read is NULL,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized
 */
rx_err_t uart_read_channel(uint8_t   channel,
                           uint8_t*  data,
                           uint16_t  length,
                           uint16_t* bytes_read);

/**
 * @brief Check if receive data is available on channel
 *
 * @param[in] channel SCI channel (0-12)
 * @param[out] available Pointer to store availability status
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if available is NULL,
 *         k_rx_err_invalid_arg if channel is invalid,
 *         k_rx_err_invalid_state if channel not initialized
 */
rx_err_t uart_rx_available(uint8_t channel, bool* available);

/* =============================================================================
 * UART Functions (Legacy Debug Output - SCI9)
 * =============================================================================
 */

/**
 * @brief Initialize debug UART (SCI9, 115200 bps, 8N1)
 *
 * Wrapper for uart_init_channel(k_uart_debug_channel, 115200).
 * Used by rx_log before ThreadX is initialized.
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t uart_init(void);

/**
 * @brief Transmit a single character on debug UART (SCI9)
 *
 * @param[in] data Character to transmit
 */
void uart_putc(char data);

/**
 * @brief Transmit a null-terminated string on debug UART (SCI9)
 *
 * @param[in] str Pointer to string
 */
void uart_puts(const char* str);

/**
 * @brief Print a signed integer on debug UART (SCI9)
 *
 * @param[in] value Integer value to print
 */
void uart_putint(int32_t value);

/**
 * @brief Print a hexadecimal value on debug UART (SCI9)
 *
 * @param[in] value Value to print
 * @param[in] digits Number of hex digits (1-8)
 */
void uart_puthex(uint32_t value, uint8_t digits);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_HARDWARE_H */
