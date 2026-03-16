/**
 * @file mock_drv8263_port.h
 * @brief Test helper API for DRV8263 mock port registers
 *
 * @details
 * Provides functions to manipulate mock GPIO port registers for testing
 * the rx_drv8263 module. Allows tests to set simulated input pin values
 * (PIDR) and read output pin values (PODR).
 *
 * @author Locked, Inc.
 * @date 2026-03-03
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 3: No dynamic allocation; g_mock_port_regs is statically allocated
 * - Rule 5: All functions validate port/pin bounds before access
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Each function handles one mock operation
 * - **I (Interface Segregation):** API split into reset, set, get operations
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Port Limit Constants
 * ============================================================================= */

/**
 * @enum mock_port_limits_t
 * @brief Bounds constants for mock port register arrays and pin widths
 *
 * @details
 * Defines the array size for mock port registers and the number of pins per
 * port. Used throughout the mock implementation for bounds checking and
 * iteration limits.
 *
 * @code
 * assert(port < k_mock_port_count);
 * for (uint8_t i = 0; i < k_mock_pins_per_port; i++) { ... }
 * @endcode
 *
 * @see mock_drv8263_port_reset() Uses k_mock_port_count
 * @see mock_drv8263_port_get_pin_output() Uses k_mock_pins_per_port
 *
 * @invariant k_mock_port_count must equal the hardware RX72N port count (20 ports)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mock_port_count    = 20, /**< Number of mock port register structs
                                  (ports 0-9, A-F, G, reserved, J, spare) */
  k_mock_pins_per_port = 8,  /**< Number of pins per port (8-bit width) */
} mock_port_limits_t;

/* =============================================================================
 * Test Helper Functions
 * ============================================================================= */

/**
 * @brief Reset all mock port registers to zero
 *
 * @details
 * Clears the entire g_mock_port_regs array (all k_mock_port_count entries)
 * to zero using memset. This should be called in test setup (before each
 * test case) to ensure a clean starting state.
 *
 * @pre g_mock_port_regs array must be defined (in mock_drv8263_port.c)
 * @pre No concurrent access to g_mock_port_regs during reset
 *
 * @post All k_mock_port_count port register structs contain zero
 * @post All PDR, PODR, PIDR, PMR, PCR, DSCR, DSCR2 fields are zero
 *
 * @note Not thread-safe; call only from single-threaded test setup
 *
 * @see mock_drv8263_port_set_pidr() Set simulated input after reset
 * @see mock_drv8263_port_get_podr() Read output after device-under-test runs
 *
 * @since Version 1.0.0
 */
void mock_drv8263_port_reset(void);

/**
 * @brief Set the PIDR (input data register) value for a port
 *
 * @details
 * Writes a byte value to the PIDR field of the specified mock port register.
 * Each bit in the value corresponds to one pin (bit 0 = pin 0, etc.).
 * Used to simulate hardware input pin states before exercising the
 * device-under-test.
 *
 * @param[in] port Port index in range [0, k_mock_port_count). Uses the same
 *                 indices as the real hardware (e.g., k_port_idx_a = 10 for
 *                 Port A)
 * @param[in] value Byte value for PIDR; each bit represents one pin, with
 *                  k_mock_pins_per_port bits total
 *
 * @pre port must be less than k_mock_port_count
 * @pre g_mock_port_regs array must be initialized (call
 *      mock_drv8263_port_reset() first)
 *
 * @post g_mock_port_regs[port].pidr == value when port is valid
 * @post No effect if port >= k_mock_port_count (silent bounds check)
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @see mock_drv8263_port_set_pin_input() Set a single pin instead of full byte
 * @see mock_drv8263_port_get_podr() Read the output register
 *
 * @since Version 1.0.0
 */
void mock_drv8263_port_set_pidr(uint8_t port, uint8_t value);

/**
 * @brief Get the PODR (output data register) value for a port
 *
 * @details
 * Reads the PODR field of the specified mock port register. Each bit in the
 * returned value corresponds to one output pin. Used to verify that the
 * device-under-test wrote expected values to output pins.
 *
 * @param[in] port Port index in range [0, k_mock_port_count). Uses the same
 *                 indices as the real hardware
 *
 * @return Current PODR byte value for the requested port
 * @retval 0 If port >= k_mock_port_count (out-of-bounds returns zero)
 * @retval 0x00-0xFF Valid PODR value when port is in range
 *
 * @pre port must be less than k_mock_port_count
 * @pre g_mock_port_regs array must be initialized
 *
 * @post No side effects on any port registers
 * @post Returned value reflects current PODR contents
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @see mock_drv8263_port_get_pin_output() Query a single output pin
 * @see mock_drv8263_port_set_pidr() Set corresponding input register
 *
 * @since Version 1.0.0
 */
uint8_t mock_drv8263_port_get_podr(uint8_t port);

/**
 * @brief Check if a specific output pin is HIGH
 *
 * @details
 * Reads a single bit from the PODR field of the specified mock port register.
 * Extracts bit at position 'pin' from PODR and returns whether it is set.
 * Useful for verifying individual GPIO output pin states after the
 * device-under-test runs.
 *
 * @param[in] port Port index in range [0, k_mock_port_count)
 * @param[in] pin  Pin number in range [0, k_mock_pins_per_port)
 *
 * @return true if pin output is HIGH, false otherwise
 * @retval true  PODR bit at position 'pin' is set
 * @retval false PODR bit at position 'pin' is clear, or indices out of range
 *
 * @pre port must be less than k_mock_port_count
 * @pre pin must be less than k_mock_pins_per_port
 *
 * @post No side effects on any port registers
 * @post Returned value reflects current PODR bit state
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @see mock_drv8263_port_get_podr() Read full PODR byte
 * @see mock_drv8263_port_set_pin_input() Set corresponding input pin
 *
 * @since Version 1.0.0
 */
bool mock_drv8263_port_get_pin_output(uint8_t port, uint8_t pin);

/**
 * @brief Set a specific input pin HIGH or LOW
 *
 * @details
 * Sets or clears a single bit in the PIDR field of the specified mock port
 * register. When high is true, the bit at position 'pin' is set using OR;
 * when false, it is cleared using AND with inverted mask. Used to simulate
 * individual GPIO input pin state changes.
 *
 * @param[in] port Port index in range [0, k_mock_port_count)
 * @param[in] pin  Pin number in range [0, k_mock_pins_per_port)
 * @param[in] high true to set pin HIGH (bit = 1), false to set LOW (bit = 0)
 *
 * @pre port must be less than k_mock_port_count
 * @pre pin must be less than k_mock_pins_per_port
 *
 * @post PIDR bit at position 'pin' matches 'high' parameter when indices valid
 * @post No effect if port or pin is out of range (silent bounds check)
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @see mock_drv8263_port_set_pidr() Set full PIDR byte instead of single pin
 * @see mock_drv8263_port_get_pin_output() Read corresponding output pin
 *
 * @since Version 1.0.0
 */
void mock_drv8263_port_set_pin_input(uint8_t port, uint8_t pin, bool high);

#ifdef __cplusplus
}
#endif
