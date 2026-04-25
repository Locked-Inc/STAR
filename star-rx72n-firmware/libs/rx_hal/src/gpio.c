/**
 * @file gpio.c
 * @brief GPIO Driver for RX72N - General Purpose Input/Output Control
 *
 * @details
 * ## Overview
 *
 * This file provides the hardware abstraction layer (HAL) for GPIO operations on
 * the Renesas RX72N microcontroller. It handles pin direction configuration,
 * digital output control, and digital input reading with integrated error handling,
 * logging, and pin validation through the pin validator interface.
 *
 * ## System Architecture
 *
 * @dot
 * digraph gpio_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_app {
 *     label="Application Layer";
 *     style=dashed;
 *     led [label="LED Control"];
 *     button [label="Button Input"];
 *     status [label="Status Indicators"];
 *   }
 *
 *   subgraph cluster_hal {
 *     label="HAL Layer (THIS FILE)";
 *     style=filled;
 *     fillcolor=lightblue;
 *     gpio_api [label="GPIO API\ngpio_set_output()\ngpio_set_input()\ngpio_write_high()\ngpio_write_low()\ngpio_toggle()\ngpio_read()"];
 *   }
 *
 *   subgraph cluster_infra {
 *     label="Infrastructure";
 *     style=dashed;
 *     pin_validator [label="Pin Validator\n(Conflict Detection)"];
 *     port_utils [label="Port Utilities\n(Address Lookup)"];
 *   }
 *
 *   subgraph cluster_hw {
 *     label="Hardware";
 *     style=dashed;
 *     port_regs [label="PORT Registers\nPDR, PODR, PIDR, PMR"];
 *   }
 *
 *   led -> gpio_api;
 *   button -> gpio_api;
 *   status -> gpio_api;
 *   gpio_api -> pin_validator [label="reserve_pin"];
 *   gpio_api -> port_utils [label="get_base"];
 *   gpio_api -> port_regs [label="register access"];
 * }
 * @enddot
 *
 * ## RX72N GPIO Register Architecture
 *
 * Each GPIO port on the RX72N has multiple control registers:
 *
 * | Register | Name | Purpose |
 * |----------|------|---------|
 * | PDR | Port Direction Register | 0=input, 1=output |
 * | PODR | Port Output Data Register | Output level (0=low, 1=high) |
 * | PIDR | Port Input Data Register | Read current pin level |
 * | PMR | Port Mode Register | 0=GPIO, 1=peripheral function |
 * | PCR | Pull-up Control Register | Enable internal pull-up |
 * | ODR | Open-Drain Control Register | Enable open-drain mode |
 *
 * This driver primarily uses PDR, PODR, PIDR, and PMR.
 *
 * ## Pin Configuration Sequence
 *
 * @msc
 * App, GPIO, PinValidator, PortRegs;
 *
 * App => GPIO [label="gpio_set_output(pin)"];
 * GPIO => PinValidator [label="reserve_pin(port, pin, 'GPIO_OUT')"];
 * PinValidator => GPIO [label="k_rx_ok"];
 * GPIO => PortRegs [label="PMR &= ~(1<<pin)  // GPIO mode"];
 * GPIO => PortRegs [label="PDR |= (1<<pin)   // Output"];
 * GPIO => App [label="k_rx_ok"];
 *
 * App => GPIO [label="gpio_write_high(pin)"];
 * GPIO => PortRegs [label="PODR |= (1<<pin)"];
 * GPIO => App [label="k_rx_ok"];
 * @endmsc
 *
 * ## Thread Safety
 *
 * | Function | Thread-Safe | Notes |
 * |----------|-------------|-------|
 * | gpio_set_output() | Partial | Pin reservation is mutex-protected |
 * | gpio_set_input() | Partial | Pin reservation is mutex-protected |
 * | gpio_write_high() | No | Direct register write |
 * | gpio_write_low() | No | Direct register write |
 * | gpio_toggle() | No | Read-modify-write, not atomic |
 * | gpio_read() | Yes | Read-only operation |
 *
 * For thread-safe output operations, caller must provide synchronization.
 *
 * ## Performance Characteristics
 *
 * | Operation | Cycles | Time @ 240 MHz | Notes |
 * |-----------|--------|----------------|-------|
 * | gpio_set_output | ~200-500 | ~1-2 us | Includes pin reservation |
 * | gpio_set_input | ~200-500 | ~1-2 us | Includes pin reservation |
 * | gpio_write_high | ~20-30 | ~100 ns | Direct register |
 * | gpio_write_low | ~20-30 | ~100 ns | Direct register |
 * | gpio_toggle | ~30-40 | ~150 ns | Read-modify-write |
 * | gpio_read | ~20-30 | ~100 ns | Direct register read |
 *
 * ## Memory Usage
 *
 * | Section | Size | Contents |
 * |---------|------|----------|
 * | .text | ~600 bytes | Function code |
 * | .rodata | ~100 bytes | Log strings |
 * | .bss | 0 bytes | No module-level state |
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simplify control flow | [OK] | No goto, setjmp, recursion |
 * | 2. Fixed loop bounds | [OK] | No loops in this module |
 * | 3. No dynamic memory | [OK] | All static, no malloc |
 * | 4. Functions < 60 lines | [OK] | All functions under 30 lines |
 * | 5. Use assertions | [OK] | RX_CHECK_NULL_PTR, validation |
 * | 6. Data at smallest scope | [OK] | Local variables only |
 * | 7. Check return values | [OK] | RX_RETURN_ON_ERROR macro |
 * | 8. Limit preprocessor | [OK] | Only includes |
 * | 9. Restrict pointers | [OK] | Only hardware register pointers |
 * | 10. Compile warnings | [OK] | -Wall -Wextra -Werror clean |
 *
 * ## SOLID Principles
 *
 * | Principle | Implementation |
 * |-----------|----------------|
 * | **Single Responsibility** | GPIO operations only (no PWM, no ADC) |
 * | **Open/Closed** | Extended via rx_port_pin_t encoding |
 * | **Liskov Substitution** | N/A (not interface-based) |
 * | **Interface Segregation** | Separate functions for each operation |
 * | **Dependency Inversion** | Uses pin_interface_t for validation |
 *
 * @see rx_port_constants.h Port and pin definitions
 * @see rx_port_utils.h Port address lookup utilities
 * @see rx72n_regs.h Register definitions
 * @see rx_pin_validator.h Pin conflict detection
 *
 * @author Locked, Inc.
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#ifdef __RX__

#include <stddef.h>

#include "hardware.h"
#include "rx72n_regs.h"
#include "rx_port_constants.h"
#include "rx_port_utils.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum gpio_bit_constants_t
 * @brief GPIO register bit manipulation constants
 *
 * @details
 * Defines constants used for bit manipulation in GPIO register operations.
 * Using named constants instead of magic numbers improves code readability
 * and makes the intent of bit operations clear.
 *
 * ## Usage Examples
 *
 * @code
 * // Set bit 5 in PDR register (configure as output)
 * port->pdr |= (k_gpio_bit_set << 5);
 *
 * // Clear bit 3 in PMR register (configure as GPIO, not peripheral)
 * port->pmr &= ~(k_gpio_bit_set << 3);
 *
 * // Check if bit 2 is set in PIDR register
 * if ((port->pidr & (k_gpio_bit_set << 2)) != k_gpio_bit_clear) {
 *   // Pin is high
 * }
 * @endcode
 *
 * @see gpio_set_output() Uses these for PDR/PMR manipulation
 * @see gpio_read() Uses these for PIDR comparison
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_gpio_bit_set   = 1, /**< Value for setting a single bit (1 << n pattern) */
  k_gpio_bit_clear = 0, /**< Value for comparison when checking if bit is cleared */
} gpio_bit_constants_t;

/** @brief Log tag for this module */
static const char* const s_tag = "GPIO";

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Validate port and pin numbers against RX72N hardware limits
 *
 * @details
 * Performs validation of GPIO port and pin numbers before any register access.
 * This function is called by all public GPIO functions to ensure safe operation
 * and prevent out-of-bounds hardware register access.
 *
 * ## Validation Algorithm
 *
 * 1. Check port validity via rx_port_get_base() (returns NULL for invalid ports)
 * 2. Check pin number against k_rx_pin_max (maximum valid pin number per port)
 *
 * ## Port Validation
 *
 * Uses rx_port_get_base() which returns NULL for any port number that does not
 * correspond to a physical port on the RX72N. Valid port numbers are 0-9
 * (decimal) and 0xA-0x10 (hexadecimal, corresponding to ports A through G).
 * Passing an out-of-range port is safely caught here before any register
 * dereference occurs.
 *
 * ## Pin Validation
 *
 * Each RX72N GPIO port exposes up to 8 pins numbered 0 through 7. The
 * k_rx_pin_max constant defines the inclusive upper bound (7). A lower-bound
 * check against zero is omitted because pin is a uint8_t, making pin < 0
 * structurally impossible; omitting it avoids a -Wtype-limits compiler warning.
 *
 * ## Why Both Checks Are Required
 *
 * An invalid port with a valid pin, or a valid port with an invalid pin, must
 * both be rejected. Either condition alone would result in incorrect hardware
 * register access, undefined behavior, or corrupted peripheral state.
 *
 *
 *
 * @pre port must be within the RX72N valid port range (0-9, 0xA-0x10);
 *      values outside this range are rejected before any register access
 * @pre pin must be within the valid pin range for the specified port (0 to
 *      k_rx_pin_max); values above k_rx_pin_max are rejected with an error
 * @post Returns k_rx_ok only when both port and pin are simultaneously valid;
 *       no partial-success return is possible
 * @post No hardware state is modified; this function is purely read-only with
 *       respect to peripheral registers and module-level variables
 *
 * @note Thread-safe: No shared mutable state accessed; safe to call concurrently
 * @note Performance: O(1), approximately 50 cycles at 240 MHz
 * @note An error log message is emitted for each failed validation to aid debugging
 *
 * @par Example:
 * @code
 * // Typical usage inside a GPIO public function
 * const uint8_t port    = rx_port_from_pin(pin);
 * const uint8_t pin_num = rx_pin_from_pin(pin);
 * rx_err_t err = internal_validate_port_pin(port, pin_num);
 * RX_RETURN_ON_ERROR(err, s_tag,"Port/pin validation failed");
 * @endcode
 *
 * @see rx_port_get_base() Port lookup function used internally for port validation
 * @see k_rx_pin_max Maximum valid pin index (inclusive upper bound)
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 preconditions, 2 postconditions
 */
static rx_err_t internal_validate_port_pin(const uint8_t port, const uint8_t pin)
{
  /* Validate port */
  const volatile rx_port_regs_t* port_base = rx_port_get_base(port);
  if (port_base == nullptr) {
    rx_log_error(s_tag, "Invalid port number");
    return k_rx_err_gpio_invalid_port;
  }

  /* Validate pin (lower bound check omitted - pin is uint8_t, k_rx_pin_min == 0,
   * so pin >= k_rx_pin_min is always true, avoiding -Wtype-limits warning) */
  if (pin > k_rx_pin_max) {
    rx_log_error(s_tag, "Invalid pin number");
    return k_rx_err_gpio_invalid_pin;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Configure GPIO pin as digital output
 *
 * @details
 * Configures a GPIO pin for digital output mode. This function performs:
 * 1. Parameter validation (port and pin number)
 * 2. Pin reservation via the global pin validator (conflict detection)
 * 3. PMR register clear (select GPIO mode instead of peripheral function)
 * 4. PDR register set (configure as output direction)
 *
 * ## Register Operations
 *
 * ```
 * PMR &= ~(1 << pin)  // Clear bit: Select GPIO mode (not peripheral)
 * PDR |= (1 << pin)   // Set bit: Configure as output
 * ```
 *
 * ## Pin Reservation
 *
 * The function attempts to reserve the pin through rx_infrastructure_get_pin_interface().
 * If the pin is already reserved (k_rx_err_gpio_conflict), the function continues
 * without error - this allows reconfiguration of already-owned pins.
 *
 * ## Initial Output State
 *
 * This function does NOT set the initial output level. After calling gpio_set_output(),
 * call gpio_write_high() or gpio_write_low() to set the desired level.
 *
 *
 *
 * @pre Pin must be a valid GPIO-capable pin on RX72N
 * @pre System infrastructure should be initialized (for pin validation)
 *
 * @post Pin is configured as GPIO output
 * @post PMR bit for pin is cleared (GPIO mode)
 * @post PDR bit for pin is set (output direction)
 * @post Pin is reserved in validator (if available)
 *
 * @note Thread-safe: Pin reservation uses mutex, register writes are not atomic
 * @note Performance: ~1-2 us @ 240 MHz (includes reservation)
 * @note Idempotent: Safe to call multiple times on same pin
 *
 * @warning Output level is undefined after this call - set with gpio_write_*
 * @warning Do not call from ISR (pin reservation may block)
 *
 * @par Example:
 * @code
 * // Configure LED pin as output
 * rx_err_t err = gpio_set_output(RX_PORT_PIN(0, 3));  // Port 0, Pin 3
 * if (err != k_rx_ok) {
 *   rx_log_error("LED", "Failed to configure LED pin");
 *   return err;
 * }
 *
 * // Set initial state to low (LED off)
 * gpio_write_low(RX_PORT_PIN(0, 3));
 *
 * // Toggle LED
 * gpio_toggle(RX_PORT_PIN(0, 3));
 * @endcode
 *
 * @see gpio_set_input() Configure pin as input
 * @see gpio_write_high() Set output high after configuration
 * @see gpio_write_low() Set output low after configuration
 * @see RX_PORT_PIN() Macro to create rx_port_pin_t
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 preconditions, 4 postconditions
 */
rx_err_t gpio_set_output(const rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, s_tag, "Port/pin validation failed");

  /* Reserve pin through global pin validator */
  const rx_pin_interface_t* const pin_iface = rx_infrastructure_get_pin_interface();
  if (pin_iface != nullptr) {
    err = pin_iface->reserve_pin(pin_iface->ctx, port, pin_num, "GPIO_OUT");
    if (err != k_rx_ok && err != k_rx_err_gpio_conflict) {
      /* Allow conflict (pin already reserved), but fail on other errors */
      RX_RETURN_ON_ERROR(err, s_tag, "Pin reservation failed");
    }
  }

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  const uint8_t pin_mask = (uint8_t)(k_gpio_bit_set << pin_num);

  /* Set as GPIO mode (not peripheral) */
  port_base->pmr &= (uint8_t)~pin_mask;

  /* Set as output */
  port_base->pdr |= pin_mask;

  rx_log_debug(s_tag, "Pin configured as output");

  return k_rx_ok;
}

/**
 * @brief Configure GPIO pin as digital input
 *
 * @details
 * Configures a GPIO pin for digital input mode. This function performs:
 * 1. Parameter validation (port and pin number)
 * 2. Pin reservation via the global pin validator (conflict detection)
 * 3. PMR register clear (select GPIO mode instead of peripheral function)
 * 4. PDR register clear (configure as input direction)
 *
 * ## Register Operations
 *
 * ```
 * PMR &= ~(1 << pin)  // Clear bit: Select GPIO mode (not peripheral)
 * PDR &= ~(1 << pin)  // Clear bit: Configure as input
 * ```
 *
 * ## Pull-up Configuration
 *
 * This function does NOT configure internal pull-up resistors. To enable
 * pull-ups, additional configuration of the PCR register is required.
 *
 * ## Reading Input State
 *
 * After configuration, use gpio_read() to read the current pin state.
 * The PIDR register is read for input state, not PODR.
 *
 *
 *
 * @pre Pin must be a valid GPIO-capable pin on RX72N
 * @pre System infrastructure should be initialized (for pin validation)
 *
 * @post Pin is configured as GPIO input
 * @post PMR bit for pin is cleared (GPIO mode)
 * @post PDR bit for pin is cleared (input direction)
 * @post Pin is reserved in validator (if available)
 *
 * @note Thread-safe: Pin reservation uses mutex
 * @note Performance: ~1-2 us @ 240 MHz (includes reservation)
 * @note Pull-ups: Not enabled by this function (use PCR register)
 *
 * @warning Floating inputs may read unpredictable values - use pull-up/pull-down
 * @warning Do not call from ISR (pin reservation may block)
 *
 * @par Example:
 * @code
 * // Configure button pin as input
 * rx_err_t err = gpio_set_input(RX_PORT_PIN(2, 0));  // Port 2, Pin 0
 * if (err != k_rx_ok) {
 *   rx_log_error("BTN", "Failed to configure button pin");
 *   return err;
 * }
 *
 * // Read button state
 * bool pressed;
 * err = gpio_read(RX_PORT_PIN(2, 0), &pressed);
 * if (err == k_rx_ok && pressed) {
 *   rx_log_info("BTN", "Button pressed");
 * }
 * @endcode
 *
 * @see gpio_set_output() Configure pin as output
 * @see gpio_read() Read input pin state
 * @see RX_PORT_PIN() Macro to create rx_port_pin_t
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 preconditions, 4 postconditions
 */
rx_err_t gpio_set_input(const rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, s_tag, "Port/pin validation failed");

  /* Reserve pin through global pin validator */
  const rx_pin_interface_t* const pin_iface = rx_infrastructure_get_pin_interface();
  if (pin_iface != nullptr) {
    err = pin_iface->reserve_pin(pin_iface->ctx, port, pin_num, "GPIO_IN");
    if (err != k_rx_ok && err != k_rx_err_gpio_conflict) {
      /* Allow conflict (pin already reserved), but fail on other errors */
      RX_RETURN_ON_ERROR(err, s_tag, "Pin reservation failed");
    }
  }

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  /* Set as GPIO mode */
  const uint8_t pin_mask = (uint8_t)(k_gpio_bit_set << pin_num);
  port_base->pmr &= (uint8_t)~pin_mask;

  /* Set as input */
  port_base->pdr &= (uint8_t)~pin_mask;

  rx_log_debug(s_tag, "Pin configured as input");

  return k_rx_ok;
}

/**
 * @brief Set GPIO output pin to logic high (3.3V)
 *
 * @details
 * Sets a GPIO output pin to logic high level. On the RX72N, this means
 * the pin outputs VCC (typically 3.3V). The pin must be configured as
 * an output before calling this function.
 *
 * ## Register Operation
 *
 * ```
 * PODR |= (1 << pin)  // Set bit: Output high
 * ```
 *
 * ## Timing
 *
 * The output level changes on the next peripheral clock cycle after the
 * register write completes. GPIO toggle rate is limited by bus speed.
 *
 *
 *
 * @pre Pin must be configured as output via gpio_set_output()
 * @pre Port and pin numbers must be valid
 *
 * @post PODR bit for pin is set (output high)
 * @post Pin outputs VCC level (typically 3.3V)
 *
 * @note NOT thread-safe: Direct register write without synchronization
 * @note Performance: ~100 ns @ 240 MHz
 * @note ISR-safe: Can be called from interrupt handlers
 *
 * @warning No check that pin is configured as output
 * @warning Calling on input pin has no visible effect
 *
 * @par Example:
 * @code
 * // Turn on LED (active high)
 * rx_err_t err = gpio_write_high(RX_PORT_PIN(0, 3));
 * if (err != k_rx_ok) {
 *   // Handle error
 * }
 * @endcode
 *
 * @see gpio_write_low() Set output low
 * @see gpio_toggle() Toggle output state
 * @see gpio_set_output() Configure pin as output first
 *
 * @since Version 1.0.0
 */
rx_err_t gpio_write_high(const rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate parameters */
  const rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, s_tag, "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  const uint8_t pin_mask = (uint8_t)(k_gpio_bit_set << pin_num);
  port_base->podr |= pin_mask;

  return k_rx_ok;
}

/**
 * @brief Set GPIO output pin to logic low (0V)
 *
 * @details
 * Sets a GPIO output pin to logic low level. On the RX72N, this means
 * the pin outputs GND (0V). The pin must be configured as an output
 * before calling this function.
 *
 * ## Register Operation
 *
 * ```
 * PODR &= ~(1 << pin)  // Clear bit: Output low
 * ```
 *
 * ## Open-Drain Mode
 *
 * If the pin is configured for open-drain mode (ODR register), setting
 * low actively pulls the pin to GND. Setting high makes the pin float
 * (high-impedance), requiring an external pull-up.
 *
 *
 *
 * @pre Pin must be configured as output via gpio_set_output()
 * @pre Port and pin numbers must be valid
 *
 * @post PODR bit for pin is cleared (output low)
 * @post Pin outputs GND level (0V)
 *
 * @note NOT thread-safe: Direct register write without synchronization
 * @note Performance: ~100 ns @ 240 MHz
 * @note ISR-safe: Can be called from interrupt handlers
 *
 * @warning No check that pin is configured as output
 * @warning Calling on input pin has no visible effect
 *
 * @par Example:
 * @code
 * // Turn off LED (active high)
 * rx_err_t err = gpio_write_low(RX_PORT_PIN(0, 3));
 * if (err != k_rx_ok) {
 *   // Handle error
 * }
 * @endcode
 *
 * @see gpio_write_high() Set output high
 * @see gpio_toggle() Toggle output state
 * @see gpio_set_output() Configure pin as output first
 *
 * @since Version 1.0.0
 */
rx_err_t gpio_write_low(const rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate parameters */
  const rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, s_tag, "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  const uint8_t pin_mask = (uint8_t)(k_gpio_bit_set << pin_num);
  port_base->podr &= (uint8_t)~pin_mask;

  return k_rx_ok;
}

/**
 * @brief Toggle GPIO output pin state (high<->low)
 *
 * @details
 * Inverts the current output state of a GPIO pin. If the pin is currently
 * high, it becomes low. If currently low, it becomes high. Uses XOR
 * operation for efficient toggling.
 *
 * ## Register Operation
 *
 * ```
 * PODR ^= (1 << pin)  // XOR bit: Toggle output
 * ```
 *
 * ## Atomicity Warning
 *
 * This operation is a read-modify-write sequence and is NOT atomic.
 * If an interrupt modifies the same port register between the read
 * and write, the interrupt's change may be lost.
 *
 * For thread-safe toggling, use explicit write_high/write_low with
 * external synchronization, or disable interrupts around the toggle.
 *
 *
 *
 * @pre Pin must be configured as output via gpio_set_output()
 * @pre Port and pin numbers must be valid
 *
 * @post PODR bit for pin is inverted
 * @post Pin output level is opposite of previous state
 *
 * @note NOT thread-safe: Read-modify-write operation
 * @note NOT atomic: Can race with ISRs on same port
 * @note Performance: ~150 ns @ 240 MHz
 *
 * @warning Race condition possible with ISRs modifying same port
 * @warning Use gpio_write_high/low with sync for critical applications
 *
 * @par Example:
 * @code
 * // Blink LED in loop
 * while (running) {
 *   gpio_toggle(RX_PORT_PIN(0, 3));  // Toggle LED
 *   tx_thread_sleep(50);             // 50ms delay (20 Hz blink)
 * }
 * @endcode
 *
 * @see gpio_write_high() Set output high explicitly
 * @see gpio_write_low() Set output low explicitly
 * @see gpio_read() Read current output state
 *
 * @since Version 1.0.0
 */
rx_err_t gpio_toggle(const rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate parameters */
  const rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, s_tag, "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  const uint8_t pin_mask = (uint8_t)(k_gpio_bit_set << pin_num);
  port_base->podr ^= pin_mask;

  return k_rx_ok;
}

/**
 * @brief Read GPIO input pin state
 *
 * @details
 * Reads the current logic level of a GPIO pin. Uses the PIDR (Port Input Data
 * Register) which reflects the actual voltage level on the pin, regardless
 * of whether the pin is configured as input or output.
 *
 * ## Register Operation
 *
 * ```
 * value = (PIDR & (1 << pin)) != 0
 * ```
 *
 * ## Input vs Output Reading
 *
 * - **Input pin**: PIDR reflects external voltage on pin
 * - **Output pin**: PIDR reflects actual pin state (may differ from PODR
 *   if pin is shorted or overdriven)
 *
 * ## Voltage Thresholds (RX72N)
 *
 * | Level | Voltage Range | Returned Value |
 * |-------|---------------|----------------|
 * | Low | 0V to ~0.8V | false |
 * | High | ~2.0V to VCC | true |
 * | Undefined | 0.8V to 2.0V | Unpredictable |
 *
 *
 *
 * @pre value must be non-nullptr
 * @pre Pin should be configured as input for external signals
 *
 * @post *value contains current pin level (true=high, false=low)
 * @post No hardware state changes
 *
 * @note Thread-safe: Read-only operation
 * @note Performance: ~100 ns @ 240 MHz
 * @note ISR-safe: Can be called from interrupt handlers
 *
 * @warning Floating inputs may return unpredictable values
 * @warning Use pull-up/pull-down for stable readings on open pins
 *
 * @par Example:
 * @code
 * // Read button state (active low with pull-up)
 * bool button_pressed;
 * rx_err_t err = gpio_read(RX_PORT_PIN(2, 0), &button_pressed);
 *
 * if (err == k_rx_ok) {
 *   // Invert because button is active low
 *   if (!button_pressed) {
 *     rx_log_info("BTN", "Button pressed");
 *     handle_button_press();
 *   }
 * }
 * @endcode
 *
 * @par Example: Debounced button read
 * @code
 * bool debounced_read(rx_port_pin_t pin) {
 *   bool first, second;
 *   gpio_read(pin, &first);
 *   tx_thread_sleep(1);  // 1 tick delay
 *   gpio_read(pin, &second);
 *   return (first == second) ? first : false;  // Only return if stable
 * }
 * @endcode
 *
 * @see gpio_set_input() Configure pin as input first
 * @see gpio_write_high() Set output high
 * @see gpio_write_low() Set output low
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 preconditions, 2 postconditions
 */
rx_err_t gpio_read(const rx_port_pin_t pin, bool* value)
{
  /* Check null pointer */
  RX_CHECK_NULL_PTR(value, s_tag, "Value pointer is nullptr");

  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate parameters */
  const rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, s_tag, "Port/pin validation failed");

  /* Get port base address */
  const volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  const uint8_t pin_mask = (uint8_t)(k_gpio_bit_set << pin_num);
  *value                 = (port_base->pidr & pin_mask) != k_gpio_bit_clear;

  return k_rx_ok;
}

#endif /* __RX__ */
