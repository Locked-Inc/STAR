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
 * @brief Set the fixed CRC-8 value returned when mock override is active
 *
 * @details
 * Stores crc as a uint32_t internally (widened). When the override is enabled
 * via mock_crc8_set_override(true), rx_crc_compute() returns this value
 * directly without computing the real CRC. Use this to inject specific CRC
 * values (including intentionally wrong values) to test error-handling paths.
 *
 * @param[in] crc Fixed CRC-8 value to inject (0x00-0xFF)
 *
 * @pre Called from the test thread; not thread-safe
 * @pre mock_crc8_set_override(true) must be called to activate the override
 * @post s_mock_crc_value set to (uint32_t)crc
 * @post Subsequent rx_crc_compute() calls return this value when override active
 *
 * @note No allocation or blocking; safe to call from setUp()
 * @since Version 1.0.0
 */
void mock_crc8_set_return_value(uint8_t crc);

/**
 * @brief Enable or disable the CRC calculation override
 *
 * @details
 * When enabled (true), rx_crc_compute() skips the real algorithm and returns
 * the value previously set by mock_crc8_set_return_value(). When disabled
 * (false), rx_crc_compute() performs the actual CRC-8/Maxim computation.
 *
 * Use this to force a CRC mismatch in tests that verify corruption detection.
 *
 * @param[in] enable true = use injected mock value; false = compute real CRC
 *
 * @pre Called from the test thread; not thread-safe
 * @pre mock_crc8_set_return_value() should be called first when enabling
 * @post s_override_enabled set to enable
 * @post All subsequent rx_crc_compute() calls respect the new setting
 *
 * @note Reset by calling mock_crc8_set_override(false) in tearDown()
 * @since Version 1.0.0
 */
void mock_crc8_set_override(bool enable);

#ifdef __cplusplus
}
#endif
