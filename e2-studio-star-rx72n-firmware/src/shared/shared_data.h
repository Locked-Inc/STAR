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
 * (void)shared_data_set_event(k_event_low_battery | k_event_comm_timeout);
 *
 * // Test for a specific flag in the consumer task:
 * shared_event_flags_t flags;
 * (void)tx_event_flags_get(&g_event_flags, k_event_low_battery,
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
 * ISR-safe version of shared_data_trigger_estop() that DOES NOT block on mutex.
 * Sets volatile flag s_estop_pending_from_isr and s_pending_estop_reason, then
 * sets k_event_estop_triggered event flag. Motor control task commits pending
 * ISR e-stop via shared_data_commit_isr_estop() within 4ms (250 Hz loop).
 *
 * **CRITICAL:** This function MUST be used instead of shared_data_trigger_estop()
 * when called from interrupt handlers (POEG, BMS alert ISRs).
 *
 * **Algorithm:** Write s_pending_estop_reason first, then s_estop_pending_from_isr
 * (ensures reason always valid when flag is set), then set event flag via
 * tx_event_flags_set(&g_shared_data.event_flags, k_event_estop_triggered, TX_OR).
 *
 * @param[in] reason E-stop reason code (driver_fault, battery_fault, etc.)
 *
 * @return void (no return value)
 * @retval N/A Always succeeds (void return, no error cases, side-effect only)
 *
 * @pre Called from ISR context only (POEG motor fault ISRs, BMS alert ISR)
 * @pre shared_data_init() completed and g_shared_data.event_flags initialized
 * @post s_estop_pending_from_isr == true (volatile flag set)
 * @post s_pending_estop_reason == reason (volatile reason stored)
 * @post k_event_estop_triggered set via tx_event_flags_set(&g_shared_data.event_flags, k_event_estop_triggered, TX_OR)
 * @post Motor task will commit within 4ms via shared_data_commit_isr_estop()
 *
 * @note ISR-safe: No blocking calls, no mutex usage (~1 µs execution)
 * @note Thread Safety: Volatile writes are atomic on RX72N
 * @note Multiple ISRs may race - last reason wins (acceptable for safety)
 *
 * @code{.c}
 * // POEG motor fault ISR example
 * void __attribute__((interrupt)) poeg_group_a_isr(void) {
 *     icu()->ir[k_poeg_irq_group_a] = 0; // Clear IR flag
 *     rx_log_error("POEG", "nFAULT motor 0");
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
 * @since Version 1.1.0
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
 * @param None
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
 * @note Performance: ~2.5 µs when pending, ~0.5 µs when not pending
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
