/**
 * @file stm32_err.h
 * @brief STM32 firmware error codes.
 *
 * @details
 * Defines the unified error type used across all STM32 firmware libraries.
 * All public API functions return stm32_err_t. Callers must check every
 * return value (NASA Power of 10 Rule 7).
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
 * @enum stm32_err_t
 * @brief Unified error codes for all STM32 firmware libraries.
 *
 * @details
 * Every public API function returns one of these codes. The zero value
 * k_stm32_err_ok indicates success; all non-zero values indicate failure.
 *
 * @since Version 1.0.0
 */
typedef enum {
    k_stm32_err_ok                   = 0,  /**< Success */
    k_stm32_err_null_ptr             = 1,  /**< NULL pointer argument */
    k_stm32_err_invalid_arg          = 2,  /**< Invalid argument value */
    k_stm32_err_not_initialized      = 3,  /**< Module not initialized */
    k_stm32_err_already_initialized  = 4,  /**< Module already initialized */
    k_stm32_err_timeout              = 5,  /**< Operation timed out */
    k_stm32_err_buffer_full          = 6,  /**< Buffer has no space */
    k_stm32_err_buffer_empty         = 7,  /**< Buffer has no data */
    k_stm32_err_crc_mismatch         = 8,  /**< CRC validation failed */
    k_stm32_err_crc_selftest_failed  = 9,  /**< CRC self-test failed at boot */
    k_stm32_err_resync_failed        = 10, /**< Frame resync scan exhausted */
    k_stm32_err_decode_failed        = 11, /**< Protobuf decode error */
    k_stm32_err_encode_failed        = 12, /**< Protobuf encode error */
    k_stm32_err_hardware             = 13, /**< Hardware peripheral fault */
} stm32_err_t;

#ifdef __cplusplus
}
#endif
