/**
 * @file mock_rx_eccram.c
 * @brief Mock implementation of the rx_eccram public API for unit tests
 *
 * @details
 * Provides stub definitions for every rx_eccram_* function declared in
 * rx_eccram.h. Each stub records the call, captures its arguments, and
 * returns a programmable rx_err_t value so tests can inject success or
 * specific error codes.
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "mock_rx_eccram.h"

#include <string.h>

/* =============================================================================
 * Defaults
 * =============================================================================
 */

/**
 * @enum mock_rx_eccram_defaults_t
 * @brief Region boundary defaults used by mock_rx_eccram_reset()
 *
 * @details
 * Mirror the hardware values from the RX72N HW manual Chapter 60 (page 2977)
 * so tests observe realistic defaults without needing to configure the mock.
 *
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /** @brief Default region_start_return value (RX72N ECCRAM base) */
  k_mock_eccram_region_base_default = 0x00FF8000,
  /** @brief Default region_end_return value (RX72N ECCRAM last byte) */
  k_mock_eccram_region_end_default = 0x00FFFFFF,
} mock_rx_eccram_defaults_t;

/* =============================================================================
 * State
 * =============================================================================
 */

/** @brief Singleton mock state accessed via mock_rx_eccram_state() */
static mock_rx_eccram_state_t s_state;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_rx_eccram_reset(void)
{
  memset(&s_state, 0, sizeof(s_state));
  s_state.init_return         = k_rx_ok;
  s_state.get_status_return   = k_rx_ok;
  s_state.clear_errors_return = k_rx_ok;
  s_state.register_isr_return = k_rx_ok;
  s_state.region_start_return = (uintptr_t)k_mock_eccram_region_base_default;
  s_state.region_end_return   = (uintptr_t)k_mock_eccram_region_end_default;
}

mock_rx_eccram_state_t* mock_rx_eccram_state(void)
{
  return &s_state;
}

/* =============================================================================
 * rx_eccram Public API Stubs
 * =============================================================================
 */

rx_err_t rx_eccram_init(rx_eccram_mode_t mode)
{
  s_state.init_calls++;
  s_state.init_last_mode = mode;
  return s_state.init_return;
}

rx_err_t rx_eccram_get_error_status(rx_eccram_status_t* out)
{
  s_state.get_status_calls++;
  if (out != NULL) {
    *out = s_state.get_status_fill;
  }
  return s_state.get_status_return;
}

rx_err_t rx_eccram_clear_errors(void)
{
  s_state.clear_errors_calls++;
  return s_state.clear_errors_return;
}

rx_err_t rx_eccram_register_error_isr(rx_eccram_on_1bit_fn_t on_1bit,
                                      rx_eccram_on_2bit_fn_t on_2bit,
                                      void*                  ctx)
{
  s_state.register_isr_calls++;
  s_state.last_on_1bit = on_1bit;
  s_state.last_on_2bit = on_2bit;
  s_state.last_ctx     = ctx;
  return s_state.register_isr_return;
}

uintptr_t rx_eccram_region_start(void)
{
  s_state.region_start_calls++;
  return s_state.region_start_return;
}

uintptr_t rx_eccram_region_end(void)
{
  s_state.region_end_calls++;
  return s_state.region_end_return;
}
