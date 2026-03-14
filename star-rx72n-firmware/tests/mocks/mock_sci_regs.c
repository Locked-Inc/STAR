/**
 * @file mock_sci_regs.c
 * @brief Mock SCI Register Implementation
 *
 * Provides mock register instances and helper functions for testing
 * UART driver code on the host without actual hardware.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_sci_regs.h"

#include "rx72n_sci_regs.h"

/* =============================================================================
 * Global Mock Register Instances
 * =============================================================================
 */

mock_sci_regs_t g_mock_sci[k_mock_sci_channel_count];

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

void mock_sci_regs_init(void)
{
  mock_sci_regs_clear();

  /* Set reasonable defaults for UART testing */
  for (uint8_t ch = 0; ch < k_mock_sci_channel_count; ch++) {
    /* TDRE = 1: Transmit buffer is empty (ready to send) */
    g_mock_sci[ch].ssr = k_mock_sci_ssr_tdre;

    /* SMR = 0x00: Async mode, 8 data bits, no parity, 1 stop bit */
    g_mock_sci[ch].smr = 0x00;

    /* SCR = 0x00: TX/RX disabled initially */
    g_mock_sci[ch].scr = 0x00;
  }
}

void mock_sci_regs_clear(void)
{
  uint8_t* const p   = (uint8_t*)g_mock_sci;
  const uint32_t len = (uint32_t)sizeof(g_mock_sci);
  for (uint32_t i = 0U; i < len; i++) {
    p[i] = 0U;
  }
}

void mock_sci_set_ssr(uint8_t channel, uint8_t value)
{
  if (channel < k_mock_sci_channel_count) {
    g_mock_sci[channel].ssr = value;
  }
}

void mock_sci_set_tdre(uint8_t channel, bool empty)
{
  if (channel < k_mock_sci_channel_count) {
    if (empty) {
      g_mock_sci[channel].ssr |= k_mock_sci_ssr_tdre;
    } else {
      g_mock_sci[channel].ssr &= (uint8_t)~k_mock_sci_ssr_tdre;
    }
  }
}

void mock_sci_set_rdrf(uint8_t channel, bool full)
{
  if (channel < k_mock_sci_channel_count) {
    if (full) {
      g_mock_sci[channel].ssr |= k_mock_sci_ssr_rdrf;
    } else {
      g_mock_sci[channel].ssr &= (uint8_t)~k_mock_sci_ssr_rdrf;
    }
  }
}

void mock_sci_set_rx_data(uint8_t channel, uint8_t data)
{
  if (channel < k_mock_sci_channel_count) {
    g_mock_sci[channel].rdr = data;
    /* Set RDRF flag to indicate data is available */
    g_mock_sci[channel].ssr |= k_mock_sci_ssr_rdrf;
  }
}

uint8_t mock_sci_get_tx_data(uint8_t channel)
{
  if (channel < k_mock_sci_channel_count) {
    return g_mock_sci[channel].tdr;
  }
  return 0;
}

void mock_sci_set_errors(uint8_t channel, bool orer, bool fer, bool per)
{
  if (channel < k_mock_sci_channel_count) {
    uint8_t errors = 0;
    if (orer) {
      errors |= k_mock_sci_ssr_orer;
    }
    if (fer) {
      errors |= k_mock_sci_ssr_fer;
    }
    if (per) {
      errors |= k_mock_sci_ssr_per;
    }
    g_mock_sci[channel].ssr |= errors;
  }
}

void mock_sci_clear_errors(uint8_t channel)
{
  if (channel < k_mock_sci_channel_count) {
    uint8_t error_mask = k_mock_sci_ssr_orer | k_mock_sci_ssr_fer | k_mock_sci_ssr_per;
    g_mock_sci[channel].ssr &= (uint8_t)~error_mask;
  }
}

bool mock_sci_is_tx_enabled(uint8_t channel)
{
  if (channel < k_mock_sci_channel_count) {
    return (g_mock_sci[channel].scr & k_mock_sci_scr_te) != 0;
  }
  return false;
}

bool mock_sci_is_rx_enabled(uint8_t channel)
{
  if (channel < k_mock_sci_channel_count) {
    return (g_mock_sci[channel].scr & k_mock_sci_scr_re) != 0;
  }
  return false;
}

uint8_t mock_sci_get_brr(uint8_t channel)
{
  if (channel < k_mock_sci_channel_count) {
    return g_mock_sci[channel].brr;
  }
  return 0;
}

/* =============================================================================
 * sci_get_channel Mock Implementation
 * =============================================================================
 */

/**
 * @brief Mock implementation of sci_get_channel for testing
 *
 * Returns pointer to mock SCI registers instead of real hardware.
 */
volatile rx_sci_regs_t* sci_get_channel(uint8_t channel)
{
  if (channel >= k_mock_sci_channel_count) {
    return (volatile rx_sci_regs_t*)0;
  }
  return (volatile rx_sci_regs_t*)&g_mock_sci[channel];
}
