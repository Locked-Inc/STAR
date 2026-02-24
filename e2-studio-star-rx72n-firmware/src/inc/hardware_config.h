/**
 * @file hardware_config.h
 * @brief Hardware Pin Configuration Constants for STAR RX72N Platform (144-pin LFQFP)
 *
 * @details
 * This file defines all hardware pin assignments for the STAR robot platform
 * using the 144-pin LFQFP package (R5F572NNHxFB). Pin assignments are verified
 * against pinout.txt (repository root, 57/144 pins used).
 *
 * **CRITICAL**: These pin assignments MUST match the PCB design. Any changes
 * to this file require corresponding updates to pinout.txt and vice versa.
 *
 * ## Functional Groups (14 total)
 *
 * | Group | Peripheral | Signals |
 * |-------|------------|---------|
 * | Motor PWM | GPTW 0-3 | 8 pins (IN1/IN2 per motor) |
 * | Motor Driver Ctrl | GPIO | 4 DRVOFF + 4 nSLEEP = 8 pins |
 * | GTETRG Emergency Stop | GPTW triggers | 4 pins (nFAULT per motor) |
 * | Host SPI | RSPI2_A | 3 data + 1 CS = 4 pins |
 * | Host I2C | RIIC0 | 2 pins (SCL0/SDA0) |
 * | IMU | RIIC1 + GPIO | 2 I2C + INT + RST = 4 pins |
 * | Debug UART | SCI9 | 2 pins (TXD9/RXD9) |
 * | MTU Encoders | MTU1/MTU2 | 4 pins |
 * | TPU Encoders | TPU1/TPU2 | 4 pins |
 * | Motor Current ADC | S12AD0 | 4 pins (AN004-AN007) |
 * | Sonar HC-SR04 | IRQ + GPIO | 4 ECHO + 4 TRIG = 8 pins |
 * | LEDs | GPIO | 6 pins |
 * | 1-Wire Temperature | GPIO | 1 pin |
 * | HOST_IRQ | IRQ15 | 1 pin |
 *
 * @see pinout.txt Complete verified pin assignment table
 * @see 144_PIN_MIGRATION_PLAN.md Migration planning details
 * @see RX72N_ROADMAP.md Peripheral implementation status
 *
 * @author STAR Development Team
 * @date 2026-02-09
 */

#pragma once

#include <stdint.h>

/* =========================================================================
 * Motor PWM (GPTW channels 0-3)
 * ========================================================================= */

/**
 * @defgroup motor_pwm_pins Motor PWM Pin Assignments
 * @brief GPTW output pins for DRV8263H IN1 (PWM) and IN2 (direction) control
 *
 * @details
 * Each motor uses one GPTW channel with two outputs for the DRV8263H H-bridge
 * operating in IN2/IN1 mode:
 * - **IN2 (GTIOC_A)**: Direction control (static HIGH/LOW to DRV8263H)
 * - **IN1 (GTIOC_B)**: PWM duty cycle (PWM waveform to DRV8263H)
 *
 * Pins are distributed across PORT2, PORT1, PORT8, PORTC, and PORTE.
 *
 * | Motor | IN2 Pin | IN2 Pkg Pin | IN1 Pin | IN1 Pkg Pin | GPTW Ch |
 * |-------|---------|-------------|---------|-------------|---------|
 * | 0 | P23 | 34 | P17 | 38 | GPTW0 |
 * | 1 | P22 | 35 | PC3 | 67 | GPTW1 |
 * | 2 | PE3 | 108 | P86 | 41 | GPTW2 |
 * | 3 | PE7 | 101 | PC6 | 61 | GPTW3 |
 * @{
 */

/**
 * @enum motor_in2_ports_t
 * @brief Port numbers for DRV8263H IN2 (direction) signals
 */
typedef enum : uint8_t {
  k_motor_0_in2_port = 2,  /**< Motor 0 IN2 on PORT2 (P23/GTIOC0A, pin 34) */
  k_motor_1_in2_port = 2,  /**< Motor 1 IN2 on PORT2 (P22/GTIOC1A, pin 35) */
  k_motor_2_in2_port = 14, /**< Motor 2 IN2 on PORTE (PE3/GTIOC2A, pin 108) */
  k_motor_3_in2_port = 14, /**< Motor 3 IN2 on PORTE (PE7/GTIOC3A, pin 101) */
} motor_in2_ports_t;

/**
 * @enum motor_in2_pins_t
 * @brief Pin numbers for DRV8263H IN2 (direction) signals
 */
typedef enum : uint8_t {
  k_motor_0_in2_pin = 3, /**< Motor 0 IN2 pin 3 (P23, pin 34) */
  k_motor_1_in2_pin = 2, /**< Motor 1 IN2 pin 2 (P22, pin 35) */
  k_motor_2_in2_pin = 3, /**< Motor 2 IN2 pin 3 (PE3, pin 108) */
  k_motor_3_in2_pin = 7, /**< Motor 3 IN2 pin 7 (PE7, pin 101) */
} motor_in2_pins_t;

/**
 * @enum motor_in1_ports_t
 * @brief Port numbers for DRV8263H IN1 (PWM) signals
 */
typedef enum : uint8_t {
  k_motor_0_in1_port = 1,  /**< Motor 0 IN1 on PORT1 (P17/GTIOC0B, pin 38) */
  k_motor_1_in1_port = 12, /**< Motor 1 IN1 on PORTC (PC3/GTIOC1B, pin 67) */
  k_motor_2_in1_port = 8,  /**< Motor 2 IN1 on PORT8 (P86/GTIOC2B, pin 41) */
  k_motor_3_in1_port = 12, /**< Motor 3 IN1 on PORTC (PC6/GTIOC3B, pin 61) */
} motor_in1_ports_t;

/**
 * @enum motor_in1_pins_t
 * @brief Pin numbers for DRV8263H IN1 (PWM) signals
 */
typedef enum : uint8_t {
  k_motor_0_in1_pin = 7, /**< Motor 0 IN1 pin 7 (P17, pin 38) */
  k_motor_1_in1_pin = 3, /**< Motor 1 IN1 pin 3 (PC3, pin 67) */
  k_motor_2_in1_pin = 6, /**< Motor 2 IN1 pin 6 (P86, pin 41) */
  k_motor_3_in1_pin = 6, /**< Motor 3 IN1 pin 6 (PC6, pin 61) */
} motor_in1_pins_t;

/** @} */ /* end of motor_pwm_pins */

/* =========================================================================
 * Motor Driver Control (DRV8263H DRVOFF + nSLEEP)
 * ========================================================================= */

/**
 * @defgroup motor_drv_ctrl_pins Motor Driver Control Pin Assignments
 * @brief GPIO output pins for DRV8263H DRVOFF and nSLEEP control
 *
 * @details
 * Each DRV8263H motor driver has two control pins:
 * - **DRVOFF**: Driver output disable (active-high, LOW = outputs enabled)
 * - **nSLEEP**: Sleep mode control (active-low, HIGH = awake)
 *
 * **DRVOFF pins (GPIO output, initial HIGH = outputs disabled for safe startup):**
 * | Motor | Pin | Pkg Pin |
 * |-------|-----|---------|
 * | 0 | P61 | 115 |
 * | 1 | P63 | 113 |
 * | 2 | PE0 | 111 |
 * | 3 | PE2 | 109 |
 *
 * **nSLEEP pins (GPIO output, initial HIGH = awake):**
 * | Motor | Pin | Pkg Pin |
 * |-------|-----|---------|
 * | 0 | P60 | 117 |
 * | 1 | P62 | 114 |
 * | 2 | P64 | 112 |
 * | 3 | PE1 | 110 |
 *
 * @see motor_pwm_pins Motor PWM pin assignments
 * @since STAR v1.0.0
 * @{
 */

/**
 * @enum motor_drvoff_ports_t
 * @brief Port numbers for DRV8263H DRVOFF (output disable) GPIO pins
 * @invariant Values must match PCB schematic and pinout.txt
 * @see motor_drvoff_pins_t Corresponding bit positions within each port
 */
typedef enum : uint8_t {
  k_motor_0_drvoff_port = 6,  /**< Motor 0 DRVOFF on PORT6 (P61, pin 115) */
  k_motor_1_drvoff_port = 6,  /**< Motor 1 DRVOFF on PORT6 (P63, pin 113) */
  k_motor_2_drvoff_port = 14, /**< Motor 2 DRVOFF on PORTE (PE0, pin 111) */
  k_motor_3_drvoff_port = 14, /**< Motor 3 DRVOFF on PORTE (PE2, pin 109) */
} motor_drvoff_ports_t;

/**
 * @enum motor_drvoff_pins_t
 * @brief Pin numbers for DRV8263H DRVOFF (output disable) GPIO pins
 * @invariant Values must match PCB schematic and pinout.txt
 * @see motor_drvoff_ports_t Corresponding port register indices
 */
typedef enum : uint8_t {
  k_motor_0_drvoff_pin = 1, /**< Motor 0 DRVOFF pin 1 (P61, pin 115) */
  k_motor_1_drvoff_pin = 3, /**< Motor 1 DRVOFF pin 3 (P63, pin 113) */
  k_motor_2_drvoff_pin = 0, /**< Motor 2 DRVOFF pin 0 (PE0, pin 111) */
  k_motor_3_drvoff_pin = 2, /**< Motor 3 DRVOFF pin 2 (PE2, pin 109) */
} motor_drvoff_pins_t;

/**
 * @enum motor_nsleep_ports_t
 * @brief Port numbers for DRV8263H nSLEEP (sleep mode) GPIO pins
 * @invariant Values must match PCB schematic and pinout.txt
 * @see motor_nsleep_pins_t Corresponding bit positions within each port
 */
typedef enum : uint8_t {
  k_motor_0_nsleep_port = 6,  /**< Motor 0 nSLEEP on PORT6 (P60, pin 117) */
  k_motor_1_nsleep_port = 6,  /**< Motor 1 nSLEEP on PORT6 (P62, pin 114) */
  k_motor_2_nsleep_port = 6,  /**< Motor 2 nSLEEP on PORT6 (P64, pin 112) */
  k_motor_3_nsleep_port = 14, /**< Motor 3 nSLEEP on PORTE (PE1, pin 110) */
} motor_nsleep_ports_t;

/**
 * @enum motor_nsleep_pins_t
 * @brief Pin numbers for DRV8263H nSLEEP (sleep mode) GPIO pins
 * @invariant Values must match PCB schematic and pinout.txt
 * @see motor_nsleep_ports_t Corresponding port register indices
 */
typedef enum : uint8_t {
  k_motor_0_nsleep_pin = 0, /**< Motor 0 nSLEEP pin 0 (P60, pin 117) */
  k_motor_1_nsleep_pin = 2, /**< Motor 1 nSLEEP pin 2 (P62, pin 114) */
  k_motor_2_nsleep_pin = 4, /**< Motor 2 nSLEEP pin 4 (P64, pin 112) */
  k_motor_3_nsleep_pin = 1, /**< Motor 3 nSLEEP pin 1 (PE1, pin 110) */
} motor_nsleep_pins_t;

/** @} */ /* end of motor_drv_ctrl_pins */

/* =========================================================================
 * GTETRG Emergency Stop (hardware fault triggers)
 * ========================================================================= */

/**
 * @defgroup gtetrg_pins GTETRG Emergency Stop Pin Assignments
 * @brief Hardware GPTW external trigger pins for motor fault (nFAULT) signals
 *
 * @details
 * GTETRG provides hardware-level emergency stop: when nFAULT goes low,
 * the GPTW channel automatically disables PWM output without CPU intervention.
 * This gives sub-microsecond fault response time.
 *
 * | Motor | Signal | Pin | Pkg Pin | Trigger |
 * |-------|--------|-----|---------|---------|
 * | 0 | nFAULT | P15 | 42 | GTETRGA |
 * | 1 | nFAULT | PA6 | 89 | GTETRGB |
 * | 2 | nFAULT | PC4 | 66 | GTETRGC |
 * | 3 | nFAULT | P14 | 43 | GTETRGD |
 * @{
 */

typedef enum : uint8_t {
  k_motor_0_nfault_port = 1,  /**< Motor 0 nFAULT on PORT1 (P15/GTETRGA, pin 42) */
  k_motor_1_nfault_port = 10, /**< Motor 1 nFAULT on PORTA (PA6/GTETRGB, pin 89) */
  k_motor_2_nfault_port = 12, /**< Motor 2 nFAULT on PORTC (PC4/GTETRGC, pin 66) */
  k_motor_3_nfault_port = 1,  /**< Motor 3 nFAULT on PORT1 (P14/GTETRGD, pin 43) */
} motor_nfault_ports_t;

typedef enum : uint8_t {
  k_motor_0_nfault_pin = 5, /**< Motor 0 nFAULT pin 5 (P15, pin 42) */
  k_motor_1_nfault_pin = 6, /**< Motor 1 nFAULT pin 6 (PA6, pin 89) */
  k_motor_2_nfault_pin = 4, /**< Motor 2 nFAULT pin 4 (PC4, pin 66) */
  k_motor_3_nfault_pin = 4, /**< Motor 3 nFAULT pin 4 (P14, pin 43) */
} motor_nfault_pins_t;

/** @} */ /* end of gtetrg_pins */

/* =========================================================================
 * Host SPI (RSPI2 channel A)
 * ========================================================================= */

/**
 * @defgroup host_spi_pins Host SPI Pin Assignments
 * @brief RSPI2_A for RPi5 <-> RX72N high-speed SPI communication
 *
 * @details
 * RSPI2 channel A pins on PORTD. All pins use MPC alternate function "A".
 *
 * **Pin Naming Convention:** Hardware signal names (MOSIC/MISOC) are from
 * the RX72N datasheet. This project uses COPI/CIPO terminology per OSHWA
 * inclusive naming standards.
 *
 * | Signal | Pin | Pkg Pin | Function |
 * |--------|-----|---------|----------|
 * | HOST_SCLK | PD3 | 123 | RSPCKC |
 * | HOST_COPI | PD1 | 125 | MOSIC |
 * | HOST_CIPO | PD2 | 124 | MISOC |
 * | HOST_CS0 | PD4 | 122 | SSLC0 |
 * @{
 */

typedef enum : uint8_t {
  k_host_spi_port = 13, /**< Host SPI on PORTD (PD1-PD4, pins 122-125) */
} host_spi_ports_t;

typedef enum : uint8_t {
  k_host_sclk_pin = 3, /**< HOST_SCLK pin 3 (PD3/RSPCKC, pin 123) */
  k_host_copi_pin = 1, /**< HOST_COPI pin 1 (PD1/MOSIC, pin 125) */
  k_host_cipo_pin = 2, /**< HOST_CIPO pin 2 (PD2/MISOC, pin 124) */
  k_host_cs0_pin  = 4, /**< HOST_CS0 pin 4 (PD4/SSLC0, pin 122) */
} host_spi_pins_t;

/** @} */ /* end of host_spi_pins */

/* =========================================================================
 * Debug UART (SCI9)
 * ========================================================================= */

/**
 * @defgroup debug_uart_pins Debug UART Pin Assignments
 * @brief SCI9 UART for debug console output
 *
 * @details
 * | Signal | Pin | Pkg Pin | Function |
 * |--------|-----|---------|----------|
 * | DEBUG_TXD9 | PB7 | 78 | TXD9 |
 * | DEBUG_RXD9 | PB6 | 79 | RXD9 |
 * @{
 */

typedef enum : uint8_t {
  k_debug_uart_port = 11, /**< Debug UART on PORTB (PB6/PB7) */
} debug_uart_ports_t;

typedef enum : uint8_t {
  k_debug_txd_pin = 7, /**< DEBUG_TXD9 pin 7 (PB7/TXD9, pin 78) */
  k_debug_rxd_pin = 6, /**< DEBUG_RXD9 pin 6 (PB6/RXD9, pin 79) */
} debug_uart_pins_t;

typedef enum : uint8_t {
  k_debug_uart_channel = 9, /**< Debug UART uses SCI9 */
} debug_uart_channel_t;

/** @} */ /* end of debug_uart_pins */

/* =========================================================================
 * Host I2C (RIIC0)
 * ========================================================================= */

/**
 * @defgroup host_i2c_pins Host I2C Pin Assignments
 * @brief RIIC0 for RPi5 <-> RX72N I2C communication
 *
 * @details
 * | Signal | Pin | Pkg Pin | Function |
 * |--------|-----|---------|----------|
 * | HOST_SCL0 | P12 | 45 | SCL0 |
 * | HOST_SDA0 | P13 | 44 | SDA0 |
 * @{
 */

typedef enum : uint8_t {
  k_host_i2c_port = 1, /**< Host I2C on PORT1 (P12/P13) */
} host_i2c_ports_t;

typedef enum : uint8_t {
  k_host_scl0_pin = 2, /**< HOST_SCL0 pin 2 (P12/SCL0, pin 45) */
  k_host_sda0_pin = 3, /**< HOST_SDA0 pin 3 (P13/SDA0, pin 44) */
} host_i2c_pins_t;

/** @} */ /* end of host_i2c_pins */

/* =========================================================================
 * IMU (RIIC1 + GPIO)
 * ========================================================================= */

/**
 * @defgroup imu_pins IMU Pin Assignments
 * @brief RIIC1 I2C and GPIO pins for inertial measurement unit
 *
 * @details
 * | Signal | Pin | Pkg Pin | Function |
 * |--------|-----|---------|----------|
 * | IMU_SCL | P21/SCL1 | 36 | RIIC1 clock |
 * | IMU_SDA | P20/SDA1 | 37 | RIIC1 data |
 * | IMU_INT | P32 | 27 | IRQ input (active-low) |
 * | IMU_RST | P83 | 58 | GPIO output (active-low reset) |
 *
 * @see host_i2c_pins Host I2C pin assignments (RIIC0)
 * @since STAR v1.0.0
 * @{
 */

/**
 * @enum imu_ports_t
 * @brief Port numbers for IMU (RIIC1 I2C + GPIO) pins
 * @invariant Values must match PCB schematic and pinout.txt
 * @see imu_pins_t Corresponding bit positions within each port
 */
typedef enum : uint8_t {
  k_imu_scl_port = 2, /**< IMU SCL on PORT2 (P21/SCL1, pin 36) */
  k_imu_sda_port = 2, /**< IMU SDA on PORT2 (P20/SDA1, pin 37) */
  k_imu_int_port = 3, /**< IMU INT on PORT3 (P32, pin 27) */
  k_imu_rst_port = 8, /**< IMU RST on PORT8 (P83, pin 58) */
} imu_ports_t;

/**
 * @enum imu_pins_t
 * @brief Pin numbers for IMU (RIIC1 I2C + GPIO) pins
 * @invariant Values must match PCB schematic and pinout.txt
 * @see imu_ports_t Corresponding port register indices
 */
typedef enum : uint8_t {
  k_imu_scl_pin = 1, /**< IMU SCL pin 1 (P21, pin 36) */
  k_imu_sda_pin = 0, /**< IMU SDA pin 0 (P20, pin 37) */
  k_imu_int_pin = 2, /**< IMU INT pin 2 (P32, pin 27) */
  k_imu_rst_pin = 3, /**< IMU RST pin 3 (P83, pin 58) */
} imu_pins_t;

/** @} */ /* end of imu_pins */

/* =========================================================================
 * MTU Encoders (MTU1 and MTU2 phase counting)
 * ========================================================================= */

/**
 * @defgroup mtu_encoder_pins MTU Encoder Pin Assignments
 * @brief MTU1/MTU2 phase counting clock inputs for front encoders
 *
 * @details
 * MTU1 and MTU2 support 32-bit phase counting mode for quadrature encoders.
 *
 * | Encoder | Unit | Phase A | Pkg Pin | Phase B | Pkg Pin |
 * |---------|------|---------|---------|---------|---------|
 * | 0 (FL) | MTU1 | P24/MTCLKA | 33 | P25/MTCLKB | 32 |
 * | 1 (FR) | MTU2 | PA1/MTCLKC | 96 | PC5/MTCLKD | 62 |
 * @{
 */

typedef enum : uint8_t {
  k_encoder_0_phase_a_port = 2, /**< Encoder 0 Phase A on PORT2 (P24/MTCLKA, pin 33) */
  k_encoder_0_phase_b_port = 2, /**< Encoder 0 Phase B on PORT2 (P25/MTCLKB, pin 32) */
} encoder_0_ports_t;

typedef enum : uint8_t {
  k_encoder_0_phase_a_pin = 4, /**< Encoder 0 Phase A pin 4 (P24, pin 33) */
  k_encoder_0_phase_b_pin = 5, /**< Encoder 0 Phase B pin 5 (P25, pin 32) */
} encoder_0_pins_t;

typedef enum : uint8_t {
  k_encoder_1_phase_a_port = 10, /**< Encoder 1 Phase A on PORTA (PA1/MTCLKC, pin 96) */
  k_encoder_1_phase_b_port = 12, /**< Encoder 1 Phase B on PORTC (PC5/MTCLKD, pin 62) */
} encoder_1_ports_t;

typedef enum : uint8_t {
  k_encoder_1_phase_a_pin = 1, /**< Encoder 1 Phase A pin 1 (PA1, pin 96) */
  k_encoder_1_phase_b_pin = 5, /**< Encoder 1 Phase B pin 5 (PC5, pin 62) */
} encoder_1_pins_t;

/** @} */ /* end of mtu_encoder_pins */

/* =========================================================================
 * TPU Encoders (TPU1 and TPU2 phase counting)
 * ========================================================================= */

/**
 * @defgroup tpu_encoder_pins TPU Encoder Pin Assignments
 * @brief TPU1/TPU2 phase counting clock inputs for rear encoders
 *
 * @details
 * TPU1 and TPU2 support 16-bit phase counting mode for quadrature encoders.
 *
 * | Encoder | Unit | Phase A | Pkg Pin | Phase B | Pkg Pin |
 * |---------|------|---------|---------|---------|---------|
 * | 2 (RL) | TPU1 | PC2/TCLKA | 70 | PA3/TCLKB | 94 |
 * | 3 (RR) | TPU2 | PC0/TCLKC | 75 | PB3/TCLKD | 82 |
 * @{
 */

typedef enum : uint8_t {
  k_encoder_2_phase_a_port = 12, /**< Encoder 2 Phase A on PORTC (PC2/TCLKA, pin 70) */
  k_encoder_2_phase_b_port = 10, /**< Encoder 2 Phase B on PORTA (PA3/TCLKB, pin 94) */
} encoder_2_ports_t;

typedef enum : uint8_t {
  k_encoder_2_phase_a_pin = 2, /**< Encoder 2 Phase A pin 2 (PC2, pin 70) */
  k_encoder_2_phase_b_pin = 3, /**< Encoder 2 Phase B pin 3 (PA3, pin 94) */
} encoder_2_pins_t;

typedef enum : uint8_t {
  k_encoder_3_phase_a_port = 12, /**< Encoder 3 Phase A on PORTC (PC0/TCLKC, pin 75) */
  k_encoder_3_phase_b_port = 11, /**< Encoder 3 Phase B on PORTB (PB3/TCLKD, pin 82) */
} encoder_3_ports_t;

typedef enum : uint8_t {
  k_encoder_3_phase_a_pin = 0, /**< Encoder 3 Phase A pin 0 (PC0, pin 75) */
  k_encoder_3_phase_b_pin = 3, /**< Encoder 3 Phase B pin 3 (PB3, pin 82) */
} encoder_3_pins_t;

/** @} */ /* end of tpu_encoder_pins */

/* =========================================================================
 * Motor Current Sense ADC (S12AD0, AN004-AN007)
 * ========================================================================= */

/**
 * @defgroup motor_adc_pins Motor Current Sense ADC Pin Assignments
 * @brief S12AD0 channels for motor current sense (AN004-AN007)
 *
 * @details
 * All motor current sense uses ADC Unit 0 (S12AD0) on P44-P47.
 *
 * | Motor | Pin | Pkg Pin | ADC Channel |
 * |-------|-----|---------|-------------|
 * | 0 | P47 | 133 | AN007 |
 * | 1 | P46 | 134 | AN006 |
 * | 2 | P45 | 135 | AN005 |
 * | 3 | P44 | 136 | AN004 |
 * @{
 */

typedef enum : uint8_t {
  k_motor_0_current_port = 4, /**< Motor 0 current on PORT4 (P47/AN007, pin 133) */
  k_motor_1_current_port = 4, /**< Motor 1 current on PORT4 (P46/AN006, pin 134) */
  k_motor_2_current_port = 4, /**< Motor 2 current on PORT4 (P45/AN005, pin 135) */
  k_motor_3_current_port = 4, /**< Motor 3 current on PORT4 (P44/AN004, pin 136) */
} motor_current_ports_t;

typedef enum : uint8_t {
  k_motor_0_current_pin = 7, /**< Motor 0 current pin 7 (P47/AN007, pin 133) */
  k_motor_1_current_pin = 6, /**< Motor 1 current pin 6 (P46/AN006, pin 134) */
  k_motor_2_current_pin = 5, /**< Motor 2 current pin 5 (P45/AN005, pin 135) */
  k_motor_3_current_pin = 4, /**< Motor 3 current pin 4 (P44/AN004, pin 136) */
} motor_current_pins_t;

typedef enum : uint8_t {
  k_motor_0_current_adc_ch = 7, /**< Motor 0 current ADC channel AN007 */
  k_motor_1_current_adc_ch = 6, /**< Motor 1 current ADC channel AN006 */
  k_motor_2_current_adc_ch = 5, /**< Motor 2 current ADC channel AN005 */
  k_motor_3_current_adc_ch = 4, /**< Motor 3 current ADC channel AN004 */
} motor_current_adc_channels_t;

/** @} */ /* end of motor_adc_pins */

/* =========================================================================
 * HC-SR04 Ultrasonic Sensors
 * ========================================================================= */

/**
 * @defgroup hc_sr04_pins HC-SR04 Ultrasonic Sensor Pin Assignments
 * @brief IRQ-based ECHO inputs and GPIO TRIG outputs for 4 sonar sensors
 *
 * @details
 * ECHO pins use IRQ for microsecond-accurate timing. TRIG pins are GPIO outputs.
 *
 * **ECHO (IRQ inputs):**
 * | Sonar | Pin | Pkg Pin | IRQ |
 * |-------|-----|---------|-----|
 * | 0 | P03 | 4 | IRQ11 |
 * | 1 | P02 | 6 | IRQ10 |
 * | 2 | P01 | 7 | IRQ9 |
 * | 3 | P00 | 8 | IRQ8 |
 *
 * **TRIG (GPIO outputs):**
 * | Sonar | Pin | Pkg Pin |
 * |-------|-----|---------|
 * | 0 | PF5 | 9 |
 * | 1 | PJ5 | 11 |
 * | 2 | PJ3 | 13 |
 * | 3 | P33 | 26 |
 * @{
 */

typedef enum : uint8_t {
  k_sonar_0_echo_port = 0, /**< Sonar 0 ECHO on PORT0 (P03/IRQ11, pin 4) */
  k_sonar_1_echo_port = 0, /**< Sonar 1 ECHO on PORT0 (P02/IRQ10, pin 6) */
  k_sonar_2_echo_port = 0, /**< Sonar 2 ECHO on PORT0 (P01/IRQ9, pin 7) */
  k_sonar_3_echo_port = 0, /**< Sonar 3 ECHO on PORT0 (P00/IRQ8, pin 8) */
} sonar_echo_ports_t;

typedef enum : uint8_t {
  k_sonar_0_echo_pin = 3, /**< Sonar 0 ECHO pin 3 (P03, pin 4) */
  k_sonar_1_echo_pin = 2, /**< Sonar 1 ECHO pin 2 (P02, pin 6) */
  k_sonar_2_echo_pin = 1, /**< Sonar 2 ECHO pin 1 (P01, pin 7) */
  k_sonar_3_echo_pin = 0, /**< Sonar 3 ECHO pin 0 (P00, pin 8) */
} sonar_echo_pins_t;

typedef enum : uint8_t {
  k_sonar_0_echo_irq = 11, /**< Sonar 0 ECHO IRQ11 */
  k_sonar_1_echo_irq = 10, /**< Sonar 1 ECHO IRQ10 */
  k_sonar_2_echo_irq = 9,  /**< Sonar 2 ECHO IRQ9 */
  k_sonar_3_echo_irq = 8,  /**< Sonar 3 ECHO IRQ8 */
} sonar_echo_irqs_t;

typedef enum : uint8_t {
  k_sonar_0_trig_port = 15, /**< Sonar 0 TRIG on PORTF (PF5, pin 9) */
  k_sonar_1_trig_port = 18, /**< Sonar 1 TRIG on PORTJ (PJ5, pin 11) */
  k_sonar_2_trig_port = 18, /**< Sonar 2 TRIG on PORTJ (PJ3, pin 13) */
  k_sonar_3_trig_port = 3,  /**< Sonar 3 TRIG on PORT3 (P33, pin 26) */
} sonar_trig_ports_t;

typedef enum : uint8_t {
  k_sonar_0_trig_pin = 5, /**< Sonar 0 TRIG pin 5 (PF5, pin 9) */
  k_sonar_1_trig_pin = 5, /**< Sonar 1 TRIG pin 5 (PJ5, pin 11) */
  k_sonar_2_trig_pin = 3, /**< Sonar 2 TRIG pin 3 (PJ3, pin 13) */
  k_sonar_3_trig_pin = 3, /**< Sonar 3 TRIG pin 3 (P33, pin 26) */
} sonar_trig_pins_t;

/** @} */ /* end of hc_sr04_pins */

/* =========================================================================
 * LEDs (GPIO outputs)
 * ========================================================================= */

/**
 * @defgroup led_pins LED Pin Assignments
 * @brief GPIO output pins for 6 status LEDs
 *
 * @details
 * | LED | Pin | Pkg Pin |
 * |-----|-----|---------|
 * | 0 | PA7 | 88 |
 * | 1 | PB0 | 87 |
 * | 2 | P71 | 86 |
 * | 3 | P72 | 85 |
 * | 4 | PB1 | 84 |
 * | 5 | PB2 | 83 |
 * @{
 */

typedef enum : uint8_t {
  k_led_0_port = 10, /**< LED0 on PORTA (PA7, pin 88) */
  k_led_1_port = 11, /**< LED1 on PORTB (PB0, pin 87) */
  k_led_2_port = 7,  /**< LED2 on PORT7 (P71, pin 86) */
  k_led_3_port = 7,  /**< LED3 on PORT7 (P72, pin 85) */
  k_led_4_port = 11, /**< LED4 on PORTB (PB1, pin 84) */
  k_led_5_port = 11, /**< LED5 on PORTB (PB2, pin 83) */
} led_ports_t;

typedef enum : uint8_t {
  k_led_0_pin = 7, /**< LED0 pin 7 (PA7, pin 88) */
  k_led_1_pin = 0, /**< LED1 pin 0 (PB0, pin 87) */
  k_led_2_pin = 1, /**< LED2 pin 1 (P71, pin 86) */
  k_led_3_pin = 2, /**< LED3 pin 2 (P72, pin 85) */
  k_led_4_pin = 1, /**< LED4 pin 1 (PB1, pin 84) */
  k_led_5_pin = 2, /**< LED5 pin 2 (PB2, pin 83) */
} led_pins_t;

typedef enum : uint8_t {
  k_led_count = 6, /**< Total number of LEDs */
} led_count_t;

/** @} */ /* end of led_pins */

/* =========================================================================
 * 1-Wire Temperature Sensor
 * ========================================================================= */

/**
 * @defgroup onewire_pins 1-Wire Temperature Sensor Pin Assignment
 * @brief GPIO pin for Dallas 1-Wire temperature sensor
 *
 * @details
 * | Signal | Pin | Pkg Pin |
 * |--------|-----|---------|
 * | TEMP_1WIRE | P51 | 55 |
 * @{
 */

typedef enum : uint8_t {
  k_temp_1wire_port = 5, /**< 1-Wire on PORT5 (P51, pin 55) */
} onewire_ports_t;

typedef enum : uint8_t {
  k_temp_1wire_pin = 1, /**< 1-Wire pin 1 (P51, pin 55) */
} onewire_pins_t;

/** @} */ /* end of onewire_pins */

/* =========================================================================
 * HOST_IRQ (IRQ15)
 * ========================================================================= */

/**
 * @defgroup host_irq_pins Host Interrupt Pin Assignment
 * @brief GPIO output to RPi5 indicating data ready (active-low)
 *
 * @details
 * HOST_IRQ is a GPIO **output** from the RX72N to the RPi5. The RX72N
 * asserts this pin LOW to signal that SPI data is ready for transfer.
 * Despite the IRQ15 naming, this pin is configured as a GPIO output,
 * NOT as an ICU interrupt input.
 *
 * | Signal | Pin | Pkg Pin | Direction |
 * |--------|-----|---------|-----------|
 * | HOST_IRQ | P67 | 98 | RX72N -> RPi5 (output, active-low) |
 *
 * @see rx_host_irq.h GPIO output driver
 * @{
 */

typedef enum : uint8_t {
  k_host_irq_port = 6, /**< HOST_IRQ on PORT6 (P67/IRQ15, pin 98) */
} host_irq_ports_t;

typedef enum : uint8_t {
  k_host_irq_pin = 7, /**< HOST_IRQ pin 7 (P67, pin 98) */
} host_irq_pins_t;

typedef enum : uint8_t {
  k_host_irq_num = 15, /**< HOST_IRQ uses IRQ15 */
} host_irq_nums_t;

/** @} */ /* end of host_irq_pins */

/* =========================================================================
 * USB (CDC debug interface)
 * ========================================================================= */

/**
 * @defgroup usb_pins USB Pin Assignments
 * @brief USB0 pins for CDC debug interface
 *
 * @details
 * | Signal | Pin | Pkg Pin |
 * |--------|-----|---------|
 * | HOST_USB0_VBUS | P16 | 40 |
 * | HOST_USB0_DM | USB0_DM | 47 |
 * | HOST_USB0_DP | USB0_DP | 48 |
 *
 * P16 is the VBUS detection input. USB0_DM and USB0_DP are dedicated USB pins
 * (not GPIO-capable).
 * @{
 */

typedef enum : uint8_t {
  k_usb_vbus_port = 1, /**< USB0_VBUS on PORT1 (P16, pin 40) */
} usb_ports_t;

typedef enum : uint8_t {
  k_usb_vbus_pin = 6, /**< USB0_VBUS pin 6 (P16, pin 40) */
} usb_pins_t;

typedef enum : uint8_t {
  k_usb_dm_pkg_pin   = 47, /**< USB0_DM dedicated pin (pin 47) */
  k_usb_dp_pkg_pin   = 48, /**< USB0_DP dedicated pin (pin 48) */
  k_usb_vbus_pkg_pin = 40, /**< USB0_VBUS on P16 (pin 40) */
} usb_pkg_pins_t;

/** @} */ /* end of usb_pins */
