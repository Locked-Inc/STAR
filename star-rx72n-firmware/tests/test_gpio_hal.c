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

#include "mock_gpio_hal.h"
#include "rx_err.h"
#include "unity.h"

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

typedef enum {
  k_invalid_port_1 = k_rx_port_j + 1,
  k_invalid_port_2 = k_rx_port_j + 2,
  k_invalid_port_3 = k_rx_port_j + 3,
  k_invalid_port_4 = k_rx_port_j + 4,
  k_invalid_pin_1  = k_rx_pin_max + 1,
  k_invalid_pin_2  = k_rx_pin_max + 2,
  k_invalid_pin_3  = k_rx_pin_max + 3,
  k_invalid_pin_4  = k_rx_pin_max + 4,
} gpio_invalid_constants_t;

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
  rx_err_t err = gpio_set_output(k_rx_pb_2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, mock_gpio_get_direction(k_rx_pb_2));
}

/**
 * @brief Test gpio_set_output() with multiple valid pins
 */
void test_gpio_set_output_multiple_pins(void)
{
  rx_err_t err;

  err = gpio_set_output(k_rx_pa_0);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_pc_6);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_pe_5);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, mock_gpio_get_direction(k_rx_pa_0));
  TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, mock_gpio_get_direction(k_rx_pc_6));
  TEST_ASSERT_EQUAL(k_mock_gpio_dir_output, mock_gpio_get_direction(k_rx_pe_5));
}

/**
 * @brief Test gpio_set_output() with invalid port
 */
void test_gpio_set_output_invalid_port(void)
{
  /* Port 0x15 is not valid - beyond Port J (0x13) */
  enum {
    k_invalid_port = k_rx_port_j + 1,
    k_invalid_pin  = k_rx_pin_0,
  };
  rx_port_pin_t invalid_pin = (rx_port_pin_t)((k_invalid_port << k_port_shift) | k_invalid_pin);

  rx_err_t err = gpio_set_output(invalid_pin);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/**
 * @brief Test gpio_set_output() with invalid pin number
 */
void test_gpio_set_output_invalid_pin(void)
{
  /* Pin 8 is invalid (only 0-7 allowed) */
  enum { k_invalid_pin = k_rx_pin_max + 1 };
  rx_port_pin_t invalid_pin = (rx_port_pin_t)((k_rx_port_b << k_port_shift) | k_invalid_pin);

  rx_err_t err = gpio_set_output(invalid_pin);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_pin, err);
}

/**
 * @brief Test gpio_set_output() with error injection
 */
void test_gpio_set_output_error_injection(void)
{
  mock_gpio_set_next_error(k_rx_err_gpio_conflict);

  rx_err_t err = gpio_set_output(k_rx_pb_2);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_conflict, err);
}

/**
 * @brief Test gpio_set_output() call history recording
 */
void test_gpio_set_output_call_history(void)
{
  rx_err_t err = gpio_set_output(k_rx_pb_2);
  (void)gpio_set_output(k_rx_pa_0);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(2, mock_gpio_get_call_count());

  const mock_gpio_call_t* call0 = mock_gpio_get_call(0);
  TEST_ASSERT_NOT_NULL(call0);
  TEST_ASSERT_EQUAL(k_mock_gpio_call_set_output, call0->type);
  TEST_ASSERT_EQUAL(k_rx_pb_2, call0->pin);

  const mock_gpio_call_t* call1 = mock_gpio_get_call(1);
  TEST_ASSERT_NOT_NULL(call1);
  TEST_ASSERT_EQUAL(k_mock_gpio_call_set_output, call1->type);
  TEST_ASSERT_EQUAL(k_rx_pa_0, call1->pin);
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
  rx_err_t err = gpio_set_input(k_rx_pb_2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_mock_gpio_dir_input, mock_gpio_get_direction(k_rx_pb_2));
}

/**
 * @brief Test gpio_set_input() with invalid port
 */
void test_gpio_set_input_invalid_port(void)
{
  rx_port_pin_t invalid_pin = (rx_port_pin_t)((k_invalid_port_1 << k_port_shift) | k_rx_pin_0);

  rx_err_t err = gpio_set_input(invalid_pin);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/**
 * @brief Test gpio_set_input() with invalid pin number
 */
void test_gpio_set_input_invalid_pin(void)
{
  rx_port_pin_t invalid_pin = (rx_port_pin_t)((k_rx_port_b << k_port_shift) | k_invalid_pin_1);

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
  (void)gpio_set_output(k_rx_pb_2);
  rx_err_t err = gpio_write_high(k_rx_pb_2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_gpio_get_output_value(k_rx_pb_2));
}

/**
 * @brief Test gpio_write_high() with invalid port
 */
void test_gpio_write_high_invalid_port(void)
{
  rx_port_pin_t invalid_pin = (rx_port_pin_t)((k_invalid_port_2 << k_port_shift) | k_rx_pin_0);

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
  (void)gpio_set_output(k_rx_pb_2);
  (void)gpio_write_high(k_rx_pb_2);
  rx_err_t err = gpio_write_low(k_rx_pb_2);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_get_output_value(k_rx_pb_2));
}

/**
 * @brief Test gpio_write_low() with invalid port
 */
void test_gpio_write_low_invalid_port(void)
{
  rx_port_pin_t invalid_pin = (rx_port_pin_t)((k_invalid_port_3 << k_port_shift) | k_rx_pin_0);

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
  (void)gpio_set_output(k_rx_pb_2);

  /* Initial state is low (false) */
  TEST_ASSERT_FALSE(mock_gpio_get_output_value(k_rx_pb_2));

  /* Toggle to high */
  rx_err_t err = gpio_toggle(k_rx_pb_2);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_gpio_get_output_value(k_rx_pb_2));

  /* Toggle back to low */
  err = gpio_toggle(k_rx_pb_2);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_gpio_get_output_value(k_rx_pb_2));
}

/**
 * @brief Test gpio_toggle() with invalid pin
 */
void test_gpio_toggle_invalid_pin(void)
{
  rx_port_pin_t invalid_pin = (rx_port_pin_t)((k_rx_port_b << k_port_shift) | k_invalid_pin_2);

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
  (void)gpio_set_input(k_rx_pb_2);

  /* Set simulated input high */
  mock_gpio_set_input_value(k_rx_pb_2, true);

  bool     value = false;
  rx_err_t err   = gpio_read(k_rx_pb_2, &value);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(value);

  /* Set simulated input low */
  mock_gpio_set_input_value(k_rx_pb_2, false);

  err = gpio_read(k_rx_pb_2, &value);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(value);
}

/**
 * @brief Test gpio_read() with null pointer
 */
void test_gpio_read_null_pointer(void)
{
  rx_err_t err = gpio_read(k_rx_pb_2, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/**
 * @brief Test gpio_read() with invalid port
 */
void test_gpio_read_invalid_port(void)
{
  rx_port_pin_t invalid_pin = (rx_port_pin_t)((k_invalid_port_4 << k_port_shift) | k_rx_pin_0);
  bool          value;

  rx_err_t err = gpio_read(invalid_pin, &value);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_invalid_port, err);
}

/**
 * @brief Test gpio_read() with invalid pin
 */
void test_gpio_read_invalid_pin(void)
{
  rx_port_pin_t invalid_pin = (rx_port_pin_t)((k_rx_port_b << k_port_shift) | k_invalid_pin_3);
  bool          value;

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
  err = gpio_set_output(k_rx_p0_5); /* Port 0 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_p1_2); /* Port 1 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_p2_0); /* Port 2 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_p3_0); /* Port 3 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_p4_0); /* Port 4 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_p5_0); /* Port 5 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_pa_0); /* Port A */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_pb_0); /* Port B */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_pc_0); /* Port C */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_pd_0); /* Port D */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_pe_0); /* Port E */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = gpio_set_output(k_rx_pj_3); /* Port J */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test clear call history
 */
void test_gpio_clear_history(void)
{
  (void)gpio_set_output(k_rx_pb_2);
  (void)gpio_set_output(k_rx_pa_0);
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
