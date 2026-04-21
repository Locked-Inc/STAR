/**
 * @file mock_rx72n_eccram_regs.h
 * @brief Mock ECCRAM Register Definitions for Unit Testing
 *
 * @details
 * Wraps the real rx72n_eccram_regs.h but overrides the eccram_regs() inline
 * accessor to return a pointer to a mock register area instead of the hardware
 * address. Source files that use ECCRAM registers select this header via
 * `#ifdef UNIT_TEST` guards:
 * - `#include "mock_rx72n_eccram_regs.h"` in test builds
 * - `#include "rx72n_eccram_regs.h"` in production builds
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

/* Rename the real eccram_regs() to real_eccram_regs() so we can substitute
 * the mock accessor without violating C's one-definition rule. The same
 * trick is applied to system_regs() so that the ECCRAM driver's calls to
 * system_regs()->mstpcrc are routed to the mock register area below. */
#define eccram_regs real_eccram_regs
#define system_regs real_system_regs

/* Include the real headers for all type/enum/struct definitions */
#include "../../libs/rx_hal/inc/rx72n_eccram_regs.h"
#include "mock_rx72n_system_regs.h"

/* Undo the renames so the symbols below are the ones test/driver code
 * binds to */
#undef eccram_regs
#undef system_regs

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mock ECCRAM register area (defined in test_rx_eccram.c)
 *
 * @details
 * Ordinary memory block that stands in for the hardware register file at
 * 0x000812C0. Tests write to this structure to simulate hardware state
 * transitions, then observe how the driver reacts.
 */
extern rx_eccram_regs_t g_mock_eccram_regs;

/**
 * @brief Mock system register area (defined in test_rx_eccram.c)
 *
 * @details
 * Ordinary memory block that stands in for the hardware system register
 * file at 0x00080000. Only mstpcrc is actively read/written by the ECCRAM
 * driver; the remaining fields are present for structural compatibility.
 */
extern rx_system_regs_t g_mock_eccram_system_regs;

#ifdef __cplusplus
}
#endif

/**
 * @brief Mock eccram_regs() accessor returning pointer to mock register area
 *
 * @return Pointer to mock ECCRAM registers (safe to read/write in tests)
 *
 * @note Shadows the real accessor renamed to real_eccram_regs() above.
 * @since Version 1.0.0
 */
static inline volatile rx_eccram_regs_t* eccram_regs(void)
{
  return (volatile rx_eccram_regs_t*)&g_mock_eccram_regs;
}

/**
 * @brief Mock system_regs() accessor returning pointer to mock system regs
 *
 * @return Pointer to mock system registers (safe to read/write in tests)
 *
 * @note Shadows the real accessor renamed to real_system_regs() above.
 * @since Version 1.0.0
 */
static inline volatile rx_system_regs_t* system_regs(void)
{
  return (volatile rx_system_regs_t*)&g_mock_eccram_system_regs;
}
