/**
 * @file mock_rx_tmr.h
 * @brief TMR Mock Header for Unit Tests
 *
 * @details
 * Convenience header for TMR unit tests. Includes the shadow
 * mock_rx72n_tmr_regs.h which provides all TMR register types, enums,
 * and mock accessor functions.
 *
 * System register access (system_regs, prcr_reg) comes from
 * mock_rx_onewire_hw.h and mock_rx72n_system_regs.h, included via the
 * shadow mock_rx72n_regs.h.
 *
 * @see mock_rx72n_tmr_regs.h Shadow TMR register header
 * @see rx_tmr.h TMR HAL driver API
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once
#define MOCK_RX_TMR_H

#ifdef UNIT_TEST

#include "mock_rx72n_tmr_regs.h"

#endif /* UNIT_TEST */
