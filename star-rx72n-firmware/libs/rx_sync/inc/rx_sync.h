/**
 * @file rx_sync.h
 * @brief Multi-Motor Velocity Synchronization Layer
 *
 * @details
 * Implements minimum-velocity tracking with EMA filtering for 4-motor
 * synchronization.  When one motor degrades, all healthy motors reduce
 * their effective setpoint to match the slowest, keeping the robot's
 * kinematics coherent.
 *
 * **Synchronization strategy:**
 * Each control cycle:
 * 1. Apply per-motor EMA filter to measured velocities (noise suppression).
 * 2. Compute v_sync = min(v_ema[i]) over non-faulted motors.
 * 3. Each motor's effective setpoint = min(v_commanded, v_sync).
 *
 * **Fault exclusion:**
 * If a motor velocity stays below (k_sync_fault_thresh_frac * v_commanded)
 * for longer than k_sync_fault_ticks samples, it is marked faulted and
 * excluded from the minimum.  This prevents a dead/disconnected motor from
 * stopping the entire robot.
 *
 * **Designed for 250 Hz (Ts = 4 ms) control loop.**
 *
 * **Simulation-validated gains** (see matlab/sync_pid_sim.m):
 *   - EMA alpha = 0.30 (3 Hz cutoff at 250 Hz)
 *   - Fault timeout = 500 ms = 125 ticks
 *   - Steady-state sync error < 5 RPM for motor degradation 20-80%
 *
 * @note Not thread-safe.  Call rx_sync_update() and rx_sync_get_setpoint()
 *       from the same ThreadX task.
 *
 * @see rx_pid.h  Individual motor PID controller (upstream consumer)
 * @see matlab/sync_pid_sim.m  Simulation that validated these parameters
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

/** @brief Number of motors managed by the sync layer. */
typedef enum : uint8_t {
    k_sync_num_motors = 4U,
} sync_motor_count_t;

/**
 * @brief Sync layer tuning parameters.
 *
 * @details
 * These constants are derived from matlab/sync_pid_sim.m and
 * matlab/sync_robustness_sweep.m.  Do not change without re-running the
 * simulations.
 */
typedef enum : uint32_t {
    /**
     * @brief EMA filter coefficient scaled to Q16.16 fixed-point.
     *
     * @details
     * alpha = 0.30.  Scaled: round(0.30 * 65536) = 19661.
     * Cutoff frequency at 250 Hz: f_c = alpha / (2*pi*Ts) ~= 12 Hz
     * (approximation for small alpha).
     */
    k_sync_ema_alpha_q16 = 19661U,

    /**
     * @brief Fault detection threshold, fraction of commanded speed (Q16.16).
     *
     * @details
     * If v_ema[i] < (threshold_frac * v_commanded) for fault_ticks samples,
     * motor i is declared faulted and excluded from v_sync.
     * Value: 0.05 * 65536 = 3277.
     */
    k_sync_fault_thresh_q16 = 3277U,

    /**
     * @brief Number of consecutive ticks below threshold before fault declared.
     *
     * @details
     * At 250 Hz: 125 ticks = 500 ms.
     */
    k_sync_fault_ticks = 125U,
} sync_params_t;

/**
 * @struct rx_sync_config_t
 * @brief Configuration for the synchronization layer.
 *
 * @details
 * All fields are validated in rx_sync_init().  Use the default-initializer
 * macro rx_sync_config_default() for production parameters that match the
 * MATLAB simulation.
 *
 * @see rx_sync_init()
 * @since Version 1.0.0
 */
typedef struct {
    float ema_alpha;          /**< EMA filter coefficient [0, 1). Default: 0.30f */
    float fault_thresh_frac;  /**< Fault threshold as fraction of commanded vel. Default: 0.05f */
    uint32_t fault_ticks;     /**< Ticks below threshold before fault declared. Default: 125 */
} rx_sync_config_t;

/**
 * @brief Initialize rx_sync_config_t with simulation-validated defaults.
 *
 * @code
 * rx_sync_config_t cfg = RX_SYNC_CONFIG_DEFAULT();
 * rx_sync_init(&h, &cfg);
 * @endcode
 */
#define RX_SYNC_CONFIG_DEFAULT() \
    ((rx_sync_config_t){ \
        .ema_alpha         = 0.30f, \
        .fault_thresh_frac = 0.05f, \
        .fault_ticks       = 125U,  \
    })

/**
 * @struct rx_sync_handle_t
 * @brief Runtime state for the synchronization layer.
 *
 * @details
 * Opaque handle.  Callers must not access fields directly.
 * Initialize with rx_sync_init() before use.
 *
 * @invariant v_ema[i] >= 0 for all i
 * @invariant fault_timer[i] <= config.fault_ticks + 1 for all i
 *
 * @since Version 1.0.0
 */
typedef struct {
    rx_sync_config_t config;                    /**< Tuning parameters */
    float            v_ema[k_sync_num_motors];  /**< EMA-filtered velocities [rad/s] */
    float            v_sync;                    /**< Current sync reference [rad/s] */
    uint32_t         fault_timer[k_sync_num_motors]; /**< Consecutive ticks below threshold */
    bool             faulted[k_sync_num_motors]; /**< True if motor is excluded from sync */
    bool             initialized;               /**< Initialized flag */
} rx_sync_handle_t;

/**
 * @brief Initialize the synchronization layer.
 *
 * @details
 * Validates configuration and zeroes all runtime state.  Must be called
 * before rx_sync_update() or rx_sync_get_setpoint().
 *
 * @param[out] h    Sync handle to initialize. Must not be nullptr.
 * @param[in]  cfg  Configuration. Must not be nullptr. Fields validated:
 *                    - ema_alpha in (0, 1)
 *                    - fault_thresh_frac in (0, 1)
 *                    - fault_ticks >= 1
 *
 * @return rx_err_t
 * @retval k_rx_ok                 Success.
 * @retval k_rx_err_null_ptr       h or cfg is nullptr.
 * @retval k_rx_err_invalid_arg    ema_alpha or fault_thresh_frac out of range.
 *
 * @pre h must point to a valid rx_sync_handle_t (may be uninitialized).
 * @pre cfg->ema_alpha must be in (0.0f, 1.0f).
 *
 * @post h->initialized == true on success.
 * @post All v_ema[i] == 0.0f and faulted[i] == false on success.
 *
 * @note Not thread-safe during initialization.
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_sync_init(rx_sync_handle_t *h, const rx_sync_config_t *cfg);

/**
 * @brief Update sync state with new velocity measurements.
 *
 * @details
 * Call once per control cycle (250 Hz) before rx_sync_get_setpoint().
 * Updates EMA filters, fault detection, and v_sync for all motors.
 *
 * @param[in,out] h           Initialized sync handle.
 * @param[in]     v_measured  Array of k_sync_num_motors measured velocities [rad/s].
 * @param[in]     v_commanded Commanded velocity for fault threshold reference [rad/s].
 *                            Must be >= 0.
 * @param[in]     dt_s        Control loop period in seconds. Must be > 0.
 *
 * @return rx_err_t
 * @retval k_rx_ok                 Success.
 * @retval k_rx_err_null_ptr       h or v_measured is nullptr.
 * @retval k_rx_err_not_initialized h not initialized.
 * @retval k_rx_err_invalid_arg    dt_s <= 0 or v_commanded < 0.
 *
 * @pre h must be initialized via rx_sync_init().
 * @pre v_commanded >= 0.0f.
 *
 * @post h->v_sync updated to min of non-faulted EMA velocities.
 * @post h->faulted[i] updated for each motor.
 *
 * @note Not thread-safe. Call from the same task as rx_sync_get_setpoint().
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_sync_update(rx_sync_handle_t *h,
                                      const float       v_measured[k_sync_num_motors],
                                      float             v_commanded,
                                      float             dt_s);

/**
 * @brief Get the effective setpoint for one motor after sync adjustment.
 *
 * @details
 * Returns min(v_commanded, v_sync), where v_sync was computed by the most
 * recent rx_sync_update() call.  Pass this as the setpoint to rx_pid_compute().
 *
 * @param[in] h           Initialized sync handle.
 * @param[in] motor_id    Motor index [0, k_sync_num_motors).
 * @param[in] v_commanded External commanded velocity [rad/s]. Must be >= 0.
 *
 * @return Effective setpoint in [rad/s], or 0.0f on error (null pointer /
 *         out-of-range motor_id / uninitialized).
 *
 * @pre h must be initialized via rx_sync_init().
 * @pre motor_id < k_sync_num_motors.
 * @pre rx_sync_update() must have been called this control cycle.
 *
 * @post Return value <= v_commanded.
 * @post Return value >= 0.0f.
 *
 * @note Not thread-safe.
 *
 * @since Version 1.0.0
 */
[[nodiscard]] float rx_sync_get_setpoint(const rx_sync_handle_t *h,
                                         uint8_t                 motor_id,
                                         float                   v_commanded);

/**
 * @brief Reset sync state (clear EMA, faults, v_sync).
 *
 * @details
 * Call when the robot stops or changes direction to avoid stale EMA values
 * pulling the sync reference to zero at the start of the next run.
 *
 * @param[in,out] h  Initialized sync handle.
 *
 * @return rx_err_t
 * @retval k_rx_ok               Success.
 * @retval k_rx_err_null_ptr     h is nullptr.
 * @retval k_rx_err_not_initialized h not initialized.
 *
 * @pre h must be initialized.
 * @post v_ema[i] == 0, fault_timer[i] == 0, faulted[i] == false for all i.
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_sync_reset(rx_sync_handle_t *h);
