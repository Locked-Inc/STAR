/* star-rx72n-firmware/tests/mocks/mock_rx_obstacle_detect.h */

/**
 * @file mock_rx_obstacle_detect.h
 * @brief Mock obstacle detection module for autonomous navigation testing
 *
 * @details
 * Provides test double for obstacle detection module (HC-SR04 ultrasonic sensors)
 * to enable unit testing of autonomous navigation, collision avoidance, and
 * emergency stop logic without actual sensors.
 *
 * Enables testing of: Multi-sensor distance monitoring (4 sensors: front, back,
 * left, right), Collision avoidance algorithms, Emergency stop conditions,
 * Sensor fusion logic, Timeout/out-of-range handling
 *
 * @par Mock Capabilities: Configurable distance values per sensor, Obstacle
 * detection simulation (binary detected/clear), Error injection (sensor timeout,
 * invalid readings), Call tracking
 *
 * @par Usage: tests/test_rx_obstacle_detect.c, tests/test_navigation_task.c
 * @see rx_obstacle_detect.h Real obstacle detection module
 * @see mock_hcsr04_hw.h HC-SR04 hardware mock
 * @par NASA Power of 10: [OK] Static allocation, bounded loops
 * @par SOLID: D - Navigation depends on obstacle detection interface
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum mock_obstacle_detect_constants_t
 * @brief Mock obstacle detection constants
 */
typedef enum : uint8_t {
  k_mock_obstacle_max_sensors = 8, /**< Maximum tracked sensors */
} mock_obstacle_detect_constants_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @typedef rx_obstacle_detect_callback_t
 * @brief Obstacle detection event callback
 */
typedef void (*rx_obstacle_detect_callback_t)(bool    obstacle_detected,
                                              uint8_t sensor_idx,
                                              float   distance_cm,
                                              void*   user_data);

/**
 * @struct rx_obstacle_detect_config_t
 * @brief Obstacle detection configuration
 */
typedef struct {
  uint8_t                       sensor_count;           /**< Number of sensors */
  float                         detection_threshold_cm; /**< Detection threshold (cm) */
  uint32_t                      debounce_samples;       /**< Debounce sample count */
  uint32_t                      poll_interval_ms;       /**< Polling interval (ms) */
  rx_obstacle_detect_callback_t callback;               /**< Event callback */
  void*                         user_data;              /**< User context */
} rx_obstacle_detect_config_t;

/**
 * @struct rx_obstacle_detect_t
 * @brief Obstacle detection handle
 */
typedef struct {
  uint8_t                       sensor_count;           /**< Number of sensors */
  float                         detection_threshold_cm; /**< Detection threshold */
  uint32_t                      debounce_samples;       /**< Debounce count */
  uint32_t                      poll_interval_ms;       /**< Poll interval */
  rx_obstacle_detect_callback_t callback;               /**< Event callback */
  void*                         user_data;              /**< User context */
  bool                          initialized;            /**< Init flag */
  bool                          running;                /**< Running flag */
} rx_obstacle_detect_t;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Reset all mock obstacle detection state
 */
void mock_obstacle_detect_reset(void);

/**
 * @brief Set return value for rx_obstacle_detect_init()
 */
void mock_obstacle_detect_set_init_return(rx_err_t err);

/**
 * @brief Set return value for rx_obstacle_detect_start()
 */
void mock_obstacle_detect_set_start_return(rx_err_t err);

/**
 * @brief Set return value for rx_obstacle_detect_stop()
 */
void mock_obstacle_detect_set_stop_return(rx_err_t err);

/**
 * @brief Trigger obstacle callback (simulates detection)
 *
 * @param[in] detected  True if obstacle detected
 * @param[in] sensor    Sensor index
 * @param[in] distance  Distance in cm
 */
void mock_obstacle_detect_trigger_callback(bool detected, uint8_t sensor, float distance);

/**
 * @brief Set stats values for rx_obstacle_detect_get_stats()
 */
void mock_obstacle_detect_set_stats(uint32_t polls, uint32_t events, uint32_t false_pos);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

uint32_t mock_obstacle_detect_get_init_count(void);
uint32_t mock_obstacle_detect_get_start_count(void);
uint32_t mock_obstacle_detect_get_stop_count(void);
bool     mock_obstacle_detect_was_initialized(void);

/* =============================================================================
 * Mock Obstacle Detection API
 * =============================================================================
 */

rx_err_t rx_obstacle_detect_init(rx_obstacle_detect_t*              handle,
                                 const rx_obstacle_detect_config_t* config);

rx_err_t rx_obstacle_detect_deinit(rx_obstacle_detect_t* handle);

rx_err_t rx_obstacle_detect_start(rx_obstacle_detect_t* handle);

rx_err_t rx_obstacle_detect_stop(rx_obstacle_detect_t* handle);

rx_err_t rx_obstacle_detect_get_stats(rx_obstacle_detect_t* handle,
                                      uint32_t*             total_polls,
                                      uint32_t*             obstacle_events,
                                      uint32_t*             false_positives);

#ifdef __cplusplus
}
#endif
