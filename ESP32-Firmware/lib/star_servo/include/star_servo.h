/* lib/star_servo/include/star_servo.h */

#ifndef STAR_SERVO_H
#define STAR_SERVO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_servo.h
 * @brief Stateless servo angle calculation library
 *
 * This library provides pure calculation functions for converting angles
 * to PWM values for standard hobby servos. It has NO dependencies and
 * maintains NO state - just mathematical conversions.
 *
 * Use this library with the PCA9685 driver to control servos.
 *
 * Standard servo specifications:
 * - 50 Hz (20ms period)
 * - 1.0ms pulse = 0 degrees
 * - 1.5ms pulse = 90 degrees (center)
 * - 2.0ms pulse = 180 degrees
 *
 * For PCA9685 at 50Hz:
 * - 20ms / 4096 steps = 4.88us per step
 * - 1.0ms = ~205 counts
 * - 1.5ms = ~307 counts
 * - 2.0ms = ~410 counts
 */

/** Default servo pulse width range (microseconds) */
#define SERVO_PULSE_MIN_US (1000)
#define SERVO_PULSE_MAX_US (2000)
#define SERVO_PULSE_CENTER_US (1500)

/** Default servo angle range (degrees) */
#define SERVO_ANGLE_MIN (0)
#define SERVO_ANGLE_MAX (180)
#define SERVO_ANGLE_CENTER (90)

/** PCA9685 at 50Hz has 4096 steps per 20ms period */
#define PCA9685_MAX_COUNT (4096)
#define PCA9685_FREQ_HZ (50)
#define PCA9685_PERIOD_US (20000)

/**
 * @brief Calculate PCA9685 count value from angle (0-180 degrees)
 *
 * Uses standard servo mapping:
 * - 0° = 1.0ms pulse = ~205 counts
 * - 90° = 1.5ms pulse = ~307 counts
 * - 180° = 2.0ms pulse = ~410 counts
 *
 * @param[in] angle Servo angle in degrees (0-180)
 *
 * @return PCA9685 count value (0-4095)
 *
 * @note This is a pure calculation function with no side effects
 * @note Angles outside 0-180 are clamped to valid range
 */
uint16_t star_servo_angle_to_count(uint8_t angle);

/**
 * @brief Calculate PCA9685 count value from pulse width in microseconds
 *
 * For PCA9685 at 50Hz (20ms period):
 * count = (pulse_us * 4096) / 20000
 *
 * @param[in] pulse_us Pulse width in microseconds (typically 1000-2000)
 *
 * @return PCA9685 count value (0-4095)
 *
 * @note This is a pure calculation function with no side effects
 * @note Pulse widths are clamped to prevent overflow
 */
uint16_t star_servo_pulse_to_count(uint16_t pulse_us);

/**
 * @brief Calculate pulse width in microseconds from angle (0-180 degrees)
 *
 * Linear mapping:
 * - 0° = 1000us
 * - 90° = 1500us
 * - 180° = 2000us
 *
 * @param[in] angle Servo angle in degrees (0-180)
 *
 * @return Pulse width in microseconds
 *
 * @note This is a pure calculation function with no side effects
 * @note Angles outside 0-180 are clamped to valid range
 */
uint16_t star_servo_angle_to_pulse(uint8_t angle);

/**
 * @brief Calculate angle from pulse width in microseconds
 *
 * Inverse of star_servo_angle_to_pulse()
 *
 * @param[in] pulse_us Pulse width in microseconds (typically 1000-2000)
 *
 * @return Servo angle in degrees (0-180)
 *
 * @note This is a pure calculation function with no side effects
 * @note Pulse widths outside range are clamped
 */
uint8_t star_servo_pulse_to_angle(uint16_t pulse_us);

/**
 * @brief Calculate PCA9685 count for servo center position (90 degrees)
 *
 * @return PCA9685 count value for 1.5ms pulse (~307)
 *
 * @note This is a pure calculation function with no side effects
 */
uint16_t star_servo_get_center_count(void);

/**
 * @brief Calculate PCA9685 count for servo minimum position (0 degrees)
 *
 * @return PCA9685 count value for 1.0ms pulse (~205)
 *
 * @note This is a pure calculation function with no side effects
 */
uint16_t star_servo_get_min_count(void);

/**
 * @brief Calculate PCA9685 count for servo maximum position (180 degrees)
 *
 * @return PCA9685 count value for 2.0ms pulse (~410)
 *
 * @note This is a pure calculation function with no side effects
 */
uint16_t star_servo_get_max_count(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SERVO_H */
