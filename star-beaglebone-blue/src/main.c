/**
 * @file main.c
 * @brief BeagleBone Blue firmware entry point.
 *
 * @details
 * System initialization and task thread launch sequence (Phase 1):
 *   1. rc_kill_existing_process() -- ensures only one instance runs
 *   2. bb_hardware_init() -- rc_initialize, motors, encoders
 *   3. bb_shared_data_init() -- zero all inter-task state, init mutex
 *   4. bb_usb_cdc_init() -- open /dev/ttyGS0, configure termios
 *   5. Spawn pthreads: comm, motor_control, imu, telemetry, watchdog
 *   6. pthread_join() on all threads (runs until SIGINT)
 *   7. bb_hardware_deinit() + bb_usb_cdc_deinit() on exit
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "bb_usb_cdc.h"
#include "hardware_config.h"
#include "hardware_init.h"
#include "shared_data.h"

#include "comm_task.h"
#include "imu_task.h"
#include "motor_control_task.h"
#include "telemetry_task.h"
#include "watchdog_task.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include <robotcontrol.h>

/* ---------------------------------------------------------------------------
 * Private constants
 * ---------------------------------------------------------------------------*/

/**
 * @enum main_thread_count_t
 * @brief Number of task threads spawned by main().
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_thread_comm         = 0U, /**< comm_task thread index */
    k_thread_motor        = 1U, /**< motor_control_task thread index */
    k_thread_imu          = 2U, /**< imu_task thread index */
    k_thread_telemetry    = 3U, /**< telemetry_task thread index */
    k_thread_watchdog     = 4U, /**< watchdog_task thread index */
    k_thread_count        = 5U, /**< Total number of task threads */
} main_thread_count_t;

/* ---------------------------------------------------------------------------
 * Private state
 * ---------------------------------------------------------------------------*/

/**
 * @var s_shared_data
 * @brief Global shared data instance -- owned by main().
 *
 * @warning Access only via bb_shared_data_*() API.
 * @since Version 1.0.0
 */
static bb_shared_data_t s_shared_data;

/* Signal handling is done via rc_enable_signal_handler() from librobotcontrol.
 * It catches SIGINT/SIGTERM and sets rc_state to EXITING automatically.
 * No custom signal handler needed -- avoids async-signal-safety issues. */

/* ---------------------------------------------------------------------------
 * main()
 * ---------------------------------------------------------------------------*/

/**
 * @brief Firmware entry point.
 *
 * @details
 * Performs the full initialization sequence then blocks on pthread_join()
 * for all task threads. On SIGINT, sets estop and waits for threads to exit.
 *
 * @return int 0 on clean exit, 1 on initialization failure
 *
 * @since Version 1.0.0
 */
int main(void)
{
    /* Step 1: Kill any existing firmware instance */
    if (rc_kill_existing_process(2.0F) < -2) {
        return 1;
    }

    /* Step 2: Initialize hardware */
    if (bb_hardware_init() != k_bb_err_ok) {
        fprintf(stderr, "bb_hardware_init failed\n");
        return 1;
    }

    /* Step 3: Initialize shared data */
    if (bb_shared_data_init(&s_shared_data) != k_bb_err_ok) {
        fprintf(stderr, "bb_shared_data_init failed\n");
        (void)bb_hardware_deinit();
        return 1;
    }

    /* Step 4: Initialize USB CDC transport */
    if (bb_usb_cdc_init(BB_USB_CDC_DEVICE) != k_bb_err_ok) {
        fprintf(stderr, "bb_usb_cdc_init failed on %s\n", BB_USB_CDC_DEVICE);
        (void)bb_shared_data_destroy(&s_shared_data);
        (void)bb_hardware_deinit();
        return 1;
    }

    /* Step 5: Enable librobotcontrol signal handler (SIGINT/SIGTERM -> EXITING) */
    (void)rc_enable_signal_handler();
    rc_set_state(RUNNING);

    /* Step 6: Spawn task threads */
    pthread_t threads[k_thread_count];

    typedef void* (*thread_fn_t)(void*);
    static const thread_fn_t k_thread_fns[k_thread_count] = {
        [k_thread_comm]      = bb_comm_task,
        [k_thread_motor]     = bb_motor_control_task,
        [k_thread_imu]       = bb_imu_task,
        [k_thread_telemetry] = bb_telemetry_task,
        [k_thread_watchdog]  = bb_watchdog_task,
    };

    uint8_t threads_created = 0U;

    for (uint8_t i = 0U; i < (uint8_t)k_thread_count; i++) {
        if (pthread_create(&threads[i], NULL, k_thread_fns[i],
                           &s_shared_data) != 0) {
            (void)fprintf(stderr, "pthread_create failed for thread %u\n", i);
            (void)rc_set_state(EXITING);
            break;
        }
        threads_created++;
    }

    /* Step 7: Wait for successfully created threads only */
    for (uint8_t i = 0U; i < threads_created; i++) {
        (void)pthread_join(threads[i], NULL);
    }

    /* Step 8: Clean up */
    (void)bb_usb_cdc_deinit();
    (void)bb_shared_data_destroy(&s_shared_data);
    (void)bb_hardware_deinit();

    return 0;
}
