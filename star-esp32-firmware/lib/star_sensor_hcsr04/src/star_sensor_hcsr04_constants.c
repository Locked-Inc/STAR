/* lib/star_sensor_hcsr04/src/star_sensor_hcsr04_constants.c */

/**
 * @file star_sensor_hcsr04_constants.c
 * @brief Default configuration values for HC-SR04 ultrasonic sensor
 */

#include "star_sensor_hcsr04_constants.h"

/**
 * @brief Default minimum distance measurement in cm
 * Can be overridden via hcsr04_config_t during initialization
 */
const uint32_t g_hcsr04_min_distance_cm = 2U;

/**
 * @brief Default maximum distance measurement in cm
 * Can be overridden via hcsr04_config_t during initialization
 */
const uint32_t g_hcsr04_max_distance_cm = 400U;

/**
 * @brief Speed of sound in cm/us at 25°C
 * Temperature compensation applied in driver via config.temperature_c
 */
const float g_hcsr04_speed_of_sound_cm_us = 0.0343f;

/**
 * @brief Default timeout for echo pulse in microseconds
 * Can be overridden via hcsr04_config_t during initialization
 */
const uint32_t g_hcsr04_timeout_us = 23200U;

/**
 * @brief Setup time before trigger pulse (microseconds)
 */
const uint32_t g_hcsr04_trigger_setup_us = 2U;

/**
 * @brief Duration of trigger pulse (microseconds)
 */
const uint32_t g_hcsr04_trigger_pulse_us = 10U;

/**
 * @brief Short polling delay (microseconds)
 */
const uint32_t g_hcsr04_poll_delay_short_us = 10U;

/**
 * @brief Medium polling delay (microseconds)
 */
const uint32_t g_hcsr04_poll_delay_med_us = 50U;

/**
 * @brief Overall measurement timeout (milliseconds)
 */
const uint32_t g_hcsr04_measurement_timeout_ms = 60U;