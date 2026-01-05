/* tests/test_rx_motor.c */

/**
 * @file test_rx_motor.c
 * @brief Comprehensive Unit Tests for RX Motor Driver (GPTW-based)
 * @details
 * Tests motor initialization, duty cycle control, direction changes,
 * brake/coast modes, duty cycle clamping, and parameter validation
 * using the GPTW mock implementation.
 *
 * Test Coverage:
 * - Initialization and deinitialization
 * - Duty cycle control (forward/reverse)
 * - Duty cycle clamping at boundaries
 * - Forward/reverse transitions
 * - Brake and coast modes
 * - PWM frequency configurations
 * - Dead-time configuration
 * - Parameter validation (NULL checks, bounds)
 * - State machine validation
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <math.h>
#include <string.h>

#include "mock_rx_gptw.h"
#include "rx_motor.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/** @brief Float comparison tolerance */
typedef enum {
  k_float_tolerance_percent = 1, /**< 0.01 tolerance for percentage comparisons */
} test_constants_t;

/** @brief Common test PWM frequencies in Hz */
typedef enum {
  k_test_freq_20khz = 20000, /**< Standard motor PWM frequency */
  k_test_freq_25khz = 25000, /**< Alternative motor PWM frequency */
  k_test_freq_1khz  = 1000,  /**< Low frequency for testing */
  k_test_freq_50khz = 50000, /**< High frequency for testing */
} test_frequencies_t;

/** @brief Common test dead-time values in nanoseconds */
typedef enum {
  k_test_deadtime_0ns    = 0,    /**< No dead-time */
  k_test_deadtime_500ns  = 500,  /**< 500ns dead-time */
  k_test_deadtime_1000ns = 1000, /**< 1us dead-time */
  k_test_deadtime_2000ns = 2000, /**< 2us dead-time */
} test_deadtimes_t;

/** @brief Duty cycle test values */
typedef enum {
  k_duty_full_forward = 100,  /**< Full forward duty */
  k_duty_half         = 50,   /**< Half duty */
  k_duty_quarter      = 25,   /**< Quarter duty */
  k_duty_zero         = 0,    /**< Zero duty */
  k_duty_full_reverse = -100, /**< Full reverse duty */
  k_duty_over_max     = 150,  /**< Over maximum for clamping test */
  k_duty_under_min    = -150, /**< Under minimum for clamping test */
} test_duty_values_t;

static const float s_float_tolerance = 0.01f;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static rx_motor_handle_t s_motor;
static rx_motor_config_t s_config;

/**
 * @brief Setup function run before each test
 */
void setUp(void)
{
  mock_gptw_reset();
  memset(&s_motor, 0, sizeof(s_motor));

  /* Default config for channel 0 */
  s_config.channel      = k_gptw_channel_0;
  s_config.output_a     = k_gptw_output_a;
  s_config.output_b     = k_gptw_output_b;
  s_config.pwm_freq_hz  = (uint32_t)k_test_freq_20khz;
  s_config.dead_time_ns = (uint32_t)k_test_deadtime_1000ns;
  s_config.invert_pwm   = false;
}

/**
 * @brief Teardown function run after each test
 */
void tearDown(void)
{
  /* No cleanup needed - mock_gptw_reset() handles state */
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

void test_motor_init_success(void)
{
  rx_err_t err = rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_motor.initialized);
  TEST_ASSERT_EQUAL(s_config.channel, s_motor.channel);
  TEST_ASSERT_EQUAL(s_config.output_a, s_motor.output_a);
  TEST_ASSERT_EQUAL(s_config.output_b, s_motor.output_b);
  TEST_ASSERT_EQUAL(s_config.pwm_freq_hz, s_motor.pwm_freq_hz);
  TEST_ASSERT_FALSE(s_motor.invert_pwm);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, 0.0f, s_motor.current_duty);
}

void test_motor_init_null_handle_fails(void)
{
  rx_err_t err = rx_motor_init(NULL, &s_config);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_motor_init_null_config_fails(void)
{
  rx_err_t err = rx_motor_init(&s_motor, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_motor_init_gptw_initialized(void)
{
  rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_TRUE(mock_gptw_is_initialized(k_gptw_channel_0));
  TEST_ASSERT_TRUE(mock_gptw_is_running(k_gptw_channel_0));
}

void test_motor_init_outputs_start_at_zero(void)
{
  rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_init_already_initialized_fails(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_motor_init_frequency_20khz(void)
{
  s_config.pwm_freq_hz = (uint32_t)k_test_freq_20khz;

  rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_EQUAL_UINT32((uint32_t)k_test_freq_20khz, mock_gptw_get_frequency(k_gptw_channel_0));
}

void test_motor_init_frequency_25khz(void)
{
  s_config.pwm_freq_hz = (uint32_t)k_test_freq_25khz;

  rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_EQUAL_UINT32((uint32_t)k_test_freq_25khz, mock_gptw_get_frequency(k_gptw_channel_0));
}

void test_motor_init_with_invert_pwm(void)
{
  s_config.invert_pwm = true;

  rx_err_t err = rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_motor.invert_pwm);
}

void test_motor_init_channel_1(void)
{
  s_config.channel = k_gptw_channel_1;

  rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_TRUE(mock_gptw_is_initialized(k_gptw_channel_1));
  TEST_ASSERT_FALSE(mock_gptw_is_initialized(k_gptw_channel_0));
}

void test_motor_init_channel_2(void)
{
  s_config.channel = k_gptw_channel_2;

  rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_TRUE(mock_gptw_is_initialized(k_gptw_channel_2));
}

void test_motor_init_channel_3(void)
{
  s_config.channel = k_gptw_channel_3;

  rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_TRUE(mock_gptw_is_initialized(k_gptw_channel_3));
}

/* =============================================================================
 * Deinitialization Tests
 * =============================================================================
 */

void test_motor_deinit_success(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_deinit(&s_motor);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(s_motor.initialized);
}

void test_motor_deinit_null_handle_fails(void)
{
  rx_err_t err = rx_motor_deinit(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_motor_deinit_not_initialized_fails(void)
{
  rx_err_t err = rx_motor_deinit(&s_motor);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_motor_deinit_gptw_deinitialized(void)
{
  rx_motor_init(&s_motor, &s_config);
  rx_motor_deinit(&s_motor);

  TEST_ASSERT_FALSE(mock_gptw_is_initialized(k_gptw_channel_0));
}

void test_motor_deinit_stops_motor_first(void)
{
  rx_motor_init(&s_motor, &s_config);
  rx_motor_set_duty(&s_motor, 50.0f);

  rx_motor_deinit(&s_motor);

  /* After deinit, GPTW is deinitialized (duty reads as 0) */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

/* =============================================================================
 * Set Duty Tests - Forward Direction
 * =============================================================================
 */

void test_motor_set_duty_forward_50_percent(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, 50.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* PH/EN mode: PH (output_a) = 100% for forward, EN (output_b) = speed */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           50.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_set_duty_forward_100_percent(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, 100.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_set_duty_forward_25_percent(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, 25.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           25.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_set_duty_zero(void)
{
  rx_motor_init(&s_motor, &s_config);
  rx_motor_set_duty(&s_motor, 50.0f); /* First set to non-zero */

  rx_err_t err = rx_motor_set_duty(&s_motor, 0.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Zero duty: PH = 100% (forward direction), EN = 0% */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

/* =============================================================================
 * Set Duty Tests - Reverse Direction
 * =============================================================================
 */

void test_motor_set_duty_reverse_50_percent(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, -50.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* PH/EN mode: PH (output_a) = 0% for reverse, EN (output_b) = speed */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           50.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_set_duty_reverse_100_percent(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, -100.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_set_duty_reverse_75_percent(void)
{
  s_config.channel = k_gptw_channel_1;
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, -75.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_1, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           75.0f,
                           mock_gptw_get_duty(k_gptw_channel_1, k_gptw_output_b));
}

/* =============================================================================
 * Duty Cycle Clamping Tests
 * =============================================================================
 */

void test_motor_set_duty_clamp_above_100(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, 150.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Should clamp to 100% */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_set_duty_clamp_below_minus_100(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, -150.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Should clamp to -100% (reverse at 100% speed) */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_set_duty_clamp_extreme_positive(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, 1000.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_set_duty_clamp_extreme_negative(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, -1000.0f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

/* =============================================================================
 * Forward/Reverse Transition Tests
 * =============================================================================
 */

void test_motor_transition_forward_to_reverse(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_motor_set_duty(&s_motor, 50.0f);
  /* Verify forward */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));

  rx_motor_set_duty(&s_motor, -50.0f);
  /* Verify reverse */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           50.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_transition_reverse_to_forward(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_motor_set_duty(&s_motor, -75.0f);
  /* Verify reverse */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));

  rx_motor_set_duty(&s_motor, 75.0f);
  /* Verify forward */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           75.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_transition_through_zero(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_motor_set_duty(&s_motor, 50.0f);
  rx_motor_set_duty(&s_motor, 0.0f);
  rx_motor_set_duty(&s_motor, -50.0f);

  /* Final state should be reverse at 50% */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           50.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

/* =============================================================================
 * PWM Inversion Tests
 * =============================================================================
 */

void test_motor_invert_pwm_forward_becomes_reverse(void)
{
  s_config.invert_pwm = true;
  rx_motor_init(&s_motor, &s_config);

  rx_motor_set_duty(&s_motor, 50.0f);

  /* With inversion, +50% input becomes -50% (reverse) */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           50.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_invert_pwm_reverse_becomes_forward(void)
{
  s_config.invert_pwm = true;
  rx_motor_init(&s_motor, &s_config);

  rx_motor_set_duty(&s_motor, -50.0f);

  /* With inversion, -50% input becomes +50% (forward) */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           50.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

/* =============================================================================
 * Stop Tests - Brake Mode
 * =============================================================================
 */

void test_motor_stop_brake_from_running(void)
{
  rx_motor_init(&s_motor, &s_config);
  rx_motor_set_duty(&s_motor, 50.0f);

  rx_err_t err = rx_motor_stop(&s_motor, true);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Brake not supported in PH/EN mode - falls back to coast (both 0%) */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_stop_brake_sets_duty_zero(void)
{
  rx_motor_init(&s_motor, &s_config);
  rx_motor_set_duty(&s_motor, 75.0f);
  rx_motor_stop(&s_motor, true);

  float duty;
  rx_motor_get_duty(&s_motor, &duty);

  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, 0.0f, duty);
}

/* =============================================================================
 * Stop Tests - Coast Mode
 * =============================================================================
 */

void test_motor_stop_coast_from_running(void)
{
  s_config.channel = k_gptw_channel_3;
  rx_motor_init(&s_motor, &s_config);
  rx_motor_set_duty(&s_motor, 50.0f);

  rx_err_t err = rx_motor_stop(&s_motor, false);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Coast: EN = LOW for high impedance */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_3, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_3, k_gptw_output_b));
}

void test_motor_stop_coast_from_reverse(void)
{
  rx_motor_init(&s_motor, &s_config);
  rx_motor_set_duty(&s_motor, -80.0f);

  rx_err_t err = rx_motor_stop(&s_motor, false);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_stop_null_handle_fails(void)
{
  rx_err_t err = rx_motor_stop(NULL, false);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_motor_stop_not_initialized_fails(void)
{
  rx_err_t err = rx_motor_stop(&s_motor, false);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Get Duty Tests
 * =============================================================================
 */

void test_motor_get_duty_after_set(void)
{
  rx_motor_init(&s_motor, &s_config);
  rx_motor_set_duty(&s_motor, 65.0f);

  float    duty;
  rx_err_t err = rx_motor_get_duty(&s_motor, &duty);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, 65.0f, duty);
}

void test_motor_get_duty_reverse(void)
{
  rx_motor_init(&s_motor, &s_config);
  rx_motor_set_duty(&s_motor, -45.0f);

  float    duty;
  rx_err_t err = rx_motor_get_duty(&s_motor, &duty);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, -45.0f, duty);
}

void test_motor_get_duty_after_stop(void)
{
  rx_motor_init(&s_motor, &s_config);
  rx_motor_set_duty(&s_motor, 80.0f);
  rx_motor_stop(&s_motor, false);

  float duty;
  rx_motor_get_duty(&s_motor, &duty);

  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, 0.0f, duty);
}

void test_motor_get_duty_null_handle_fails(void)
{
  float    duty;
  rx_err_t err = rx_motor_get_duty(NULL, &duty);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_motor_get_duty_null_output_fails(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_get_duty(&s_motor, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_motor_get_duty_not_initialized_fails(void)
{
  float    duty;
  rx_err_t err = rx_motor_get_duty(&s_motor, &duty);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_motor_get_duty_initial_zero(void)
{
  rx_motor_init(&s_motor, &s_config);

  float duty;
  rx_motor_get_duty(&s_motor, &duty);

  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, 0.0f, duty);
}

/* =============================================================================
 * Set Duty Parameter Validation Tests
 * =============================================================================
 */

void test_motor_set_duty_null_handle_fails(void)
{
  rx_err_t err = rx_motor_set_duty(NULL, 50.0f);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_motor_set_duty_not_initialized_fails(void)
{
  rx_err_t err = rx_motor_set_duty(&s_motor, 50.0f);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Multiple Motor Channel Tests
 * =============================================================================
 */

void test_motor_multiple_channels_independent(void)
{
  rx_motor_handle_t motor0 = {0};
  rx_motor_handle_t motor1 = {0};

  rx_motor_config_t config0 = s_config;
  config0.channel           = k_gptw_channel_0;

  rx_motor_config_t config1 = s_config;
  config1.channel           = k_gptw_channel_1;

  rx_motor_init(&motor0, &config0);
  rx_motor_init(&motor1, &config1);

  rx_motor_set_duty(&motor0, 30.0f);
  rx_motor_set_duty(&motor1, -60.0f);

  /* Verify channel 0 is forward at 30% */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           30.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));

  /* Verify channel 1 is reverse at 60% */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_1, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           60.0f,
                           mock_gptw_get_duty(k_gptw_channel_1, k_gptw_output_b));
}

void test_motor_all_four_channels(void)
{
  rx_motor_handle_t motors[4] = {0};
  rx_motor_config_t configs[4];

  /* Initialize all 4 motors */
  for (int32_t i = 0; i < 4; i++) {
    configs[i]         = s_config;
    configs[i].channel = (rx_gptw_channel_t)i;
    rx_motor_init(&motors[i], &configs[i]);
  }

  /* Verify all channels initialized */
  for (int32_t i = 0; i < 4; i++) {
    TEST_ASSERT_TRUE(mock_gptw_is_initialized((rx_gptw_channel_t)i));
  }
}

/* =============================================================================
 * Edge Case Tests
 * =============================================================================
 */

void test_motor_set_duty_small_positive(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, 0.1f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Very small positive - still forward direction */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           100.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.1f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_set_duty_small_negative(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_err_t err = rx_motor_set_duty(&s_motor, -0.1f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Very small negative - reverse direction */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.0f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a));
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance,
                           0.1f,
                           mock_gptw_get_duty(k_gptw_channel_0, k_gptw_output_b));
}

void test_motor_reinit_after_deinit(void)
{
  rx_motor_init(&s_motor, &s_config);
  rx_motor_set_duty(&s_motor, 50.0f);
  rx_motor_deinit(&s_motor);

  /* Reinitialize */
  rx_err_t err = rx_motor_init(&s_motor, &s_config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_motor.initialized);

  /* Duty should be back to 0 after reinit */
  float duty;
  rx_motor_get_duty(&s_motor, &duty);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, 0.0f, duty);
}

/* =============================================================================
 * Duty Cycle Tracking Tests
 * =============================================================================
 */

void test_motor_duty_tracking_updates_on_set(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_motor_set_duty(&s_motor, 25.0f);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, 25.0f, s_motor.current_duty);

  rx_motor_set_duty(&s_motor, -75.0f);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, -75.0f, s_motor.current_duty);

  rx_motor_set_duty(&s_motor, 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, 0.0f, s_motor.current_duty);
}

void test_motor_duty_tracking_clamped_value(void)
{
  rx_motor_init(&s_motor, &s_config);

  rx_motor_set_duty(&s_motor, 200.0f);
  /* Stored value should be clamped */
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, 100.0f, s_motor.current_duty);

  rx_motor_set_duty(&s_motor, -200.0f);
  TEST_ASSERT_FLOAT_WITHIN(s_float_tolerance, -100.0f, s_motor.current_duty);
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_motor_init_success);
  RUN_TEST(test_motor_init_null_handle_fails);
  RUN_TEST(test_motor_init_null_config_fails);
  RUN_TEST(test_motor_init_gptw_initialized);
  RUN_TEST(test_motor_init_outputs_start_at_zero);
  RUN_TEST(test_motor_init_already_initialized_fails);
  RUN_TEST(test_motor_init_frequency_20khz);
  RUN_TEST(test_motor_init_frequency_25khz);
  RUN_TEST(test_motor_init_with_invert_pwm);
  RUN_TEST(test_motor_init_channel_1);
  RUN_TEST(test_motor_init_channel_2);
  RUN_TEST(test_motor_init_channel_3);

  /* Deinitialization tests */
  RUN_TEST(test_motor_deinit_success);
  RUN_TEST(test_motor_deinit_null_handle_fails);
  RUN_TEST(test_motor_deinit_not_initialized_fails);
  RUN_TEST(test_motor_deinit_gptw_deinitialized);
  RUN_TEST(test_motor_deinit_stops_motor_first);

  /* Forward duty cycle tests */
  RUN_TEST(test_motor_set_duty_forward_50_percent);
  RUN_TEST(test_motor_set_duty_forward_100_percent);
  RUN_TEST(test_motor_set_duty_forward_25_percent);
  RUN_TEST(test_motor_set_duty_zero);

  /* Reverse duty cycle tests */
  RUN_TEST(test_motor_set_duty_reverse_50_percent);
  RUN_TEST(test_motor_set_duty_reverse_100_percent);
  RUN_TEST(test_motor_set_duty_reverse_75_percent);

  /* Duty cycle clamping tests */
  RUN_TEST(test_motor_set_duty_clamp_above_100);
  RUN_TEST(test_motor_set_duty_clamp_below_minus_100);
  RUN_TEST(test_motor_set_duty_clamp_extreme_positive);
  RUN_TEST(test_motor_set_duty_clamp_extreme_negative);

  /* Forward/reverse transition tests */
  RUN_TEST(test_motor_transition_forward_to_reverse);
  RUN_TEST(test_motor_transition_reverse_to_forward);
  RUN_TEST(test_motor_transition_through_zero);

  /* PWM inversion tests */
  RUN_TEST(test_motor_invert_pwm_forward_becomes_reverse);
  RUN_TEST(test_motor_invert_pwm_reverse_becomes_forward);

  /* Brake mode tests */
  RUN_TEST(test_motor_stop_brake_from_running);
  RUN_TEST(test_motor_stop_brake_sets_duty_zero);

  /* Coast mode tests */
  RUN_TEST(test_motor_stop_coast_from_running);
  RUN_TEST(test_motor_stop_coast_from_reverse);
  RUN_TEST(test_motor_stop_null_handle_fails);
  RUN_TEST(test_motor_stop_not_initialized_fails);

  /* Get duty tests */
  RUN_TEST(test_motor_get_duty_after_set);
  RUN_TEST(test_motor_get_duty_reverse);
  RUN_TEST(test_motor_get_duty_after_stop);
  RUN_TEST(test_motor_get_duty_null_handle_fails);
  RUN_TEST(test_motor_get_duty_null_output_fails);
  RUN_TEST(test_motor_get_duty_not_initialized_fails);
  RUN_TEST(test_motor_get_duty_initial_zero);

  /* Set duty parameter validation tests */
  RUN_TEST(test_motor_set_duty_null_handle_fails);
  RUN_TEST(test_motor_set_duty_not_initialized_fails);

  /* Multiple channel tests */
  RUN_TEST(test_motor_multiple_channels_independent);
  RUN_TEST(test_motor_all_four_channels);

  /* Edge case tests */
  RUN_TEST(test_motor_set_duty_small_positive);
  RUN_TEST(test_motor_set_duty_small_negative);
  RUN_TEST(test_motor_reinit_after_deinit);

  /* Duty cycle tracking tests */
  RUN_TEST(test_motor_duty_tracking_updates_on_set);
  RUN_TEST(test_motor_duty_tracking_clamped_value);

  return UNITY_END();
}
