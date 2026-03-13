/**
 * @file mock_hcsr04_hw.h
 * @brief Mock HC-SR04 ultrasonic sensor hardware for distance measurement testing
 *
 * @details
 * Provides test double for HC-SR04 ultrasonic range finder hardware (GPIO
 * and timing) to enable unit testing of obstacle detection without real
 * sensor hardware. Simulates trigger pulse generation, echo pulse timing,
 * and distance-to-time conversion.
 *
 * Enables testing of:
 * - HC-SR04 trigger pulse generation (10us pulse)
 * - Echo pulse width measurement (distance->time)
 * - Distance calculation (pulse width -> cm/mm)
 * - Timeout handling (no echo received)
 * - Out-of-range detection (2cm-400cm limits)
 * - Multi-sensor management
 *
 * @par Test Architecture:
 * @dot
 * digraph mock_hcsr04 {
 *   rankdir=LR;
 *   test [label="Obstacle\nDetection Tests"];
 *   mock [label="Mock HC-SR04\n(simulated echo)"];
 *   real [label="Real HC-SR04\nSensor", style=dashed];
 *   test -> mock [label="Set distance"];
 *   mock -> real [style=dashed, label="(replaced)"];
 * }
 * @enddot
 *
 * @par Mock Capabilities:
 * | Feature | Supported | Description |
 * |---------|-----------|-------------|
 * | Echo simulation | Yes | Configurable pulse width for distance |
 * | Timeout simulation | Yes | No echo (out of range) |
 * | Call tracking | Yes | Verifies trigger sequences |
 * | Error injection | Yes | Timeout, invalid range |
 *
 * @par Usage: tests/test_rx_hcsr04.c, tests/test_rx_obstacle_detect.c
 *
 * @see rx_hcsr04.h Real HC-SR04 driver
 * @see rx_obstacle_detect.h Obstacle detection module
 * @see tests/test_rx_hcsr04.c Ultrasonic sensor tests
 *
 * @par NASA Power of 10: [OK] Static allocation, timeout bounds
 * @par SOLID: D - Dependency Inversion
 *
 * @author Locked, Inc.
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief Mock configuration constants
 */
typedef enum : uint8_t {
  k_mock_hcsr04_max_calls     = 64, /**< Maximum call records to track */
  k_mock_hcsr04_func_name_max = 32, /**< Max function name length */
  k_mock_hcsr04_max_pins      = 8,  /**< Max pins per sensor tracked */
} mock_hcsr04_constants_t;

/* =============================================================================
 * Call Record Structure
 * =============================================================================
 */

/**
 * @brief Record of a single function call
 */
typedef struct {
  char     function[k_mock_hcsr04_func_name_max]; /**< Function name */
  uint8_t  port;                                  /**< Port argument */
  uint8_t  pin;                                   /**< Pin argument */
  bool     value;                                 /**< Value (for write/read) */
  rx_err_t return_value;                          /**< Return value */
} mock_hcsr04_call_t;

/* =============================================================================
 * Mock Hardware State
 * =============================================================================
 */

/**
 * @brief Mock HC-SR04 hardware state structure
 */
typedef struct {
  /* Initialization state */
  bool initialized; /**< Mock is initialized */

  /* GPIO simulation */
  bool trigger_pin_state; /**< Current trigger output level */
  bool echo_pin_state;    /**< Current echo input level (simulated) */

  /* Echo timing simulation */
  uint32_t simulated_echo_us;    /**< Echo pulse width to simulate (us) */
  uint32_t current_time_us;      /**< Simulated current time (us) */
  uint32_t last_trigger_time_us; /**< Time when the most recent trigger pulse completed */
  bool     auto_advance_time;    /**< Auto-advance time on timing calls */
  uint32_t time_step_us;         /**< Time step per auto-advance */

  /* Error injection */
  bool inject_timeout;           /**< Simulate timeout (no echo) */
  bool inject_gpio_error;        /**< Simulate GPIO set_output failure */
  bool inject_gpio_input_error;  /**< Simulate GPIO set_input failure */
  bool inject_gpio_read_error;   /**< Simulate GPIO read failure */
  bool     inject_gpio_write_error;            /**< Simulate GPIO write_low failure (all calls) */
  bool     inject_gpio_write_high_error;       /**< Simulate GPIO write_high failure */
  uint32_t gpio_write_low_fail_after_count;   /**< Fail write_low after this many successes (0=disabled) */
  bool     inject_gpio_deinit_error;           /**< Simulate GPIO deinit failure on all calls */
  uint32_t gpio_deinit_fail_after_count;      /**< Fail gpio_deinit after this many successes (0 = disabled) */
  bool     inject_pin_conflict;                /**< Simulate pin already reserved */

  /* Call tracking */
  mock_hcsr04_call_t call_history[k_mock_hcsr04_max_calls]; /**< Call log */
  uint32_t           call_count;                            /**< Number of calls recorded */

  /* Call counts by function */
  uint32_t trigger_pulse_count;   /**< Number of trigger pulses sent */
  uint32_t gpio_set_output_count; /**< gpio_set_output calls */
  uint32_t gpio_set_input_count;  /**< gpio_set_input calls */
  uint32_t gpio_write_high_count; /**< gpio_write_high calls */
  uint32_t gpio_write_low_count;  /**< gpio_write_low calls */
  uint32_t gpio_read_count;       /**< gpio_read calls */
  uint32_t gpio_deinit_count;     /**< gpio_deinit calls */

  /* Statistics */
  uint32_t total_delay_us; /**< Total simulated delay */

} mock_hcsr04_hw_t;

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

/**
 * @brief Global mock instance for simple usage
 *
 * Use mock_hcsr04_hw_init(nullptr) to initialize global instance.
 */
extern mock_hcsr04_hw_t g_mock_hcsr04_hw;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock hardware state
 *
 * @param[in] mock Mock instance (NULL = use global)
 */
void mock_hcsr04_hw_init(mock_hcsr04_hw_t* mock);

/**
 * @brief Deinitialize mock hardware state
 *
 * @param[in] mock Mock instance (NULL = use global)
 */
void mock_hcsr04_hw_deinit(mock_hcsr04_hw_t* mock);

/**
 * @brief Reset mock state without full reinit
 *
 * Clears call history and statistics, keeps error injection settings.
 *
 * @param[in] mock Mock instance (NULL = use global)
 */
void mock_hcsr04_hw_reset(mock_hcsr04_hw_t* mock);

/* =============================================================================
 * Echo Simulation Functions
 * =============================================================================
 */

/**
 * @brief Set simulated echo time for distance testing
 *
 * @param[in] mock    Mock instance (NULL = use global)
 * @param[in] echo_us Echo pulse duration in microseconds
 */
void mock_hcsr04_hw_set_echo_time(mock_hcsr04_hw_t* mock, uint32_t echo_us);

/**
 * @brief Set simulated distance (converts to echo time)
 *
 * @param[in] mock        Mock instance (NULL = use global)
 * @param[in] distance_cm Distance in centimeters
 */
void mock_hcsr04_hw_set_distance(mock_hcsr04_hw_t* mock, float distance_cm);

/**
 * @brief Set timeout injection
 *
 * When enabled, simulated echo never goes high (object out of range).
 *
 * @param[in] mock    Mock instance (NULL = use global)
 * @param[in] timeout True to inject timeout
 */
void mock_hcsr04_hw_set_timeout(mock_hcsr04_hw_t* mock, bool timeout);

/* =============================================================================
 * Error Injection Functions
 * =============================================================================
 */

/**
 * @brief Set GPIO error injection
 *
 * @param[in] mock  Mock instance (NULL = use global)
 * @param[in] error True to inject GPIO init failure
 */
void mock_hcsr04_hw_set_gpio_error(mock_hcsr04_hw_t* mock, bool error);

/**
 * @brief Set GPIO set_input error injection (independent of set_output)
 *
 * @param[in] mock  Mock instance (NULL = use global)
 * @param[in] error True to inject GPIO set_input failure
 */
void mock_hcsr04_hw_set_gpio_input_error(mock_hcsr04_hw_t* mock, bool error);

/**
 * @brief Set GPIO read error injection
 *
 * @param[in] mock  Mock instance (NULL = use global)
 * @param[in] error True to inject GPIO read failure
 */
void mock_hcsr04_hw_set_gpio_read_error(mock_hcsr04_hw_t* mock, bool error);

/**
 * @brief Set GPIO write_low error injection
 *
 * @param[in] mock  Mock instance (NULL = use global)
 * @param[in] error True to inject GPIO write_low failure
 */
void mock_hcsr04_hw_set_gpio_write_error(mock_hcsr04_hw_t* mock, bool error);

/**
 * @brief Set GPIO write_high error injection
 *
 * @param[in] mock  Mock instance (NULL = use global)
 * @param[in] error True to inject GPIO write_high failure
 */
void mock_hcsr04_hw_set_gpio_write_high_error(mock_hcsr04_hw_t* mock, bool error);

/**
 * @brief Set GPIO write_low to fail after N successful calls
 *
 * @param[in] mock  Mock instance (NULL = use global)
 * @param[in] count Number of successful write_low calls before failure (0 = disabled)
 */
void mock_hcsr04_hw_set_gpio_write_low_fail_after(mock_hcsr04_hw_t* mock, uint32_t count);

/**
 * @brief Set GPIO deinit error injection
 *
 * @param[in] mock  Mock instance (NULL = use global)
 * @param[in] error True to inject GPIO deinit failure on all calls
 */
void mock_hcsr04_hw_set_gpio_deinit_error(mock_hcsr04_hw_t* mock, bool error);

/**
 * @brief Set GPIO deinit to fail after N successful calls
 *
 * When count > 0, the (count+1)th gpio_deinit call will return k_rx_err_hw_error.
 * Set count to 0 to disable this feature.
 *
 * @param[in] mock  Mock instance (NULL = use global)
 * @param[in] count Number of successful deinit calls before failure
 */
void mock_hcsr04_hw_set_gpio_deinit_fail_after(mock_hcsr04_hw_t* mock, uint32_t count);

/**
 * @brief Set pin conflict injection
 *
 * @param[in] mock     Mock instance (NULL = use global)
 * @param[in] conflict True to inject pin already reserved error
 */
void mock_hcsr04_hw_set_pin_conflict(mock_hcsr04_hw_t* mock, bool conflict);

/* =============================================================================
 * Timing Simulation Functions
 * =============================================================================
 */

/**
 * @brief Set current simulated time
 *
 * @param[in] mock    Mock instance (NULL = use global)
 * @param[in] time_us Current time in microseconds
 */
void mock_hcsr04_hw_set_time(mock_hcsr04_hw_t* mock, uint32_t time_us);

/**
 * @brief Advance simulated time
 *
 * @param[in] mock    Mock instance (NULL = use global)
 * @param[in] delta_us Time to advance in microseconds
 */
void mock_hcsr04_hw_advance_time(mock_hcsr04_hw_t* mock, uint32_t delta_us);

/**
 * @brief Enable auto-advance mode
 *
 * When enabled, time automatically advances on timing-related calls.
 *
 * @param[in] mock          Mock instance (NULL = use global)
 * @param[in] enable        True to enable auto-advance
 * @param[in] step_us       Time step per call (e.g., 1us)
 */
void mock_hcsr04_hw_set_auto_advance(mock_hcsr04_hw_t* mock, bool enable, uint32_t step_us);

/* =============================================================================
 * Call Tracking Functions
 * =============================================================================
 */

/**
 * @brief Check if a function was called
 *
 * @param[in] mock          Mock instance (NULL = use global)
 * @param[in] function_name Function name to check
 *
 * @return true if function was called
 */
bool mock_hcsr04_hw_was_called(mock_hcsr04_hw_t* mock, const char* function_name);

/**
 * @brief Get call count for a function
 *
 * @param[in] mock          Mock instance (NULL = use global)
 * @param[in] function_name Function name to check
 *
 * @return Number of times function was called
 */
uint32_t mock_hcsr04_hw_get_call_count(mock_hcsr04_hw_t* mock, const char* function_name);

/**
 * @brief Get number of trigger pulses sent
 *
 * A trigger pulse is detected as gpio_write_high followed by gpio_write_low
 * on the trigger pin.
 *
 * @param[in] mock Mock instance (NULL = use global)
 *
 * @return Number of trigger pulses detected
 */
uint32_t mock_hcsr04_hw_get_trigger_count(mock_hcsr04_hw_t* mock);

/* =============================================================================
 * Mock GPIO Functions (replace real implementations in tests)
 * =============================================================================
 */

/**
 * @brief Mock gpio_set_output
 *
 * @param[in] port Port number
 * @param[in] pin  Pin number
 *
 * @return k_rx_ok or injected error
 */
rx_err_t mock_gpio_set_output(uint8_t port, uint8_t pin);

/**
 * @brief Mock gpio_set_input
 *
 * @param[in] port Port number
 * @param[in] pin  Pin number
 *
 * @return k_rx_ok or injected error
 */
rx_err_t mock_gpio_set_input(uint8_t port, uint8_t pin);

/**
 * @brief Mock gpio_write_high
 *
 * @param[in] port Port number
 * @param[in] pin  Pin number
 *
 * @return k_rx_ok or injected error
 */
rx_err_t mock_gpio_write_high(uint8_t port, uint8_t pin);

/**
 * @brief Mock gpio_write_low
 *
 * @param[in] port Port number
 * @param[in] pin  Pin number
 *
 * @return k_rx_ok or injected error
 */
rx_err_t mock_gpio_write_low(uint8_t port, uint8_t pin);

/**
 * @brief Mock gpio_read
 *
 * @param[in]  port  Port number
 * @param[in]  pin   Pin number
 * @param[out] value Pin value (simulated based on timing)
 *
 * @return k_rx_ok or injected error
 */
rx_err_t mock_gpio_read(uint8_t port, uint8_t pin, bool* value);

/**
 * @brief Mock gpio_deinit
 *
 * @param[in] port Port number
 * @param[in] pin  Pin number
 *
 * @return k_rx_ok
 */
rx_err_t mock_gpio_deinit(uint8_t port, uint8_t pin);

/* =============================================================================
 * Mock Timing Functions
 * =============================================================================
 */

/**
 * @brief Mock microsecond delay
 *
 * @param[in] us Microseconds to delay
 */
void mock_delay_us(uint32_t us);

/**
 * @brief Mock get current time in microseconds
 *
 * @return Simulated current time
 */
uint32_t mock_get_time_us(void);

#ifdef __cplusplus
}
#endif
