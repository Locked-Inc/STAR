/* lib/rx_bq4050/inc/rx_bq4050.h */

/**
 * @file rx_bq4050.h
 * @brief Texas Instruments BQ4050 Battery Fuel Gauge Driver API
 *
 * @details
 * High-level driver interface for the Texas Instruments BQ4050RSMR battery
 * fuel gauge IC. The BQ4050 is a fully integrated battery management solution
 * for 1-4 series Li-Ion/Li-Polymer battery packs with advanced gas gauging,
 * cell balancing, and protection features.
 *
 * **BQ4050 Key Features:**
 * - CEDV (Compensated End-of-Discharge Voltage) gas gauging algorithm
 * - Accurate state of charge estimation (±1% typical at 25°C)
 * - Individual cell voltage monitoring and active balancing (1-4 cells)
 * - Integrated battery protection (OVP, UVP, OCP, OTP, SCP)
 * - SHA-1/HMAC authentication for battery identification
 * - Non-volatile data flash for configuration storage (512 bytes)
 * - Temperature monitoring via internal thermistor
 * - Cycle count and battery health tracking
 *
 * **Communication Protocol:**
 * - Interface: SMBus v1.1/v2.0 (System Management Bus, I2C compatible)
 * - Default I2C Address: 0x0B (7-bit)
 * - Speed: Supports up to 400 kHz (Fast Mode I2C)
 * - Data: Implements Smart Battery System (SBS) 1.1 specification
 * - Error Detection: Optional Packet Error Checking (PEC) using CRC-8
 *
 * **Driver Architecture:**
 * This driver provides abstracted access to BQ4050 battery telemetry via
 * the bus manager infrastructure. All communication is performed through
 * rx_bus_manager and rx_bus_smbus layers for hardware independence.
 *
 * **Design Principles:**
 * - Zero dynamic memory allocation (safety-critical embedded)
 * - All register reads via SMBus word protocol (16-bit)
 * - Comprehensive error checking on all bus operations
 * - Temperature conversion with validated bounds checking
 * - State of charge clamping to prevent display anomalies
 *
 * **Typical Usage Flow:**
 * 1. Initialize SMBus and verify BQ4050 communication
 * 2. Read individual parameters or complete status structure
 * 3. Monitor battery state in periodic telemetry loop (1-10 Hz)
 * 4. React to alarm flags (low capacity, temperature, etc.)
 *
 * @par Hardware Requirements:
 * | Component | Specification |
 * |-----------|---------------|
 * | IC | Texas Instruments BQ4050RSMR |
 * | Package | 32-pin TSSOP |
 * | Protocol | SMBus v1.1/v2.0 |
 * | I2C Address | 0x0B (7-bit, fixed) |
 * | I2C Speed | 10-400 kHz |
 * | Supply Voltage | 2.7V - 5.5V (I/O compatible with 3.3V/5V) |
 * | Battery Config | 1-4 series Li-Ion/Li-Polymer cells |
 * | Temperature Range | -20°C to +70°C (operational) |
 *
 * @par STAR Hardware Configuration:
 * - Battery Pack: 4S Li-Ion (14.8V nominal, 16.8V max)
 * - Capacity: TBD mAh (configured in BQ4050 data flash)
 * - SMBus: Connected via I2C peripheral (RIIC0/RIIC1/RIIC2)
 * - Pull-ups: 4.7kΩ on SDA/SCL lines
 * - Thermistor: 10kΩ NTC (Beta=3380K)
 *
 * @par Module Dependencies:
 * - rx_err.h: Error code definitions
 * - rx_bus_manager.h: Bus manager for SMBus access
 * - rx_bus_smbus.h: SMBus protocol implementation
 * - rx_bq4050_constants.h: SBS command codes and status flags
 * - rx_check.h: Parameter validation macros
 * - rx_log.h: Logging infrastructure
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp/longjmp, or recursion
 * - Rule 2: [OK] All loops have statically provable bounds (fixed cell count iterations)
 * - Rule 3: [OK] No dynamic memory allocation (all structures static or stack)
 * - Rule 4: [OK] Functions < 60 lines (largest is ~50 lines)
 * - Rule 5: [OK] Minimum 2 assertions per function (NULL checks, range validation)
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] All SMBus return values checked
 * - Rule 8: [OK] C23 typed enums for all constants (no macros)
 * - Rule 9: [OK] Single-level pointer dereferencing only
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility:** BQ4050 driver handles ONLY fuel gauge communication
 * - **Open/Closed:** Extensible via bus manager interface, closed for modification
 * - **Liskov Substitution:** Works with any bus manager implementing SMBus interface
 * - **Interface Segregation:** Clean separation: init, read individual, read all
 * - **Dependency Inversion:** Depends on bus manager abstraction, not I2C hardware
 *
 * @par Thread Safety:
 * - **NOT thread-safe:** Caller must provide synchronization for concurrent access
 * - **Recommendation:** Use ThreadX mutex for multi-threaded battery monitoring
 * - **SMBus transactions:** Atomic at bus manager level (transaction boundaries protected)
 *
 * @par Performance Characteristics:
 * - Initialization: ~10-50 ms (depends on SMBus speed and verification reads)
 * - Single register read: ~1-5 ms @ 100 kHz SMBus, ~0.3-1.5 ms @ 400 kHz
 * - Full status read: ~50-150 ms (14 SMBus transactions)
 * - Recommended polling rate: 1-10 Hz (battery state changes slowly)
 *
 * @see BQ4050 Data Sheet (Texas Instruments SLUUAQ3A)
 * @see BQ4050 Technical Reference Manual (SLUUAQ3A)
 * @see Smart Battery Data Specification v1.1 (SBS-IF)
 * @see rx_bq4050_constants.h SBS command codes and status flag definitions
 * @see rx_bus_smbus.h SMBus protocol layer
 * @see rx_bus_manager.h Bus manager interface
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright MIT License
 *
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum bq4050_i2c_constants_t
 * @brief BQ4050 I2C/SMBus addressing constants
 *
 * @details
 * The BQ4050 has a fixed 7-bit I2C address of 0x0B that cannot be changed.
 * This address is defined by hardware and programmed into the device ROM.
 *
 * @par I2C Address Format:
 * - 7-bit address: 0x0B (0b0001011)
 * - 8-bit write address: 0x16 (0x0B << 1 | 0)
 * - 8-bit read address: 0x17 (0x0B << 1 | 1)
 *
 * @note The BQ4050 does not support address configuration - all BQ4050 devices
 *       on the same I2C bus will conflict. Multi-battery systems require
 *       I2C multiplexing or separate I2C buses.
 *
 * @see Smart Battery Data Specification v1.1 section 3.1.2 (Smart Battery Address)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief BQ4050 default I2C address (7-bit): 0x0B
   * @details
   * Fixed I2C address for all BQ4050 devices. Cannot be changed.
   * **Value:** 0x0B (11 decimal, 0b0001011 binary)
   * **Type:** 7-bit I2C address
   * **Write Transaction:** 0x16 (0x0B << 1 | 0)
   * **Read Transaction:** 0x17 (0x0B << 1 | 1)
   */
  k_bq4050_i2c_addr = 0x0B,
} bq4050_i2c_constants_t;

/**
 * @enum bq4050_cell_constants_t
 * @brief BQ4050 battery cell configuration constants
 *
 * @details
 * The BQ4050 supports 1-4 series-connected Li-Ion/Li-Polymer cells.
 * The actual number of cells is configured in the BQ4050's non-volatile
 * data flash during battery pack manufacturing.
 *
 * **Cell Voltage Monitoring:**
 * - Each cell monitored individually for voltage and balance status
 * - Cell voltage registers: 0x3F (Cell 1) through 0x3C (Cell 4)
 * - Balancing: Active cell balancing via internal FETs (100mA typical)
 *
 * @par Typical Cell Configurations:
 * - 1S: 3.7V nominal (e.g., single-cell smartphone battery)
 * - 2S: 7.4V nominal (e.g., camera batteries)
 * - 3S: 11.1V nominal (e.g., laptop batteries)
 * - 4S: 14.8V nominal (e.g., power tool batteries, STAR robot)
 *
 * @see BQ4050 Technical Reference section 7.3.7 (Cell Voltage Registers)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Maximum number of battery cells supported (4 series cells)
   * @details
   * BQ4050 can monitor up to 4 series-connected cells individually.
   * **Value:** 4
   * **Range:** 1-4 cells
   * **Typical for STAR:** 4S configuration (14.8V nominal)
   * **Voltage per cell:** 3.0V (empty) to 4.2V (full) for Li-Ion
   */
  k_bq4050_max_cells = 4,
} bq4050_cell_constants_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @struct rx_bq4050_status_t
 * @brief Complete battery status and telemetry structure
 *
 * @details
 * Aggregates all battery telemetry data from the BQ4050 fuel gauge into a
 * single structure. This includes electrical parameters (voltage, current),
 * state estimation (SOC, capacity), thermal data, timing estimates, and
 * status flags.
 *
 * **Update Frequency:**
 * - BQ4050 internal updates: ~250ms for electrical, ~1s for SOC/capacity
 * - Recommended read rate: 1-10 Hz (battery state changes slowly)
 *
 * **Data Freshness:**
 * All fields represent the most recent BQ4050 measurements at the time of
 * the SMBus read. Values may be up to 250ms-1s old depending on BQ4050
 * internal update rates.
 *
 * @par Memory Layout:
 * | Offset | Size | Field | Type | Alignment |
 * |--------|------|-------|------|-----------|
 * | 0 | 2 | voltage_mv | uint16_t | 2 |
 * | 2 | 8 | cell_voltages_mv[4] | uint16_t[4] | 2 |
 * | 10 | 2 | current_ma | int16_t | 2 |
 * | 12 | 2 | average_current_ma | int16_t | 2 |
 * | 14 | 1 | relative_soc | uint8_t | 1 |
 * | 15 | 1 | absolute_soc | uint8_t | 1 |
 * | 16 | 2 | temperature_c | int16_t | 2 |
 * | 18 | 2 | remaining_capacity_mah | uint16_t | 2 |
 * | 20 | 2 | full_capacity_mah | uint16_t | 2 |
 * | 22 | 2 | design_capacity_mah | uint16_t | 2 |
 * | 24 | 2 | cycle_count | uint16_t | 2 |
 * | 26 | 2 | time_to_empty_min | uint16_t | 2 |
 * | 28 | 2 | time_to_full_min | uint16_t | 2 |
 * | 30 | 1 | is_charging | bool | 1 |
 * | 31 | 1 | is_fully_charged | bool | 1 |
 * | 32 | 1 | is_fully_discharged | bool | 1 |
 * | 33 | 1 | is_low_capacity | bool | 1 |
 *
 * **Total Size:** ~34 bytes (compiler-dependent padding)
 *
 * @par Usage Example:
 * @code{.c}
 * // Read complete battery status
 * rx_bq4050_status_t battery_status;
 * rx_err_t err = rx_bq4050_read_status(bus_mgr, "smbus0", &battery_status, 4);
 *
 * if (err == k_rx_ok) {
 *     // Log battery telemetry
 *     rx_log_info_val("Battery", "SOC (%)", battery_status.relative_soc);
 *     rx_log_info_val("Battery", "Voltage (mV)", battery_status.voltage_mv);
 *     rx_log_info_val("Battery", "Current (mA)", battery_status.current_ma);
 *     rx_log_info_val("Battery", "Temp (C)", battery_status.temperature_c);
 *
 *     // Check for critical conditions
 *     if (battery_status.is_low_capacity) {
 *         rx_log_warn("Battery", "Low battery warning - connect charger");
 *     }
 *
 *     if (battery_status.temperature_c > 45) {
 *         rx_log_error("Battery", "Over-temperature - stop charging");
 *     }
 * }
 * @endcode
 *
 * @see rx_bq4050_read_status() Function to populate this structure
 * @see BQ4050 Data Sheet section 7.3 (SBS Data Commands)
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Battery pack voltage in millivolts (sum of all cells)
   * @details
   * Total voltage across all series-connected cells. For a 4S pack,
   * this is the sum of all 4 cell voltages.
   * **Units:** Millivolts (mV)
   * **Range:** 0-65535 mV (0-65.535V)
   * **Typical (4S Li-Ion):** 12000-16800 mV (3.0V-4.2V per cell)
   * **Resolution:** 1 mV
   * **Source:** SBS register 0x09 (Voltage)
   * **Update Rate:** ~250 ms
   */
  uint16_t voltage_mv;

  /**
   * @brief Individual cell voltages in millivolts (array of 4 cells)
   * @details
   * Voltage of each cell in the battery pack. Indices correspond to:
   * - [0]: Cell 1 (bottom cell, closest to ground)
   * - [1]: Cell 2
   * - [2]: Cell 3
   * - [3]: Cell 4 (top cell, highest potential)
   * **Units:** Millivolts (mV)
   * **Range:** 0-65535 mV per cell
   * **Typical (Li-Ion):** 3000-4200 mV per cell
   * **Resolution:** 1 mV per cell
   * **Source:** SBS registers 0x3F-0x3C (BQ4050 extension)
   * **Update Rate:** ~250 ms
   * **Note:** Only num_cells entries are valid (rest are 0 or stale)
   * **Balancing:** BQ4050 actively balances cells >50mV imbalance
   */
  uint16_t cell_voltages_mv[k_bq4050_max_cells];

  /**
   * @brief Instantaneous battery current in milliamps
   * @details
   * Current flow in/out of battery measured by BQ4050 sense resistor.
   * **Units:** Milliamps (mA)
   * **Range:** -32768 to +32767 mA
   * **Sign Convention:** Positive = charging, Negative = discharging
   * **Resolution:** 1 mA (typical), depends on sense resistor value
   * **Source:** SBS register 0x0A (Current)
   * **Update Rate:** ~250 ms
   * **Accuracy:** ±1% typical (±3% max) at room temperature
   * **Note:** Noisy - use average_current_ma for stable readings
   */
  int16_t current_ma;

  /**
   * @brief 1-minute rolling average battery current in milliamps
   * @details
   * Rolling average of battery current over the past 60 seconds.
   * Used by BQ4050 for SOC calculations and runtime estimates.
   * **Units:** Milliamps (mA)
   * **Range:** -32768 to +32767 mA
   * **Sign Convention:** Positive = charging, Negative = discharging
   * **Averaging Window:** 60 seconds (rolling)
   * **Resolution:** 1 mA
   * **Source:** SBS register 0x0B (AverageCurrent)
   * **Update Rate:** ~1 second
   * **Use Case:** Preferred over current_ma for runtime estimates
   */
  int16_t average_current_ma;

  /**
   * @brief Relative state of charge (0-100%, accounts for aging)
   * @details
   * SOC as percentage of current full charge capacity. Recommended for
   * user display as it accounts for battery aging.
   * **Units:** Percent (%)
   * **Range:** 0-100%
   * **Resolution:** 1%
   * **Source:** SBS register 0x0D (RelativeStateOfCharge)
   * **Update Rate:** ~1 second
   * **Algorithm:** CEDV (Compensated End-of-Discharge Voltage)
   * **Accuracy:** ±1% typical after learning cycle
   * **Note:** Clamped to 0-100% by driver (SBS allows >100%)
   * **Typical Use:** Battery icon percentage, "% remaining"
   */
  uint8_t relative_soc;

  /**
   * @brief Absolute state of charge (0-100%, vs design capacity)
   * @details
   * SOC as percentage of original design capacity. Does NOT account
   * for aging - used for battery health assessment.
   * **Units:** Percent (%)
   * **Range:** 0-100%
   * **Resolution:** 1%
   * **Source:** SBS register 0x0E (AbsoluteStateOfCharge)
   * **Update Rate:** ~1 second
   * **Typical Use:** Battery health monitoring
   * **Relationship:** absolute_soc < relative_soc indicates aging
   */
  uint8_t absolute_soc;

  /**
   * @brief Battery pack temperature in degrees Celsius
   * @details
   * Temperature measured by thermistor in battery pack. Critical for
   * charge/discharge safety and performance optimization.
   * **Units:** Degrees Celsius (°C)
   * **Range:** -273 to +6280°C (full int16_t range after conversion)
   * **Typical Range:** -20 to +70°C (operational)
   * **Resolution:** 1°C (after conversion from 0.1K SBS format)
   * **Source:** SBS register 0x08 (Temperature)
   * **Update Rate:** ~1 second
   * **Sensor:** 10kΩ NTC thermistor (Beta=3380K typical)
   * **Critical Thresholds:**
   * - Charge: 0-45°C (stop charging outside this range)
   * - Discharge: -20-60°C (performance degrades at extremes)
   */
  int16_t temperature_c;

  /**
   * @brief Remaining battery capacity in milliamp-hours
   * @details
   * Current remaining energy in the battery pack. Decreases during
   * discharge, increases during charge.
   * **Units:** Milliamp-hours (mAh)
   * **Range:** 0-65535 mAh
   * **Resolution:** 1 mAh
   * **Source:** SBS register 0x0F (RemainingCapacity)
   * **Update Rate:** ~1 second
   * **Relationship:** remaining = (relative_soc / 100) * full_capacity
   * **Use Case:** Absolute energy remaining for load planning
   */
  uint16_t remaining_capacity_mah;

  /**
   * @brief Current full charge capacity accounting for aging (mAh)
   * @details
   * Maximum capacity battery can hold in current aged state. Decreases
   * over battery lifetime as cells degrade.
   * **Units:** Milliamp-hours (mAh)
   * **Range:** 0-65535 mAh
   * **Resolution:** 1 mAh
   * **Source:** SBS register 0x10 (FullChargeCapacity)
   * **Update Rate:** Updated after each full charge cycle
   * **Typical Degradation:** 80% of design capacity after 500 cycles
   * **Relationship:** full_capacity ≤ design_capacity (aging)
   * **Use Case:** Battery health monitoring, SOC calculations
   */
  uint16_t full_capacity_mah;

  /**
   * @brief Original design capacity when battery was new (mAh)
   * @details
   * Factory-programmed capacity of new battery. Constant throughout
   * battery life - used as reference for health calculations.
   * **Units:** Milliamp-hours (mAh)
   * **Range:** 0-65535 mAh
   * **Resolution:** 1 mAh
   * **Source:** SBS register 0x18 (DesignCapacity)
   * **Update Rate:** Constant (read-only)
   * **Programmed:** During battery pack manufacturing
   * **Relationship:** Constant reference, full_capacity ≤ design_capacity
   * **Use Case:** Calculate battery health: (full/design) * 100%
   */
  uint16_t design_capacity_mah;

  /**
   * @brief Number of charge/discharge cycles completed
   * @details
   * Accumulated charge/discharge cycles. One cycle = 100% of capacity
   * discharged (may span multiple partial charge/discharge events).
   * **Units:** Cycle count
   * **Range:** 0-65535 cycles
   * **Resolution:** 1 cycle
   * **Source:** SBS register 0x17 (CycleCount)
   * **Update Rate:** Updated after each full cycle
   * **Definition:** 1 cycle = cumulative 100% discharge
   * **Typical Lifespan:** 500-1000 cycles to 80% capacity (Li-Ion)
   * **Use Case:** Battery lifetime tracking, warranty claims
   */
  uint16_t cycle_count;

  /**
   * @brief Estimated time to empty in minutes (0xFFFF if not discharging)
   * @details
   * Time until battery reaches 0% SOC at current discharge rate.
   * **Units:** Minutes
   * **Range:** 0-65534 minutes, 0xFFFF = invalid/not applicable
   * **Resolution:** 1 minute
   * **Source:** SBS register 0x11 (RunTimeToEmpty)
   * **Update Rate:** ~10 seconds
   * **Algorithm:** remaining_capacity / abs(average_current) * 60
   * **Invalid When:** Charging, idle, or SOC increasing
   * **Note:** Assumes constant discharge current (estimate only)
   */
  uint16_t time_to_empty_min;

  /**
   * @brief Estimated time to full charge in minutes (0xFFFF if not charging)
   * @details
   * Time until battery reaches 100% SOC at current charge rate.
   * **Units:** Minutes
   * **Range:** 0-65534 minutes, 0xFFFF = invalid/not applicable
   * **Resolution:** 1 minute
   * **Source:** SBS register 0x13 (AverageTimeToFull)
   * **Update Rate:** ~10 seconds
   * **Algorithm:** (full_capacity - remaining) / average_current * 60
   * **Invalid When:** Discharging, idle, or SOC decreasing
   * **Note:** Assumes constant charge current (estimate only)
   */
  uint16_t time_to_full_min;

  /**
   * @brief True if battery is currently charging (positive current)
   * @details
   * Charging state derived from BatteryStatus DISCHARGING flag (inverted).
   * **Type:** Boolean
   * **Values:** true = charging, false = discharging or idle
   * **Source:** SBS register 0x16 bit 6 (DISCHARGING, inverted)
   * **Update Rate:** ~1 second
   * **Logic:** is_charging = !(status_flags & DISCHARGING_BIT)
   */
  bool is_charging;

  /**
   * @brief True if battery has reached full charge
   * @details
   * Set when charge termination conditions met (voltage + current thresholds).
   * **Type:** Boolean
   * **Values:** true = fully charged, false = not fully charged
   * **Source:** SBS register 0x16 bit 5 (FULLY_CHARGED)
   * **Update Rate:** ~1 second
   * **Conditions:** Voltage at charge limit, current < termination threshold
   * **Note:** May be set even if relative_soc < 100% due to gauging
   */
  bool is_fully_charged;

  /**
   * @brief True if battery has reached full discharge (empty)
   * @details
   * Set when discharge cutoff voltage reached (battery protection active).
   * **Type:** Boolean
   * **Values:** true = fully discharged, false = not empty
   * **Source:** SBS register 0x16 bit 4 (FULLY_DISCHARGED)
   * **Update Rate:** ~1 second
   * **Conditions:** Voltage at minimum safe voltage, SOC at 0%
   * **Action:** Disconnect load to prevent battery damage
   */
  bool is_fully_discharged;

  /**
   * @brief True if remaining capacity alarm triggered (low battery)
   * @details
   * Warning flag set when remaining capacity drops below configured threshold.
   * **Type:** Boolean
   * **Values:** true = low capacity alarm, false = normal
   * **Source:** SBS register 0x16 bit 9 (REMAINING_CAPACITY_ALARM)
   * **Update Rate:** ~1 second
   * **Typical Threshold:** 10-20% SOC (configurable in BQ4050 data flash)
   * **Action:** Warn user, reduce power consumption, initiate shutdown
   */
  bool is_low_capacity;

  /**
   * @brief Raw BatteryStatus register value (0x16)
   * @details
   * 16-bit status flags from SBS BatteryStatus register.
   * Use bq4050_status_flags_t masks to decode individual flags.
   * **Source:** SBS register 0x16
   * **Format:** 16-bit flags (see bq4050_status_flags_t)
   * **Update Rate:** ~1 second
   */
  uint16_t battery_status;
} rx_bq4050_status_t;

/**
 * @struct rx_bq4050_config_t
 * @brief BQ4050 initialization configuration structure
 *
 * @details
 * Configuration parameters for BQ4050 driver initialization. Most BQ4050
 * parameters are stored in non-volatile data flash and programmed during
 * battery pack manufacturing, so minimal runtime configuration is needed.
 *
 * **Data Flash Parameters (Configured at Manufacturing):**
 * - Design capacity, design voltage
 * - Charge/discharge termination voltages
 * - Overcurrent/overtemperature protection thresholds
 * - Cell balancing thresholds
 * - Thermistor Beta coefficient
 * - Cycle count limits and logging
 *
 * **Runtime Configuration (This Structure):**
 * - Number of cells (must match physical battery configuration)
 *
 * @par Usage Example:
 * @code{.c}
 * // Initialize BQ4050 for 4S battery pack
 * rx_bq4050_config_t config = {
 *     .num_cells = 4,  // 4 series cells (14.8V nominal)
 * };
 *
 * rx_err_t err = rx_bq4050_init(bus_mgr, "smbus0", &config);
 * if (err != k_rx_ok) {
 *     rx_log_error("Init", "BQ4050 initialization failed");
 * }
 * @endcode
 *
 * @note Future expansion: Additional runtime parameters may be added (e.g.,
 *       polling rate, alarm thresholds, logging verbosity).
 *
 * @see rx_bq4050_init() Initialization function using this configuration
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Number of cells in series (1-4)
   * @details
   * Number of series-connected cells in the battery pack. Must match
   * the physical battery configuration and BQ4050 data flash settings.
   * **Range:** 1-4 cells
   * **Typical for STAR:** 4 (4S Li-Ion, 14.8V nominal)
   * **Validation:** Driver will validate against k_bq4050_max_cells
   * **Note:** Mismatch with physical cells causes incorrect voltage readings
   */
  uint8_t num_cells;
} rx_bq4050_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize BQ4050 battery fuel gauge and verify communication
 *
 * @details
 * Initializes the BQ4050 battery fuel gauge by:
 * 1. Initializing the SMBus interface via bus manager
 * 2. Verifying communication by reading battery pack voltage
 * 3. Validating BQ4050 is present and responding at address 0x0B
 *
 * The BQ4050 is pre-configured via non-volatile data flash during battery
 * pack manufacturing, so minimal initialization is required. This function
 * primarily verifies hardware connectivity.
 *
 * **Algorithm Steps:**
 * 1. Validate input parameters (NULL pointer checks)
 * 2. Call rx_bus_smbus_init() to initialize SMBus peripheral
 * 3. Read voltage register (0x09) as communication test
 * 4. If read succeeds, BQ4050 is confirmed present and operational
 * 5. Log initialization status (success/failure with address)
 *
 * @param[in] manager Bus manager instance for SMBus communication
 *   - **Must not be nullptr**
 *   - **Must be initialized** via rx_bus_manager_init()
 *   - **Lifetime:** Must remain valid for all subsequent BQ4050 operations
 *
 * @param[in] bus_name SMBus bus name string (e.g., "smbus0", "riic0")
 *   - **Must not be nullptr**
 *   - **Must be null-terminated** C string
 *   - **Must exist** in bus manager's bus registry
 *   - **Must be SMBus-capable** (not plain I2C)
 *   - **Typical values:** "smbus0", "riic0", "riic1"
 *
 * @param[in] config BQ4050 configuration structure (optional, can be NULL)
 *   - **Can be NULL** for default configuration
 *   - **If provided:** num_cells must be 1-4
 *   - **Currently unused** - reserved for future configuration options
 *   - **Data flash config** takes precedence (programmed at manufacturing)
 *
 * @return rx_err_t Error code indicating initialization result
 * @retval k_rx_ok Success - BQ4050 initialized and responding
 * @retval k_rx_err_null_ptr NULL pointer in manager or bus_name
 * @retval k_rx_err_not_found SMBus bus not found in bus manager
 * @retval k_rx_err_invalid_state SMBus bus exists but not initialized
 * @retval k_rx_err_timeout SMBus communication timeout (no ACK, bus hung)
 * @retval k_rx_err_nack BQ4050 not responding at address 0x0B (not present/powered)
 * @retval k_rx_err_crc_mismatch PEC (Packet Error Check) failure if enabled
 *
 * @pre Bus manager must be initialized via rx_bus_manager_init()
 * @pre SMBus peripheral hardware must be configured and enabled
 * @pre BQ4050 must be powered (2.7V-5.5V supply)
 * @pre I2C pull-up resistors must be present on SDA/SCL (4.7kΩ typical)
 *
 * @post SMBus bus is initialized and ready for BQ4050 transactions
 * @post BQ4050 communication verified (voltage register read successful)
 * @post Initialization status logged (info on success, error on failure)
 *
 * @note Thread Safety: NOT thread-safe - caller must serialize init calls
 * @note Re-entrancy: Can be called multiple times (SMBus init is idempotent)
 * @note Performance: ~10-50ms execution time (depends on SMBus speed)
 *
 * @warning Do not call from interrupt context (performs blocking SMBus I/O)
 * @warning Ensure BQ4050 is powered before calling (check VBAT present)
 *
 * @par Example - Basic Initialization:
 * @code{.c}
 * // Initialize BQ4050 with default configuration
 * rx_err_t err = rx_bq4050_init(bus_manager, "smbus0", nullptr);
 *
 * if (err == k_rx_ok) {
 *     rx_log_info("Battery", "BQ4050 initialized successfully");
 * } else {
 *     rx_log_error("Battery", "BQ4050 init failed - check hardware");
 *     return err;
 * }
 * @endcode
 *
 * @par Example - With Configuration:
 * @code{.c}
 * // Initialize BQ4050 for 4S battery pack
 * rx_bq4050_config_t config = {
 *     .num_cells = 4,  // 4 series Li-Ion cells
 * };
 *
 * rx_err_t err = rx_bq4050_init(bus_manager, "riic0", &config);
 *
 * if (err != k_rx_ok) {
 *     rx_log_error("Battery", "Failed to initialize BQ4050");
 *     rx_log_error_val("Battery", "Error code", (uint32_t)err);
 *
 *     // Handle specific errors
 *     if (err == k_rx_err_nack) {
 *         rx_log_error("Battery", "BQ4050 not detected - check power/connections");
 *     } else if (err == k_rx_err_timeout) {
 *         rx_log_error("Battery", "SMBus timeout - check pull-ups and bus speed");
 *     }
 *
 *     return err;
 * }
 * @endcode
 *
 * @par Example - Error Handling:
 * @code{.c}
 * rx_err_t err = rx_bq4050_init(bus_manager, "smbus0", nullptr);
 *
 * switch (err) {
 *     case k_rx_ok:
 *         // Continue with battery monitoring
 *         break;
 *
 *     case k_rx_err_nack:
 *         rx_log_error("Battery", "BQ4050 not present on SMBus");
 *         // Check: Power supply, physical connections, I2C pull-ups
 *         break;
 *
 *     case k_rx_err_timeout:
 *         rx_log_error("Battery", "SMBus communication timeout");
 *         // Check: Bus speed (100kHz/400kHz), pull-up values (4.7kΩ)
 *         break;
 *
 *     case k_rx_err_not_found:
 *         rx_log_error("Battery", "SMBus peripheral not found");
 *         // Check: bus_name string, bus manager configuration
 *         break;
 *
 *     default:
 *         rx_log_error_val("Battery", "Unexpected error", (uint32_t)err);
 *         break;
 * }
 * @endcode
 *
 * @see rx_bus_smbus_init() SMBus initialization function
 * @see rx_bq4050_read_voltage() Voltage read used for communication test
 * @see rx_bq4050_config_t Configuration structure definition
 * @see BQ4050 Data Sheet section 7.2 (SMBus Interface)
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_bq4050_init(rx_bus_manager_t* manager, const char* bus_name, const rx_bq4050_config_t* config);

/**
 * @brief Read total battery pack voltage
 *
 * @details
 * Reads the total battery pack voltage from SBS Voltage register (0x09).
 * This is the sum of all series-connected cell voltages.
 *
 * For a 4S pack: voltage_mv = cell1_mv + cell2_mv + cell3_mv + cell4_mv
 *
 * **Algorithm:**
 * 1. Validate parameters (NULL pointer checks)
 * 2. Execute SMBus Read Word transaction to register 0x09
 * 3. Return 16-bit unsigned voltage value in millivolts
 *
 * @param[in] manager Bus manager instance
 *   - **Must not be nullptr**
 *   - **Must be initialized**
 *
 * @param[in] bus_name SMBus bus name
 *   - **Must not be nullptr**
 *   - **Must be null-terminated**
 *   - **Must exist** in bus manager
 *
 * @param[out] voltage_mv Pointer to store pack voltage in millivolts
 *   - **Must not be nullptr**
 *   - **Modified only on success** (k_rx_ok)
 *   - **Typical range (4S):** 12000-16800 mV (3.0V-4.2V per cell)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, voltage_mv contains valid voltage
 * @retval k_rx_err_null_ptr NULL pointer in manager, bus_name, or voltage_mv
 * @retval k_rx_err_not_found SMBus bus not found
 * @retval k_rx_err_invalid_state SMBus bus not initialized
 * @retval k_rx_err_timeout SMBus timeout (no response from BQ4050)
 * @retval k_rx_err_crc_mismatch PEC check failed (if enabled)
 * @retval k_rx_err_nack BQ4050 NACK (device not present or busy)
 *
 * @pre rx_bq4050_init() must have been called successfully
 * @pre SMBus bus must be in idle state (no ongoing transactions)
 *
 * @post voltage_mv contains pack voltage on success
 * @post SMBus bus returned to idle state
 *
 * @note Thread Safety: NOT thread-safe, caller must serialize access
 * @note Performance: ~1-5ms @ 100kHz, ~0.3-1.5ms @ 400kHz
 * @note Update Rate: BQ4050 updates voltage every ~250ms
 *
 * @par Example:
 * @code{.c}
 * uint16_t pack_voltage_mv;
 * rx_err_t err = rx_bq4050_read_voltage(bus_mgr, "smbus0", &pack_voltage_mv);
 *
 * if (err == k_rx_ok) {
 *     rx_log_info_val("Battery", "Pack voltage (mV)", pack_voltage_mv);
 *
 *     // Convert to volts for display
 *     float pack_voltage_v = pack_voltage_mv / 1000.0f;
 *     rx_log_info("Battery", "Pack voltage (V): %.2f", pack_voltage_v);
 *
 *     // Check voltage thresholds
 *     const uint16_t k_min_voltage_mv = 12000;  // 3.0V per cell (4S)
 *     const uint16_t k_max_voltage_mv = 16800;  // 4.2V per cell (4S)
 *
 *     if (pack_voltage_mv < k_min_voltage_mv) {
 *         rx_log_warn("Battery", "Pack voltage critically low!");
 *     } else if (pack_voltage_mv > k_max_voltage_mv) {
 *         rx_log_error("Battery", "Pack voltage over limit - stop charging!");
 *     }
 * }
 * @endcode
 *
 * @see rx_bq4050_read_cell_voltages() Read individual cell voltages
 * @see k_sbs_voltage SBS register address (0x09)
 * @see BQ4050 Data Sheet section 7.3.3 (Voltage)
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_bq4050_read_voltage(rx_bus_manager_t* manager, const char* bus_name, uint16_t* voltage_mv);

/**
 * @brief Read individual battery cell voltages
 *
 * @details
 * Reads voltages for each individual cell in the battery pack from BQ4050-
 * specific cell voltage registers (0x3F-0x3C). This is a Texas Instruments
 * manufacturer extension beyond the standard SBS specification.
 *
 * Cell voltage monitoring is essential for:
 * - Detecting cell imbalance (>50mV difference triggers balancing)
 * - Identifying weak/failed cells
 * - Early detection of battery degradation
 * - Safety monitoring (individual cell over/under voltage)
 *
 * **Cell Mapping:**
 * - cell_voltages[0] = Cell 1 (register 0x3F, bottom cell, closest to ground)
 * - cell_voltages[1] = Cell 2 (register 0x3E)
 * - cell_voltages[2] = Cell 3 (register 0x3D)
 * - cell_voltages[3] = Cell 4 (register 0x3C, top cell, highest potential)
 *
 * **Algorithm:**
 * 1. Validate parameters (NULL pointers, num_cells range)
 * 2. Loop through cell registers (0x3F down to 0x3C)
 * 3. For each cell (up to num_cells), execute SMBus Read Word
 * 4. Store voltage in corresponding cell_voltages[] array index
 * 5. Return error immediately if any read fails
 *
 * @param[in] manager Bus manager instance
 *   - **Must not be nullptr**
 *
 * @param[in] bus_name SMBus bus name
 *   - **Must not be nullptr**
 *
 * @param[out] cell_voltages Array to store cell voltages (must have 4 elements)
 *   - **Must not be nullptr**
 *   - **Size:** Must accommodate k_bq4050_max_cells (4) elements
 *   - **Only first num_cells entries written** on success
 *   - **Units:** Millivolts (mV)
 *   - **Typical range:** 3000-4200 mV per cell (Li-Ion)
 *
 * @param[in] num_cells Number of cells to read (1-4)
 *   - **Range:** 1 to k_bq4050_max_cells (4)
 *   - **Must match** physical battery configuration
 *   - **Validation:** Checked against k_bq4050_max_cells
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, cell_voltages[] populated
 * @retval k_rx_err_null_ptr NULL pointer in manager, bus_name, or cell_voltages
 * @retval k_rx_err_invalid_arg num_cells < 1 or > k_bq4050_max_cells
 * @retval k_rx_err_not_found SMBus bus not found
 * @retval k_rx_err_invalid_state SMBus not initialized
 * @retval k_rx_err_timeout SMBus timeout on any cell read
 * @retval k_rx_err_crc_mismatch PEC check failed on any cell read
 *
 * @pre rx_bq4050_init() called successfully
 * @pre num_cells must match actual battery cell count
 *
 * @post First num_cells entries of cell_voltages[] contain valid voltages on success
 * @post Remaining entries (if num_cells < 4) are unchanged
 * @post All SMBus transactions completed (or first failure returned)
 *
 * @note Thread Safety: NOT thread-safe
 * @note Performance: ~4-20ms for 4 cells (4 sequential SMBus reads)
 * @note Update Rate: BQ4050 updates cell voltages every ~250ms
 *
 * @warning Stop loop on first error - partial results may be stale
 * @warning Only valid for num_cells - do not read beyond configured cell count
 *
 * @par Example - Read All Cells:
 * @code{.c}
 * uint16_t cell_voltages_mv[k_bq4050_max_cells];
 * const uint8_t num_cells = 4;  // 4S battery pack
 *
 * rx_err_t err = rx_bq4050_read_cell_voltages(bus_mgr, "smbus0",
 *                                               cell_voltages_mv, num_cells);
 *
 * if (err == k_rx_ok) {
 *     // Log all cell voltages
 *     for (uint8_t i = 0; i < num_cells; i++) {
 *         rx_log_info("Battery", "Cell %u: %u mV", i + 1, cell_voltages_mv[i]);
 *     }
 *
 *     // Calculate cell imbalance
 *     uint16_t min_cell = cell_voltages_mv[0];
 *     uint16_t max_cell = cell_voltages_mv[0];
 *     for (uint8_t i = 1; i < num_cells; i++) {
 *         if (cell_voltages_mv[i] < min_cell) min_cell = cell_voltages_mv[i];
 *         if (cell_voltages_mv[i] > max_cell) max_cell = cell_voltages_mv[i];
 *     }
 *     uint16_t imbalance_mv = max_cell - min_cell;
 *
 *     rx_log_info_val("Battery", "Cell imbalance (mV)", imbalance_mv);
 *
 *     // Check imbalance threshold
 *     const uint16_t k_max_imbalance_mv = 100;  // 100mV warning
 *     if (imbalance_mv > k_max_imbalance_mv) {
 *         rx_log_warn("Battery", "Excessive cell imbalance - check for weak cell");
 *     }
 * }
 * @endcode
 *
 * @par Example - Detect Weak Cell:
 * @code{.c}
 * uint16_t cells[4];
 * if (rx_bq4050_read_cell_voltages(bus_mgr, "smbus0", cells, 4) == k_rx_ok) {
 *     const uint16_t k_weak_cell_threshold_mv = 3000;  // 3.0V minimum
 *
 *     for (uint8_t i = 0; i < 4; i++) {
 *         if (cells[i] < k_weak_cell_threshold_mv) {
 *             rx_log_error("Battery", "Cell %u critically low: %u mV",
 *                          i + 1, cells[i]);
 *             // Initiate emergency shutdown
 *             motor_emergency_stop();
 *             return k_rx_err_battery_fault;
 *         }
 *     }
 * }
 * @endcode
 *
 * @see k_sbs_cell_voltage_1 Cell 1 register address (0x3F)
 * @see k_sbs_cell_voltage_2 Cell 2 register address (0x3E)
 * @see k_sbs_cell_voltage_3 Cell 3 register address (0x3D)
 * @see k_sbs_cell_voltage_4 Cell 4 register address (0x3C)
 * @see BQ4050 Technical Reference section 7.3.7 (Cell Voltage)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bq4050_read_cell_voltages(rx_bus_manager_t* manager,
                                                    const char*       bus_name,
                                                    uint16_t*         cell_voltages,
                                                    uint8_t           num_cells);

/**
 * @brief Read instantaneous battery current
 *
 * @details
 * Reads instantaneous battery current from SBS Current register (0x0A).
 * Current is measured by the BQ4050 via a precision sense resistor in
 * series with the battery pack.
 *
 * **Sign Convention:**
 * - Positive current (+): Battery is charging (current flowing into battery)
 * - Negative current (-): Battery is discharging (current flowing out)
 * - Zero current (0): Battery idle (no charge/discharge)
 *
 * **Measurement Characteristics:**
 * - Sense method: Voltage drop across precision sense resistor (5-20mΩ typical)
 * - ADC resolution: 14-16 bits
 * - Measurement accuracy: ±1% typical, ±3% max at 25°C
 * - Update rate: ~250ms (BQ4050 internal ADC conversion)
 * - Noise: Instantaneous reading can be noisy - use average_current for stability
 *
 * @param[in] manager Bus manager instance
 *   - **Must not be nullptr**
 *
 * @param[in] bus_name SMBus bus name
 *   - **Must not be nullptr**
 *
 * @param[out] current_ma Pointer to store current in milliamps
 *   - **Must not be nullptr**
 *   - **Range:** -32768 to +32767 mA
 *   - **Sign:** Positive = charging, Negative = discharging
 *   - **Modified only on success**
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, current_ma contains valid current
 * @retval k_rx_err_null_ptr NULL pointer in manager, bus_name, or current_ma
 * @retval k_rx_err_not_found SMBus bus not found
 * @retval k_rx_err_invalid_state SMBus not initialized
 * @retval k_rx_err_timeout SMBus timeout
 * @retval k_rx_err_crc_mismatch PEC check failed
 *
 * @pre rx_bq4050_init() called successfully
 *
 * @post current_ma contains instantaneous current on success
 *
 * @note Thread Safety: NOT thread-safe
 * @note Performance: ~1-5ms @ 100kHz SMBus
 * @note Noise: Consider using rx_bq4050_read_average_current() for stable readings
 *
 * @par Example:
 * @code{.c}
 * int16_t current_ma;
 * rx_err_t err = rx_bq4050_read_current(bus_mgr, "smbus0", &current_ma);
 *
 * if (err == k_rx_ok) {
 *     if (current_ma > 0) {
 *         rx_log_info_val("Battery", "Charging current (mA)", current_ma);
 *     } else if (current_ma < 0) {
 *         rx_log_info_val("Battery", "Discharge current (mA)", -current_ma);
 *     } else {
 *         rx_log_info("Battery", "Battery idle (no current flow)");
 *     }
 *
 *     // Check for overcurrent conditions
 *     const int16_t k_max_charge_current_ma = 2000;    // 2A charge limit
 *     const int16_t k_max_discharge_current_ma = -5000; // 5A discharge limit
 *
 *     if (current_ma > k_max_charge_current_ma) {
 *         rx_log_error("Battery", "Charge overcurrent detected!");
 *     } else if (current_ma < k_max_discharge_current_ma) {
 *         rx_log_error("Battery", "Discharge overcurrent detected!");
 *     }
 * }
 * @endcode
 *
 * @see rx_bq4050_read_average_current() Read 1-minute rolling average (preferred)
 * @see k_sbs_current SBS register address (0x0A)
 * @see BQ4050 Data Sheet section 7.3.4 (Current)
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_bq4050_read_current(rx_bus_manager_t* manager, const char* bus_name, int16_t* current_ma);

/**
 * @brief Read 1-minute rolling average battery current
 *
 * @details
 * Reads 1-minute rolling average battery current from SBS AverageCurrent
 * register (0x0B). This provides a smoothed current reading that is less
 * susceptible to transient spikes and is used by the BQ4050 for SOC
 * calculations and runtime estimates.
 *
 * **Averaging Algorithm:**
 * - Window: 60 seconds (rolling)
 * - Update: Every ~1 second (new sample added, oldest dropped)
 * - Filter: Simple moving average
 *
 * **Preferred Over Instantaneous Current For:**
 * - Runtime estimates (time to empty/full)
 * - SOC trending calculations
 * - Load profile analysis
 * - Charge/discharge rate monitoring
 *
 * @param[in] manager Bus manager instance
 *   - **Must not be nullptr**
 *
 * @param[in] bus_name SMBus bus name
 *   - **Must not be nullptr**
 *
 * @param[out] avg_current_ma Pointer to store average current in milliamps
 *   - **Must not be nullptr**
 *   - **Range:** -32768 to +32767 mA
 *   - **Sign:** Positive = charging average, Negative = discharging average
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, avg_current_ma contains valid average
 * @retval k_rx_err_null_ptr NULL pointer in manager, bus_name, or avg_current_ma
 * @retval k_rx_err_not_found SMBus bus not found
 * @retval k_rx_err_invalid_state SMBus not initialized
 * @retval k_rx_err_timeout SMBus timeout
 * @retval k_rx_err_crc_mismatch PEC check failed
 *
 * @pre rx_bq4050_init() called successfully
 *
 * @post avg_current_ma contains 1-minute average current on success
 *
 * @note Thread Safety: NOT thread-safe
 * @note Performance: ~1-5ms @ 100kHz SMBus
 * @note Warm-up: Average is valid after ~60 seconds of operation
 *
 * @par Example:
 * @code{.c}
 * int16_t avg_current_ma;
 * rx_err_t err = rx_bq4050_read_average_current(bus_mgr, "smbus0", &avg_current_ma);
 *
 * if (err == k_rx_ok) {
 *     rx_log_info_val("Battery", "Average current (mA)", avg_current_ma);
 *
 *     // Estimate power consumption
 *     uint16_t voltage_mv;
 *     if (rx_bq4050_read_voltage(bus_mgr, "smbus0", &voltage_mv) == k_rx_ok) {
 *         // Power = Voltage * Current (in watts)
 *         float power_w = (voltage_mv / 1000.0f) * (avg_current_ma / 1000.0f);
 *         rx_log_info("Battery", "Average power: %.2f W", power_w);
 *     }
 * }
 * @endcode
 *
 * @see rx_bq4050_read_current() Read instantaneous current (noisier)
 * @see k_sbs_average_current SBS register address (0x0B)
 * @see BQ4050 Data Sheet section 7.3.5 (AverageCurrent)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bq4050_read_average_current(rx_bus_manager_t* manager,
                                                      const char*       bus_name,
                                                      int16_t*          avg_current_ma);

/**
 * @brief Read relative state of charge (accounts for aging)
 *
 * @details
 * Reads RelativeStateOfCharge from SBS register (0x0D). This is the SOC
 * as a percentage of the current full charge capacity, accounting for
 * battery aging. **This is the recommended SOC for user display.**
 *
 * **CEDV Algorithm:**
 * The BQ4050 uses CEDV (Compensated End-of-Discharge Voltage) gas gauging:
 * - Tracks voltage, current, temperature
 * - Compensates for rate, temperature effects
 * - Learns battery characteristics over cycles
 * - Provides ±1% accuracy after learning cycle
 *
 * **Relative vs Absolute SOC:**
 * - Relative: SOC vs current full capacity (accounts for aging)
 * - Absolute: SOC vs original design capacity (does not account for aging)
 * - For aged battery: relative_soc > absolute_soc
 *
 * @param[in] manager Bus manager instance
 *   - **Must not be nullptr**
 *
 * @param[in] bus_name SMBus bus name
 *   - **Must not be nullptr**
 *
 * @param[out] soc_percent Pointer to store state of charge (0-100%)
 *   - **Must not be nullptr**
 *   - **Range:** 0-100%
 *   - **Resolution:** 1%
 *   - **Clamped:** Driver clamps to 0-100% (SBS spec allows >100%)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, soc_percent contains valid SOC
 * @retval k_rx_err_null_ptr NULL pointer in manager, bus_name, or soc_percent
 * @retval k_rx_err_not_found SMBus bus not found
 * @retval k_rx_err_invalid_state SMBus not initialized
 * @retval k_rx_err_timeout SMBus timeout
 * @retval k_rx_err_crc_mismatch PEC check failed
 *
 * @pre rx_bq4050_init() called successfully
 * @pre BQ4050 initialized flag set (learning cycle complete)
 *
 * @post soc_percent contains 0-100% SOC on success
 *
 * @note Thread Safety: NOT thread-safe
 * @note Performance: ~1-5ms @ 100kHz SMBus
 * @note Accuracy: ±1% typical after battery learning cycle
 *
 * @warning SOC invalid until BQ4050 initialized flag set (see BatteryStatus)
 *
 * @par Example:
 * @code{.c}
 * uint8_t soc_percent;
 * rx_err_t err = rx_bq4050_read_relative_soc(bus_mgr, "smbus0", &soc_percent);
 *
 * if (err == k_rx_ok) {
 *     rx_log_info_val("Battery", "State of charge (%)", soc_percent);
 *
 *     // Update battery icon
 *     if (soc_percent <= 10) {
 *         display_battery_icon(BATTERY_CRITICAL);
 *         rx_log_warn("Battery", "Battery critically low - charge soon!");
 *     } else if (soc_percent <= 20) {
 *         display_battery_icon(BATTERY_LOW);
 *     } else if (soc_percent >= 95) {
 *         display_battery_icon(BATTERY_FULL);
 *     } else {
 *         display_battery_icon(BATTERY_NORMAL);
 *     }
 * }
 * @endcode
 *
 * @see rx_bq4050_read_absolute_soc() Read absolute SOC (vs design capacity)
 * @see k_sbs_relative_state_of_charge SBS register (0x0D)
 * @see BQ4050 Data Sheet section 7.3.6 (RelativeStateOfCharge)
 * @see BQ4050 Technical Reference section 4.1 (CEDV Algorithm)
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_bq4050_read_relative_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc_percent);

/**
 * @brief Read absolute state of charge (vs design capacity)
 *
 * @details
 * Reads AbsoluteStateOfCharge from SBS register (0x0E). This is the SOC
 * as a percentage of the original design capacity, NOT accounting for aging.
 * **Used for battery health monitoring, not user display.**
 *
 * **Health Calculation:**
 * Battery health = (full_capacity / design_capacity) * 100%
 *
 * Comparing absolute_soc to relative_soc indicates battery degradation:
 * - New battery: absolute_soc ≈ relative_soc
 * - Aged battery: absolute_soc < relative_soc
 *
 * @param[in] manager Bus manager instance
 *   - **Must not be nullptr**
 *
 * @param[in] bus_name SMBus bus name
 *   - **Must not be nullptr**
 *
 * @param[out] soc_percent Pointer to store absolute SOC (0-100%)
 *   - **Must not be nullptr**
 *   - **Range:** 0-100%
 *   - **Clamped:** Driver clamps to 0-100%
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, soc_percent contains valid absolute SOC
 * @retval k_rx_err_null_ptr NULL pointer in manager, bus_name, or soc_percent
 * @retval k_rx_err_not_found SMBus bus not found
 * @retval k_rx_err_invalid_state SMBus not initialized
 * @retval k_rx_err_timeout SMBus timeout
 * @retval k_rx_err_crc_mismatch PEC check failed
 *
 * @pre rx_bq4050_init() called successfully
 *
 * @post soc_percent contains 0-100% absolute SOC on success
 *
 * @note Thread Safety: NOT thread-safe
 * @note Performance: ~1-5ms @ 100kHz SMBus
 *
 * @par Example - Battery Health Check:
 * @code{.c}
 * uint8_t relative_soc, absolute_soc;
 *
 * rx_bq4050_read_relative_soc(bus_mgr, "smbus0", &relative_soc);
 * rx_bq4050_read_absolute_soc(bus_mgr, "smbus0", &absolute_soc);
 *
 * // Check for aging
 * if (relative_soc > absolute_soc + 10) {
 *     rx_log_warn("Battery", "Battery shows significant aging");
 *     rx_log_warn_val("Battery", "Relative SOC (%)", relative_soc);
 *     rx_log_warn_val("Battery", "Absolute SOC (%)", absolute_soc);
 * }
 *
 * // Calculate battery health from capacity ratio
 * uint16_t full_cap, design_cap;
 * if (rx_bq4050_read_capacity(bus_mgr, "smbus0", nullptr, &full_cap) == k_rx_ok) {
 *     // Read design capacity from register 0x18
 *     uint8_t health_percent = (full_cap * 100) / design_cap;
 *     rx_log_info_val("Battery", "Health (%)", health_percent);
 *
 *     if (health_percent < 80) {
 *         rx_log_warn("Battery", "Battery health <80% - consider replacement");
 *     }
 * }
 * @endcode
 *
 * @see rx_bq4050_read_relative_soc() Read relative SOC (recommended for display)
 * @see k_sbs_absolute_state_of_charge SBS register (0x0E)
 * @see BQ4050 Data Sheet section 7.3.7 (AbsoluteStateOfCharge)
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_bq4050_read_absolute_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc_percent);

/**
 * @brief Read battery pack temperature
 *
 * @details
 * Reads battery pack temperature from SBS Temperature register (0x08).
 * Temperature is measured by a thermistor inside the battery pack and
 * converted by the BQ4050's ADC.
 *
 * **SBS Temperature Format:**
 * - BQ4050 reports: Unsigned 16-bit in 0.1 Kelvin units
 * - Driver converts: To signed int16_t in whole degrees Celsius
 * - Conversion: temp_c = (temp_0.1k - 2731) / 10
 *
 * **Temperature Monitoring Critical For:**
 * - Charge safety (0-45°C safe charge range for Li-Ion)
 * - Discharge safety (-20-60°C operational range)
 * - Thermal protection (>60°C triggers alarm)
 * - Performance optimization (capacity varies with temperature)
 *
 * **Algorithm:**
 * 1. Validate parameters (NULL pointer checks)
 * 2. Read temperature from register 0x08 (uint16_t in 0.1K units)
 * 3. Convert using internal_convert_temperature() with validation:
 *    a. Subtract Kelvin offset (2731 = 273.15K * 10)
 *    b. Validate intermediate result fits in range
 *    c. Divide by 10 to get whole degrees Celsius
 *    d. Validate final result fits in int16_t
 * 4. Return converted temperature in degrees Celsius
 *
 * @param[in] manager Bus manager instance
 *   - **Must not be nullptr**
 *
 * @param[in] bus_name SMBus bus name
 *   - **Must not be nullptr**
 *
 * @param[out] temperature_c Pointer to store temperature in degrees Celsius
 *   - **Must not be nullptr**
 *   - **Range:** -273 to +6280°C (full conversion range)
 *   - **Typical Range:** -20 to +70°C (operational)
 *   - **Resolution:** 1°C (after conversion from 0.1K)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, temperature_c contains valid temperature
 * @retval k_rx_err_null_ptr NULL pointer in manager, bus_name, or temperature_c
 * @retval k_rx_err_not_found SMBus bus not found
 * @retval k_rx_err_invalid_state SMBus not initialized
 * @retval k_rx_err_timeout SMBus timeout
 * @retval k_rx_err_crc_mismatch PEC check failed
 * @retval k_rx_err_out_of_range Temperature conversion overflow (extremely rare)
 *
 * @pre rx_bq4050_init() called successfully
 *
 * @post temperature_c contains temperature in °C on success
 *
 * @note Thread Safety: NOT thread-safe
 * @note Performance: ~1-5ms @ 100kHz SMBus + conversion overhead (<1µs)
 * @note Update Rate: BQ4050 updates temperature every ~1 second
 *
 * @warning Stop charging if temperature outside 0-45°C (Li-Ion safety)
 * @warning Stop discharging if temperature >60°C (thermal protection)
 *
 * @par Example - Temperature Monitoring:
 * @code{.c}
 * int16_t temperature_c;
 * rx_err_t err = rx_bq4050_read_temperature(bus_mgr, "smbus0", &temperature_c);
 *
 * if (err == k_rx_ok) {
 *     rx_log_info_val("Battery", "Temperature (C)", temperature_c);
 *
 *     // Check temperature thresholds
 *     const int16_t k_charge_temp_min_c = 0;    // 0°C min charge temp
 *     const int16_t k_charge_temp_max_c = 45;   // 45°C max charge temp
 *     const int16_t k_temp_critical_c = 60;     // 60°C critical shutdown
 *
 *     if (temperature_c > k_temp_critical_c) {
 *         rx_log_error("Battery", "CRITICAL: Temperature >60C - emergency stop!");
 *         motor_emergency_stop();
 *         charger_disable();
 *         return k_rx_err_over_temperature;
 *     }
 *
 *     // Check charge temperature range
 *     bool is_charging = battery_status.is_charging;
 *     if (is_charging) {
 *         if (temperature_c < k_charge_temp_min_c ||
 *             temperature_c > k_charge_temp_max_c) {
 *             rx_log_warn("Battery", "Temperature outside safe charge range");
 *             charger_disable();
 *         }
 *     }
 * }
 * @endcode
 *
 * @par Example - Temperature Derating:
 * @code{.c}
 * // Reduce max charge current at high temperatures
 * int16_t temp_c;
 * if (rx_bq4050_read_temperature(bus_mgr, "smbus0", &temp_c) == k_rx_ok) {
 *     uint16_t max_charge_current_ma;
 *
 *     if (temp_c >= 40) {
 *         max_charge_current_ma = 1000;  // 1A @ >40C (reduced)
 *     } else if (temp_c >= 10) {
 *         max_charge_current_ma = 2000;  // 2A @ 10-40C (normal)
 *     } else {
 *         max_charge_current_ma = 500;   // 0.5A @ <10C (reduced)
 *     }
 *
 *     charger_set_max_current(max_charge_current_ma);
 * }
 * @endcode
 *
 * @see k_sbs_temperature SBS register address (0x08)
 * @see internal_convert_temperature() Temperature conversion with validation
 * @see BQ4050 Data Sheet section 7.3.2 (Temperature)
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_bq4050_read_temperature(rx_bus_manager_t* manager, const char* bus_name, int16_t* temperature_c);

/**
 * @brief Read battery capacity information (remaining and full)
 *
 * @details
 * Reads both remaining capacity and full charge capacity from SBS registers
 * (0x0F and 0x10) in a single function call. This is more efficient than
 * calling two separate functions.
 *
 * **Capacity Relationships:**
 * - remaining_capacity ≤ full_charge_capacity ≤ design_capacity
 * - SOC = (remaining_capacity / full_charge_capacity) * 100%
 * - Health = (full_charge_capacity / design_capacity) * 100%
 *
 * **Algorithm:**
 * 1. Validate parameters (NULL pointer checks)
 * 2. Read RemainingCapacity register (0x0F)
 * 3. Read FullChargeCapacity register (0x10)
 * 4. Return both values via output pointers
 *
 * @param[in] manager Bus manager instance
 *   - **Must not be nullptr**
 *
 * @param[in] bus_name SMBus bus name
 *   - **Must not be nullptr**
 *
 * @param[out] remaining_mah Pointer to store remaining capacity (mAh)
 *   - **Must not be nullptr**
 *   - **Range:** 0-65535 mAh
 *   - **Resolution:** 1 mAh
 *
 * @param[out] full_mah Pointer to store full charge capacity (mAh)
 *   - **Must not be nullptr**
 *   - **Range:** 0-65535 mAh
 *   - **Resolution:** 1 mAh
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, both capacity values populated
 * @retval k_rx_err_null_ptr NULL pointer in any parameter
 * @retval k_rx_err_not_found SMBus bus not found
 * @retval k_rx_err_invalid_state SMBus not initialized
 * @retval k_rx_err_timeout SMBus timeout on either read
 * @retval k_rx_err_crc_mismatch PEC check failed on either read
 *
 * @pre rx_bq4050_init() called successfully
 *
 * @post remaining_mah and full_mah contain valid capacities on success
 * @post If error on second read, remaining_mah may be updated but full_mah invalid
 *
 * @note Thread Safety: NOT thread-safe
 * @note Performance: ~2-10ms @ 100kHz SMBus (2 sequential reads)
 * @note Update Rate: Remaining ~1s, Full after each charge cycle
 *
 * @par Example:
 * @code{.c}
 * uint16_t remaining_mah, full_mah;
 * rx_err_t err = rx_bq4050_read_capacity(bus_mgr, "smbus0",
 *                                         &remaining_mah, &full_mah);
 *
 * if (err == k_rx_ok) {
 *     rx_log_info_val("Battery", "Remaining capacity (mAh)", remaining_mah);
 *     rx_log_info_val("Battery", "Full capacity (mAh)", full_mah);
 *
 *     // Calculate percentage manually
 *     uint8_t soc_percent = (remaining_mah * 100) / full_mah;
 *     rx_log_info_val("Battery", "Calculated SOC (%)", soc_percent);
 *
 *     // Estimate runtime at current load
 *     int16_t avg_current_ma;
 *     if (rx_bq4050_read_average_current(bus_mgr, "smbus0", &avg_current_ma) == k_rx_ok) {
 *         if (avg_current_ma < 0) {  // Discharging
 *             uint16_t runtime_min = (remaining_mah * 60) / (-avg_current_ma);
 *             rx_log_info_val("Battery", "Estimated runtime (min)", runtime_min);
 *         }
 *     }
 * }
 * @endcode
 *
 * @see k_sbs_remaining_capacity SBS register (0x0F)
 * @see k_sbs_full_charge_capacity SBS register (0x10)
 * @see BQ4050 Data Sheet section 7.3.8 (RemainingCapacity)
 * @see BQ4050 Data Sheet section 7.3.9 (FullChargeCapacity)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bq4050_read_capacity(rx_bus_manager_t* manager,
                                               const char*       bus_name,
                                               uint16_t*         remaining_mah,
                                               uint16_t*         full_mah);

/**
 * @brief Read complete battery status (all telemetry in single call)
 *
 * @details
 * Reads all available battery telemetry from the BQ4050 and populates the
 * rx_bq4050_status_t structure. This includes:
 * - Pack and cell voltages
 * - Instantaneous and average current
 * - Relative and absolute SOC
 * - Temperature
 * - Remaining, full, and design capacity
 * - Cycle count
 * - Time to empty/full
 * - Status flags (charging, fully charged, etc.)
 *
 * **More efficient than individual reads** - Use this for periodic telemetry
 * logging instead of calling 10+ individual read functions.
 *
 * **Algorithm:**
 * 1. Validate parameters (NULL pointers, num_cells range)
 * 2. Call internal helper functions to read register groups:
 *    a. Electrical: voltage, cell voltages, currents
 *    b. SOC: relative/absolute SOC, temperature
 *    c. Capacity: remaining, full, design, cycle count
 *    d. Timing: time to empty/full
 *    e. Flags: charging, fully charged, low capacity status
 * 3. Populate all rx_bq4050_status_t fields
 * 4. Return first error encountered, or k_rx_ok if all successful
 *
 * @param[in] manager Bus manager instance
 *   - **Must not be nullptr**
 *
 * @param[in] bus_name SMBus bus name
 *   - **Must not be nullptr**
 *
 * @param[out] status Pointer to status structure to populate
 *   - **Must not be nullptr**
 *   - **All fields written** on success
 *   - **Partial fields may be written** on error (use with caution)
 *
 * @param[in] num_cells Number of battery cells (1-4)
 *   - **Range:** 1 to k_bq4050_max_cells (4)
 *   - **Must match** physical battery configuration
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, status fully populated
 * @retval k_rx_err_null_ptr NULL pointer in manager, bus_name, or status
 * @retval k_rx_err_invalid_arg num_cells < 1 or > k_bq4050_max_cells
 * @retval k_rx_err_not_found SMBus bus not found
 * @retval k_rx_err_invalid_state SMBus not initialized
 * @retval k_rx_err_timeout SMBus timeout on any register read
 * @retval k_rx_err_crc_mismatch PEC check failed on any read
 *
 * @pre rx_bq4050_init() called successfully
 * @pre num_cells matches physical battery cell count
 *
 * @post status structure fully populated on success
 * @post On error, status may contain partial data (do not use)
 *
 * @note Thread Safety: NOT thread-safe
 * @note Performance: ~50-150ms @ 100kHz SMBus (14 SMBus transactions)
 * @note Recommended Rate: 1-10 Hz polling (battery state changes slowly)
 *
 * @warning On error, status contains partial/stale data - do not use
 * @warning High latency function - do not call from time-critical code
 *
 * @par Example - Periodic Telemetry Logging:
 * @code{.c}
 * // ThreadX task for battery monitoring (1 Hz)
 * void battery_monitor_task_entry(ULONG thread_input)
 * {
 *     rx_bq4050_status_t battery;
 *     const uint8_t num_cells = 4;
 *
 *     while (1) {
 *         rx_err_t err = rx_bq4050_read_status(bus_mgr, "smbus0", &battery, num_cells);
 *
 *         if (err == k_rx_ok) {
 *             // Log critical battery parameters
 *             rx_log_info_val("Battery", "SOC (%)", battery.relative_soc);
 *             rx_log_info_val("Battery", "Voltage (mV)", battery.voltage_mv);
 *             rx_log_info_val("Battery", "Current (mA)", battery.current_ma);
 *             rx_log_info_val("Battery", "Temp (C)", battery.temperature_c);
 *
 *             // Check for alarm conditions
 *             if (battery.is_low_capacity) {
 *                 rx_log_warn("Battery", "LOW BATTERY - Connect charger");
 *                 led_set_warning(LED_BATTERY_LOW);
 *             }
 *
 *             if (battery.temperature_c > 50) {
 *                 rx_log_error("Battery", "OVER-TEMP - Stop charging");
 *                 charger_disable();
 *             }
 *
 *             // Publish battery telemetry to ROS2
 *             publish_battery_telemetry(&battery);
 *         } else {
 *             rx_log_error_val("Battery", "Read status failed", (uint32_t)err);
 *         }
 *
 *         tx_thread_sleep(100);  // Sleep 1000ms (1 Hz @ 10ms tick)
 *     }
 * }
 * @endcode
 *
 * @par Example - Battery Health Report:
 * @code{.c}
 * rx_bq4050_status_t battery;
 * if (rx_bq4050_read_status(bus_mgr, "smbus0", &battery, 4) == k_rx_ok) {
 *     rx_log_info("=== Battery Health Report ===");
 *
 *     // Capacity health
 *     uint8_t health = (battery.full_capacity_mah * 100) / battery.design_capacity_mah;
 *     rx_log_info_val("  Health (%)", health);
 *     rx_log_info_val("  Cycle count", battery.cycle_count);
 *     rx_log_info_val("  Full capacity (mAh)", battery.full_capacity_mah);
 *     rx_log_info_val("  Design capacity (mAh)", battery.design_capacity_mah);
 *
 *     // Cell balance
 *     uint16_t min_cell = battery.cell_voltages_mv[0];
 *     uint16_t max_cell = battery.cell_voltages_mv[0];
 *     for (uint8_t i = 1; i < 4; i++) {
 *         if (battery.cell_voltages_mv[i] < min_cell) min_cell = battery.cell_voltages_mv[i];
 *         if (battery.cell_voltages_mv[i] > max_cell) max_cell = battery.cell_voltages_mv[i];
 *     }
 *     rx_log_info_val("  Cell imbalance (mV)", max_cell - min_cell);
 *
 *     // Recommendations
 *     if (health < 80) {
 *         rx_log_warn("  Battery degraded - consider replacement");
 *     }
 *     if ((max_cell - min_cell) > 100) {
 *         rx_log_warn("  Excessive cell imbalance - weak cell detected");
 *     }
 * }
 * @endcode
 *
 * @see rx_bq4050_status_t Complete status structure definition
 * @see internal_read_electrical_status() Voltage/current read helper
 * @see internal_read_soc_status() SOC/temperature read helper
 * @see internal_read_capacity_status() Capacity read helper
 * @see internal_read_timing_status() Runtime estimate read helper
 * @see internal_read_status_flags() Battery status flags read helper
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bq4050_read_status(rx_bus_manager_t*   manager,
                                             const char*         bus_name,
                                             rx_bq4050_status_t* status,
                                             uint8_t             num_cells);

#ifdef __cplusplus
}
#endif
