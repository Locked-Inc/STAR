/**
 * @file bb_err.h
 * @brief BeagleBone Blue firmware error codes.
 *
 * @details
 * Defines the unified error type used across all BeagleBone Blue firmware
 * libraries. All public API functions return bb_err_t. Callers must check
 * every return value (NASA Power of 10 Rule 7).
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum bb_err_t
 * @brief Unified error codes for all BeagleBone Blue firmware libraries.
 *
 * @details
 * Every public API function returns one of these codes. The zero value
 * k_bb_err_ok indicates success; all non-zero values indicate failure.
 *
 * @code
 * bb_err_t err = bb_usb_cdc_send(data, len);
 * if (err != k_bb_err_ok) {
 *     // handle error
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
typedef enum : int32_t {
    k_bb_err_ok                  = 0,  /**< Success */
    k_bb_err_null_ptr            = 1,  /**< NULL pointer argument */
    k_bb_err_invalid_arg         = 2,  /**< Invalid argument value */
    k_bb_err_not_initialized     = 3,  /**< Module not initialized */
    k_bb_err_already_initialized = 4,  /**< Module already initialized */
    k_bb_err_timeout             = 5,  /**< Operation timed out */
    k_bb_err_buffer_full         = 6,  /**< Circular buffer has no space */
    k_bb_err_buffer_empty        = 7,  /**< Circular buffer has no data */
    k_bb_err_crc_mismatch        = 8,  /**< CRC-32 validation failed */
    k_bb_err_resync_failed       = 9,  /**< Frame resync scan exhausted */
    k_bb_err_decode_failed       = 10, /**< Protobuf decode error */
    k_bb_err_encode_failed       = 11, /**< Protobuf encode error */
    k_bb_err_hardware            = 12, /**< Hardware or OS resource fault */
    k_bb_err_io                  = 13, /**< POSIX I/O error (check errno) */
} bb_err_t;

#ifdef __cplusplus
}
#endif
