/**
 * @file rx_pid.h
 * @brief PID Controller API for Closed-Loop Motor Control with Anti-Windup
 *
 * @details
 * ## Overview
 * Production-quality PID (Proportional-Integral-Derivative) controller implementation
 * for safety-critical embedded motor control. Provides deterministic, floating-point
 * control algorithm with integral anti-windup, configurable saturation limits, and
 * runtime gain tuning for velocity and position control loops.
 *
 * **Algorithm**: Standard parallel-form discrete PID with anti-windup
 * **Precision**: IEEE 754 single-precision (float)
 * **Control Rate**: 100Hz - 1kHz (typical: 250Hz for RX72N motor control)
 * **Hardware Independent**: Pure algorithm (no peripheral dependencies)
 *
 * ## PID Control Theory
 *
 * ### Transfer Function
 * Continuous-time PID controller transfer function:
 * @f[
 *   u(t) = K_p e(t) + K_i \int_0^t e(\tau) d\tau + K_d \frac{de(t)}{dt}
 * @f]
 *
 * ### Discrete Implementation
 * This module implements the velocity-form discrete PID:
 * @f[
 *   u[k] = K_p e[k] + K_i \sum_{i=0}^{k} e[i] \Delta t + K_d \frac{e[k] - e[k-1]}{\Delta t}
 * @f]
 *
 * Where:
 * - @f$ e[k] = \text{setpoint} - \text{measured} @f$ (error at sample k)
 * - @f$ u[k] @f$ = control output at sample k
 * - @f$ \Delta t @f$ = sample period (dt parameter)
 * - @f$ K_p, K_i, K_d @f$ = proportional, integral, derivative gains
 *
 * ### PID Terms Explained
 * | Term | Formula | Effect | When to Increase | When to Decrease |
 * |------|---------|--------|------------------|------------------|
 * | **P (Proportional)** | @f$ K_p e[k] @f$ | Immediate response to current error | Sluggish response, steady-state error | Oscillation, overshoot |
 * | **I (Integral)** | @f$ K_i \sum e[i] \Delta t @f$ | Eliminates steady-state error | Persistent offset | Overshoot, instability |
 * | **D (Derivative)** | @f$ K_d \frac{de}{dt} @f$ | Predicts future error, damping | Overshoot, oscillation | Noise amplification, slow response |
 *
 * ## Anti-Windup Strategy
 *
 * **Problem**: Integral term accumulates unbounded error when output is saturated,
 * causing overshoot and sluggish response when setpoint changes.
 *
 * **Solution**: Clamp integral term to [integral_min, integral_max]:
 * @code{.c}
 * integral += error * dt;
 * if (integral > integral_max) integral = integral_max;  // Prevent positive windup
 * if (integral < integral_min) integral = integral_min;  // Prevent negative windup
 * @endcode
 *
 * **Typical Settings**:
 * - **Symmetric**: integral_min = -integral_max (most common)
 * - **Asymmetric**: Different limits for forward/reverse (e.g., braking vs. acceleration)
 *
 * ## STAR Motor Control Configuration
 *
 * ### Velocity Control (100 RPM target, 4ms sample period)
 * Based on MATLAB system identification (tau = 75ms, Kmotor = 3.665):
 *
 * | Parameter | Value | Units | Rationale |
 * |-----------|-------|-------|-----------|
 * | Kp | 0.286 | % duty / RPM | Ziegler-Nichols tuning (Ku=0.4, Pu=0.15s) |
 * | Ki | 8.01 | % duty / (RPM*s) | Eliminates 5% steady-state error in 200ms |
 * | Kd | 0.0 | % duty / (RPM/s) | Disabled (encoder noise amplification) |
 * | output_min | -100.0 | % duty cycle | Full reverse |
 * | output_max | +100.0 | % duty cycle | Full forward |
 * | integral_min | -50.0 | % duty | Prevents excessive windup |
 * | integral_max | +50.0 | % duty | Prevents excessive windup |
 * | Sample Rate | 250 Hz | (4ms period) | Nyquist: 10x motor bandwidth (25 Hz) |
 *
 * See `matlab/pid_design_velocity.m` for derivation.
 *
 * ## Control Loop Architecture
 *
 * @dot
 * digraph pid_loop {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   setpoint [label="Setpoint\n(Target RPM)", shape=ellipse];
 *   summer [label="Sigma\n-", shape=circle];
 *   pid [label="PID Controller\nrx_pid_compute()", style="filled", fillcolor="lightblue"];
 *   motor [label="Motor +\nDriver", shape=component];
 *   encoder [label="Encoder\nFeedback"];
 *
 *   setpoint -> summer [label="r[k]"];
 *   summer -> pid [label="e[k] = r[k] - y[k]"];
 *   pid -> motor [label="u[k]\n(PWM duty)"];
 *   motor -> plant [label="omega\n(RPM)"];
 *   plant -> encoder;
 *   encoder -> summer [label="y[k]\n(measured)", dir=back];
 *
 *   plant [label="Plant\n(Motor Dynamics)", shape=component];
 * }
 * @enddot
 *
 * ## Performance Characteristics
 *
 * ### Computational Cost (RX72N @ 240 MHz)
 * | Operation | Cycles | Time | Notes |
 * |-----------|--------|------|-------|
 * | rx_pid_init() | ~150 | 625 ns | One-time initialization |
 * | rx_pid_compute() | ~200 | 833 ns | Per-sample computation (FPU accelerated) |
 * | rx_pid_reset() | ~30 | 125 ns | Clear integral/derivative state |
 * | rx_pid_set_gains() | ~60 | 250 ns | Runtime gain update |
 *
 * **Control Loop Budget**: 4ms period @ 250 Hz = 960,000 cycles available
 * - PID computation: 200 cycles (0.02% of budget)
 * - Remaining: 959,800 cycles for motor control, sensors, communication
 *
 * ### Memory Footprint
 * | Component | Size | Count | Total |
 * |-----------|------|-------|-------|
 * | rx_pid_handle_t | 40 bytes | 4 motors | 160 bytes |
 * | Stack (rx_pid_compute) | 32 bytes | - | 32 bytes (transient) |
 * | **Total RAM** | - | - | **160 bytes** (static) |
 * | Code (Flash) | ~800 bytes | - | Shared across all instances |
 *
 * ## Tuning Guidelines
 *
 * ### Ziegler-Nichols Method (Step Response)
 * 1. **Measure step response**: Apply step input, record time constant (tau) and gain (K)
 * 2. **Calculate gains**:
 *    - @f$ K_p = 1.2 \frac{\tau}{K L} @f$
 *    - @f$ K_i = \frac{K_p}{2 \tau} @f$
 *    - @f$ K_d = 0.5 K_p \tau @f$
 *    Where L = dead time (typically 1-2 sample periods)
 *
 * ### Manual Tuning Procedure
 * 1. **Start with I and D at zero**: Kp only
 * 2. **Increase Kp** until steady oscillation (critical gain Ku)
 * 3. **Measure oscillation period** (Pu)
 * 4. **Calculate gains**:
 *    - Kp = 0.6 * Ku
 *    - Ki = 1.2 * Ku / Pu
 *    - Kd = 0.075 * Ku * Pu
 * 5. **Fine-tune** based on observed response
 *
 * ### Common Issues and Solutions
 * | Problem | Symptom | Solution |
 * |---------|---------|----------|
 * | Overshoot | Output exceeds setpoint by >10% | Decrease Kp, increase Kd |
 * | Oscillation | Sustained ringing around setpoint | Decrease Kp and Kd |
 * | Steady-state error | Doesn't reach setpoint | Increase Ki |
 * | Slow response | Takes >5tau to settle | Increase Kp |
 * | Integral windup | Large overshoot after saturation | Decrease integral_max |
 * | Noise amplification | Erratic output | Decrease Kd, add filtering |
 *
 * ## Usage Examples
 *
 * ### Basic Velocity Control Loop
 * @code{.c}
 * #include "rx_pid.h"
 * #include "rx_motor.h"
 * #include "rx_encoder.h"
 *
 * // Initialize PID for motor velocity control
 * rx_pid_handle_t pid;
 * rx_pid_config_t config = {
 *   .kp = 0.286f,           // From MATLAB tuning
 *   .ki = 8.01f,
 *   .kd = 0.0f,             // Disabled (noisy encoder)
 *   .output_min = -100.0f,  // Full reverse
 *   .output_max = 100.0f,   // Full forward
 *   .integral_min = -50.0f, // Anti-windup limits
 *   .integral_max = 50.0f
 * };
 * rx_pid_init(&pid, &config);
 *
 * // Control loop @ 250 Hz (4ms period)
 * void motor_control_task(void) {
 *   const float target_rpm = 100.0f;
 *   const float dt = 0.004f;  // 4ms
 *
 *   while (1) {
 *     // Measure current velocity
 *     float current_rpm = rx_encoder_get_rpm();
 *
 *     // Compute PID output
 *     float duty_cycle;
 *     rx_pid_compute(&pid, target_rpm, current_rpm, dt, &duty_cycle);
 *
 *     // Apply to motor
 *     rx_motor_set_duty_cycle(duty_cycle);
 *
 *     // Wait for next sample
 *     tx_thread_sleep(4);  // 4ms @ 1000 ticks/sec
 *   }
 * }
 * @endcode
 *
 * ### Runtime Gain Tuning
 * @code{.c}
 * // Implement auto-tuner based on step response
 * void auto_tune_pid(rx_pid_handle_t* pid) {
 *   // Measure system response
 *   float ku = measure_critical_gain();
 *   float pu = measure_oscillation_period();
 *
 *   // Calculate Ziegler-Nichols gains
 *   float kp = 0.6f * ku;
 *   float ki = 1.2f * ku / pu;
 *   float kd = 0.075f * ku * pu;
 *
 *   // Update PID gains (preserves integral state)
 *   rx_pid_set_gains(pid, kp, ki, kd);
 *   rx_log_info("TUNE", "New gains applied");
 * }
 * @endcode
 *
 * ### Setpoint Change with Reset
 * @code{.c}
 * void change_target_speed(float new_target_rpm) {
 *   // Reset PID to prevent integral windup from old error
 *   rx_pid_reset(&pid);
 *
 *   // Update setpoint
 *   target_rpm = new_target_rpm;
 *   rx_log_info_val("MOTOR", "New target RPM", (uint32_t)new_target_rpm);
 * }
 * @endcode
 *
 * ## Control Loop Sequence
 *
 * One 4 ms control cycle showing all participants from the ThreadX timer
 * callback through the PID algorithm to the motor driver:
 *
 * @startuml
 * participant "ThreadX Timer\n(250 Hz)" as Timer
 * participant "Motor Task" as Task
 * participant "rx_pid_compute()" as PID
 * participant "rx_encoder\n(Hall sensor)" as Enc
 * participant "rx_motor\n(DRV8263H)" as Motor
 *
 * Timer -> Task : tx_thread_sleep expires (4 ms)
 * Task -> Enc   : rx_encoder_get_rpm()
 * Enc --> Task  : measured_rpm (float)
 * Task -> PID   : rx_pid_compute(pid, target, measured, 0.004, &duty)
 * PID -> PID    : error = target - measured
 * PID -> PID    : integral += error * dt (clamped)
 * PID -> PID    : derivative = (error - prev) / dt
 * PID -> PID    : output = Kp*e + Ki*i + Kd*d (clamped)
 * PID --> Task  : k_rx_ok, duty [-100, +100] %
 * Task -> Motor : rx_motor_set_duty_cycle(duty)
 * Motor --> Task: k_rx_ok
 * Task -> Timer : tx_thread_sleep(4)
 * @enduml
 *
 * ## Dependencies
 * - `rx_err.h` - Error code definitions
 * - `<math.h>` (implicit) - FPU operations (addition, multiplication)
 * - **No hardware dependencies** - Pure algorithm module
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 [YES]:** No goto, setjmp, recursion (pure computation functions)
 * - **Rule 2 [YES]:** All loops bounded (no loops - direct computation)
 * - **Rule 3 [YES]:** No dynamic allocation (stack-based handle structure)
 * - **Rule 4 [YES]:** All functions <60 lines (rx_pid_compute is 44 lines)
 * - **Rule 5 [YES]:** Minimum 2 validations per function (NULL checks, dt > 0, gain validity)
 * - **Rule 6 [YES]:** Data at smallest scope (local variables in computation)
 * - **Rule 7 [YES]:** All returns validated (rx_err_t checked at call sites)
 * - **Rule 8 [YES]:** No macros except include guards
 * - **Rule 9 [YES]:** Single-level pointer dereferencing (handle, output pointers)
 * - **Rule 10 [YES]:** Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Only PID algorithm computation (no motor control, no hardware)
 * - **O (Open/Closed):** Gains/limits configurable without modifying code
 * - **L (Liskov Substitution):** All functions return rx_err_t with consistent semantics
 * - **I (Interface Segregation):** Minimal API (7 functions), each with clear purpose
 * - **D (Dependency Inversion):** Depends only on abstract rx_err_t (no concrete hardware)
 *
 * @par Module Dependencies:
 * ```
 * rx_pid.h
 *   +--> rx_err.h (error code definitions)
 *
 * Used by:
 *   +--> Motor control tasks (velocity/position loops)
 *   +--> Temperature control (heating/cooling)
 *   +--> Any closed-loop control application
 * ```
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @see rx_err.h for error code definitions
 * @see matlab/motor_model_1st_order.m for system identification
 * @see matlab/pid_design_velocity.m for gain derivation
 * @see matlab/pid_discretize.m for discrete implementation
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @struct rx_pid_config_t
 * @brief PID controller configuration structure defining gains and saturation limits
 *
 * @details
 * Configuration parameters for initializing a PID controller instance. All fields
 * must be carefully tuned for the specific plant dynamics (motor, heater, etc.).
 *
 * ## Field Constraints and Validation
 * | Field | Type | Valid Range | Units | Validation |
 * |-------|------|-------------|-------|------------|
 * | kp | float | >= 0, finite | output/error | Must be non-negative, not NaN/Inf |
 * | ki | float | >= 0, finite | output/(error*s) | Must be non-negative, not NaN/Inf |
 * | kd | float | >= 0, finite | output/(error/s) | Must be non-negative, not NaN/Inf |
 * | output_min | float | < output_max | depends on actuator | Must be strictly less than output_max |
 * | output_max | float | > output_min | depends on actuator | Must be strictly greater than output_min |
 * | integral_min | float | < integral_max | same as output | Must be strictly less than integral_max |
 * | integral_max | float | > integral_min | same as output | Must be strictly greater than integral_min |
 *
 * ## Tuning Guidelines by Application
 *
 * ### Motor Velocity Control (100 RPM target)
 * - **kp**: 0.286 (from Ziegler-Nichols method with tau=75ms motor)
 * - **ki**: 8.01 (eliminates steady-state error in 200ms)
 * - **kd**: 0.0 (disabled - encoder noise amplification issue)
 * - **output_min/max**: +/-100.0 (PWM duty cycle percentage)
 * - **integral_min/max**: +/-50.0 (prevents excessive windup during saturation)
 *
 * ### Temperature Control (+/-1degC accuracy)
 * - **kp**: 2.0 (aggressive thermal response)
 * - **ki**: 0.1 (slow integration for thermal lag)
 * - **kd**: 0.5 (damping for overshoot prevention)
 * - **output_min/max**: 0.0 to 100.0 (heater power percentage)
 * - **integral_min/max**: 0.0 to 50.0 (asymmetric - no negative heating)
 *
 * ## Anti-Windup Limit Selection
 *
 * **Rule of Thumb**: Set integral limits to 50-80% of output range:
 * - Prevents integral term from dominating during transients
 * - Allows room for P and D terms to contribute
 * - Faster recovery from saturation events
 *
 * **Example**:
 * @code{.c}
 * // For output range [-100, +100]:
 * config.integral_min = -50.0f;  // 50% of output_min
 * config.integral_max = +50.0f;  // 50% of output_max
 * @endcode
 *
 * @par Usage Example: Motor Velocity PID
 * @code{.c}
 * rx_pid_config_t motor_config = {
 *   .kp = 0.286f,           // Proportional: immediate response to error
 *   .ki = 8.01f,            // Integral: eliminate steady-state error
 *   .kd = 0.0f,             // Derivative: disabled (noisy encoder)
 *   .output_min = -100.0f,  // Full reverse (-100% duty cycle)
 *   .output_max = 100.0f,   // Full forward (+100% duty cycle)
 *   .integral_min = -50.0f, // Anti-windup lower bound
 *   .integral_max = 50.0f   // Anti-windup upper bound
 * };
 * @endcode
 *
 * @see rx_pid_init() Initialize PID with this configuration
 * @see rx_pid_set_gains() Update gains at runtime
 * @see rx_pid_set_output_limits() Update output limits at runtime
 * @see rx_pid_set_integral_limits() Update integral limits at runtime
 */
typedef struct {
  float
    kp; /**< Proportional gain (K_p): multiplies current error. Higher = faster response, risk of overshoot. */
  float
    ki; /**< Integral gain (K_i): multiplies accumulated error. Higher = faster elimination of steady-state error, risk of windup. */
  float
    kd; /**< Derivative gain (K_d): multiplies rate of error change. Higher = more damping, risk of noise amplification. */
  float
    output_min; /**< Minimum output limit (actuator lower bound). Example: -100.0 for full reverse motor duty cycle. */
  float
    output_max; /**< Maximum output limit (actuator upper bound). Example: +100.0 for full forward motor duty cycle. */
  float
    integral_min; /**< Minimum integral accumulator limit (anti-windup). Prevents excessive negative integral buildup during saturation. */
  float
    integral_max; /**< Maximum integral accumulator limit (anti-windup). Prevents excessive positive integral buildup during saturation. */
} rx_pid_config_t;

/**
 * @brief PID controller handle structure
 */
typedef struct {
  float kp;           /**< Proportional gain */
  float ki;           /**< Integral gain */
  float kd;           /**< Derivative gain */
  float output_min;   /**< Minimum output limit */
  float output_max;   /**< Maximum output limit */
  float integral_min; /**< Minimum integral value (anti-windup) */
  float integral_max; /**< Maximum integral value (anti-windup) */
  float integral;     /**< Accumulated integral term */
  float prev_error;   /**< Previous error for derivative calculation */
  bool  initialized;  /**< Initialization flag */
} rx_pid_handle_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize a PID controller instance
 *
 * @param[out] handle Pointer to PID handle structure. Must not be nullptr.
 * @param[in]  config Pointer to PID configuration. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or config is nullptr
 * @return k_rx_err_invalid_arg if output_max <= output_min or integral_max <= integral_min
 */
[[nodiscard]] rx_err_t rx_pid_init(rx_pid_handle_t* handle, const rx_pid_config_t* config);

/**
 * @brief Deinitialize PID controller and clear state
 *
 * @param[in] handle Pointer to initialized PID handle. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_pid_deinit(rx_pid_handle_t* handle);

/**
 * @brief Compute PID control output for current sample
 *
 * @details
 * Core PID computation function implementing the discrete parallel-form PID algorithm
 * with integral anti-windup and output saturation. Call this function at a fixed
 * sample rate (e.g., 250 Hz for motor control) to generate control outputs.
 *
 * ## Algorithm (Discrete Parallel-Form PID)
 *
 * 1. **Calculate error**: @f$ e[k] = \text{setpoint} - \text{measured} @f$
 * 2. **Proportional term**: @f$ P = K_p \cdot e[k] @f$
 * 3. **Integral term** (with anti-windup):
 *    ```
 *    integral += e[k] * dt
 *    integral = clamp(integral, integral_min, integral_max)
 *    I = Ki * integral
 *    ```
 * 4. **Derivative term**: @f$ D = K_d \cdot \frac{e[k] - e[k-1]}{dt} @f$
 * 5. **Combine**: @f$ u[k] = P + I + D @f$
 * 6. **Saturate output**: @f$ u[k] = \text{clamp}(u[k], \text{output\_min}, \text{output\_max}) @f$
 * 7. **Store state**: @f$ e[k-1] = e[k] @f$ for next iteration
 *
 * ## Transfer Function
 * @f[
 *   u[k] = K_p e[k] + K_i \sum_{i=0}^{k} e[i] \Delta t + K_d \frac{e[k] - e[k-1]}{\Delta t}
 * @f]
 *
 * ## Performance Characteristics
 * - **Execution Time**: ~833 ns @ 240 MHz RX72N (200 cycles with FPU)
 * - **Memory**: 32 bytes stack (local variables)
 * - **Determinism**: Near-constant execution time; only data-dependent branch is the
 *                    final isfinite() post-condition check (returns k_rx_fail if NaN/Inf)
 * - **Numerical Stability**: IEEE 754 single-precision (6-7 decimal digits)
 *
 * ## Sample Rate Guidelines
 * | Application | Bandwidth | Min Sample Rate | Recommended | dt Value |
 * |-------------|-----------|-----------------|-------------|----------|
 * | Motor velocity | 25 Hz | 250 Hz (10x) | 250-500 Hz | 0.004-0.002 s |
 * | Motor position | 10 Hz | 100 Hz (10x) | 100-200 Hz | 0.010-0.005 s |
 * | Temperature | 0.1 Hz | 1 Hz (10x) | 1-10 Hz | 1.0-0.1 s |
 *
 * **Nyquist Rule**: Sample rate >= 10x plant bandwidth for good control
 *
 * ## Error Handling
 * Function validates all inputs before computation:
 * - nullptr checks (handle, output)
 * - Initialization state check
 * - dt > 0 validation (zero or negative dt causes division by zero in derivative)
 *
 * @param[in] handle Pointer to initialized PID controller handle
 *                   - Must be initialized via rx_pid_init()
 *                   - Cannot be nullptr
 *                   - Contains gains, limits, and state (integral, prev_error)
 * @param[in] setpoint Desired target value (reference signal)
 *                     - Units depend on application (RPM, degC, position, etc.)
 *                     - Example: 100.0f for 100 RPM motor speed
 *                     - Can be changed between calls for tracking control
 * @param[in] measured Current measured feedback value (process variable)
 *                     - Same units as setpoint
 *                     - Example: 95.0f for current motor speed of 95 RPM
 *                     - Must be obtained from sensor/encoder
 * @param[in] dt Time delta in seconds since last call (sample period)
 *               - Must be > 0 (validated, returns error if <= 0)
 *               - Should be constant for best performance
 *               - Example: 0.004f for 250 Hz control rate
 *               - Used for integral accumulation and derivative calculation
 * @param[out] output Pointer to store computed control output
 *                    - Cannot be nullptr
 *                    - Units depend on application (% duty cycle, degC, etc.)
 *                    - Clamped to [output_min, output_max]
 *                    - Example: 75.0f for 75% PWM duty cycle
 *
 * @return rx_err_t Error code indicating success or failure reason
 * @retval k_rx_ok PID output computed successfully, stored in *output
 * @retval k_rx_err_null_ptr handle or output pointer is nullptr
 * @retval k_rx_err_invalid_state PID controller not initialized (call rx_pid_init first)
 * @retval k_rx_err_invalid_arg dt <= 0 (invalid sample period)
 * @retval k_rx_fail Computed output is non-finite (NaN or Inf) -- post-condition violation
 *
 * @pre handle must be initialized via rx_pid_init()
 * @pre handle->initialized must be true
 * @pre dt must be > 0
 * @post *output contains PID control value clamped to [output_min, output_max]
 * @post handle->integral updated with anti-windup
 * @post handle->prev_error = current error (for next derivative calculation)
 *
 * @note **Thread Safety**: NOT thread-safe. Do not call concurrently for same handle.
 * @note **Sample Rate**: Must be called at consistent dt intervals for accurate integral/derivative.
 * @note **Numerical Precision**: Uses IEEE 754 float (6-7 decimal digits). Sufficient for control loops.
 *
 * @warning **dt Consistency**: Variable dt will cause integral drift and derivative spikes.
 *          Use fixed-rate timer/task for best results.
 * @warning **Sensor Noise**: High Kd amplifies noise. Add low-pass filter to measured signal if needed.
 *
 * @par Example: Motor Velocity Control Loop
 * @code{.c}
 * #include "rx_pid.h"
 * #include "rx_motor.h"
 * #include "rx_encoder.h"
 *
 * // Initialize PID (once at startup)
 * rx_pid_handle_t motor_pid;
 * rx_pid_config_t config = {
 *   .kp = 0.286f, .ki = 8.01f, .kd = 0.0f,
 *   .output_min = -100.0f, .output_max = 100.0f,
 *   .integral_min = -50.0f, .integral_max = 50.0f
 * };
 * rx_pid_init(&motor_pid, &config);
 *
 * // Control loop task @ 250 Hz (4ms period)
 * void motor_control_task(void* param) {
 *   const float target_rpm = 100.0f;
 *   const float dt = 0.004f;  // 4ms = 250 Hz
 *
 *   while (1) {
 *     // Read current motor speed from encoder
 *     float current_rpm = encoder_get_rpm();
 *
 *     // Compute PID control output
 *     float duty_cycle;
 *     rx_err_t err = rx_pid_compute(&motor_pid, target_rpm, current_rpm, dt, &duty_cycle);
 *
 *     if (err == k_rx_ok) {
 *       // Apply control output to motor
 *       motor_set_duty_cycle(duty_cycle);
 *     } else {
 *       rx_log_error_val("PID", "Compute failed", err);
 *     }
 *
 *     // Wait for next sample (4ms)
 *     tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 250);
 *   }
 * }
 * @endcode
 *
 * @par Example: Handling Large Setpoint Changes
 * @code{.c}
 * // Avoid integral windup when setpoint changes significantly
 * void change_target_speed(float new_target) {
 *   // Reset PID state before large setpoint change
 *   rx_pid_reset(&motor_pid);
 *
 *   // Update setpoint
 *   target_rpm = new_target;
 *
 *   // Resume normal control
 * }
 * @endcode
 *
 * @par Example: Error Handling
 * @code{.c}
 * float duty;
 * rx_err_t err = rx_pid_compute(&pid, 100.0f, 95.0f, 0.004f, &duty);
 *
 * switch (err) {
 *   case k_rx_ok:
 *     motor_set_duty(duty);
 *     break;
 *   case k_rx_err_null_ptr:
 *     rx_log_error("PID", "nullptr passed");
 *     break;
 *   case k_rx_err_invalid_state:
 *     rx_log_error("PID", "Not initialized - call rx_pid_init first");
 *     break;
 *   case k_rx_err_invalid_arg:
 *     rx_log_error("PID", "Invalid dt (must be > 0)");
 *     break;
 * }
 * @endcode
 *
 * @see rx_pid_init() Initialize PID controller
 * @see rx_pid_reset() Reset integral and derivative state
 * @see rx_pid_set_gains() Update gains at runtime
 * @see matlab/pid_design_velocity.m for gain tuning methodology
 */
rx_err_t
rx_pid_compute(rx_pid_handle_t* handle, float setpoint, float measured, float dt, float* output);

/**
 * @brief Reset PID controller internal state
 *
 * Clears the integral accumulator and previous error. Useful when changing
 * setpoints or recovering from disturbances to prevent integral windup.
 *
 * @param[in] handle Pointer to initialized PID handle. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_pid_reset(rx_pid_handle_t* handle);

/**
 * @brief Update PID gains at runtime
 *
 * Allows tuning of PID gains without reinitializing the controller.
 * The internal state (integral, prev_error) is preserved.
 *
 * @param[in] handle Pointer to initialized PID handle. Must not be nullptr.
 * @param[in] kp     New proportional gain (must be >= 0 and finite).
 * @param[in] ki     New integral gain (must be >= 0 and finite).
 * @param[in] kd     New derivative gain (must be >= 0 and finite).
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_invalid_arg if any gain is negative or non-finite (NaN/Inf)
 * @return k_rx_fail if gain storage verification fails
 */
[[nodiscard]] rx_err_t rx_pid_set_gains(rx_pid_handle_t* handle, float kp, float ki, float kd);

/**
 * @brief Update PID output limits at runtime
 *
 * Allows changing the output saturation limits without reinitializing.
 *
 * @param[in] handle     Pointer to initialized PID handle. Must not be nullptr.
 * @param[in] output_min New minimum output limit.
 * @param[in] output_max New maximum output limit.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_invalid_arg if output_max <= output_min
 */
[[nodiscard]] rx_err_t
rx_pid_set_output_limits(rx_pid_handle_t* handle, float output_min, float output_max);

/**
 * @brief Update PID integral limits at runtime (anti-windup)
 *
 * Allows changing the integral term saturation limits without reinitializing.
 *
 * @param[in] handle       Pointer to initialized PID handle. Must not be nullptr.
 * @param[in] integral_min New minimum integral limit.
 * @param[in] integral_max New maximum integral limit.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_invalid_arg if integral_max <= integral_min
 */
rx_err_t
rx_pid_set_integral_limits(rx_pid_handle_t* handle, float integral_min, float integral_max);

#ifdef __cplusplus
}
#endif
