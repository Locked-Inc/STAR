/**
 * @file telemetry_task.h
 * @brief Telemetry task for BeagleBone Blue firmware.
 *
 * @details
 * telemetry_task runs at 10 Hz and is responsible for:
 * - Reading encoder, IMU, and motor state from bb_shared_data
 * - Serializing a telemetry protobuf message via nanopb
 * - Sending the serialized frame upstream via bb_usb_cdc_send()
 *
 * @see tasks/inc/shared_data.h
 * @see libs/bb_usb_cdc/inc/bb_usb_cdc.h
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
 * @brief telemetry_task thread entry point.
 *
 * @details
 * Passed to pthread_create(). Loops at k_bb_rate_telemetry_hz using
 * clock_nanosleep(). On each iteration reads all sensor data from shared_data
 * and sends a telemetry frame to the gateway.
 *
 * @param[in] arg Pointer to the bb_shared_data_t instance (cast from void*)
 *
 * @return void* Always returns NULL
 *
 * @pre bb_usb_cdc_init() must have been called before this thread starts
 * @pre arg must point to a valid, initialized bb_shared_data_t
 * @post No side effects on exit
 *
 * @note Thread-safe; does not share state except via bb_shared_data
 * @since Version 1.0.0
 */
void* bb_telemetry_task(void* arg);

#ifdef __cplusplus
}
#endif
