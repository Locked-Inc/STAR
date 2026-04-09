/**
 * @file shared_data.c
 * @brief Thread-safe inter-task shared data implementation.
 *
 * @details
 * All public functions acquire the internal mutex, copy data in or out,
 * then release the mutex. No function holds the mutex while sleeping or
 * doing I/O.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "shared_data.h"

#include <assert.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------------------
 * Private helpers
 * ---------------------------------------------------------------------------*/

/**
 * @brief Get the current time in microseconds since the POSIX epoch.
 *
 * @return uint64_t Microseconds since epoch (CLOCK_MONOTONIC)
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

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * @brief Initialize the shared data store.
 */
bb_err_t bb_shared_data_init(bb_shared_data_t* sd)
{
    if (sd == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)memset(sd, 0, sizeof(*sd));

    if (pthread_mutex_init(&sd->mutex, NULL) != 0) {
        return k_bb_err_hardware;
    }

    return k_bb_err_ok;
}

/**
 * @brief Destroy the shared data store.
 */
bb_err_t bb_shared_data_destroy(bb_shared_data_t* sd)
{
    if (sd == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)pthread_mutex_destroy(&sd->mutex);
    return k_bb_err_ok;
}

/**
 * @brief Write a motor command to shared data.
 */
bb_err_t bb_shared_data_set_motor_cmd(bb_shared_data_t* sd,
                                      const bb_motor_cmd_t* cmd)
{
    if (sd == NULL || cmd == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)pthread_mutex_lock(&sd->mutex);
    sd->motor_cmd  = *cmd;
    sd->last_cmd_us = internal_now_us();
    (void)pthread_mutex_unlock(&sd->mutex);

    return k_bb_err_ok;
}

/**
 * @brief Read the current motor command from shared data.
 */
bb_err_t bb_shared_data_get_motor_cmd(bb_shared_data_t* sd,
                                      bb_motor_cmd_t* cmd)
{
    if (sd == NULL || cmd == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)pthread_mutex_lock(&sd->mutex);
    *cmd = sd->motor_cmd;
    (void)pthread_mutex_unlock(&sd->mutex);

    return k_bb_err_ok;
}

/**
 * @brief Write encoder data to shared data.
 */
bb_err_t bb_shared_data_set_encoder(bb_shared_data_t* sd,
                                    const bb_encoder_data_t* enc)
{
    if (sd == NULL || enc == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)pthread_mutex_lock(&sd->mutex);
    sd->encoder = *enc;
    (void)pthread_mutex_unlock(&sd->mutex);

    return k_bb_err_ok;
}

/**
 * @brief Read encoder data from shared data.
 */
bb_err_t bb_shared_data_get_encoder(bb_shared_data_t* sd,
                                    bb_encoder_data_t* enc)
{
    if (sd == NULL || enc == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)pthread_mutex_lock(&sd->mutex);
    *enc = sd->encoder;
    (void)pthread_mutex_unlock(&sd->mutex);

    return k_bb_err_ok;
}

/**
 * @brief Write IMU data to shared data.
 */
bb_err_t bb_shared_data_set_imu(bb_shared_data_t* sd,
                                const bb_imu_data_t* imu)
{
    if (sd == NULL || imu == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)pthread_mutex_lock(&sd->mutex);
    sd->imu = *imu;
    (void)pthread_mutex_unlock(&sd->mutex);

    return k_bb_err_ok;
}

/**
 * @brief Read IMU data from shared data.
 */
bb_err_t bb_shared_data_get_imu(bb_shared_data_t* sd,
                                bb_imu_data_t* imu)
{
    if (sd == NULL || imu == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)pthread_mutex_lock(&sd->mutex);
    *imu = sd->imu;
    (void)pthread_mutex_unlock(&sd->mutex);

    return k_bb_err_ok;
}

/**
 * @brief Set or clear the emergency stop flag.
 */
bb_err_t bb_shared_data_set_estop(bb_shared_data_t* sd, bool estop)
{
    if (sd == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)pthread_mutex_lock(&sd->mutex);
    sd->estop = estop;
    (void)pthread_mutex_unlock(&sd->mutex);

    return k_bb_err_ok;
}

/**
 * @brief Read the emergency stop flag.
 */
bb_err_t bb_shared_data_get_estop(bb_shared_data_t* sd, bool* estop)
{
    if (sd == NULL || estop == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)pthread_mutex_lock(&sd->mutex);
    *estop = sd->estop;
    (void)pthread_mutex_unlock(&sd->mutex);

    return k_bb_err_ok;
}

/**
 * @brief Read the timestamp of the last valid motor command.
 */
bb_err_t bb_shared_data_get_last_cmd_us(bb_shared_data_t* sd,
                                        uint64_t* last_us)
{
    if (sd == NULL || last_us == NULL) {
        return k_bb_err_null_ptr;
    }

    (void)pthread_mutex_lock(&sd->mutex);
    *last_us = sd->last_cmd_us;
    (void)pthread_mutex_unlock(&sd->mutex);

    return k_bb_err_ok;
}
