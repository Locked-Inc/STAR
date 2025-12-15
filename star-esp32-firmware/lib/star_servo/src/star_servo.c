/* lib/star_servo/src/star_servo.c */

/* TODO: Add doxygen doc comment for @file */

/* FIXME: I am unsure if this is using MCPWM like how the `star_motor` is using. We need center aligned PWM for motor/(servo motor) movement */

#include "star_servo.h"

#include "star_servo_constants.h"

/* --- Clamping Helper --- */

static inline uint8_t clamp_angle(int32_t angle)
{
  const star_servo_angle_t* angles = star_servo_get_angle_constants();
  if (angle < angles->min) {
    return angles->min;
  }
  if (angle > angles->max) {
    return angles->max;
  }
  return (uint8_t)angle;
}

static inline uint16_t clamp_pulse(int32_t pulse_us)
{
  const star_servo_pulse_t* pulse = star_servo_get_pulse_constants();
  if (pulse_us < pulse->min_us) {
    return pulse->min_us;
  }
  if (pulse_us > pulse->max_us) {
    return pulse->max_us;
  }
  return (uint16_t)pulse_us;
}

/* --- Public API --- */

uint16_t star_servo_angle_to_pulse(uint8_t angle)
{
  angle = clamp_angle(angle);

  const star_servo_constants_t* constants = star_servo_get_constants();
  const star_servo_pulse_t*     pulse     = &constants->pulse;
  const star_servo_angle_t*     angles    = &constants->angle;

  /* Linear interpolation: pulse = min + (angle * range / max_angle) */
  uint16_t pulse_us =
    pulse->min_us + ((uint32_t)angle * (pulse->max_us - pulse->min_us)) / angles->max;

  return pulse_us;
}

uint8_t star_servo_pulse_to_angle(uint16_t pulse_us)
{
  pulse_us = clamp_pulse(pulse_us);

  const star_servo_constants_t* constants = star_servo_get_constants();
  const star_servo_pulse_t*     pulse     = &constants->pulse;
  const star_servo_angle_t*     angles    = &constants->angle;

  /* Linear interpolation: angle = (pulse - min) * max_angle / range */
  uint32_t angle =
    ((uint32_t)(pulse_us - pulse->min_us) * angles->max) / (pulse->max_us - pulse->min_us);

  return clamp_angle(angle);
}

uint16_t star_servo_pulse_to_count(uint16_t pulse_us)
{
  pulse_us = clamp_pulse(pulse_us);

  const star_servo_pca9685_t* pca = star_servo_get_pca9685_constants();

  /* For PCA9685 at 50Hz: count = (pulse_us * max_count) / period_us */
  uint32_t count = ((uint32_t)pulse_us * pca->max_count) / pca->period_us;

  /* Clamp to valid PCA9685 range */
  if (count >= pca->max_count) {
    count = pca->max_count - 1;
  }

  return (uint16_t)count;
}

uint16_t star_servo_angle_to_count(uint8_t angle)
{
  angle          = clamp_angle(angle);
  uint16_t pulse = star_servo_angle_to_pulse(angle);
  uint16_t count = star_servo_pulse_to_count(pulse);

  return count;
}

uint16_t star_servo_get_center_count(void)
{
  const star_servo_pulse_t* pulse = star_servo_get_pulse_constants();
  return star_servo_pulse_to_count(pulse->center_us);
}

uint16_t star_servo_get_min_count(void)
{
  const star_servo_pulse_t* pulse = star_servo_get_pulse_constants();
  return star_servo_pulse_to_count(pulse->min_us);
}

uint16_t star_servo_get_max_count(void)
{
  const star_servo_pulse_t* pulse = star_servo_get_pulse_constants();
  return star_servo_pulse_to_count(pulse->max_us);
}
