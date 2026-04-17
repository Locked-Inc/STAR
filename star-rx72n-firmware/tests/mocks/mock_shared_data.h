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
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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
 * @enum mock_shared_channel_t
 * @brief Channel identifier constants mirroring rx_comm_channel_t values
 *
 * @details
 * Named channel constants for use in mock_shared_data without requiring
 * rx_comm_manager.h (which would transitively include rx_frame.h and cause
 * redeclaration conflicts with mock frame types in unit test builds).
 * Values MUST remain bit-for-bit identical to rx_comm_channel_t.
 *
 * @invariant k_mock_channel_uart == k_comm_channel_uart (0)
 * @invariant k_mock_channel_spi == k_comm_channel_spi (1)
 *
 * @code
 * // Set active channel to SPI before running code under test:
 * mock_shared_data_set_active_channel(k_mock_channel_spi);
 *
 * // Assert active channel after code under test:
 * TEST_ASSERT_EQUAL(k_mock_channel_spi, shared_data_get_active_channel());
 * @endcode
 *
 * @see rx_comm_channel_t Production definition (authoritative)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mock_channel_uart = 0U, /**< UART channel (matches k_comm_channel_uart) */
  k_mock_channel_spi  = 1U, /**< SPI channel  (matches k_comm_channel_spi) */
  k_mock_channel_i2c  = 2U, /**< I2C channel  (matches k_comm_channel_i2c) */
} mock_shared_channel_t;

/**
 * @enum shared_event_flags_t
 * @brief Event flags for inter-task signaling (must match shared_data.h)
 *
 * @details
 * One-hot bitmask constants used to signal events between RTOS tasks.
 * In production code the underlying ThreadX event-flags group uses TX_OR
 * semantics (bits are sticky until explicitly cleared). The mock mirrors
 * this by OR-accumulating flags in shared_data_set_event().
 *
 * **IMPORTANT:** This enum must remain bit-for-bit identical to the
 * shared_event_flags_t defined in shared_data.h. If either definition
 * changes, update both simultaneously.
 *
 * @invariant All non-zero members are distinct powers of two (one-hot)
 * @invariant k_event_none == 0 (no-op / cleared state)
 *
 * @code
 * // Test that a task sets the expected event flag
 * shared_data_set_event(k_event_obstacle_detected);
 * TEST_ASSERT_EQUAL(k_event_obstacle_detected,
 *                   mock_shared_data_get_last_event_flags());
 * @endcode
 *
 * @see shared_data.h Production definition (authoritative)
 * @see shared_data_set_event() Function that raises flags
 * @see mock_shared_data_get_last_event_flags() Retrieves accumulated bits
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
  k_estop_reason_none           = 0, /**< No e-stop active */
  k_estop_reason_comm_timeout   = 1, /**< No commands for 500ms */
  k_estop_reason_obstacle       = 2, /**< Obstacle detected by HC-SR04 */
  k_estop_reason_driver_fault   = 3, /**< Motor driver hardware fault detected */
  k_estop_reason_overcurrent    = 4, /**< Motor overcurrent detected */
  k_estop_reason_manual         = 5, /**< Manual e-stop request */
  k_estop_reason_sensor_failure = 6, /**< ADC or sensor read failure */
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
 * @brief Return the number of times shared_data_set_event() has been called
 *
 * @details
 * Provides test code with a way to assert that a task called
 * shared_data_set_event() exactly the expected number of times within a test.
 *
 * @return uint32_t Call count since last mock_shared_data_reset()
 * @retval 0 shared_data_set_event() has not been called since last reset
 * @retval n Number of times shared_data_set_event() was called
 *
 * @pre mock_shared_data_reset() has been called at least once (setUp)
 * @pre Module initialized via mock_shared_data_reset() before first use
 *
 * @post s_set_event_count is unchanged (read-only accessor)
 * @post Return value reflects all calls since last reset
 *
 * @note Thread safety: read-only; safe in single-threaded test context only
 *
 * @code
 * shared_data_set_event(k_event_obstacle_detected);
 * TEST_ASSERT_EQUAL_UINT32(k_expect_call_count_one, mock_shared_data_get_set_event_count());
 * @endcode
 *
 * @see shared_data_set_event() The function whose calls are counted
 * @see mock_shared_data_reset() Resets the counter to zero
 *
 * @since Version 1.0.0
 */
uint32_t mock_shared_data_get_set_event_count(void);

/**
 * @brief Return the accumulated event flags set via shared_data_set_event()
 *
 * @details
 * Returns the bitwise OR of all flag values passed to shared_data_set_event()
 * since the last mock_shared_data_reset(). Test code uses this to verify that
 * the expected event bits were raised without caring about call order.
 *
 * @return shared_event_flags_t Accumulated (OR'd) event flags
 * @retval k_event_none No events have been set since last reset
 * @retval other OR of every flags argument passed to shared_data_set_event()
 *
 * @pre mock_shared_data_reset() has been called at least once (setUp)
 * @pre Module initialized via mock_shared_data_reset() before first use
 *
 * @post s_last_event_flags is unchanged (read-only accessor)
 * @post Return value is a superset of any single shared_data_set_event() call
 *
 * @note Thread safety: read-only; safe in single-threaded test context only
 *
 * @code
 * shared_data_set_event(k_event_obstacle_detected);
 * TEST_ASSERT_EQUAL(k_event_obstacle_detected,
 *                   mock_shared_data_get_last_event_flags());
 * @endcode
 *
 * @see shared_data_set_event() The function that accumulates into this value
 * @see mock_shared_data_reset() Clears accumulated flags to k_event_none
 *
 * @since Version 1.0.0
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

/* Temperature State */
rx_err_t shared_data_update_temp(const temp_sensor_state_t* state);
rx_err_t shared_data_get_temp(temp_sensor_state_t* out_state);

/* Obstacle State */
rx_err_t shared_data_update_obstacle(const obstacle_state_t* state);
rx_err_t shared_data_get_obstacle(obstacle_state_t* out_state);

/* Communication Timeout */
bool shared_data_is_comm_timeout(void);
void shared_data_update_last_comm_tick(void);

/* Active Channel Routing */

/**
 * @brief Mock implementation of shared_data_update_active_channel()
 *
 * @details
 * Records @p channel as the active transport and increments the update counter.
 * Always returns k_rx_ok (no mutex or initialization checks in mock).
 * Mirrors the production API signature so comm_task calls compile unmodified
 * against this mock.
 *
 * @param[in] channel Channel to record (rx_comm_channel_t cast to uint8_t;
 *                    use k_mock_channel_uart (0) or k_mock_channel_spi (1))
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Always succeeds in mock
 *
 * @pre mock_shared_data_reset() has been called at least once (setUp)
 * @pre channel is a valid mock_shared_channel_t value (0 or 1)
 * @post s_active_channel == channel
 * @post s_active_channel_update_count incremented by 1
 *
 * @note Thread safety: not thread-safe; intended for single-threaded test use only
 * @note Mock only: no mutex, always returns k_rx_ok
 *
 * @code
 * (void)shared_data_update_active_channel(k_mock_channel_spi);
 * TEST_ASSERT_EQUAL(k_mock_channel_spi, shared_data_get_active_channel());
 * @endcode
 *
 * @see shared_data_get_active_channel() Reads the stored channel
 * @see mock_shared_data_get_active_channel_update_count() Retrieves call count
 *
 * @since Version 1.0.0
 */
rx_err_t shared_data_update_active_channel(uint8_t channel);

/**
 * @brief Mock implementation of shared_data_get_active_channel()
 *
 * @details
 * Returns s_active_channel, reflecting the last value stored by
 * shared_data_update_active_channel() or mock_shared_data_set_active_channel().
 * Defaults to k_mock_channel_uart (0) after mock_shared_data_reset().
 *
 * @return uint8_t Active communication channel (rx_comm_channel_t cast to uint8_t)
 * @retval k_mock_channel_uart (0) Default before any update or after reset
 * @retval k_mock_channel_spi (1) SPI was the last channel stored
 *
 * @pre mock_shared_data_reset() has been called at least once (setUp)
 * @pre s_active_channel set via shared_data_update_active_channel() or
 *      mock_shared_data_set_active_channel()
 * @post s_active_channel is unchanged (read-only accessor)
 * @post Return value is k_mock_channel_uart or k_mock_channel_spi
 *
 * @note Thread safety: read-only; safe in single-threaded test context only
 * @note Mock only: no mutex, always returns the raw stored value
 *
 * @code
 * (void)shared_data_update_active_channel(k_mock_channel_spi);
 * TEST_ASSERT_EQUAL(k_mock_channel_spi, shared_data_get_active_channel());
 * @endcode
 *
 * @see shared_data_update_active_channel() Writer
 * @see mock_shared_data_set_active_channel() Test setup writer (no count increment)
 *
 * @since Version 1.0.0
 */
uint8_t shared_data_get_active_channel(void);

/* Event Flags */

/**
 * @brief Accumulate event flag bits into the mock's tracking state
 *
 * @details
 * Mirrors the TX_OR semantics of the production shared_data_set_event() by
 * OR-accumulating the supplied flags. Unlike a simple assignment, multiple
 * calls build up the set of raised flags, matching real RTOS behavior where
 * event bits are sticky until cleared.
 *
 * @param[in] flags One or more shared_event_flags_t bits to set
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Flags accumulated successfully (always in mock)
 *
 * @pre mock_shared_data_reset() has been called at least once (setUp)
 * @pre flags is a valid member or OR combination of shared_event_flags_t
 *
 * @post mock_shared_data_get_set_event_count() incremented by 1
 * @post mock_shared_data_get_last_event_flags() has the supplied bits OR'd in
 *
 * @note Thread safety: not thread-safe; intended for single-threaded test use only
 *
 * @see mock_shared_data_get_set_event_count() Retrieve call count
 * @see mock_shared_data_get_last_event_flags() Retrieve accumulated flags
 * @see mock_shared_data_reset() Clear accumulated state between tests
 *
 * @since Version 1.0.0
 */
rx_err_t shared_data_set_event(shared_event_flags_t flags);

/**
 * @brief Configure the channel returned by shared_data_get_active_channel()
 *
 * @details
 * Directly writes @p channel into the mock's internal s_active_channel state
 * without incrementing s_active_channel_update_count or checking s_initialized.
 * Use this for test setup (Arrange phase) to establish pre-test channel state
 * before invoking code under test. Do NOT use to simulate a runtime channel
 * update -- call shared_data_update_active_channel() for that purpose.
 *
 * @param[in] channel Channel to store; must be a valid mock_shared_channel_t value
 *                    (k_mock_channel_uart or k_mock_channel_spi)
 *
 * @pre mock_shared_data_reset() called at least once (setUp)
 * @pre channel is k_mock_channel_uart (0) or k_mock_channel_spi (1)
 * @post shared_data_get_active_channel() returns (uint8_t)channel
 * @post s_active_channel_update_count is unchanged
 *
 * @note Thread safety: not thread-safe; intended for single-threaded test setup only
 * @note For Arrange phase only; call before the code under test runs
 *
 * @code
 * // Arrange: set SPI as active before running telemetry task code
 * mock_shared_data_set_active_channel(k_mock_channel_spi);
 * TEST_ASSERT_EQUAL(k_mock_channel_spi, shared_data_get_active_channel());
 * @endcode
 *
 * @see shared_data_update_active_channel() Runtime writer (increments update count)
 * @see mock_shared_data_get_active_channel_update_count() Retrieves update call count
 * @see mock_shared_data_reset() Resets channel to k_mock_channel_uart (0)
 *
 * @since Version 1.0.0
 */
void mock_shared_data_set_active_channel(mock_shared_channel_t channel);

/**
 * @brief Return how many times shared_data_update_active_channel() was called
 *
 * @details
 * Returns the accumulated call count for shared_data_update_active_channel()
 * since the last mock_shared_data_reset(). Allows tests to assert that comm_task
 * called the update function exactly the expected number of times (e.g., once
 * per COMMAND frame received). Note that mock_shared_data_set_active_channel()
 * does NOT increment this counter -- only shared_data_update_active_channel() does.
 *
 * @return uint32_t Number of shared_data_update_active_channel() calls since reset
 * @retval 0 shared_data_update_active_channel() has not been called since last reset
 * @retval n Number of times shared_data_update_active_channel() was called
 *
 * @pre mock_shared_data_reset() called at least once (setUp)
 * @pre s_active_channel_update_count reflects only calls via shared_data_update_active_channel()
 * @post s_active_channel_update_count is unchanged (read-only accessor)
 * @post Return value >= 0
 *
 * @note Thread safety: read-only; safe in single-threaded test context only
 * @note mock_shared_data_set_active_channel() does NOT increment this counter
 *
 * @code
 * (void)shared_data_update_active_channel(k_mock_channel_spi);
 * TEST_ASSERT_EQUAL_UINT32(1, mock_shared_data_get_active_channel_update_count());
 * @endcode
 *
 * @see shared_data_update_active_channel() The function whose calls are counted
 * @see mock_shared_data_reset() Resets the counter to zero
 *
 * @since Version 1.0.0
 */
uint32_t mock_shared_data_get_active_channel_update_count(void);

#ifdef __cplusplus
}
#endif
