/**
 * @file imu_task.c
 * @brief IMU task -- polls MPU-9250 sensor data and writes to shared_data.
 *
 * @details
 * 100 Hz loop that polls accelerometer and gyroscope data from the MPU-9250
 * via rc_mpu_read_accel/gyro into the global g_bb_mpu_data struct, then
 * copies accelerometer, gyroscope, magnetometer, and temperature data to
 * shared_data for consumption by the telemetry task.
 *
 * The MPU-9250 on the BeagleBone Blue provides:
 * - 3-axis accelerometer (m/s^2)
 * - 3-axis gyroscope (rad/s)
 * - 3-axis magnetometer (uT)
 * - On-chip temperature (degrees C)
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
 * Polls MPU-9250 accelerometer and gyroscope into g_bb_mpu_data via
 * rc_mpu_read_accel/gyro, then writes sensor data to shared_data at 100 Hz.
 *
 * @param[in] arg Pointer to bb_shared_data_t
 *
 * @return void* Always NULL
 *
 * @pre rc_mpu_initialize() must have been called
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

        /* Poll MPU-9250 accelerometer and gyroscope.
         * DMP interrupt mode does not work on 5.10-ti kernel (GPIO ISR
         * never fires). Polling at 100 Hz via rc_mpu_read_accel/gyro is
         * reliable and produces fresh sensor data every cycle. */
        const int accel_status = rc_mpu_read_accel(&g_bb_mpu_data);
        const int gyro_status = rc_mpu_read_gyro(&g_bb_mpu_data);
        const int mag_status  = rc_mpu_read_mag(&g_bb_mpu_data);
        const int temp_status = rc_mpu_read_temp(&g_bb_mpu_data);

        /* Only publish when all reads succeed; stale data is worse than a
         * skipped cycle. The sleep at the bottom still runs to keep cadence. */
        if ((accel_status == 0) && (gyro_status == 0) &&
            (mag_status == 0) && (temp_status == 0)) {
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
        }

        next.tv_nsec += (long)k_bb_imu_period_ns;
        if (next.tv_nsec >= (long)k_bb_ns_per_sec) {
            next.tv_sec++;
            next.tv_nsec -= (long)k_bb_ns_per_sec;
        }
        (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    return NULL;
}
