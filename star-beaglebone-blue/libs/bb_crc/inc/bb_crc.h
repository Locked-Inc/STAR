/**
 * @file bb_crc.h
 * @brief CRC-32 IEEE 802.3 computation for BeagleBone Blue firmware.
 *
 * @details
 * Software table-based CRC-32 implementation using the reflected polynomial
 * 0xEDB88320 (normal form: 0x04C11DB7). Compatible with Go crc32.ChecksumIEEE(),
 * Python zlib.crc32(), and Ethernet FCS.
 *
 * The AM335x has no hardware CRC peripheral, so this is software-only.
 *
 * @par Algorithm:
 * - Polynomial: 0xEDB88320 (reflected)
 * - Initial value: 0xFFFFFFFF
 * - Final XOR: 0xFFFFFFFF
 * - Bit order: LSB-first (reflected)
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "bb_err.h"

/**
 * @enum bb_crc32_public_constants_t
 * @brief CRC-32/IEEE 802.3 init and finalization values.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
    k_bb_crc32_init_value = 0xFFFFFFFFU, /**< Initial CRC accumulator */
    k_bb_crc32_final_xor  = 0xFFFFFFFFU, /**< Final XOR for CRC result */
} bb_crc32_public_constants_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute CRC-32/IEEE 802.3 over a data buffer.
 *
 * @details
 * One-shot CRC-32 computation. Initializes with 0xFFFFFFFF, processes all
 * bytes via table lookup, and applies final XOR of 0xFFFFFFFF.
 *
 * @param[in]  data    Input buffer (must not be NULL)
 * @param[in]  len     Number of bytes to process (0 is valid)
 * @param[out] out_crc Computed CRC-32 value
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Success
 * @retval k_bb_err_null_ptr data or out_crc is NULL
 *
 * @pre data != NULL
 * @pre out_crc != NULL
 * @post *out_crc contains the finalized CRC-32/IEEE value
 *
 * @note Thread-safe: no shared state (pure function)
 *
 * @since Version 1.0.0
 */
bb_err_t bb_crc32_compute(const uint8_t* data, uint32_t len, uint32_t* out_crc);

/**
 * @brief Incrementally update a running CRC-32 with additional data.
 *
 * @details
 * Allows computing CRC-32 across non-contiguous buffers. The caller passes
 * the intermediate CRC state (NOT finalized). To get the final CRC, apply
 * XOR with 0xFFFFFFFF after the last update.
 *
 * Usage pattern:
 * @code
 * uint32_t crc = 0xFFFFFFFF;
 * bb_crc32_update(crc, buf1, len1, &crc);
 * bb_crc32_update(crc, buf2, len2, &crc);
 * crc ^= 0xFFFFFFFF;  // finalize
 * @endcode
 *
 * @param[in]  crc_in  Running CRC state (init with 0xFFFFFFFF for first call)
 * @param[in]  data    Input buffer (must not be NULL)
 * @param[in]  len     Number of bytes to process (0 is valid)
 * @param[out] out_crc Updated intermediate CRC state (NOT finalized)
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Success
 * @retval k_bb_err_null_ptr data or out_crc is NULL
 *
 * @pre data != NULL
 * @pre out_crc != NULL
 * @post *out_crc contains intermediate CRC state
 *
 * @note Thread-safe: no shared state (pure function)
 *
 * @since Version 1.0.0
 */
bb_err_t bb_crc32_update(uint32_t crc_in, const uint8_t* data, uint32_t len,
                         uint32_t* out_crc);

#ifdef __cplusplus
}
#endif
