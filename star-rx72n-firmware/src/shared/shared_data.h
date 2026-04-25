/**
 * @file shared_data.h
 * @brief Thread-Safe Shared Data Infrastructure for Multi-Task Communication
 *
 * @details
 * Declares all shared data types and accessor functions for inter-task
 * communication in the STAR firmware. All access is mutex-protected.
 *
 * @see shared_data.c Implementation
 
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
*/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "tx_api.h"

/***********************************************************************************************************************
 * Forward Declarations and Type Definitions
 ***********************************************************************************************************************/

/**
 * @enum shared_data_counts_t
 * @brief Hardware count constants for array sizing in shared data structures
 *
 * @details
 * Provides named constants for the number of motors, temperature sensors,
 * and obstacle sensors in the STAR platform hardware configuration.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_shared_motor_count           = 4U, /**< Number of brushed DC gearmotors (one per wheel) */
  k_shared_temp_sensor_count     = 4U, /**< Number of DS18B20 temperature sensors */
  k_shared_obstacle_sensor_count = 4U, /**< Number of HC-SR04 ultrasonic obstacle sensors */
} shared_data_counts_t;

/**
 * @brief Emergency stop reason codes
 */
typedef enum : uint8_t {
  k_estop_reason_none           = 0, /**< No e-stop active */
  k_estop_reason_comm_timeout   = 1, /**< Communication timeout */
  k_estop_reason_obstacle       = 2, /**< Obstacle too close */
  k_estop_reason_driver_fault   = 3, /**< Motor driver hardware fault */
  k_estop_reason_overcurrent    = 4, /**< Motor overcurrent */
  k_estop_reason_manual         = 5, /**< Manual request */
  k_estop_reason_sensor_failure = 6, /**< ADC or sensor read failure */

} estop_reason_t;

/**
 * @brief Motor control mode
 */
typedef enum : uint8_t {
  k_motor_mode_idle     = 0, /**< Motors idle, no control active */
  k_motor_mode_velocity = 1, /**< Velocity control mode */
  k_motor_mode_estop    = 2, /**< Emergency stop mode */
} motor_mode_t;

/**
 * @brief Motor velocity command from RPi5
 */
typedef struct {
  float    target_velocity_mps[k_shared_motor_count]; /**< Target velocity for each motor (m/s) */
  uint32_t sequence;                                  /**< Command sequence number */
  uint32_t timestamp_ms; /**< ThreadX tick when command received (ms) */
  bool     valid;        /**< true if command is valid, false if not set yet */
} motor_command_t;

/**
 * @brief Motor state telemetry
 */
typedef struct {
  float   current_velocity_mps[k_shared_motor_count]; /**< Current velocity for each motor (m/s) */
  float   current_ma[k_shared_motor_count];           /**< Current for each motor (mA) */
  int32_t encoder_counts[k_shared_motor_count];       /**< Encoder counts for each motor (signed) */
  uint8_t fault_flags[k_shared_motor_count];          /**< Fault flags for each motor */
  float   duty_cycle_percent[k_shared_motor_count];   /**< PWM duty cycle for each motor (%) */
  bool    estop_active;                               /**< Emergency stop active flag */
  estop_reason_t estop_reason;                        /**< Emergency stop reason code */
  motor_mode_t   mode;                                /**< Current motor control mode */
} motor_state_t;

/**
 * @brief PID controller gains
 */
typedef struct {
  float kp;             /**< Proportional gain */
  float ki;             /**< Integral gain */
  float kd;             /**< Derivative gain */
  float output_min;     /**< Minimum output limit */
  float output_max;     /**< Maximum output limit */
  float integral_min;   /**< Minimum integral limit (anti-windup) */
  float integral_max;   /**< Maximum integral limit (anti-windup) */
  bool  update_pending; /**< true if gains should be updated */
} pid_gains_t;

/**
 * @brief Temperature sensor state
 */
typedef struct {
  int16_t temperature_cdegc
    [k_shared_temp_sensor_count]; /**< Temperature for each sensor (centi-degrees C) */
  bool     sensor_valid[k_shared_temp_sensor_count]; /**< Validity flag for each sensor */
  uint8_t  sensor_count;                             /**< Number of sensors */
  uint32_t timestamp_ms;                             /**< Timestamp (ms) */
} temp_sensor_state_t;

/**
 * @brief Obstacle detection state
 */
typedef struct {
  uint16_t distance_cm[k_shared_obstacle_sensor_count]; /**< Distance for each sensor (cm) */
  bool     obstacle_detected
    [k_shared_obstacle_sensor_count]; /**< Obstacle detected flag for each sensor */
  bool     any_obstacle;              /**< true if any sensor detects obstacle */
  uint32_t timestamp_ms;              /**< Timestamp (ms) */
} obstacle_state_t;

/**
 * @enum imu_scale_t
 * @brief Scale constants for converting imu_state_t integer fields to physical units
 *
 * @details
 * The BNO055 outputs fixed-point integers. Divide by these constants to get
 * SI/degree values. Use these named constants instead of raw literals.
 *
 * @see imu_state_t Fields that use these scales
 * @see bno055_scale_t Equivalent constants in the BNO055 driver
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_imu_scale_euler = 16,    /**< Euler angle divisor: raw / 16 = degrees */
  k_imu_scale_quat  = 16384, /**< Quaternion divisor: raw / 16384 = unit quaternion */
  k_imu_scale_acc   = 100,   /**< Linear accel divisor: raw / 100 = m/s^2 */
  k_imu_scale_gyro =
    16, /**< Gyroscope divisor: raw / 16 = deg/s (matches k_bno055_scale_gyro_lsb_per_dps) */
} imu_scale_t;

/**
 * @enum baro_scale_t
 * @brief Scale constants for converting baro_state_t integer fields to SI units
 *
 * @details
 * The BMP280 driver outputs fixed-point integers. Divide by these constants.
 *
 * @see baro_state_t Fields that use these scales
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_baro_scale_temp  = 100, /**< Temperature divisor: temp_centi_degc / 100 = degC */
  k_baro_scale_press = 256, /**< Pressure divisor: press_pa_256 / 256 = Pa */
} baro_scale_t;

/**
 * @struct imu_state_t
 * @brief IMU sensor fusion output state (BNO055 NDOF mode)
 *
 * @details
 * Holds the most recent compensated output from the BNO055 in NDOF fusion mode.
 * All 16-bit fields use raw integer scaling to avoid floating-point in the shared
 * data layer. Convert to physical units in application code:
 *
 * @code
 * float heading_deg = (float)imu.heading_deg16 / (float)k_imu_scale_euler;
 * float quat_w      = (float)imu.quat_w        / (float)k_imu_scale_quat;
 * float acc_x_mps2  = (float)imu.lin_acc_x     / (float)k_imu_scale_acc;
 * @endcode
 *
 * @invariant valid == false until imu_task successfully reads BNO055 at least once
 * @invariant calib_stat byte: SYS[7:6] GYR[5:4] ACC[3:2] MAG[1:0], each 0-3
 *
 * @see bno055_data_t BNO055 driver output structure (same scaling)
 * @see shared_data_update_imu() Write accessor
 * @see shared_data_get_imu() Read accessor
 *
 * @since Version 1.0.0
 */
typedef struct {
  int16_t heading_deg16; /**< Euler heading 0-359.9375 deg (scale: k_imu_scale_euler) */
  int16_t roll_deg16;    /**< Euler roll -90 to +90 deg (scale: k_imu_scale_euler) */
  int16_t pitch_deg16;   /**< Euler pitch -180 to +180 deg (scale: k_imu_scale_euler) */
  int16_t quat_w;        /**< Quaternion W component (scale: k_imu_scale_quat) */
  int16_t quat_x;        /**< Quaternion X component (scale: k_imu_scale_quat) */
  int16_t quat_y;        /**< Quaternion Y component (scale: k_imu_scale_quat) */
  int16_t quat_z;        /**< Quaternion Z component (scale: k_imu_scale_quat) */
  int16_t lin_acc_x;     /**< Linear acceleration X, gravity-compensated (scale: k_imu_scale_acc) */
  int16_t lin_acc_y;     /**< Linear acceleration Y, gravity-compensated (scale: k_imu_scale_acc) */
  int16_t lin_acc_z;     /**< Linear acceleration Z, gravity-compensated (scale: k_imu_scale_acc) */
  int16_t
    gyro_x_dps16; /**< Gyroscope X angular rate in deg/s (scale: k_imu_scale_gyro; 1 LSB = 1/16 deg/s) */
  int16_t
    gyro_y_dps16; /**< Gyroscope Y angular rate in deg/s (scale: k_imu_scale_gyro; 1 LSB = 1/16 deg/s) */
  int16_t
    gyro_z_dps16; /**< Gyroscope Z angular rate in deg/s (scale: k_imu_scale_gyro; 1 LSB = 1/16 deg/s) */
  uint32_t
    timestamp_ms; /**< ThreadX tick when data was last updated (ms); placed before 8-bit fields to avoid padding */
  int8_t  temp_degc;  /**< On-chip temperature in degrees Celsius (1 deg C per LSB) */
  uint8_t calib_stat; /**< Raw CALIB_STAT byte: SYS[7:6] GYR[5:4] ACC[3:2] MAG[1:0] */
  bool    valid;      /**< true after first successful read from BNO055 */
} imu_state_t;

/**
 * @struct baro_state_t
 * @brief Barometric pressure and temperature state (BMP280 forced mode)
 *
 * @details
 * Holds the most recent compensated output from the BMP280 barometric pressure sensor.
 * Integer-scaled to avoid floating-point in the shared data layer. Convert to SI units:
 *
 * @code
 * float temp_celsius   = (float)baro.temp_centi_degc / (float)k_baro_scale_temp;
 * float pressure_pa    = (float)baro.press_pa_256    / (float)k_baro_scale_press;
 * float pressure_hpa   = pressure_pa / 100.0F;
 * @endcode
 *
 * @invariant valid == false until imu_task successfully reads BMP280 at least once
 * @invariant press_pa_256 > 0 when valid == true (absolute pressure always positive)
 * @invariant temp_centi_degc in [k_bmp280_temp_min_cdegc, k_bmp280_temp_max_cdegc] when valid == true
 *
 * @see bmp280_data_t BMP280 driver output structure (same scaling)
 * @see shared_data_update_baro() Write accessor
 * @see shared_data_get_baro() Read accessor
 *
 * @since Version 1.0.0
 */
typedef struct {
  int32_t temp_centi_degc; /**< Temperature * 100 (e.g. 2523 = 25.23 degC); int32_t matches the
                                BMP280 driver arithmetic width; valid range
                                [k_bmp280_temp_min_cdegc, k_bmp280_temp_max_cdegc] */
  uint32_t
    press_pa_256; /**< Pressure * k_baro_scale_press in Pa (divide by k_baro_scale_press for Pa) */
  uint32_t timestamp_ms; /**< ThreadX tick when data was last updated (ms) */
  bool     valid;        /**< true after first successful read from BMP280 */
} baro_state_t;

/**
 * @enum shared_event_flags_t
 * @brief Event flags for inter-task signaling
 *
 * @details
 * One-hot bitmask constants used to signal asynchronous events between RTOS
 * tasks via a ThreadX TX_EVENT_FLAGS_GROUP. Flags are OR-combined when raised
 * and remain set (sticky) until explicitly cleared by the consuming task.
 * Multiple flags may be set simultaneously; consumers should test each bit
 * independently.
 *
 * @invariant k_event_none == 0 (no-op / cleared state; safe as a default value)
 * @invariant All non-zero members are distinct powers of two (one-hot bit fields)
 *
 * @code
 * // Raise two flags at once:
 * (void)shared_data_set_event(k_event_comm_timeout | k_event_estop_triggered);
 *
 * // Test for a specific flag in the consumer task:
 * shared_event_flags_t flags;
 * (void)tx_event_flags_get(&g_event_flags, k_event_comm_timeout,
 *                          TX_OR_CLEAR, (ULONG*)&flags, TX_WAIT_FOREVER);
 * @endcode
 *
 * @see shared_data_set_event() Raises one or more flags
 * @see shared_data.c ThreadX event-flags group used internally
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_event_none                  = 0x00000000, /**< No events pending (cleared state) */
  k_event_motor_command_updated = 0x00000001, /**< New motor command available */
  k_event_estop_triggered       = 0x00000002, /**< E-stop activated */
  k_event_pid_gains_updated     = 0x00000004, /**< PID gains changed */
  k_event_obstacle_detected     = 0x00000010, /**< Obstacle detected */
  k_event_obstacle_cleared      = 0x00000020, /**< Obstacle cleared */
  k_event_estop_cleared         = 0x00000040, /**< E-stop cleared */
  k_event_comm_timeout          = 0x00000080, /**< Communication timeout */
} shared_event_flags_t;

/**
 * @enum shared_data_constants_t
 * @brief Shared data module timing constants
 *
 * @details
 * Communication timing thresholds used by the shared data module for
 * timeout detection and watchdog monitoring. Values fit in uint32_t
 * because they represent millisecond durations that exceed uint16_t range
 * at higher values.
 *
 * @invariant k_shared_comm_timeout_ms > 0
 *
 * @code{.c}
 * if ((current_tick - last_tick) * k_tick_period_ms > k_shared_comm_timeout_ms) {
 *     shared_data_trigger_estop(k_estop_reason_comm_timeout);
 * }
 * @endcode
 *
 * @see shared_data_is_comm_timeout() Communication timeout check
 * @see shared_data_update_last_comm_tick() Update communication watchdog
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_shared_comm_timeout_ms = 500, /**< Communication timeout threshold in milliseconds */
} shared_data_constants_t;

/**
 * @brief Main shared data container structure
 *
 * @details
 * This structure contains all shared data and synchronization primitives
 * for inter-task communication. All access must go through the accessor
 * functions - never access this structure directly!
 */
typedef struct {
  /* ThreadX synchronization primitives */
  TX_MUTEX             motor_mutex;    /**< Mutex for motor data */
  TX_MUTEX             temp_mutex;     /**< Mutex for temperature data */
  TX_MUTEX             obstacle_mutex; /**< Mutex for obstacle data */
  TX_MUTEX             estop_mutex;    /**< Mutex for e-stop data */
  TX_MUTEX             imu_mutex;      /**< Mutex for IMU (BNO055) data */
  TX_MUTEX             baro_mutex;     /**< Mutex for barometric (BMP280) data */
  TX_EVENT_FLAGS_GROUP event_flags;    /**< Event flags for inter-task signaling */

  /* Shared data structures */
  motor_command_t     motor_command;  /**< Motor velocity command */
  motor_state_t       motor_state;    /**< Motor state telemetry */
  pid_gains_t         pid_gains;      /**< PID controller gains */
  temp_sensor_state_t temp_state;     /**< Temperature sensor state */
  obstacle_state_t    obstacle_state; /**< Obstacle detection state */
  imu_state_t         imu_state;      /**< BNO055 IMU fusion output state */
  baro_state_t        baro_state;     /**< BMP280 barometric sensor state */

  /* E-stop state */
  bool           estop_active; /**< Emergency stop active flag */
  estop_reason_t estop_reason; /**< Emergency stop reason */

  /* Communication tracking */
  uint32_t last_comm_tick; /**< Last communication timestamp */
  uint8_t
       active_channel; /**< Channel that last delivered a command frame (rx_comm_channel_t value) */
  bool active_channel_valid; /**< True once at least one command frame has been received */

  /* Initialization flag */
  bool initialized; /**< Module initialized flag */
} shared_data_t;

/**
 * @brief Global shared data instance (do not access directly!)
 */
extern shared_data_t g_shared_data;

/***********************************************************************************************************************
 * Public Function Declarations
 ***********************************************************************************************************************/

/**
 * @brief Initialize shared data module
 * @return rx_err_t Error code
 */
rx_err_t shared_data_init(void);

/**
 * @brief Set motor velocity command
 * @param cmd Motor command
 * @return rx_err_t Error code
 */
rx_err_t shared_data_set_motor_command(const motor_command_t* cmd);

/**
 * @brief Get motor velocity command
 * @param out_cmd Output motor command
 * @return rx_err_t Error code
 */
rx_err_t shared_data_get_motor_command(motor_command_t* out_cmd);

/**
 * @brief Update motor state telemetry
 * @param state Motor state
 * @return rx_err_t Error code
 */
rx_err_t shared_data_update_motor_state(const motor_state_t* state);

/**
 * @brief Get motor state telemetry
 * @param out_state Output motor state
 * @return rx_err_t Error code
 */
rx_err_t shared_data_get_motor_state(motor_state_t* out_state);

/**
 * @brief Set PID controller gains
 * @param gains PID gains
 * @return rx_err_t Error code
 */
rx_err_t shared_data_set_pid_gains(const pid_gains_t* gains);

/**
 * @brief Get PID controller gains
 * @param out_gains Output PID gains
 * @return rx_err_t Error code
 */
rx_err_t shared_data_get_pid_gains(pid_gains_t* out_gains);

/**
 * @brief Check if PID update is pending
 * @return true if update pending
 */
bool shared_data_pid_update_pending(void);

/**
 * @brief Clear PID update flag
 */
void shared_data_clear_pid_update_flag(void);

/**
 * @brief Trigger emergency stop
 * @param reason E-stop reason code
 * @return rx_err_t Error code
 */
rx_err_t shared_data_trigger_estop(estop_reason_t reason);

/**
 * @brief Trigger emergency stop from ISR context (ISR-safe, no blocking)
 *
 * @details
 * ISR-safe version of shared_data_trigger_estop() that DOES NOT block on mutex.
 * Sets volatile flag s_estop_pending_from_isr and s_pending_estop_reason, then
 * sets k_event_estop_triggered event flag. Motor control task commits pending
 * ISR e-stop via shared_data_commit_isr_estop() within 4ms (250 Hz loop).
 *
 * **CRITICAL:** This function MUST be used instead of shared_data_trigger_estop()
 * when called from interrupt handlers (POEG ISRs).
 *
 * **Algorithm:** Write s_pending_estop_reason first, then s_estop_pending_from_isr
 * (ensures reason always valid when flag is set), then set event flag via
 * tx_event_flags_set(&g_shared_data.event_flags, k_event_estop_triggered, TX_OR).
 *
 * @param[in] reason E-stop reason code (driver_fault, overcurrent, etc.)
 *
 * @pre Called from ISR context only (POEG motor fault ISRs)
 * @pre shared_data_init() completed and g_shared_data.event_flags initialized
 * @post s_estop_pending_from_isr == true (volatile flag set)
 * @post s_pending_estop_reason == reason (volatile reason stored)
 * @post k_event_estop_triggered set via tx_event_flags_set(&g_shared_data.event_flags, k_event_estop_triggered, TX_OR)
 * @post Motor task will commit within 4ms via shared_data_commit_isr_estop()
 *
 * @note ISR-safe: No blocking calls, no mutex usage (~1 us execution)
 * @note Thread Safety: Volatile writes are atomic on RX72N
 * @note Multiple ISRs may race - last reason wins (acceptable for safety)
 *
 * @code{.c}
 * // POEG motor fault ISR example (shared GROUPBL2 dispatcher)
 * void __attribute__((interrupt)) poeg_groupbl2_isr(void) {
 *     icu()->ir[k_poeg_irq_groupbl2_vector] = 0; // Clear GROUPBL2 IR flag
 *     rx_log_error("POEG", "nFAULT (dispatch via GRPBL2)");
 *     shared_data_trigger_estop_isr_safe(k_estop_reason_driver_fault); // ISR-safe
 * }
 * @endcode
 *
 * @warning ONLY call from ISR context. For task context, use shared_data_trigger_estop()
 * @warning Race condition: If multiple ISRs fire, last reason wins (acceptable)
 *
 * @see shared_data_commit_isr_estop() Commit function (motor task)
 * @see shared_data_trigger_estop() Task-context version (uses mutex)
 * @see shared_data_is_estop_active() Check if e-stop active
 *
 * @since Version 1.0.0
 */
void shared_data_trigger_estop_isr_safe(estop_reason_t reason);

/**
 * @brief Commit ISR-triggered e-stop to mutex-protected state (task context)
 *
 * @details
 * Transfers ISR-triggered e-stop from volatile flags (s_estop_pending_from_isr,
 * s_pending_estop_reason) to mutex-protected shared state (g_shared_data.estop_active,
 * g_shared_data.estop_reason). Called by motor control task at start of each 4ms
 * iteration (250 Hz).
 *
 * **Why this is needed:** ISRs cannot block on mutexes, so they set volatile flags.
 * This function runs in task context where mutex acquisition is safe.
 *
 * **Algorithm:** Uses ThreadX critical section (tx_interrupt_control) to atomically
 * check-and-clear s_estop_pending_from_isr flag, preventing race with ISRs. If
 * pending, acquires estop_mutex and commits to shared state.
 *
 * @return rx_err_t Operation status
 * @retval k_rx_ok E-stop committed successfully or no pending e-stop
 * @retval k_rx_err_not_initialized Module not initialized
 * @retval k_rx_err_rtos_mutex Mutex acquisition failed
 *
 * @pre Module initialized via shared_data_init()
 * @pre Called from task context only (motor control task, NOT ISR)
 * @post If pending e-stop: s_estop_pending_from_isr cleared, g_shared_data.estop_active set, g_shared_data.estop_reason updated
 * @post If no pending e-stop: State unchanged, estop_mutex not acquired
 * @post estop_mutex released (if acquired) or state unchanged if none pending
 *
 * @note Thread Safety: Protected by estop_mutex and critical section
 * @note Performance: ~2.5 us when pending, ~0.5 us when not pending
 * @note Frequency: Called every 4ms by motor task (250 Hz)
 *
 * @code{.c}
 * // Motor control task main loop
 * while (true) {
 *     // Commit any ISR-triggered e-stop first
 *     (void)shared_data_commit_isr_estop();
 *
 *     // Check e-stop status (will see committed ISR e-stop)
 *     if (shared_data_is_estop_active()) {
 *         internal_active_brake_sequence();
 *         tx_thread_sleep(1); // 4ms
 *         continue;
 *     }
 *
 *     // Normal control loop...
 * }
 * @endcode
 *
 * @warning ONLY call from task context (motor control task). Uses blocking mutex.
 * @warning DO NOT call from ISR context (will cause deadlock/fault)
 *
 * @see shared_data_trigger_estop_isr_safe() ISR-safe e-stop trigger
 * @see shared_data_trigger_estop() Task-context e-stop trigger
 * @see shared_data_is_estop_active() Check if e-stop active
 *
 * @since Version 1.0.0
 */
rx_err_t shared_data_commit_isr_estop(void);

/**
 * @brief Clear emergency stop
 * @return rx_err_t Error code
 */
rx_err_t shared_data_clear_estop(void);

/**
 * @brief Check if emergency stop is active
 * @return true if e-stop active
 */
bool shared_data_is_estop_active(void);

/**
 * @brief Get emergency stop reason code
 * @return estop_reason_t Reason code for current e-stop
 */
estop_reason_t shared_data_get_estop_reason(void);

/**
 * @brief Update temperature sensor state
 * @param state Temperature state
 * @return rx_err_t Error code
 */
rx_err_t shared_data_update_temp(const temp_sensor_state_t* state);

/**
 * @brief Get temperature sensor state
 * @param out_state Output temperature state
 * @return rx_err_t Error code
 */
rx_err_t shared_data_get_temp(temp_sensor_state_t* out_state);

/**
 * @brief Update obstacle detection state
 * @param state Obstacle state
 * @return rx_err_t Error code
 */
rx_err_t shared_data_update_obstacle(const obstacle_state_t* state);

/**
 * @brief Get obstacle detection state
 * @param out_state Output obstacle state
 * @return rx_err_t Error code
 */
rx_err_t shared_data_get_obstacle(obstacle_state_t* out_state);

/**
 * @brief Update IMU state from BNO055 driver output
 *
 * @details
 * Acquires imu_mutex, copies the caller's imu_state_t into g_shared_data.imu_state,
 * and releases the mutex. Called by imu_task after each successful BNO055 read.
 *
 * @param[in] state Pointer to populated imu_state_t. Must not be NULL.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok State stored successfully
 * @retval k_rx_err_null_ptr state is NULL
 * @retval k_rx_err_not_initialized shared_data_init() not called
 * @retval k_rx_err_rtos_mutex Mutex acquisition failed
 *
 * @pre shared_data_init() called successfully
 * @pre state non-NULL with valid IMU data
 * @post g_shared_data.imu_state updated under imu_mutex protection (on k_rx_ok)
 * @post imu_mutex released if it was acquired; not released on k_rx_err_rtos_mutex
 *       (mutex acquisition never occurred in that case)
 *
 * @note Not ISR-safe (blocking mutex wait)
 *
 * @see shared_data_get_imu() Consumer accessor
 *
 * @since Version 1.0.0
 */
rx_err_t shared_data_update_imu(const imu_state_t* state);

/**
 * @brief Get IMU state (BNO055 fusion output)
 *
 * @details
 * Acquires imu_mutex, copies g_shared_data.imu_state into the caller's buffer,
 * and releases the mutex. Called by telemetry_task to populate TelemetryData.
 *
 * @param[out] out_state Output buffer for IMU state. Must not be NULL.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok State copied into out_state
 * @retval k_rx_err_null_ptr out_state is NULL
 * @retval k_rx_err_not_initialized shared_data_init() not called
 * @retval k_rx_err_rtos_mutex Mutex busy (TX_NO_WAIT); caller should retry or use zero-init fallback
 *
 * @pre shared_data_init() called successfully
 * @pre out_state non-NULL
 * @post *out_state contains latest IMU data (check out_state->valid before use)
 * @post imu_mutex released if acquired; not acquired (and not released) if k_rx_err_rtos_mutex
 *
 * @note Check out_state->valid before relying on data values
 * @note Non-blocking: uses TX_NO_WAIT mutex acquisition; returns k_rx_err_rtos_mutex if mutex busy
 * @note Not ISR-safe
 *
 * @see shared_data_update_imu() Producer accessor
 *
 * @since Version 1.0.0
 */
rx_err_t shared_data_get_imu(imu_state_t* out_state);

/**
 * @brief Update barometric pressure state from BMP280 driver output
 *
 * @details
 * Acquires baro_mutex, copies the caller's baro_state_t into g_shared_data.baro_state,
 * and releases the mutex. Called by imu_task after each successful BMP280 read.
 *
 * @param[in] state Pointer to populated baro_state_t. Must not be NULL.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok State stored successfully
 * @retval k_rx_err_null_ptr state is NULL
 * @retval k_rx_err_not_initialized shared_data_init() not called
 * @retval k_rx_err_rtos_mutex Mutex acquisition failed
 *
 * @pre shared_data_init() called successfully
 * @pre state non-NULL with valid BMP280 data
 * @post g_shared_data.baro_state updated under baro_mutex protection on success
 * @post baro_mutex released after successful update; not acquired (and not released) if mutex acquisition fails (k_rx_err_rtos_mutex)
 *
 * @note Not ISR-safe (blocking mutex wait)
 *
 * @see shared_data_get_baro() Consumer accessor
 *
 * @since Version 1.0.0
 */
rx_err_t shared_data_update_baro(const baro_state_t* state);

/**
 * @brief Get barometric pressure state (BMP280 output)
 *
 * @details
 * Acquires baro_mutex, copies g_shared_data.baro_state into the caller's buffer,
 * and releases the mutex. Called by telemetry_task to populate TelemetryData.
 *
 * @param[out] out_state Output buffer for barometric state. Must not be NULL.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok State copied into out_state
 * @retval k_rx_err_null_ptr out_state is NULL
 * @retval k_rx_err_not_initialized shared_data_init() not called
 * @retval k_rx_err_rtos_mutex Mutex acquisition failed
 *
 * @pre shared_data_init() called successfully
 * @pre out_state non-NULL
 * @post *out_state contains latest baro data (check out_state->valid before use)
 * @post baro_mutex released if acquired; not acquired (and not released) if k_rx_err_rtos_mutex
 *
 * @note Check out_state->valid before relying on data values
 * @note Non-blocking: uses TX_NO_WAIT mutex acquisition; returns k_rx_err_rtos_mutex if mutex busy
 * @note Not ISR-safe
 *
 * @see shared_data_update_baro() Producer accessor
 *
 * @since Version 1.0.0
 */
rx_err_t shared_data_get_baro(baro_state_t* out_state);

/**
 * @brief Check if communication timeout occurred
 * @return true if timeout (>500ms since last command)
 */
bool shared_data_is_comm_timeout(void);

/**
 * @brief Update last communication timestamp
 */
void shared_data_update_last_comm_tick(void);

/**
 * @brief Record the channel that most recently delivered a command frame
 *
 * @details
 * Called by the comm task inside internal_frame_callback() on every valid
 * frame reception.  The value is used by the telemetry task to route
 * outgoing frames back on the same transport that the host is actively
 * using, avoiding asymmetric USB/SPI usage in SPI-only mode.
 *
 * @param[in] channel Channel on which the frame was received (rx_comm_channel_t cast to uint8_t).
 *                    Must be k_comm_channel_uart (0) or k_comm_channel_spi (1).
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Channel stored successfully
 * @retval k_rx_err_not_initialized shared_data_init() not yet called
 * @retval k_rx_err_invalid_arg channel is out of range (>= k_comm_channel_count)
 * @retval k_rx_err_rtos_mutex Mutex acquisition failed
 *
 * @pre shared_data_init() has been called successfully
 * @pre channel is a valid rx_comm_channel_t value cast to uint8_t (< k_comm_channel_count)
 * @post g_shared_data.active_channel == channel on k_rx_ok
 * @post g_shared_data.active_channel_valid == true on k_rx_ok
 *
 * @note Thread safety: protected by motor_mutex (same section as last_comm_tick)
 * @note Uses uint8_t to avoid including rx_comm_manager.h in this header
 *
 * @see shared_data_get_active_channel() Consumer accessor for telemetry routing
 * @see shared_data_update_last_comm_tick() Updated in the same callback
 *
 * @since Version 1.0.0
 */
rx_err_t shared_data_update_active_channel(uint8_t channel);

/**
 * @brief Return the channel that last delivered a command frame
 *
 * @details
 * Used by the telemetry task to select the outgoing transport channel so
 * that telemetry replies travel on the same physical link as commands.
 * Returns k_comm_channel_uart when no command has been received yet
 * (active_channel_valid == false), preserving existing USB-default behaviour.
 *
 * @return uint8_t Active communication channel (rx_comm_channel_t cast to uint8_t)
 * @retval 0 (k_comm_channel_uart) Default before any command is received, or on
 *           mutex failure, or when USB was the last channel
 * @retval 1 (k_comm_channel_spi) SPI was the last channel to deliver a command
 *
 * @pre shared_data_init() has been called (returns USB default if not)
 * @pre At least one frame has been received for a non-default result
 * @post Return value is 0 (USB) or 1 (SPI) matching rx_comm_channel_t values
 * @post g_shared_data is unchanged (read-only accessor)
 *
 * @note Thread safety: protected by motor_mutex
 * @note Fail-safe: returns 0 (k_comm_channel_uart) on any error
 * @note Uses uint8_t to avoid including rx_comm_manager.h in this header
 *
 * @see shared_data_update_active_channel() Writer called by comm task
 *
 * @since Version 1.0.0
 */
uint8_t shared_data_get_active_channel(void);

/**
 * @brief Set event flags for inter-task signaling
 * @param flags Event flags to set
 * @return rx_err_t Error code
 */
rx_err_t shared_data_set_event(shared_event_flags_t flags);

/**
 * @brief Wait for event flags with optional timeout
 * @param[in]  flags            Event flags to wait for (OR combination)
 * @param[in]  wait_option      ThreadX wait option (TX_WAIT_FOREVER, tick count, TX_NO_WAIT)
 * @param[out] out_actual_flags Actual flags that were set (may be NULL)
 * @return rx_err_t Error code
 * @retval k_rx_ok             Flags received
 * @retval k_rx_err_timeout    Wait timed out (TX_NO_EVENTS)
 * @retval k_rx_err_not_initialized Shared data not initialized
 * @retval k_rx_err_rtos_error ThreadX error
 */
rx_err_t shared_data_wait_event(shared_event_flags_t flags,
                                uint32_t             wait_option,
                                uint32_t*            out_actual_flags);
