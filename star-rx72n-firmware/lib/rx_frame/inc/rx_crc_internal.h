/**
 * @file rx_crc_internal.h
 * @brief Internal CRC-32 Abstraction Layer
 *
 * Provides compile-time selection between hardware and software CRC-32.
 * This header is internal to the rx_frame library.
 *
 * Configuration:
 * - Define RX_CRC32_USE_HARDWARE to use hardware CRC (default on RX target)
 * - Define RX_CRC32_USE_SOFTWARE to force software CRC (for host testing)
 *
 * References:
 * - RX72N Group User's Manual: Hardware (CRC Calculator section)
 * - https://renesas.github.io/fsp/group___c_r_c.html
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#ifndef STAR_RX_CRC_INTERNAL_H
#define STAR_RX_CRC_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration - Compile-time Hardware/Software Selection
 *
 * Default behavior:
 * - On RX target (__RX__ defined): Use hardware CRC
 * - On host (no __RX__): Use software CRC for testing
 *
 * Override by defining RX_CRC32_USE_SOFTWARE before including this header.
 * =============================================================================
 */

#if defined(__RX__) && !defined(RX_CRC32_USE_SOFTWARE)
#define RX_CRC32_USE_HARDWARE
#elif !defined(RX_CRC32_USE_SOFTWARE)
#define RX_CRC32_USE_SOFTWARE
#endif

/* =============================================================================
 * Internal API - Implementation Functions
 *
 * These functions are called by the public API in rx_crc32.c.
 * The actual implementation is provided by rx_crc32_sw.c or rx_crc32_hw.c
 * depending on the compile-time configuration.
 * =============================================================================
 */

/**
 * @brief Initialize CRC module
 *
 * For hardware: Enables CRC peripheral, configures for IEEE 802.3 CRC-32
 * For software: No-op (lookup table is static)
 *
 * @return RX_OK on success
 */
rx_err_t rx_crc_init(void);

/**
 * @brief Deinitialize CRC module
 *
 * For hardware: Optionally disables peripheral to save power
 * For software: No-op
 *
 * @return RX_OK on success
 */
rx_err_t rx_crc_deinit(void);

/**
 * @brief Calculate IEEE 802.3 CRC-32 (implementation)
 *
 * Internal implementation function called by rx_crc32_ieee().
 *
 * @param[in] data Input data buffer
 * @param[in] len  Data length in bytes
 * @return CRC-32 checksum (0 if data is NULL or len is 0)
 */
uint32_t rx_crc32_ieee_impl(const uint8_t *data, size_t len);

/**
 * @brief Update CRC-32 with additional data (implementation)
 *
 * Internal implementation function called by rx_crc32_update().
 *
 * @param[in] crc  Previous CRC value (finalized)
 * @param[in] data Additional data buffer
 * @param[in] len  Data length in bytes
 * @return Updated CRC-32 checksum
 */
uint32_t rx_crc32_update_impl(uint32_t crc, const uint8_t *data, size_t len);

/**
 * @brief Software CRC-32 update (always available)
 *
 * This function is always compiled and available, even when using hardware CRC.
 * Used as fallback for incremental CRC where hardware state management is complex.
 *
 * @param[in] crc  Previous CRC value (finalized)
 * @param[in] data Additional data buffer
 * @param[in] len  Data length in bytes
 * @return Updated CRC-32 checksum
 */
uint32_t rx_crc32_update_sw(uint32_t crc, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_CRC_INTERNAL_H */
