/* lib/star_sensor_hcsr04/include/star_sensor_hcsr04_constants.h */

#ifndef STAR_SENSOR_HCSR04_CONSTANTS_H
#define STAR_SENSOR_HCSR04_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_hcsr04_constants.h
 * @brief Type-safe constants for HC-SR04 ultrasonic sensor
 *
 * This file provides type-safe constant alternatives to macros for the HC-SR04 sensor.
 * These constants provide better type safety and are visible in debuggers.
 */

/**
 * @brief Minimum measurable distance in centimeters
 *
 * The HC-SR04 sensor cannot accurately measure distances below 2cm due to
 * the physical separation between transmitter and receiver.
 */
extern const uint32_t g_hcsr04_min_distance_cm;

/**
 * @brief Maximum measurable distance in centimeters
 *
 * The HC-SR04 sensor has a maximum range of 400cm. Beyond this distance,
 * the echo signal becomes too weak to reliably detect.
 */
extern const uint32_t g_hcsr04_max_distance_cm;

/**
 * @brief Speed of sound in cm/microsecond at 20°C
 *
 * This is the nominal speed of sound used for distance calculations.
 * The actual speed varies with temperature and is calculated as:
 * speed = (331.4 + 0.606 * temperature_c) / 10000.0
 */
extern const float g_hcsr04_speed_of_sound_cm_us;

/**
 * @brief Maximum echo timeout in microseconds
 *
 * This timeout corresponds to the maximum measurable distance (400cm).
 * Calculation: 400cm * 2 (round trip) / 0.0343 cm/us = ~23200us
 */
extern const uint32_t g_hcsr04_timeout_us;

/**
 * @brief Trigger pulse setup delay in microseconds
 *
 * Short delay before the trigger pulse to ensure GPIO is stable.
 */
extern const uint32_t g_hcsr04_trigger_setup_us;

/**
 * @brief Trigger pulse width in microseconds
 *
 * The HC-SR04 requires a 10us trigger pulse to initiate measurement.
 */
extern const uint32_t g_hcsr04_trigger_pulse_us;

/**
 * @brief Short polling delay in microseconds during measurement wait
 */
extern const uint32_t g_hcsr04_poll_delay_short_us;

/**
 * @brief Medium polling delay in microseconds during measurement wait
 */
extern const uint32_t g_hcsr04_poll_delay_med_us;

/**
 * @brief Measurement timeout in milliseconds
 *
 * Maximum time to wait for a measurement to complete.
 */
extern const uint32_t g_hcsr04_measurement_timeout_ms;

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_HCSR04_CONSTANTS_H */