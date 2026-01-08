/* include/tasks/motor_controller.h */

/**
 * @file motor_controller.h
 * @brief Motor Controller thread for deterministic motor control
 * @details
 * Highest priority thread (priority 2) running at 250 Hz for real-time
 * motor control. Implements closed-loop PID velocity control for 4 motors.
 *
 * Thread Responsibilities:
 * - Read encoder positions/velocities (MTU quadrature)
 * - Execute PID control for 4 motors
 * - Update PWM duty cycles (GPTW)
 * - Monitor motor current (ADC)
 * - Detect faults (nFAULT pins)
 * - Check emergency stop flag every cycle (4ms)
 * - Feed watchdog timer (250 Hz)
 *
 * Priority Rationale:
 * Highest priority (2) ensures deterministic 4ms control loop.
 * If interrupted by sensor delays, robot would jitter or drift.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "tx_api.h"
#include "rx_err.h"

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Create and start Motor_Controller thread
 * @details
 * Creates the Motor_Controller thread with:
 * - Priority: 2 (highest)
 * - Stack: 4096 bytes (statically allocated)
 * - Frequency: 250 Hz (4ms period)
 *
 * Must be called from tx_application_define() after shared_state_init().
 *
 * Thread will:
 * 1. Initialize motor subsystem (motors, encoders, PIDs)
 * 2. Enter 250 Hz control loop
 * 3. Feed watchdog every cycle
 *
 * @return TX_SUCCESS on success
 * @return TX_THREAD_ERROR if thread creation fails
 */
UINT motor_controller_create(void);

#endif /* MOTOR_CONTROLLER_H */
