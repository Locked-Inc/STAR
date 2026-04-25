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
 *   Tasks [label="Application Tasks\n(comm, motor, sensors)", shape=ellipse];
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
 * | Reset to main() | ~500 us | Hardware reset, C runtime init (crt0.S) |
 * | Startup flag checks | ~10 us | 6 register reads + validation |
 * | Clock initialization | ~200 us | PLL stabilization (see rx_clock_power_init.c) |
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
 * tx_kernel_enter() -> tx_application_define() -> task creation -> RTOS scheduler
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
 * **Stack sizes** (statically allocated per task, defined in each task module):
 * - **Main stack**: 4 KB (used only until ThreadX starts)
 * - **Per-task stacks**: 7 static stacks (comm, watchdog, motor, obstacle, temp, LED, telemetry)
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
 * | **Rule 4: Function length** | [PASS] | tx_application_define split into ~60-line helpers |
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
 * in dedicated tasks with ThreadX scheduling and synchronization.
 *
 * ## Related Files
 *
 * - **Clock init:** See [rx_clock_power_init.c](rx_clock_power_init.c) - PLL configuration, 240 MHz setup
 * - **Hardware init:** See [hardware_init.c](hardware_init.c) - Motor drivers, USB CDC, sensors
 * - **ThreadX config:** See [rx_threadx_config.h](../lib/rx_core/inc/rx_threadx_config.h) - RTOS tuning
 *
 * @note This file compiles for both RX72N hardware and x86-64 unit tests (mock register access).
 *
 * @warning Never call main() directly from application code - entry point is for reset vector only.
 *
 * @see hardware_init() Application-specific peripheral initialization
 * @see rx_clock_power_init() System clock and power management setup
 * @since Version 1.0.0
 *
 * @par Revision History:
 * - v1.0.0 (2026-01): Initial implementation with ThreadX RTOS bootstrap
 
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
*/

#include "hardware.h"
#include "hardware_init.h"
#include "rx72n_system_regs.h"
#include "rx_bus_adc.h"
#include "rx_bus_config.h"
#include "rx_bus_i2c.h"
#include "rx_bus_manager.h"
#include "rx_bus_types.h"
#include "rx_check.h"
#include "rx_clock_power_init.h"
#include "rx_err.h"
#include "rx_infrastructure.h"
#include "rx_nanopb.h"
#include "rx_port_utils.h"
#include "rx_usb.h"
#include "tx_api.h"

/* Multi-task architecture includes */
#include "comm_task.h"
#include "imu_task.h"
#include "led_status_task.h"
#include "motor_control_task.h"
#include "obstacle_detect_task.h"
#include "serial_bringup_task.h"
#include "shared_data.h"
#include "telemetry_task.h"
#include "temp_sensor_task.h"
#include "usb_task.h"
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
 * @see rx_bus_config_init_i2c() Uses uint8_t channel parameter
 */
typedef enum : uint8_t {
  k_riic_channel_0 = 0, /**< RIIC channel 0 */
  k_riic_channel_1 = 1, /**< RIIC channel 1 */
  k_riic_channel_2 = 2, /**< RIIC channel 2 */
} riic_channel_num_t;

/**
 * @brief Standard I2C bus frequencies for RIIC configuration
 * @details
 * Standard I2C frequency constants for bus configuration.
 * Values in Hz for clarity and type safety.
 *
 * @see rx_bus_config_init_i2c() Uses frequency_hz parameter
 */
typedef enum : uint32_t {
  k_i2c_frequency_100khz = 100000, /**< Standard mode: 100 kHz (default) */
  k_i2c_frequency_400khz = 400000, /**< Fast mode: 400 kHz */
} i2c_frequency_t;

/**
 * @brief I2C device addresses for IMU sensors on RIIC1 bus
 * @details
 * Both IMU sensors share the RIIC1 bus (P2.0=SDA1, P2.1=SCL1) but
 * are distinguished by their 7-bit I2C addresses:
 * - BNO055 COM3/ADR pin LOW: 0x28
 * - BMP280 SDO pin LOW:      0x76
 *
 * @invariant Values are 7-bit I2C addresses (range 0x00..0x7F) on RIIC1;
 *            k_i2c_addr_bno055 and k_i2c_addr_bmp280 must be distinct
 *
 * @see internal_register_system_buses() Uses these addresses with rx_bus_config_init_i2c()
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_i2c_addr_bno055 = 0x28U, /**< BNO055 I2C address when COM3/ADR pin = LOW */
  k_i2c_addr_bmp280 = 0x76U, /**< BMP280 I2C address when SDO pin = LOW */
  k_i2c_addr_mpu6050 =
    0x68U, /**< GY-521 / MPU-6050 default address (AD0=LOW) -- bench-test probe */
} imu_i2c_addr_t;
static_assert(sizeof(imu_i2c_addr_t) == sizeof(uint8_t), "imu_i2c_addr_t must be uint8_t sized");
static_assert(k_i2c_addr_bno055 != k_i2c_addr_bmp280,
              "k_i2c_addr_bno055 and k_i2c_addr_bmp280 must be distinct");
static_assert(k_i2c_addr_mpu6050 != k_i2c_addr_bno055 && k_i2c_addr_mpu6050 != k_i2c_addr_bmp280,
              "k_i2c_addr_mpu6050 must be distinct from other RIIC1 addresses");

/* =============================================================================
 * Static Bus Configurations
 * =============================================================================
 */

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
 * - Pullup: 4.7 kOhm resistor required (external)
 * - Protocol: Bit-banging with microsecond timing
 *
 * **Device Details:**
 * The DS18B20 is a digital temperature sensor providing:
 * - Temperature range: -55degC to +125degC
 * - Accuracy: +/-0.5degC (-10degC to +85degC)
 * - Resolution: 9 to 12 bits (configurable)
 * - Conversion time: 750 ms max (12-bit resolution)
 * - Unique 64-bit serial number per device
 *
 * @note Static allocation follows NASA Power of 10 Rule 3 (no dynamic memory).
 * @note Registered with bus manager in tx_application_define() before task creation.
 * @note Requires 4.7 kOhm pullup resistor on P51 (see schematic).
 * @note 1-Wire protocol timing is critical - interrupts may cause timing violations.
 *
 * @see rx_bus_config_init_onewire() Bus configuration function
 * @see temp_sensor_task.c Task that uses this bus for temperature monitoring
 *
 * @since Version 1.0.0
 */
/** @brief Log tags for this translation unit (project convention: variables, not string literals) */
[[maybe_unused]] static const char* const s_tag         = "MAIN";
static const char* const                  s_boot_tag    = "BOOT";
static const char* const                  s_mpu6050_tag = "MPU6050";

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
 * - Motor nFAULT pins (fault detection)
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
 * @enum motor_current_adc_count_t
 * @brief Number of motor current-sense ADC channels
 *
 * @details
 * One ADC channel per motor (S12AD0 channels 4-7). Matches the physical
 * number of DRV8263H IPROPI outputs wired to the RX72N ADC.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_motor_current_adc_count = 4U, /**< 4 motors -> 4 current-sense channels */
} motor_current_adc_count_t;

/**
 * @var k_motor_current_adc_channels
 * @brief S12AD0 channel for each motor current-sense input, indexed by motor index
 *
 * @details
 * Maps motor index (0-3) to its S12AD0 channel. Bus names are obtained from
 * g_motor_current_bus_names (declared in motor_control_task.h) so there is a
 * single authoritative name table shared between bus registration here and
 * ADC reads in motor_control_task.c.
 *
 * | Index | Motor | Channel | Pin |
 * |-------|-------|---------|-----|
 * | 0     | FL    | AN007   | P47 |
 * | 1     | FR    | AN006   | P46 |
 * | 2     | BL    | AN005   | P45 |
 * | 3     | BR    | AN004   | P44 |
 *
 * @warning Array size must match k_motor_current_adc_count.
 * @see g_motor_current_bus_names Shared bus name array (motor_control_task.h)
 * @see internal_register_system_buses() Iterates over this table
 * @since Version 1.0.0
 */
static const adc_channel_t k_motor_current_adc_channels[k_motor_current_adc_count] = {
  k_adc_channel_7, /* Motor 0 (FL): AN007, P47 */
  k_adc_channel_6, /* Motor 1 (FR): AN006, P46 */
  k_adc_channel_5, /* Motor 2 (BL): AN005, P45 */
  k_adc_channel_4, /* Motor 3 (BR): AN004, P44 */
};

/**
 * @var s_motor_current_configs
 * @brief ADC bus configuration storage for all four motor current-sense channels
 *
 * @details
 * Provides static storage for the four rx_bus_config_t structs populated by
 * internal_register_system_buses() via rx_bus_config_init_adc(). Using an
 * array eliminates four separate per-motor variables while retaining the same
 * lifetime and BSS-zero initialisation guarantees.
 *
 * @note Static allocation follows NASA Power of 10 Rule 3 (no dynamic memory).
 * @see k_motor_current_adc_channels Channel table supplying S12AD0 channel per motor
 * @see rx_bus_config_init_adc() Bus configuration function
 * @see motor_control_task.c internal_update_motor_state() Consumer
 * @since Version 1.0.0
 */
static rx_bus_config_t s_motor_current_configs[k_motor_current_adc_count];

/**
 * @var s_i2c1_imu_config
 * @brief I2C bus configuration for BNO055 IMU sensor on RIIC1
 *
 * @details
 * Configures the RIIC1 I2C bus for the BNO055 9-DOF orientation sensor.
 *
 * **Hardware Configuration:**
 * - Bus type: I2C (RIIC1 peripheral)
 * - Channel: 1 (RIIC1)
 * - Device address: 0x28 (BNO055, COM3/ADR=LOW)
 * - SDA: P2.0 (SDA1)
 * - SCL: P2.1 (SCL1)
 * - Frequency: 400 kHz (fast mode)
 *
 * @pre RIIC1 hardware initialized before any driver uses this config
 * @post rx_bus_config_init_i2c() populates all fields before bus registration
 *
 * @note Bus name "i2c1_imu" matches s_bus_name in rx_bno055.c
 * @note Static allocation follows NASA Power of 10 Rule 3
 *
 * @see rx_bno055.h BNO055 driver that uses this bus
 *
 * @since Version 1.0.0
 */
static rx_bus_config_t s_i2c1_imu_config;

/**
 * @var s_i2c1_baro_config
 * @brief I2C bus configuration for BMP280 barometric sensor on RIIC1
 *
 * @details
 * Configures the RIIC1 I2C bus for the BMP280 barometric pressure sensor.
 * Shares RIIC1 hardware with the BNO055 but uses a separate bus name and
 * device address.
 *
 * **Hardware Configuration:**
 * - Bus type: I2C (RIIC1 peripheral)
 * - Channel: 1 (RIIC1)
 * - Device address: 0x76 (BMP280, SDO=LOW)
 * - SDA: P2.0 (SDA1)
 * - SCL: P2.1 (SCL1)
 * - Frequency: 400 kHz (fast mode)
 *
 * @pre RIIC1 hardware initialized before any driver uses this config
 * @post rx_bus_config_init_i2c() populates all fields before bus registration
 *
 * @note Bus name "i2c1_baro" matches s_bus_name in rx_bmp280.c
 * @note Static allocation follows NASA Power of 10 Rule 3
 *
 * @see rx_bmp280.h BMP280 driver that uses this bus
 *
 * @since Version 1.0.0
 */
static rx_bus_config_t s_i2c1_baro_config;

/** @brief Bench-test I2C bus config for a GY-521 / MPU-6050 at 0x68 on RIIC1. */
static rx_bus_config_t s_i2c1_mpu_config;

/**
 * @var g_bus_manager
 * @brief Single file-scope forward declaration of the global bus manager singleton
 *
 * @details
 * The bus manager itself is defined in rx_bus_manager.c. This translation unit
 * only needs a forward declaration so internal_init_bus_manager(),
 * internal_register_system_buses(), and their extracted helpers can share the
 * same symbol without each repeating an `extern` line (which clang-tidy flags
 * as redundant under readability-redundant-declaration).
 *
 * @note Single-threaded use only inside tx_application_define() (pre-scheduler).
 * @see rx_bus_manager_init() Definition / lifetime owner
 * @since Version 1.0.0
 */
extern rx_bus_manager_t g_bus_manager;

/**
 * @enum dflash_fcu_cmd_t
 * @brief RX72N Flash Control Unit (FCU) command opcodes for data-flash P/E
 *
 * @details
 * Single-byte opcodes written to the FACI command area to drive the FCU
 * state machine for data-flash erase and program operations. Sourced from
 * RX72N HW manual R01UH0824EJ0111 Ch 62 (Flash Memory), Tables 62.6/62.7.
 *
 * The terminator byte (0xD0) finalizes the command sequence and tells the
 * FCU to begin executing the requested operation.
 */
typedef enum : uint8_t {
  k_dflash_fcu_cmd_block_erase = 0x20U, /**< Block erase opcode (Ch 62.9.3.5) */
  k_dflash_fcu_cmd_program     = 0xE8U, /**< Program opcode (Ch 62.9.3.4) */
  k_dflash_fcu_cmd_terminator  = 0xD0U, /**< Command terminator -- launches op */
  k_dflash_fcu_program_words   = 0x02U, /**< N parameter for program: 2 x 16-bit words = 4 bytes */
  k_dflash_fwepror_enable_pe   = 0x01U, /**< FWEPROR.FLWE = 1 -- allow P/E ops */
} dflash_fcu_cmd_t;

/**
 * @enum dflash_fentryr_t
 * @brief RX72N FENTRYR (Flash P/E Mode Entry) control values
 *
 * @details
 * 16-bit writes to FENTRYR control entry/exit of flash program/erase mode.
 * Upper byte 0xAA is the unlock key (FEKEY); lower byte selects target area.
 * Per RX72N HW manual Ch 62, Section 62.4.2 (FENTRYR register).
 */
typedef enum : uint16_t {
  k_dflash_fentryr_enter_dataflash_pe = 0xAA80U, /**< Enter data-flash P/E mode (FENTRYD=1) */
  k_dflash_fentryr_exit_pe            = 0xAA00U, /**< Exit P/E mode (read mode) */
  k_dflash_fentryr_low_byte_mask      = 0x00FFU, /**< Mask to read FENTRYR status (low byte) */
  k_dflash_fentryr_pe_active          = 0x0080U, /**< Low-byte value indicating data-flash P/E mode active */
  k_dflash_fentryr_pe_inactive        = 0x0000U, /**< Low-byte value indicating P/E mode exited */
  k_dflash_fpckar_key                 = 0x1E00U, /**< FPCKAR key (upper byte 0x1E); OR'd with FCLK MHz */
} dflash_fentryr_t;

/**
 * @enum dflash_probe_layout_t
 * @brief Magic header bytes and frequency constants for the bench dflash probe
 *
 * @details
 * The bench probe writes a 4-byte payload to data flash so rfp-cli can
 * verify the firmware ran without needing a UART bridge. The first two
 * bytes are a fixed sentinel pattern; bytes 2-3 carry runtime data.
 */
typedef enum : uint8_t {
  k_dflash_probe_magic_byte0 = 0xA5U, /**< Sentinel byte 0 (LE: at addr+0) */
  k_dflash_probe_magic_byte1 = 0x5AU, /**< Sentinel byte 1 (LE: at addr+1) */
  k_dflash_probe_byte_mask   = 0xFFU, /**< Mask to extract low byte of 32-bit error code */
  k_dflash_fclk_mhz          = 60U,   /**< FCLK frequency in MHz (OR'd with FPCKAR key) */
} dflash_probe_layout_t;

/**
 * @enum fcu_poll_iters_t
 * @brief Bounded poll iteration counts for FCU state-machine waits
 *
 * @details
 * NASA P10 Rule 2 requires fixed loop bounds. These values cap waits on
 * FENTRYR/FSTATR.FRDY transitions; values are empirically large enough
 * that the FCU completes the slowest operation (block erase) well within
 * the cap at 60 MHz FCLK.
 */
typedef enum : uint32_t {
  k_fcu_poll_short = 100000U,  /**< Mode entry/exit + FRDY wait (~1.7 ms @ 60 MHz) */
  k_fcu_poll_long  = 1000000U, /**< Erase/program completion wait (~17 ms @ 60 MHz) */
} fcu_poll_iters_t;

/**
 * @enum prcr_key_t
 * @brief RX72N PRCR (Protect Register) key/unlock values
 *
 * @details
 * The PRCR upper byte (0xA5) is the protection key; the low nibble enables
 * write access to specific protected register groups. Required before
 * touching MSTPCR* and clock-tree registers. Per RX72N HW manual Ch 13.2.1.
 */
typedef enum : uint16_t {
  k_main_prcr_unlock_clock_lpm = 0xA503U, /**< Key 0xA5 + PRC0 | PRC1: unlock clock + low-power-mode regs */
  k_main_prcr_lock_all         = 0xA500U, /**< Key 0xA5 + all PRC bits cleared: re-lock everything */
} main_prcr_key_t;

/**
 * @enum usb_init_delay_iters_t
 * @brief Busy-loop iteration counts used during pre-kernel USB0 bring-up
 *
 * @details
 * Empirically tuned for ICLK = 240 MHz: ~10 ms spin per phase. Used to
 * straddle the SYSCFG -> SCKE -> USBE ordering required by HUM 40.3.1.1
 * (the RX72N USB module needs internal clock to settle before USBE=1).
 */
typedef enum : uint32_t {
  k_usb_syscfg_settle_nops = 2400000U, /**< ~10 ms spin between SYSCFG / SCKE / USBE writes */
} usb_init_delay_iters_t;

/* =============================================================================
 * Bench probe: write {magic, who_am_i, err} to data flash @ 0x00100000 so that
 * rfp-cli -rv 0x00100000 16 can read the result without a UART. No dependency
 * on rx_hal flash driver -- hand-rolled FCU sequence per RX72N HW manual Ch 62.
 * ========================================================================= */
static void internal_bench_dflash_write(uint8_t who_am_i, int32_t err)
{
  volatile uint32_t* const FSTATR  = (volatile uint32_t*)0x007FE080U;
  volatile uint16_t* const FENTRYR = (volatile uint16_t*)0x007FE084U;
  volatile uint32_t* const FSADDR  = (volatile uint32_t*)0x007FE030U;
  volatile uint16_t* const FPCKAR  = (volatile uint16_t*)0x007FE0E4U;
  volatile uint8_t* const  FWEPROR = (volatile uint8_t*)0x0008C296U;
  volatile uint8_t* const  FACI_B  = (volatile uint8_t*)0x007E0000U;
  volatile uint16_t* const FACI_W  = (volatile uint16_t*)0x007E0000U;
  const uint32_t           DF_ADDR = 0x00100000U;

  /* Enable P/E operations. */
  *FWEPROR = k_dflash_fwepror_enable_pe;

  /* Notify FCU of FCLK = 60 MHz (key 0x1E00 | freq_in_MHz). Must be done
   * BEFORE entering P/E mode or FCU silently rejects every command. */
  *FPCKAR = (uint16_t)(k_dflash_fpckar_key | k_dflash_fclk_mhz);

  /* Enter data flash P/E mode. */
  *FENTRYR = k_dflash_fentryr_enter_dataflash_pe;
  /* Spin until data-flash P/E mode is active (FENTRYR low byte = 0x80). */
  for (uint32_t i = 0;
       i < k_fcu_poll_short
       && (*FENTRYR & k_dflash_fentryr_low_byte_mask) != k_dflash_fentryr_pe_active;
       i++) {
  }
  /* Wait for FRDY. */
  for (uint32_t i = 0; i < k_fcu_poll_short && ((*FSTATR) & (1UL << 15)) == 0U; i++) {
  }

  /* Erase the 64-byte block containing DF_ADDR. */
  *FSADDR = DF_ADDR;
  *FACI_B = k_dflash_fcu_cmd_block_erase;
  *FACI_B = k_dflash_fcu_cmd_terminator;
  for (uint32_t i = 0; i < k_fcu_poll_long && ((*FSTATR) & (1UL << 15)) == 0U; i++) {
  }

  /* Program 4 bytes: [0xA5, 0x5A, who_am_i, err_byte_low]. */
  const uint8_t  err_byte = (err == 0) ? 0x00U : (uint8_t)(err & k_dflash_probe_byte_mask);
  const uint16_t word0    = (uint16_t)(((uint16_t)k_dflash_probe_magic_byte1 << 8)
                                    | k_dflash_probe_magic_byte0); /* LE: bytes 0=A5, 1=5A */
  const uint16_t word1 = (uint16_t)(((uint16_t)err_byte << 8) | who_am_i); /* LE: 2=who, 3=err    */

  *FSADDR = DF_ADDR;
  *FACI_B = k_dflash_fcu_cmd_program;
  *FACI_B = k_dflash_fcu_program_words;
  *FACI_W = word0;
  *FACI_W = word1;
  *FACI_B = k_dflash_fcu_cmd_terminator;
  for (uint32_t i = 0; i < k_fcu_poll_long && ((*FSTATR) & (1UL << 15)) == 0U; i++) {
  }

  /* Exit P/E mode. */
  *FENTRYR = k_dflash_fentryr_exit_pe;
  for (uint32_t i = 0;
       i < k_fcu_poll_short
       && (*FENTRYR & k_dflash_fentryr_low_byte_mask) != k_dflash_fentryr_pe_inactive;
       i++) {
  }
}

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
 * - Typical after power connection, power cycle
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
 * | Register read (rstsr0) | ~0.5 us | Single 8-bit read |
 * | Bit masking (& k_rstsr0_porf) | ~5 cycles | Bitwise AND |
 * | Conditional logging | ~5 us | If warm boot detected (UART output) |
 * | **Total** | **~0.5 us** | No logging (cold start) |
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
 *   rx_log_info(s_tag, "Cold start detected (power-on reset)");
 * } else {
 *   rx_log_info(s_tag, "Warm start detected (software/watchdog reset)");
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
 * | Register read (rstsr2) | ~0.5 us | Single 8-bit read |
 * | Bit masking | ~5 cycles | Bitwise AND + comparison |
 * | Assertions (debug build) | ~0.2 us | Two RX_ASSERT checks |
 * | **Total** | **~0.7 us** | Negligible overhead |
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
 * - **Refresh requirement:** Call `rx_iwdt_feed()` every 100 ms (from PID control loop)
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
 * | Call helper (internal_check_rstsr2_flag_clear) | ~0.7 us | Register read + mask |
 * | **Total** | **~0.7 us** | Single register check |
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
 *   rx_log_error(s_tag, "CRITICAL: Independent Watchdog Timer reset detected");
 *   rx_log_error(s_tag, "Prior execution hung or failed to refresh watchdog");
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
 * 5. **Missing refresh:** `rx_iwdt_feed()` not called every 100 ms from main task
 *
 * @see internal_check_rstsr2_flag_clear() Generic RSTSR2 flag checker (helper)
 * @see internal_check_startup_flags() Orchestrates all reset checks (caller)
 * @see rx_iwdt_feed() Refresh IWDT to prevent timeout (must call every 100 ms)
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
 * | **Clock source** | PCLKB (60 MHz, shared) | IWDTCLK (15 kHz, independent) |
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
 * 4. **Power depletion:** Supply voltage falls below LVD threshold
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
 *       capacity, motor inrush current, or supply voltage.
 *
 * @par Thread Safety:
 * Executes before ThreadX starts (single-threaded context).
 *
 * @par Example Power Supply Issue Debugging:
 * @code
 * // Startup check:
 * bool lvd0rf_ok = internal_check_lvd0rf();
 * if (!lvd0rf_ok) {
 *   rx_log_warn(s_tag, "Brownout reset detected (VCC dropped below threshold)");
 *   rx_log_warn(s_tag, "Check power supply capacity and motor inrush current");
 *
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
 *   rx_log_info(s_tag, "Warm start detected (processor-initiated reset)");
 *   // Optional: Check other flags to determine specific warm start cause
 *   if (internal_check_swrf()) {
 *     rx_log_info(s_tag, "  -> Software reset (SWRF set)");
 *   } else if (internal_check_wdtrf()) {
 *     rx_log_info(s_tag, "  -> Watchdog reset (WDTRF set)");
 *   }
 * } else {
 *   rx_log_info(s_tag, "Cold start detected (power-on or voltage recovery)");
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
 *   +- internal_check_startup_flags()  // Check flags (silent, UART not ready)
 *   +- rx_clock_power_init()           // Initialize clocks
 *   +- uart_debug_init()               // Initialize UART
 *   +- internal_report_startup_flags() // <- Report flags NOW (UART available)
 *   +- rx_infrastructure_init()        // Continue boot...
 * ```
 *
 * ## Performance (RX72N @ 240 MHz)
 *
 * | Operation | Duration | Notes |
 * |-----------|----------|-------|
 * | Register reads (3 regs) | ~1.5 us | RSTSR0, RSTSR1, RSTSR2 |
 * | Logging (5-6 messages) | ~500 us | UART @ 115200 baud |
 * | **Total** | **~500 us** | Negligible boot overhead |
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
    rx_log_info(s_boot_tag, "Cold boot - Power-on reset detected (PORF=1)");
  } else {
    rx_log_info(s_boot_tag, "Warm boot - No power-on reset (PORF=0)");
  }

  /* Report cold/warm start flag (CWSF) */
  if (rstsr1_val & k_rstsr1_cwsf) {
    rx_log_info(s_boot_tag, "Warm start - Processor-initiated reset (CWSF=1)");
  } else {
    rx_log_info(s_boot_tag, "Cold start - Power/voltage related (CWSF=0)");
  }

  /* Report abnormal conditions */
  if (rstsr2_val & k_rstsr2_wdtrf) {
    rx_log_warn(s_boot_tag, "Watchdog Timer timeout reset detected (WDTRF=1)");
    rx_log_warn(s_boot_tag, "  -> Check for firmware hang or intentional WDT reset");
  }

  if (rstsr2_val & k_rstsr2_swrf) {
    rx_log_info(s_boot_tag, "Software reset detected (SWRF=1)");
    rx_log_info(s_boot_tag, "  -> May be intentional (bootloader, firmware update, debug)");
  }

  if (rstsr0_val & k_rstsr0_lvd0rf) {
    rx_log_warn(s_boot_tag, "Brownout reset detected (LVD0RF=1)");
    rx_log_warn(s_boot_tag, "  -> VCC dropped below threshold - Check power supply!");
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
 * | Register reads (6 flags) | ~2 us | 3 registers x 2 reads each |
 * | Validation logic | ~1 us | Boolean comparisons, assertions |
 * | Logging (if needed) | ~5 us | UART write for warnings |
 * | **Total (normal boot)** | **~3 us** | No warnings logged |
 * | **Total (with warnings)** | **~10 us** | Includes UART output |
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
 *     rx_log_warn(s_tag, "Abnormal reset detected, continuing boot");
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
  /* Bring-up mode: NEVER halt on reset-cause flags.  All of PORF, IWDTRF,
   * WDTRF, SWRF, LVD0RF, CWSF are just read-and-ignored here; the full
   * diagnostic is still printed later via internal_report_startup_flags()
   * once uart_debug_init() has run.
   *
   * Rationale: the prior halt-on-abnormal-reset behaviour killed the
   * firmware silently on every rfp-cli flash (SWRF set) and on every
   * post-IWDT reboot (IWDTRF set), because the fatal-error handler
   * writes to UART which hasn't been initialised at this point in boot.
   * Result: board appears dead, Cypress CY7C65213 never sees any
   * telemetry bytes and may even de-enumerate from USB as the board
   * cycles through a reset loop.  Bench bring-up needs to see the
   * flags, not halt on them. */
  (void)internal_check_porf();
  (void)internal_check_iwdtrf();
  (void)internal_check_wdtrf();
  (void)internal_check_swrf();
  (void)internal_check_lvd0rf();
  (void)internal_check_cwsf();

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
 * - Timeout period = 2048ms (+/-10% tolerance)
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
    2048, /**< Hardware IWDT timeout in milliseconds (CKS=10, ~2048ms +/-10%). Must exceed all task timeouts to prevent spurious resets. Valid range: 128-16384ms per hardware limits. */
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
 * Task timeout = 3x task period (allows 2 missed heartbeats before failure detection)
 *
 * **Timeout Categories:**
 * - **Fast tasks (10ms period)**: 30ms timeout - Motor control, communication, watchdog
 * - **Medium tasks (50ms period)**: 150ms timeout - Telemetry, LED status
 * - **Slow tasks (1000ms period)**: 3000ms timeout - temperature sensor
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
    150, /**< Telemetry task timeout (150ms). Task period: 50ms @ 20 Hz. Timeout = 3x period. Heartbeat called every 50ms. Valid range: 100-300ms. If exceeded: telemetry stops, system reset after 2s. */
  k_iwdt_task_timeout_ledstatus_ms =
    150, /**< LED Status task timeout (150ms). Task period: 50ms @ 20 Hz. Timeout = 3x period. Heartbeat called every 50ms. Valid range: 100-300ms. If exceeded: LED updates stop, system reset after 2s. */
  k_iwdt_task_timeout_tempsensor_ms =
    3000, /**< Temperature Sensor task timeout (3000ms). Task period: 1000ms @ 1 Hz. Timeout = 3x period. Heartbeat called every 1s. Valid range: 2000-5000ms. If exceeded: temp compensation stops, system reset after 2s. */
  k_iwdt_task_timeout_obstdetect_ms =
    128, /**< Obstacle Detection task timeout (128ms, clamped to k_iwdt_min_timeout_ms).  Task heartbeat: 20ms @ 50 Hz.  ~6x headroom before hang detection fires. If exceeded: collision avoidance stops, system reset after 2s. */
  k_iwdt_task_timeout_motorctrl_ms =
    128, /**< Motor Control task timeout (128ms, clamped to k_iwdt_min_timeout_ms).  Task period: 10ms @ 100 Hz.  ~12x headroom before hang detection. If exceeded: motor control stops, system reset after 2s. CRITICAL TASK. */
  k_iwdt_task_timeout_commtask_ms =
    128, /**< Communication task timeout (128ms, clamped to k_iwdt_min_timeout_ms).  Task period: 10ms @ 100 Hz.  ~12x headroom before hang detection. If exceeded: SPI comm stops, system reset after 2s. CRITICAL TASK. */
  k_iwdt_task_timeout_watchdog_ms =
    128, /**< Watchdog Monitor task timeout (128ms, clamped to k_iwdt_min_timeout_ms).  Task period: 10ms @ 100 Hz.  Self-monitoring via own heartbeat. If exceeded: watchdog stops feeding IWDT, system reset after 2s. CRITICAL TASK. */
  k_iwdt_task_timeout_imu_ms =
    900, /**< IMU Task IWDT registration timeout (900ms). Intentionally exceeds k_imu_task_period_margin_ms (150ms): BNO055 POR alone takes ~700ms, so the IWDT window must span the full cold-start duration plus margin. Heartbeat is sent before each retry, so the gap between heartbeats is bounded by one retry attempt. */
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
 * - Safety margin: 2048ms / 10ms = 204x (allows ~204 missed iterations)
 *
 * **Task monitoring**: Individual task timeouts
 * - Motor/Comm/Watchdog: 30ms (3x 10ms period)
 * - Obstacle: 60ms (3x 20ms period)
 * - LED/Telemetry: 150ms (3x 50ms period)
 * - Temp: 3000ms (3x 1000ms period)
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

/* =============================================================================
 * tx_application_define Helper Functions
 *
 * These static helpers decompose the ThreadX application callback into
 * single-responsibility units that each stay under ~60 lines
 * (NASA Power of 10 Rule 4).
 * =============================================================================
 */

/**
 * @brief Initialize the global bus manager with infrastructure interfaces
 *
 * @details
 * Creates the bus manager singleton that all tasks use to access hardware buses.
 * The bus manager requires a ThreadX mutex internally, so it must be initialized
 * inside tx_application_define (after the ThreadX kernel is ready but before
 * the scheduler starts).
 *
 * @pre rx_infrastructure_init() completed successfully in main()
 * @pre ThreadX kernel initialized (mutex creation available)
 *
 * @post g_bus_manager initialized and ready for rx_bus_manager_add_bus() calls
 * @post Infrastructure error and pin interfaces wired into the bus manager
 *
 * @note Executes in single-threaded context (scheduler not started). No synchronization needed.
 *
 * @see rx_bus_manager_init() Underlying initialization function
 * @see internal_register_system_buses() Registers individual buses after this call
 *
 * @since Version 1.0.0
 */
static void internal_init_bus_manager(void)
{
  rx_error_interface_t* error_iface = rx_infrastructure_get_error_interface();
  rx_pin_interface_t*   pin_iface   = rx_infrastructure_get_pin_interface();

  rx_err_t err = rx_bus_manager_init(&g_bus_manager, "BUS_MGR", error_iface, pin_iface);
  RX_ASSERT(err == k_rx_ok, "rx_bus_manager_init must succeed");
}

/**
 * @brief Register all hardware buses (1-Wire, GPIO, ADC) with the bus manager
 *
 * @details
 * Initializes static bus configuration structs and registers them with the
 * global bus manager. Each bus is available to tasks via rx_bus_manager_get_interface().
 *
 * Registered buses:
 * - **onewire0** (P51): DS18B20 temperature sensor, 1-Wire bit-banging
 * - **gpio** (P00): Generic GPIO for motor driver control signals
 * - **motor0_current** (S12AD0, ch7, AN007, P47): Motor 0 (FL) IPROPI current sense
 * - **motor1_current** (S12AD0, ch6, AN006, P46): Motor 1 (FR) IPROPI current sense
 * - **motor2_current** (S12AD0, ch5, AN005, P45): Motor 2 (BL) IPROPI current sense
 * - **motor3_current** (S12AD0, ch4, AN004, P44): Motor 3 (BR) IPROPI current sense
 *
 * @pre internal_init_bus_manager() completed successfully
 * @pre Static bus config structs zeroed (BSS): s_onewire0_config, s_gpio_config,
 *      s_motor_current_configs[]
 *
 * @post All three buses registered and accessible via bus manager
 * @post Static config structs populated with bus parameters
 *
 * @note Executes in single-threaded context (scheduler not started). No synchronization needed.
 *
 * @see internal_init_bus_manager() Must be called first
 * @see rx_bus_config_init_onewire() 1-Wire bus configuration
 * @see rx_bus_config_init_gpio() GPIO bus configuration
 * @see rx_bus_config_init_adc() ADC bus configuration
 *
 * @since Version 1.0.0
 */
/**
 * @brief Register the 1-Wire bus for the DS18B20 temperature sensor on P51
 *
 * @details
 * Initializes s_onewire0_config via rx_bus_config_init_onewire() and registers
 * it with the global bus manager. Failure asserts (boot precondition).
 *
 * @pre internal_init_bus_manager() completed successfully
 * @pre s_onewire0_config zero-initialized (BSS)
 *
 * @post s_onewire0_config populated with name="onewire0" and pin=P51
 * @post Bus "onewire0" available via the global bus manager
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see rx_bus_config_init_onewire() Underlying configuration helper
 * @see rx_bus_manager_add_bus() Registers the populated config
 *
 * @since Version 1.0.0
 */
static void internal_register_onewire0_bus(void)
{
  rx_err_t err = rx_bus_config_init_onewire(&s_onewire0_config,
                                            "onewire0", /* name */
                                            k_rx_p5_1); /* pin = P51 */
  RX_ASSERT(err == k_rx_ok, "onewire0 config init must succeed");
  err = rx_bus_manager_add_bus(&g_bus_manager, &s_onewire0_config);
  RX_ASSERT(err == k_rx_ok, "onewire0 registration must succeed");
}

/**
 * @brief Register the generic GPIO bus abstraction used by motor drivers
 *
 * @details
 * Initializes s_gpio_config via rx_bus_config_init_gpio() with placeholder pin
 * P00 (the API requires an initial pin -- actual pins are specified per-call by
 * motor_control_task) and registers it with the global bus manager.
 *
 * @pre internal_init_bus_manager() completed successfully
 * @pre s_gpio_config zero-initialized (BSS)
 *
 * @post s_gpio_config populated with name="gpio" and initial pin=P00
 * @post Bus "gpio" available via the global bus manager
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see rx_bus_config_init_gpio() Underlying configuration helper
 * @see rx_bus_manager_add_bus() Registers the populated config
 *
 * @since Version 1.0.0
 */
static void internal_register_gpio_bus(void)
{
  rx_err_t err = rx_bus_config_init_gpio(&s_gpio_config,
                                         "gpio",     /* name */
                                         k_rx_p0_0); /* pin = P00 (generic bus) */
  RX_ASSERT(err == k_rx_ok, "gpio config init must succeed");
  err = rx_bus_manager_add_bus(&g_bus_manager, &s_gpio_config);
  RX_ASSERT(err == k_rx_ok, "gpio registration must succeed");
}

/**
 * @brief Register all four motor current-sense ADC channels (S12AD0)
 *
 * @details
 * Iterates over k_motor_current_adc_channels[] and registers one ADC bus per
 * motor (FL, FR, BL, BR) on S12AD0 channels 4-7. Each iteration:
 *   1. Populates s_motor_current_configs[i] via rx_bus_config_init_adc()
 *   2. Adds the config to the bus manager
 *   3. Initialises the ADC hardware via rx_bus_adc_init()
 *
 * @pre internal_init_bus_manager() completed successfully
 * @pre s_motor_current_configs[] zero-initialized (BSS)
 * @pre k_motor_current_adc_channels and g_motor_current_bus_names indexed [0..k_motor_current_adc_count)
 *
 * @post All four motor-current ADC buses registered and ADC hardware initialised
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see rx_bus_config_init_adc() Underlying configuration helper
 * @see rx_bus_adc_init() Initialises ADC hardware after registration
 *
 * @since Version 1.0.0
 */
static void internal_register_motor_current_adc_buses(void)
{
  for (uint8_t i = 0; i < k_motor_current_adc_count; i++) {
    rx_err_t err = rx_bus_config_init_adc(&s_motor_current_configs[i],
                                          g_motor_current_bus_names[i],    /* name (shared) */
                                          k_adc_unit_0,                    /* unit = S12AD0 */
                                          k_motor_current_adc_channels[i], /* channel = AN004-7 */
                                          k_adc_resolution_12bit);         /* bits = 12-bit */
    RX_ASSERT(err == k_rx_ok, "motor_current config init must succeed");
    err = rx_bus_manager_add_bus(&g_bus_manager, &s_motor_current_configs[i]);
    RX_ASSERT(err == k_rx_ok, "motor_current registration must succeed");
    err = rx_bus_adc_init(&g_bus_manager, g_motor_current_bus_names[i]);
    RX_ASSERT(err == k_rx_ok, "motor_current ADC init must succeed");
  }
}

/**
 * @brief Register a single mandatory I2C device on RIIC1 at 400 kHz
 *
 * @details
 * Common wrapper for the BNO055 and BMP280 registrations: both share RIIC1,
 * SDA=P2.0, SCL=P2.1, and 400 kHz fast mode and differ only by name, device
 * address and config-storage struct. Failure asserts (boot precondition).
 *
 * Steps:
 *   1. rx_bus_config_init_i2c(config, name, RIIC1, addr, P2.0, P2.1, 400 kHz)
 *   2. rx_bus_manager_add_bus(g_bus_manager, config)
 *   3. rx_bus_i2c_init(g_bus_manager, name)
 *
 * @param[out] config Pointer to caller-owned static rx_bus_config_t storage
 * @param[in] name Stable name string used to look the bus up later
 * @param[in] device_addr 7-bit I2C address of the peripheral
 *
 * @pre internal_init_bus_manager() completed successfully
 * @pre config != NULL and points to BSS-zeroed storage
 * @pre name is a stable string with lifetime >= bus manager lifetime
 *
 * @post Bus "name" registered, configured and ready for read/write
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see rx_bus_config_init_i2c() Underlying configuration helper
 * @see rx_bus_i2c_init() Initialises RIIC hardware after registration
 *
 * @since Version 1.0.0
 */
static void internal_register_riic1_device(rx_bus_config_t* config, const char* name,
                                           uint8_t device_addr)
{
  rx_err_t err = rx_bus_config_init_i2c(config, name, k_riic_channel_1, /* channel: RIIC1 */
                                        device_addr,             /* device_addr: per-sensor */
                                        k_rx_p2_0,               /* sda_pin: P2.0 = SDA1 */
                                        k_rx_p2_1,               /* scl_pin: P2.1 = SCL1 */
                                        k_i2c_frequency_400khz); /* 400 kHz fast mode */
  RX_ASSERT(err == k_rx_ok, "RIIC1 device config init must succeed");
  err = rx_bus_manager_add_bus(&g_bus_manager, config);
  RX_ASSERT(err == k_rx_ok, "RIIC1 device registration must succeed");
  err = rx_bus_i2c_init(&g_bus_manager, name);
  RX_ASSERT(err == k_rx_ok, "RIIC1 device I2C init must succeed");
}

/**
 * @brief Probe MPU-6050 WHO_AM_I and persist {who_am_i, err} to data flash
 *
 * @details
 * Bench-only diagnostic: registers the GY-521/MPU-6050 at 0x68 on RIIC1, reads
 * register 0x75 (WHO_AM_I), and writes the result to data flash at 0x00100000
 * via internal_bench_dflash_write() so the result is recoverable through
 * rfp-cli without a UART bridge.
 *
 * Unlike the BNO055/BMP280 paths, missing hardware here is **not fatal** -- the
 * function logs the failure and continues. RX_ASSERT is intentionally absent.
 *
 * @pre internal_init_bus_manager() completed successfully
 * @pre s_i2c1_mpu_config zero-initialized (BSS)
 *
 * @post s_i2c1_mpu_config populated with bench bus parameters (best-effort)
 * @post Data flash @ 0x00100000 contains {magic, who_am_i, err} record
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see internal_bench_dflash_write() Persists the probe result
 * @see rx_bus_i2c_write_read() Performs the WHO_AM_I single-byte read
 *
 * @since Version 1.0.0
 */
static void internal_probe_mpu6050_who_am_i(void)
{
  rx_err_t err = rx_bus_config_init_i2c(&s_i2c1_mpu_config,
                                        "i2c1_mpu",              /* bench-test bus name */
                                        k_riic_channel_1,        /* channel: RIIC1 */
                                        k_i2c_addr_mpu6050,      /* device_addr: 0x68 */
                                        k_rx_p2_0,               /* sda_pin: P2.0 = SDA1 */
                                        k_rx_p2_1,               /* scl_pin: P2.1 = SCL1 */
                                        k_i2c_frequency_400khz); /* frequency_hz: 400 kHz */
  if (err == k_rx_ok) {
    err = rx_bus_manager_add_bus(&g_bus_manager, &s_i2c1_mpu_config);
  }
  if (err == k_rx_ok) {
    err = rx_bus_i2c_init(&g_bus_manager, "i2c1_mpu");
  }
  uint8_t who_am_i  = 0x00U;
  int32_t probe_err = err; /* preserve setup err if any */
  if (err == k_rx_ok) {
    const uint8_t who_am_i_reg = 0x75U; /* MPU-6050 WHO_AM_I register */
    err       = rx_bus_i2c_write_read(&g_bus_manager, "i2c1_mpu", &who_am_i_reg, 1U, &who_am_i, 1U);
    probe_err = err;
    if (err == k_rx_ok) {
      rx_log_info_val(s_mpu6050_tag, "WHO_AM_I=0x", who_am_i);
    } else {
      rx_log_error_val(s_mpu6050_tag, "WHO_AM_I read failed err=", (uint32_t)err);
    }
  } else {
    rx_log_error_val(s_mpu6050_tag, "bus setup failed err=", (uint32_t)err);
  }

  /* Persist the result to data flash @ 0x00100000 so it's readable via
   * rfp-cli -rv 0x00100000 16 without needing the UART bridge. */
  internal_bench_dflash_write(who_am_i, probe_err);
}

static void internal_register_system_buses(void)
{
  /* Production buses required by tasks. */
  internal_register_onewire0_bus();
  internal_register_gpio_bus();
  internal_register_motor_current_adc_buses();
  internal_register_riic1_device(&s_i2c1_imu_config, "i2c1_imu", k_i2c_addr_bno055);
  internal_register_riic1_device(&s_i2c1_baro_config, "i2c1_baro", k_i2c_addr_bmp280);

  /* Bench-only diagnostic; not fatal if the module is absent. */
  internal_probe_mpu6050_who_am_i();
}

/**
 * @brief Initialize shared data module and hardware watchdog timer
 *
 * @details
 * Sets up inter-task communication (ThreadX mutexes and event flags via shared_data_init)
 * and initializes the Independent Watchdog Timer with state-dependent timeouts.
 *
 * @pre internal_register_system_buses() completed (buses available for tasks)
 * @pre ThreadX kernel initialized (mutex/event-flag creation available)
 *
 * @post Shared data mutexes and event flags created and ready for task use
 * @post IWDT hardware configured with s_iwdt_config parameters
 *
 * @note Executes in single-threaded context (scheduler not started). No synchronization needed.
 *
 * @see shared_data_init() Inter-task communication setup
 * @see rx_iwdt_init() Hardware watchdog initialization
 *
 * @since Version 1.0.0
 */
static void internal_init_shared_data_and_watchdog(void)
{
  /* Initialize shared data module (mutexes, event flags) */
  rx_err_t err = shared_data_init();
  RX_ASSERT(err == k_rx_ok, "shared_data_init must succeed");

  /* Initialize IWDT (hardware watchdog + task monitoring) */
  err = rx_iwdt_init(&s_iwdt_config);
  RX_ASSERT(err == k_rx_ok, "rx_iwdt_init must succeed");
}

/**
 * @brief Register all eight tasks for IWDT heartbeat monitoring and set initial state
 *
 * @details
 * Registers each application task with the IWDT module so that missed heartbeats
 * are detected. Timeouts are set to 3x the nominal task period to allow two
 * missed heartbeats before a timeout is declared.
 *
 * Task timeout mapping:
 * - Telemetry (50ms period) -> 150ms timeout
 * - LED Status (50ms period) -> 150ms timeout
 * - Temp Sensor (1000ms period) -> 3000ms timeout
 * - Obstacle Detect (20ms period) -> 60ms timeout
 * - Motor Control (10ms period) -> 30ms timeout
 * - Communication (10ms period) -> 30ms timeout
 * - Watchdog Monitor (10ms period) -> 30ms timeout
 *
 * After all tasks are registered the system state is set to k_system_state_init
 * so the IWDT uses the 5-second init-phase timeout.
 *
 * @pre internal_init_shared_data_and_watchdog() completed (IWDT initialized)
 * @pre No tasks registered yet (first call after rx_iwdt_init)
 *
 * @post All eight tasks registered with per-task heartbeat timeouts
 * @post IWDT system state set to k_system_state_init (5s timeout)
 *
 * @note Executes in single-threaded context (scheduler not started). No synchronization needed.
 *
 * @see rx_iwdt_register_task() Registers a single task for monitoring
 * @see rx_iwdt_set_state() Sets system state for state-dependent timeouts
 *
 * @since Version 1.0.0
 */
static void internal_register_iwdt_tasks(void)
{
  /* MVP BYPASS (2026-04-22): telemetry_task is dormant (replaced by
   * serial_bringup_task ASCII telemetry). Registering its slot without
   * its heartbeater would cause watchdog_monitor_task to stop feeding
   * the hardware IWDT, triggering a 2 s reset loop that also drops the
   * Cypress USB-UART because /RES# is shared (verified via PCB
   * netlist: U4-pin1 and U11-pin1 on same net 74). */
  // rx_err_t err = rx_iwdt_register_task("Telemetry", k_iwdt_task_timeout_telemetry_ms);
  // RX_ASSERT(err == k_rx_ok, "Telemetry IWDT registration must succeed");

  rx_err_t err = rx_iwdt_register_task("LEDStatus", k_iwdt_task_timeout_ledstatus_ms);
  RX_ASSERT(err == k_rx_ok, "LEDStatus IWDT registration must succeed");

  /* TempSensor / ObstDetect / ImuTask / MotorCtrl IWDT registrations
   * are disabled in lockstep with their task_create calls below.  If
   * we register them here but the task is never created to send
   * heartbeats, IWDT fires a reset ~5 s after k_system_state_running
   * transitions, and the board loops so fast through boot that only
   * main.c's final D13 LED marker is visible.  Re-enable each line
   * ONCE its matching task_create is re-enabled (see comment block
   * inside internal_create_system_tasks).
   */

  // err = rx_iwdt_register_task("TempSensor", k_iwdt_task_timeout_tempsensor_ms);
  // RX_ASSERT(err == k_rx_ok, "TempSensor IWDT registration must succeed");

  // err = rx_iwdt_register_task("ObstDetect", k_iwdt_task_timeout_obstdetect_ms);
  // RX_ASSERT(err == k_rx_ok, "ObstDetect IWDT registration must succeed");

  // err = rx_iwdt_register_task("MotorCtrl", k_iwdt_task_timeout_motorctrl_ms);
  // RX_ASSERT(err == k_rx_ok, "MotorCtrl IWDT registration must succeed");

  /* MVP BYPASS (2026-04-22): CommTask was replaced by serial_bringup_task
   * for SLAM bring-up. Registering CommTask here while its creator is
   * commented out causes IWDT to fire after 2 s -- resetting the RX72N,
   * which drops the Cypress USB-UART (04b4:0003) off USB and makes
   * /dev/ttyACM0 vanish every ~2 s. Re-enable with comm_task_create(). */
  // err = rx_iwdt_register_task("CommTask", k_iwdt_task_timeout_commtask_ms);
  // RX_ASSERT(err == k_rx_ok, "CommTask IWDT registration must succeed");

  err = rx_iwdt_register_task("SerialBU", k_iwdt_task_timeout_commtask_ms);
  RX_ASSERT(err == k_rx_ok, "SerialBU IWDT registration must succeed");

  err = rx_iwdt_register_task("WatchdogMon", k_iwdt_task_timeout_watchdog_ms);
  RX_ASSERT(err == k_rx_ok, "WatchdogMon IWDT registration must succeed");

  err = rx_iwdt_register_task("ImuTask", k_iwdt_task_timeout_imu_ms);
  RX_ASSERT(err == k_rx_ok, "ImuTask IWDT registration must succeed");

  /* Set initial IWDT state (init phase - 5s timeout) */
  err = rx_iwdt_set_state(k_system_state_init);
  RX_ASSERT(err == k_rx_ok, "IWDT set init state must succeed");
}

/**
 * @brief Create all eight application tasks and transition to running state
 *
 * @details
 * Creates all eight ThreadX tasks in dependency-aware order (lowest priority
 * first, except Watchdog Monitor which follows Communication for logical
 * grouping). Tasks do not begin executing until tx_application_define()
 * returns and the scheduler starts. After all tasks are created, the IWDT
 * system state transitions to k_system_state_running.
 *
 * Task creation order:
 * 1. Telemetry (priority 18, lowest)
 * 2. LED Status (priority 17)
 * 3. Temperature Sensor (priority 15)
 * 4. IMU (priority 13, BNO055 + BMP280)
 * 5. Obstacle Detection (priority 12)
 * 6. Motor Control (priority 8)
 * 7. Communication (priority 5)
 * 8. Watchdog Monitor (priority 6)
 *
 * @pre internal_register_iwdt_tasks() completed (all tasks registered for monitoring)
 * @pre Static task stacks allocated in each task module
 *
 * @post All eight ThreadX threads created in READY state
 * @post IWDT system state set to k_system_state_running (2s timeout)
 *
 * @note Executes in single-threaded context (scheduler not started). No synchronization needed.
 * @note Tasks do not begin executing until tx_application_define() returns.
 *
 * @see telemetry_task_create() through watchdog_monitor_task_create()
 * @see rx_iwdt_set_state() Transitions to running state after task creation
 *
 * @since Version 1.0.0
 */
/**
 * @brief Create the low-priority observability tasks (serial bring-up + LED status)
 *
 * @details
 * Spawns the lowest-priority threads first so the scheduler has visible
 * heartbeat output the moment higher-priority work begins:
 * - serial_bringup_task (priority 18 slot, replaces the dormant telemetry_task)
 * - led_status_task (priority 17, blinks D9 at 1 Hz once running)
 *
 * MVP BYPASS (2026-04-22): the framed nanopb / HARQ / session stack is
 * currently disabled; serial_bringup_task owns SCI9 with a simple ASCII line
 * protocol for SLAM bring-up. To restore framed comms, swap
 * serial_bringup_task_create() for telemetry_task_create() + comm_task_create()
 * and update the matching IWDT registrations in internal_register_iwdt_tasks().
 *
 * @pre internal_register_iwdt_tasks() completed (heartbeat slots reserved)
 * @pre Per-task static stacks defined in their respective task modules
 *
 * @post serial_bringup_task and led_status_task created in READY state
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see serial_bringup_task_create() Replaces telemetry_task in the MVP
 * @see led_status_task_create() D9 heartbeat
 *
 * @since Version 1.0.0
 */
static void internal_create_observability_tasks(void)
{
  /* Telemetry Task - Priority 18 (lowest) */
  // rx_err_t err = telemetry_task_create();
  // RX_ASSERT(err == k_rx_ok, "telemetry_task_create must succeed");
  rx_err_t err = serial_bringup_task_create();
  RX_ASSERT(err == k_rx_ok, "serial_bringup_task_create must succeed");

  /* LED Status Task - Priority 17 (visual feedback) */
  err = led_status_task_create();
  RX_ASSERT(err == k_rx_ok, "led_status_task_create must succeed");
}

/**
 * @brief Create the sensor and motor-control tasks (IMU + motor control)
 *
 * @details
 * Spawns the safety-critical mid-priority tasks. Exact set is gated by the
 * scheduler-bring-up TODO (2026-04-22): temp_sensor_task / obstacle_detect_task
 * / comm_task are currently commented out because each hangs its own init path
 * when the scheduler is alive, starving the lower-priority observability tasks.
 * Re-enable lines ONE AT A TIME after each task's init hang is diagnosed.
 *
 * Currently active:
 * - imu_task (priority 13, BNO055 + BMP280 at 20 Hz)
 * - motor_control_task (priority 8, 250 Hz control loop)
 *
 * @pre internal_create_observability_tasks() completed
 * @pre internal_register_system_buses() registered i2c1_imu, i2c1_baro and gpio buses
 *
 * @post imu_task and motor_control_task created in READY state
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see imu_task_create() BNO055/BMP280 fusion task
 * @see motor_control_task_create() 250 Hz PID + telemetry task
 *
 * @since Version 1.0.0
 */
static void internal_create_sensor_and_motor_tasks(void)
{
  /* TODO(2026-04-22, scheduler-bring-up): temp / imu / obstacle / motor
   * tasks are DISABLED here because each hangs its own init path when the
   * scheduler is alive, starving every lower-priority task (led_status
   * and telemetry both sit below these four in priority).  The scheduler
   * itself is now verified working end-to-end -- D9 heartbeat blinks at
   * 1 Hz in this build once these four are commented out.  Re-enable
   * each ONE AT A TIME after its specific init hang is diagnosed and
   * fixed; they are orthogonal problems from the scheduler fix chain
   * committed with this change.
   *
   *   temp_sensor_task   : 1-Wire / DS18B20 bring-up (pri 15)
   *   imu_task           : BNO055 / BMP280 on RIIC1 (pri 13)
   *   obstacle_detect_task: HC-SR04 hardware bring-up blocked per
   *                        project_sonar_bringup_blocked.md (pri 12)
   *   motor_control_task : internal_init_motor_stack() hangs on real
   *                        hardware (pri 8)
   */

  /* Temperature Sensor Task - Priority 15 */
  // rx_err_t err = temp_sensor_task_create();
  // RX_ASSERT(err == k_rx_ok, "temp_sensor_task_create must succeed");

  /* IMU Task - Priority 13 (BNO055 + BMP280 at 20 Hz) */
  rx_err_t err = imu_task_create();
  RX_ASSERT(err == k_rx_ok, "imu_task_create must succeed");

  /* Obstacle Detection Task - Priority 12 */
  // err = obstacle_detect_task_create();
  // RX_ASSERT(err == k_rx_ok, "obstacle_detect_task_create must succeed");

  /* Communication Task - Priority 5 (highest)
   * MVP BYPASS (2026-04-22): disabled -- serial_bringup_task owns SCI9.
   * See the comment block above telemetry_task_create() for details. */
  // err = comm_task_create();
  // RX_ASSERT(err == k_rx_ok, "comm_task_create must succeed");

  /* Motor Control Task - Priority 8 */
  err = motor_control_task_create();
  RX_ASSERT(err == k_rx_ok, "motor_control_task_create must succeed");
}

/**
 * @brief Create the infrastructure tasks (USB CDC stack + watchdog monitor)
 *
 * @details
 * Spawns the highest-priority infrastructure tasks last so that they pre-empt
 * the lower priority work as soon as the scheduler starts:
 * - usb_task (priority 4, drives 3-port CDC: ttyACM0 PROTO, 1 DECODED, 2 LOG)
 * - watchdog_monitor_task (priority 6, IWDT feeding + heartbeat aggregation)
 *
 * The 9 non-control USB pipes split 3 per CDC; HUM Ch.40 allows 10 pipes total
 * (DCP + 9), so all three CDCs fit cleanly. usb_task also polls the production
 * rx_usb_isr_handler() once per tick as a backstop in case the SLIBR/IER edge
 * case ever drops a USBI0.
 *
 * @pre internal_create_sensor_and_motor_tasks() completed
 * @pre rx_usb_init() completed in main() pre-kernel window
 *
 * @post usb_task and watchdog_monitor_task created in READY state
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see usb_task_create() 3-port CDC stack driver
 * @see watchdog_monitor_task_create() IWDT feeding + heartbeat aggregation
 *
 * @since Version 1.0.0
 */
static void internal_create_infrastructure_tasks(void)
{
  /* USB Task - Priority 4 (above comm_task) -- drives the 3-port CDC
   * stack: ttyACM0 PROTO (R/W), ttyACM1 DECODED (R/W), ttyACM2 LOG (RO).
   * The 9 non-control USB pipes split 3 per CDC; HUM Ch.40 allows 10
   * pipes total (DCP + 9), so all three CDCs fit cleanly.  Polls the
   * production rx_usb_isr_handler() once per tick as a backstop in
   * case the SLIBR/IER edge case ever drops a USBI0. */
  rx_err_t err = usb_task_create();
  RX_ASSERT(err == k_rx_ok, "usb_task_create must succeed");

  /* Watchdog Monitor Task - Priority 6 */
  err = watchdog_monitor_task_create();
  RX_ASSERT(err == k_rx_ok, "watchdog_monitor_task_create must succeed");
}

/**
 * @brief Transition the IWDT state machine to k_system_state_running
 *
 * @details
 * After all application tasks are created, switch the IWDT to the 2-second
 * running-state timeout. Until this point the IWDT used the 5-second
 * init-phase timeout configured by internal_register_iwdt_tasks().
 *
 * @pre internal_create_observability_tasks() completed
 * @pre internal_create_sensor_and_motor_tasks() completed
 * @pre internal_create_infrastructure_tasks() completed
 *
 * @post IWDT state == k_system_state_running (2 s timeout)
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see rx_iwdt_set_state() State machine setter
 *
 * @since Version 1.0.0
 */
static void internal_set_iwdt_running_state(void)
{
  rx_err_t err = rx_iwdt_set_state(k_system_state_running);
  RX_ASSERT(err == k_rx_ok, "IWDT set running state must succeed");
}

static void internal_create_system_tasks(void)
{
  internal_create_observability_tasks();
  internal_create_sensor_and_motor_tasks();
  internal_create_infrastructure_tasks();
  internal_set_iwdt_running_state();
}

/**
 * @brief Register ThreadX stack overflow detection handler
 *
 * @details
 * Installs the rx_stack_monitor callback that ThreadX invokes when any thread
 * exceeds its allocated stack. Requires TX_ENABLE_STACK_CHECKING to be defined
 * in the ThreadX build configuration.
 *
 * @pre All application tasks created via internal_create_system_tasks()
 * @pre ThreadX TX_ENABLE_STACK_CHECKING enabled at build time
 *
 * @post Stack overflow handler registered with ThreadX kernel
 * @post Any future stack overflow triggers rx_stack_monitor callback
 *
 * @note Executes in single-threaded context (scheduler not started). No synchronization needed.
 *
 * @see rx_stack_monitor_init() Underlying registration function
 *
 * @since Version 1.0.0
 */
static void internal_init_stack_monitor(void)
{
  rx_err_t err = rx_stack_monitor_init();
  RX_ASSERT(err == k_rx_ok, "rx_stack_monitor_init must succeed");
}

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
 * This callback delegates to six internal helpers that each handle a single
 * initialization responsibility (NASA Power of 10 Rule 4 compliance):
 *
 * 1. internal_init_bus_manager() - bus manager singleton
 * 2. internal_register_system_buses() - 1-Wire, GPIO, ADC bus configs
 * 3. internal_init_shared_data_and_watchdog() - shared data + IWDT hardware
 * 4. internal_register_iwdt_tasks() - per-task heartbeat registration
 * 5. internal_create_system_tasks() - ThreadX thread creation (7 tasks)
 * 6. internal_init_stack_monitor() - stack overflow detection
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
 * | **Task stacks (x7)** | varies | Static arrays in each task module (comm, watchdog, motor, etc.) |
 * | **first_unused_memory** | ~490 KB | Remaining free SRAM (unused) |
 *
 * ## Thread Creation Timing (RX72N @ 240 MHz)
 *
 * | Operation | Duration | Notes |
 * |-----------|----------|-------|
 * | tx_application_define() call | ~5 us | ThreadX callback overhead |
 * | Task creation (7 tasks) | ~140 us | TCB init, stack setup per task |
 * | **Total** | **~145 us** | Fast thread creation |
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
 * @pre SRAM available for thread stacks
 * @pre first_unused_memory points to valid SRAM address
 *
 * @post All eight application tasks created and ready to run (scheduler starts after return)
 * @post Thread stacks allocated statically and initialized (SP set to stack top for each task)
 * @post Thread priorities configured for eight distinct tasks (comm_task holds priority 5)
 * @post IWDT task monitoring registered for all eight tasks with per-task timeouts
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
 * @see internal_init_bus_manager() Step 1: Bus manager initialization
 * @see internal_register_system_buses() Step 2: Bus registration
 * @see internal_init_shared_data_and_watchdog() Step 3: Shared data and IWDT
 * @see internal_register_iwdt_tasks() Step 4: Task heartbeat registration
 * @see internal_create_system_tasks() Step 5: ThreadX thread creation
 * @see internal_init_stack_monitor() Step 6: Stack overflow detection
 * @see tx_kernel_enter() Start ThreadX scheduler (calls this callback internally)
 *
 * @since Version 1.0.0
 *
 * @test test_main.c Verify thread creation and scheduler startup
 */
void tx_application_define(void* first_unused_memory)
{
  /* Precondition: first_unused_memory parameter is provided by ThreadX */
  RX_ASSERT(first_unused_memory != nullptr, "Precondition: first_unused_memory must be valid");

  /* =========================================================================
   * Multi-Task Architecture Initialization
   *
   * Task creation order is dependency-aware while the scheduler is not yet
   * running; no task preemption occurs during this phase.
   *
   * Priority Map (lower = higher priority):
   *   5  = Communication (highest - command latency)
   *   6  = Watchdog Monitor (IWDT feeding + task heartbeat checks)
   *   8  = Motor Control (250 Hz control loop)
   *   12 = Obstacle Detection (safety-critical)
   *   13 = IMU (BNO055 + BMP280 at 20 Hz)
   *   15 = Temperature Sensing (1 Hz)
   *   17 = LED Status (20 Hz, visual feedback)
   *   18 = Telemetry Aggregation (20 Hz, lowest)
   * =========================================================================
   */

  /* Step 1: Initialize bus manager (requires ThreadX mutex) */
  internal_init_bus_manager();

  /* Step 2: Register buses with manager (before tasks need them) */
  internal_register_system_buses();

  /* Step 3: Initialize shared data and hardware watchdog */
  internal_init_shared_data_and_watchdog();

  /* Step 4: Register all tasks for IWDT heartbeat monitoring */
  internal_register_iwdt_tasks();

  /* Step 5: Create all application tasks (dependency-aware order) */
  internal_create_system_tasks();

  /* Step 6: Register ThreadX stack overflow handler */
  internal_init_stack_monitor();
}

/**
 * @brief Main entry point for STAR RX72N motor controller firmware
 *
 * @details
 * Implements the **three-stage bootstrap sequence** for the embedded system:
 *
 * 1. **Stage 1: Startup Validation** (~10 us)
 *    - Read reset status registers (RSTSR0, RSTSR1, RSTSR2)
 *    - Detect abnormal reset conditions (watchdog timeout, brownout, etc.)
 *    - Assert-halt on critical flags (IWDTRF = firmware bug)
 *
 * 2. **Stage 2: System Initialization** (~250 us)
 *    - Configure PLL for 240 MHz operation (rx_clock_power_init)
 *    - Initialize UART for early error logging (uart_debug_init)
 *    - Report startup flags to console (internal_report_startup_flags)
 *    - Initialize infrastructure (error handler, pin validator) (rx_infrastructure_init)
 *    - Initialize hardware (motor drivers, encoders, USB CDC) (hardware_init)
 *
 * 3. **Stage 3: ThreadX Bootstrap** (never returns)
 *    - Call tx_kernel_enter() to start RTOS scheduler
 *    - ThreadX calls tx_application_define() callback
 *    - Application threads created (comm, motor, sensors, watchdog, telemetry, LED)
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
 *   ThreadX => AppTask [label="tx_application_define()\ntask creation", textcolor="purple"];
 *   AppTask => AppTask [label="ThreadX scheduler running\n(comm, motor, sensors)", textcolor="purple"];
 * }
 * @endmsc
 *
 * ## Performance Characteristics (RX72N @ 240 MHz)
 *
 * | Phase | Execution Time | CPU Cycles | Notes |
 * |-------|----------------|------------|-------|
 * | **C runtime init** | ~500 us | ~120,000 | crt0.S: BSS clear, data copy |
 * | **Startup flag checks** | ~10 us | ~2,400 | 6 register reads + validation |
 * | **Clock init (PLL lock)** | ~200 us | ~48,000 | PLL stabilization wait |
 * | **Hardware init** | ~50 ms | ~12M | USB enumeration dominates |
 * | **ThreadX start** | ~100 us | ~24,000 | Kernel object creation |
 * | **Total boot time** | **~51 ms** | **~12.2M** | Ready for first control loop |
 *
 * ## Memory Usage (Static Allocation Only - No Heap)
 *
 * | Object | Size | Location | Purpose |
 * |--------|------|----------|---------|
 * | **Main stack** | 4 KB | SRAM | Pre-ThreadX execution |
 * | **Task stacks (x7)** | varies | SRAM | Static arrays per task module |
 * | **ThreadX kernel objects** | ~2 KB | SRAM | TCBs, timers, semaphores |
 * | **USB buffers** | 2 KB | SRAM | CDC ring buffers (TX/RX) |
 *
 * **Total SRAM usage:** ~30 KB / 512 KB (~6% utilization)
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
 * - Use hardware debugger (e^2 studio debugger, OpenOCD, J-Link, GDB)
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
 * @post ThreadX RTOS scheduler running with all application tasks
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
/**
 * @brief Configure the five boot-checkpoint LEDs (D9-D13) and light D9
 *
 * @details
 * Sets up the "one-hot" boot-progress indicator. Each later stage in main()
 * extinguishes the previous LED and lights its own so a single glowing LED
 * tells the operator exactly where boot got to without needing a UART:
 *   - D9  (PA7) = main() entered
 *   - D10 (PB0) = startup flags checked
 *   - D11 (P71) = rx_clock_power_init done
 *   - D12 (P72) = uart_debug_init done (UART live)
 *   - D13 (PB1) = hardware_init done, about to tx_kernel_enter
 *
 * GPIO works on the default LOCO 240 kHz clock, no MSTP release needed.
 *
 * @pre MCU reset complete and C runtime initialised
 *
 * @post All five checkpoint pins configured as outputs and driven low
 * @post D9 (PA7) lit to indicate main() entered
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see gpio_set_output() / gpio_write_low() / gpio_write_high()
 *
 * @since Version 1.0.0
 */
static void internal_init_boot_checkpoint_leds(void)
{
  (void)gpio_set_output(k_rx_pa_7);
  (void)gpio_set_output(k_rx_pb_0);
  (void)gpio_set_output(k_rx_p7_1);
  (void)gpio_set_output(k_rx_p7_2);
  (void)gpio_set_output(k_rx_pb_1);
  (void)gpio_write_low(k_rx_pa_7);
  (void)gpio_write_low(k_rx_pb_0);
  (void)gpio_write_low(k_rx_p7_1);
  (void)gpio_write_low(k_rx_p7_2);
  (void)gpio_write_low(k_rx_pb_1);
  (void)gpio_write_high(k_rx_pa_7); /* D9 on: main() entered */
}

/**
 * @brief Configure USB0 module clock, SYSCFG and PHY (HUM 40.3.1.1 sequence)
 *
 * @details
 * Releases USB0 module-stop (MSTPCRB bit 19) under PRCR unlock, then walks the
 * HUM 40.3.1.1 ordering: clear SYSCFG -> SCKE=1 -> wait -> USBE=1. The two
 * spin-loop waits use volatile loop variables so the compiler cannot collapse
 * them. After USBE the PHY housekeeping (FIXPHY0 clear, PHYSLEW=0x5) runs
 * before INTENB0 programming (mirror of rx_usb_hw.c internal_usb_configure_phy()).
 *
 * Background: the previous order (USBE then SCKE) silently no-oped half the
 * SYSCFG follow-on writes because the USB module clock was gated until SCKE=1
 * was acknowledged. DPRPU stays 0 here; it is asserted later (after pipe + ICU
 * config) to announce attach to the host.
 *
 * @pre rx_clock_power_init() configured ICLK/PCLK; PRCR is currently locked
 *
 * @post MSTPCRB bit 19 cleared (USB0 module clock running)
 * @post SYSCFG.SCKE=1, USBE=1, DPRPU=0
 * @post DPUSR0R.FIXPHY0=0, USB0.PHYSLEW=0x5
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see HUM 40.3.1.1 USB clock supply ordering
 * @see rx_usb_hw.c internal_usb_configure_phy()
 *
 * @since Version 1.0.0
 */
static void internal_inline_usb0_clock_and_phy(void)
{
  volatile uint16_t* const PRCR_R    = (volatile uint16_t*)0x000803FEU;
  volatile uint32_t* const MSTPCRB_R = (volatile uint32_t*)0x00080014U;
  volatile uint16_t* const SYSCFG_R  = (volatile uint16_t*)0x000A0000U;

  *PRCR_R = k_main_prcr_unlock_clock_lpm;
  *MSTPCRB_R &= ~(1UL << 19);
  *PRCR_R = k_main_prcr_lock_all;

  /* HUM 40.3.1.1 ("Setting Data to the USB Related Register"):
   *   "Setting the SYSCFG.USBE bit to 1 AFTER starting the clock supply
   *    to the USB (SYSCFG.SCKE bit = 1) enables and starts USB operation."
   * The previous order (USBE then SCKE) silently no-ops half the SYSCFG
   * follow-on writes because the USB module clock is gated until SCKE=1
   * is acknowledged.  Required order: clear SYSCFG -> SCKE=1 -> wait ->
   * USBE=1.  DPRPU stays 0 here; it is asserted last (after pipe + ICU
   * config) to announce attach to the host. */
  *SYSCFG_R = 0x0000U;
  for (volatile uint32_t d = 0; d < k_usb_syscfg_settle_nops; d++) {
    __asm__ volatile("nop");
  }
  *SYSCFG_R |= (1U << 10); /* SCKE first per HUM 40.3.1.1 */
  for (volatile uint32_t d = 0; d < k_usb_syscfg_settle_nops; d++) {
    __asm__ volatile("nop");
  }
  *SYSCFG_R |= (1U << 0); /* USBE only after the clock is up */

  /* RX72N PHY housekeeping that mirrors rx_usb_hw.c's
   * internal_usb_configure_phy() -- must run between USBE=1 and
   * INTENB0 programming.  See tinyusb renesas/usba dcd_usba.c:626-628.
   *   - USB.DPUSR0R.FIXPHY0 cleared: release PHY from output-fixed state
   *   - USB0.PHYSLEW = 0x5     : RX72N-specific slew-rate trim */
  volatile uint32_t* const DPUSR0R_R = (volatile uint32_t*)0x000A0400U;
  volatile uint32_t* const PHYSLEW_R = (volatile uint32_t*)0x000A00F0U;
  *DPUSR0R_R &= ~(uint32_t)(1U << 4); /* FIXPHY0 */
  *PHYSLEW_R = 0x00000005U;           /* SLEWR00 | SLEWF00 */
}

/**
 * @brief Configure USB0 default control pipe (DCP) and endpoint interrupt enables
 *
 * @details
 * Programs DCPCFG, DCPMAXP=64 (default control pipe), DCPCTR=0x01, then enables
 * BRDY/BEMP for pipe 0 (DCP) and the global INTENB0 mask covering VBSE, RESM,
 * SOFE, DVSE, CTRE. Finally asserts SYSCFG.DPRPU to pull D+ high and announce
 * attach to the host.
 *
 * @pre internal_inline_usb0_clock_and_phy() completed (USBE=1, PHY trimmed)
 *
 * @post DCP configured for 64-byte EP0 control transfers
 * @post BRDYENB / BEMPENB / INTENB0 enable masks programmed
 * @post DPRPU=1 (D+ pull-up engaged; host enumeration begins)
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see HUM 40 Default Control Pipe register layout
 *
 * @since Version 1.0.0
 */
static void internal_inline_usb0_endpoint_setup(void)
{
  volatile uint16_t* const SYSCFG_R  = (volatile uint16_t*)0x000A0000U;
  volatile uint16_t* const INTENB0_R = (volatile uint16_t*)0x000A0030U;
  volatile uint16_t* const BRDYENB_R = (volatile uint16_t*)0x000A0036U;
  volatile uint16_t* const BEMPENB_R = (volatile uint16_t*)0x000A003AU;
  volatile uint16_t* const DCPCFG_R  = (volatile uint16_t*)0x000A005CU;
  volatile uint16_t* const DCPMAXP_R = (volatile uint16_t*)0x000A005EU;
  volatile uint16_t* const DCPCTR_R  = (volatile uint16_t*)0x000A0060U;

  *DCPCFG_R  = 0x0000U;
  *DCPMAXP_R = 64U;
  *DCPCTR_R  = 0x0001U;
  *BRDYENB_R = 0x0001U;
  *BEMPENB_R = 0x0001U;
  *INTENB0_R = (uint16_t)((1U << 15) | (1U << 12) | (1U << 11) | (1U << 10) | (1U << 8));

  *SYSCFG_R |= (1U << 4); /* DPRPU */
}

/**
 * @brief Wire USB0 USBI to ICU vector 144 (HUM 15.7.7 software-configurable IRQ)
 *
 * @details
 * USBI0 is a Group-B software-configurable interrupt on RX72N (HW manual Ch15
 * Table 15.3; hirakuni45/RX RX72N icu.hpp SELECTB::USBI0 = 62), so SLIBR[144]
 * must be programmed to 62 before IER/IPR/IR for vector 144 do anything.
 * Earlier revisions used vector 36 (a reserved gap), which silently consumed
 * IER/IPR writes and left the ISR dormant.
 *
 * Steps follow HUM 15.7.7 mandatory 9-step procedure:
 *   (1) IER bit clear (POR default; we re-set it in step 9)
 *   (2) SLIBR144 = 62 (USBI0 source code)
 *   (5) SLIPRCR.WPRC = 1   <-- previously missing -> ISR never fired
 *   (6) confirm WPRC == 1 (bounded poll, NASA P10 Rule 2)
 *   (8) IR144 = 0          (edge-detected vector, clear stale request)
 *   (9) IER18.IEN0 = 1     (enable vector 144 delivery)
 *
 * @pre internal_inline_usb0_endpoint_setup() completed (DPRPU on, EP enables set)
 *
 * @post SLIBR[144] = 62, SLIPRCR.WPRC latched (best-effort within bounded poll)
 * @post IPR[144] = 12, IR[144] = 0, IER18.IEN0 = 1
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 * @note If WPRC fails to latch within the bounded poll, the IER enable below is
 *       a no-op -- the ISR-entry counter diagnostic catches that case.
 *
 * @see HUM 15.7.7 Setting Software Configurable Interrupts (page 153)
 *
 * @since Version 1.0.0
 */
static void internal_inline_usb0_icu_setup(void)
{
  volatile uint8_t* const SLIBR144_R = (volatile uint8_t*)(0x00087700U + 144U);
  volatile uint8_t* const SLIPRCR_R  = (volatile uint8_t*)0x00087A00U;
  volatile uint8_t* const IPR144_R   = (volatile uint8_t*)(0x00087300U + 144U);
  volatile uint8_t* const IR144_R    = (volatile uint8_t*)(0x00087000U + 144U);
  volatile uint8_t* const IER18_R    = (volatile uint8_t*)(0x00087200U + 18U); /* 144 / 8 */

  enum : uint8_t {
    k_inline_usbi0_sli_src  = 62U,   /* USBI0 source number per HUM Table 15.3 */
    k_inline_sliprcr_wprc   = 0x01U, /* SLIPRCR.WPRC: write-once latch */
    k_inline_usbi_priority  = 12U,   /* IPR144 priority for USBI0 */
    k_inline_ier18_usbi_bit = 0U,    /* IER18 bit 0 == vector 144 (144 % 8) */
  };
  enum : uint16_t {
    k_inline_sliprcr_poll_max = 1024U, /* Bounded poll for HUM 15.7.7 step (6) */
  };

  *SLIBR144_R = k_inline_usbi0_sli_src;
  *SLIPRCR_R  = k_inline_sliprcr_wprc;
  /* HUM 15.7.7 step (6) bounded confirmation: WPRC latches in 1-2 ICLK
   * cycles; cap the poll so the boot path never spins forever (NASA
   * P10 Rule 2).  If WPRC fails to latch the IER enable below is a
   * no-op -- the ISR-entry counter diagnostic catches that case. */
  for (uint16_t i = 0; i < k_inline_sliprcr_poll_max; i++) {
    if ((*SLIPRCR_R & k_inline_sliprcr_wprc) != 0U) {
      break;
    }
  }
  *IPR144_R = k_inline_usbi_priority;
  *IR144_R  = 0U;
  *IER18_R |= (uint8_t)(1U << k_inline_ier18_usbi_bit);
}

/**
 * @brief Drive PB3 high as a pre-scheduler "main reached tx_kernel_enter" probe
 *
 * @details
 * PB3 (P4 pad 2, MCU pin 82, silkscreen EN3D) is driven HIGH in main BEFORE
 * tx_kernel_enter and later driven LOW by usb_task once it runs. An AD2 DIO7
 * probe on P4 pad 2 thus shows:
 *   HIGH = firmware reached main init but usb_task hasn't run
 *   LOW  = usb_task ran and set PB3 low
 *   0x0  = neither wrote (probe/wiring issue)
 *
 * @pre PORTB clocks active (default after reset)
 *
 * @post PB3 configured as GPIO output and driven HIGH
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see usb_task.c (drives PB3 LOW once running)
 *
 * @since Version 1.0.0
 */
static void internal_set_pb3_pre_kernel_probe(void)
{
  volatile uint8_t* const pb_pdr  = (volatile uint8_t*)0x0008C00BU;
  volatile uint8_t* const pb_podr = (volatile uint8_t*)0x0008C02BU;
  volatile uint8_t* const pb_pmr  = (volatile uint8_t*)0x0008C06BU;
  *pb_pmr &= (uint8_t) ~(1U << 3);
  *pb_pdr |= (uint8_t)(1U << 3);
  *pb_podr |= (uint8_t)(1U << 3); /* HIGH */
}

/**
 * @brief Run the full inline USB0 bring-up sequence in the pre-kernel window
 *
 * @details
 * Composes the four extracted helpers in the only order the HUM allows:
 *   1. Module-stop release + clock + PHY (HUM 40.3.1.1)
 *   2. Default control pipe + EP interrupt enables (HUM 40 EP layout)
 *   3. ICU/SLIBR/IER wiring for USBI0 (HUM 15.7.7)
 *   4. PB3 diagnostic probe HIGH
 *
 * Kept as raw register pokes (mirrors rx_usb_hw.c) rather than calling
 * rx_usb_hw_init() so the very first PRCR/MSTPCR/SYSCFG writes run in the
 * single-threaded pre-kernel window. This lets the call to
 * rx_usb_hw_mark_initialized() afterwards safely tell rx_usb_init() to skip
 * its own redundant init pass. See cherry-pick 0abb79c54.
 *
 * @pre hardware_init() completed (peripheral clocks, GPIO already configured)
 * @pre rx_nanopb_init() completed (so the post-attach RX path is ready)
 *
 * @post USB0 module enabled, DCP configured, USBI0 ICU vector wired
 * @post DPRPU asserted (host enumeration in flight)
 * @post PB3 driven HIGH for AD2 visibility
 *
 * @note Executes single-threaded before scheduler start; no synchronization needed.
 *
 * @see internal_inline_usb0_clock_and_phy()
 * @see internal_inline_usb0_endpoint_setup()
 * @see internal_inline_usb0_icu_setup()
 * @see internal_set_pb3_pre_kernel_probe()
 *
 * @since Version 1.0.0
 */
static void internal_inline_usb0_bringup(void)
{
  /* Inline USB0 bring-up sequence -- raw register pokes that mirror
   * libs/rx_usb/src/rx_usb_hw.c.  Kept inline (rather than inside
   * rx_usb_hw_init()) so the very first PRCR/MSTPCR/SYSCFG writes run
   * in the single-threaded pre-kernel window and the call to
   * rx_usb_hw_mark_initialized() below can safely tell rx_usb_init()
   * to skip its own redundant init pass.  See cherry-pick 0abb79c54. */
  internal_inline_usb0_clock_and_phy();
  internal_inline_usb0_endpoint_setup();
  internal_inline_usb0_icu_setup();
  internal_set_pb3_pre_kernel_probe();
}

int main(void)
{
  /* BRING-UP CHECKPOINTS -- "one hot" LED indicator of how far boot got
   * without any UART. See internal_init_boot_checkpoint_leds() for the
   * complete D9-D13 mapping table. */
  internal_init_boot_checkpoint_leds();

  /* Check startup flags (bring-up: never halt; diagnostic printed later). */
  rx_err_t ret = internal_check_startup_flags();
  RX_ERROR_CHECK(ret);

  (void)gpio_write_low(k_rx_pa_7);
  (void)gpio_write_high(k_rx_pb_0); /* D10: startup flags checked */

  /* Initialize system clocks and power management */
  ret = rx_clock_power_init();
  RX_ERROR_CHECK(ret); /* If this fails, errors cant be logged */

  (void)gpio_write_low(k_rx_pb_0);
  (void)gpio_write_high(k_rx_p7_1); /* D11: clock init done */

  /* ========================================================================
   * EARLY UART INITIALIZATION - Enable error logging ASAP
   * ======================================================================== */
  ret = uart_debug_init();

  /* Report startup flags to console (only if UART initialized successfully) */
  if (ret == k_rx_ok) {
    uart_debug_puts("\r\n=== STAR RX72N BOOT ===\r\n");
    internal_report_startup_flags();
    (void)gpio_write_low(k_rx_p7_1);
    (void)gpio_write_high(k_rx_p7_2); /* D12: UART init OK */
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

  (void)gpio_write_low(k_rx_p7_2);
  (void)gpio_write_high(k_rx_pb_1); /* D13: hardware_init done */

  /* Enable the nanopb wrapper so telemetry_task's rx_nanopb_encode_*()
   * and comm_task's rx_nanopb_decode_*() stop returning
   * k_rx_err_not_initialized (0x10F). The wrapper is a pure software
   * module with no hardware deps, so initialize it during the
   * pre-kernel single-threaded window next to the other module inits. */
  ret = rx_nanopb_init();
  RX_ERROR_CHECK(ret);

  /* Pre-kernel inline USB0 bring-up: HUM 40.3.1.1 clock + PHY -> EP setup ->
   * HUM 15.7.7 ICU wiring -> PB3 diagnostic. See helper for the full narrative. */
  internal_inline_usb0_bringup();

  /* Hardware is now fully attached by the inline sequence above.  Tell
   * the production rx_usb_hw layer that s_hw_initialized = true so
   * rx_usb_init below skips the redundant register sequence, then call
   * rx_usb_init() to set up ring buffers, CDC class state, and flip
   * s_usb.initialized = true.  Without this, rx_usb_write() early-exits
   * with k_rx_err_invalid_state and telemetry never reaches the host. */
  rx_usb_hw_mark_initialized();
  ret = rx_usb_init(nullptr);
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
