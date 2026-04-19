/**
 * @file shared_data.h
 * @brief Thread-safe inter-task shared data for BeagleBone Blue firmware.
 *
 * @details
 * Provides mutex-protected structures for passing data between the firmware
 * tasks (comm, motor control, IMU, telemetry, watchdog). All access must go
 * through the provided getter/setter API -- never access fields directly.
 *
 * Architecture:
 * - One global bb_shared_data_t instance protected by a single pthread_mutex.
 * - Tasks lock the mutex, copy in or out, then unlock immediately.
 * - Holding time is O(memcpy) -- never hold while doing I/O or sleeping.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "bb_err.h"
#include "hardware_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Data structures
 * ---------------------------------------------------------------------------*/

/**
 * @enum bb_ctrl_mode_t
 * @brief Per-frame motor control dispatch mode.
 *
 * @details
 * Selects which field of bb_motor_cmd_t the motor_control_task will act
 * on for the most recent command. Default value (zero-initialized state
 * after bb_shared_data_init) is k_bb_ctrl_mode_velocity so a freshly
 * initialized robot with no command yet behaves as a stopped velocity
 * setpoint, not a stale duty.
 *
 * @since Version 1.2.0
 */
typedef enum : uint8_t {
    k_bb_ctrl_mode_velocity    = 0U, /**< Closed-loop PID on velocity_setpoint_mps */
    k_bb_ctrl_mode_direct_duty = 1U, /**< Bypass PID, drive duty_percent directly */
} bb_ctrl_mode_t;

/**
 * @struct bb_motor_cmd_t
 * @brief Motor command written by comm_task, read by motor_control_task.
 *
 * @details
 * Carries both a velocity setpoint (consumed by the PID closed loop) and
 * a raw duty cycle (consumed by the open-loop debug path used by
 * motor_power_command frames). The control_mode field selects which one
 * the motor task acts on; the unused field is ignored for that frame.
 *
 * Field semantics:
 * - velocity_setpoint_mps: signed wheel surface velocity in meters per
 *   second. Range is bounded only by the motor's no-load speed times the
 *   wheel radius (about +/-1.58 m/s for the goBILDA Wasteland 144mm
 *   wheel on a 6V 210 RPM motor). Setpoints beyond that will saturate
 *   the duty output but will not error.
 * - duty_percent: signed unit-fraction duty cycle in [-1.0, 1.0] where
 *   -1.0 is full reverse, 0.0 is stopped, and 1.0 is full forward. Used
 *   only when control_mode == k_bb_ctrl_mode_direct_duty.
 *
 * @invariant control_mode is one of the bb_ctrl_mode_t enum values
 *
 * @since Version 1.2.0
 */
typedef struct {
    bb_ctrl_mode_t control_mode;                       /**< Dispatch mode for this command */
    float velocity_setpoint_mps[k_bb_motor_count];     /**< Wheel velocity setpoint per motor (m/s) */
    float duty_percent[k_bb_motor_count];              /**< Direct duty cycle per motor [-1.0, 1.0] */
} bb_motor_cmd_t;

/**
 * @struct bb_encoder_data_t
 * @brief Encoder feedback written by motor_control_task, read by telemetry_task.
 *
 * @since Version 1.0.0
 */
typedef struct {
    int32_t ticks[k_bb_encoder_count]; /**< Raw encoder tick counts */
} bb_encoder_data_t;

/**
 * @struct bb_imu_data_t
 * @brief IMU data written by imu_task, read by telemetry_task.
 *
 * @details
 * All values in SI units: acceleration in m/s^2, gyro in rad/s,
 * magnetometer in uT.
 *
 * @since Version 1.0.0
 */
typedef struct {
    float accel_mps2[3]; /**< Accelerometer X, Y, Z (m/s^2) */
    float gyro_rps[3];   /**< Gyroscope X, Y, Z (rad/s) */
    float mag_ut[3];     /**< Magnetometer X, Y, Z (uT) */
    float temp_c;        /**< IMU die temperature (degrees C) */
} bb_imu_data_t;

/**
 * @struct bb_shared_data_t
 * @brief All inter-task shared state.
 *
 * @details
 * Protected by an internal pthread_mutex_t. Never access fields directly;
 * use bb_shared_data_set_motor_cmd() and bb_shared_data_get_*() API.
 *
 * @invariant mutex must be held whenever any field is read or written
 * @since Version 1.0.0
 */
typedef struct {
    pthread_mutex_t  mutex;       /**< Protects all fields below */
    bb_motor_cmd_t   motor_cmd;   /**< Latest motor command from gateway */
    bb_encoder_data_t encoder;    /**< Latest encoder tick counts */
    bb_imu_data_t    imu;         /**< Latest IMU reading */
    bool             estop;       /**< Emergency stop: true = all motors off */
    uint64_t         last_cmd_us; /**< Timestamp of last valid command (us) */
} bb_shared_data_t;

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * @brief Initialize the shared data store.
 *
 * @details
 * Zeros all fields and initializes the internal pthread_mutex_t.
 * Must be called from main() before spawning any task threads.
 *
 * @param[out] sd Pointer to the shared data instance to initialize
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Initialized successfully
 * @retval k_bb_err_null_ptr sd is NULL
 * @retval k_bb_err_hardware pthread_mutex_init() failed
 *
 * @pre sd must point to a valid bb_shared_data_t
 * @post All fields zeroed; mutex initialized
 *
 * @note Call once from main() before vTaskStartScheduler equivalent
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_init(bb_shared_data_t* sd);

/**
 * @brief Destroy the shared data store.
 *
 * @param[in,out] sd Pointer to the shared data instance
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Destroyed successfully
 * @retval k_bb_err_null_ptr sd is NULL
 *
 * @pre bb_shared_data_init() must have been called
 * @post mutex destroyed
 *
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_destroy(bb_shared_data_t* sd);

/**
 * @brief Write a motor command to shared data.
 *
 * @param[in,out] sd  Shared data instance
 * @param[in]     cmd Motor command to write
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Written successfully
 * @retval k_bb_err_null_ptr sd is NULL
 *
 * @pre bb_shared_data_init() must have been called
 * @post sd->motor_cmd updated; sd->last_cmd_us set to current time
 *
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_set_motor_cmd(bb_shared_data_t* sd,
                                      const bb_motor_cmd_t* cmd);

/**
 * @brief Read the current motor command from shared data.
 *
 * @param[in]  sd  Shared data instance
 * @param[out] cmd Destination for the motor command
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Read successfully
 * @retval k_bb_err_null_ptr sd or cmd is NULL
 *
 * @pre bb_shared_data_init() must have been called
 * @post *cmd contains a consistent snapshot of sd->motor_cmd
 *
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_get_motor_cmd(bb_shared_data_t* sd,
                                      bb_motor_cmd_t* cmd);

/**
 * @brief Write encoder data to shared data.
 *
 * @param[in,out] sd  Shared data instance
 * @param[in]     enc Encoder data to write
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Written successfully
 * @retval k_bb_err_null_ptr sd or enc is NULL
 *
 * @pre bb_shared_data_init() must have been called
 * @post sd->encoder updated
 *
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_set_encoder(bb_shared_data_t* sd,
                                    const bb_encoder_data_t* enc);

/**
 * @brief Read encoder data from shared data.
 *
 * @param[in]  sd  Shared data instance
 * @param[out] enc Destination for encoder data
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Read successfully
 * @retval k_bb_err_null_ptr sd or enc is NULL
 *
 * @pre bb_shared_data_init() must have been called
 * @post *enc contains a consistent snapshot of sd->encoder
 *
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_get_encoder(bb_shared_data_t* sd,
                                    bb_encoder_data_t* enc);

/**
 * @brief Write IMU data to shared data.
 *
 * @param[in,out] sd  Shared data instance
 * @param[in]     imu IMU data to write
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Written successfully
 * @retval k_bb_err_null_ptr sd or imu is NULL
 *
 * @pre bb_shared_data_init() must have been called
 * @post sd->imu updated
 *
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_set_imu(bb_shared_data_t* sd,
                                const bb_imu_data_t* imu);

/**
 * @brief Read IMU data from shared data.
 *
 * @param[in]  sd  Shared data instance
 * @param[out] imu Destination for IMU data
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Read successfully
 * @retval k_bb_err_null_ptr sd or imu is NULL
 *
 * @pre bb_shared_data_init() must have been called
 * @post *imu contains a consistent snapshot of sd->imu
 *
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_get_imu(bb_shared_data_t* sd,
                                bb_imu_data_t* imu);

/**
 * @brief Set or clear the emergency stop flag.
 *
 * @param[in,out] sd    Shared data instance
 * @param[in]     estop true to engage estop, false to clear
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Written successfully
 * @retval k_bb_err_null_ptr sd is NULL
 *
 * @pre bb_shared_data_init() must have been called
 * @post sd->estop updated
 *
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_set_estop(bb_shared_data_t* sd, bool estop);

/**
 * @brief Read the emergency stop flag.
 *
 * @param[in]  sd    Shared data instance
 * @param[out] estop Destination for the estop flag
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Read successfully
 * @retval k_bb_err_null_ptr sd or estop is NULL
 *
 * @pre bb_shared_data_init() must have been called
 * @post *estop reflects the current estop state
 *
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_get_estop(bb_shared_data_t* sd, bool* estop);

/**
 * @brief Read the timestamp of the last valid motor command.
 *
 * @param[in]  sd       Shared data instance
 * @param[out] last_us  Microseconds since epoch of last valid command
 *
 * @return bb_err_t
 * @retval k_bb_err_ok       Read successfully
 * @retval k_bb_err_null_ptr sd or last_us is NULL
 *
 * @pre bb_shared_data_init() must have been called
 * @post *last_us set to sd->last_cmd_us
 *
 * @since Version 1.0.0
 */
bb_err_t bb_shared_data_get_last_cmd_us(bb_shared_data_t* sd,
                                        uint64_t* last_us);

#ifdef __cplusplus
}
#endif
