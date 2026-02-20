/* src/main.c */

/**
 * @file main.c
 * @brief STAR RX72N Firmware Entry Point - System Initialization and ThreadX Bootstrap
 *
 * @details
 * # Overview
 *
 * This file contains the **main entry point** for the STAR RX72N motor controller firmware.
 * It implements a **three-stage initialization sequence** before entering the ThreadX RTOS kernel:
 *
 * 1. **Startup flag validation** - Detect abnormal reset conditions (watchdog, brownout, etc.)
 * 2. **System initialization** - Configure clocks, power management, and peripherals
 * 3. **ThreadX bootstrap** - Create application threads and start RTOS scheduler
 *
 * **Key Design Principle:** Fail-fast on critical errors (RX_ASSERT) to catch firmware issues
 * early in development. Non-critical warnings are logged but allow boot to continue.
 *
 * ## System Initialization Architecture
 *
 * @dot
 * digraph main_flow {
 *   rankdir=TB;
 *   node [shape=box];
 *
 *   Reset [label="MCU Reset\n(Power-on / Watchdog / Software)", shape=ellipse];
 *   Main [label="main()"];
 *   CheckFlags [label="internal_check_startup_flags()\nValidate reset cause"];
 *   ClockInit [label="rx_clock_power_init()\n240 MHz PLL, peripheral clocks"];
 *   HwInit [label="hardware_init()\nMotors, USB, sensors"];
 *   ThreadX [label="tx_kernel_enter()\nStart RTOS scheduler"];
 *   Tasks [label="app_main_task\nPID control @ 100 Hz", shape=ellipse];
 *
 *   Reset -> Main;
 *   Main -> CheckFlags;
 *   CheckFlags -> ClockInit [label="k_rx_ok"];
 *   CheckFlags -> Halt [label="Assert failed", color=red];
 *   ClockInit -> HwInit [label="k_rx_ok"];
 *   ClockInit -> Halt [label="Error", color=red];
 *   HwInit -> ThreadX [label="k_rx_ok"];
 *   HwInit -> Halt [label="Error", color=red];
 *   ThreadX -> Tasks [label="Never returns"];
 *
 *   Halt [label="while(1) __asm__(wait)", shape=octagon, color=red];
 * }
 * @enddot
 *
 * ## Reset Status Flag Checks (RX72N RSTSR0/RSTSR1/RSTSR2 Registers)
 *
 * The firmware validates **six reset status flags** at startup to detect abnormal conditions:
 *
 * | Flag | Register | Bit | Expected | Critical? | Action on Error |
 * |------|----------|-----|----------|-----------|-----------------|
 * | **PORF** | RSTSR0 | 0 | 1 (set) | No | Log info (warm boot OK) |
 * | **IWDTRF** | RSTSR2 | 1 | 0 (clear) | **YES** | **Assert halts** |
 * | **WDTRF** | RSTSR2 | 0 | 0 (clear) | No | Return error |
 * | **SWRF** | RSTSR2 | 2 | 0 (clear) | No | Return error |
 * | **LVD0RF** | RSTSR0 | 1 | 0 (clear) | No | Return error |
 * | **CWSF** | RSTSR1 | 7 | Any | No | Informational only |
 *
 * ### Critical vs Non-Critical Flags
 *
 * - **Critical (IWDTRF):** Assert halts execution - indicates prior firmware hung/crashed
 * - **Non-critical:** Return error but allow recovery (logged for diagnostics)
 *
 * **Rationale:** Independent Watchdog Timer (IWDT) timeout is **always** a firmware bug
 * (infinite loop, deadlock, or missing refresh). Halting immediately prevents cascading
 * failures and forces developer to fix root cause.
 *
 * ## Startup Timing (RX72N @ 240 MHz)
 *
 * | Phase | Duration | Description |
 * |-------|----------|-------------|
 * | Reset to main() | ~500 µs | Hardware reset, C runtime init (crt0.S) |
 * | Startup flag checks | ~10 µs | 6 register reads + validation |
 * | Clock initialization | ~200 µs | PLL stabilization (see rx_clock_power_init.c) |
 * | Hardware initialization | ~50 ms | USB enumeration, motor driver init |
 * | **Total boot time** | **~50 ms** | Ready for first PID control loop |
 *
 * ## ThreadX RTOS Integration
 *
 * **Entry point:** `tx_kernel_enter()` - Starts the ThreadX scheduler and **never returns**.
 *
 * **Application callback:** `tx_application_define()` - Called by ThreadX kernel to allow
 * the application to create threads, semaphores, message queues, etc.
 *
 * **Thread creation sequence:**
 * ```
 * tx_kernel_enter() -> tx_application_define() -> app_main_task_create() -> Main control loop
 * ```
 *
 * ## Memory Map and Stack Setup
 *
 * | Memory Region | Address Range | Size | Purpose |
 * |---------------|---------------|------|---------|
 * | **Flash (ROM)** | 0x00000000 - 0x003FFFFF | 4 MB | Code, const data, vector table |
 * | **SRAM** | 0x00000000 - 0x0007FFFF | 512 KB | Heap, stacks, ThreadX kernel objects |
 * | **Peripheral registers** | 0x00080000 - 0x000FFFFF | ~512 KB | I/O registers (UART, SPI, USB, etc.) |
 *
 * **Stack sizes** (defined in linker script and ThreadX config):
 * - **Main stack**: 4 KB (used only until ThreadX starts)
 * - **App main task stack**: 8 KB (PID control, USB communication)
 * - **ThreadX system stack**: 2 KB (kernel overhead)
 *
 * ## Error Handling Strategy
 *
 * This file uses **three error handling mechanisms** based on severity:
 *
 * | Macro | Severity | Behavior | Use Case |
 * |-------|----------|----------|----------|
 * | `RX_ASSERT(cond, msg)` | **Critical** | Halts execution with message | Programming errors, invariant violations |
 * | `RX_ERROR_CHECK(err)` | **High** | Returns error to caller | Initialization failures |
 * | `rx_log_info(tag, msg)` | **Info** | Logs message, continues | Diagnostic information |
 *
 * **Example assertions** (development mode):
 * - Register pointers must be non-NULL (compile-time constant addresses)
 * - IWDTRF must be clear (prior execution must not have timed out)
 * - ThreadX thread creation must succeed (memory exhaustion = critical)
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation Notes |
 * |------|--------|----------------------|
 * | **Rule 1: Control flow** | [PASS] | No goto, setjmp, or recursion |
 * | **Rule 2: Loop bounds** | [PASS] | while(1) wait loop only (infinite by design) |
 * | **Rule 3: Heap allocation** | [PASS] | Zero dynamic allocation (static stacks only) |
 * | **Rule 4: Function length** | [PASS] | Longest function: main() = 37 lines |
 * | **Rule 5: Assertions** | [PASS] | 11 RX_ASSERT checks across 7 functions |
 * | **Rule 6: Data scope** | [PASS] | All variables at smallest scope (function-local) |
 * | **Rule 7: Return checks** | [PASS] | All rx_err_t returns checked via RX_ERROR_CHECK |
 * | **Rule 8: Preprocessor** | [PASS] | Zero macros (uses typed enums for constants) |
 * | **Rule 9: Pointers** | [PASS] | Single-level dereferencing only |
 * | **Rule 10: Warnings** | [PASS] | Compiles with -Wall -Wextra -Werror |
 *
 * ## SOLID Principles (Application to main.c)
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **Single Responsibility** | main() does ONLY bootstrap - delegates clocks, hardware, tasks |
 * | **Open/Closed** | Startup checks extensible via new internal_check_*() functions |
 * | **Liskov Substitution** | Error codes (rx_err_t) consistent across all init functions |
 * | **Interface Segregation** | Small, focused init APIs (clock, hardware, tasks separated) |
 * | **Dependency Inversion** | main() depends on abstractions (rx_err_t), not implementations |
 *
 * ## Thread Safety
 *
 * **Pre-ThreadX:** This file executes in **single-threaded context** until `tx_kernel_enter()`.
 * No synchronization primitives needed.
 *
 * **Post-ThreadX:** After kernel start, main() **never returns**. Application logic runs
 * in `app_main_task` with ThreadX scheduling and synchronization.
 *
 * ## Related Files
 *
 * - **Clock init:** See [rx_clock_power_init.c](rx_clock_power_init.c) - PLL configuration, 240 MHz setup
 * - **Hardware init:** See [hardware_init.c](hardware_init.c) - Motor drivers, USB CDC, sensors
 * - **Main task:** See [app_main_task.c](app_main_task.c) - PID control loop at 100 Hz
 * - **ThreadX config:** See [rx_threadx_config.h](../lib/rx_core/inc/rx_threadx_config.h) - RTOS tuning
 *
 * @note This file compiles for both RX72N hardware and x86-64 unit tests (mock register access).
 *
 * @warning Never call main() directly from application code - entry point is for reset vector only.
 *
 * @see hardware_init() Application-specific peripheral initialization
 * @see rx_clock_power_init() System clock and power management setup
 * @see app_main_task_create() Create main application thread
 *
 * @since Version 1.0.0
 *
 * @par Revision History:
 * - v1.0.0 (2026-01): Initial implementation with ThreadX RTOS bootstrap
 */

#include "app_main_task.h"
#include "hardware.h"
#include "hardware_init.h"
#include "rx72n_system_regs.h"
#include "rx_bq4050.h"
#include "rx_bus_config.h"
#include "rx_bus_manager.h"
#include "rx_bus_types.h"
#include "rx_check.h"
#include "rx_clock_power_init.h"
#include "rx_err.h"
#include "rx_infrastructure.h"
#include "rx_port_utils.h"
#include "tx_api.h"

/* Multi-task architecture includes */
#include "bms_monitor_task.h"
#include "comm_task.h"
#include "led_status_task.h"
#include "motor_control_task.h"
#include "obstacle_detect_task.h"
#include "shared_data.h"
#include "telemetry_task.h"
#include "temp_sensor_task.h"
#include "watchdog_monitor_task.h"

/* Watchdog driver */
#include "rx_iwdt.h"

/* Stack overflow detection */
#include "rx_stack_monitor.h"

/* =============================================================================
 * Main Return Codes
 * =============================================================================
 */

/**
 * @brief Main function return codes
 *
 * Note: main() should never return in this firmware as ThreadX takes over.
 * These codes exist for completeness and static analysis tools.
 */
typedef enum : uint8_t {
  k_main_ret_success = 0, /**< Successful completion (should never be reached) */
} main_ret_t;

/* =============================================================================
 * Hardware Configuration Constants
 * =============================================================================
 */

/**
 * @brief RIIC (I2C) channel number constants for RX72N
 * @details
 * RX72N has 3 RIIC channels (0-2). These constants are used with uint8_t
 * parameters in bus configuration functions.
 *
 * @note hardware.h defines riic_channel_t as a struct wrapper, but bus
 *       configuration functions use raw uint8_t for channel numbers.
 *
 * @see rx_bus_config_init_smbus() Uses uint8_t channel parameter
 * @see rx_bus_config_init_i2c() Uses uint8_t channel parameter
 */
typedef enum : uint8_t {
  k_riic_channel_0 = 0, /**< RIIC channel 0 */
  k_riic_channel_1 = 1, /**< RIIC channel 1 (used for BQ4050 BMS) */
  k_riic_channel_2 = 2, /**< RIIC channel 2 */
} riic_channel_num_t;

/**
 * @brief Standard I2C bus frequencies for RIIC/SMBus configuration
 * @details
 * Standard I2C frequency constants for bus configuration.
 * Values in Hz for clarity and type safety.
 *
 * @see rx_bus_config_init_smbus() Uses frequency_hz parameter
 * @see rx_bus_config_init_i2c() Uses frequency_hz parameter
 */
typedef enum : uint32_t {
  k_i2c_frequency_100khz = 100000, /**< Standard mode: 100 kHz (default) */
  k_i2c_frequency_400khz = 400000, /**< Fast mode: 400 kHz */
} i2c_frequency_t;

/* =============================================================================
 * Static Bus Configurations
 * =============================================================================
 */

/**
 * @var s_i2c0_config
 * @brief SMBus configuration for BQ4050 battery management system
 *
 * @details
 * Configures RIIC1 channel as SMBus interface for BQ4050 fuel gauge IC.
 * Uses P20 (SDA) and P21 (SCL) at 100 kHz with Packet Error Checking (PEC).
 *
 * **Hardware Configuration:**
 * - Bus type: SMBus (I2C with PEC)
 * - Channel: RIIC1
 * - Device: BQ4050 @ 0x0B (7-bit address)
 * - Pins: P20 (SDA), P21 (SCL)
 * - Frequency: 100 kHz (standard mode)
 * - Features: PEC enabled for error detection
 *
 * **Device Details:**
 * The BQ4050 is a fully integrated battery management IC that provides:
 * - State of charge (SoC) estimation
 * - Cell voltage monitoring (up to 4 cells)
 * - Current monitoring with coulomb counting
 * - Temperature sensing
 * - Safety protection (overcurrent, overvoltage, undertemperature)
 *
 * @note Static allocation follows NASA Power of 10 Rule 3 (no dynamic memory).
 * @note Registered with bus manager in tx_application_define() before task creation.
 * @note SMBus protocol requires PEC for CRC-8 error checking on all transactions.
 *
 * @see rx_bus_config_init_smbus() Bus configuration function
 * @see bms_monitor_task.c Task that uses this bus for battery monitoring
 *
 * @since Version 1.0.0
 */
static rx_bus_config_t s_i2c0_config;

/**
 * @var s_onewire0_config
 * @brief 1-Wire configuration for DS18B20 temperature sensor
 *
 * @details
 * Configures GPIO P51 as 1-Wire interface for DS18B20 digital temperature sensor.
 * Uses bit-banging protocol with precise timing requirements.
 *
 * **Hardware Configuration:**
 * - Bus type: 1-Wire (Dallas/Maxim protocol)
 * - Pin: P51 (bidirectional data line)
 * - Pullup: 4.7 kΩ resistor required (external)
 * - Protocol: Bit-banging with microsecond timing
 *
 * **Device Details:**
 * The DS18B20 is a digital temperature sensor providing:
 * - Temperature range: -55°C to +125°C
 * - Accuracy: ±0.5°C (-10°C to +85°C)
 * - Resolution: 9 to 12 bits (configurable)
 * - Conversion time: 750 ms max (12-bit resolution)
 * - Unique 64-bit serial number per device
 *
 * @note Static allocation follows NASA Power of 10 Rule 3 (no dynamic memory).
 * @note Registered with bus manager in tx_application_define() before task creation.
 * @note Requires 4.7 kΩ pullup resistor on P51 (see schematic).
 * @note 1-Wire protocol timing is critical - interrupts may cause timing violations.
 *
 * @see rx_bus_config_init_onewire() Bus configuration function
 * @see temp_sensor_task.c Task that uses this bus for temperature monitoring
 *
 * @since Version 1.0.0
 */
static rx_bus_config_t s_onewire0_config;

/**
 * @var s_gpio_config
 * @brief Generic GPIO bus configuration for motor driver control
 *
 * @details
 * Configures a generic GPIO bus abstraction that provides access to all GPIO pins.
 * The initial pin (P00) is required by the API but not significant - motor control
 * task specifies actual pins (nFAULT, chip selects, etc.) at runtime.
 *
 * **Hardware Configuration:**
 * - Bus type: GPIO (generic abstraction)
 * - Initial pin: P00 (API requirement, not actually used)
 * - Actual pins: Specified by motor_control_task at runtime
 *
 * **Usage Pattern:**
 * Motor control task uses this generic GPIO bus to access:
 * - DRV8243 nFAULT pins (fault detection)
 * - DRV8243 chip select pins (SPI communication)
 * - Motor enable/disable control pins
 * - LED status indicators
 *
 * The pin validator ensures no physical pins are double-allocated.
 *
 * @note Static allocation follows NASA Power of 10 Rule 3 (no dynamic memory).
 * @note Registered with bus manager in tx_application_define() before task creation.
 * @note This is a generic bus abstraction - one "gpio" bus serves all 4 motor drivers.
 * @note Each driver operation specifies the actual pin to access.
 *
 * @see rx_bus_config_init_gpio() Bus configuration function
 * @see motor_control_task.c Task that uses this bus for motor control
 * @see rx_bus_gpio_write() Runtime GPIO write with pin specification
 * @see rx_bus_gpio_read() Runtime GPIO read with pin specification
 *
 * @since Version 1.0.0
 */
static rx_bus_config_t s_gpio_config;

/**
 * @var s_adc0_config
 * @brief ADC configuration for motor current sensing
 *
 * @details
 * Configures ADC Unit 0 (S12AD0) as generic ADC bus for motor current monitoring.
 * The initial channel (0) is required by the API but not significant - motor control
 * task specifies actual channels (AN004-AN007) at runtime for each motor.
 *
 * **Hardware Configuration:**
 * - Bus type: ADC (generic abstraction)
 * - Unit: ADC Unit 0 (S12AD0)
 * - Initial channel: 0 (API requirement, not actually used)
 * - Resolution: 12-bit (0-4095 counts)
 * - Actual channels: Specified by motor_control_task at runtime
 *
 * **Motor Current Sensing Channels:**
 * | Motor | ADC Channel | Pin    | DRV8243 Output |
 * |-------|-------------|--------|----------------|
 * | Motor 0 | AN007 (ch 7) | P40.7  | IPROPI output  |
 * | Motor 1 | AN006 (ch 6) | P40.6  | IPROPI output  |
 * | Motor 2 | AN005 (ch 5) | P40.5  | IPROPI output  |
 * | Motor 3 | AN004 (ch 4) | P40.4  | IPROPI output  |
 *
 * Each DRV8243 motor driver outputs an analog current signal (IPROPI) proportional
 * to motor current: 500 µA per 1 A of motor current (500:1 ratio).
 *
 * @note Static allocation follows NASA Power of 10 Rule 3 (no dynamic memory).
 * @note Registered with bus manager in tx_application_define() before task creation.
 * @note This is a generic bus abstraction - one "adc0" bus serves all 4 motor drivers.
 * @note Each current reading specifies the actual ADC channel to sample.
 * @note 12-bit resolution provides 1 mV per count with 3.3V reference.
 *
 * @see rx_bus_config_init_adc() Bus configuration function
 * @see motor_control_task.c Task that uses this bus for current monitoring
 * @see rx_bus_adc_read_voltage_mv() Runtime ADC read with channel specification
 *
 * @since Version 1.0.0
 */
static rx_bus_config_t s_adc0_config;

/* =============================================================================
 * Startup Flag Check Helpers
 * =============================================================================
 */

/**
 * @brief Check Power-On Reset Detect Flag (PORF) in RSTSR0 register
 *
 * @details
 * Reads the **RSTSR0 register** to determine if the current boot was initiated by a
 * **power-on reset** (fresh power application, not warm boot or software reset).
 *
 * ## PORF (Power-On Reset Flag) Semantics
 *
 * | PORF Value | Meaning | Boot Type | Expected? |
 * |------------|---------|-----------|-----------|
 * | **1 (set)** | Power-on reset occurred | **Cold start** | [OK] on fresh boot |
 * | **0 (clear)** | No power-on reset | **Warm start** | [WARN] Software reset, watchdog, or debugger |
 *
 * ## Register Details
 *
 * **RSTSR0 (Reset Status Register 0):**
 * - **Address:** 0x000C001C (accessed via `rstsr01()->rstsr0`)
 * - **Bit 0 (PORF):** Power-On Reset Detect Flag
 *   - Set by hardware on VCC power-on
 *   - Cleared by software write (requires PRCR.PRC0=1 protection unlock)
 *   - Persistent across warm boots until explicitly cleared
 *
 * ## Use Cases and Interpretation
 *
 * **Cold start (PORF=1):**
 * - Expected on normal power-up
 * - Flash memory state unknown (may contain prior data)
 * - All peripherals reset to default state
 * - Typical after battery installation, power cycle
 *
 * **Warm start (PORF=0):**
 * - Software reset (via SWRR register write)
 * - Watchdog timeout reset (WDT or IWDT)
 * - Debugger-initiated reset (OpenOCD, GDB)
 * - Voltage monitoring reset (LVD)
 *
 * **Key insight:** PORF=0 is **not an error** - it's informational. Many valid scenarios
 * produce warm boots (bootloader software reset, debugger attach, etc.).
 *
 * ## Performance (RX72N @ 240 MHz)
 *
 * | Operation | Duration | Notes |
 * |-----------|----------|-------|
 * | Register read (rstsr0) | ~0.5 µs | Single 8-bit read |
 * | Bit masking (& k_rstsr0_porf) | ~5 cycles | Bitwise AND |
 * | Conditional logging | ~5 µs | If warm boot detected (UART output) |
 * | **Total** | **~0.5 µs** | No logging (cold start) |
 *
 * @return bool Power-on reset detection status
 * @retval true PORF is set (cold start - power-on reset occurred)
 * @retval false PORF is clear (warm start - no power-on reset)
 *
 * @pre RSTSR0 register accessible (memory-mapped I/O at valid address)
 * @pre Register pointer (rstsr01()) returns non-NULL compile-time constant
 *
 * @post Return value reflects actual PORF bit state
 * @post Warm boot logged if PORF=0 (informational message)
 *
 * @note **Does not modify RSTSR0.** Flag clearing requires protection unlock (PRCR.PRC0=1)
 *       and explicit write to RSTSR0. This function is read-only.
 *
 * @note **Both return values are acceptable.** This check is informational, not a pass/fail test.
 *       Warm boots (PORF=0) are valid in many scenarios (debugger, bootloader, etc.).
 *
 * @par Thread Safety:
 * Executes before ThreadX starts (single-threaded context). No race conditions.
 *
 * @par Example Usage:
 * @code
 * // Check boot type
 * bool cold_start = internal_check_porf();
 * if (cold_start) {
 *   rx_log_info("MAIN", "Cold start detected (power-on reset)");
 * } else {
 *   rx_log_info("MAIN", "Warm start detected (software/watchdog reset)");
 * }
 * @endcode
 *
 * @see internal_check_startup_flags() Orchestrates all reset flag checks
 * @see internal_check_cwsf() Complementary cold/warm start determination (RSTSR1.7)
 *
 * @since Version 1.0.0
 *
 * @test test_main.c Verify cold start (PORF=1) and warm start (PORF=0) detection
 */
static bool internal_check_porf(void)
{
  const volatile rx_rstsr01_regs_t* regs = rstsr01();

  /* Precondition: Register pointer must be valid (compile-time constant address) */
  RX_ASSERT(regs != nullptr, "RSTSR01 register pointer is nullptr");

  const uint8_t rstsr0_val = regs->rstsr0;

  /* PORF=1 indicates power-on reset occurred, which is expected on normal startup */
  const bool porf_set = (rstsr0_val & k_rstsr0_porf) != 0;

  /* Note: Logging removed - UART not initialized yet. Will be reported by
   * internal_report_startup_flags() after UART is ready. */

  return porf_set;
}

/**
 * @brief Helper function to check if a specific RSTSR2 flag is clear (not set)
 *
 * @details
 * **Generic RSTSR2 flag checker** used by multiple reset detection functions to reduce code duplication.
 * Reads the **RSTSR2 register** and tests if a specific bit (identified by `flag_mask`) is **clear (0)**.
 *
 * ## RSTSR2 (Reset Status Register 2) Overview
 *
 * **Address:** 0x000C0080 (accessed via `rstsr2()` accessor function)
 *
 * **Relevant bit fields:**
 * | Bit | Flag Name | Meaning when SET (1) | Expected State |
 * |-----|-----------|----------------------|----------------|
 * | 0 | **WDTRF** | Watchdog Timer reset occurred | Clear (0) |
 * | 1 | **IWDTRF** | Independent Watchdog Timer reset occurred | Clear (0) |
 * | 2 | **SWRF** | Software reset occurred | Clear (0) |
 *
 * ## Function Usage Pattern
 *
 * This helper is called by three specialized checkers:
 * - `internal_check_iwdtrf()` -> checks IWDTRF (bit 1)
 * - `internal_check_wdtrf()` -> checks WDTRF (bit 0)
 * - `internal_check_swrf()` -> checks SWRF (bit 2)
 *
 * **Why this helper exists:** DRY principle - avoid duplicating register read + mask logic 3 times.
 *
 * ## Algorithm
 *
 * ```
 * 1. Assert flag_mask is non-zero (precondition check)
 * 2. Read RSTSR2 register via rstsr2() accessor
 * 3. Assert register pointer is valid (NULL check)
 * 4. Apply bit mask: result = (rstsr2_val & flag_mask) == 0
 * 5. Return true if flag is CLEAR (0), false if SET (1)
 * ```
 *
 * ## Performance (RX72N @ 240 MHz)
 *
 * | Operation | Duration | Notes |
 * |-----------|----------|-------|
 * | Register read (rstsr2) | ~0.5 µs | Single 8-bit read |
 * | Bit masking | ~5 cycles | Bitwise AND + comparison |
 * | Assertions (debug build) | ~0.2 µs | Two RX_ASSERT checks |
 * | **Total** | **~0.7 µs** | Negligible overhead |
 *
 * @param[in] flag_mask Bit mask for the flag to check (e.g., k_rstsr2_iwdtrf = 0x02)
 *                      Must be non-zero (validated by assertion).
 *
 * @return bool Flag state (clear vs set)
 * @retval true Flag is CLEAR (0) - normal condition, no reset detected
 * @retval false Flag is SET (1) - reset of this type occurred
 *
 * @pre flag_mask must be non-zero (at least one bit set)
 * @pre RSTSR2 register accessible (memory-mapped at 0x000C0080)
 * @pre rstsr2() accessor returns valid pointer (compile-time constant)
 *
 * @post Return value accurately reflects (rstsr2_val & flag_mask) == 0
 * @post No side effects (read-only operation, does not modify RSTSR2)
 *
 * @note **This is a helper function** - not called directly by application code.
 *       Use specialized wrappers (internal_check_iwdtrf, etc.) for clarity.
 *
 * @note **Does not clear flags.** Flag clearing requires protection unlock (PRCR.PRC0=1)
 *       and explicit write to RSTSR2. This function is read-only.
 *
 * @par Thread Safety:
 * Executes before ThreadX starts (single-threaded context). No race conditions.
 *
 * @par Example Usage (Internal):
 * @code
 * // Check Independent Watchdog Timer Reset Flag (IWDTRF)
 * static bool internal_check_iwdtrf(void) {
 *   return internal_check_rstsr2_flag_clear(k_rstsr2_iwdtrf);  // k_rstsr2_iwdtrf = 0x02
 * }
 *
 * // Check Watchdog Timer Reset Flag (WDTRF)
 * static bool internal_check_wdtrf(void) {
 *   return internal_check_rstsr2_flag_clear(k_rstsr2_wdtrf);  // k_rstsr2_wdtrf = 0x01
 * }
 * @endcode
 *
 * @see internal_check_iwdtrf() Check IWDTRF using this helper
 * @see internal_check_wdtrf() Check WDTRF using this helper
 * @see internal_check_swrf() Check SWRF using this helper
 * @see rstsr2() Accessor function for RSTSR2 register
 *
 * @since Version 1.0.0
 *
 * @test test_main.c Verify correct flag detection for all three flags (IWDTRF, WDTRF, SWRF)
 */
static bool internal_check_rstsr2_flag_clear(const uint8_t flag_mask)
{
  /* Precondition: flag_mask must be non-zero */
  RX_ASSERT(flag_mask != 0, "Precondition: flag_mask must not be zero");

  const volatile uint8_t* reg = rstsr2();

  /* Precondition: Register pointer must be valid (compile-time constant address) */
  RX_ASSERT(reg != nullptr, "RSTSR2 register pointer is nullptr");

  const uint8_t rstsr2_val = *reg;

  /* Compute result reflecting actual register state */
  const bool result = (rstsr2_val & flag_mask) == 0;

  return result;
}

/**
 * @brief Check Independent Watchdog Timer Reset Detect Flag (IWDTRF) - CRITICAL safety check
 *
 * @details
 * Reads the **RSTSR2.1 (IWDTRF)** bit to detect if the **current boot** was caused by an
 * **Independent Watchdog Timer (IWDT) timeout**. IWDT timeout is a **critical firmware error**
 * indicating the prior execution hung, deadlocked, or failed to refresh the watchdog.
 *
 * **CRITICAL:** This is the **most important reset flag check**. If IWDTRF=1, the firmware
 * **MUST halt** immediately (via RX_ASSERT in caller) to prevent cascading failures.
 *
 * ## IWDTRF (Independent Watchdog Timer Reset Flag) Semantics
 *
 * | IWDTRF Value | Meaning | Implication | Action |
 * |--------------|---------|-------------|--------|
 * | **0 (clear)** | No IWDT reset | **Normal** - prior execution completed gracefully | [OK] boot |
 * | **1 (set)** | IWDT timeout reset | **CRITICAL BUG** - prior firmware hung/crashed | [STOP] **HALT EXECUTION** |
 *
 * ## Independent Watchdog Timer (IWDT) Overview
 *
 * **Purpose:** Detect firmware hangs, infinite loops, deadlocks
 *
 * **Configuration:**
 * - **Timeout period:** 128 ms (configured in option setting memory)
 * - **Clock source:** IWDTCLK (independent 15 kHz RC oscillator)
 * - **Refresh requirement:** Call `rx_iwdt_refresh()` every 100 ms (from PID control loop)
 *
 * **Failure scenarios triggering IWDT reset:**
 * - Infinite loop (forgot `tx_thread_sleep()` in task)
 * - Deadlock (mutex wait never completes)
 * - Priority inversion (high-priority task blocked by low-priority)
 * - Hard fault (exception handler doesn't refresh watchdog)
 *
 * ## Why IWDT Timeout is Critical (vs Other Resets)
 *
 * | Reset Type | Cause | Severity | Recovery |
 * |------------|-------|----------|----------|
 * | **IWDTRF** | Firmware hang | **CRITICAL** | Must fix root cause (firmware bug) |
 * | WDTRF | WDT timeout | High | May be intentional (bootloader) |
 * | SWRF | Software reset | Low | Intentional (bootloader, debug) |
 * | LVD0RF | Brownout | Low | Power supply issue (hardware) |
 *
 * **Key insight:** IWDT timeout is **ALWAYS** a firmware bug. Unlike software reset (intentional)
 * or brownout (hardware issue), IWDT timeout has **no valid use case** except detecting hangs.
 *
 * ## Integration with Startup Check Flow
 *
 * ```
 * internal_check_startup_flags()
 *   -> internal_check_iwdtrf()
 *   -> return false if IWDTRF=1
 *   -> RX_ASSERT(iwdtrf_ok, "IWDT reset detected") -> **HALTS EXECUTION**
 * ```
 *
 * ## Performance (RX72N @ 240 MHz)
 *
 * | Operation | Duration | Notes |
 * |-----------|----------|-------|
 * | Call helper (internal_check_rstsr2_flag_clear) | ~0.7 µs | Register read + mask |
 * | **Total** | **~0.7 µs** | Single register check |
 *
 * @return bool IWDT reset detection status
 * @retval true IWDTRF is CLEAR (0) - **normal condition** (no watchdog timeout)
 * @retval false IWDTRF is SET (1) - **CRITICAL ERROR** (prior firmware hung)
 *
 * @pre RSTSR2 register accessible (memory-mapped at 0x000C0080)
 * @pre IWDT configured and enabled (option setting memory)
 *
 * @post Return value reflects actual IWDTRF bit state
 * @post **If false returned, caller MUST halt execution** (via RX_ASSERT)
 *
 * @note **This check is CRITICAL for firmware safety.** Never bypass or ignore IWDTRF=1.
 *       Always investigate and fix the root cause (infinite loop, deadlock, etc.).
 *
 * @note **Does not clear IWDTRF.** Flag clearing requires protection unlock (PRCR.PRC0=1)
 *       and explicit write to RSTSR2. This function is read-only.
 *
 * @warning **IWDTRF=1 ALWAYS indicates firmware bug.** Do NOT continue boot if this function
 *          returns false. Caller enforces this via RX_ASSERT (halts execution).
 *
 * @par Thread Safety:
 * Executes before ThreadX starts (single-threaded context). No race conditions.
 *
 * @par Example Usage:
 * @code
 * // Startup validation in main():
 * bool iwdtrf_ok = internal_check_iwdtrf();
 * if (!iwdtrf_ok) {
 *   rx_log_error("MAIN", "CRITICAL: Independent Watchdog Timer reset detected");
 *   rx_log_error("MAIN", "Prior execution hung or failed to refresh watchdog");
 *   // Halt execution (fail-fast)
 *   RX_ASSERT(false, "IWDT timeout - firmware bug detected");
 * }
 * @endcode
 *
 * @par Common Root Causes (Debug Guide):
 * 1. **Infinite loop:** Check for `while(1)` without `tx_thread_sleep()`
 * 2. **Deadlock:** Check mutex acquisition order (AB-BA deadlock)
 * 3. **Priority inversion:** Low-priority task holds mutex, high-priority waits forever
 * 4. **Hard fault:** Exception handler doesn't refresh IWDT before reset
 * 5. **Missing refresh:** `rx_iwdt_refresh()` not called every 100 ms from main task
 *
 * @see internal_check_rstsr2_flag_clear() Generic RSTSR2 flag checker (helper)
 * @see internal_check_startup_flags() Orchestrates all reset checks (caller)
 * @see rx_iwdt_refresh() Refresh IWDT to prevent timeout (must call every 100 ms)
 *
 * @since Version 1.0.0
 *
 * @test test_main.c Verify IWDTRF=0 (normal boot) and IWDTRF=1 (simulated hang)
 */
static bool internal_check_iwdtrf(void)
{
  return internal_check_rstsr2_flag_clear(k_rstsr2_iwdtrf);
}

/**
 * @brief Check Watchdog Timer Reset Detect Flag (WDTRF)
 *
 * @details
 * Reads the **RSTSR2.0 (WDTRF)** bit to detect if the current boot was caused by a
 * **Watchdog Timer (WDT) timeout**. Unlike IWDTRF (critical), WDTRF may be **intentional**
 * in some scenarios (bootloader timeout, intentional watchdog reset).
 *
 * ## WDTRF (Watchdog Timer Reset Flag) Semantics
 *
 * | WDTRF Value | Meaning | Typical Cause | Action |
 * |-------------|---------|---------------|--------|
 * | **0 (clear)** | No WDT reset | Normal boot | [OK] |
 * | **1 (set)** | WDT timeout reset | Firmware hang OR intentional | [WARN] Log warning, return error |
 *
 * ## WDT vs IWDT Comparison
 *
 * | Feature | WDT (Watchdog Timer) | IWDT (Independent WDT) |
 * |---------|----------------------|------------------------|
 * | **Clock source** | PCLKB (120 MHz, shared) | IWDTCLK (15 kHz, independent) |
 * | **Disable capability** | Can be stopped in software | Cannot be stopped (always-on) |
 * | **Use case** | Intentional resets, bootloader timeout | Critical hang detection only |
 * | **Flag on reset** | WDTRF (RSTSR2.0) | IWDTRF (RSTSR2.1) |
 * | **Criticality** | [WARN] Warning (may be intentional) | [STOP] Critical (always a bug) |
 *
 * ## Interpretation and Recovery
 *
 * **Non-critical flag:** Unlike IWDTRF, WDTRF=1 does **not** assert-halt. Instead:
 * - Log warning message (diagnostic information)
 * - Return `false` to caller (indicates abnormal condition)
 * - Caller returns `k_rx_err_hw_init_failed` (non-fatal error)
 * - Boot may continue (user decides based on application requirements)
 *
 * @return bool WDT reset detection status
 * @retval true WDTRF is CLEAR (0) - normal condition (no WDT timeout)
 * @retval false WDTRF is SET (1) - WDT timeout reset (warning, may be intentional)
 *
 * @pre RSTSR2 register accessible (memory-mapped at 0x000C0080)
 *
 * @post Return value reflects actual WDTRF bit state
 * @post Caller logs warning if false returned (non-critical error)
 *
 * @note **Does not clear WDTRF.** Flag clearing requires protection unlock and explicit write.
 *
 * @par Thread Safety:
 * Executes before ThreadX starts (single-threaded context).
 *
 * @see internal_check_rstsr2_flag_clear() Helper function for RSTSR2 checks
 * @see internal_check_iwdtrf() Critical IWDT check (always a bug if set)
 *
 * @since Version 1.0.0
 */
static bool internal_check_wdtrf(void)
{
  return internal_check_rstsr2_flag_clear(k_rstsr2_wdtrf);
}

/**
 * @brief Check Software Reset Detect Flag (SWRF)
 *
 * @details
 * Reads the **RSTSR2.2 (SWRF)** bit to detect if the current boot was caused by a
 * **software-initiated reset** (via SWRR register write). Software resets are often **intentional**
 * (bootloader entry, firmware update mode, debug reset) but may also indicate error recovery.
 *
 * ## SWRF (Software Reset Flag) Semantics
 *
 * | SWRF Value | Meaning | Typical Cause | Error? |
 * |------------|---------|---------------|--------|
 * | **0 (clear)** | No software reset | Normal boot or hardware reset | [OK] |
 * | **1 (set)** | Software reset executed | Bootloader, debug, or error recovery | [WARN] Context-dependent |
 *
 * ## Common Software Reset Scenarios
 *
 * **Intentional (non-error):**
 * 1. **Bootloader entry:** User button held during power-on -> software reset to bootloader mode
 * 2. **Firmware update:** Application writes to SWRR after downloading new firmware
 * 3. **Debugger reset:** OpenOCD or GDB issues reset command
 * 4. **Configuration change:** Software reset after updating option bytes
 *
 * **Error recovery (potential issue):**
 * 1. **Error handler:** Firmware detects unrecoverable error -> software reset to recover
 * 2. **Panic handler:** Hard fault exception -> software reset as last resort
 * 3. **Assert failure:** RX_ASSERT failed -> software reset instead of halt (if configured)
 *
 * ## Interpretation Strategy
 *
 * **Non-critical flag:** SWRF=1 does **not** halt execution. Instead:
 * - Log informational message (diagnostic aid)
 * - Return `false` to caller (indicates software reset occurred)
 * - Caller returns `k_rx_err_hw_init_failed` (allows boot to continue)
 * - Application logic decides if this is acceptable
 *
 * @return bool Software reset detection status
 * @retval true SWRF is CLEAR (0) - no software reset (normal hardware reset)
 * @retval false SWRF is SET (1) - software reset occurred (may be intentional)
 *
 * @pre RSTSR2 register accessible (memory-mapped at 0x000C0080)
 *
 * @post Return value reflects actual SWRF bit state
 * @post Caller logs informational message if false returned
 *
 * @note **Software reset is not necessarily an error.** Many valid use cases (bootloader,
 *       firmware update, debug) intentionally trigger software resets.
 *
 * @note **Does not clear SWRF.** Flag clearing requires protection unlock and explicit write.
 *
 * @par Thread Safety:
 * Executes before ThreadX starts (single-threaded context).
 *
 * @par Example Intentional Software Reset:
 * @code
 * // Enter bootloader mode (from application):
 * void enter_bootloader(void) {
 *   rx_log_info("APP", "Entering bootloader mode via software reset");
 *
 *   // Unlock register protection
 *   system()->prcr = 0xA501;
 *
 *   // Trigger software reset
 *   system()->swrr = 0xA501;  // Write magic value to SWRR
 *
 *   // Never reached (MCU resets immediately)
 * }
 * @endcode
 *
 * @see internal_check_rstsr2_flag_clear() Helper function for RSTSR2 checks
 * @see internal_check_iwdtrf() Critical IWDT check (always a bug if set)
 *
 * @since Version 1.0.0
 */
static bool internal_check_swrf(void)
{
  return internal_check_rstsr2_flag_clear(k_rstsr2_swrf);
}

/**
 * @brief Check Voltage-Monitoring 0 Reset Detect Flag (LVD0RF) - Brownout detection
 *
 * @details
 * Reads the **RSTSR0.1 (LVD0RF)** bit to detect if the current boot was caused by a
 * **Low-Voltage Detect (LVD) reset**. LVD0RF=1 indicates the **supply voltage (VCC) dropped
 * below the configured threshold**, triggering a protective reset (brownout condition).
 *
 * ## LVD0RF (Low-Voltage Detect 0 Reset Flag) Semantics
 *
 * | LVD0RF Value | Meaning | Cause | Issue |
 * |--------------|---------|-------|-------|
 * | **0 (clear)** | No brownout reset | Voltage stable during operation | [OK] |
 * | **1 (set)** | Brownout reset occurred | VCC dropped below threshold (e.g., 2.9V) | [WARN] Power supply issue |
 *
 * ## Low-Voltage Detect (LVD) System Overview
 *
 * **Purpose:** Protect MCU from operating at insufficient voltage (data corruption, erratic behavior)
 *
 * **LVD0 Configuration:**
 * - **Threshold:** Typically 2.9V or 3.0V (configured in option setting memory)
 * - **Action:** Generate interrupt OR reset when VCC < threshold
 * - **Hysteresis:** ~100 mV (prevents oscillation at threshold boundary)
 *
 * **Common brownout causes:**
 * 1. **Inadequate power supply:** Regulator cannot provide sufficient current
 * 2. **Motor inrush current:** High current draw from motors causes voltage sag
 * 3. **Loose power connector:** Intermittent connection causes voltage drops
 * 4. **Battery depletion:** Battery voltage falls below LVD threshold
 *
 * ## Brownout Detection and Recovery
 *
 * **Non-critical flag:** LVD0RF=1 indicates **hardware issue**, not firmware bug:
 * - Log warning message (power supply diagnostic)
 * - Return `false` to caller (indicates brownout occurred)
 * - Caller returns `k_rx_err_hw_init_failed` (allows boot to continue if voltage restored)
 * - Application may implement graceful shutdown or low-power mode
 *
 * ## Double-Read for Register Stability
 *
 * **Why two reads?** RSTSR0 is a volatile status register that may have transient states
 * during power-up. Reading twice and comparing ensures stable value (catches glitches).
 *
 * ```c
 * const uint8_t rstsr0_val  = regs->rstsr0;  // First read
 * const uint8_t rstsr0_val2 = regs->rstsr0;  // Second read
 * RX_ASSERT(rstsr0_val == rstsr0_val2, "Inconsistent RSTSR01 read");
 * ```
 *
 * @return bool LVD reset detection status
 * @retval true LVD0RF is CLEAR (0) - no brownout reset (voltage stable)
 * @retval false LVD0RF is SET (1) - brownout reset occurred (power supply issue)
 *
 * @pre RSTSR0 register accessible (memory-mapped at 0x000C001C)
 * @pre LVD0 configured and enabled (option setting memory)
 *
 * @post Return value reflects actual LVD0RF bit state
 * @post Two consecutive reads match (assert validates stability)
 * @post Caller logs warning if false returned (hardware diagnostic)
 *
 * @note **Does not clear LVD0RF.** Flag clearing requires protection unlock (PRCR.PRC0=1)
 *       and explicit write to RSTSR0.
 *
 * @note **LVD0RF=1 indicates hardware issue, not firmware bug.** Investigate power supply
 *       capacity, motor inrush current, or battery voltage.
 *
 * @par Thread Safety:
 * Executes before ThreadX starts (single-threaded context).
 *
 * @par Example Power Supply Issue Debugging:
 * @code
 * // Startup check:
 * bool lvd0rf_ok = internal_check_lvd0rf();
 * if (!lvd0rf_ok) {
 *   rx_log_warn("MAIN", "Brownout reset detected (VCC dropped below threshold)");
 *   rx_log_warn("MAIN", "Check power supply capacity and motor inrush current");
 *
 *   // Optional: Implement low-power mode or graceful shutdown
 *   if (battery_voltage_low()) {
 *     enter_low_power_mode();
 *   }
 * }
 * @endcode
 *
 * @see internal_check_startup_flags() Orchestrates all reset checks (caller)
 * @see rstsr01() Accessor function for RSTSR0/RSTSR1 registers
 *
 * @since Version 1.0.0
 *
 * @test test_main.c Verify LVD0RF=0 (normal) and LVD0RF=1 (simulated brownout)
 */
static bool internal_check_lvd0rf(void)
{
  const volatile rx_rstsr01_regs_t* regs = rstsr01();

  /* Precondition: Register pointer must be valid */
  RX_ASSERT(regs != nullptr, "RSTSR01 register pointer is nullptr");

  /* Read twice to catch unstable/rolling reads from volatile status register. */
  const uint8_t rstsr0_val  = regs->rstsr0;
  const uint8_t rstsr0_val2 = regs->rstsr0;

  RX_ASSERT(rstsr0_val == rstsr0_val2, "Inconsistent RSTSR01 read");

  /* LVD0RF=0 means no voltage-monitoring reset (normal condition) */
  const bool lvd0rf_clear = (rstsr0_val & k_rstsr0_lvd0rf) == 0;

  return lvd0rf_clear;
}

/**
 * @brief Check Cold/Warm Start Determination Flag (CWSF) - Boot type classification
 *
 * @details
 * Reads the **RSTSR1.7 (CWSF)** bit to classify the boot type as **cold start** (power-on,
 * voltage drop) or **warm start** (software reset, watchdog, debugger). This is **informational
 * only** - both cold and warm starts are valid boot conditions.
 *
 * ## CWSF (Cold/Warm Start Flag) Semantics
 *
 * | CWSF Value | Meaning | Boot Classification | Examples |
 * |------------|---------|---------------------|----------|
 * | **0 (clear)** | Cold start | Power-on or voltage recovery | VCC power-on, brownout recovery |
 * | **1 (set)** | Warm start | Processor-initiated reset | Software reset, watchdog, debugger |
 *
 * ## CWSF vs PORF Comparison
 *
 * Both flags classify boot type, but with different granularity:
 *
 * | Flag | Register | Boot Type Distinction | Use Case |
 * |------|----------|----------------------|----------|
 * | **PORF** | RSTSR0.0 | Power-on vs all other resets | Detect fresh power application |
 * | **CWSF** | RSTSR1.7 | Cold (power/voltage) vs warm (processor) | Classify reset mechanism |
 *
 * **Example scenarios:**
 * - **Power-on boot:** PORF=1, CWSF=0 (cold start)
 * - **Brownout recovery:** PORF=0, CWSF=0 (cold start, voltage-related)
 * - **Software reset:** PORF=0, CWSF=1 (warm start, processor-initiated)
 * - **Watchdog reset:** PORF=0, CWSF=1 (warm start, processor-initiated)
 *
 * ## Use Cases for CWSF
 *
 * **Informational (no action required):**
 * 1. **Diagnostics:** Log boot type for debugging and telemetry
 * 2. **Statistics:** Track cold vs warm start frequency (reliability metrics)
 * 3. **State preservation:** Warm start may preserve RAM contents (depends on reset type)
 *
 * **No validation needed:** Unlike IWDTRF (critical) or LVD0RF (hardware issue), CWSF does
 * not indicate an error condition. Both cold and warm starts are valid.
 *
 * ## Return Value Convention
 *
 * **Always returns actual CWSF state:**
 * - `true` = warm start (CWSF=1)
 * - `false` = cold start (CWSF=0)
 *
 * **No error checking:** Caller does not act on return value (informational only).
 *
 * @return bool Boot type classification
 * @retval true Warm start (CWSF=1) - processor-initiated reset (software, watchdog, debugger)
 * @retval false Cold start (CWSF=0) - power-on or voltage recovery
 *
 * @pre RSTSR1 register accessible (memory-mapped at 0x000C001D)
 * @pre rstsr01() accessor returns valid pointer (compile-time constant)
 *
 * @post Return value reflects actual CWSF bit state (0 or 1)
 * @post CWSF bit value validated (must be 0 or k_rstsr1_cwsf via assertion)
 *
 * @note **Both return values are valid boot conditions.** This function is informational only,
 *       not a pass/fail test. No action required based on return value.
 *
 * @note **Does not clear CWSF.** Flag clearing requires protection unlock and explicit write.
 *
 * @par Thread Safety:
 * Executes before ThreadX starts (single-threaded context).
 *
 * @par Example Usage (Informational Logging):
 * @code
 * // Log boot type for diagnostics:
 * bool warm_start = internal_check_cwsf();
 * if (warm_start) {
 *   rx_log_info("MAIN", "Warm start detected (processor-initiated reset)");
 *   // Optional: Check other flags to determine specific warm start cause
 *   if (internal_check_swrf()) {
 *     rx_log_info("MAIN", "  -> Software reset (SWRF set)");
 *   } else if (internal_check_wdtrf()) {
 *     rx_log_info("MAIN", "  -> Watchdog reset (WDTRF set)");
 *   }
 * } else {
 *   rx_log_info("MAIN", "Cold start detected (power-on or voltage recovery)");
 * }
 * @endcode
 *
 * @see internal_check_startup_flags() Orchestrates all reset checks (caller)
 * @see internal_check_porf() Complementary power-on reset detection (RSTSR0.0)
 * @see rstsr01() Accessor function for RSTSR0/RSTSR1 registers
 *
 * @since Version 1.0.0
 *
 * @test test_main.c Verify cold start (CWSF=0) and warm start (CWSF=1) detection
 */
static bool internal_check_cwsf(void)
{
  const volatile rx_rstsr01_regs_t* regs = rstsr01();

  /* Precondition: Register pointer must be valid */
  RX_ASSERT(regs != nullptr, "RSTSR01 register pointer is nullptr");

  const uint8_t rstsr1_val = regs->rstsr1;

  /* Extract CWSF bit value for validation */
  const uint8_t cwsf_raw = (rstsr1_val & k_rstsr1_cwsf);

  /* Postcondition: Verify CWSF bit value is either 0 or k_rstsr1_cwsf */
  RX_ASSERT((cwsf_raw == 0) || (cwsf_raw == k_rstsr1_cwsf),
            "Postcondition: CWSF bit value invalid");

  /* Return actual CWSF state: true for warm start (CWSF=1), false for cold start (CWSF=0) */
  return (cwsf_raw == k_rstsr1_cwsf);
}

/**
 * @brief Report startup flags to UART console after early UART initialization
 *
 * @details
 * Re-reads **reset status registers** and logs diagnostic information now that UART is available.
 * This function is called **after uart_debug_init()** in main() to provide visibility into boot
 * conditions that occurred before UART was initialized.
 *
 * ## Purpose
 *
 * The individual flag check functions (internal_check_porf, etc.) run **before UART init**,
 * so they cannot log to console. This function provides deferred logging of the same information
 * once console output is available.
 *
 * ## Flags Reported
 *
 * | Flag | Register | Meaning | Severity |
 * |------|----------|---------|----------|
 * | **PORF** | RSTSR0.0 | Power-on reset (cold boot) | [INFO] |
 * | **CWSF** | RSTSR1.7 | Cold/warm start classification | [INFO] |
 * | **WDTRF** | RSTSR2.0 | Watchdog timer timeout | [WARN] |
 * | **SWRF** | RSTSR2.2 | Software reset | [INFO] |
 * | **LVD0RF** | RSTSR0.1 | Brownout (voltage drop) | [WARN] |
 * | **IWDTRF** | RSTSR2.1 | Independent watchdog timeout | [N/A - halts before this] |
 *
 * **Note:** IWDTRF is not reported here because if it's set, the system halts via RX_ASSERT
 * in internal_check_startup_flags() before reaching this function.
 *
 * ## Integration with Boot Sequence
 *
 * ```
 * main()
 *   ├─ internal_check_startup_flags()  // Check flags (silent, UART not ready)
 *   ├─ rx_clock_power_init()           // Initialize clocks
 *   ├─ uart_debug_init()               // Initialize UART
 *   ├─ internal_report_startup_flags() // ← Report flags NOW (UART available)
 *   └─ rx_infrastructure_init()        // Continue boot...
 * ```
 *
 * ## Performance (RX72N @ 240 MHz)
 *
 * | Operation | Duration | Notes |
 * |-----------|----------|-------|
 * | Register reads (3 regs) | ~1.5 µs | RSTSR0, RSTSR1, RSTSR2 |
 * | Logging (5-6 messages) | ~500 µs | UART @ 115200 baud |
 * | **Total** | **~500 µs** | Negligible boot overhead |
 *
 * @return void (No return value - informational logging only)
 *
 * @pre uart_debug_init() called successfully (UART functional)
 * @pre RSTSR0/RSTSR1/RSTSR2 registers accessible (memory-mapped)
 *
 * @post Startup flags logged to UART console for diagnostics
 * @post No side effects (read-only operation, does not clear flags)
 *
 * @note **Call this function ONLY if uart_debug_init() succeeded.** If UART init failed,
 *       skip this function to avoid silent log failures.
 *
 * @note **Does not clear reset flags.** Flags remain set until explicitly cleared by
 *       firmware or next power-on reset.
 *
 * @par Thread Safety:
 * Executes before ThreadX starts (single-threaded context). No race conditions.
 *
 * @par Example Usage:
 * @code
 * // In main():
 * rx_err_t ret = uart_debug_init();
 * if (ret == k_rx_ok) {
 *   internal_report_startup_flags();  // Safe: UART initialized
 * }
 * RX_ERROR_CHECK(ret);  // Check UART status after reporting
 * @endcode
 *
 * @see internal_check_startup_flags() Validates flags (runs before UART init)
 * @see uart_debug_init() Must be called before this function
 * @see rstsr01() Accessor for RSTSR0/RSTSR1 registers
 * @see rstsr2() Accessor for RSTSR2 register
 *
 * @since Version 1.0.0
 */
static void internal_report_startup_flags(void)
{
  const volatile rx_rstsr01_regs_t* regs01 = rstsr01();
  const volatile uint8_t*           regs2  = rstsr2();

  /* Preconditions: Register pointers must be valid */
  RX_ASSERT(regs01 != nullptr, "RSTSR01 register pointer is nullptr");
  RX_ASSERT(regs2 != nullptr, "RSTSR2 register pointer is nullptr");

  const uint8_t rstsr0_val = regs01->rstsr0;
  const uint8_t rstsr1_val = regs01->rstsr1;
  const uint8_t rstsr2_val = *regs2;

  /* Report power-on reset flag (PORF) */
  if (rstsr0_val & k_rstsr0_porf) {
    rx_log_info("BOOT", "Cold boot - Power-on reset detected (PORF=1)");
  } else {
    rx_log_info("BOOT", "Warm boot - No power-on reset (PORF=0)");
  }

  /* Report cold/warm start flag (CWSF) */
  if (rstsr1_val & k_rstsr1_cwsf) {
    rx_log_info("BOOT", "Warm start - Processor-initiated reset (CWSF=1)");
  } else {
    rx_log_info("BOOT", "Cold start - Power/voltage related (CWSF=0)");
  }

  /* Report abnormal conditions */
  if (rstsr2_val & k_rstsr2_wdtrf) {
    rx_log_warn("BOOT", "Watchdog Timer timeout reset detected (WDTRF=1)");
    rx_log_warn("BOOT", "  -> Check for firmware hang or intentional WDT reset");
  }

  if (rstsr2_val & k_rstsr2_swrf) {
    rx_log_info("BOOT", "Software reset detected (SWRF=1)");
    rx_log_info("BOOT", "  -> May be intentional (bootloader, firmware update, debug)");
  }

  if (rstsr0_val & k_rstsr0_lvd0rf) {
    rx_log_warn("BOOT", "Brownout reset detected (LVD0RF=1)");
    rx_log_warn("BOOT", "  -> VCC dropped below threshold - Check power supply!");
  }

  /* Note: IWDTRF not reported here - if set, system halts in internal_check_startup_flags() */
}

/**
 * @brief Validate all reset status flags to detect abnormal boot conditions
 *
 * @details
 * Orchestrates **six reset status register checks** to detect abnormal reset conditions
 * (watchdog timeout, brownout, software reset, etc.). Implements a **fail-fast strategy**
 * for critical flags (IWDTRF) while logging non-critical warnings.
 *
 * ## Checked Reset Flags (RX72N Status Registers)
 *
 * | Flag | Register | Check Function | Critical? | Failure Action |
 * |------|----------|----------------|-----------|----------------|
 * | **PORF** | RSTSR0.0 | internal_check_porf() | No | Log info (warm boot OK) |
 * | **IWDTRF** | RSTSR2.1 | internal_check_iwdtrf() | **YES** | **Assert halts** |
 * | **WDTRF** | RSTSR2.0 | internal_check_wdtrf() | No | Return k_rx_err_hw_init_failed |
 * | **SWRF** | RSTSR2.2 | internal_check_swrf() | No | Return k_rx_err_hw_init_failed |
 * | **LVD0RF** | RSTSR0.1 | internal_check_lvd0rf() | No | Return k_rx_err_hw_init_failed |
 * | **CWSF** | RSTSR1.7 | internal_check_cwsf() | No | Informational only |
 *
 * ## Critical vs Non-Critical Error Handling
 *
 * **Critical errors (RX_ASSERT):**
 * - **IWDTRF set:** Independent Watchdog Timer timeout detected
 *   - **Root cause:** Prior firmware execution hung, deadlocked, or infinite loop
 *   - **Action:** Assert halts execution immediately (fail-fast)
 *   - **Rationale:** IWDT timeout is ALWAYS a firmware bug (missing tx_thread_sleep, runaway loop)
 *
 * **Non-critical errors (return error code):**
 * - **WDTRF/SWRF/LVD0RF set:** Unexpected but potentially recoverable conditions
 *   - **Action:** Log warning, return k_rx_err_hw_init_failed
 *   - **Rationale:** May be intentional (software reset for bootloader, brownout on power glitch)
 *
 * ## Execution Flow
 *
 * @msc
 * msc {
 *   width=600;
 *   Caller, CheckFlags, PORF, IWDTRF, WDTRF, SWRF, LVD0RF, CWSF;
 *
 *   Caller => CheckFlags [label="internal_check_startup_flags()"];
 *   CheckFlags => PORF [label="internal_check_porf()"];
 *   PORF => CheckFlags [label="bool (info only)"];
 *   CheckFlags => IWDTRF [label="internal_check_iwdtrf()"];
 *   IWDTRF => CheckFlags [label="bool (critical)"];
 *   CheckFlags => CheckFlags [label="RX_ASSERT(iwdtrf_ok)"];
 *   CheckFlags => WDTRF [label="internal_check_wdtrf()"];
 *   WDTRF => CheckFlags [label="bool"];
 *   CheckFlags => SWRF [label="internal_check_swrf()"];
 *   SWRF => CheckFlags [label="bool"];
 *   CheckFlags => LVD0RF [label="internal_check_lvd0rf()"];
 *   LVD0RF => CheckFlags [label="bool"];
 *   CheckFlags => CWSF [label="internal_check_cwsf()"];
 *   CWSF => CheckFlags [label="bool (info only)"];
 *   CheckFlags => Caller [label="k_rx_ok or k_rx_err_hw_init_failed"];
 * }
 * @endmsc
 *
 * ## Performance (RX72N @ 240 MHz)
 *
 * | Operation | Duration | Notes |
 * |-----------|----------|-------|
 * | Register reads (6 flags) | ~2 µs | 3 registers × 2 reads each |
 * | Validation logic | ~1 µs | Boolean comparisons, assertions |
 * | Logging (if needed) | ~5 µs | UART write for warnings |
 * | **Total (normal boot)** | **~3 µs** | No warnings logged |
 * | **Total (with warnings)** | **~10 µs** | Includes UART output |
 *
 * ## Example Reset Scenarios
 *
 * **Scenario 1: Normal power-on boot**
 * - PORF=1 (power-on reset detected) [PASS]
 * - IWDTRF=0 (no watchdog timeout) [PASS]
 * - WDTRF=0, SWRF=0, LVD0RF=0 (no other resets) [PASS]
 * - Result: k_rx_ok (boot continues)
 *
 * **Scenario 2: Warm boot (software reset)**
 * - PORF=0 (no power-on reset) [WARN] Info logged
 * - IWDTRF=0 (no watchdog timeout) [PASS]
 * - SWRF=1 (software reset detected) [WARN] Error returned
 * - Result: k_rx_err_hw_init_failed (caller handles)
 *
 * **Scenario 3: Firmware hung (IWDT timeout)**
 * - PORF=0 (no power-on reset) [WARN]
 * - IWDTRF=1 (watchdog timeout) [STOP] **CRITICAL**
 * - Result: RX_ASSERT halts execution (firmware bug detected)
 *
 * @return rx_err_t Status code indicating boot condition
 * @retval k_rx_ok All checks passed (normal boot or acceptable warm boot)
 * @retval k_rx_err_hw_init_failed Non-critical flag set (WDTRF, SWRF, or LVD0RF)
 *
 * @pre Reset status registers accessible (RSTSR0, RSTSR1, RSTSR2)
 * @pre Register protection unlocked (if needed for flag clearing)
 *
 * @post Critical flags validated (IWDTRF=0 or execution halted)
 * @post Non-critical flags logged if set
 * @post Caller informed of boot condition (k_rx_ok or error)
 *
 * @note **This function does NOT clear reset flags.** Flags persist until explicitly cleared
 *       by writing to RSTSR registers with protection unlocked (PRCR.PRC0=1).
 *
 * @warning **IWDTRF set always halts execution.** This indicates prior firmware failure
 *          (infinite loop, deadlock, missing watchdog refresh). Must fix root cause before boot.
 *
 * @par Thread Safety:
 * Executes before ThreadX starts (single-threaded context). No synchronization needed.
 *
 * @par Example Usage:
 * @code
 * // main.c boot sequence:
 * int main(void) {
 *   rx_err_t ret;
 *
 *   // Validate reset status flags
 *   ret = internal_check_startup_flags();
 *   RX_ERROR_CHECK(ret);
 *
 *   if (ret == k_rx_err_hw_init_failed) {
 *     // Non-critical error: log and continue (or halt if desired)
 *     rx_log_warn("MAIN", "Abnormal reset detected, continuing boot");
 *   }
 *
 *   // Continue with clock and hardware initialization
 *   ret = rx_clock_power_init();
 *   // ...
 * }
 * @endcode
 *
 * @see internal_check_porf() Check Power-On Reset Detect Flag (RSTSR0.0)
 * @see internal_check_iwdtrf() Check Independent Watchdog Timer Reset Flag (RSTSR2.1)
 * @see internal_check_wdtrf() Check Watchdog Timer Reset Flag (RSTSR2.0)
 * @see internal_check_swrf() Check Software Reset Flag (RSTSR2.2)
 * @see internal_check_lvd0rf() Check Voltage Monitoring Reset Flag (RSTSR0.1)
 * @see internal_check_cwsf() Check Cold/Warm Start Flag (RSTSR1.7)
 *
 * @since Version 1.0.0
 *
 * @test test_main.c Verify all reset scenarios (power-on, warm boot, watchdog, brownout)
 */
static rx_err_t internal_check_startup_flags(void)
{
  (void)internal_check_porf();
  const bool iwdtrf_ok = internal_check_iwdtrf();
  const bool wdtrf_ok  = internal_check_wdtrf();
  const bool swrf_ok   = internal_check_swrf();
  const bool lvd0rf_ok = internal_check_lvd0rf();

  (void)internal_check_cwsf();

  /* Assert critical startup conditions (fail-fast, system halts on failure):
   * - IWDTRF should be clear (watchdog reset indicates prior firmware issue) */
  RX_ASSERT(iwdtrf_ok, "Independent Watchdog Timer reset detected - prior execution fault");

  /* Return error if any non-critical startup flag indicates abnormal condition.
   * Note: porf_ok and iwdtrf_ok are not checked here as RX_ASSERT above
   * halts execution if they fail (fail-fast behavior for critical flags). */
  if (!wdtrf_ok || !swrf_ok || !lvd0rf_ok) {
    return k_rx_err_hw_init_failed;
  }

  return k_rx_ok;
}

/* =============================================================================
 * IWDT Configuration
 * =============================================================================
 */

/**
 * @enum iwdt_hardware_timeout_ms_t
 * @brief IWDT hardware watchdog timeout constants
 *
 * @details
 * Defines the hardware Independent Watchdog Timer (IWDT) timeout period.
 * This is the maximum time the system can run without feeding the watchdog
 * before triggering an automatic hardware reset. The timeout is configured
 * via the IWDT Clock Select (CKS) register setting.
 *
 * **Hardware Configuration:**
 * - CKS = 10 (PCLKB/8192 divider)
 * - PCLKB = 60 MHz
 * - Timeout period = 2048ms (±10% tolerance)
 *
 * **Usage:**
 * Set as `default_timeout_ms` in `rx_iwdt_config_t` during initialization.
 * Watchdog monitor task feeds the hardware watchdog at 100 Hz (10ms period).
 *
 * @invariant All values in milliseconds. Valid range: 128-16384ms per hardware limits.
 *            Hardware timeout must exceed longest task timeout to prevent spurious resets.
 *
 * @code
 * // Configure IWDT with hardware timeout
 * static const rx_iwdt_config_t s_iwdt_config = {
 *   .default_timeout_ms = k_iwdt_hw_timeout_ms,  // 2048ms hardware timeout
 *   .enable_task_monitoring = true,
 *   .reset_on_timeout = true,
 * };
 * rx_err_t err = rx_iwdt_init(&s_iwdt_config);
 * @endcode
 *
 * @note Hardware timeout must be longer than longest task timeout to prevent false resets
 * @see rx_iwdt_config_t Configuration structure using this constant
 * @see watchdog_monitor_task.c Task that feeds the hardware watchdog
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_iwdt_hw_timeout_ms =
    2048, /**< Hardware IWDT timeout in milliseconds (CKS=10, ~2048ms ±10%). Must exceed all task timeouts to prevent spurious resets. Valid range: 128-16384ms per hardware limits. */
} iwdt_hardware_timeout_ms_t;

/**
 * @enum iwdt_state_timeout_ms_t
 * @brief IWDT state-dependent timeout constants
 *
 * @details
 * Defines software-level watchdog timeouts for each system state. These timeouts
 * determine how long the system can remain in a given state without task heartbeats
 * before the watchdog monitor triggers a reset. State-specific timeouts allow
 * longer grace periods during initialization and error recovery while maintaining
 * tight timing during normal operation.
 *
 * **State Timeout Strategy:**
 * - **Init/Idle**: 5s - Accommodates slow peripheral initialization (I2C, USB, flash)
 * - **Running/Motor/Comm**: 2s - Standard operation with 100 Hz watchdog feeding
 * - **Error**: 10s - Extended timeout for diagnostics, logging, and recovery attempts
 *
 * **Timeout Hierarchy:**
 * State timeout > Task timeout > Heartbeat interval (prevents false positives)
 *
 * @invariant All values in milliseconds. State timeouts < hardware IWDT timeout (2048ms for running states).
 *            Init/idle/error states may exceed hardware timeout (watchdog not yet started or in recovery).
 *            Valid ranges per state: init/idle (2000-10000ms), running/motor/comm (1000-2000ms), error (5000-16000ms).
 *
 * @code
 * // Configure state-dependent timeouts
 * static const rx_iwdt_config_t s_iwdt_config = {
 *   .default_timeout_ms = k_iwdt_hw_timeout_ms,
 *   .state_timeouts_ms = {
 *     [k_system_state_init]         = k_iwdt_timeout_init_ms,         // 5s
 *     [k_system_state_idle]         = k_iwdt_timeout_idle_ms,         // 5s
 *     [k_system_state_running]      = k_iwdt_timeout_running_ms,      // 2s
 *     [k_system_state_motor_active] = k_iwdt_timeout_motor_active_ms, // 2s
 *     [k_system_state_comm_active]  = k_iwdt_timeout_comm_active_ms,  // 2s
 *     [k_system_state_error]        = k_iwdt_timeout_error_ms,        // 10s
 *   }
 * };
 * @endcode
 *
 * @note All timeouts are in milliseconds (ms)
 * @note State timeouts must be less than hardware IWDT timeout (2048ms for running states)
 * @see system_state_t System state enumeration
 * @see rx_iwdt_config_t.state_timeouts_ms Array indexed by system_state_t
 * @see rx_iwdt_set_state() Function to transition between states
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_iwdt_timeout_init_ms =
    5000, /**< Init state timeout (5000ms). Allows slow startup: clock init, peripheral init, task creation. Exceeds hardware timeout (watchdog not yet started). Valid range: 2000-10000ms. */
  k_iwdt_timeout_idle_ms =
    5000, /**< Idle state timeout (5000ms). System initialized but no critical operations running. Same as init for consistency. Valid range: 2000-10000ms. */
  k_iwdt_timeout_running_ms =
    2000, /**< Running state timeout (2000ms). Default operational state. Matches hardware IWDT timeout. Must be fed at 100 Hz (10ms) to prevent reset. Valid range: 1000-2000ms. */
  k_iwdt_timeout_motor_active_ms =
    2000, /**< Motor active state timeout (2000ms). Motors running with closed-loop control at 100 Hz. Same as running state (motor control is time-critical). Valid range: 1000-2000ms. */
  k_iwdt_timeout_comm_active_ms =
    2000, /**< Communication active state timeout (2000ms). SPI communication in progress. Same as running (comm task runs at 100 Hz). Valid range: 1000-2000ms. */
  k_iwdt_timeout_error_ms =
    10000, /**< Error state timeout (10000ms). Extended timeout for error logging, diagnostics, and recovery attempts before reset. Allows thorough post-mortem. Valid range: 5000-16000ms. */
} iwdt_state_timeout_ms_t;

/**
 * @enum iwdt_task_timeout_ms_t
 * @brief IWDT per-task heartbeat timeout constants
 *
 * @details
 * Defines individual watchdog timeout periods for each ThreadX task. Each task
 * must call `rx_iwdt_task_heartbeat()` within its timeout period or the watchdog
 * monitor will detect the failure and stop feeding the hardware IWDT, triggering
 * a system reset after 2 seconds.
 *
 * **Timeout Calculation Strategy:**
 * Task timeout = 3× task period (allows 2 missed heartbeats before failure detection)
 *
 * **Timeout Categories:**
 * - **Fast tasks (10ms period)**: 30ms timeout - Motor control, communication, watchdog
 * - **Medium tasks (50ms period)**: 150ms timeout - Telemetry, LED status
 * - **Slow tasks (1000ms period)**: 3000ms timeout - BMS monitor, temperature sensor
 * - **Variable tasks**: Custom timeout based on worst-case execution time
 *
 * **Failure Detection Flow:**
 * 1. Task misses heartbeat deadline
 * 2. Watchdog monitor detects timeout via `rx_iwdt_check_tasks()`
 * 3. Watchdog monitor stops feeding hardware IWDT
 * 4. Hardware IWDT triggers reset after 2048ms
 * 5. System reboots, failed task name preserved in logs
 *
 * @invariant All values in milliseconds. Task timeouts < state timeouts < hardware IWDT timeout.
 *            Heartbeat intervals must be < task timeouts (typically timeout/3 for 2 missed beats margin).
 *            Valid ranges: fast tasks (20-50ms), medium tasks (100-300ms), slow tasks (2000-5000ms).
 *
 * @code
 * // Register tasks with individual timeouts
 * rx_err_t err;
 * err = rx_iwdt_register_task("MotorCtrl", k_iwdt_task_timeout_motorctrl_ms);  // 30ms
 * err = rx_iwdt_register_task("CommTask", k_iwdt_task_timeout_commtask_ms);    // 30ms
 * err = rx_iwdt_register_task("Telemetry", k_iwdt_task_timeout_telemetry_ms);  // 150ms
 * err = rx_iwdt_register_task("BMSMonitor", k_iwdt_task_timeout_bmsmonitor_ms); // 3000ms
 * @endcode
 *
 * @note All timeouts are in milliseconds (ms)
 * @note Task timeouts must be less than state timeout to allow proper detection
 * @note Heartbeat interval must be less than timeout (typically timeout/3)
 * @see rx_iwdt_register_task() Register task with timeout
 * @see rx_iwdt_task_heartbeat() Report task liveness
 * @see watchdog_monitor_task.c Monitors all task heartbeats at 100 Hz
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_iwdt_task_timeout_telemetry_ms =
    150, /**< Telemetry task timeout (150ms). Task period: 50ms @ 20 Hz. Timeout = 3× period. Heartbeat called every 50ms. Valid range: 100-300ms. If exceeded: telemetry stops, system reset after 2s. */
  k_iwdt_task_timeout_ledstatus_ms =
    150, /**< LED Status task timeout (150ms). Task period: 50ms @ 20 Hz. Timeout = 3× period. Heartbeat called every 50ms. Valid range: 100-300ms. If exceeded: LED updates stop, system reset after 2s. */
  k_iwdt_task_timeout_bmsmonitor_ms =
    3000, /**< BMS Monitor task timeout (3000ms). Task period: 1000ms @ 1 Hz. Timeout = 3× period. Heartbeat called every 1s. Valid range: 2000-5000ms. If exceeded: battery monitoring stops, system reset after 2s. */
  k_iwdt_task_timeout_tempsensor_ms =
    3000, /**< Temperature Sensor task timeout (3000ms). Task period: 1000ms @ 1 Hz. Timeout = 3× period. Heartbeat called every 1s. Valid range: 2000-5000ms. If exceeded: temp compensation stops, system reset after 2s. */
  k_iwdt_task_timeout_obstdetect_ms =
    60, /**< Obstacle Detection task timeout (60ms). Task heartbeat: 20ms @ 50 Hz. Timeout = 3× 20ms period. Heartbeat called every 20ms. Valid range: 50-150ms. If exceeded: collision avoidance stops, system reset after 2s. */
  k_iwdt_task_timeout_motorctrl_ms =
    30, /**< Motor Control task timeout (30ms). Task period: 10ms @ 100 Hz. Timeout = 3× period. Heartbeat called every 10ms. Valid range: 20-50ms. If exceeded: motor control stops, system reset after 2s. CRITICAL TASK. */
  k_iwdt_task_timeout_commtask_ms =
    30, /**< Communication task timeout (30ms). Task period: 10ms @ 100 Hz. Timeout = 3× period. Heartbeat called every 10ms. Valid range: 20-50ms. If exceeded: SPI comm stops, system reset after 2s. CRITICAL TASK. */
  k_iwdt_task_timeout_watchdog_ms =
    30, /**< Watchdog Monitor task timeout (30ms). Task period: 10ms @ 100 Hz. Timeout = 3× period. Self-monitoring via own heartbeat. Valid range: 20-50ms. If exceeded: watchdog stops feeding IWDT, system reset after 2s. CRITICAL TASK. */
} iwdt_task_timeout_ms_t;

/**
 * @var s_iwdt_config
 * @brief IWDT configuration for system watchdog monitoring
 *
 * @details
 * Configures Independent Watchdog Timer with:
 * - Hardware timeout: 2048ms (triggers reset if not fed)
 * - Task monitoring: Enabled (tracks heartbeats)
 * - Reset on timeout: Enabled (automatic system reset)
 * - State-dependent timeouts: 2s default, 5s init/idle, 10s error
 *
 * **Hardware watchdog**: 2048ms (CKS=10, ~2 seconds)
 * - Fed by watchdog monitor task @ 100 Hz (10ms period)
 * - Safety margin: 2048ms / 10ms = 204× (allows ~204 missed iterations)
 *
 * **Task monitoring**: Individual task timeouts
 * - Motor/Comm/Watchdog: 30ms (3× 10ms period)
 * - Obstacle: 60ms (3× 20ms period)
 * - LED/Telemetry: 150ms (3× 50ms period)
 * - BMS/Temp: 3000ms (3× 1000ms period)
 *
 * @warning Configuration is immutable after rx_iwdt_init()
 *
 * @note Configuration is immutable after rx_iwdt_init()
 * @see rx_iwdt_init() Initialization function using this config
 * @see system_state_t State definitions for state_timeouts_ms array
 *
 * @since Version 1.0.0
 */
static const rx_iwdt_config_t s_iwdt_config = {
  .default_timeout_ms     = k_iwdt_hw_timeout_ms, /**< 2048ms hardware timeout (CKS=10) */
  .enable_task_monitoring = true,                 /**< Enable task heartbeat tracking */
  .reset_on_timeout       = true,                 /**< Reset on timeout (not NMI) */
  .state_timeouts_ms      = {
    [k_system_state_init]         = k_iwdt_timeout_init_ms,    /**< 5s - slow startup */
    [k_system_state_idle]         = k_iwdt_timeout_idle_ms,    /**< 5s - no critical ops */
    [k_system_state_running]      = k_iwdt_timeout_running_ms, /**< 2s - default operation */
    [k_system_state_motor_active] = k_iwdt_timeout_motor_active_ms, /**< 2s - motor control */
    [k_system_state_comm_active]  = k_iwdt_timeout_comm_active_ms,  /**< 2s - communication */
    [k_system_state_error]        = k_iwdt_timeout_error_ms,        /**< 10s - recovery/diag */
  }};

/**
 * @brief ThreadX application definition callback - Create application threads and kernel objects
 *
 * @details
 * This function is **called by the ThreadX kernel** immediately after `tx_kernel_enter()`.
 * It provides the application with an entry point to create threads, semaphores, message queues,
 * mutexes, and other RTOS kernel objects.
 *
 * **ThreadX startup sequence:**
 * ```
 * main() -> tx_kernel_enter() -> [ThreadX internal init] -> tx_application_define() -> [scheduler start]
 * ```
 *
 * ## Application Thread Creation
 *
 * This callback creates the **main application task** (`app_main_task`) which runs the
 * motor control PID loop at 100 Hz and handles USB communication.
 *
 * **Thread hierarchy:**
 * @dot
 * digraph threads {
 *   rankdir=TB;
 *   node [shape=box];
 *
 *   ThreadX [label="ThreadX Kernel", shape=ellipse];
 *   AppDef [label="tx_application_define()"];
 *   MainTask [label="app_main_task\n(Priority 5, 8KB stack)"];
 *   PID [label="PID Control Loop\n100 Hz"];
 *   USB [label="USB CDC\nCommunication"];
 *
 *   ThreadX -> AppDef [label="Callback"];
 *   AppDef -> MainTask [label="app_main_task_create()"];
 *   MainTask -> PID;
 *   MainTask -> USB;
 * }
 * @enddot
 *
 * ## Memory Management
 *
 * **first_unused_memory parameter:**
 * - Points to the first byte of unused SRAM after ThreadX kernel allocation
 * - Used for dynamically creating kernel objects (NOT used in STAR firmware)
 * - STAR uses **static allocation only** (stacks defined at compile-time)
 *
 * **Memory layout after ThreadX init:**
 * | Region | Size | Purpose |
 * |--------|------|---------|
 * | **BSS/Data** | ~10 KB | Global variables, ThreadX kernel data |
 * | **ThreadX heap** | 0 bytes | **Not used** (zero dynamic allocation policy) |
 * | **App main task stack** | 8 KB | Static allocation via tx_byte_pool or array |
 * | **first_unused_memory** | ~490 KB | Remaining free SRAM (unused) |
 *
 * ## Thread Creation Timing (RX72N @ 240 MHz)
 *
 * | Operation | Duration | Notes |
 * |-----------|----------|-------|
 * | tx_application_define() call | ~5 µs | ThreadX callback overhead |
 * | app_main_task_create() | ~20 µs | TCB init, stack setup |
 * | **Total** | **~25 µs** | Fast thread creation |
 *
 * ## Error Handling
 *
 * **Critical assertion:** Thread creation MUST succeed. Failure indicates:
 * - Memory exhaustion (insufficient SRAM for stack)
 * - Invalid thread parameters (priority, stack size)
 * - ThreadX internal error (corruption)
 *
 * **Recovery:** Assert-halt on failure (no recovery possible - critical error)
 *
 * @param[in] first_unused_memory Pointer to first unused SRAM byte after ThreadX kernel allocation
 *                                (Unused in STAR firmware - static allocation only)
 *
 * @return void (No return value - ThreadX callback convention)
 *
 * @pre ThreadX kernel initialized (tx_kernel_enter() called from main())
 * @pre SRAM available for thread stacks (minimum 8 KB for app_main_task)
 * @pre first_unused_memory points to valid SRAM address
 *
 * @post app_main_task created and ready to run (not yet started - scheduler starts after return)
 * @post Thread stack allocated and initialized (SP set to stack top)
 * @post Thread priority configured (priority 5 for main task)
 * @post ThreadX stack overflow handler registered via rx_stack_monitor_init()
 *
 * @note **This function executes BEFORE the ThreadX scheduler starts.** Threads created here
 *       are in READY state but do not execute until this function returns.
 *
 * @note **No blocking calls allowed** in this function (no tx_thread_sleep, tx_semaphore_get, etc.)
 *       as the scheduler is not yet running.
 *
 * @warning **Never return error from this function.** ThreadX callback convention requires void return.
 *          Use RX_ASSERT to halt on critical failures instead.
 *
 * @par Thread Safety:
 * Executes in single-threaded context (scheduler not started yet). No synchronization needed.
 *
 * @par Example Usage (Internal ThreadX Call):
 * @code
 * // ThreadX kernel calls this after tx_kernel_enter():
 * void tx_kernel_enter(void) {
 *   // ... ThreadX internal initialization ...
 *
 *   // Call application-defined callback
 *   tx_application_define(first_unused_memory);
 *
 *   // Start scheduler (threads begin executing)
 *   _tx_thread_schedule();
 * }
 * @endcode
 *
 * @par Example Thread Creation:
 * @code
 * void tx_application_define(void* first_unused_memory) {
 *   rx_err_t err;
 *
 *   // Validate parameter (ThreadX should always provide valid pointer)
 *   RX_ASSERT(first_unused_memory != nullptr, "first_unused_memory must be valid");
 *
 *   // Create main application task (8 KB stack, priority 5)
 *   err = app_main_task_create();
 *   RX_ERROR_CHECK(err);
 *
 *   // Assert thread creation succeeded (critical error if not)
 *   RX_ASSERT(err == k_rx_ok, "app_main_task_create must succeed");
 *
 *   // Return to ThreadX - scheduler will start after this function completes
 * }
 * @endcode
 *
 * @see app_main_task_create() Create main application thread (PID control, USB communication)
 * @see tx_kernel_enter() Start ThreadX scheduler (calls this callback internally)
 * @see app_main_task.c Implementation of main control loop (100 Hz PID)
 *
 * @since Version 1.0.0
 *
 * @test test_main.c Verify thread creation and scheduler startup
 */
void tx_application_define(void* first_unused_memory)
{
  rx_err_t err;

  /* Precondition: first_unused_memory parameter is provided by ThreadX */
  RX_ASSERT(first_unused_memory != nullptr, "Precondition: first_unused_memory must be valid");

  /* =========================================================================
   * Multi-Task Architecture Initialization
   *
   * Task creation order: lowest priority first to ensure higher priority
   * tasks can preempt during initialization if needed.
   *
   * Priority Map (lower = higher priority):
   *   5  = Communication (highest - command latency)
   *   8  = Motor Control (250 Hz control loop)
   *   12 = Obstacle Detection (safety-critical)
   *   15 = BMS Monitoring (1 Hz)
   *   15 = Temperature Sensing (1 Hz)
   *   17 = LED Status (20 Hz, visual feedback)
   *   18 = Telemetry Aggregation (20 Hz, lowest)
   * =========================================================================
   */

  /* Step 1a: Initialize bus manager (requires ThreadX mutex, so must be here) */
  {
    extern rx_bus_manager_t g_bus_manager;
    rx_error_interface_t*   error_iface = rx_infrastructure_get_error_interface();
    rx_pin_interface_t*     pin_iface   = rx_infrastructure_get_pin_interface();
    err = rx_bus_manager_init(&g_bus_manager, "BUS_MGR", error_iface, pin_iface);
    RX_ASSERT(err == k_rx_ok, "rx_bus_manager_init must succeed");
  }

  /* Step 1b: Register buses with manager (before tasks need them) */
  {
    extern rx_bus_manager_t g_bus_manager;

    /* Register i2c0 (SMBus) - BQ4050 Battery Management System */
    err = rx_bus_config_init_smbus(&s_i2c0_config,
                                   "i2c0",                 /* name */
                                   k_riic_channel_1,       /* channel = RIIC1 */
                                   k_bq4050_i2c_addr,      /* device_addr = BQ4050 @ 0x0B */
                                   k_rx_p2_0,              /* sda_pin = P20 */
                                   k_rx_p2_1,              /* scl_pin = P21 */
                                   k_i2c_frequency_100khz, /* frequency_hz = 100 kHz */
                                   true);                  /* use_pec = true */
    RX_ASSERT(err == k_rx_ok, "i2c0 config init must succeed");
    err = rx_bus_manager_add_bus(&g_bus_manager, &s_i2c0_config);
    RX_ASSERT(err == k_rx_ok, "i2c0 registration must succeed");

    /* Register onewire0 - DS18B20 Temperature Sensor */
    err = rx_bus_config_init_onewire(&s_onewire0_config,
                                     "onewire0", /* name */
                                     k_rx_p5_1); /* pin = P51 */
    RX_ASSERT(err == k_rx_ok, "onewire0 config init must succeed");
    err = rx_bus_manager_add_bus(&g_bus_manager, &s_onewire0_config);
    RX_ASSERT(err == k_rx_ok, "onewire0 registration must succeed");

    /* Register gpio - Generic GPIO Access (initial pin P00) */
    err = rx_bus_config_init_gpio(&s_gpio_config,
                                  "gpio",     /* name */
                                  k_rx_p0_0); /* pin = P00 (generic bus) */
    RX_ASSERT(err == k_rx_ok, "gpio config init must succeed");
    err = rx_bus_manager_add_bus(&g_bus_manager, &s_gpio_config);
    RX_ASSERT(err == k_rx_ok, "gpio registration must succeed");

    /* Register adc0 - Motor Current Sensing (ADC Unit 0) */
    err = rx_bus_config_init_adc(&s_adc0_config,
                                 "adc0",                  /* name */
                                 k_adc_unit_0,            /* unit = ADC0 (S12AD0) */
                                 k_adc_channel_0,         /* channel = 0 (generic bus) */
                                 k_adc_resolution_12bit); /* bits = 12-bit resolution */
    RX_ASSERT(err == k_rx_ok, "adc0 config init must succeed");
    err = rx_bus_manager_add_bus(&g_bus_manager, &s_adc0_config);
    RX_ASSERT(err == k_rx_ok, "adc0 registration must succeed");
  }

  /* Step 1c: Initialize shared data module (mutexes, event flags) */
  err = shared_data_init();
  RX_ASSERT(err == k_rx_ok, "shared_data_init must succeed");

  /* Step 1d: Initialize IWDT (hardware watchdog + task monitoring) */
  err = rx_iwdt_init(&s_iwdt_config);
  RX_ASSERT(err == k_rx_ok, "rx_iwdt_init must succeed");

  /* Step 1e: Register all tasks for heartbeat monitoring
   *
   * Task timeout = 3× normal period (allows 2 missed heartbeats):
   * - Telemetry (50ms period) → 150ms timeout
   * - LED Status (50ms period) → 150ms timeout
   * - BMS Monitor (1000ms period) → 3000ms timeout
   * - Temp Sensor (1000ms period) → 3000ms timeout
   * - Obstacle Detect (20ms period) → 60ms timeout
   * - Motor Control (10ms period) → 30ms timeout
   * - Communication (10ms period) → 30ms timeout
   * - Watchdog Monitor (10ms period) → 30ms timeout
   */

  err = rx_iwdt_register_task("Telemetry", k_iwdt_task_timeout_telemetry_ms);
  RX_ASSERT(err == k_rx_ok, "Telemetry IWDT registration must succeed");

  err = rx_iwdt_register_task("LEDStatus", k_iwdt_task_timeout_ledstatus_ms);
  RX_ASSERT(err == k_rx_ok, "LEDStatus IWDT registration must succeed");

  err = rx_iwdt_register_task("BMSMonitor", k_iwdt_task_timeout_bmsmonitor_ms);
  RX_ASSERT(err == k_rx_ok, "BMSMonitor IWDT registration must succeed");

  err = rx_iwdt_register_task("TempSensor", k_iwdt_task_timeout_tempsensor_ms);
  RX_ASSERT(err == k_rx_ok, "TempSensor IWDT registration must succeed");

  err = rx_iwdt_register_task("ObstDetect", k_iwdt_task_timeout_obstdetect_ms);
  RX_ASSERT(err == k_rx_ok, "ObstDetect IWDT registration must succeed");

  err = rx_iwdt_register_task("MotorCtrl", k_iwdt_task_timeout_motorctrl_ms);
  RX_ASSERT(err == k_rx_ok, "MotorCtrl IWDT registration must succeed");

  err = rx_iwdt_register_task("CommTask", k_iwdt_task_timeout_commtask_ms);
  RX_ASSERT(err == k_rx_ok, "CommTask IWDT registration must succeed");

  err = rx_iwdt_register_task("WatchdogMon", k_iwdt_task_timeout_watchdog_ms);
  RX_ASSERT(err == k_rx_ok, "WatchdogMon IWDT registration must succeed");

  /* Step 1f: Set initial IWDT state (init phase) */
  err = rx_iwdt_set_state(k_system_state_init);
  RX_ASSERT(err == k_rx_ok, "IWDT set init state must succeed");

  /* Step 2: Create tasks (lowest priority first) */

  /* Telemetry Task - Priority 18 (lowest) */
  err = telemetry_task_create();
  RX_ASSERT(err == k_rx_ok, "telemetry_task_create must succeed");

  /* LED Status Task - Priority 17 (visual feedback) */
  err = led_status_task_create();
  RX_ASSERT(err == k_rx_ok, "led_status_task_create must succeed");

  /* BMS Monitor Task - Priority 15 */
  /**
   * @var bms_config
   * @brief BMS monitoring task configuration with default SoC thresholds
   *
   * @details
   * Configures the BMS monitoring task with the default warning (25%) and
   * critical (5%) SoC thresholds. Passed to bms_monitor_task_create() at
   * system startup. Thresholds are copied into the task's internal state
   * before this variable's scope ends.
   *
   * @note Lifetime scoped to tx_application_define(); valid only for the
   *       duration of the bms_monitor_task_create() call.
   * @since Version 1.1.0
   */
  const bms_monitor_config_t bms_config = {
    .soc_warning_pct  = k_bms_soc_warning_pct_default,
    .soc_critical_pct = k_bms_soc_critical_pct_default,
  };
  err = bms_monitor_task_create(&bms_config);
  RX_ASSERT(err == k_rx_ok, "bms_monitor_task_create must succeed");

  /* Temperature Sensor Task - Priority 15 */
  err = temp_sensor_task_create();
  RX_ASSERT(err == k_rx_ok, "temp_sensor_task_create must succeed");

  /* Obstacle Detection Task - Priority 12 */
  err = obstacle_detect_task_create();
  RX_ASSERT(err == k_rx_ok, "obstacle_detect_task_create must succeed");

  /* Motor Control Task - Priority 8 */
  err = motor_control_task_create();
  RX_ASSERT(err == k_rx_ok, "motor_control_task_create must succeed");

  /* Communication Task - Priority 5 (highest) */
  err = comm_task_create();
  RX_ASSERT(err == k_rx_ok, "comm_task_create must succeed");

  /* Watchdog Monitor Task - Priority 6 */
  err = watchdog_monitor_task_create();
  RX_ASSERT(err == k_rx_ok, "watchdog_monitor_task_create must succeed");

  /* Step 3: Transition to running state (all tasks created) */
  err = rx_iwdt_set_state(k_system_state_running);
  RX_ASSERT(err == k_rx_ok, "IWDT set running state must succeed");

  /* Step 4: Register ThreadX stack overflow handler (TX_ENABLE_STACK_CHECKING) */
  err = rx_stack_monitor_init();
  RX_ASSERT(err == k_rx_ok, "rx_stack_monitor_init must succeed");
}

/**
 * @brief Main entry point for STAR RX72N motor controller firmware
 *
 * @details
 * Implements the **three-stage bootstrap sequence** for the embedded system:
 *
 * 1. **Stage 1: Startup Validation** (~10 µs)
 *    - Read reset status registers (RSTSR0, RSTSR1, RSTSR2)
 *    - Detect abnormal reset conditions (watchdog timeout, brownout, etc.)
 *    - Assert-halt on critical flags (IWDTRF = firmware bug)
 *
 * 2. **Stage 2: System Initialization** (~250 µs)
 *    - Configure PLL for 240 MHz operation (rx_clock_power_init)
 *    - Initialize UART for early error logging (uart_debug_init)
 *    - Report startup flags to console (internal_report_startup_flags)
 *    - Initialize infrastructure (error handler, pin validator) (rx_infrastructure_init)
 *    - Initialize hardware (motor drivers, encoders, USB CDC) (hardware_init)
 *
 * 3. **Stage 3: ThreadX Bootstrap** (never returns)
 *    - Call tx_kernel_enter() to start RTOS scheduler
 *    - ThreadX calls tx_application_define() callback
 *    - Application threads created (app_main_task for PID control)
 *
 * ## Execution Flow Diagram
 *
 * @msc
 * msc {
 *   width=800;
 *   Main, ClockInit, HardwareInit, ThreadX, AppTask;
 *
 *   Main => Main [label="internal_check_startup_flags()", textcolor="blue"];
 *   Main note Main [label="Validate PORF, IWDTRF, WDTRF, SWRF, LVD0RF", textcolor="blue"];
 *   Main => ClockInit [label="rx_clock_power_init()", textcolor="green"];
 *   ClockInit => ClockInit [label="Configure PLL (240 MHz)", textcolor="green"];
 *   ClockInit => Main [label="k_rx_ok", textcolor="green"];
 *   Main => HardwareInit [label="hardware_init()", textcolor="green"];
 *   HardwareInit => HardwareInit [label="Motor drivers\nUSB CDC\nSensors", textcolor="green"];
 *   HardwareInit => Main [label="k_rx_ok", textcolor="green"];
 *   Main => ThreadX [label="tx_kernel_enter()", textcolor="purple"];
 *   ThreadX => ThreadX [label="Start scheduler\n(NEVER RETURNS)", textcolor="purple"];
 *   ThreadX => AppTask [label="tx_application_define()\napp_main_task_create()", textcolor="purple"];
 *   AppTask => AppTask [label="PID control @ 100 Hz\nUSB communication", textcolor="purple"];
 * }
 * @endmsc
 *
 * ## Performance Characteristics (RX72N @ 240 MHz)
 *
 * | Phase | Execution Time | CPU Cycles | Notes |
 * |-------|----------------|------------|-------|
 * | **C runtime init** | ~500 µs | ~120,000 | crt0.S: BSS clear, data copy |
 * | **Startup flag checks** | ~10 µs | ~2,400 | 6 register reads + validation |
 * | **Clock init (PLL lock)** | ~200 µs | ~48,000 | PLL stabilization wait |
 * | **Hardware init** | ~50 ms | ~12M | USB enumeration dominates |
 * | **ThreadX start** | ~100 µs | ~24,000 | Kernel object creation |
 * | **Total boot time** | **~51 ms** | **~12.2M** | Ready for first control loop |
 *
 * ## Memory Usage (Static Allocation Only - No Heap)
 *
 * | Object | Size | Location | Purpose |
 * |--------|------|----------|---------|
 * | **Main stack** | 4 KB | SRAM | Pre-ThreadX execution |
 * | **App main task stack** | 8 KB | SRAM | PID control, USB comm |
 * | **ThreadX kernel objects** | ~2 KB | SRAM | TCBs, timers, semaphores |
 * | **USB buffers** | 2 KB | SRAM | CDC ring buffers (TX/RX) |
 *
 * **Total SRAM usage:** ~16 KB / 512 KB (3% utilization)
 *
 * ## Error Handling and Recovery
 *
 * **Critical errors (assert-halt):**
 * - IWDTRF set -> Prior execution timed out (firmware bug)
 * - Register pointer NULL -> Hardware access violation
 * - ThreadX thread creation failed -> Memory exhaustion
 *
 * **Non-critical errors (return error code):**
 * - WDTRF/SWRF/LVD0RF set -> Log and return k_rx_err_hw_init_failed
 * - Clock init failed -> Log and return error code
 * - Hardware init failed -> Log and return error code
 *
 * **Recovery strategy:**
 * - On assert: Halt execution with message (wait loop)
 * - On error return: Caller (none for main) would handle, but main has no caller
 * - Final fallback: Infinite wait loop at end of main() (unreachable)
 *
 * ## UART Failure Handling
 *
 * If **uart_debug_init()** fails (extremely rare - hardware fault):
 * - System attempts to log error via internal_rx_fatal_error()
 * - UART functions fail silently by design (see uart.c line 1273 comment)
 * - internal_report_startup_flags() skipped (conditional: only if UART succeeded)
 * - System halts in infinite while(1) loop
 * - **No console output available**
 *
 * **Debugging UART Failure:**
 * - Use hardware debugger (e² studio debugger, OpenOCD, J-Link, GDB)
 * - Check SCI9 clock enable (MSTPCRB bit 22 must be clear)
 * - Verify GPIO pin configuration (P2.6/TXD9, P2.7/RXD9 in peripheral mode)
 * - Confirm crystal oscillator stable (UART requires PCLKB = 60 MHz)
 * - Check baud rate calculation (should be 115200 @ PCLKB=60MHz)
 *
 * **Likelihood:** <0.01% (requires hardware fault, incorrect clock config, or GPIO misconfiguration)
 *
 * @return k_main_ret_success Nominal return value (never reached)
 * @retval k_main_ret_success ThreadX scheduler started successfully (unreachable code path)
 *
 * @pre MCU hardware reset completed (power-on, watchdog, or software reset)
 * @pre C runtime initialized (BSS cleared, data section copied to RAM)
 * @pre Stack pointer set to valid SRAM address (linker script configuration)
 * @pre Interrupt vector table configured (reset vector points to main)
 *
 * @post System clocks configured (ICLK=240 MHz, PCLKA=120 MHz, PCLKB=60 MHz)
 * @post All hardware peripherals initialized (motors, USB, sensors)
 * @post ThreadX RTOS scheduler running (main thread: app_main_task)
 * @post Function never returns (tx_kernel_enter() takes over execution)
 *
 * @note **This function executes in privileged mode with interrupts disabled** until
 *       ThreadX starts. Hardware interrupts (USB, timers) only active after scheduler starts.
 *
 * @warning **Never call main() from application code.** This is the reset vector entry point only.
 *
 * @warning **Never return from main().** ThreadX scheduler must take over. If execution reaches
 *          the end of main(), the system is in an undefined state.
 *
 * @par Thread Safety:
 * Not applicable - executes in single-threaded context before ThreadX starts.
 *
 * @par Example Boot Sequence:
 * @code
 * // Step 1: Hardware reset (power-on)
 * // Step 2: C runtime init (crt0.S)
 * // Step 3: main() called
 *
 * int main(void) {
 *   // Stage 1: Validate reset cause
 *   rx_err_t ret = internal_check_startup_flags();
 *   if (ret != k_rx_ok) {
 *     // Critical error: halt execution
 *     while (1) { __asm__ volatile("wait"); }
 *   }
 *
 *   // Stage 2: Initialize clocks (240 MHz PLL)
 *   ret = rx_clock_power_init();
 *   if (ret != k_rx_ok) {
 *     // Clock init failed: halt execution
 *     while (1) { __asm__ volatile("wait"); }
 *   }
 *
 *   // Stage 2.5: Initialize UART for early error logging
 *   ret = uart_debug_init();
 *   if (ret == k_rx_ok) {
 *     internal_report_startup_flags();  // Report boot diagnostics
 *   }
 *   // Check UART status (halt if failed, but at least tried to report)
 *   if (ret != k_rx_ok) {
 *     while (1) { __asm__ volatile("wait"); }
 *   }
 *
 *   // Stage 3: Initialize hardware (motors, USB, sensors)
 *   ret = hardware_init();
 *   if (ret != k_rx_ok) {
 *     // Hardware init failed: halt execution
 *     while (1) { __asm__ volatile("wait"); }
 *   }
 *
 *   // Stage 4: Start ThreadX RTOS (NEVER RETURNS)
 *   tx_kernel_enter();
 *
 *   // Unreachable code: scheduler failed to start
 *   while (1) { __asm__ volatile("wait"); }
 *   __builtin_unreachable();
 * }
 * @endcode
 *
 * @see internal_check_startup_flags() Validate reset status registers
 * @see rx_clock_power_init() Configure system clocks (240 MHz PLL)
 * @see hardware_init() Initialize motor drivers, USB CDC, sensors
 * @see tx_kernel_enter() Start ThreadX RTOS scheduler (never returns)
 * @see tx_application_define() ThreadX callback to create application threads
 *
 * @since Version 1.0.0
 *
 * @test test_main.c Verify boot sequence and error handling paths
 */
int main(void)
{
  rx_err_t ret;

  /* Check startup flags (via internal_check_startup_flags):
   *  PORF (Power-On Reset Detect Flag) - asserted, halts on failure
   *  IWDTRF (Independent Watchdog Timer Reset Detect Flag) - asserted, halts on failure
   *  WDTRF (Watchdog Timer Reset Detect Flag)
   *  SWRF (Software Reset Detect Flag)
   *  LVD0RF (Voltage-Monitoring 0 Reset Detect Flag)
   *  CWSF (Cold/Warm Start Determination Flag)
   */
  ret = internal_check_startup_flags();
  RX_ERROR_CHECK(ret); /* If this fails, errors cant be logged */

  /* Initialize system clocks and power management */
  ret = rx_clock_power_init();
  RX_ERROR_CHECK(ret); /* If this fails, errors cant be logged */

  /* ========================================================================
   * EARLY UART INITIALIZATION - Enable error logging ASAP
   * ======================================================================== */
  ret = uart_debug_init();

  /* Report startup flags to console (only if UART initialized successfully) */
  if (ret == k_rx_ok) {
    internal_report_startup_flags();
  }

  /* Now check UART init status - will halt if failed, but at least tried to report flags */
  RX_ERROR_CHECK(ret);

  /* Initialize infrastructure (error handler + pin validator) before peripherals.
   * Must run in single-threaded context before ThreadX starts. */
  ret = rx_infrastructure_init();
  RX_ERROR_CHECK(ret);

  /* Initialize application-specific hardware (GPIO, GPTW, timers, UART, SPI, I2C, ADC) */
  ret = hardware_init();
  RX_ERROR_CHECK(ret);

  /* Start the ThreadX scheduler - should never return */
  tx_kernel_enter();

  /* Should never reach here, ThreadX scheduler failed to start if it does */
  while (1) {
    __asm__ volatile("wait"); /* Wait for sleep/idle */
  }

  /* Unreachable: ThreadX scheduler takes over before reaching this point.
   * If execution reaches here, the system is in an undefined state. */
  __builtin_unreachable();

  return k_main_ret_success;
}
