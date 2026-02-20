/* lib/rx_bq4050/inc/rx_bq4050_constants.h */

/**
 * @file rx_bq4050_constants.h
 * @brief BQ4050 Smart Battery System (SBS) command codes and status flag constants
 *
 * @details
 * This header centralizes all Texas Instruments BQ4050 battery fuel gauge constants
 * including SBS 1.1 command codes, battery status flags, and configuration parameters.
 * These constants are shared across the BQ4050 driver implementation and unit tests
 * to ensure consistent interpretation of battery status and correct register access.
 *
 * The BQ4050 is a fully integrated Li-Ion/Li-Polymer battery management IC that
 * implements the Smart Battery System (SBS) v1.1 specification over SMBus/I2C.
 * This file provides type-safe C23 enum constants for all SBS register addresses
 * and status bit definitions.
 *
 * **Key Features:**
 * - SBS 1.1 standard command codes (0x08-0x18)
 * - BQ4050-specific cell voltage registers (0x3C-0x3F)
 * - Battery status flag bit positions and masks
 * - Type-safe C23 typed enums with explicit underlying types
 * - Zero magic numbers policy compliance
 *
 * **Usage Context:**
 * - Used by rx_bq4050.c driver for register reads
 * - Used by test_rx_bq4050.c for mock data generation
 * - Ensures consistent interpretation of battery status across firmware
 *
 * @par Hardware Requirements:
 * | Component | Specification |
 * |-----------|---------------|
 * | IC | Texas Instruments BQ4050RSMR |
 * | Protocol | SMBus v1.1/v2.0 (System Management Bus) |
 * | I2C Speed | Up to 400 kHz (Fast Mode) |
 * | I2C Address | 0x0B (7-bit) |
 * | Battery Configuration | 1-4 series Li-Ion/Li-Polymer cells |
 *
 * @par Module Dependencies:
 * - stdint.h: Standard integer types (uint8_t, uint16_t)
 * - No STAR firmware dependencies (constants only)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No control flow (constants only)
 * - Rule 2: [OK] N/A (no loops)
 * - Rule 3: [OK] No dynamic memory
 * - Rule 4: [OK] N/A (no functions)
 * - Rule 5: [OK] N/A (no assertions needed for constants)
 * - Rule 6: [OK] All constants at file scope
 * - Rule 7: [OK] N/A (no function calls)
 * - Rule 8: [OK] C23 typed enums used for ALL constants (no macros)
 * - Rule 9: [OK] N/A (no pointers)
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @see BQ4050 Data Sheet (Texas Instruments SLUUAQ3A)
 * @see Smart Battery Data Specification v1.1 (SBS-IF)
 * @see rx_bq4050.h BQ4050 driver API
 * @see rx_bq4050.c BQ4050 driver implementation
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright MIT License
 *
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * SBS Command Codes
 * =============================================================================
 */

/**
 * @enum bq4050_sbs_command_t
 * @brief Smart Battery System (SBS) 1.1 standard command register addresses
 *
 * @details
 * Defines SMBus/I2C register addresses for accessing battery parameters via
 * the Smart Battery Data Specification v1.1. All registers are 16-bit word
 * registers accessed via SMBus Read Word protocol.
 *
 * The BQ4050 implements both SBS standard commands (0x00-0x18) and Texas
 * Instruments manufacturer-specific extensions (0x3C-0x3F for individual
 * cell voltages).
 *
 * **Register Access Protocol:**
 * - All registers: SMBus Read Word (2 bytes + optional PEC)
 * - Address format: 7-bit I2C address 0x0B
 * - Byte order: Little-endian (LSB first)
 * - Optional: Packet Error Checking (PEC) using CRC-8
 *
 * **Data Formats:**
 * - Voltage: Unsigned 16-bit, millivolts (mV)
 * - Current: Signed 16-bit, milliamps (mA, + = charging, - = discharging)
 * - Temperature: Unsigned 16-bit, 0.1 Kelvin units
 * - Capacity: Unsigned 16-bit, milliamp-hours (mAh)
 * - State of Charge: Unsigned 16-bit, percent (0-100%)
 * - Time: Unsigned 16-bit, minutes (0xFFFF = invalid/not applicable)
 *
 * @par Register Map Table:
 *
 * | Address | Name | Type | Units | Description |
 * |---------|------|------|-------|-------------|
 * | 0x00 | ManufacturerAccess | word | - | Manufacturer commands |
 * | 0x08 | Temperature | word | 0.1K | Pack temperature |
 * | 0x09 | Voltage | word | mV | Pack voltage |
 * | 0x0A | Current | word | mA | Instantaneous current (signed) |
 * | 0x0B | AverageCurrent | word | mA | 1-min average (signed) |
 * | 0x0D | RelativeStateOfCharge | word | % | SOC vs full capacity |
 * | 0x0E | AbsoluteStateOfCharge | word | % | SOC vs design capacity |
 * | 0x0F | RemainingCapacity | word | mAh | Current remaining |
 * | 0x10 | FullChargeCapacity | word | mAh | Aged full capacity |
 * | 0x11 | RunTimeToEmpty | word | min | Time to empty |
 * | 0x13 | AverageTimeToFull | word | min | Time to full |
 * | 0x16 | BatteryStatus | word | flags | Status flags |
 * | 0x17 | CycleCount | word | cycles | Charge/discharge cycles |
 * | 0x18 | DesignCapacity | word | mAh | New battery capacity |
 * | 0x3C | CellVoltage4 | word | mV | Cell 4 voltage (TI ext) |
 * | 0x3D | CellVoltage3 | word | mV | Cell 3 voltage (TI ext) |
 * | 0x3E | CellVoltage2 | word | mV | Cell 2 voltage (TI ext) |
 * | 0x3F | CellVoltage1 | word | mV | Cell 1 voltage (TI ext) |
 *
 * @par Usage Example:
 * @code{.c}
 * // Read pack voltage from SBS register 0x09
 * uint16_t voltage_mv;
 * rx_err_t err = rx_bus_smbus_read_word_data(manager,
 *                                              bus_name,
 *                                              k_sbs_voltage,
 *                                              &voltage_mv);
 * if (err == k_rx_ok) {
 *     rx_log_info_val("BQ4050", "Pack voltage (mV)", voltage_mv);
 * }
 *
 * // Read all cell voltages using BQ4050-specific registers
 * uint16_t cell_voltages[4];
 * const uint8_t cell_regs[] = {
 *     k_sbs_cell_voltage_1, k_sbs_cell_voltage_2,
 *     k_sbs_cell_voltage_3, k_sbs_cell_voltage_4
 * };
 * for (uint8_t i = 0; i < 4; i++) {
 *     err = rx_bus_smbus_read_word_data(manager, bus_name,
 *                                        cell_regs[i], &cell_voltages[i]);
 * }
 * @endcode
 *
 * @see rx_bus_smbus_read_word_data() SMBus read word function
 * @see rx_bq4050_read_voltage() High-level voltage read API
 * @see rx_bq4050_read_cell_voltages() High-level cell voltage read API
 * @see Smart Battery Data Specification v1.1 section 5.1 (Register Map)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief ManufacturerAccess command register (0x00)
   * @details
   * Write-then-read register for manufacturer-specific commands.
   * Used for advanced BQ4050 configuration, authentication, and diagnostics.
   * Requires specific command sequences defined in BQ4050 technical reference.
   * **Access:** Read/Write word
   * **Format:** Command-dependent
   */
  k_sbs_manufacturer_access = 0x00,

  /**
   * @brief Temperature register (0x08)
   * @details
   * Battery pack temperature measured by internal thermistor.
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, 0.1 Kelvin units
   * **Range:** 0-6553.5K (-273.15degC to 6280.35degC)
   * **Typical Range:** 2731-3731 (0degC to 100degC)
   * **Conversion:** temp_celsius = (value / 10.0) - 273.15
   * **Update Rate:** ~1 second
   */
  k_sbs_temperature = 0x08,

  /**
   * @brief Voltage register (0x09)
   * @details
   * Total battery pack voltage (sum of all cells in series).
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, millivolts
   * **Range:** 0-65535 mV (0-65.535V)
   * **Typical Range (4S):** 12000-16800 mV (3.0V-4.2V per cell)
   * **Resolution:** 1 mV
   * **Update Rate:** ~250 ms
   */
  k_sbs_voltage = 0x09,

  /**
   * @brief Current register (0x0A)
   * @details
   * Instantaneous battery current (charge/discharge rate).
   * **Access:** Read-only word
   * **Format:** Signed 16-bit, milliamps
   * **Range:** -32768 to +32767 mA
   * **Sign Convention:** Positive = charging, Negative = discharging
   * **Resolution:** 1 mA
   * **Update Rate:** ~250 ms
   * **Accuracy:** +/-1% typical
   */
  k_sbs_current = 0x0A,

  /**
   * @brief AverageCurrent register (0x0B)
   * @details
   * Rolling 1-minute average of battery current.
   * Used for smoother SOC calculations and time-to-empty estimates.
   * **Access:** Read-only word
   * **Format:** Signed 16-bit, milliamps
   * **Range:** -32768 to +32767 mA
   * **Sign Convention:** Positive = charging, Negative = discharging
   * **Averaging Window:** 60 seconds (rolling)
   * **Resolution:** 1 mA
   * **Update Rate:** ~1 second
   */
  k_sbs_average_current = 0x0B,

  /**
   * @brief RelativeStateOfCharge register (0x0D)
   * @details
   * State of charge as percentage of current full charge capacity.
   * Accounts for battery aging - shows charge relative to what battery
   * can actually hold now, not original design capacity.
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, percent
   * **Range:** 0-100% (values >100% possible during overcharge)
   * **Resolution:** 1%
   * **Update Rate:** ~1 second
   * **Algorithm:** CEDV (Compensated End-of-Discharge Voltage)
   * **Typical Use:** Display to user (accounts for aging)
   */
  k_sbs_relative_state_of_charge = 0x0D,

  /**
   * @brief AbsoluteStateOfCharge register (0x0E)
   * @details
   * State of charge as percentage of original design capacity.
   * Does NOT account for aging - shows charge vs new battery capacity.
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, percent
   * **Range:** 0-100%
   * **Resolution:** 1%
   * **Update Rate:** ~1 second
   * **Typical Use:** Battery health monitoring (compare to relative SOC)
   */
  k_sbs_absolute_state_of_charge = 0x0E,

  /**
   * @brief RemainingCapacity register (0x0F)
   * @details
   * Current remaining battery capacity in milliamp-hours.
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, milliamp-hours
   * **Range:** 0-65535 mAh
   * **Resolution:** 1 mAh
   * **Update Rate:** ~1 second
   * **Algorithm:** Based on CEDV gauging + coulomb counting
   * **Relationship:** remaining_capacity = (relative_soc / 100) * full_charge_capacity
   */
  k_sbs_remaining_capacity = 0x0F,

  /**
   * @brief FullChargeCapacity register (0x10)
   * @details
   * Current full charge capacity accounting for battery aging.
   * This value decreases over battery lifetime as cells degrade.
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, milliamp-hours
   * **Range:** 0-65535 mAh
   * **Resolution:** 1 mAh
   * **Update Rate:** Updated after each full charge cycle
   * **Typical Degradation:** 80% of design capacity after 500 cycles
   * **Relationship:** Aged capacity, less than design_capacity
   */
  k_sbs_full_charge_capacity = 0x10,

  /**
   * @brief RunTimeToEmpty register (0x11)
   * @details
   * Estimated time until battery is empty at current discharge rate.
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, minutes
   * **Range:** 0-65534 minutes, 0xFFFF = invalid/not discharging
   * **Resolution:** 1 minute
   * **Update Rate:** ~10 seconds
   * **Invalid Conditions:** Charging, idle, or SOC increasing
   * **Algorithm:** remaining_capacity / abs(average_current) * 60
   */
  k_sbs_run_time_to_empty = 0x11,

  /**
   * @brief AverageTimeToFull register (0x13)
   * @details
   * Estimated time until battery is fully charged at current charge rate.
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, minutes
   * **Range:** 0-65534 minutes, 0xFFFF = invalid/not charging
   * **Resolution:** 1 minute
   * **Update Rate:** ~10 seconds
   * **Invalid Conditions:** Discharging, idle, or SOC decreasing
   * **Algorithm:** (full_capacity - remaining_capacity) / average_current * 60
   */
  k_sbs_average_time_to_full = 0x13,

  /**
   * @brief BatteryStatus register (0x16)
   * @details
   * 16-bit status flags indicating battery state and alarm conditions.
   * See bq4050_status_flags_t for bit definitions.
   * **Access:** Read-only word
   * **Format:** 16-bit flags (see status flag enums below)
   * **Update Rate:** ~1 second
   * **Critical Flags:** Over/under voltage, over/under temp, overcurrent
   * **State Flags:** Charging, discharging, fully charged, fully discharged
   */
  k_sbs_battery_status = 0x16,

  /**
   * @brief CycleCount register (0x17)
   * @details
   * Number of charge/discharge cycles completed.
   * Cycle = discharge of 100% capacity (may span multiple partial cycles).
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, cycle count
   * **Range:** 0-65535 cycles
   * **Resolution:** 1 cycle
   * **Update Rate:** Updated after each full cycle
   * **Typical Lifespan:** 500-1000 cycles to 80% capacity (Li-Ion)
   */
  k_sbs_cycle_count = 0x17,

  /**
   * @brief DesignCapacity register (0x18)
   * @details
   * Original battery capacity when new (factory specification).
   * **Access:** Read-only word (programmed during manufacturing)
   * **Format:** Unsigned 16-bit, milliamp-hours
   * **Range:** 0-65535 mAh
   * **Resolution:** 1 mAh
   * **Typical Values:** 2000-10000 mAh (depends on battery pack)
   * **Relationship:** Constant, compare to full_charge_capacity for health
   */
  k_sbs_design_capacity = 0x18,

  /**
   * @brief CellVoltage4 register (0x3C, BQ4050-specific extension)
   * @details
   * Individual voltage of cell 4 (top cell in 4S configuration).
   * Texas Instruments BQ4050 manufacturer extension (not in SBS spec).
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, millivolts
   * **Range:** 0-65535 mV (typical 3000-4200 mV for Li-Ion)
   * **Resolution:** 1 mV
   * **Update Rate:** ~250 ms
   * **Note:** Only valid if battery has 4 cells
   */
  k_sbs_cell_voltage_4 = 0x3C,

  /**
   * @brief CellVoltage3 register (0x3D, BQ4050-specific extension)
   * @details
   * Individual voltage of cell 3.
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, millivolts
   * **Range:** 0-65535 mV (typical 3000-4200 mV for Li-Ion)
   * **Resolution:** 1 mV
   * **Update Rate:** ~250 ms
   * **Note:** Only valid if battery has 3 or 4 cells
   */
  k_sbs_cell_voltage_3 = 0x3D,

  /**
   * @brief CellVoltage2 register (0x3E, BQ4050-specific extension)
   * @details
   * Individual voltage of cell 2.
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, millivolts
   * **Range:** 0-65535 mV (typical 3000-4200 mV for Li-Ion)
   * **Resolution:** 1 mV
   * **Update Rate:** ~250 ms
   * **Note:** Only valid if battery has 2, 3, or 4 cells
   */
  k_sbs_cell_voltage_2 = 0x3E,

  /**
   * @brief CellVoltage1 register (0x3F, BQ4050-specific extension)
   * @details
   * Individual voltage of cell 1 (bottom cell, closest to ground).
   * **Access:** Read-only word
   * **Format:** Unsigned 16-bit, millivolts
   * **Range:** 0-65535 mV (typical 3000-4200 mV for Li-Ion)
   * **Resolution:** 1 mV
   * **Update Rate:** ~250 ms
   * **Note:** Always valid (all configurations have at least 1 cell)
   */
  k_sbs_cell_voltage_1 = 0x3F,
} bq4050_sbs_command_t;

/* =============================================================================
 * Battery Status Flags
 * =============================================================================
 */

/**
 * @enum bq4050_status_check_constants_t
 * @brief Constants for checking battery status flags
 *
 * @details
 * Provides named constant for comparing status flag bitwise operations.
 * Used to check if specific status bits are set without magic number 0.
 *
 * **Usage Pattern:**
 * @code{.c}
 * if ((status_flags & k_bq4050_status_fully_charged) != k_bq4050_status_flag_clear) {
 *     // Battery is fully charged
 * }
 * @endcode
 *
 * @par Rationale:
 * Using named constant instead of literal 0 improves code readability and
 * follows zero magic numbers policy. Makes intent explicit in comparisons.
 *
 * @see bq4050_status_flags_t Status flag bit masks
 * @see rx_bq4050_read_status() Function that interprets status flags
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Status flag clear value (0x0000)
   * @details
   * Represents no status flags set. Use in != comparisons to check if flag is set.
   * **Value:** 0
   * **Usage:** if ((flags & k_bq4050_status_xxx) != k_bq4050_status_flag_clear)
   */
  k_bq4050_status_flag_clear = 0,
} bq4050_status_check_constants_t;

/**
 * @enum bq4050_status_bit_positions_t
 * @brief Bit positions for battery status flags in BatteryStatus register (0x16)
 *
 * @details
 * Defines bit positions for status flags in the SBS BatteryStatus register.
 * These values are used to construct bit masks in bq4050_status_flags_t.
 *
 * **Register Format:** 16-bit status word (SBS register 0x16)
 * - Bits 15-12: Alarm flags
 * - Bits 11-8: Warning flags
 * - Bits 7-4: Battery state flags
 * - Bits 3-0: Reserved/vendor-specific
 *
 * @par Status Flag Bit Map:
 * | Bit | Name | Description |
 * |-----|------|-------------|
 * | 15 | OCA | Over-Charged Alarm |
 * | 14 | TCA | Terminate Charge Alarm |
 * | 13 | (reserved) | |
 * | 12 | OTA | Over-Temperature Alarm |
 * | 11 | TDA | Terminate Discharge Alarm |
 * | 10 | (reserved) | |
 * | 9 | RCA | Remaining Capacity Alarm |
 * | 8 | RTA | Remaining Time Alarm |
 * | 7 | INIT | Initialized |
 * | 6 | DSG | Discharging |
 * | 5 | FC | Fully Charged |
 * | 4 | FD | Fully Discharged |
 * | 3-0 | (reserved) | |
 *
 * @note Not all bits are used - only bits with flags defined in SBS spec
 *
 * @see bq4050_status_flags_t Bit masks constructed from these positions
 * @see Smart Battery Data Specification v1.1 section 5.4 (BatteryStatus)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Over-Charged Alarm bit position (bit 15)
   * @details
   * Set when battery voltage exceeds safe overcharge threshold.
   * **Position:** Bit 15 (MSB)
   * **Priority:** CRITICAL - stop charging immediately
   * **Action:** Disconnect charger, log fault
   */
  k_bq4050_status_bit_pos_over_charged_alarm = 15,

  /**
   * @brief Terminate Charge Alarm bit position (bit 14)
   * @details
   * Set when charging should stop (voltage reached, temp limit, etc).
   * **Position:** Bit 14
   * **Priority:** HIGH - normal charge termination
   * **Action:** Transition to maintenance/trickle charge
   */
  k_bq4050_status_bit_pos_terminate_charge_alarm = 14,

  /**
   * @brief Over-Temperature Alarm bit position (bit 12)
   * @details
   * Set when battery temperature exceeds safe operating range.
   * **Position:** Bit 12
   * **Priority:** CRITICAL - thermal protection
   * **Action:** Stop charge/discharge, allow cooling
   * **Typical Threshold:** >60degC (configurable in data flash)
   */
  k_bq4050_status_bit_pos_over_temp_alarm = 12,

  /**
   * @brief Terminate Discharge Alarm bit position (bit 11)
   * @details
   * Set when discharge should stop (low voltage, undervoltage protection).
   * **Position:** Bit 11
   * **Priority:** HIGH - prevent deep discharge damage
   * **Action:** Disconnect load, enter low-power mode
   */
  k_bq4050_status_bit_pos_terminate_discharge_alarm = 11,

  /**
   * @brief Remaining Capacity Alarm bit position (bit 9)
   * @details
   * Set when remaining capacity drops below configured threshold.
   * **Position:** Bit 9
   * **Priority:** MEDIUM - low battery warning
   * **Action:** Warn user, reduce power consumption
   * **Typical Threshold:** 10-20% SOC (configurable)
   */
  k_bq4050_status_bit_pos_remaining_capacity_alarm = 9,

  /**
   * @brief Remaining Time Alarm bit position (bit 8)
   * @details
   * Set when estimated time to empty drops below threshold.
   * **Position:** Bit 8
   * **Priority:** MEDIUM - low runtime warning
   * **Action:** Warn user to save work/shutdown soon
   * **Typical Threshold:** 10 minutes (configurable)
   */
  k_bq4050_status_bit_pos_remaining_time_alarm = 8,

  /**
   * @brief Initialized bit position (bit 7)
   * @details
   * Set when battery has been initialized and gauging is valid.
   * **Position:** Bit 7
   * **Priority:** INFO - gauging status
   * **Cleared:** After battery replacement or data flash reset
   * **Set:** After initial capacity learning cycle complete
   */
  k_bq4050_status_bit_pos_initialized = 7,

  /**
   * @brief Discharging bit position (bit 6)
   * @details
   * Set when battery is discharging (negative current).
   * **Position:** Bit 6
   * **Priority:** INFO - charge state indicator
   * **Cleared:** When idle or charging
   * **Set:** When current < 0 mA
   */
  k_bq4050_status_bit_pos_discharging = 6,

  /**
   * @brief Fully Charged bit position (bit 5)
   * @details
   * Set when battery has reached full charge.
   * **Position:** Bit 5
   * **Priority:** INFO - charge complete indicator
   * **Conditions:** Voltage at charge voltage, current < termination current
   */
  k_bq4050_status_bit_pos_fully_charged = 5,

  /**
   * @brief Fully Discharged bit position (bit 4)
   * @details
   * Set when battery has been fully discharged.
   * **Position:** Bit 4
   * **Priority:** INFO - discharge complete indicator
   * **Conditions:** Voltage at discharge cutoff, SOC at 0%
   */
  k_bq4050_status_bit_pos_fully_discharged = 4,
} bq4050_status_bit_positions_t;

/**
 * @enum bq4050_status_flags_t
 * @brief Battery status flag bit masks for BatteryStatus register (0x16)
 *
 * @details
 * Defines bit masks for testing individual status flags in the 16-bit
 * BatteryStatus register (SBS register 0x16). Each flag is a single bit
 * extracted from the status word using bitwise AND operations.
 *
 * **Testing Flags:**
 * @code{.c}
 * uint16_t status_word;
 * rx_bus_smbus_read_word_data(manager, bus, k_sbs_battery_status, &status_word);
 *
 * // Check if battery is charging (DISCHARGING bit is clear)
 * bool is_charging = !(status_word & k_bq4050_status_discharging);
 *
 * // Check if battery is fully charged
 * if ((status_word & k_bq4050_status_fully_charged) != k_bq4050_status_flag_clear) {
 *     // Battery is fully charged
 * }
 *
 * // Check for critical alarms
 * if (status_word & (k_bq4050_status_over_charged_alarm |
 *                     k_bq4050_status_over_temp_alarm)) {
 *     // CRITICAL: Emergency stop charge/discharge
 * }
 * @endcode
 *
 * @par Status Priority Levels:
 * - **CRITICAL:** over_charged_alarm, over_temp_alarm -> Emergency shutdown
 * - **HIGH:** terminate_charge_alarm, terminate_discharge_alarm -> Stop operation
 * - **MEDIUM:** remaining_capacity_alarm, remaining_time_alarm -> User warnings
 * - **INFO:** initialized, discharging, fully_charged, fully_discharged -> Status display
 *
 * @note All flags use explicit != k_bq4050_status_flag_clear comparisons to avoid
 *       implicit boolean conversion and satisfy zero magic numbers policy.
 *
 * @see bq4050_status_bit_positions_t Bit positions used to construct these masks
 * @see bq4050_status_check_constants_t Constants for flag comparisons
 * @see rx_bq4050_read_status() High-level function that interprets these flags
 * @see Smart Battery Data Specification v1.1 section 5.4
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Over-Charged Alarm flag (0x8000, bit 15)
   * @details
   * **CRITICAL ALARM** - Battery voltage exceeded safe charging limit.
   * **Action Required:** Disconnect charger immediately, enter fault state.
   * **Cause:** Charger malfunction, overvoltage condition.
   * **Typical Threshold:** >4.3V per cell for Li-Ion.
   * **Recovery:** Disconnect charger, wait for voltage to stabilize <4.2V/cell.
   */
  k_bq4050_status_over_charged_alarm = (1U << k_bq4050_status_bit_pos_over_charged_alarm),

  /**
   * @brief Terminate Charge Alarm flag (0x4000, bit 14)
   * @details
   * **HIGH PRIORITY** - Charge termination conditions met.
   * **Action Required:** Stop constant-current charge, transition to trickle.
   * **Cause:** Reached charge voltage + termination current threshold.
   * **Typical Conditions:** 4.2V/cell AND current <100mA.
   */
  k_bq4050_status_terminate_charge_alarm = (1U << k_bq4050_status_bit_pos_terminate_charge_alarm),

  /**
   * @brief Over-Temperature Alarm flag (0x1000, bit 12)
   * @details
   * **CRITICAL ALARM** - Battery temperature exceeded safe operating range.
   * **Action Required:** Stop charge/discharge, monitor temperature.
   * **Cause:** Excessive ambient temp, high current, poor thermal design.
   * **Typical Threshold:** >60degC (configurable in BQ4050 data flash).
   * **Recovery:** Allow battery to cool <45degC before resuming operation.
   */
  k_bq4050_status_over_temp_alarm = (1U << k_bq4050_status_bit_pos_over_temp_alarm),

  /**
   * @brief Terminate Discharge Alarm flag (0x0800, bit 11)
   * @details
   * **HIGH PRIORITY** - Discharge termination to prevent damage.
   * **Action Required:** Disconnect load, enter low-power mode.
   * **Cause:** Cell voltage at minimum safe discharge voltage.
   * **Typical Threshold:** 3.0V per cell for Li-Ion.
   * **Recovery:** Charge battery above 3.3V/cell minimum.
   */
  k_bq4050_status_terminate_discharge_alarm =
    (1U << k_bq4050_status_bit_pos_terminate_discharge_alarm),

  /**
   * @brief Remaining Capacity Alarm flag (0x0200, bit 9)
   * @details
   * **MEDIUM PRIORITY** - Low battery capacity warning.
   * **Action Required:** Warn user, initiate power saving mode.
   * **Cause:** Remaining capacity < configured threshold (typically 10-20%).
   * **Typical Use:** Battery icon warning, reduce system performance.
   */
  k_bq4050_status_remaining_capacity_alarm =
    (1U << k_bq4050_status_bit_pos_remaining_capacity_alarm),

  /**
   * @brief Remaining Time Alarm flag (0x0100, bit 8)
   * @details
   * **MEDIUM PRIORITY** - Low runtime warning.
   * **Action Required:** Warn user to save work/prepare for shutdown.
   * **Cause:** Estimated time to empty < threshold (typically 10 minutes).
   * **Note:** Only valid when discharging at consistent load.
   */
  k_bq4050_status_remaining_time_alarm = (1U << k_bq4050_status_bit_pos_remaining_time_alarm),

  /**
   * @brief Initialized flag (0x0080, bit 7)
   * @details
   * **INFO** - Battery gauging algorithm initialized and calibrated.
   * **Set:** After initial capacity learning cycle complete.
   * **Cleared:** After battery replacement or BQ4050 reset.
   * **Note:** SOC readings invalid until this bit is set.
   */
  k_bq4050_status_initialized = (1U << k_bq4050_status_bit_pos_initialized),

  /**
   * @brief Discharging flag (0x0040, bit 6)
   * @details
   * **INFO** - Battery is currently discharging (negative current).
   * **Set:** When average current < 0 mA (load connected).
   * **Cleared:** When idle or charging (current >= 0).
   * **Note:** Inverted logic - cleared means charging or idle.
   */
  k_bq4050_status_discharging = (1U << k_bq4050_status_bit_pos_discharging),

  /**
   * @brief Fully Charged flag (0x0020, bit 5)
   * @details
   * **INFO** - Battery has reached full charge state.
   * **Set:** When charge termination complete + stabilization period.
   * **Conditions:** Voltage at charge voltage, current < C/20, stable.
   * **Note:** May be set even if RelativeSOC shows <100% due to gauging.
   */
  k_bq4050_status_fully_charged = (1U << k_bq4050_status_bit_pos_fully_charged),

  /**
   * @brief Fully Discharged flag (0x0010, bit 4)
   * @details
   * **INFO** - Battery has reached full discharge state.
   * **Set:** When discharge cutoff voltage reached.
   * **Conditions:** Voltage at minimum safe voltage, SOC at 0%.
   * **Note:** Load should be disconnected when this is set.
   */
  k_bq4050_status_fully_discharged = (1U << k_bq4050_status_bit_pos_fully_discharged),
} bq4050_status_flags_t;

/* =============================================================================
 * Time Constants
 * =============================================================================
 */

/**
 * @var s_bq4050_time_to_full_invalid
 * @brief Invalid/not-applicable time-to-full value (0xFFFF)
 *
 * @details
 * The BQ4050 returns 0xFFFF (65535) for time-to-full when the value is
 * invalid or not applicable. This occurs when:
 * - Battery is discharging (no charging current)
 * - Battery is idle (no charge/discharge)
 * - Charge rate too low to estimate time to full
 * - Battery is already fully charged
 *
 * **Value:** 0xFFFF (65535 decimal)
 * **Register:** AverageTimeToFull (0x13)
 * **Type:** uint16_t constant
 *
 * @par Usage Example:
 * @code{.c}
 * uint16_t time_to_full_min;
 * rx_err_t err = rx_bus_smbus_read_word_data(manager, bus_name,
 *                                              k_sbs_average_time_to_full,
 *                                              &time_to_full_min);
 *
 * if (err == k_rx_ok) {
 *     if (time_to_full_min == s_bq4050_time_to_full_invalid) {
 *         rx_log_info("BQ4050", "Time to full: N/A (not charging)");
 *     } else {
 *         rx_log_info_val("BQ4050", "Time to full (min)", time_to_full_min);
 *     }
 * }
 * @endcode
 *
 * @par Rationale:
 * - Static const instead of enum because 0xFFFF doesn't fit in uint8_t
 * - Static storage avoids redundant copies in multiple translation units
 * - Const ensures compile-time optimization and prevents modification
 *
 * @note RunTimeToEmpty (0x11) also uses 0xFFFF when battery is charging
 *
 * @see k_sbs_average_time_to_full SBS register address
 * @see k_sbs_run_time_to_empty SBS register for time to empty
 * @see rx_bq4050_read_status() Function that checks this value
 *
 * @since Version 1.0.0
 */
static const uint16_t s_bq4050_time_to_full_invalid = 0xFFFF;

#ifdef __cplusplus
}
#endif
