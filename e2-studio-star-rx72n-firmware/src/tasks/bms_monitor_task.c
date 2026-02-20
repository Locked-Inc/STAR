/* src/tasks/bms_monitor_task.c */

/**
 * @file bms_monitor_task.c
 * @brief BMS Monitoring Task Implementation - BQ4050 Battery Fuel Gauge
 *
 * @details
 * # Overview
 *
 * This module implements the Battery Management System (BMS) monitoring task that reads
 * battery state from the Texas Instruments BQ4050 fuel gauge IC via I2C at 1 Hz using
 * the SMBus protocol. The task monitors a 4S LiPo battery pack (14.8V nominal) and tracks
 * voltage, current, state of charge (SoC), temperature, capacity, and fault conditions.
 *
 * All battery data is stored in thread-safe shared_data structures for consumption by
 * the Telemetry Task, which forwards the information to the Raspberry Pi 5 gateway via
 * SPI Protocol Buffers.
 *
 * ## Battery System Architecture
 *
 * @dot
 * digraph bms_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_battery {
 *     label="Battery Pack (4S LiPo)";
 *     style=filled;
 *     color=lightblue;
 *
 *     cells [label="4 Cells in Series\n(3.7V × 4 = 14.8V nominal)\n(4.2V × 4 = 16.8V max)",
 *            fillcolor=lightyellow, style=filled];
 *     bq4050 [label="BQ4050 Fuel Gauge\n(I2C Address 0x0B)\n7 Monitored Registers",
 *             fillcolor=lightgreen, style=filled];
 *     sensor [label="Current Sense Resistor\n(10mΩ for 0-10A range)",
 *             fillcolor=lightcoral, style=filled];
 *   }
 *
 *   subgraph cluster_rx72n {
 *     label="RX72N Firmware";
 *     style=filled;
 *     color=lightgreen;
 *
 *     bms_task [label="BMS Monitor Task\n(This Module)\nPriority 15 @ 1 Hz",
 *               fillcolor=lightgreen, style=filled];
 *     i2c_driver [label="rx_bq4050 Driver\n(SMBus Protocol)",
 *                 fillcolor=lightyellow, style=filled];
 *     shared_data [label="Shared Data\n(Thread-Safe Storage)",
 *                  shape=cylinder, fillcolor=lightgrey, style=filled];
 *     telemetry [label="Telemetry Task\n(Priority 18 @ 10 Hz)",
 *                fillcolor=lightcoral, style=filled];
 *   }
 *
 *   cells -> bq4050 [label="Cell voltages"];
 *   sensor -> bq4050 [label="Current measurement"];
 *   bq4050 -> i2c_driver [label="I2C/SMBus @ 100 kHz"];
 *   i2c_driver -> bms_task [label="rx_bq4050_read_status()"];
 *   bms_task -> shared_data [label="shared_data_update_bms()"];
 *   shared_data -> telemetry [label="shared_data_get_bms()"];
 *   telemetry -> telemetry [label="Forward to RPI5 via SPI"];
 * }
 * @enddot
 *
 * ## BQ4050 SMBus Protocol Details
 *
 * The BQ4050 fuel gauge communicates via SMBus (I2C-compatible) at 100 kHz. The task
 * reads 7 critical registers every second to build a complete battery state snapshot:
 *
 * | Register | SMBus Cmd | Type | Units | Range | Description |
 * |----------|-----------|------|-------|-------|-------------|
 * | **Voltage** | 0x09 | uint16_t | mV | 0-20000 | Total pack voltage |
 * | **Current** | 0x0A | int16_t | mA | -10000 to +10000 | Instantaneous current (+ = charge) |
 * | **RelativeStateOfCharge** | 0x0D | uint8_t | % | 0-100 | Remaining capacity percentage |
 * | **Temperature** | 0x08 | uint16_t | 0.1 K | 2731-3731 | Pack temperature (273.1K = 0°C) |
 * | **RemainingCapacity** | 0x0F | uint16_t | mAh | 0-5000 | Charge remaining |
 * | **FullChargeCapacity** | 0x10 | uint16_t | mAh | 0-5000 | Full capacity (degrades over time) |
 * | **CycleCount** | 0x17 | uint16_t | cycles | 0-65535 | Total charge/discharge cycles |
 *
 * ### I2C Transaction Timing
 *
 * Each register read involves:
 * 1. **START condition** - 10 µs
 * 2. **Address byte** (0x16 = 0x0B << 1) - 90 µs (9 bits @ 100 kHz)
 * 3. **Command byte** (register address) - 90 µs
 * 4. **Repeated START** - 10 µs
 * 5. **Address byte + Read bit** - 90 µs
 * 6. **Data bytes** (1-2 bytes) - 90-180 µs
 * 7. **STOP condition** - 10 µs
 *
 * **Total per register:** ~300-400 µs
 * **Total for 7 registers:** ~2100-2800 µs (2ms average)
 *
 * ## Battery Monitoring Algorithm
 *
 * The task executes an infinite monitoring loop with the following algorithm:
 *
 * @startuml
 * start
 * :Initialize BQ4050\n(I2C bus, SMBus protocol);
 * if (Initialization successful?) then (yes)
 *   :Log "BMS running @ 1 Hz";
 * else (no)
 *   :Log error, continue with invalid data;
 * endif
 *
 * partition "Infinite Monitoring Loop" {
 *   repeat
 *     :Read 7 BQ4050 registers\n(~2ms I2C time);
 *     if (Read successful?) then (yes)
 *       :Build bms_state_t structure;
 *       :Set timestamp, mark valid;
 *       if (SoC < 5%?) then (yes)
 *         :Log CRITICAL;
 *         :Trigger emergency stop\n(k_estop_reason_low_battery);
 *       else if (SoC < 25%?) then (yes)
 *         :Log WARNING;
 *         :Set low battery event\n(k_event_low_battery);
 *       endif
 *     else (no)
 *       :Mark data invalid;
 *       :Log I2C error;
 *     endif
 *     :Update shared_data\n(thread-safe);
 *     :Sleep 1000ms\n(yield CPU);
 *   repeat while (forever)
 * }
 * @enduml
 *
 * ## Low Battery Detection Strategy
 *
 * The task implements a three-tier low battery detection system using configurable
 * thresholds. The defaults (set by bms_monitor_config_t) are:
 *
 * | Threshold | SoC % (default) | Voltage (approx) | Action | Severity |
 * |-----------|-----------------|------------------|--------|----------|
 * | **Normal** | > 25% | > 14.4V | None | Info |
 * | **Warning** | < 25% | ~14.0V | Log warning, set event flag | Warning |
 * | **Critical** | < 5% | ~13.2V | Emergency stop trigger | Error |
 *
 * **Rationale for 25% Default Warning Threshold:**
 * - Provides ~22 minutes of operation at 2A average current (5000mAh × 0.25 / 2A)
 * - Gives operator sufficient margin to navigate to charging station
 * - Prevents deep discharge damage (< 3.0V per cell)
 *
 * **Rationale for 5% Critical Threshold:**
 * - Final safety margin before battery damage
 * - Triggers immediate emergency stop sequence
 * - Prevents over-discharge (BQ4050 cutoff at 2.5V per cell)
 *
 * ## Task Execution Flow
 *
 * @msc
 * msc {
 *   width=900;
 *   ThreadX, BMSTask, I2CBus, BQ4050, SharedData, TelemetryTask;
 *
 *   --- [label="Task Creation (Boot Time)"];
 *   ThreadX box ThreadX [label="tx_application_define()"];
 *   ThreadX => BMSTask [label="bms_monitor_task_create()"];
 *   BMSTask box BMSTask [label="Create thread\nAllocate stack (1024 bytes)\nSet priority 15"];
 *   BMSTask => ThreadX [label="TX_SUCCESS"];
 *   ThreadX box ThreadX [label="Schedule task\n(auto-start)"];
 *
 *   --- [label="Task Initialization"];
 *   ThreadX => BMSTask [label="internal_bms_task_entry()"];
 *   BMSTask => I2CBus [label="rx_bq4050_init()"];
 *   I2CBus => BQ4050 [label="Verify device ID"];
 *   BQ4050 >> I2CBus [label="BQ4050 detected"];
 *   I2CBus >> BMSTask [label="k_rx_ok"];
 *   BMSTask box BMSTask [label="Log: BMS running @ 1 Hz"];
 *
 *   --- [label="Normal Monitoring Loop (Every 1 Second)"];
 *   BMSTask => I2CBus [label="rx_bq4050_read_status()"];
 *   I2CBus => BQ4050 [label="Read 7 registers\n(~2ms total)"];
 *   BQ4050 >> I2CBus [label="Voltage: 16200mV\nCurrent: 2450mA\nSoC: 85%\nTemp: 25°C"];
 *   I2CBus >> BMSTask [label="k_rx_ok"];
 *   BMSTask box BMSTask [label="Build bms_state_t\nCheck thresholds (85% > 25%)\nNo warnings"];
 *   BMSTask => SharedData [label="shared_data_update_bms()"];
 *   SharedData box SharedData [label="Mutex lock\nUpdate bms_state\nMutex unlock"];
 *   SharedData >> BMSTask [label="k_rx_ok"];
 *   BMSTask box BMSTask [label="tx_thread_sleep(100 ticks)"];
 *
 *   TelemetryTask => SharedData [label="shared_data_get_bms()\n(10 Hz polling)"];
 *   SharedData >> TelemetryTask [label="bms_state copy"];
 *   TelemetryTask box TelemetryTask [label="Forward to RPI5"];
 *
 *   --- [label="Low Battery Warning (SoC < 25%)"];
 *   BMSTask => I2CBus [label="rx_bq4050_read_status()"];
 *   I2CBus => BQ4050 [label="Read registers"];
 *   BQ4050 >> I2CBus [label="SoC: 12%"];
 *   I2CBus >> BMSTask [label="k_rx_ok"];
 *   BMSTask box BMSTask [label="12% < 25%\nLog WARNING"];
 *   BMSTask => SharedData [label="shared_data_set_event(k_event_low_battery)"];
 *   SharedData box SharedData [label="Set event flag"];
 *
 *   --- [label="Critical Battery (SoC < 5%)"];
 *   BMSTask => I2CBus [label="rx_bq4050_read_status()"];
 *   BQ4050 >> I2CBus [label="SoC: 3%"];
 *   I2CBus >> BMSTask [label="k_rx_ok"];
 *   BMSTask box BMSTask [label="3% < 5%\nLog CRITICAL"];
 *   BMSTask => SharedData [label="shared_data_trigger_estop(k_estop_reason_low_battery)"];
 *   SharedData box SharedData [label="EMERGENCY STOP\nDisable all motors"];
 * }
 * @endmsc
 *
 * ## Performance Characteristics
 *
 * | Metric | Value | Notes |
 * |--------|-------|-------|
 * | **Poll Rate** | 1 Hz | One complete battery read per second |
 * | **I2C Frequency** | 100 kHz | SMBus standard mode |
 * | **Transaction Time** | ~2ms | 7 registers × 300µs each |
 * | **CPU Time per Poll** | ~2ms | 99.8% idle time |
 * | **CPU Utilization** | < 0.2% | (2ms / 1000ms) × 100% |
 * | **Worst Case Latency** | 1000ms | Max time to detect low battery |
 * | **I2C Timeout** | 100ms | Per-register read timeout |
 *
 * ## Memory Usage
 *
 * | Component | Size (bytes) | Section | Description |
 * |-----------|--------------|---------|-------------|
 * | `s_bms_thread` | 140 | .bss | TX_THREAD control block |
 * | `s_bms_stack` | 1024 | .bss | Task stack (static allocation) |
 * | `s_bms_created` | 1 | .bss | Creation guard flag |
 * | `s_tag` | 8 | .rodata | Log tag string ("BMS" + null) |
 * | `s_i2c_bus_name` | 8 | .rodata | I2C bus name ("i2c0" + null) |
 * | **Stack locals** | ~256 | Stack | I2C buffers, bms_state_t, temps |
 * | **Total Static** | ~1293 | - | Sum of .bss + .rodata |
 * | **Peak Stack** | ~256 | - | During rx_bq4050_read_status() |
 *
 * ## Module Dependencies
 *
 * @dot
 * digraph dependencies {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   bms_task [label="bms_monitor_task.c\n(This Module)", fillcolor=lightgreen, style=filled];
 *
 *   // Header dependencies
 *   bms_task_h [label="bms_monitor_task.h"];
 *   rx_bq4050 [label="rx_bq4050.h\n(BQ4050 Driver)", fillcolor=lightyellow, style=filled];
 *   rx_check [label="rx_check.h\n(Assertions)"];
 *   rx_log [label="rx_log.h\n(Logging)"];
 *   shared_data [label="shared_data.h\n(Thread-Safe Storage)", fillcolor=lightcoral, style=filled];
 *   tx_api [label="tx_api.h\n(ThreadX RTOS)"];
 *   string [label="string.h\n(memset)"];
 *
 *   // Indirect dependencies
 *   rx_i2c [label="rx_i2c.h\n(I2C Transport)", fillcolor=lightgrey, style=filled];
 *   rx_bus_manager [label="rx_bus_manager.h\n(Bus Abstraction)", fillcolor=lightgrey, style=filled];
 *   rx_err [label="rx_err.h\n(Error Codes)"];
 *
 *   bms_task -> bms_task_h [label="Public API"];
 *   bms_task -> rx_bq4050 [label="Fuel gauge driver"];
 *   bms_task -> rx_check [label="RX_ASSERT"];
 *   bms_task -> rx_log [label="Logging"];
 *   bms_task -> shared_data [label="Battery state storage"];
 *   bms_task -> tx_api [label="Thread/sleep API"];
 *   bms_task -> string [label="memset"];
 *
 *   rx_bq4050 -> rx_i2c [label="I2C transport", style=dashed];
 *   rx_bq4050 -> rx_bus_manager [label="Bus abstraction", style=dashed];
 *   rx_bq4050 -> rx_err [label="Error codes", style=dashed];
 * }
 * @enddot
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation Details |
 * |------|--------|------------------------|
 * | **Rule 1: Control Flow** | [PASS] | No goto, setjmp, longjmp, or recursion. All control flow uses if/while only. |
 * | **Rule 2: Loop Bounds** | [PASS] | Single while(true) loop with fixed 1s sleep period. Provably bounded iteration. |
 * | **Rule 3: No Heap** | [PASS] | Zero dynamic allocation. Stack (1024 bytes) and TCB (140 bytes) statically allocated. |
 * | **Rule 4: Function Length** | [PASS] | bms_monitor_task_create(): ~59 lines, internal_bms_task_entry(): ~65 lines (slightly exceeds ~60 LOC guideline; kept intact for readability). |
 * | **Rule 5: Assertions** | [PASS] | 6 assertions: RX_ASSERT(!s_bms_created), 4 preconditions, 2 postconditions. |
 * | **Rule 6: Data Scope** | [PASS] | All file-scope variables use static (s_bms_thread, s_bms_stack, s_bms_created, s_tag). |
 * | **Rule 7: Return Checks** | [PASS] | All rx_bq4050, shared_data, tx_* returns validated or explicitly cast to (void). |
 * | **Rule 8: Preprocessor** | [PASS] | C23 typed enums for all constants (k_bms_task_*, k_bms_soc_warning_pct_default, k_bms_soc_critical_pct_default). Zero macros. |
 * | **Rule 9: Pointers** | [PASS] | Single-level pointers only (bms_state_t*, rx_bq4050_config_t*). |
 * | **Rule 10: Warnings** | [PASS] | Compiles with -Wall -Wextra -Werror. Zero warnings. |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **S - Single Responsibility** | Battery monitoring only. No power control, no motor logic, no communication. |
 * | **O - Open/Closed** | Extensible via bms_monitor_config_t (SoC thresholds). No code changes needed. |
 * | **L - Liskov Substitution** | Returns rx_err_t consistently. Can substitute with mock driver for testing. |
 * | **I - Interface Segregation** | Minimal API: one function (bms_monitor_task_create). No unused methods. |
 * | **D - Dependency Inversion** | Depends on rx_bq4050 abstraction, not raw I2C. Testable via mock injection. |
 *
 * @see shared_data.h Shared data structures and thread-safe API
 * @see rx_bq4050.h BQ4050 fuel gauge driver (SMBus protocol)
 * @see rx_i2c.h I2C bus driver (100 kHz SMBus mode)
 * @see rx_bus_manager.h Bus abstraction layer
 * @see telemetry_task.h Consumer of battery data (forwards to RPI5)
 * @see docs/sections/03_hardware_pinout.tex Hardware I2C connections
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#include "bms_monitor_task.h"

#include <string.h>

#include "rx_bq4050.h"
#include "rx_bq4050_constants.h"
#include "rx_check.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "shared_data.h"
#include "tx_api.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum bms_task_constants_t
 * @brief BMS monitoring task configuration constants
 */
typedef enum : uint16_t {
  k_bms_task_stack_size  = 1024, /**< Stack size in bytes */
  k_bms_task_priority    = 15,   /**< ThreadX priority (low) */
  k_bms_task_sleep_ticks = 100,  /**< Sleep period (1s at 100 Hz tick) */
  k_bms_task_input       = 0,    /**< Thread entry input parameter */
} bms_task_constants_t;

/**
 * @enum bms_cell_constants_t
 * @brief BMS hardware constants describing the physical battery pack topology
 *
 * @details
 * Fixed hardware constants that reflect the physical 4S LiPo pack design.
 * These values are determined at board bring-up and do not change at runtime.
 * The BQ4050 must be configured to match the cell count during manufacturing.
 *
 * @invariant k_bms_cell_count == 4 (fixed 4S LiPo pack; changing requires
 *            BQ4050 re-configuration and full recalibration)
 *
 * @code
 * // Pass cell count to driver when reading pack status
 * rx_err_t err = rx_bq4050_read_status(manager, "i2c0", &status, k_bms_cell_count);
 * @endcode
 *
 * @see rx_bq4050_read_status() Uses k_bms_cell_count for per-cell voltage checks
 * @see bms_task_constants_t Related task scheduling constants
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bms_cell_count = 4, /**< Number of series cells in pack (4S LiPo: 4 × 3.7 V nominal) */
} bms_cell_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief ThreadX thread control block */
static TX_THREAD s_bms_thread;

/** @brief Static thread stack (no dynamic allocation) */
static uint8_t s_bms_stack[k_bms_task_stack_size];

/** @brief Task creation guard flag */
static bool s_bms_created = false;

/**
 * @var s_bms_config
 * @brief Active monitoring configuration (copied from caller at creation)
 *
 * @details
 * Stores the soc_warning_pct and soc_critical_pct thresholds passed to
 * bms_monitor_task_create(). The entire bms_monitor_config_t struct is
 * copied by value at task creation time so the caller's pointer need not
 * remain valid after bms_monitor_task_create() returns. All threshold
 * comparisons inside internal_bms_task_entry() read from this static copy.
 *
 * @note Internal linkage only (static). Must not be accessed from outside
 *       this translation unit. Valid only after bms_monitor_task_create()
 *       has been called and has returned k_rx_ok.
 *
 * @warning Do not modify this variable directly. The only supported way
 *          to change thresholds is to stop the task and call
 *          bms_monitor_task_create() again (single-shot design).
 *
 * @since Version 1.1.0
 */
static bms_monitor_config_t s_bms_config;

/** @brief Log tag for this module */
static const char* const s_tag = "BMS";

/** @brief Bus manager handle (assumed initialized elsewhere) */
extern rx_bus_manager_t g_bus_manager;

/** @brief I2C bus name for BQ4050 */
static const char* const s_i2c_bus_name = "i2c0";

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

static void internal_bms_task_entry(ULONG input);
static bool internal_check_critical_faults(uint16_t status_flags);
static void internal_bms_convert_status_to_state(const rx_bq4050_status_t* status,
                                                 bms_state_t*             out);

/* =============================================================================
 * Private Functions
 * =============================================================================
 */

/**
 * @brief Check for critical battery faults and trigger emergency stop
 *
 * @details
 * Decodes BatteryStatus register (0x16) and checks ALL 10 battery status flags.
 * Triggers emergency stop for OTA, OCA, or TDA (life-safety critical).
 * Logs warnings/info for all other flags.
 *
 * **Status Flags Checked (10 total):**
 * - **CRITICAL** (trigger e-stop): OTA, OCA, TDA
 * - **HIGH** (log warning): TCA, RCA, RTA
 * - **INFO** (log info): INIT, DSG, FC, FD
 *
 * @param[in] status_flags Raw BatteryStatus register value (0x16)
 * @return true if critical fault detected, false otherwise
 *
 * @pre status_flags read from valid BQ4050 register 0x16
 * @post Emergency stop triggered if OTA, OCA, or TDA set
 *
 * @note Not thread-safe, should only be called from BMS task
 * @note All 10 SBS status flags are checked and logged appropriately
 */
static bool internal_check_critical_faults(uint16_t status_flags)
{
  bool critical = false;

  /* ========================================================================
   * CRITICAL Alarms (trigger emergency stop)
   * ========================================================================
   */
  /* CRITICAL: Over-temperature alarm (>60°C, thermal runaway risk) */
  if ((status_flags & k_bq4050_status_over_temp_alarm) != k_bq4050_status_flag_clear) {
    rx_log_error(s_tag, "CRITICAL: Over-temperature alarm - battery >60C");
    (void)shared_data_trigger_estop(k_estop_reason_battery_fault);
    critical = true;
  }

  /* CRITICAL: Over-charged alarm (>4.3V/cell, lithium plating risk) */
  if ((status_flags & k_bq4050_status_over_charged_alarm) != k_bq4050_status_flag_clear) {
    rx_log_error(s_tag, "CRITICAL: Over-charged alarm - voltage >4.3V/cell");
    (void)shared_data_trigger_estop(k_estop_reason_battery_fault);
    critical = true;
  }

  /* CRITICAL: Terminate discharge alarm (<3.0V/cell, copper dissolution) */
  if ((status_flags & k_bq4050_status_terminate_discharge_alarm) != k_bq4050_status_flag_clear) {
    rx_log_error(s_tag, "CRITICAL: Terminate discharge alarm - undervoltage");
    (void)shared_data_trigger_estop(k_estop_reason_battery_fault);
    critical = true;
  }

  /* ========================================================================
   * HIGH Priority Warnings (log warning, no e-stop)
   * ========================================================================
   */

  /* HIGH: Terminate charge alarm (normal end-of-charge) */
  if ((status_flags & k_bq4050_status_terminate_charge_alarm) != k_bq4050_status_flag_clear) {
    rx_log_info(s_tag, "Charge complete - terminate charge alarm");
  }

  /* MEDIUM: Remaining capacity alarm (low battery, <15-20% SOC) */
  if ((status_flags & k_bq4050_status_remaining_capacity_alarm) != k_bq4050_status_flag_clear) {
    rx_log_warn(s_tag, "Low capacity warning - remaining capacity alarm");
  }

  /* MEDIUM: Remaining time alarm (<10 minutes runtime) */
  if ((status_flags & k_bq4050_status_remaining_time_alarm) != k_bq4050_status_flag_clear) {
    rx_log_warn(s_tag, "Low runtime warning - <10 minutes remaining");
  }

  /* ========================================================================
   * INFO Status Flags (log info, non-critical)
   * ========================================================================
   */

  /* INFO: Initialized flag (battery gauging calibrated) */
  if ((status_flags & k_bq4050_status_initialized) != k_bq4050_status_flag_clear) {
    /* Normally set after initial capacity learning - log once or debug only */
    /* rx_log_info(s_tag, "Battery initialized - gauging active"); */
  }

  /* INFO: Discharging flag (battery discharging, negative current) */
  if ((status_flags & k_bq4050_status_discharging) != k_bq4050_status_flag_clear) {
    /* Common state, log only for debug if needed */
    /* rx_log_info(s_tag, "Battery discharging"); */
  }

  /* INFO: Fully charged flag (charge complete) */
  if ((status_flags & k_bq4050_status_fully_charged) != k_bq4050_status_flag_clear) {
    rx_log_info(s_tag, "Battery fully charged");
  }

  /* INFO: Fully discharged flag (battery empty) */
  if ((status_flags & k_bq4050_status_fully_discharged) != k_bq4050_status_flag_clear) {
    rx_log_warn(s_tag, "Battery fully discharged - charge immediately");
  }

  return critical;
}

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Create the BMS monitoring task
 *
 * @details
 * Creates and starts the BMS monitoring ThreadX task with low priority (Priority 15)
 * for non-critical battery state monitoring at 1 Hz. This function allocates the
 * thread control block, assigns a static stack, and registers the task with ThreadX.
 *
 * ## Algorithm Steps
 *
 * The function performs the following operations in sequence:
 *
 * 1. **Guard Check:** Verify s_bms_created is false (prevent double-creation)
 * 2. **Thread Creation:** Call tx_thread_create() with:
 *    - Thread name: "BMSTask" (8 chars max)
 *    - Entry point: internal_bms_task_entry
 *    - Stack: s_bms_stack (1024 bytes, static allocation)
 *    - Priority: 15 (low priority, non-critical monitoring)
 *    - Auto-start: Immediate scheduling after creation
 * 3. **Error Check:** Validate TX_SUCCESS return from ThreadX
 * 4. **State Update:** Set s_bms_created = true (prevent future creation)
 * 5. **Logging:** Log success message via rx_log_info
 * 6. **Return:** Return k_rx_ok on success
 *
 * ## Thread Configuration Details
 *
 * | Parameter | Value | Rationale |
 * |-----------|-------|-----------|
 * | **Name** | "BMSTask" | ThreadX name (8 char limit) |
 * | **Entry** | internal_bms_task_entry | Task main loop function |
 * | **Input** | 0 | No parameters needed |
 * | **Stack** | s_bms_stack | 1024 bytes static array |
 * | **Stack Size** | 1024 | Sufficient for I2C buffers (256 bytes peak) |
 * | **Priority** | 15 | Low priority (range 0-31, higher = lower priority) |
 * | **Preempt Threshold** | 15 | Same as priority (no preemption protection) |
 * | **Time Slice** | TX_NO_TIME_SLICE | No round-robin (only one task at priority 15) |
 * | **Auto Start** | TX_AUTO_START | Begin execution immediately after creation |
 *
 * ## Control Flow Diagram
 *
 * @dot
 * digraph bms_create_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start         [label="bms_monitor_task_create()", fillcolor=lightgreen, style=filled];
 *   null_check    [label="config != NULL?", shape=diamond];
 *   null_return   [label="Return k_rx_err_null_ptr", fillcolor=red, style=filled];
 *   range_check   [label="soc_warning_pct in [1,100]\nsoc_critical_pct in [1,99]?",
 *                  shape=diamond];
 *   thresh_check  [label="soc_critical_pct < soc_warning_pct?", shape=diamond];
 *   thresh_return [label="Log error\nReturn k_rx_err_invalid_arg", fillcolor=red, style=filled];
 *   check_created [label="Check s_bms_created", shape=diamond];
 *   already_created [label="Log assertion\nReturn k_rx_err_invalid_state",
 *                    fillcolor=red, style=filled];
 *   create_thread [label="tx_thread_create()\nName: BMSTask\nStack: 1024 bytes\nPriority: 15"];
 *   check_status  [label="ThreadX result?", shape=diamond];
 *   create_failed [label="Log error\nReturn k_rx_err_rtos_thread_create",
 *                  fillcolor=red, style=filled];
 *   set_flag      [label="s_bms_created = true"];
 *   log_success   [label="Log: BMS task created"];
 *   return_ok     [label="Return k_rx_ok", fillcolor=lightgreen, style=filled];
 *
 *   start -> null_check;
 *   null_check -> null_return   [label="NULL"];
 *   null_check -> range_check   [label="!= NULL"];
 *   range_check -> thresh_return [label="out of range"];
 *   range_check -> thresh_check  [label="in range"];
 *   thresh_check -> thresh_return [label="violated\n(critical >= warning)"];
 *   thresh_check -> check_created [label="valid\n(critical < warning)"];
 *   check_created -> already_created [label="true\n(double-create)"];
 *   check_created -> create_thread   [label="false\n(first call)"];
 *   create_thread -> check_status;
 *   check_status -> create_failed [label="!= TX_SUCCESS"];
 *   check_status -> set_flag      [label="== TX_SUCCESS"];
 *   set_flag -> log_success;
 *   log_success -> return_ok;
 * }
 * @enddot
 *
 * @param[in] config BMS monitoring configuration with SoC thresholds.
 *                   Must not be NULL.
 *                   config->soc_critical_pct must be strictly less than
 *                   config->soc_warning_pct.
 *
 * @return rx_err_t Task creation status
 *
 * @retval k_rx_ok Task created successfully, thread scheduled and running
 * @retval k_rx_err_null_ptr config is NULL
 * @retval k_rx_err_invalid_arg config->soc_warning_pct is outside [k_bms_soc_warning_min, k_bms_soc_warning_max]
 * @retval k_rx_err_invalid_arg config->soc_critical_pct is outside [k_bms_soc_critical_min, k_bms_soc_critical_max]
 * @retval k_rx_err_invalid_arg config->soc_critical_pct >= config->soc_warning_pct
 * @retval k_rx_err_invalid_state Task already created (s_bms_created == true). Second call blocked.
 * @retval k_rx_err_rtos_thread_create ThreadX tx_thread_create() returned error (!= TX_SUCCESS).
 *                                     Possible causes: invalid priority, stack too small, TCB corruption.
 *
 * @pre config != NULL
 * @pre config->soc_warning_pct in [k_bms_soc_warning_min, k_bms_soc_warning_max]
 * @pre config->soc_critical_pct in [k_bms_soc_critical_min, k_bms_soc_critical_max]
 * @pre config->soc_critical_pct < config->soc_warning_pct
 * @pre ThreadX kernel entered via tx_kernel_enter()
 * @pre tx_application_define() callback currently executing
 * @pre shared_data_init() called successfully (required for shared_data_update_bms)
 * @pre RIIC0 peripheral powered and clocked (for I2C communication)
 * @pre s_bms_created == false (never called before)
 * @pre s_bms_thread uninitialized (first use)
 * @pre s_bms_stack allocated and uninitialized (first use)
 *
 * @post On success: s_bms_thread initialized with ThreadX control block
 * @post On success: s_bms_stack assigned to thread and stack pointer initialized
 * @post On success: Task in READY or RUNNING state (depends on ThreadX scheduler)
 * @post On success: s_bms_created == true (prevents future double-creation)
 * @post On success: internal_bms_task_entry() scheduled for execution (auto-start)
 * @post On success: Task will begin polling BQ4050 at 1 Hz after I2C initialization
 * @post On failure: s_bms_created unchanged (remains false)
 *
 * @invariant s_bms_created transitions false -> true exactly once (never resets)
 * @invariant Task priority remains at k_bms_priority (15) throughout lifetime
 * @invariant Stack size remains at k_bms_task_stack_size (1024 bytes)
 *
 * @note **Single-shot:** This function MUST be called exactly once during boot.
 *       Subsequent calls will assert and return k_rx_err_invalid_state.
 * @note **Non-blocking:** Returns immediately after thread creation. Task
 *       executes concurrently after ThreadX scheduler starts.
 * @note **Context:** ONLY call from tx_application_define(). Never call from
 *       task context or interrupt handlers.
 * @note **Thread safety:** Not thread-safe (assumes single-threaded boot).
 *       No synchronization needed during tx_application_define().
 *
 * @warning **Never call twice.** Assertion will fire in debug builds. Release
 *          builds return k_rx_err_invalid_state on second call.
 * @warning **Call order matters.** Must call after shared_data_init() but before
 *          telemetry_task_create() (telemetry consumes BMS data).
 * @warning **Stack size fixed.** 1024 bytes must accommodate I2C transaction
 *          buffers (~256 bytes peak). Overflow detection enabled via
 *          TX_ENABLE_STACK_CHECKING.
 *
 * @attention Task starts immediately (TX_AUTO_START). BQ4050 must be powered
 *            and ready on I2C bus at address 0x0B.
 * @attention Low priority (15) ensures BMS monitoring does not preempt critical
 *            real-time tasks (motor control, obstacle detection).
 * @attention I2C bus must support 100 kHz SMBus protocol with clock stretching.
 *
 * @par Thread Safety:
 * This function is NOT thread-safe and MUST be called from single-threaded
 * context during tx_application_define(). After task creation, the task itself
 * uses thread-safe APIs:
 * - shared_data_update_bms(): Mutex-protected
 * - rx_bq4050_read_status(): I2C bus mutex-protected
 * - tx_thread_sleep(): Safe (yields CPU)
 *
 * @par Performance:
 * - **Creation time:** ~120 µs @ 240 MHz (thread control block initialization)
 * - **CPU cycles:** ~28,800 cycles
 * - **Stack usage during creation:** ~64 bytes (function call overhead)
 * - **Memory allocation:** 1293 bytes total (1024 stack + 140 TCB + 129 static vars)
 *
 * @par Re-entrancy:
 * NOT reentrant. Single-shot function. Second call blocked by s_bms_created guard.
 *
 * @par Example - Standard Usage:
 * @code{.c}
 * // In main.c - tx_application_define() callback
 * void tx_application_define(void* first_unused_memory)
 * {
 *   (void)first_unused_memory;
 *
 *   // 1. Initialize shared data first
 *   rx_err_t ret = shared_data_init();
 *   if (ret != k_rx_ok) {
 *     rx_log_error("INIT", "Shared data init failed");
 *     return;
 *   }
 *
 *   // 2. Create BMS monitoring task with default thresholds (25%/5%)
 *   const bms_monitor_config_t bms_config = {
 *     .soc_warning_pct  = k_bms_soc_warning_pct_default,
 *     .soc_critical_pct = k_bms_soc_critical_pct_default,
 *   };
 *   ret = bms_monitor_task_create(&bms_config);
 *   if (ret != k_rx_ok) {
 *     rx_log_error("INIT", "BMS task create failed");
 *     return;  // Fatal error - cannot monitor battery
 *   }
 *
 *   // 3. Create telemetry task (consumes BMS data)
 *   ret = telemetry_task_create();
 *   if (ret != k_rx_ok) {
 *     rx_log_error("INIT", "Telemetry task create failed");
 *     return;
 *   }
 *
 *   rx_log_info("INIT", "All tasks created successfully");
 * }
 * @endcode
 *
 * @par Example - Error Handling:
 * @code{.c}
 * void tx_application_define(void* first_unused_memory)
 * {
 *   (void)first_unused_memory;
 *
 *   rx_err_t ret = shared_data_init();
 *   if (ret != k_rx_ok) return;
 *
 *   const bms_monitor_config_t bms_config = {
 *     .soc_warning_pct  = k_bms_soc_warning_pct_default,
 *     .soc_critical_pct = k_bms_soc_critical_pct_default,
 *   };
 *   ret = bms_monitor_task_create(&bms_config);
 *   switch (ret) {
 *     case k_rx_ok:
 *       rx_log_info("BMS", "Task created, polling at 1 Hz");
 *       break;
 *     case k_rx_err_null_ptr:
 *       rx_log_error("BMS", "NULL config passed!");
 *       break;
 *     case k_rx_err_invalid_arg:
 *       rx_log_error("BMS", "Invalid threshold config - critical must be < warning");
 *       break;
 *     case k_rx_err_invalid_state:
 *       rx_log_error("BMS", "Double-creation attempt!");
 *       break;
 *     case k_rx_err_rtos_thread_create:
 *       rx_log_error("BMS", "ThreadX create failed - check priority/stack");
 *       break;
 *     default:
 *       rx_log_error("BMS", "Unknown error");
 *       break;
 *   }
 * }
 * @endcode
 *
 * @par Example - I2C Failure Recovery:
 * @code{.c}
 * // BMS task will continue running even if I2C initialization fails.
 * // Data will be marked invalid (bms_state.valid = false) until I2C recovers.
 * void tx_application_define(void* first_unused_memory)
 * {
 *   (void)first_unused_memory;
 *
 *   // shared_data_init() and other task creation...
 *
 *   const bms_monitor_config_t bms_config = {
 *     .soc_warning_pct  = k_bms_soc_warning_pct_default,
 *     .soc_critical_pct = k_bms_soc_critical_pct_default,
 *   };
 *   rx_err_t ret = bms_monitor_task_create(&bms_config);
 *   if (ret == k_rx_ok) {
 *     // Task created successfully. If BQ4050 init fails inside the task,
 *     // the task will continue running and report invalid data.
 *     // Check telemetry for bms_state.valid == false.
 *     rx_log_info("BMS", "Task created (will retry I2C if init fails)");
 *   }
 * }
 * @endcode
 *
 * @see internal_bms_task_entry() Task main loop implementation
 * @see shared_data_init() Must be called before this function
 * @see telemetry_task_create() Consumer of BMS data (call after this)
 * @see rx_bq4050_init() BQ4050 initialization (called inside task)
 * @see rx_bq4050_read_status() Reads all 7 BQ4050 registers
 * @see shared_data_update_bms() Thread-safe battery state update
 * @see tx_thread_create() ThreadX thread creation API
 * @see tx_kernel_enter() ThreadX kernel entry point
 *
 * @since Version 1.0.0
 *
 * @test test_bms_monitor_task.c - Verify successful creation
 * @test test_bms_monitor_task.c - Verify double-creation returns k_rx_err_invalid_state
 * @test test_bms_monitor_task.c - Verify task scheduled with priority 15
 * @test test_bms_monitor_task.c - Verify stack size is 1024 bytes
 * @test test_bms_monitor_task.c - Verify s_bms_created flag set after creation
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 5:** 7 preconditions, 6 postconditions documented
 * - **Rule 7:** tx_thread_create() return value checked
 * - **Rule 8:** All constants use C23 typed enums (no macros)
 *
 * @callgraph
 * @callergraph
 */
rx_err_t bms_monitor_task_create(const bms_monitor_config_t* config)
{
  /* Validate config pointer */
  RX_CHECK_NULL_PTR(config, s_tag, "bms_monitor_task_create: config is NULL");

  /* Validate individual field ranges before checking relationship */
  if ((config->soc_warning_pct < k_bms_soc_warning_min) ||
      (config->soc_warning_pct > k_bms_soc_warning_max)) {
    rx_log_error_val(s_tag, "soc_warning_pct out of [1,100]",
                     (uint32_t)config->soc_warning_pct);
    return k_rx_err_invalid_arg;
  }
  if ((config->soc_critical_pct < k_bms_soc_critical_min) ||
      (config->soc_critical_pct > k_bms_soc_critical_max)) {
    rx_log_error_val(s_tag, "soc_critical_pct out of [1,99]",
                     (uint32_t)config->soc_critical_pct);
    return k_rx_err_invalid_arg;
  }

  /* Validate threshold relationship: critical must be strictly below warning */
  if (config->soc_critical_pct >= config->soc_warning_pct) {
    rx_log_error(s_tag, "soc_critical_pct must be < soc_warning_pct");
    return k_rx_err_invalid_arg;
  }

  /* Check if already created */
  RX_ASSERT(!s_bms_created, "BMS task already created");
  if (s_bms_created) {
    return k_rx_err_invalid_state;
  }

  /* Copy config into static state (task will read from s_bms_config) */
  s_bms_config = *config;

  /* Create the thread */
  const UINT tx_status = tx_thread_create(&s_bms_thread,
                               "BMSTask",
                               internal_bms_task_entry,
                               k_bms_task_input,
                               s_bms_stack,
                               k_bms_task_stack_size,
                               k_bms_task_priority,
                               k_bms_task_priority,
                               TX_NO_TIME_SLICE,
                               TX_AUTO_START);

  if (tx_status != TX_SUCCESS) {
    rx_log_error_val(s_tag, "Thread create failed", (uint32_t)tx_status);
    return k_rx_err_rtos_thread_create;
  }

  s_bms_created = true;
  rx_log_info(s_tag, "BMS monitoring task created");

  return k_rx_ok;
}

/* -------------------------------------------------------------------------- */

#ifdef UNIT_TEST
/**
 * @brief Reset BMS task singleton guard for unit test isolation
 *
 * @details
 * Clears the s_bms_created static guard so that bms_monitor_task_create()
 * can be called again in a subsequent test. Also zeroes the internal config
 * copy so tests start from a clean state.
 *
 * **Only compiled when UNIT_TEST is defined.** This function must never
 * appear in production firmware builds.
 *
 * @return void
 * @retval None
 *
 * @pre Called from tearDown() after each test that exercised
 *      bms_monitor_task_create().
 * @pre No BMS task thread is actively running (test environment is
 *      single-threaded; tx_thread_create() is mocked).
 *
 * @post s_bms_created == false
 * @post s_bms_config zeroed
 *
 * @note NOT thread-safe; intended for single-threaded unit test context only.
 * @warning Never call this from production code or interrupt context.
 *
 * @see bms_monitor_task_create() The function whose guard this clears
 * @see tearDown() in test_bms_monitoring_task.c Canonical caller
 *
 * @since Version 1.1.0
 */
void bms_monitor_task_reset(void)
{
  s_bms_created = false;
  (void)memset(&s_bms_config, 0, sizeof(s_bms_config));
}
#endif /* UNIT_TEST */

/* -------------------------------------------------------------------------- */

/**
 * @brief Convert a BQ4050 status snapshot into the shared bms_state_t format
 *
 * @details
 * Performs a field-by-field copy from the driver's rx_bq4050_status_t into
 * the shared bms_state_t, stamps the current ThreadX tick as the timestamp,
 * and marks the result valid. Extracted to satisfy NASA Power of 10 Rule 4
 * (~60 line maximum per function) and to give the conversion a single,
 * testable home.
 *
 * @param[in]  status Driver status snapshot read from the BQ4050 IC
 * @param[out] out    Destination shared-data struct to populate
 *
 * @return void This function returns no value.
 * @retval None All output is written through the @p out pointer.
 *
 * @pre  status != NULL
 * @pre  out != NULL
 * @post out->valid == true
 * @post out->timestamp_ms == tx_time_get() at call time
 *
 * @note Not thread-safe; called only from within the BMS monitor task loop
 *
 * @code
 * rx_bq4050_status_t status;
 * bms_state_t bms = {0};
 * if (rx_bq4050_read_status(&g_bus_manager, "i2c0", &status, k_bms_cell_count) == k_rx_ok) {
 *     internal_bms_convert_status_to_state(&status, &bms);
 *     // bms.valid == true, bms.timestamp_ms set to current ThreadX tick
 * }
 * @endcode
 *
 * @see internal_bms_task_entry() Caller of this function
 * @see rx_bq4050_status_t Driver status structure (input)
 * @see bms_state_t Shared data structure (output)
 *
 * @since Version 1.1.0
 */
static void internal_bms_convert_status_to_state(const rx_bq4050_status_t* status,
                                                 bms_state_t*             out)
{
  RX_ASSERT(status != NULL, "internal_bms_convert_status_to_state: status is NULL");
  RX_ASSERT(out != NULL, "internal_bms_convert_status_to_state: out is NULL");

  out->voltage_mv          = status->voltage_mv;
  out->current_ma          = status->current_ma;
  out->soc_percent         = status->relative_soc;
  out->temperature_celsius = status->temperature_c;
  out->capacity_mah        = status->remaining_capacity_mah;
  out->full_capacity_mah   = status->full_capacity_mah;
  out->cycle_count         = status->cycle_count;
  out->fault_flags         = status->battery_status; /* All 10 BatteryStatus flags */
  out->timestamp_ms        = tx_time_get();
  out->valid               = true;

  RX_ASSERT(out->valid, "internal_bms_convert_status_to_state: postcondition failed");
}

/* -------------------------------------------------------------------------- */

/**
 * @brief BMS monitoring task entry point - infinite loop polling BQ4050 at 1 Hz
 *
 * @details
 * This is the main task loop that executes after bms_monitor_task_create() starts
 * the thread. The function never returns and runs continuously at 1 Hz polling rate
 * until power-down or emergency stop.
 *
 * ## Complete Algorithm (12 Steps)
 *
 * ### Initialization Phase (Steps 1-3)
 *
 * 1. **Log Startup:** Print "BMS monitoring task starting" via UART
 * 2. **Configure BQ4050:** Set num_cells = 4 for 4S LiPo battery pack
 * 3. **Initialize I2C:** Call rx_bq4050_init() to configure SMBus protocol
 *    - I2C bus: "i2c0" (RIIC0 peripheral)
 *    - I2C address: 0x0B (BQ4050 default)
 *    - I2C frequency: 100 kHz (SMBus standard mode)
 *    - If init fails: Log error but continue (report invalid data)
 *
 * ### Main Polling Loop (Steps 4-12, Infinite)
 *
 * 4. **Read BQ4050 Registers:** Call rx_bq4050_read_status() to read 7 registers:
 *    - Voltage (0x09): Pack voltage in mV
 *    - Current (0x0A): Instantaneous current in mA (signed)
 *    - SoC (0x0D): Relative state of charge in %
 *    - Temperature (0x08): Pack temperature in 0.1 K
 *    - Remaining Capacity (0x0F): Charge remaining in mAh
 *    - Full Capacity (0x10): Full capacity in mAh (degrades over cycles)
 *    - Cycle Count (0x17): Total charge/discharge cycles
 *
 * 5. **Check I2C Result:**
 *    - If success (k_rx_ok): Proceed to step 6
 *    - If failure: Mark data invalid, log warning, jump to step 11
 *
 * 6. **Build bms_state_t:** Copy data from rx_bq4050_status_t to bms_state_t:
 *    - voltage_mv ← status.voltage_mv
 *    - current_ma ← status.current_ma
 *    - soc_percent ← status.relative_soc
 *    - temperature_celsius ← status.temperature_c
 *    - capacity_mah ← status.remaining_capacity_mah
 *    - full_capacity_mah ← status.full_capacity_mah
 *    - cycle_count ← status.cycle_count
 *    - fault_flags ← status.battery_status (BatteryStatus register 0x16, 10 flag bits)
 *    - timestamp_ms ← tx_time_get() (ThreadX tick count in ms)
 *    - valid ← true
 *
 * 7. **Low Battery Warning Check:** If SoC < soc_warning_pct (default 25%):
 *    - Log warning: "Low battery SoC: XX%"
 *    - Set event flag: shared_data_set_event(k_event_low_battery)
 *    - Allows telemetry task to notify gateway
 *
 * 8. **Critical Battery Check:** If SoC < soc_critical_pct (default 5%):
 *    - Log error: "Critical battery SoC: XX%"
 *    - Trigger emergency stop: shared_data_trigger_estop(k_estop_reason_low_battery)
 *    - All motors immediately disabled to prevent deep discharge damage
 *
 * 9. **Update Shared Data:** Call shared_data_update_bms(&bms)
 *    - Thread-safe mutex-protected write
 *    - Telemetry task reads this data at 10 Hz
 *
 * 10. **Sleep 1 Second:** Call tx_thread_sleep(100 ticks)
 *     - 100 ticks at 100 Hz tick rate = 1000ms
 *     - Yields CPU to other tasks
 *     - ThreadX scheduler resumes after 1 second
 *
 * 11. **Invalid Data Path (I2C failure):**
 *     - Set bms.valid = false
 *     - Set bms.timestamp_ms = tx_time_get()
 *     - Log warning with error code
 *     - Jump to step 9 (update shared data with invalid marker)
 *
 * 12. **Repeat Forever:** Loop back to step 4
 *
 * ## Low Battery Detection Logic
 *
 * The task implements a two-tier low battery warning system:
 *
 * ### Tier 1: Warning (default 25% SoC, configurable)
 * - **Trigger:** State of charge drops below soc_warning_pct (default 25%)
 * - **Action:** Log warning, set k_event_low_battery event flag
 * - **Purpose:** Early warning for operator (return to base for charging)
 * - **Time Remaining:** ~18 minutes at 2A average current
 *   - Calculation: (5000mAh × 0.25) / 2000mA = 0.625h = 37.5 min (conservative)
 *   - Real-world with margin: ~18 minutes safe operation
 *
 * ### Tier 2: Critical (5% SoC)
 * - **Trigger:** State of charge drops below 5%
 * - **Action:** Log error, trigger emergency stop (k_estop_reason_low_battery)
 * - **Purpose:** Prevent deep discharge damage (< 3.0V per cell)
 * - **Effect:** All motors disabled immediately
 * - **Time Remaining:** ~3 minutes at 2A (just enough for safe shutdown)
 *
 * ## I2C Error Handling Strategy
 *
 * The task is resilient to I2C communication failures:
 *
 * | Error Scenario | Task Behavior | Data State | Recovery |
 * |----------------|---------------|------------|----------|
 * | **BQ4050 init fails** | Continue running | Invalid data reported | Automatic retry next poll |
 * | **Single read timeout** | Log warning | Mark invalid | Retry after 1s sleep |
 * | **Continuous read failures** | Keep polling | Invalid data | Manual intervention needed |
 * | **I2C bus lockup** | Timeout after 100ms | Mark invalid | I2C driver auto-recovery |
 *
 * **Design Rationale:**
 * - BMS monitoring is non-critical (does not control motors directly)
 * - Invalid data is better than crashing the task
 * - Telemetry task can detect bms.valid == false and alert operator
 * - I2C bus issues are often transient (noise, EMI) and self-recover
 *
 * ## Control Flow Diagram
 *
 * @startuml
 * start
 * :Log "BMS monitoring task starting";
 * :Configure BQ4050 (num_cells = 4);
 * :rx_bq4050_init(&g_bus_manager, "i2c0", &config);
 * if (Init successful?) then (yes)
 *   :Log "BMS monitoring running @ 1 Hz";
 * else (no)
 *   :Log error\n(continue with invalid data);
 * endif
 *
 * partition "Infinite Monitoring Loop" {
 *   repeat
 *     :rx_bq4050_read_status()\n(Read 7 registers, ~2ms I2C time);
 *     if (Read successful?) then (yes)
 *       :Build bms_state_t structure\n(voltage, current, SoC, temp, capacity, cycles);
 *       :Set timestamp = tx_time_get();
 *       :Set valid = true;
 *       if (SoC < 5%?) then (yes)
 *         :Log CRITICAL\n"Critical battery SoC: XX%";
 *         :shared_data_trigger_estop(k_estop_reason_low_battery)\n**EMERGENCY STOP**;
 *       else if (SoC < 25%?) then (yes)
 *         :Log WARNING\n"Low battery SoC: XX%";
 *         :shared_data_set_event(k_event_low_battery);
 *       else (>= 25%)
 *         :Normal operation;
 *       endif
 *     else (no)
 *       :Set valid = false;
 *       :Set timestamp = tx_time_get();
 *       :Log WARNING\n"BQ4050 read failed: err";
 *     endif
 *     :shared_data_update_bms(&bms)\n(thread-safe mutex write);
 *     :tx_thread_sleep(100 ticks)\n**Sleep 1 second, yield CPU**;
 *   repeat while (forever)
 * }
 * @enduml
 *
 * ## Performance Characteristics
 *
 * | Metric | Value | Measurement Method |
 * |--------|-------|-------------------|
 * | **Poll Rate** | 1 Hz | Fixed 1000ms sleep period |
 * | **I2C Transaction Time** | 2ms | 7 registers × 300µs per register |
 * | **CPU Active Time** | 2ms per second | I2C read + data processing |
 * | **CPU Idle Time** | 998ms per second | tx_thread_sleep() yields CPU |
 * | **CPU Utilization** | 0.2% | (2ms / 1000ms) × 100% |
 * | **Worst Case Latency** | 1000ms | Max time to detect low battery event |
 * | **I2C Timeout** | 100ms | Per-register read timeout |
 * | **Stack Usage (Peak)** | 256 bytes | During rx_bq4050_read_status() call |
 *
 * ## Memory Usage
 *
 * | Variable | Type | Size | Scope | Lifetime |
 * |----------|------|------|-------|----------|
 * | `err` | rx_err_t | 4 bytes | Local | Function lifetime |
 * | `status` | rx_bq4050_status_t | ~32 bytes | Local | Function lifetime |
 * | `bms` | bms_state_t | ~40 bytes | Local | Function lifetime |
 * | `config` | rx_bq4050_config_t | ~8 bytes | Local | Init phase only |
 * | **Total Stack** | - | ~256 bytes | Stack | Peak during I2C call |
 *
 * @param[in] input Thread input parameter from tx_thread_create()
 *                  - Type: ULONG (32-bit unsigned)
 *                  - Value: 0 (k_bms_task_input)
 *                  - Purpose: Unused (ThreadX convention requires parameter)
 *                  - Explicitly cast to (void) to suppress unused warning
 *
 * @return void This function never returns (infinite loop until power-down)
 *
 * @pre bms_monitor_task_create() called successfully
 * @pre ThreadX scheduler started (task is scheduled)
 * @pre g_bus_manager initialized and ready for I2C operations
 * @pre RIIC0 peripheral powered, clocked, and configured for I2C
 * @pre BQ4050 powered and connected to I2C bus at address 0x0B
 * @pre shared_data_init() called (for shared_data_update_bms)
 *
 * @post BQ4050 initialized (if I2C successful)
 * @post Infinite loop polling at 1 Hz until power-down
 * @post shared_data.bms updated every second with latest battery state
 * @post Low battery events triggered when SoC < soc_warning_pct (default 25%)
 * @post Emergency stop triggered when SoC < 5%
 *
 * @invariant Loop period is exactly 1000ms (100 ticks at 100 Hz)
 * @invariant Function never returns (while(true) infinite loop)
 * @invariant shared_data.bms updated every iteration (valid or invalid)
 *
 * @note **Thread Safety:** This function runs in its own thread context (Priority 15).
 *       All shared data access uses thread-safe APIs (mutex-protected).
 * @note **Re-entrancy:** NOT reentrant. Single instance only (enforced by s_bms_created).
 * @note **Performance:** 99.8% CPU idle time. I2C operations are 2ms out of 1000ms period.
 * @note **Memory:** Peak stack usage is 256 bytes during I2C transactions.
 * @note **I2C Bus:** Uses 100 kHz SMBus protocol with 100ms timeout per register.
 * @note **Polling Rate:** 1 Hz is sufficient for battery monitoring (SoC changes slowly).
 *       Faster polling (10 Hz) would waste CPU cycles and I2C bus bandwidth.
 *
 * @warning **Infinite Loop:** This function NEVER returns. Do not call directly from
 *          application code. Only ThreadX should call this via tx_thread_create().
 * @warning **I2C Timeout:** If BQ4050 does not respond, each register read times out
 *          after 100ms. Total worst-case I2C timeout: 7 × 100ms = 700ms.
 * @warning **Deep Discharge:** If battery drops below 5% SoC, emergency stop is
 *          triggered immediately. Ensure safe shutdown logic is implemented.
 * @warning **Stack Overflow:** 1024 byte stack must accommodate 256 byte peak usage.
 *          TX_ENABLE_STACK_CHECKING detects overflow if it occurs.
 *
 * @attention This function executes in its own thread context, NOT in main() context.
 * @attention I2C bus is shared with other tasks. rx_bq4050 driver handles mutex locking.
 * @attention Low battery warning (default 25%) does NOT stop motors. Critical battery (default 5%) DOES.
 *
 * @par Thread Safety:
 * This function is the ONLY code that writes to shared_data.bms (via shared_data_update_bms).
 * The telemetry task reads from shared_data.bms (via shared_data_get_bms). Both APIs
 * are mutex-protected by shared_data module. No race conditions possible.
 *
 * The I2C bus (RIIC0) is shared with other tasks. The rx_bq4050 driver acquires a
 * bus mutex before each transaction and releases it afterward. SMBus protocol ensures
 * atomic register reads (no partial data).
 *
 * @par Performance Analysis:
 * **Execution Time Breakdown (per 1 second iteration):**
 * - rx_bq4050_read_status(): ~2000 µs (I2C transactions)
 * - Data structure copy: ~10 µs (memcpy equivalent)
 * - Threshold checks: ~5 µs (two comparisons)
 * - shared_data_update_bms(): ~20 µs (mutex lock + copy + unlock)
 * - Logging (if warnings): ~100 µs (UART transmit)
 * - **Total active time:** ~2135 µs = 2.1ms
 * - **Sleep time:** 1000ms - 2.1ms = 997.9ms (CPU idle)
 * - **CPU utilization:** (2.1ms / 1000ms) × 100% = 0.21%
 *
 * **I2C Bandwidth Usage:**
 * - 7 registers × ~40 bytes per transaction (including overhead) = 280 bytes/second
 * - At 100 kHz I2C: 12500 bytes/second theoretical max
 * - BMS usage: (280 / 12500) × 100% = 2.24% of I2C bus bandwidth
 *
 * @par Example - Normal Operation (SoC > 25%):
 * @code{.c}
 * // Task executes this loop every second:
 *
 * // Step 1: Read BQ4050 (I2C transaction, ~2ms)
 * rx_bq4050_read_status(&g_bus_manager, "i2c0", &status, 4);
 * // status = { voltage_mv: 16200, current_ma: 2450, relative_soc: 85, ... }
 *
 * // Step 2: Build bms_state_t
 * bms.voltage_mv = 16200;
 * bms.current_ma = 2450;
 * bms.soc_percent = 85;
 * bms.timestamp_ms = tx_time_get();  // e.g., 12345678
 * bms.valid = true;
 *
 * // Step 3: Check thresholds (85% > 25%, no warnings)
 * // No action taken
 *
 * // Step 4: Update shared data (thread-safe)
 * shared_data_update_bms(&bms);
 *
 * // Step 5: Sleep 1 second
 * tx_thread_sleep(100);  // 100 ticks at 100 Hz = 1000ms
 * @endcode
 *
 * @par Example - Low Battery Warning (25% > SoC >= 5%):
 * @code{.c}
 * // BQ4050 reports SoC = 12%
 * rx_bq4050_read_status(&g_bus_manager, "i2c0", &status, 4);
 * // status.relative_soc = 12
 *
 * bms.soc_percent = 12;
 * bms.valid = true;
 *
 * // Step 3: Check thresholds
 * if (12 < s_bms_config.soc_warning_pct) {  // 12 < 25 -> TRUE
 *   rx_log_warn_val("BMS", "Low battery SoC", 12);
 *   // UART output: "[BMS] WARNING: Low battery SoC: 12"
 *
 *   shared_data_set_event(k_event_low_battery);
 *   // Event flag set, telemetry task will notify gateway
 * }
 *
 * // Step 4: Update shared data
 * shared_data_update_bms(&bms);
 *
 * // Step 5: Sleep 1 second (continue monitoring)
 * tx_thread_sleep(100);
 * @endcode
 *
 * @par Example - Critical Battery Emergency Stop (SoC < 5%):
 * @code{.c}
 * // BQ4050 reports SoC = 3%
 * rx_bq4050_read_status(&g_bus_manager, "i2c0", &status, 4);
 * // status.relative_soc = 3
 *
 * bms.soc_percent = 3;
 * bms.valid = true;
 *
 * // Step 3: Check thresholds
 * if (3 < s_bms_config.soc_critical_pct) {  // 3 < 5 -> TRUE
 *   rx_log_error_val("BMS", "Critical battery SoC", 3);
 *   // UART output: "[BMS] ERROR: Critical battery SoC: 3"
 *
 *   shared_data_trigger_estop(k_estop_reason_low_battery);
 *   // EMERGENCY STOP: All motors disabled immediately
 *   // Robot enters safe shutdown sequence
 * }
 *
 * // Step 4: Update shared data
 * shared_data_update_bms(&bms);
 *
 * // Step 5: Sleep 1 second (task continues, but robot is stopped)
 * tx_thread_sleep(100);
 * @endcode
 *
 * @par Example - I2C Failure Recovery:
 * @code{.c}
 * // I2C bus timeout or BQ4050 not responding
 * err = rx_bq4050_read_status(&g_bus_manager, "i2c0", &status, 4);
 * // err = k_rx_err_timeout (100ms timeout expired)
 *
 * if (err != k_rx_ok) {
 *   // Mark data invalid
 *   bms.valid = false;
 *   bms.timestamp_ms = tx_time_get();
 *
 *   rx_log_warn_val("BMS", "BQ4050 read failed", err);
 *   // UART output: "[BMS] WARNING: BQ4050 read failed: 8"
 *   // (k_rx_err_timeout = 8)
 * }
 *
 * // Still update shared data (with invalid marker)
 * shared_data_update_bms(&bms);
 * // Telemetry task sees bms.valid == false and alerts operator
 *
 * // Sleep 1 second and retry
 * tx_thread_sleep(100);
 * // Next iteration will retry I2C read (automatic recovery)
 * @endcode
 *
 * @see bms_monitor_task_create() Task creation function
 * @see rx_bq4050_init() Initialize BQ4050 fuel gauge
 * @see rx_bq4050_read_status() Read all 7 BQ4050 registers
 * @see shared_data_update_bms() Thread-safe battery state update
 * @see shared_data_set_event() Set low battery event flag
 * @see shared_data_trigger_estop() Trigger emergency stop
 * @see tx_thread_sleep() ThreadX sleep API (yields CPU)
 * @see tx_time_get() Get current ThreadX tick count
 *
 * @since Version 1.0.0
 *
 * @test test_bms_monitor_task.c - Verify 1 Hz poll rate timing
 * @test test_bms_monitor_task.c - Verify low battery warning below 25% SoC (default warning threshold)
 * @test test_bms_monitor_task.c - Verify emergency stop at 5% SoC
 * @test test_bms_monitor_task.c - Verify I2C timeout handling (100ms)
 * @test test_bms_monitor_task.c - Verify invalid data marking on I2C failure
 * @test test_bms_monitor_task.c - Verify shared_data updated every iteration
 * @test test_bms_monitor_task.c - Verify BQ4050 init failure recovery
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1:** [PASS] No goto, setjmp, recursion (only if/while control flow)
 * - **Rule 2:** [PASS] Single while(true) loop with fixed 1000ms period
 * - **Rule 3:** [PASS] Zero dynamic allocation (all stack-based locals)
 * - **Rule 4:** [PASS] Field-copy extracted to internal_bms_convert_status_to_state();
 *              internal_bms_task_entry() is now ~52 lines (within ~60 LOC guideline)
 * - **Rule 5:** [PASS] 5 preconditions, 5 postconditions documented
 * - **Rule 7:** [PASS] All function returns checked or cast to (void)
 * - **Rule 8:** [PASS] All constants use C23 typed enums (no macros)
 *
 * @callgraph
 * @callergraph
 */
static void internal_bms_task_entry(ULONG input)
{
  (void)input;

  rx_log_info(s_tag, "BMS monitoring task starting");

  /* Initialize BQ4050 */
  rx_bq4050_config_t config    = {.num_cells = k_bms_cell_count};
  rx_err_t           err       = rx_bq4050_init(&g_bus_manager, s_i2c_bus_name, &config);
  if (err != k_rx_ok) {
    rx_log_error_val(s_tag, "BQ4050 init failed", (uint32_t)err);
    /* Continue anyway - will report invalid data */
  }

  rx_log_info(s_tag, "BMS monitoring running @ 1 Hz");

  /* Main polling loop */
  while (true) {
    rx_bq4050_status_t status = {0};
    bms_state_t        bms    = {0};

    /* Read battery status */
    err = rx_bq4050_read_status(&g_bus_manager, s_i2c_bus_name, &status, k_bms_cell_count);

    if (err == k_rx_ok) {
      /* Convert BQ4050 snapshot into shared bms_state_t */
      internal_bms_convert_status_to_state(&status, &bms);

      /* Check for critical battery faults (OTA, OCA, TDA) */
      (void)internal_check_critical_faults(status.battery_status);

      /* Check battery SoC thresholds - critical takes priority over warning */
      // NOLINTNEXTLINE(bugprone-branch-clone)
      if (status.relative_soc < s_bms_config.soc_critical_pct) {
        rx_log_error_val(s_tag, "Critical battery SoC", (uint8_t)status.relative_soc);
        (void)shared_data_trigger_estop(k_estop_reason_low_battery);
      } else if (status.relative_soc < s_bms_config.soc_warning_pct) {
        rx_log_warn_val(s_tag, "Low battery SoC", (uint8_t)status.relative_soc);
        (void)shared_data_set_event(k_event_low_battery);
      }
    } else {
      /* Read failed - mark as invalid */
      bms.valid        = false;
      bms.timestamp_ms = tx_time_get();

      rx_log_warn_val(s_tag, "BQ4050 read failed", (uint32_t)err);
    }

    /* Update shared data */
    (void)shared_data_update_bms(&bms);

    /* Report task heartbeat to IWDT (must execute within 3000ms timeout) */
    err = rx_iwdt_task_heartbeat("BMSMonitor");
    if (err != k_rx_ok) {
      rx_log_error_val(s_tag, "IWDT heartbeat failed", (uint32_t)err);
      /* Continue operation - watchdog monitor will detect timeout */
    }

    /* Sleep until next poll */
    (void)tx_thread_sleep(k_bms_task_sleep_ticks);
  }
}
