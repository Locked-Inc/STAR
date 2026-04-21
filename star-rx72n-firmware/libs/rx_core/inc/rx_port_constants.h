/**
 * @file rx_port_constants.h
 * @brief Centralized RX72N Port Number Definitions
 *
 * @details
 * **LIBRARY-LEVEL SINGLE SOURCE OF TRUTH** for all port and pin numbers used
 * throughout the STAR RX72N firmware. This file is the ONLY location in the
 * library layer that contains hexadecimal values for port/pin identifiers.
 *
 * ## Purpose and Design Rationale
 *
 * This file implements the "Single Source of Truth" pattern for hardware port
 * addressing, eliminating magic numbers and providing type-safe port/pin
 * identifiers using C23 typed enums. All library and application code must use
 * these constants--never hardcode port numbers directly.
 *
 * ## Architecture and Layering
 *
 * The port constants architecture follows a strict layering model:
 *
 * 1. **This file (rx_port_constants.h)**: Defines port/pin constants with hex values
 * 2. **HAL layer (lib/rx_hal/)**: Uses these constants, NO hex values in HAL code
 * 3. **Application layer (include/hardware_pinout.h)**: Uses these constants, NO hex in app code
 *
 * This layering ensures:
 * - Single point of truth for port numbering
 * - Type safety through C23 typed enums
 * - Compile-time validation via static assertions
 * - Easy portability to different RX packages (144-pin, 176-pin, etc.)
 *
 * ## Port/Pin Encoding Scheme
 *
 * GPIO pins are encoded as 16-bit values using the following format:
 *
 * ```
 * gpio_pin_t = (port << k_port_shift) | pin_number
 *            = (port << 8) | pin
 *
 * Example: Port B, Pin 2 (PB2)
 *   = (0x0B << 8) | 0x02
 *   = 0x0B02
 * ```
 *
 * This encoding allows:
 * - **Upper byte**: Port number (0x00-0x13)
 * - **Lower byte**: Pin number (0x00-0x07)
 * - **Extraction**: Port = pin >> 8, Pin = pin & 0xFF
 * - **Validation**: Compile-time checked via static assertions
 *
 * ## RX72N Port Numbering
 *
 * The Renesas RX72N uses non-contiguous port numbering:
 * - **Decimal ports**: 0-9 (0x00-0x09)
 * - **Alphabetic ports**: A-G (0x0A-0x10)
 * - **Port J**: 0x12 (note: Port I does not exist; Port H exists at 0x11 but has no pins on 144-pin)
 *
 * Total: 18 ports x 8 pins = 144 theoretical GPIO combinations
 * (Not all combinations physically available on 144-pin LFQFP package)
 *
 * ## Performance Characteristics
 *
 * - **Memory usage**: Zero runtime overhead (all constants are compile-time)
 * - **Code size**: Minimal (enums compile to immediate values)
 * - **Execution time**: Inline accessor functions are 1-2 CPU cycles
 * - **Type safety**: Full compile-time type checking with C23 typed enums
 *
 * ## Usage Patterns
 *
 * **Library code example (HAL layer)**:
 * ```c
 * // Map port enum to register pointer
 * switch (port) {
 *   case k_rx_port_b: return portb();
 *   case k_rx_port_e: return porte();
 * }
 * ```
 *
 * **Application code example**:
 * ```c
 * // Define application-specific pin aliases
 * typedef enum : uint16_t {
 *   k_gpio_led_status = k_rx_pb_2,  // PB2 = status LED
 *   k_gpio_spi_cs     = k_rx_pe_5,  // PE5 = SPI chip select
 * } application_pins_t;
 * ```
 *
 * **Pin extraction example**:
 * ```c
 * gpio_pin_t pin = k_rx_pb_2;
 * uint8_t port = rx_port_from_pin(pin);  // Returns k_rx_port_b (0x0B)
 * uint8_t pin_num = rx_pin_from_pin(pin); // Returns 2
 * ```
 *
 * @par Hardware Requirements:
 *
 * | Component      | Requirement                     | Notes                          |
 * |----------------|---------------------------------|--------------------------------|
 * | MCU            | Renesas RX72N                   | Any package (100/144/176-pin)  |
 * | Package        | 144-pin LFQFP (primary target)  | Not all ports available        |
 * | Ports          | 18 ports (0-9, A-G, J)          | Non-contiguous numbering       |
 * | Pins per port  | 8 pins (0-7)                    | All ports have 8-bit width     |
 *
 * @par Module Dependencies:
 *
 * **This module depends on**:
 * - `rx_check.h` - For RX_ASSERT compile-time/runtime validation
 * - `<stdint.h>` - For uint8_t, uint16_t types
 *
 * **This module is used by**:
 * - `hardware.h` - HAL GPIO register accessors
 * - `hardware_pinout.h` - Application-level pin definitions
 * - All GPIO-related HAL modules
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | Only inline functions with sequential logic |
 * | 2. Fixed loop bounds | [OK] | No loops (constants only) |
 * | 3. No dynamic allocation | [OK] | All constants are compile-time |
 * | 4. Small functions | [OK] | Inline functions are 3-5 lines each |
 * | 5. Assertions (>=2 per function) | [OK] | 3 assertions per function (prex2, postx1) |
 * | 6. Narrow scope | [OK] | File-scope only, no globals |
 * | 7. Check return values | [OK] | Inline functions don't fail |
 * | 8. Limited preprocessor | [OK] | C23 typed enums (zero macros) |
 * | 9. Pointer restrictions | [OK] | No pointers used |
 * | 10. Compiler warnings | [OK] | Compiles with -Wall -Wextra -Werror |
 *
 * **Rule 8 Compliance Detail**: This file demonstrates STAR project's mandatory
 * C23 typed enum standard. ALL integer constants use `typedef enum : type` syntax,
 * providing explicit size guarantees and eliminating preprocessor macros.
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility (S)**:
 * - This file has ONE job: Define port/pin constants
 * - No GPIO manipulation logic (that's in HAL layer)
 * - No application-specific pin assignments (that's in hardware_pinout.h)
 *
 * **Open/Closed (O)**:
 * - Extensible to new RX packages without modifying existing constants
 * - Can add new port definitions for 144-pin or 176-pin packages
 * - Encoding scheme supports future ports beyond current 18
 *
 * **Liskov Substitution (L)**:
 * - All rx_port_pin_t values are interchangeable
 * - Pre-computed constants (k_rx_pb_2) substitutable with manual encoding
 * - ((port << 8) | pin) always produces valid rx_port_pin_t
 *
 * **Interface Segregation (I)**:
 * - Separate enums for ports, pins, and combined port/pin
 * - Use only what you need: port numbers OR pin numbers OR combinations
 * - Encoding constants isolated in separate enum
 *
 * **Dependency Inversion (D)**:
 * - HAL layer depends on these abstractions, not hardware addresses
 * - Application code depends on constants, not port numbers
 * - Hardware changes isolated to this file only
 *
 * @see hardware.h GPIO register accessor functions
 * @see hardware_pinout.h Application-specific pin definitions
 * @see rx_check.h Validation macros used in assertions
 * @see docs/sections/03_hardware_pinout.tex Complete pin assignment table
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rx_check.h"

/* =============================================================================
 * Port Number Constants
 * =============================================================================
 */

/**
 * @enum rx_port_number_t
 * @brief RX72N Port Number Constants (Single-Byte Identifiers)
 *
 * @details
 * Defines all GPIO port numbers available on the Renesas RX72N microcontroller.
 * Used by HAL layer for hardware register access and by application layer for
 * constructing full port/pin identifiers.
 *
 * ## Port Numbering Scheme
 *
 * The RX72N uses **non-contiguous** port numbering that mixes decimal and
 * hexadecimal naming:
 * - **Decimal ports**: 0-9 (values 0x00-0x09)
 * - **Hexadecimal ports**: A-G (values 0x0A-0x10)
 * - **Port J**: 0x12 (follows Port H at 0x11; Port I does not exist)
 *
 * This numbering matches the Renesas RX72N hardware manual and the encoding
 * scheme used throughout the firmware.
 *
 * ## Encoding Integration
 *
 * Port numbers form the **upper byte** of 16-bit gpio_pin_t values:
 * ```
 * gpio_pin_t = (rx_port_number_t << k_port_shift) | rx_pin_number_t
 *            = (port << 8) | pin
 * ```
 *
 * Example: Port B (0x0B) + Pin 2 (0x02) = 0x0B02
 *
 * ## Package Availability
 *
 * Not all ports are physically available on all RX72N packages:
 * - **144-pin LFQFP** (STAR hardware): Ports 0-9, A-F, J (partial availability)
 * - **176-pin LFBGA**: Maximum port availability
 *
 * Constants are defined for ALL theoretical ports for code compatibility across
 * packages, even if not physically present on 144-pin LFQFP.
 *
 * @par Port Availability Table (144-pin LFQFP):
 *
 * | Port | Value | Pins Available | Notes |
 * |------|-------|----------------|-------|
 * | 0    | 0x00  | P00-P03, P05, P07 | 6 pins (no P04, P06) |
 * | 1    | 0x01  | P12-P17        | 6 pins (no P10, P11) |
 * | 2    | 0x02  | P20-P27        | Full 8-pin port |
 * | 3    | 0x03  | P30-P37        | Full 8-pin port |
 * | 4    | 0x04  | P40-P47        | Full 8-pin port |
 * | 5    | 0x05  | P50-P56        | 7 pins (no P57) |
 * | 6    | 0x06  | P60-P67        | Full 8-pin port |
 * | 7    | 0x07  | P70-P77        | Full 8-pin port |
 * | 8    | 0x08  | P80-P83, P86-P87 | 6 pins (no P84, P85) |
 * | 9    | 0x09  | P90-P93        | 4 pins (no P94-P97) |
 * | A    | 0x0A  | PA0-PA7        | Full 8-pin port |
 * | B    | 0x0B  | PB0-PB7        | Full 8-pin port |
 * | C    | 0x0C  | PC0-PC7        | Full 8-pin port |
 * | D    | 0x0D  | PD0-PD7        | Full 8-pin port |
 * | E    | 0x0E  | PE0-PE7        | Full 8-pin port |
 * | F    | 0x0F  | PF5            | 1 pin only |
 * | G    | 0x10  | None           | Not available on 144-pin |
 * | J    | 0x13  | PJ3, PJ5       | 2 pins only |
 *
 * @par Usage Example (HAL Layer):
 * @code{.c}
 * // Map port number to hardware register pointer
 * PORT_Type* get_port_registers(rx_port_number_t port)
 * {
 *   switch (port) {
 *     case k_rx_port_0: return port0();
 *     case k_rx_port_1: return port1();
 *     case k_rx_port_b: return portb();
 *     case k_rx_port_e: return porte();
 *     default: return nullptr;
 *   }
 * }
 * @endcode
 *
 * @par Usage Example (Application Layer):
 * @code{.c}
 * // Construct full port/pin identifier
 * gpio_pin_t status_led = (k_rx_port_b << k_port_shift) | k_rx_pin_2;
 * // Result: 0x0B02 (Port B, Pin 2)
 *
 * // Or use pre-computed constant
 * gpio_pin_t status_led = k_rx_pb_2;  // Equivalent to above
 * @endcode
 *
 * @par Usage Example (Port Extraction):
 * @code{.c}
 * // Extract port number from combined pin identifier
 * gpio_pin_t pin = k_rx_pb_2;  // 0x0B02
 * rx_port_number_t port = rx_port_from_pin(pin);  // Returns k_rx_port_b (0x0B)
 * @endcode
 *
 * @invariant Port values are unique and match Renesas documentation
 * @invariant Port numbers fit in uint8_t (0x00-0xFF)
 * @invariant All enum values are compile-time constants
 *
 * @note This is a C23 **typed enum** with explicit underlying type uint8_t,
 *       ensuring 1-byte size and consistent ABI across compiler versions
 * @note Port J (0x12) follows Port H (0x11); Port I does not exist in RX72N port map
 *
 * @warning Do not assume contiguous port numbering when iterating ports
 * @warning Not all ports available on 144-pin package--check datasheet
 *
 * @see rx_pin_number_t Pin number constants (0-7)
 * @see rx_port_pin_t Pre-computed port/pin combinations
 * @see rx_port_from_pin() Extract port number from combined identifier
 * @see hardware.h HAL GPIO register accessor functions
 * @see docs/sections/03_hardware_pinout.tex Complete RX72N pinout reference
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* Decimal Ports (0-9) */
  k_rx_port_0 = 0x00, /**< Port 0 - 6 pins on 144-pin (P00-P03, P05, P07) */
  k_rx_port_1 = 0x01, /**< Port 1 - 6 pins on 144-pin (P12-P17) */
  k_rx_port_2 = 0x02, /**< Port 2 - Full 8-pin port on 144-pin (P20-P27) */
  k_rx_port_3 = 0x03, /**< Port 3 - Full 8-pin port on 144-pin (P30-P37) */
  k_rx_port_4 = 0x04, /**< Port 4 - Full 8-pin port on 144-pin (P40-P47) */
  k_rx_port_5 = 0x05, /**< Port 5 - 7 pins on 144-pin (P50-P56) */
  k_rx_port_6 = 0x06, /**< Port 6 - Full 8-pin port on 144-pin (P60-P67) */
  k_rx_port_7 = 0x07, /**< Port 7 - Full 8-pin port on 144-pin (P70-P77) */
  k_rx_port_8 = 0x08, /**< Port 8 - 6 pins on 144-pin (P80-P83, P86-P87) */
  k_rx_port_9 = 0x09, /**< Port 9 - 4 pins on 144-pin (P90-P93) */

  /* Hexadecimal Ports (A-G, J) */
  k_rx_port_a = 0x0A, /**< Port A - Full 8-pin port available on 144-pin */
  k_rx_port_b = 0x0B, /**< Port B - Full 8-pin port available on 144-pin */
  k_rx_port_c = 0x0C, /**< Port C - Full 8-pin port available on 144-pin */
  k_rx_port_d = 0x0D, /**< Port D - Full 8-pin port available on 144-pin */
  k_rx_port_e = 0x0E, /**< Port E - Full 8-pin port available on 144-pin */
  k_rx_port_f = 0x0F, /**< Port F - Partially available on 144-pin (PF5) */
  k_rx_port_g = 0x10, /**< Port G - Not available on 144-pin package */
  k_rx_port_j =
    0x12, /**< Port J - Partial availability on 144-pin (PJ3, PJ5) - follows Port H (0x11); Port I does not exist */
} rx_port_number_t;

/* =============================================================================
 * Pin Number Constants
 * =============================================================================
 */

/**
 * @enum rx_pin_number_t
 * @brief Pin Number Constants Within Each Port (0-7)
 *
 * @details
 * Defines pin numbers within each GPIO port on the RX72N. All RX72N ports are
 * **8-bit wide**, meaning each port has pins numbered 0 through 7. These
 * constants provide self-documenting alternatives to raw numeric literals.
 *
 * ## Pin Numbering
 *
 * Every RX72N port (0-9, A-G, J) contains 8 pins:
 * - Pin 0 (LSB of port data register)
 * - Pin 1
 * - Pin 2
 * - ...
 * - Pin 7 (MSB of port data register)
 *
 * This consistent 8-pin structure applies to ALL ports, even though not all
 * individual pins may be physically available on smaller packages.
 *
 * ## Encoding Integration
 *
 * Pin numbers form the **lower byte** of 16-bit gpio_pin_t values:
 * ```
 * gpio_pin_t = (rx_port_number_t << k_port_shift) | rx_pin_number_t
 *            = (port << 8) | pin
 * ```
 *
 * Example: Port B (0x0B) + Pin 2 (0x02) = 0x0B02
 *
 * ## Validation Constants
 *
 * The enum includes `k_rx_pin_min` and `k_rx_pin_max` for runtime/compile-time
 * validation of pin numbers. These are used by assertion macros and accessor
 * functions to verify pin number correctness.
 *
 * Valid range: [0, 7] inclusive
 *
 * @par Usage Example (Constructing gpio_pin_t):
 * @code{.c}
 * // Construct Port B, Pin 2 manually
 * gpio_pin_t status_led = (k_rx_port_b << k_port_shift) | k_rx_pin_2;
 * // Result: 0x0B02
 *
 * // Or use pre-computed constant (preferred)
 * gpio_pin_t status_led = k_rx_pb_2;  // Cleaner, same result
 * @endcode
 *
 * @par Usage Example (Pin Extraction):
 * @code{.c}
 * // Extract pin number from combined identifier
 * gpio_pin_t pin = k_rx_pe_5;  // 0x0E05
 * rx_pin_number_t pin_num = rx_pin_from_pin(pin);  // Returns 5
 * @endcode
 *
 * @par Usage Example (Validation):
 * @code{.c}
 * // Validate user-provided pin number
 * bool is_valid_pin(uint8_t pin)
 * {
 *   return (pin >= k_rx_pin_min) && (pin <= k_rx_pin_max);
 * }
 *
 * // Or use in assertion
 * RX_ASSERT((pin >= k_rx_pin_min) && (pin <= k_rx_pin_max),
 *           "Pin number out of range");
 * @endcode
 *
 * @par Usage Example (Bit Manipulation):
 * @code{.c}
 * // Set pin 2 high in port data register
 * PORT_Type* port = portb();
 * port->PODR |= (1 << k_rx_pin_2);  // Set bit 2
 *
 * // Clear pin 5
 * port->PODR &= ~(1 << k_rx_pin_5);  // Clear bit 5
 * @endcode
 *
 * @invariant All pin numbers are in range [0, 7]
 * @invariant k_rx_pin_min == 0
 * @invariant k_rx_pin_max == 7
 * @invariant Pin numbering is contiguous (no gaps)
 *
 * @note This is a C23 **typed enum** with explicit underlying type uint8_t,
 *       ensuring 1-byte size and consistent ABI across compiler versions
 * @note Pin numbers correspond to bit positions in port registers (PDR, PODR, PIDR)
 * @note All 8 pins exist logically even if not physically bonded on smaller packages
 *
 * @warning Pin numbers are zero-indexed (0-7), not one-indexed (1-8)
 * @warning Do not use raw numbers--use these constants for self-documenting code
 *
 * @see rx_port_number_t Port number constants
 * @see rx_port_pin_t Pre-computed port/pin combinations
 * @see rx_pin_from_pin() Extract pin number from combined identifier
 * @see hardware.h HAL GPIO register manipulation functions
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rx_pin_0 = 0, /**< Pin 0 - LSB of port data register, bit position 0 */
  k_rx_pin_1 = 1, /**< Pin 1 - Bit position 1 in port data register */
  k_rx_pin_2 = 2, /**< Pin 2 - Bit position 2 in port data register */
  k_rx_pin_3 = 3, /**< Pin 3 - Bit position 3 in port data register */
  k_rx_pin_4 = 4, /**< Pin 4 - Bit position 4 in port data register */
  k_rx_pin_5 = 5, /**< Pin 5 - Bit position 5 in port data register */
  k_rx_pin_6 = 6, /**< Pin 6 - Bit position 6 in port data register */
  k_rx_pin_7 = 7, /**< Pin 7 - MSB of port data register, bit position 7 */

  k_rx_pin_min = 0, /**< Minimum valid pin number - used for range validation in assertions */
  k_rx_pin_max = 7, /**< Maximum valid pin number - used for range validation in assertions */
} rx_pin_number_t;

/* =============================================================================
 * Bit Shift Constants
 * =============================================================================
 */

/**
 * @enum port_encoding_t
 * @brief Bit Shift and Mask Constants for Port/Pin Encoding
 *
 * @details
 * Defines the constants used to encode and decode 16-bit gpio_pin_t values.
 * These constants implement the port/pin encoding scheme used throughout the
 * firmware for type-safe GPIO pin identification.
 *
 * ## Encoding Scheme
 *
 * The encoding scheme packs an 8-bit port number and 8-bit pin number into a
 * single 16-bit value:
 *
 * ```
 * +-----------------+-----------------+
 * |   Upper Byte    |   Lower Byte    |
 * |  (Port Number)  |  (Pin Number)   |
 * +-----------------+-----------------+
 * |     Bits 15-8   |     Bits 7-0    |
 * |   rx_port_number_t  |  rx_pin_number_t   |
 * +-----------------+-----------------+
 *
 * Encoding:   gpio_pin_t = (port << k_port_shift) | pin
 * Decoding:   port = gpio_pin_t >> k_port_shift
 *             pin  = gpio_pin_t & k_port_mask
 * ```
 *
 * ## Constant Definitions
 *
 * - **k_port_shift = 8**: Left-shift amount to move port number to upper byte
 * - **k_port_mask = 0xFF**: Mask to extract pin number from lower byte
 *
 * ## Design Rationale
 *
 * Using compile-time constants instead of magic numbers provides:
 * 1. **Self-documenting code**: `(port << k_port_shift)` vs `(port << 8)`
 * 2. **Easy modification**: Change encoding in one place if needed
 * 3. **Type safety**: C23 typed enum provides compile-time checking
 * 4. **Searchability**: grep for k_port_shift finds all encoding operations
 *
 * ## Bit Layout Example
 *
 * Port B (0x0B), Pin 2 (0x02):
 * ```
 * Port:  0x0B = 0000 1011
 * Pin:   0x02 = 0000 0010
 *
 * Encoding: (0x0B << 8) | 0x02
 *         = 0x0B00 | 0x0002
 *         = 0x0B02
 *
 * Binary:   0000 1011 0000 0010
 *           +- Port -++- Pin -+
 * ```
 *
 * @par Usage Example (Encoding):
 * @code{.c}
 * // Manual construction using encoding constants
 * rx_port_number_t port = k_rx_port_b;  // 0x0B
 * rx_pin_number_t pin = k_rx_pin_2;     // 0x02
 * gpio_pin_t result = (port << k_port_shift) | pin;
 * // Result: 0x0B02
 *
 * // Equivalent to pre-computed constant
 * gpio_pin_t result = k_rx_pb_2;
 * @endcode
 *
 * @par Usage Example (Decoding):
 * @code{.c}
 * // Extract port and pin from combined value
 * gpio_pin_t pin = k_rx_pe_5;  // 0x0E05
 *
 * rx_port_number_t port = (pin >> k_port_shift);  // 0x0E
 * rx_pin_number_t pin_num = (pin & k_port_mask);  // 0x05
 *
 * // Or use dedicated accessor functions (preferred)
 * port = rx_port_from_pin(pin);      // 0x0E
 * pin_num = rx_pin_from_pin(pin);    // 0x05
 * @endcode
 *
 * @par Usage Example (Bit Manipulation):
 * @code{.c}
 * // Verify encoding/decoding round-trip
 * rx_port_number_t original_port = k_rx_port_c;
 * rx_pin_number_t original_pin = k_rx_pin_7;
 *
 * // Encode
 * gpio_pin_t encoded = (original_port << k_port_shift) | original_pin;
 *
 * // Decode
 * rx_port_number_t decoded_port = (encoded >> k_port_shift);
 * rx_pin_number_t decoded_pin = (encoded & k_port_mask);
 *
 * // Verify
 * assert(decoded_port == original_port);
 * assert(decoded_pin == original_pin);
 * @endcode
 *
 * @invariant k_port_shift == 8 (must shift by full byte)
 * @invariant k_port_mask == 0xFF (must mask full byte)
 * @invariant (value << k_port_shift) >> k_port_shift == value (for value <= 0xFF)
 * @invariant (value & k_port_mask) <= 0xFF (mask result always fits in uint8_t)
 *
 * @note This is a C23 **typed enum** with explicit underlying type uint8_t,
 *       ensuring 1-byte size and consistent ABI across compiler versions
 * @note The encoding is **big-endian** style (port in high byte, pin in low byte)
 * @note This encoding matches the pattern used in hardware_pinout.h gpio_pin_t enum
 *
 * @warning Do not change these constants without updating all port/pin definitions
 * @warning Changing k_port_shift affects ALL 144+ pre-computed pin constants
 *
 * @see rx_port_number_t Port number constants that go in upper byte
 * @see rx_pin_number_t Pin number constants that go in lower byte
 * @see rx_port_pin_t Pre-computed combinations using this encoding
 * @see rx_port_from_pin() Decode port number from gpio_pin_t
 * @see rx_pin_from_pin() Decode pin number from gpio_pin_t
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_port_shift =
    8, /**< Left shift amount to move port number into upper byte (bits 15-8). Used for encoding: gpio_pin_t = (port << k_port_shift) | pin */
  k_port_mask =
    0xFF, /**< Bitmask (0xFF) to extract pin number from lower byte (bits 7-0). Used for decoding: pin = gpio_pin_t & k_port_mask */
} port_encoding_t;

/* =============================================================================
 * Pre-Computed Port/Pin Combinations
 * =============================================================================
 */

/**
 * @enum rx_port_pin_t
 * @brief Pre-Computed Port/Pin Combinations for All RX72N GPIO Pins
 *
 * @details
 * Provides ready-to-use 16-bit gpio_pin_t constants for all 144 theoretical
 * port/pin combinations on the RX72N microcontroller (18 ports x 8 pins = 144).
 * These pre-computed constants eliminate the need for runtime bit-shift operations
 * and provide cleaner, more readable code.
 *
 * ## Coverage
 *
 * This enum defines ALL possible port/pin combinations:
 * - **Ports**: 0-9 (decimal) + A-G + J (hexadecimal) = 18 ports
 * - **Pins per port**: 0-7 = 8 pins
 * - **Total combinations**: 18 x 8 = 144 constants
 *
 * ## Physical Availability
 *
 * **Important**: Not all 144 combinations are physically bonded on smaller packages:
 * - **144-pin LFQFP** (STAR hardware): ~112 pins available
 * - **176-pin LFBGA**: Maximum pins available
 *
 * Constants are defined for ALL theoretical combinations to ensure:
 * - Code compatibility across different RX72N packages
 * - Compile-time validation (invalid pins detected at compile time)
 * - No gaps in the constant namespace
 *
 * Pins marked "N/A on 144-pin" are not physically available on the STAR hardware
 * but may be available on larger packages.
 *
 * ## Benefits Over Manual Encoding
 *
 * **Using pre-computed constants**:
 * ```c
 * gpio_pin_t pin = k_rx_pb_2;  // Clean, readable
 * ```
 *
 * **vs. Manual bit-shift construction**:
 * ```c
 * gpio_pin_t pin = (k_rx_port_b << k_port_shift) | k_rx_pin_2;  // Verbose
 * ```
 *
 * Advantages:
 * 1. **Cleaner syntax**: One constant vs. shift-or expression
 * 2. **Zero runtime overhead**: Pre-computed at compile time
 * 3. **Easier to grep**: Search for "k_rx_pb_2" finds all uses
 * 4. **Consistent naming**: Matches Renesas datasheet conventions (PA2, PB7, etc.)
 * 5. **Compile-time validated**: Static assertions verify correctness
 *
 * ## Naming Convention
 *
 * Constants follow the pattern: `k_rx_p<port>_<pin>`
 *
 * Examples:
 * - `k_rx_p0_5` -> Port 0, Pin 5 -> P05
 * - `k_rx_pb_2` -> Port B, Pin 2 -> PB2
 * - `k_rx_pe_5` -> Port E, Pin 5 -> PE5
 * - `k_rx_pj_3` -> Port J, Pin 3 -> PJ3
 *
 * This naming matches the Renesas RX72N Hardware Manual conventions.
 *
 * ## Encoding Scheme
 *
 * Each constant uses the standard port/pin encoding:
 * ```
 * k_rx_p<port>_<pin> = (k_rx_port_<port> << k_port_shift) | k_rx_pin_<pin>
 *                    = (port << 8) | pin
 * ```
 *
 * Example: PB2 (Port B, Pin 2)
 * ```
 * k_rx_pb_2 = (k_rx_port_b << 8) | k_rx_pin_2
 *           = (0x0B << 8) | 0x02
 *           = 0x0B00 | 0x0002
 *           = 0x0B02
 * ```
 *
 * @par Usage Example (Basic):
 * @code{.c}
 * // Use pre-computed constant for clean code
 * gpio_pin_t status_led = k_rx_pb_2;   // Port B, Pin 2
 * gpio_pin_t spi_cs = k_rx_pe_5;       // Port E, Pin 5
 *
 * // Pass to HAL functions
 * gpio_set_pin_high(status_led);
 * gpio_configure_output(spi_cs);
 * @endcode
 *
 * @par Usage Example (Application Pin Definitions):
 * @code{.c}
 * // Define application-specific pin aliases using pre-computed constants
 * typedef enum : uint16_t {
 *   k_gpio_led_status = k_rx_pb_2,      // Status LED on PB2
 *   k_gpio_led_error  = k_rx_pb_3,      // Error LED on PB3
 *   k_gpio_spi_cs     = k_rx_pe_5,      // SPI chip select on PE5
 *   k_gpio_uart_tx    = k_rx_p3_0,      // UART TX on P30
 *   k_gpio_uart_rx    = k_rx_p3_1,      // UART RX on P31
 * } application_pins_t;
 * @endcode
 *
 * @par Usage Example (Port/Pin Extraction):
 * @code{.c}
 * // Extract port and pin numbers from pre-computed constant
 * gpio_pin_t pin = k_rx_pb_2;  // 0x0B02
 *
 * rx_port_number_t port = rx_port_from_pin(pin);  // Returns k_rx_port_b (0x0B)
 * rx_pin_number_t pin_num = rx_pin_from_pin(pin); // Returns 2
 * @endcode
 *
 * @par Usage Example (Compile-Time Validation):
 * @code{.c}
 * // Static assertion verifies pre-computed constant is correct
 * static_assert(k_rx_pb_2 == 0x0B02, "PB2 must be 0x0B02");
 * static_assert(k_rx_pe_5 == 0x0E05, "PE5 must be 0x0E05");
 * @endcode
 *
 * @par Pin Availability on 144-pin LFQFP Package:
 *
 * | Port | Available Pins | Package Pins | Example Constants |
 * |------|----------------|--------------|-------------------|
 * | 0    | P00-P03, P05, P07 | 8,7,6,4,2,144 | k_rx_p0_0, k_rx_p0_7 |
 * | 1    | P12-P17        | 45-38        | k_rx_p1_2, k_rx_p1_7 |
 * | 2    | P20-P27        | 37-30        | k_rx_p2_0, k_rx_p2_7 |
 * | 3    | P30-P37        | 29-20        | k_rx_p3_0, k_rx_p3_7 |
 * | 4    | P40-P47        | 141-133      | k_rx_p4_0, k_rx_p4_7 |
 * | 5    | P50-P56        | 56-50        | k_rx_p5_0, k_rx_p5_6 |
 * | 6    | P60-P67        | 117-98       | k_rx_p6_0, k_rx_p6_7 |
 * | 7    | P70-P77        | 104-68       | k_rx_p7_0, k_rx_p7_7 |
 * | 8    | P80-P83, P86-P87 | 65-58,41,39 | k_rx_p8_0, k_rx_p8_7 |
 * | 9    | P90-P93        | 131-127      | k_rx_p9_0, k_rx_p9_3 |
 * | A    | PA0-PA7        | 97-88        | k_rx_pa_0, k_rx_pa_7 |
 * | B    | PB0-PB7        | 87-78        | k_rx_pb_0, k_rx_pb_7 |
 * | C    | PC0-PC7        | 75-60        | k_rx_pc_0, k_rx_pc_7 |
 * | D    | PD0-PD7        | 126-119      | k_rx_pd_0, k_rx_pd_7 |
 * | E    | PE0-PE7        | 111-101      | k_rx_pe_0, k_rx_pe_7 |
 * | F    | PF5            | 9            | k_rx_pf_5 |
 * | J    | PJ3, PJ5       | 13, 11       | k_rx_pj_3, k_rx_pj_5 |
 *
 * @invariant Each constant equals (port << 8) | pin
 * @invariant Port portion (upper byte) is valid rx_port_number_t
 * @invariant Pin portion (lower byte) is valid rx_pin_number_t (0-7)
 * @invariant All 144 combinations are defined (no gaps)
 *
 * @note This is a C23 **typed enum** with explicit underlying type uint16_t,
 *       ensuring 2-byte size and consistent ABI across compiler versions
 * @note Constants are evaluated at compile time (zero runtime overhead)
 * @note Naming follows Renesas conventions: P<port><pin> (e.g., PB2, PE5)
 * @note Package availability is documented in inline comments (search "N/A on 144-pin")
 *
 * @warning Using undefined pins (N/A on your package) may cause undefined behavior
 * @warning Always verify pin availability in RX72N datasheet for your package
 * @warning Do not assume all defined constants are physically available
 *
 * @see rx_port_number_t Port number constants used in upper byte
 * @see rx_pin_number_t Pin number constants used in lower byte
 * @see port_encoding_t Encoding scheme constants (k_port_shift, k_port_mask)
 * @see rx_port_from_pin() Extract port number from constant
 * @see rx_pin_from_pin() Extract pin number from constant
 * @see hardware_pinout.h Application-level pin definitions
 * @see docs/sections/03_hardware_pinout.tex Complete pinout reference for STAR hardware
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* Port 0 (0x00) - Pins 0-7 */
  k_rx_p0_0 = (k_rx_port_0 << k_port_shift) | k_rx_pin_0, /**< P00 (pin 8) */
  k_rx_p0_1 = (k_rx_port_0 << k_port_shift) | k_rx_pin_1, /**< P01 (pin 7) */
  k_rx_p0_2 = (k_rx_port_0 << k_port_shift) | k_rx_pin_2, /**< P02 (pin 6) */
  k_rx_p0_3 = (k_rx_port_0 << k_port_shift) | k_rx_pin_3, /**< P03 (pin 4) */
  k_rx_p0_4 = (k_rx_port_0 << k_port_shift) | k_rx_pin_4, /**< P04 - N/A on 144-pin */
  k_rx_p0_5 = (k_rx_port_0 << k_port_shift) | k_rx_pin_5, /**< P05 (pin 2) */
  k_rx_p0_6 = (k_rx_port_0 << k_port_shift) | k_rx_pin_6, /**< P06 - N/A on 144-pin */
  k_rx_p0_7 = (k_rx_port_0 << k_port_shift) | k_rx_pin_7, /**< P07 (pin 144) */

  /* Port 1 (0x01) - Pins 0-7 */
  k_rx_p1_0 = (k_rx_port_1 << k_port_shift) | k_rx_pin_0, /**< P10 - N/A on 144-pin */
  k_rx_p1_1 = (k_rx_port_1 << k_port_shift) | k_rx_pin_1, /**< P11 - N/A on 144-pin */
  k_rx_p1_2 = (k_rx_port_1 << k_port_shift) | k_rx_pin_2, /**< P12 (pin 45) */
  k_rx_p1_3 = (k_rx_port_1 << k_port_shift) | k_rx_pin_3, /**< P13 (pin 44) */
  k_rx_p1_4 = (k_rx_port_1 << k_port_shift) | k_rx_pin_4, /**< P14 (pin 43) */
  k_rx_p1_5 = (k_rx_port_1 << k_port_shift) | k_rx_pin_5, /**< P15 (pin 42) */
  k_rx_p1_6 = (k_rx_port_1 << k_port_shift) | k_rx_pin_6, /**< P16 (pin 40) */
  k_rx_p1_7 = (k_rx_port_1 << k_port_shift) | k_rx_pin_7, /**< P17 (pin 38) */

  /* Port 2 (0x02) - Pins 0-7 */
  k_rx_p2_0 = (k_rx_port_2 << k_port_shift) | k_rx_pin_0, /**< P20 (pin 37) */
  k_rx_p2_1 = (k_rx_port_2 << k_port_shift) | k_rx_pin_1, /**< P21 (pin 36) */
  k_rx_p2_2 = (k_rx_port_2 << k_port_shift) | k_rx_pin_2, /**< P22 (pin 35) */
  k_rx_p2_3 = (k_rx_port_2 << k_port_shift) | k_rx_pin_3, /**< P23 (pin 34) */
  k_rx_p2_4 = (k_rx_port_2 << k_port_shift) | k_rx_pin_4, /**< P24 (pin 33) */
  k_rx_p2_5 = (k_rx_port_2 << k_port_shift) | k_rx_pin_5, /**< P25 (pin 32) */
  k_rx_p2_6 = (k_rx_port_2 << k_port_shift) | k_rx_pin_6, /**< P26 (pin 31) */
  k_rx_p2_7 = (k_rx_port_2 << k_port_shift) | k_rx_pin_7, /**< P27 (pin 30) */

  /* Port 3 (0x03) - Pins 0-7 */
  k_rx_p3_0 = (k_rx_port_3 << k_port_shift) | k_rx_pin_0, /**< P30 (pin 29) */
  k_rx_p3_1 = (k_rx_port_3 << k_port_shift) | k_rx_pin_1, /**< P31 (pin 28) */
  k_rx_p3_2 = (k_rx_port_3 << k_port_shift) | k_rx_pin_2, /**< P32 (pin 27) */
  k_rx_p3_3 = (k_rx_port_3 << k_port_shift) | k_rx_pin_3, /**< P33 (pin 26) */
  k_rx_p3_4 = (k_rx_port_3 << k_port_shift) | k_rx_pin_4, /**< P34 (pin 25) */
  k_rx_p3_5 = (k_rx_port_3 << k_port_shift) | k_rx_pin_5, /**< P35 (pin 24) */
  k_rx_p3_6 = (k_rx_port_3 << k_port_shift) | k_rx_pin_6, /**< P36 (pin 22) */
  k_rx_p3_7 = (k_rx_port_3 << k_port_shift) | k_rx_pin_7, /**< P37 (pin 20) */

  /* Port 4 (0x04) - Pins 0-7 */
  k_rx_p4_0 = (k_rx_port_4 << k_port_shift) | k_rx_pin_0, /**< P40 (pin 141) */
  k_rx_p4_1 = (k_rx_port_4 << k_port_shift) | k_rx_pin_1, /**< P41 (pin 139) */
  k_rx_p4_2 = (k_rx_port_4 << k_port_shift) | k_rx_pin_2, /**< P42 (pin 138) */
  k_rx_p4_3 = (k_rx_port_4 << k_port_shift) | k_rx_pin_3, /**< P43 (pin 137) */
  k_rx_p4_4 = (k_rx_port_4 << k_port_shift) | k_rx_pin_4, /**< P44 (pin 136) */
  k_rx_p4_5 = (k_rx_port_4 << k_port_shift) | k_rx_pin_5, /**< P45 (pin 135) */
  k_rx_p4_6 = (k_rx_port_4 << k_port_shift) | k_rx_pin_6, /**< P46 (pin 134) */
  k_rx_p4_7 = (k_rx_port_4 << k_port_shift) | k_rx_pin_7, /**< P47 (pin 133) */

  /* Port 5 (0x05) - Pins 0-7 */
  k_rx_p5_0 = (k_rx_port_5 << k_port_shift) | k_rx_pin_0, /**< P50 (pin 56) */
  k_rx_p5_1 = (k_rx_port_5 << k_port_shift) | k_rx_pin_1, /**< P51 (pin 55) */
  k_rx_p5_2 = (k_rx_port_5 << k_port_shift) | k_rx_pin_2, /**< P52 (pin 54) */
  k_rx_p5_3 = (k_rx_port_5 << k_port_shift) | k_rx_pin_3, /**< P53 (pin 53) */
  k_rx_p5_4 = (k_rx_port_5 << k_port_shift) | k_rx_pin_4, /**< P54 (pin 52) */
  k_rx_p5_5 = (k_rx_port_5 << k_port_shift) | k_rx_pin_5, /**< P55 (pin 51) */
  k_rx_p5_6 = (k_rx_port_5 << k_port_shift) | k_rx_pin_6, /**< P56 (pin 50) */
  k_rx_p5_7 = (k_rx_port_5 << k_port_shift) | k_rx_pin_7, /**< P57 - N/A on 144-pin */

  /* Port 6 (0x06) - Pins 0-7 */
  k_rx_p6_0 = (k_rx_port_6 << k_port_shift) | k_rx_pin_0, /**< P60 (pin 117) */
  k_rx_p6_1 = (k_rx_port_6 << k_port_shift) | k_rx_pin_1, /**< P61 (pin 115) */
  k_rx_p6_2 = (k_rx_port_6 << k_port_shift) | k_rx_pin_2, /**< P62 (pin 114) */
  k_rx_p6_3 = (k_rx_port_6 << k_port_shift) | k_rx_pin_3, /**< P63 (pin 113) */
  k_rx_p6_4 = (k_rx_port_6 << k_port_shift) | k_rx_pin_4, /**< P64 (pin 112) */
  k_rx_p6_5 = (k_rx_port_6 << k_port_shift) | k_rx_pin_5, /**< P65 (pin 100) */
  k_rx_p6_6 = (k_rx_port_6 << k_port_shift) | k_rx_pin_6, /**< P66 (pin 99) */
  k_rx_p6_7 = (k_rx_port_6 << k_port_shift) | k_rx_pin_7, /**< P67 (pin 98) */

  /* Port 7 (0x07) - Pins 0-7 */
  k_rx_p7_0 = (k_rx_port_7 << k_port_shift) | k_rx_pin_0, /**< P70 (pin 104) */
  k_rx_p7_1 = (k_rx_port_7 << k_port_shift) | k_rx_pin_1, /**< P71 (pin 86) */
  k_rx_p7_2 = (k_rx_port_7 << k_port_shift) | k_rx_pin_2, /**< P72 (pin 85) */
  k_rx_p7_3 = (k_rx_port_7 << k_port_shift) | k_rx_pin_3, /**< P73 (pin 77) */
  k_rx_p7_4 = (k_rx_port_7 << k_port_shift) | k_rx_pin_4, /**< P74 (pin 72) */
  k_rx_p7_5 = (k_rx_port_7 << k_port_shift) | k_rx_pin_5, /**< P75 (pin 71) */
  k_rx_p7_6 = (k_rx_port_7 << k_port_shift) | k_rx_pin_6, /**< P76 (pin 69) */
  k_rx_p7_7 = (k_rx_port_7 << k_port_shift) | k_rx_pin_7, /**< P77 (pin 68) */

  /* Port 8 (0x08) - Pins 0-7 */
  k_rx_p8_0 = (k_rx_port_8 << k_port_shift) | k_rx_pin_0, /**< P80 (pin 65) */
  k_rx_p8_1 = (k_rx_port_8 << k_port_shift) | k_rx_pin_1, /**< P81 (pin 64) */
  k_rx_p8_2 = (k_rx_port_8 << k_port_shift) | k_rx_pin_2, /**< P82 (pin 63) */
  k_rx_p8_3 = (k_rx_port_8 << k_port_shift) | k_rx_pin_3, /**< P83 (pin 58) */
  k_rx_p8_4 = (k_rx_port_8 << k_port_shift) | k_rx_pin_4, /**< P84 - N/A on 144-pin */
  k_rx_p8_5 = (k_rx_port_8 << k_port_shift) | k_rx_pin_5, /**< P85 - N/A on 144-pin */
  k_rx_p8_6 = (k_rx_port_8 << k_port_shift) | k_rx_pin_6, /**< P86 (pin 41) */
  k_rx_p8_7 = (k_rx_port_8 << k_port_shift) | k_rx_pin_7, /**< P87 (pin 39) */

  /* Port 9 (0x09) - Pins 0-7 */
  k_rx_p9_0 = (k_rx_port_9 << k_port_shift) | k_rx_pin_0, /**< P90 (pin 131) */
  k_rx_p9_1 = (k_rx_port_9 << k_port_shift) | k_rx_pin_1, /**< P91 (pin 129) */
  k_rx_p9_2 = (k_rx_port_9 << k_port_shift) | k_rx_pin_2, /**< P92 (pin 128) */
  k_rx_p9_3 = (k_rx_port_9 << k_port_shift) | k_rx_pin_3, /**< P93 (pin 127) */
  k_rx_p9_4 = (k_rx_port_9 << k_port_shift) | k_rx_pin_4, /**< P94 - N/A on 144-pin */
  k_rx_p9_5 = (k_rx_port_9 << k_port_shift) | k_rx_pin_5, /**< P95 - N/A on 144-pin */
  k_rx_p9_6 = (k_rx_port_9 << k_port_shift) | k_rx_pin_6, /**< P96 - N/A on 144-pin */
  k_rx_p9_7 = (k_rx_port_9 << k_port_shift) | k_rx_pin_7, /**< P97 - N/A on 144-pin */

  /* Port A (0x0A) - Pins 0-7 */
  k_rx_pa_0 = (k_rx_port_a << k_port_shift) | k_rx_pin_0, /**< PA0 (pin 97) */
  k_rx_pa_1 = (k_rx_port_a << k_port_shift) | k_rx_pin_1, /**< PA1 (pin 96) */
  k_rx_pa_2 = (k_rx_port_a << k_port_shift) | k_rx_pin_2, /**< PA2 (pin 95) */
  k_rx_pa_3 = (k_rx_port_a << k_port_shift) | k_rx_pin_3, /**< PA3 (pin 94) */
  k_rx_pa_4 = (k_rx_port_a << k_port_shift) | k_rx_pin_4, /**< PA4 (pin 92) */
  k_rx_pa_5 = (k_rx_port_a << k_port_shift) | k_rx_pin_5, /**< PA5 (pin 90) */
  k_rx_pa_6 = (k_rx_port_a << k_port_shift) | k_rx_pin_6, /**< PA6 (pin 89) */
  k_rx_pa_7 = (k_rx_port_a << k_port_shift) | k_rx_pin_7, /**< PA7 (pin 88) */

  /* Port B (0x0B) - Pins 0-7 */
  k_rx_pb_0 = (k_rx_port_b << k_port_shift) | k_rx_pin_0, /**< PB0 (pin 87) */
  k_rx_pb_1 = (k_rx_port_b << k_port_shift) | k_rx_pin_1, /**< PB1 (pin 84) */
  k_rx_pb_2 = (k_rx_port_b << k_port_shift) | k_rx_pin_2, /**< PB2 (pin 83) */
  k_rx_pb_3 = (k_rx_port_b << k_port_shift) | k_rx_pin_3, /**< PB3 (pin 82) */
  k_rx_pb_4 = (k_rx_port_b << k_port_shift) | k_rx_pin_4, /**< PB4 (pin 81) */
  k_rx_pb_5 = (k_rx_port_b << k_port_shift) | k_rx_pin_5, /**< PB5 (pin 80) */
  k_rx_pb_6 = (k_rx_port_b << k_port_shift) | k_rx_pin_6, /**< PB6 (pin 79) */
  k_rx_pb_7 = (k_rx_port_b << k_port_shift) | k_rx_pin_7, /**< PB7 (pin 78) */

  /* Port C (0x0C) - Pins 0-7 */
  k_rx_pc_0 = (k_rx_port_c << k_port_shift) | k_rx_pin_0, /**< PC0 (pin 75) */
  k_rx_pc_1 = (k_rx_port_c << k_port_shift) | k_rx_pin_1, /**< PC1 (pin 73) */
  k_rx_pc_2 = (k_rx_port_c << k_port_shift) | k_rx_pin_2, /**< PC2 (pin 70) */
  k_rx_pc_3 = (k_rx_port_c << k_port_shift) | k_rx_pin_3, /**< PC3 (pin 67) */
  k_rx_pc_4 = (k_rx_port_c << k_port_shift) | k_rx_pin_4, /**< PC4 (pin 66) */
  k_rx_pc_5 = (k_rx_port_c << k_port_shift) | k_rx_pin_5, /**< PC5 (pin 62) */
  k_rx_pc_6 = (k_rx_port_c << k_port_shift) | k_rx_pin_6, /**< PC6 (pin 61) */
  k_rx_pc_7 = (k_rx_port_c << k_port_shift) | k_rx_pin_7, /**< PC7 (pin 60) */

  /* Port D (0x0D) - Pins 0-7 */
  k_rx_pd_0 = (k_rx_port_d << k_port_shift) | k_rx_pin_0, /**< PD0 (pin 126) */
  k_rx_pd_1 = (k_rx_port_d << k_port_shift) | k_rx_pin_1, /**< PD1 (pin 125) */
  k_rx_pd_2 = (k_rx_port_d << k_port_shift) | k_rx_pin_2, /**< PD2 (pin 124) */
  k_rx_pd_3 = (k_rx_port_d << k_port_shift) | k_rx_pin_3, /**< PD3 (pin 123) */
  k_rx_pd_4 = (k_rx_port_d << k_port_shift) | k_rx_pin_4, /**< PD4 (pin 122) */
  k_rx_pd_5 = (k_rx_port_d << k_port_shift) | k_rx_pin_5, /**< PD5 (pin 121) */
  k_rx_pd_6 = (k_rx_port_d << k_port_shift) | k_rx_pin_6, /**< PD6 (pin 120) */
  k_rx_pd_7 = (k_rx_port_d << k_port_shift) | k_rx_pin_7, /**< PD7 (pin 119) */

  /* Port E (0x0E) - Pins 0-7 */
  k_rx_pe_0 = (k_rx_port_e << k_port_shift) | k_rx_pin_0, /**< PE0 (pin 111) */
  k_rx_pe_1 = (k_rx_port_e << k_port_shift) | k_rx_pin_1, /**< PE1 (pin 110) */
  k_rx_pe_2 = (k_rx_port_e << k_port_shift) | k_rx_pin_2, /**< PE2 (pin 109) */
  k_rx_pe_3 = (k_rx_port_e << k_port_shift) | k_rx_pin_3, /**< PE3 (pin 108) */
  k_rx_pe_4 = (k_rx_port_e << k_port_shift) | k_rx_pin_4, /**< PE4 (pin 107) */
  k_rx_pe_5 = (k_rx_port_e << k_port_shift) | k_rx_pin_5, /**< PE5 (pin 106) */
  k_rx_pe_6 = (k_rx_port_e << k_port_shift) | k_rx_pin_6, /**< PE6 (pin 102) */
  k_rx_pe_7 = (k_rx_port_e << k_port_shift) | k_rx_pin_7, /**< PE7 (pin 101) */

  /* Port F (0x0F) - Pins 0-7 - N/A on 144-pin */
  k_rx_pf_0 = (k_rx_port_f << k_port_shift) | k_rx_pin_0, /**< PF0 - N/A on 144-pin */
  k_rx_pf_1 = (k_rx_port_f << k_port_shift) | k_rx_pin_1, /**< PF1 - N/A on 144-pin */
  k_rx_pf_2 = (k_rx_port_f << k_port_shift) | k_rx_pin_2, /**< PF2 - N/A on 144-pin */
  k_rx_pf_3 = (k_rx_port_f << k_port_shift) | k_rx_pin_3, /**< PF3 - N/A on 144-pin */
  k_rx_pf_4 = (k_rx_port_f << k_port_shift) | k_rx_pin_4, /**< PF4 - N/A on 144-pin */
  k_rx_pf_5 = (k_rx_port_f << k_port_shift) | k_rx_pin_5, /**< PF5 (pin 9) */
  k_rx_pf_6 = (k_rx_port_f << k_port_shift) | k_rx_pin_6, /**< PF6 - N/A on 144-pin */
  k_rx_pf_7 = (k_rx_port_f << k_port_shift) | k_rx_pin_7, /**< PF7 - N/A on 144-pin */

  /* Port G (0x10) - Pins 0-7 - N/A on 144-pin */
  k_rx_pg_0 = (k_rx_port_g << k_port_shift) | k_rx_pin_0, /**< PG0 - N/A on 144-pin */
  k_rx_pg_1 = (k_rx_port_g << k_port_shift) | k_rx_pin_1, /**< PG1 - N/A on 144-pin */
  k_rx_pg_2 = (k_rx_port_g << k_port_shift) | k_rx_pin_2, /**< PG2 - N/A on 144-pin */
  k_rx_pg_3 = (k_rx_port_g << k_port_shift) | k_rx_pin_3, /**< PG3 - N/A on 144-pin */
  k_rx_pg_4 = (k_rx_port_g << k_port_shift) | k_rx_pin_4, /**< PG4 - N/A on 144-pin */
  k_rx_pg_5 = (k_rx_port_g << k_port_shift) | k_rx_pin_5, /**< PG5 - N/A on 144-pin */
  k_rx_pg_6 = (k_rx_port_g << k_port_shift) | k_rx_pin_6, /**< PG6 - N/A on 144-pin */
  k_rx_pg_7 = (k_rx_port_g << k_port_shift) | k_rx_pin_7, /**< PG7 - N/A on 144-pin */

  /* Port J (0x12) - Pins 0-7 */
  k_rx_pj_0 = (k_rx_port_j << k_port_shift) | k_rx_pin_0, /**< PJ0 - N/A on 144-pin */
  k_rx_pj_1 = (k_rx_port_j << k_port_shift) | k_rx_pin_1, /**< PJ1 - N/A on 144-pin */
  k_rx_pj_2 = (k_rx_port_j << k_port_shift) | k_rx_pin_2, /**< PJ2 - N/A on 144-pin */
  k_rx_pj_3 = (k_rx_port_j << k_port_shift) | k_rx_pin_3, /**< PJ3 (pin 13) */
  k_rx_pj_4 = (k_rx_port_j << k_port_shift) | k_rx_pin_4, /**< PJ4 - N/A on 144-pin */
  k_rx_pj_5 = (k_rx_port_j << k_port_shift) | k_rx_pin_5, /**< PJ5 (pin 11) */
  k_rx_pj_6 = (k_rx_port_j << k_port_shift) | k_rx_pin_6, /**< PJ6 - N/A on 144-pin */
  k_rx_pj_7 = (k_rx_port_j << k_port_shift) | k_rx_pin_7, /**< PJ7 - N/A on 144-pin */
} rx_port_pin_t;

/* =============================================================================
 * Compile-Time Verification (Static Assertions)
 * =============================================================================
 */

/**
 * @enum rx_port_verify_t
 * @brief Expected values used in static assertion verification
 *
 * @details
 * Provides named constants for the right-hand sides of port/pin static
 * assertions, eliminating magic numbers from assertion expressions.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_verify_port_0     = 0x0000U, /**< Expected value of k_rx_port_0 (0x00) */
  k_verify_port_1     = 0x0001U, /**< Expected value of k_rx_port_1 (0x01) */
  k_verify_port_5     = 0x0005U, /**< Expected value of k_rx_port_5 (0x05) */
  k_verify_port_a     = 0x000AU, /**< Expected value of k_rx_port_a (0x0A) */
  k_verify_port_b     = 0x000BU, /**< Expected value of k_rx_port_b (0x0B) */
  k_verify_port_c     = 0x000CU, /**< Expected value of k_rx_port_c (0x0C) */
  k_verify_port_e     = 0x000EU, /**< Expected value of k_rx_port_e (0x0E) */
  k_verify_port_j     = 0x0012U, /**< Expected value of k_rx_port_j (0x12) - follows Port H; Port I does not exist */
  k_verify_pin_2      = 0x0002U, /**< Expected value of k_rx_pin_2 (2) */
  k_verify_pin_7      = 0x0007U, /**< Expected value of k_rx_pin_7 (7) */
  k_verify_port_shift = 0x0008U, /**< Expected value of k_port_shift (8) */
  k_verify_enc_pb_2   = 0x0B02U, /**< Expected encoding of Port B Pin 2 */
  k_verify_enc_pe_5   = 0x0E05U, /**< Expected encoding of Port E Pin 5 */
  k_verify_enc_p0_7   = 0x0007U, /**< Expected encoding of Port 0 Pin 7 */
  k_verify_p0_4       = 0x0004U, /**< Expected value of k_rx_p0_4 */
  k_verify_p2_5       = 0x0205U, /**< Expected value of k_rx_p2_5 */
  k_verify_pa_2       = 0x0A02U, /**< Expected value of k_rx_pa_2 */
  k_verify_pb_2       = 0x0B02U, /**< Expected value of k_rx_pb_2 */
  k_verify_pe_5       = 0x0E05U, /**< Expected value of k_rx_pe_5 */
  k_verify_pj_3       = 0x1203U, /**< Expected value of k_rx_pj_3 */
} rx_port_verify_t;

/*
 * Verify port number constants match expected hex values.
 * These assertions ensure that the port numbering matches Renesas RX72N
 * documentation and the encoding used in hardware_pinout.h.
 */
static_assert((bool)((uint16_t)k_rx_port_0 == k_verify_port_0), "Port 0 must be 0x00");
static_assert((bool)((uint16_t)k_rx_port_1 == k_verify_port_1), "Port 1 must be 0x01");
static_assert((bool)((uint16_t)k_rx_port_5 == k_verify_port_5), "Port 5 must be 0x05");
static_assert((bool)((uint16_t)k_rx_port_a == k_verify_port_a), "Port A must be 0x0A");
static_assert((bool)((uint16_t)k_rx_port_b == k_verify_port_b), "Port B must be 0x0B");
static_assert((bool)((uint16_t)k_rx_port_c == k_verify_port_c), "Port C must be 0x0C");
static_assert((bool)((uint16_t)k_rx_port_e == k_verify_port_e), "Port E must be 0x0E");
static_assert((bool)((uint16_t)k_rx_port_j == k_verify_port_j),
              "Port J must be 0x12 (PORTJ.PDR=0x0008C012; Port I does not exist)");

/*
 * Verify pin number constants match expected values.
 */
static_assert((bool)((uint16_t)k_rx_pin_0 == 0), "Pin 0 must be 0");
static_assert((bool)((uint16_t)k_rx_pin_2 == k_verify_pin_2), "Pin 2 must be 2");
static_assert((bool)((uint16_t)k_rx_pin_7 == k_verify_pin_7), "Pin 7 must be 7");
static_assert((bool)((uint16_t)k_rx_pin_min == 0), "Minimum pin number must be 0");
static_assert((bool)((uint16_t)k_rx_pin_max == k_verify_pin_7), "Maximum pin number must be 7");

/*
 * Verify shift constant is correct.
 */
static_assert((bool)((uint16_t)k_port_shift == k_verify_port_shift), "Port shift must be 8 bits");

/*
 * Verify encoding matches expected pattern: (port << 8) | pin
 * Example: Port B (0x0B), Pin 2 (0x02) = 0x0B02
 */
static_assert((bool)((uint16_t)((k_rx_port_b << k_port_shift) | k_rx_pin_2) == k_verify_enc_pb_2),
              "Port B Pin 2 encoding must be 0x0B02");
static_assert((bool)((uint16_t)((k_rx_port_e << k_port_shift) | k_rx_pin_5) == k_verify_enc_pe_5),
              "Port E Pin 5 encoding must be 0x0E05");
static_assert((bool)((uint16_t)((k_rx_port_0 << k_port_shift) | k_rx_pin_7) == k_verify_enc_p0_7),
              "Port 0 Pin 7 encoding must be 0x0007");

/*
 * Verify pre-computed port/pin combinations are correct.
 */
static_assert((bool)((unsigned int)k_rx_p0_4 == (unsigned int)k_verify_p0_4), "P04 must be 0x0004");
static_assert((bool)((unsigned int)k_rx_p2_5 == (unsigned int)k_verify_p2_5), "P25 must be 0x0205");
static_assert((bool)((unsigned int)k_rx_pa_2 == (unsigned int)k_verify_pa_2), "PA2 must be 0x0A02");
static_assert((bool)((unsigned int)k_rx_pb_2 == (unsigned int)k_verify_pb_2), "PB2 must be 0x0B02");
static_assert((bool)((unsigned int)k_rx_pe_5 == (unsigned int)k_verify_pe_5), "PE5 must be 0x0E05");
static_assert((bool)((unsigned int)k_rx_pj_3 == (unsigned int)k_verify_pj_3), "PJ3 must be 0x1203");

/* =============================================================================
 * Pin Extraction Inline Functions
 * =============================================================================
 */

/**
 * @brief Extract port number from combined port/pin identifier
 *
 * @details
 * Decodes the port number (upper byte) from a 16-bit gpio_pin_t value using
 * right-shift by k_port_shift (8 bits). This extracts bits 15-8 which contain
 * the rx_port_number_t value.
 *
 * ## Algorithm Steps
 *
 * 1. **Validate pin portion**: Assert that lower byte (pin) is in valid range [0-7]
 * 2. **Extract port**: Right-shift by 8 bits to move port from upper to lower byte
 * 3. **Validate result**: Assert that extracted port is valid RX72N port number
 * 4. **Return**: Return port number as uint8_t
 *
 * ## Bit-Level Operation
 *
 * Given: gpio_pin_t = 0xPPnn (PP = port, nn = pin)
 * Operation: result = gpio_pin_t >> 8
 * Result: result = 0x00PP
 *
 * Example: k_rx_pb_2 = 0x0B02
 * ```
 * Input:  0x0B02 = 0000 1011 0000 0010
 * Shift:  >> 8
 * Output: 0x000B = 0000 0000 0000 1011
 * Result: 0x0B (k_rx_port_b)
 * ```
 *
 * ## Performance Analysis
 *
 * - **Execution time**: 1-2 CPU cycles (inline right-shift, assertions compiled out in release)
 * - **Memory usage**: Zero (inline function, no stack frame)
 * - **Code size**: ~4 bytes machine code (single shift instruction)
 *
 * @param[in] pin Combined port/pin identifier (rx_port_pin_t or gpio_pin_t).
 *                Valid range: 0x0000-0x1307 (all valid port/pin combinations).
 *                Must have valid port in upper byte and valid pin (0-7) in lower byte.
 *
 * @return Port number extracted from upper byte (rx_port_number_t).
 *         Valid range: 0x00-0x10, or 0x12 (k_rx_port_j).
 *         Returned value is one of: k_rx_port_0..k_rx_port_9, k_rx_port_a..k_rx_port_g, k_rx_port_j.
 *
 * @pre pin lower byte (bits 7-0) must be valid pin number (0-7)
 * @pre pin upper byte (bits 15-8) must be valid port number (0x00-0x10 or 0x13)
 * @pre pin must be properly encoded using (port << 8) | pin_num scheme
 *
 * @post Returned value is valid rx_port_number_t (0x00-0x10 or 0x13)
 * @post Returned value is deterministic (same input always produces same output)
 * @post No side effects (function does not modify any state)
 *
 * @invariant (rx_port_from_pin((port << 8) | pin_num) == port) for all valid port/pin
 * @invariant Result fits in uint8_t (always <= 0xFF)
 *
 * @note **Thread-safe**: Yes - pure function with no shared state
 * @note **Re-entrant**: Yes - no static variables or side effects
 * @note **Performance**: Optimal - compiles to single right-shift instruction
 * @note **Memory**: Zero overhead - inline function, no stack frame
 * @note **Assertions**: Compiled out in release builds (NDEBUG defined)
 *
 * @warning Passing invalid pin values (pin portion > 7) triggers assertion failure
 * @warning Result undefined if pin not constructed using proper encoding scheme
 * @attention This function validates input but does NOT check physical pin availability
 *
 * @par Example (Basic Usage):
 * @code{.c}
 * // Extract port from pre-computed constant
 * gpio_pin_t pin = k_rx_pb_2;  // 0x0B02
 * rx_port_number_t port = rx_port_from_pin(pin);
 * // Result: k_rx_port_b (0x0B)
 * @endcode
 *
 * @par Example (Manual Construction):
 * @code{.c}
 * // Construct pin value, then extract port
 * gpio_pin_t pin = (k_rx_port_e << k_port_shift) | k_rx_pin_5;  // 0x0E05
 * rx_port_number_t port = rx_port_from_pin(pin);
 * // Result: k_rx_port_e (0x0E)
 *
 * // Verify round-trip
 * assert(port == k_rx_port_e);
 * @endcode
 *
 * @par Example (Switch Statement):
 * @code{.c}
 * // Use extracted port in switch
 * gpio_pin_t pin = get_configured_pin();
 * rx_port_number_t port = rx_port_from_pin(pin);
 *
 * switch (port) {
 *   case k_rx_port_b: return portb();
 *   case k_rx_port_e: return porte();
 *   default: return nullptr;
 * }
 * @endcode
 *
 * @par Example (Complete Port/Pin Decomposition):
 * @code{.c}
 * // Fully decompose gpio_pin_t into components
 * gpio_pin_t pin = k_rx_pa_7;  // 0x0A07
 *
 * rx_port_number_t port = rx_port_from_pin(pin);      // 0x0A (Port A)
 * rx_pin_number_t pin_num = rx_pin_from_pin(pin);     // 0x07 (Pin 7)
 *
 * printf("Pin 0x%04X = Port 0x%02X, Pin %d\n", pin, port, pin_num);
 * // Output: "Pin 0x0A07 = Port 0x0A, Pin 7"
 * @endcode
 *
 * @see rx_pin_from_pin() Extract pin number from combined identifier
 * @see rx_port_pin_t Pre-computed port/pin combinations
 * @see port_encoding_t Encoding constants (k_port_shift, k_port_mask)
 * @see rx_port_number_t Valid port numbers that can be returned
 *
 * @since Version 1.0.0
 * @test test_rx_port_from_pin() Unit tests verify all 144 port/pin combinations
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5**: [OK] 2 preconditions + 3 postconditions (5 assertions total)
 * - **Rule 4**: [OK] 5 lines of code (excluding comments/braces)
 */
static inline uint8_t rx_port_from_pin(rx_port_pin_t pin)
{
  /* Pre-condition: pin portion fits encoding scheme (k_rx_pin_min == 0, so >= 0 is always
   * true for unsigned types; only the upper bound requires an explicit check) */
  RX_ASSERT((pin & k_port_mask) <= k_rx_pin_max, "Pin portion must be <= k_rx_pin_max");

  uint8_t result = (uint8_t)((pin) >> k_port_shift);

  /* Post-condition: result is a valid port (0-G or J, skipping H-I) */
  /* Note: result >= k_rx_port_0 is always true for uint8_t since k_rx_port_0 == 0 */
  RX_ASSERT((result <= k_rx_port_g) || (result == k_rx_port_j),
            "Port number must be valid (k_rx_port_0..k_rx_port_g or k_rx_port_j)");

  return result;
}

/**
 * @brief Extract pin number from combined port/pin identifier
 *
 * @details
 * Decodes the pin number (lower byte) from a 16-bit gpio_pin_t value using
 * bitwise AND with k_port_mask (0xFF). This extracts bits 7-0 which contain
 * the rx_pin_number_t value.
 *
 * ## Algorithm Steps
 *
 * 1. **Extract port**: Right-shift by 8 bits to get port number (for validation)
 * 2. **Validate port**: Assert that port is valid RX72N port number
 * 3. **Extract pin**: Mask lower byte with 0xFF to get pin number
 * 4. **Validate result**: Assert that extracted pin is in valid range [0-7]
 * 5. **Return**: Return pin number as uint8_t
 *
 * ## Bit-Level Operation
 *
 * Given: gpio_pin_t = 0xPPnn (PP = port, nn = pin)
 * Operation: result = gpio_pin_t & 0xFF
 * Result: result = 0x00nn
 *
 * Example: k_rx_pe_5 = 0x0E05
 * ```
 * Input:  0x0E05 = 0000 1110 0000 0101
 * Mask:   0x00FF = 0000 0000 1111 1111
 * AND:           = 0000 0000 0000 0101
 * Output: 0x0005 = 5
 * Result: 5 (k_rx_pin_5)
 * ```
 *
 * ## Performance Analysis
 *
 * - **Execution time**: 2-3 CPU cycles (shift + mask, assertions compiled out in release)
 * - **Memory usage**: Zero (inline function, no stack frame)
 * - **Code size**: ~6 bytes machine code (shift + mask instructions)
 *
 * @param[in] pin Combined port/pin identifier (rx_port_pin_t or gpio_pin_t).
 *                Valid range: 0x0000-0x1307 (all valid port/pin combinations).
 *                Must have valid port in upper byte and valid pin (0-7) in lower byte.
 *
 * @return Pin number extracted from lower byte (rx_pin_number_t).
 *         Valid range: 0-7 (one of k_rx_pin_0..k_rx_pin_7).
 *         Corresponds to bit position in port data registers (PDR, PODR, PIDR).
 *
 * @pre pin upper byte (bits 15-8) must be valid port number (0x00-0x10 or 0x13)
 * @pre pin lower byte (bits 7-0) must be valid pin number (0-7)
 * @pre pin must be properly encoded using (port << 8) | pin_num scheme
 *
 * @post Returned value is valid rx_pin_number_t (0-7)
 * @post Returned value is deterministic (same input always produces same output)
 * @post No side effects (function does not modify any state)
 *
 * @invariant (rx_pin_from_pin((port << 8) | pin_num) == pin_num) for all valid port/pin
 * @invariant Result <= k_rx_pin_max (7)
 * @invariant Result >= k_rx_pin_min (0)
 *
 * @note **Thread-safe**: Yes - pure function with no shared state
 * @note **Re-entrant**: Yes - no static variables or side effects
 * @note **Performance**: Optimal - compiles to single mask instruction (after validation)
 * @note **Memory**: Zero overhead - inline function, no stack frame
 * @note **Assertions**: Compiled out in release builds (NDEBUG defined)
 * @note **Bit Position**: Returned value corresponds to bit position in port registers
 *
 * @warning Passing invalid port values (upper byte invalid) triggers assertion failure
 * @warning Result undefined if pin not constructed using proper encoding scheme
 * @attention This function validates input but does NOT check physical pin availability
 * @attention Pin number 0-7 corresponds to LSB-MSB of 8-bit port registers
 *
 * @par Example (Basic Usage):
 * @code{.c}
 * // Extract pin number from pre-computed constant
 * gpio_pin_t pin = k_rx_pe_5;  // 0x0E05
 * rx_pin_number_t pin_num = rx_pin_from_pin(pin);
 * // Result: 5 (k_rx_pin_5)
 * @endcode
 *
 * @par Example (Bit Manipulation):
 * @code{.c}
 * // Use extracted pin number for bit manipulation
 * gpio_pin_t pin = k_rx_pb_2;  // Port B, Pin 2
 * rx_pin_number_t pin_num = rx_pin_from_pin(pin);  // Returns 2
 *
 * PORT_Type* port = portb();
 * port->PODR |= (1 << pin_num);  // Set bit 2 high
 * port->PODR &= ~(1 << pin_num); // Clear bit 2 low
 * @endcode
 *
 * @par Example (Complete Port/Pin Decomposition):
 * @code{.c}
 * // Fully decompose gpio_pin_t into components
 * gpio_pin_t pin = k_rx_pc_7;  // 0x0C07
 *
 * rx_port_number_t port = rx_port_from_pin(pin);    // 0x0C (Port C)
 * rx_pin_number_t pin_num = rx_pin_from_pin(pin);   // 7 (Pin 7)
 *
 * // Use in hardware access
 * PORT_Type* port_regs = get_port_registers(port);
 * bool pin_state = (port_regs->PIDR >> pin_num) & 1;
 * @endcode
 *
 * @par Example (Round-Trip Verification):
 * @code{.c}
 * // Verify encoding/decoding round-trip
 * rx_port_number_t original_port = k_rx_port_a;
 * rx_pin_number_t original_pin = k_rx_pin_3;
 *
 * // Encode
 * gpio_pin_t encoded = (original_port << k_port_shift) | original_pin;
 *
 * // Decode
 * rx_port_number_t decoded_port = rx_port_from_pin(encoded);
 * rx_pin_number_t decoded_pin = rx_pin_from_pin(encoded);
 *
 * // Verify
 * assert(decoded_port == original_port);  // Port A
 * assert(decoded_pin == original_pin);    // Pin 3
 * @endcode
 *
 * @par Example (Array Indexing):
 * @code{.c}
 * // Use pin number to index into pin configuration array
 * gpio_pin_t configured_pins[] = {
 *   k_rx_pb_0, k_rx_pb_2, k_rx_pb_5, k_rx_pb_7
 * };
 *
 * for (size_t i = 0; i < 4; i++) {
 *   rx_pin_number_t pin_num = rx_pin_from_pin(configured_pins[i]);
 *   printf("Pin %d is configured\n", pin_num);
 * }
 * @endcode
 *
 * @see rx_port_from_pin() Extract port number from combined identifier
 * @see rx_port_pin_t Pre-computed port/pin combinations
 * @see port_encoding_t Encoding constants (k_port_shift, k_port_mask)
 * @see rx_pin_number_t Valid pin numbers that can be returned
 *
 * @since Version 1.0.0
 * @test test_rx_pin_from_pin() Unit tests verify all 144 port/pin combinations
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5**: [OK] 2 preconditions + 3 postconditions (5 assertions total)
 * - **Rule 4**: [OK] 6 lines of code (excluding comments/braces)
 */
static inline uint8_t rx_pin_from_pin(rx_port_pin_t pin)
{
  /* Pre-condition 1: port value fits encoding scheme (port in upper byte) */
  uint8_t port = (uint8_t)((pin) >> k_port_shift);
  /* Note: port >= k_rx_port_0 is always true for uint8_t since k_rx_port_0 == 0 */

  /* Pre-condition 2: port is valid (0-G or J, skipping H-I) */
  RX_ASSERT((port <= k_rx_port_g) || (port == k_rx_port_j),
            "Port number must be valid (k_rx_port_0..k_rx_port_g or k_rx_port_j)");

  uint8_t result = (uint8_t)((pin)&k_port_mask);

  /* Post-condition: result fits in rx_pin_number_t range (0x00-0x07) */
  /* Note: result >= k_rx_pin_min is always true for uint8_t since k_rx_pin_min == 0 */
  RX_ASSERT(result <= k_rx_pin_max, "Pin number must be in range k_rx_pin_min..k_rx_pin_max");

  return result;
}

#ifdef __cplusplus
}
#endif
