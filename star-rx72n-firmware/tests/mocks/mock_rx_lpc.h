/**
 * @file mock_rx_lpc.h
 * @brief Mock Helpers for Low Power Consumption (LPC) HAL Driver
 *
 * @details
 * The rx_lpc HAL driver exposes all host-testable state through its own
 * `#ifdef UNIT_TEST` helpers (rx_lpc_test_reset, rx_lpc_test_get_last_mode,
 * rx_lpc_test_set_deep_standby_wake, rx_lpc_test_set_pending_wake_flags),
 * declared in rx_lpc.h. Because the driver wraps every hardware register
 * write in `#ifdef __RX__`, host-side tests need no additional mocking of
 * SBYCR / DPSBYCR / DPSIER / DPSIFR: the driver simply does not touch those
 * registers when compiled for the host. This header therefore re-exports
 * the driver-owned test hooks and provides a single convenience wrapper so
 * test setUp() code can read like other test suites in this repository.
 *
 * @par Why a mock file exists at all
 * The LPC integration deliberately mirrors test_rx_iwdt in structure; having
 * a mock header of the same shape (mock_rx_lpc.h/.c) keeps the mocks/
 * directory consistent and gives future mocks a place to grow if the LPC
 * driver ever stops being fully self-contained on the host.
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_lpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset the rx_lpc driver to just-after-reset state
 *
 * @details
 * Thin wrapper over rx_lpc_test_reset(). Call from setUp() of every test to
 * isolate state between tests.
 *
 * @post Driver is uninitialized, last-mode is k_lpc_mode_none, all injected
 *       test state is cleared.
 *
 * @since Version 1.0.0
 */
void mock_rx_lpc_reset(void);

#ifdef __cplusplus
}
#endif
