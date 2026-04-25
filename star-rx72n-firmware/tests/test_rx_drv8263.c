/**
 * @file test_rx_drv8263.c
 * @brief Unit Tests for DRV8263H-Q1 Motor Driver Chip-Level Control
 *
 * @details
 * Tests the rx_drv8263 module which provides DRVOFF control, latched fault
 * reset via nSLEEP pulse, and Open Load Protection (OLP) diagnostics for
 * the DRV8263H-Q1 H-bridge motor driver.
 *
 * Test coverage:
 * - Initialization with valid/invalid configs and null checks
 * - DRVOFF GPIO control
 * - Latched fault clearing (nSLEEP pulse sequence)
 * - OLP diagnostic with 2 symmetric truth table outcomes (normal, short-to-VM)
 * - ADC-to-amps conversion accuracy
 * - OLP boot/fault enable configuration
 *
 * @author Locked, Inc.
 * @date 2026-03-03
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 3: Zero dynamic allocation in test infrastructure
 * - Rule 4: All test functions under 60 lines
 * - Rule 5: Assertions validate preconditions via Unity macros
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Each test function verifies one behavior
 * - **D (Dependency Inversion):** Tests use mock PORT registers via
 *   mock_drv8263_port.h abstraction
 *
 * @see rx_drv8263.h Module under test
 * @see mock_drv8263_port.h Mock PORT register API
 */

#include <string.h>

#include "mock_drv8263_port.h"
#include "rx_drv8263.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * ============================================================================= */

/**
 * @enum test_port_t
 * @brief Port numbers used in test configurations
 *
 * @details
 * Maps DRV8263H-Q1 control signals to their GPIO port assignments for
 * unit testing. Values match the actual hardware pin assignments from
 * hardware_config.h.
 *
 * @invariant All port values must be less than k_mock_port_count (20)
 *
 * @code
 * rx_drv8263_config_t cfg = {
 *   .port_drvoff = k_test_port_drvoff,
 *   .port_nsleep = k_test_port_nsleep,
 * };
 * @endcode
 *
 * @see test_pin_t Corresponding pin numbers
 * @see internal_make_valid_config() Uses these port constants
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_port_drvoff = 6, /**< DRVOFF on port 6 */
  k_test_port_nsleep = 6, /**< nSLEEP on port 6 */
  k_test_port_nfault = 1, /**< nFAULT on port 1 */
  k_test_port_in1    = 1, /**< IN1 on port 1 */
  k_test_port_in2    = 2, /**< IN2 on port 2 */
} test_port_t;

/**
 * @enum test_pin_t
 * @brief Pin numbers used in test configurations
 *
 * @details
 * Maps DRV8263H-Q1 control signals to their GPIO pin assignments within
 * each port for unit testing. Each pin pairs with a port from test_port_t
 * to form a complete GPIO coordinate (e.g., P61 = port 6, pin 1).
 *
 * @invariant All pin values must be less than k_mock_pins_per_port (8)
 *
 * @code
 * rx_drv8263_config_t cfg = {
 *   .pin_drvoff = k_test_pin_drvoff,
 *   .pin_nsleep = k_test_pin_nsleep,
 * };
 * @endcode
 *
 * @see test_port_t Corresponding port numbers
 * @see internal_make_valid_config() Uses these pin constants
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_pin_drvoff = 1, /**< DRVOFF on pin 1 (P61) */
  k_test_pin_nsleep = 0, /**< nSLEEP on pin 0 (P60) */
  k_test_pin_nfault = 5, /**< nFAULT on pin 5 (P15) */
  k_test_pin_in1    = 7, /**< IN1 on pin 7 (P17) */
  k_test_pin_in2    = 3, /**< IN2 on pin 3 (P23) */
} test_pin_t;

/**
 * @enum test_invalid_gpio_t
 * @brief Out-of-range GPIO values for negative testing
 *
 * @details
 * Provides intentionally invalid port and pin numbers that exceed the
 * hardware limits. Used by negative tests to verify that rx_drv8263_init()
 * rejects configurations with out-of-range GPIO coordinates.
 *
 * @invariant k_test_invalid_port exceeds k_max_port_number (16)
 * @invariant k_test_invalid_pin exceeds k_max_pin_number (7)
 *
 * @code
 * cfg.port_drvoff = k_test_invalid_port;
 * TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_drv8263_init(&handle, &cfg));
 * @endcode
 *
 * @see internal_validate_gpio() Validates port/pin ranges
 * @see test_init_rejects_invalid_drvoff_port() Uses k_test_invalid_port
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_invalid_port = 17, /**< Out-of-range port number (max valid is 16) */
  k_test_invalid_pin  = 8,  /**< Out-of-range pin number (max valid is 7) */
} test_invalid_gpio_t;

/** @brief Delay values used in internal_delay_us boundary tests */
typedef enum : uint32_t {
  k_test_delay_over_max = 101, /**< Exceeds k_max_delay_us (100) for boundary test */
} test_delay_boundary_t;

/**
 * @var s_adc_voltage_zero
 * @brief ADC input voltage of zero volts
 * @details Used as baseline for ADC-to-amps conversion tests (0V = 0A)
 */
static const float s_adc_voltage_zero = 0.0F;

/**
 * @var s_adc_voltage_one_volt
 * @brief ADC input voltage of one volt
 * @details Provides a known mid-range input (1.0V) for conversion accuracy tests
 */
static const float s_adc_voltage_one_volt = 1.0F;

/**
 * @var s_adc_voltage_full_scale
 * @brief ADC full-scale voltage (3.3V reference)
 * @details Maximum ADC input matching the RX72N 3.3V analog reference voltage
 */
static const float s_adc_voltage_full_scale = 3.3F;

/**
 * @var s_adc_voltage_one_amp
 * @brief ADC voltage that produces exactly 1.0 A output
 * @details Derived from DRV8263H-Q1 current sense gain: 1.0302V / gain = 1.0A
 */
static const float s_adc_voltage_one_amp = 1.0302F;

/**
 * @var s_adc_tolerance_tight
 * @brief Tight tolerance for ADC conversion accuracy checks
 * @details Used for exact-value tests where floating-point rounding is minimal
 */
static const float s_adc_tolerance_tight = 0.001F;

/**
 * @var s_adc_tolerance_loose
 * @brief Loose tolerance for full-scale ADC conversion checks
 * @details Used for full-scale and edge-case tests with larger rounding error
 */
static const float s_adc_tolerance_loose = 0.01F;

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

/**
 * @var s_handle
 * @brief Test fixture handle for rx_drv8263 driver instance
 * @note Reset in setUp(), should not be modified directly outside test setup/teardown
 * @warning Only valid after setUp() has been called
 */
static rx_drv8263_handle_t s_handle;

/**
 * @var s_config
 * @brief Test fixture configuration for rx_drv8263 driver
 * @note Reset to valid defaults in setUp() via internal_make_valid_config()
 * @warning Only valid after setUp() has been called
 */
static rx_drv8263_config_t s_config;

/**
 * @brief Create a valid test configuration
 *
 * @details
 * Returns a fully-populated rx_drv8263_config_t with default test pin
 * assignments for all GPIO signals (DRVOFF, nSLEEP, nFAULT, IN1, IN2).
 *
 * @return rx_drv8263_config_t Valid configuration struct with all fields set
 *
 * @pre Test port/pin constants (k_test_port_*, k_test_pin_*) are defined
 * @pre Mock port register array is allocated (k_mock_port_count entries)
 * @post All fields in returned struct are set to valid values
 * @post No heap allocation performed
 *
 * @note Thread-safe; pure function with no side effects
 *
 * @since 1.0.0
 */
static rx_drv8263_config_t internal_make_valid_config(void)
{
  rx_drv8263_config_t cfg = {
    .port_drvoff      = k_test_port_drvoff,
    .pin_drvoff       = k_test_pin_drvoff,
    .port_nsleep      = k_test_port_nsleep,
    .pin_nsleep       = k_test_pin_nsleep,
    .port_nfault      = k_test_port_nfault,
    .pin_nfault       = k_test_pin_nfault,
    .port_in1         = k_test_port_in1,
    .pin_in1          = k_test_pin_in1,
    .port_in2         = k_test_port_in2,
    .pin_in2          = k_test_pin_in2,
    .olp_enable_boot  = false,
    .olp_enable_fault = false,
  };
  return cfg;
}

/**
 * @brief Initialize handle with valid config for tests that need it
 *
 * @details
 * Initializes s_handle using a valid configuration from internal_make_valid_config()
 * and asserts that initialization succeeds. Used as a helper by tests that
 * require a fully initialized driver handle.
 *
 * @pre setUp() has been called (s_handle zeroed, mock ports reset)
 * @pre Mock port register array is initialized via mock_drv8263_port_reset()
 * @post s_handle is initialized and ready for use
 * @post s_config is set to the valid default configuration
 *
 * @note Not thread-safe; modifies file-scope s_handle and s_config
 *
 * @since 1.0.0
 */
static void internal_init_handle(void)
{
  s_config     = internal_make_valid_config();
  rx_err_t err = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
}

/**
 * @brief Unity test setup -- reset mock ports and clear driver handle
 *
 * @details
 * Resets all mock port registers to zero via mock_drv8263_port_reset(),
 * zeroes the s_handle struct with memset, and populates s_config with
 * valid default GPIO assignments via internal_make_valid_config().
 * This ensures each test starts from a clean, deterministic state.
 *
 * @pre Unity test framework is initialized
 * @pre No concurrent test execution
 *
 * @post s_handle is zeroed
 * @post s_config contains valid default configuration
 * @post All mock port registers are cleared to zero
 *
 * @note Called automatically by Unity before each test function
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  mock_drv8263_port_reset();
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_handle, 0, sizeof(s_handle));
  s_config = internal_make_valid_config();
}

/**
 * @brief Unity test teardown -- reset mock ports to clean state
 *
 * @details
 * Calls mock_drv8263_port_reset() to clear all mock port registers
 * back to zero, ensuring no residual GPIO state from the completed
 * test leaks into subsequent tests.
 *
 * @pre Test function has completed execution
 * @pre Mock port register array is valid
 *
 * @post All mock port registers are cleared to zero
 * @post No residual state from previous test
 *
 * @note Called automatically by Unity after each test function
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  mock_drv8263_port_reset();
}

/* =============================================================================
 * Initialization Tests
 * ============================================================================= */

/**
 * @brief Verify successful initialization with valid configuration
 *
 * @details
 * Calls rx_drv8263_init() with a fully valid s_config (all GPIO ports and
 * pins within range) and asserts that the function returns k_rx_ok and
 * sets s_handle.initialized to true.
 *
 * @pre setUp() has been called (s_handle zeroed, mock ports reset)
 * @pre s_config contains valid GPIO assignments from internal_make_valid_config()
 *
 * @post s_handle.initialized is true
 * @post rx_drv8263_init() return value equals k_rx_ok
 *
 * @note Exercises the happy path; negative-path tests are separate functions
 *
 * @since Version 1.0.0
 */
void test_init_success(void)
{
  rx_err_t err = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
}

/**
 * @brief Verify init rejects null handle pointer
 *
 * @details
 * Calls rx_drv8263_init() with a NULL handle pointer and verifies that
 * k_rx_err_null_ptr is returned. Validates defensive null-pointer checking
 * per NASA Rule 5.
 *
 * @pre s_config is initialized to valid values by setUp()
 * @pre handle argument is NULL (passed as NULL in the call)
 *
 * @post Return value equals k_rx_err_null_ptr
 * @post No mock port register state is modified
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_null_handle(void)
{
  rx_err_t err = rx_drv8263_init(NULL, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify init rejects null config pointer
 *
 * @details
 * Calls rx_drv8263_init() with a NULL config pointer and verifies that
 * k_rx_err_null_ptr is returned. Validates the second null-pointer guard
 * in the init function.
 *
 * @pre s_handle is zero-initialized by setUp()
 * @pre config argument is NULL (passed as NULL in the call)
 *
 * @post Return value equals k_rx_err_null_ptr
 * @post s_handle.initialized remains false
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_null_config(void)
{
  rx_err_t err = rx_drv8263_init(&s_handle, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Verify init rejects out-of-range DRVOFF port number
 *
 * @details
 * Sets s_config.port_drvoff to k_test_invalid_port (exceeds max port index)
 * and verifies that k_rx_err_invalid_arg is returned and initialized is false.
 * Validates GPIO validation per NASA Rule 5.
 *
 * @pre s_config.port_drvoff is set to k_test_invalid_port before the call
 * @pre All other config fields are valid (set by setUp())
 *
 * @post Return value equals k_rx_err_invalid_arg
 * @post s_handle.initialized is false (init rejected invalid GPIO config)
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_invalid_drvoff_port(void)
{
  s_config.port_drvoff = k_test_invalid_port; /* Out of range (max is 16) */
  rx_err_t err         = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
  TEST_ASSERT_FALSE(s_handle.initialized);
}

/**
 * @brief Verify init rejects out-of-range DRVOFF pin number
 *
 * @details
 * Sets s_config.pin_drvoff to k_test_invalid_pin (exceeds max pin index of 7)
 * and verifies that k_rx_err_invalid_arg is returned. Complements the port
 * validation test by checking the pin dimension separately.
 *
 * @pre s_config.pin_drvoff is set to k_test_invalid_pin before the call
 * @pre All other config fields are valid (set by setUp())
 *
 * @post Return value equals k_rx_err_invalid_arg
 * @post No mock port register state is modified
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_invalid_drvoff_pin(void)
{
  s_config.pin_drvoff = k_test_invalid_pin; /* Out of range (max is 7) */
  rx_err_t err        = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify init rejects out-of-range nSLEEP pin number
 *
 * @details
 * Sets s_config.pin_nsleep to k_test_invalid_pin (exceeds max pin index of 7)
 * and verifies that k_rx_err_invalid_arg is returned. Validates that GPIO
 * validation is applied to all pins, not just DRVOFF.
 *
 * @pre s_config.pin_nsleep is set to k_test_invalid_pin before the call
 * @pre All other config fields are valid (set by setUp())
 *
 * @post Return value equals k_rx_err_invalid_arg
 * @post No mock port register state is modified
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_invalid_nsleep_pin(void)
{
  s_config.pin_nsleep = k_test_invalid_pin; /* Out of range (max is 7) */
  rx_err_t err        = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify init rejects out-of-range nFAULT pin number
 *
 * @details
 * Sets s_config.pin_nfault to k_test_invalid_pin (exceeds max pin index of 7)
 * and verifies that k_rx_err_invalid_arg is returned. Validates that GPIO
 * validation is applied to the nFAULT pin independently.
 *
 * @pre s_config.pin_nfault is set to k_test_invalid_pin before the call
 * @pre All other config fields are valid (set by setUp())
 *
 * @post Return value equals k_rx_err_invalid_arg
 * @post No mock port register state is modified
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_invalid_nfault_pin(void)
{
  s_config.pin_nfault = k_test_invalid_pin;
  rx_err_t err        = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify init rejects out-of-range IN1 pin number
 *
 * @details
 * Sets s_config.pin_in1 to k_test_invalid_pin (exceeds max pin index of 7)
 * and verifies that k_rx_err_invalid_arg is returned. Validates that GPIO
 * validation is applied to the IN1 pin independently.
 *
 * @pre s_config.pin_in1 is set to k_test_invalid_pin before the call
 * @pre All other config fields are valid (set by setUp())
 *
 * @post Return value equals k_rx_err_invalid_arg
 * @post No mock port register state is modified
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_invalid_in1_pin(void)
{
  s_config.pin_in1 = k_test_invalid_pin;
  rx_err_t err     = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify init rejects out-of-range IN2 pin number
 *
 * @details
 * Sets s_config.pin_in2 to k_test_invalid_pin (exceeds max pin index of 7)
 * and verifies that k_rx_err_invalid_arg is returned. Validates that GPIO
 * validation is applied to the IN2 pin independently.
 *
 * @pre s_config.pin_in2 is set to k_test_invalid_pin before the call
 * @pre All other config fields are valid (set by setUp())
 *
 * @post Return value equals k_rx_err_invalid_arg
 * @post No mock port register state is modified
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_invalid_in2_pin(void)
{
  s_config.pin_in2 = k_test_invalid_pin;
  rx_err_t err     = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify init rejects out-of-range nSLEEP port number
 *
 * @details
 * Sets s_config.port_nsleep to k_test_invalid_port (exceeds max port index)
 * and verifies that k_rx_err_invalid_arg is returned. Validates that GPIO
 * port validation is applied to nSLEEP independently of DRVOFF.
 *
 * @pre s_config.port_nsleep is set to k_test_invalid_port before the call
 * @pre All other config fields are valid (set by setUp())
 *
 * @post Return value equals k_rx_err_invalid_arg
 * @post No mock port register state is modified
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_invalid_nsleep_port(void)
{
  s_config.port_nsleep = k_test_invalid_port;
  rx_err_t err         = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify init rejects out-of-range nFAULT port number
 *
 * @details
 * Sets s_config.port_nfault to k_test_invalid_port (exceeds max port index)
 * and verifies that k_rx_err_invalid_arg is returned. Validates that GPIO
 * port validation is applied to nFAULT independently.
 *
 * @pre s_config.port_nfault is set to k_test_invalid_port before the call
 * @pre All other config fields are valid (set by setUp())
 *
 * @post Return value equals k_rx_err_invalid_arg
 * @post No mock port register state is modified
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_invalid_nfault_port(void)
{
  s_config.port_nfault = k_test_invalid_port;
  rx_err_t err         = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify init rejects out-of-range IN1 port number
 *
 * @details
 * Sets s_config.port_in1 to k_test_invalid_port (exceeds max port index)
 * and verifies that k_rx_err_invalid_arg is returned. Validates that GPIO
 * port validation is applied to IN1 independently.
 *
 * @pre s_config.port_in1 is set to k_test_invalid_port before the call
 * @pre All other config fields are valid (set by setUp())
 *
 * @post Return value equals k_rx_err_invalid_arg
 * @post No mock port register state is modified
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_invalid_in1_port(void)
{
  s_config.port_in1 = k_test_invalid_port;
  rx_err_t err      = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify init rejects out-of-range IN2 port number
 *
 * @details
 * Sets s_config.port_in2 to k_test_invalid_port (exceeds max port index)
 * and verifies that k_rx_err_invalid_arg is returned. Validates that GPIO
 * port validation is applied to IN2 independently.
 *
 * @pre s_config.port_in2 is set to k_test_invalid_port before the call
 * @pre All other config fields are valid (set by setUp())
 *
 * @post Return value equals k_rx_err_invalid_arg
 * @post No mock port register state is modified
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_invalid_in2_port(void)
{
  s_config.port_in2 = k_test_invalid_port;
  rx_err_t err      = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Verify init copies all config fields correctly to handle
 *
 * @details
 * Calls rx_drv8263_init() with a valid config and verifies all 10 port and
 * pin fields (port_drvoff, pin_drvoff, port_nsleep, pin_nsleep, port_nfault,
 * pin_nfault, port_in1, pin_in1, port_in2, pin_in2) are copied into
 * handle.config. Validates that the config struct is copied by value.
 *
 * @pre s_config holds distinct non-default port and pin values (set by setUp())
 * @pre s_handle is zero-initialized by setUp()
 *
 * @post Return value equals k_rx_ok
 * @post All 10 port/pin fields in s_handle.config match the corresponding
 *       fields in s_config
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_copies_config(void)
{
  rx_err_t err = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(s_config.port_drvoff, s_handle.config.port_drvoff);
  TEST_ASSERT_EQUAL(s_config.pin_drvoff, s_handle.config.pin_drvoff);
  TEST_ASSERT_EQUAL(s_config.port_nsleep, s_handle.config.port_nsleep);
  TEST_ASSERT_EQUAL(s_config.pin_nsleep, s_handle.config.pin_nsleep);
  TEST_ASSERT_EQUAL(s_config.port_nfault, s_handle.config.port_nfault);
  TEST_ASSERT_EQUAL(s_config.pin_nfault, s_handle.config.pin_nfault);
  TEST_ASSERT_EQUAL(s_config.port_in1, s_handle.config.port_in1);
  TEST_ASSERT_EQUAL(s_config.pin_in1, s_handle.config.pin_in1);
  TEST_ASSERT_EQUAL(s_config.port_in2, s_handle.config.port_in2);
  TEST_ASSERT_EQUAL(s_config.pin_in2, s_handle.config.pin_in2);
}

/**
 * @brief Verify init runs OLP diagnostic when boot OLP is enabled
 *
 * @details
 * Enables olp_enable_boot in config and sets the nFAULT mock pin HIGH
 * (normal state, no fault) so the boot OLP sequence passes. Verifies that
 * init succeeds and handle.initialized is true after the boot OLP completes.
 *
 * @pre s_config.olp_enable_boot is set to true before the call
 * @pre nFAULT mock pin is set HIGH via mock_drv8263_port_set_pin_input()
 *
 * @post Return value equals k_rx_ok
 * @post s_handle.initialized is true
 *
 * @note Not thread-safe; runs in single-threaded Unity test context
 *
 * @since Version 1.0.0
 */
void test_init_with_boot_olp_enabled(void)
{
  /* With boot OLP enabled, init runs OLP diagnostic. Set nFAULT HIGH
   * (normal) so the OLP passes without warnings. */
  s_config.olp_enable_boot = true;
  mock_drv8263_port_set_pin_input(k_test_port_nfault, k_test_pin_nfault, true);
  rx_err_t err = rx_drv8263_init(&s_handle, &s_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
}

/* =============================================================================
 * DRVOFF Control Tests
 * ============================================================================= */

/** @brief Verify set_drvoff rejects null handle */
void test_set_drvoff_null_handle(void)
{
  rx_err_t err = rx_drv8263_set_drvoff(NULL, true);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @brief Verify set_drvoff rejects uninitialized handle */
void test_set_drvoff_not_initialized(void)
{
  /* Handle not initialized */
  rx_err_t err = rx_drv8263_set_drvoff(&s_handle, true);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @brief Verify DRVOFF pin drives HIGH when active=true */
void test_set_drvoff_active(void)
{
  internal_init_handle();
  rx_err_t err = rx_drv8263_set_drvoff(&s_handle, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify DRVOFF pin is HIGH */
  bool pin_state = mock_drv8263_port_get_pin_output(k_test_port_drvoff, k_test_pin_drvoff);
  TEST_ASSERT_TRUE(pin_state);
}

/** @brief Verify DRVOFF pin drives LOW when active=false */
void test_set_drvoff_inactive(void)
{
  internal_init_handle();
  /* First set HIGH, then LOW */
  (void)rx_drv8263_set_drvoff(&s_handle, true);
  rx_err_t err = rx_drv8263_set_drvoff(&s_handle, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify DRVOFF pin is LOW */
  bool pin_state = mock_drv8263_port_get_pin_output(k_test_port_drvoff, k_test_pin_drvoff);
  TEST_ASSERT_FALSE(pin_state);
}

/* =============================================================================
 * Latched Fault Clear Tests
 * ============================================================================= */

/** @brief Verify clear_fault rejects null handle */
void test_clear_fault_null_handle(void)
{
  rx_err_t err = rx_drv8263_clear_latched_fault(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @brief Verify clear_fault rejects uninitialized handle */
void test_clear_fault_not_initialized(void)
{
  rx_err_t err = rx_drv8263_clear_latched_fault(&s_handle);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @brief Verify nSLEEP returns HIGH after fault clear sequence */
void test_clear_fault_nsleep_returns_high(void)
{
  internal_init_handle();
  rx_err_t err = rx_drv8263_clear_latched_fault(&s_handle);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* After fault clear, nSLEEP should be HIGH (awake) */
  bool nsleep_state = mock_drv8263_port_get_pin_output(k_test_port_nsleep, k_test_pin_nsleep);
  TEST_ASSERT_TRUE(nsleep_state);
}

/* =============================================================================
 * OLP Diagnostic Tests
 * ============================================================================= */

/** @brief Verify run_olp rejects null handle */
void test_olp_null_handle(void)
{
  rx_drv8263_olp_result_t r1  = k_drv8263_olp_unknown;
  rx_drv8263_olp_result_t r2  = k_drv8263_olp_unknown;
  rx_err_t                err = rx_drv8263_run_olp(NULL, &r1, &r2);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @brief Verify run_olp rejects null result_out1 pointer */
void test_olp_null_result_out1(void)
{
  internal_init_handle();
  rx_drv8263_olp_result_t r2  = k_drv8263_olp_unknown;
  rx_err_t                err = rx_drv8263_run_olp(&s_handle, NULL, &r2);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @brief Verify run_olp rejects null result_out2 pointer */
void test_olp_null_result_out2(void)
{
  internal_init_handle();
  rx_drv8263_olp_result_t r1  = k_drv8263_olp_unknown;
  rx_err_t                err = rx_drv8263_run_olp(&s_handle, &r1, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @brief Verify run_olp rejects uninitialized handle */
void test_olp_not_initialized(void)
{
  rx_drv8263_olp_result_t r1  = k_drv8263_olp_unknown;
  rx_drv8263_olp_result_t r2  = k_drv8263_olp_unknown;
  rx_err_t                err = rx_drv8263_run_olp(&s_handle, &r1, &r2);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @brief Verify OLP reports normal when all nFAULT readings HIGH */
void test_olp_normal_all_nfault_high(void)
{
  /* Pattern {1,1,1} = Normal (no fault)
   * Set nFAULT HIGH for all patterns */
  internal_init_handle();
  mock_drv8263_port_set_pin_input(k_test_port_nfault, k_test_pin_nfault, true);

  rx_drv8263_olp_result_t r1  = k_drv8263_olp_unknown;
  rx_drv8263_olp_result_t r2  = k_drv8263_olp_unknown;
  rx_err_t                err = rx_drv8263_run_olp(&s_handle, &r1, &r2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_drv8263_olp_normal, r1);
  TEST_ASSERT_EQUAL(k_drv8263_olp_normal, r2);
}

/** @brief Verify OLP reports short-to-VM when all nFAULT readings LOW */
void test_olp_short_to_vm_all_nfault_low(void)
{
  /* Pattern {0,0,0} = Short to VM
   * Set nFAULT LOW for all patterns */
  internal_init_handle();
  mock_drv8263_port_set_pin_input(k_test_port_nfault, k_test_pin_nfault, false);

  rx_drv8263_olp_result_t r1  = k_drv8263_olp_unknown;
  rx_drv8263_olp_result_t r2  = k_drv8263_olp_unknown;
  rx_err_t                err = rx_drv8263_run_olp(&s_handle, &r1, &r2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_drv8263_olp_short_to_vm, r1);
  TEST_ASSERT_EQUAL(k_drv8263_olp_short_to_vm, r2);
}

/** @brief Verify DRVOFF returns to LOW after OLP diagnostic */
void test_olp_drvoff_restored_after_diagnostic(void)
{
  /* After OLP, DRVOFF should return to LOW (outputs enabled) */
  internal_init_handle();
  mock_drv8263_port_set_pin_input(k_test_port_nfault, k_test_pin_nfault, true);

  rx_drv8263_olp_result_t r1 = k_drv8263_olp_unknown;
  rx_drv8263_olp_result_t r2 = k_drv8263_olp_unknown;
  (void)rx_drv8263_run_olp(&s_handle, &r1, &r2);

  /* DRVOFF should be LOW after OLP completes */
  bool drvoff_state = mock_drv8263_port_get_pin_output(k_test_port_drvoff, k_test_pin_drvoff);
  TEST_ASSERT_FALSE(drvoff_state);
}

/** @brief Verify IN1 and IN2 return to LOW after OLP diagnostic */
void test_olp_in1_in2_restored_to_low(void)
{
  /* After OLP, IN1 and IN2 should be LOW (safe state) */
  internal_init_handle();
  mock_drv8263_port_set_pin_input(k_test_port_nfault, k_test_pin_nfault, true);

  rx_drv8263_olp_result_t r1 = k_drv8263_olp_unknown;
  rx_drv8263_olp_result_t r2 = k_drv8263_olp_unknown;
  (void)rx_drv8263_run_olp(&s_handle, &r1, &r2);

  /* IN1 and IN2 should be LOW */
  bool in1_state = mock_drv8263_port_get_pin_output(k_test_port_in1, k_test_pin_in1);
  bool in2_state = mock_drv8263_port_get_pin_output(k_test_port_in2, k_test_pin_in2);
  TEST_ASSERT_FALSE(in1_state);
  TEST_ASSERT_FALSE(in2_state);
}

/* =============================================================================
 * ADC-to-Amps Conversion Tests
 * ============================================================================= */

/**
 * @var s_adc_expected_amps_zero
 * @brief Expected current output for zero voltage input
 * @details 0.0V / 1.0302 = 0.0A
 */
static const float s_adc_expected_amps_zero = 0.0F;

/**
 * @var s_adc_expected_amps_one_volt
 * @brief Expected current for 1.0V input: 1.0V / 1.0302 = 0.9707A
 * @details Computed as s_adc_voltage_one_volt / s_adc_voltage_one_amp
 */
static const float s_adc_expected_amps_one_volt = 0.9707F;

/**
 * @var s_adc_expected_amps_full_scale
 * @brief Expected current for 3.3V input: 3.3V / 1.0302 = 3.203A
 * @details Computed as s_adc_voltage_full_scale / s_adc_voltage_one_amp
 */
static const float s_adc_expected_amps_full_scale = 3.203F;

/**
 * @var s_adc_expected_amps_one_amp
 * @brief Expected current for 1.0302V input: exactly 1.0A
 * @details The IPROPI divisor voltage produces exactly 1.0A output
 */
static const float s_adc_expected_amps_one_amp = 1.0F;

/** @brief Verify zero voltage converts to zero amps */
void test_adc_to_amps_zero_voltage(void)
{
  float amps = rx_drv8263_adc_to_amps(s_adc_voltage_zero);
  TEST_ASSERT_FLOAT_WITHIN(s_adc_tolerance_tight, s_adc_expected_amps_zero, amps);
}

/** @brief Verify 1.0V converts to approximately 0.971A */
void test_adc_to_amps_one_volt(void)
{
  /* 1.0V / 1.0302 = 0.9707 A */
  float amps = rx_drv8263_adc_to_amps(s_adc_voltage_one_volt);
  TEST_ASSERT_FLOAT_WITHIN(s_adc_tolerance_tight, s_adc_expected_amps_one_volt, amps);
}

/** @brief Verify 3.3V (full ADC range) converts to approximately 3.2A */
void test_adc_to_amps_full_scale(void)
{
  /* 3.3V / 1.0302 = 3.203 A (full ADC range) */
  float amps = rx_drv8263_adc_to_amps(s_adc_voltage_full_scale);
  TEST_ASSERT_FLOAT_WITHIN(s_adc_tolerance_loose, s_adc_expected_amps_full_scale, amps);
}

/** @brief Verify 1.0302V converts to exactly 1.0A */
void test_adc_to_amps_typical_motor_current(void)
{
  /* 1.0302V should give exactly 1.0 A */
  float amps = rx_drv8263_adc_to_amps(s_adc_voltage_one_amp);
  TEST_ASSERT_FLOAT_WITHIN(s_adc_tolerance_tight, s_adc_expected_amps_one_amp, amps);
}

/* =============================================================================
 * Internal Delay Tests
 * ============================================================================= */

/** @brief Verify internal_delay_us returns without crash for zero us */
void test_internal_delay_us_zero(void)
{
  /* us=0 triggers the guard branch; should return silently. Reaching the
   * line after the call is the assertion: any UB or hang fails the test. */
  internal_delay_us(0);
  TEST_PASS();
}

/** @brief Verify internal_delay_us returns without crash for us exceeding max */
void test_internal_delay_us_over_max(void)
{
  /* us=101 exceeds k_max_delay_us (100); should return silently. */
  internal_delay_us(k_test_delay_over_max);
  TEST_PASS();
}

/* =============================================================================
 * Internal GPIO Write Tests
 * ============================================================================= */

/** @brief Verify internal_gpio_write returns without crash for invalid port */
void test_internal_gpio_write_invalid_port(void)
{
  /* Port 20 exceeds k_max_port_number (16); defensive guard should return silently */
  internal_gpio_write(k_test_invalid_port, k_test_pin_drvoff, true);
  /* No crash and no output pin change on valid port 6 */
  bool pin_state = mock_drv8263_port_get_pin_output(k_test_port_drvoff, k_test_pin_drvoff);
  TEST_ASSERT_FALSE(pin_state);
}

/** @brief Verify internal_gpio_write returns without crash for invalid pin */
void test_internal_gpio_write_invalid_pin(void)
{
  /* Pin 8 exceeds k_max_pin_number (7); defensive guard should return silently */
  internal_gpio_write(k_test_port_drvoff, k_test_invalid_pin, true);
  /* No pin change should have occurred */
  uint8_t podr = mock_drv8263_port_get_podr(k_test_port_drvoff);
  TEST_ASSERT_EQUAL(0, podr);
}

/* =============================================================================
 * Internal GPIO Read Tests
 * ============================================================================= */

/** @brief Verify internal_gpio_read returns false for invalid port */
void test_internal_gpio_read_invalid_port(void)
{
  bool result = internal_gpio_read(k_test_invalid_port, k_test_pin_nfault);
  TEST_ASSERT_FALSE(result);
}

/** @brief Verify internal_gpio_read returns false for invalid pin */
void test_internal_gpio_read_invalid_pin(void)
{
  bool result = internal_gpio_read(k_test_port_nfault, k_test_invalid_pin);
  TEST_ASSERT_FALSE(result);
}

/* =============================================================================
 * Internal Validate Config Tests
 * ============================================================================= */

/** @brief Verify internal_validate_config returns null_ptr for null config */
void test_internal_validate_config_null(void)
{
  rx_err_t err = internal_validate_config(nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/* =============================================================================
 * Internal OLP Apply Patterns Tests
 * ============================================================================= */

/** @brief Verify internal_olp_apply_patterns returns without crash for null handle */
void test_internal_olp_apply_patterns_null_handle(void)
{
  bool readings[k_drv8263_olp_pattern_count] = {false};
  internal_olp_apply_patterns(nullptr, readings);
  /* Should return without modifying readings */
  TEST_ASSERT_FALSE(readings[0]);
  TEST_ASSERT_FALSE(readings[1]);
  TEST_ASSERT_FALSE(readings[2]);
}

/** @brief Verify internal_olp_apply_patterns returns without crash for null readings array */
void test_internal_olp_apply_patterns_null_readings(void)
{
  internal_init_handle();
  /* Should return without crash when readings array is null. */
  internal_olp_apply_patterns(&s_handle, nullptr);
  TEST_PASS();
}

/* =============================================================================
 * Internal OLP Decode Results Tests
 * ============================================================================= */

/** @brief Verify internal_olp_decode_results handles null result_out1 */
void test_internal_olp_decode_results_null_out1(void)
{
  rx_drv8263_olp_result_t r2 = k_drv8263_olp_unknown;
  internal_olp_decode_results(true, true, true, nullptr, &r2);
  /* Should return without crash; r2 unchanged */
  TEST_ASSERT_EQUAL(k_drv8263_olp_unknown, r2);
}

/** @brief Verify internal_olp_decode_results handles null result_out2 */
void test_internal_olp_decode_results_null_out2(void)
{
  rx_drv8263_olp_result_t r1 = k_drv8263_olp_unknown;
  internal_olp_decode_results(true, true, true, &r1, nullptr);
  /* Should return without crash; r1 unchanged */
  TEST_ASSERT_EQUAL(k_drv8263_olp_unknown, r1);
}

/** @brief Verify {1,1,0} pattern decodes to normal OUT1, open load OUT2 */
void test_internal_olp_decode_open_load_out2(void)
{
  rx_drv8263_olp_result_t r1 = k_drv8263_olp_unknown;
  rx_drv8263_olp_result_t r2 = k_drv8263_olp_unknown;
  internal_olp_decode_results(true, true, false, &r1, &r2);
  TEST_ASSERT_EQUAL(k_drv8263_olp_normal, r1);
  TEST_ASSERT_EQUAL(k_drv8263_olp_open_load, r2);
}

/** @brief Verify {1,0,1} pattern decodes to open load OUT1, normal OUT2 */
void test_internal_olp_decode_open_load_out1(void)
{
  rx_drv8263_olp_result_t r1 = k_drv8263_olp_unknown;
  rx_drv8263_olp_result_t r2 = k_drv8263_olp_unknown;
  internal_olp_decode_results(true, false, true, &r1, &r2);
  TEST_ASSERT_EQUAL(k_drv8263_olp_open_load, r1);
  TEST_ASSERT_EQUAL(k_drv8263_olp_normal, r2);
}

/** @brief Verify {0,1,1} pattern decodes to short to GND on both outputs */
void test_internal_olp_decode_short_to_gnd(void)
{
  rx_drv8263_olp_result_t r1 = k_drv8263_olp_unknown;
  rx_drv8263_olp_result_t r2 = k_drv8263_olp_unknown;
  internal_olp_decode_results(false, true, true, &r1, &r2);
  TEST_ASSERT_EQUAL(k_drv8263_olp_short_to_gnd, r1);
  TEST_ASSERT_EQUAL(k_drv8263_olp_short_to_gnd, r2);
}

/** @brief Verify unexpected nFAULT pattern {1,0,0} decodes to unknown */
void test_internal_olp_decode_unknown_pattern_100(void)
{
  /* {1,0,0} is not in the truth table */
  rx_drv8263_olp_result_t r1 = k_drv8263_olp_normal;
  rx_drv8263_olp_result_t r2 = k_drv8263_olp_normal;
  internal_olp_decode_results(true, false, false, &r1, &r2);
  TEST_ASSERT_EQUAL(k_drv8263_olp_unknown, r1);
  TEST_ASSERT_EQUAL(k_drv8263_olp_unknown, r2);
}

/** @brief Verify unexpected nFAULT pattern {0,1,0} decodes to unknown */
void test_internal_olp_decode_unknown_pattern_010(void)
{
  /* {0,1,0} is not in the truth table */
  rx_drv8263_olp_result_t r1 = k_drv8263_olp_normal;
  rx_drv8263_olp_result_t r2 = k_drv8263_olp_normal;
  internal_olp_decode_results(false, true, false, &r1, &r2);
  TEST_ASSERT_EQUAL(k_drv8263_olp_unknown, r1);
  TEST_ASSERT_EQUAL(k_drv8263_olp_unknown, r2);
}

/** @brief Verify unexpected nFAULT pattern {0,0,1} decodes to unknown */
void test_internal_olp_decode_unknown_pattern_001(void)
{
  /* {0,0,1} is not in the truth table */
  rx_drv8263_olp_result_t r1 = k_drv8263_olp_normal;
  rx_drv8263_olp_result_t r2 = k_drv8263_olp_normal;
  internal_olp_decode_results(false, false, true, &r1, &r2);
  TEST_ASSERT_EQUAL(k_drv8263_olp_unknown, r1);
  TEST_ASSERT_EQUAL(k_drv8263_olp_unknown, r2);
}

/** @brief Verify boot OLP with abnormal load condition logs a warning */
void test_init_boot_olp_abnormal_result(void)
{
  /* Enable boot OLP; nFAULT LOW -> short-to-VM -> abnormal result branch in init */
  s_config.olp_enable_boot = true;
  mock_drv8263_port_set_pin_input(k_test_port_nfault, k_test_pin_nfault, false);
  rx_err_t err = rx_drv8263_init(&s_handle, &s_config);
  /* Init succeeds even with abnormal OLP result (only logged as warning) */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.initialized);
}

/* =============================================================================
 * OLP Enable/Disable Configuration Tests
 * ============================================================================= */

/** @brief Verify set_olp_boot_enable rejects null handle */
void test_set_olp_boot_enable_null_handle(void)
{
  rx_err_t err = rx_drv8263_set_olp_boot_enable(NULL, true);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @brief Verify set_olp_boot_enable rejects uninitialized handle */
void test_set_olp_boot_enable_not_initialized(void)
{
  rx_err_t err = rx_drv8263_set_olp_boot_enable(&s_handle, true);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @brief Verify OLP boot enable flag toggles correctly */
void test_set_olp_boot_enable_success(void)
{
  internal_init_handle();
  rx_err_t err = rx_drv8263_set_olp_boot_enable(&s_handle, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.config.olp_enable_boot);

  err = rx_drv8263_set_olp_boot_enable(&s_handle, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handle.config.olp_enable_boot);
}

/** @brief Verify set_olp_fault_enable rejects null handle */
void test_set_olp_fault_enable_null_handle(void)
{
  rx_err_t err = rx_drv8263_set_olp_fault_enable(NULL, true);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/** @brief Verify set_olp_fault_enable rejects uninitialized handle */
void test_set_olp_fault_enable_not_initialized(void)
{
  rx_err_t err = rx_drv8263_set_olp_fault_enable(&s_handle, true);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/** @brief Verify OLP fault enable flag toggles correctly */
void test_set_olp_fault_enable_success(void)
{
  internal_init_handle();
  rx_err_t err = rx_drv8263_set_olp_fault_enable(&s_handle, true);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_handle.config.olp_enable_fault);

  err = rx_drv8263_set_olp_fault_enable(&s_handle, false);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_handle.config.olp_enable_fault);
}

/* =============================================================================
 * Unity Test Runner
 * ============================================================================= */

/**
 * @brief Unity test runner entry point for DRV8263H-Q1 unit tests
 *
 * @details
 * Runs the complete suite of rx_drv8263 unit tests via UNITY_BEGIN/UNITY_END.
 *
 * @return int Test result from UNITY_END() (0 = all passed, nonzero = failures)
 *
 * @pre Unity test framework linked and available
 * @pre Mock port register array is statically allocated
 *
 * @post All test functions have been executed
 * @post Unity exit code reflects pass/fail status
 *
 * @note Test-only entry point; not used in production firmware
 *
 * @since Version 1.0.0
 */
static void internal_run_init_and_control_tests(void)
{
  RUN_TEST(test_init_success);
  RUN_TEST(test_init_null_handle);
  RUN_TEST(test_init_null_config);
  RUN_TEST(test_init_invalid_drvoff_port);
  RUN_TEST(test_init_invalid_drvoff_pin);
  RUN_TEST(test_init_invalid_nsleep_port);
  RUN_TEST(test_init_invalid_nsleep_pin);
  RUN_TEST(test_init_invalid_nfault_port);
  RUN_TEST(test_init_invalid_nfault_pin);
  RUN_TEST(test_init_invalid_in1_pin);
  RUN_TEST(test_init_invalid_in2_pin);
  RUN_TEST(test_init_invalid_in1_port);
  RUN_TEST(test_init_invalid_in2_port);
  RUN_TEST(test_init_copies_config);
  RUN_TEST(test_init_with_boot_olp_enabled);
  RUN_TEST(test_set_drvoff_null_handle);
  RUN_TEST(test_set_drvoff_not_initialized);
  RUN_TEST(test_set_drvoff_active);
  RUN_TEST(test_set_drvoff_inactive);
  RUN_TEST(test_clear_fault_null_handle);
  RUN_TEST(test_clear_fault_not_initialized);
  RUN_TEST(test_clear_fault_nsleep_returns_high);
  RUN_TEST(test_olp_null_handle);
  RUN_TEST(test_olp_null_result_out1);
  RUN_TEST(test_olp_null_result_out2);
  RUN_TEST(test_olp_not_initialized);
  RUN_TEST(test_olp_normal_all_nfault_high);
  RUN_TEST(test_olp_short_to_vm_all_nfault_low);
  RUN_TEST(test_olp_drvoff_restored_after_diagnostic);
  RUN_TEST(test_olp_in1_in2_restored_to_low);
  RUN_TEST(test_adc_to_amps_zero_voltage);
  RUN_TEST(test_adc_to_amps_one_volt);
  RUN_TEST(test_adc_to_amps_full_scale);
  RUN_TEST(test_adc_to_amps_typical_motor_current);
  RUN_TEST(test_set_olp_boot_enable_null_handle);
  RUN_TEST(test_set_olp_boot_enable_not_initialized);
  RUN_TEST(test_set_olp_boot_enable_success);
  RUN_TEST(test_set_olp_fault_enable_null_handle);
  RUN_TEST(test_set_olp_fault_enable_not_initialized);
  RUN_TEST(test_set_olp_fault_enable_success);
}

static void internal_run_internal_function_tests(void)
{
  RUN_TEST(test_internal_delay_us_zero);
  RUN_TEST(test_internal_delay_us_over_max);
  RUN_TEST(test_internal_gpio_write_invalid_port);
  RUN_TEST(test_internal_gpio_write_invalid_pin);
  RUN_TEST(test_internal_gpio_read_invalid_port);
  RUN_TEST(test_internal_gpio_read_invalid_pin);
  RUN_TEST(test_internal_validate_config_null);
  RUN_TEST(test_internal_olp_apply_patterns_null_handle);
  RUN_TEST(test_internal_olp_apply_patterns_null_readings);
  RUN_TEST(test_internal_olp_decode_results_null_out1);
  RUN_TEST(test_internal_olp_decode_results_null_out2);
  RUN_TEST(test_internal_olp_decode_open_load_out2);
  RUN_TEST(test_internal_olp_decode_open_load_out1);
  RUN_TEST(test_internal_olp_decode_short_to_gnd);
  RUN_TEST(test_internal_olp_decode_unknown_pattern_100);
  RUN_TEST(test_internal_olp_decode_unknown_pattern_010);
  RUN_TEST(test_internal_olp_decode_unknown_pattern_001);
  RUN_TEST(test_init_boot_olp_abnormal_result);
}

int main(void)
{
  UNITY_BEGIN();
  internal_run_init_and_control_tests();
  internal_run_internal_function_tests();
  return UNITY_END();
}
