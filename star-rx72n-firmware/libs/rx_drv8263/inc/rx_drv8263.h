/**
 * @file rx_drv8263.h
 * @brief DRV8263H-Q1 Motor Driver Chip-Level Control API
 *
 * @details
 * ## Overview
 *
 * Provides chip-level control for the DRV8263H-Q1 H-bridge motor driver including:
 * - DRVOFF output disable control
 * - Latched fault reset via nSLEEP pulse
 * - Open Load Protection (OLP) diagnostics
 * - Current sensing ADC-to-ampere conversion
 *
 * This module complements rx_motor (which handles PWM/direction) by providing
 * DRV8263H-Q1-specific features not covered by the generic motor control API.
 *
 * ## Hardware Interface
 *
 * | Signal | Direction | Description |
 * |--------|-----------|-------------|
 * | DRVOFF | MCU -> DRV | Output disable (active-high, LOW = enabled) |
 * | nSLEEP | MCU -> DRV | Sleep control (active-low, HIGH = awake) |
 * | nFAULT | DRV -> MCU | Fault indicator (active-low, LOW = fault) |
 * | IN1 | MCU -> DRV | Half-bridge B control (PWM or GPIO for OLP) |
 * | IN2 | MCU -> DRV | Half-bridge A control (PWM or GPIO for OLP) |
 *
 * ## Current Sensing
 *
 * DRV8263H-Q1 IPROPI output characteristics:
 * - Current mirror ratio: 202 uA/A
 * - External sense resistor: 5100 ohm (5.1k)
 * - V_IPROPI = I_motor * 202e-6 * 5100 = I_motor * 1.0302
 * - I_motor = V_IPROPI / 1.0302
 *
 * @par Module Dependencies:
 * - rx_err.h - Error code definitions
 * - rx_log.h - Structured logging
 * - rx_port_utils.h - GPIO register access
 *
 * @par NASA Power of 10 Compliance:
 * | Rule | Status | Details |
 * |------|--------|---------|
 * | Rule 1 | [PASS] | No goto, setjmp, or recursion |
 * | Rule 2 | [PASS] | All loops bounded by enum constants |
 * | Rule 3 | [PASS] | Zero dynamic allocation |
 * | Rule 4 | [PASS] | All functions under 60 lines |
 * | Rule 5 | [PASS] | Minimum 2 validations per function |
 * | Rule 6 | [PASS] | Variables at smallest scope |
 * | Rule 7 | [PASS] | All return values checked |
 * | Rule 8 | [PASS] | C23 typed enums, minimal macros |
 * | Rule 9 | [WARN] | No function pointers in this module |
 * | Rule 10 | [PASS] | -Wall -Wextra -Werror clean |
 *
 * @see rx_motor.h Motor PWM control (complementary to this module)
 * @see hardware_config.h Pin assignments for DRVOFF, nSLEEP, nFAULT
 *
 * @author Locked, Inc.
 * @date 2026-03-03
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @version 1.0.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * ============================================================================= */

/**
 * @enum rx_drv8263_ipropi_t
 * @brief DRV8263H-Q1 IPROPI current sensing constants
 *
 * @details
 * Constants for converting IPROPI analog voltage to motor current.
 * V_IPROPI = I_motor * k_drv8263_ipropi_mirror_ua_per_a * k_drv8263_ipropi_sense_ohm * 1e-6
 * I_motor = V_IPROPI / 1.0302
 *
 * @see rx_drv8263_adc_to_amps() Conversion helper function
 *
 * @code
 * float divisor = (float)k_drv8263_ipropi_mirror_ua_per_a
 *               * (float)k_drv8263_ipropi_sense_ohm * 1e-6F;
 * // divisor == 1.0302F
 * @endcode
 *
 * @invariant k_drv8263_ipropi_mirror_ua_per_a == 202 (integer compile-time constant)
 * @invariant k_drv8263_ipropi_sense_ohm == 5100 (integer compile-time constant)
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_drv8263_ipropi_mirror_ua_per_a =
    202, /**< Current mirror ratio: 202 uA per amp of motor current */
  k_drv8263_ipropi_sense_ohm =
    5100, /**< IPROPI sense resistor value in ohms (5.1k, 1% tolerance) */
} rx_drv8263_ipropi_t;

/**
 * @enum rx_drv8263_timing_us_t
 * @brief DRV8263H-Q1 timing constants in microseconds
 *
 * @details
 * Timing parameters from DRV8263H-Q1 datasheet for fault reset and OLP.
 *
 * @invariant k_drv8263_nsleep_pulse_us must be in range [5, 35] per datasheet
 *
 * @code
 * uint16_t pulse = k_drv8263_nsleep_pulse_us;  // 20 us
 * uint16_t wake  = k_drv8263_twake_us;         // 1200 us
 * uint16_t settle = k_drv8263_olp_settle_us;   // 50 us
 * @endcode
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_drv8263_nsleep_pulse_us =
    20, /**< nSLEEP LOW pulse duration for latched fault reset (target middle of 5-35 us window) */
  k_drv8263_twake_us =
    1200, /**< Wake-up time after nSLEEP HIGH (nFAULT returns HIGH, ~1.2 ms typ) */
  k_drv8263_olp_settle_us = 50, /**< Settling time between OLP test patterns (conservative) */
} rx_drv8263_timing_us_t;

/**
 * @enum rx_drv8263_olp_pattern_count_t
 * @brief Number of OLP test patterns
 *
 * @details
 * The DRV8263H-Q1 OLP diagnostic uses exactly 3 test patterns (count=3).
 * Each pattern sets a unique IN1/IN2 combination while DRVOFF=HIGH, then
 * reads nFAULT to determine load condition:
 * - Pattern 0: IN1=0, IN2=0 (both inputs low, baseline reading)
 * - Pattern 1: IN1=1, IN2=0 (drives OUT1 side, detects open on OUT1)
 * - Pattern 2: IN1=0, IN2=1 (drives OUT2 side, detects open on OUT2)
 *
 * The 3-bit nFAULT result vector from these patterns is decoded against
 * the OLP truth table to classify the load as normal, open, shorted to
 * GND, or shorted to VM.
 *
 * @see rx_drv8263_run_olp() Executes the OLP diagnostic using these patterns
 * @see rx_drv8263_olp_result_t Result classification from pattern decoding
 *
 * @code
 * for (uint8_t i = 0; i < k_drv8263_olp_pattern_count; i++) {
 *     // apply pattern[i], read nFAULT
 * }
 * @endcode
 *
 * @invariant k_drv8263_olp_pattern_count == 3
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_drv8263_olp_pattern_count = 3, /**< Three test patterns for OLP diagnostic */
} rx_drv8263_olp_pattern_count_t;

/* =============================================================================
 * Types
 * ============================================================================= */

/**
 * @enum rx_drv8263_olp_result_t
 * @brief Open Load Protection diagnostic result per motor channel
 *
 * @details
 * Results from the DRV8263H-Q1 OLP diagnostic sequence. The 3-pattern test
 * with DRVOFF=HIGH reads nFAULT for each pattern to determine load condition.
 *
 * OLP Truth Table (nFAULT readings for patterns {IN1=0/IN2=0, IN1=1/IN2=0, IN1=0/IN2=1}):
 * - {1,1,1} = Normal (no fault)
 * - {1,1,0} = Open load on OUT2
 * - {1,0,1} = Open load on OUT1
 * - {0,1,1} = Short to GND
 * - {0,0,0} = Short to VM (or both shorted)
 *
 * @see rx_drv8263_run_olp() Executes OLP diagnostic
 *
 * @code
 * rx_drv8263_olp_result_t res = k_drv8263_olp_unknown;
 * // ... run OLP ...
 * if (res != k_drv8263_olp_normal) {
 *     // handle fault condition
 * }
 * @endcode
 *
 * @invariant Each value maps uniquely to a diagnostic state (0=normal, 1=open, 2=gnd, 3=vm, 4=unknown)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_drv8263_olp_normal       = 0, /**< No fault detected, load connected normally */
  k_drv8263_olp_open_load    = 1, /**< Open load detected (motor disconnected) */
  k_drv8263_olp_short_to_gnd = 2, /**< Short circuit to ground detected */
  k_drv8263_olp_short_to_vm  = 3, /**< Short circuit to motor supply (VM) detected */
  k_drv8263_olp_unknown      = 4, /**< Unknown or ambiguous diagnostic result */
} rx_drv8263_olp_result_t;

/**
 * @struct rx_drv8263_config_t
 * @brief Configuration for a DRV8263H-Q1 motor driver instance
 *
 * @details
 * Specifies GPIO port/pin assignments for each control signal and OLP
 * diagnostic behavior flags. Port numbers use rx_port_utils.h constants
 * (e.g., k_rx_port_6 = 6, k_rx_port_e = 14).
 *
 * @par Usage Example:
 * @code
 * const rx_drv8263_config_t config = {
 *     .port_drvoff = 6,  .pin_drvoff = 1,   // P61
 *     .port_nsleep = 6,  .pin_nsleep = 0,   // P60
 *     .port_nfault = 1,  .pin_nfault = 5,   // P15
 *     .port_in1    = 1,  .pin_in1    = 7,   // P17
 *     .port_in2    = 2,  .pin_in2    = 3,   // P23
 *     .olp_enable_boot  = true,
 *     .olp_enable_fault = true,
 * };
 * @endcode
 *
 * @invariant port_drvoff, port_nsleep, port_nfault, port_in1, port_in2 must be valid RX72N ports
 * @invariant pin values must be 0-7 (8-bit port width)
 *
 * @see rx_drv8263_init() Uses this configuration
 * @since Version 1.0.0
 */
typedef struct {
  uint8_t port_drvoff;      /**< DRVOFF GPIO port number (e.g., 6 for PORT6) */
  uint8_t pin_drvoff;       /**< DRVOFF GPIO pin number within port (0-7) */
  uint8_t port_nsleep;      /**< nSLEEP GPIO port number */
  uint8_t pin_nsleep;       /**< nSLEEP GPIO pin number within port (0-7) */
  uint8_t port_nfault;      /**< nFAULT GPIO port number (input, directly read for OLP) */
  uint8_t pin_nfault;       /**< nFAULT GPIO pin number within port (0-7) */
  uint8_t port_in1;         /**< IN1 GPIO port number (for OLP pattern output) */
  uint8_t pin_in1;          /**< IN1 GPIO pin number within port (0-7) */
  uint8_t port_in2;         /**< IN2 GPIO port number (for OLP pattern output) */
  uint8_t pin_in2;          /**< IN2 GPIO pin number within port (0-7) */
  bool    olp_enable_boot;  /**< Run OLP diagnostic during rx_drv8263_init() */
  bool    olp_enable_fault; /**< Run OLP diagnostic after clearing latched faults */
} rx_drv8263_config_t;

/**
 * @struct rx_drv8263_handle_t
 * @brief Runtime handle for a DRV8263H-Q1 motor driver instance
 *
 * @details
 * Stores configuration and initialization state. Allocated by caller
 * (typically static), initialized by rx_drv8263_init().
 *
 * @invariant initialized is false until rx_drv8263_init() succeeds
 *
 * @code
 * rx_drv8263_handle_t drv = {};
 * rx_err_t err = rx_drv8263_init(&drv, &config);
 * if (drv.initialized) { // ready to use }
 * @endcode
 *
 * @see rx_drv8263_init() Initializes handle
 * @since Version 1.0.0
 */
typedef struct {
  rx_drv8263_config_t config;      /**< Copied configuration (OLP flags mutable via setters) */
  bool                initialized; /**< true after successful init, false otherwise */
} rx_drv8263_handle_t;

/* =============================================================================
 * Public API
 * ============================================================================= */

/**
 * @brief Initialize DRV8263H-Q1 driver instance
 *
 * @details
 * Copies configuration, validates GPIO assignments, and optionally runs
 * OLP diagnostic if olp_enable_boot is set.
 *
 * @param[out] handle Pointer to handle structure. Must not be nullptr.
 * @param[in]  config Pointer to configuration. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_null_ptr if handle or config is nullptr
 * @retval k_rx_err_invalid_arg if GPIO port/pin values are invalid
 *
 * @pre handle must point to valid memory
 * @pre config must contain valid GPIO port/pin assignments
 *
 * @post handle->initialized = true on success
 * @post handle->config contains copy of *config
 *
 * @note Not thread-safe
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_drv8263_init(rx_drv8263_handle_t*       handle,
                                       const rx_drv8263_config_t* config);

/**
 * @brief Control DRVOFF output disable signal
 *
 * @details
 * DRVOFF is active-high: setting active=true drives DRVOFF HIGH to disable
 * H-bridge outputs. Setting active=false drives DRVOFF LOW to enable outputs.
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 * @param[in] active true = DRVOFF HIGH (outputs disabled), false = DRVOFF LOW (outputs enabled)
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_null_ptr if handle is nullptr
 * @retval k_rx_err_invalid_state if not initialized
 *
 * @pre handle must be initialized via rx_drv8263_init()
 * @pre DRVOFF GPIO must be configured as output (hardware_init responsibility)
 *
 * @post DRVOFF GPIO set to requested level
 * @post Handle remains initialized; no other H-bridge control pins modified
 *
 * @note Not thread-safe
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_drv8263_set_drvoff(rx_drv8263_handle_t* handle, bool active);

/**
 * @brief Clear latched fault by pulsing nSLEEP LOW
 *
 * @details
 * DRV8263H-Q1 latches faults until nSLEEP is pulsed LOW for 5-35 us.
 * This function:
 * 1. Drives nSLEEP LOW
 * 2. Busy-waits ~20 us (middle of 5-35 us window)
 * 3. Drives nSLEEP HIGH
 * 4. Waits ~1.2 ms for tWAKE (nFAULT returns HIGH)
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_null_ptr if handle is nullptr
 * @retval k_rx_err_invalid_state if not initialized
 *
 * @pre handle must be initialized via rx_drv8263_init()
 * @pre Motor outputs should be stopped before clearing fault
 *
 * @post Latched fault cleared (if fault condition removed)
 * @post nSLEEP returns to HIGH (awake)
 *
 * @warning Uses CPU busy-wait for ~20 us pulse. Interrupts are NOT disabled
 *          during the pulse, so actual duration may be slightly longer.
 * @note Uses tx_thread_sleep(1) for tWAKE delay (10 ms tick >= 1.2 ms required)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_drv8263_clear_latched_fault(rx_drv8263_handle_t* handle);

/**
 * @brief Run Open Load Protection diagnostic
 *
 * @details
 * Executes the DRV8263H-Q1 OLP diagnostic sequence with DRVOFF=HIGH:
 * 1. Set DRVOFF = HIGH (standby mode, enables OLP)
 * 2. For each of 3 test patterns, set IN1/IN2 and read nFAULT:
 *    - Pattern 0: IN1=0, IN2=0 -> read nFAULT
 *    - Pattern 1: IN1=1, IN2=0 -> read nFAULT
 *    - Pattern 2: IN1=0, IN2=1 -> read nFAULT
 * 3. Decode 3-bit nFAULT result against truth table
 * 4. Set DRVOFF = LOW (return to active mode)
 *
 * @param[in]  handle Pointer to initialized handle. Must not be nullptr.
 * @param[out] result_out1 OLP result for OUT1 (IN1 side). Must not be nullptr.
 * @param[out] result_out2 OLP result for OUT2 (IN2 side). Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_null_ptr if any pointer is nullptr
 * @retval k_rx_err_invalid_state if not initialized
 *
 * @pre handle must be initialized via rx_drv8263_init()
 * @pre Motor must be stopped (PWM disabled) before running OLP
 *
 * @post DRVOFF returns to LOW (outputs enabled)
 * @post result_out1 and result_out2 contain diagnostic results
 *
 * @warning IN1/IN2 pins are temporarily driven as GPIO during OLP.
 *          Motor PWM must be stopped before calling this function.
 * @note Execution time: ~200 us (3 patterns with settling delays)
 *
 * @code
 * rx_drv8263_handle_t drv;
 * // ... init drv ...
 *
 * // Stop PWM before running OLP
 * rx_drv8263_olp_result_t out1 = k_drv8263_olp_unknown;
 * rx_drv8263_olp_result_t out2 = k_drv8263_olp_unknown;
 * rx_err_t err = rx_drv8263_run_olp(&drv, &out1, &out2);
 * if (err == k_rx_ok) {
 *   if (out1 != k_drv8263_olp_normal) { // OUT1 fault
 *   }
 *   if (out2 != k_drv8263_olp_normal) { // OUT2 fault
 *   }
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_drv8263_run_olp(rx_drv8263_handle_t*     handle,
                                          rx_drv8263_olp_result_t* result_out1,
                                          rx_drv8263_olp_result_t* result_out2);

/**
 * @brief Convert IPROPI ADC voltage to motor current in amperes
 *
 * @details
 * Pure math conversion using DRV8263H-Q1 current sensing parameters:
 * I_motor = V_IPROPI / 1.0302
 * where 1.0302 = 202e-6 A/A * 5100 ohm
 *
 * @param[in] adc_voltage_v IPROPI analog voltage in volts (0.0 to 3.3V)
 *
 * @return Motor current in amperes
 *
 * @pre adc_voltage_v must be non-negative
 * @pre adc_voltage_v should be within ADC range (0.0 to 3.3V typical)
 *
 * @post Return value >= 0.0
 * @post Return value is adc_voltage_v / k_ipropi_divisor (exact mathematical relationship)
 *
 * @note Pure function, thread-safe, no side effects
 *
 * @par Example:
 * @code
 * float voltage = adc_read_voltage(k_adc_ch_motor0_current);
 * float amps = rx_drv8263_adc_to_amps(voltage);
 * // 1.65V -> 1.602A
 * @endcode
 *
 * @since Version 1.0.0
 */
static inline float rx_drv8263_adc_to_amps(float adc_voltage_v)
{
  /* Combined IPROPI conversion factor: 202e-6 * 5100 = 1.0302 */
  static const float s_ipropi_divisor = 1.0302F;

  return adc_voltage_v / s_ipropi_divisor;
}

/**
 * @brief Enable or disable OLP diagnostic at boot time
 *
 * @details
 * Toggles whether the Open Load Protection diagnostic runs during
 * rx_drv8263_init(). When enabled, init will execute the full 3-pattern
 * OLP sequence and log the result. This allows early detection of
 * disconnected or shorted motors before the control loop begins.
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 * @param[in] enable true to enable boot OLP, false to disable
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_null_ptr if handle is nullptr
 * @retval k_rx_err_invalid_state if not initialized
 *
 * @pre handle must not be nullptr
 * @pre handle must be initialized via rx_drv8263_init()
 *
 * @post handle->config.olp_enable_boot updated to match enable parameter
 * @post No hardware side effects (configuration change only)
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_drv8263_set_olp_boot_enable(rx_drv8263_handle_t* handle, bool enable);

/**
 * @brief Enable or disable OLP diagnostic in fault handler
 *
 * @details
 * Toggles whether the Open Load Protection diagnostic runs after clearing
 * a latched fault via rx_drv8263_clear_latched_fault(). When enabled, the
 * fault handler will execute the 3-pattern OLP sequence to determine if
 * the fault was caused by an open or shorted load condition.
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 * @param[in] enable true to enable fault OLP, false to disable
 *
 * @return k_rx_ok on success
 * @retval k_rx_err_null_ptr if handle is nullptr
 * @retval k_rx_err_invalid_state if not initialized
 *
 * @pre handle must not be nullptr
 * @pre handle must be initialized via rx_drv8263_init()
 *
 * @post handle->config.olp_enable_fault updated to match enable parameter
 * @post No hardware side effects (configuration change only)
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_drv8263_set_olp_fault_enable(rx_drv8263_handle_t* handle, bool enable);

/* =============================================================================
 * Internal Functions (exposed for unit testing only)
 * ============================================================================= */

#ifdef UNIT_TEST
#include "rx_check.h"
void     internal_delay_us(uint32_t us);
rx_err_t internal_validate_config(const rx_drv8263_config_t* config);
void     internal_gpio_write(uint8_t port, uint8_t pin, bool high);
bool     internal_gpio_read(uint8_t port, uint8_t pin);
void     internal_olp_apply_patterns(const rx_drv8263_handle_t* handle, bool nfault_readings[]);
void     internal_olp_decode_results(bool                     f0,
                                     bool                     f1,
                                     bool                     f2,
                                     rx_drv8263_olp_result_t* result_out1,
                                     rx_drv8263_olp_result_t* result_out2);
#endif /* UNIT_TEST */

#ifdef __cplusplus
}
#endif
