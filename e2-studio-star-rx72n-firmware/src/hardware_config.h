/**
 * @file hardware_config.h
 * @brief Hardware Pin Configuration Constants for STAR RX72N Platform
 *
 * @details
 * This file defines all hardware pin assignments for the STAR robot platform.
 * Pin assignments are verified against the hardware pinout documentation
 * (docs/sections/03_hardware_pinout.tex) and RX72N datasheet.
 *
 * **CRITICAL**: These pin assignments MUST match the PCB design. Any changes
 * to this file require corresponding updates to the hardware pinout documentation
 * and vice versa.
 *
 * ## Pin Assignment Philosophy
 *
 * Pin assignments follow these design principles:
 * 1. **IRQ pins** reserved for time-critical signals (motor faults, ultrasonic sensors)
 * 2. **ADC pins** grouped by function for clean PCB routing
 * 3. **Adjacent pins** used for related signals (e.g., all motor current sense on PD4-PD7)
 * 4. **Alternate functions** chosen to minimize pin conflicts
 *
 * ## Recent Changes (2026-02-06)
 *
 * **Pin Reassignments to resolve HC-SR04/Motor conflicts:**
 * - Motor Current ADC: Moved from P40-P43 to PD4-PD7 (frees IRQ8-11)
 * - Motor nFAULT: Moved from P44-P47/IRQ12-15 to P40-P43/IRQ8-11
 * - HC-SR04 ECHO: Assigned to P44-P47/IRQ12-15 (freed by motor nFAULT move)
 * - HC-SR04 TRIG: Assigned to P52-P55 (unused GPIO)
 *
 * @see docs/sections/03_hardware_pinout.tex Complete pin assignment table
 * @see RX72N_ROADMAP.md Peripheral implementation status
 *
 * @author STAR Development Team
 * @date 2026-02-06
 */

#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <stdint.h>

/**
 * @defgroup motor_pins Motor Control Pin Assignments
 * @{
 */

/**
 * @brief Motor nFAULT pin assignments (DRV8243S fault signals)
 *
 * @details
 * **Pin Reassignment (2026-02-06):**
 * - BEFORE: P44-P47/IRQ12-15 (pins 87-90)
 * - AFTER: P40-P43/IRQ8-11 (pins 91-95)
 *
 * **Rationale:** Freed IRQ12-15 for HC-SR04 ECHO signals while maintaining
 * IRQ-based fault detection (<10µs response time).
 */
typedef enum : uint8_t {
    k_motor_0_nfault_port = 4,  /**< Motor 0 nFAULT on PORT4 (P40/IRQ8, pin 95) */
    k_motor_1_nfault_port = 4,  /**< Motor 1 nFAULT on PORT4 (P41/IRQ9, pin 93) */
    k_motor_2_nfault_port = 4,  /**< Motor 2 nFAULT on PORT4 (P42/IRQ10, pin 92) */
    k_motor_3_nfault_port = 4,  /**< Motor 3 nFAULT on PORT4 (P43/IRQ11, pin 91) */
} motor_nfault_ports_t;

typedef enum : uint8_t {
    k_motor_0_nfault_pin = 0,   /**< Motor 0 nFAULT pin 0 (P40/IRQ8, pin 95) */
    k_motor_1_nfault_pin = 1,   /**< Motor 1 nFAULT pin 1 (P41/IRQ9, pin 93) */
    k_motor_2_nfault_pin = 2,   /**< Motor 2 nFAULT pin 2 (P42/IRQ10, pin 92) */
    k_motor_3_nfault_pin = 3,   /**< Motor 3 nFAULT pin 3 (P43/IRQ11, pin 91) */
} motor_nfault_pins_t;

typedef enum : uint8_t {
    k_motor_0_nfault_irq = 8,   /**< Motor 0 nFAULT IRQ8 */
    k_motor_1_nfault_irq = 9,   /**< Motor 1 nFAULT IRQ9 */
    k_motor_2_nfault_irq = 10,  /**< Motor 2 nFAULT IRQ10 */
    k_motor_3_nfault_irq = 11,  /**< Motor 3 nFAULT IRQ11 */
} motor_nfault_irqs_t;

/**
 * @brief Motor current sense ADC pin assignments (IPROPI from DRV8243S)
 *
 * @details
 * **Pin Reassignment (2026-02-06):**
 * - BEFORE: P40-P43/AN000-003 (pins 91-95)
 * - AFTER: PD7,PD6,PD0,PD4/AN107,106,108,112 (pins 79,80,86,82)
 *
 * **Motor 2 Additional Change:** PD5/AN113 → PD0/AN108 to free PD5 for Encoder 1 (MTU2)
 *
 * **Rationale:** Freed P40-P43 IRQ8-11 capability for motor nFAULT while
 * maintaining same 12-bit ADC Unit 0 performance. All motor current sense
 * pins remain on PORTD for clean PCB routing.
 */
typedef enum : uint8_t {
    k_motor_0_current_port = 13,  /**< Motor 0 current on PORTD (PD7/AN107, pin 79) - Note: PORTD = 13 in RX72N */
    k_motor_1_current_port = 13,  /**< Motor 1 current on PORTD (PD6/AN106, pin 80) */
    k_motor_2_current_port = 13,  /**< Motor 2 current on PORTD (PD0/AN108, pin 86) */
    k_motor_3_current_port = 13,  /**< Motor 3 current on PORTD (PD4/AN112, pin 82) */
} motor_current_ports_t;

typedef enum : uint8_t {
    k_motor_0_current_pin = 7,   /**< Motor 0 current pin 7 (PD7/AN107, pin 79) */
    k_motor_1_current_pin = 6,   /**< Motor 1 current pin 6 (PD6/AN106, pin 80) */
    k_motor_2_current_pin = 0,   /**< Motor 2 current pin 0 (PD0/AN108, pin 86) */
    k_motor_3_current_pin = 4,   /**< Motor 3 current pin 4 (PD4/AN112, pin 82) */
} motor_current_pins_t;

typedef enum : uint8_t {
    k_motor_0_current_adc_ch = 107,  /**< Motor 0 current ADC channel AN107 */
    k_motor_1_current_adc_ch = 106,  /**< Motor 1 current ADC channel AN106 */
    k_motor_2_current_adc_ch = 108,  /**< Motor 2 current ADC channel AN108 */
    k_motor_3_current_adc_ch = 112,  /**< Motor 3 current ADC channel AN112 */
} motor_current_adc_channels_t;

/** @} */ // end of motor_pins

/**
 * @defgroup hc_sr04_pins HC-SR04 Ultrasonic Sensor Pin Assignments
 * @{
 */

/**
 * @brief HC-SR04 TRIG pin assignments (GPIO output for 10µs trigger pulse)
 *
 * @details
 * **New Assignment (2026-02-06):**
 * - Sonar 0 TRIG: P55 (pin 39)
 * - Sonar 1 TRIG: P54 (pin 40)
 * - Sonar 2 TRIG: P53 (pin 41)
 * - Sonar 3 TRIG: P52 (pin 42)
 *
 * **Rationale:** Used previously unused GPIO pins (formerly labeled PMOD).
 */
typedef enum : uint8_t {
    k_sonar_0_trig_port = 5,  /**< Sonar 0 TRIG on PORT5 (P55, pin 39) */
    k_sonar_1_trig_port = 5,  /**< Sonar 1 TRIG on PORT5 (P54, pin 40) */
    k_sonar_2_trig_port = 5,  /**< Sonar 2 TRIG on PORT5 (P53, pin 41) */
    k_sonar_3_trig_port = 5,  /**< Sonar 3 TRIG on PORT5 (P52, pin 42) */
} sonar_trig_ports_t;

typedef enum : uint8_t {
    k_sonar_0_trig_pin = 5,   /**< Sonar 0 TRIG pin 5 (P55, pin 39) */
    k_sonar_1_trig_pin = 4,   /**< Sonar 1 TRIG pin 4 (P54, pin 40) */
    k_sonar_2_trig_pin = 3,   /**< Sonar 2 TRIG pin 3 (P53, pin 41) */
    k_sonar_3_trig_pin = 2,   /**< Sonar 3 TRIG pin 2 (P52, pin 42) */
} sonar_trig_pins_t;

/**
 * @brief HC-SR04 ECHO pin assignments (IRQ input for microsecond-accurate timing)
 *
 * @details
 * **New Assignment (2026-02-06):**
 * - Sonar 0 ECHO: P44/IRQ12 (pin 90)
 * - Sonar 1 ECHO: P45/IRQ13 (pin 89)
 * - Sonar 2 ECHO: P46/IRQ14 (pin 88)
 * - Sonar 3 ECHO: P47/IRQ15 (pin 87)
 *
 * **Rationale:** Freed by moving motor nFAULT to IRQ8-11. IRQ-based timing
 * required for 1µs accuracy (1µs error = 0.17mm distance error).
 */
typedef enum : uint8_t {
    k_sonar_0_echo_port = 4,  /**< Sonar 0 ECHO on PORT4 (P44/IRQ12, pin 90) */
    k_sonar_1_echo_port = 4,  /**< Sonar 1 ECHO on PORT4 (P45/IRQ13, pin 89) */
    k_sonar_2_echo_port = 4,  /**< Sonar 2 ECHO on PORT4 (P46/IRQ14, pin 88) */
    k_sonar_3_echo_port = 4,  /**< Sonar 3 ECHO on PORT4 (P47/IRQ15, pin 87) */
} sonar_echo_ports_t;

typedef enum : uint8_t {
    k_sonar_0_echo_pin = 4,   /**< Sonar 0 ECHO pin 4 (P44/IRQ12, pin 90) */
    k_sonar_1_echo_pin = 5,   /**< Sonar 1 ECHO pin 5 (P45/IRQ13, pin 89) */
    k_sonar_2_echo_pin = 6,   /**< Sonar 2 ECHO pin 6 (P46/IRQ14, pin 88) */
    k_sonar_3_echo_pin = 7,   /**< Sonar 3 ECHO pin 7 (P47/IRQ15, pin 87) */
} sonar_echo_pins_t;

typedef enum : uint8_t {
    k_sonar_0_echo_irq = 12,  /**< Sonar 0 ECHO IRQ12 */
    k_sonar_1_echo_irq = 13,  /**< Sonar 1 ECHO IRQ13 */
    k_sonar_2_echo_irq = 14,  /**< Sonar 2 ECHO IRQ14 */
    k_sonar_3_echo_irq = 15,  /**< Sonar 3 ECHO IRQ15 */
} sonar_echo_irqs_t;

/** @} */ // end of hc_sr04_pins

/**
 * @defgroup encoder_pins Encoder Pin Assignments
 * @{
 */

/**
 * @brief Encoder pin assignments (verified correct for 100-pin LFQFP)
 *
 * @details
 * **Pin Verification (2026-02-06):**
 * Verified against RX72N Manual Ch01 Table 1.10 (100-Pin LFQFP pinout) and
 * Ch28 Table 28.2 (TPU phase counting clock requirements).
 *
 * **Front Encoders (MTU - 32-bit counters):**
 * - Encoder 0 (MTU1): P24/MTCLKA (pin 24), P15/MTCLKB (pin 31)
 * - Encoder 1 (MTU2): **PD5/MTCLKA (pin 81)**, P22/MTCLKC (pin 26)
 *   - **CHANGED from P14 to PD5** to free P14 for TPU1
 *
 * **Rear Encoders (TPU - 16-bit counters):**
 * - Encoder 2 (TPU1): **P14/TCLKA (pin 32)**, **PA3/TCLKB (pin 67)**
 *   - Phase counting mode uses TCLKA + TCLKB inputs
 * - Encoder 3 (TPU2): **PC0/TCLKC (pin 52)**, **PC1/TCLKD (pin 51)**
 *   - Phase counting mode uses TCLKC + TCLKD inputs
 *
 * **Critical Fix:** Previous documentation had WRONG TCLK functions for 100-pin package:
 * - Was: PC1/TCLKA, PC0/TCLKB → **Actual: PC1/TCLKD, PC0/TCLKC**
 * - Was: PA3/TCLKC, PA2/TCLKD → **Actual: PA3/TCLKB, PA2 has NO TCLK**
 *
 * **Note:** MTU3/MTU4 do NOT support phase counting mode per RX72N Manual Ch24.
 * Rear encoders use TPU1 and TPU2 instead.
 *
 * @see TPU_ENCODER_IMPLEMENTATION_PLAN.md TPU encoder driver implementation
 * @see /home/star/.claude/plans/delegated-mapping-clock.md Pin verification details
 */

/* Encoder 0 (MTU1) - Front Left */
typedef enum : uint8_t {
    k_encoder_0_phase_a_port = 2,   /**< Encoder 0 Phase A on PORT2 (P24/MTCLKA, pin 24) */
    k_encoder_0_phase_b_port = 1,   /**< Encoder 0 Phase B on PORT1 (P15/MTCLKB, pin 31) */
} encoder_0_ports_t;

typedef enum : uint8_t {
    k_encoder_0_phase_a_pin = 4,    /**< Encoder 0 Phase A pin 4 (P24, pin 24) */
    k_encoder_0_phase_b_pin = 5,    /**< Encoder 0 Phase B pin 5 (P15, pin 31) */
} encoder_0_pins_t;

/* Encoder 1 (MTU2) - Front Right */
typedef enum : uint8_t {
    k_encoder_1_phase_a_port = 13,  /**< Encoder 1 Phase A on PORTD (PD5/MTCLKA, pin 81) - CHANGED from P14! */
    k_encoder_1_phase_b_port = 2,   /**< Encoder 1 Phase B on PORT2 (P22/MTCLKC, pin 26) */
} encoder_1_ports_t;

typedef enum : uint8_t {
    k_encoder_1_phase_a_pin = 5,    /**< Encoder 1 Phase A pin 5 (PD5, pin 81) - CHANGED from P14! */
    k_encoder_1_phase_b_pin = 2,    /**< Encoder 1 Phase B pin 2 (P22, pin 26) */
} encoder_1_pins_t;

/* Encoder 2 (TPU1) - Rear Left */
typedef enum : uint8_t {
    k_encoder_2_phase_a_port = 1,   /**< Encoder 2 Phase A on PORT1 (P14/TCLKA, pin 32) */
    k_encoder_2_phase_b_port = 10,  /**< Encoder 2 Phase B on PORTA (PA3/TCLKB, pin 67) */
} encoder_2_ports_t;

typedef enum : uint8_t {
    k_encoder_2_phase_a_pin = 4,    /**< Encoder 2 Phase A pin 4 (P14, pin 32) */
    k_encoder_2_phase_b_pin = 3,    /**< Encoder 2 Phase B pin 3 (PA3, pin 67) */
} encoder_2_pins_t;

/* Encoder 3 (TPU2) - Rear Right */
typedef enum : uint8_t {
    k_encoder_3_phase_a_port = 12,  /**< Encoder 3 Phase A on PORTC (PC0/TCLKC, pin 52) */
    k_encoder_3_phase_b_port = 12,  /**< Encoder 3 Phase B on PORTC (PC1/TCLKD, pin 51) */
} encoder_3_ports_t;

typedef enum : uint8_t {
    k_encoder_3_phase_a_pin = 0,    /**< Encoder 3 Phase A pin 0 (PC0, pin 52) */
    k_encoder_3_phase_b_pin = 1,    /**< Encoder 3 Phase B pin 1 (PC1, pin 51) */
} encoder_3_pins_t;

/** @} */ // end of encoder_pins

#endif /* HARDWARE_CONFIG_H */
