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
 *
 * Example Usage:
 * @code
 * // === Basic Angle to PWM Conversion ===
 *
 * #include "star_servo.h"
 * #include "star_sensor_pca9685.h"
 *
 * // Convert angle to PCA9685 count
 * uint16_t count_0   = star_servo_angle_to_count(0);    // ~205 (1.0ms)
 * uint16_t count_90  = star_servo_angle_to_count(90);   // ~307 (1.5ms)
 * uint16_t count_180 = star_servo_angle_to_count(180);  // ~410 (2.0ms)
 *
 * // Use with PCA9685 driver
 * star_sensor_pca9685_set_pwm(&pwm_handle, channel, 0, count_90);
 *
 *
 * // === Using with PCA9685 for Servo Control ===
 *
 * // Center all servos
 * uint16_t center = star_servo_get_center_count();
 * for (int ch = 0; ch < 16; ch++) {
 *     star_sensor_pca9685_set_pwm(&pwm_handle, ch, 0, center);
 * }
 *
 * // Move servo to specific angle
 * void set_servo_angle(pca9685_handle_t* pwm, uint8_t channel, uint8_t angle) {
 *     uint16_t count = star_servo_angle_to_count(angle);
 *     star_sensor_pca9685_set_pwm(pwm, channel, 0, count);
 * }
 *
 * set_servo_angle(&pwm_handle, 0, 45);   // 45 degrees
 * set_servo_angle(&pwm_handle, 1, 135);  // 135 degrees
 *
 *
 * // === Smooth Servo Sweep ===
 *
 * void sweep_servo(pca9685_handle_t* pwm, uint8_t channel) {
 *     // Sweep from 0 to 180
 *     for (uint8_t angle = 0; angle <= 180; angle++) {
 *         uint16_t count = star_servo_angle_to_count(angle);
 *         star_sensor_pca9685_set_pwm(pwm, channel, 0, count);
 *         vTaskDelay(pdMS_TO_TICKS(15));  // 15ms delay per step
 *     }
 *
 *     // Sweep back from 180 to 0
 *     for (int angle = 180; angle >= 0; angle--) {
 *         uint16_t count = star_servo_angle_to_count((uint8_t)angle);
 *         star_sensor_pca9685_set_pwm(pwm, channel, 0, count);
 *         vTaskDelay(pdMS_TO_TICKS(15));
 *     }
 * }
 *
 *
 * // === Pulse Width Conversions ===
 *
 * // Convert angle to pulse width (microseconds)
 * uint16_t pulse_0   = star_servo_angle_to_pulse(0);    // 1000us
 * uint16_t pulse_90  = star_servo_angle_to_pulse(90);   // 1500us
 * uint16_t pulse_180 = star_servo_angle_to_pulse(180);  // 2000us
 *
 * // Convert pulse width to PCA9685 count
 * uint16_t count = star_servo_pulse_to_count(1500);  // Center position
 *
 * // Custom pulse widths for non-standard servos
 * uint16_t extended_count = star_servo_pulse_to_count(2200);  // Extended range
 *
 *
 * // === Inverse Conversions ===
 *
 * // Convert pulse width back to angle
 * uint8_t angle = star_servo_pulse_to_angle(1500);  // Returns 90
 *
 * // Useful for reading servo position from PWM feedback
 *
 *
 * // === Get Standard Positions ===
 *
 * uint16_t min_count    = star_servo_get_min_count();     // 0 degrees (~205)
 * uint16_t center_count = star_servo_get_center_count();  // 90 degrees (~307)
 * uint16_t max_count    = star_servo_get_max_count();     // 180 degrees (~410)
 *
 * ESP_LOGI(TAG, "Servo counts: min=%d, center=%d, max=%d",
 *          min_count, center_count, max_count);
 *
 *
 * // === Robotic Arm Example ===
 *
 * typedef struct {
 *     pca9685_handle_t* pwm;
 *     uint8_t base_channel;
 *     uint8_t shoulder_channel;
 *     uint8_t elbow_channel;
 *     uint8_t gripper_channel;
 * } robot_arm_t;
 *
 * void robot_arm_move(robot_arm_t* arm, uint8_t base, uint8_t shoulder,
 *                     uint8_t elbow, uint8_t gripper) {
 *     star_sensor_pca9685_set_pwm(arm->pwm, arm->base_channel, 0,
 *                                  star_servo_angle_to_count(base));
 *     star_sensor_pca9685_set_pwm(arm->pwm, arm->shoulder_channel, 0,
 *                                  star_servo_angle_to_count(shoulder));
 *     star_sensor_pca9685_set_pwm(arm->pwm, arm->elbow_channel, 0,
 *                                  star_servo_angle_to_count(elbow));
 *     star_sensor_pca9685_set_pwm(arm->pwm, arm->gripper_channel, 0,
 *                                  star_servo_angle_to_count(gripper));
 * }
 *
 * // Home position
 * robot_arm_move(&arm, 90, 90, 90, 0);
 *
 * // Pick position
 * robot_arm_move(&arm, 45, 120, 60, 90);
 *
 *
 * // === Constants Reference ===
 *
 * // Pulse widths (microseconds)
 * // SERVO_PULSE_MIN_US     = 1000
 * // SERVO_PULSE_CENTER_US  = 1500
 * // SERVO_PULSE_MAX_US     = 2000
 *
 * // Angles (degrees)
 * // SERVO_ANGLE_MIN    = 0
 * // SERVO_ANGLE_CENTER = 90
 * // SERVO_ANGLE_MAX    = 180
 *
 * // PCA9685 at 50Hz
 * // PCA9685_MAX_COUNT = 4096
 * // PCA9685_FREQ_HZ   = 50
 * // PCA9685_PERIOD_US = 20000
 * @endcode
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
