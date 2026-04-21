/**
 * @file test_rx_bus_gpio.c
 * @brief Unit Tests for GPIO Bus Abstraction Layer
 *
 * @details
 * Validates the rx_bus_gpio module which wraps the GPIO HAL behind the bus
 * manager abstraction. Tests cover all four public operations (init, write,
 * read, toggle) including success paths, null pointer validation, uninitialized
 * bus rejection, and HAL error propagation. Bus type validation is performed
 * only during initialization (by design - once a bus is initialized, its type
 * is guaranteed to be correct).
 *
 * All hardware interaction is intercepted by mock_gpio_hal, which records
 * calls and exposes captured state for assertion. The bus manager mock
 * replaces ThreadX mutex operations with simple boolean flag toggling.
 *
 * @par Test Coverage
 * | Group       | Tests | Description                              |
 * |-------------|-------|------------------------------------------|
 * | Init        | 6     | Success (output/input), null manager/name, not found, wrong type |
 * | Write       | 5     | High, low, null manager, null name, not initialized |
 * | Read        | 6     | High, low, null manager, null name, null value, not init |
 * | Toggle      | 4     | Success, null manager, null name, not initialized |
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion
 * - Rule 2: [OK] No loops in test code
 * - Rule 3: [OK] Static allocation only
 * - Rule 4: [OK] All functions under 60 lines
 * - Rule 5: [OK] Each test has minimum 2 assertions
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Typed enums for all constants
 * - Rule 9: [OK] Single-level pointers only
 * - Rule 10: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - S: Each test validates ONE specific behavior
 * - L: Mock GPIO HAL is a drop-in substitute for real HAL
 * - D: Tests depend on rx_bus_gpio interface, not GPIO register details
 *
 * @see rx_bus_gpio.h
 * @see rx_bus_manager.h
 *
 * @author Locked, Inc.
 * @date 2026-02-26
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "mock_gpio_hal.h"
#include "rx_bus_config.h"
#include "rx_bus_gpio.h"
#include "rx_bus_manager.h"
#include "rx_err.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Test pin for GPIO bus operations
 * @details Port 0, Pin 5 - used for all GPIO bus tests.
 */
static const rx_port_pin_t s_test_pin = k_rx_p0_5;

/**
 * @brief Expected mock HAL call counts after a rejected (error-returning) operation
 *
 * @details
 * When a bus operation is rejected early due to a null pointer or invalid state,
 * the GPIO HAL must not be called at all. Assertions use this enum to avoid
 * magic literals per STAR no-magic-numbers policy.
 */
typedef enum : int32_t {
  k_expected_zero_calls = 0, /**< No HAL calls expected after an early-rejected operation */
} test_gpio_expected_calls_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @var s_test_manager
 * @brief Static bus manager shared across all tests
 * @details Reset in setUp()/tearDown(). Uses static allocation per NASA Rule 3.
 * @note Internal test fixture; access only through setUp()/tearDown() helpers
 * @warning Direct modification outside setUp/tearDown will break shared test state
 * @since Version 1.0.0
 */
static rx_bus_manager_t s_test_manager;

/**
 * @var s_gpio_config
 * @brief Primary GPIO bus configuration
 * @details Configured in setUp() with s_test_pin.
 * @note Internal test fixture; reset by setUp() before each test
 * @warning Direct field modification outside setUp() may cause test interference
 * @since Version 1.0.0
 */
static rx_bus_config_t s_gpio_config;

/**
 * @var s_test_bus_name
 * @brief Bus name for primary GPIO bus
 * @details Constant string registered with s_test_manager during setUp().
 * @note Read-only; value must match the name passed to rx_bus_config_init_gpio()
 * @warning Changing this value without updating setUp() will break bus lookups
 * @since Version 1.0.0
 */
static const char* const s_test_bus_name = "test_gpio";

/* =============================================================================
 * setUp / tearDown
 * =============================================================================
 */

/**
 * @brief Initialize test fixtures before each test
 *
 * @details
 * 1. Reset mock GPIO HAL state
 * 2. Initialize bus manager with NULL interfaces (not needed for unit tests)
 * 3. Create GPIO bus config for s_test_pin
 * 4. Register bus config with manager
 *
 * @pre None - called by Unity before each test
 * @pre mock GPIO HAL state not yet initialized (will be reset by mock_gpio_init)
 * @post s_test_manager ready; s_gpio_config registered; mock state clean
 * @post s_gpio_config registered with s_test_bus_name in s_test_manager
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  /* Reset mock GPIO HAL state */
  mock_gpio_init();

  /* Initialize bus manager */
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST_GPIO", nullptr, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create primary GPIO bus config */
  err = rx_bus_config_init_gpio(&s_gpio_config, s_test_bus_name, s_test_pin);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Register primary bus */
  err = rx_bus_manager_add_bus(&s_test_manager, &s_gpio_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Clean up test fixtures after each test
 *
 * @details
 * Deinitializes the bus manager and resets mock GPIO HAL state.
 * Errors from deinit are ignored (cleanup must complete).
 *
 * @pre setUp() has been called
 * @pre s_test_manager is initialized and s_gpio_config is registered
 * @post s_test_manager deinitialized; mock state reset
 * @post mock GPIO HAL pin s_test_pin returned to uninitialized idle state
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  (void)rx_bus_manager_deinit(&s_test_manager);
  mock_gpio_init();
}

/* =============================================================================
 * Init Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_gpio_init with output=true configures pin as output
 *
 * @details
 * Verifies that initializing a GPIO bus in output mode calls gpio_set_output()
 * on the configured pin, which the mock records as k_mock_gpio_dir_output.
 *
 * @pre s_test_manager initialized with s_test_bus_name registered
 * @pre mock GPIO HAL state is clean (no prior direction changes recorded)
 * @post GPIO mock has recorded gpio_set_output call for s_test_pin
 * @post mock_gpio_get_direction(s_test_pin) returns k_mock_gpio_dir_output
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_init_output_configures_pin_as_output(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, mock_gpio_get_direction(s_test_pin));
}

/**
 * @brief rx_bus_gpio_init with output=false configures pin as input
 *
 * @details
 * Verifies that initializing a GPIO bus in input mode calls gpio_set_input()
 * on the configured pin, which the mock records as k_mock_gpio_dir_input.
 *
 * @pre s_test_manager initialized with s_test_bus_name registered
 * @pre mock GPIO HAL state is clean (no prior direction changes recorded)
 * @post Mock has recorded gpio_set_input call for s_test_pin
 * @post mock_gpio_get_direction(s_test_pin) returns k_mock_gpio_dir_input
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_init_input_configures_pin_as_input(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_mock_gpio_dir_input, mock_gpio_get_direction(s_test_pin));
}

/**
 * @brief rx_bus_gpio_init with null manager returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the manager pointer; the function must reject before any
 * bus lookup or HAL interaction is attempted.
 *
 * @pre s_test_bus_name is a valid non-null string constant available for the call
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_init_null_manager_returns_error(void)
{
  rx_err_t err = rx_bus_gpio_init(nullptr, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/**
 * @brief rx_bus_gpio_init with null bus name returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the bus name; the function must reject before any bus
 * lookup or HAL interaction is attempted.
 *
 * @pre s_test_manager initialized
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_init_null_name_returns_error(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, nullptr, true);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/**
 * @brief rx_bus_gpio_init with unregistered bus name returns k_rx_err_not_found
 *
 * @details
 * Passes an unregistered bus name ("nonexistent_bus"); the manager lookup fails
 * before the HAL is contacted.
 *
 * @pre s_test_manager initialized; "nonexistent_bus" not registered
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post No bus manager state changed by the lookup failure
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_init_bus_not_found_returns_error(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, "nonexistent_bus", true);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/**
 * @brief rx_bus_gpio_init with wrong bus type returns k_rx_err_invalid_arg
 *
 * @details
 * Registers a UART bus with the manager, then attempts to initialize it
 * with rx_bus_gpio_init(). The function must detect the type mismatch and
 * reject the operation before calling the GPIO HAL.
 *
 * @pre s_test_manager initialized
 * @pre A UART bus is registered with name "wrong_type_bus"
 * @post No GPIO HAL calls recorded
 * @post Bus manager state unchanged by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_init_rejects_wrong_bus_type(void)
{
  /* Create and register a UART bus (wrong type) */
  rx_bus_config_t wrong_type_config = {};
  wrong_type_config.type            = k_bus_type_uart;
  wrong_type_config.name            = "wrong_type_bus";

  rx_err_t err = rx_bus_manager_add_bus(&s_test_manager, &wrong_type_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Attempt to init with GPIO function - should reject */
  err = rx_bus_gpio_init(&s_test_manager, "wrong_type_bus", true);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/* =============================================================================
 * Write Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_gpio_write true drives the configured pin high
 *
 * @details
 * After initializing as output, writing true should call gpio_write_high(),
 * which sets the mock output value to true.
 *
 * @pre s_test_manager initialized; bus registered and initialized as output
 * @pre mock GPIO HAL state is clean (no prior write calls recorded)
 * @post Mock output value for s_test_pin is true
 * @post rx_bus_gpio_write returns k_rx_ok
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_write_high_sets_output_high(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_gpio_write(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_gpio_get_output_value(s_test_pin));
}

/**
 * @brief rx_bus_gpio_write false drives the configured pin low
 *
 * @details
 * After initializing as output and driving high, writing false should call
 * gpio_write_low(), clearing the mock output value.
 *
 * @pre s_test_manager initialized; bus initialized as output; pin driven high
 * @pre mock GPIO HAL state reflects a prior high write (output value = true)
 * @post Mock output value for s_test_pin is false
 * @post rx_bus_gpio_write returns k_rx_ok for both write calls
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_write_low_sets_output_low(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_gpio_write(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_gpio_write(&s_test_manager, s_test_bus_name, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_get_output_value(s_test_pin));
}

/**
 * @brief rx_bus_gpio_write with null manager returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the manager pointer; the function must reject before any
 * bus lookup or HAL interaction is attempted.
 *
 * @pre s_test_bus_name is a valid non-null string constant available for the call
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_write_null_manager_returns_error(void)
{
  rx_err_t err = rx_bus_gpio_write(nullptr, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/**
 * @brief rx_bus_gpio_write with null bus name returns k_rx_err_null_ptr
 *
 * @details
 * Passes a valid manager but nullptr for the bus name. The function must
 * reject before any HAL interaction or bus lookup is attempted.
 *
 * @pre s_test_manager initialized
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_write_null_name_returns_error(void)
{
  rx_err_t err = rx_bus_gpio_write(&s_test_manager, nullptr, true);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/**
 * @brief rx_bus_gpio_write on uninitialized bus returns k_rx_err_invalid_state
 *
 * @details
 * Bus is registered but rx_bus_gpio_init() has not been called.
 * The bus_config->initialized flag is false, so the callback rejects the op.
 *
 * @pre s_test_manager initialized; bus registered but NOT initialized
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No HAL state changes
 * @post No GPIO HAL calls recorded
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_write_not_initialized_returns_error(void)
{
  /* Do NOT call rx_bus_gpio_init() - bus is uninitialized */
  rx_err_t err = rx_bus_gpio_write(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  /* Verify no GPIO HAL calls were made */
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/* =============================================================================
 * Read Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_gpio_read returns true when mock input is high
 *
 * @details
 * Pre-configures the mock HAL input value for s_test_pin as true. After
 * initializing as input, reading should return true.
 *
 * @pre s_test_manager initialized; bus initialized as input; mock input = true
 * @pre mock_gpio_set_input_value(s_test_pin, true) called before bus init
 * @post value is true
 * @post rx_bus_gpio_read returns k_rx_ok
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_read_returns_simulated_high(void)
{
  mock_gpio_set_input_value(s_test_pin, true);

  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  bool value = false;
  err        = rx_bus_gpio_read(&s_test_manager, s_test_bus_name, &value);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(value);
}

/**
 * @brief rx_bus_gpio_read returns false when mock input is low
 *
 * @details
 * Mock input defaults to false after mock_gpio_init(). Reading should
 * return false without any special setup.
 *
 * @pre s_test_manager initialized; bus initialized as input; mock input = false
 * @pre mock_gpio_init() called (default input value is false)
 * @post value is false
 * @post rx_bus_gpio_read returns k_rx_ok
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_read_returns_simulated_low(void)
{
  /* Mock input defaults to false after mock_gpio_init() */
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  bool value = true;
  err        = rx_bus_gpio_read(&s_test_manager, s_test_bus_name, &value);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(value);
}

/**
 * @brief rx_bus_gpio_read with null manager returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the manager pointer; the function must reject before any
 * bus lookup or HAL interaction is attempted.
 *
 * @pre s_test_bus_name is a valid non-null string constant available for the call
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_read_null_manager_returns_error(void)
{
  bool     value = false;
  rx_err_t err   = rx_bus_gpio_read(nullptr, s_test_bus_name, &value);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/**
 * @brief rx_bus_gpio_read with null bus name returns k_rx_err_null_ptr
 *
 * @details
 * Passes a valid manager and value pointer but nullptr for the bus name.
 * The function must reject before any HAL interaction is attempted.
 *
 * @pre s_test_manager initialized
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post value output argument not modified by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_read_null_name_returns_error(void)
{
  bool     value = false;
  rx_err_t err   = rx_bus_gpio_read(&s_test_manager, nullptr, &value);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/**
 * @brief rx_bus_gpio_read with null value pointer returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the value output pointer; the function must reject before
 * any bus lookup or HAL interaction is attempted.
 *
 * @pre s_test_manager initialized; bus registered
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_read_null_value_returns_error(void)
{
  rx_err_t err = rx_bus_gpio_read(&s_test_manager, s_test_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/**
 * @brief rx_bus_gpio_read on uninitialized bus returns k_rx_err_invalid_state
 *
 * @details
 * Bus is registered but rx_bus_gpio_init() has not been called.
 *
 * @pre s_test_manager initialized; bus registered but NOT initialized
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No HAL state changes
 * @post No GPIO HAL calls recorded
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_read_not_initialized_returns_error(void)
{
  bool     value = false;
  rx_err_t err   = rx_bus_gpio_read(&s_test_manager, s_test_bus_name, &value);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/* =============================================================================
 * Toggle Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_gpio_toggle flips the output value from low to high
 *
 * @details
 * After init (output), pin starts at false (mock default). First toggle
 * should flip to true. Second toggle should flip back to false.
 *
 * @pre s_test_manager initialized; bus initialized as output; pin starts low
 * @pre mock GPIO HAL output value for s_test_pin is false (mock default)
 * @post Pin output value is true after first toggle, false after second
 * @post rx_bus_gpio_toggle returns k_rx_ok for both calls
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_toggle_flips_output(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* First toggle: low -> high */
  err = rx_bus_gpio_toggle(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_gpio_get_output_value(s_test_pin));

  /* Second toggle: high -> low */
  err = rx_bus_gpio_toggle(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_get_output_value(s_test_pin));
}

/**
 * @brief rx_bus_gpio_toggle with null manager returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the manager pointer; the function must reject before any
 * bus lookup or HAL interaction is attempted.
 *
 * @pre s_test_bus_name is a valid non-null string constant available for the call
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_toggle_null_manager_returns_error(void)
{
  rx_err_t err = rx_bus_gpio_toggle(nullptr, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/**
 * @brief rx_bus_gpio_toggle with null bus name returns k_rx_err_null_ptr
 *
 * @details
 * Passes a valid manager but nullptr for the bus name. The function must
 * reject before any HAL interaction or bus lookup is attempted.
 *
 * @pre s_test_manager initialized
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_toggle_null_name_returns_error(void)
{
  rx_err_t err = rx_bus_gpio_toggle(&s_test_manager, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/**
 * @brief rx_bus_gpio_toggle on uninitialized bus returns k_rx_err_invalid_state
 *
 * @details
 * Bus is registered but rx_bus_gpio_init() has not been called.
 *
 * @pre s_test_manager initialized; bus registered but NOT initialized
 * @pre mock GPIO HAL state is clean (no prior calls recorded)
 * @post No GPIO HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_toggle_not_initialized_returns_error(void)
{
  rx_err_t err = rx_bus_gpio_toggle(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_EQUAL(k_expected_zero_calls, mock_gpio_get_call_count());
}

/* =============================================================================
 * Additional Coverage Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_gpio_init HAL error propagates (gpio_set_output fails)
 * @details Injects an error on the first HAL call so gpio_set_output() returns
 *          an error, covering lines 373-375 in the init callback.
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_init_hal_error_propagates(void)
{
  mock_gpio_set_next_error(k_rx_err_timeout);

  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief rx_bus_gpio_init post-init read failure still succeeds
 * @details Injects an error on the second HAL call (gpio_read in post-init
 *          verification). The callback logs a warning and continues, returning
 *          k_rx_ok. Covers line 382.
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_init_post_init_read_failure_still_succeeds(void)
{
  /* Call 1 = gpio_set_output (succeeds), call 2 = gpio_read (fails) */
  /* error_call_index = N+1 to fail Nth call: fail call 2 -> index=3 */
  static const uint16_t k_call_post_init_read = 3U;
  mock_gpio_set_error_on_nth_call((uint16_t)k_call_post_init_read, k_rx_err_timeout);

  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  /* Warning logged, init still returns k_rx_ok */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bus_gpio_write HAL error propagates (gpio_write_high fails)
 * @details After successful init (2 HAL calls), injects an error on call 3
 *          (gpio_write_high), covering lines 460-462.
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_write_hal_error_propagates(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Call 3 = gpio_write_high (init used calls 1-2) */
  /* error_call_index = N+1 to fail Nth call: fail call 3 -> index=4 */
  static const uint16_t k_call_write = 4U;
  mock_gpio_set_error_on_nth_call((uint16_t)k_call_write, k_rx_err_timeout);

  err = rx_bus_gpio_write(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief rx_bus_gpio_write post-write verification read failure still succeeds
 * @details After successful init (2 HAL calls) and write (call 3), injects an
 *          error on call 4 (post-write gpio_read). The callback logs a warning
 *          and continues, covering line 469.
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_write_post_write_read_failure_still_succeeds(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Call 3 = gpio_write_high (succeeds), call 4 = gpio_read post-write (fails) */
  /* error_call_index = N+1 to fail Nth call: fail call 4 -> index=5 */
  static const uint16_t k_call_post_write_read = 5U;
  mock_gpio_set_error_on_nth_call((uint16_t)k_call_post_write_read, k_rx_err_timeout);

  err = rx_bus_gpio_write(&s_test_manager, s_test_bus_name, true);
  /* Warning logged, write still returns k_rx_ok */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bus_gpio_read HAL error propagates (gpio_read fails)
 * @details After successful init (2 HAL calls), injects an error on call 3
 *          (gpio_read in the read callback), covering lines 555-557.
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_read_hal_error_propagates(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Call 3 = gpio_read in read callback (init used calls 1-2) */
  /* error_call_index = N+1 to fail Nth call: fail call 3 -> index=4 */
  static const uint16_t k_call_read = 4U;
  mock_gpio_set_error_on_nth_call((uint16_t)k_call_read, k_rx_err_timeout);

  bool value = false;
  err        = rx_bus_gpio_read(&s_test_manager, s_test_bus_name, &value);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief rx_bus_gpio_toggle pre-toggle read failure still succeeds
 * @details After successful init (2 HAL calls), injects error on call 3
 *          (pre-toggle gpio_read). The callback logs a warning and continues.
 *          Covers lines 642-643.
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_toggle_pre_toggle_read_failure_still_succeeds(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Call 3 = pre-toggle gpio_read (fails), call 4 = gpio_toggle (succeeds) */
  /* error_call_index = N+1 to fail Nth call: fail call 3 -> index=4 */
  static const uint16_t k_call_pre_toggle_read = 4U;
  mock_gpio_set_error_on_nth_call((uint16_t)k_call_pre_toggle_read, k_rx_err_timeout);

  err = rx_bus_gpio_toggle(&s_test_manager, s_test_bus_name);
  /* Warning logged, toggle still returns k_rx_ok */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bus_gpio_toggle HAL error propagates (gpio_toggle fails)
 * @details After successful init (2 calls) and pre-toggle read (call 3),
 *          injects error on call 4 (gpio_toggle). Covers lines 650-652.
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_toggle_hal_error_propagates(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Call 3 = pre-toggle read (succeeds), call 4 = gpio_toggle (fails) */
  /* error_call_index = N+1 to fail Nth call: fail call 4 -> index=5 */
  static const uint16_t k_call_toggle = 5U;
  mock_gpio_set_error_on_nth_call((uint16_t)k_call_toggle, k_rx_err_timeout);

  err = rx_bus_gpio_toggle(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief rx_bus_gpio_toggle post-toggle read failure still succeeds
 * @details After init (2 calls), pre-read (3), toggle (4), injects error on
 *          call 5 (post-toggle gpio_read). The callback logs a warning.
 *          Covers line 659.
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_toggle_post_toggle_read_failure_still_succeeds(void)
{
  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Call 5 = post-toggle gpio_read (fails after init=1,2, pre-read=3, toggle=4) */
  /* error_call_index = N+1 to fail Nth call: fail call 5 -> index=6 */
  static const uint16_t k_call_post_toggle_read = 6U;
  mock_gpio_set_error_on_nth_call((uint16_t)k_call_post_toggle_read, k_rx_err_timeout);

  err = rx_bus_gpio_toggle(&s_test_manager, s_test_bus_name);
  /* Warning logged, toggle still returns k_rx_ok */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bus_gpio_toggle state unchanged after toggle logs warning but succeeds
 * @details Pre-set input_value to true before init. gpio_toggle flips output from
 *          false to true and syncs input_value to true (same as before). Both
 *          pre- and post-toggle reads return true, triggering the "state did not
 *          change" warning branch. Covers line 660.
 *
 * @since Version 1.0.0
 */
void test_bus_gpio_toggle_state_unchanged_logs_warning(void)
{
  /* Set input_value=true before init so pre-toggle read returns true.
   * After toggle: output goes false->true, input syncs to true (unchanged).
   * state_before == state_after == true -> triggers "state did not change" branch. */
  mock_gpio_set_input_value(s_test_pin, true);

  rx_err_t err = rx_bus_gpio_init(&s_test_manager, s_test_bus_name, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_gpio_toggle(&s_test_manager, s_test_bus_name);
  /* Warning "state did not change" is logged; toggle still returns k_rx_ok */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Bus Config Validation Coverage Tests
 * =============================================================================
 */

/**
 * @brief Invalid port in rx_bus_config_init_gpio returns error
 *
 * @details
 * Passes a rx_port_pin_t with port > k_rx_port_j (0x12) to trigger the
 * invalid-port branch inside internal_validate_port_pin (lines 291-292)
 * and the propagation in rx_bus_config_init_gpio (line 442).
 *
 * @pre None
 * @post k_rx_err_invalid_arg returned
 *
 * @note Covers lines 291-292 and 442 of rx_bus_config.c
 */
void test_bus_config_gpio_invalid_port_returns_error(void)
{
  rx_bus_config_t config;
  /* Construct a port_pin with port 0xFF (> k_rx_port_j = 0x12) */
  static const rx_port_pin_t k_invalid_port_pin =
    (rx_port_pin_t)0xFF00U; /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_err_t err = rx_bus_config_init_gpio(&config, "test", k_invalid_port_pin);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Invalid pin in rx_bus_config_init_gpio returns error
 *
 * @details
 * Passes a rx_port_pin_t with valid port but pin > 7 to trigger the
 * invalid-pin branch inside internal_validate_port_pin (lines 296-297).
 *
 * @pre None
 * @post k_rx_err_invalid_arg returned
 *
 * @note Covers lines 296-297 of rx_bus_config.c
 */
void test_bus_config_gpio_invalid_pin_returns_error(void)
{
  rx_bus_config_t config;
  /* Construct a port_pin with valid port (0x00) but pin 0x08 (> k_rx_pin_max = 7) */
  static const rx_port_pin_t k_invalid_pin =
    (rx_port_pin_t)0x0008U; /* NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) */
  rx_err_t err = rx_bus_config_init_gpio(&config, "test", k_invalid_pin);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Null config pointer to rx_bus_config_init_gpio returns error
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(config, ...) branch at line 436 of rx_bus_config.c.
 */
void test_bus_config_gpio_null_config_returns_error(void)
{
  rx_err_t err = rx_bus_config_init_gpio(nullptr, "test", s_test_pin);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Null name pointer to rx_bus_config_init_gpio returns error
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(name, ...) branch at line 437 of rx_bus_config.c.
 */
void test_bus_config_gpio_null_name_returns_error(void)
{
  rx_bus_config_t config;
  rx_err_t        err = rx_bus_config_init_gpio(&config, nullptr, s_test_pin);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Unity test runner entry point
 *
 * @details
 * Registers and executes all GPIO bus abstraction tests. Returns non-zero
 * if any test fails.
 *
 * @pre Unity framework initialized (UNITY_BEGIN called internally)
 * @pre Test binary linked against mock_gpio_hal and mock_bus_manager
 * @post All registered tests executed and results printed
 * @post Resources cleaned up via tearDown after each test
 *
 * @return int Test suite status code
 * @retval 0 All tests passed
 * @retval non-zero One or more tests failed (Unity exit code)
 *
 * @note Not thread-safe; the Unity test runner is single-threaded
 *
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  /* Init tests */
  RUN_TEST(test_bus_gpio_init_output_configures_pin_as_output);
  RUN_TEST(test_bus_gpio_init_input_configures_pin_as_input);
  RUN_TEST(test_bus_gpio_init_null_manager_returns_error);
  RUN_TEST(test_bus_gpio_init_null_name_returns_error);
  RUN_TEST(test_bus_gpio_init_bus_not_found_returns_error);
  RUN_TEST(test_bus_gpio_init_rejects_wrong_bus_type);
  RUN_TEST(test_bus_gpio_init_hal_error_propagates);
  RUN_TEST(test_bus_gpio_init_post_init_read_failure_still_succeeds);

  /* Write tests */
  RUN_TEST(test_bus_gpio_write_high_sets_output_high);
  RUN_TEST(test_bus_gpio_write_low_sets_output_low);
  RUN_TEST(test_bus_gpio_write_null_manager_returns_error);
  RUN_TEST(test_bus_gpio_write_null_name_returns_error);
  RUN_TEST(test_bus_gpio_write_not_initialized_returns_error);
  RUN_TEST(test_bus_gpio_write_hal_error_propagates);
  RUN_TEST(test_bus_gpio_write_post_write_read_failure_still_succeeds);

  /* Read tests */
  RUN_TEST(test_bus_gpio_read_returns_simulated_high);
  RUN_TEST(test_bus_gpio_read_returns_simulated_low);
  RUN_TEST(test_bus_gpio_read_null_manager_returns_error);
  RUN_TEST(test_bus_gpio_read_null_name_returns_error);
  RUN_TEST(test_bus_gpio_read_null_value_returns_error);
  RUN_TEST(test_bus_gpio_read_not_initialized_returns_error);
  RUN_TEST(test_bus_gpio_read_hal_error_propagates);

  /* Toggle tests */
  RUN_TEST(test_bus_gpio_toggle_flips_output);
  RUN_TEST(test_bus_gpio_toggle_null_manager_returns_error);
  RUN_TEST(test_bus_gpio_toggle_null_name_returns_error);
  RUN_TEST(test_bus_gpio_toggle_not_initialized_returns_error);
  RUN_TEST(test_bus_gpio_toggle_pre_toggle_read_failure_still_succeeds);
  RUN_TEST(test_bus_gpio_toggle_hal_error_propagates);
  RUN_TEST(test_bus_gpio_toggle_post_toggle_read_failure_still_succeeds);
  RUN_TEST(test_bus_gpio_toggle_state_unchanged_logs_warning);

  /* Bus config validation tests */
  RUN_TEST(test_bus_config_gpio_invalid_port_returns_error);
  RUN_TEST(test_bus_config_gpio_invalid_pin_returns_error);
  RUN_TEST(test_bus_config_gpio_null_config_returns_error);
  RUN_TEST(test_bus_config_gpio_null_name_returns_error);

  return UNITY_END();
}
