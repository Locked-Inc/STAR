/**
 * @file hardware_init.c
 * @brief BeagleBone Blue hardware initialization via librobotcontrol.
 *
 * @details
 * Initializes each peripheral subsystem required by the firmware tasks.
 * Uses the modern (non-deprecated) librobotcontrol API -- each subsystem
 * is initialized individually rather than using rc_initialize().
 *
 * Initialization sequence:
 *   1. rc_motor_init() -- enables DRV8838 motor driver H-bridges
 *   2. rc_encoder_eqep_init() -- enables eQEP quadrature decoder
 *   3. rc_mpu_initialize_dmp() -- configures MPU-9250 with DMP at 100 Hz
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "hardware_init.h"

#include <robotcontrol.h>

#include "hardware_config.h"

/**
 * @var g_bb_mpu_data
 * @brief MPU-9250 data populated by DMP interrupt callback.
 *
 * @since Version 1.0.0
 */
rc_mpu_data_t g_bb_mpu_data;

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * @brief Initialize all BeagleBone Blue hardware peripherals.
 *
 * @details
 * Calls librobotcontrol subsystem initializers in dependency order.
 * Must be called from main() before spawning any task threads.
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       All peripherals initialized successfully
 * @retval k_bb_err_hardware One or more rc_* initializers failed
 *
 * @pre Process must run with sufficient privilege for /dev/mem access
 * @pre rc_kill_existing_process() should be called before this function
 *      to ensure no other firmware instance is running
 * @post Motor channels enabled; encoders zeroed; IMU streaming at 100 Hz
 *
 * @note Running on non-BBB hardware returns k_bb_err_hardware
 * @since Version 1.0.0
 */
bb_err_t bb_hardware_init(void)
{
    if (rc_motor_init() != 0) {
        return k_bb_err_hardware;
    }

    if (rc_encoder_eqep_init() != 0) {
        rc_motor_cleanup();
        return k_bb_err_hardware;
    }

    /* MPU-9250 with DMP at IMU task rate */
    rc_mpu_config_t mpu_cfg = rc_mpu_default_config();
    mpu_cfg.dmp_sample_rate = k_bb_rate_imu_hz;

    if (rc_mpu_initialize_dmp(&g_bb_mpu_data, mpu_cfg) != 0) {
        rc_encoder_eqep_cleanup();
        rc_motor_cleanup();
        return k_bb_err_hardware;
    }

    return k_bb_err_ok;
}

/**
 * @brief De-initialize all BeagleBone Blue hardware peripherals.
 *
 * @details
 * Disables motor outputs, releases encoder hardware, and powers off IMU.
 * Called from main() after all threads have joined.
 *
 * @return bb_err_t
 * @retval k_bb_err_ok De-initialization successful
 *
 * @pre bb_hardware_init() must have been called
 * @post All motor outputs disabled; all subsystems released
 *
 * @since Version 1.0.0
 */
bb_err_t bb_hardware_deinit(void)
{
    (void)rc_motor_set(k_bb_motor_all, 0.0);
    rc_mpu_power_off();
    rc_motor_cleanup();
    rc_encoder_eqep_cleanup();
    return k_bb_err_ok;
}
