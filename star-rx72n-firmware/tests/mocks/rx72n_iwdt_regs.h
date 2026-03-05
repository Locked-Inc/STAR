/**
 * @file rx72n_iwdt_regs.h
 * @brief Mock IWDT Register Definitions for Unit Testing
 *
 * @details
 * Wraps the real rx72n_iwdt_regs.h but overrides the iwdt() inline accessor
 * to return a pointer to a mock register area instead of the hardware address.
 * The mock directory is included BEFORE the HAL directory so this file shadows
 * the real header.
 *
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

/* Rename the real iwdt() to real_iwdt() so we can substitute our mock */
#define iwdt real_iwdt

/* Include the real header for all type/enum/struct definitions */
#include "../../libs/rx_hal/inc/rx72n_iwdt_regs.h"

/* Undo the rename */
#undef iwdt

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mock IWDT register area (allocated in mock_rx_system_regs.c)
 */
extern rx_iwdt_regs_t g_mock_iwdt_regs;

#ifdef __cplusplus
}
#endif

/**
 * @brief Mock iwdt() accessor returning pointer to mock register area
 *
 * @return Pointer to mock IWDT registers (safe to read/write in tests)
 */
static inline volatile rx_iwdt_regs_t* iwdt(void)
{
  return (volatile rx_iwdt_regs_t*)&g_mock_iwdt_regs;
}
