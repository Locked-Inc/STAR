/* include/hardware_config.h */

/**
 * @file hardware_config.h
 * @brief Hardware pin mapping and configuration for RX72N STAR firmware
 * @details
 * Defines all hardware pin assignments, peripheral channels, and physical connections
 * for the STAR RX72N motor controller board. This file serves as the single source of
 * truth for hardware configuration.
 *
 * Pin Naming Convention:
 * - PORT notation: PORT3.2 = port 3, pin 2
 * - Peripheral notation: MTIOC3A = MTU channel 3, output A
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rx_mtu3a.h"

/* =============================================================================
 * ADC Channel Definitions
 * =============================================================================
 */

/**
 * @brief S12AD ADC channel numbers
 */
typedef enum {
  k_adc_channel_0  = 0, /**< AN000 */
  k_adc_channel_1  = 1, /**< AN001 */
  k_adc_channel_2  = 2, /**< AN002 */
  k_adc_channel_3  = 3, /**< AN003 */
  k_adc_channel_4  = 4, /**< AN004 */
  k_adc_channel_5  = 5, /**< AN005 */
  k_adc_channel_6  = 6, /**< AN006 */
  k_adc_channel_7  = 7, /**< AN007 */
} adc_channel_t;

/**
 * @brief S12AD ADC unit numbers
 */
typedef enum {
  k_adc_unit_0 = 0, /**< S12AD unit 0 */
  k_adc_unit_1 = 1, /**< S12AD unit 1 */
} adc_unit_t;

/**
 * @brief ADC resolution settings
 */
typedef enum {
  k_adc_resolution_8  = 8,  /**< 8-bit resolution */
  k_adc_resolution_10 = 10, /**< 10-bit resolution */
  k_adc_resolution_12 = 12, /**< 12-bit resolution */
} adc_resolution_t;

/* =============================================================================
 * Motor Control Configuration
 * =============================================================================
 */

/**
 * @brief Motor and encoder channel assignments
 *
 * RX72N Advantage: 182 GPIO pins means no multiplexing needed!
 * Each motor gets dedicated MTU channel for PWM and encoder.
 */
typedef enum {
  /* Motor 0 (Front-Left) */
  k_motor_0_mtu_channel = k_mtu_channel_3,     /**< MTU3 for PWM */
  k_motor_0_pwm_ph      = k_mtu_output_a,      /**< MTIOC3A = PH (phase) */
  k_motor_0_pwm_en      = k_mtu_output_b,      /**< MTIOC3B = EN (enable) */
  k_motor_0_encoder_ch  = k_mtu_channel_1,     /**< MTU1 for encoder */
  k_motor_0_nfault_port = 3,                   /**< PORT3 */
  k_motor_0_nfault_pin  = 2,                   /**< PORT3.2 = nFAULT */
  k_motor_0_ipropi_ch   = k_adc_channel_0,     /**< AN000 = IPROPI */

  /* Motor 1 (Front-Right) */
  k_motor_1_mtu_channel = k_mtu_channel_4,     /**< MTU4 for PWM */
  k_motor_1_pwm_ph      = k_mtu_output_a,      /**< MTIOC4A = PH */
  k_motor_1_pwm_en      = k_mtu_output_b,      /**< MTIOC4B = EN */
  k_motor_1_encoder_ch  = k_mtu_channel_2,     /**< MTU2 for encoder */
  k_motor_1_nfault_port = 3,                   /**< PORT3 */
  k_motor_1_nfault_pin  = 3,                   /**< PORT3.3 = nFAULT */
  k_motor_1_ipropi_ch   = k_adc_channel_1,     /**< AN001 = IPROPI */

  /* Motor 2 (Rear-Left) */
  k_motor_2_mtu_channel = k_mtu_channel_6,     /**< MTU6 for PWM */
  k_motor_2_pwm_ph      = k_mtu_output_a,      /**< MTIOC6A = PH */
  k_motor_2_pwm_en      = k_mtu_output_b,      /**< MTIOC6B = EN */
  k_motor_2_encoder_ch  = k_mtu_channel_0,     /**< MTU0 for encoder (note: CMT0 reserved for ThreadX) */
  k_motor_2_nfault_port = 3,                   /**< PORT3 */
  k_motor_2_nfault_pin  = 4,                   /**< PORT3.4 = nFAULT */
  k_motor_2_ipropi_ch   = k_adc_channel_2,     /**< AN002 = IPROPI */

  /* Motor 3 (Rear-Right) */
  k_motor_3_mtu_channel = k_mtu_channel_7,     /**< MTU7 for PWM */
  k_motor_3_pwm_ph      = k_mtu_output_a,      /**< MTIOC7A = PH */
  k_motor_3_pwm_en      = k_mtu_output_b,      /**< MTIOC7B = EN */
  k_motor_3_encoder_ch  = (rx_mtu_channel_t)5, /**< MTU5 for encoder (custom enum value) */
  k_motor_3_nfault_port = 3,                   /**< PORT3 */
  k_motor_3_nfault_pin  = 5,                   /**< PORT3.5 = nFAULT */
  k_motor_3_ipropi_ch   = k_adc_channel_3,     /**< AN003 = IPROPI */
} motor_hw_config_t;

/**
 * @brief Motor control timing configuration
 */
typedef enum {
  k_motor_pwm_freq_hz  = 20000, /**< 20 kHz PWM frequency */
  k_motor_dead_time_ns = 1000,  /**< 1μs dead-time for H-bridge */
} motor_timing_config_t;

/* =============================================================================
 * Battery Management System (BMS) Configuration
 * =============================================================================
 */

/**
 * @brief BMS I2C and GPIO configuration
 */
typedef enum {
  /* BQ25798 Charger */
  k_bq25798_i2c_channel = 0,                /**< RIIC0 (I2C bus 0) */
  k_bq25798_int_port    = 5,                /**< PORT5 */
  k_bq25798_int_pin     = 0,                /**< PORT5.0 = INT pin */

  /* Fuel Gauge (SMBUS) */
  k_fuel_gauge_smbus_channel = 1,           /**< RIIC1 (I2C bus 1, SMBUS mode) */
  k_fuel_gauge_alert_port    = 5,           /**< PORT5 */
  k_fuel_gauge_alert_pin     = 1,           /**< PORT5.1 = ALERT pin */
} bms_hw_config_t;

/* =============================================================================
 * Communication Interfaces
 * =============================================================================
 */

/**
 * @brief Debug UART configuration
 */
typedef enum {
  k_debug_uart_channel = 1,                 /**< SCI1 */
  k_debug_uart_tx_port = 2,                 /**< PORT2 */
  k_debug_uart_tx_pin  = 6,                 /**< PORT2.6 = TXD1 */
  k_debug_uart_rx_port = 2,                 /**< PORT2 */
  k_debug_uart_rx_pin  = 5,                 /**< PORT2.5 = RXD1 */
  k_debug_uart_baud    = 115200,            /**< 115200 baud */
} debug_uart_config_t;

/**
 * @brief SPI configuration for Raspberry Pi communication
 */
typedef enum {
  k_spi_rpi_channel    = 0,                 /**< RSPI0 */
  k_spi_rpi_cipo_port  = 7,                 /**< PORT7 */
  k_spi_rpi_cipo_pin   = 6,                 /**< PORT7.6 = MISOA (CIPO) */
  k_spi_rpi_copi_port  = 7,                 /**< PORT7 */
  k_spi_rpi_copi_pin   = 7,                 /**< PORT7.7 = MOSIA (COPI) */
  k_spi_rpi_sclk_port  = 7,                 /**< PORT7 */
  k_spi_rpi_sclk_pin   = 5,                 /**< PORT7.5 = RSPCKA */
  k_spi_rpi_cs_port    = 7,                 /**< PORT7 */
  k_spi_rpi_cs_pin     = 4,                 /**< PORT7.4 = SSLA0 */
} spi_rpi_config_t;

/* =============================================================================
 * Status LEDs and Indicators
 * =============================================================================
 */

/**
 * @brief LED GPIO configuration
 */
typedef enum {
  k_led_status_port = 0,                    /**< PORT0 */
  k_led_status_pin  = 3,                    /**< PORT0.3 = Status LED */
  k_led_error_port  = 0,                    /**< PORT0 */
  k_led_error_pin   = 5,                    /**< PORT0.5 = Error LED */
  k_led_power_port  = 0,                    /**< PORT0 */
  k_led_power_pin   = 7,                    /**< PORT0.7 = Power LED */
} led_hw_config_t;

/* =============================================================================
 * Emergency Stop and Safety
 * =============================================================================
 */

/**
 * @brief Emergency stop button configuration
 */
typedef enum {
  k_estop_button_port = 1,                  /**< PORT1 */
  k_estop_button_pin  = 0,                  /**< PORT1.0 = E-STOP button */
} estop_hw_config_t;

/* =============================================================================
 * System Configuration
 * =============================================================================
 */

/**
 * @brief Clock configuration
 */
typedef enum {
  k_system_clock_hz = 240000000,            /**< 240 MHz CPU clock */
  k_pclka_hz        = 120000000,            /**< 120 MHz PCLKA (MTU) */
  k_pclkb_hz        = 60000000,             /**< 60 MHz PCLKB (CMT, SCI) */
} clock_config_t;

/**
 * @brief ADC configuration
 */
typedef enum {
  k_adc_unit         = k_adc_unit_0,        /**< S12AD unit 0 */
  k_adc_resolution   = k_adc_resolution_12, /**< 12-bit resolution */
  k_adc_sample_time  = 24,                  /**< 24 state sample time */
} adc_config_t;

/* =============================================================================
 * Physical Constants
 * =============================================================================
 */

/**
 * @brief Robot physical parameters
 */
typedef enum {
  k_wheel_diameter_mm   = 100,              /**< Wheel diameter (mm) */
  k_wheel_base_mm       = 300,              /**< Distance between left/right wheels (mm) */
  k_track_width_mm      = 250,              /**< Distance between front/rear wheels (mm) */
  k_encoder_ppr         = 341,              /**< Encoder pulses per revolution (after 4x) */
  k_max_motor_current_ma = 2000,            /**< Maximum motor current (mA) */
} robot_physical_config_t;

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_CONFIG_H */
