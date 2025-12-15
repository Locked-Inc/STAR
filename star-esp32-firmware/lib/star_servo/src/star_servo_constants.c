/* lib/star_servo/src/star_servo_constants.c */

/**
 * @file star_servo_constants.c
 * @brief Servo timing constants for standard hobby servos
 */

#include "star_servo_constants.h"

/**
 * @brief Static allocation of default servo constants
 *
 * These values match the standard hobby servo specifications:
 * - 50 Hz PWM frequency (20ms period)
 * - 1.0ms to 2.0ms pulse width range
 * - 0 to 180 degree angle range
 * - PCA9685 12-bit resolution (4096 steps)
 */
static const star_servo_constants_t s_default_constants = {
  .pulse   = {.min_us = 1000, .center_us = 1500, .max_us = 2000},
  .angle   = {.min = 0, .center = 90, .max = 180},
  .pca9685 = {.max_count = 4096, .freq_hz = 50, .period_us = 20000}};

const star_servo_constants_t* star_servo_get_constants(void)
{
  return &s_default_constants;
}

const star_servo_pulse_t* star_servo_get_pulse_constants(void)
{
  return &s_default_constants.pulse;
}

const star_servo_angle_t* star_servo_get_angle_constants(void)
{
  return &s_default_constants.angle;
}

const star_servo_pca9685_t* star_servo_get_pca9685_constants(void)
{
  return &s_default_constants.pca9685;
}