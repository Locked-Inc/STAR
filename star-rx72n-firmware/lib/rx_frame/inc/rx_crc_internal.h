/**
 * @file rx_crc_internal.h
 * @brief Internal CRC-32 Abstraction Layer
 *
 * Provides compile-time selection between hardware and software CRC-32.
 * This header is internal to the rx_frame library.
 *
 * ## Design Rationale
 *
 * The CRC module uses a compile-time selection strategy:
 *
 * 1. **Hardware CRC** (default on RX72N target):
 *    - Uses RX72N CRC Calculator peripheral at 0x00088280
 *    - ~10x faster than software for large buffers
 *    - IEEE 802.3 polynomial (0x04C11DB7)
 *    - Automatically enabled when `__RX__` is defined
 *
 * 2. **Software CRC** (default on host, available as fallback):
 *    - Lookup table implementation (256-entry, 1KB table)
 *    - Bit-exact compatible with hardware and Go's crc32.ChecksumIEEE()
 *    - Used for host-side unit testing (no hardware available)
 *    - Always compiled via rx_crc32_update_sw() for incremental CRC
 *
 * ## Why Both Implementations?
 *
 * - **Testing**: Host-side tests validate CRC correctness without hardware
 * - **Debugging**: Software can be forced on target to compare results
 * - **Incremental CRC**: rx_crc32_update_sw() is always available because
 *   hardware state management for incremental updates is complex
 *
 * ## Configuration
 *
 * | Build Target | Default | Override |
 * |--------------|---------|----------|
 * | RX72N (`__RX__`) | Hardware | Define `RX_CRC32_USE_SOFTWARE` |
 * | Host (testing) | Software | N/A |
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
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration - Compile-time Hardware/Software Selection
 *
 * This preprocessor logic implements the following decision matrix:
 *
 * | __RX__ | RX_CRC32_USE_SOFTWARE | Result              |
 * |--------|----------------------|---------------------|
 * | Yes    | No                   | Hardware (default)  |
 * | Yes    | Yes                  | Software (override) |
 * | No     | (any)                | Software (testing)  |
 *
 * To force software CRC on hardware target:
 *   gcc -DRX_CRC32_USE_SOFTWARE ...
 *   or #define RX_CRC32_USE_SOFTWARE before including this header
 *
 * The software implementation is always compiled (rx_crc32_sw.c) because:
 * 1. It's needed for host-side unit tests
 * 2. rx_crc32_update_sw() is used for incremental CRC on hardware targets
 * 3. It enables A/B comparison testing on hardware for validation
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
uint32_t rx_crc32_ieee_impl(const uint8_t* data, uint32_t len);

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
uint32_t rx_crc32_update_impl(uint32_t crc, const uint8_t* data, uint32_t len);

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
uint32_t rx_crc32_update_sw(uint32_t crc, const uint8_t* data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_CRC_INTERNAL_H */
