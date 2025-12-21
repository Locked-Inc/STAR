/* include/tasks/motor_control_task.h */

/**
 * @file motor_control_task.h
 * @brief Public API for the motor control ThreadX task
 * @details
 * This header defines the public interface for the motor control task that runs
 * a 250Hz PID control loop for 4 independent motors. The task reads encoder feedback
 * from dedicated MTU channels, computes PID outputs, and applies commands to motors
 * while communicating with the command handler task through shared state.
 *
 * Port of ESP32 motor_control_task using ThreadX RTOS and RX72N peripherals.
 *
 * Key Features:
 * - 250Hz control loop with CMT1 timer interrupt
 * - Direct encoder access (no multiplexing - RX72N has 182 GPIO!)
 * - 4 independent PID controllers (skid-steer kinematics)
 * - Thread-safe shared state access via ThreadX mutex
 * - Emergency stop support with <4ms latency
 *
 * Key Differences from ESP32:
 * - No encoder multiplexer (RX72N has enough pins for 4 dedicated encoders)
 * - Uses ThreadX instead of FreeRTOS
 * - Reads 4 MTU encoder channels directly
 * - CMT1 provides 250Hz timing instead of vTaskDelayUntil
 *
 * Example Usage:
 * @code
 * // Initialize hardware components (in main.c)
 * rx_motor_handle_t motors[4];
 * rx_mtu_channel_t encoder_channels[4] = {k_mtu_channel_1, k_mtu_channel_2, ...};
 * rx_pid_handle_t pids[4];
 * motor_shared_state_t shared_state;
 * // ... initialize all components ...
 *
 * // Create and start motor control task
 * motor_control_task_create(&shared_state, motors, encoder_channels, pids);
 * @endcode
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef MOTOR_CONTROL_TASK_H
#define MOTOR_CONTROL_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "motor_shared_state.h"
#include "rx_motor.h"
#include "rx_mtu_encoder.h"
#include "rx_pid.h"

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

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
  k_motor_control_task_stack_size = 4096, /**< Stack size (bytes) - RX72N has more RAM */
  k_motor_control_task_priority   = 8,    /**< Task priority (higher than telemetry) */
} motor_task_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Create and start the motor control task
 *
 * This task handles:
 * - Reading all 4 encoders from dedicated MTU channels
 * - Computing PID control for 4 motors
 * - Applying motor commands
 * - Updating shared state with status
 * - Monitoring emergency stop
 *
 * The task runs at 250Hz triggered by CMT1 timer interrupt.
 *
 * @param[in] shared_state     Pointer to initialized shared state structure
 * @param[in] motors           Array of 4 initialized motor handles
 * @param[in] encoder_channels Array of 4 MTU channel numbers for encoders
 * @param[in] pids             Array of 4 initialized PID controllers
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if any pointer is NULL
 * @return RX_ERR_INVALID_STATE if thread creation fails
 *
 * @note All parameters must remain valid for the lifetime of the task
 * @note The task must be created after initializing the shared state
 */
rx_err_t motor_control_task_create(motor_shared_state_t* shared_state,
                                    rx_motor_handle_t     motors[k_motor_count],
                                    rx_mtu_channel_t      encoder_channels[k_motor_count],
                                    rx_pid_handle_t       pids[k_motor_count]);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CONTROL_TASK_H */
