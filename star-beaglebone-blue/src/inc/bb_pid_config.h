/**
 * @file bb_pid_config.h
 * @brief Seed gain constants for the BBB velocity PID loop.
 *
 * @details
 * Single source of truth for the BeagleBone Blue motor velocity PID gains
 * and saturation limits. The loop runs at 100 Hz inside motor_control_task
 * and operates in motor output-shaft angular-velocity units (rad/s), with
 * the PID output in percent duty (-100..+100).
 *
 * @par Plant model and gain provenance:
 * The motor on the BBB chassis is the same DFRobot 6V 210 RPM 34:1 341 PPR
 * gearmotor used by the RX72N firmware target, so the plant from motor
 * voltage to output-shaft angular velocity is structurally unchanged
 * between targets. The starting-point gains below are copied from the
 * MATLAB-tuned configuration generated for that motor:
 *   - Plant model:  G(s) = 3.665 / (0.075 s + 1)
 *   - Design tool:  matlab/pid_design_velocity.m  (MATLAB pidtune())
 *   - Bandwidth:    20 rad/s
 *   - Discretized at 100 Hz (Tustin) by matlab/pid_discretize.m
 *   - Output struct: matlab/pid_config_output.txt
 *
 * @warning These gains have NOT been validated on the assembled BBB build.
 *          The chassis is heavier and the new miter-gear stage adds Coulomb
 *          friction, so the effective time constant and DC gain may have
 *          shifted slightly from the model. Bench tuning is tracked in the
 *          GitHub issue for Phase B of the PID-fix plan -- update this file
 *          with refined values once data is collected.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.2.0
 */

#pragma once

#include <stdint.h>

#include "hardware_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum bb_pid_loop_t
 * @brief Closed-loop control rate constants.
 *
 * @details
 * The BBB velocity PID runs in the same control tick as
 * motor_control_task, so the rate is derived directly from
 * k_bb_motor_period_ns in hardware_config.h. Deriving (rather than
 * hardcoding) ensures the discretization assumption documented above
 * (Tustin transform at the sample rate) cannot drift away from the
 * actual loop period if the motor task rate ever changes.
 *
 * @invariant k_bb_pid_loop_hz == 1 / k_bb_motor_period_seconds
 *
 * @since Version 1.2.0
 */
typedef enum : uint32_t {
    k_bb_pid_loop_hz =
        (uint32_t)(k_bb_ns_per_sec / k_bb_motor_period_ns), /**< PID compute rate (Hz) */
} bb_pid_loop_t;

/**
 * @brief Proportional gain (seed value from MATLAB pidtune at 20 rad/s).
 * @details Units: percent duty per (rad/s) of motor output-shaft error.
 * @since Version 1.2.0
 */
static const float s_bb_pid_velocity_kp = 0.286F;

/**
 * @brief Integral gain (seed value from MATLAB pidtune at 20 rad/s).
 * @details Units: percent duty per (rad) of accumulated output-shaft error.
 * @since Version 1.2.0
 */
static const float s_bb_pid_velocity_ki = 8.01F;

/**
 * @brief Derivative gain (disabled at seed).
 *
 * @details
 * Encoder velocity is computed from raw tick deltas without filtering, so
 * the measurement noise floor at low speeds is several percent. Enabling Kd
 * without first adding a low-pass filter on the velocity feedback would
 * amplify that noise into duty chatter. Leave at zero until either the
 * filter is added or bench tuning shows D is required.
 *
 * @since Version 1.2.0
 */
static const float s_bb_pid_velocity_kd = 0.0F;

/**
 * @brief PID output saturation, lower bound (percent duty).
 * @since Version 1.2.0
 */
static const float s_bb_pid_output_min = -100.0F;

/**
 * @brief PID output saturation, upper bound (percent duty).
 * @since Version 1.2.0
 */
static const float s_bb_pid_output_max = 100.0F;

/**
 * @brief Anti-windup integral lower clamp.
 *
 * @details
 * Half of the output range. When the output saturates, the integral cannot
 * accumulate beyond what could be undone by a single full-range proportional
 * swing, which keeps recovery from saturation fast.
 *
 * @since Version 1.2.0
 */
static const float s_bb_pid_integral_min = -50.0F;

/**
 * @brief Anti-windup integral upper clamp.
 * @since Version 1.2.0
 */
static const float s_bb_pid_integral_max = 50.0F;

#ifdef __cplusplus
}
#endif
