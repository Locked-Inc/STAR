/**
 * @file motor_control_task.h
 * @brief Motor Control Task - 250 Hz PID-Based Velocity Control
 *
 * @details
 * Declares the motor control task responsible for closed-loop velocity
 * control of four brushed DC gearmotors using PID controllers.
 *
 * **Responsibilities:**
 * - Run PID control loop at 250 Hz (4 ms period)
 * - Read encoder feedback from MTU quadrature inputs
 * - Compute PID output for each motor
 * - Command DRV8243 H-bridge drivers via SPI
 * - Monitor motor faults (overcurrent, stall detection)
 * - Handle emergency stop conditions
 *
 * **Control Architecture:**
 * ```
 * RPi5 -> CommTask -> MotorControlTask (this)
 *                        ↓ (250 Hz)
 *                   [PID Controller]
 *                        ↓
 *                   [Encoder Feedback] ← MTU (Hall effect)
 *                        ↓
 *                   [PWM Output] -> DRV8243 -> Motors
 * ```
 *
 * @see motor_control_task.c Implementation
 * @see rx_pid.h PID controller
 * @see rx_motor.h Motor abstraction
 * @see rx_drv8243.h H-bridge driver
 *
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include "rx_err.h"
#include "rx_motor.h"

/**
 * @enum motor_count_t
 * @brief Motor count sentinels for motor_control_task_get_motors()
 *
 * @details
 * Named constants for motor count return values. Used to avoid magic
 * number 0 in callers checking whether motors are ready. Callers should
 * compare the out_count value returned by motor_control_task_get_motors()
 * against k_motor_count_none to determine whether motors are available.
 *
 * @invariant k_motor_count_none == 0 (sentinel value, never a valid motor count)
 *
 * @code
 * uint8_t motor_count = k_motor_count_none;
 * rx_motor_handle_t** motors = motor_control_task_get_motors(&motor_count);
 * if (motors == nullptr || motor_count == k_motor_count_none) {
 *     // Motor task not yet created — motors not available
 * }
 * @endcode
 *
 * @see motor_control_task_get_motors() Returns k_motor_count_none when not ready
 *
 * @since STAR v1.0.0
 */
typedef enum : uint8_t {
  k_motor_count_none =
    0, /**< Zero motors available — motor_control_task_create() has not yet been called */
} motor_count_t;

/**
 * @brief Create and start the motor control task
 *
 * @details
 * Creates the MotorTask ThreadX task for high-frequency motor control.
 * This is the highest-priority application task to ensure deterministic
 * control loop timing.
 *
 * **Task Configuration:**
 * - Priority: 6 (highest application priority after AppMain)
 * - Stack: 3072 bytes (PID computations require stack)
 * - Period: 4 ms (250 Hz control loop)
 * - Auto-start: Enabled
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_* ThreadX task creation failed
 *
 * @pre ThreadX kernel running
 * @pre Motor drivers (DRV8243) initialized
 * @pre Encoders (MTU) configured
 * @pre PID controllers tuned
 * @pre Shared data initialized
 *
 * @post MotorTask created and running at 250 Hz
 * @post Motors ready to respond to velocity commands
 *
 * @note Call this from AppMainTask after motor hardware initialization
 * @note Highest priority task - runs before all other application tasks
 *
 * @see motor_control_task.c Implementation details
 * @see app_main_task.c Task creation coordinator
 *
 * @since STAR v1.0.0
 */
rx_err_t motor_control_task_create(void);

/**
 * @brief Get motor handle array for obstacle detection emergency stop
 *
 * @details
 * Returns pointers to the 4 initialized motor handles for use by the
 * obstacle detection system. These handles allow the obstacle detection
 * task to execute emergency motor stops when obstacles are detected.
 *
 * **Usage Pattern:**
 * ```c
 * uint8_t motor_count = 0;
 * rx_motor_handle_t** motors = motor_control_task_get_motors(&motor_count);
 * config.motors = motors;
 * config.motor_count = motor_count;
 * ```
 *
 * **Motor Array:**
 * - Index 0: Front-left motor
 * - Index 1: Front-right motor
 * - Index 2: Rear-left motor
 * - Index 3: Rear-right motor
 *
 * @param[out] out_count Pointer to receive motor count
 *                       Set to k_motor_count (4) on success, or k_motor_count_none (0) on failure
 *                       Must be non-NULL
 *
 * @return rx_motor_handle_t** Pointer to array of motor handle pointers
 *
 * @retval Non-null pointer to static array of k_motor_count (4) rx_motor_handle_t*,
 *         with out_count set to k_motor_count — returned when motors are initialized
 *         and out_count is non-NULL
 * @retval nullptr with out_count set to k_motor_count_none (0) — returned when
 *         motor_control_task_create() has not yet been called (motors not ready)
 * @retval nullptr (out_count unchanged) — returned when out_count argument is nullptr
 *
 * @pre motor_control_task_create() has been called successfully
 * @pre out_count != nullptr (NULL pointer returns nullptr immediately)
 * @post out_count set to k_motor_count (4) on success
 * @post Returns valid pointer to motor handle array on success
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 9 Exception**: Returns rx_motor_handle_t** (double indirection)
 *   - Standard Rule 9: Maximum one level of pointer dereferencing
 *   - **Justification**: Enables zero-copy access to motor handles for emergency stop
 *   - **Safety**: Returns pointer to static array (no dynamic allocation, lifetime guaranteed)
 *   - **Alternative rejected**: Copy-out API would require 128 bytes stack per call (4 handles × 32 bytes)
 *   - **Usage pattern**: Similar to argv in main(int argc, char** argv) - read-only array access
 *   - **Verification**: Static analysis confirms single dereference in obstacle_detect_task.c
 *
 * @note Thread-safe: Returns pointer to static memory
 * @note Lifetime: Motor handles valid until program termination
 * @warning Do NOT call before motor_control_task initialization
 * @warning Do NOT modify returned motor handles directly
 *
 * @see rx_motor.h Motor handle operations
 * @see obstacle_detect_task.c Uses these handles for emergency stop
 *
 * @since STAR v1.0.0
 */
rx_motor_handle_t** motor_control_task_get_motors(uint8_t* out_count);
