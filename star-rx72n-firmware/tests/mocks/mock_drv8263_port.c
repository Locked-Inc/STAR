/**
 * @file mock_drv8263_port.c
 * @brief Mock PORT register implementation for DRV8263 unit testing
 *
 * @details
 * Provides RAM-based port register storage and test helper functions.
 * The g_mock_port_regs array is referenced by the mock port accessor
 * functions in drv8263/rx72n_port_regs.h.
 *
 * @author STAR Team
 * @date 2026-03-03
 * @version 1.0.0
 * @copyright STAR Project
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 3: Static allocation for g_mock_port_regs (no dynamic memory)
 * - Rule 4: All functions under 60 lines
 * - Rule 5: Assertions validate preconditions and postconditions
 *
 * @par SOLID Principles:
 * - **L (Liskov Substitution):** Mock PORT register API substitutes for real
 *   hardware registers, enabling host-side unit testing of rx_drv8263.c
 * - **S (Single Responsibility):** Each function handles one operation
 *   (reset, set, get) on the mock port register array
 */

#include "mock_drv8263_port.h"

#include "rx72n_port_regs.h"

#include <assert.h>
#include <string.h>

/* =============================================================================
 * File-Scope Named Constants
 * ============================================================================= */

/**
 * @enum memset_zero_t
 * @brief Zero-fill constant for memset calls
 * @see mock_drv8263_port_reset() Uses k_memset_zero to zero g_mock_port_regs
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_memset_zero = 0, /**< Zero-fill value for memset (clears all register bytes) */
} memset_zero_t;

/**
 * @enum mock_port_index_t
 * @brief First valid index into the g_mock_port_regs array
 * @see g_mock_port_regs Global array indexed by this value
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mock_port_first_index = 0, /**< First index in g_mock_port_regs (port 0 entry) */
} mock_port_index_t;

/**
 * @enum mock_port_reset_t
 * @brief Expected register value after array reset
 * @see mock_drv8263_port_reset() Postcondition uses k_mock_port_reset_value
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mock_port_reset_value = 0, /**< Expected PODR value after reset (all bits cleared) */
} mock_port_reset_t;

/**
 * @enum mock_port_default_t
 * @brief Default return values for out-of-bounds mock port queries
 *
 * @details
 * Provides named constants for default/fallback return values,
 * eliminating magic literals per NASA Rule 8.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mock_port_default_podr = 0, /**< Default PODR value for invalid port */
} mock_port_default_t;

/**
 * @enum mock_bit_const_t
 * @brief Named constants for bit manipulation operations
 *
 * @details
 * Replaces magic literals 1 and 0 used in bit shift and comparison
 * operations per NASA Rule 8.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bit_clear      = 0, /**< Bit comparison value for cleared state */
  k_bit_shift_base = 1, /**< Base value for single-bit shift masks */
} mock_bit_const_t;

/* =============================================================================
 * Global Mock Port Register Array
 * ============================================================================= */

/**
 * @var g_mock_port_regs
 * @brief Global RAM array of mock port register structs
 *
 * @details
 * Provides k_mock_port_max entries of rx_port_regs_t, backing all mock port
 * accessor functions (port0() through portj()). Zeroed by
 * mock_drv8263_port_reset() before each test.
 *
 * @note Accessed by port accessor inlines in drv8263/rx72n_port_regs.h
 *
 * @warning Do not modify directly in test code; use the mock_drv8263_port_*
 *          helper functions for clean test interfaces
 *
 * @see mock_drv8263_port_reset() Clears all entries to zero before each test
 * @see mock_drv8263_port_set_pidr() Writes simulated input pin state
 * @see mock_drv8263_port_get_podr() Reads output pin state written by DUT
 * @see mock_drv8263_port_get_pin_output() Reads a single output pin bit
 * @see mock_drv8263_port_set_pin_input() Sets a single input pin bit
 *
 * @since Version 1.0.0
 */
rx_port_regs_t g_mock_port_regs[k_mock_port_max];

/* =============================================================================
 * Test Helper Functions
 * ============================================================================= */

/**
 * @brief Reset all mock port registers to zero
 *
 * @details
 * Clears the entire g_mock_port_regs array using memset. Should be called in
 * test setup before each test case to ensure a known-clean initial state.
 *
 * @pre g_mock_port_regs array is statically allocated and always valid
 * @pre No concurrent access to g_mock_port_regs during reset
 *
 * @post All k_mock_port_max port register structs contain zero
 * @post All PDR, PODR, PIDR, PMR, PCR, DSCR, DSCR2 fields are zero
 *
 * @note Not thread-safe; call only from single-threaded test setup
 *
 * @since Version 1.0.0
 */
void mock_drv8263_port_reset(void)
{
  /** @brief Minimum valid array size (at least one port entry) */
  enum : uint8_t { k_mock_port_min_size = 1 };
  assert(sizeof(g_mock_port_regs) >= k_mock_port_min_size * sizeof(rx_port_regs_t));
  (void)memset(g_mock_port_regs, k_memset_zero, sizeof(g_mock_port_regs));
  assert(g_mock_port_regs[k_mock_port_first_index].podr == k_mock_port_reset_value);
}

/**
 * @brief Set the PIDR (input data register) value for a port
 *
 * @details
 * Writes a full byte to the PIDR field of the specified mock port. Used to
 * simulate hardware input pin states. Asserts that port is in bounds.
 *
 * @param[in] port Port index, must be less than k_mock_port_max
 * @param[in] value Byte value for PIDR; each bit represents one pin
 *                  (k_mock_pins_per_port bits total)
 *
 * @pre port must be less than k_mock_port_max
 * @pre g_mock_port_regs must be initialized via mock_drv8263_port_reset()
 *
 * @post g_mock_port_regs[port].pidr == value
 * @post Other port registers are unmodified
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @since Version 1.0.0
 */
void mock_drv8263_port_set_pidr(uint8_t port, uint8_t value)
{
  assert(port < k_mock_port_max);
  if (port < k_mock_port_max) {
    g_mock_port_regs[port].pidr = value;
    assert(g_mock_port_regs[port].pidr == value);
  }
}

/**
 * @brief Get the PODR (output data register) value for a port
 *
 * @details
 * Reads the PODR field of the specified mock port. Used to verify that the
 * device-under-test wrote expected values to output pins. Asserts that port
 * is in bounds.
 *
 * @param[in] port Port index, must be less than k_mock_port_max
 *
 * @return Current PODR byte value for the requested port
 * @retval 0x00-0xFF Valid PODR value when port is in range
 * @retval 0 If port >= k_mock_port_max (after assertion failure in debug)
 *
 * @pre port must be less than k_mock_port_max
 * @pre g_mock_port_regs must be initialized via mock_drv8263_port_reset()
 *
 * @post No side effects on any port registers
 * @post Returned value reflects current PODR contents
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @since Version 1.0.0
 */
uint8_t mock_drv8263_port_get_podr(uint8_t port)
{
  assert(port < k_mock_port_max);
  assert(sizeof(g_mock_port_regs) == k_mock_port_max * sizeof(rx_port_regs_t));
  if (port < k_mock_port_max) {
    return g_mock_port_regs[port].podr;
  }
  return k_mock_port_default_podr;
}

/**
 * @brief Check if a specific output pin is HIGH
 *
 * @details
 * Reads a single bit from the PODR field of the specified mock port register.
 * Extracts the bit at position 'pin' and returns whether it is set. Asserts
 * that both port and pin are in bounds.
 *
 * @param[in] port Port index, must be less than k_mock_port_max
 * @param[in] pin  Pin number, must be less than k_mock_pins_per_port
 *
 * @return true if pin output is HIGH, false otherwise
 * @retval true  PODR bit at position 'pin' is set
 * @retval false PODR bit is clear, or indices out of range
 *
 * @pre port must be less than k_mock_port_max
 * @pre pin must be less than k_mock_pins_per_port
 *
 * @post No side effects on any port registers
 * @post Returned value reflects current PODR bit state
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @since Version 1.0.0
 */
bool mock_drv8263_port_get_pin_output(uint8_t port, uint8_t pin)
{
  assert(port < k_mock_port_max);
  assert(pin < k_mock_pins_per_port);
  if (port < k_mock_port_max && pin < k_mock_pins_per_port) {
    return (g_mock_port_regs[port].podr &
            (uint8_t)((uint8_t)k_bit_shift_base << pin)) !=
           (uint8_t)k_bit_clear;
  }
  return false;
}

/**
 * @brief Set a specific input pin HIGH or LOW
 *
 * @details
 * Sets or clears a single bit in the PIDR field of the specified mock port.
 * When high is true, the bit at position 'pin' is set via OR mask; when
 * false, it is cleared via AND with inverted mask. Asserts that both port
 * and pin are in bounds.
 *
 * @param[in] port Port index, must be less than k_mock_port_max
 * @param[in] pin  Pin number, must be less than k_mock_pins_per_port
 * @param[in] high true to set pin HIGH (bit = 1), false to set LOW (bit = 0)
 *
 * @pre port must be less than k_mock_port_max
 * @pre pin must be less than k_mock_pins_per_port
 *
 * @post PIDR bit at position 'pin' matches 'high' parameter
 * @post Other PIDR bits and all other registers are unmodified
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @since Version 1.0.0
 */
void mock_drv8263_port_set_pin_input(uint8_t port, uint8_t pin, bool high)
{
  assert(port < k_mock_port_max);
  assert(pin < k_mock_pins_per_port);
  if (port < k_mock_port_max && pin < k_mock_pins_per_port) {
    const uint8_t mask = (uint8_t)((uint8_t)k_bit_shift_base << pin);
    if (high) {
      g_mock_port_regs[port].pidr |= mask;
    } else {
      g_mock_port_regs[port].pidr &= (uint8_t)~mask;
    }
    assert(high ? ((g_mock_port_regs[port].pidr & mask) != k_bit_clear)
                : ((g_mock_port_regs[port].pidr & mask) == k_bit_clear));
  }
}
