/**
 * @file rx_obstacle_detect.h
 * @brief Basic obstacle detection and emergency stop using HC-SR04 sensors.
 *
 * This module monitors a set of HC-SR04 ultrasonic sensors and triggers an
 * emergency stop if an obstacle is detected within a configurable threshold.
 * It is designed as a simple safety mechanism to prevent collisions.
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef RX_OBSTACLE_DETECT_H
#define RX_OBSTACLE_DETECT_H

#include "rx_hcsr04.h"
#include "rx_error.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Maximum number of sensors that can be managed by this module.
 */
#define RX_OBSTACLE_DETECT_MAX_SENSORS 4

/**
 * @brief Callback function type for when an obstacle is detected.
 *
 * @param sensor_index The index of the sensor that detected the obstacle.
 * @param distance_cm The measured distance in centimeters.
 * @param user_data Optional user data passed to the init function.
 */
typedef void (*rx_obstacle_detect_callback_t)(uint8_t sensor_index, float distance_cm, void* user_data);

/**
 * @brief Configuration for the obstacle detection module.
 */
typedef struct {
    rx_hcsr04_t* sensors[RX_OBSTACLE_DETECT_MAX_SENSORS]; ///< Array of pointers to initialized HC-SR04 handles.
    uint8_t num_sensors;                                 ///< Number of sensors in the `sensors` array.
    rx_motor_handle_t* motors[RX_OBSTACLE_DETECT_MAX_SENSORS]; ///< Array of pointers to initialized motor handles.
    uint8_t num_motors;                                  ///< Number of motors in the `motors` array.
    float detection_threshold_cm;                        ///< Distance in cm to trigger an obstacle detection.
    uint8_t debounce_samples;                            ///< Number of consecutive readings required to confirm an obstacle.
    uint32_t poll_interval_ms;                           ///< Interval in milliseconds to poll the sensors.
    rx_obstacle_detect_callback_t callback;              ///< Callback function to invoke when an obstacle is detected.
    void* user_data;                                     ///< Optional user data for the callback.
} rx_obstacle_detect_config_t;

/**
 * @brief Initializes the obstacle detection module.
 *
 * This function sets up the polling task and prepares the module for operation.
 * It does not start the sensor measurements automatically.
 *
 * @param config Pointer to the configuration structure.
 * @return k_rx_ok on success, or an error code on failure.
 */
rx_err_t rx_obstacle_detect_init(const rx_obstacle_detect_config_t* config);

/**
 * @brief Starts the obstacle detection polling.
 *
 * @return k_rx_ok on success, or an error code on failure.
 */
rx_err_t rx_obstacle_detect_start(void);

/**
 * @brief Stops the obstacle detection polling.
 *
 * @return k_rx_ok on success, or an error code on failure.
 */
rx_err_t rx_obstacle_detect_stop(void);

/**
 * @brief Checks if an obstacle is currently detected.
 *
 * @return True if an obstacle is present, false otherwise.
 */
bool rx_obstacle_detect_is_obstacle_present(void);

#endif // RX_OBSTACLE_DETECT_H
