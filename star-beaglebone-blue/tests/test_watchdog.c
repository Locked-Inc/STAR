/**
 * @file test_watchdog.c
 * @brief Unit tests for watchdog timeout logic via shared_data.
 *
 * @details
 * Tests the shared_data timestamp and estop mechanisms that the watchdog
 * task relies on. Does not test the thread itself -- tests the underlying
 * data operations.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "shared_data.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

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

static uint64_t now_us(void)
{
    struct timespec ts = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000U + (uint64_t)(ts.tv_nsec / 1000L);
}

/* ---------------------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------------------*/

/**
 * @brief Test: last_cmd_us is updated by set_motor_cmd.
 */
static void test_last_cmd_us_updated(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    (void)bb_shared_data_init(&sd);

    uint64_t before = 0U;
    (void)bb_shared_data_get_last_cmd_us(&sd, &before);

    /* Set a motor command -- this should update last_cmd_us */
    bb_motor_cmd_t cmd = {0};
    (void)bb_shared_data_set_motor_cmd(&sd, &cmd);

    uint64_t after = 0U;
    (void)bb_shared_data_get_last_cmd_us(&sd, &after);

    TEST_ASSERT(after >= before, "last_cmd_us should advance after set_motor_cmd");
    TEST_ASSERT(after > 0U, "last_cmd_us should be non-zero after set_motor_cmd");

    (void)bb_shared_data_destroy(&sd);
}

/**
 * @brief Test: estop flag can be set and cleared independently.
 */
static void test_estop_toggle(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    (void)bb_shared_data_init(&sd);

    /* Initially false */
    bool estop = true;
    (void)bb_shared_data_get_estop(&sd, &estop);
    TEST_ASSERT(!estop, "estop should be false initially");

    /* Set true */
    (void)bb_shared_data_set_estop(&sd, true);
    (void)bb_shared_data_get_estop(&sd, &estop);
    TEST_ASSERT(estop, "estop should be true after set");

    /* Clear */
    (void)bb_shared_data_set_estop(&sd, false);
    (void)bb_shared_data_get_estop(&sd, &estop);
    TEST_ASSERT(!estop, "estop should be false after clear");

    (void)bb_shared_data_destroy(&sd);
}

/**
 * @brief Test: elapsed time calculation matches expected range.
 */
static void test_elapsed_time_reasonable(void)
{
    s_tests_run++;
    bb_shared_data_t sd;
    (void)bb_shared_data_init(&sd);

    /* Set motor cmd to establish timestamp */
    bb_motor_cmd_t cmd = {0};
    (void)bb_shared_data_set_motor_cmd(&sd, &cmd);

    uint64_t last_us = 0U;
    (void)bb_shared_data_get_last_cmd_us(&sd, &last_us);

    uint64_t current = now_us();
    uint64_t elapsed = (current >= last_us) ? (current - last_us) : 0U;

    /* Elapsed should be very small (< 10ms) since we just set the cmd */
    TEST_ASSERT(elapsed < 10000U,
                "elapsed should be < 10ms right after set_motor_cmd");

    (void)bb_shared_data_destroy(&sd);
}

/* ---------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------------*/

int main(void)
{
    test_last_cmd_us_updated();
    test_estop_toggle();
    test_elapsed_time_reasonable();

    if (s_failures == 0U) {
        (void)fprintf(stdout, "test_watchdog: ALL %u TESTS PASSED\n",
                      (unsigned)s_tests_run);
    } else {
        (void)fprintf(stderr, "test_watchdog: %u FAILURES\n",
                      (unsigned)s_failures);
    }

    return (int)s_failures;
}
