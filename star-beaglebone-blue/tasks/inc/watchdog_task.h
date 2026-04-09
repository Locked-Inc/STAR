/**
 * @file watchdog_task.h
 * @brief Communication watchdog task for BeagleBone Blue firmware.
 *
 * @details
 * watchdog_task runs at 10 Hz and monitors communication liveness. If the
 * timestamp of the last valid motor command (sd->last_cmd_us) is older than
 * k_bb_watchdog_ms, it asserts estop via bb_shared_data_set_estop().
 *
 * motor_control_task reads the estop flag and zeros all motor outputs when
 * estop is asserted.
 *
 * @see tasks/inc/shared_data.h
 * @see src/inc/hardware_config.h k_bb_watchdog_ms
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
 * @brief watchdog_task thread entry point.
 *
 * @details
 * Passed to pthread_create(). Loops at k_bb_rate_watchdog_hz using
 * clock_nanosleep(). On each iteration checks the age of the last received
 * motor command and asserts estop if it exceeds k_bb_watchdog_ms.
 *
 * @param[in] arg Pointer to the bb_shared_data_t instance (cast from void*)
 *
 * @return void* Always returns NULL
 *
 * @pre arg must point to a valid, initialized bb_shared_data_t
 * @post estop asserted in shared_data on exit
 *
 * @note Thread-safe; does not share state except via bb_shared_data
 * @since Version 1.0.0
 */
void* bb_watchdog_task(void* arg);

#ifdef __cplusplus
}
#endif
