/**
 * @file test_rx_hcsr04.c
 * @brief Unit Tests for HC-SR04 Ultrasonic Distance Sensor Driver
 *
 * Tests the HC-SR04 driver using mock GPIO and timing functions.
 * All tests run on the host (not on RX72N hardware).
 *
 * STAR Project - Texas A&M University
 * January 2026
 */

#include "unity.h"

#include <string.h>

#include "mock_hcsr04_hw.h"
#include "rx_hcsr04.h"

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static rx_hcsr04_t        s_sensor;
static rx_hcsr04_config_t s_config;

/**
 * @brief Setup function run before each test
 */
void
setUp(void)
{
  /* Initialize mock hardware */
  mock_hcsr04_hw_init(NULL);

  /* Reset sensor handle */
  memset(&s_sensor, 0, sizeof(s_sensor));

  /* Setup default config for sensor 1 (J24) */
  s_config.trigger_port = 0xC;
  s_config.trigger_pin  = 6;
  s_config.echo_port    = 0x5;
  s_config.echo_pin     = 5;
  s_config.timeout_us   = 30000;
}

/**
 * @brief Teardown function run after each test
 */
void
tearDown(void)
{
  mock_hcsr04_hw_deinit(NULL);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

void
test_hcsr04_init_success(void)
{
  rx_err_t err = rx_hcsr04_init(&s_sensor, &s_config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_sensor.initialized);
  TEST_ASSERT_EQUAL(s_config.trigger_port, s_sensor.trigger_port);
  TEST_ASSERT_EQUAL(s_config.trigger_pin, s_sensor.trigger_pin);
  TEST_ASSERT_EQUAL(s_config.echo_port, s_sensor.echo_port);
  TEST_ASSERT_EQUAL(s_config.echo_pin, s_sensor.echo_pin);
}

void
test_hcsr04_init_null_handle_fails(void)
{
  rx_err_t err = rx_hcsr04_init(NULL, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void
test_hcsr04_init_null_config_fails(void)
{
  rx_err_t err = rx_hcsr04_init(&s_sensor, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void
test_hcsr04_init_configures_gpio(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  /* Verify GPIO calls were made */
  TEST_ASSERT_TRUE(mock_hcsr04_hw_was_called(NULL, "gpio_set_output"));
  TEST_ASSERT_TRUE(mock_hcsr04_hw_was_called(NULL, "gpio_set_input"));
}

void
test_hcsr04_init_gpio_error_fails(void)
{
  mock_hcsr04_hw_set_gpio_error(NULL, true);

  rx_err_t err = rx_hcsr04_init(&s_sensor, &s_config);

  TEST_ASSERT_EQUAL(k_rx_err_hw_init_failed, err);
  TEST_ASSERT_FALSE(s_sensor.initialized);
}

void
test_hcsr04_init_pin_conflict_fails(void)
{
  mock_hcsr04_hw_set_pin_conflict(NULL, true);

  rx_err_t err = rx_hcsr04_init(&s_sensor, &s_config);

  TEST_ASSERT_EQUAL(k_rx_err_gpio_conflict, err);
  TEST_ASSERT_FALSE(s_sensor.initialized);
}

void
test_hcsr04_init_twice_fails(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  rx_err_t err = rx_hcsr04_init(&s_sensor, &s_config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Deinitialization Tests
 * =============================================================================
 */

void
test_hcsr04_deinit_success(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  rx_err_t err = rx_hcsr04_deinit(&s_sensor);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_sensor.initialized);
}

void
test_hcsr04_deinit_null_fails(void)
{
  rx_err_t err = rx_hcsr04_deinit(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void
test_hcsr04_deinit_not_initialized_fails(void)
{
  rx_err_t err = rx_hcsr04_deinit(&s_sensor);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Blocking Measurement Tests
 * =============================================================================
 */

void
test_hcsr04_measure_10cm(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  /* Set simulated distance to 10cm */
  mock_hcsr04_hw_set_distance(NULL, 10.0f);
  mock_hcsr04_hw_set_auto_advance(NULL, true, 10);

  float    distance_cm;
  rx_err_t err = rx_hcsr04_measure_blocking(&s_sensor, &distance_cm);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 10.0f, distance_cm);
}

void
test_hcsr04_measure_100cm(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  mock_hcsr04_hw_set_distance(NULL, 100.0f);
  mock_hcsr04_hw_set_auto_advance(NULL, true, 10);

  float    distance_cm;
  rx_err_t err = rx_hcsr04_measure_blocking(&s_sensor, &distance_cm);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(3.0f, 100.0f, distance_cm);
}

void
test_hcsr04_measure_max_range_400cm(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  mock_hcsr04_hw_set_distance(NULL, 400.0f);
  mock_hcsr04_hw_set_auto_advance(NULL, true, 10);

  float    distance_cm;
  rx_err_t err = rx_hcsr04_measure_blocking(&s_sensor, &distance_cm);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(10.0f, 400.0f, distance_cm);
}

void
test_hcsr04_measure_timeout(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  mock_hcsr04_hw_set_timeout(NULL, true);
  mock_hcsr04_hw_set_auto_advance(NULL, true, 100);

  float    distance_cm;
  rx_err_t err = rx_hcsr04_measure_blocking(&s_sensor, &distance_cm);

  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

void
test_hcsr04_measure_null_handle_fails(void)
{
  float    distance_cm;
  rx_err_t err = rx_hcsr04_measure_blocking(NULL, &distance_cm);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void
test_hcsr04_measure_null_output_fails(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  rx_err_t err = rx_hcsr04_measure_blocking(&s_sensor, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void
test_hcsr04_measure_not_initialized_fails(void)
{
  float    distance_cm;
  rx_err_t err = rx_hcsr04_measure_blocking(&s_sensor, &distance_cm);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void
test_hcsr04_measure_sends_trigger_pulse(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  mock_hcsr04_hw_set_distance(NULL, 50.0f);
  mock_hcsr04_hw_set_auto_advance(NULL, true, 10);

  float distance_cm;
  rx_hcsr04_measure_blocking(&s_sensor, &distance_cm);

  TEST_ASSERT_EQUAL_UINT32(1, mock_hcsr04_hw_get_trigger_count(NULL));
}

/* =============================================================================
 * Full Result Measurement Tests
 * =============================================================================
 */

void
test_hcsr04_measure_full_result(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  mock_hcsr04_hw_set_distance(NULL, 50.0f);
  mock_hcsr04_hw_set_auto_advance(NULL, true, 10);

  rx_hcsr04_result_t result;
  rx_err_t           err = rx_hcsr04_measure(&s_sensor, &result);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(3.0f, 50.0f, result.distance_cm);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 19.7f, result.distance_in); /* 50cm / 2.54 */
  TEST_ASSERT_EQUAL(k_rx_ok, result.status);
}

/* =============================================================================
 * Conversion Tests
 * =============================================================================
 */

void
test_hcsr04_cm_to_inches(void)
{
  float inches = rx_hcsr04_cm_to_inches(2.54f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, inches);

  inches = rx_hcsr04_cm_to_inches(100.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 39.37f, inches);
}

void
test_hcsr04_echo_to_cm(void)
{
  /* 58us per cm */
  float cm = rx_hcsr04_echo_to_cm(580);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 10.0f, cm);

  cm = rx_hcsr04_echo_to_cm(5800);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, cm);
}

/* =============================================================================
 * Statistics Tests
 * =============================================================================
 */

void
test_hcsr04_stats_initial_zero(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  uint32_t measurements, timeouts, range_errors;
  rx_err_t err =
    rx_hcsr04_get_stats(&s_sensor, &measurements, &timeouts, &range_errors);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(0, measurements);
  TEST_ASSERT_EQUAL_UINT32(0, timeouts);
  TEST_ASSERT_EQUAL_UINT32(0, range_errors);
}

void
test_hcsr04_stats_increment_on_measurement(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  mock_hcsr04_hw_set_distance(NULL, 50.0f);
  mock_hcsr04_hw_set_auto_advance(NULL, true, 10);

  float distance_cm;
  rx_hcsr04_measure_blocking(&s_sensor, &distance_cm);

  uint32_t measurements;
  rx_hcsr04_get_stats(&s_sensor, &measurements, NULL, NULL);

  TEST_ASSERT_EQUAL_UINT32(1, measurements);
}

void
test_hcsr04_stats_increment_timeout(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  mock_hcsr04_hw_set_timeout(NULL, true);
  mock_hcsr04_hw_set_auto_advance(NULL, true, 100);

  float distance_cm;
  rx_hcsr04_measure_blocking(&s_sensor, &distance_cm);

  uint32_t timeouts;
  rx_hcsr04_get_stats(&s_sensor, NULL, &timeouts, NULL);

  TEST_ASSERT_EQUAL_UINT32(1, timeouts);
}

void
test_hcsr04_stats_reset(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  mock_hcsr04_hw_set_distance(NULL, 50.0f);
  mock_hcsr04_hw_set_auto_advance(NULL, true, 10);

  float distance_cm;
  rx_hcsr04_measure_blocking(&s_sensor, &distance_cm);

  rx_hcsr04_reset_stats(&s_sensor);

  uint32_t measurements;
  rx_hcsr04_get_stats(&s_sensor, &measurements, NULL, NULL);

  TEST_ASSERT_EQUAL_UINT32(0, measurements);
}

void
test_hcsr04_measure_out_of_range_too_close(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);

  /* Simulate 1cm echo (too close, <2cm minimum) */
  mock_hcsr04_hw_set_echo_time(NULL, 58); /* 58us = 1cm */
  mock_hcsr04_hw_set_auto_advance(NULL, true, 10);

  float    distance;
  rx_err_t err = rx_hcsr04_measure_blocking(&s_sensor, &distance);

  TEST_ASSERT_EQUAL(k_rx_err_out_of_range, err);
}

/* =============================================================================
 * Async API Tests
 * =============================================================================
 */

/* Callback tracking for async tests */
static bool               s_async_callback_invoked = false;
static rx_hcsr04_result_t s_async_callback_result;

static void
test_async_callback(rx_hcsr04_t* handle, const rx_hcsr04_result_t* result, void* user_data)
{
  (void)handle;
  (void)user_data;
  s_async_callback_invoked = true;
  s_async_callback_result  = *result;
}

void
test_hcsr04_measure_async_callback_invoked(void)
{
  s_async_callback_invoked = false;
  rx_hcsr04_init(&s_sensor, &s_config);

  /* Simulate 100cm echo */
  mock_hcsr04_hw_set_echo_time(NULL, 5800); /* 5800us = 100cm */
  mock_hcsr04_hw_set_auto_advance(NULL, true, 10);

  rx_err_t err = rx_hcsr04_measure_async(&s_sensor, test_async_callback, NULL);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_async_callback_invoked);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, s_async_callback_result.distance_cm);
  TEST_ASSERT_EQUAL(k_rx_ok, s_async_callback_result.status);
}

void
test_hcsr04_is_busy_initial_false(void)
{
  rx_hcsr04_init(&s_sensor, &s_config);
  TEST_ASSERT_FALSE(rx_hcsr04_is_busy(&s_sensor));
}

void
test_hcsr04_is_busy_null_returns_false(void)
{
  TEST_ASSERT_FALSE(rx_hcsr04_is_busy(NULL));
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

int
main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_hcsr04_init_success);
  RUN_TEST(test_hcsr04_init_null_handle_fails);
  RUN_TEST(test_hcsr04_init_null_config_fails);
  RUN_TEST(test_hcsr04_init_configures_gpio);
  RUN_TEST(test_hcsr04_init_gpio_error_fails);
  RUN_TEST(test_hcsr04_init_pin_conflict_fails);
  RUN_TEST(test_hcsr04_init_twice_fails);

  /* Deinitialization tests */
  RUN_TEST(test_hcsr04_deinit_success);
  RUN_TEST(test_hcsr04_deinit_null_fails);
  RUN_TEST(test_hcsr04_deinit_not_initialized_fails);

  /* Blocking measurement tests */
  RUN_TEST(test_hcsr04_measure_10cm);
  RUN_TEST(test_hcsr04_measure_100cm);
  RUN_TEST(test_hcsr04_measure_max_range_400cm);
  RUN_TEST(test_hcsr04_measure_timeout);
  RUN_TEST(test_hcsr04_measure_out_of_range_too_close);
  RUN_TEST(test_hcsr04_measure_null_handle_fails);
  RUN_TEST(test_hcsr04_measure_null_output_fails);
  RUN_TEST(test_hcsr04_measure_not_initialized_fails);
  RUN_TEST(test_hcsr04_measure_sends_trigger_pulse);

  /* Full result measurement tests */
  RUN_TEST(test_hcsr04_measure_full_result);

  /* Conversion tests */
  RUN_TEST(test_hcsr04_cm_to_inches);
  RUN_TEST(test_hcsr04_echo_to_cm);

  /* Statistics tests */
  RUN_TEST(test_hcsr04_stats_initial_zero);
  RUN_TEST(test_hcsr04_stats_increment_on_measurement);
  RUN_TEST(test_hcsr04_stats_increment_timeout);
  RUN_TEST(test_hcsr04_stats_reset);

  /* Async API tests */
  RUN_TEST(test_hcsr04_measure_async_callback_invoked);
  RUN_TEST(test_hcsr04_is_busy_initial_false);
  RUN_TEST(test_hcsr04_is_busy_null_returns_false);

  return UNITY_END();
}
