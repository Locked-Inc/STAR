/* include/hardware.h */

/**
 * @file hardware.h
 * @brief Hardware Abstraction Layer
 *
 * Public interface for RX72N hardware drivers.
 */

#ifndef STAR_RX72N_HARDWARE_H
#define STAR_RX72N_HARDWARE_H

#include <stdbool.h>
#include <stdint.h>

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
 */
void system_init(void);

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

/**
 * @brief Configure GPIO pin as output
 *
 * @param[in] port Port number (0-9, or 0xA-0x10 for A-G)
 * @param[in] pin Pin number (0-7)
 */
void gpio_set_output(uint8_t port, uint8_t pin);

/**
 * @brief Configure GPIO pin as input
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 */
void gpio_set_input(uint8_t port, uint8_t pin);

/**
 * @brief Set GPIO pin high
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 */
void gpio_write_high(uint8_t port, uint8_t pin);

/**
 * @brief Set GPIO pin low
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 */
void gpio_write_low(uint8_t port, uint8_t pin);

/**
 * @brief Toggle GPIO pin
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 */
void gpio_toggle(uint8_t port, uint8_t pin);

/**
 * @brief Read GPIO pin
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 * @return true if pin is high, false if low
 */
bool gpio_read(uint8_t port, uint8_t pin);

/* =============================================================================
 * Timer Functions
 * =============================================================================
 */

/**
 * @brief Initialize CMT0 for ThreadX system tick (100 Hz)
 */
void timer_init(void);

/**
 * @brief Stop CMT0 timer
 */
void timer_stop(void);

/**
 * @brief Get current CMT0 counter value
 *
 * @return Current counter value
 */
uint16_t timer_get_count(void);

/* =============================================================================
 * UART Functions (Debug Output)
 * =============================================================================
 */

/**
 * @brief Initialize SCI5 UART (115200 bps, 8N1, TX only)
 */
void uart_init(void);

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
