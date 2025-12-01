/* lib/star_sensor_pca9685/src/pca9685_constants.c */

#include "pca9685_constants.h"

/**
 * @file pca9685_constants.c
 * @brief Implementation of type-safe constants for PCA9685
 *
 * This file provides the actual definitions of const variables
 * declared as extern in pca9685_constants.h
 */

/* Hardware limits definition */
const pca9685_hardware_limits_t pca9685_hardware_limits = {
  .num_channels       = 16,       /* 16 PWM channels */
  .pwm_resolution     = 4096,     /* 12-bit resolution */
  .min_frequency_hz   = 24,       /* Minimum PWM frequency */
  .max_frequency_hz   = 1526,     /* Maximum PWM frequency */
  .oscillator_freq_hz = 25000000, /* 25 MHz internal oscillator */
  .min_prescale       = 3,        /* Minimum prescale value for max frequency */
  .max_prescale       = 255       /* Maximum prescale value for min frequency */
};

/* Timing constants definition */
const pca9685_timing_constants_t pca9685_timing = {
  .mutex_timeout_ms     = 1000, /* 1 second mutex timeout */
  .reset_delay_us       = 500,  /* 500us after reset command */
  .wake_delay_us        = 500,  /* 500us after wake from sleep */
  .mode_switch_delay_us = 100   /* 100us after mode change */
};