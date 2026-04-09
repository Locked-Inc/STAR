/**
 * @file imu_task.c
 * @brief IMU task -- reads MPU-9250 DMP data and writes to shared_data.
 *
 * @details
 * 100 Hz loop that reads the global g_bb_mpu_data struct (populated by
 * librobotcontrol's DMP interrupt handler) and copies accelerometer,
 * gyroscope, magnetometer, and temperature data to shared_data for
 * consumption by the telemetry task.
 *
 * The MPU-9250 on the BeagleBone Blue provides:
 * - 3-axis accelerometer (m/s^2)
 * - 3-axis gyroscope (rad/s)
 * - 3-axis magnetometer (uT)
 * - On-chip temperature (degrees C)
 * - DMP-fused orientation (quaternion, Tait-Bryan angles)
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#include "imu_task.h"
#include "hardware_config.h"
#include "hardware_init.h"

#include <robotcontrol.h>
#include <time.h>

/* Axis indices from hardware_config.h: k_bb_axis_x/y/z */

/**
 * @brief imu_task thread entry point.
 *
 * @details
 * Reads MPU-9250 sensor data from the global g_bb_mpu_data struct and writes
 * to shared_data. The DMP fills g_bb_mpu_data via interrupt at the configured
 * sample rate (100 Hz).
 *
 * @param[in] arg Pointer to bb_shared_data_t
 *
 * @return void* Always NULL
 *
 * @pre rc_mpu_initialize_dmp() must have been called
 * @pre arg must point to initialized bb_shared_data_t
 * @post IMU data written to shared_data each cycle
 *
 * @since Version 1.0.0
 */
void* bb_imu_task(void* arg)
{
    if (arg == NULL) {
        return NULL;
    }

    bb_shared_data_t* sd = (bb_shared_data_t*)arg;

    /* Period derived from k_bb_rate_imu_hz in hardware_config.h */

    struct timespec next = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &next);

    while (rc_get_state() != EXITING) {
        bb_imu_data_t imu = {0};

        /*
         * Atomicity model for g_bb_mpu_data reads:
         *
         * Each individual double field is read atomically on ARM Cortex-A8
         * with VFP (VLDR is a single 64-bit load). However, cross-field
         * consistency is NOT guaranteed -- accel may come from DMP sample N
         * while gyro comes from sample N+1 if the DMP ISR fires mid-copy.
         *
         * This is acceptable for 100 Hz telemetry because:
         *   - DMP runs at the same 100 Hz rate (k_bb_rate_imu_hz)
         *   - One-sample jitter across fields is within noise margin
         *   - No control loop depends on cross-field coherence here
         *
         * For safety-critical paths requiring a fully consistent snapshot,
         * use rc_mpu_set_data_func() to register a DMP callback that copies
         * g_bb_mpu_data under a mutex instead of reading it directly.
         */
        imu.accel_mps2[k_bb_axis_x] = (float)g_bb_mpu_data.accel[k_bb_axis_x];
        imu.accel_mps2[k_bb_axis_y] = (float)g_bb_mpu_data.accel[k_bb_axis_y];
        imu.accel_mps2[k_bb_axis_z] = (float)g_bb_mpu_data.accel[k_bb_axis_z];

        imu.gyro_rps[k_bb_axis_x] = (float)g_bb_mpu_data.gyro[k_bb_axis_x];
        imu.gyro_rps[k_bb_axis_y] = (float)g_bb_mpu_data.gyro[k_bb_axis_y];
        imu.gyro_rps[k_bb_axis_z] = (float)g_bb_mpu_data.gyro[k_bb_axis_z];

        imu.mag_ut[k_bb_axis_x] = (float)g_bb_mpu_data.mag[k_bb_axis_x];
        imu.mag_ut[k_bb_axis_y] = (float)g_bb_mpu_data.mag[k_bb_axis_y];
        imu.mag_ut[k_bb_axis_z] = (float)g_bb_mpu_data.mag[k_bb_axis_z];

        imu.temp_c = (float)g_bb_mpu_data.temp;

        (void)bb_shared_data_set_imu(sd, &imu);

        next.tv_nsec += (long)k_bb_imu_period_ns;
        if (next.tv_nsec >= (long)k_bb_ns_per_sec) {
            next.tv_sec++;
            next.tv_nsec -= (long)k_bb_ns_per_sec;
        }
        (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    return NULL;
}
