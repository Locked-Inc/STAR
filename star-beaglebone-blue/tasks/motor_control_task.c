/**
 * @file motor_control_task.c
 * @brief Motor control task -- drives motors and reads encoders.
 *
 * @details
 * 100 Hz loop that:
 * 1. Checks estop flag -- if set, commands all motors to zero
 * 2. Reads motor_cmd from shared_data and applies duty cycles via rc_motor_set()
 * 3. Reads quadrature encoder counts via rc_encoder_read() and writes to shared_data
 *
 * Initial implementation uses direct duty mode (motor_cmd.duty_percent maps
 * directly to rc_motor_set duty parameter). PID velocity control can be added
 * once encoder-to-velocity conversion is calibrated on hardware.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#include "motor_control_task.h"
#include "hardware_config.h"

#include <robotcontrol.h>
#include <time.h>

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

/* Motor indices from hardware_config.h: k_bb_motor_idx_fl/fr/rl/rr */

/**
 * @brief motor_control_task thread entry point.
 *
 * @details
 * 100 Hz control loop. Reads motor commands from shared_data and applies
 * them to the DRV8838 H-bridge drivers via librobotcontrol. Reads encoder
 * feedback and publishes to shared_data for telemetry.
 *
 * @param[in] arg Pointer to bb_shared_data_t
 *
 * @return void* Always NULL
 *
 * @pre rc_motor_init() and rc_encoder_eqep_init() must have been called
 * @pre arg must point to initialized bb_shared_data_t
 * @post All motors disabled on task exit (safety)
 *
 * @since Version 1.0.0
 */
void* bb_motor_control_task(void* arg)
{
    if (arg == NULL) {
        return NULL;
    }

    bb_shared_data_t* sd = (bb_shared_data_t*)arg;

    struct timespec next = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &next);

    while (rc_get_state() != EXITING) {
        const float max_duty = internal_get_max_duty();

        bool estop = false;
        (void)bb_shared_data_get_estop(sd, &estop);

        if (estop) {
            /* Emergency stop -- all motors to zero */
            (void)rc_motor_set(k_bb_motor_front_left, 0.0);
            (void)rc_motor_set(k_bb_motor_front_right, 0.0);
            (void)rc_motor_set(k_bb_motor_rear_left, 0.0);
            (void)rc_motor_set(k_bb_motor_rear_right, 0.0);
        } else {
            /* Apply motor commands with voltage-based duty clamp */
            bb_motor_cmd_t cmd = {0};
            (void)bb_shared_data_get_motor_cmd(sd, &cmd);

            for (uint8_t i = 0U; i < (uint8_t)k_bb_motor_count; i++) {
                float d = cmd.duty_percent[i];
                if (d > max_duty)  { d = max_duty; }
                if (d < -max_duty) { d = -max_duty; }
                cmd.duty_percent[i] = d;
            }

            (void)rc_motor_set(k_bb_motor_front_left,
                               (double)cmd.duty_percent[k_bb_motor_idx_fl]);
            (void)rc_motor_set(k_bb_motor_front_right,
                               (double)cmd.duty_percent[k_bb_motor_idx_fr]);
            (void)rc_motor_set(k_bb_motor_rear_left,
                               (double)cmd.duty_percent[k_bb_motor_idx_rl]);
            (void)rc_motor_set(k_bb_motor_rear_right,
                               (double)cmd.duty_percent[k_bb_motor_idx_rr]);
        }

        /* Read encoder feedback (eQEP channels 1-3 only).
         * Channel 4 (rear-right) uses the PRU encoder which is not available
         * on the 5.10-ti kernel. As a degraded-mode workaround, mirror the
         * front-right tick count onto rear-right: in skid-steer both right-
         * side wheels are commanded together and slip-couple through the
         * chassis, so FR is the closest available proxy. Without this, the
         * gateway's right-side average becomes (fr + 0)/2 = fr/2 and pose
         * drifts rightward on straight-line motion. */
        bb_encoder_data_t enc = {0};
        enc.ticks[k_bb_motor_idx_fl] = rc_encoder_read(k_bb_encoder_front_left);
        enc.ticks[k_bb_motor_idx_fr] = rc_encoder_read(k_bb_encoder_front_right);
        enc.ticks[k_bb_motor_idx_rl] = rc_encoder_read(k_bb_encoder_rear_left);
        enc.ticks[k_bb_motor_idx_rr] = enc.ticks[k_bb_motor_idx_fr];

        (void)bb_shared_data_set_encoder(sd, &enc);

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
