/* lib/star_servo/src/star_servo.c */

#include "star_servo.h"

/* --- Clamping Helper --- */

static inline uint8_t clamp_angle(int32_t angle)
{
  if (angle < SERVO_ANGLE_MIN) {
    return SERVO_ANGLE_MIN;
  }
  if (angle > SERVO_ANGLE_MAX) {
    return SERVO_ANGLE_MAX;
  }
  return (uint8_t)angle;
}

static inline uint16_t clamp_pulse(int32_t pulse_us)
{
  if (pulse_us < SERVO_PULSE_MIN_US) {
    return SERVO_PULSE_MIN_US;
  }
  if (pulse_us > SERVO_PULSE_MAX_US) {
    return SERVO_PULSE_MAX_US;
  }
  return (uint16_t)pulse_us;
}

/* --- Public API --- */

uint16_t star_servo_angle_to_pulse(uint8_t angle)
{
  angle = clamp_angle(angle);

  /* Linear interpolation: pulse = 1000 + (angle * 1000 / 180) */
  uint16_t pulse_us =
    SERVO_PULSE_MIN_US +
    ((uint32_t)angle * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)) / SERVO_ANGLE_MAX;

  return pulse_us;
}

uint8_t star_servo_pulse_to_angle(uint16_t pulse_us)
{
  pulse_us = clamp_pulse(pulse_us);

  /* Linear interpolation: angle = (pulse - 1000) * 180 / 1000 */
  uint32_t angle = ((uint32_t)(pulse_us - SERVO_PULSE_MIN_US) * SERVO_ANGLE_MAX) /
                   (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US);

  return clamp_angle(angle);
}

uint16_t star_servo_pulse_to_count(uint16_t pulse_us)
{
  pulse_us = clamp_pulse(pulse_us);

  /* For PCA9685 at 50Hz: count = (pulse_us * 4096) / 20000 */
  uint32_t count = ((uint32_t)pulse_us * PCA9685_MAX_COUNT) / PCA9685_PERIOD_US;

  /* Clamp to valid PCA9685 range */
  if (count >= PCA9685_MAX_COUNT) {
    count = PCA9685_MAX_COUNT - 1;
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
  return star_servo_pulse_to_count(SERVO_PULSE_CENTER_US);
}

uint16_t star_servo_get_min_count(void)
{
  return star_servo_pulse_to_count(SERVO_PULSE_MIN_US);
}

uint16_t star_servo_get_max_count(void)
{
  return star_servo_pulse_to_count(SERVO_PULSE_MAX_US);
}
