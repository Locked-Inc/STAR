/**
 * @file test_rx_bus_adc.c
 * @brief Unit Tests for ADC Bus Abstraction Layer
 *
 * @details
 * Validates the rx_bus_adc module which wraps the ADC HAL behind the bus
 * manager abstraction. Tests cover all three public operations (init, read,
 * read_voltage_mv) including success paths, null pointer validation,
 * uninitialized bus rejection, bus not found, and HAL error propagation.
 *
 * All hardware interaction is intercepted by mock_adc_hal, which records
 * calls and exposes captured state for assertion. The bus manager mock
 * replaces ThreadX mutex operations with simple boolean flag toggling.
 *
 * @par Test Coverage
 * | Group         | Tests | Description                                       |
 * |---------------|-------|---------------------------------------------------|
 * | Init          | 5     | Success, null manager/name, not found, HAL error  |
 * | Read raw      | 5     | Success, null params, not init, HAL error         |
 * | Read voltage  | 4     | Success, null params, not init                    |
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
 * - L: Mock ADC HAL is a drop-in substitute for real HAL
 * - D: Tests depend on rx_bus_adc interface, not register-level ADC details
 *
 * @author Locked, Inc.
 * @date 2026-02-26
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "mock_adc_hal.h"
#include "rx_bus_adc.h"
#include "rx_bus_config.h"
#include "rx_bus_manager.h"
#include "rx_err.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @enum test_adc_constants_t
 * @brief ADC test configuration constants
 *
 * @details
 * Sized to match ADC0, channel 0, 12-bit - the most common STAR configuration
 * for analog sensor monitoring that feeds into bus abstraction tests.
 * These constants drive setUp() and all test fixture initialization to avoid
 * magic literals per STAR no-magic-numbers policy.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_adc_unit    = 0,  /**< S12AD unit 0 (ADC0) */
  k_test_adc_channel = 0,  /**< Channel AN000 */
  k_test_adc_bits    = 12, /**< 12-bit resolution (0-4095) */
} test_adc_constants_t;

/**
 * @enum test_adc_values_t
 * @brief Raw ADC value constants used in read tests
 *
 * @details
 * Covers zero, mid-scale, and full-scale raw counts for a 12-bit ADC
 * (range 0-4095). Used to exercise rx_bus_adc_read() and
 * rx_bus_adc_read_voltage_mv() with representative inputs,
 * avoiding magic literals per STAR no-magic-numbers policy.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_test_adc_value_zero = 0,    /**< Zero raw value (0V) */
  k_test_adc_value_mid  = 2048, /**< Mid-scale raw value */
  k_test_adc_value_max  = 4095, /**< Full-scale raw value (3.3V) */
} test_adc_values_t;

/**
 * @enum test_adc_expected_calls_t
 * @brief Expected mock HAL call counts after a rejected (error-returning) operation
 *
 * @details
 * When a bus operation is rejected early due to a null pointer or invalid state,
 * the ADC HAL must not be called at all. Assertions use this enum to avoid
 * magic literals per STAR no-magic-numbers policy.
 *
 * @since Version 1.0.0
 */
typedef enum : int32_t {
  k_expected_zero_adc_calls = 0, /**< No HAL calls expected after an early-rejected operation */
} test_adc_expected_calls_t;

/**
 * @enum test_adc_voltage_thresholds_t
 * @brief Voltage comparison thresholds for read_voltage_mv assertions
 *
 * @details
 * Named constant for the minimum acceptable voltage in greater-than comparisons.
 * Avoids the magic literal 0 in TEST_ASSERT_GREATER_THAN per STAR policy.
 *
 * @since Version 1.0.0
 */
typedef enum : int32_t {
  k_voltage_threshold_mv = 0, /**< Minimum expected voltage (mV) for a nonzero ADC reading */
} test_adc_voltage_thresholds_t;

/**
 * @enum test_adc_voltage_init_t
 * @brief Zero initializer constant for voltage output variables
 *
 * @details
 * Named constant for initializing uint32_t voltage_mv variables to zero.
 * Avoids the magic literal 0 in variable initializers per STAR policy.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_test_voltage_zero = 0U, /**< Zero voltage initializer (mV) */
} test_adc_voltage_init_t;

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
 * @var s_adc_config
 * @brief ADC bus configuration for the test device
 * @details Configured in setUp() with unit=0, channel=0, bits=12.
 * @note Internal test fixture; reset by setUp() before each test
 * @warning Direct field modification outside setUp() may cause test interference
 * @since Version 1.0.0
 */
static rx_bus_config_t s_adc_config;

/**
 * @var s_test_bus_name
 * @brief Bus name used for all ADC bus lookups in tests
 * @details Constant string registered with s_test_manager during setUp().
 * @note Read-only; value must match the name passed to rx_bus_config_init_adc()
 * @warning Changing this value without updating setUp() will break bus lookups
 * @since Version 1.0.0
 */
static const char* const s_test_bus_name = "test_adc";

/* =============================================================================
 * setUp / tearDown
 * =============================================================================
 */

/**
 * @brief Initialize test fixtures before each test
 *
 * @details
 * 1. Reset mock ADC HAL state
 * 2. Initialize bus manager
 * 3. Create ADC bus config (unit 0, channel 0, 12-bit)
 * 4. Register bus config with manager
 *
 * @pre None - called by Unity before each test
 * @pre mock ADC HAL state not yet initialized (will be reset by mock_adc_init)
 * @post s_test_manager ready; s_adc_config registered; mock state clean
 * @post s_adc_config registered with s_test_bus_name in s_test_manager
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  /* Reset mock ADC HAL state */
  mock_adc_init();

  /* Initialize bus manager */
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST_ADC", nullptr, nullptr);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create ADC bus config: unit=0, channel=0, 12-bit resolution */
  err = rx_bus_config_init_adc(&s_adc_config,
                               s_test_bus_name,
                               (uint8_t)k_test_adc_unit,
                               (uint8_t)k_test_adc_channel,
                               (uint8_t)k_test_adc_bits);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Register ADC bus with manager */
  err = rx_bus_manager_add_bus(&s_test_manager, &s_adc_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Clean up test fixtures after each test
 *
 * @details
 * Deinitializes the bus manager and resets mock ADC HAL state.
 * Errors from deinit are ignored (cleanup must complete).
 *
 * @pre setUp() has been called
 * @pre s_test_manager is initialized and s_adc_config is registered
 * @post s_test_manager deinitialized; mock state reset
 * @post mock ADC HAL unit 0 returned to uninitialized idle state
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  (void)rx_bus_manager_deinit(&s_test_manager);
  mock_adc_init();
}

/* =============================================================================
 * Init Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_adc_init marks ADC unit as initialized in mock
 *
 * @details
 * Successful init drives adc_init() via the callback; the mock records this
 * and mock_adc_is_initialized() confirms unit 0 is ready.
 *
 * @pre s_test_manager initialized with s_test_bus_name registered
 * @pre mock ADC HAL state is clean (adc_init() not yet called)
 * @post mock_adc_is_initialized(0) returns true
 * @post rx_bus_adc_init returns k_rx_ok
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_adc_init_marks_unit_initialized(void)
{
  rx_err_t err = rx_bus_adc_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_adc_is_initialized((uint8_t)k_test_adc_unit));
}

/**
 * @brief rx_bus_adc_init with null manager returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the manager pointer; the function must reject before any
 * bus lookup or HAL interaction is attempted.
 *
 * @pre None
 * @pre mock ADC HAL state is clean (no prior calls recorded)
 * @post No ADC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
*/
void test_bus_adc_init_null_manager_returns_error(void)
{
  rx_err_t err = rx_bus_adc_init(nullptr, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_FALSE(mock_adc_is_initialized((uint8_t)k_test_adc_unit));
}

/**
 * @brief rx_bus_adc_init with null bus name returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the bus name; the function must reject before any bus
 * lookup or HAL interaction is attempted.
 *
 * @pre s_test_manager initialized
 * @pre mock ADC HAL state is clean (no prior calls recorded)
 * @post No ADC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
*/
void test_bus_adc_init_null_name_returns_error(void)
{
  rx_err_t err = rx_bus_adc_init(&s_test_manager, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_FALSE(mock_adc_is_initialized((uint8_t)k_test_adc_unit));
}

/**
 * @brief rx_bus_adc_init with unregistered bus name returns k_rx_err_not_found
 *
 * @details
 * Passes an unregistered bus name ("unknown_adc"); the manager lookup fails
 * before the HAL is contacted.
 *
 * @pre s_test_manager initialized; "unknown_adc" not registered
 * @pre mock ADC HAL state is clean (no prior calls recorded)
 * @post No ADC HAL calls recorded
 * @post No bus manager state changed by the lookup failure
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
*/
void test_bus_adc_init_bus_not_found_returns_error(void)
{
  rx_err_t err = rx_bus_adc_init(&s_test_manager, "unknown_adc");
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
  TEST_ASSERT_FALSE(mock_adc_is_initialized((uint8_t)k_test_adc_unit));
}

/**
 * @brief rx_bus_adc_init propagates HAL error when adc_init fails
 *
 * @details
 * Injects a timeout error via mock_adc_set_next_error(). The internal callback
 * calls adc_init() which returns the injected error, which propagates upward.
 * Verifies the exact error code (k_rx_err_timeout) is preserved, not just
 * that any error occurred.
 *
 * @pre s_test_manager initialized; error injection configured
 * @pre mock_adc_set_next_error(k_rx_err_timeout) called before rx_bus_adc_init
 * @post mock_adc_is_initialized(0) remains false (init was rejected by HAL)
 * @post returned error equals k_rx_err_timeout (injected value preserved)
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 * @since Version 1.0.0
 */
void test_bus_adc_init_hal_error_propagates(void)
{
  mock_adc_set_next_error(k_rx_err_timeout);

  rx_err_t err = rx_bus_adc_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  TEST_ASSERT_FALSE(mock_adc_is_initialized((uint8_t)k_test_adc_unit));
}

/* =============================================================================
 * Read Raw Value Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_adc_read returns configured mock value
 *
 * @details
 * Sets a known raw ADC value in the mock, then verifies rx_bus_adc_read()
 * returns exactly that value through the bus abstraction.
 *
 * @pre s_test_manager initialized; bus initialized; mock value set to mid-scale
 * @pre mock_adc_set_value called with k_test_adc_value_mid before bus init
 * @post value == k_test_adc_value_mid
 * @post rx_bus_adc_read returns k_rx_ok
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 * @since Version 1.0.0
 */
void test_bus_adc_read_returns_configured_value(void)
{
  mock_adc_set_value((uint8_t)k_test_adc_unit,
                     (uint8_t)k_test_adc_channel,
                     (uint16_t)k_test_adc_value_mid);

  rx_err_t err = rx_bus_adc_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint16_t value = (uint16_t)k_test_adc_value_zero;
  err            = rx_bus_adc_read(&s_test_manager, s_test_bus_name, &value);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL((uint16_t)k_test_adc_value_mid, value);
}

/**
 * @brief rx_bus_adc_read with null manager returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the manager pointer; the function must reject before any
 * bus lookup or HAL interaction is attempted.
 *
 * @pre None
 * @pre mock ADC HAL state is clean (no prior calls recorded)
 * @post No ADC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
*/
void test_bus_adc_read_null_manager_returns_error(void)
{
  uint16_t value = (uint16_t)k_test_adc_value_zero;
  rx_err_t err   = rx_bus_adc_read(nullptr, s_test_bus_name, &value);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_adc_calls, mock_adc_get_call_count());
}

/**
 * @brief rx_bus_adc_read with null value pointer returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the output value pointer; the function must reject before
 * the bus lookup or HAL interaction is attempted.
 *
 * @pre s_test_manager initialized; bus registered
 * @pre mock ADC HAL state is clean (no prior calls recorded)
 * @post No ADC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
*/
void test_bus_adc_read_null_value_returns_error(void)
{
  rx_err_t err = rx_bus_adc_read(&s_test_manager, s_test_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_adc_calls, mock_adc_get_call_count());
}

/**
 * @brief rx_bus_adc_read with null bus_name returns k_rx_err_null_ptr
 *
 * @since Version 1.0.0
 */
void test_bus_adc_read_null_bus_name_returns_error(void)
{
  uint16_t value = (uint16_t)k_test_adc_value_zero;
  rx_err_t err   = rx_bus_adc_read(&s_test_manager, nullptr, &value);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_adc_calls, mock_adc_get_call_count());
}

/**
 * @brief rx_bus_adc_read on uninitialized bus returns k_rx_err_invalid_state
 *
 * @details
 * Bus is registered but rx_bus_adc_init() has not been called.
 * The bus_config->initialized flag is false, so the callback rejects the op.
 *
 * @pre s_test_manager initialized; bus registered but NOT initialized
 * @pre mock ADC HAL state is clean (no prior calls recorded)
 * @post No HAL calls recorded
 * @post rx_bus_adc_read returns k_rx_err_invalid_state
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_adc_read_not_initialized_returns_error(void)
{
  uint16_t value = (uint16_t)k_test_adc_value_zero;
  rx_err_t err   = rx_bus_adc_read(&s_test_manager, s_test_bus_name, &value);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_EQUAL(k_expected_zero_adc_calls, mock_adc_get_call_count());
}

/**
 * @brief rx_bus_adc_read propagates HAL error from adc_read
 *
 * @details
 * After a successful init, injects an error before reading. The internal
 * callback calls adc_read() which returns the injected error. Verifies the
 * exact error code (k_rx_err_timeout) is preserved, not just that any error
 * occurred.
 *
 * @pre s_test_manager initialized; bus initialized; error injection set
 * @pre mock_adc_set_next_error(k_rx_err_timeout) called after successful init
 * @post returned error equals k_rx_err_timeout (injected value preserved)
 * @post value output argument not modified by the failing read
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 * @since Version 1.0.0
 */
void test_bus_adc_read_hal_error_propagates(void)
{
  rx_err_t err = rx_bus_adc_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_adc_set_next_error(k_rx_err_timeout);

  uint16_t value = (uint16_t)k_test_adc_value_mid;
  err            = rx_bus_adc_read(&s_test_manager, s_test_bus_name, &value);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
  TEST_ASSERT_EQUAL((uint16_t)k_test_adc_value_mid, value);
}

/* =============================================================================
 * Read Voltage Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_adc_read_voltage_mv returns positive voltage for nonzero raw
 *
 * @details
 * Sets full-scale raw ADC value (4095 for 12-bit), calls read_voltage_mv,
 * and verifies the returned voltage is nonzero. The exact value depends on
 * the mock's reference voltage scaling (typically 3300 mV at full scale).
 *
 * @pre s_test_manager initialized; bus initialized; raw value = 4095
 * @pre mock_adc_set_value called with k_test_adc_value_max before bus init
 * @post voltage_mv > 0
 * @post rx_bus_adc_read_voltage_mv returns k_rx_ok
 *
 * @note Relies on the mock reference voltage scaling (3300 mV at full scale)
 *
 * @since Version 1.0.0
 */
void test_bus_adc_read_voltage_returns_nonzero_for_full_scale(void)
{
  mock_adc_set_value((uint8_t)k_test_adc_unit,
                     (uint8_t)k_test_adc_channel,
                     (uint16_t)k_test_adc_value_max);

  rx_err_t err = rx_bus_adc_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint32_t voltage_mv = (uint32_t)k_test_voltage_zero;
  err                 = rx_bus_adc_read_voltage_mv(&s_test_manager, s_test_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN((int32_t)k_voltage_threshold_mv, (int32_t)voltage_mv);
}

/**
 * @brief rx_bus_adc_read_voltage_mv with null manager returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the manager pointer; the function must reject before any
 * bus lookup or HAL interaction is attempted.
 *
 * @pre None
 * @pre mock ADC HAL state is clean (no prior calls recorded)
 * @post No ADC HAL calls recorded
 * @post voltage_mv output argument not modified by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
*/
void test_bus_adc_read_voltage_null_manager_returns_error(void)
{
  uint32_t voltage_mv = (uint32_t)k_test_voltage_zero;
  rx_err_t err        = rx_bus_adc_read_voltage_mv(nullptr, s_test_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_adc_calls, mock_adc_get_call_count());
}

/**
 * @brief rx_bus_adc_read_voltage_mv with null output pointer returns k_rx_err_null_ptr
 *
 * @details
 * Passes nullptr as the voltage_mv output pointer; the function must reject
 * before the bus lookup or HAL interaction is attempted.
 *
 * @pre s_test_manager initialized; bus registered
 * @pre mock ADC HAL state is clean (no prior calls recorded)
 * @post No ADC HAL calls recorded
 * @post No bus manager state changed by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
*/
void test_bus_adc_read_voltage_null_value_returns_error(void)
{
  rx_err_t err = rx_bus_adc_read_voltage_mv(&s_test_manager, s_test_bus_name, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_adc_calls, mock_adc_get_call_count());
}

/**
 * @brief rx_bus_adc_read_voltage_mv with null bus_name returns k_rx_err_null_ptr
 *
 * @since Version 1.0.0
 */
void test_bus_adc_read_voltage_null_bus_name_returns_error(void)
{
  uint32_t voltage_mv = (uint32_t)k_test_voltage_zero;
  rx_err_t err        = rx_bus_adc_read_voltage_mv(&s_test_manager, nullptr, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
  TEST_ASSERT_EQUAL(k_expected_zero_adc_calls, mock_adc_get_call_count());
}

/**
 * @brief rx_bus_adc_read_voltage_mv on uninitialized bus returns k_rx_err_invalid_state
 *
 * @details
 * Bus is registered but rx_bus_adc_init() has not been called.
 *
 * @pre s_test_manager initialized; bus registered but NOT initialized
 * @pre mock ADC HAL state is clean (no prior calls recorded)
 * @post No ADC HAL calls recorded
 * @post voltage_mv output argument not modified by the rejected operation
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
*/
void test_bus_adc_read_voltage_not_initialized_returns_error(void)
{
  uint32_t voltage_mv = (uint32_t)k_test_voltage_zero;
  rx_err_t err        = rx_bus_adc_read_voltage_mv(&s_test_manager, s_test_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_EQUAL(k_expected_zero_adc_calls, mock_adc_get_call_count());
}

/* =============================================================================
 * Additional Coverage Tests
 * =============================================================================
 */

/**
 * @brief rx_bus_adc_init with wrong bus type returns error from callback
 *
 * @details
 * Tampers the registered bus config type to a non-ADC type, then calls
 * rx_bus_adc_init(). The internal callback detects the mismatch and returns
 * k_rx_err_invalid_arg, covering lines 404-406.
 *
 * @pre s_test_manager initialized; s_adc_config registered as ADC type
 * @pre s_adc_config.type patched to k_bus_type_gpio to simulate wrong type
 * @post rx_bus_adc_init returns k_rx_err_invalid_arg
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_adc_init_wrong_bus_type_returns_error(void)
{
  /* Patch the registered config to look like a non-ADC bus */
  s_adc_config.type = k_bus_type_gpio;

  rx_err_t err = rx_bus_adc_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief rx_bus_adc_init with post-init read failure still succeeds
 *
 * @details
 * Enables timeout simulation so that adc_read() returns an error during the
 * post-init verification step (line 428-431). The init callback logs a warning
 * and continues, returning k_rx_ok. Covers line 429.
 *
 * @pre s_test_manager initialized; s_test_bus_name registered as ADC
 * @pre mock timeout simulation enabled BEFORE calling rx_bus_adc_init
 * @post rx_bus_adc_init returns k_rx_ok (warning logged, init still succeeds)
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_adc_init_post_init_read_failure_still_succeeds(void)
{
  /* Let adc_init() succeed but make adc_read() return timeout */
  mock_adc_simulate_timeout(true);

  rx_err_t err = rx_bus_adc_init(&s_test_manager, s_test_bus_name);
  /* Init should still succeed - post-init read failure is a warning only */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief rx_bus_adc_read with value exceeding 12-bit maximum logs warning but succeeds
 *
 * @details
 * Sets the mock raw value to 5000 (> 4095, the maximum for 12-bit resolution).
 * The read callback logs a warning and continues, covering line 534.
 *
 * @pre s_test_manager initialized; ADC bus initialized with 12-bit resolution
 * @pre mock raw value set to 5000 (> 2^12 - 1)
 * @post rx_bus_adc_read returns k_rx_ok
 * @post Returned value is 5000 (warning logged but read succeeds)
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_adc_read_value_exceeds_max_logs_warning(void)
{
  /* Set raw value above 12-bit max (4095) to trigger the bounds warning */
  static const uint16_t k_test_out_of_range_value = 5000U;
  mock_adc_set_value((uint8_t)k_test_adc_unit,
                     (uint8_t)k_test_adc_channel,
                     k_test_out_of_range_value);

  rx_err_t err = rx_bus_adc_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint16_t value = (uint16_t)k_test_adc_value_zero;
  err            = rx_bus_adc_read(&s_test_manager, s_test_bus_name, &value);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL((uint16_t)k_test_out_of_range_value, value);
}

/**
 * @brief rx_bus_adc_read_voltage_mv HAL error propagates correctly
 *
 * @details
 * Injects an error on the next ADC HAL call (adc_read_voltage_mv), then
 * verifies the function returns that error. Covers lines 649-651.
 *
 * @pre s_test_manager initialized; ADC bus initialized
 * @pre mock error injected via mock_adc_set_next_error
 * @post rx_bus_adc_read_voltage_mv returns k_rx_err_timeout
 * @post voltage_mv output argument is not modified
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_adc_read_voltage_hal_error_propagates(void)
{
  rx_err_t err = rx_bus_adc_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  mock_adc_set_next_error(k_rx_err_timeout);

  uint32_t voltage_mv = (uint32_t)k_test_voltage_zero;
  err                 = rx_bus_adc_read_voltage_mv(&s_test_manager, s_test_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/**
 * @brief rx_bus_adc_read_voltage_mv with value exceeding safety limit logs warning
 *
 * @details
 * Sets the raw mock value to UINT16_MAX so that the computed voltage
 * (65535 * 3300 / 4095 = ~52812 mV) exceeds k_adc_max_safety_voltage_mv (5500 mV).
 * The callback logs a warning and continues, covering line 656.
 *
 * @pre s_test_manager initialized; ADC bus initialized with 12-bit resolution
 * @pre mock raw value set to UINT16_MAX
 * @post rx_bus_adc_read_voltage_mv returns k_rx_ok (warning logged, read succeeds)
 * @post Returned voltage_mv > k_adc_max_safety_voltage_mv
 *
 * @note Not thread-safe; must be run from the single-threaded Unity test harness
 *
 * @since Version 1.0.0
 */
void test_bus_adc_read_voltage_exceeds_safety_limit_logs_warning(void)
{
  /* Set raw value to UINT16_MAX so computed voltage >> 5500 mV */
  mock_adc_set_value((uint8_t)k_test_adc_unit, (uint8_t)k_test_adc_channel, UINT16_MAX);

  rx_err_t err = rx_bus_adc_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint32_t voltage_mv = (uint32_t)k_test_voltage_zero;
  err                 = rx_bus_adc_read_voltage_mv(&s_test_manager, s_test_bus_name, &voltage_mv);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Computed voltage should exceed 5500 mV safety limit */
  static const uint32_t k_safety_voltage_mv = 5500U;
  TEST_ASSERT_GREATER_THAN(k_safety_voltage_mv, voltage_mv);
}

/* =============================================================================
 * Bus Config Validation Coverage Tests
 * =============================================================================
 */

/**
 * @brief Invalid ADC unit in rx_bus_config_init_adc returns error
 *
 * @details
 * Passes unit >= k_adc_unit_count (2) to trigger the invalid-unit branch
 * inside rx_bus_config_init_adc (lines 661-662).
 *
 * @pre None
 * @post k_rx_err_invalid_arg returned
 *
 * @note Covers lines 661-662 of rx_bus_config.c
 */
void test_bus_config_adc_invalid_unit_returns_error(void)
{
  rx_bus_config_t      config;
  static const uint8_t k_invalid_unit = k_adc_unit_count;
  rx_err_t             err =
    rx_bus_config_init_adc(&config, "test", k_invalid_unit, k_test_adc_channel, k_test_adc_bits);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Invalid ADC channel in rx_bus_config_init_adc returns error
 *
 * @details
 * Passes channel > k_adc_channel_max (7) to trigger the invalid-channel branch
 * inside rx_bus_config_init_adc (lines 667-668).
 *
 * @pre None
 * @post k_rx_err_invalid_arg returned
 *
 * @note Covers lines 667-668 of rx_bus_config.c
 */
void test_bus_config_adc_invalid_channel_returns_error(void)
{
  rx_bus_config_t      config;
  static const uint8_t k_invalid_channel = k_adc_channel_max + 1U;
  rx_err_t             err =
    rx_bus_config_init_adc(&config, "test", k_test_adc_unit, k_invalid_channel, k_test_adc_bits);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Invalid ADC resolution in rx_bus_config_init_adc returns error
 *
 * @details
 * Passes bits=9 (not 8, 10, or 12) to trigger the invalid-resolution branch
 * inside rx_bus_config_init_adc (lines 674-675).
 *
 * @pre None
 * @post k_rx_err_invalid_arg returned
 *
 * @note Covers lines 674-675 of rx_bus_config.c
 */
void test_bus_config_adc_invalid_resolution_returns_error(void)
{
  rx_bus_config_t      config;
  static const uint8_t k_invalid_bits = 9U;
  rx_err_t             err =
    rx_bus_config_init_adc(&config, "test", k_test_adc_unit, k_test_adc_channel, k_invalid_bits);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Null config pointer to rx_bus_config_init_adc returns error
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(config, ...) branch at line 656 of rx_bus_config.c.
 */
void test_bus_config_adc_null_config_returns_error(void)
{
  rx_err_t err = rx_bus_config_init_adc(nullptr,
                                        "test",
                                        (uint8_t)k_test_adc_unit,
                                        (uint8_t)k_test_adc_channel,
                                        (uint8_t)k_test_adc_bits);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Null name pointer to rx_bus_config_init_adc returns error
 *
 * @details
 * Covers the RX_CHECK_NULL_PTR(name, ...) branch at line 657 of rx_bus_config.c.
 */
void test_bus_config_adc_null_name_returns_error(void)
{
  rx_bus_config_t config;
  rx_err_t        err = rx_bus_config_init_adc(&config,
                                        nullptr,
                                        (uint8_t)k_test_adc_unit,
                                        (uint8_t)k_test_adc_channel,
                                        (uint8_t)k_test_adc_bits);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief 8-bit ADC resolution accepted by rx_bus_config_init_adc
 *
 * @details
 * Exercises the `bits != k_adc_resolution_8bit` = FALSE short-circuit path
 * at line 672 of rx_bus_config.c. When bits==8 the first operand of the &&
 * chain is false, so the entire condition short-circuits to false and the
 * function proceeds without returning an error.
 *
 * @pre rx_bus_config_init_adc compiled with branch coverage instrumentation
 * @pre k_adc_resolution_8bit == 8
 * @post config initialised successfully with bits == k_adc_resolution_8bit
 * @post return value is k_rx_ok
 *
 * @since Version 1.0.0
 */
void test_bus_config_adc_resolution_8bit_valid(void)
{
  rx_bus_config_t config;
  rx_err_t        err = rx_bus_config_init_adc(&config,
                                        "adc0",
                                        (uint8_t)k_test_adc_unit,
                                        (uint8_t)k_test_adc_channel,
                                        (uint8_t)k_adc_resolution_8bit);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL((uint8_t)k_adc_resolution_8bit, config.proto.adc.bits);
}

/**
 * @brief 10-bit ADC resolution accepted by rx_bus_config_init_adc
 *
 * @details
 * Exercises the `bits != k_adc_resolution_10bit` = FALSE short-circuit path
 * at line 672 of rx_bus_config.c. When bits==10 the first operand is true
 * (bits != 8) but the second operand is false (bits == 10), so the && chain
 * short-circuits to false and the function proceeds without returning an error.
 *
 * @pre rx_bus_config_init_adc compiled with branch coverage instrumentation
 * @pre k_adc_resolution_10bit == 10
 * @post config initialised successfully with bits == k_adc_resolution_10bit
 * @post return value is k_rx_ok
 *
 * @since Version 1.0.0
 */
void test_bus_config_adc_resolution_10bit_valid(void)
{
  rx_bus_config_t config;
  rx_err_t        err = rx_bus_config_init_adc(&config,
                                        "adc0",
                                        (uint8_t)k_test_adc_unit,
                                        (uint8_t)k_test_adc_channel,
                                        (uint8_t)k_adc_resolution_10bit);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL((uint8_t)k_adc_resolution_10bit, config.proto.adc.bits);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Unity test runner entry point
 *
 * @details
 * Registers and executes all ADC bus abstraction tests. Returns non-zero
 * if any test fails.
 *
 * @pre Unity framework initialized (UNITY_BEGIN called internally)
 * @pre Test binary linked against mock_adc_hal and mock_bus_manager
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
  RUN_TEST(test_bus_adc_init_marks_unit_initialized);
  RUN_TEST(test_bus_adc_init_null_manager_returns_error);
  RUN_TEST(test_bus_adc_init_null_name_returns_error);
  RUN_TEST(test_bus_adc_init_bus_not_found_returns_error);
  RUN_TEST(test_bus_adc_init_hal_error_propagates);
  RUN_TEST(test_bus_adc_init_wrong_bus_type_returns_error);
  RUN_TEST(test_bus_adc_init_post_init_read_failure_still_succeeds);

  /* Read raw value tests */
  RUN_TEST(test_bus_adc_read_returns_configured_value);
  RUN_TEST(test_bus_adc_read_null_manager_returns_error);
  RUN_TEST(test_bus_adc_read_null_bus_name_returns_error);
  RUN_TEST(test_bus_adc_read_null_value_returns_error);
  RUN_TEST(test_bus_adc_read_not_initialized_returns_error);
  RUN_TEST(test_bus_adc_read_hal_error_propagates);
  RUN_TEST(test_bus_adc_read_value_exceeds_max_logs_warning);

  /* Read voltage tests */
  RUN_TEST(test_bus_adc_read_voltage_returns_nonzero_for_full_scale);
  RUN_TEST(test_bus_adc_read_voltage_null_manager_returns_error);
  RUN_TEST(test_bus_adc_read_voltage_null_bus_name_returns_error);
  RUN_TEST(test_bus_adc_read_voltage_null_value_returns_error);
  RUN_TEST(test_bus_adc_read_voltage_not_initialized_returns_error);
  RUN_TEST(test_bus_adc_read_voltage_hal_error_propagates);
  RUN_TEST(test_bus_adc_read_voltage_exceeds_safety_limit_logs_warning);

  /* Bus config validation tests */
  RUN_TEST(test_bus_config_adc_invalid_unit_returns_error);
  RUN_TEST(test_bus_config_adc_invalid_channel_returns_error);
  RUN_TEST(test_bus_config_adc_invalid_resolution_returns_error);
  RUN_TEST(test_bus_config_adc_null_config_returns_error);
  RUN_TEST(test_bus_config_adc_null_name_returns_error);
  RUN_TEST(test_bus_config_adc_resolution_8bit_valid);
  RUN_TEST(test_bus_config_adc_resolution_10bit_valid);

  return UNITY_END();
}
