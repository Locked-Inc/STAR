/* lib/rx_obstacle_detect/inc/rx_obstacle_detect.h */

/**
 * @file rx_obstacle_detect.h
 * @brief Basic obstacle detection with emergency stop for RX72N
 *
 * @details
 * Provides basic obstacle detection using HC-SR04 ultrasonic sensors with
 * immediate emergency stop of motors when obstacles are detected within a
 * configurable threshold distance.
 *
 * This is a simplified safety system focused on emergency stop, not full
 * Dynamic Window Approach (DWA) path planning. The module polls configured
 * HC-SR04 sensors at a regular interval and immediately stops all configured
 * motors when any sensor detects an obstacle within the threshold distance.
 *
 * Features:
 * - Configurable detection threshold (e.g., 30cm)
 * - Debouncing to avoid false positives
 * - Support for multiple sensors (front, sides, back)
 * - Configurable polling rate (e.g., 50Hz)
 * - Event logging
 * - ThreadX task for autonomous operation
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_OBSTACLE_DETECT_H
#define STAR_RX_OBSTACLE_DETECT_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_hcsr04.h"
#include "rx_motor.h"
#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief Obstacle detection constants
 */
typedef enum {
  k_obstacle_detect_max_sensors = 8,  /**< Maximum number of HC-SR04 sensors */
  k_obstacle_detect_max_motors  = 4,  /**< Maximum number of motors to control */
  k_obstacle_detect_task_stack_size = 2048, /**< Detection task stack size (bytes) */
  k_obstacle_detect_task_priority   = 8,    /**< Detection task priority */
} rx_obstacle_detect_constants_t;

/**
 * @brief Obstacle detection state
 */
typedef enum {
  k_obstacle_detect_state_stopped  = 0, /**< Detection not running */
  k_obstacle_detect_state_running  = 1, /**< Detection active, no obstacle */
  k_obstacle_detect_state_obstacle = 2, /**< Obstacle detected, motors stopped */
} rx_obstacle_detect_state_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief Obstacle detection event callback
 *
 * Called when obstacle state changes (detected or cleared).
 *
 * @param[in] obstacle_detected True if obstacle detected, false if cleared
 * @param[in] sensor_idx        Index of sensor that triggered (0-based)
 * @param[in] distance_cm       Distance measured in centimeters
 * @param[in] user_data         User context passed during init
 */
typedef void (*rx_obstacle_detect_callback_t)(bool     obstacle_detected,
                                              uint8_t  sensor_idx,
                                              float    distance_cm,
                                              void*    user_data);

/**
 * @brief Obstacle detection configuration
 *
 * Configures detection parameters including sensors, motors, thresholds,
 * and debouncing.
 */
typedef struct {
  /* Sensor configuration */
  rx_hcsr04_t** sensors;       /**< Array of HC-SR04 sensor handles (required) */
  uint8_t       sensor_count;  /**< Number of sensors (1 to k_obstacle_detect_max_sensors) */

  /* Motor configuration */
  rx_motor_handle_t** motors;       /**< Array of motor handles to stop (required) */
  uint8_t             motor_count;  /**< Number of motors (1 to k_obstacle_detect_max_motors) */

  /* Detection parameters */
  float    detection_threshold_cm; /**< Stop distance threshold (e.g., 30.0cm) */
  uint32_t debounce_samples;       /**< Consecutive readings to confirm (e.g., 3) */
  uint32_t poll_interval_ms;       /**< Sensor polling interval (e.g., 20ms = 50Hz) */

  /* Optional callback */
  rx_obstacle_detect_callback_t callback;   /**< Event callback (can be NULL) */
  void*                         user_data;  /**< User context for callback (can be NULL) */
} rx_obstacle_detect_config_t;

/**
 * @brief Obstacle detection handle
 *
 * Maintains detection state and statistics. Caller allocates and passes to
 * init. Do not modify fields directly after initialization.
 */
typedef struct {
  /* Configuration (copied from config) */
  rx_hcsr04_t*        sensors[k_obstacle_detect_max_sensors]; /**< Sensor handles */
  uint8_t             sensor_count;                           /**< Number of sensors */
  rx_motor_handle_t*  motors[k_obstacle_detect_max_motors];  /**< Motor handles */
  uint8_t             motor_count;                            /**< Number of motors */
  float               detection_threshold_cm; /**< Stop distance threshold */
  uint32_t            debounce_samples;       /**< Debounce sample count */
  uint32_t            poll_interval_ms;       /**< Polling interval */
  rx_obstacle_detect_callback_t callback;     /**< Event callback */
  void*                         user_data;    /**< User context */

  /* ThreadX resources */
  TX_THREAD thread;                                         /**< Detection task thread */
  uint8_t   thread_stack[k_obstacle_detect_task_stack_size]; /**< Task stack */
  TX_EVENT_FLAGS_GROUP event_flags;                         /**< Control event flags */

  /* State */
  bool                        initialized;     /**< True if initialized */
  rx_obstacle_detect_state_t  state;           /**< Current detection state */
  bool                        stop_requested;  /**< True if stop requested */

  /* Debouncing state (per-sensor) */
  uint32_t debounce_counter[k_obstacle_detect_max_sensors]; /**< Consecutive detections */
  bool     obstacle_active[k_obstacle_detect_max_sensors];  /**< Per-sensor obstacle state */

  /* Statistics */
  uint32_t total_polls;         /**< Total sensor polls performed */
  uint32_t obstacle_events;     /**< Total obstacle detection events */
  uint32_t false_positive_count; /**< Debounced false positives */
} rx_obstacle_detect_t;

/* =============================================================================
 * Public API - Initialization
 * =============================================================================
 */

/**
 * @brief Initialize obstacle detection system
 *
 * Configures the obstacle detection system with specified sensors and motors.
 * Creates a ThreadX task that polls sensors at the configured interval and
 * stops motors immediately when an obstacle is detected within the threshold.
 *
 * The detection task starts in stopped state. Call rx_obstacle_detect_start()
 * to begin monitoring.
 *
 * @param[out] handle Handle to initialize (caller-allocated)
 * @param[in]  config Configuration with sensors, motors, and parameters
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or config is NULL
 * @return k_rx_err_invalid_arg if config is invalid (sensor/motor count, threshold)
 * @return k_rx_err_invalid_state if handle already initialized
 * @return k_rx_err_rtos_error if ThreadX resources fail to create
 */
rx_err_t rx_obstacle_detect_init(rx_obstacle_detect_t*              handle,
                                 const rx_obstacle_detect_config_t* config);

/**
 * @brief Deinitialize obstacle detection system
 *
 * Stops detection task, releases ThreadX resources, and resets handle state.
 * Does NOT deinitialize the sensor or motor handles (caller's responsibility).
 *
 * @param[in,out] handle Obstacle detection handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_obstacle_detect_deinit(rx_obstacle_detect_t* handle);

/* =============================================================================
 * Public API - Control
 * =============================================================================
 */

/**
 * @brief Start obstacle detection monitoring
 *
 * Starts the detection task to begin polling sensors. If an obstacle is
 * already detected, motors will be stopped immediately.
 *
 * @param[in,out] handle Obstacle detection handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_obstacle_detect_start(rx_obstacle_detect_t* handle);

/**
 * @brief Stop obstacle detection monitoring
 *
 * Stops the detection task. Motors will NOT be automatically restarted even
 * if they were previously stopped due to obstacle detection.
 *
 * @param[in,out] handle Obstacle detection handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_obstacle_detect_stop(rx_obstacle_detect_t* handle);

/**
 * @brief Resume motors after obstacle cleared
 *
 * Clears the obstacle state and allows motors to be controlled again.
 * This is a manual override - the user must verify the obstacle is truly
 * cleared before calling this function.
 *
 * If detection is still running and obstacle is still present, motors
 * will be stopped again on the next poll cycle.
 *
 * @param[in,out] handle Obstacle detection handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_obstacle_detect_clear_obstacle(rx_obstacle_detect_t* handle);

/* =============================================================================
 * Public API - Status
 * =============================================================================
 */

/**
 * @brief Get current detection state
 *
 * Returns the current state of the obstacle detection system.
 *
 * @param[in]  handle    Obstacle detection handle
 * @param[out] out_state Current state
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle or out_state is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_obstacle_detect_get_state(const rx_obstacle_detect_t*  handle,
                                      rx_obstacle_detect_state_t*  out_state);

/**
 * @brief Check if obstacle is currently detected
 *
 * Returns true if any sensor currently detects an obstacle within the
 * threshold distance (after debouncing).
 *
 * @param[in] handle Obstacle detection handle
 *
 * @return true if obstacle detected
 * @return false if no obstacle or handle is NULL/invalid
 */
bool rx_obstacle_detect_is_obstacle_detected(const rx_obstacle_detect_t* handle);

/**
 * @brief Get obstacle detection statistics
 *
 * Returns statistics about detection performance and events.
 *
 * @param[in]  handle              Obstacle detection handle
 * @param[out] out_total_polls     Total sensor polls (can be NULL)
 * @param[out] out_obstacle_events Total obstacle events (can be NULL)
 * @param[out] out_false_positives Debounced false positives (can be NULL)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_obstacle_detect_get_stats(const rx_obstacle_detect_t* handle,
                                      uint32_t*                   out_total_polls,
                                      uint32_t*                   out_obstacle_events,
                                      uint32_t*                   out_false_positives);

/**
 * @brief Reset statistics counters
 *
 * Resets all statistics counters to zero.
 *
 * @param[in,out] handle Obstacle detection handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if handle is NULL
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_obstacle_detect_reset_stats(rx_obstacle_detect_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_OBSTACLE_DETECT_H */
