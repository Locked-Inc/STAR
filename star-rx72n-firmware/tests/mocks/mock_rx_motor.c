/**
 * @file mock_rx_motor.c
 * @brief Mock Motor Control Implementation
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_motor.h"

/* =============================================================================
 * Mock State
 * =============================================================================
 */

typedef enum : uint8_t {
  k_max_stop_calls = 100U, /**< Maximum stop calls to track */
} mock_motor_limits_t;

typedef struct {
  const rx_motor_handle_t* motor_handle; /**< Motor handle that was stopped */
  bool                     immediate;    /**< Whether immediate stop was requested */
} stop_call_t;

static rx_err_t    s_stop_return_value = k_rx_ok;
static stop_call_t s_stop_calls[k_max_stop_calls];
static uint32_t    s_stop_call_count = 0;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_rx_motor_init(void)
{
  mock_rx_motor_reset();
}

void mock_rx_motor_deinit(void)
{
  mock_rx_motor_reset();
}

void mock_rx_motor_set_stop_return(rx_err_t ret_val)
{
  s_stop_return_value = ret_val;
}

bool mock_rx_motor_was_stop_called(const rx_motor_handle_t* motor_handle)
{
  if (motor_handle == nullptr) {
    return s_stop_call_count > 0; // NOLINT(readability-implicit-bool-conversion)
  }

  for (uint32_t i = 0; i < s_stop_call_count; i++) {
    if (s_stop_calls[i].motor_handle == motor_handle) {
      return true;
    }
  }
  return false;
}

uint32_t mock_rx_motor_get_stop_count(const rx_motor_handle_t* motor_handle)
{
  if (motor_handle == nullptr) {
    return s_stop_call_count;
  }

  uint32_t count = 0;
  for (uint32_t i = 0; i < s_stop_call_count; i++) {
    if (s_stop_calls[i].motor_handle == motor_handle) {
      count++;
    }
  }
  return count;
}

void mock_rx_motor_reset(void)
{
  s_stop_return_value = k_rx_ok;
  s_stop_call_count   = 0;
  for (uint8_t i = 0; i < k_max_stop_calls; i++) {
    s_stop_calls[i].motor_handle = nullptr;
    s_stop_calls[i].immediate    = false;
  }
}

/* =============================================================================
 * Motor API Implementation (Mocked)
 * =============================================================================
 */

rx_err_t rx_motor_stop(rx_motor_handle_t* handle, bool brake)
{
  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Record the stop call */
  if (s_stop_call_count < k_max_stop_calls) {
    s_stop_calls[s_stop_call_count].motor_handle = handle;
    s_stop_calls[s_stop_call_count].immediate    = brake;
    s_stop_call_count++;
  }

  return s_stop_return_value;
}
