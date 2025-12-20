/**
 * @file motor_shared_state.c
 * @brief Shared state implementation for motor control system
 * @details
 * Implements thread-safe access to shared state between motor_control_task
 * and command_handler_task using FreeRTOS mutexes.
 *
 * @date 2025-12-20
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "motor_shared_state.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"

/* --- Constants --- */

/**
 * @brief Logging tag for ESP-IDF logging macros
 */
static const char* s_tag = "MOTOR_SHARED_STATE";

/* --- Public Functions --- */

esp_err_t motor_shared_state_init(motor_shared_state_t* state)
{
  ESP_RETURN_ON_FALSE(state, ESP_ERR_INVALID_ARG, s_tag, "State is NULL");

  /* Zero out structure */
  memset(state, 0, sizeof(motor_shared_state_t));

  /* Initialize all motors to IDLE state */
  for (int i = 0; i < k_motor_count; i++) {
    state->state[i] = star_v1_MotorState_MOTOR_STATE_IDLE;
  }

  /* Create mutex for thread safety */
  state->mutex = xSemaphoreCreateMutex();
  if (!state->mutex) {
    ESP_LOGE(s_tag, "Failed to create mutex");
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(s_tag, "Motor shared state initialized");
  return ESP_OK;
}

esp_err_t
motor_shared_state_set_velocity(motor_shared_state_t* state, uint8_t motor_idx, float velocity_mps)
{
  ESP_RETURN_ON_FALSE(state, ESP_ERR_INVALID_ARG, s_tag, "State is NULL");
  ESP_RETURN_ON_FALSE(motor_idx < k_motor_count,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "motor_idx must be < %d",
                      k_motor_count);

  /* Take mutex with write timeout */
  if (xSemaphoreTake(state->mutex, pdMS_TO_TICKS(k_mutex_timeout_write_ms)) != pdTRUE) {
    ESP_LOGW(s_tag, "Timeout setting velocity for motor %d", motor_idx);
    return ESP_ERR_TIMEOUT;
  }

  /* Update setpoint */
  state->velocity_setpoint_mps[motor_idx] = velocity_mps;

  /* Release mutex */
  xSemaphoreGive(state->mutex);

  return ESP_OK;
}

esp_err_t motor_shared_state_get_velocity(motor_shared_state_t* state,
                                          uint8_t               motor_idx,
                                          float*                out_velocity_mps)
{
  ESP_RETURN_ON_FALSE(state && out_velocity_mps,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "State or out_velocity_mps is NULL");
  ESP_RETURN_ON_FALSE(motor_idx < k_motor_count,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "motor_idx must be < %d",
                      k_motor_count);

  /* Take mutex with short read timeout (control loop) */
  if (xSemaphoreTake(state->mutex, pdMS_TO_TICKS(k_mutex_timeout_read_ms)) != pdTRUE) {
    ESP_LOGW(s_tag, "Timeout getting velocity for motor %d, using cached value", motor_idx);
    return ESP_ERR_TIMEOUT;
  }

  /* Read setpoint */
  *out_velocity_mps = state->velocity_setpoint_mps[motor_idx];

  /* Release mutex */
  xSemaphoreGive(state->mutex);

  return ESP_OK;
}

esp_err_t motor_shared_state_set_status(motor_shared_state_t* state,
                                        uint8_t               motor_idx,
                                        float                 velocity_mps,
                                        float                 duty_cycle,
                                        float                 current_ma,
                                        star_v1_MotorState    motor_state)
{
  ESP_RETURN_ON_FALSE(state, ESP_ERR_INVALID_ARG, s_tag, "State is NULL");
  ESP_RETURN_ON_FALSE(motor_idx < k_motor_count,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "motor_idx must be < %d",
                      k_motor_count);

  /* Take mutex with read timeout (called from control loop) */
  if (xSemaphoreTake(state->mutex, pdMS_TO_TICKS(k_mutex_timeout_read_ms)) != pdTRUE) {
    ESP_LOGW(s_tag, "Timeout setting status for motor %d", motor_idx);
    return ESP_ERR_TIMEOUT;
  }

  /* Update status */
  state->measured_velocity_mps[motor_idx] = velocity_mps;
  state->duty_cycle_percent[motor_idx]    = duty_cycle;
  state->current_ma[motor_idx]            = current_ma;
  state->state[motor_idx]                 = motor_state;

  /* Release mutex */
  xSemaphoreGive(state->mutex);

  return ESP_OK;
}

esp_err_t motor_shared_state_get_status(motor_shared_state_t* state,
                                        uint8_t               motor_idx,
                                        float*                out_velocity_mps,
                                        float*                out_duty_cycle,
                                        float*                out_current_ma,
                                        star_v1_MotorState*   out_motor_state)
{
  ESP_RETURN_ON_FALSE(state && out_velocity_mps && out_duty_cycle && out_current_ma &&
                        out_motor_state,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "State or output pointers are NULL");
  ESP_RETURN_ON_FALSE(motor_idx < k_motor_count,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "motor_idx must be < %d",
                      k_motor_count);

  /* Take mutex with write timeout */
  if (xSemaphoreTake(state->mutex, pdMS_TO_TICKS(k_mutex_timeout_write_ms)) != pdTRUE) {
    ESP_LOGW(s_tag, "Timeout getting status for motor %d", motor_idx);
    return ESP_ERR_TIMEOUT;
  }

  /* Read status */
  *out_velocity_mps = state->measured_velocity_mps[motor_idx];
  *out_duty_cycle   = state->duty_cycle_percent[motor_idx];
  *out_current_ma   = state->current_ma[motor_idx];
  *out_motor_state  = state->state[motor_idx];

  /* Release mutex */
  xSemaphoreGive(state->mutex);

  return ESP_OK;
}

esp_err_t motor_shared_state_set_estop(motor_shared_state_t* state, bool active)
{
  ESP_RETURN_ON_FALSE(state, ESP_ERR_INVALID_ARG, s_tag, "State is NULL");

  /* Take mutex with write timeout */
  if (xSemaphoreTake(state->mutex, pdMS_TO_TICKS(k_mutex_timeout_write_ms)) != pdTRUE) {
    ESP_LOGE(s_tag, "Timeout setting emergency stop");
    return ESP_ERR_TIMEOUT;
  }

  /* Update emergency stop flag */
  state->estop_active = active;

  /* Release mutex */
  xSemaphoreGive(state->mutex);

  ESP_LOGI(s_tag, "Emergency stop %s", active ? "ACTIVATED" : "CLEARED");
  return ESP_OK;
}

esp_err_t motor_shared_state_get_estop(motor_shared_state_t* state, bool* out_active)
{
  ESP_RETURN_ON_FALSE(state && out_active,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "State or out_active is NULL");

  /* Take mutex with read timeout (called from control loop) */
  if (xSemaphoreTake(state->mutex, pdMS_TO_TICKS(k_mutex_timeout_read_ms)) != pdTRUE) {
    ESP_LOGW(s_tag, "Timeout getting emergency stop status, assuming safe (active)");
    *out_active = true; /* Fail-safe: assume emergency stop if mutex timeout */
    return ESP_ERR_TIMEOUT;
  }

  /* Read emergency stop flag */
  *out_active = state->estop_active;

  /* Release mutex */
  xSemaphoreGive(state->mutex);

  return ESP_OK;
}
