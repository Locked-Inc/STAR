/* lib/star_servo/include/star_servo_constants.h */

#ifndef STAR_SERVO_CONSTANTS_H
#define STAR_SERVO_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_servo_constants.h
 * @brief Type-safe constants for servo control
 *
 * This header provides type-safe structured constants for servo control
 * with proper encapsulation.
 */

/**
 * @brief Servo pulse timing constants
 *
 * Standard pulse widths for hobby servos operating at 50Hz.
 * These values define the pulse duration in microseconds.
 */
typedef struct {
  const uint16_t min_us;    /**< Minimum pulse width (0 degrees) */
  const uint16_t center_us; /**< Center pulse width (90 degrees) */
  const uint16_t max_us;    /**< Maximum pulse width (180 degrees) */
} star_servo_pulse_t;

/**
 * @brief Servo angle constants
 *
 * Standard angle range for hobby servos in degrees.
 */
typedef struct {
  const uint8_t min;    /**< Minimum angle (typically 0°) */
  const uint8_t center; /**< Center angle (typically 90°) */
  const uint8_t max;    /**< Maximum angle (typically 180°) */
} star_servo_angle_t;

/**
 * @brief PCA9685 PWM controller constants
 *
 * Constants specific to the PCA9685 16-channel PWM controller
 * when configured for servo control at 50Hz.
 */
typedef struct {
  const uint16_t max_count; /**< Maximum count value (12-bit resolution) */
  const uint8_t  freq_hz;   /**< Operating frequency for servos */
  const uint32_t period_us; /**< Period in microseconds (1/freq) */
} star_servo_pca9685_t;

/**
 * @brief Complete servo configuration constants
 *
 * Aggregates all servo-related constants into a single structure.
 */
typedef struct {
  const star_servo_pulse_t   pulse;   /**< Pulse timing constants */
  const star_servo_angle_t   angle;   /**< Angle range constants */
  const star_servo_pca9685_t pca9685; /**< PCA9685 specific constants */
} star_servo_constants_t;

/**
 * @brief Get the default servo constants
 *
 * Returns a pointer to the statically allocated default servo constants.
 * These values are suitable for standard hobby servos.
 *
 * @return Pointer to immutable servo constants structure
 *
 * @note The returned pointer references static const data and should not be freed
 * @note Thread-safe: returns pointer to read-only data
 *
 * Example usage:
 * @code
 * const star_servo_constants_t* constants = star_servo_get_constants();
 * printf("Min pulse: %u us\n", constants->pulse.min_us);
 * printf("Max angle: %u degrees\n", constants->angle.max);
 * printf("PCA9685 frequency: %u Hz\n", constants->pca9685.freq_hz);
 * @endcode
 */
const star_servo_constants_t* star_servo_get_constants(void);

/**
 * @brief Get standard servo pulse timing constants
 *
 * @return Pointer to immutable pulse timing structure
 *
 * @note Thread-safe: returns pointer to read-only data
 */
const star_servo_pulse_t* star_servo_get_pulse_constants(void);

/**
 * @brief Get standard servo angle constants
 *
 * @return Pointer to immutable angle range structure
 *
 * @note Thread-safe: returns pointer to read-only data
 */
const star_servo_angle_t* star_servo_get_angle_constants(void);

/**
 * @brief Get PCA9685 controller constants
 *
 * @return Pointer to immutable PCA9685 constants structure
 *
 * @note Thread-safe: returns pointer to read-only data
 */
const star_servo_pca9685_t* star_servo_get_pca9685_constants(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SERVO_CONSTANTS_H */