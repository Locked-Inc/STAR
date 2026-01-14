/* tests/mocks/mock_rx_motor.c */

/**
 * @file mock_rx_motor.c
 * @brief Mock Motor Control Implementation
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_motor.h"

#include <string.h>

/* =============================================================================
 * Mock State
 * =============================================================================
 */

typedef enum {
  k_max_stop_calls = 100, /**< Maximum stop calls to track */
} mock_motor_constants_t;

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
  if (motor_handle == NULL) {
    return s_stop_call_count > 0;
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
  if (motor_handle == NULL) {
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
  memset(s_stop_calls, 0, sizeof(s_stop_calls));
}

/* =============================================================================
 * Motor API Implementation (Mocked)
 * =============================================================================
 */

rx_err_t rx_motor_stop(rx_motor_handle_t* motor, bool immediate)
{
  if (motor == NULL) {
    return k_rx_err_null_pointer;
  }

  /* Record the stop call */
  if (s_stop_call_count < k_max_stop_calls) {
    s_stop_calls[s_stop_call_count].motor_handle = motor;
    s_stop_calls[s_stop_call_count].immediate    = immediate;
    s_stop_call_count++;
  }

  return s_stop_return_value;
}
