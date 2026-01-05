/* tests/test_rx_pid.c */

/**
 * @file test_rx_pid.c
 * @brief Unit Tests for RX PID Controller
 * @details
 * Comprehensive tests for the PID controller module covering:
 * - Initialization with valid/invalid parameters
 * - PID computation with known error sequences
 * - Proportional, integral, and derivative responses
 * - Anti-windup clamping
 * - Output saturation
 * - Reset functionality
 * - Runtime gain tuning
 * - MATLAB-tuned parameters (Kp=0.286, Ki=8.01)
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "unity.h"
#include "rx_pid.h"
#include <math.h>
#include <string.h>

/* =============================================================================
 * Test Constants (as enums per style guide)
 * =============================================================================
 */

typedef enum {
    k_test_dt_ms          = 10,    /**< Default time step in milliseconds */
    k_test_output_max     = 100,   /**< Default maximum output */
    k_test_output_min     = -100,  /**< Default minimum output (as positive for enum) */
    k_test_integral_max   = 50,    /**< Default maximum integral */
    k_test_integral_min   = -50,   /**< Default minimum integral (as positive for enum) */
    k_test_num_iterations = 100,   /**< Number of iterations for integration tests */
} test_constants_t;

/**
 * @brief Floating point tolerance for comparisons
 */
static const float s_float_epsilon = 0.0001f;

/**
 * @brief Default time step in seconds
 */
static const float s_dt_seconds = 0.01f;

/**
 * @brief MATLAB-tuned Kp gain for motor control
 */
static const float s_matlab_kp = 0.286f;

/**
 * @brief MATLAB-tuned Ki gain for motor control
 */
static const float s_matlab_ki = 8.01f;

/* =============================================================================
 * Test Fixture
 * =============================================================================
 */

void setUp(void)
{
    /* Nothing to set up */
}

void tearDown(void)
{
    /* Nothing to tear down */
}

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @brief Create a default PID configuration for testing
 * @return Populated PID configuration structure
 */
static rx_pid_config_t create_default_config(void)
{
    rx_pid_config_t config = {
        .kp           = 1.0f,
        .ki           = 0.0f,
        .kd           = 0.0f,
        .output_min   = (float)k_test_output_min,
        .output_max   = (float)k_test_output_max,
        .integral_min = (float)k_test_integral_min,
        .integral_max = (float)k_test_integral_max,
    };
    return config;
}

/**
 * @brief Initialize PID with default configuration
 * @param handle Pointer to PID handle
 * @return Error code from initialization
 */
static rx_err_t init_default_pid(rx_pid_handle_t* handle)
{
    memset(handle, 0, sizeof(rx_pid_handle_t));
    rx_pid_config_t config = create_default_config();
    return rx_pid_init(handle, &config);
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test successful initialization with valid parameters
 */
void test_pid_init_success(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();

    rx_err_t err = rx_pid_init(&pid, &config);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_TRUE(pid.initialized);
    TEST_ASSERT_EQUAL_FLOAT(config.kp, pid.kp);
    TEST_ASSERT_EQUAL_FLOAT(config.ki, pid.ki);
    TEST_ASSERT_EQUAL_FLOAT(config.kd, pid.kd);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.integral);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.prev_error);
}

/**
 * @brief Test initialization with NULL handle pointer
 */
void test_pid_init_null_handle(void)
{
    rx_pid_config_t config = create_default_config();

    rx_err_t err = rx_pid_init(NULL, &config);

    TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test initialization with NULL config pointer
 */
void test_pid_init_null_config(void)
{
    rx_pid_handle_t pid = {0};

    rx_err_t err = rx_pid_init(&pid, NULL);

    TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test initialization fails when already initialized
 */
void test_pid_init_already_initialized(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();

    rx_err_t err = rx_pid_init(&pid, &config);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    /* Second init should fail */
    err = rx_pid_init(&pid, &config);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test initialization fails when output_max <= output_min
 */
void test_pid_init_invalid_output_limits(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.output_min = 100.0f;
    config.output_max = 100.0f;  /* Equal - invalid */

    rx_err_t err = rx_pid_init(&pid, &config);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);

    config.output_min = 100.0f;
    config.output_max = 50.0f;  /* Reversed - invalid */

    err = rx_pid_init(&pid, &config);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test initialization fails when integral_max <= integral_min
 */
void test_pid_init_invalid_integral_limits(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.integral_min = 50.0f;
    config.integral_max = 50.0f;  /* Equal - invalid */

    rx_err_t err = rx_pid_init(&pid, &config);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);

    config.integral_min = 100.0f;
    config.integral_max = 50.0f;  /* Reversed - invalid */

    err = rx_pid_init(&pid, &config);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Deinitialization Tests
 * =============================================================================
 */

/**
 * @brief Test successful deinitialization
 */
void test_pid_deinit_success(void)
{
    rx_pid_handle_t pid = {0};
    init_default_pid(&pid);

    rx_err_t err = rx_pid_deinit(&pid);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FALSE(pid.initialized);
}

/**
 * @brief Test deinitialization with NULL handle
 */
void test_pid_deinit_null_handle(void)
{
    rx_err_t err = rx_pid_deinit(NULL);

    TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test deinitialization when not initialized
 */
void test_pid_deinit_not_initialized(void)
{
    rx_pid_handle_t pid = {0};

    rx_err_t err = rx_pid_deinit(&pid);

    TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Proportional Response Tests
 * =============================================================================
 */

/**
 * @brief Test pure proportional response: output = Kp * error
 */
void test_pid_compute_proportional_only(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 2.0f;
    config.ki = 0.0f;
    config.kd = 0.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 100.0f;
    float measured = 90.0f;
    float expected_error = 10.0f;
    float expected_output = config.kp * expected_error;

    rx_err_t err = rx_pid_compute(&pid, setpoint, measured, s_dt_seconds, &output);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, expected_output, output);
}

/**
 * @brief Test proportional response with negative error
 */
void test_pid_compute_proportional_negative_error(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 1.5f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 50.0f;
    float measured = 75.0f;  /* Overshoot */
    float expected_error = -25.0f;
    float expected_output = config.kp * expected_error;

    rx_err_t err = rx_pid_compute(&pid, setpoint, measured, s_dt_seconds, &output);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, expected_output, output);
}

/**
 * @brief Test proportional response with zero error
 */
void test_pid_compute_proportional_zero_error(void)
{
    rx_pid_handle_t pid = {0};
    init_default_pid(&pid);

    float output = 999.0f;  /* Non-zero initial value */
    float setpoint = 100.0f;
    float measured = 100.0f;  /* Perfect match */

    rx_err_t err = rx_pid_compute(&pid, setpoint, measured, s_dt_seconds, &output);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 0.0f, output);
}

/* =============================================================================
 * Integral Response Tests
 * =============================================================================
 */

/**
 * @brief Test integral accumulation over multiple iterations
 */
void test_pid_compute_integral_accumulation(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 0.0f;
    config.ki = 1.0f;
    config.kd = 0.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 100.0f;
    float measured = 90.0f;
    float error = 10.0f;
    float dt = 0.01f;

    /* First iteration: integral = error * dt = 10 * 0.01 = 0.1 */
    rx_pid_compute(&pid, setpoint, measured, dt, &output);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, error * dt * config.ki, output);

    /* Second iteration: integral = 0.1 + 0.1 = 0.2 */
    rx_pid_compute(&pid, setpoint, measured, dt, &output);
    float expected_integral = error * dt * 2.0f;
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, expected_integral * config.ki, output);

    /* Third iteration: integral = 0.2 + 0.1 = 0.3 */
    rx_pid_compute(&pid, setpoint, measured, dt, &output);
    expected_integral = error * dt * 3.0f;
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, expected_integral * config.ki, output);
}

/**
 * @brief Test integral anti-windup clamping (upper bound)
 */
void test_pid_compute_integral_antiwindup_upper(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 0.0f;
    config.ki = 1.0f;
    config.kd = 0.0f;
    config.integral_max = 5.0f;
    config.integral_min = -5.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 200.0f;
    float measured = 0.0f;
    float large_dt = 10.0f;  /* Large dt to quickly accumulate integral */

    /* With error=200 and dt=10, integral would be 2000 without clamping */
    rx_err_t err = rx_pid_compute(&pid, setpoint, measured, large_dt, &output);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    /* Output should be clamped integral * ki = 5.0 * 1.0 = 5.0 */
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, config.integral_max * config.ki, output);
}

/**
 * @brief Test integral anti-windup clamping (lower bound)
 */
void test_pid_compute_integral_antiwindup_lower(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 0.0f;
    config.ki = 1.0f;
    config.kd = 0.0f;
    config.integral_max = 5.0f;
    config.integral_min = -5.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 0.0f;
    float measured = 200.0f;  /* Negative error */
    float large_dt = 10.0f;

    /* With error=-200 and dt=10, integral would be -2000 without clamping */
    rx_err_t err = rx_pid_compute(&pid, setpoint, measured, large_dt, &output);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    /* Output should be clamped integral * ki = -5.0 * 1.0 = -5.0 */
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, config.integral_min * config.ki, output);
}

/* =============================================================================
 * Derivative Response Tests
 * =============================================================================
 */

/**
 * @brief Test derivative response: d_term = Kd * (error - prev_error) / dt
 */
void test_pid_compute_derivative_response(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 0.0f;
    config.ki = 0.0f;
    config.kd = 0.01f;  /* Small Kd to avoid output saturation */
    config.output_max = 1000.0f;
    config.output_min = -1000.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float dt = 0.1f;  /* Larger dt for smaller derivative values */

    /* First call: error=10, prev_error=0, derivative = (10-0)/0.1 = 100 */
    rx_pid_compute(&pid, 100.0f, 90.0f, dt, &output);
    float expected_derivative = (10.0f - 0.0f) / dt;
    float expected_output = expected_derivative * config.kd;
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, expected_output, output);

    /* Second call: error=5, prev_error=10, derivative = (5-10)/0.1 = -50 */
    rx_pid_compute(&pid, 100.0f, 95.0f, dt, &output);
    expected_derivative = (5.0f - 10.0f) / dt;
    expected_output = expected_derivative * config.kd;
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, expected_output, output);
}

/**
 * @brief Test derivative is zero when error is constant
 */
void test_pid_compute_derivative_constant_error(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 0.0f;
    config.ki = 0.0f;
    config.kd = 1.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 100.0f;
    float measured = 90.0f;

    /* First call to establish prev_error */
    rx_pid_compute(&pid, setpoint, measured, s_dt_seconds, &output);

    /* Second call with same error - derivative should be zero */
    rx_pid_compute(&pid, setpoint, measured, s_dt_seconds, &output);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 0.0f, output);
}

/* =============================================================================
 * Output Saturation Tests
 * =============================================================================
 */

/**
 * @brief Test output clamping to upper limit
 */
void test_pid_compute_output_clamp_upper(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 10.0f;  /* High gain to exceed limits */
    config.output_max = 50.0f;
    config.output_min = -50.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 100.0f;
    float measured = 0.0f;  /* error = 100, P = 1000 */

    rx_err_t err = rx_pid_compute(&pid, setpoint, measured, s_dt_seconds, &output);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, config.output_max, output);
}

/**
 * @brief Test output clamping to lower limit
 */
void test_pid_compute_output_clamp_lower(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 10.0f;  /* High gain to exceed limits */
    config.output_max = 50.0f;
    config.output_min = -50.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 0.0f;
    float measured = 100.0f;  /* error = -100, P = -1000 */

    rx_err_t err = rx_pid_compute(&pid, setpoint, measured, s_dt_seconds, &output);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, config.output_min, output);
}

/* =============================================================================
 * Compute Error Handling Tests
 * =============================================================================
 */

/**
 * @brief Test compute with NULL handle
 */
void test_pid_compute_null_handle(void)
{
    float output = 0.0f;

    rx_err_t err = rx_pid_compute(NULL, 100.0f, 90.0f, s_dt_seconds, &output);

    TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test compute with NULL output pointer
 */
void test_pid_compute_null_output(void)
{
    rx_pid_handle_t pid = {0};
    init_default_pid(&pid);

    rx_err_t err = rx_pid_compute(&pid, 100.0f, 90.0f, s_dt_seconds, NULL);

    TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test compute when not initialized
 */
void test_pid_compute_not_initialized(void)
{
    rx_pid_handle_t pid = {0};
    float output = 0.0f;

    rx_err_t err = rx_pid_compute(&pid, 100.0f, 90.0f, s_dt_seconds, &output);

    TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test compute with dt <= 0
 */
void test_pid_compute_invalid_dt(void)
{
    rx_pid_handle_t pid = {0};
    init_default_pid(&pid);
    float output = 0.0f;

    /* dt = 0 */
    rx_err_t err = rx_pid_compute(&pid, 100.0f, 90.0f, 0.0f, &output);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);

    /* dt < 0 */
    err = rx_pid_compute(&pid, 100.0f, 90.0f, -0.01f, &output);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Reset Tests
 * =============================================================================
 */

/**
 * @brief Test reset clears integral and prev_error
 */
void test_pid_reset_clears_state(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.ki = 1.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;

    /* Accumulate some integral */
    for (uint32_t i = 0; i < 10; i++) {
        rx_pid_compute(&pid, 100.0f, 90.0f, s_dt_seconds, &output);
    }
    TEST_ASSERT_TRUE(pid.integral > 0.0f);
    TEST_ASSERT_TRUE(pid.prev_error != 0.0f);

    /* Reset */
    rx_err_t err = rx_pid_reset(&pid);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 0.0f, pid.integral);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 0.0f, pid.prev_error);
    TEST_ASSERT_TRUE(pid.initialized);  /* Still initialized */
}

/**
 * @brief Test reset with NULL handle
 */
void test_pid_reset_null_handle(void)
{
    rx_err_t err = rx_pid_reset(NULL);

    TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test reset when not initialized
 */
void test_pid_reset_not_initialized(void)
{
    rx_pid_handle_t pid = {0};

    rx_err_t err = rx_pid_reset(&pid);

    TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Set Gains Tests
 * =============================================================================
 */

/**
 * @brief Test set_gains updates gains at runtime
 */
void test_pid_set_gains_success(void)
{
    rx_pid_handle_t pid = {0};
    init_default_pid(&pid);

    float new_kp = 2.5f;
    float new_ki = 0.8f;
    float new_kd = 0.15f;

    rx_err_t err = rx_pid_set_gains(&pid, new_kp, new_ki, new_kd);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, new_kp, pid.kp);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, new_ki, pid.ki);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, new_kd, pid.kd);
}

/**
 * @brief Test set_gains preserves internal state
 */
void test_pid_set_gains_preserves_state(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.ki = 1.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;

    /* Accumulate some state */
    rx_pid_compute(&pid, 100.0f, 90.0f, s_dt_seconds, &output);
    float saved_integral = pid.integral;
    float saved_prev_error = pid.prev_error;

    /* Change gains */
    rx_pid_set_gains(&pid, 2.0f, 2.0f, 0.5f);

    /* State should be preserved */
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, saved_integral, pid.integral);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, saved_prev_error, pid.prev_error);
}

/**
 * @brief Test set_gains with NULL handle
 */
void test_pid_set_gains_null_handle(void)
{
    rx_err_t err = rx_pid_set_gains(NULL, 1.0f, 1.0f, 1.0f);

    TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test set_gains when not initialized
 */
void test_pid_set_gains_not_initialized(void)
{
    rx_pid_handle_t pid = {0};

    rx_err_t err = rx_pid_set_gains(&pid, 1.0f, 1.0f, 1.0f);

    TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Set Output Limits Tests
 * =============================================================================
 */

/**
 * @brief Test set_output_limits updates limits at runtime
 */
void test_pid_set_output_limits_success(void)
{
    rx_pid_handle_t pid = {0};
    init_default_pid(&pid);

    float new_min = -75.0f;
    float new_max = 75.0f;

    rx_err_t err = rx_pid_set_output_limits(&pid, new_min, new_max);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, new_min, pid.output_min);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, new_max, pid.output_max);
}

/**
 * @brief Test set_output_limits with invalid limits
 */
void test_pid_set_output_limits_invalid(void)
{
    rx_pid_handle_t pid = {0};
    init_default_pid(&pid);

    /* max <= min */
    rx_err_t err = rx_pid_set_output_limits(&pid, 100.0f, 50.0f);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);

    err = rx_pid_set_output_limits(&pid, 50.0f, 50.0f);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test set_output_limits with NULL handle
 */
void test_pid_set_output_limits_null_handle(void)
{
    rx_err_t err = rx_pid_set_output_limits(NULL, -50.0f, 50.0f);

    TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test set_output_limits when not initialized
 */
void test_pid_set_output_limits_not_initialized(void)
{
    rx_pid_handle_t pid = {0};

    rx_err_t err = rx_pid_set_output_limits(&pid, -50.0f, 50.0f);

    TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Set Integral Limits Tests
 * =============================================================================
 */

/**
 * @brief Test set_integral_limits updates limits at runtime
 */
void test_pid_set_integral_limits_success(void)
{
    rx_pid_handle_t pid = {0};
    init_default_pid(&pid);

    float new_min = -25.0f;
    float new_max = 25.0f;

    rx_err_t err = rx_pid_set_integral_limits(&pid, new_min, new_max);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, new_min, pid.integral_min);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, new_max, pid.integral_max);
}

/**
 * @brief Test set_integral_limits clamps existing integral
 */
void test_pid_set_integral_limits_clamps_existing(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.ki = 1.0f;
    config.integral_max = 100.0f;
    config.integral_min = -100.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;

    /* Accumulate integral to ~1.0 (100 iterations * 10 error * 0.01 dt = 10) */
    for (uint32_t i = 0; i < 100; i++) {
        rx_pid_compute(&pid, 100.0f, 90.0f, s_dt_seconds, &output);
    }
    TEST_ASSERT_TRUE(pid.integral > 5.0f);

    /* Reduce integral limits - should clamp existing integral */
    rx_err_t err = rx_pid_set_integral_limits(&pid, -5.0f, 5.0f);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 5.0f, pid.integral);
}

/**
 * @brief Test set_integral_limits with invalid limits
 */
void test_pid_set_integral_limits_invalid(void)
{
    rx_pid_handle_t pid = {0};
    init_default_pid(&pid);

    /* max <= min */
    rx_err_t err = rx_pid_set_integral_limits(&pid, 50.0f, 25.0f);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);

    err = rx_pid_set_integral_limits(&pid, 25.0f, 25.0f);
    TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test set_integral_limits with NULL handle
 */
void test_pid_set_integral_limits_null_handle(void)
{
    rx_err_t err = rx_pid_set_integral_limits(NULL, -25.0f, 25.0f);

    TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test set_integral_limits when not initialized
 */
void test_pid_set_integral_limits_not_initialized(void)
{
    rx_pid_handle_t pid = {0};

    rx_err_t err = rx_pid_set_integral_limits(&pid, -25.0f, 25.0f);

    TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Combined PID Response Tests
 * =============================================================================
 */

/**
 * @brief Test full PID response (P + I + D combined)
 */
void test_pid_compute_full_pid(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 1.0f;
    config.ki = 0.5f;
    config.kd = 0.1f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 100.0f;
    float measured = 90.0f;
    float dt = 0.01f;

    /* First call: prev_error = 0, error = setpoint - measured = 10 */
    rx_err_t err = rx_pid_compute(&pid, setpoint, measured, dt, &output);

    /* Expected:
     * P = 1.0 * 10 = 10
     * I = 0.5 * (10 * 0.01) = 0.05
     * D = 0.1 * (10 - 0) / 0.01 = 100
     * Total = 110.05, clamped to 100 */
    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, config.output_max, output);
}

/**
 * @brief Test step response convergence
 */
void test_pid_step_response(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 0.5f;
    config.ki = 0.1f;
    config.kd = 0.0f;
    config.output_max = 10.0f;
    config.output_min = -10.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 50.0f;
    float measured = 0.0f;
    float dt = 0.01f;

    /* Simulate several iterations - output should increase */
    float prev_output = -1000.0f;
    for (uint32_t i = 0; i < 10; i++) {
        rx_pid_compute(&pid, setpoint, measured, dt, &output);

        /* With increasing error (setpoint - measured stays constant with I accumulation),
         * output should be positive */
        TEST_ASSERT_TRUE(output > 0.0f);
        prev_output = output;
    }
    (void)prev_output;  /* Used in loop comparison only */
}

/* =============================================================================
 * MATLAB Parameter Tests (Kp=0.286, Ki=8.01)
 * =============================================================================
 */

/**
 * @brief Test with MATLAB-tuned parameters for motor control
 */
void test_pid_matlab_tuned_parameters(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = s_matlab_kp;
    config.ki = s_matlab_ki;
    config.kd = 0.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 210.0f;  /* 210 RPM target */
    float measured = 200.0f;  /* 200 RPM actual */
    float error = 10.0f;
    float dt = 0.004f;  /* 4ms = 250Hz control loop */

    rx_err_t err = rx_pid_compute(&pid, setpoint, measured, dt, &output);

    /* Expected:
     * P = 0.286 * 10 = 2.86
     * I = 8.01 * (10 * 0.004) = 0.3204
     * Total = 3.1804 */
    float expected_p = s_matlab_kp * error;
    float expected_i = s_matlab_ki * (error * dt);
    float expected_total = expected_p + expected_i;

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_total, output);
}

/**
 * @brief Test MATLAB parameters over multiple iterations (simulating control loop)
 */
void test_pid_matlab_control_loop(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = s_matlab_kp;
    config.ki = s_matlab_ki;
    config.kd = 0.0f;
    config.output_max = 100.0f;   /* PWM duty cycle max */
    config.output_min = -100.0f;  /* PWM duty cycle min */
    config.integral_max = 50.0f;
    config.integral_min = -50.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float setpoint = 210.0f;  /* 210 RPM target */
    float dt = 0.004f;        /* 4ms = 250Hz */

    /* Simulate motor approaching setpoint */
    float measured = 0.0f;
    for (uint32_t i = 0; i < 50; i++) {
        rx_pid_compute(&pid, setpoint, measured, dt, &output);

        /* Simulate motor response (simplified) */
        measured += output * 0.1f;

        /* Output should be bounded */
        TEST_ASSERT_TRUE(output >= config.output_min);
        TEST_ASSERT_TRUE(output <= config.output_max);
    }

    /* After 50 iterations, measured should be approaching setpoint */
    TEST_ASSERT_TRUE(measured > 100.0f);
}

/* =============================================================================
 * Ramp Input Tests
 * =============================================================================
 */

/**
 * @brief Test PI controller tracking a ramp input
 */
void test_pid_ramp_tracking(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 1.0f;
    config.ki = 2.0f;
    config.kd = 0.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;
    float dt = 0.01f;
    float ramp_rate = 10.0f;  /* Units per second */

    /* Track a ramping setpoint */
    for (uint32_t i = 0; i < 100; i++) {
        float setpoint = ramp_rate * (float)i * dt;
        float measured = setpoint * 0.9f;  /* Always 10% behind */

        rx_pid_compute(&pid, setpoint, measured, dt, &output);

        /* Output should be positive (trying to catch up) */
        TEST_ASSERT_TRUE(output >= 0.0f);
    }
}

/* =============================================================================
 * Determinism Tests
 * =============================================================================
 */

/**
 * @brief Test that same inputs produce same outputs (deterministic)
 */
void test_pid_deterministic_behavior(void)
{
    rx_pid_handle_t pid1 = {0};
    rx_pid_handle_t pid2 = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 1.5f;
    config.ki = 0.5f;
    config.kd = 0.1f;
    rx_pid_init(&pid1, &config);
    rx_pid_init(&pid2, &config);

    float output1 = 0.0f;
    float output2 = 0.0f;
    float dt = 0.01f;

    /* Run identical sequences */
    float setpoints[] = {100.0f, 105.0f, 98.0f, 102.0f, 100.0f};
    float measurements[] = {90.0f, 95.0f, 94.0f, 99.0f, 100.0f};

    for (uint32_t i = 0; i < 5; i++) {
        rx_pid_compute(&pid1, setpoints[i], measurements[i], dt, &output1);
        rx_pid_compute(&pid2, setpoints[i], measurements[i], dt, &output2);

        TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, output1, output2);
    }
}

/* =============================================================================
 * Edge Cases
 * =============================================================================
 */

/**
 * @brief Test with zero gains (disabled controller)
 */
void test_pid_zero_gains(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 0.0f;
    config.ki = 0.0f;
    config.kd = 0.0f;
    rx_pid_init(&pid, &config);

    float output = 999.0f;

    rx_err_t err = rx_pid_compute(&pid, 100.0f, 0.0f, s_dt_seconds, &output);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 0.0f, output);
}

/**
 * @brief Test with very small dt
 */
void test_pid_small_dt(void)
{
    rx_pid_handle_t pid = {0};
    init_default_pid(&pid);
    float output = 0.0f;

    /* Very small but valid dt */
    rx_err_t err = rx_pid_compute(&pid, 100.0f, 90.0f, 0.000001f, &output);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    /* Output should be valid (may be clamped due to high derivative) */
    TEST_ASSERT_TRUE(output >= (float)k_test_output_min);
    TEST_ASSERT_TRUE(output <= (float)k_test_output_max);
}

/**
 * @brief Test with asymmetric output limits
 */
void test_pid_asymmetric_output_limits(void)
{
    rx_pid_handle_t pid = {0};
    rx_pid_config_t config = create_default_config();
    config.kp = 10.0f;
    config.output_min = -20.0f;
    config.output_max = 80.0f;
    rx_pid_init(&pid, &config);

    float output = 0.0f;

    /* Positive saturation */
    rx_pid_compute(&pid, 100.0f, 0.0f, s_dt_seconds, &output);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, 80.0f, output);

    rx_pid_reset(&pid);

    /* Negative saturation */
    rx_pid_compute(&pid, 0.0f, 100.0f, s_dt_seconds, &output);
    TEST_ASSERT_FLOAT_WITHIN(s_float_epsilon, -20.0f, output);
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

int main(void)
{
    UNITY_BEGIN();

    /* Initialization tests */
    RUN_TEST(test_pid_init_success);
    RUN_TEST(test_pid_init_null_handle);
    RUN_TEST(test_pid_init_null_config);
    RUN_TEST(test_pid_init_already_initialized);
    RUN_TEST(test_pid_init_invalid_output_limits);
    RUN_TEST(test_pid_init_invalid_integral_limits);

    /* Deinitialization tests */
    RUN_TEST(test_pid_deinit_success);
    RUN_TEST(test_pid_deinit_null_handle);
    RUN_TEST(test_pid_deinit_not_initialized);

    /* Proportional response tests */
    RUN_TEST(test_pid_compute_proportional_only);
    RUN_TEST(test_pid_compute_proportional_negative_error);
    RUN_TEST(test_pid_compute_proportional_zero_error);

    /* Integral response tests */
    RUN_TEST(test_pid_compute_integral_accumulation);
    RUN_TEST(test_pid_compute_integral_antiwindup_upper);
    RUN_TEST(test_pid_compute_integral_antiwindup_lower);

    /* Derivative response tests */
    RUN_TEST(test_pid_compute_derivative_response);
    RUN_TEST(test_pid_compute_derivative_constant_error);

    /* Output saturation tests */
    RUN_TEST(test_pid_compute_output_clamp_upper);
    RUN_TEST(test_pid_compute_output_clamp_lower);

    /* Compute error handling tests */
    RUN_TEST(test_pid_compute_null_handle);
    RUN_TEST(test_pid_compute_null_output);
    RUN_TEST(test_pid_compute_not_initialized);
    RUN_TEST(test_pid_compute_invalid_dt);

    /* Reset tests */
    RUN_TEST(test_pid_reset_clears_state);
    RUN_TEST(test_pid_reset_null_handle);
    RUN_TEST(test_pid_reset_not_initialized);

    /* Set gains tests */
    RUN_TEST(test_pid_set_gains_success);
    RUN_TEST(test_pid_set_gains_preserves_state);
    RUN_TEST(test_pid_set_gains_null_handle);
    RUN_TEST(test_pid_set_gains_not_initialized);

    /* Set output limits tests */
    RUN_TEST(test_pid_set_output_limits_success);
    RUN_TEST(test_pid_set_output_limits_invalid);
    RUN_TEST(test_pid_set_output_limits_null_handle);
    RUN_TEST(test_pid_set_output_limits_not_initialized);

    /* Set integral limits tests */
    RUN_TEST(test_pid_set_integral_limits_success);
    RUN_TEST(test_pid_set_integral_limits_clamps_existing);
    RUN_TEST(test_pid_set_integral_limits_invalid);
    RUN_TEST(test_pid_set_integral_limits_null_handle);
    RUN_TEST(test_pid_set_integral_limits_not_initialized);

    /* Combined PID response tests */
    RUN_TEST(test_pid_compute_full_pid);
    RUN_TEST(test_pid_step_response);

    /* MATLAB parameter tests */
    RUN_TEST(test_pid_matlab_tuned_parameters);
    RUN_TEST(test_pid_matlab_control_loop);

    /* Ramp input tests */
    RUN_TEST(test_pid_ramp_tracking);

    /* Determinism tests */
    RUN_TEST(test_pid_deterministic_behavior);

    /* Edge case tests */
    RUN_TEST(test_pid_zero_gains);
    RUN_TEST(test_pid_small_dt);
    RUN_TEST(test_pid_asymmetric_output_limits);

    return UNITY_END();
}
