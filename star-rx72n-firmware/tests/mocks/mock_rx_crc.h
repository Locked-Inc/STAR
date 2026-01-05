/* tests/mocks/mock_rx_crc.h */

/**
 * @file mock_rx_crc.h
 * @brief Mock CRC Implementation for Host-Side Testing
 *
 * Provides mock CRC functions for testing OneWire ROM search
 * and other CRC-dependent functionality.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_CRC_H
#define MOCK_RX_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CRC-8 Maxim polynomial implementation
 *
 * Used by Dallas/Maxim 1-Wire devices for ROM code and data validation.
 * Polynomial: x^8 + x^5 + x^4 + 1 (0x31, reversed 0x8C)
 *
 * @param[in] data Pointer to data buffer
 * @param[in] length Number of bytes to process
 *
 * @return Computed CRC-8 value
 */
uint8_t rx_crc8_maxim(const uint8_t* data, size_t length);

/**
 * @brief Set fixed CRC-8 value to return (for testing)
 *
 * @param[in] crc Fixed CRC value to return
 */
void mock_crc8_set_return_value(uint8_t crc);

/**
 * @brief Enable/disable CRC calculation override
 *
 * @param[in] enable True to use mock value, false for real calculation
 */
void mock_crc8_set_override(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RX_CRC_H */
