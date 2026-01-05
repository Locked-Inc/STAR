/* tests/test_rx_pin_validator.c */

/**
 * @file test_rx_pin_validator.c
 * @brief Unit Tests for Pin Validator Implementation
 *
 * Tests the pin validator concrete implementation including:
 * - Initialization with valid/invalid parameters
 * - Pin reservation (valid ports/pins)
 * - Pin release
 * - Conflict detection (double reservation)
 * - Invalid port/pin values
 * - All 17 ports x 8 pins coverage
 * - Thread-safety with mock mutexes
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "unity.h"

/* Include mock implementations first to override real headers */
#include "tx_api.h"

/* Include the module under test */
#include "rx_pin_validator.h"
#include "rx_pin_interface.h"
#include "rx_gpio_constants.h"

#include <string.h>

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static pin_validator_t s_validator;

void setUp(void)
{
  memset(&s_validator, 0, sizeof(s_validator));
}

void tearDown(void)
{
  if (s_validator.initialized) {
    pin_validator_deinit(&s_validator);
  }
}

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

typedef enum {
  k_test_port_0     = 0,
  k_test_port_5     = 5,
  k_test_port_9     = 9,
  k_test_port_a     = 0xA,
  k_test_port_b     = 0xB,
  k_test_port_g     = 0x10,
  k_test_pin_0      = 0,
  k_test_pin_3      = 3,
  k_test_pin_7      = 7,
  k_test_pin_8      = 8, /* Invalid: max is 7 */
  k_test_port_bad   = 0x11, /* Invalid: exceeds max port */
  k_test_port_gap   = 0x08, /* Gap port (8 is valid) */
} test_constants_t;

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test successful initialization
 */
void test_pin_validator_init_success(void)
{
  rx_err_t err = pin_validator_init(&s_validator);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_validator.initialized);
}

/**
 * @brief Test initialization with NULL validator pointer
 */
void test_pin_validator_init_null_pointer(void)
{
  rx_err_t err = pin_validator_init(NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test initialization clears all reservations
 */
void test_pin_validator_init_clears_reservations(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  for (uint32_t port = 0; port < k_pin_validator_max_ports; port++) {
    for (uint32_t pin = 0; pin < k_pin_validator_max_pins; pin++) {
      TEST_ASSERT_FALSE(s_validator.reservations[port][pin].reserved);
    }
  }
}

/* =============================================================================
 * Deinitialization Tests
 * =============================================================================
 */

/**
 * @brief Test successful deinitialization
 */
void test_pin_validator_deinit_success(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_validator.initialized);

  err = pin_validator_deinit(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_validator.initialized);
}

/**
 * @brief Test deinitialization with NULL pointer
 */
void test_pin_validator_deinit_null_pointer(void)
{
  rx_err_t err = pin_validator_deinit(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test deinitialization of already deinitialized validator
 */
void test_pin_validator_deinit_already_deinitialized(void)
{
  rx_err_t err = pin_validator_deinit(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/* =============================================================================
 * Interface Tests
 * =============================================================================
 */

/**
 * @brief Test getting interface from initialized validator
 */
void test_pin_validator_get_interface_success(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  err = pin_validator_get_interface(&iface, &s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_NOT_NULL(iface.ctx);
  TEST_ASSERT_NOT_NULL(iface.validate_pin);
  TEST_ASSERT_NOT_NULL(iface.reserve_pin);
  TEST_ASSERT_NOT_NULL(iface.release_pin);
  TEST_ASSERT_NOT_NULL(iface.is_pin_reserved);
  TEST_ASSERT_NOT_NULL(iface.get_pin_function);
  TEST_ASSERT_NOT_NULL(iface.clear_all_reservations);
}

/**
 * @brief Test getting interface with NULL interface pointer
 */
void test_pin_validator_get_interface_null_iface(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = pin_validator_get_interface(NULL, &s_validator);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test getting interface with NULL validator pointer
 */
void test_pin_validator_get_interface_null_validator(void)
{
  rx_pin_interface_t iface;
  rx_err_t           err = pin_validator_get_interface(&iface, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test getting interface from uninitialized validator
 */
void test_pin_validator_get_interface_not_initialized(void)
{
  rx_pin_interface_t iface;
  rx_err_t           err = pin_validator_get_interface(&iface, &s_validator);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Pin Validation Tests
 * =============================================================================
 */

/**
 * @brief Test validating decimal ports (0-9)
 */
void test_pin_validator_validate_decimal_ports(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  /* Test all decimal ports with pin 0 */
  for (uint8_t port = 0; port <= k_max_decimal_port; port++) {
    err = iface.validate_pin(iface.ctx, port, k_test_pin_0);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
  }
}

/**
 * @brief Test validating hex ports (A-G / 0xA-0x10)
 */
void test_pin_validator_validate_hex_ports(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  /* Test all hex ports with pin 0 */
  for (uint8_t port = k_hex_port_start; port <= k_hex_port_end; port++) {
    err = iface.validate_pin(iface.ctx, port, k_test_pin_0);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
  }
}

/**
 * @brief Test validating all pins (0-7)
 */
void test_pin_validator_validate_all_pins(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  /* Test all pins on port 0 */
  for (uint8_t pin = 0; pin < k_pins_per_port; pin++) {
    err = iface.validate_pin(iface.ctx, k_test_port_0, pin);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
  }
}

/**
 * @brief Test invalid port number
 */
void test_pin_validator_validate_invalid_port(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  err = iface.validate_pin(iface.ctx, k_test_port_bad, k_test_pin_0);
  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/**
 * @brief Test invalid pin number
 */
void test_pin_validator_validate_invalid_pin(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  err = iface.validate_pin(iface.ctx, k_test_port_0, k_test_pin_8);
  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_pin, err);
}

/* =============================================================================
 * Pin Reservation Tests
 * =============================================================================
 */

/**
 * @brief Test successful pin reservation
 */
void test_pin_validator_reserve_success(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  err = iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, "SPI_COPI");
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(iface.is_pin_reserved(iface.ctx, k_test_port_a, k_test_pin_3));
}

/**
 * @brief Test pin reservation with NULL function name
 */
void test_pin_validator_reserve_null_function(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  err = iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test pin reservation with invalid port
 */
void test_pin_validator_reserve_invalid_port(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  err = iface.reserve_pin(iface.ctx, k_test_port_bad, k_test_pin_3, "SPI_COPI");
  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/**
 * @brief Test pin reservation with invalid pin
 */
void test_pin_validator_reserve_invalid_pin(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  err = iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_8, "SPI_COPI");
  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_pin, err);
}

/**
 * @brief Test conflict detection (double reservation)
 */
void test_pin_validator_reserve_conflict(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  /* First reservation succeeds */
  err = iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, "SPI_COPI");
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Second reservation should fail with conflict */
  err = iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, "UART_TX");
  TEST_ASSERT_EQUAL(k_rx_err_gpio_conflict, err);
}

/* =============================================================================
 * Pin Release Tests
 * =============================================================================
 */

/**
 * @brief Test successful pin release
 */
void test_pin_validator_release_success(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  /* Reserve then release */
  iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, "SPI_COPI");
  TEST_ASSERT_TRUE(iface.is_pin_reserved(iface.ctx, k_test_port_a, k_test_pin_3));

  err = iface.release_pin(iface.ctx, k_test_port_a, k_test_pin_3);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(iface.is_pin_reserved(iface.ctx, k_test_port_a, k_test_pin_3));
}

/**
 * @brief Test release of unreserved pin
 */
void test_pin_validator_release_not_reserved(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  err = iface.release_pin(iface.ctx, k_test_port_a, k_test_pin_3);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test release with invalid port
 */
void test_pin_validator_release_invalid_port(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  err = iface.release_pin(iface.ctx, k_test_port_bad, k_test_pin_3);
  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/**
 * @brief Test release with invalid pin
 */
void test_pin_validator_release_invalid_pin(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  err = iface.release_pin(iface.ctx, k_test_port_a, k_test_pin_8);
  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_pin, err);
}

/**
 * @brief Test reservation after release (re-use)
 */
void test_pin_validator_reserve_after_release(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  /* Reserve, release, and reserve again */
  iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, "SPI_COPI");
  iface.release_pin(iface.ctx, k_test_port_a, k_test_pin_3);

  err = iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, "UART_TX");
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(iface.is_pin_reserved(iface.ctx, k_test_port_a, k_test_pin_3));
}

/* =============================================================================
 * Get Pin Function Tests
 * =============================================================================
 */

/**
 * @brief Test getting function name of reserved pin
 */
void test_pin_validator_get_function_success(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, "SPI_COPI");

  char function_out[k_pin_function_name_max_len];
  err = iface.get_pin_function(iface.ctx, k_test_port_a, k_test_pin_3, function_out,
                               sizeof(function_out));
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_STRING("SPI_COPI", function_out);
}

/**
 * @brief Test getting function name with NULL output buffer
 */
void test_pin_validator_get_function_null_output(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, "SPI_COPI");

  err = iface.get_pin_function(iface.ctx, k_test_port_a, k_test_pin_3, NULL,
                               k_pin_function_name_max_len);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test getting function name with too small buffer
 */
void test_pin_validator_get_function_buffer_too_small(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, "SPI_COPI");

  enum {
    k_small_buffer_size = 5,
  };

  char function_out[k_small_buffer_size];
  err = iface.get_pin_function(iface.ctx, k_test_port_a, k_test_pin_3, function_out,
                               sizeof(function_out));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test getting function name of unreserved pin
 */
void test_pin_validator_get_function_not_reserved(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  char function_out[k_pin_function_name_max_len];
  err = iface.get_pin_function(iface.ctx, k_test_port_a, k_test_pin_3, function_out,
                               sizeof(function_out));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Clear All Reservations Tests
 * =============================================================================
 */

/**
 * @brief Test clearing all reservations
 */
void test_pin_validator_clear_all_reservations(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  /* Reserve multiple pins */
  iface.reserve_pin(iface.ctx, k_test_port_0, k_test_pin_0, "GPIO_OUT");
  iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, "SPI_COPI");
  iface.reserve_pin(iface.ctx, k_test_port_b, k_test_pin_7, "I2C_SDA");

  TEST_ASSERT_TRUE(iface.is_pin_reserved(iface.ctx, k_test_port_0, k_test_pin_0));
  TEST_ASSERT_TRUE(iface.is_pin_reserved(iface.ctx, k_test_port_a, k_test_pin_3));
  TEST_ASSERT_TRUE(iface.is_pin_reserved(iface.ctx, k_test_port_b, k_test_pin_7));

  /* Clear all */
  err = iface.clear_all_reservations(iface.ctx);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_FALSE(iface.is_pin_reserved(iface.ctx, k_test_port_0, k_test_pin_0));
  TEST_ASSERT_FALSE(iface.is_pin_reserved(iface.ctx, k_test_port_a, k_test_pin_3));
  TEST_ASSERT_FALSE(iface.is_pin_reserved(iface.ctx, k_test_port_b, k_test_pin_7));
}

/* =============================================================================
 * Port Coverage Tests (All 17 Ports)
 * =============================================================================
 */

/**
 * @brief Test reserving pins on all 17 ports
 */
void test_pin_validator_all_ports_coverage(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  /* Test decimal ports 0-9 */
  for (uint8_t port = 0; port <= k_max_decimal_port; port++) {
    char function_name[k_pin_function_name_max_len];
    snprintf(function_name, sizeof(function_name), "PORT%d_PIN0", port);

    err = iface.reserve_pin(iface.ctx, port, k_test_pin_0, function_name);
    TEST_ASSERT_EQUAL_MESSAGE(k_rx_ok, err, "Failed to reserve decimal port");
    TEST_ASSERT_TRUE(iface.is_pin_reserved(iface.ctx, port, k_test_pin_0));
  }

  /* Test hex ports A-G (0xA-0x10) */
  for (uint8_t port = k_hex_port_start; port <= k_hex_port_end; port++) {
    char function_name[k_pin_function_name_max_len];
    snprintf(function_name, sizeof(function_name), "PORT%X_PIN0", port);

    err = iface.reserve_pin(iface.ctx, port, k_test_pin_0, function_name);
    TEST_ASSERT_EQUAL_MESSAGE(k_rx_ok, err, "Failed to reserve hex port");
    TEST_ASSERT_TRUE(iface.is_pin_reserved(iface.ctx, port, k_test_pin_0));
  }
}

/**
 * @brief Test reserving all 8 pins on a single port
 */
void test_pin_validator_all_pins_on_port(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  /* Reserve all 8 pins on port A */
  for (uint8_t pin = 0; pin < k_pins_per_port; pin++) {
    char function_name[k_pin_function_name_max_len];
    snprintf(function_name, sizeof(function_name), "FUNC_PIN%d", pin);

    err = iface.reserve_pin(iface.ctx, k_test_port_a, pin, function_name);
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_TRUE(iface.is_pin_reserved(iface.ctx, k_test_port_a, pin));
  }
}

/* =============================================================================
 * Interface Validation Tests
 * =============================================================================
 */

/**
 * @brief Test interface validation with valid interface
 */
void test_pin_interface_validate_success(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  err = rx_pin_interface_validate(&iface);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test interface validation with NULL
 */
void test_pin_interface_validate_null(void)
{
  rx_err_t err = rx_pin_interface_validate(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test interface validation with missing function pointers
 */
void test_pin_interface_validate_missing_functions(void)
{
  rx_pin_interface_t iface;
  memset(&iface, 0, sizeof(iface));

  rx_err_t err = rx_pin_interface_validate(&iface);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Edge Case Tests
 * =============================================================================
 */

/**
 * @brief Test is_pin_reserved with invalid port returns false
 */
void test_pin_validator_is_reserved_invalid_port(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  bool reserved = iface.is_pin_reserved(iface.ctx, k_test_port_bad, k_test_pin_0);
  TEST_ASSERT_FALSE(reserved);
}

/**
 * @brief Test is_pin_reserved with invalid pin returns false
 */
void test_pin_validator_is_reserved_invalid_pin(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  bool reserved = iface.is_pin_reserved(iface.ctx, k_test_port_0, k_test_pin_8);
  TEST_ASSERT_FALSE(reserved);
}

/**
 * @brief Test long function name truncation
 */
void test_pin_validator_long_function_name(void)
{
  rx_err_t err = pin_validator_init(&s_validator);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  rx_pin_interface_t iface;
  pin_validator_get_interface(&iface, &s_validator);

  /* Create a very long function name */
  char long_name[64];
  memset(long_name, 'A', sizeof(long_name) - 1);
  long_name[sizeof(long_name) - 1] = '\0';

  err = iface.reserve_pin(iface.ctx, k_test_port_a, k_test_pin_3, long_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify truncation */
  char function_out[k_pin_function_name_max_len];
  err = iface.get_pin_function(iface.ctx, k_test_port_a, k_test_pin_3, function_out,
                               sizeof(function_out));
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_pin_function_name_max_len - 1, strlen(function_out));
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_pin_validator_init_success);
  RUN_TEST(test_pin_validator_init_null_pointer);
  RUN_TEST(test_pin_validator_init_clears_reservations);

  /* Deinitialization tests */
  RUN_TEST(test_pin_validator_deinit_success);
  RUN_TEST(test_pin_validator_deinit_null_pointer);
  RUN_TEST(test_pin_validator_deinit_already_deinitialized);

  /* Interface tests */
  RUN_TEST(test_pin_validator_get_interface_success);
  RUN_TEST(test_pin_validator_get_interface_null_iface);
  RUN_TEST(test_pin_validator_get_interface_null_validator);
  RUN_TEST(test_pin_validator_get_interface_not_initialized);

  /* Pin validation tests */
  RUN_TEST(test_pin_validator_validate_decimal_ports);
  RUN_TEST(test_pin_validator_validate_hex_ports);
  RUN_TEST(test_pin_validator_validate_all_pins);
  RUN_TEST(test_pin_validator_validate_invalid_port);
  RUN_TEST(test_pin_validator_validate_invalid_pin);

  /* Pin reservation tests */
  RUN_TEST(test_pin_validator_reserve_success);
  RUN_TEST(test_pin_validator_reserve_null_function);
  RUN_TEST(test_pin_validator_reserve_invalid_port);
  RUN_TEST(test_pin_validator_reserve_invalid_pin);
  RUN_TEST(test_pin_validator_reserve_conflict);

  /* Pin release tests */
  RUN_TEST(test_pin_validator_release_success);
  RUN_TEST(test_pin_validator_release_not_reserved);
  RUN_TEST(test_pin_validator_release_invalid_port);
  RUN_TEST(test_pin_validator_release_invalid_pin);
  RUN_TEST(test_pin_validator_reserve_after_release);

  /* Get function tests */
  RUN_TEST(test_pin_validator_get_function_success);
  RUN_TEST(test_pin_validator_get_function_null_output);
  RUN_TEST(test_pin_validator_get_function_buffer_too_small);
  RUN_TEST(test_pin_validator_get_function_not_reserved);

  /* Clear all reservations tests */
  RUN_TEST(test_pin_validator_clear_all_reservations);

  /* Port coverage tests */
  RUN_TEST(test_pin_validator_all_ports_coverage);
  RUN_TEST(test_pin_validator_all_pins_on_port);

  /* Interface validation tests */
  RUN_TEST(test_pin_interface_validate_success);
  RUN_TEST(test_pin_interface_validate_null);
  RUN_TEST(test_pin_interface_validate_missing_functions);

  /* Edge case tests */
  RUN_TEST(test_pin_validator_is_reserved_invalid_port);
  RUN_TEST(test_pin_validator_is_reserved_invalid_pin);
  RUN_TEST(test_pin_validator_long_function_name);

  return UNITY_END();
}
