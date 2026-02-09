/* tests/mocks/mock_gpio_hal.h */

/**
 * @file mock_gpio_hal.h
 * @brief Mock GPIO HAL implementation for unit testing without hardware
 *
 * @details
 * Provides test double for GPIO Hardware Abstraction Layer to enable unit
 * testing of GPIO-dependent modules without actual RX72N hardware. Supports
 * comprehensive state tracking, error injection, and call history verification.
 *
 * This mock enables testing of:
 * - GPIO initialization sequences
 * - Pin direction configuration (input/output)
 * - Digital I/O operations (read/write/toggle)
 * - Error handling in GPIO-dependent code
 * - Call sequence validation
 *
 * @par Test Architecture:
 * @dot
 * digraph mock_gpio_arch {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_test {
 *     label="Unit Test Environment";
 *     style=filled;
 *     color=lightyellow;
 *     test [label="test_gpio_hal.c\nUnit Tests"];
 *   }
 *
 *   subgraph cluster_mock {
 *     label="Mock GPIO HAL (This Module)";
 *     style=filled;
 *     color=lightblue;
 *     state [label="g_mock_gpio\nGlobal State"];
 *     funcs [label="gpio_set_output()\ngpio_write_high()\ngpio_read()\netc."];
 *   }
 *
 *   subgraph cluster_real {
 *     label="Real GPIO HAL (Production)";
 *     style=filled;
 *     color=lightgreen;
 *     hw [label="Hardware Registers\nPORT0.PDR\nPORT0.PODR", style=dashed];
 *   }
 *
 *   test -> state [label="1. Configure mock"];
 *   test -> funcs [label="2. Call GPIO HAL"];
 *   funcs -> state [label="3. Update state"];
 *   test -> state [label="4. Verify state"];
 *
 *   hw [style=dashed, label="(Not accessed\nin tests)"];
 * }
 * @enddot
 *
 * @par Mock Capabilities:
 * | Feature                | Supported | Description |
 * |------------------------|-----------|-------------|
 * | Per-pin state tracking | Yes       | Tracks direction and value for all 160 pins |
 * | Error injection        | Yes       | Configurable error return on next call |
 * | Call history           | Yes       | Records last 64 function calls with arguments |
 * | Input simulation       | Yes       | Set simulated input values for gpio_read() |
 * | State inspection       | Yes       | Query pin direction and output value |
 * | Call verification      | Yes       | Verify call count and call sequence |
 *
 * @par Mock vs Real Implementation:
 * | Aspect              | Mock (This Module)          | Real (gpio_hal.c) |
 * |---------------------|-----------------------------|--------------------|
 * | Hardware access     | None - state in RAM         | Direct PORT register access |
 * | Timing              | Instant response            | Hardware delays (~50ns) |
 * | Error conditions    | Configurable via injection  | Actual hardware errors |
 * | State persistence   | Reset on mock_gpio_init()   | Persistent until power off |
 * | Thread safety       | Not thread-safe             | Not thread-safe (same as real) |
 *
 * @par Usage in Tests:
 * - tests/test_gpio_hal.c - Primary user: GPIO HAL unit tests
 *
 * @par Example Usage:
 * @code
 * #include "mock_gpio_hal.h"
 * #include "unity.h"
 *
 * void setUp(void) {
 *   mock_gpio_init();  // Reset state before each test
 * }
 *
 * void test_gpio_set_output_configures_pin_direction(void) {
 *   // Act: Configure pin as output
 *   rx_err_t err = gpio_set_output(RX_PIN(k_rx_port_0, k_rx_pin_5));
 *
 *   // Assert: Verify success
 *   TEST_ASSERT_EQUAL(k_rx_ok, err);
 *
 *   // Assert: Verify pin direction changed
 *   mock_gpio_direction_t dir = mock_gpio_get_direction(RX_PIN(k_rx_port_0, k_rx_pin_5));
 *   TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, dir);
 * }
 *
 * void test_gpio_write_high_error_handling(void) {
 *   // Arrange: Inject error
 *   mock_gpio_set_next_error(k_rx_err_gpio_invalid_pin);
 *
 *   // Act: Attempt to write
 *   rx_err_t err = gpio_write_high(RX_PIN(k_rx_port_0, k_rx_pin_5));
 *
 *   // Assert: Verify error returned
 *   TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_pin, err);
 * }
 *
 * void test_gpio_read_returns_simulated_value(void) {
 *   // Arrange: Set simulated input value
 *   rx_port_pin_t pin = RX_PIN(k_rx_port_0, k_rx_pin_3);
 *   mock_gpio_set_input_value(pin, true);
 *
 *   // Act: Read pin
 *   bool value;
 *   rx_err_t err = gpio_read(pin, &value);
 *
 *   // Assert: Verify simulated value returned
 *   TEST_ASSERT_EQUAL(k_rx_ok, err);
 *   TEST_ASSERT_TRUE(value);
 * }
 * @endcode
 *
 * @see gpio_hal.h Real GPIO HAL interface
 * @see tests/test_gpio_hal.c Test file using this mock
 * @see rx_port_constants.h Port and pin definitions
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: ✓ No goto, setjmp, or recursion
 * - Rule 2: ✓ All loops have fixed bounds (array iteration)
 * - Rule 3: ✓ Static allocation only (g_mock_gpio is global)
 * - Rule 4: ✓ Functions kept under 60 lines
 * - Rule 5: ✓ Input validation in all functions
 * - Rule 6: ✓ Variables declared at smallest scope
 * - Rule 7: ✓ All return values checked in implementation
 * - Rule 8: ✓ Preprocessor used only for include guards
 * - Rule 9: ✓ Single-level pointers only
 * - Rule 10: ✓ Code compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - S: Single Responsibility - Only provides GPIO HAL mock functionality
 * - O: Open/Closed - Extensible via error injection without modifying code
 * - L: Liskov Substitution - Drop-in replacement for real GPIO HAL
 * - I: Interface Segregation - Minimal focused interface matching real HAL
 * - D: Dependency Inversion - Tests depend on GPIO HAL interface, not implementation
 *
 * @author STAR Team
 * @date 2026-01-05
 * @version 1.0.0
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_GPIO_HAL_H
#define MOCK_GPIO_HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Mock GPIO constants */
typedef enum : uint8_t {
  k_mock_gpio_max_ports         = 20, /**< Maximum port numbers */
  k_mock_gpio_pins_per_port     = 8,  /**< Pins per port */
  k_mock_gpio_call_history_size = 64, /**< Call history buffer size */
} mock_gpio_constants_t;

/** @brief GPIO direction enum for mock tracking */
typedef enum : uint8_t {
  k_mock_gpio_dir_undefined = 0, /**< Direction not set */
  k_mock_gpio_dir_input     = 1, /**< Input direction */
  k_mock_gpio_dir_output    = 2, /**< Output direction */
} mock_gpio_direction_t;

/* =============================================================================
 * Types
 * =============================================================================
 */

/** @brief GPIO HAL function call types */
typedef enum : uint8_t {
  k_mock_gpio_call_set_output,
  k_mock_gpio_call_set_input,
  k_mock_gpio_call_write_high,
  k_mock_gpio_call_write_low,
  k_mock_gpio_call_toggle,
  k_mock_gpio_call_read,
} mock_gpio_call_type_t;

/** @brief GPIO HAL function call record */
typedef struct {
  mock_gpio_call_type_t type; /**< Call type */
  rx_port_pin_t         pin;  /**< Pin that was operated on */
} mock_gpio_call_t;

/** @brief Per-pin GPIO state */
typedef struct {
  mock_gpio_direction_t direction;    /**< Current direction */
  bool                  output_value; /**< Current output value */
  bool                  input_value;  /**< Simulated input value */
} mock_gpio_pin_state_t;

/** @brief Global mock GPIO state */
typedef struct {
  mock_gpio_pin_state_t pins[k_mock_gpio_max_ports][k_mock_gpio_pins_per_port]; /**< Pin states */
  mock_gpio_call_t      call_history[k_mock_gpio_call_history_size];            /**< Call history */
  uint16_t              call_count; /**< Number of calls recorded */
  rx_err_t              next_error; /**< Error to return on next call */
  bool                  error_set;  /**< Whether error injection is active */
} mock_gpio_state_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

/** @brief Global mock GPIO state instance */
extern mock_gpio_state_t g_mock_gpio;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock GPIO state to default values
 *
 * @details
 * Resets all pin states to undefined direction with low values, clears
 * call history, and disables error injection. Must be called in setUp()
 * before each test to ensure clean state.
 *
 * State reset includes:
 * - All 160 pins set to undefined direction
 * - All output values set to false (low)
 * - All input values set to false (low)
 * - Call history cleared (call_count = 0)
 * - Error injection disabled
 *
 * @par Example:
 * @code
 * void setUp(void) {
 *   mock_gpio_init();  // Clean state before each test
 * }
 * @endcode
 *
 * @see mock_gpio_reset() Alias for this function
 *
 * @since Version 1.0.0
 */
void mock_gpio_init(void);

/**
 * @brief Reset mock GPIO state (alias for mock_gpio_init)
 *
 * @details
 * Convenience function that calls mock_gpio_init(). Provided for
 * consistency with other mock modules that use "reset" terminology.
 *
 * @see mock_gpio_init() Primary initialization function
 *
 * @since Version 1.0.0
 */
void mock_gpio_reset(void);

/* =============================================================================
 * Test Setup Functions
 * =============================================================================
 */

/**
 * @brief Set simulated input value for a pin
 *
 * @details
 * Configures the value that will be returned when gpio_read() is called
 * for the specified pin. This simulates external hardware driving the pin
 * high or low. The value persists until changed or mock is reset.
 *
 * @param[in] pin GPIO pin (created via RX_PIN macro)
 * @param[in] value Simulated input value (true=high/3.3V, false=low/0V)
 *
 * @par Example:
 * @code
 * // Simulate button press (active low)
 * rx_port_pin_t button_pin = RX_PIN(k_rx_port_0, k_rx_pin_3);
 * mock_gpio_set_input_value(button_pin, false);  // Pressed
 *
 * bool pressed;
 * gpio_read(button_pin, &pressed);
 * TEST_ASSERT_FALSE(pressed);  // Button is pressed (active low)
 * @endcode
 *
 * @note Does not validate pin number. Invalid pins are silently ignored.
 *
 * @see gpio_read() Function that returns this simulated value
 *
 * @since Version 1.0.0
 */
void mock_gpio_set_input_value(rx_port_pin_t pin, bool value);

/**
 * @brief Set error to return on next GPIO HAL call
 *
 * @details
 * Configures the mock to return the specified error code on the next
 * call to any GPIO HAL function (gpio_set_output, gpio_write_high, etc.).
 * Error is consumed after one call (single-shot injection).
 *
 * Used to test error handling code paths without requiring actual
 * hardware failures.
 *
 * @param[in] err Error code to return (e.g., k_rx_err_gpio_invalid_pin)
 *
 * @par Example:
 * @code
 * // Test error handling
 * mock_gpio_set_next_error(k_rx_err_gpio_invalid_pin);
 * rx_err_t err = gpio_write_high(some_pin);
 * TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_pin, err);
 *
 * // Next call succeeds (error was consumed)
 * err = gpio_write_high(some_pin);
 * TEST_ASSERT_EQUAL(k_rx_ok, err);
 * @endcode
 *
 * @warning Error is consumed after ONE call. Set again for repeated errors.
 *
 * @see mock_gpio_clear_error() Disable pending error injection
 *
 * @since Version 1.0.0
 */
void mock_gpio_set_next_error(rx_err_t err);

/**
 * @brief Clear any pending error injection
 *
 * @details
 * Disables error injection set by mock_gpio_set_next_error(). Subsequent
 * GPIO HAL calls will return success (or actual validation errors).
 *
 * Typically used in tearDown() or when abandoning an error injection test.
 *
 * @par Example:
 * @code
 * mock_gpio_set_next_error(k_rx_err_timeout);
 * // Test changed, don't want error anymore
 * mock_gpio_clear_error();
 *
 * rx_err_t err = gpio_read(pin, &value);
 * TEST_ASSERT_EQUAL(k_rx_ok, err);  // No error injected
 * @endcode
 *
 * @see mock_gpio_set_next_error() Set error injection
 *
 * @since Version 1.0.0
 */
void mock_gpio_clear_error(void);

/* =============================================================================
 * State Inspection Functions
 * =============================================================================
 */

/**
 * @brief Get pin direction configuration
 *
 * @details
 * Returns the current direction configuration for the specified pin.
 * Used in tests to verify that gpio_set_output() and gpio_set_input()
 * correctly configured the pin direction.
 *
 * @param[in] pin GPIO pin (created via RX_PIN macro)
 *
 * @return Pin direction enum value
 * @retval k_mock_gpio_dir_undefined Direction not yet configured
 * @retval k_mock_gpio_dir_input Configured as input (gpio_set_input called)
 * @retval k_mock_gpio_dir_output Configured as output (gpio_set_output called)
 *
 * @par Example:
 * @code
 * rx_port_pin_t led_pin = RX_PIN(k_rx_port_0, k_rx_pin_5);
 * gpio_set_output(led_pin);
 *
 * mock_gpio_direction_t dir = mock_gpio_get_direction(led_pin);
 * TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, dir);
 * @endcode
 *
 * @note Returns k_mock_gpio_dir_undefined for invalid pins
 *
 * @see gpio_set_output() Function that sets direction to output
 * @see gpio_set_input() Function that sets direction to input
 *
 * @since Version 1.0.0
 */
mock_gpio_direction_t mock_gpio_get_direction(rx_port_pin_t pin);

/**
 * @brief Get pin output value
 *
 * @details
 * Returns the current output value written to the specified pin.
 * Used in tests to verify that gpio_write_high(), gpio_write_low(),
 * and gpio_toggle() correctly updated the pin output state.
 *
 * This is the value that would be driven on the physical pin if this
 * were real hardware (assuming pin is configured as output).
 *
 * @param[in] pin GPIO pin (created via RX_PIN macro)
 *
 * @return Output value
 * @retval true Pin is driven high (3.3V)
 * @retval false Pin is driven low (0V)
 *
 * @par Example:
 * @code
 * rx_port_pin_t led_pin = RX_PIN(k_rx_port_0, k_rx_pin_5);
 * gpio_write_high(led_pin);
 *
 * bool value = mock_gpio_get_output_value(led_pin);
 * TEST_ASSERT_TRUE(value);  // LED should be on
 * @endcode
 *
 * @note Returns false for invalid pins
 * @note This returns OUTPUT value, not input. Use gpio_read() for inputs.
 *
 * @see gpio_write_high() Function that sets output high
 * @see gpio_write_low() Function that sets output low
 * @see gpio_toggle() Function that toggles output
 *
 * @since Version 1.0.0
 */
bool mock_gpio_get_output_value(rx_port_pin_t pin);

/* =============================================================================
 * Call History Functions
 * =============================================================================
 */

/**
 * @brief Get call history entry by index
 *
 * @details
 * Retrieves a specific call record from the call history buffer.
 * Used to verify call sequences and arguments in tests.
 *
 * Call history records:
 * - Function type (set_output, write_high, read, etc.)
 * - Pin argument
 *
 * History is stored in chronological order (index 0 = oldest call).
 *
 * @param[in] index Index in call history (0 to call_count-1)
 *
 * @return Pointer to call record, or NULL if index out of range
 *
 * @par Example:
 * @code
 * gpio_write_high(RX_PIN(k_rx_port_0, k_rx_pin_5));
 * gpio_write_low(RX_PIN(k_rx_port_0, k_rx_pin_3));
 *
 * const mock_gpio_call_t* call0 = mock_gpio_get_call(0);
 * TEST_ASSERT_EQUAL(k_mock_gpio_call_write_high, call0->type);
 * TEST_ASSERT_EQUAL(RX_PIN(k_rx_port_0, k_rx_pin_5), call0->pin);
 *
 * const mock_gpio_call_t* call1 = mock_gpio_get_call(1);
 * TEST_ASSERT_EQUAL(k_mock_gpio_call_write_low, call1->type);
 * @endcode
 *
 * @note Maximum 64 calls are recorded (k_mock_gpio_call_history_size)
 * @note Returns NULL for invalid index
 *
 * @see mock_gpio_get_call_count() Get total number of calls
 *
 * @since Version 1.0.0
 */
const mock_gpio_call_t* mock_gpio_get_call(uint16_t index);

/**
 * @brief Get total number of recorded calls
 *
 * @details
 * Returns the number of GPIO HAL function calls that have been recorded
 * in the call history. Used to verify expected call counts.
 *
 * @return Number of calls in history (0 to k_mock_gpio_call_history_size)
 *
 * @par Example:
 * @code
 * gpio_write_high(RX_PIN(k_rx_port_0, k_rx_pin_5));
 * gpio_write_low(RX_PIN(k_rx_port_0, k_rx_pin_3));
 * gpio_read(RX_PIN(k_rx_port_1, k_rx_pin_0), &value);
 *
 * uint16_t count = mock_gpio_get_call_count();
 * TEST_ASSERT_EQUAL(3, count);
 * @endcode
 *
 * @note Call count saturates at k_mock_gpio_call_history_size (64)
 *
 * @see mock_gpio_get_call() Get individual call records
 * @see mock_gpio_clear_history() Reset call count to zero
 *
 * @since Version 1.0.0
 */
uint16_t mock_gpio_get_call_count(void);

/**
 * @brief Clear call history buffer
 *
 * @details
 * Resets call count to zero without clearing the actual call records.
 * Subsequent calls to mock_gpio_get_call_count() will return 0 until
 * new GPIO HAL functions are called.
 *
 * Typically used when you want to verify calls that occur AFTER a
 * certain point in the test.
 *
 * @par Example:
 * @code
 * // Do some setup that calls GPIO functions
 * some_init_function();
 *
 * // Clear history, we only care about calls from now on
 * mock_gpio_clear_history();
 *
 * // Now test the actual functionality
 * some_function_under_test();
 *
 * // Verify only the calls made by function under test
 * TEST_ASSERT_EQUAL(2, mock_gpio_get_call_count());
 * @endcode
 *
 * @note Does not reset pin states or error injection settings
 * @note Use mock_gpio_init() for full state reset
 *
 * @see mock_gpio_get_call_count() Get call count
 * @see mock_gpio_init() Full state reset including history
 *
 * @since Version 1.0.0
 */
void mock_gpio_clear_history(void);

/* =============================================================================
 * GPIO HAL Function Declarations (Mock Implementations)
 * =============================================================================
 */

/**
 * @brief Configure GPIO pin as output
 *
 * @details
 * Mock implementation that sets pin direction to output in mock state.
 * Records call in history and supports error injection.
 *
 * Mock behavior:
 * - Validates port and pin number
 * - Updates g_mock_gpio.pins[port][pin].direction to k_mock_gpio_dir_output
 * - Records call with type k_mock_gpio_call_set_output
 * - Returns injected error if mock_gpio_set_next_error() was called
 *
 * @param[in] pin GPIO pin to configure (use RX_PIN macro)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, pin configured as output
 * @retval k_rx_err_gpio_invalid_port Invalid port number
 * @retval k_rx_err_gpio_invalid_pin Invalid pin number (>7)
 * @retval (injected error) If error injection active
 *
 * @see mock_gpio_get_direction() Verify direction was set
 * @see mock_gpio_set_next_error() Inject error for testing
 *
 * @since Version 1.0.0
 */
rx_err_t gpio_set_output(rx_port_pin_t pin);

/**
 * @brief Configure GPIO pin as input
 *
 * @details
 * Mock implementation that sets pin direction to input in mock state.
 * Records call in history and supports error injection.
 *
 * Mock behavior:
 * - Validates port and pin number
 * - Updates g_mock_gpio.pins[port][pin].direction to k_mock_gpio_dir_input
 * - Records call with type k_mock_gpio_call_set_input
 * - Returns injected error if mock_gpio_set_next_error() was called
 *
 * @param[in] pin GPIO pin to configure (use RX_PIN macro)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, pin configured as input
 * @retval k_rx_err_gpio_invalid_port Invalid port number
 * @retval k_rx_err_gpio_invalid_pin Invalid pin number (>7)
 * @retval (injected error) If error injection active
 *
 * @see mock_gpio_get_direction() Verify direction was set
 * @see mock_gpio_set_next_error() Inject error for testing
 *
 * @since Version 1.0.0
 */
rx_err_t gpio_set_input(rx_port_pin_t pin);

/**
 * @brief Write GPIO output pin high (3.3V)
 *
 * @details
 * Mock implementation that sets output value to true (high) in mock state.
 * Records call in history and supports error injection.
 *
 * Mock behavior:
 * - Validates port and pin number
 * - Updates g_mock_gpio.pins[port][pin].output_value to true
 * - Records call with type k_mock_gpio_call_write_high
 * - Returns injected error if mock_gpio_set_next_error() was called
 *
 * @param[in] pin GPIO pin to write (use RX_PIN macro)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, pin driven high
 * @retval k_rx_err_gpio_invalid_port Invalid port number
 * @retval k_rx_err_gpio_invalid_pin Invalid pin number (>7)
 * @retval (injected error) If error injection active
 *
 * @note Does not check if pin is configured as output (unlike real HAL)
 *
 * @see mock_gpio_get_output_value() Verify output was set high
 * @see gpio_write_low() Set output low
 * @see gpio_toggle() Toggle output value
 *
 * @since Version 1.0.0
 */
rx_err_t gpio_write_high(rx_port_pin_t pin);

/**
 * @brief Write GPIO output pin low (0V)
 *
 * @details
 * Mock implementation that sets output value to false (low) in mock state.
 * Records call in history and supports error injection.
 *
 * Mock behavior:
 * - Validates port and pin number
 * - Updates g_mock_gpio.pins[port][pin].output_value to false
 * - Records call with type k_mock_gpio_call_write_low
 * - Returns injected error if mock_gpio_set_next_error() was called
 *
 * @param[in] pin GPIO pin to write (use RX_PIN macro)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, pin driven low
 * @retval k_rx_err_gpio_invalid_port Invalid port number
 * @retval k_rx_err_gpio_invalid_pin Invalid pin number (>7)
 * @retval (injected error) If error injection active
 *
 * @note Does not check if pin is configured as output (unlike real HAL)
 *
 * @see mock_gpio_get_output_value() Verify output was set low
 * @see gpio_write_high() Set output high
 * @see gpio_toggle() Toggle output value
 *
 * @since Version 1.0.0
 */
rx_err_t gpio_write_low(rx_port_pin_t pin);

/**
 * @brief Toggle GPIO output pin value
 *
 * @details
 * Mock implementation that flips the output value in mock state
 * (true→false, false→true). Records call in history and supports
 * error injection.
 *
 * Mock behavior:
 * - Validates port and pin number
 * - Flips g_mock_gpio.pins[port][pin].output_value
 * - Records call with type k_mock_gpio_call_toggle
 * - Returns injected error if mock_gpio_set_next_error() was called
 *
 * @param[in] pin GPIO pin to toggle (use RX_PIN macro)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, pin toggled
 * @retval k_rx_err_gpio_invalid_port Invalid port number
 * @retval k_rx_err_gpio_invalid_pin Invalid pin number (>7)
 * @retval (injected error) If error injection active
 *
 * @note Does not check if pin is configured as output (unlike real HAL)
 *
 * @see mock_gpio_get_output_value() Verify toggle occurred
 *
 * @since Version 1.0.0
 */
rx_err_t gpio_toggle(rx_port_pin_t pin);

/**
 * @brief Read GPIO input pin value
 *
 * @details
 * Mock implementation that returns the simulated input value from mock
 * state. The returned value is set by mock_gpio_set_input_value().
 * Records call in history and supports error injection.
 *
 * Mock behavior:
 * - Validates port and pin number
 * - Validates value pointer is not NULL
 * - Reads g_mock_gpio.pins[port][pin].input_value
 * - Records call with type k_mock_gpio_call_read
 * - Returns injected error if mock_gpio_set_next_error() was called
 *
 * @param[in] pin GPIO pin to read (use RX_PIN macro)
 * @param[out] value Pointer to store read value (true=high, false=low)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, value written to *value
 * @retval k_rx_err_null_ptr value pointer is NULL
 * @retval k_rx_err_gpio_invalid_port Invalid port number
 * @retval k_rx_err_gpio_invalid_pin Invalid pin number (>7)
 * @retval (injected error) If error injection active
 *
 * @note Reads input_value, not output_value
 * @note Input value defaults to false until set via mock_gpio_set_input_value()
 *
 * @see mock_gpio_set_input_value() Set simulated input value
 *
 * @since Version 1.0.0
 */
rx_err_t gpio_read(rx_port_pin_t pin, bool* value);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_GPIO_HAL_H */
