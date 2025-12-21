/* src/motor_shared_state.c */

/**
 * @file motor_shared_state.c
 * @brief Shared state implementation for motor control system
 * @details
 * Implements thread-safe access to shared state between motor_control_task
 * and command_handler_task using ThreadX mutexes.
 *
 * Port of ESP32 motor_shared_state using ThreadX RTOS primitives.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "motor_shared_state.h"

#include "rx_check.h"
#include "rx_log.h"

#include <string.h>

static const char* s_tag = "MOTOR_STATE";

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t motor_shared_state_init(motor_shared_state_t* state)
{
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is NULL");

  /* Zero out structure */
  memset(state, 0, sizeof(motor_shared_state_t));

  /* Initialize all motors to IDLE state */
  for (int i = 0; i < k_motor_count; i++) {
    state->state[i] = k_motor_state_idle;
  }

  /* Create mutex for thread safety */
  UINT status = tx_mutex_create(&state->mutex, "motor_state_mutex", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_STATE;
  }

  RX_LOG_INFO(s_tag, "Motor shared state initialized");

  return RX_OK;
}

rx_err_t motor_shared_state_set_velocity(motor_shared_state_t* state,
                                          uint8_t               motor_idx,
                                          float                 velocity_mps)
{
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is NULL");

  if (motor_idx >= k_motor_count) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  /* Take mutex with write timeout */
  UINT status = tx_mutex_get(&state->mutex, k_mutex_timeout_write_ticks);
  if (status != TX_SUCCESS) {
    RX_LOG_WARN(s_tag, "Warning");
    return RX_ERR_TIMEOUT;
  }

  /* Update setpoint */
  state->velocity_setpoint_mps[motor_idx] = velocity_mps;

  /* Release mutex */
  tx_mutex_put(&state->mutex);

  return RX_OK;
}

rx_err_t motor_shared_state_get_velocity(motor_shared_state_t* state,
                                          uint8_t               motor_idx,
                                          float*                out_velocity_mps)
{
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is NULL");
  RX_CHECK_NULL_PTR(out_velocity_mps, s_tag, "out_velocity_mps pointer is NULL");

  if (motor_idx >= k_motor_count) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  /* Take mutex with short read timeout (control loop) */
  UINT status = tx_mutex_get(&state->mutex, k_mutex_timeout_read_ticks);
  if (status != TX_SUCCESS) {
    RX_LOG_WARN(s_tag, "Warning");
    return RX_ERR_TIMEOUT;
  }

  /* Read setpoint */
  *out_velocity_mps = state->velocity_setpoint_mps[motor_idx];

  /* Release mutex */
  tx_mutex_put(&state->mutex);

  return RX_OK;
}

rx_err_t motor_shared_state_set_status(motor_shared_state_t* state,
                                        uint8_t               motor_idx,
                                        float                 velocity_mps,
                                        float                 duty_cycle,
                                        float                 current_ma,
                                        rx_motor_state_t      motor_state)
{
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is NULL");

  if (motor_idx >= k_motor_count) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  /* Take mutex with read timeout (called from control loop) */
  UINT status = tx_mutex_get(&state->mutex, k_mutex_timeout_read_ticks);
  if (status != TX_SUCCESS) {
    RX_LOG_WARN(s_tag, "Warning");
    return RX_ERR_TIMEOUT;
  }

  /* Update status */
  state->measured_velocity_mps[motor_idx] = velocity_mps;
  state->duty_cycle_percent[motor_idx]    = duty_cycle;
  state->current_ma[motor_idx]            = current_ma;
  state->state[motor_idx]                 = motor_state;

  /* Release mutex */
  tx_mutex_put(&state->mutex);

  return RX_OK;
}

rx_err_t motor_shared_state_get_status(motor_shared_state_t* state,
                                        uint8_t               motor_idx,
                                        float*                out_velocity_mps,
                                        float*                out_duty_cycle,
                                        float*                out_current_ma,
                                        rx_motor_state_t*     out_motor_state)
{
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is NULL");
  RX_CHECK_NULL_PTR(out_velocity_mps, s_tag, "out_velocity_mps pointer is NULL");
  RX_CHECK_NULL_PTR(out_duty_cycle, s_tag, "out_duty_cycle pointer is NULL");
  RX_CHECK_NULL_PTR(out_current_ma, s_tag, "out_current_ma pointer is NULL");
  RX_CHECK_NULL_PTR(out_motor_state, s_tag, "out_motor_state pointer is NULL");

  if (motor_idx >= k_motor_count) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  /* Take mutex with write timeout */
  UINT status = tx_mutex_get(&state->mutex, k_mutex_timeout_write_ticks);
  if (status != TX_SUCCESS) {
    RX_LOG_WARN(s_tag, "Warning");
    return RX_ERR_TIMEOUT;
  }

  /* Read status */
  *out_velocity_mps = state->measured_velocity_mps[motor_idx];
  *out_duty_cycle   = state->duty_cycle_percent[motor_idx];
  *out_current_ma   = state->current_ma[motor_idx];
  *out_motor_state  = state->state[motor_idx];

  /* Release mutex */
  tx_mutex_put(&state->mutex);

  return RX_OK;
}

rx_err_t motor_shared_state_set_estop(motor_shared_state_t* state, bool active)
{
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is NULL");

  /* Take mutex with write timeout */
  UINT status = tx_mutex_get(&state->mutex, k_mutex_timeout_write_ticks);
  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Timeout setting emergency stop");
    return RX_ERR_TIMEOUT;
  }

  /* Update emergency stop flag */
  state->estop_active = active;

  /* Release mutex */
  tx_mutex_put(&state->mutex);

  RX_LOG_INFO(s_tag, "Info");

  return RX_OK;
}

rx_err_t motor_shared_state_get_estop(motor_shared_state_t* state, bool* out_active)
{
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is NULL");
  RX_CHECK_NULL_PTR(out_active, s_tag, "out_active pointer is NULL");

  /* Take mutex with read timeout (called from control loop) */
  UINT status = tx_mutex_get(&state->mutex, k_mutex_timeout_read_ticks);
  if (status != TX_SUCCESS) {
    RX_LOG_WARN(s_tag, "Timeout getting emergency stop status, assuming safe (active)");
    *out_active = true; /* Fail-safe: assume emergency stop if mutex timeout */
    return RX_ERR_TIMEOUT;
  }

  /* Read emergency stop flag */
  *out_active = state->estop_active;

  /* Release mutex */
  tx_mutex_put(&state->mutex);

  return RX_OK;
}
