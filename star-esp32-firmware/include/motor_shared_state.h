/**
 * @file motor_shared_state.h
 * @brief Shared state structure for motor control system
 * @details
 * Provides a thread-safe shared state structure for communication between
 * motor_control_task (250Hz PID loop) and command_handler_task (ARQ message processing).
 * All access to shared data is protected by a mutex.
 *
 * The shared state contains:
 * - Velocity setpoints (written by command_handler, read by motor_control)
 * - Motor status (written by motor_control, read by command_handler)
 * - Emergency stop flag (written by command_handler, read by motor_control)
 *
 * @date 2025-12-20
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef MOTOR_SHARED_STATE_H
#define MOTOR_SHARED_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "star/v1/motor_control.pb.h"

/* --- Configuration Constants --- */

/**
 * @brief Number of motors in the system
 */
typedef enum {
  k_motor_count = 4, /**< 4 motors (skid-steer 4WD) */
} motor_count_t;

/**
 * @brief Mutex timeout configuration
 */
typedef enum {
  k_mutex_timeout_write_ms = 1000, /**< Timeout for command handler writes (ms) */
  k_mutex_timeout_read_ms  = 10,   /**< Timeout for motor control reads (ms) */
} mutex_timeout_t;

/* --- Type Definitions --- */

/**
 * @brief Motor shared state structure
 *
 * Contains all data shared between motor_control_task and command_handler_task.
 * Access to all fields must be protected by the mutex.
 */
typedef struct {
  /* Command setpoints (written by command_handler_task, read by motor_control_task) */
  float velocity_setpoint_mps[k_motor_count]; /**< Target velocities (m/s) */

  /* Motor status (written by motor_control_task, read by command_handler_task) */
  float              measured_velocity_mps[k_motor_count]; /**< Measured velocities (m/s) */
  float              duty_cycle_percent[k_motor_count];    /**< Motor duty cycles (%) */
  float              current_ma[k_motor_count];            /**< Motor currents (mA) */
  star_v1_MotorState state[k_motor_count];                 /**< Motor states */

  /* Emergency stop (written by command_handler_task, read by motor_control_task) */
  bool estop_active; /**< Emergency stop flag */

  /* Thread safety */
  SemaphoreHandle_t mutex; /**< Mutex for protecting shared data */
} motor_shared_state_t;

/* --- Public Function Prototypes --- */

/**
 * @brief Initialize the motor shared state
 *
 * Initializes all fields to safe defaults and creates the mutex for thread safety.
 * Must be called before creating any tasks that access the shared state.
 *
 * @param[out] state Pointer to shared state structure (must not be NULL)
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid state pointer
 *   - ESP_ERR_NO_MEM: Failed to create mutex
 */
esp_err_t motor_shared_state_init(motor_shared_state_t* state);

/**
 * @brief Set velocity setpoint for a specific motor
 *
 * Thread-safe write of velocity setpoint. Typically called by command_handler_task.
 * Uses long timeout (1000ms) suitable for non-time-critical writes.
 *
 * @param[in] state       Pointer to shared state structure (must not be NULL)
 * @param[in] motor_idx   Motor index (0-3)
 * @param[in] velocity_mps Target velocity in meters per second
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid state or motor index
 *   - ESP_ERR_TIMEOUT: Mutex timeout
 */
esp_err_t
motor_shared_state_set_velocity(motor_shared_state_t* state, uint8_t motor_idx, float velocity_mps);

/**
 * @brief Get velocity setpoint for a specific motor
 *
 * Thread-safe read of velocity setpoint. Typically called by motor_control_task.
 * Uses short timeout (10ms) to avoid blocking the time-critical control loop.
 *
 * @param[in]  state          Pointer to shared state structure (must not be NULL)
 * @param[in]  motor_idx      Motor index (0-3)
 * @param[out] out_velocity_mps Pointer to store velocity (must not be NULL)
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid state, motor index, or output pointer
 *   - ESP_ERR_TIMEOUT: Mutex timeout (control loop should use cached value)
 */
esp_err_t motor_shared_state_get_velocity(motor_shared_state_t* state,
                                          uint8_t               motor_idx,
                                          float*                out_velocity_mps);

/**
 * @brief Set motor status for a specific motor
 *
 * Thread-safe write of motor status (velocity, duty cycle, current, state).
 * Typically called by motor_control_task.
 *
 * @param[in] state           Pointer to shared state structure (must not be NULL)
 * @param[in] motor_idx       Motor index (0-3)
 * @param[in] velocity_mps    Measured velocity (m/s)
 * @param[in] duty_cycle      Duty cycle percentage (%)
 * @param[in] current_ma      Current draw (mA)
 * @param[in] motor_state     Motor state
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid state or motor index
 *   - ESP_ERR_TIMEOUT: Mutex timeout
 */
esp_err_t motor_shared_state_set_status(motor_shared_state_t* state,
                                        uint8_t               motor_idx,
                                        float                 velocity_mps,
                                        float                 duty_cycle,
                                        float                 current_ma,
                                        star_v1_MotorState    motor_state);

/**
 * @brief Get motor status for a specific motor
 *
 * Thread-safe read of motor status. Typically called by command_handler_task.
 *
 * @param[in]  state           Pointer to shared state structure (must not be NULL)
 * @param[in]  motor_idx       Motor index (0-3)
 * @param[out] out_velocity_mps Pointer to store measured velocity (must not be NULL)
 * @param[out] out_duty_cycle  Pointer to store duty cycle (must not be NULL)
 * @param[out] out_current_ma  Pointer to store current (must not be NULL)
 * @param[out] out_motor_state Pointer to store motor state (must not be NULL)
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid state, motor index, or output pointers
 *   - ESP_ERR_TIMEOUT: Mutex timeout
 */
esp_err_t motor_shared_state_get_status(motor_shared_state_t* state,
                                        uint8_t               motor_idx,
                                        float*                out_velocity_mps,
                                        float*                out_duty_cycle,
                                        float*                out_current_ma,
                                        star_v1_MotorState*   out_motor_state);

/**
 * @brief Set emergency stop flag
 *
 * Thread-safe write of emergency stop flag. Typically called by command_handler_task
 * in response to EmergencyStopRequest.
 *
 * @param[in] state  Pointer to shared state structure (must not be NULL)
 * @param[in] active true to activate emergency stop, false to clear
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid state pointer
 *   - ESP_ERR_TIMEOUT: Mutex timeout
 */
esp_err_t motor_shared_state_set_estop(motor_shared_state_t* state, bool active);

/**
 * @brief Get emergency stop flag
 *
 * Thread-safe read of emergency stop flag. Typically called by motor_control_task
 * to check if motors should be disabled.
 *
 * @param[in]  state      Pointer to shared state structure (must not be NULL)
 * @param[out] out_active Pointer to store emergency stop status (must not be NULL)
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid state or output pointer
 *   - ESP_ERR_TIMEOUT: Mutex timeout
 */
esp_err_t motor_shared_state_get_estop(motor_shared_state_t* state, bool* out_active);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_SHARED_STATE_H */
