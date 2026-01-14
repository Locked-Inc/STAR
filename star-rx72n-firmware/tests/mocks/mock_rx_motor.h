/* tests/mocks/mock_rx_motor.h */

/**
 * @file mock_rx_motor.h
 * @brief Mock Motor Control for Unit Testing
 *
 * @details
 * Provides mock implementation of motor control functions for testing
 * obstacle detection without requiring actual hardware or GPTW PWM peripherals.
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_MOCK_RX_MOTOR_H
#define STAR_MOCK_RX_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_motor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Initialize motor mock
 */
void mock_rx_motor_init(void);

/**
 * @brief Deinitialize motor mock
 */
void mock_rx_motor_deinit(void);

/**
 * @brief Set return value for next motor_stop call
 * @param[in] ret_val Return value to use
 */
void mock_rx_motor_set_stop_return(rx_err_t ret_val);

/**
 * @brief Check if motor_stop was called
 * @param[in] motor_handle Motor handle to check (NULL = any motor)
 * @return True if stop was called for this motor
 */
bool mock_rx_motor_was_stop_called(const rx_motor_handle_t* motor_handle);

/**
 * @brief Get number of times motor_stop was called
 * @param[in] motor_handle Motor handle to check (NULL = all motors)
 * @return Number of stop calls
 */
uint32_t mock_rx_motor_get_stop_count(const rx_motor_handle_t* motor_handle);

/**
 * @brief Reset mock state
 */
void mock_rx_motor_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_MOCK_RX_MOTOR_H */
