/**
 * @file test_motor_control.c
 * @brief Host-build unit tests for motor control shared data layer.
 *
 * @details
 * Tests the bb_shared_data motor command path using the host compiler.
 * No librobotcontrol dependency -- only tests pure C logic in shared_data.c.
 *
 * Each test function returns 0 on pass, 1 on fail. main() runs all tests
 * and returns the count of failures (0 = all pass).
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "shared_data.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------------------*/

/**
 * @enum test_result_t
 * @brief Test pass/fail result codes.
 *
 * @since Version 1.0.0
 */
typedef enum : int {
    k_test_pass = 0, /**< Test passed */
    k_test_fail = 1, /**< Test failed */
} test_result_t;

static uint32_t s_tests_run = 0U;

/* ---------------------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------------------*/

/**
 * @brief Test: bb_shared_data_init() zeroes all fields.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.0.0
 */
static test_result_t test_init_zeroes_fields(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    (void)memset(&sd, 0xFF, sizeof(sd)); /* Fill with non-zero garbage */

    bb_err_t err = bb_shared_data_init(&sd);
    if (err != k_bb_err_ok) {
        fprintf(stderr, "[FAIL] test_init_zeroes_fields: init returned %d\n",
                (int)err);
        return k_test_fail;
    }

    bool estop = true;
    (void)bb_shared_data_get_estop(&sd, &estop);
    if (estop != false) {
        fprintf(stderr, "[FAIL] test_init_zeroes_fields: estop not zeroed\n");
        (void)bb_shared_data_destroy(&sd);
        return k_test_fail;
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_init_zeroes_fields\n");
    return k_test_pass;
}

/**
 * @brief Test: null pointer guards return k_bb_err_null_ptr.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.0.0
 */
static test_result_t test_null_ptr_guards(void)
{
    s_tests_run++;
    bb_err_t err;

    err = bb_shared_data_init(NULL);
    if (err != k_bb_err_null_ptr) {
        fprintf(stderr, "[FAIL] test_null_ptr_guards: init(NULL) = %d\n",
                (int)err);
        return k_test_fail;
    }

    err = bb_shared_data_set_motor_cmd(NULL, NULL);
    if (err != k_bb_err_null_ptr) {
        fprintf(stderr,
                "[FAIL] test_null_ptr_guards: set_motor_cmd(NULL,NULL) = %d\n",
                (int)err);
        return k_test_fail;
    }

    fprintf(stdout, "[PASS] test_null_ptr_guards\n");
    return k_test_pass;
}

/**
 * @brief Test: motor command round-trip through shared data.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.0.0
 */
static test_result_t test_motor_cmd_round_trip(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    bb_err_t err = bb_shared_data_init(&sd);
    if (err != k_bb_err_ok) {
        fprintf(stderr, "[FAIL] test_motor_cmd_round_trip: init failed\n");
        return k_test_fail;
    }

    bb_motor_cmd_t cmd_in = {
        .duty_percent = {0.5F, -0.5F, 1.0F, -1.0F},
    };
    (void)bb_shared_data_set_motor_cmd(&sd, &cmd_in);

    bb_motor_cmd_t cmd_out = {0};
    (void)bb_shared_data_get_motor_cmd(&sd, &cmd_out);

    for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
        if (cmd_out.duty_percent[i] != cmd_in.duty_percent[i]) {
            fprintf(stderr,
                    "[FAIL] test_motor_cmd_round_trip: channel %u mismatch\n",
                    i);
            (void)bb_shared_data_destroy(&sd);
            return k_test_fail;
        }
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_motor_cmd_round_trip\n");
    return k_test_pass;
}

/**
 * @brief Test: estop flag set/get.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.0.0
 */
static test_result_t test_estop_set_get(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    (void)bb_shared_data_init(&sd);

    (void)bb_shared_data_set_estop(&sd, true);
    bool estop = false;
    (void)bb_shared_data_get_estop(&sd, &estop);

    if (!estop) {
        fprintf(stderr, "[FAIL] test_estop_set_get: estop not set\n");
        (void)bb_shared_data_destroy(&sd);
        return k_test_fail;
    }

    (void)bb_shared_data_set_estop(&sd, false);
    (void)bb_shared_data_get_estop(&sd, &estop);

    if (estop) {
        fprintf(stderr, "[FAIL] test_estop_set_get: estop not cleared\n");
        (void)bb_shared_data_destroy(&sd);
        return k_test_fail;
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_estop_set_get\n");
    return k_test_pass;
}

/**
 * @brief Test: IMU data round-trip through shared data.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.0.0
 */
static test_result_t test_imu_round_trip(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    (void)bb_shared_data_init(&sd);

    bb_imu_data_t imu_in = {
        .accel_mps2 = {1.0F, 2.0F, 9.81F},
        .gyro_rps   = {0.1F, 0.2F, 0.3F},
        .mag_ut     = {10.0F, 20.0F, 30.0F},
        .temp_c     = 25.5F,
    };
    (void)bb_shared_data_set_imu(&sd, &imu_in);

    bb_imu_data_t imu_out = {0};
    (void)bb_shared_data_get_imu(&sd, &imu_out);

    if (imu_out.accel_mps2[0] != 1.0F || imu_out.accel_mps2[2] != 9.81F ||
        imu_out.gyro_rps[1] != 0.2F || imu_out.mag_ut[2] != 30.0F ||
        imu_out.temp_c != 25.5F) {
        fprintf(stderr, "[FAIL] test_imu_round_trip: data mismatch\n");
        (void)bb_shared_data_destroy(&sd);
        return k_test_fail;
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_imu_round_trip\n");
    return k_test_pass;
}

/**
 * @brief Test: encoder data round-trip through shared data.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.0.0
 */
static test_result_t test_encoder_round_trip(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    (void)bb_shared_data_init(&sd);

    bb_encoder_data_t enc_in = {
        .ticks = {100, -200, 300, -400},
    };
    (void)bb_shared_data_set_encoder(&sd, &enc_in);

    bb_encoder_data_t enc_out = {0};
    (void)bb_shared_data_get_encoder(&sd, &enc_out);

    for (uint8_t i = 0U; i < 4U; i++) {
        if (enc_out.ticks[i] != enc_in.ticks[i]) {
            fprintf(stderr,
                    "[FAIL] test_encoder_round_trip: ticks[%u] mismatch\n", i);
            (void)bb_shared_data_destroy(&sd);
            return k_test_fail;
        }
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_encoder_round_trip\n");
    return k_test_pass;
}

/**
 * @brief Test: set motor_cmd and estop, then read both back correctly.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.0.0
 */
static test_result_t test_concurrent_set_get(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    (void)bb_shared_data_init(&sd);

    /* Set motor command */
    bb_motor_cmd_t cmd_in = {
        .duty_percent = {0.25F, -0.75F, 0.5F, -1.0F},
    };
    (void)bb_shared_data_set_motor_cmd(&sd, &cmd_in);

    /* Set estop */
    (void)bb_shared_data_set_estop(&sd, true);

    /* Read both back */
    bb_motor_cmd_t cmd_out = {0};
    (void)bb_shared_data_get_motor_cmd(&sd, &cmd_out);

    bool estop = false;
    (void)bb_shared_data_get_estop(&sd, &estop);

    for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
        if (cmd_out.duty_percent[i] != cmd_in.duty_percent[i]) {
            fprintf(stderr,
                    "[FAIL] test_concurrent_set_get: channel %u mismatch\n", i);
            (void)bb_shared_data_destroy(&sd);
            return k_test_fail;
        }
    }

    if (!estop) {
        fprintf(stderr,
                "[FAIL] test_concurrent_set_get: estop not true\n");
        (void)bb_shared_data_destroy(&sd);
        return k_test_fail;
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_concurrent_set_get\n");
    return k_test_pass;
}

/**
 * @brief Test: last_cmd_us updates after set_motor_cmd.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.0.0
 */
static test_result_t test_last_cmd_us_after_set(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    (void)bb_shared_data_init(&sd);

    uint64_t ts_before = 0U;
    (void)bb_shared_data_get_last_cmd_us(&sd, &ts_before);

    bb_motor_cmd_t cmd = { .duty_percent = {0.1F, 0.2F, 0.3F, 0.4F} };
    (void)bb_shared_data_set_motor_cmd(&sd, &cmd);

    uint64_t ts_after = 0U;
    (void)bb_shared_data_get_last_cmd_us(&sd, &ts_after);

    if (ts_after <= ts_before) {
        fprintf(stderr,
                "[FAIL] test_last_cmd_us_after_set: timestamp did not advance\n");
        (void)bb_shared_data_destroy(&sd);
        return k_test_fail;
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_last_cmd_us_after_set\n");
    return k_test_pass;
}

/**
 * @brief Test: velocity_setpoint_mps round-trip through shared data.
 *
 * @details
 * Verifies that the new velocity setpoint field added in version 1.2.0
 * survives a set_motor_cmd / get_motor_cmd round-trip with bit-exact
 * values for all four motors.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.2.0
 */
static test_result_t test_motor_cmd_velocity_setpoint_round_trip(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    bb_err_t err = bb_shared_data_init(&sd);
    if (err != k_bb_err_ok) {
        fprintf(stderr,
                "[FAIL] test_motor_cmd_velocity_setpoint_round_trip: init failed\n");
        return k_test_fail;
    }

    bb_motor_cmd_t cmd_in = {
        .control_mode = k_bb_ctrl_mode_velocity,
        .velocity_setpoint_mps = {0.5F, -0.5F, 1.0F, -1.0F},
    };
    (void)bb_shared_data_set_motor_cmd(&sd, &cmd_in);

    bb_motor_cmd_t cmd_out = {0};
    (void)bb_shared_data_get_motor_cmd(&sd, &cmd_out);

    for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
        if (cmd_out.velocity_setpoint_mps[i] !=
            cmd_in.velocity_setpoint_mps[i]) {
            fprintf(stderr,
                    "[FAIL] test_motor_cmd_velocity_setpoint_round_trip: channel %u mismatch\n",
                    i);
            (void)bb_shared_data_destroy(&sd);
            return k_test_fail;
        }
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_motor_cmd_velocity_setpoint_round_trip\n");
    return k_test_pass;
}

/**
 * @brief Test: bb_motor_cmd_t.control_mode default after init is velocity.
 *
 * @details
 * The motor control task dispatches on control_mode. A freshly
 * initialized shared_data must default to k_bb_ctrl_mode_velocity (the
 * zero value of the enum) so a robot with no command yet behaves as
 * "velocity setpoint = 0" rather than "duty = 0" -- both produce a
 * stopped motor, but the velocity path goes through the PID and is the
 * intended steady state.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.2.0
 */
static test_result_t test_motor_cmd_default_mode_is_velocity(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    bb_err_t err = bb_shared_data_init(&sd);
    if (err != k_bb_err_ok) {
        fprintf(stderr,
                "[FAIL] test_motor_cmd_default_mode_is_velocity: init failed\n");
        return k_test_fail;
    }

    bb_motor_cmd_t cmd_out = {0};
    (void)bb_shared_data_get_motor_cmd(&sd, &cmd_out);

    if (cmd_out.control_mode != k_bb_ctrl_mode_velocity) {
        fprintf(stderr,
                "[FAIL] test_motor_cmd_default_mode_is_velocity: mode = %d (expected %d)\n",
                (int)cmd_out.control_mode, (int)k_bb_ctrl_mode_velocity);
        (void)bb_shared_data_destroy(&sd);
        return k_test_fail;
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_motor_cmd_default_mode_is_velocity\n");
    return k_test_pass;
}

/**
 * @brief Test: control_mode round-trips between velocity and direct_duty.
 *
 * @details
 * Switching from the closed-loop velocity dispatch to the open-loop
 * direct-duty debug path (and back) must persist through shared data
 * unchanged.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.2.0
 */
static test_result_t test_motor_cmd_control_mode_round_trip(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    bb_err_t err = bb_shared_data_init(&sd);
    if (err != k_bb_err_ok) {
        fprintf(stderr,
                "[FAIL] test_motor_cmd_control_mode_round_trip: init failed\n");
        return k_test_fail;
    }

    /* Switch to direct duty */
    bb_motor_cmd_t cmd_dd = {
        .control_mode = k_bb_ctrl_mode_direct_duty,
        .duty_percent = {0.1F, 0.2F, 0.3F, 0.4F},
    };
    (void)bb_shared_data_set_motor_cmd(&sd, &cmd_dd);

    bb_motor_cmd_t cmd_out = {0};
    (void)bb_shared_data_get_motor_cmd(&sd, &cmd_out);
    if (cmd_out.control_mode != k_bb_ctrl_mode_direct_duty) {
        fprintf(stderr,
                "[FAIL] test_motor_cmd_control_mode_round_trip: direct_duty not persisted\n");
        (void)bb_shared_data_destroy(&sd);
        return k_test_fail;
    }

    /* Switch back to velocity */
    bb_motor_cmd_t cmd_vel = {
        .control_mode = k_bb_ctrl_mode_velocity,
        .velocity_setpoint_mps = {0.0F, 0.0F, 0.0F, 0.0F},
    };
    (void)bb_shared_data_set_motor_cmd(&sd, &cmd_vel);

    (void)bb_shared_data_get_motor_cmd(&sd, &cmd_out);
    if (cmd_out.control_mode != k_bb_ctrl_mode_velocity) {
        fprintf(stderr,
                "[FAIL] test_motor_cmd_control_mode_round_trip: velocity not persisted\n");
        (void)bb_shared_data_destroy(&sd);
        return k_test_fail;
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_motor_cmd_control_mode_round_trip\n");
    return k_test_pass;
}

/**
 * @brief Test: destroy then reinit succeeds and zeroes state.
 *
 * @return test_result_t k_test_pass or k_test_fail
 *
 * @since Version 1.0.0
 */
static test_result_t test_destroy_reinit(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    (void)bb_shared_data_init(&sd);

    /* Set some state */
    (void)bb_shared_data_set_estop(&sd, true);
    bb_motor_cmd_t cmd = { .duty_percent = {1.0F, 1.0F, 1.0F, 1.0F} };
    (void)bb_shared_data_set_motor_cmd(&sd, &cmd);

    /* Destroy */
    (void)bb_shared_data_destroy(&sd);

    /* Reinit */
    bb_err_t err = bb_shared_data_init(&sd);
    if (err != k_bb_err_ok) {
        fprintf(stderr, "[FAIL] test_destroy_reinit: reinit failed\n");
        return k_test_fail;
    }

    /* Verify state is zeroed */
    bool estop = true;
    (void)bb_shared_data_get_estop(&sd, &estop);
    if (estop) {
        fprintf(stderr,
                "[FAIL] test_destroy_reinit: estop not cleared after reinit\n");
        (void)bb_shared_data_destroy(&sd);
        return k_test_fail;
    }

    bb_motor_cmd_t cmd_out = {0};
    (void)bb_shared_data_get_motor_cmd(&sd, &cmd_out);
    for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
        if (cmd_out.duty_percent[i] != 0.0F) {
            fprintf(stderr,
                    "[FAIL] test_destroy_reinit: duty[%u] not zero\n", i);
            (void)bb_shared_data_destroy(&sd);
            return k_test_fail;
        }
    }

    (void)bb_shared_data_destroy(&sd);
    fprintf(stdout, "[PASS] test_destroy_reinit\n");
    return k_test_pass;
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/

/**
 * @brief Run all unit tests and report results.
 *
 * @return int Number of failed tests (0 = all pass)
 *
 * @since Version 1.0.0
 */
int main(void)
{
    int failures = 0;

    failures += (int)test_init_zeroes_fields();
    failures += (int)test_null_ptr_guards();
    failures += (int)test_motor_cmd_round_trip();
    failures += (int)test_estop_set_get();
    failures += (int)test_imu_round_trip();
    failures += (int)test_encoder_round_trip();
    failures += (int)test_concurrent_set_get();
    failures += (int)test_last_cmd_us_after_set();
    failures += (int)test_motor_cmd_velocity_setpoint_round_trip();
    failures += (int)test_motor_cmd_default_mode_is_velocity();
    failures += (int)test_motor_cmd_control_mode_round_trip();
    failures += (int)test_destroy_reinit();

    if (failures == 0) {
        fprintf(stdout, "\ntest_motor_control: ALL %u TESTS PASSED\n",
                (unsigned)s_tests_run);
    } else {
        fprintf(stderr, "\n%d test(s) failed.\n", failures);
    }

    return failures;
}
