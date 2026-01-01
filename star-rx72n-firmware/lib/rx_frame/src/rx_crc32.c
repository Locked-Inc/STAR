/* lib/rx_frame/src/rx_crc32.c */

/**
 * @file rx_crc32.c
 * @brief IEEE 802.3 CRC-32 Public API
 *
 * Provides CRC-32 calculation using the IEEE 802.3 polynomial (0x04C11DB7).
 * This file contains the public API that delegates to either hardware or
 * software implementations depending on compile-time configuration.
 *
 * Implementation Selection:
 * - On RX72N target (__RX__ defined): Uses hardware CRC Calculator peripheral
 * - On host (testing): Uses software lookup table implementation
 * - Override with RX_CRC32_USE_SOFTWARE to force software mode
 *
 * Bit-exact compatible with Go's crc32.ChecksumIEEE().
 *
 * References:
 * - IEEE 802.3-2018 Section 3.2.9 (Frame check sequence)
 * - RX72N Group User's Manual: Hardware (CRC Calculator section)
 * - https://renesas.github.io/fsp/group___c_r_c.html
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_crc_internal.h"
#include "rx_frame.h"

/* =============================================================================
 * Public API
 *
 * These functions delegate to the appropriate implementation (hw or sw)
 * based on compile-time configuration in rx_crc_internal.h.
 * =============================================================================
 */

/**
 * @brief Calculate IEEE 802.3 CRC-32 checksum
 *
 * @param[in] data Input data buffer
 * @param[in] len  Data length in bytes
 * @return CRC-32 checksum (0 if data is NULL or len is 0)
 */
uint32_t rx_crc32_ieee(const uint8_t* data, uint32_t len)
{
  return rx_crc32_ieee_impl(data, len);
}

/**
 * @brief Update CRC-32 with additional data (for incremental calculation)
 *
 * Use this to calculate CRC over non-contiguous data or streaming data.
 *
 * Example:
 * @code
 * uint32_t crc = rx_crc32_ieee(header, header_len);
 * crc = rx_crc32_update(crc, payload, payload_len);
 * @endcode
 *
 * @param[in] crc  Previous CRC value (from rx_crc32_ieee or rx_crc32_update)
 * @param[in] data Additional data buffer
 * @param[in] len  Data length in bytes
 * @return Updated CRC-32 checksum
 */
uint32_t rx_crc32_update(uint32_t crc, const uint8_t* data, uint32_t len)
{
  return rx_crc32_update_impl(crc, data, len);
}
