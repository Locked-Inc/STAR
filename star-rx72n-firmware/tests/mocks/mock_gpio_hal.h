/* tests/mocks/mock_gpio_hal.h */

/**
 * @file mock_gpio_hal.h
 * @brief Mock GPIO HAL Functions for Unit Testing
 *
 * Provides mock GPIO HAL functions with state tracking for testing
 * higher-level code without actual hardware.
 *
 * Features:
 * - Per-pin state tracking (direction, output value)
 * - Error injection support
 * - Call history recording
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_GPIO_HAL_H
#define MOCK_GPIO_HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Mock GPIO constants */
typedef enum {
  k_mock_gpio_max_ports         = 20, /**< Maximum port numbers */
  k_mock_gpio_pins_per_port     = 8,  /**< Pins per port */
  k_mock_gpio_call_history_size = 64, /**< Call history buffer size */
} mock_gpio_constants_t;

/** @brief GPIO direction enum for mock tracking */
typedef enum {
  k_mock_gpio_dir_undefined = 0, /**< Direction not set */
  k_mock_gpio_dir_input     = 1, /**< Input direction */
  k_mock_gpio_dir_output    = 2, /**< Output direction */
} mock_gpio_direction_t;

/* =============================================================================
 * Types
 * =============================================================================
 */

/** @brief GPIO HAL function call types */
typedef enum {
  k_mock_gpio_call_set_output,
  k_mock_gpio_call_set_input,
  k_mock_gpio_call_write_high,
  k_mock_gpio_call_write_low,
  k_mock_gpio_call_toggle,
  k_mock_gpio_call_read,
} mock_gpio_call_type_t;

/** @brief GPIO HAL function call record */
typedef struct {
  mock_gpio_call_type_t type; /**< Call type */
  rx_port_pin_t         pin;  /**< Pin that was operated on */
} mock_gpio_call_t;

/** @brief Per-pin GPIO state */
typedef struct {
  mock_gpio_direction_t direction;    /**< Current direction */
  bool                  output_value; /**< Current output value */
  bool                  input_value;  /**< Simulated input value */
} mock_gpio_pin_state_t;

/** @brief Global mock GPIO state */
typedef struct {
  mock_gpio_pin_state_t pins[k_mock_gpio_max_ports][k_mock_gpio_pins_per_port]; /**< Pin states */
  mock_gpio_call_t      call_history[k_mock_gpio_call_history_size];            /**< Call history */
  uint16_t              call_count; /**< Number of calls recorded */
  rx_err_t              next_error; /**< Error to return on next call */
  bool                  error_set;  /**< Whether error injection is active */
} mock_gpio_state_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

/** @brief Global mock GPIO state instance */
extern mock_gpio_state_t g_mock_gpio;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock GPIO state
 *
 * Resets all pin states and clears call history.
 */
void mock_gpio_init(void);

/**
 * @brief Reset mock GPIO state (alias for init)
 */
void mock_gpio_reset(void);

/* =============================================================================
 * Test Setup Functions
 * =============================================================================
 */

/**
 * @brief Set simulated input value for a pin
 *
 * @param[in] pin GPIO pin
 * @param[in] value Simulated input value (true=high, false=low)
 */
void mock_gpio_set_input_value(rx_port_pin_t pin, bool value);

/**
 * @brief Set error to return on next GPIO HAL call
 *
 * @param[in] err Error code to return
 */
void mock_gpio_set_next_error(rx_err_t err);

/**
 * @brief Clear any pending error injection
 */
void mock_gpio_clear_error(void);

/* =============================================================================
 * State Inspection Functions
 * =============================================================================
 */

/**
 * @brief Get pin direction
 *
 * @param[in] pin GPIO pin
 *
 * @return Pin direction
 */
mock_gpio_direction_t mock_gpio_get_direction(rx_port_pin_t pin);

/**
 * @brief Get pin output value
 *
 * @param[in] pin GPIO pin
 *
 * @return Output value (true=high, false=low)
 */
bool mock_gpio_get_output_value(rx_port_pin_t pin);

/* =============================================================================
 * Call History Functions
 * =============================================================================
 */

/**
 * @brief Get call history entry
 *
 * @param[in] index Index in call history
 *
 * @return Pointer to call record, or NULL if index out of range
 */
const mock_gpio_call_t* mock_gpio_get_call(uint16_t index);

/**
 * @brief Get total number of recorded calls
 *
 * @return Number of calls in history
 */
uint16_t mock_gpio_get_call_count(void);

/**
 * @brief Clear call history
 */
void mock_gpio_clear_history(void);

/* =============================================================================
 * GPIO HAL Function Declarations (for test linking)
 * =============================================================================
 */

rx_err_t gpio_set_output(rx_port_pin_t pin);
rx_err_t gpio_set_input(rx_port_pin_t pin);
rx_err_t gpio_write_high(rx_port_pin_t pin);
rx_err_t gpio_write_low(rx_port_pin_t pin);
rx_err_t gpio_toggle(rx_port_pin_t pin);
rx_err_t gpio_read(rx_port_pin_t pin, bool* value);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_GPIO_HAL_H */
