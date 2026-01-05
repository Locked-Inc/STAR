/* tests/mocks/mock_rx_onewire_hw.c */

/**
 * @file mock_rx_onewire_hw.c
 * @brief Mock Hardware Implementation for OneWire Testing
 *
 * Provides mock implementations of CMT timer and system register
 * access functions for host-side OneWire driver testing.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_onewire_hw.h"

#include <stdbool.h>
#include <string.h>

/* =============================================================================
 * Mock Hardware State
 * =============================================================================
 */

/**
 * @brief Timer auto-increment constants
 */
typedef enum {
  k_timer_auto_increment = 100, /**< Timer ticks to add on each read */
} timer_constants_t;

/** @brief Mock CMT channel 3 registers */
static mock_cmt_channel_t s_mock_cmt3;

/** @brief Shadow counter for auto-incrementing (volatile storage is for interface) */
static uint16_t s_cmt3_counter_shadow = 0;

/** @brief Mock CMT control registers */
static mock_cmt_ctrl_t s_mock_cmt_ctrl;

/** @brief Mock system registers */
static mock_system_regs_t s_mock_system;

/** @brief Flag indicating if mock is initialized */
static bool s_mock_initialized = false;

/* =============================================================================
 * Mock Hardware Access Functions
 * =============================================================================
 */

mock_cmt_channel_t* cmt3(void)
{
  /* Auto-increment timer on each access to simulate timer advancement */
  /* This prevents infinite loops in delay functions */
  s_cmt3_counter_shadow = (uint16_t)(s_cmt3_counter_shadow + k_timer_auto_increment);
  s_mock_cmt3.cmcnt     = s_cmt3_counter_shadow;

  return &s_mock_cmt3;
}

mock_cmt_ctrl_t* cmt_ctrl(void)
{
  return &s_mock_cmt_ctrl;
}

mock_system_regs_t* system_regs(void)
{
  return &s_mock_system;
}

/* =============================================================================
 * Mock State Control Functions
 * =============================================================================
 */

void mock_onewire_hw_init(void)
{
  memset(&s_mock_cmt3, 0, sizeof(s_mock_cmt3));
  memset(&s_mock_cmt_ctrl, 0, sizeof(s_mock_cmt_ctrl));
  memset(&s_mock_system, 0, sizeof(s_mock_system));

  s_cmt3_counter_shadow = 0;

  /* Set default system register values */
  s_mock_system.prcr    = 0;
  s_mock_system.mstpcrb = 0xFFFFFFFF; /* All modules stopped by default */

  s_mock_initialized = true;
}

void mock_onewire_hw_deinit(void)
{
  memset(&s_mock_cmt3, 0, sizeof(s_mock_cmt3));
  memset(&s_mock_cmt_ctrl, 0, sizeof(s_mock_cmt_ctrl));
  memset(&s_mock_system, 0, sizeof(s_mock_system));
  s_cmt3_counter_shadow = 0;
  s_mock_initialized    = false;
}

void mock_onewire_hw_set_timer_count(uint16_t count)
{
  s_cmt3_counter_shadow = count;
  s_mock_cmt3.cmcnt     = count;
}

void mock_onewire_hw_advance_timer(uint16_t ticks)
{
  s_cmt3_counter_shadow = (uint16_t)(s_cmt3_counter_shadow + ticks);
  s_mock_cmt3.cmcnt     = s_cmt3_counter_shadow;
}
