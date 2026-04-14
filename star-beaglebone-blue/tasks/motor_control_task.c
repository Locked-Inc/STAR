/**
 * @file motor_control_task.c
 * @brief Motor control task -- closed-loop velocity PID + open-loop debug bypass.
 *
 * @details
 * 100 Hz control loop that:
 * 1. Reads quadrature encoder counts and computes per-motor angular velocity
 *    on the gearbox output shaft (rad/s) from tick deltas across the loop dt.
 * 2. Reads the latest motor command (with control mode) from shared_data.
 * 3. If estop is asserted: zeroes all motors and resets PID integrals to
 *    prevent windup spike on resume.
 * 4. If control_mode == k_bb_ctrl_mode_velocity: converts the wheel m/s
 *    setpoint to motor output-shaft rad/s using the wheel radius, runs
 *    per-motor bb_pid_compute(), and applies the resulting duty.
 * 5. If control_mode == k_bb_ctrl_mode_direct_duty: bypasses the PID and
 *    drives the duty cycle directly (used by motor_power_command for
 *    manual debugging from the gateway).
 * 6. Voltage-derates the final duty via internal_get_max_duty() and pushes
 *    it to rc_motor_set().
 * 7. Publishes the encoder ticks to shared_data for telemetry_task.
 *
 * Per-motor PID gains and saturation limits live in bb_pid_config.h.
 *
 * The rear-right motor encoder is unavailable on the 5.10-ti kernel
 * (ch4 PRU encoder is unsupported), so its tick count is mirrored from
 * the front-right encoder as a degraded-mode skid-steer proxy. The PID
 * loop runs on all four motors uniformly; the rear-right loop simply
 * tracks the front-right loop because they share the same input ticks.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.2.0
 */

#include "motor_control_task.h"
#include "bb_pid.h"
#include "bb_pid_config.h"
#include "hardware_config.h"

#include <assert.h>
#include <math.h>
#include <robotcontrol.h>
#include <time.h>

/* ---------------------------------------------------------------------------
 * Numeric constants (no magic numbers)
 * ---------------------------------------------------------------------------*/

/** Multiplier for full revolution in radians (2 * pi). */
static const float s_bb_two_pi = 2.0F * (float)M_PI;

/** Minimum loop dt floor to keep PID divisions safe (1 microsecond in s). */
static const float s_bb_min_dt_s = 1.0e-6F;

/* ---------------------------------------------------------------------------
 * Battery voltage sampling (timer-based, 10 Hz)
 * ---------------------------------------------------------------------------*/

/** Minimum interval between ADC reads in nanoseconds (100 ms = 10 Hz). */
enum : uint64_t { k_adc_interval_ns = 100000000ULL };

/**
 * @brief Compute the voltage-proportional max duty cycle.
 *
 * Reads rc_adc_batt() at most once per k_adc_interval_ns using
 * CLOCK_MONOTONIC for timing (immune to wall-clock adjustments).
 * Returns a cached value between calls. First call always reads ADC
 * (s_last_read initializes to zero, so elapsed is always large).
 *
 * Protects 6V motors from overvoltage on 2S LiPo by clamping:
 * max_duty = 0.95 * (6.0 / V_batt).
 *
 * @return float Max allowable abs(duty), in [0, 1].
 *
 * @warning NOT thread-safe. Must be called from the motor control
 *          thread only (single-caller assumption).
 * @since Version 1.1.0
 */
static float internal_get_max_duty(void)
{
    static float s_max_duty = s_bb_duty_fallback_max;
    static struct timespec s_last_read = {};

    struct timespec now = {};
    (void)clock_gettime(CLOCK_MONOTONIC, &now);

    /* Signed arithmetic for correct elapsed time when tv_nsec wraps.
     * Example: now={10, 0.1s} last={9, 0.9s} -> elapsed = 0.2s.
     * Using unsigned would underflow (0.1 - 0.9 wraps to ~18e18). */
    int64_t elapsed_ns = (int64_t)(now.tv_sec - s_last_read.tv_sec)
                       * (int64_t)k_bb_ns_per_sec
                       + (int64_t)(now.tv_nsec - s_last_read.tv_nsec);

    if (elapsed_ns >= (int64_t)k_adc_interval_ns) {
        s_last_read = now;

        double vbatt = rc_adc_batt();
        if (vbatt > (double)s_bb_batt_deadzone_v) {
            s_max_duty = s_bb_duty_derating
                       * (s_bb_motor_rated_v / (float)vbatt);
            if (s_max_duty > 1.0F) {
                s_max_duty = 1.0F;
            }
        } else {
            s_max_duty = s_bb_duty_fallback_max;
        }
    }

    return s_max_duty;
}

/* ---------------------------------------------------------------------------
 * PID instances and per-loop state
 * ---------------------------------------------------------------------------*/

/** Per-motor PID handles. Initialized once at task entry by internal_init_pids(). */
static bb_pid_handle_t s_pid[k_bb_motor_count];

/** Previous-iteration encoder tick counts (for finite-difference velocity). */
static int32_t s_prev_ticks[k_bb_motor_count] = {0};

/** Previous-iteration loop wakeup time (for finite-difference dt). */
static struct timespec s_prev_loop_time = {0};

/** First-iteration sentinel: skip velocity computation until prev_ticks are seeded. */
static bool s_loop_first_iter = true;

/* Motor indices from hardware_config.h: k_bb_motor_idx_fl/fr/rl/rr */

/**
 * @brief Initialize all four PID instances with the seed gains from bb_pid_config.h.
 *
 * @details
 * Builds a single bb_pid_config_t from the named seed constants and applies
 * it to each per-motor handle via bb_pid_init(). The runtime state
 * (integral, prev_error) is cleared to zero by bb_pid_init() implicitly.
 *
 * @return bb_err_t Status code.
 * @retval k_bb_err_ok            All four PID handles initialized successfully.
 * @retval k_bb_err_null_ptr      bb_pid_init() reported a null handle (impossible
 *                                here -- s_pid[] is file-static).
 * @retval k_bb_err_invalid_arg   bb_pid_init() rejected the limits (would only
 *                                fire if bb_pid_config.h is mis-edited so
 *                                output_max <= output_min).
 *
 * @pre s_pid[] handles are uninitialized (handle->initialized == false)
 * @pre bb_pid_config.h provides valid output_min < output_max and
 *      integral_min < integral_max
 * @post On success, all four s_pid[] handles are usable by bb_pid_compute()
 * @post On failure, any partially-initialized handles remain in their last state
 *
 * @note Not thread-safe; call from the motor control thread before the main
 *       loop spins up.
 *
 * @since Version 1.2.0
 */
static bb_err_t internal_init_pids(void)
{
    /* Pre-conditions: gain limits in bb_pid_config.h are well-formed */
    assert(s_bb_pid_output_max > s_bb_pid_output_min);
    assert(s_bb_pid_integral_max > s_bb_pid_integral_min);

    const bb_pid_config_t cfg = {
        .kp           = s_bb_pid_velocity_kp,
        .ki           = s_bb_pid_velocity_ki,
        .kd           = s_bb_pid_velocity_kd,
        .output_min   = s_bb_pid_output_min,
        .output_max   = s_bb_pid_output_max,
        .integral_min = s_bb_pid_integral_min,
        .integral_max = s_bb_pid_integral_max,
    };

    for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
        bb_err_t err = bb_pid_init(&s_pid[i], &cfg);
        if (err != k_bb_err_ok) {
            return err;
        }
    }

    /* Post-condition: all handles initialized */
    assert(s_pid[k_bb_motor_idx_fl].initialized);
    assert(s_pid[k_bb_motor_idx_rr].initialized);

    return k_bb_err_ok;
}

/**
 * @brief Compute monotonic time delta in seconds between two CLOCK_MONOTONIC samples.
 *
 * @details
 * Uses signed nanosecond arithmetic so a tv_nsec wrap across the second
 * boundary produces the correct positive elapsed time (same pattern as
 * internal_get_max_duty above). Clamps the floor to 1 microsecond so PID
 * divisions stay safe even if the loop is called twice within the same
 * scheduler tick.
 *
 * @param[in] now      Current timestamp (must not be NULL).
 * @param[in] previous Previous timestamp (must not be NULL, must be earlier).
 *
 * @return float Elapsed seconds, always >= 1e-6.
 *
 * @pre now and previous are non-null and acquired via CLOCK_MONOTONIC.
 * @pre now >= previous in real time
 * @post Return value is strictly positive
 * @post Return value is finite
 *
 * @note Pure function; thread-safe.
 *
 * @since Version 1.2.0
 */
static float internal_dt_seconds(const struct timespec* now,
                                 const struct timespec* previous)
{
    /* Pre-conditions: non-null timestamps */
    assert(now != NULL);
    assert(previous != NULL);

    const int64_t elapsed_ns = (int64_t)(now->tv_sec - previous->tv_sec)
                             * (int64_t)k_bb_ns_per_sec
                             + (int64_t)(now->tv_nsec - previous->tv_nsec);

    float dt = (float)elapsed_ns / (float)k_bb_ns_per_sec;
    if (dt < s_bb_min_dt_s) {
        dt = s_bb_min_dt_s;
    }

    /* Post-conditions: positive and finite */
    assert(dt >= s_bb_min_dt_s);
    assert(isfinite(dt));

    return dt;
}

/**
 * @brief Saturate a duty fraction to the voltage-derated motor envelope.
 *
 * @param[in] duty_frac Unclamped duty fraction (any real value).
 * @param[in] max_duty  Voltage-derated max abs(duty) from internal_get_max_duty().
 *
 * @return float Clamped value in [-max_duty, +max_duty].
 *
 * @pre max_duty >= 0
 * @post |return value| <= max_duty
 *
 * @note Pure function; thread-safe.
 *
 * @since Version 1.2.0
 */
static float internal_saturate_duty(const float duty_frac, const float max_duty)
{
    /* Pre-conditions: max_duty non-negative, duty_frac is a finite float */
    assert(max_duty >= 0.0F);
    assert(isfinite(duty_frac));

    float result = duty_frac;
    if (result >  max_duty) { result =  max_duty; }
    if (result < -max_duty) { result = -max_duty; }

    /* Post-condition: result is within +/- max_duty */
    assert((result >= -max_duty) && (result <= max_duty));

    return result;
}

/* ---------------------------------------------------------------------------
 * Task entry point
 * ---------------------------------------------------------------------------*/

/**
 * @brief motor_control_task thread entry point.
 *
 * @details
 * 100 Hz control loop. Reads encoder counts, computes per-motor measured
 * angular velocity, dispatches to either the velocity PID or the direct-
 * duty bypass based on control_mode, voltage-derates the result, and
 * pushes it to the DRV8838 H-bridge drivers via librobotcontrol.
 *
 * On the first iteration the prev_ticks/prev_loop_time state is unseeded,
 * so omega_measured is taken to be zero for that single tick; this avoids
 * a spurious omega computation against the zero-initialized prev_ticks
 * which would otherwise spike the PID if the encoders had non-zero counts
 * at task start.
 *
 * @param[in] arg Pointer to bb_shared_data_t (non-null).
 *
 * @return void* Always NULL.
 *
 * @pre rc_motor_init() and rc_encoder_eqep_init() must have been called
 * @pre arg must point to an initialized bb_shared_data_t
 * @post All motors disabled on task exit (safety)
 * @post Encoder ticks published to shared_data each loop iteration
 *
 * @warning Not thread-safe. Spawn at most one instance of this task.
 * @note Owns exclusive access to the librobotcontrol motor and encoder APIs.
 *
 * @since Version 1.0.0
 */
void* bb_motor_control_task(void* arg)
{
    if (arg == NULL) {
        return NULL;
    }

    bb_shared_data_t* sd = (bb_shared_data_t*)arg;

    if (internal_init_pids() != k_bb_err_ok) {
        return NULL;
    }

    /* Tick-to-rad conversion factor for output-shaft angular velocity.
     * Same constant used by telemetry_task for its m/s computation. */
    const float rad_per_tick = s_bb_two_pi / (float)k_bb_ticks_per_rev;

    struct timespec next = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &next);

    while (rc_get_state() != EXITING) {
        struct timespec loop_now = {0};
        (void)clock_gettime(CLOCK_MONOTONIC, &loop_now);

        const float max_duty = internal_get_max_duty();

        /* Read encoder feedback first so the PID consumes the freshest
         * measurement. eQEP channels 1-3 only -- channel 4 (rear-right)
         * uses the PRU encoder which is not supported on the 5.10-ti
         * kernel, so we mirror the front-right tick count onto rear-right
         * as a degraded-mode skid-steer proxy. The rear-right PID then
         * tracks the front-right PID because they share the same input. */
        bb_encoder_data_t enc = {0};
        enc.ticks[k_bb_motor_idx_fl] = rc_encoder_read(k_bb_encoder_front_left);
        enc.ticks[k_bb_motor_idx_fr] = rc_encoder_read(k_bb_encoder_front_right);
        enc.ticks[k_bb_motor_idx_rl] = rc_encoder_read(k_bb_encoder_rear_left);
        enc.ticks[k_bb_motor_idx_rr] = enc.ticks[k_bb_motor_idx_fr];

        /* Compute per-motor measured angular velocity (rad/s on the gearbox
         * output shaft). On the very first iteration prev_ticks is the
         * zero-initialized static, which would yield a bogus huge delta if
         * the encoders had already accumulated counts at task start. Skip
         * the velocity computation on iteration 0; the PID then runs with
         * measured = 0 for that single tick, which only contributes a
         * proportional term on the order of Kp*setpoint and no integral
         * surprise. */
        float omega_measured[k_bb_motor_count] = {0.0F, 0.0F, 0.0F, 0.0F};
        float dt_s = (float)k_bb_motor_period_ns / (float)k_bb_ns_per_sec;
        if (!s_loop_first_iter) {
            dt_s = internal_dt_seconds(&loop_now, &s_prev_loop_time);
            for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
                const int32_t d_ticks = enc.ticks[i] - s_prev_ticks[i];
                omega_measured[i] = ((float)d_ticks * rad_per_tick) / dt_s;
            }
        }

        bool estop = false;
        (void)bb_shared_data_get_estop(sd, &estop);

        bb_motor_cmd_t cmd = {0};
        (void)bb_shared_data_get_motor_cmd(sd, &cmd);

        float duty_out[k_bb_motor_count] = {0.0F, 0.0F, 0.0F, 0.0F};

        if (estop) {
            /* Emergency stop -- zero all motors and reset PID integrals
             * so the next un-stopped command starts from a clean slate. */
            for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
                (void)bb_pid_reset(&s_pid[i]);
            }
            /* duty_out stays at zero */
        } else if (cmd.control_mode == k_bb_ctrl_mode_velocity) {
            /* Closed-loop velocity dispatch. Convert m/s setpoint to
             * output-shaft rad/s via wheel radius (1:1 miter gear means
             * output-shaft rad/s * radius == wheel surface m/s exactly),
             * then run the per-motor PID. */
            for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
                /* Rear-right's encoder feedback is mirrored from front-
                 * right above (PRU encoder ch4 is unsupported on 5.10-ti).
                 * Mirror its SETPOINT too: if the gateway sends a non-
                 * symmetric right-side command, an independent RR PID
                 * would chase its own target while its feedback reports
                 * what FR is doing, causing the integral term to diverge
                 * and drive the right side against itself. With both
                 * input and feedback mirrored, the RR PID tracks the
                 * FR PID exactly. */
                float setpoint_mps = cmd.velocity_setpoint_mps[i];
                if (i == (uint8_t)k_bb_motor_idx_rr) {
                    setpoint_mps =
                        cmd.velocity_setpoint_mps[k_bb_motor_idx_fr];
                }
                const float omega_setpoint =
                    setpoint_mps / s_bb_wheel_radius_m;

                float duty_pct = 0.0F;
                bb_err_t err = bb_pid_compute(&s_pid[i],
                                              omega_setpoint,
                                              omega_measured[i],
                                              dt_s,
                                              &duty_pct);
                if (err == k_bb_err_ok) {
                    /* PID output is in percent (-100..+100); convert to
                     * unit fraction for rc_motor_set(). */
                    duty_out[i] = duty_pct / s_bb_duty_percent_scale;
                }
            }
        } else if (cmd.control_mode == k_bb_ctrl_mode_direct_duty) {
            /* Direct-duty debug path: pass the duty values through with
             * no PID involvement. Used by motor_power_command for manual
             * bench testing. */
            for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
                duty_out[i] = cmd.duty_percent[i];
            }
        } else {
            /* Unknown control mode -- fail closed. duty_out stays at
             * zero, and PID integrals are reset so the next valid
             * command starts from a clean slate. This catches enum
             * corruption from shared memory and any future enum
             * additions that forget to update this dispatch. */
            for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
                (void)bb_pid_reset(&s_pid[i]);
            }
        }

        /* Voltage-derated saturation, then push to motors. */
        for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
            duty_out[i] = internal_saturate_duty(duty_out[i], max_duty);
        }

        (void)rc_motor_set(k_bb_motor_front_left,
                           (double)duty_out[k_bb_motor_idx_fl]);
        (void)rc_motor_set(k_bb_motor_front_right,
                           (double)duty_out[k_bb_motor_idx_fr]);
        (void)rc_motor_set(k_bb_motor_rear_left,
                           (double)duty_out[k_bb_motor_idx_rl]);
        (void)rc_motor_set(k_bb_motor_rear_right,
                           (double)duty_out[k_bb_motor_idx_rr]);

        (void)bb_shared_data_set_encoder(sd, &enc);

        /* Save state for next iteration */
        for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
            s_prev_ticks[i] = enc.ticks[i];
        }
        s_prev_loop_time  = loop_now;
        s_loop_first_iter = false;

        next.tv_nsec += (long)k_bb_motor_period_ns;
        if (next.tv_nsec >= (long)k_bb_ns_per_sec) {
            next.tv_sec++;
            next.tv_nsec -= (long)k_bb_ns_per_sec;
        }
        (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    /* Safety: disable all motors on exit */
    (void)rc_motor_set(k_bb_motor_all, 0.0);

    return NULL;
}
