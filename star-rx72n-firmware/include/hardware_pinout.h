/* include/hardware_pinout.h */

/**
 * @file hardware_pinout.h
 * @brief STAR RX72N Hardware Pin Assignments
 *
 * Complete pin mapping for the STAR motor controller PCB. This file documents
 * all RX72N GPIO assignments, peripheral connections, and alternate functions.
 *
 * Pin assignments are derived from the hardware design documented in
 * docs/sections/03_hardware_pinout.tex. Any changes to the PCB layout must
 * be reflected in this file.
 *
 * Design Philosophy:
 * - GPTW (General PWM Timer) for motor PWM control (32-bit, high resolution)
 * - MTU (Multi-Function Timer Unit) for encoder quadrature counting
 * - Clean functional separation prevents resource conflicts
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_HARDWARE_PINOUT_H
#define STAR_RX72N_HARDWARE_PINOUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* =============================================================================
 * Type-Safe GPIO Pin Enum
 * =============================================================================
 */

/**
 * @brief Type-safe GPIO pin enumeration
 *
 * Unified enum for all RX72N GPIO pins. Encodes both port and pin number
 * in a single value for compile-time safety and better IDE autocomplete.
 *
 * Encoding: (port << 8) | pin
 * - Upper byte: Port number (0x0-0x10 for ports 0-J)
 * - Lower byte: Pin number (0-7)
 *
 * Benefits:
 * - Compile-time validation (can't pass invalid port/pin combinations)
 * - Self-documenting code (k_gpio_pc6 vs separate 0xC, 6)
 * - IDE autocomplete shows all valid pins
 * - Easy to extract port/pin with helper functions
 *
 * Use helper functions to extract port/pin:
 * - gpio_pin_get_port(gpio_pin_t pin) -> uint8_t port
 * - gpio_pin_get_pin(gpio_pin_t pin) -> uint8_t pin_num
 */
typedef enum {
  /* Port 0 GPIO Pins */
  k_gpio_p00 = 0x0000, /**< P00 (pin 103) */
  k_gpio_p01 = 0x0001, /**< P01 (pin 102) */
  k_gpio_p02 = 0x0002, /**< P02 (pin 101) */
  k_gpio_p03 = 0x0003, /**< P03 (pin 99) - MD */
  k_gpio_p05 = 0x0005, /**< P05 (pin 100) - 1-Wire Temperature Sensor */
  k_gpio_p07 = 0x0007, /**< P07 (pin 98) - PMOD JF GPIO1 */

  /* Port 1 GPIO Pins */
  k_gpio_p10 = 0x0100, /**< P10 (pin 35) */
  k_gpio_p11 = 0x0101, /**< P11 (pin 36) */
  k_gpio_p12 = 0x0102, /**< P12 (pin 34) - BMS I2C SCL */
  k_gpio_p13 = 0x0103, /**< P13 (pin 33) - BMS I2C SDA */
  k_gpio_p14 = 0x0104, /**< P14 (pin 32) - Encoder 1 Phase A */
  k_gpio_p15 = 0x0105, /**< P15 (pin 31) - Encoder 0 Phase B */
  k_gpio_p16 = 0x0106, /**< P16 (pin 30) */
  k_gpio_p17 = 0x0107, /**< P17 (pin 29) - Motor 0 SPI CS */

  /* Port 2 GPIO Pins */
  k_gpio_p20 = 0x0200, /**< P20 (pin 28) - PMOD JF I2C SDA */
  k_gpio_p21 = 0x0201, /**< P21 (pin 27) - PMOD JF I2C SCL */
  k_gpio_p22 = 0x0202, /**< P22 (pin 26) - Encoder 1 Phase B */
  k_gpio_p23 = 0x0203, /**< P23 (pin 25) - Motor 1 SPI CS */
  k_gpio_p24 = 0x0204, /**< P24 (pin 24) - Encoder 0 Phase A */
  k_gpio_p25 = 0x0205, /**< P25 (pin 23) - Encoder 3 Phase A */
  k_gpio_p26 = 0x0206, /**< P26 (pin 22) - JTAG TDO */
  k_gpio_p27 = 0x0207, /**< P27 (pin 21) - JTAG TCK */

  /* Port 3 GPIO Pins */
  k_gpio_p30 = 0x0300, /**< P30 (pin 20) - JTAG TDI */
  k_gpio_p31 = 0x0301, /**< P31 (pin 19) - JTAG TMS */
  k_gpio_p32 = 0x0302, /**< P32 (pin 18) - Motor 2 SPI CS */
  k_gpio_p33 = 0x0303, /**< P33 (pin 17) */
  k_gpio_p34 = 0x0304, /**< P34 (pin 16) - JTAG TRST */

  /* Port 4 GPIO Pins (ADC + nFAULT) */
  k_gpio_p40 = 0x0400, /**< P40 (pin 95) - Motor 0 Current ADC */
  k_gpio_p41 = 0x0401, /**< P41 (pin 93) - Motor 1 Current ADC */
  k_gpio_p42 = 0x0402, /**< P42 (pin 92) - Motor 2 Current ADC */
  k_gpio_p43 = 0x0403, /**< P43 (pin 91) - Motor 3 Current ADC */
  k_gpio_p44 = 0x0404, /**< P44 (pin 90) - Motor 0 nFAULT */
  k_gpio_p45 = 0x0405, /**< P45 (pin 89) - Motor 1 nFAULT */
  k_gpio_p46 = 0x0406, /**< P46 (pin 88) - Motor 2 nFAULT */
  k_gpio_p47 = 0x0407, /**< P47 (pin 87) - Motor 3 nFAULT */

  /* Port 5 GPIO Pins (PMOD JA + PMOD JB) */
  k_gpio_p50 = 0x0500, /**< P50 (pin 44) - PMOD JA COPI */
  k_gpio_p51 = 0x0501, /**< P51 (pin 43) - PMOD JA CIPO */
  k_gpio_p52 = 0x0502, /**< P52 (pin 42) - PMOD JA CS0 */
  k_gpio_p53 = 0x0503, /**< P53 (pin 41) - PMOD JA SCK */
  k_gpio_p54 = 0x0504, /**< P54 (pin 40) - PMOD JB GPIO2 */
  k_gpio_p55 = 0x0505, /**< P55 (pin 39) - PMOD JB GPIO1 */

  /* Port A GPIO Pins (SPI + PMOD) */
  k_gpio_pa0 = 0x0A00, /**< PA0 (pin 70) - Motor 3 SPI CS */
  k_gpio_pa1 = 0x0A01, /**< PA1 (pin 69) - PMOD JE GPIO3 */
  k_gpio_pa2 = 0x0A02, /**< PA2 (pin 68) - PMOD JB GPIO3 */
  k_gpio_pa3 = 0x0A03, /**< PA3 (pin 67) - Encoder 3 Phase B */
  k_gpio_pa4 = 0x0A04, /**< PA4 (pin 66) - RPi5 SPI CS */
  k_gpio_pa5 = 0x0A05, /**< PA5 (pin 65) - RPi5 SPI SCLK */
  k_gpio_pa6 = 0x0A06, /**< PA6 (pin 64) - RPi5 SPI COPI */
  k_gpio_pa7 = 0x0A07, /**< PA7 (pin 63) - RPi5 SPI CIPO */

  /* Port B GPIO Pins (Debug UART + PMOD) */
  k_gpio_pb0 = 0x0B00, /**< PB0 (pin 61) - PMOD JE GPIO2 */
  k_gpio_pb1 = 0x0B01, /**< PB1 (pin 59) - PMOD JE GPIO1 */
  k_gpio_pb3 = 0x0B03, /**< PB3 (pin 58) - PMOD JA RST */
  k_gpio_pb4 = 0x0B04, /**< PB4 (pin 57) - PMOD JA DC */
  k_gpio_pb5 = 0x0B05, /**< PB5 (pin 55) - PMOD JA CS1 */
  k_gpio_pb6 = 0x0B06, /**< PB6 (pin 54) - Debug UART RX */
  k_gpio_pb7 = 0x0B07, /**< PB7 (pin 53) - Debug UART TX */

  /* Port C GPIO Pins (Encoder + PMOD) */
  k_gpio_pc0 = 0x0C00, /**< PC0 (pin 52) - Encoder 2 Phase B */
  k_gpio_pc1 = 0x0C01, /**< PC1 (pin 51) - Encoder 2 Phase A */
  k_gpio_pc2 = 0x0C02, /**< PC2 (pin 50) - PMOD JC I2C SCL */
  k_gpio_pc3 = 0x0C03, /**< PC3 (pin 49) - PMOD JC I2C SDA */
  k_gpio_pc4 = 0x0C04, /**< PC4 (pin 48) - PMOD JC INT1 */
  k_gpio_pc5 = 0x0C05, /**< PC5 (pin 47) - PMOD JC INT2 */
  k_gpio_pc6 = 0x0C06, /**< PC6 (pin 46) - PMOD JB GPIO0 */
  k_gpio_pc7 = 0x0C07, /**< PC7 (pin 45) */

  /* Port D GPIO Pins (Motor SPI + PMOD JD + PMOD JF) */
  k_gpio_pd0 = 0x0D00, /**< PD0 (pin 86) - PMOD JF GPIO0 */
  k_gpio_pd1 = 0x0D01, /**< PD1 (pin 85) - Motor SPI COPI */
  k_gpio_pd2 = 0x0D02, /**< PD2 (pin 84) - Motor SPI CIPO */
  k_gpio_pd3 = 0x0D03, /**< PD3 (pin 83) - Motor SPI SCLK */
  k_gpio_pd4 = 0x0D04, /**< PD4 (pin 82) - PMOD JD GPIO3 */
  k_gpio_pd5 = 0x0D05, /**< PD5 (pin 81) - PMOD JD GPIO2 */
  k_gpio_pd6 = 0x0D06, /**< PD6 (pin 80) - PMOD JD GPIO1 */
  k_gpio_pd7 = 0x0D07, /**< PD7 (pin 79) - PMOD JD GPIO0 */

  /* Port E GPIO Pins (Motor PWM) */
  k_gpio_pe0 = 0x0E00, /**< PE0 (pin 78) - Motor 2 EN */
  k_gpio_pe1 = 0x0E01, /**< PE1 (pin 77) - Motor 1 EN */
  k_gpio_pe2 = 0x0E02, /**< PE2 (pin 76) - Motor 0 EN */
  k_gpio_pe3 = 0x0E03, /**< PE3 (pin 75) - Motor 2 PH */
  k_gpio_pe4 = 0x0E04, /**< PE4 (pin 74) - Motor 1 PH */
  k_gpio_pe5 = 0x0E05, /**< PE5 (pin 73) - Motor 0 PH */
  k_gpio_pe6 = 0x0E06, /**< PE6 (pin 72) - Motor 3 EN */
  k_gpio_pe7 = 0x0E07, /**< PE7 (pin 71) - Motor 3 PH */

  /* Port J GPIO Pins */
  k_gpio_pj3 = 0x1003, /**< PJ3 (pin 4) - PMOD JE GPIO0 */
  k_gpio_pj5 = 0x1005, /**< PJ5 (pin 2) - JTAG/TDO */

  /* Aliases for common use cases (point to same enum values) */
  k_gpio_temp_sensor         = k_gpio_p05,  /**< 1-Wire Temperature Sensor (DS18B20+) */
  k_gpio_pmod_jb_gpio0       = k_gpio_pc6,  /**< PMOD JB GPIO 0 */
  k_gpio_pmod_jb_gpio1       = k_gpio_p55,  /**< PMOD JB GPIO 1 */
  k_gpio_pmod_jb_gpio2       = k_gpio_p54,  /**< PMOD JB GPIO 2 */
  k_gpio_pmod_jb_gpio3       = k_gpio_pa2,  /**< PMOD JB GPIO 3 */
  k_gpio_pmod_je_gpio0       = k_gpio_pj3,  /**< PMOD JE GPIO 0 */
  k_gpio_pmod_je_gpio1       = k_gpio_pb1,  /**< PMOD JE GPIO 1 */
  k_gpio_pmod_je_gpio2       = k_gpio_pb0,  /**< PMOD JE GPIO 2 */
  k_gpio_pmod_je_gpio3       = k_gpio_pa1,  /**< PMOD JE GPIO 3 */

  /* Force enum to be 16-bit wide for safety */
  k_gpio_pin_force_u16 = 0xFFFF
} gpio_pin_t;

/**
 * @brief Extract GPIO port number from gpio_pin_t enum
 *
 * @param[in] pin GPIO pin enum value
 *
 * @return Port number (0x0-0x10 for ports 0-J)
 */
static inline uint8_t gpio_pin_get_port(gpio_pin_t pin)
{
  return (uint8_t)((uint16_t)pin >> 8);
}

/**
 * @brief Extract GPIO pin number from gpio_pin_t enum
 *
 * @param[in] pin GPIO pin enum value
 *
 * @return Pin number (0-7)
 */
static inline uint8_t gpio_pin_get_pin(gpio_pin_t pin)
{
  return (uint8_t)((uint16_t)pin & 0xFF);
}

/**
 * @brief Create gpio_pin_t from port and pin numbers
 *
 * This function allows constructing a gpio_pin_t from separate port and pin
 * values, useful when working with legacy code or configuration data that
 * stores port and pin separately.
 *
 * @param[in] port Port number (0x0-0x10 for ports 0-J)
 * @param[in] pin Pin number (0-7)
 *
 * @return gpio_pin_t enum value
 *
 * @note No validation is performed - use valid port/pin combinations
 */
static inline gpio_pin_t gpio_pin_make(uint8_t port, uint8_t pin)
{
  return (gpio_pin_t)(((uint16_t)port << 8) | (uint16_t)pin);
}

/* =============================================================================
 * Motor Control - PWM Outputs (GPTW Channels)
 * =============================================================================
 */

/**
 * @brief Motor PWM pin assignments (GPTW channels)
 *
 * All motor PWM signals are on Port E (PE0-PE7, pins 71-78) for optimal PCB
 * routing. Each motor uses PH/EN control mode requiring 2 PWM outputs.
 *
 * PWM Peripheral: GPTW (General PWM Timer)
 * - 32-bit resolution
 * - 4 independent channels with complementary outputs
 * - Up to 25 kHz PWM frequency
 */
typedef enum {
  /* Motor 0 - GPTW Channel 0 */
  k_pin_motor0_ph_port = 0xE, /**< Port E */
  k_pin_motor0_ph_pin  = 5,   /**< PE5/GTIOC0A (pin 73) - Phase/Direction */
  k_pin_motor0_en_port = 0xE, /**< Port E */
  k_pin_motor0_en_pin  = 2,   /**< PE2/GTIOC0B (pin 76) - Enable/Speed */

  /* Motor 1 - GPTW Channel 1 */
  k_pin_motor1_ph_port = 0xE, /**< Port E */
  k_pin_motor1_ph_pin  = 4,   /**< PE4/GTIOC1A (pin 74) - Phase/Direction */
  k_pin_motor1_en_port = 0xE, /**< Port E */
  k_pin_motor1_en_pin  = 1,   /**< PE1/GTIOC1B (pin 77) - Enable/Speed */

  /* Motor 2 - GPTW Channel 2 */
  k_pin_motor2_ph_port = 0xE, /**< Port E */
  k_pin_motor2_ph_pin  = 3,   /**< PE3/GTIOC2A (pin 75) - Phase/Direction */
  k_pin_motor2_en_port = 0xE, /**< Port E */
  k_pin_motor2_en_pin  = 0,   /**< PE0/GTIOC2B (pin 78) - Enable/Speed */

  /* Motor 3 - GPTW Channel 3 */
  k_pin_motor3_ph_port = 0xE, /**< Port E */
  k_pin_motor3_ph_pin  = 7,   /**< PE7/GTIOC3A (pin 71) - Phase/Direction */
  k_pin_motor3_en_port = 0xE, /**< Port E */
  k_pin_motor3_en_pin  = 6,   /**< PE6/GTIOC3B (pin 72) - Enable/Speed */
} motor_pwm_pins_t;

/* =============================================================================
 * Encoder Inputs (MTU Quadrature Counting)
 * =============================================================================
 */

/**
 * @brief Encoder input pin assignments (MTU channels)
 *
 * Hall effect encoders provide quadrature signals (Phase A and Phase B) for
 * precise position and velocity measurement. MTU peripheral provides hardware
 * quadrature decoding.
 *
 * Encoder Peripheral: MTU (Multi-Function Timer Unit)
 * - Hardware quadrature counting (no CPU intervention)
 * - 16-bit counters with overflow detection
 * - MTU1/2/3/4 dedicated to encoders (MTU0 reserved)
 */
typedef enum {
  /* Encoder 0 - MTU1 */
  k_pin_encoder0_a_port = 2, /**< Port 2 */
  k_pin_encoder0_a_pin  = 4, /**< P24/MTCLKA (pin 24) - Phase A */
  k_pin_encoder0_b_port = 1, /**< Port 1 */
  k_pin_encoder0_b_pin  = 5, /**< P15/MTCLKB (pin 31) - Phase B */

  /* Encoder 1 - MTU2 */
  k_pin_encoder1_a_port = 1, /**< Port 1 */
  k_pin_encoder1_a_pin  = 4, /**< P14/MTCLKA (pin 32) - Phase A */
  k_pin_encoder1_b_port = 2, /**< Port 2 */
  k_pin_encoder1_b_pin  = 2, /**< P22/MTCLKC (pin 26) - Phase B */

  /* Encoder 2 - MTU3 */
  k_pin_encoder2_a_port = 0xC, /**< Port C */
  k_pin_encoder2_a_pin  = 1,   /**< PC1/MTIOC3A (pin 51) - Phase A */
  k_pin_encoder2_b_port = 0xC, /**< Port C */
  k_pin_encoder2_b_pin  = 0,   /**< PC0/MTIOC3C (pin 52) - Phase B */

  /* Encoder 3 - MTU4 */
  k_pin_encoder3_a_port = 2,   /**< Port 2 */
  k_pin_encoder3_a_pin  = 5,   /**< P25/MTCLKB (pin 23) - Phase A */
  k_pin_encoder3_b_port = 0xA, /**< Port A */
  k_pin_encoder3_b_pin  = 3,   /**< PA3/MTCLKD (pin 67) - Phase B */
} encoder_pins_t;

/* =============================================================================
 * ADC - Current Sensing
 * =============================================================================
 */

/**
 * @brief ADC pin assignments for motor current sensing
 *
 * Each motor driver (DRV8243S) provides an IPROPI analog output proportional
 * to motor current. Measured via 12-bit ADC for overcurrent protection.
 *
 * ADC Configuration:
 * - Unit 0 channels (AN000-AN003)
 * - 12-bit resolution
 * - VREFH = 3.3V, VREFL = GND
 * - Typical Ki_PROPI = 525 A/V
 */
typedef enum {
  k_adc_motor0_current = 0, /**< P40/AN000 (pin 95) - Motor 0 IPROPI */
  k_adc_motor1_current = 1, /**< P41/AN001 (pin 93) - Motor 1 IPROPI */
  k_adc_motor2_current = 2, /**< P42/AN002 (pin 92) - Motor 2 IPROPI */
  k_adc_motor3_current = 3, /**< P43/AN003 (pin 91) - Motor 3 IPROPI */
} adc_channels_t;

/* =============================================================================
 * GPIO - Motor Driver Fault Detection
 * =============================================================================
 */

/**
 * @brief Motor driver nFAULT pin assignments
 *
 * DRV8243S asserts nFAULT low on overcurrent, thermal shutdown, undervoltage,
 * or overvoltage conditions. All pins are IRQ-capable for interrupt-driven
 * fault handling.
 *
 * Fault Pin Configuration:
 * - Active low (pulled high by internal pull-up)
 * - IRQ12-IRQ15 capable for interrupt-driven detection
 * - Read via PORT.PIDR register
 */
typedef enum {
  k_pin_motor0_nfault_port = 4, /**< Port 4 */
  k_pin_motor0_nfault_pin  = 4, /**< P44/IRQ12-DS (pin 90) */

  k_pin_motor1_nfault_port = 4, /**< Port 4 */
  k_pin_motor1_nfault_pin  = 5, /**< P45/IRQ13-DS (pin 89) */

  k_pin_motor2_nfault_port = 4, /**< Port 4 */
  k_pin_motor2_nfault_pin  = 6, /**< P46/IRQ14-DS (pin 88) */

  k_pin_motor3_nfault_port = 4, /**< Port 4 */
  k_pin_motor3_nfault_pin  = 7, /**< P47/IRQ15-DS (pin 87) */
} motor_fault_pins_t;

/* =============================================================================
 * SPI - Raspberry Pi 5 Communication
 * =============================================================================
 */

/**
 * @brief SPI pin assignments for RPi5 communication
 *
 * High-speed SPI interface for Protocol Buffer messages between RX72N and
 * Raspberry Pi 5. Configured at 10 Mbps for motor commands, encoder feedback,
 * and telemetry data.
 *
 * SPI Peripheral: RSPIA (RSPIA-B alternate function)
 * - 10 Mbps clock frequency
 * - Mode 0 (CPOL=0, CPHA=0)
 * - RPi5 is controller, RX72N is peripheral
 */
typedef enum {
  k_pin_rpi5_spi_cs_port   = 0xA, /**< Port A */
  k_pin_rpi5_spi_cs_pin    = 4,   /**< PA4/SSLA0-B (pin 66) - Chip Select */
  k_pin_rpi5_spi_sclk_port = 0xA, /**< Port A */
  k_pin_rpi5_spi_sclk_pin  = 5,   /**< PA5/RSPCKA-B (pin 65) - Clock */
  k_pin_rpi5_spi_copi_port = 0xA, /**< Port A */
  k_pin_rpi5_spi_copi_pin  = 6,   /**< PA6/MOSIA-B (pin 64) - Controller Out */
  k_pin_rpi5_spi_cipo_port = 0xA, /**< Port A */
  k_pin_rpi5_spi_cipo_pin  = 7,   /**< PA7/MISOA-B (pin 63) - Controller In */
} rpi5_spi_pins_t;

/* =============================================================================
 * SPI - Motor Driver Configuration
 * =============================================================================
 */

/**
 * @brief SPI pin assignments for motor driver configuration
 *
 * Separate SPI bus for configuring DRV8243S motor drivers. Used for setting
 * current limits, slew rates, dead-time insertion, and fault thresholds.
 *
 * SPI Peripheral: RSPIC (RSPIC-A alternate function)
 * - 1 MHz clock frequency (configuration not time-critical)
 * - Shared SCLK/COPI/CIPO, individual chip selects per motor
 * - Configure at startup or when changing operating parameters
 */
typedef enum {
  /* Shared SPI signals */
  k_pin_motor_spi_sclk_port = 0xD, /**< Port D */
  k_pin_motor_spi_sclk_pin  = 3,   /**< PD3/RSPCKC-A (pin 83) - Clock */
  k_pin_motor_spi_copi_port = 0xD, /**< Port D */
  k_pin_motor_spi_copi_pin  = 1,   /**< PD1/MOSIC-A (pin 85) - Controller Out */
  k_pin_motor_spi_cipo_port = 0xD, /**< Port D */
  k_pin_motor_spi_cipo_pin  = 2,   /**< PD2/MISOC-A (pin 84) - Controller In */

  /* Individual chip selects */
  k_pin_motor0_spi_cs_port = 1,   /**< Port 1 */
  k_pin_motor0_spi_cs_pin  = 7,   /**< P17 (pin 29) - Motor 0 nCS */
  k_pin_motor1_spi_cs_port = 2,   /**< Port 2 */
  k_pin_motor1_spi_cs_pin  = 3,   /**< P23 (pin 25) - Motor 1 nCS */
  k_pin_motor2_spi_cs_port = 3,   /**< Port 3 */
  k_pin_motor2_spi_cs_pin  = 2,   /**< P32 (pin 18) - Motor 2 nCS */
  k_pin_motor3_spi_cs_port = 0xA, /**< Port A */
  k_pin_motor3_spi_cs_pin  = 0,   /**< PA0 (pin 70) - Motor 3 nCS */
} motor_spi_pins_t;

/* =============================================================================
 * I2C/SMBUS - Battery Management System
 * =============================================================================
 */

/**
 * @brief I2C/SMBUS pin assignments for BMS communication
 *
 * Dedicated I2C bus for battery management system (BQ25798 fuel gauge).
 * Supports SMBus protocol with Fast Mode Plus (1 MHz).
 *
 * I2C Peripheral: RIIC0
 * - Fast Mode Plus capable (1 MHz)
 * - SMBus compatible
 * - External 2.2kΩ pull-ups required
 */
typedef enum {
  k_pin_bms_scl_port = 1, /**< Port 1 */
  k_pin_bms_scl_pin  = 2, /**< P12/SMBC0 (pin 34) - SMBUS Clock */
  k_pin_bms_sda_port = 1, /**< Port 1 */
  k_pin_bms_sda_pin  = 3, /**< P13/SMBD0 (pin 33) - SMBUS Data */
} bms_i2c_pins_t;

/* =============================================================================
 * UART - Debug Console
 * =============================================================================
 */

/**
 * @brief UART pin assignments for debug console
 *
 * Debug UART connected to USB-C via CY7C65213A USB-to-UART bridge.
 * Used for printf debugging, logging, and system diagnostics.
 *
 * UART Peripheral: SCI9
 * - 115200 baud rate
 * - 8N1 configuration
 * - TX/RX crossed in hardware
 *
 * Note: Requires MPC configuration (see Table 7 in hardware_pinout.tex)
 */
typedef enum {
  k_pin_debug_uart_tx_port = 0xB, /**< Port B */
  k_pin_debug_uart_tx_pin  = 7,   /**< PB7/TXD9 (pin 53) - UART TX */
  k_pin_debug_uart_rx_port = 0xB, /**< Port B */
  k_pin_debug_uart_rx_pin  = 6,   /**< PB6/RXD9 (pin 54) - UART RX */
} debug_uart_pins_t;

/* =============================================================================
 * 1-Wire - Temperature Sensor
 * =============================================================================
 */

/**
 * @brief 1-Wire pin assignment for DS18B20+ temperature sensor
 *
 * Digital temperature sensor for ambient monitoring and thermal management.
 * Supports parasite power mode or separate VDD connection.
 *
 * 1-Wire Configuration:
 * - 4.7kΩ pull-up resistor required (P05 to 3.3V)
 * - IRQ13 capable for interrupt-driven communication
 * - Temperature range: -55°C to +125°C
 * - Resolution: 9-12 bit configurable (0.5°C to 0.0625°C)
 */
typedef enum {
  k_pin_temp_sensor_port = 0, /**< Port 0 */
  k_pin_temp_sensor_pin  = 5, /**< P05 (pin 100) - 1-Wire DQ */
} temp_sensor_pins_t;

/* =============================================================================
 * PMOD Expansion Connectors
 * =============================================================================
 */

/**
 * @brief PMOD JA - High-Speed SPI/Display Interface
 *
 * Standard 12-pin PMOD connector for SPI peripherals (displays, SD cards,
 * sensors). Supports 2 chip selects for dual-device configurations.
 *
 * Note: PMOD JA GPIO pin is not available on LQFP-176 package (PB2 does not
 * exist). Verify actual hardware schematic for correct GPIO assignment.
 */
typedef enum {
  k_pmod_ja_cs0_port  = 5,   /**< Port 5 */
  k_pmod_ja_cs0_pin   = 2,   /**< P52 (pin 42) - SPI CS0 */
  k_pmod_ja_copi_port = 5,   /**< Port 5 */
  k_pmod_ja_copi_pin  = 0,   /**< P50 (pin 44) - SPI COPI */
  k_pmod_ja_cipo_port = 5,   /**< Port 5 */
  k_pmod_ja_cipo_pin  = 1,   /**< P51 (pin 43) - SPI CIPO */
  k_pmod_ja_sck_port  = 5,   /**< Port 5 */
  k_pmod_ja_sck_pin   = 3,   /**< P53 (pin 41) - SPI SCK */
  k_pmod_ja_dc_port   = 0xB, /**< Port B */
  k_pmod_ja_dc_pin    = 4,   /**< PB4 (pin 57) - Data/Command */
  k_pmod_ja_rst_port  = 0xB, /**< Port B */
  k_pmod_ja_rst_pin   = 3,   /**< PB3 (pin 58) - Reset */
  k_pmod_ja_cs1_port  = 0xB, /**< Port B */
  k_pmod_ja_cs1_pin   = 5,   /**< PB5 (pin 55) - SPI CS1 */
} pmod_ja_pins_t;

/**
 * @brief PMOD JB - GPIO Expansion
 *
 * 4 general-purpose GPIO pins for buttons, switches, LEDs, or control signals.
 */
typedef enum {
  k_pmod_jb_gpio0_port = 0xC, /**< Port C */
  k_pmod_jb_gpio0_pin  = 6,   /**< PC6 (pin 46) - GPIO 0 */
  k_pmod_jb_gpio1_port = 5,   /**< Port 5 */
  k_pmod_jb_gpio1_pin  = 5,   /**< P55 (pin 39) - GPIO 1 */
  k_pmod_jb_gpio2_port = 5,   /**< Port 5 */
  k_pmod_jb_gpio2_pin  = 4,   /**< P54 (pin 40) - GPIO 2 */
  k_pmod_jb_gpio3_port = 0xA, /**< Port A */
  k_pmod_jb_gpio3_pin  = 2,   /**< PA2 (pin 68) - GPIO 3 */
} pmod_jb_pins_t;

/**
 * @brief PMOD JC - I2C Sensor Interface with Interrupts
 *
 * I2C connector with 2 interrupt-capable pins for sensor data ready signals.
 */
typedef enum {
  k_pmod_jc_int1_port = 0xC, /**< Port C */
  k_pmod_jc_int1_pin  = 4,   /**< PC4 (pin 48) - Interrupt 1 */
  k_pmod_jc_int2_port = 0xC, /**< Port C */
  k_pmod_jc_int2_pin  = 5,   /**< PC5 (pin 47) - Interrupt 2 */
  k_pmod_jc_scl_port  = 0xC, /**< Port C */
  k_pmod_jc_scl_pin   = 2,   /**< PC2 (pin 50) - I2C Clock */
  k_pmod_jc_sda_port  = 0xC, /**< Port C */
  k_pmod_jc_sda_pin   = 3,   /**< PC3 (pin 49) - I2C Data */
} pmod_jc_pins_t;

/**
 * @brief PMOD JD - GPIO Expansion
 *
 * 4 general-purpose GPIO pins (alternate functions: SCI12 UART, QSPI).
 */
typedef enum {
  k_pmod_jd_gpio0_port = 0xD, /**< Port D */
  k_pmod_jd_gpio0_pin  = 7,   /**< PD7 (pin 79) - GPIO 0 */
  k_pmod_jd_gpio1_port = 0xD, /**< Port D */
  k_pmod_jd_gpio1_pin  = 6,   /**< PD6 (pin 80) - GPIO 1 */
  k_pmod_jd_gpio2_port = 0xD, /**< Port D */
  k_pmod_jd_gpio2_pin  = 5,   /**< PD5 (pin 81) - GPIO 2 */
  k_pmod_jd_gpio3_port = 0xD, /**< Port D */
  k_pmod_jd_gpio3_pin  = 4,   /**< PD4 (pin 82) - GPIO 3 */
} pmod_jd_pins_t;

/**
 * @brief PMOD JE - GPIO Expansion with IRQ Capability
 *
 * 4 GPIO pins with IRQ support for interrupt-driven applications.
 */
typedef enum {
  k_pmod_je_gpio0_port = 0x10, /**< Port J */
  k_pmod_je_gpio0_pin  = 3,    /**< PJ3 (pin 4) - GPIO 0 */
  k_pmod_je_gpio1_port = 0xB,  /**< Port B */
  k_pmod_je_gpio1_pin  = 1,    /**< PB1 (pin 59) - GPIO 1 */
  k_pmod_je_gpio2_port = 0xB,  /**< Port B */
  k_pmod_je_gpio2_pin  = 0,    /**< PB0 (pin 61) - GPIO 2 */
  k_pmod_je_gpio3_port = 0xA,  /**< Port A */
  k_pmod_je_gpio3_pin  = 1,    /**< PA1 (pin 69) - GPIO 3 */
} pmod_je_pins_t;

/**
 * @brief PMOD JF - I2C Expansion with GPIO
 *
 * I2C connector (RIIC1) with 2 additional GPIO pins.
 */
typedef enum {
  k_pmod_jf_gpio0_port = 0xD, /**< Port D */
  k_pmod_jf_gpio0_pin  = 0,   /**< PD0 (pin 86) - GPIO 0 */
  k_pmod_jf_gpio1_port = 0,   /**< Port 0 */
  k_pmod_jf_gpio1_pin  = 7,   /**< P07 (pin 98) - GPIO 1 */
  k_pmod_jf_scl_port   = 2,   /**< Port 2 */
  k_pmod_jf_scl_pin    = 1,   /**< P21 (pin 27) - I2C Clock */
  k_pmod_jf_sda_port   = 2,   /**< Port 2 */
  k_pmod_jf_sda_pin    = 0,   /**< P20 (pin 28) - I2C Data */
} pmod_jf_pins_t;

/* =============================================================================
 * Debug/JTAG Pins (Read-Only Reference)
 * =============================================================================
 */

/**
 * @brief Debug and JTAG pin assignments
 *
 * These pins are used for hardware debugging via E1/E2 Lite emulator.
 * DO NOT reconfigure these pins in application code.
 *
 * JTAG Signals:
 * - TMS, TDI, TDO, TCK, TRST#
 * - Connected to E1/E2 Lite emulator connector
 * - Required for GDB debugging and flash programming
 */
typedef enum {
  k_pin_jtag_tms_port  = 3, /**< Port 3 */
  k_pin_jtag_tms_pin   = 1, /**< P31/TMS (pin 19) - JTAG Test Mode Select */
  k_pin_jtag_tdi_port  = 3, /**< Port 3 */
  k_pin_jtag_tdi_pin   = 0, /**< P30/TDI (pin 20) - JTAG Test Data In */
  k_pin_jtag_tdo_port  = 2, /**< Port 2 */
  k_pin_jtag_tdo_pin   = 6, /**< P26/TDO (pin 22) - JTAG Test Data Out */
  k_pin_jtag_tck_port  = 2, /**< Port 2 */
  k_pin_jtag_tck_pin   = 7, /**< P27/TCK (pin 21) - JTAG Test Clock */
  k_pin_jtag_trst_port = 3, /**< Port 3 */
  k_pin_jtag_trst_pin  = 4, /**< P34/TST# (pin 16) - JTAG Test Reset */
} jtag_pins_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_HARDWARE_PINOUT_H */
