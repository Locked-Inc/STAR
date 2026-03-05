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
 * @par NASA Power of 10: [OK] Bounded loops (CRC table lookup)
 * @par SOLID: S - Single responsibility (CRC only)
 *
 * @author Locked, Inc.
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set fixed CRC value to return when override is active
 *
 * @param[in] crc Fixed CRC value to return
 */
void mock_crc8_set_return_value(uint8_t crc);

/**
 * @brief Enable/disable CRC calculation override
 *
 * When enabled, rx_crc_compute() returns the value set by
 * mock_crc8_set_return_value() instead of computing a real CRC.
 *
 * @param[in] enable True to use mock value, false for real calculation
 */
void mock_crc8_set_override(bool enable);

#ifdef __cplusplus
}
#endif
