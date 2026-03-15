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

/* =============================================================================
 * File-Scope Named Constants
 * =============================================================================
 */

/**
 * @enum mock_onewire_hw_const_t
 * @brief Named constants for mock hardware initialization
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_mock_mstpcrb_all_stopped = 0xFFFFFFFFU, /**< All modules stopped by default */
} mock_onewire_hw_const_t;

/* =============================================================================
 * Mock Hardware State
 * =============================================================================
 */

mock_cmt_channel_t g_mock_cmt3;
mock_cmt_ctrl_t    g_mock_cmt_ctrl;
mock_system_regs_t g_mock_onewire_system_regs;
uint16_t           g_mock_cmt3_counter_shadow = 0;
uint16_t g_mock_onewire_timer_auto_increment  = k_mock_onewire_timer_auto_increment_default;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Zero-fill a mock_cmt_channel_t struct via field assignment
 * @param[out] ch Channel struct to clear
 */
static inline void internal_clear_cmt_channel(mock_cmt_channel_t* ch)
{
  ch->cmcr  = 0;
  ch->cmcnt = 0;
  ch->cmcor = 0;
}

/**
 * @brief Zero-fill a mock_cmt_ctrl_t struct via field assignment
 * @param[out] ctrl Control struct to clear
 */
static inline void internal_clear_cmt_ctrl(mock_cmt_ctrl_t* ctrl)
{
  ctrl->cmstr0 = 0;
  ctrl->cmstr1 = 0;
}

/**
 * @brief Zero-fill a mock_system_regs_t struct via field assignment
 * @param[out] regs System register struct to clear
 */
static inline void internal_clear_system_regs(mock_system_regs_t* regs)
{
  regs->prcr    = 0;
  regs->mstpcra = 0;
  regs->mstpcrb = 0;
  regs->mstpcrc = 0;
  regs->mstpcrd = 0;
}

/* =============================================================================
 * Mock State Control Functions
 * =============================================================================
 */

void mock_onewire_hw_init(void)
{
  internal_clear_cmt_channel(&g_mock_cmt3);
  internal_clear_cmt_ctrl(&g_mock_cmt_ctrl);
  internal_clear_system_regs(&g_mock_onewire_system_regs);

  g_mock_cmt3_counter_shadow          = 0;
  g_mock_onewire_timer_auto_increment = k_mock_onewire_timer_auto_increment_default;

  /* Set default system register values */
  g_mock_onewire_system_regs.prcr    = 0;
  g_mock_onewire_system_regs.mstpcrb = k_mock_mstpcrb_all_stopped;
}

void mock_onewire_hw_deinit(void)
{
  internal_clear_cmt_channel(&g_mock_cmt3);
  internal_clear_cmt_ctrl(&g_mock_cmt_ctrl);
  internal_clear_system_regs(&g_mock_onewire_system_regs);
  g_mock_cmt3_counter_shadow          = 0;
  g_mock_onewire_timer_auto_increment = k_mock_onewire_timer_auto_increment_default;
}

void mock_onewire_hw_set_timer_auto_increment(uint16_t increment)
{
  g_mock_onewire_timer_auto_increment = increment;
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
