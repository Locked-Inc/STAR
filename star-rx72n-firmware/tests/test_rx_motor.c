/**
 * @file test_rx_motor.c
 * @brief Unit Tests for RX Motor Driver (GPTW-based)
 *
 * Tests motor initialization, duty cycle control, brake/coast modes
 * using the GPTW mock implementation.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "rx_motor.h"
#include "mocks/mock_rx_gptw.h"

/* =============================================================================
 * Test Helpers
 * =============================================================================
 */

#define TEST_ASSERT(cond, msg)                                                                     \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      printf("FAIL: %s (line %d): %s\n", __func__, __LINE__, msg);                                 \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)

#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_ASSERT_OK(err)       TEST_ASSERT((err) == k_rx_ok, "Expected k_rx_ok")

#define FLOAT_EPSILON 0.01f
#define TEST_ASSERT_FLOAT_EQ(a, b, msg) TEST_ASSERT(fabsf((a) - (b)) < FLOAT_EPSILON, msg)

static void test_setup(void)
{
  mock_gptw_reset();
}

/* =============================================================================
 * Test Cases
 * =============================================================================
 */

static int test_motor_init_success(void)
{
  test_setup();

  rx_motor_handle_t motor = {0};
  rx_motor_config_t config = {
    .channel      = k_gptw_channel_0,
    .output_a     = k_gptw_output_a,
    .output_b     = k_gptw_output_b,
    .pwm_freq_hz  = 20000,
    .dead_time_ns = 1000,
    .invert_pwm   = false,
  };

  rx_err_t err = rx_motor_init(&motor, &config);
  TEST_ASSERT_OK(err);
  TEST_ASSERT(motor.initialized, "Motor should be initialized");
  TEST_ASSERT(mock_gptw_is_initialized(k_gptw_channel_0), "GPTW should be initialized");
  TEST_ASSERT_EQ(mock_gptw_get_frequency(k_gptw_channel_0), 20000, "Frequency should be 20kHz");

  printf("PASS: %s\n", __func__);
  return 0;
}

static int test_motor_init_null_handle(void)
{
  test_setup();

  rx_motor_config_t config = {
    .channel     = k_gptw_channel_0,
    .pwm_freq_hz = 20000,
  };

  rx_err_t err = rx_motor_init(NULL, &config);
  TEST_ASSERT_EQ(err, k_rx_err_null_pointer, "Should return null pointer error");

  printf("PASS: %s\n", __func__);
  return 0;
}

static int test_motor_init_null_config(void)
{
  test_setup();

  rx_motor_handle_t motor = {0};

  rx_err_t err = rx_motor_init(&motor, NULL);
  TEST_ASSERT_EQ(err, k_rx_err_null_pointer, "Should return null pointer error");

  printf("PASS: %s\n", __func__);
  return 0;
}

static int test_motor_set_duty_forward(void)
{
  test_setup();

  rx_motor_handle_t motor = {0};
  rx_motor_config_t config = {
    .channel      = k_gptw_channel_0,
    .output_a     = k_gptw_output_a,
    .output_b     = k_gptw_output_b,
    .pwm_freq_hz  = 20000,
    .dead_time_ns = 0,
    .invert_pwm   = false,
  };

  rx_motor_init(&motor, &config);

  rx_err_t err = rx_motor_set_duty(&motor, 50.0f);
  TEST_ASSERT_OK(err);

  /* Forward: output_a = 50%, output_b = 0% */
  TEST_ASSERT_FLOAT_EQ(mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a), 50.0f,
                       "Output A should be 50%");
  TEST_ASSERT_FLOAT_EQ(mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b), 0.0f,
                       "Output B should be 0%");

  printf("PASS: %s\n", __func__);
  return 0;
}

static int test_motor_set_duty_reverse(void)
{
  test_setup();

  rx_motor_handle_t motor = {0};
  rx_motor_config_t config = {
    .channel      = k_gptw_channel_1,
    .output_a     = k_gptw_output_a,
    .output_b     = k_gptw_output_b,
    .pwm_freq_hz  = 20000,
    .dead_time_ns = 0,
    .invert_pwm   = false,
  };

  rx_motor_init(&motor, &config);

  rx_err_t err = rx_motor_set_duty(&motor, -75.0f);
  TEST_ASSERT_OK(err);

  /* Reverse: output_a = 0%, output_b = 75% */
  TEST_ASSERT_FLOAT_EQ(mock_gptw_get_duty(k_gptw_channel_1, k_gptw_output_a), 0.0f,
                       "Output A should be 0%");
  TEST_ASSERT_FLOAT_EQ(mock_gptw_get_duty(k_gptw_channel_1, k_gptw_output_b), 75.0f,
                       "Output B should be 75%");

  printf("PASS: %s\n", __func__);
  return 0;
}

static int test_motor_stop_brake(void)
{
  test_setup();

  rx_motor_handle_t motor = {0};
  rx_motor_config_t config = {
    .channel      = k_gptw_channel_2,
    .output_a     = k_gptw_output_a,
    .output_b     = k_gptw_output_b,
    .pwm_freq_hz  = 20000,
    .dead_time_ns = 0,
    .invert_pwm   = false,
  };

  rx_motor_init(&motor, &config);
  rx_motor_set_duty(&motor, 50.0f);

  rx_err_t err = rx_motor_stop(&motor, true); /* brake = true */
  TEST_ASSERT_OK(err);

  /* Brake: both outputs HIGH (100%) */
  TEST_ASSERT_FLOAT_EQ(mock_gptw_get_duty(k_gptw_channel_2, k_gptw_output_a), 100.0f,
                       "Output A should be 100% for brake");
  TEST_ASSERT_FLOAT_EQ(mock_gptw_get_duty(k_gptw_channel_2, k_gptw_output_b), 100.0f,
                       "Output B should be 100% for brake");

  printf("PASS: %s\n", __func__);
  return 0;
}

static int test_motor_stop_coast(void)
{
  test_setup();

  rx_motor_handle_t motor = {0};
  rx_motor_config_t config = {
    .channel      = k_gptw_channel_3,
    .output_a     = k_gptw_output_a,
    .output_b     = k_gptw_output_b,
    .pwm_freq_hz  = 20000,
    .dead_time_ns = 0,
    .invert_pwm   = false,
  };

  rx_motor_init(&motor, &config);
  rx_motor_set_duty(&motor, 50.0f);

  rx_err_t err = rx_motor_stop(&motor, false); /* brake = false (coast) */
  TEST_ASSERT_OK(err);

  /* Coast: both outputs LOW (0%) */
  TEST_ASSERT_FLOAT_EQ(mock_gptw_get_duty(k_gptw_channel_3, k_gptw_output_a), 0.0f,
                       "Output A should be 0% for coast");
  TEST_ASSERT_FLOAT_EQ(mock_gptw_get_duty(k_gptw_channel_3, k_gptw_output_b), 0.0f,
                       "Output B should be 0% for coast");

  printf("PASS: %s\n", __func__);
  return 0;
}

static int test_motor_get_duty(void)
{
  test_setup();

  rx_motor_handle_t motor = {0};
  rx_motor_config_t config = {
    .channel      = k_gptw_channel_0,
    .output_a     = k_gptw_output_a,
    .output_b     = k_gptw_output_b,
    .pwm_freq_hz  = 20000,
    .dead_time_ns = 0,
    .invert_pwm   = false,
  };

  rx_motor_init(&motor, &config);
  rx_motor_set_duty(&motor, 65.0f);

  float duty;
  rx_err_t err = rx_motor_get_duty(&motor, &duty);
  TEST_ASSERT_OK(err);
  TEST_ASSERT_FLOAT_EQ(duty, 65.0f, "Duty should be 65%");

  printf("PASS: %s\n", __func__);
  return 0;
}

static int test_motor_deinit(void)
{
  test_setup();

  rx_motor_handle_t motor = {0};
  rx_motor_config_t config = {
    .channel      = k_gptw_channel_0,
    .output_a     = k_gptw_output_a,
    .output_b     = k_gptw_output_b,
    .pwm_freq_hz  = 20000,
    .dead_time_ns = 0,
    .invert_pwm   = false,
  };

  rx_motor_init(&motor, &config);
  TEST_ASSERT(motor.initialized, "Motor should be initialized");

  rx_err_t err = rx_motor_deinit(&motor);
  TEST_ASSERT_OK(err);
  TEST_ASSERT(!motor.initialized, "Motor should not be initialized after deinit");
  TEST_ASSERT(!mock_gptw_is_initialized(k_gptw_channel_0), "GPTW should be deinitialized");

  printf("PASS: %s\n", __func__);
  return 0;
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  int failures = 0;

  printf("\n=== RX Motor Tests (GPTW-based) ===\n\n");

  failures += test_motor_init_success();
  failures += test_motor_init_null_handle();
  failures += test_motor_init_null_config();
  failures += test_motor_set_duty_forward();
  failures += test_motor_set_duty_reverse();
  failures += test_motor_stop_brake();
  failures += test_motor_stop_coast();
  failures += test_motor_get_duty();
  failures += test_motor_deinit();

  printf("\n=== Results: %d failures ===\n\n", failures);

  return failures;
}
