/**
 * @file shared_data.h
 * @brief Thread-Safe Shared Data Infrastructure for Multi-Task Communication
 *
 * @details
 * Declares all shared data types and accessor functions for inter-task
 * communication in the STAR firmware. All access is mutex-protected.
 *
 * @see shared_data.c Implementation
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
 * @brief Emergency stop reason codes
 */
typedef enum : uint8_t {
  k_estop_reason_none          = 0, /**< No e-stop active */
  k_estop_reason_comm_timeout  = 1, /**< Communication timeout */
  k_estop_reason_obstacle      = 2, /**< Obstacle too close */
  k_estop_reason_driver_fault  = 3, /**< DRV8243 fault */
  k_estop_reason_overcurrent   = 4, /**< Motor overcurrent */
  k_estop_reason_manual        = 5, /**< Manual request */
  k_estop_reason_low_battery   = 6, /**< Battery low */
  k_estop_reason_battery_fault = 7, /**< Battery fault (OTA, OCA, TDA) */
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
  float    target_velocity_mps[4]; /**< Target velocity for each motor (m/s) */
  uint32_t sequence;               /**< Command sequence number */
  uint32_t timestamp_ms;           /**< ThreadX tick when command received (ms) */
  bool     valid;                  /**< true if command is valid, false if not set yet */
} motor_command_t;

/**
 * @brief Motor state telemetry
 */
typedef struct {
  float          current_velocity_mps[4]; /**< Current velocity for each motor (m/s) */
  float          current_ma[4];           /**< Current for each motor (mA) */
  int32_t        encoder_counts[4];       /**< Encoder counts for each motor (signed) */
  uint8_t        fault_flags[4];          /**< Fault flags for each motor */
  float          duty_cycle_percent[4];   /**< PWM duty cycle for each motor (%) */
  bool           estop_active;            /**< Emergency stop active flag */
  estop_reason_t estop_reason;            /**< Emergency stop reason code */
  motor_mode_t   mode;                    /**< Current motor control mode */
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
 * @brief BMS (Battery Management System) state
 */
typedef struct {
  uint16_t voltage_mv;          /**< Battery voltage (mV) */
  int16_t  current_ma;          /**< Battery current (mA) */
  uint8_t  soc_percent;         /**< State of charge (%) */
  int16_t  temperature_celsius; /**< Battery temperature (°C) */
  uint16_t capacity_mah;        /**< Remaining capacity (mAh) */
  uint16_t full_capacity_mah;   /**< Full charge capacity (mAh) */
  uint16_t cycle_count;         /**< Battery cycle count */
  uint16_t fault_flags;         /**< BMS fault flags */
  uint32_t timestamp_ms;        /**< Timestamp (ms) */
  bool     valid;               /**< true if data valid */
} bms_state_t;

/**
 * @brief Temperature sensor state
 */
typedef struct {
  int16_t  temperature_cdegc[4]; /**< Temperature for each sensor (centi-degrees C) */
  bool     sensor_valid[4];      /**< Validity flag for each sensor */
  uint8_t  sensor_count;         /**< Number of sensors */
  uint32_t timestamp_ms;         /**< Timestamp (ms) */
} temp_sensor_state_t;

/**
 * @brief Obstacle detection state
 */
typedef struct {
  uint16_t distance_cm[4];       /**< Distance for each sensor (cm) */
  bool     obstacle_detected[4]; /**< Obstacle detected flag for each sensor */
  bool     any_obstacle;         /**< true if any sensor detects obstacle */
  uint32_t timestamp_ms;         /**< Timestamp (ms) */
} obstacle_state_t;

/**
 * @brief Event flags for inter-task signaling
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
 * @brief Shared data module constants
 */
typedef enum : uint32_t {
  k_shared_comm_timeout_ms = 500, /**< Communication timeout (ms) */
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
  TX_MUTEX             bms_mutex;      /**< Mutex for BMS data */
  TX_MUTEX             temp_mutex;     /**< Mutex for temperature data */
  TX_MUTEX             obstacle_mutex; /**< Mutex for obstacle data */
  TX_MUTEX             estop_mutex;    /**< Mutex for e-stop data */
  TX_EVENT_FLAGS_GROUP event_flags;    /**< Event flags for inter-task signaling */

  /* Shared data structures */
  motor_command_t     motor_command;  /**< Motor velocity command */
  motor_state_t       motor_state;    /**< Motor state telemetry */
  pid_gains_t         pid_gains;      /**< PID controller gains */
  bms_state_t         bms_state;      /**< BMS state */
  temp_sensor_state_t temp_state;     /**< Temperature sensor state */
  obstacle_state_t    obstacle_state; /**< Obstacle detection state */

  /* E-stop state */
  bool           estop_active; /**< Emergency stop active flag */
  estop_reason_t estop_reason; /**< Emergency stop reason */

  /* Communication tracking */
  uint32_t last_comm_tick; /**< Last communication timestamp */

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
 * ISR-safe version that sets volatile flag and event flag without blocking
 * on mutex. Motor control task commits pending ISR e-stop via
 * shared_data_commit_isr_estop() within 4ms.
 *
 * @param[in] reason E-stop reason code
 *
 * @pre Called from ISR context only
 * @post Motor task commits e-stop within 4ms
 *
 * @note ISR-safe: No blocking, no mutex
 * @warning ONLY call from ISR (POEG, BMS alert). For tasks use shared_data_trigger_estop()
 *
 * @see shared_data_commit_isr_estop() Commit function (motor task)
 * @see shared_data_trigger_estop() Task-context version
 *
 * @since Version 1.1.0
 */
void shared_data_trigger_estop_isr_safe(estop_reason_t reason);

/**
 * @brief Commit ISR-triggered e-stop to mutex-protected state (task context)
 *
 * @details
 * Transfers ISR-triggered e-stop from volatile flag to mutex-protected state.
 * Called by motor control task at start of each 4ms iteration.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok E-stop committed or no pending e-stop
 * @retval k_rx_err_not_initialized Module not initialized
 * @retval k_rx_err_rtos_mutex Mutex acquisition failed
 *
 * @pre Called from task context only (motor control task)
 * @post If pending ISR e-stop: committed to shared state
 *
 * @note Thread-safe: Uses estop_mutex
 * @warning ONLY call from task context (uses blocking mutex)
 *
 * @see shared_data_trigger_estop_isr_safe() ISR-safe trigger
 *
 * @since Version 1.1.0
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
 * @brief Update BMS state
 * @param state BMS state
 * @return rx_err_t Error code
 */
rx_err_t shared_data_update_bms(const bms_state_t* state);

/**
 * @brief Get BMS state
 * @param out_state Output BMS state
 * @return rx_err_t Error code
 */
rx_err_t shared_data_get_bms(bms_state_t* out_state);

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
 * @brief Check if communication timeout occurred
 * @return true if timeout (>500ms since last command)
 */
bool shared_data_is_comm_timeout(void);

/**
 * @brief Update last communication timestamp
 */
void shared_data_update_last_comm_tick(void);

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
