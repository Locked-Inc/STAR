/* lib/rx_crc/inc/rx_crc.h */

/**
 * @file rx_crc.h
 * @brief Public CRC helpers shared across STAR firmware modules.
 *
 * Exposes the IEEE 802.3 CRC-32 helpers that were historically part of
 * rx_frame so other peripherals (OneWire, SMBus, etc.) can reuse them without
 * pulling in the entire frame layer.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_CRC_H
#define STAR_RX_CRC_H

#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the CRC backend.
 *
 * On RX72N targets this enables the hardware CRC peripheral, while on the host
 * build this is a no-op.
 *
 * @return k_rx_ok on success.
 */
rx_err_t rx_crc_init(void);

/**
 * @brief Deinitialize the CRC backend (hardware only).
 *
 * @return k_rx_ok on success.
 */
rx_err_t rx_crc_deinit(void);

/**
 * @brief Calculate IEEE 802.3 CRC-32 for a buffer.
 *
 * Uses polynomial 0x04C11DB7 (reflected form 0xEDB88320). Compatible with Go's
 * `crc32.ChecksumIEEE()`.
 *
 * @param[in] data Input buffer (may be NULL when len is zero).
 * @param[in] len  Number of bytes to include.
 * @return Computed CRC-32 checksum.
 */
uint32_t rx_crc32_ieee(const uint8_t* data, uint32_t len);

/**
 * @brief Update an existing CRC value with more data.
 *
 * Allows incremental CRC calculation across multiple buffers.
 *
 * @param[in] crc  Previous CRC value (from rx_crc32_ieee or rx_crc32_update).
 * @param[in] data Additional data to consume.
 * @param[in] len  Number of bytes to add.
 * @return Updated CRC-32 checksum.
 */
uint32_t rx_crc32_update(uint32_t crc, const uint8_t* data, uint32_t len);

/**
 * @brief Calculate Dallas/Maxim CRC-8 for OneWire ROM/scratchpad.
 *
 * Polynomial x^8 + x^5 + x^4 + 1 (0x31, reflected 0x8C), initial value 0, LSB-first.
 * Matches CRC used by DS18B20 and other Maxim OneWire devices.
 *
 * @param[in] data Buffer to checksum (NULL allowed when len is 0).
 * @param[in] len  Number of bytes.
 * @return CRC-8 value (0 if data NULL or len 0).
 */
uint8_t rx_crc8_maxim(const uint8_t* data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_CRC_H */
