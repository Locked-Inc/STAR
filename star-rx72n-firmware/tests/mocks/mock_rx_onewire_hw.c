/**
 * @file mock_rx_onewire_hw.c
 * @brief Mock Hardware Implementation for OneWire Testing
 *
 * Provides mock implementations of CMT timer and system register
 * access functions for host-side OneWire driver testing.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_onewire_hw.h"

#include <string.h>

/* =============================================================================
 * Mock Hardware State
 * =============================================================================
 */

mock_cmt_channel_t g_mock_cmt3;
mock_cmt_ctrl_t    g_mock_cmt_ctrl;
mock_system_regs_t g_mock_onewire_system_regs;
uint16_t           g_mock_cmt3_counter_shadow = 0;

/* =============================================================================
 * Mock State Control Functions
 * =============================================================================
 */

void mock_onewire_hw_init(void)
{
  memset(&g_mock_cmt3, 0, sizeof(g_mock_cmt3));
  memset(&g_mock_cmt_ctrl, 0, sizeof(g_mock_cmt_ctrl));
  memset(&g_mock_onewire_system_regs, 0, sizeof(g_mock_onewire_system_regs));

  g_mock_cmt3_counter_shadow = 0;

  /* Set default system register values */
  g_mock_onewire_system_regs.prcr    = 0;
  g_mock_onewire_system_regs.mstpcrb = 0xFFFFFFFF; /* All modules stopped by default */
}

void mock_onewire_hw_deinit(void)
{
  memset(&g_mock_cmt3, 0, sizeof(g_mock_cmt3));
  memset(&g_mock_cmt_ctrl, 0, sizeof(g_mock_cmt_ctrl));
  memset(&g_mock_onewire_system_regs, 0, sizeof(g_mock_onewire_system_regs));
  g_mock_cmt3_counter_shadow = 0;
}

void mock_onewire_hw_set_timer_count(uint16_t count)
{
  g_mock_cmt3_counter_shadow = count;
  g_mock_cmt3.cmcnt          = count;
}

void mock_onewire_hw_advance_timer(uint16_t ticks)
{
  g_mock_cmt3_counter_shadow = (uint16_t)(g_mock_cmt3_counter_shadow + ticks);
  g_mock_cmt3.cmcnt          = g_mock_cmt3_counter_shadow;
}
