/* tests/mocks/mock_rx_crc.h */

/**
 * @file mock_rx_crc.h
 * @brief Mock CRC calculation functions for protocol testing
 *
 * @details
 * Provides test double for CRC (Cyclic Redundancy Check) functions to enable
 * unit testing of protocols (1-Wire, SPI, UART) that use CRC validation.
 * Implements actual CRC algorithms (not mocked calculation).
 *
 * Enables testing of:
 * - 1-Wire ROM code validation (CRC-8/MAXIM)
 * - DS18B20 temperature data validation
 * - Protocol frame integrity checking
 * - CRC failure handling
 *
 * @par Supported CRC Algorithms:
 * - CRC-8/MAXIM (polynomial 0x31): 1-Wire devices
 * - CRC-32 (polynomial 0x04C11DB7): Frame validation
 *
 * @par Usage: tests/test_rx_ds18b20.c, tests/test_rx_onewire.c
 *
 * @see rx_crc.h Real CRC implementation
 * @see rx_ds18b20.h DS18B20 driver (uses CRC-8)
 *
 * @par NASA Power of 10: ✓ Bounded loops (CRC table lookup)
 * @par SOLID: S - Single responsibility (CRC only)
 *
 * @author STAR Team
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_CRC_H
#define MOCK_RX_CRC_H

#include <stdbool.h>
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
uint8_t rx_crc8_maxim(const uint8_t* data, uint32_t length);

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
