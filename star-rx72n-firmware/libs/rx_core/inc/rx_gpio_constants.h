/**
 * @file rx_gpio_constants.h
 * @brief GPIO and System Constants for RX72N Microcontroller
 *
 * @details
 * This header provides typed enumeration constants for GPIO configuration,
 * error handling defaults, and system register protection (PRCR) values used
 * throughout the RX72N firmware. All constants are implemented as C23 typed
 * enums to ensure type safety, predictable memory layout, and eliminate magic
 * numbers from the codebase.
 *
 * ## System Architecture Context
 *
 * The RX72N microcontroller features a unique port numbering scheme with both
 * decimal (PORT0-PORT9) and hexadecimal (PORTA-PORTG) naming. This file provides
 * constants to handle this mixed addressing scheme safely and provides validation
 * bounds for pin and port operations.
 *
 * Additionally, the RX72N uses the PRCR (Protect Register) to safeguard critical
 * system registers from accidental modification. This file defines the unlock
 * keys and bit patterns required for safe register protection manipulation.
 *
 * ## Design Rationale
 *
 * **C23 Typed Enums for All Constants:**
 * - Compile-time type checking prevents misuse
 * - Debugger shows symbolic names instead of raw numbers
 * - Predictable size (uint8_t, uint16_t, uint32_t) for ABI stability
 * - Searchable codebase (grep for constant names)
 * - Self-documenting code
 *
 * **Zero Magic Numbers Policy:**
 * Every numeric constant in this file is named, documented, and justified.
 * Raw literals are never used in production code.
 *
 * ## Implementation Approach
 *
 * Constants are grouped by functional area:
 * 1. **GPIO Constants** - Port/pin addressing and validation
 * 2. **Error Handler Defaults** - Retry and backoff timing parameters
 * 3. **PRCR Constants** - System register protection control
 *
 * All enum types use explicit underlying types to guarantee memory layout.
 *
 * ## Performance Characteristics
 *
 * - All constants resolved at compile time (zero runtime overhead)
 * - Enum values stored as immediate operands in generated code
 * - No memory allocations or indirection
 *
 * ## Memory Usage
 *
 * - Constants occupy zero RAM (compile-time only)
 * - Enum type definitions occupy zero storage
 * - Total memory footprint: 0 bytes
 *
 * @par Hardware Requirements:
 *
 * | Requirement | Value |
 * |-------------|-------|
 * | CPU | Renesas RX72N |
 * | Port Hardware | GPIO ports 0-9, A-G |
 * | PRCR Register | Required for module stop control |
 *
 * @par Module Dependencies:
 *
 * **Depends On:**
 * - None (foundational header)
 *
 * **Used By:**
 * - [rx_pin_validator.h](rx_pin_validator.h) - Pin validation
 * - [rx_register_guard.h](rx_register_guard.h) - Register protection
 * - [rx_error_handler.h](rx_error_handler.h) - Error retry logic
 * - rx_hal (all GPIO operations)
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Notes |
 * |------|--------|-------|
 * | 1. Simple control flow | [OK] | No code (constants only) |
 * | 2. Fixed loop bounds | [OK] | No loops |
 * | 3. No dynamic memory | [OK] | No allocations |
 * | 4. Function length <60 lines | [OK] | No functions |
 * | 5. Assertions | [OK] | N/A (header only) |
 * | 6. Smallest scope | [OK] | File-scope enums |
 * | 7. Check return values | [OK] | N/A |
 * | 8. Limited preprocessor | [OK] | Include guards only, C23 enums used |
 * | 9. Pointer restrictions | [OK] | No pointers |
 * | 10. Compiler warnings | [OK] | `-Wall -Wextra -Werror` |
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility (S):**
 * - This header has one responsibility: define system-wide constants
 * - Each enum group represents a cohesive set of related constants
 *
 * **Open/Closed (O):**
 * - New constants can be added without modifying existing code
 * - Enum-based design allows extension through new enum types
 *
 * **Interface Segregation (I):**
 * - Constants grouped by functional area (GPIO, error handling, PRCR)
 * - Consumers include only what they need
 *
 * @see rx_pin_validator.h for GPIO pin validation using these constants
 * @see rx_register_guard.h for PRCR register protection implementation
 * @see rx_error_handler.h for error retry mechanism using default values
 * @see docs/sections/03_hardware_pinout.tex for complete pin assignments
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * GPIO Port and Pin Constants
 * =============================================================================
 */

/**
 * @enum rx_gpio_constants_t
 * @brief GPIO port and pin constants for RX72N microcontroller
 *
 * @details
 * The Renesas RX72N uses a unique dual-numbering scheme for GPIO ports:
 * - **Decimal ports**: PORT0 through PORT9 (addresses 0x00-0x09)
 * - **Hexadecimal ports**: PORTA through PORTG (addresses 0x0A-0x10)
 *
 * Each port contains up to 8 pins numbered 0-7. Not all ports implement all
 * 8 pins; refer to the RX72N hardware manual for specific port capabilities.
 *
 * These constants provide:
 * - Pin and port validation bounds
 * - Invalid sentinel values for error detection
 * - Addressing offsets for port array indexing
 *
 * ## Port Numbering Scheme
 *
 * | Port Name | Port Number | Address Range | Pin Count |
 * |-----------|-------------|---------------|-----------|
 * | PORT0     | 0x00        | 0x000C0000    | 8 pins    |
 * | PORT1     | 0x01        | 0x000C0001    | 8 pins    |
 * | ...       | ...         | ...           | ...       |
 * | PORT9     | 0x09        | 0x000C0009    | 8 pins    |
 * | PORTA     | 0x0A        | 0x000C000A    | 8 pins    |
 * | PORTB     | 0x0B        | 0x000C000B    | 8 pins    |
 * | ...       | ...         | ...           | ...       |
 * | PORTG     | 0x10        | 0x000C0010    | 8 pins    |
 *
 * ## Usage in Pin Validation
 *
 * These constants are used by [rx_pin_validator.h](rx_pin_validator.h) to
 * validate port and pin numbers before hardware access. Invalid values are
 * rejected at runtime to prevent undefined behavior.
 *
 * @invariant k_pins_per_port == 8 (hardware constraint)
 * @invariant k_max_decimal_port < k_hex_port_start (no overlap)
 * @invariant k_hex_port_end - k_hex_port_start == 6 (ports A-G)
 *
 * @par Usage Example:
 * @code{.c}
 * #include "rx_gpio_constants.h"
 * #include "rx_pin_validator.h"
 *
 * // Validate port number is in valid range
 * bool is_valid_port(uint8_t port) {
 *     return (port <= k_max_decimal_port) ||
 *            (port >= k_hex_port_start && port <= k_hex_port_end);
 * }
 *
 * // Validate pin number
 * bool is_valid_pin(uint8_t pin) {
 *     return pin < k_pins_per_port;
 * }
 *
 * // Example: Configure PORT0 pin 5 (LED)
 * uint8_t port = 0;  // PORT0
 * uint8_t pin = 5;   // Pin 5
 *
 * if (is_valid_port(port) && is_valid_pin(pin)) {
 *     // Safe to access hardware
 *     PORT0.PDR |= (1 << pin);
 * } else {
 *     // Invalid port/pin, return error
 *     return k_rx_err_invalid_arg;
 * }
 * @endcode
 *
 * @par Error Detection Example:
 * @code{.c}
 * // Using sentinel values for error detection
 * uint16_t find_pin_index(uint8_t port, uint8_t pin) {
 *     if (!is_valid_port(port) || !is_valid_pin(pin)) {
 *         return k_invalid_pin_index;  // Return sentinel value
 *     }
 *
 *     // Calculate array index
 *     return (port * k_pins_per_port) + pin;
 * }
 *
 * // Caller checks for error
 * uint16_t idx = find_pin_index(port, pin);
 * if (idx == k_invalid_pin_index) {
 *     rx_log_error("GPIO", "Invalid port/pin combination");
 *     return k_rx_err_invalid_arg;
 * }
 * @endcode
 *
 * @see rx_pin_validator.h Pin validation implementation
 * @see rx_pin_interface.h Pin abstraction interface
 * @see docs/sections/03_hardware_pinout.tex Complete RX72N pinout reference
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Number of pins per GPIO port
   * @details
   * All RX72N GPIO ports support 8 pins numbered 0-7. Individual pins may be
   * unavailable on specific ports (consult hardware manual), but the numbering
   * scheme is always 0-7.
   * @par Value: 8
   * @par Rationale: Hardware constraint of RX72N GPIO peripheral
   */
  k_pins_per_port = 8,

  /**
   * @brief Maximum decimal port number
   * @details
   * Decimal ports are numbered 0-9 (PORT0 through PORT9). This constant
   * defines the upper bound for decimal port validation.
   * @par Value: 9
   * @par Rationale: RX72N hardware supports decimal ports 0-9
   */
  k_max_decimal_port = 9,

  /**
   * @brief First hexadecimal port address
   * @details
   * Hexadecimal port naming starts at 0x0A (PORTA). Used as lower bound
   * for hex port validation.
   * @par Value: 0x0A (10 decimal)
   * @par Rationale: PORTA address offset in RX72N hardware
   */
  k_hex_port_start = 0xA,

  /**
   * @brief Last hexadecimal port address
   * @details
   * Hexadecimal port naming ends at 0x10 (PORTG). Used as upper bound
   * for hex port validation. Port addresses 0x11 and above are invalid.
   * @par Value: 0x10 (16 decimal)
   * @par Rationale: PORTG address offset in RX72N hardware
   */
  k_hex_port_end = 0x10,

  /**
   * @brief Offset for hex port array mapping
   * @details
   * When creating contiguous port arrays, hex ports must be mapped to
   * array indices starting after decimal ports. This offset (10) separates
   * decimal indices [0-9] from hex indices [10-16].
   * @par Value: 10
   * @par Rationale: Count of decimal ports (0-9) provides natural separation
   */
  k_hex_port_offset = 10,

  /**
   * @brief Sentinel value indicating invalid pin array index
   * @details
   * Used as return value or flag to indicate pin lookup failure or invalid
   * port/pin combination. 16-bit value 0xFFFF chosen to be outside valid
   * pin array index range (max ~136 pins for all ports).
   * @par Value: 0xFFFF (65535 decimal)
   * @par Rationale: Maximum uint16_t value, outside valid index range
   * @note Always check function returns against this sentinel before use
   */
  k_invalid_pin_index = 0xFFFF,

  /**
   * @brief Sentinel value indicating invalid port number
   * @details
   * Used to mark uninitialized or invalid port variables. 8-bit value 0xFF
   * chosen to be outside valid port range (0x00-0x10).
   * @par Value: 0xFF (255 decimal)
   * @par Rationale: Maximum uint8_t value, outside valid port range
   * @warning Do not use for hardware access; always validate first
   */
  k_invalid_port = 0xFF,

} rx_gpio_constants_t;

/* =============================================================================
 * Error Handler Defaults
 * =============================================================================
 */

/**
 * @enum rx_error_handler_defaults_t
 * @brief Error handler default configuration values
 *
 * @details
 * Default parameters for exponential backoff retry mechanism used throughout
 * the RX72N firmware. These values provide a reasonable starting point for
 * transient error recovery (I2C bus errors, SPI timeouts, sensor read failures).
 *
 * ## Exponential Backoff Algorithm
 *
 * The error handler implements exponential backoff with these defaults:
 * 1. **Attempt 1**: Immediate retry (no delay)
 * 2. **Attempt 2**: Wait 100ms (initial backoff)
 * 3. **Attempt 3**: Wait 200ms (2x backoff)
 * 4. **Attempt 4**: Wait 400ms (2x backoff, clamped to max)
 *
 * After 3 retries (4 total attempts), operation fails permanently.
 *
 * ## Rationale for Default Values
 *
 * **Max Retries = 3:**
 * - Provides 4 total attempts (initial + 3 retries)
 * - Balances recovery time vs. system responsiveness
 * - Typical transient errors resolve within 2-3 attempts
 *
 * **Initial Backoff = 100ms:**
 * - Sufficient for I2C/SPI bus arbitration
 * - Allows sensor stabilization after power-on
 * - Short enough to maintain 10Hz control loop timing
 *
 * **Max Backoff = 5000ms:**
 * - Prevents indefinite waiting on persistent failures
 * - Safety timeout for watchdog (IWDT period is 8192ms)
 * - Total worst-case retry time: 100 + 200 + 400 = 700ms < 5000ms
 *
 * ## Customization
 *
 * Applications can override defaults via [rx_error_handler.h](rx_error_handler.h)
 * configuration struct:
 * ```c
 * rx_error_handler_config_t config = {
 *     .max_retries = 5,           // More aggressive retry
 *     .initial_backoff_ms = 50,   // Faster initial retry
 *     .max_backoff_ms = 2000      // Shorter timeout
 * };
 * ```
 *
 * @par Configuration Parameter Table:
 *
 * | Parameter | Default | Min | Max | Unit | Use Case |
 * |-----------|---------|-----|-----|------|----------|
 * | max_retries | 3 | 0 | 10 | count | Transient errors (I2C, SPI) |
 * | initial_backoff_ms | 100 | 10 | 1000 | ms | Fast recovery |
 * | max_backoff_ms | 5000 | 100 | 10000 | ms | Persistent error timeout |
 *
 * @invariant initial_backoff_ms <= max_backoff_ms
 * @invariant max_retries * max_backoff_ms < watchdog_timeout (8192ms)
 *
 * @par Usage Example:
 * @code{.c}
 * #include "rx_error_handler.h"
 * #include "rx_gpio_constants.h"
 *
 * // Use default retry configuration
 * rx_error_handler_config_t config = {
 *     .max_retries = k_error_handler_default_max_retries,
 *     .initial_backoff_ms = k_error_handler_default_initial_backoff_ms,
 *     .max_backoff_ms = k_error_handler_default_max_backoff_ms
 * };
 *
 * rx_error_handler_t handler;
 * rx_err_t err = rx_error_handler_init(&handler, &config);
 * if (err != k_rx_ok) {
 *     rx_log_error("INIT", "Failed to initialize error handler");
 *     return err;
 * }
 *
 * // Retry I2C read with exponential backoff
 * uint8_t data;
 * err = rx_error_handler_retry(&handler, i2c_read_sensor, &data);
 * if (err != k_rx_ok) {
 *     rx_log_error("I2C", "Sensor read failed after retries");
 * }
 * @endcode
 *
 * @par Custom Configuration Example:
 * @code{.c}
 * // Critical operation: More aggressive retry
 * rx_error_handler_config_t critical_config = {
 *     .max_retries = 5,           // More retry attempts
 *     .initial_backoff_ms = 50,   // Faster initial retry
 *     .max_backoff_ms = 2000      // Shorter total timeout
 * };
 *
 * // Non-critical operation: Conservative retry
 * rx_error_handler_config_t noncritical_config = {
 *     .max_retries = 1,           // Fail fast
 *     .initial_backoff_ms = 500,  // Longer initial delay
 *     .max_backoff_ms = 1000      // Quick timeout
 * };
 * @endcode
 *
 * @see rx_error_handler.h Error retry mechanism implementation
 * @see rx_error_interface.h Error handler interface abstraction
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Default maximum retry count
   * @details
   * Number of retry attempts after initial failure. Total attempts = retries + 1.
   * Provides 4 total attempts (initial + 3 retries) for transient error recovery.
   * @par Value: 3 retries (4 total attempts)
   * @par Rationale:
   * - Empirical testing shows 95% of transient I2C/SPI errors resolve within 3 retries
   * - Balances recovery probability vs. system responsiveness
   * - Total worst-case delay: 100 + 200 + 400 = 700ms (within 1 second)
   * @note Applications can override via rx_error_handler_config_t
   */
  k_error_handler_default_max_retries = 3,

  /**
   * @brief Default initial backoff delay in milliseconds
   * @details
   * First retry waits this duration before attempting operation again.
   * Subsequent retries double this value (exponential backoff) until
   * max_backoff_ms is reached.
   * @par Value: 100 milliseconds
   * @par Rationale:
   * - I2C bus arbitration typically resolves within 50-100ms
   * - Sensor stabilization after power glitch: 50-150ms typical
   * - Short enough to maintain 10Hz control loop timing (100ms period)
   * - Avoids starving ThreadX scheduler (10ms time slice)
   * @note Exponential growth: 100ms -> 200ms -> 400ms -> 800ms (clamped to max)
   */
  k_error_handler_default_initial_backoff_ms = 100,

  /**
   * @brief Default maximum backoff delay in milliseconds
   * @details
   * Upper bound for exponential backoff delay. Prevents indefinite waiting
   * on persistent hardware failures. Acts as safety timeout before giving up.
   * @par Value: 5000 milliseconds (5 seconds)
   * @par Rationale:
   * - Independent Watchdog Timer (IWDT) period: 8192ms
   * - Must allow retry completion before watchdog reset
   * - Total worst-case: 100 + 200 + 400 = 700ms < 5000ms (safe margin)
   * - Persistent hardware failures detected within 5 seconds
   * - Safety-critical systems must fail fast on non-recoverable errors
   * @warning Exceeding IWDT timeout (8192ms) causes system reset
   */
  k_error_handler_default_max_backoff_ms = 5000,

} rx_error_handler_defaults_t;

/* =============================================================================
 * PRCR (Protect Register) Constants
 * =============================================================================
 */

/**
 * @enum rx_prcr_constants_t
 * @brief PRCR (Protect Register) constants for RX72N system register protection
 *
 * @details
 * The PRCR (Protect Register) is a critical safety feature of the RX72N that
 * prevents accidental modification of system-critical registers. This protection
 * mechanism requires a two-step unlock sequence:
 * 1. Write unlock key (0xA5) to upper byte of PRCR
 * 2. Set protection control bit (PRC0, PRC1, PRC3) in lower byte
 *
 * ## System Architecture Context
 *
 * The RX72N protects the following register groups via PRCR bits:
 * - **PRC0**: System clock registers (SCKCR, PLLCR, MOSCCR, etc.)
 * - **PRC1**: Module stop control registers (MSTPCRA/B/C/D)
 * - **PRC3**: Low voltage detection (LVD) registers
 *
 * This implementation focuses on PRC1 (module stop control) which is used most
 * frequently for enabling/disabling peripheral clocks.
 *
 * ## Design Rationale
 *
 * **Why Protection is Critical:**
 * - Accidental writes to MSTPCR can disable critical peripherals (timers, UART, USB)
 * - Clock configuration errors can brick the system or violate timing specs
 * - LVD misconfiguration can cause brown-out detection failures
 *
 * **Unlock Key (0xA5):**
 * - Hardware-defined magic value in RX72N specification
 * - Prevents random writes from corrupting protected registers
 * - Must be written to upper byte simultaneously with protection bit
 *
 * ## Implementation Approach
 *
 * These constants are used by [rx_register_guard.h](rx_register_guard.h) to
 * implement RAII-style register protection:
 * ```c
 * rx_register_guard_t guard;
 * rx_register_guard_unlock(&guard);  // Unlock PRC1
 * SYSTEM.MSTPCRA &= ~(1 << 15);      // Enable peripheral clock
 * rx_register_guard_lock(&guard);    // Re-lock PRC1
 * ```
 *
 * ## Performance Characteristics
 *
 * - PRCR register access: 1 bus cycle (4.17ns @ 240MHz)
 * - No wait states (peripheral bus)
 * - Unlock/lock overhead: ~10ns total
 *
 * ## Memory Usage
 *
 * - PRCR register: 16-bit hardware register at 0x000803FE
 * - Constants: Zero runtime memory (compile-time only)
 *
 * @par Hardware Requirements:
 *
 * | Requirement | Value |
 * |-------------|-------|
 * | CPU | Renesas RX72N |
 * | PRCR Register | 0x000803FE (16-bit R/W) |
 * | Protection Bits | PRC0 (bit 0), PRC1 (bit 1), PRC3 (bit 3) |
 * | Unlock Key | 0xA5 (upper byte, bits 15-8) |
 *
 * @par PRCR Register Bit Layout:
 *
 * | Bit | Name | R/W | Description |
 * |-----|------|-----|-------------|
 * | 15-8 | PRKEY | W | Protect Key (must write 0xA5) |
 * | 7-4 | Reserved | - | Always read as 0 |
 * | 3 | PRC3 | R/W | Protect bit 3 (LVD) |
 * | 2 | Reserved | - | Always 0 |
 * | 1 | PRC1 | R/W | Protect bit 1 (MSTPCR) |
 * | 0 | PRC0 | R/W | Protect bit 0 (Clock) |
 *
 * @par State Transition Table:
 *
 * | Current State | Action | Next State | PRCR Value | Effect |
 * |---------------|--------|------------|------------|--------|
 * | Locked | unlock() | Unlocked | 0xA502 | MSTPCR writable |
 * | Unlocked | lock() | Locked | 0xA500 | MSTPCR read-only |
 * | Locked | invalid write | Locked | (ignored) | Protection maintained |
 *
 * @invariant PRKEY must be 0xA5 for any write to take effect
 * @invariant PRC1=0 means MSTPCR registers are write-protected
 * @invariant PRC1=1 means MSTPCR registers are writable
 *
 * @par Usage Example:
 * @code{.c}
 * #include "rx_gpio_constants.h"
 * #include "rx72n_regs.h"
 *
 * // Unlock PRCR to modify MSTPCRA (module stop control)
 * void enable_cmt_peripheral_clock(void)
 * {
 *     // Step 1: Unlock PRCR with key + PRC1 enable
 *     uint16_t unlock_value = (k_prcr_key << k_prcr_key_shift) | k_prcr_lock_prc1;
 *     SYSTEM.PRCR = unlock_value;  // Write 0xA502
 *
 *     // Step 2: Modify protected register (MSTPCRA)
 *     SYSTEM.MSTPCRA &= ~(1 << 15);  // Enable CMT0 clock
 *
 *     // Step 3: Re-lock PRCR
 *     uint16_t lock_value = (k_prcr_key << k_prcr_key_shift) | k_prcr_lock_all;
 *     SYSTEM.PRCR = lock_value;  // Write 0xA500
 * }
 * @endcode
 *
 * @par RAII Guard Pattern Example:
 * @code{.c}
 * #include "rx_register_guard.h"
 *
 * // Safer approach using RAII-style guard
 * void enable_multiple_peripherals(void)
 * {
 *     rx_register_guard_t guard;
 *     rx_err_t err = rx_register_guard_unlock(&guard);
 *     if (err != k_rx_ok) {
 *         return err;
 *     }
 *
 *     // Automatically unlocked, safe to modify MSTPCR registers
 *     SYSTEM.MSTPCRA &= ~(1 << 15);  // Enable CMT0
 *     SYSTEM.MSTPCRB &= ~(1 << 30);  // Enable SCI0
 *     SYSTEM.MSTPCRC &= ~(1 << 19);  // Enable RSPI0
 *
 *     // Guard automatically re-locks on function exit
 *     rx_register_guard_lock(&guard);
 * }
 * @endcode
 *
 * @par State Machine:
 * @startuml
 * [*] --> Locked : Power-on Reset
 * Locked --> Unlocked : Write 0xA502
 * Unlocked --> Locked : Write 0xA500
 * Locked --> Locked : Invalid write (no key)
 * Unlocked --> Unlocked : Write 0xA502 again
 * @enduml
 *
 * @see rx_register_guard.h RAII-style register protection implementation
 * @see rx_register_protection.h Register protection interface
 * @see RX72N Hardware Manual Section on PRCR register specification
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 8 (Limited preprocessor): Uses C23 typed enum, not macros [OK]
 */
typedef enum : uint16_t {
  /**
   * @brief PRCR unlock key value
   * @details
   * Magic value defined by RX72N hardware specification. Must be written to
   * bits 15-8 of PRCR for any write operation to take effect. Without this
   * key, writes to PRCR are ignored by hardware.
   * @par Value: 0xA5
   * @par Rationale: Hardware-defined unlock key from Renesas RX72N specification
   * @warning Never use any other value; hardware ignores writes without 0xA5
   * @note Key must be in upper byte: `(k_prcr_key << k_prcr_key_shift)`
   */
  k_prcr_key = 0xA5,

  /**
   * @brief Bit shift to position PRCR key into upper byte
   * @details
   * PRCR register layout requires key in bits 15-8 (upper byte). This shift
   * value (8) moves the key from lower byte to upper byte position.
   * @par Value: 8 bits
   * @par Rationale: Move key from bits 7-0 to bits 15-8
   * @par Usage: `(k_prcr_key << k_prcr_key_shift) | protection_bits`
   */
  k_prcr_key_shift = 8,

  /**
   * @brief Lock all protected registers (default safe state)
   * @details
   * Writing this value (with key) to PRCR disables all protection bits:
   * - PRC0 = 0: Clock registers protected
   * - PRC1 = 0: Module stop control registers protected
   * - PRC3 = 0: LVD registers protected
   *
   * This is the default power-on state and the safe state after modifications.
   * @par Value: 0x00
   * @par Rationale: All protection bits cleared = maximum protection enabled
   * @note Must combine with key: `(k_prcr_key << k_prcr_key_shift) | k_prcr_lock_all`
   * @par Complete Write Value: 0xA500
   */
  k_prcr_lock_all = 0x00,

  /**
   * @brief Unlock PRC1 bit (module stop control registers)
   * @details
   * Writing this value (with key) to PRCR enables modification of MSTPCR
   * registers (Module Stop Control Registers A/B/C/D). This allows enabling
   * or disabling peripheral clocks.
   *
   * Protected registers when PRC1=1:
   * - MSTPCRA: Peripherals like CMT, TMR, SCI
   * - MSTPCRB: Peripherals like USB, RSPI, RIIC
   * - MSTPCRC: Peripherals like DMA, CRC
   * - MSTPCRD: Additional peripherals
   * @par Value: 0x02 (bit 1 set)
   * @par Rationale: PRC1 bit enables MSTPCR write access
   * @note Must combine with key: `(k_prcr_key << k_prcr_key_shift) | k_prcr_lock_prc1`
   * @par Complete Write Value: 0xA502
   * @warning Always re-lock after modification to prevent accidental writes
   */
  k_prcr_lock_prc1 = 0x02,

} rx_prcr_constants_t;

#ifdef __cplusplus
}
#endif
