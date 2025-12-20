/**
 * @file motor_control_task.h
 * @brief Public API for the motor control FreeRTOS task
 * @details
 * This header defines the public interface for the motor control task that runs
 * a 250Hz PID control loop for 4 independent motors. The task reads encoder feedback,
 * computes PID outputs, and applies commands to motors while communicating with the
 * command handler task through shared state.
 *
 * Key Features:
 * - 250Hz control loop with precise timing (vTaskDelayUntil)
 * - Reads all 4 multiplexed encoders sequentially
 * - 4 independent PID controllers (skid-steer kinematics)
 * - Thread-safe shared state access via mutex
 * - Emergency stop support with <4ms latency
 *
 * Example Usage:
 * @code
 * // Initialize hardware components (in main.c)
 * star_motor_handle_t motors[4];
 * star_encoder_mux_handle_t encoder_mux;
 * star_pid_handle_t pids[4];
 * motor_shared_state_t shared_state;
 * // ... initialize all components ...
 *
 * // Create and start motor control task
 * motor_control_task_create(&shared_state, motors, &encoder_mux, pids);
 * @endcode
 *
 * @date 2025-12-20
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef MOTOR_CONTROL_TASK_H
#define MOTOR_CONTROL_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "motor_shared_state.h"
#include "star_encoder_mux.h"
#include "star_motor.h"
#include "star_pid.h"

/* --- Configuration Constants --- */

/**
 * @brief Motor control loop configuration
 */
typedef enum {
  k_motor_control_loop_hz   = 250, /**< Control loop frequency (Hz) */
  k_motor_control_period_ms = 4,   /**< Control loop period (ms) - 1000/250 */
} motor_control_config_t;

/**
 * @brief Task configuration
 */
typedef enum {
  k_motor_control_task_stack_size = 8192, /**< Stack size (bytes) */
  k_motor_control_task_priority   = 8,    /**< Task priority (higher than telemetry) */
} motor_task_config_t;

/**
 * @brief Wheel configuration
 */
typedef enum {
  k_wheel_diameter_m = 100, /**< Wheel diameter (mm) */
  k_encoder_ppr      = 341, /**< Encoder pulses per revolution (PPR) */
} wheel_config_t;

/* --- Public Function Prototypes --- */

/**
 * @brief Create and start the motor control task
 *
 * This task handles:
 * - Reading all 4 encoders via multiplexer
 * - Computing PID control for 4 motors
 * - Applying motor commands
 * - Updating shared state with status
 * - Monitoring emergency stop
 *
 * The task runs at 250Hz with precise timing using vTaskDelayUntil().
 *
 * @param[in] shared_state Pointer to initialized shared state structure
 * @param[in] motors       Array of 4 initialized motor handles
 * @param[in] encoder_mux  Pointer to initialized encoder multiplexer
 * @param[in] pids         Array of 4 initialized PID controllers
 *
 * @note All parameters must remain valid for the lifetime of the task
 * @note The task must be created after initializing the shared state
 */
void motor_control_task_create(motor_shared_state_t*      shared_state,
                               star_motor_handle_t        motors[k_motor_count],
                               star_encoder_mux_handle_t* encoder_mux,
                               star_pid_handle_t          pids[k_motor_count]);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CONTROL_TASK_H */
