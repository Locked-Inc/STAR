/**
 * @file mock_rx_dmaca.c
 * @brief Mock DMAC Driver Implementation for Unit Testing
 *
 * @details
 * Provides mock implementations of rx_dmaca_init(), rx_dmaca_transfer_poll(),
 * rx_dmaca_abort(), and rx_dmaca_deinit() for host-side unit testing of the
 * rx_crc hw_dma backend without RX72N hardware.
 *
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_dmaca.h"

#include <stddef.h>
#include <string.h>

/* =============================================================================
 * Mock State
 * =============================================================================
 */

/** @brief Number of rx_dmaca_init() calls since last reset */
static uint32_t s_init_count;

/** @brief Number of rx_dmaca_deinit() calls since last reset */
static uint32_t s_deinit_count;

/** @brief Number of rx_dmaca_transfer_poll() calls since last reset */
static uint32_t s_transfer_count;

/** @brief Preset return value for rx_dmaca_transfer_poll() */
static rx_err_t s_transfer_result;

/** @brief Copy of the last config passed to rx_dmaca_transfer_poll() */
static rx_dmaca_config_t s_last_config;

/** @brief Whether transfer_poll has been called at least once */
static bool s_transfer_called;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_rx_dmaca_reset(void)
{
  s_init_count      = 0;
  s_deinit_count    = 0;
  s_transfer_count  = 0;
  s_transfer_result = k_rx_ok;
  memset(&s_last_config, 0, sizeof(s_last_config));
  s_transfer_called = false;
}

void mock_rx_dmaca_set_transfer_result(rx_err_t result)
{
  s_transfer_result = result;
}

uint32_t mock_rx_dmaca_get_init_count(void)
{
  return s_init_count;
}

uint32_t mock_rx_dmaca_get_deinit_count(void)
{
  return s_deinit_count;
}

uint32_t mock_rx_dmaca_get_transfer_count(void)
{
  return s_transfer_count;
}

const rx_dmaca_config_t* mock_rx_dmaca_get_last_config(void)
{
  if (!s_transfer_called) {
    return nullptr;
  }
  return &s_last_config;
}

/* =============================================================================
 * Mock Driver Functions (replace real rx_dmaca_* in test builds)
 * =============================================================================
 */

rx_err_t rx_dmaca_init(void)
{
  s_init_count++;
  return k_rx_ok;
}

rx_err_t rx_dmaca_deinit(void)
{
  s_deinit_count++;
  return k_rx_ok;
}

rx_err_t rx_dmaca_transfer_poll(const rx_dmaca_config_t* config)
{
  s_transfer_count++;
  s_transfer_called = true;
  if (config != nullptr) {
    s_last_config = *config;
  }
  return s_transfer_result;
}

rx_err_t rx_dmaca_abort(uint8_t channel)
{
  (void)channel;
  return k_rx_ok;
}
