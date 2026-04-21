/**
 * @file mock_rx_lpc.c
 * @brief Mock Helpers for Low Power Consumption (LPC) HAL Driver
 *
 * @details
 * Trivial wrappers over the UNIT_TEST helpers already defined inside
 * rx_lpc.c. See mock_rx_lpc.h for the rationale.
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "mock_rx_lpc.h"

void mock_rx_lpc_reset(void)
{
  rx_lpc_test_reset();
}
