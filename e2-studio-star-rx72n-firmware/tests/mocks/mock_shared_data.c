/* tests/mocks/mock_shared_data.c */

/**
 * @file mock_shared_data.c
 * @brief Mock Shared Data Module Implementation
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "mock_shared_data.h"

#include <string.h>

/* =============================================================================
 * Static Return Values
 * =============================================================================
 */

static rx_err_t s_init_return = k_rx_ok;

/* =============================================================================
 * Static Call Counts
 * =============================================================================
 */

static uint32_t s_init_count               = 0;
static uint32_t s_trigger_estop_count      = 0;
static uint32_t s_motor_state_update_count = 0;
static uint32_t s_set_event_count          = 0;

/* =============================================================================
 * Static State
 * =============================================================================
 */

static bool           s_initialized  = false;
static bool           s_estop_active = false;
static estop_reason_t s_estop_reason = k_estop_reason_none;
static bool           s_comm_timeout = false;

static motor_command_t s_motor_command      = {0};
static motor_state_t   s_motor_state        = {0};
static pid_gains_t     s_pid_gains          = {0};
static bool            s_pid_update_pending = false;

static bms_state_t         s_bms_state      = {0};
static temp_sensor_state_t s_temp_state     = {0};
static obstacle_state_t    s_obstacle_state = {0};

static estop_reason_t    s_last_triggered_reason = k_estop_reason_none;
static shared_event_flags_t s_last_event_flags   = (shared_event_flags_t)0;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_shared_data_reset(void)
{
  s_init_return              = k_rx_ok;
  s_init_count               = 0;
  s_trigger_estop_count      = 0;
  s_motor_state_update_count = 0;

  s_initialized  = false;
  s_estop_active = false;
  s_estop_reason = k_estop_reason_none;
  s_comm_timeout = false;

  (void)memset(&s_motor_command, 0, sizeof(s_motor_command));
  (void)memset(&s_motor_state, 0, sizeof(s_motor_state));
  (void)memset(&s_pid_gains, 0, sizeof(s_pid_gains));
  s_pid_update_pending = false;

  /* Set default PID gains (MATLAB-tuned) */
  s_pid_gains.kp           = 0.286f;
  s_pid_gains.ki           = 8.01f;
  s_pid_gains.kd           = 0.0f;
  s_pid_gains.output_min   = -100.0f;
  s_pid_gains.output_max   = 100.0f;
  s_pid_gains.integral_min = -50.0f;
  s_pid_gains.integral_max = 50.0f;

  (void)memset(&s_bms_state, 0, sizeof(s_bms_state));
  (void)memset(&s_temp_state, 0, sizeof(s_temp_state));
  (void)memset(&s_obstacle_state, 0, sizeof(s_obstacle_state));

  s_last_triggered_reason = k_estop_reason_none;
  s_set_event_count       = 0;
  s_last_event_flags      = (shared_event_flags_t)0;
}

void mock_shared_data_set_init_return(rx_err_t err)
{
  s_init_return = err;
}

void mock_shared_data_set_estop_active(bool active, estop_reason_t reason)
{
  s_estop_active = active;
  s_estop_reason = reason;
}

void mock_shared_data_set_comm_timeout(bool timeout)
{
  s_comm_timeout = timeout;
}

void mock_shared_data_set_motor_command(const motor_command_t* cmd)
{
  if (cmd != nullptr) {
    (void)memcpy(&s_motor_command, cmd, sizeof(s_motor_command));
  }
}

void mock_shared_data_set_pid_gains(const pid_gains_t* gains)
{
  if (gains != nullptr) {
    (void)memcpy(&s_pid_gains, gains, sizeof(s_pid_gains));
  }
}

void mock_shared_data_set_pid_update_pending(bool pending)
{
  s_pid_update_pending = pending;
}

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

uint32_t mock_shared_data_get_init_count(void)
{
  return s_init_count;
}

uint32_t mock_shared_data_get_trigger_estop_count(void)
{
  return s_trigger_estop_count;
}

estop_reason_t mock_shared_data_get_last_estop_reason(void)
{
  return s_last_triggered_reason;
}

uint32_t mock_shared_data_get_set_event_count(void)
{
  return s_set_event_count;
}

shared_event_flags_t mock_shared_data_get_last_event_flags(void)
{
  return s_last_event_flags;
}

rx_err_t mock_shared_data_get_last_motor_state(motor_state_t* out_state)
{
  if (out_state == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(out_state, &s_motor_state, sizeof(*out_state));
  return k_rx_ok;
}

uint32_t mock_shared_data_get_motor_state_update_count(void)
{
  return s_motor_state_update_count;
}

bool mock_shared_data_was_initialized(void)
{
  return s_init_count > 0;
}

/* =============================================================================
 * Mock Shared Data API
 * =============================================================================
 */

rx_err_t shared_data_init(void)
{
  s_init_count++;

  if (s_init_return == k_rx_ok) {
    s_initialized = true;
  }

  return s_init_return;
}

/* Motor Command */
rx_err_t shared_data_set_motor_command(const motor_command_t* cmd)
{
  if (cmd == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(&s_motor_command, cmd, sizeof(s_motor_command));
  return k_rx_ok;
}

rx_err_t shared_data_get_motor_command(motor_command_t* out_cmd)
{
  if (out_cmd == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(out_cmd, &s_motor_command, sizeof(*out_cmd));
  return k_rx_ok;
}

/* Motor State */
rx_err_t shared_data_update_motor_state(const motor_state_t* state)
{
  if (state == nullptr) {
    return k_rx_err_null_ptr;
  }

  s_motor_state_update_count++;
  (void)memcpy(&s_motor_state, state, sizeof(s_motor_state));
  return k_rx_ok;
}

rx_err_t shared_data_get_motor_state(motor_state_t* out_state)
{
  if (out_state == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(out_state, &s_motor_state, sizeof(*out_state));
  return k_rx_ok;
}

/* PID Gains */
rx_err_t shared_data_set_pid_gains(const pid_gains_t* gains)
{
  if (gains == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(&s_pid_gains, gains, sizeof(s_pid_gains));
  s_pid_update_pending = true;
  return k_rx_ok;
}

rx_err_t shared_data_get_pid_gains(pid_gains_t* out_gains)
{
  if (out_gains == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(out_gains, &s_pid_gains, sizeof(*out_gains));
  return k_rx_ok;
}

bool shared_data_pid_update_pending(void)
{
  return s_pid_update_pending;
}

void shared_data_clear_pid_update_flag(void)
{
  s_pid_update_pending = false;
}

/* Emergency Stop */
rx_err_t shared_data_trigger_estop(estop_reason_t reason)
{
  s_trigger_estop_count++;
  s_last_triggered_reason = reason;
  s_estop_active          = true;
  s_estop_reason          = reason;
  return k_rx_ok;
}

rx_err_t shared_data_clear_estop(void)
{
  s_estop_active = false;
  s_estop_reason = k_estop_reason_none;
  return k_rx_ok;
}

bool shared_data_is_estop_active(void)
{
  return s_estop_active;
}

estop_reason_t shared_data_get_estop_reason(void)
{
  return s_estop_reason;
}

/* BMS State */
rx_err_t shared_data_update_bms(const bms_state_t* state)
{
  if (state == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(&s_bms_state, state, sizeof(s_bms_state));
  return k_rx_ok;
}

rx_err_t shared_data_get_bms(bms_state_t* out_state)
{
  if (out_state == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(out_state, &s_bms_state, sizeof(*out_state));
  return k_rx_ok;
}

/* Temperature State */
rx_err_t shared_data_update_temp(const temp_sensor_state_t* state)
{
  if (state == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(&s_temp_state, state, sizeof(s_temp_state));
  return k_rx_ok;
}

rx_err_t shared_data_get_temp(temp_sensor_state_t* out_state)
{
  if (out_state == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(out_state, &s_temp_state, sizeof(*out_state));
  return k_rx_ok;
}

/* Obstacle State */
rx_err_t shared_data_update_obstacle(const obstacle_state_t* state)
{
  if (state == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(&s_obstacle_state, state, sizeof(s_obstacle_state));
  return k_rx_ok;
}

rx_err_t shared_data_get_obstacle(obstacle_state_t* out_state)
{
  if (out_state == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memcpy(out_state, &s_obstacle_state, sizeof(*out_state));
  return k_rx_ok;
}

/* Communication Timeout */
bool shared_data_is_comm_timeout(void)
{
  return s_comm_timeout;
}

void shared_data_update_last_comm_tick(void)
{
  /* In mock, this just clears the timeout flag */
  s_comm_timeout = false;
}

/* Event Flags */
rx_err_t shared_data_set_event(shared_event_flags_t flags)
{
  s_set_event_count++;
  s_last_event_flags = flags;
  return k_rx_ok;
}
