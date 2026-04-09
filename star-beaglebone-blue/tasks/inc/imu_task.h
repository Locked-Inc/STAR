/**
 * @file imu_task.h
 * @brief IMU task for BeagleBone Blue firmware.
 *
 * @details
 * imu_task runs at 100 Hz and is responsible for:
 * - Reading 9-axis IMU data (accel, gyro, mag) via rc_mpu_read_accel_data(),
 *   rc_mpu_read_gyro_data(), rc_mpu_read_mag_data()
 * - Writing IMU data to bb_shared_data via bb_shared_data_set_imu()
 *
 * The BeagleBone Blue uses an MPU-9250 (accelerometer + gyroscope) with
 * an AK8963 magnetometer.
 *
 * @see tasks/inc/shared_data.h
 * @see librobotcontrol rc_mpu.h
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "shared_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief imu_task thread entry point.
 *
 * @details
 * Passed to pthread_create(). Loops at k_bb_rate_imu_hz using
 * clock_nanosleep(). On each iteration reads the MPU-9250 and writes
 * the result to shared data.
 *
 * @param[in] arg Pointer to the bb_shared_data_t instance (cast from void*)
 *
 * @return void* Always returns NULL
 *
 * @pre rc_mpu_initialize() must be called before this thread starts
 * @pre arg must point to a valid, initialized bb_shared_data_t
 * @post No side effects on exit
 *
 * @note Thread-safe; does not share state except via bb_shared_data
 * @since Version 1.0.0
 */
void* bb_imu_task(void* arg);

#ifdef __cplusplus
}
#endif
