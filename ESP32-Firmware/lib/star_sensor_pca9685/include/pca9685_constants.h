/* lib/star_sensor_pca9685/include/pca9685_constants.h */

#ifndef PCA9685_CONSTANTS_H
#define PCA9685_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file pca9685_constants.h
 * @brief Type-safe constants for PCA9685 16-channel 12-bit PWM controller
 *
 * This file provides type-safe alternatives to preprocessor macros for
 * improved type checking, debugging, and namespace management.
 */

/* ===== Device Constants ===== */

/** PCA9685 I2C addresses */
typedef enum {
  k_pca9685_i2c_addr_default = 0x40, /**< Default I2C address (A0-A5 = 0) */
  k_pca9685_i2c_addr_allcall = 0x70, /**< All-call broadcast address */
  k_pca9685_i2c_addr_min     = 0x40, /**< Minimum configurable address */
  k_pca9685_i2c_addr_max     = 0x7F  /**< Maximum configurable address */
} pca9685_i2c_address_t;

/** PCA9685 hardware capabilities */
typedef struct {
  const uint8_t  num_channels;       /**< Number of PWM channels */
  const uint16_t pwm_resolution;     /**< PWM counter resolution (12-bit) */
  const uint16_t min_frequency_hz;   /**< Minimum PWM frequency */
  const uint16_t max_frequency_hz;   /**< Maximum PWM frequency */
  const uint32_t oscillator_freq_hz; /**< Internal oscillator frequency */
  const uint8_t  min_prescale;       /**< Minimum prescale value */
  const uint8_t  max_prescale;       /**< Maximum prescale value */
} pca9685_hardware_limits_t;

/** External declaration for hardware limits */
extern const pca9685_hardware_limits_t pca9685_hardware_limits;

/* ===== Register Map ===== */

/** PCA9685 register addresses */
typedef enum {
  k_pca9685_reg_mode1      = 0x00, /**< Mode register 1 */
  k_pca9685_reg_mode2      = 0x01, /**< Mode register 2 */
  k_pca9685_reg_subadr1    = 0x02, /**< I2C sub-address 1 */
  k_pca9685_reg_subadr2    = 0x03, /**< I2C sub-address 2 */
  k_pca9685_reg_subadr3    = 0x04, /**< I2C sub-address 3 */
  k_pca9685_reg_allcalladr = 0x05, /**< All-call I2C address */
  k_pca9685_reg_led0_on_l  = 0x06, /**< LED0 output ON time, low byte */
  k_pca9685_reg_led0_on_h  = 0x07, /**< LED0 output ON time, high byte */
  k_pca9685_reg_led0_off_l = 0x08, /**< LED0 output OFF time, low byte */
  k_pca9685_reg_led0_off_h = 0x09, /**< LED0 output OFF time, high byte */
  /* LED1-LED15 registers continue in same pattern */
  /* 0x0A-0x45: LED1-LED15 ON/OFF registers */
  k_pca9685_reg_all_led_on_l  = 0xFA, /**< All LED ON time, low byte */
  k_pca9685_reg_all_led_on_h  = 0xFB, /**< All LED ON time, high byte */
  k_pca9685_reg_all_led_off_l = 0xFC, /**< All LED OFF time, low byte */
  k_pca9685_reg_all_led_off_h = 0xFD, /**< All LED OFF time, high byte */
  k_pca9685_reg_prescale      = 0xFE  /**< Prescaler for PWM output frequency */
} pca9685_register_t;

/* ===== MODE1 Register Bits ===== */

/** MODE1 register bit fields */
typedef enum {
  k_pca9685_mode1_restart_bit = 7, /**< Restart enabled bit position */
  k_pca9685_mode1_extclk_bit  = 6, /**< External clock bit position */
  k_pca9685_mode1_ai_bit      = 5, /**< Auto-increment bit position */
  k_pca9685_mode1_sleep_bit   = 4, /**< Sleep mode bit position */
  k_pca9685_mode1_sub1_bit    = 3, /**< Sub-address 1 enable bit position */
  k_pca9685_mode1_sub2_bit    = 2, /**< Sub-address 2 enable bit position */
  k_pca9685_mode1_sub3_bit    = 1, /**< Sub-address 3 enable bit position */
  k_pca9685_mode1_allcall_bit = 0  /**< All-call address enable bit position */
} pca9685_mode1_bit_t;

/** MODE1 register bit masks */
typedef enum {
  k_pca9685_mode1_restart_mask = (1 << 7), /**< Restart enabled */
  k_pca9685_mode1_extclk_mask  = (1 << 6), /**< Use external clock */
  k_pca9685_mode1_ai_mask      = (1 << 5), /**< Auto-increment enabled */
  k_pca9685_mode1_sleep_mask   = (1 << 4), /**< Low power mode */
  k_pca9685_mode1_sub1_mask    = (1 << 3), /**< Respond to sub-address 1 */
  k_pca9685_mode1_sub2_mask    = (1 << 2), /**< Respond to sub-address 2 */
  k_pca9685_mode1_sub3_mask    = (1 << 1), /**< Respond to sub-address 3 */
  k_pca9685_mode1_allcall_mask = (1 << 0)  /**< Respond to all-call address */
} pca9685_mode1_mask_t;

/* ===== MODE2 Register Bits ===== */

/** MODE2 register bit fields */
typedef enum {
  k_pca9685_mode2_invrt_bit  = 4, /**< Invert output bit position */
  k_pca9685_mode2_och_bit    = 3, /**< Output change bit position */
  k_pca9685_mode2_outdrv_bit = 2, /**< Output driver bit position */
  k_pca9685_mode2_outne1_bit = 1, /**< Output enable bit 1 position */
  k_pca9685_mode2_outne0_bit = 0  /**< Output enable bit 0 position */
} pca9685_mode2_bit_t;

/** MODE2 register bit masks */
typedef enum {
  k_pca9685_mode2_invrt_mask  = (1 << 4), /**< Output logic state inverted */
  k_pca9685_mode2_och_mask    = (1 << 3), /**< Outputs change on STOP command */
  k_pca9685_mode2_outdrv_mask = (1 << 2), /**< Totem pole (1) or open-drain (0) */
  k_pca9685_mode2_outne1_mask = (1 << 1), /**< Output enable bit 1 */
  k_pca9685_mode2_outne0_mask = (1 << 0)  /**< Output enable bit 0 */
} pca9685_mode2_mask_t;

/* ===== Special PWM Values ===== */

/** Special PWM control values */
typedef enum {
  k_pca9685_pwm_full_on_flag  = (1 << 4), /**< Full ON flag (in ON_H register) */
  k_pca9685_pwm_full_off_flag = (1 << 4), /**< Full OFF flag (in OFF_H register) */
  k_pca9685_pwm_max_value     = 4095,     /**< Maximum PWM counter value (12-bit) */
  k_pca9685_pwm_min_value     = 0         /**< Minimum PWM counter value */
} pca9685_pwm_special_t;

/* ===== Timing Constants ===== */

/** Timing and timeout constants */
typedef struct {
  const uint32_t mutex_timeout_ms;     /**< Mutex acquisition timeout */
  const uint32_t reset_delay_us;       /**< Delay after reset command */
  const uint32_t wake_delay_us;        /**< Delay after wake from sleep */
  const uint32_t mode_switch_delay_us; /**< Delay after mode change */
} pca9685_timing_constants_t;

/** External declaration for timing constants */
extern const pca9685_timing_constants_t pca9685_timing;

/* ===== Helper Macros for Calculations ===== */

/**
 * @brief Calculate prescale value from frequency
 * @param freq_hz Frequency in Hz
 * @return Prescale value (caller must validate range)
 */
static inline uint8_t pca9685_freq_to_prescale_calc(uint16_t freq_hz)
{
  // Using oscillator frequency from hardware limits
  // Formula: prescale = round(oscillator / (4096 * freq)) - 1
  return (uint8_t)((25000000 / (4096UL * (uint32_t)freq_hz)) - 1);
}

/**
 * @brief Calculate frequency from prescale value
 * @param prescale Prescale register value
 * @return Frequency in Hz
 */
static inline uint16_t pca9685_prescale_to_freq_calc(uint8_t prescale)
{
  // Formula: freq = oscillator / (4096 * (prescale + 1))
  return (uint16_t)(25000000 / (4096UL * ((uint32_t)prescale + 1)));
}

/**
 * @brief Calculate register offset for a specific LED channel
 * @param channel Channel number (0-15)
 * @return Base register address for the channel
 */
static inline uint8_t pca9685_channel_base_register(uint8_t channel)
{
  // Each channel uses 4 registers (ON_L, ON_H, OFF_L, OFF_H)
  return k_pca9685_reg_led0_on_l + (channel * 4);
}

#ifdef __cplusplus
}
#endif

#endif /* PCA9685_CONSTANTS_H */