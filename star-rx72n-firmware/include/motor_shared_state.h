/* include/motor_shared_state.h */

/**
 * @file motor_shared_state.h
 * @brief Shared state structure for motor control system
 * @details
 * Provides a thread-safe shared state structure for communication between
 * motor_control_task (250Hz PID loop) and command_handler_task (message processing).
 * All access to shared data is protected by a ThreadX mutex.
 *
 * The shared state contains:
 * - Velocity setpoints (written by command_handler, read by motor_control)
 * - Motor status (written by motor_control, read by command_handler)
 * - Emergency stop flag (written by command_handler, read by motor_control)
 *
 * Port of ESP32 motor_shared_state using ThreadX RTOS primitives.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef MOTOR_SHARED_STATE_H
#define MOTOR_SHARED_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "tx_api.h"

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief Number of motors in the system
 */
typedef enum {
  k_motor_count = 4, /**< 4 motors (skid-steer 4WD) */
} motor_count_t;

/**
 * @brief Motor indices for skid-steer 4WD configuration
 */
typedef enum {
  k_motor_idx_front_left  = 0, /**< Front-left motor */
  k_motor_idx_front_right = 1, /**< Front-right motor */
  k_motor_idx_rear_left   = 2, /**< Rear-left motor */
  k_motor_idx_rear_right  = 3, /**< Rear-right motor */
} motor_idx_t;

/**
 * @brief Mutex timeout configuration (in ThreadX ticks)
 *
 * ThreadX tick rate is configured in tx_user.h (typically 100 Hz = 10ms per tick).
 * For 100 Hz tick rate:
 * - 100 ticks = 1000ms
 * - 1 tick = 10ms
 */
typedef enum {
  k_mutex_timeout_write_ticks = 100, /**< Timeout for command handler writes (1000ms @ 100Hz) */
  k_mutex_timeout_read_ticks  = 1,   /**< Timeout for motor control reads (10ms @ 100Hz) */
} mutex_timeout_t;

/**
 * @brief Motor state enumeration
 *
 * Note: This will eventually be replaced with Protocol Buffer generated enum
 * once nanopb code generation is integrated into RX72N build system.
 */
typedef enum {
  k_motor_state_unknown = 0, /**< Unknown state */
  k_motor_state_idle    = 1, /**< Motor stopped */
  k_motor_state_running = 2, /**< Motor running */
  k_motor_state_fault   = 3, /**< Motor fault detected */
} rx_motor_state_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

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
  float            measured_velocity_mps[k_motor_count]; /**< Measured velocities (m/s) */
  float            duty_cycle_percent[k_motor_count];    /**< Motor duty cycles (%) */
  float            current_ma[k_motor_count];            /**< Motor currents (mA) */
  rx_motor_state_t state[k_motor_count];                 /**< Motor states */

  /* Emergency stop (written by command_handler_task, read by motor_control_task) */
  bool estop_active; /**< Emergency stop flag */

  /* Thread safety */
  TX_MUTEX mutex; /**< ThreadX mutex for protecting shared data */
} motor_shared_state_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize the motor shared state
 *
 * @param[out] state Pointer to shared state structure. Must not be NULL.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if state is NULL
 * @return RX_ERR_INVALID_STATE if mutex creation fails
 */
rx_err_t motor_shared_state_init(motor_shared_state_t* state);

/**
 * @brief Set velocity setpoint for a specific motor
 *
 * @param[in] state        Pointer to shared state structure. Must not be NULL.
 * @param[in] motor_idx    Motor index (0-3)
 * @param[in] velocity_mps Target velocity in meters per second
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if state is NULL
 * @return RX_ERR_INVALID_ARG if motor_idx is invalid
 * @return RX_ERR_TIMEOUT if mutex timeout
 */
rx_err_t
motor_shared_state_set_velocity(motor_shared_state_t* state, uint8_t motor_idx, float velocity_mps);

/**
 * @brief Get velocity setpoint for a specific motor
 *
 * @param[in]  state           Pointer to shared state structure. Must not be NULL.
 * @param[in]  motor_idx       Motor index (0-3)
 * @param[out] out_velocity_mps Pointer to store velocity. Must not be NULL.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if state or out_velocity_mps is NULL
 * @return RX_ERR_INVALID_ARG if motor_idx is invalid
 * @return RX_ERR_TIMEOUT if mutex timeout
 */
rx_err_t motor_shared_state_get_velocity(motor_shared_state_t* state,
                                         uint8_t               motor_idx,
                                         float*                out_velocity_mps);

/**
 * @brief Set motor status for a specific motor
 *
 * @param[in] state        Pointer to shared state structure. Must not be NULL.
 * @param[in] motor_idx    Motor index (0-3)
 * @param[in] velocity_mps Measured velocity (m/s)
 * @param[in] duty_cycle   Duty cycle percentage (%)
 * @param[in] current_ma   Current draw (mA)
 * @param[in] motor_state  Motor state
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if state is NULL
 * @return RX_ERR_INVALID_ARG if motor_idx is invalid
 * @return RX_ERR_TIMEOUT if mutex timeout
 */
rx_err_t motor_shared_state_set_status(motor_shared_state_t* state,
                                       uint8_t               motor_idx,
                                       float                 velocity_mps,
                                       float                 duty_cycle,
                                       float                 current_ma,
                                       rx_motor_state_t      motor_state);

/**
 * @brief Get motor status for a specific motor
 *
 * @param[in]  state            Pointer to shared state structure. Must not be NULL.
 * @param[in]  motor_idx        Motor index (0-3)
 * @param[out] out_velocity_mps Pointer to store measured velocity. Must not be NULL.
 * @param[out] out_duty_cycle   Pointer to store duty cycle. Must not be NULL.
 * @param[out] out_current_ma   Pointer to store current. Must not be NULL.
 * @param[out] out_motor_state  Pointer to store motor state. Must not be NULL.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if any pointer is NULL
 * @return RX_ERR_INVALID_ARG if motor_idx is invalid
 * @return RX_ERR_TIMEOUT if mutex timeout
 */
rx_err_t motor_shared_state_get_status(motor_shared_state_t* state,
                                       uint8_t               motor_idx,
                                       float*                out_velocity_mps,
                                       float*                out_duty_cycle,
                                       float*                out_current_ma,
                                       rx_motor_state_t*     out_motor_state);

/**
 * @brief Set emergency stop flag
 *
 * @param[in] state  Pointer to shared state structure. Must not be NULL.
 * @param[in] active true to activate emergency stop, false to clear
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if state is NULL
 * @return RX_ERR_TIMEOUT if mutex timeout
 */
rx_err_t motor_shared_state_set_estop(motor_shared_state_t* state, bool active);

/**
 * @brief Get emergency stop flag
 *
 * @param[in]  state      Pointer to shared state structure. Must not be NULL.
 * @param[out] out_active Pointer to store emergency stop status. Must not be NULL.
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if state or out_active is NULL
 * @return RX_ERR_TIMEOUT if mutex timeout
 */
rx_err_t motor_shared_state_get_estop(motor_shared_state_t* state, bool* out_active);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_SHARED_STATE_H */
