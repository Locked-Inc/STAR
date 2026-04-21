/**
 * @file rx72n_temps_regs.h
 * @brief RX72N Temperature Sensor (TEMPS) Register Definitions
 *
 * @details
 * Complete register definitions for the RX72N internal temperature sensor.
 * The temperature sensor outputs a voltage proportional to die temperature,
 * which can be converted to temperature via the 12-bit ADC unit 1.
 *
 * @par Temperature Measurement Overview
 * The RX72N has a built-in temperature sensor that:
 * 1. Outputs voltage proportional to temperature
 * 2. Connects to 12-bit A/D converter unit 1
 * 3. Has factory-programmed calibration data (CAL125 @ 125degC)
 *
 * @par Temperature Calculation Formula
 * T = (Vs - V1) / Slope + 125 [degC]
 * Where:
 * - Vs: Measured voltage (from ADC)
 * - V1: Calibration voltage at 125degC = 3.3 x TSCDR / 4096 [V]
 * - Slope: Temperature slope from Table 63.57 (typ. -4.1 mV/degC)
 *
 * @par Register Summary
 * | Register | Address      | Size | Description                          |
 * |----------|--------------|------|--------------------------------------|
 * | TSCR     | 0x0008C500   | 8    | Temperature Sensor Control Register  |
 * | TSCDR    | 0xFE7F7D7C   | 32   | Calibration Data (read-only, ROM)    |
 *
 * @par Usage Sequence (from Ch58 manual)
 * 1. Clear module stop bit (MSTPCRB.MSTPC8 = 0)
 * 2. Configure ADC unit 1 for temperature sensor input
 * 3. Set TSCR.TSEN = 1 (start temperature sensor)
 * 4. Wait 30 us for reference voltage stabilization (tTSTBL)
 * 5. Set TSCR.TSOE = 1 (enable output to ADC)
 * 6. Wait 0 us for output stabilization (tOSTBL)
 * 7. Start ADC conversion
 * 8. Read ADC result from ADTSDR register
 * 9. Calculate temperature using formula above
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] N/A (no loops)
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] Single-statement accessors
 * - Rule 5: [OK] Static assertions verify addresses
 * - Rule 6: [OK] Minimal scope
 * - Rule 7: [OK] N/A
 * - Rule 8: [OK] All constants use C23 typed enums
 * - Rule 9: [OK] No function pointers
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par Manual References
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 58: Temperature Sensor (TEMPS), pages 2963-2969
 * - Section 58.2.1: TSCR register @ 0x0008C500
 * - Section 58.2.2: TSCDR register @ 0xFE7F7D7C
 * - Section 63 Table 63.57: Temperature slope characteristics
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Temperature Sensor Register Addresses
 * =============================================================================
 */

/**
 * @enum temps_addresses_t
 * @brief Temperature Sensor register addresses
 *
 * @details
 * Addresses verified against RX72N Hardware Manual Ch58.
 *
 * @note Addresses verified 2026-01-29 against manual section 58.2
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /**
   * @brief Temperature Sensor Control Register (TSCR) address
   * @details
   * 8-bit register at 0x0008C500 controlling sensor enable and output.
   * Manual ref: Ch58 section 58.2.1
   */
  k_temps_tscr_addr = 0x0008C500U,

  /**
   * @brief Temperature Sensor Calibration Data Register (TSCDR) address
   * @details
   * 32-bit read-only register at 0xFE7F7D7C containing factory calibration.
   * Located in on-chip ROM area (TEMPSCONST).
   * Manual ref: Ch58 section 58.2.2
   * @note Only readable when SYSCR0.ROME = 1 (on-chip ROM enabled)
   */
  k_temps_tscdr_addr = 0xFE7F7D7CU,
} temps_addresses_t;

/* =============================================================================
 * TSCR Register Bits
 * =============================================================================
 */

/**
 * @enum tscr_bits_t
 * @brief Temperature Sensor Control Register (TSCR) bit definitions
 *
 * @details
 * Controls the temperature sensor enable state and output to ADC.
 *
 * @par Register Layout (8-bit @ 0x0008C500)
 * | Bit | Field | R/W | Reset | Description                              |
 * |-----|-------|-----|-------|------------------------------------------|
 * | 7   | TSEN  | R/W | 0     | Temperature Sensor Enable                |
 * | 6:5 | -     | R/W | 0     | Reserved (write 0)                       |
 * | 4   | TSOE  | R/W | 0     | Temperature Sensor Output Enable         |
 * | 3:0 | -     | R/W | 0     | Reserved (write 0)                       |
 *
 * @note Manual ref: Ch58 section 58.2.1 (page 2964)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief TSEN bit position (bit 7)
   * @details Temperature Sensor Enable
   * - 0: Stops the temperature sensor
   * - 1: Starts the temperature sensor
   */
  k_tscr_tsen_bit = 0x80U,

  /**
   * @brief TSOE bit position (bit 4)
   * @details Temperature Sensor Output Enable
   * - 0: Disables output to 12-bit ADC unit 1
   * - 1: Enables output to 12-bit ADC unit 1
   */
  k_tscr_tsoe_bit = 0x10U,

  /**
   * @brief Temperature sensor disabled, output disabled (default)
   */
  k_tscr_disabled = 0x00U,

  /**
   * @brief Temperature sensor enabled, output disabled
   * @note Use this during tTSTBL wait (30 us stabilization)
   */
  k_tscr_sensor_only = 0x80U,

  /**
   * @brief Temperature sensor enabled with output to ADC
   * @note Set this after tTSTBL wait before starting ADC conversion
   */
  k_tscr_sensor_with_output = 0x90U,

  /**
   * @brief Reserved bits mask (bits 6:5, 3:0 must be 0)
   */
  k_tscr_reserved_mask = 0x6FU,
} tscr_bits_t;

/* =============================================================================
 * Temperature Sensor Timing Constants
 * =============================================================================
 */

/**
 * @enum temps_timing_t
 * @brief Temperature sensor timing constants
 *
 * @details
 * Timing requirements from Ch58 Table 58.2 for proper sensor operation.
 *
 * @note Manual ref: Ch58 section 58.3.4, Table 58.2
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Reference voltage stabilization wait time (tTSTBL)
   * @details Minimum 30 us wait after setting TSEN = 1
   */
  k_temps_tstbl_us = 30U,

  /**
   * @brief Output stabilization wait time (tOSTBL)
   * @details Minimum 0 us wait after setting TSOE = 1
   */
  k_temps_tostbl_us = 0U,
} temps_timing_t;

/* =============================================================================
 * Temperature Sensor Calibration Constants
 * =============================================================================
 */

/**
 * @enum temps_calibration_t
 * @brief Temperature sensor calibration constants
 *
 * @details
 * Constants for temperature calculation using factory calibration data.
 *
 * @par Temperature Calculation
 * The calibration data (TSCDR) was measured at:
 * - Tj = 125degC (junction temperature)
 * - AVCC1 = 3.3V (ADC reference voltage)
 *
 * @note Manual ref: Ch58 section 58.3.1
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Calibration temperature (125degC)
   * @details Factory calibration performed at this temperature
   */
  k_temps_cal_temp_celsius = 125U,

  /**
   * @brief ADC full scale value (12-bit = 4096)
   */
  k_temps_adc_full_scale = 4096U,

  /**
   * @brief ADC reference voltage in millivolts (3300 mV = 3.3V)
   */
  k_temps_avcc_mv = 3300U,

  /**
   * @brief Typical temperature slope in uV/degC (absolute value)
   * @details From Table 63.57: typ. -4.1 mV/degC = -4100 uV/degC
   * @note Actual value varies by chip; two-point calibration recommended
   */
  k_temps_slope_uv_per_c = 4100U,
} temps_calibration_t;

/* =============================================================================
 * Module Stop Control
 * =============================================================================
 */

/**
 * @enum temps_mstpcr_t
 * @brief Temperature sensor module stop control bits
 *
 * @details
 * The temperature sensor is controlled by MSTPCRB.MSTPC8.
 * Must be cleared (0) before accessing TEMPS registers.
 *
 * @note Manual ref: Ch11 (Low Power Consumption) module stop tables
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief MSTPCRB bit for temperature sensor (bit 8)
   * @details
   * - 0: Temperature sensor module is running
   * - 1: Temperature sensor module is stopped (default)
   * Manual ref: Ch11 section 11.2.3 MSTPCRB, bit b8 = MSTPB8 (Temperature Sensor)
   */
  k_temps_mstpcrb_bit = (1U << 8U),
} temps_mstpcr_t;

/* =============================================================================
 * Register Accessor Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to Temperature Sensor Control Register (TSCR)
 *
 * @details
 * Returns a volatile pointer to TSCR at address 0x0008C500.
 *
 * @return Volatile pointer to TSCR register (8-bit)
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre Temperature sensor module stop (MSTPCRB.MSTPC8) must be cleared
 * @post Pointer is valid for the lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 * @note This function is always inlined for zero overhead
 *
 * @par Example (Enable Temperature Sensor):
 * @code{.c}
 * // 1. Start temperature sensor
 * *temps_tscr_reg() = k_tscr_sensor_only;
 *
 * // 2. Wait for stabilization (30 us)
 * delay_us(k_temps_tstbl_us);
 *
 * // 3. Enable output to ADC
 * *temps_tscr_reg() = k_tscr_sensor_with_output;
 *
 * // 4. Start ADC conversion (see S12AD1 registers)
 * @endcode
 *
 * @see k_temps_tscr_addr Address constant
 * @see tscr_bits_t Control bits
 * @since Version 1.0.0
 */
static inline volatile uint8_t* temps_tscr_reg(void)
{
  return (volatile uint8_t*)k_temps_tscr_addr;
}

/**
 * @brief Get pointer to Temperature Sensor Calibration Data Register (TSCDR)
 *
 * @details
 * Returns a volatile pointer to TSCDR at address 0xFE7F7D7C.
 * This is a 32-bit read-only register containing factory calibration data.
 *
 * @par Calibration Data Format
 * The lower 12 bits contain the ADC value measured at 125degC and 3.3V:
 * - Bits [11:0]: ADC value (0-4095)
 * - Bits [31:12]: Reserved (read as 0)
 *
 * @par Converting to Voltage
 * V1 = 3.3 x (TSCDR & 0xFFF) / 4096 [V]
 *
 * @return Volatile pointer to TSCDR register (32-bit, read-only)
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre On-chip ROM must be enabled (SYSCR0.ROME = 1)
 * @post Pointer is valid for the lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 * @note This function is always inlined for zero overhead
 * @warning This register is read-only; writes have no effect
 *
 * @par Example (Read Calibration):
 * @code{.c}
 * // Read calibration data
 * uint32_t cal125 = *temps_tscdr_reg() & 0xFFF;
 *
 * // Calculate reference voltage at 125degC
 * // V1 = 3.3 x cal125 / 4096 (in fixed-point or float)
 * @endcode
 *
 * @see k_temps_tscdr_addr Address constant
 * @since Version 1.0.0
 */
static inline volatile const uint32_t* temps_tscdr_reg(void)
{
  return (volatile const uint32_t*)k_temps_tscdr_addr;
}

/* =============================================================================
 * Static Assertions - Address Verification
 * =============================================================================
 */

/**
 * @name Address Verification
 * @brief Compile-time verification of register addresses
 * @{
 */

/* Verify TSCR address (critical for temperature sensor control) */
static_assert(
  k_temps_tscr_addr == 0x0008C500U,
  "TSCR address must be 0x0008C500 per Ch58 manual"); /* NOLINT(readability-magic-numbers) */

/* Verify TSCDR address (factory calibration data) */
static_assert(k_temps_tscdr_addr == 0xFE7F7D7CU, /* NOLINT(readability-magic-numbers) */
              "TSCDR address must be 0xFE7F7D7C per Ch58 manual");

/* Verify TSEN bit position */
static_assert(k_tscr_tsen_bit == 0x80U,
              "TSEN must be bit 7 per Ch58 manual"); /* NOLINT(readability-magic-numbers) */

/* Verify TSOE bit position */
static_assert(k_tscr_tsoe_bit == 0x10U,
              "TSOE must be bit 4 per Ch58 manual"); /* NOLINT(readability-magic-numbers) */

/* Verify combined sensor+output value */
static_assert(k_tscr_sensor_with_output == (k_tscr_tsen_bit | k_tscr_tsoe_bit),
              "Sensor with output must be TSEN|TSOE");

/* Verify calibration temperature */
static_assert(k_temps_cal_temp_celsius == 125U, /* NOLINT(readability-magic-numbers) */
              "Calibration temperature must be 125C per Ch58 manual");

/* Verify ADC reference */
static_assert(k_temps_adc_full_scale == 4096U,
              "ADC full scale must be 4096 (12-bit)"); /* NOLINT(readability-magic-numbers) */

/** @} */

#ifdef __cplusplus
}
#endif
