/**
 * @file hardware_init.h
 * @brief BeagleBone Blue hardware initialization API.
 *
 * @details
 * Provides bb_hardware_init() and bb_hardware_deinit() which wrap
 * librobotcontrol subsystem initialization in a consistent pattern
 * that matches the STAR firmware convention.
 *
 * @see src/hardware_init.c
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <robotcontrol.h>

#include "bb_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @var g_bb_mpu_data
 * @brief Global MPU-9250 data structure populated by DMP callback.
 *
 * @details
 * librobotcontrol's DMP interrupt handler writes accelerometer, gyroscope,
 * magnetometer, temperature, and fused orientation data into this struct
 * at k_bb_rate_imu_hz (100 Hz). The imu_task reads it each cycle without
 * a mutex and copies the fields into shared_data.
 *
 * @par Atomicity
 * Individual field reads (double) are atomic on ARM Cortex-A8 with VFP
 * because VLDR/VSTR are single 64-bit bus transactions. Cross-field
 * consistency is NOT guaranteed -- the DMP ISR may update some fields
 * between consecutive reads in imu_task, so accel and gyro could come
 * from adjacent samples. This is acceptable for telemetry; for control
 * loops requiring a coherent snapshot, register a callback with
 * rc_mpu_set_data_func() and copy under a mutex.
 *
 * @warning Do not assume cross-field consistency without synchronization.
 *
 * @since Version 1.0.0
 */
extern rc_mpu_data_t g_bb_mpu_data;

/**
 * @brief Initialize all BeagleBone Blue hardware peripherals.
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       All peripherals initialized successfully
 * @retval k_bb_err_hardware One or more rc_* initializers failed
 *
 * @pre Must run with privilege; no other firmware instance running
 * @post Motor channels enabled; encoders zeroed; IMU ready
 *
 * @since Version 1.0.0
 */
bb_err_t bb_hardware_init(void);

/**
 * @brief De-initialize all BeagleBone Blue hardware peripherals.
 *
 * @return bb_err_t
 * @retval k_bb_err_ok De-initialization successful
 *
 * @pre bb_hardware_init() must have been called
 * @post All motor outputs disabled; hardware released
 *
 * @since Version 1.0.0
 */
bb_err_t bb_hardware_deinit(void);

#ifdef __cplusplus
}
#endif
