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
 * RPi5 → CommTask → MotorControlTask (this)
 *                        ↓ (250 Hz)
 *                   [PID Controller]
 *                        ↓
 *                   [Encoder Feedback] ← MTU (Hall effect)
 *                        ↓
 *                   [PWM Output] → DRV8243 → Motors
 * ```
 *
 * @see motor_control_task.c Implementation
 * @see rx_pid.h PID controller
 * @see rx_motor.h Motor abstraction
 * @see rx_drv8243.h H-bridge driver
 *
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_MOTOR_CONTROL_TASK_H
#define STAR_MOTOR_CONTROL_TASK_H

#include "rx_err.h"

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

#endif /* STAR_MOTOR_CONTROL_TASK_H */
