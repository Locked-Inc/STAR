/* tests/mocks/mock_rx_gpio.h */

/**
 * @file mock_rx_gpio.h
 * @brief Mock high-level GPIO functions for bus protocol testing
 *
 * @details
 * Provides test double for high-level GPIO functions (rx_gpio) used by
 * bus protocols (1-Wire, bit-banged I2C). Tracks pin state and direction
 * for verification in unit tests.
 *
 * Enables testing of:
 * - 1-Wire bit-level communication
 * - GPIO-based bus protocols
 * - Pin direction changes (open-drain simulation)
 * - Bit timing sequences
 *
 * @par Mock vs Real:
 * | Aspect | Mock | Real |
 * |--------|------|------|
 * | Hardware | Simulated state | Actual PORT registers |
 * | Timing | No delays | Actual GPIO delays |
 *
 * @par Usage: tests/test_rx_onewire.c, tests/test_rx_ds18b20.c
 *
 * @see rx_gpio.h Real GPIO functions
 * @see mock_gpio_hal.h Lower-level GPIO HAL mock
 *
 * @par NASA Power of 10: ✓ Static allocation
 * @par SOLID: D - Bus protocols depend on GPIO interface
 *
 * @author STAR Team
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Constants
 * =============================================================================
 */

/**
 * @brief Mock GPIO constants
 */
typedef enum : uint32_t {
  /* Size must accommodate internal_pin_to_index: (port << k_port_shift) | pin_num
   * With k_port_shift == 8 and k_pin_bits == 8, maximum index is
   * (0xFF << 8) | 0xFF = 0xFFFF + 1 = 65536
   * Compute as: (1u << (k_port_shift + k_pin_bits)) to track the port-shift encoding
   */
  k_pin_bits = 8, /**< Number of bits used for the pin field in rx_port_pin_t encoding */
  k_mock_gpio_max_pins =
    (1u << (k_port_shift +
            k_pin_bits)), /**< Full index space for (port<<k_port_shift)|pin encoding */
  k_mock_gpio_max_read_sequence = 512, /**< Maximum programmable read sequence length */
} mock_gpio_constants_t;

/* =============================================================================
 * Mock State Control
 * =============================================================================
 */

/**
 * @brief Initialize mock GPIO subsystem
 *
 * Clears all pin states and directions.
 */
void mock_gpio_init(void);

/**
 * @brief Deinitialize mock GPIO subsystem
 */
void mock_gpio_deinit(void);

/**
 * @brief Set the value to be returned by the next gpio_read call
 *
 * @param[in] pin GPIO pin enum
 * @param[in] high True for high, false for low
 */
void mock_gpio_set_read_value(rx_port_pin_t pin, bool high);

/**
 * @brief Get the last written value for a pin
 *
 * @param[in] pin GPIO pin enum
 *
 * @return Last written value (true = high, false = low)
 */
bool mock_gpio_get_written_value(rx_port_pin_t pin);

/**
 * @brief Check if pin is configured as output
 *
 * @param[in] pin GPIO pin enum
 *
 * @return True if pin is output, false if input
 */
bool mock_gpio_is_output(rx_port_pin_t pin);

/**
 * @brief Set next error to return from GPIO operations
 *
 * @param[in] err Error to return on next GPIO call
 */
void mock_gpio_set_next_error(rx_err_t err);

/**
 * @brief Get number of times gpio_write_low was called
 *
 * @return Call count
 */
uint32_t mock_gpio_get_write_low_count(void);

/**
 * @brief Get number of times gpio_write_high was called
 *
 * @return Call count
 */
uint32_t mock_gpio_get_write_high_count(void);

/**
 * @brief Get number of times gpio_set_output was called
 *
 * @return Call count
 */
uint32_t mock_gpio_get_set_output_count(void);

/**
 * @brief Get number of times gpio_set_input was called
 *
 * @return Call count
 */
uint32_t mock_gpio_get_set_input_count(void);

/**
 * @brief Get number of times gpio_read was called
 *
 * @return Call count
 */
uint32_t mock_gpio_get_read_count(void);

/**
 * @brief Reset all call counters
 */
void mock_gpio_reset_counters(void);

/**
 * @brief Program a sequence of read values for successive gpio_read() calls
 *
 * When a sequence is active, each gpio_read() on the specified pin returns
 * the next value from the sequence. After the sequence is exhausted, reads
 * fall back to the static value set by mock_gpio_set_read_value().
 *
 * @param[in] pin GPIO pin enum
 * @param[in] values Array of boolean values (true=high, false=low)
 * @param[in] count Number of values in the sequence
 */
void mock_gpio_set_read_sequence(rx_port_pin_t pin, const bool* values, uint32_t count);

/**
 * @brief Clear any active read sequence for a pin
 *
 * @param[in] pin GPIO pin enum
 */
void mock_gpio_clear_read_sequence(rx_port_pin_t pin);

/* =============================================================================
 * GPIO Functions (Mock Implementations)
 * =============================================================================
 */

/**
 * @brief Set GPIO pin as output
 *
 * @param[in] pin GPIO pin enum
 *
 * @return k_rx_ok on success, or injected error
 */
rx_err_t gpio_set_output(rx_port_pin_t pin);

/**
 * @brief Set GPIO pin as input
 *
 * @param[in] pin GPIO pin enum
 *
 * @return k_rx_ok on success, or injected error
 */
rx_err_t gpio_set_input(rx_port_pin_t pin);

/**
 * @brief Write GPIO pin low
 *
 * @param[in] pin GPIO pin enum
 *
 * @return k_rx_ok on success, or injected error
 */
rx_err_t gpio_write_low(rx_port_pin_t pin);

/**
 * @brief Write GPIO pin high
 *
 * @param[in] pin GPIO pin enum
 *
 * @return k_rx_ok on success, or injected error
 */
rx_err_t gpio_write_high(rx_port_pin_t pin);

/**
 * @brief Read GPIO pin state
 *
 * @param[in] pin GPIO pin enum
 * @param[out] high Pin state (true = high, false = low)
 *
 * @return k_rx_ok on success, or injected error
 */
rx_err_t gpio_read(rx_port_pin_t pin, bool* high);

#ifdef __cplusplus
}
#endif
