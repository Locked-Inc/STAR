/* tests/test_gpio_hal.c */

/**
 * @file test_gpio_hal.c
 * @brief Unit Tests for GPIO HAL Functions
 *
 * Tests GPIO HAL parameter validation, error handling, and state management.
 * Uses mock GPIO implementation for host-side testing.
 *
 * Test Coverage:
 * - gpio_set_output(): Direction configuration, port/pin validation
 * - gpio_set_input(): Direction configuration, port/pin validation
 * - gpio_write_high(): Output value setting, validation
 * - gpio_write_low(): Output value setting, validation
 * - gpio_toggle(): Output value toggling, validation
 * - gpio_read(): Input reading, null pointer check, validation
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "unity.h"

#include "hardware_pinout.h"
#include "mock_gpio_hal.h"
#include "rx_err.h"

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

void setUp(void)
{
  mock_gpio_init();
}

void tearDown(void)
{
  mock_gpio_reset();
}

/* =============================================================================
 * gpio_set_output() Tests
 * =============================================================================
 */

/**
 * @brief Test gpio_set_output() with valid pin
 */
void test_gpio_set_output_valid_pin(void)
{
  rx_err_t err = gpio_set_output(k_gpio_pb2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, mock_gpio_get_direction(k_gpio_pb2));
}

/**
 * @brief Test gpio_set_output() with multiple valid pins
 */
void test_gpio_set_output_multiple_pins(void)
{
  rx_err_t err;

  err = gpio_set_output(k_gpio_pa0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_pc6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_pe5);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, mock_gpio_get_direction(k_gpio_pa0));
  TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, mock_gpio_get_direction(k_gpio_pc6));
  TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, mock_gpio_get_direction(k_gpio_pe5));
}

/**
 * @brief Test gpio_set_output() with invalid port
 */
void test_gpio_set_output_invalid_port(void)
{
  /* Port 0x15 is not valid - beyond Port J (0x13) */
  gpio_pin_t invalid_pin = gpio_pin_make(0x15, 0);

  rx_err_t err = gpio_set_output(invalid_pin);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/**
 * @brief Test gpio_set_output() with invalid pin number
 */
void test_gpio_set_output_invalid_pin(void)
{
  /* Pin 8 is invalid (only 0-7 allowed) */
  gpio_pin_t invalid_pin = gpio_pin_make(k_rx_port_b, 8);

  rx_err_t err = gpio_set_output(invalid_pin);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_pin, err);
}

/**
 * @brief Test gpio_set_output() with error injection
 */
void test_gpio_set_output_error_injection(void)
{
  mock_gpio_set_next_error(k_rx_err_gpio_conflict);

  rx_err_t err = gpio_set_output(k_gpio_pb2);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_conflict, err);
}

/**
 * @brief Test gpio_set_output() call history recording
 */
void test_gpio_set_output_call_history(void)
{
  (void)gpio_set_output(k_gpio_pb2);
  (void)gpio_set_output(k_gpio_pa0);

  TEST_ASSERT_EQUAL(2, mock_gpio_get_call_count());

  const mock_gpio_call_t* call0 = mock_gpio_get_call(0);
  TEST_ASSERT_NOT_NULL(call0);
  TEST_ASSERT_EQUAL(k_mock_gpio_call_set_output, call0->type);
  TEST_ASSERT_EQUAL(k_gpio_pb2, call0->pin);

  const mock_gpio_call_t* call1 = mock_gpio_get_call(1);
  TEST_ASSERT_NOT_NULL(call1);
  TEST_ASSERT_EQUAL(k_mock_gpio_call_set_output, call1->type);
  TEST_ASSERT_EQUAL(k_gpio_pa0, call1->pin);
}

/* =============================================================================
 * gpio_set_input() Tests
 * =============================================================================
 */

/**
 * @brief Test gpio_set_input() with valid pin
 */
void test_gpio_set_input_valid_pin(void)
{
  rx_err_t err = gpio_set_input(k_gpio_pb2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_mock_gpio_dir_input, mock_gpio_get_direction(k_gpio_pb2));
}

/**
 * @brief Test gpio_set_input() with invalid port
 */
void test_gpio_set_input_invalid_port(void)
{
  gpio_pin_t invalid_pin = gpio_pin_make(0x18, 0);

  rx_err_t err = gpio_set_input(invalid_pin);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/**
 * @brief Test gpio_set_input() with invalid pin number
 */
void test_gpio_set_input_invalid_pin(void)
{
  gpio_pin_t invalid_pin = gpio_pin_make(k_rx_port_b, 9);

  rx_err_t err = gpio_set_input(invalid_pin);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_pin, err);
}

/* =============================================================================
 * gpio_write_high() Tests
 * =============================================================================
 */

/**
 * @brief Test gpio_write_high() sets output value
 */
void test_gpio_write_high_sets_value(void)
{
  (void)gpio_set_output(k_gpio_pb2);
  rx_err_t err = gpio_write_high(k_gpio_pb2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_gpio_get_output_value(k_gpio_pb2));
}

/**
 * @brief Test gpio_write_high() with invalid port
 */
void test_gpio_write_high_invalid_port(void)
{
  gpio_pin_t invalid_pin = gpio_pin_make(0x20, 0);

  rx_err_t err = gpio_write_high(invalid_pin);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/* =============================================================================
 * gpio_write_low() Tests
 * =============================================================================
 */

/**
 * @brief Test gpio_write_low() clears output value
 */
void test_gpio_write_low_clears_value(void)
{
  (void)gpio_set_output(k_gpio_pb2);
  (void)gpio_write_high(k_gpio_pb2);
  rx_err_t err = gpio_write_low(k_gpio_pb2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_get_output_value(k_gpio_pb2));
}

/**
 * @brief Test gpio_write_low() with invalid port
 */
void test_gpio_write_low_invalid_port(void)
{
  gpio_pin_t invalid_pin = gpio_pin_make(0x25, 0);

  rx_err_t err = gpio_write_low(invalid_pin);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/* =============================================================================
 * gpio_toggle() Tests
 * =============================================================================
 */

/**
 * @brief Test gpio_toggle() toggles output value
 */
void test_gpio_toggle_toggles_value(void)
{
  (void)gpio_set_output(k_gpio_pb2);

  /* Initial state is low (false) */
  TEST_ASSERT_FALSE(mock_gpio_get_output_value(k_gpio_pb2));

  /* Toggle to high */
  rx_err_t err = gpio_toggle(k_gpio_pb2);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_gpio_get_output_value(k_gpio_pb2));

  /* Toggle back to low */
  err = gpio_toggle(k_gpio_pb2);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_get_output_value(k_gpio_pb2));
}

/**
 * @brief Test gpio_toggle() with invalid pin
 */
void test_gpio_toggle_invalid_pin(void)
{
  gpio_pin_t invalid_pin = gpio_pin_make(k_rx_port_b, 10);

  rx_err_t err = gpio_toggle(invalid_pin);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_pin, err);
}

/* =============================================================================
 * gpio_read() Tests
 * =============================================================================
 */

/**
 * @brief Test gpio_read() returns correct input value
 */
void test_gpio_read_returns_input_value(void)
{
  (void)gpio_set_input(k_gpio_pb2);

  /* Set simulated input high */
  mock_gpio_set_input_value(k_gpio_pb2, true);

  bool value = false;
  rx_err_t err = gpio_read(k_gpio_pb2, &value);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(value);

  /* Set simulated input low */
  mock_gpio_set_input_value(k_gpio_pb2, false);

  err = gpio_read(k_gpio_pb2, &value);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(value);
}

/**
 * @brief Test gpio_read() with null pointer
 */
void test_gpio_read_null_pointer(void)
{
  rx_err_t err = gpio_read(k_gpio_pb2, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test gpio_read() with invalid port
 */
void test_gpio_read_invalid_port(void)
{
  gpio_pin_t invalid_pin = gpio_pin_make(0x30, 0);
  bool value;

  rx_err_t err = gpio_read(invalid_pin, &value);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/**
 * @brief Test gpio_read() with invalid pin
 */
void test_gpio_read_invalid_pin(void)
{
  gpio_pin_t invalid_pin = gpio_pin_make(k_rx_port_b, 15);
  bool value;

  rx_err_t err = gpio_read(invalid_pin, &value);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_pin, err);
}

/* =============================================================================
 * Edge Case Tests
 * =============================================================================
 */

/**
 * @brief Test all valid ports
 */
void test_gpio_valid_ports(void)
{
  rx_err_t err;

  /* Test all valid ports (Port 0, 1, 2, 3, 4, 5, A, B, C, D, E, J) */
  err = gpio_set_output(k_gpio_p05);  /* Port 0 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_p12);  /* Port 1 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_p20);  /* Port 2 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_p30);  /* Port 3 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_p40);  /* Port 4 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_p50);  /* Port 5 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_pa0);  /* Port A */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_pb0);  /* Port B */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_pc0);  /* Port C */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_pd0);  /* Port D */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_pe0);  /* Port E */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_gpio_pj3);  /* Port J */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test clear call history
 */
void test_gpio_clear_history(void)
{
  (void)gpio_set_output(k_gpio_pb2);
  (void)gpio_set_output(k_gpio_pa0);
  TEST_ASSERT_EQUAL(2, mock_gpio_get_call_count());

  mock_gpio_clear_history();
  TEST_ASSERT_EQUAL(0, mock_gpio_get_call_count());
}

/* =============================================================================
 * Test Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* gpio_set_output tests */
  RUN_TEST(test_gpio_set_output_valid_pin);
  RUN_TEST(test_gpio_set_output_multiple_pins);
  RUN_TEST(test_gpio_set_output_invalid_port);
  RUN_TEST(test_gpio_set_output_invalid_pin);
  RUN_TEST(test_gpio_set_output_error_injection);
  RUN_TEST(test_gpio_set_output_call_history);

  /* gpio_set_input tests */
  RUN_TEST(test_gpio_set_input_valid_pin);
  RUN_TEST(test_gpio_set_input_invalid_port);
  RUN_TEST(test_gpio_set_input_invalid_pin);

  /* gpio_write_high tests */
  RUN_TEST(test_gpio_write_high_sets_value);
  RUN_TEST(test_gpio_write_high_invalid_port);

  /* gpio_write_low tests */
  RUN_TEST(test_gpio_write_low_clears_value);
  RUN_TEST(test_gpio_write_low_invalid_port);

  /* gpio_toggle tests */
  RUN_TEST(test_gpio_toggle_toggles_value);
  RUN_TEST(test_gpio_toggle_invalid_pin);

  /* gpio_read tests */
  RUN_TEST(test_gpio_read_returns_input_value);
  RUN_TEST(test_gpio_read_null_pointer);
  RUN_TEST(test_gpio_read_invalid_port);
  RUN_TEST(test_gpio_read_invalid_pin);

  /* Edge case tests */
  RUN_TEST(test_gpio_valid_ports);
  RUN_TEST(test_gpio_clear_history);

  return UNITY_END();
}
