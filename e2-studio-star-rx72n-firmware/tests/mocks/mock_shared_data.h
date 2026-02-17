/* tests/mocks/mock_shared_data.h */

/**
 * @file mock_shared_data.h
 * @brief Mock shared data module for inter-task communication testing
 *
 * @details
 * Provides test double for shared data module (inter-task communication) to enable
 * unit testing of data sharing, synchronization logic, and task coordination without
 * ThreadX RTOS or actual mutex primitives.
 *
 * Enables testing of: Inter-task data exchange, Data consistency checks, Producer-consumer
 * patterns, Task synchronization logic
 *
 * @par Mock Capabilities: Shared state storage (no actual mutex), Call tracking,
 * Deterministic behavior (no race conditions in tests)
 * @par Usage: tests/test_motor_control_task.c, tests/test_telemetry_task.c
 * @see shared_data.h Real shared data module (with ThreadX mutex)
 * @par NASA Power of 10: [OK] Static allocation
 * @par SOLID: S - Single responsibility (data sharing only)
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * Constants (must match shared_data.h)
 * =============================================================================
 */

typedef enum : uint16_t {
  k_shared_max_motors        = 4,   /**< Number of motors in system */
  k_shared_max_temp_sensors  = 4,   /**< Maximum DS18B20 temperature sensors */
  k_shared_max_hcsr04        = 4,   /**< Maximum HC-SR04 ultrasonic sensors */
  k_shared_comm_timeout_ms   = 500, /**< Communication timeout (500ms) */
  k_shared_active_brake_ms   = 50,  /**< Active brake duration (50ms) */
  k_shared_active_brake_duty = 30,  /**< Active brake PWM duty cycle (30%) */
} mock_shared_data_constants_t;

/**
 * @enum shared_event_flags_t
 * @brief Event flags for inter-task signaling (must match shared_data.h)
 */
typedef enum : uint32_t {
  k_event_motor_command_updated = 0x00000001, /**< New motor command available */
  k_event_estop_triggered       = 0x00000002, /**< E-stop activated */
  k_event_pid_gains_updated     = 0x00000004, /**< PID gains changed */
  k_event_low_battery           = 0x00000008, /**< Low battery detected */
  k_event_obstacle_detected     = 0x00000010, /**< Obstacle detected */
  k_event_obstacle_cleared      = 0x00000020, /**< Obstacle cleared */
  k_event_estop_cleared         = 0x00000040, /**< E-stop cleared */
  k_event_comm_timeout          = 0x00000080, /**< Communication timeout */
} shared_event_flags_t;

/**
 * @enum motor_control_mode_t
 * @brief Motor control operating modes
 */
typedef enum : uint8_t {
  k_motor_mode_idle         = 0, /**< Motors idle (coasting) */
  k_motor_mode_velocity     = 1, /**< Velocity control via PID */
  k_motor_mode_direct_pwm   = 2, /**< Direct PWM control (testing only) */
  k_motor_mode_estop        = 3, /**< Emergency stop (outputs disabled) */
  k_motor_mode_active_brake = 4, /**< Active braking sequence in progress */
} motor_control_mode_t;

/**
 * @enum estop_reason_t
 * @brief Emergency stop trigger reasons
 */
typedef enum : uint8_t {
  k_estop_reason_none         = 0, /**< No e-stop active */
  k_estop_reason_comm_timeout = 1, /**< No commands for 500ms */
  k_estop_reason_obstacle     = 2, /**< Obstacle detected by HC-SR04 */
  k_estop_reason_driver_fault = 3, /**< DRV8243 fault detected */
  k_estop_reason_overcurrent  = 4, /**< Motor overcurrent detected */
  k_estop_reason_manual       = 5, /**< Manual e-stop request */
  k_estop_reason_low_battery  = 6, /**< Critical battery level */
} estop_reason_t;

/* =============================================================================
 * Data Structures (must match shared_data.h)
 * =============================================================================
 */

/**
 * @struct motor_command_t
 * @brief Motor velocity command from communication layer
 */
typedef struct {
  float    target_velocity_mps[k_shared_max_motors]; /**< Target velocities (m/s) */
  uint32_t sequence;                                 /**< Command sequence number */
  uint32_t timestamp_ms;                             /**< Timestamp when received */
  bool     valid;                                    /**< True if valid data */
} motor_command_t;

/**
 * @struct motor_state_t
 * @brief Current motor state for telemetry reporting
 */
typedef struct {
  float                current_velocity_mps[k_shared_max_motors]; /**< Measured velocity */
  float                duty_cycle_percent[k_shared_max_motors];   /**< Current PWM duty */
  float                current_ma[k_shared_max_motors];           /**< Motor current */
  int32_t              encoder_counts[k_shared_max_motors];       /**< Raw encoder counts */
  uint8_t              fault_flags[k_shared_max_motors];          /**< Fault bits */
  motor_control_mode_t mode;                                      /**< Control mode */
  bool                 estop_active;                              /**< E-stop flag */
  estop_reason_t       estop_reason;                              /**< E-stop reason */
} motor_state_t;

/**
 * @struct pid_gains_t
 * @brief PID controller gains for runtime tuning
 */
typedef struct {
  float kp;             /**< Proportional gain */
  float ki;             /**< Integral gain */
  float kd;             /**< Derivative gain */
  float output_min;     /**< Output minimum */
  float output_max;     /**< Output maximum */
  float integral_min;   /**< Integral anti-windup min */
  float integral_max;   /**< Integral anti-windup max */
  bool  update_pending; /**< Gains need applying */
} pid_gains_t;

/**
 * @struct bms_state_t
 * @brief Battery Management System state
 */
typedef struct {
  uint16_t voltage_mv;          /**< Pack voltage (mV) */
  int16_t  current_ma;          /**< Current draw (mA) */
  uint8_t  soc_percent;         /**< State of charge (0-100%) */
  int16_t  temperature_celsius; /**< Battery temperature */
  uint16_t capacity_mah;        /**< Remaining capacity */
  uint16_t full_capacity_mah;   /**< Full charge capacity */
  uint16_t cycle_count;         /**< Charge cycle count */
  uint32_t fault_flags;         /**< BMS fault flags */
  uint32_t timestamp_ms;        /**< Last update timestamp */
  bool     valid;               /**< Data is valid */
} bms_state_t;

/**
 * @struct temp_sensor_state_t
 * @brief Temperature sensor readings
 */
typedef struct {
  int16_t  temperature_cdegc[k_shared_max_temp_sensors]; /**< Temps in 0.01 C */
  bool     sensor_valid[k_shared_max_temp_sensors];      /**< Sensor validity */
  uint8_t  sensor_count;                                 /**< Active sensors */
  uint32_t timestamp_ms;                                 /**< Last update */
} temp_sensor_state_t;

/**
 * @struct obstacle_state_t
 * @brief Obstacle detection state
 */
typedef struct {
  uint16_t distance_cm[k_shared_max_hcsr04];       /**< Distance readings */
  bool     obstacle_detected[k_shared_max_hcsr04]; /**< Per-sensor obstacle */
  bool     any_obstacle;                           /**< Any sensor detected */
  uint32_t timestamp_ms;                           /**< Last update */
} obstacle_state_t;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Reset all mock shared data state
 */
void mock_shared_data_reset(void);

/**
 * @brief Set return value for shared_data_init()
 */
void mock_shared_data_set_init_return(rx_err_t err);

/**
 * @brief Set estop active state for mock
 */
void mock_shared_data_set_estop_active(bool active, estop_reason_t reason);

/**
 * @brief Set communication timeout state
 */
void mock_shared_data_set_comm_timeout(bool timeout);

/**
 * @brief Set motor command for mock
 */
void mock_shared_data_set_motor_command(const motor_command_t* cmd);

/**
 * @brief Set PID gains for mock
 */
void mock_shared_data_set_pid_gains(const pid_gains_t* gains);

/**
 * @brief Set PID update pending flag
 */
void mock_shared_data_set_pid_update_pending(bool pending);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

/**
 * @brief Get number of times init was called
 */
uint32_t mock_shared_data_get_init_count(void);

/**
 * @brief Get number of times estop was triggered
 */
uint32_t mock_shared_data_get_trigger_estop_count(void);

/**
 * @brief Get number of times shared_data_set_event() was called
 */
uint32_t mock_shared_data_get_set_event_count(void);

/**
 * @brief Get the last event flags value passed to shared_data_set_event()
 */
shared_event_flags_t mock_shared_data_get_last_event_flags(void);

/**
 * @brief Get the last estop reason that was triggered
 */
estop_reason_t mock_shared_data_get_last_estop_reason(void);

/**
 * @brief Get the last motor state that was updated
 */
rx_err_t mock_shared_data_get_last_motor_state(motor_state_t* out_state);

/**
 * @brief Get number of motor state updates
 */
uint32_t mock_shared_data_get_motor_state_update_count(void);

/**
 * @brief Check if shared_data was initialized
 */
bool mock_shared_data_was_initialized(void);

/* =============================================================================
 * Mock Shared Data API (matches shared_data.h)
 * =============================================================================
 */

rx_err_t shared_data_init(void);

/* Motor Command */
rx_err_t shared_data_set_motor_command(const motor_command_t* cmd);
rx_err_t shared_data_get_motor_command(motor_command_t* out_cmd);

/* Motor State */
rx_err_t shared_data_update_motor_state(const motor_state_t* state);
rx_err_t shared_data_get_motor_state(motor_state_t* out_state);

/* PID Gains */
rx_err_t shared_data_set_pid_gains(const pid_gains_t* gains);
rx_err_t shared_data_get_pid_gains(pid_gains_t* out_gains);
bool     shared_data_pid_update_pending(void);
void     shared_data_clear_pid_update_flag(void);

/* Emergency Stop */
rx_err_t       shared_data_trigger_estop(estop_reason_t reason);
rx_err_t       shared_data_clear_estop(void);
bool           shared_data_is_estop_active(void);
estop_reason_t shared_data_get_estop_reason(void);

/* BMS State */
rx_err_t shared_data_update_bms(const bms_state_t* state);
rx_err_t shared_data_get_bms(bms_state_t* out_state);

/* Temperature State */
rx_err_t shared_data_update_temp(const temp_sensor_state_t* state);
rx_err_t shared_data_get_temp(temp_sensor_state_t* out_state);

/* Obstacle State */
rx_err_t shared_data_update_obstacle(const obstacle_state_t* state);
rx_err_t shared_data_get_obstacle(obstacle_state_t* out_state);

/* Communication Timeout */
bool shared_data_is_comm_timeout(void);
void shared_data_update_last_comm_tick(void);

/* Event Flags */
rx_err_t shared_data_set_event(shared_event_flags_t flags);

#ifdef __cplusplus
}
#endif
