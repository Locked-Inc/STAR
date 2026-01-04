/* tests/mocks/mock_hcsr04_hw.h */

/**
 * @file mock_hcsr04_hw.h
 * @brief Mock HC-SR04 Hardware Layer for Host-Side Testing
 *
 * @details
 * Mock implementation of GPIO and timing for HC-SR04 driver testing.
 * Provides call tracking, echo timing simulation, and error injection.
 *
 * Features:
 * - Simulates GPIO trigger/echo pin behavior
 * - Configurable echo pulse timing for distance simulation
 * - Call tracking to verify trigger pulse generation
 * - Error injection for timeout and range error testing
 * - No actual hardware access (runs on host)
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_HCSR04_HW_H
#define MOCK_HCSR04_HW_H

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
typedef enum {
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
  uint32_t simulated_echo_us; /**< Echo pulse width to simulate (us) */
  uint32_t current_time_us;   /**< Simulated current time (us) */
  bool     auto_advance_time; /**< Auto-advance time on timing calls */
  uint32_t time_step_us;      /**< Time step per auto-advance */

  /* Error injection */
  bool inject_timeout;      /**< Simulate timeout (no echo) */
  bool inject_gpio_error;   /**< Simulate GPIO init failure */
  bool inject_pin_conflict; /**< Simulate pin already reserved */

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
 * Use mock_hcsr04_hw_init(NULL) to initialize global instance.
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

#endif /* MOCK_HCSR04_HW_H */
