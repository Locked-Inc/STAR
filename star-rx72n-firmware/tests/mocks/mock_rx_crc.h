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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum mock_crc8_test_values_t
 * @brief Named CRC-8 test constants for use in mock override examples
 *
 * @details
 * Provides named constants for the CRC-8 values injected via
 * mock_crc8_set_return_value(). Use these instead of magic literals in
 * test code and documentation examples so all call sites are searchable and
 * self-documenting.
 *
 * @invariant All members are test-only constants; never written at runtime.
 *
 * @code
 * // Inject a known bad CRC to verify corruption-detection logic:
 * mock_crc8_set_return_value(k_mock_crc8_injected);
 * mock_crc8_set_override(true);
 *
 * // Inject a zero CRC to verify CRC-mismatch error path:
 * mock_crc8_set_return_value(k_mock_crc8_wrong);
 * mock_crc8_set_override(true);
 * @endcode
 *
 * @see mock_crc8_set_return_value() Injects the chosen constant into the mock
 * @see mock_crc8_set_override() Activates or deactivates the override
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mock_crc8_injected = 0xAAU, /**< Injected CRC-8 value for testing corruption-detection logic */
  k_mock_crc8_wrong    = 0x00U, /**< Wrong CRC-8 value for testing CRC-mismatch error paths */
} mock_crc8_test_values_t;

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
 *
 * @code
 * // Inject a bad CRC to test corruption-detection logic in the caller:
 * mock_crc8_set_return_value(k_mock_crc8_injected);  // stage the injected value first
 * mock_crc8_set_override(true);                      // then activate the override
 * rx_err_t ret = rx_crc_compute(&cfg, data, len, &result);
 * // result == k_mock_crc8_injected regardless of the actual byte content of data
 * @endcode
 *
 * @since Version 1.0.0
 */
void mock_crc8_set_return_value(uint8_t crc);

/**
 * @brief Set the fixed 32-bit CRC value returned when mock override is active
 *
 * @details
 * Stores value directly as a uint32_t in s_mock_crc_value without truncation.
 * When the override is enabled via mock_crc8_set_override(true), rx_crc_compute()
 * returns this value for any polynomial (including CRC-32). Use this instead of
 * mock_crc8_set_return_value() when injecting CRC-32 results.
 *
 * @param[in] value Fixed 32-bit CRC value to inject
 *
 * @pre Called from the test thread; not thread-safe
 * @pre mock_crc8_set_override(true) must be called to activate the override
 * @post s_mock_crc_value set to value without truncation
 * @post Subsequent rx_crc_compute() calls return this value when override active
 *
 * @note No allocation or blocking; safe to call from setUp()
 *
 * @since Version 1.0.0
 */
void mock_crc32_set_return_value(uint32_t value);

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
 *
 * @code
 * // Test that the caller detects a bad CRC and returns an error:
 * mock_crc8_set_return_value(k_mock_crc8_wrong);  // inject a wrong checksum
 * mock_crc8_set_override(true);                   // activate override
 * TEST_ASSERT_EQUAL(k_rx_err_crc, driver_read(&handle, buf, len));
 * mock_crc8_set_override(false);                  // restore real CRC in tearDown()
 * @endcode
 *
 * @since Version 1.0.0
 */
void mock_crc8_set_override(bool enable);

#ifdef __cplusplus
}
#endif
