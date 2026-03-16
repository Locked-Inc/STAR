/**
 * @file mock_rx_motor.h
 * @brief Mock motor control driver for high-level testing without hardware
 *
 * @details
 * Provides test double for rx_motor driver to enable unit testing of higher-level
 * modules (obstacle detection, navigation) without requiring actual motors,
 * H-bridge drivers, or PWM peripherals.
 *
 * Enables testing of:
 * - Motor control integration (obstacle detection -> motor commands)
 * - Velocity command sequences
 * - Emergency stop logic
 * - Multi-motor coordination
 * - Motor state tracking
 *
 * @par Test Architecture:
 * @dot
 * digraph mock_motor {
 *   rankdir=LR;
 *   test [label="Obstacle Detection\nNavigation Tests"];
 *   mock [label="Mock Motor\n(velocity tracking)"];
 *   real [label="Real Motor\nDRV8263H+PWM", style=dashed];
 *   test -> mock [label="Set velocity"];
 *   mock -> real [style=dashed, label="(replaced)"];
 * }
 * @enddot
 *
 * @par Mock Capabilities:
 * | Feature | Supported | Description |
 * |---------|-----------|-------------|
 * | Velocity tracking | Yes | Records commanded velocities |
 * | Multi-motor | Yes | 4 independent motors |
 * | Call history | Yes | Verifies command sequences |
 *
 * @par Usage: tests/test_rx_obstacle_detect.c
 *
 * @see rx_motor.h Real motor control driver
 * @see tests/test_rx_obstacle_detect.c Obstacle detection tests
 *
 * @par NASA Power of 10: [OK] Static allocation
 * @par SOLID: D - High-level modules depend on motor interface
 *
 * @author Locked, Inc.
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

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
