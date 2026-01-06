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

#include "rx_port_constants.h"

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
  k_gpio_p05 = k_rx_p0_5, /**< P05 (pin 100) - 1-Wire Temperature Sensor (DS18B20) */
  k_gpio_p07 = k_rx_p0_7, /**< P07 (pin 98) - PMOD JF GPIO1 */

  /* Port 1 GPIO Pins */
  k_gpio_p12 = k_rx_p1_2, /**< P12 (pin 34) - BMS I2C SCL */
  k_gpio_p13 = k_rx_p1_3, /**< P13 (pin 33) - BMS I2C SDA */
  k_gpio_p14 = k_rx_p1_4, /**< P14 (pin 32) - Encoder 1 Phase A */
  k_gpio_p15 = k_rx_p1_5, /**< P15 (pin 31) - Encoder 0 Phase B */
  k_gpio_p16 = k_rx_p1_6, /**< P16 (pin 30) */
  k_gpio_p17 = k_rx_p1_7, /**< P17 (pin 29) - Motor 0 SPI CS */

  /* Port 2 GPIO Pins */
  k_gpio_p20 = k_rx_p2_0, /**< P20 (pin 28) - PMOD JF I2C SDA */
  k_gpio_p21 = k_rx_p2_1, /**< P21 (pin 27) - PMOD JF I2C SCL */
  k_gpio_p22 = k_rx_p2_2, /**< P22 (pin 26) - Encoder 1 Phase B */
  k_gpio_p23 = k_rx_p2_3, /**< P23 (pin 25) - Motor 1 SPI CS */
  k_gpio_p24 = k_rx_p2_4, /**< P24 (pin 24) - Encoder 0 Phase A */
  k_gpio_p25 = k_rx_p2_5, /**< P25 (pin 23) - Encoder 3 Phase A */
  k_gpio_p26 = k_rx_p2_6, /**< P26 (pin 22) - JTAG TDO */
  k_gpio_p27 = k_rx_p2_7, /**< P27 (pin 21) - JTAG TCK */

  /* Port 3 GPIO Pins */
  k_gpio_p30 = k_rx_p3_0, /**< P30 (pin 20) - JTAG TDI */
  k_gpio_p31 = k_rx_p3_1, /**< P31 (pin 19) - JTAG TMS */
  k_gpio_p32 = k_rx_p3_2, /**< P32 (pin 18) - Motor 2 SPI CS */
  k_gpio_p33 = k_rx_p3_3, /**< P33 (pin 17) - USB UART Reset */
  k_gpio_p34 = k_rx_p3_4, /**< P34 (pin 16) - JTAG TRST */
  k_gpio_p35 = k_rx_p3_5, /**< P35 (pin 15) - NMI/UPSEL */
  k_gpio_p36 = k_rx_p3_6, /**< P36 (pin 13) - EXTAL (24MHz Crystal) */
  k_gpio_p37 = k_rx_p3_7, /**< P37 (pin 11) - XTAL (24MHz Crystal) */

  /* Port 4 GPIO Pins (ADC + nFAULT) */
  k_gpio_p40 = k_rx_p4_0, /**< P40 (pin 95) - Motor 0 Current ADC */
  k_gpio_p41 = k_rx_p4_1, /**< P41 (pin 93) - Motor 1 Current ADC */
  k_gpio_p42 = k_rx_p4_2, /**< P42 (pin 92) - Motor 2 Current ADC */
  k_gpio_p43 = k_rx_p4_3, /**< P43 (pin 91) - Motor 3 Current ADC */
  k_gpio_p44 = k_rx_p4_4, /**< P44 (pin 90) - Motor 0 nFAULT */
  k_gpio_p45 = k_rx_p4_5, /**< P45 (pin 89) - Motor 1 nFAULT */
  k_gpio_p46 = k_rx_p4_6, /**< P46 (pin 88) - Motor 2 nFAULT */
  k_gpio_p47 = k_rx_p4_7, /**< P47 (pin 87) - Motor 3 nFAULT */

  /* Port 5 GPIO Pins (PMOD JA + PMOD JB) */
  k_gpio_p50 = k_rx_p5_0, /**< P50 (pin 44) - PMOD JA COPI */
  k_gpio_p51 = k_rx_p5_1, /**< P51 (pin 43) - PMOD JA CIPO */
  k_gpio_p52 = k_rx_p5_2, /**< P52 (pin 42) - PMOD JA CS0 */
  k_gpio_p53 = k_rx_p5_3, /**< P53 (pin 41) - PMOD JA SCK */
  k_gpio_p54 = k_rx_p5_4, /**< P54 (pin 40) - PMOD JB GPIO2 */
  k_gpio_p55 = k_rx_p5_5, /**< P55 (pin 39) - PMOD JB GPIO1 */

  /* Port A GPIO Pins (SPI + PMOD) */
  k_gpio_pa0 = k_rx_pa_0, /**< PA0 (pin 70) - Motor 3 SPI CS */
  k_gpio_pa1 = k_rx_pa_1, /**< PA1 (pin 69) - PMOD JE GPIO3 */
  k_gpio_pa2 = k_rx_pa_2, /**< PA2 (pin 68) - PMOD JB GPIO3 */
  k_gpio_pa3 = k_rx_pa_3, /**< PA3 (pin 67) - Encoder 3 Phase B */
  k_gpio_pa4 = k_rx_pa_4, /**< PA4 (pin 66) - RPi5 SPI CS */
  k_gpio_pa5 = k_rx_pa_5, /**< PA5 (pin 65) - RPi5 SPI SCLK */
  k_gpio_pa6 = k_rx_pa_6, /**< PA6 (pin 64) - RPi5 SPI COPI */
  k_gpio_pa7 = k_rx_pa_7, /**< PA7 (pin 63) - RPi5 SPI CIPO */

  /* Port B GPIO Pins (Debug UART + PMOD) */
  k_gpio_pb0 = k_rx_pb_0, /**< PB0 (pin 61) - PMOD JE GPIO2 */
  k_gpio_pb1 = k_rx_pb_1, /**< PB1 (pin 59) - PMOD JE GPIO1 */
  k_gpio_pb2 = k_rx_pb_2, /**< PB2 (pin 58) - PMOD JA GPIO */
  k_gpio_pb3 = k_rx_pb_3, /**< PB3 (pin 57) - PMOD JA RST */
  k_gpio_pb4 = k_rx_pb_4, /**< PB4 (pin 56) - PMOD JA DC */
  k_gpio_pb5 = k_rx_pb_5, /**< PB5 (pin 55) - PMOD JA CS1 */
  k_gpio_pb6 = k_rx_pb_6, /**< PB6 (pin 54) - Debug UART RX */
  k_gpio_pb7 = k_rx_pb_7, /**< PB7 (pin 53) - Debug UART TX */

  /* Port C GPIO Pins (Encoder + PMOD) */
  k_gpio_pc0 = k_rx_pc_0, /**< PC0 (pin 52) - Encoder 2 Phase B */
  k_gpio_pc1 = k_rx_pc_1, /**< PC1 (pin 51) - Encoder 2 Phase A */
  k_gpio_pc2 = k_rx_pc_2, /**< PC2 (pin 50) - PMOD JC I2C SCL */
  k_gpio_pc3 = k_rx_pc_3, /**< PC3 (pin 49) - PMOD JC I2C SDA */
  k_gpio_pc4 = k_rx_pc_4, /**< PC4 (pin 48) - PMOD JC INT1 */
  k_gpio_pc5 = k_rx_pc_5, /**< PC5 (pin 47) - PMOD JC INT2 */
  k_gpio_pc6 = k_rx_pc_6, /**< PC6 (pin 46) - PMOD JB GPIO0 */
  k_gpio_pc7 = k_rx_pc_7, /**< PC7 (pin 45) */

  /* Port D GPIO Pins (Motor SPI + PMOD JD + PMOD JF) */
  k_gpio_pd0 = k_rx_pd_0, /**< PD0 (pin 86) - PMOD JF GPIO0 */
  k_gpio_pd1 = k_rx_pd_1, /**< PD1 (pin 85) - Motor SPI COPI */
  k_gpio_pd2 = k_rx_pd_2, /**< PD2 (pin 84) - Motor SPI CIPO */
  k_gpio_pd3 = k_rx_pd_3, /**< PD3 (pin 83) - Motor SPI SCLK */
  k_gpio_pd4 = k_rx_pd_4, /**< PD4 (pin 82) - PMOD JD GPIO3 */
  k_gpio_pd5 = k_rx_pd_5, /**< PD5 (pin 81) - PMOD JD GPIO2 */
  k_gpio_pd6 = k_rx_pd_6, /**< PD6 (pin 80) - PMOD JD GPIO1 */
  k_gpio_pd7 = k_rx_pd_7, /**< PD7 (pin 79) - PMOD JD GPIO0 */

  /* Port E GPIO Pins (Motor PWM) */
  k_gpio_pe0 = k_rx_pe_0, /**< PE0 (pin 78) - Motor 2 EN */
  k_gpio_pe1 = k_rx_pe_1, /**< PE1 (pin 77) - Motor 1 EN */
  k_gpio_pe2 = k_rx_pe_2, /**< PE2 (pin 76) - Motor 0 EN */
  k_gpio_pe3 = k_rx_pe_3, /**< PE3 (pin 75) - Motor 2 PH */
  k_gpio_pe4 = k_rx_pe_4, /**< PE4 (pin 74) - Motor 1 PH */
  k_gpio_pe5 = k_rx_pe_5, /**< PE5 (pin 73) - Motor 0 PH */
  k_gpio_pe6 = k_rx_pe_6, /**< PE6 (pin 72) - Motor 3 EN */
  k_gpio_pe7 = k_rx_pe_7, /**< PE7 (pin 71) - Motor 3 PH */

  /* Port J GPIO Pins */
  k_gpio_pj3 = k_rx_pj_3, /**< PJ3 (pin 4) - PMOD JE GPIO0 */
  k_gpio_pj5 = k_rx_pj_5, /**< PJ5 (pin 2) - JTAG/TDO */

  /* Aliases for common use cases (point to same enum values) */
  k_gpio_temp_sensor   = k_gpio_p05, /**< 1-Wire Temperature Sensor (DS18B20+) */
  k_gpio_pmod_jb_gpio0 = k_gpio_pc6, /**< PMOD JB GPIO 0 */
  k_gpio_pmod_jb_gpio1 = k_gpio_p55, /**< PMOD JB GPIO 1 */
  k_gpio_pmod_jb_gpio2 = k_gpio_p54, /**< PMOD JB GPIO 2 */
  k_gpio_pmod_jb_gpio3 = k_gpio_pa2, /**< PMOD JB GPIO 3 */
  k_gpio_pmod_je_gpio0 = k_gpio_pj3, /**< PMOD JE GPIO 0 */
  k_gpio_pmod_je_gpio1 = k_gpio_pb1, /**< PMOD JE GPIO 1 */
  k_gpio_pmod_je_gpio2 = k_gpio_pb0, /**< PMOD JE GPIO 2 */
  k_gpio_pmod_je_gpio3 = k_gpio_pa1, /**< PMOD JE GPIO 3 */
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
  k_pin_motor0_ph =
    (k_rx_port_e << k_port_shift) | k_rx_pin_5, /**< PE5/GTIOC0A (pin 73) - Phase/Direction */
  k_pin_motor0_en =
    (k_rx_port_e << k_port_shift) | k_rx_pin_2, /**< PE2/GTIOC0B (pin 76) - Enable/Speed */

  /* Motor 1 - GPTW Channel 1 */
  k_pin_motor1_ph =
    (k_rx_port_e << k_port_shift) | k_rx_pin_4, /**< PE4/GTIOC1A (pin 74) - Phase/Direction */
  k_pin_motor1_en =
    (k_rx_port_e << k_port_shift) | k_rx_pin_1, /**< PE1/GTIOC1B (pin 77) - Enable/Speed */

  /* Motor 2 - GPTW Channel 2 */
  k_pin_motor2_ph =
    (k_rx_port_e << k_port_shift) | k_rx_pin_3, /**< PE3/GTIOC2A (pin 75) - Phase/Direction */
  k_pin_motor2_en =
    (k_rx_port_e << k_port_shift) | k_rx_pin_0, /**< PE0/GTIOC2B (pin 78) - Enable/Speed */

  /* Motor 3 - GPTW Channel 3 */
  k_pin_motor3_ph =
    (k_rx_port_e << k_port_shift) | k_rx_pin_7, /**< PE7/GTIOC3A (pin 71) - Phase/Direction */
  k_pin_motor3_en =
    (k_rx_port_e << k_port_shift) | k_rx_pin_6, /**< PE6/GTIOC3B (pin 72) - Enable/Speed */
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
  k_pin_encoder0_a =
    (k_rx_port_2 << k_port_shift) | k_rx_pin_4, /**< P24/MTCLKA (pin 24) - Phase A */
  k_pin_encoder0_b =
    (k_rx_port_1 << k_port_shift) | k_rx_pin_5, /**< P15/MTCLKB (pin 31) - Phase B */

  /* Encoder 1 - MTU2 */
  k_pin_encoder1_a =
    (k_rx_port_1 << k_port_shift) | k_rx_pin_4, /**< P14/MTCLKA (pin 32) - Phase A */
  k_pin_encoder1_b =
    (k_rx_port_2 << k_port_shift) | k_rx_pin_2, /**< P22/MTCLKC (pin 26) - Phase B */

  /* Encoder 2 - MTU3 */
  k_pin_encoder2_a =
    (k_rx_port_c << k_port_shift) | k_rx_pin_1, /**< PC1/MTIOC3A (pin 51) - Phase A */
  k_pin_encoder2_b =
    (k_rx_port_c << k_port_shift) | k_rx_pin_0, /**< PC0/MTIOC3C (pin 52) - Phase B */

  /* Encoder 3 - MTU4 */
  k_pin_encoder3_a =
    (k_rx_port_2 << k_port_shift) | k_rx_pin_5, /**< P25/MTCLKB (pin 23) - Phase A */
  k_pin_encoder3_b =
    (k_rx_port_a << k_port_shift) | k_rx_pin_3, /**< PA3/MTCLKD (pin 67) - Phase B */
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
  k_pin_motor0_nfault = (k_rx_port_4 << k_port_shift) | k_rx_pin_4, /**< P44/IRQ12-DS (pin 90) */
  k_pin_motor1_nfault = (k_rx_port_4 << k_port_shift) | k_rx_pin_5, /**< P45/IRQ13-DS (pin 89) */
  k_pin_motor2_nfault = (k_rx_port_4 << k_port_shift) | k_rx_pin_6, /**< P46/IRQ14-DS (pin 88) */
  k_pin_motor3_nfault = (k_rx_port_4 << k_port_shift) | k_rx_pin_7, /**< P47/IRQ15-DS (pin 87) */
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
  k_pin_rpi5_spi_cs =
    (k_rx_port_a << k_port_shift) | k_rx_pin_4, /**< PA4/SSLA0-B (pin 66) - Chip Select */
  k_pin_rpi5_spi_sclk =
    (k_rx_port_a << k_port_shift) | k_rx_pin_5, /**< PA5/RSPCKA-B (pin 65) - Clock */
  k_pin_rpi5_spi_copi =
    (k_rx_port_a << k_port_shift) | k_rx_pin_6, /**< PA6/MOSIA-B (pin 64) - Controller Out */
  k_pin_rpi5_spi_cipo =
    (k_rx_port_a << k_port_shift) | k_rx_pin_7, /**< PA7/MISOA-B (pin 63) - Controller In */
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
  k_pin_motor_spi_sclk =
    (k_rx_port_d << k_port_shift) | k_rx_pin_3, /**< PD3/RSPCKC-A (pin 83) - Clock */
  k_pin_motor_spi_copi =
    (k_rx_port_d << k_port_shift) | k_rx_pin_1, /**< PD1/MOSIC-A (pin 85) - Controller Out */
  k_pin_motor_spi_cipo =
    (k_rx_port_d << k_port_shift) | k_rx_pin_2, /**< PD2/MISOC-A (pin 84) - Controller In */

  /* Individual chip selects */
  k_pin_motor0_spi_cs =
    (k_rx_port_1 << k_port_shift) | k_rx_pin_7, /**< P17 (pin 29) - Motor 0 nCS */
  k_pin_motor1_spi_cs =
    (k_rx_port_2 << k_port_shift) | k_rx_pin_3, /**< P23 (pin 25) - Motor 1 nCS */
  k_pin_motor2_spi_cs =
    (k_rx_port_3 << k_port_shift) | k_rx_pin_2, /**< P32 (pin 18) - Motor 2 nCS */
  k_pin_motor3_spi_cs =
    (k_rx_port_a << k_port_shift) | k_rx_pin_0, /**< PA0 (pin 70) - Motor 3 nCS */
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
  k_pin_bms_scl =
    (k_rx_port_1 << k_port_shift) | k_rx_pin_2, /**< P12/SMBC0 (pin 34) - SMBUS Clock */
  k_pin_bms_sda =
    (k_rx_port_1 << k_port_shift) | k_rx_pin_3, /**< P13/SMBD0 (pin 33) - SMBUS Data */
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
  k_pin_debug_uart_tx =
    (k_rx_port_b << k_port_shift) | k_rx_pin_7, /**< PB7/TXD9 (pin 53) - UART TX */
  k_pin_debug_uart_rx =
    (k_rx_port_b << k_port_shift) | k_rx_pin_6, /**< PB6/RXD9 (pin 54) - UART RX */
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
  k_pin_temp_sensor = (k_rx_port_0 << k_port_shift) | k_rx_pin_5, /**< P05 (pin 100) - 1-Wire DQ */
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
 */
typedef enum {
  k_pmod_ja_cs0  = (k_rx_port_5 << k_port_shift) | k_rx_pin_2, /**< P52 (pin 42) - SPI CS0 */
  k_pmod_ja_copi = (k_rx_port_5 << k_port_shift) | k_rx_pin_0, /**< P50 (pin 44) - SPI COPI */
  k_pmod_ja_cipo = (k_rx_port_5 << k_port_shift) | k_rx_pin_1, /**< P51 (pin 43) - SPI CIPO */
  k_pmod_ja_sck  = (k_rx_port_5 << k_port_shift) | k_rx_pin_3, /**< P53 (pin 41) - SPI SCK */
  k_pmod_ja_dc   = (k_rx_port_b << k_port_shift) | k_rx_pin_4, /**< PB4 (pin 56) - Data/Command */
  k_pmod_ja_rst  = (k_rx_port_b << k_port_shift) | k_rx_pin_3, /**< PB3 (pin 57) - Reset */
  k_pmod_ja_gpio =
    (k_rx_port_b << k_port_shift) | k_rx_pin_2,               /**< PB2 (pin 58) - General Purpose */
  k_pmod_ja_cs1 = (k_rx_port_b << k_port_shift) | k_rx_pin_5, /**< PB5 (pin 55) - SPI CS1 */
} pmod_ja_pins_t;

/**
 * @brief PMOD JB - GPIO Expansion
 *
 * 4 general-purpose GPIO pins for buttons, switches, LEDs, or control signals.
 */
typedef enum {
  k_pmod_jb_gpio0 = (k_rx_port_c << k_port_shift) | k_rx_pin_6, /**< PC6 (pin 46) - GPIO 0 */
  k_pmod_jb_gpio1 = (k_rx_port_5 << k_port_shift) | k_rx_pin_5, /**< P55 (pin 39) - GPIO 1 */
  k_pmod_jb_gpio2 = (k_rx_port_5 << k_port_shift) | k_rx_pin_4, /**< P54 (pin 40) - GPIO 2 */
  k_pmod_jb_gpio3 = (k_rx_port_a << k_port_shift) | k_rx_pin_2, /**< PA2 (pin 68) - GPIO 3 */
} pmod_jb_pins_t;

/**
 * @brief PMOD JC - I2C Sensor Interface with Interrupts
 *
 * I2C connector with 2 interrupt-capable pins for sensor data ready signals.
 */
typedef enum {
  k_pmod_jc_int1 = (k_rx_port_c << k_port_shift) | k_rx_pin_4, /**< PC4 (pin 48) - Interrupt 1 */
  k_pmod_jc_int2 = (k_rx_port_c << k_port_shift) | k_rx_pin_5, /**< PC5 (pin 47) - Interrupt 2 */
  k_pmod_jc_scl  = (k_rx_port_c << k_port_shift) | k_rx_pin_2, /**< PC2 (pin 50) - I2C Clock */
  k_pmod_jc_sda  = (k_rx_port_c << k_port_shift) | k_rx_pin_3, /**< PC3 (pin 49) - I2C Data */
} pmod_jc_pins_t;

/**
 * @brief PMOD JD - GPIO Expansion
 *
 * 4 general-purpose GPIO pins (alternate functions: SCI12 UART, QSPI).
 */
typedef enum {
  k_pmod_jd_gpio0 = (k_rx_port_d << k_port_shift) | k_rx_pin_7, /**< PD7 (pin 79) - GPIO 0 */
  k_pmod_jd_gpio1 = (k_rx_port_d << k_port_shift) | k_rx_pin_6, /**< PD6 (pin 80) - GPIO 1 */
  k_pmod_jd_gpio2 = (k_rx_port_d << k_port_shift) | k_rx_pin_5, /**< PD5 (pin 81) - GPIO 2 */
  k_pmod_jd_gpio3 = (k_rx_port_d << k_port_shift) | k_rx_pin_4, /**< PD4 (pin 82) - GPIO 3 */
} pmod_jd_pins_t;

/**
 * @brief PMOD JE - GPIO Expansion with IRQ Capability
 *
 * 4 GPIO pins with IRQ support for interrupt-driven applications.
 */
typedef enum {
  k_pmod_je_gpio0 = (k_rx_port_j << k_port_shift) | k_rx_pin_3, /**< PJ3 (pin 4) - GPIO 0 */
  k_pmod_je_gpio1 = (k_rx_port_b << k_port_shift) | k_rx_pin_1, /**< PB1 (pin 59) - GPIO 1 */
  k_pmod_je_gpio2 = (k_rx_port_b << k_port_shift) | k_rx_pin_0, /**< PB0 (pin 61) - GPIO 2 */
  k_pmod_je_gpio3 = (k_rx_port_a << k_port_shift) | k_rx_pin_1, /**< PA1 (pin 69) - GPIO 3 */
} pmod_je_pins_t;

/**
 * @brief PMOD JF - I2C Expansion with GPIO
 *
 * I2C connector (RIIC1) with 2 additional GPIO pins.
 */
typedef enum {
  k_pmod_jf_gpio0 = (k_rx_port_d << k_port_shift) | k_rx_pin_0, /**< PD0 (pin 86) - GPIO 0 */
  k_pmod_jf_gpio1 = (k_rx_port_0 << k_port_shift) | k_rx_pin_7, /**< P07 (pin 98) - GPIO 1 */
  k_pmod_jf_scl   = (k_rx_port_2 << k_port_shift) | k_rx_pin_1, /**< P21 (pin 27) - I2C Clock */
  k_pmod_jf_sda   = (k_rx_port_2 << k_port_shift) | k_rx_pin_0, /**< P20 (pin 28) - I2C Data */
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
  k_pin_jtag_tms =
    (k_rx_port_3 << k_port_shift) | k_rx_pin_1, /**< P31/TMS (pin 19) - JTAG Test Mode Select */
  k_pin_jtag_tdi =
    (k_rx_port_3 << k_port_shift) | k_rx_pin_0, /**< P30/TDI (pin 20) - JTAG Test Data In */
  k_pin_jtag_tdo =
    (k_rx_port_2 << k_port_shift) | k_rx_pin_6, /**< P26/TDO (pin 22) - JTAG Test Data Out */
  k_pin_jtag_tck =
    (k_rx_port_2 << k_port_shift) | k_rx_pin_7, /**< P27/TCK (pin 21) - JTAG Test Clock */
  k_pin_jtag_trst =
    (k_rx_port_3 << k_port_shift) | k_rx_pin_4, /**< P34/TST# (pin 16) - JTAG Test Reset */
} jtag_pins_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_HARDWARE_PINOUT_H */
