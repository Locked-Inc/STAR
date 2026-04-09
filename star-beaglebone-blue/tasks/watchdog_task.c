/**
 * @file watchdog_task.c
 * @brief Communication watchdog task implementation.
 *
 * @details
 * Monitors the timestamp of the last received motor command. If no valid
 * command is received within k_bb_watchdog_ms, asserts estop to zero all
 * motor outputs.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "watchdog_task.h"
#include "hardware_config.h"

#include <assert.h>
#include <robotcontrol.h>
#include <time.h>

/**
 * @brief Get the current time in microseconds (CLOCK_MONOTONIC).
 *
 * @return uint64_t Microseconds since an arbitrary fixed epoch
 *
 * @pre CLOCK_MONOTONIC must be available on the platform
 * @post Returned value increases monotonically across successive calls
 *
 * @since Version 1.0.0
 */
static uint64_t internal_now_us(void)
{
    struct timespec ts = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * k_bb_us_per_sec
         + (uint64_t)(ts.tv_nsec / (long)k_bb_ns_per_us);
}

/**
 * @brief watchdog_task thread entry point.
 *
 * @details
 * 10 Hz loop. On each iteration reads the last_cmd_us timestamp from
 * shared_data. If the elapsed time exceeds k_bb_watchdog_ms, asserts estop.
 * Clears estop if a fresh command has been received.
 *
 * @param[in] arg Pointer to bb_shared_data_t
 *
 * @return void* Always NULL
 *
 * @pre arg must point to a valid initialized bb_shared_data_t
 * @post estop asserted in shared_data when watchdog fires
 * @post estop cleared when comm resumes
 *
 * @since Version 1.0.0
 */
void* bb_watchdog_task(void* arg)
{
    if (arg == NULL) {
        return NULL;
    }

    bb_shared_data_t* sd = (bb_shared_data_t*)arg;

    /* Period derived from k_bb_rate_watchdog_hz in hardware_config.h */

    struct timespec next = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &next);

    /* Watchdog threshold in microseconds */
    typedef enum : uint64_t {
        k_us_per_ms   = 1000ULL,
        k_watchdog_us = (uint64_t)k_bb_watchdog_ms * k_us_per_ms,
    } watchdog_threshold_t;

    while (rc_get_state() != EXITING) {
        uint64_t last_us = 0U;
        (void)bb_shared_data_get_last_cmd_us(sd, &last_us);

        uint64_t const now_us  = internal_now_us();
        uint64_t const elapsed = (now_us >= last_us) ? (now_us - last_us) : 0U;
        bool const     expired = (elapsed > k_watchdog_us);

        (void)bb_shared_data_set_estop(sd, expired);

        next.tv_nsec += (long)k_bb_watchdog_period_ns;
        if (next.tv_nsec >= (long)k_bb_ns_per_sec) {
            next.tv_sec++;
            next.tv_nsec -= (long)k_bb_ns_per_sec;
        }
        (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    return NULL;
}
