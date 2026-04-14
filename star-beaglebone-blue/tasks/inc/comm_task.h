/**
 * @file comm_task.h
 * @brief Communication task for BeagleBone Blue firmware.
 *
 * @details
 * comm_task runs at 100 Hz and is responsible for:
 * - Polling bb_usb_cdc_receive() for incoming frames from the RPi5 gateway
 * - Decoding wire frames (SYNC | SEQ | LEN | TYPE | FLAGS | PAYLOAD | CRC32)
 * - Deserializing nanopb protobuf payloads into motor commands
 * - Writing decoded commands to bb_shared_data via bb_shared_data_set_motor_cmd()
 * - Serializing and sending telemetry frames upstream at 10 Hz
 *
 * @see tasks/inc/shared_data.h
 * @see libs/bb_usb_cdc/inc/bb_usb_cdc.h
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
 * @brief comm_task thread entry point.
 *
 * @details
 * Passed to pthread_create(). Runs indefinitely until bb_shared_data estop
 * is set or the process receives SIGINT. Loops at k_bb_rate_comm_hz using
 * clock_nanosleep() for period control.
 *
 * @param[in] arg Pointer to the bb_shared_data_t instance (cast from void*)
 *
 * @return void* Always returns NULL
 *
 * @pre bb_usb_cdc_init() must have been called before this thread starts
 * @pre arg must point to a valid, initialized bb_shared_data_t
 * @post Motors commanded to zero on exit
 *
 * @note Thread-safe; does not share state except via bb_shared_data
 * @since Version 1.0.0
 */
void* bb_comm_task(void* arg);

#ifdef __cplusplus
}
#endif
