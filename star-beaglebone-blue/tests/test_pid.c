/**
 * @file test_pid.c
 * @brief Unit tests for bb_pid library.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "bb_pid.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------------------*/

static uint32_t s_failures  = 0U;
static uint32_t s_tests_run = 0U;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            (void)fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            s_failures++; \
        } \
    } while (0)

#define FLOAT_EQ(a, b) (fabsf((a) - (b)) < 1e-5F)

static bb_pid_config_t s_default_config = {
    .kp = 1.0F,
    .ki = 0.0F,
    .kd = 0.0F,
    .output_min = -100.0F,
    .output_max = 100.0F,
    .integral_min = -50.0F,
    .integral_max = 50.0F,
};

/* ---------------------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------------------*/

static void test_pid_proportional_only(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    bb_pid_config_t cfg = s_default_config;
    cfg.kp = 2.0F;
    cfg.ki = 0.0F;
    cfg.kd = 0.0F;

    (void)bb_pid_init(&pid, &cfg);

    float output = 0.0F;
    bb_err_t err = bb_pid_compute(&pid, 10.0F, 5.0F, 0.01F, &output);
    TEST_ASSERT(err == k_bb_err_ok, "p-only: err");
    TEST_ASSERT(FLOAT_EQ(output, 10.0F), "p-only: output should be 2*5=10");
}

static void test_pid_integral_accumulation(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    bb_pid_config_t cfg = s_default_config;
    cfg.kp = 0.0F;
    cfg.ki = 1.0F;
    cfg.kd = 0.0F;

    (void)bb_pid_init(&pid, &cfg);

    /* Feed constant error=10 for 10 steps at dt=0.1 */
    float output = 0.0F;
    for (uint32_t i = 0U; i < 10U; i++) {
        (void)bb_pid_compute(&pid, 10.0F, 0.0F, 0.1F, &output);
    }

    /* integral = 10 * 0.1 * 10 = 10.0; output = ki * integral = 10.0 */
    TEST_ASSERT(FLOAT_EQ(output, 10.0F), "integral: output should be 10.0");
}

static void test_pid_integral_anti_windup(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    bb_pid_config_t cfg = s_default_config;
    cfg.kp = 0.0F;
    cfg.ki = 1.0F;
    cfg.kd = 0.0F;
    cfg.integral_min = -5.0F;
    cfg.integral_max = 5.0F;

    (void)bb_pid_init(&pid, &cfg);

    /* Feed large error to saturate integral */
    float output = 0.0F;
    for (uint32_t i = 0U; i < 100U; i++) {
        (void)bb_pid_compute(&pid, 100.0F, 0.0F, 0.1F, &output);
    }

    /* integral clamped to 5.0; output = ki * 5.0 = 5.0 */
    TEST_ASSERT(FLOAT_EQ(output, 5.0F), "anti-windup: output should be 5.0");
}

static void test_pid_output_saturation(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    bb_pid_config_t cfg = s_default_config;
    cfg.kp = 1.0F;
    cfg.output_min = -10.0F;
    cfg.output_max = 10.0F;

    (void)bb_pid_init(&pid, &cfg);

    float output = 0.0F;
    (void)bb_pid_compute(&pid, 500.0F, 0.0F, 0.01F, &output);
    TEST_ASSERT(FLOAT_EQ(output, 10.0F), "saturation: output should be 10.0");

    (void)bb_pid_compute(&pid, -500.0F, 0.0F, 0.01F, &output);
    TEST_ASSERT(FLOAT_EQ(output, -10.0F), "saturation: output should be -10.0");
}

static void test_pid_reset_clears_state(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    bb_pid_config_t cfg = s_default_config;
    cfg.kp = 0.0F;
    cfg.ki = 1.0F;

    (void)bb_pid_init(&pid, &cfg);

    /* Accumulate some integral */
    float output = 0.0F;
    (void)bb_pid_compute(&pid, 10.0F, 0.0F, 0.1F, &output);

    (void)bb_pid_reset(&pid);

    /* After reset, integral=0, prev_error=0 */
    (void)bb_pid_compute(&pid, 0.0F, 0.0F, 0.1F, &output);
    TEST_ASSERT(FLOAT_EQ(output, 0.0F), "reset: output should be 0 after reset");
}

static void test_pid_invalid_dt(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    (void)bb_pid_init(&pid, &s_default_config);

    float output = 0.0F;
    TEST_ASSERT(bb_pid_compute(&pid, 0, 0, 0.0F, &output) == k_bb_err_invalid_arg,
                "dt=0 should fail");
    TEST_ASSERT(bb_pid_compute(&pid, 0, 0, -1.0F, &output) == k_bb_err_invalid_arg,
                "dt<0 should fail");
}

static void test_pid_null_ptr_guards(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    float output = 0.0F;

    TEST_ASSERT(bb_pid_init(NULL, &s_default_config) == k_bb_err_null_ptr,
                "init null handle");
    TEST_ASSERT(bb_pid_init(&pid, NULL) == k_bb_err_null_ptr,
                "init null config");
    TEST_ASSERT(bb_pid_compute(NULL, 0, 0, 0.01F, &output) == k_bb_err_null_ptr,
                "compute null handle");

    (void)bb_pid_init(&pid, &s_default_config);
    TEST_ASSERT(bb_pid_compute(&pid, 0, 0, 0.01F, NULL) == k_bb_err_null_ptr,
                "compute null output");
    TEST_ASSERT(bb_pid_reset(NULL) == k_bb_err_null_ptr, "reset null");
    TEST_ASSERT(bb_pid_set_gains(NULL, 0, 0, 0) == k_bb_err_null_ptr,
                "set_gains null");
}

static void test_pid_derivative_response(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    bb_pid_config_t cfg = s_default_config;
    cfg.kp = 0.0F;
    cfg.ki = 0.0F;
    cfg.kd = 1.0F;

    (void)bb_pid_init(&pid, &cfg);

    /* First call: error=10, prev_error=0, derivative = (10-0)/0.01 = 1000 */
    float output = 0.0F;
    (void)bb_pid_compute(&pid, 10.0F, 0.0F, 0.01F, &output);

    /* Output = Kd * derivative = 1.0 * 1000 = 1000, clamped to 100 */
    TEST_ASSERT(FLOAT_EQ(output, 100.0F),
                "derivative: step should produce clamped spike");

    /* Second call with same error: derivative = (10-10)/0.01 = 0 */
    (void)bb_pid_compute(&pid, 10.0F, 0.0F, 0.01F, &output);
    TEST_ASSERT(FLOAT_EQ(output, 0.0F),
                "derivative: steady error should produce zero output");
}

static void test_pid_set_gains_runtime(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    bb_pid_config_t cfg = s_default_config;
    cfg.kp = 1.0F;
    cfg.ki = 0.0F;
    cfg.kd = 0.0F;

    (void)bb_pid_init(&pid, &cfg);

    float output = 0.0F;
    (void)bb_pid_compute(&pid, 10.0F, 0.0F, 0.01F, &output);
    TEST_ASSERT(FLOAT_EQ(output, 10.0F), "set_gains: initial Kp=1 output=10");

    /* Change Kp to 3.0 at runtime */
    (void)bb_pid_set_gains(&pid, 3.0F, 0.0F, 0.0F);

    (void)bb_pid_compute(&pid, 10.0F, 0.0F, 0.01F, &output);
    TEST_ASSERT(FLOAT_EQ(output, 30.0F), "set_gains: Kp=3 output=30");
}

static void test_pid_set_output_limits_runtime(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    bb_pid_config_t cfg = s_default_config;
    cfg.kp = 1.0F;
    cfg.output_min = -100.0F;
    cfg.output_max = 100.0F;

    (void)bb_pid_init(&pid, &cfg);

    /* error=50, output=50 (within limits) */
    float output = 0.0F;
    (void)bb_pid_compute(&pid, 50.0F, 0.0F, 0.01F, &output);
    TEST_ASSERT(FLOAT_EQ(output, 50.0F), "output_limits: 50 within [-100,100]");

    /* Tighten limits to [-10, 10] */
    (void)bb_pid_set_output_limits(&pid, -10.0F, 10.0F);

    (void)bb_pid_compute(&pid, 50.0F, 0.0F, 0.01F, &output);
    TEST_ASSERT(FLOAT_EQ(output, 10.0F),
                "output_limits: 50 clamped to 10 after tightening");
}

static void test_pid_already_initialized(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    (void)bb_pid_init(&pid, &s_default_config);

    bb_err_t err = bb_pid_init(&pid, &s_default_config);
    TEST_ASSERT(err == k_bb_err_already_initialized,
                "double init should return already_initialized");
}

static void test_pid_not_initialized(void)
{
    s_tests_run++;
    bb_pid_handle_t pid = {0};
    float output = 0.0F;

    bb_err_t err = bb_pid_compute(&pid, 0, 0, 0.01F, &output);
    TEST_ASSERT(err == k_bb_err_not_initialized,
                "compute before init should return not_initialized");

    err = bb_pid_reset(&pid);
    TEST_ASSERT(err == k_bb_err_not_initialized,
                "reset before init should return not_initialized");

    err = bb_pid_set_gains(&pid, 1.0F, 0.0F, 0.0F);
    TEST_ASSERT(err == k_bb_err_not_initialized,
                "set_gains before init should return not_initialized");

    err = bb_pid_set_output_limits(&pid, -10.0F, 10.0F);
    TEST_ASSERT(err == k_bb_err_not_initialized,
                "set_output_limits before init should return not_initialized");

    err = bb_pid_set_integral_limits(&pid, -5.0F, 5.0F);
    TEST_ASSERT(err == k_bb_err_not_initialized,
                "set_integral_limits before init should return not_initialized");
}

/* ---------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------------*/

int main(void)
{
    test_pid_proportional_only();
    test_pid_integral_accumulation();
    test_pid_integral_anti_windup();
    test_pid_output_saturation();
    test_pid_reset_clears_state();
    test_pid_invalid_dt();
    test_pid_null_ptr_guards();
    test_pid_derivative_response();
    test_pid_set_gains_runtime();
    test_pid_set_output_limits_runtime();
    test_pid_already_initialized();
    test_pid_not_initialized();

    if (s_failures == 0U) {
        (void)fprintf(stdout, "test_pid: ALL %u TESTS PASSED\n",
                      (unsigned)s_tests_run);
    } else {
        (void)fprintf(stderr, "test_pid: %u FAILURES\n", (unsigned)s_failures);
    }

    return (int)s_failures;
}
