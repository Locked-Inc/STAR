/* include/env_monitor_config.h */

/**
 * @file env_monitor_config.h
 * @brief Environment monitoring configuration constants
 * @details
 * Defines obstacle detection and temperature monitoring thresholds.
 * Used by Env_Monitor task.
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_ENV_MONITOR_CONFIG_H
#define STAR_RX72N_ENV_MONITOR_CONFIG_H

/* =============================================================================
 * Obstacle Detection Configuration
 * =============================================================================
 */

/**
 * @brief HC-SR04 ultrasonic sensor configuration
 * @details Obstacle detection thresholds
 */
typedef enum {
  k_hcsr04_count            = 4,  /**< Number of HC-SR04 sensors */
  k_obstacle_threshold_cm   = 30, /**< Obstacle detection threshold (cm) */
  k_obstacle_scan_hz        = 50, /**< Scan rate (Hz) */
  k_obstacle_scan_period_ms = 20, /**< Scan period (ms) = 1000/50 */
} obstacle_config_t;

/* =============================================================================
 * Temperature Monitoring Configuration
 * =============================================================================
 */

/**
 * @brief DS18B20 temperature sensor configuration
 * @details Thermal protection thresholds
 */
typedef enum {
  k_temp_warning_threshold_c  = 70, /**< Thermal warning (°C) */
  k_temp_shutdown_threshold_c = 85, /**< Thermal shutdown (°C) */
  k_temp_sample_hz            = 50, /**< Sample rate (Hz) */
} temperature_config_t;

#endif /* STAR_RX72N_ENV_MONITOR_CONFIG_H */
