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
extern const uint32_t STAR_HCSR04_MIN_DISTANCE_CM;

/**
 * @brief Maximum measurable distance in centimeters
 * 
 * The HC-SR04 sensor has a maximum range of 400cm. Beyond this distance,
 * the echo signal becomes too weak to reliably detect.
 */
extern const uint32_t STAR_HCSR04_MAX_DISTANCE_CM;

/**
 * @brief Speed of sound in cm/microsecond at 20°C
 * 
 * This is the nominal speed of sound used for distance calculations.
 * The actual speed varies with temperature and is calculated as:
 * speed = (331.4 + 0.606 * temperature_c) / 10000.0
 */
extern const float STAR_HCSR04_SPEED_OF_SOUND_CM_US;

/**
 * @brief Maximum echo timeout in microseconds
 * 
 * This timeout corresponds to the maximum measurable distance (400cm).
 * Calculation: 400cm * 2 (round trip) / 0.0343 cm/us = ~23200us
 */
extern const uint32_t STAR_HCSR04_TIMEOUT_US;

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_HCSR04_CONSTANTS_H */