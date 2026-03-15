/**
 * @file mock_rx_hcsr04_hal.c
 * @brief HC-SR04 Ultrasonic Sensor Mock HAL Implementation for Host-Side Testing
 *
 * @details
 * Provides a mock implementation of the HC-SR04 Hardware Abstraction Layer (HAL)
 * interface for host-side unit testing. This module simulates GPIO operations
 * and timing behavior without requiring actual hardware, enabling comprehensive
 * testing of the HC-SR04 driver logic on development machines.
 *
 * **Architecture:**
 * - Implements the `rx_hcsr04_hal.h` interface using mock functions
 * - Delegates GPIO operations to `mock_hcsr04_hw` test infrastructure
 * - Validates pin ranges and port configurations before delegation
 * - Simulates microsecond-precision timing for HC-SR04 protocol
 *
 * **HC-SR04 Protocol Overview:**
 * The HC-SR04 ultrasonic ranging module uses a simple trigger-echo protocol:
 *
 * @msc
 * Firmware, TRIG_Pin, ECHO_Pin, HC_SR04, Object;
 *
 * --- [label="Measurement Cycle"];
 * Firmware => TRIG_Pin [label="10us HIGH pulse"];
 * TRIG_Pin => HC_SR04 [label="trigger"];
 * HC_SR04 box HC_SR04 [label="Generate 8x 40kHz burst"];
 * HC_SR04 => Object [label="ultrasonic pulse"];
 * Object => HC_SR04 [label="echo return"];
 * HC_SR04 => ECHO_Pin [label="pulse HIGH"];
 * ECHO_Pin note ECHO_Pin [label="pulse width = round-trip time"];
 * ECHO_Pin => Firmware [label="measure pulse width"];
 * Firmware box Firmware [label="distance = (pulse_width * 343m/s) / 2"];
 * @endmsc
 *
 * **Distance Calculation:**
 *
 * The HC-SR04 measures distance by timing ultrasonic pulse round-trip travel:
 *
 * @f[
 *   d = \frac{c \cdot t}{2}
 * @f]
 *
 * where:
 * - @f$ d @f$ = distance to object (meters)
 * - @f$ c @f$ = speed of sound in air ~ 343 m/s at 20degC
 * - @f$ t @f$ = echo pulse width (seconds)
 * - Division by 2 accounts for round-trip travel
 *
 * Converting to centimeters with microsecond timing:
 *
 * @f[
 *   d_{cm} = \frac{343 \text{ m/s} \times t_{\mu s} \times 10^{-6} \text{ s}}{2} \times 100 \text{ cm/m}
 * @f]
 *
 * Simplified:
 * @f[
 *   d_{cm} = \frac{t_{\mu s}}{58.0}
 * @f]
 *
 * **Performance Characteristics:**
 * - **Execution time:** Validation ~0.1us, delegation ~0.5us per call
 * - **Memory usage:** Zero heap allocation, ~200 bytes stack per call
 * - **Thread safety:** Not thread-safe - caller must provide synchronization
 * - **Re-entrancy:** Not reentrant - uses static mock hardware state
 *
 * @par Hardware Requirements:
 * This is a mock implementation with no actual hardware requirements.
 * It simulates the HC-SR04 GPIO interface for testing purposes.
 *
 * | Resource | Requirement | Notes |
 * |----------|-------------|-------|
 * | CPU | Any host CPU | No real-time requirements |
 * | RAM | ~1KB static | Mock hardware state |
 * | GPIO | Simulated | No actual pins used |
 * | Timing | Simulated | Mock microsecond timer |
 *
 * @par Module Dependencies:
 * - `mock_hcsr04_hw`: Mock hardware simulation (test infrastructure)
 * - `rx_check`: Assertion and validation macros
 * - `rx_hcsr04`: HC-SR04 driver constants and types
 * - `rx_hcsr04_hal`: HAL interface definition
 * - `rx_log`: Logging infrastructure for warnings
 * - `rx_port_utils`: Port/pin extraction utilities
 *
 * @dot
 * digraph module_dependencies {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   mock_hal [label="mock_rx_hcsr04_hal.c\n(This Module)", fillcolor=lightblue, style=filled];
 *   hal_interface [label="rx_hcsr04_hal.h\nInterface", shape=ellipse];
 *   driver [label="rx_hcsr04.c\nDriver"];
 *   mock_hw [label="mock_hcsr04_hw\nTest Infrastructure", fillcolor=lightgray, style=filled];
 *
 *   driver -> hal_interface [label="uses"];
 *   mock_hal -> hal_interface [label="implements"];
 *   mock_hal -> mock_hw [label="delegates to"];
 * }
 * @enddot
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | No goto, setjmp, recursion |
 * | 2. Fixed loop bounds | [OK] | All loops use enum constants for bounds |
 * | 3. No dynamic memory | [OK] | Zero malloc/free, stack-only allocation |
 * | 4. Functions <=60 lines | [OK] | Longest: `hcsr04_hal_delay_us` (31 lines) |
 * | 5. Min 2 assertions/func | [OK] | All functions validate inputs |
 * | 6. Smallest scope | [OK] | Variables declared at first use |
 * | 7. Check return values | [OK] | All mock function returns checked |
 * | 8. Limit preprocessor | [OK] | Only C23 typed enums, no macros |
 * | 9. Restrict pointers | [OK] | Max one level of dereferencing |
 * | 10. Compiler warnings | [OK] | Compiles with -Wall -Wextra -Werror |
 *
 * @par SOLID Principles:
 *
 * **Interface Segregation (I):**
 * - Implements focused HAL interface with only required functions
 * - No "fat interface" - each function has single responsibility
 * - Callers only depend on methods they actually use
 *
 * **Dependency Inversion (D):**
 * - High-level driver (`rx_hcsr04.c`) depends on `rx_hcsr04_hal.h` abstraction
 * - This mock provides test-side implementation without driver modification
 * - Real hardware HAL (`rx_hcsr04_hal.c`) swapped at link time
 * - Enables comprehensive unit testing without hardware dependency
 *
 * @see rx_hcsr04_hal.h HAL interface definition
 * @see rx_hcsr04.c HC-SR04 driver implementation using this HAL
 * @see mock_hcsr04_hw.h Mock hardware simulation functions
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @version 1.0.0
 *
 * @since Version 1.0.0
 */

#include "mock_hcsr04_hw.h"
#include "rx_check.h"
#include "rx_hcsr04.h"
#include "rx_hcsr04_hal.h"
#include "rx_log.h"

/**
 * @enum hcsr04_hal_mock_constants_t
 * @brief HAL mock constants for HC-SR04 module timing and logging
 *
 * @details
 * Defines constants used by the mock HAL implementation for delay validation,
 * log message formatting, and buffer management. These constants ensure
 * safe operation of snprintf formatting and mock timing validation.
 *
 * @par Value Derivation:
 * - **k_hcsr04_delay_max_us:** Matches HC-SR04 echo timeout from driver
 * - **k_hcsr04_log_msg_max:** Sized for typical warning messages with numbers
 * - **k_hcsr04_log_null_terminator:** Standard C string terminator size
 * - **k_hcsr04_message_len_init:** Zero initialization for message length
 *
 * @see rx_hcsr04.h for k_hcsr04_echo_timeout_us definition
 * @see hcsr04_hal_delay_us() Uses k_hcsr04_delay_max_us for validation
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**< @brief No delay requested - immediate return
   *
   * @details
   * Used to skip delay operation when zero microseconds is requested.
   * Avoids unnecessary mock timer access.
   *
   * @par Valid Usage:
   * Pass to hcsr04_hal_delay_us() to perform no operation.
   */
  k_hcsr04_delay_none = 0,

  /**< @brief Maximum supported delay in microseconds
   *
   * @details
   * Maximum delay duration enforced by the mock HAL. Matches the HC-SR04
   * echo timeout value (38000us) to ensure tests don't exceed realistic
   * measurement times. Delays beyond this threshold trigger a warning.
   *
   * @par Rationale:
   * HC-SR04 measurements never exceed 38ms (6.5m max range @ 343 m/s).
   * Tests requesting longer delays likely have logic errors.
   *
   * @par Value:
   * 38000 us (38 ms) - matches k_hcsr04_echo_timeout_us
   *
   * @see rx_hcsr04.h for k_hcsr04_echo_timeout_us
   */
  k_hcsr04_delay_max_us = k_hcsr04_echo_timeout_us,

  /**< @brief Maximum log message buffer size in bytes
   *
   * @details
   * Size of stack-allocated buffer for formatting warning messages in
   * hcsr04_hal_delay_us(). Sized to hold typical warning with numeric values.
   *
   * @par Value:
   * 96 bytes - sufficient for "Delay request %lu exceeds max %lu us"
   *
   * @par Rationale:
   * Longest message template with two uint32_t values (max 10 digits each)
   * plus descriptive text fits within 96 bytes including null terminator.
   */
  k_hcsr04_log_msg_max = 96,

  /**< @brief Null terminator size for C strings
   *
   * @details
   * Size of null terminator byte for C string calculations. Used when
   * computing maximum message length after snprintf.
   *
   * @par Value:
   * 1 byte
   */
  k_hcsr04_log_null_terminator = 1,

  /**< @brief Initial message length value
   *
   * @details
   * Initial value for message_len variable before snprintf result is processed.
   * Zero indicates no message content yet.
   *
   * @par Value:
   * 0
   */
  k_hcsr04_message_len_init = 0,
} hcsr04_hal_mock_constants_t;

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @brief Validate GPIO pin parameter against RX72N port/pin constraints
 *
 * @details
 * Validates that the provided pin value represents a legitimate RX72N GPIO pin.
 * Checks both the port portion (upper bits) and pin portion (lower bits) against
 * hardware limits.
 *
 * **Algorithm:**
 * 1. Extract pin number from lower bits using k_port_mask
 * 2. Extract port number from upper bits using k_port_shift
 * 3. Assert pin <= k_rx_pin_max (7)
 * 4. Assert port <= k_rx_port_j (10)
 * 5. Compare combined value against k_rx_pj_7 (highest valid pin)
 *
 * **Port/Pin Encoding:**
 * RX72N pins are encoded as: `(port << k_port_shift) | pin`
 * - Port: Bits [15:8] - valid range 0-10 (PORT0-PORTG, PORTJ)
 * - Pin: Bits [7:0] - valid range 0-7
 *
 * @param[in] pin GPIO pin identifier to validate
 *   - Type: rx_port_pin_t (enum containing port/pin encoding)
 *   - Valid range: k_rx_p0_0 to k_rx_pj_7
 *   - Example: k_rx_p3_5 = PORT3, pin 5
 *
 * @return Validation result
 * @retval true Pin is valid RX72N GPIO
 * @retval false Pin exceeds hardware limits
 *
 * @pre None - pure validation function
 *
 * @post No state changes - read-only validation
 * @post Return value reflects pin validity
 *
 * @note This function uses RX_ASSERT for programming errors (should never fail in production)
 * @note Not thread-safe if RX_ASSERT modifies shared state (logging)
 * @note Re-entrant - uses only input parameter and constants
 *
 * @par Performance:
 * Execution time: ~5 CPU cycles (bit masks and comparison)
 * Memory: Zero allocation, ~16 bytes stack
 *
 * @par Example:
 * @code{.c}
 * rx_port_pin_t trigger_pin = k_rx_p3_5;  // PORT3, pin 5
 * if (internal_is_valid_pin(trigger_pin)) {
 *     // Pin is valid, proceed with GPIO operations
 * }
 *
 * rx_port_pin_t invalid = (rx_port_pin_t)0xFFFF;  // Invalid pin
 * if (!internal_is_valid_pin(invalid)) {
 *     // Pin validation failed
 * }
 * @endcode
 *
 * @see rx_port_utils.h Port/pin encoding constants
 * @see internal_validate_and_extract_pin() Uses this for validation
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 assertions (pin <= max, port <= max)
 * - Rule 6: [OK] Variables declared at smallest scope
 */
static inline bool internal_is_valid_pin(rx_port_pin_t pin)
{
  /* Lower bound checks omitted - unsigned types can't be < 0 (-Wtype-limits) */
  const uint16_t pin_portion  = (uint16_t)pin & k_port_mask;
  const uint16_t port_portion = (uint16_t)pin >> k_port_shift;

  RX_ASSERT(pin_portion <= k_rx_pin_max, "Pin portion must be <= k_rx_pin_max");
  RX_ASSERT(port_portion <= k_rx_port_j, "Port portion must be <= k_rx_port_j");
  return (bool)(pin <= k_rx_pj_7);
}

/**
 * @brief Validate GPIO pin and extract port/pin numbers for mock hardware calls
 *
 * @details
 * Comprehensive validation and extraction of port and pin numbers from the
 * RX72N pin encoding. This function performs three validation stages before
 * extracting the port/pin components needed by mock hardware functions.
 *
 * **Algorithm:**
 * 1. Validate pin encoding using internal_is_valid_pin()
 * 2. Check output pointers are non-NULL
 * 3. Extract port number (upper 8 bits) using rx_port_from_pin()
 * 4. Extract pin number (lower 8 bits) using rx_pin_from_pin()
 * 5. Validate port is in range [0-PORTG] or PORTJ
 * 6. Validate pin is in range [0-7]
 *
 * **Port Validation:**
 * RX72N has ports 0-6 (PORT0-PORTG) and port 10 (PORTJ).
 * Port 8-9 are reserved and invalid for GPIO.
 *
 * @param[in] pin GPIO pin identifier to validate and extract
 *   - Valid range: k_rx_p0_0 to k_rx_pj_7
 *   - Encoding: (port << 8) | pin
 *
 * @param[out] port Pointer to receive extracted port number
 *   - Range: [0-6] or 10 (PORT0-PORTG or PORTJ)
 *   - Must be non-NULL
 *   - Caller-allocated
 *
 * @param[out] pin_num Pointer to receive extracted pin number
 *   - Range: [0-7]
 *   - Must be non-NULL
 *   - Caller-allocated
 *
 * @return Validation and extraction status
 * @retval k_rx_ok Success, port and pin_num written
 * @retval k_rx_err_invalid_arg Pin encoding invalid (out of range)
 * @retval k_rx_err_invalid_arg Port or pin_num pointer is NULL
 * @retval k_rx_err_invalid_arg Extracted port out of valid range
 * @retval k_rx_err_invalid_arg Extracted pin number out of valid range
 *
 * @pre pin must be valid RX72N GPIO identifier
 * @pre port must point to valid uint8_t storage
 * @pre pin_num must point to valid uint8_t storage
 *
 * @post On success: *port and *pin_num contain extracted values
 * @post On failure: *port and *pin_num unchanged (not written)
 * @post Return value indicates success/failure
 *
 * @note Thread-safe if RX_ASSERT is thread-safe
 * @note Re-entrant - no shared state modification
 * @note Does not modify pin parameter
 *
 * @warning Caller must check return value before using extracted port/pin
 *
 * @par Performance:
 * Execution time: ~0.1us (bit operations and comparisons)
 * Memory: ~32 bytes stack
 *
 * @par Example:
 * @code{.c}
 * rx_port_pin_t trigger_pin = k_rx_p3_5;  // PORT3, pin 5
 * uint8_t port, pin_num;
 *
 * rx_err_t ret = internal_validate_and_extract_pin(trigger_pin, &port, &pin_num);
 * if (ret == k_rx_ok) {
 *     // port = 3, pin_num = 5
 *     mock_gpio_set_output(port, pin_num);
 * }
 * @endcode
 *
 * @par Error Handling Example:
 * @code{.c}
 * uint8_t port, pin_num;
 *
 * // NULL pointer error
 * rx_err_t ret = internal_validate_and_extract_pin(k_rx_p3_5, nullptr, &pin_num);
 * // Returns k_rx_err_invalid_arg
 *
 * // Invalid pin encoding
 * rx_port_pin_t invalid_pin = (rx_port_pin_t)0xFFFF;
 * ret = internal_validate_and_extract_pin(invalid_pin, &port, &pin_num);
 * // Returns k_rx_err_invalid_arg
 * @endcode
 *
 * @see internal_is_valid_pin() Pin encoding validation
 * @see rx_port_from_pin() Port extraction utility
 * @see rx_pin_from_pin() Pin extraction utility
 * @see rx_port_utils.h Port/pin encoding definitions
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 4 preconditions checked (pin valid, port non-NULL, pin_num non-NULL, extracted values valid)
 * - Rule 7: [OK] All function return values checked
 */
static rx_err_t
internal_validate_and_extract_pin(rx_port_pin_t pin, uint8_t* port, uint8_t* pin_num)
{
  if (!internal_is_valid_pin(pin)) {
    return k_rx_err_invalid_arg;
  }

  if (port == nullptr || pin_num == nullptr) {
    return k_rx_err_invalid_arg;
  }

  *port    = rx_port_from_pin(pin);
  *pin_num = rx_pin_from_pin(pin);

  /* Validate port: must be k_rx_port_0 through k_rx_port_g, or k_rx_port_j */
  /* Lower bound check omitted - uint8_t can't be < 0 (-Wtype-limits) */
  if (*port > k_rx_port_g && *port != k_rx_port_j) {
    return k_rx_err_invalid_arg;
  }

  /* Validate pin: must be within k_rx_pin_min to k_rx_pin_max */
  /* Lower bound check omitted - uint8_t can't be < 0 (-Wtype-limits) */
  if (*pin_num > k_rx_pin_max) {
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/* =============================================================================
 * GPIO Functions
 * =============================================================================
 */

/**
 * @brief Configure GPIO pin as output for HC-SR04 TRIG signal
 *
 * @details
 * Configures the specified GPIO pin as an output to drive the HC-SR04 TRIG
 * line. This HAL function validates the pin encoding and delegates to the
 * mock hardware layer for actual configuration.
 *
 * **Algorithm:**
 * 1. Validate and extract port/pin numbers from encoded pin value
 * 2. Delegate to mock_gpio_set_output() with extracted values
 * 3. Return result from mock layer
 *
 * **HC-SR04 TRIG Pin Usage:**
 * The TRIG pin must be configured as output to generate the 10us trigger pulse
 * that initiates ultrasonic measurement. After this call, use
 * hcsr04_hal_gpio_write_high() and hcsr04_hal_gpio_write_low() to control.
 *
 * @param[in] pin GPIO pin to configure as output
 *   - Valid range: k_rx_p0_0 to k_rx_pj_7 (any valid RX72N GPIO)
 *   - Typical usage: k_rx_p3_5 for TRIG line
 *
 * @return Configuration result
 * @retval k_rx_ok Pin configured as output successfully
 * @retval k_rx_err_invalid_arg Pin encoding invalid (out of range)
 * @retval k_rx_err_invalid_arg Port out of valid range
 * @retval k_rx_err_invalid_arg Pin number out of valid range
 *
 * @pre pin must be valid RX72N GPIO identifier
 * @pre Pin must not be in use by other peripheral
 *
 * @post Pin configured as GPIO output
 * @post Pin can be controlled via write_high/write_low
 *
 * @note Not thread-safe - caller must serialize GPIO operations
 * @note Re-entrant if mock layer is reentrant
 *
 * @warning Must be called before attempting to write to pin
 * @warning Reconfiguring an in-use pin may disrupt other modules
 *
 * @par Performance:
 * Execution time: ~0.5us (validation + mock delegation)
 * Memory: ~32 bytes stack
 *
 * @par Example:
 * @code{.c}
 * // Configure P3.5 as output for HC-SR04 TRIG
 * rx_port_pin_t trig_pin = k_rx_p3_5;
 *
 * rx_err_t ret = hcsr04_hal_gpio_set_output(trig_pin);
 * if (ret != k_rx_ok) {
 *     // Handle error - invalid pin or mock failure
 *     return ret;
 * }
 *
 * // Now can write to TRIG pin
 * hcsr04_hal_gpio_write_high(trig_pin);
 * hcsr04_hal_delay_us(10);
 * hcsr04_hal_gpio_write_low(trig_pin);
 * @endcode
 *
 * @see hcsr04_hal_gpio_write_high() Set output pin HIGH
 * @see hcsr04_hal_gpio_write_low() Set output pin LOW
 * @see mock_gpio_set_output() Mock hardware implementation
 * @see rx_hcsr04.c Driver usage of TRIG pin
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 checks (pin validation, mock return)
 * - Rule 7: [OK] Validation return checked before delegation
 */
rx_err_t hcsr04_hal_gpio_set_output(rx_port_pin_t pin)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err     = internal_validate_and_extract_pin(pin, &port, &pin_num);

  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_set_output(port, pin_num);
}

/**
 * @brief Configure GPIO pin as input for HC-SR04 ECHO signal
 *
 * @details
 * Configures the specified GPIO pin as an input to receive the HC-SR04 ECHO
 * pulse. This HAL function validates the pin encoding and delegates to the
 * mock hardware layer for actual configuration.
 *
 * **Algorithm:**
 * 1. Validate and extract port/pin numbers from encoded pin value
 * 2. Delegate to mock_gpio_set_input() with extracted values
 * 3. Return result from mock layer
 *
 * **HC-SR04 ECHO Pin Usage:**
 * The ECHO pin must be configured as input to measure the pulse width that
 * indicates round-trip ultrasonic time. After configuration, use
 * hcsr04_hal_gpio_read() and hcsr04_hal_get_time_us() to measure pulse width.
 *
 * @param[in] pin GPIO pin to configure as input
 *   - Valid range: k_rx_p0_0 to k_rx_pj_7 (any valid RX72N GPIO)
 *   - Typical usage: k_rx_p3_4 for ECHO line
 *
 * @return Configuration result
 * @retval k_rx_ok Pin configured as input successfully
 * @retval k_rx_err_invalid_arg Pin encoding invalid (out of range)
 * @retval k_rx_err_invalid_arg Port out of valid range
 * @retval k_rx_err_invalid_arg Pin number out of valid range
 *
 * @pre pin must be valid RX72N GPIO identifier
 * @pre Pin must not be in use by other peripheral
 *
 * @post Pin configured as GPIO input
 * @post Pin can be read via hcsr04_hal_gpio_read()
 *
 * @note Not thread-safe - caller must serialize GPIO operations
 * @note Re-entrant if mock layer is reentrant
 *
 * @warning Must be called before attempting to read from pin
 *
 * @par Performance:
 * Execution time: ~0.5us (validation + mock delegation)
 * Memory: ~32 bytes stack
 *
 * @par Example:
 * @code{.c}
 * // Configure P3.4 as input for HC-SR04 ECHO
 * rx_port_pin_t echo_pin = k_rx_p3_4;
 *
 * rx_err_t ret = hcsr04_hal_gpio_set_input(echo_pin);
 * if (ret != k_rx_ok) {
 *     return ret;
 * }
 *
 * // Measure ECHO pulse width
 * bool echo_state;
 * uint32_t start_time, pulse_width_us;
 *
 * // Wait for rising edge
 * while (!hcsr04_hal_gpio_read(echo_pin, &echo_state) && !echo_state);
 * start_time = hcsr04_hal_get_time_us();
 *
 * // Wait for falling edge
 * while (hcsr04_hal_gpio_read(echo_pin, &echo_state) && echo_state);
 * pulse_width_us = hcsr04_hal_get_time_us() - start_time;
 *
 * float distance_cm = pulse_width_us / 58.0f;
 * @endcode
 *
 * @see hcsr04_hal_gpio_read() Read input pin state
 * @see hcsr04_hal_get_time_us() Get current time for pulse measurement
 * @see mock_gpio_set_input() Mock hardware implementation
 * @see rx_hcsr04.c Driver usage of ECHO pin
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 checks (pin validation, mock return)
 * - Rule 7: [OK] Validation return checked before delegation
 */
rx_err_t hcsr04_hal_gpio_set_input(rx_port_pin_t pin)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err     = internal_validate_and_extract_pin(pin, &port, &pin_num);

  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_set_input(port, pin_num);
}

/**
 * @brief Set GPIO pin HIGH for HC-SR04 trigger pulse generation
 *
 * @details
 * Drives the specified GPIO pin to logic HIGH (3.3V). Used to generate the
 * 10us trigger pulse required by the HC-SR04 ultrasonic sensor.
 *
 * **Algorithm:**
 * 1. Validate and extract port/pin numbers
 * 2. Delegate to mock_gpio_write_high() to set pin HIGH
 * 3. Return result from mock layer
 *
 * **HC-SR04 Trigger Sequence:**
 * To initiate measurement, drive TRIG HIGH for 10us:
 * ```
 * hcsr04_hal_gpio_write_high(trig_pin);   // Start pulse
 * hcsr04_hal_delay_us(10);                // 10us pulse width
 * hcsr04_hal_gpio_write_low(trig_pin);    // End pulse
 * ```
 *
 * @param[in] pin GPIO pin to drive HIGH
 *   - Valid range: k_rx_p0_0 to k_rx_pj_7
 *   - Must be previously configured as output via hcsr04_hal_gpio_set_output()
 *
 * @return Write operation result
 * @retval k_rx_ok Pin driven HIGH successfully
 * @retval k_rx_err_invalid_arg Pin encoding invalid
 *
 * @pre Pin must be configured as output
 * @pre Pin must be valid RX72N GPIO identifier
 *
 * @post Pin output driven to logic HIGH (3.3V)
 * @post TRIG line ready to initiate HC-SR04 measurement
 *
 * @note Not thread-safe - caller must serialize access
 * @note Re-entrant if mock layer is reentrant
 *
 * @warning Pin must be configured as output first
 *
 * @par Performance:
 * Execution time: ~0.5us (validation + mock delegation)
 * Memory: ~32 bytes stack
 *
 * @par Example:
 * @code{.c}
 * // Generate 10us trigger pulse
 * rx_err_t ret = hcsr04_hal_gpio_write_high(trig_pin);
 * if (ret != k_rx_ok) return ret;
 *
 * hcsr04_hal_delay_us(10);
 * hcsr04_hal_gpio_write_low(trig_pin);
 * @endcode
 *
 * @see hcsr04_hal_gpio_write_low() Drive pin LOW
 * @see hcsr04_hal_gpio_set_output() Configure pin as output
 * @see hcsr04_hal_delay_us() Delay for pulse width
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 checks (validation, delegation)
 * - Rule 7: [OK] All returns checked
 */
rx_err_t hcsr04_hal_gpio_write_high(rx_port_pin_t pin)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err     = internal_validate_and_extract_pin(pin, &port, &pin_num);

  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_write_high(port, pin_num);
}

/**
 * @brief Set GPIO pin LOW to complete HC-SR04 trigger pulse
 *
 * @details
 * Drives the specified GPIO pin to logic LOW (0V). Used to complete the
 * HC-SR04 trigger pulse and return TRIG line to idle state.
 *
 * **Algorithm:**
 * 1. Validate and extract port/pin numbers
 * 2. Delegate to mock_gpio_write_low() to set pin LOW
 * 3. Return result from mock layer
 *
 * **HC-SR04 Protocol:**
 * After the 10us HIGH pulse, TRIG must return to LOW to complete the trigger:
 * - LOW -> HIGH: Start trigger pulse
 * - Maintain HIGH for 10us
 * - HIGH -> LOW: Complete pulse, HC-SR04 begins measurement
 *
 * @param[in] pin GPIO pin to drive LOW
 *   - Valid range: k_rx_p0_0 to k_rx_pj_7
 *   - Must be previously configured as output
 *
 * @return Write operation result
 * @retval k_rx_ok Pin driven LOW successfully
 * @retval k_rx_err_invalid_arg Pin encoding invalid
 *
 * @pre Pin must be configured as output
 * @pre Pin must be valid RX72N GPIO identifier
 *
 * @post Pin output driven to logic LOW (0V)
 * @post HC-SR04 trigger pulse completed
 *
 * @note Not thread-safe - caller must serialize access
 * @note Re-entrant if mock layer is reentrant
 *
 * @warning Pin must be configured as output first
 *
 * @par Performance:
 * Execution time: ~0.5us (validation + mock delegation)
 * Memory: ~32 bytes stack
 *
 * @par Example:
 * @code{.c}
 * // Complete trigger pulse
 * hcsr04_hal_gpio_write_high(trig_pin);
 * hcsr04_hal_delay_us(10);
 * hcsr04_hal_gpio_write_low(trig_pin);  // End pulse
 *
 * // TRIG now LOW, HC-SR04 begins measurement
 * @endcode
 *
 * @see hcsr04_hal_gpio_write_high() Drive pin HIGH
 * @see hcsr04_hal_gpio_set_output() Configure pin as output
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 checks (validation, delegation)
 * - Rule 7: [OK] All returns checked
 */
rx_err_t hcsr04_hal_gpio_write_low(rx_port_pin_t pin)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err     = internal_validate_and_extract_pin(pin, &port, &pin_num);

  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_write_low(port, pin_num);
}

/**
 * @brief Read GPIO pin state for HC-SR04 ECHO pulse measurement
 *
 * @details
 * Reads the current logic level of the specified GPIO pin. Used to detect
 * rising and falling edges of the HC-SR04 ECHO pulse for distance measurement.
 *
 * **Algorithm:**
 * 1. Validate value pointer is non-NULL
 * 2. Validate and extract port/pin numbers
 * 3. Delegate to mock_gpio_read() to read pin state
 * 4. Return result with pin state in *value
 *
 * **HC-SR04 ECHO Measurement:**
 * The ECHO pin pulse width indicates round-trip ultrasonic travel time:
 * 1. Wait for ECHO rising edge (LOW -> HIGH)
 * 2. Record start time
 * 3. Wait for ECHO falling edge (HIGH -> LOW)
 * 4. Calculate pulse width = end_time - start_time
 * 5. Convert to distance: distance_cm = pulse_width_us / 58.0
 *
 * @param[in] pin GPIO pin to read
 *   - Valid range: k_rx_p0_0 to k_rx_pj_7
 *   - Must be previously configured as input via hcsr04_hal_gpio_set_input()
 *
 * @param[out] value Pointer to receive pin state
 *   - Must be non-NULL
 *   - Caller-allocated
 *   - Written with: true = logic HIGH (3.3V), false = logic LOW (0V)
 *
 * @return Read operation result
 * @retval k_rx_ok Pin read successfully, *value written
 * @retval k_rx_err_null_ptr value pointer is NULL
 * @retval k_rx_err_invalid_arg Pin encoding invalid
 *
 * @pre Pin must be configured as input
 * @pre Pin must be valid RX72N GPIO identifier
 * @pre value must point to valid bool storage
 *
 * @post On success: *value contains current pin state
 * @post On failure: *value unchanged
 *
 * @note Not thread-safe - caller must serialize access
 * @note Re-entrant if mock layer is reentrant
 *
 * @warning Pin must be configured as input first
 * @warning Check return value before using *value
 *
 * @par Performance:
 * Execution time: ~0.5us (validation + mock delegation)
 * Memory: ~32 bytes stack
 *
 * @par Example - ECHO Pulse Detection:
 * @code{.c}
 * bool echo_state;
 * uint32_t start_time, end_time, pulse_width_us;
 * rx_err_t ret;
 *
 * // Wait for rising edge (timeout after 1ms)
 * uint32_t timeout_start = hcsr04_hal_get_time_us();
 * do {
 *     ret = hcsr04_hal_gpio_read(echo_pin, &echo_state);
 *     if (ret != k_rx_ok) return ret;
 *     if (hcsr04_hal_get_time_us() - timeout_start > 1000) {
 *         return k_rx_err_timeout;  // No echo
 *     }
 * } while (!echo_state);
 * start_time = hcsr04_hal_get_time_us();
 *
 * // Wait for falling edge
 * do {
 *     ret = hcsr04_hal_gpio_read(echo_pin, &echo_state);
 *     if (ret != k_rx_ok) return ret;
 * } while (echo_state);
 * end_time = hcsr04_hal_get_time_us();
 *
 * pulse_width_us = end_time - start_time;
 * float distance_cm = pulse_width_us / 58.0f;
 * @endcode
 *
 * @see hcsr04_hal_gpio_set_input() Configure pin as input
 * @see hcsr04_hal_get_time_us() Get timestamp for pulse measurement
 * @see rx_hcsr04.c Driver implementation using ECHO pulse
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 3 checks (NULL check, validation, delegation)
 * - Rule 7: [OK] All returns checked
 */
rx_err_t hcsr04_hal_gpio_read(rx_port_pin_t pin, bool* value)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err;

  if (value == nullptr) {
    return k_rx_err_null_ptr;
  }

  err = internal_validate_and_extract_pin(pin, &port, &pin_num);
  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_read(port, pin_num, value);
}

/**
 * @brief Deinitialize GPIO pin after HC-SR04 operations complete
 *
 * @details
 * Deinitializes the specified GPIO pin, releasing it for use by other modules.
 * Typically called during HC-SR04 driver shutdown or reconfiguration.
 *
 * **Algorithm:**
 * 1. Validate and extract port/pin numbers
 * 2. Delegate to mock_gpio_deinit() to release pin
 * 3. Return result from mock layer
 *
 * **Cleanup Sequence:**
 * When shutting down HC-SR04 driver:
 * 1. Stop any active measurements
 * 2. Deinitialize ECHO pin (input)
 * 3. Deinitialize TRIG pin (output)
 * 4. Release any other resources
 *
 * @param[in] pin GPIO pin to deinitialize
 *   - Valid range: k_rx_p0_0 to k_rx_pj_7
 *   - Can be input or output configured pin
 *
 * @return Deinitialization result
 * @retval k_rx_ok Pin deinitialized successfully
 * @retval k_rx_err_invalid_arg Pin encoding invalid
 *
 * @pre Pin must be valid RX72N GPIO identifier
 *
 * @post Pin released and available for other uses
 * @post Pin no longer driven or monitored by HC-SR04 HAL
 *
 * @note Not thread-safe - caller must ensure no concurrent access
 * @note Re-entrant if mock layer is reentrant
 *
 * @warning Do not use pin for HC-SR04 operations after deinit
 *
 * @par Performance:
 * Execution time: ~0.5us (validation + mock delegation)
 * Memory: ~32 bytes stack
 *
 * @par Example:
 * @code{.c}
 * // Cleanup HC-SR04 pins during shutdown
 * rx_err_t ret;
 *
 * ret = hcsr04_hal_gpio_deinit(echo_pin);
 * if (ret != k_rx_ok) {
 *     // Log error but continue cleanup
 * }
 *
 * ret = hcsr04_hal_gpio_deinit(trig_pin);
 * if (ret != k_rx_ok) {
 *     // Log error
 * }
 * @endcode
 *
 * @see hcsr04_hal_gpio_set_input() Initialize pin as input
 * @see hcsr04_hal_gpio_set_output() Initialize pin as output
 * @see mock_gpio_deinit() Mock hardware implementation
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 checks (validation, delegation)
 * - Rule 7: [OK] All returns checked
 */
rx_err_t hcsr04_hal_gpio_deinit(rx_port_pin_t pin)
{
  uint8_t  port    = 0;
  uint8_t  pin_num = 0;
  rx_err_t err     = internal_validate_and_extract_pin(pin, &port, &pin_num);

  if (err != k_rx_ok) {
    return err;
  }

  return mock_gpio_deinit(port, pin_num);
}

/* =============================================================================
 * Timing Functions
 * =============================================================================
 */

/**
 * @brief Delay execution for precise HC-SR04 trigger pulse timing
 *
 * @details
 * Delays program execution by the specified number of microseconds. Used to
 * generate the precise 10us trigger pulse required by the HC-SR04 protocol.
 * This mock implementation validates delay duration and delegates to the
 * mock timer infrastructure.
 *
 * **Algorithm:**
 * 1. Check for zero delay - return immediately if us == 0
 * 2. Validate delay <= k_hcsr04_delay_max_us (38000us)
 * 3. If exceeded: Format warning message with snprintf
 * 4. Handle snprintf errors gracefully
 * 5. Log warning and return (no delay performed for invalid requests)
 * 6. If valid: Delegate to mock_delay_us() for simulated delay
 *
 * **HC-SR04 Timing Requirements:**
 *
 * | Signal | Duration | Tolerance | Purpose |
 * |--------|----------|-----------|---------|
 * | TRIG pulse | 10us | +/-1us | Initiate measurement |
 * | ECHO timeout | 38ms max | - | Max range (6.5m) |
 * | Measurement cycle | 60ms min | - | Module recovery time |
 *
 * @par Timing Diagram:
 * @msc
 * Firmware, TRIG, HC_SR04;
 *
 * --- [label="Trigger Pulse Generation"];
 * Firmware note Firmware [label="hcsr04_hal_gpio_write_high()"];
 * Firmware => TRIG [label="HIGH"];
 * TRIG note TRIG [label="10us pulse"];
 * Firmware note Firmware [label="hcsr04_hal_delay_us(10)"];
 * Firmware => TRIG [label="LOW"];
 * TRIG => HC_SR04 [label="trigger"];
 * HC_SR04 note HC_SR04 [label="begin measurement"];
 * @endmsc
 *
 * @param[in] us Delay duration in microseconds
 *   - Valid range: [0, k_hcsr04_delay_max_us]
 *   - Typical usage: 10 (trigger pulse width)
 *   - Special value: k_hcsr04_delay_none (0) = no operation
 *   - Invalid: > k_hcsr04_delay_max_us (38000) triggers warning, no delay
 *
 * @pre None - can be called anytime
 *
 * @post Execution delayed by approximately `us` microseconds (mock simulation)
 * @post If us > max: Warning logged, no delay performed
 *
 * @note Mock Behavior: This implementation simulates delay but provides no real-time guarantees
 * @note Thread Safety: Not thread-safe if logging functions are not thread-safe
 * @note Re-entrancy: Not reentrant - uses static buffer for message formatting
 *
 * @warning Values exceeding k_hcsr04_delay_max_us will not delay - test logic may be incorrect
 * @warning Mock delay does not consume real time - tests run at maximum speed
 *
 * @par Performance:
 * Execution time (valid delay): Mock dependent, typically instant
 * Execution time (invalid delay): ~10us (snprintf + logging)
 * Memory: 96 bytes stack (message buffer)
 *
 * @par Example - 10us Trigger Pulse:
 * @code{.c}
 * // Generate HC-SR04 trigger pulse
 * hcsr04_hal_gpio_write_high(trig_pin);
 * hcsr04_hal_delay_us(10);  // Precise 10us delay
 * hcsr04_hal_gpio_write_low(trig_pin);
 * // HC-SR04 now begins ultrasonic measurement
 * @endcode
 *
 * @par Example - Zero Delay (No-op):
 * @code{.c}
 * uint32_t delay_us = compute_delay();
 * hcsr04_hal_delay_us(delay_us);  // If delay_us == 0, returns immediately
 * @endcode
 *
 * @par Example - Invalid Delay (Warning):
 * @code{.c}
 * // This will trigger a warning - likely test logic error
 * hcsr04_hal_delay_us(50000);  // Exceeds 38000us maximum
 * // Warning logged: "Delay request 50000 exceeds max 38000 us"
 * // No delay performed
 * @endcode
 *
 * @see hcsr04_hal_get_time_us() Get current time for pulse measurement
 * @see mock_delay_us() Mock timer implementation
 * @see rx_hcsr04.c Driver usage for trigger pulse generation
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 3: [OK] No dynamic allocation - stack-only message buffer
 * - Rule 5: [OK] 2 checks (zero delay, max delay validation)
 * - Rule 7: [OK] snprintf return value checked for errors
 */
void hcsr04_hal_delay_us(uint32_t us)
{
  char     message[k_hcsr04_log_msg_max];
  uint32_t message_len = k_hcsr04_message_len_init;

  if (us == k_hcsr04_delay_none) {
    return;
  }

  if (us > k_hcsr04_delay_max_us) {
    /* Build message manually to avoid banned snprintf/memset */
    static const char s_delay_exceed_msg[] = "Delay request exceeds max us";
    message_len = sizeof(s_delay_exceed_msg) - k_hcsr04_log_null_terminator;
    for (uint32_t ci = 0; ci < message_len; ci++) {
      message[ci] = s_delay_exceed_msg[ci];
    }
    message[message_len] = '\0';
    rx_log_warn_str("HCSR04", "Delay request warning", message, message_len);
    return;
  }

  mock_delay_us(us);
}

/**
 * @brief Get current microsecond timestamp for HC-SR04 ECHO pulse measurement
 *
 * @details
 * Returns the current time in microseconds from the mock timer. Used to measure
 * the HC-SR04 ECHO pulse width by recording timestamps at rising and falling
 * edges. This mock implementation validates that test time values remain within
 * realistic bounds.
 *
 * **Algorithm:**
 * 1. Delegate to mock_get_time_us() to get current mock time
 * 2. Assert time_us != UINT32_MAX (invalid sentinel value)
 * 3. Assert time_us <= k_hcsr04_echo_timeout_us (realistic test bounds)
 * 4. Return validated timestamp
 *
 * **ECHO Pulse Width Measurement:**
 *
 * The HC-SR04 ECHO pulse width directly correlates to measured distance:
 *
 * @f[
 *   \text{pulse\_width}_{\mu s} = t_{\text{falling}} - t_{\text{rising}}
 * @f]
 *
 * @f[
 *   d_{cm} = \frac{\text{pulse\_width}_{\mu s}}{58.0}
 * @f]
 *
 * **Pulse Width to Distance Table:**
 *
 * | Distance (cm) | Pulse Width (us) | Round-trip (mm) |
 * |---------------|------------------|-----------------|
 * | 2 (min) | 116 | 4 mm |
 * | 10 | 580 | 20 mm |
 * | 30 | 1740 | 60 mm |
 * | 100 | 5800 | 200 mm |
 * | 200 | 11600 | 400 mm |
 * | 400 (max) | 23200 | 800 mm |
 *
 * @return Current microsecond timestamp
 *   - Range: [0, k_hcsr04_echo_timeout_us] in mock tests
 *   - Unit: microseconds (us)
 *   - Resolution: 1us (mock simulation)
 *   - Monotonic: Always increases in normal operation
 *
 * @pre Mock timer must be initialized
 *
 * @post Returns current mock time value
 * @post Time value within realistic test bounds
 * @post ASSERT fires if mock returns invalid sentinel (UINT32_MAX)
 * @post ASSERT fires if mock time exceeds timeout threshold
 *
 * @note Mock Timeout Behavior: This mock enforces stricter validation than real hardware
 * @note Tests must use mock_hcsr04_hw_set_timeout() to simulate timeouts
 * @note Do NOT advance mock time beyond k_hcsr04_echo_timeout_us - use timeout flag instead
 * @note Thread-safe if mock_get_time_us() is thread-safe
 * @note Re-entrant - read-only operation
 *
 * @warning Assertions will fire if mock timer is misconfigured
 * @warning Mock time > 38000us indicates test logic error
 *
 * @attention Test Infrastructure Note:
 * Tests simulating HC-SR04 timeout conditions should call:
 * ```c
 * mock_hcsr04_hw_set_timeout(true);  // Simulate timeout
 * ```
 * NOT:
 * ```c
 * mock_set_time_us(50000);  // WRONG - exceeds validation threshold
 * ```
 *
 * @par Performance:
 * Execution time: ~0.1us (mock timer read + 2 assertions)
 * Memory: ~16 bytes stack
 *
 * @par Example - Measure ECHO Pulse Width:
 * @code{.c}
 * bool echo_state;
 * uint32_t start_time, end_time, pulse_width_us;
 *
 * // Wait for rising edge
 * do {
 *     hcsr04_hal_gpio_read(echo_pin, &echo_state);
 * } while (!echo_state);
 * start_time = hcsr04_hal_get_time_us();  // Record start
 *
 * // Wait for falling edge
 * do {
 *     hcsr04_hal_gpio_read(echo_pin, &echo_state);
 * } while (echo_state);
 * end_time = hcsr04_hal_get_time_us();  // Record end
 *
 * // Calculate pulse width and distance
 * pulse_width_us = end_time - start_time;
 * float distance_cm = pulse_width_us / 58.0f;
 * @endcode
 *
 * @par Example - Timeout Detection:
 * @code{.c}
 * uint32_t start_time = hcsr04_hal_get_time_us();
 *
 * // Wait for ECHO with timeout
 * while (1) {
 *     uint32_t elapsed = hcsr04_hal_get_time_us() - start_time;
 *     if (elapsed > k_hcsr04_echo_timeout_us) {
 *         return k_rx_err_timeout;  // No object detected
 *     }
 *
 *     bool echo_state;
 *     hcsr04_hal_gpio_read(echo_pin, &echo_state);
 *     if (echo_state) {
 *         break;  // ECHO pulse started
 *     }
 * }
 * @endcode
 *
 * @see hcsr04_hal_delay_us() Delay for trigger pulse generation
 * @see hcsr04_hal_gpio_read() Read ECHO pin state
 * @see mock_get_time_us() Mock timer implementation
 * @see mock_hcsr04_hw_set_timeout() Simulate timeout for testing
 * @see rx_hcsr04.c Driver implementation using timestamps
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 post-condition assertions (not UINT32_MAX, within max timeout)
 * - Rule 7: [OK] Mock return value validated via assertions
 */
uint32_t hcsr04_hal_get_time_us(void)
{
  /* Post-condition 1: Mock time should not be invalid sentinel value */
  uint32_t time_us = mock_get_time_us();

  RX_ASSERT(time_us != UINT32_MAX, "Mock time returned invalid sentinel value");

  /* Post-condition 2: Mock time should be within reasonable test bounds */
  RX_ASSERT(time_us <= k_hcsr04_echo_timeout_us, "Mock time exceeds maximum timeout");

  return time_us;
}

/**
 * @brief Mock: ISR-safe timestamp (delegates to mock_get_time_us, no mutex)
 *
 * @details
 * Mock implementation of the ISR-safe time function. Delegates to
 * mock_get_time_us() exactly like hcsr04_hal_get_time_us() since there
 * is no mutex concern in the test environment. In real hardware this would
 * read the CMT2 counter directly without acquiring the ThreadX mutex.
 *
 * @return Current mock time in microseconds
 * @retval uint32_t Last value set via mock_set_time_us() (validated against
 *                  UINT32_MAX sentinel and k_hcsr04_echo_timeout_us bound)
 *
 * @pre Mock time infrastructure initialized (mock_set_time_us() called before use)
 * @pre Running in unit-test mock mode (no real CMT2 hardware)
 *
 * @post No hardware side-effects (no registers read, no mutex acquired)
 * @post Returned value equals last value set via mock_set_time_us()
 *
 * @note ISR-safe: no mutex or blocking ThreadX call in mock context
 * @note In real hardware, this function is safe to call from ISR context
 *
 * @par Example: ISR Edge Capture Simulation
 * @code
 * mock_set_time_us(1000);  // Rising edge at 1000 us
 * uint32_t start = hcsr04_hal_get_time_us_isr();
 * mock_set_time_us(3000);  // Falling edge at 3000 us (2 ms echo = ~34 cm)
 * uint32_t end = hcsr04_hal_get_time_us_isr();
 * uint32_t duration_us = end - start;  // = 2000 us ~ 34 cm
 * @endcode
 *
 * @see hcsr04_hal_get_time_us_isr() Real hardware counterpart (reads CMT2 directly)
 * @see mock_set_time_us() Sets the time value returned by this function
 *
 * @since Version 1.0.0
 */
uint32_t hcsr04_hal_get_time_us_isr(void)
{
  return mock_get_time_us();
}
