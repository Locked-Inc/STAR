/* lib/rx_core/inc/rx_port_constants.h */

/**
 * @file rx_port_constants.h
 * @brief Centralized RX72N Port Number Definitions
 *
 * LIBRARY-LEVEL SINGLE SOURCE OF TRUTH for all port and pin numbers.
 * This is the ONLY file in the library layer that should contain hex values
 * for port/pin numbers.
 *
 * All library code must use these constants - never hardcode port numbers!
 *
 * Architecture:
 * - This file defines the port/pin constants (with hex values)
 * - lib/rx_hal/ uses these constants (NO hex in lib code)
 * - include/hardware_pinout.h uses these constants (NO hex in app code)
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_PORT_CONSTANTS_H
#define STAR_RX_PORT_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* =============================================================================
 * Port Number Constants
 * =============================================================================
 */

/**
 * @brief RX72N Port Numbers
 *
 * Used by HAL layer for hardware access.
 * Used by application layer for pin definitions.
 *
 * These match the Renesas RX72N port naming convention and the encoding
 * used in hardware_pinout.h gpio_pin_t enum where:
 *   gpio_pin_t = (port << 8) | pin
 *
 * Example library use:
 *   case k_rx_port_b: return portb();
 *
 * Example application use:
 *   k_gpio_pb2 = (k_rx_port_b << 8) | k_rx_pin_2
 */
typedef enum {
  /* Decimal Ports (0-9) */
  k_rx_port_0 = 0x00, /**< Port 0 */
  k_rx_port_1 = 0x01, /**< Port 1 */
  k_rx_port_2 = 0x02, /**< Port 2 */
  k_rx_port_3 = 0x03, /**< Port 3 */
  k_rx_port_4 = 0x04, /**< Port 4 */
  k_rx_port_5 = 0x05, /**< Port 5 */
  k_rx_port_6 = 0x06, /**< Port 6 */
  k_rx_port_7 = 0x07, /**< Port 7 */
  k_rx_port_8 = 0x08, /**< Port 8 */
  k_rx_port_9 = 0x09, /**< Port 9 */

  /* Hexadecimal Ports (A-G, J) */
  k_rx_port_a = 0x0A, /**< Port A */
  k_rx_port_b = 0x0B, /**< Port B */
  k_rx_port_c = 0x0C, /**< Port C */
  k_rx_port_d = 0x0D, /**< Port D */
  k_rx_port_e = 0x0E, /**< Port E */
  k_rx_port_f = 0x0F, /**< Port F */
  k_rx_port_g = 0x10, /**< Port G */
  k_rx_port_j = 0x13, /**< Port J (not contiguous - note gap after Port G!) */
} rx_port_number_t;

/* =============================================================================
 * Pin Number Constants
 * =============================================================================
 */

/**
 * @brief Pin number constants (0-7)
 *
 * All RX72N ports have pins numbered 0-7. Use these constants instead of
 * raw numbers for self-documenting code.
 *
 * Example:
 *   gpio_pin_t my_pin = (k_rx_port_b << 8) | k_rx_pin_2;  // PB2
 */
typedef enum {
  k_rx_pin_0 = 0, /**< Pin 0 */
  k_rx_pin_1 = 1, /**< Pin 1 */
  k_rx_pin_2 = 2, /**< Pin 2 */
  k_rx_pin_3 = 3, /**< Pin 3 */
  k_rx_pin_4 = 4, /**< Pin 4 */
  k_rx_pin_5 = 5, /**< Pin 5 */
  k_rx_pin_6 = 6, /**< Pin 6 */
  k_rx_pin_7 = 7, /**< Pin 7 */

  k_rx_pin_max = 7, /**< Maximum pin number (for validation) */
} rx_pin_number_t;

/* =============================================================================
 * Bit Shift Constants
 * =============================================================================
 */

/**
 * @brief Bit shift constants for port/pin encoding
 *
 * Used when constructing gpio_pin_t values from port and pin numbers.
 *
 * Example:
 *   gpio_pin_t pin = (port << k_port_shift) | pin_num;
 */
typedef enum {
  k_port_shift = 8, /**< Left shift amount to move port to upper byte */
} port_encoding_t;

/* =============================================================================
 * Compile-Time Verification (Static Assertions)
 * =============================================================================
 */

/*
 * Verify port number constants match expected hex values.
 * These assertions ensure that the port numbering matches Renesas RX72N
 * documentation and the encoding used in hardware_pinout.h.
 */
_Static_assert(k_rx_port_0 == 0x00, "Port 0 must be 0x00");
_Static_assert(k_rx_port_1 == 0x01, "Port 1 must be 0x01");
_Static_assert(k_rx_port_5 == 0x05, "Port 5 must be 0x05");
_Static_assert(k_rx_port_a == 0x0A, "Port A must be 0x0A");
_Static_assert(k_rx_port_b == 0x0B, "Port B must be 0x0B");
_Static_assert(k_rx_port_c == 0x0C, "Port C must be 0x0C");
_Static_assert(k_rx_port_e == 0x0E, "Port E must be 0x0E");
_Static_assert(k_rx_port_j == 0x13, "Port J must be 0x13 (not contiguous)");

/*
 * Verify pin number constants match expected values.
 */
_Static_assert(k_rx_pin_0 == 0, "Pin 0 must be 0");
_Static_assert(k_rx_pin_2 == 2, "Pin 2 must be 2");
_Static_assert(k_rx_pin_7 == 7, "Pin 7 must be 7");
_Static_assert(k_rx_pin_max == 7, "Maximum pin number must be 7");

/*
 * Verify shift constant is correct.
 */
_Static_assert(k_port_shift == 8, "Port shift must be 8 bits");

/*
 * Verify encoding matches expected pattern: (port << 8) | pin
 * Example: Port B (0x0B), Pin 2 (0x02) = 0x0B02
 */
_Static_assert(((k_rx_port_b << k_port_shift) | k_rx_pin_2) == 0x0B02,
               "Port B Pin 2 encoding must be 0x0B02");
_Static_assert(((k_rx_port_e << k_port_shift) | k_rx_pin_5) == 0x0E05,
               "Port E Pin 5 encoding must be 0x0E05");
_Static_assert(((k_rx_port_0 << k_port_shift) | k_rx_pin_7) == 0x0007,
               "Port 0 Pin 7 encoding must be 0x0007");

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_PORT_CONSTANTS_H */
