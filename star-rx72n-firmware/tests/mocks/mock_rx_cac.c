/**
 * @file mock_rx_cac.c
 * @brief Mock CAC Register Implementation for Unit Testing
 *
 * @details
 * Backing storage and reset helper for the CAC host-side mock. Provides the
 * rx_cac_regs_t instance referenced by mock_rx_cac.h so unit tests can inspect
 * or modify CAC register state without hardware.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto / setjmp / recursion
 * - Rule 2: [OK] No loops
 * - Rule 3: [OK] Zero dynamic memory allocation
 * - Rule 4: [OK] Function under 10 lines
 * - Rule 5: N/A (trivial reset helper)
 * - Rule 10: [OK] -Wall -Wextra -Werror clean
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "mock_rx_cac.h"

#include <string.h>

/**
 * @var g_mock_cac_regs
 * @brief Host-side mirror of the CAC peripheral's register bank
 * @details Replaces the memory-mapped block at 0x0008B000 for unit testing.
 * Zero-initialized at program start; tests should call mock_rx_cac_reset()
 * in setUp() to guarantee a known state between cases.
 * @since Version 1.0.0
 */
rx_cac_regs_t g_mock_cac_regs;

void mock_rx_cac_reset(void)
{
  /* C99 compound-literal zero-init -- avoids cert-msc24-c (memset insecure)
   * and lets the compiler emit whatever zero-fill it likes (memset, vector
   * stores, etc.). */
  g_mock_cac_regs = (rx_cac_regs_t){0};
}
