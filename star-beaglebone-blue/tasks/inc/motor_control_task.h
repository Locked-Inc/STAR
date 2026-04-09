/**
 * @file motor_control_task.h
 * @brief Motor control task for BeagleBone Blue firmware.
 *
 * @details
 * motor_control_task runs at 100 Hz and is responsible for:
 * - Reading motor commands from bb_shared_data via bb_shared_data_get_motor_cmd()
 * - Writing PWM duty cycles to the 4 DC motor channels via rc_motor_set()
 * - Reading quadrature encoder tick counts via rc_encoder_read()
 * - Writing encoder data to bb_shared_data via bb_shared_data_set_encoder()
 * - Commanding all motors to 0 when estop is asserted
 *
 * @see tasks/inc/shared_data.h
 * @see librobotcontrol rc_motor.h, rc_encoder_eqep.h
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "bb_err.h"
#include "shared_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief motor_control_task thread entry point.
 *
 * @details
 * Passed to pthread_create(). Loops at k_bb_rate_motor_hz using
 * clock_nanosleep(). On each iteration:
 * 1. Check estop flag -- if set, rc_motor_set() all channels to 0
 * 2. Read motor command from shared_data
 * 3. Apply duty cycles via rc_motor_set()
 * 4. Read encoders via rc_encoder_read() and write to shared_data
 *
 * @param[in] arg Pointer to the bb_shared_data_t instance (cast from void*)
 *
 * @return void* Always returns NULL
 *
 * @pre rc_motor_init() and rc_encoder_eqep_init() must be called before start
 * @pre arg must point to a valid, initialized bb_shared_data_t
 * @post All motors disabled (rc_motor_set() to 0) on exit
 *
 * @note Thread-safe; does not share state except via bb_shared_data
 * @since Version 1.0.0
 */
void* bb_motor_control_task(void* arg);

#ifdef __cplusplus
}
#endif
