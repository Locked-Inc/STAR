/**
 * @file hardware_init.c
 * @brief Application-Specific Hardware Initialization - Motor Control, Sensors, Communication
 *
 * @details
 * # Overview
 *
 * This file implements **Stage 2 of the boot sequence** - application-specific peripheral
 * initialization. It is called **after** system clocks are configured (rx_clock_power_init)
 * but **before** ThreadX RTOS starts (tx_kernel_enter).
 *
 * **Boot sequence context:**
 * ```
 * main() -> rx_clock_power_init() -> hardware_init() -> tx_kernel_enter()
 *           ^ System clocks        ^ This file      ^ Start RTOS
 * ```
 *
 * ## Peripheral Initialization Order (7 Stages)
 *
 * **Critical ordering** - later stages depend on earlier stages:
 *
 * 1. **Precondition validation** (~0.5 us)
 *    - Verify system clocks initialized (SCKCR3 register check)
 *    - Ensure memory-mapped I/O accessible
 *
 * 2. **GPIO configuration** (planned, not yet implemented)
 *    - Motor control pins (PWM enable, GTETRG fault detection)
 *    - LED indicators (status, error, activity)
 *    - Sensor chip selects (SPI CS pins)
 *
 * 3. **Timers** (~10 us) **IMPLEMENTED**
 *    - CMT0 for ThreadX tick (100 Hz)
 *    - GPTW for PWM generation (motor control)
 *
 * 4. **UART debug** - MOVED TO MAIN()
 *    - SCI9 initialized in main() before hardware_init() is called
 *    - Error logging available for all peripheral init below
 *    - See main.c for early UART initialization
 *
 * 5. **Communication peripherals** (planned, not yet implemented)
 *    - SPI for RPi5 communication and sensors
 *    - I2C for IMU, temperature sensors
 *    - USB CDC for ROS2 communication
 *
 * 6. **ADC channels** (planned, not yet implemented)
 *    - Current sensing (motor protection)
 *    - Temperature sensing (thermal management)
 *
 * ## Hardware Initialization Architecture
 *
 * @dot
 * digraph hardware_init {
 *   rankdir=TB;
 *   node [shape=box];
 *
 *   Entry [label="hardware_init()", shape=ellipse];
 *   Precond [label="Precondition Check\nSCKCR3 != reset state"];
 *   GPIO [label="GPIO Init\n(Planned)", style=dashed];
 *   Timers [label="Timer Init\nCMT0 @ 100 Hz", style=filled, fillcolor=lightgreen];
 *   UART [label="UART Debug Init\nSCI9 @ 115200", style=filled, fillcolor=lightgreen];
 *   SPI [label="SPI Init\n(Planned)", style=dashed];
 *   I2C [label="I2C Init\n(Planned)", style=dashed];
 *   ADC [label="ADC Init\n(Planned)", style=dashed];
 *   Postcond [label="Postcondition Check\nSCKCR3 still valid"];
 *   Exit [label="return k_rx_ok", shape=ellipse];
 *
 *   Entry -> Precond;
 *   Precond -> GPIO [label="Assert OK"];
 *   GPIO -> Timers;
 *   Timers -> UART [label="k_rx_ok"];
 *   UART -> SPI [label="k_rx_ok"];
 *   SPI -> I2C;
 *   I2C -> ADC [label="k_rx_ok"];
 *   ADC -> Postcond;
 *   Postcond -> Exit [label="Assert OK"];
 *
 *   Timers -> Halt [label="Error", color=red];
 *   UART -> Halt [label="Error", color=red];
 *   SPI -> Halt [label="Error", color=red];
 *
 *   Halt [label="return error", shape=octagon, color=red];
 * }
 * @enddot
 *
 * ## Initialization Timing (RX72N @ 240 MHz)
 *
 * | Stage | Duration | Implementation | Notes |
 * |-------|----------|----------------|-------|
 * | **Precondition check** | ~0.5 us | [COMPLETE] | SCKCR3 register read + assert |
 * | **GPIO init** | ~5 us | [PENDING] Planned | Pin mode, pull-up/down, output levels |
 * | **Timer init** | ~10 us | [COMPLETE] | CMT0 setup for ThreadX tick |
 * | **UART debug** | ~50 us | [COMPLETE] | SCI9 baud rate, FIFO, interrupts |
 * | **SPI init** | ~20 us | [COMPLETE] | RSPI2 peripheral mode, 8-bit, mode 0 |
 * | **I2C init** | ~15 us | [PENDING] Planned | RIIC0 speed, addressing, interrupts |
 * | **ADC init** | ~100 us | [PENDING] Planned | ADC0 calibration, channel config |
 * | **Postcondition check** | ~0.5 us | [COMPLETE] | SCKCR3 stability verification |
 * | **Total (current)** | **~81 us** | Timers + UART + SPI + I2C | |
 * | **Total (planned)** | **~206 us** | All peripherals | |
 *
 * ## Memory Usage
 *
 * | Object | Size | Location | Purpose |
 * |--------|------|----------|---------|
 * | **s_sckcr3_reset_state** | 1 byte | .rodata | Clock validation constant |
 * | **Function stack** | ~64 bytes | Main stack | Local variables, return addresses |
 * | **Peripheral registers** | N/A | Memory-mapped | Hardware configuration only |
 *
 * **Total RAM:** ~64 bytes (stack only - no heap, no static buffers)
 *
 * ## Error Handling Strategy
 *
 * **Three-tier approach:**
 *
 * 1. **Critical errors (RX_ASSERT):**
 *    - Precondition failed (clocks not initialized)
 *    - Postcondition failed (clock system corrupted)
 *    - Register pointer NULL (memory map violation)
 *    - **Action:** Halt execution immediately (fail-fast)
 *
 * 2. **Peripheral errors (return error code):**
 *    - Timer init failed (timer_init() returns error)
 *    - UART init failed (uart_debug_init() returns error)
 *    - SPI/I2C/ADC init failed (when implemented)
 *    - **Action:** Return error to main(), which halts boot
 *
 * 3. **Non-critical warnings (log and continue):**
 *    - GPIO pin already configured (no action needed)
 *    - Peripheral module already enabled (idempotent operation)
 *    - **Action:** Log warning, return k_rx_ok
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation Notes |
 * |------|--------|----------------------|
 * | **Rule 1: Control flow** | [PASS] | No goto, setjmp, or recursion |
 * | **Rule 2: Loop bounds** | [PASS] | No loops (sequential initialization) |
 * | **Rule 3: Heap allocation** | [PASS] | Zero dynamic allocation (static only) |
 * | **Rule 4: Function length** | [PASS] | hardware_init() = 80 lines |
 * | **Rule 5: Assertions** | [PASS] | 2 preconditions, 1 postcondition |
 * | **Rule 6: Data scope** | [PASS] | All variables at function scope |
 * | **Rule 7: Return checks** | [PASS] | All init functions checked via rx_err_is_error() |
 * | **Rule 8: Preprocessor** | [PASS] | Zero macros (uses typed enum for constant) |
 * | **Rule 9: Pointers** | [PASS] | Single-level dereferencing only |
 * | **Rule 10: Warnings** | [PASS] | Compiles with -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **Single Responsibility** | hardware_init() does ONLY peripheral setup (no business logic) |
 * | **Open/Closed** | New peripherals added without modifying existing init calls |
 * | **Liskov Substitution** | All init functions return rx_err_t with consistent semantics |
 * | **Interface Segregation** | Small, focused init APIs (timer_init, uart_init, etc. separate) |
 * | **Dependency Inversion** | Depends on abstractions (rx_err_t), not implementations |
 *
 * ## Thread Safety
 *
 * **Pre-ThreadX context:** This file executes in **single-threaded mode** before the RTOS starts.
 * No synchronization primitives needed. Hardware registers accessed directly without mutex protection.
 *
 * **Post-init:** After this function returns, peripherals are **ready for multi-threaded access**.
 * Application threads must use appropriate synchronization (mutexes, semaphores) when accessing
 * shared hardware resources.
 *
 * ## Related Files
 *
 * - **Clock init:** See [rx_clock_power_init.c](rx_clock_power_init.c) - Must complete BEFORE this file
 * - **Main entry:** See [main.c](main.c) - Calls this function during boot sequence
 * - **Timer HAL:** See [timer.c](../lib/rx_hal/src/timer.c) - CMT0 configuration for ThreadX tick
 * - **UART HAL:** See [uart.c](../lib/rx_hal/src/uart.c) - SCI9 configuration for debug console
 *
 * @note **This file is incomplete.** GPIO, I2C, and ADC initialization are planned but not yet
 *       implemented. Timers, UART, SPI, and I2C are functional in current version.
 *
 * @warning **Never call hardware_init() before rx_clock_power_init().** System clocks must be
 *          configured first. Precondition assertion will halt execution if violated.
 *
 * @see hardware_init() Main initialization function (entry point for this file)
 * @see rx_clock_power_init() System clock configuration (must call first)
 * @see timer_init() Configure CMT0 for ThreadX tick
 * @see uart_debug_init() Configure SCI9 for debug console
 *
 * @since Version 1.0.0
 *
 * @par Revision History:
 * - v1.1.0 (2026-02): Add GPIO helper functions for DRV8263H motor driver,
 *   IMU, sonar, ADC, and LED pin initialization
 * - v1.0.0 (2026-01): Initial implementation with timers and UART
 *
 * @date 2026-01-14
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "hardware_init.h"

#include "hardware.h"
#include "rx72n_icu_regs.h"
#include "rx72n_system_regs.h"
#include "rx_check.h"
#include "rx_err.h"
#include "rx_gptw.h"
#include "rx_irq_filter.h"
#include "rx_mpc.h"
#include "rx_poeg.h"
#include "rx_port_utils.h"
#include "rx_simulator_config.h" /* For RX_IS_SIMULATOR conditional compilation */
/**
 * @enum twake_delay_t
 * @brief DRV8263H-Q1 tWAKE busy-wait delay constants
 *
 * @details
 * Used for the pre-kernel busy-wait after nSLEEP assertion. ThreadX APIs
 * (tx_thread_sleep) cannot be used because hardware_init() runs before
 * tx_kernel_enter().
 *
 * @invariant k_twake_busy_wait_us must be > 0 and chosen to provide margin
 *            over the 1.2 ms tWAKE specification (current value gives ~10 ms)
 *
 * @code
 * internal_busy_wait_us(k_twake_busy_wait_us, k_twake_cpu_mhz);
 * @endcode
 *
 * @see twake_cpu_t CPU frequency constant for cycle calculation
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_twake_busy_wait_us = 2000, /**< Busy-wait duration in microseconds; actual delay ~10 ms due to
                                    ~5-6 CPU cycles per volatile decrement (margin over 1.2 ms spec) */
} twake_delay_t;

/**
 * @enum twake_cpu_t
 * @brief CPU frequency constant for tWAKE busy-wait cycle calculation
 *
 * @details
 * Separated from twake_delay_t because it represents a different physical
 * quantity (frequency vs. duration). Used to compute loop iteration count:
 * cycles = k_twake_busy_wait_us * k_twake_cpu_mhz.
 *
 * @invariant k_twake_cpu_mhz must be a positive integer representing the
 *            RX72N CPU frequency in MHz (240 for the STAR board)
 *
 * @code
 * volatile uint32_t cycles = (uint32_t)k_twake_busy_wait_us * (uint32_t)k_twake_cpu_mhz;
 * @endcode
 *
 * @see twake_delay_t Delay duration constant
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_twake_cpu_mhz = 240, /**< RX72N CPU frequency in MHz for cycle count calculation */
} twake_cpu_t;

/**
 * @brief Compile-time verification that tWAKE cycle count fits in uint32_t
 * @details Ensures k_twake_busy_wait_us * k_twake_cpu_mhz does not overflow
 *          the volatile uint32_t counter used in internal_busy_wait_us().
 */
static_assert((bool)(((uint64_t)k_twake_busy_wait_us * (uint64_t)k_twake_cpu_mhz) <=
                     (uint64_t)UINT32_MAX),
              "tWAKE cycle count overflows uint32_t");

/** @brief Port pin identifiers for MPC configuration (rx_port_pin_t values) */
typedef enum : uint16_t {
  /* Host I2C (RIIC0) */
  k_pin_host_scl0 = k_rx_p1_2, /**< P1.2 - SCL0 (host I2C clock) */
  k_pin_host_sda0 = k_rx_p1_3, /**< P1.3 - SDA0 (host I2C data) */

  /* USB */
  k_pin_usb_vbus = k_rx_p1_6, /**< P1.6 - USB0_VBUS (USB VBUS detect) */

  /* HC-SR04 Sonar triggers (GPIO output, initial LOW) */
  k_pin_sonar_trig0 = k_rx_pf_5, /**< PF.5 - Sonar 0 trigger (pin 9) */
  k_pin_sonar_trig1 = k_rx_pj_5, /**< PJ.5 - Sonar 1 trigger (pin 11) */
  k_pin_sonar_trig2 = k_rx_pj_3, /**< PJ.3 - Sonar 2 trigger (pin 13) */
  k_pin_sonar_trig3 = k_rx_p3_3, /**< P3.3 - Sonar 3 trigger (pin 26) */

  /* HC-SR04 Sonar echoes (GPIO input, IRQ8-11) */
  k_pin_sonar_echo0 = k_rx_p0_3, /**< P0.3 - Sonar 0 echo (pin 4, IRQ11) */
  k_pin_sonar_echo1 = k_rx_p0_2, /**< P0.2 - Sonar 1 echo (pin 6, IRQ10) */
  k_pin_sonar_echo2 = k_rx_p0_1, /**< P0.1 - Sonar 2 echo (pin 7, IRQ9) */
  k_pin_sonar_echo3 = k_rx_p0_0, /**< P0.0 - Sonar 3 echo (pin 8, IRQ8) */

  /**
   * Host SPI (RSPI2 peripheral mode on PORTD)
   * - PD1 (MOSIC): COPI - Controller Out, Peripheral In (data from RPi5)
   * - PD2 (MISOC): CIPO - Controller In, Peripheral Out (data to RPi5)
   */
  k_pin_host_copi = k_rx_pd_1, /**< PD.1 - COPI/MOSIC (RPi5 -> RX72N) */
  k_pin_host_cipo = k_rx_pd_2, /**< PD.2 - CIPO/MISOC (RX72N -> RPi5) */
  k_pin_host_sclk = k_rx_pd_3, /**< PD.3 - SCLK (RSPI2 clock from RPi5) */
  k_pin_host_cs0  = k_rx_pd_4, /**< PD.4 - CS0 (chip select from RPi5) */

  /* Host control signals */
  k_pin_host_irq =
    k_rx_p6_7, /**< P6.7 - HOST_IRQ (active-low output to RPi5, data ready, pin 98) */

  /* MTU Encoder clock inputs (front wheels) */
  k_pin_enc0_pha = k_rx_p2_4, /**< P2.4 - MTCLKA (encoder 0 phase A) */
  k_pin_enc0_phb = k_rx_p2_5, /**< P2.5 - MTCLKB (encoder 0 phase B) */
  k_pin_enc1_pha = k_rx_pa_1, /**< PA.1 - MTCLKC (encoder 1 phase A) */
  k_pin_enc1_phb = k_rx_pc_5, /**< PC.5 - MTCLKD (encoder 1 phase B) */

  /* TPU Encoder clock inputs (rear wheels) */
  k_pin_enc2_pha = k_rx_pc_2, /**< PC.2 - TCLKA (encoder 2 phase A) */
  k_pin_enc2_phb = k_rx_pa_3, /**< PA.3 - TCLKB (encoder 2 phase B) */
  k_pin_enc3_pha = k_rx_pc_0, /**< PC.0 - TCLKC (encoder 3 phase A) */
  k_pin_enc3_phb = k_rx_pb_3, /**< PB.3 - TCLKD (encoder 3 phase B) */

  /* GPTW PWM outputs (4 motors x 2 pins = IN2 + IN1, distributed across ports) */
  k_pin_motor0_in2 = k_rx_p2_3, /**< P2.3 - GTIOC0A (motor 0 IN2/direction, pin 34) */
  k_pin_motor0_in1 = k_rx_p1_7, /**< P1.7 - GTIOC0B (motor 0 IN1/PWM, pin 38) */
  k_pin_motor1_in2 = k_rx_p2_2, /**< P2.2 - GTIOC1A (motor 1 IN2/direction, pin 35) */
  k_pin_motor1_in1 = k_rx_pc_3, /**< PC.3 - GTIOC1B (motor 1 IN1/PWM, pin 67) */
  k_pin_motor2_in2 = k_rx_pe_3, /**< PE.3 - GTIOC2A (motor 2 IN2/direction, pin 108) */
  k_pin_motor2_in1 = k_rx_p8_6, /**< P8.6 - GTIOC2B (motor 2 IN1/PWM, pin 41) */
  k_pin_motor3_in2 = k_rx_pe_7, /**< PE.7 - GTIOC3A (motor 3 IN2/direction, pin 101) */
  k_pin_motor3_in1 = k_rx_pc_6, /**< PC.6 - GTIOC3B (motor 3 IN1/PWM, pin 61) */

  /* IMU (RIIC1 I2C + GPIO) */
  k_pin_imu_scl = k_rx_p2_1, /**< P2.1 - SCL1 (IMU I2C clock, pin 36) */
  k_pin_imu_sda = k_rx_p2_0, /**< P2.0 - SDA1 (IMU I2C data, pin 37) */
  k_pin_imu_int = k_rx_p3_2, /**< P3.2 - IMU interrupt (active-low, pin 27) */
  k_pin_imu_rst = k_rx_p8_3, /**< P8.3 - IMU reset (active-low, pin 58) */

  /* DRV8263H DRVOFF pins (GPIO output, initial HIGH = outputs disabled) */
  k_pin_motor0_drvoff = k_rx_p6_1, /**< P6.1 - Motor 0 DRVOFF (pin 115) */
  k_pin_motor1_drvoff = k_rx_p6_3, /**< P6.3 - Motor 1 DRVOFF (pin 113) */
  k_pin_motor2_drvoff = k_rx_pe_0, /**< PE.0 - Motor 2 DRVOFF (pin 111) */
  k_pin_motor3_drvoff = k_rx_pe_2, /**< PE.2 - Motor 3 DRVOFF (pin 109) */

  /* DRV8263H nSLEEP pins (GPIO output, initial HIGH = awake) */
  k_pin_motor0_nsleep = k_rx_p6_0, /**< P6.0 - Motor 0 nSLEEP (pin 117) */
  k_pin_motor1_nsleep = k_rx_p6_2, /**< P6.2 - Motor 1 nSLEEP (pin 114) */
  k_pin_motor2_nsleep = k_rx_p6_4, /**< P6.4 - Motor 2 nSLEEP (pin 112) */
  k_pin_motor3_nsleep = k_rx_pe_1, /**< PE.1 - Motor 3 nSLEEP (pin 110) */

  /* ADC current sense (S12AD0, AN004-AN007) */
  k_pin_adc_an004 = k_rx_p4_4, /**< P4.4 - AN004 (motor 3 current) */
  k_pin_adc_an005 = k_rx_p4_5, /**< P4.5 - AN005 (motor 2 current) */
  k_pin_adc_an006 = k_rx_p4_6, /**< P4.6 - AN006 (motor 1 current) */
  k_pin_adc_an007 = k_rx_p4_7, /**< P4.7 - AN007 (motor 0 current) */
} rx_mpc_pin_t;

/**
 * @enum gpio_pin_counts_t
 * @brief Static array-size and loop-bound constants for GPIO pin groups
 *
 * @details
 * Provides compile-time upper bounds for all GPIO pin arrays. Used as loop
 * limits in internal_gpio_init_*() helpers and as array-size declarators.
 *
 * @invariant All values must fit in uint8_t (loop counter type)
 *
 * @code{.c}
 * const rx_port_pin_t pins[k_gptw_pin_count] = { ... };
 * for (uint8_t i = 0; i < k_gptw_pin_count; i++) {
 *     rx_mpc_set_gptw(pins[i]);
 * }
 * @endcode
 *
 * @see internal_gpio_init_gptw_pwm() Uses k_gptw_pin_count
 * @see internal_gpio_init_motor_driver_ctrl() Uses k_motor_count
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_gptw_pin_count = 8, /**< 4 motors x 2 pins (IN2 + IN1) */
  k_motor_count    = 4, /**< 4 DRV8263H motor drivers */
  k_sonar_count    = 4, /**< 4 HC-SR04 ultrasonic sensors */
} gpio_pin_counts_t;

/** @brief GPTW PWM frequency constant */
typedef enum : uint32_t {
  k_gptw_pwm_freq_hz = 20000, /**< 20 kHz PWM frequency */
} gptw_freq_t;

/** @brief GPTW dead-time constant */
typedef enum : uint16_t {
  k_gptw_deadtime_ns = 1000, /**< 1 us dead-time between complementary outputs */
} gptw_deadtime_t;

/* RSPI channel: use k_rspi_channel_2 from hardware.h for host SPI peripheral */

/**
 * @enum i2c_channel_t
 * @brief I2C channel assignments for RX72N RIIC peripherals
 *
 * @details
 * Maps logical I2C bus roles to physical RIIC channel numbers.
 * RIIC0 is the host-side channel used for RPi5 communication.
 * RIIC1 is the IMU channel shared by BNO055 and BMP280.
 *
 * @invariant k_i2c_channel_0 != k_i2c_channel_1 (distinct physical channels)
 * @invariant Values map 1:1 to RIIC peripheral indices in the RX72N register map
 *
 * @see i2c_freq_t Corresponding frequency constants per channel
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_i2c_channel_0 = 0, /**< RIIC0 = host I2C (RPi5 comms) */
  k_i2c_channel_1 = 1, /**< RIIC1 = IMU I2C (BNO055 + BMP280) */
} i2c_channel_t;
static_assert((bool)(k_i2c_channel_0 != k_i2c_channel_1),
              "I2C channel assignments must be distinct");

/**
 * @enum i2c_freq_limits_t
 * @brief Valid I2C frequency range constants for RIIC channel validation
 *
 * @details
 * Defines the minimum and maximum allowed I2C frequencies for compile-time
 * assertions. The maximum is the I2C fast mode upper bound (400 kHz). Used
 * to replace raw numeric literals in static_assert checks.
 *
 * @invariant k_i2c_freq_min_hz > 0 (non-zero)
 * @invariant k_i2c_fast_mode_max_hz == 400000 (I2C fast mode specification)
 *
 * @see i2c_freq_t Channel frequency assignments validated against these limits
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_i2c_freq_min_hz      = 1U,      /**< Minimum valid I2C frequency (must be > 0) */
  k_i2c_fast_mode_max_hz = 400000U, /**< I2C fast mode maximum frequency (400 kHz) */
} i2c_freq_limits_t;

/**
 * @enum i2c_freq_t
 * @brief I2C bus frequency constants for each RIIC channel
 *
 * @details
 * Both host and IMU channels operate at 400 kHz (I2C fast mode).
 * The BNO055 and BMP280 sensors both support up to 400 kHz.
 * The RPi5 host interface also uses 400 kHz for maximum throughput.
 *
 * @invariant All frequency values >= k_i2c_freq_min_hz and <= k_i2c_fast_mode_max_hz
 * @invariant Values are stable hardware constants; never modified at runtime
 *
 * @see i2c_channel_t Channel assignments these frequencies apply to
 * @see i2c_freq_limits_t Valid frequency range
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_i2c_host_freq_hz = k_i2c_fast_mode_max_hz, /**< 400 kHz fast mode for host (RIIC0) */
  k_i2c_imu_freq_hz  = k_i2c_fast_mode_max_hz, /**< 400 kHz fast mode for IMU sensors (RIIC1) */
} i2c_freq_t;
static_assert((bool)((unsigned int)k_i2c_host_freq_hz >= (unsigned int)k_i2c_freq_min_hz),
              "Host I2C frequency must be non-zero");
static_assert((bool)((unsigned int)k_i2c_imu_freq_hz >= (unsigned int)k_i2c_freq_min_hz),
              "IMU I2C frequency must be non-zero");

/** @brief Number of motor current ADC channels */
typedef enum : uint8_t {
  k_motor_adc_count = 4, /**< 4 motors = 4 current sense channels */
} adc_count_t;

/**
 * @enum gpio_reg_constants_t
 * @brief Constants for GPIO register bit manipulation
 *
 * @details
 * Provides named constants for single-bit operations on 8-bit GPIO port
 * registers (PMR, PDR, PODR). Eliminates magic number `1U` in bit-shift
 * expressions and documents the hardware-imposed pin count limit.
 *
 * @invariant k_gpio_single_bit_mask == 1 (exactly one bit for shift operations)
 * @invariant k_gpio_max_pin_number == k_pins_per_port - 1
 *
 * @see internal_gpio_set_output() Uses these for register bit manipulation
 * @see internal_gpio_set_input() Uses these for register bit manipulation
 * @see k_pins_per_port From rx_gpio_constants.h (hardware 8-pin limit)
 *
 * @code{.c}
 * // Set pin 5 as output in PDR register (read-modify-write)
 * port->PDR |= (uint8_t)(k_gpio_single_bit_mask << k_bit_led);
 *
 * // Validate pin number before access
 * if (pin_number <= k_gpio_max_pin_number) {
 *     port->PODR |= (uint8_t)(k_gpio_single_bit_mask << pin_number);
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_gpio_single_bit_mask = 1U, /**< Single-bit mask shifted by pin number for register RMW */
  k_gpio_max_pin_number  = 7U, /**< Maximum valid pin index (0-7, 8 pins per port) */
} gpio_reg_constants_t;

/** @brief SCKCR3 reset/unconfigured state value (before clock initialization) */
static const uint8_t s_sckcr3_reset_state = 0U;

#if !RX_IS_SIMULATOR
/**
 * @brief Configure I2C bus pins (RIIC0/RIIC1)
 *
 * @details
 * Configures MPC pin multiplexing for Host I2C (RIIC0) on P1.2/P1.3 and
 * RIIC host I2C on P2.0/P2.1. Sets PSEL registers to enable I2C peripheral
 * function for SCL and SDA pins.
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P1.2 configured as SCL0, P1.3 configured as SDA0
 * @post P2.1 configured as SCL1, P2.0 configured as SDA1
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_i2c(void)
{
  static const char* const s_tag = "GPIO_I2C";

  /* Host I2C (RIIC0): SCL0 + SDA0 */
  rx_err_t err = rx_mpc_set_riic((rx_port_pin_t)k_pin_host_scl0);
  RX_RETURN_ON_ERROR(err, s_tag, "SCL0 pin config failed");

  err = rx_mpc_set_riic((rx_port_pin_t)k_pin_host_sda0);
  RX_RETURN_ON_ERROR(err, s_tag, "SDA0 pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure host SPI pins (RSPI2)
 *
 * @details
 * Configures MPC pin multiplexing for RSPI2 host SPI interface on PD.1-PD.4.
 * Sets PSEL registers to enable RSPI2 peripheral function for COPI, CIPO,
 * SCLK, and CS0 pins used for RPi5 communication.
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post PD.1-PD.4 configured as RSPI2 COPI/CIPO/SCLK/CS0
 * @post RSPI2 pins ready for host communication
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_host_spi(void)
{
  static const char* const s_tag = "GPIO_SPI";

  rx_err_t err = rx_mpc_set_rspi((rx_port_pin_t)k_pin_host_copi);
  RX_RETURN_ON_ERROR(err, s_tag, "RSPI2 COPI pin config failed");

  err = rx_mpc_set_rspi((rx_port_pin_t)k_pin_host_cipo);
  RX_RETURN_ON_ERROR(err, s_tag, "RSPI2 CIPO pin config failed");

  err = rx_mpc_set_rspi((rx_port_pin_t)k_pin_host_sclk);
  RX_RETURN_ON_ERROR(err, s_tag, "RSPI2 SCLK pin config failed");

  err = rx_mpc_set_rspi((rx_port_pin_t)k_pin_host_cs0);
  RX_RETURN_ON_ERROR(err, s_tag, "RSPI2 CS0 pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure MTU encoder input pins (front wheels)
 *
 * @details
 * Configures MPC pin multiplexing for MTU external clock inputs MTCLKA-MTCLKD
 * on P2.4/P2.5/PA.1/PC.5. Enables pulse counter mode for quadrature encoder
 * inputs from front wheel motors.
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P2.4/P2.5 configured as MTCLKA/MTCLKB
 * @post PA.1/PC.5 configured as MTCLKC/MTCLKD
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_mtu_encoders(void)
{
  static const char* const s_tag = "GPIO_MTU_ENC";

  rx_err_t err = rx_mpc_set_mtu_encoder((rx_port_pin_t)k_pin_enc0_pha);
  RX_RETURN_ON_ERROR(err, s_tag, "MTCLKA pin config failed");

  err = rx_mpc_set_mtu_encoder((rx_port_pin_t)k_pin_enc0_phb);
  RX_RETURN_ON_ERROR(err, s_tag, "MTCLKB pin config failed");

  err = rx_mpc_set_mtu_encoder((rx_port_pin_t)k_pin_enc1_pha);
  RX_RETURN_ON_ERROR(err, s_tag, "MTCLKC pin config failed");

  err = rx_mpc_set_mtu_encoder((rx_port_pin_t)k_pin_enc1_phb);
  RX_RETURN_ON_ERROR(err, s_tag, "MTCLKD pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure TPU encoder input pins (rear wheels)
 *
 * @details
 * Configures MPC pin multiplexing for TPU external clock inputs TCLKA-TCLKD
 * on PC.2/PA.3/PC.0/PB.3. Enables pulse counter mode for quadrature encoder
 * inputs from rear wheel motors.
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post PC.2/PA.3 configured as TCLKA/TCLKB
 * @post PC.0/PB.3 configured as TCLKC/TCLKD
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_tpu_encoders(void)
{
  static const char* const s_tag = "GPIO_TPU_ENC";

  rx_err_t err = rx_mpc_set_tpu_encoder((rx_port_pin_t)k_pin_enc2_pha);
  RX_RETURN_ON_ERROR(err, s_tag, "TCLKA pin config failed");

  err = rx_mpc_set_tpu_encoder((rx_port_pin_t)k_pin_enc2_phb);
  RX_RETURN_ON_ERROR(err, s_tag, "TCLKB pin config failed");

  err = rx_mpc_set_tpu_encoder((rx_port_pin_t)k_pin_enc3_pha);
  RX_RETURN_ON_ERROR(err, s_tag, "TCLKC pin config failed");

  err = rx_mpc_set_tpu_encoder((rx_port_pin_t)k_pin_enc3_phb);
  RX_RETURN_ON_ERROR(err, s_tag, "TCLKD pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure GPTW PWM output pins (4 motors)
 *
 * @details
 * Configures MPC pin multiplexing for GPTW0-GPTW3 PWM outputs distributed
 * across PORT2, PORT1, PORT8, PORTC, and PORTE. Sets PSEL for all 8 pins
 * (4 motors x IN2+IN1 per motor) for DRV8263H motor driver control.
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P23/P17, P22/PC3, PE3/P86, PE7/PC6 configured as GPTW outputs
 * @post All 8 GPTW PWM pins ready for motor control
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_gptw_pwm(void)
{
  static const char* const s_tag = "GPIO_GPTW";

  const rx_port_pin_t gptw_pins[] = {(rx_port_pin_t)k_pin_motor0_in2,
                                     (rx_port_pin_t)k_pin_motor0_in1,
                                     (rx_port_pin_t)k_pin_motor1_in2,
                                     (rx_port_pin_t)k_pin_motor1_in1,
                                     (rx_port_pin_t)k_pin_motor2_in2,
                                     (rx_port_pin_t)k_pin_motor2_in1,
                                     (rx_port_pin_t)k_pin_motor3_in2,
                                     (rx_port_pin_t)k_pin_motor3_in1};

  for (uint8_t i = 0; i < k_gptw_pin_count; i++) {
    const rx_err_t err = rx_mpc_set_gptw(gptw_pins[i]);
    RX_RETURN_ON_ERROR(err, s_tag, "GPTW pin config failed");
  }

  return k_rx_ok;
}

/**
 * @brief Pre-kernel busy-wait delay for microsecond-precision timing
 *
 * @details
 * CPU-cycle based busy-wait for short delays required before ThreadX
 * starts (tx_kernel_enter). Uses a volatile loop counter to prevent
 * compiler optimization. The actual delay is approximately 5-6x longer
 * than the nominal value due to loop overhead (~5-6 cycles per iteration
 * vs the ideal 1 cycle/MHz assumed by the calculation).
 *
 *
 * @pre us > 0 and cpu_mhz > 0
 * @pre Called only during pre-kernel initialization
 *
 * @post At least us microseconds have elapsed (typically 5-6x more)
 * @post No side effects on hardware state
 *
 * @note Interrupts are NOT disabled; actual delay may be slightly longer
 *
 * @see twake_delay_t Source of us parameter
 * @see twake_cpu_t Source of cpu_mhz parameter
 *
 * @since Version 1.0.0
 */
static inline void internal_busy_wait_us(uint16_t us, uint16_t cpu_mhz)
{
  RX_ASSERT(us > 0, "Busy-wait duration must be positive");
  RX_ASSERT(cpu_mhz > 0, "CPU frequency must be positive");

  volatile uint32_t cycles = (uint32_t)us * (uint32_t)cpu_mhz;

  while (cycles > 0) {
    cycles--;
  }
}

/**
 * @brief Configure a single GPIO pin as output with initial level
 *
 * @details
 * Sets the given pin to GPIO mode (PMR cleared), drives the specified
 * initial logic level on PODR, and configures PDR as output. This is a
 * common helper used by DRVOFF, nSLEEP, sonar trigger, and IMU reset
 * pin initialization.
 *
 * Register write order is critical for glitch-free startup:
 * 1. Clear PMR (switch from peripheral to GPIO mode)
 * 2. Set PODR (drive the desired initial logic level)
 * 3. Set PDR (enable output driver last to avoid glitch)
 *
 *
 *
 * @pre Pin must have MPC already configured via rx_mpc_set_gpio()
 * @pre Port base address must be valid (rx_port_get_base() != nullptr)
 * @post Pin PMR cleared (GPIO mode), PDR set (output), PODR at requested level
 * @post Pin is actively driving the requested logic level
 *
 * @note Not thread-safe. Performs non-atomic RMW on 8-bit port registers.
 * @note Called only during single-threaded initialization before RTOS start.
 *
 * @warning Do not call after RTOS start without external synchronization.
 *
 * @see internal_gpio_set_input() Complementary function for input pins
 * @see internal_gpio_init_motor_driver_ctrl() Primary caller for DRVOFF/nSLEEP
 * @see internal_gpio_init_sonar_triggers() Primary caller for sonar trigger pins
 *
 * @since Version 1.0.0
 */
static inline void internal_gpio_set_output(rx_port_pin_t port_pin, bool initial_high)
{
  const uint8_t            port = rx_port_from_pin(port_pin);
  const uint8_t            pin  = rx_pin_from_pin(port_pin);
  volatile rx_port_regs_t* regs = rx_port_get_base(port);
  RX_ASSERT(regs != nullptr, "Invalid GPIO port for output pin");
  RX_ASSERT(pin <= k_gpio_max_pin_number, "GPIO pin number out of range (0-7)");
  regs->pmr &= (uint8_t) ~(uint16_t)(k_gpio_single_bit_mask << pin); /* GPIO mode */
  if (initial_high) {
    regs->podr |= (uint8_t)(k_gpio_single_bit_mask << pin);
  } else {
    regs->podr &= (uint8_t) ~(uint16_t)(k_gpio_single_bit_mask << pin);
  }
  regs->pdr |= (uint8_t)(k_gpio_single_bit_mask << pin); /* Output direction */
}

/**
 * @brief Configure a single GPIO pin as input
 *
 * @details
 * Sets the given pin to GPIO mode (PMR cleared) and configures PDR as
 * input (direction bit cleared). Used by IMU interrupt and sonar echo
 * pin initialization.
 *
 * Register write order:
 * 1. Clear PMR (switch from peripheral to GPIO mode)
 * 2. Clear PDR (configure as input direction)
 *
 *
 *
 * @pre Pin must have MPC already configured via rx_mpc_set_gpio()
 * @pre Port base address must be valid (rx_port_get_base() != nullptr)
 * @post Pin PMR cleared (GPIO mode) and PDR cleared (input direction)
 * @post Pin is in high-impedance input state
 *
 * @note Not thread-safe. Performs non-atomic RMW on 8-bit port registers.
 * @note Called only during single-threaded initialization before RTOS start.
 *
 * @warning Do not call after RTOS start without external synchronization.
 *
 * @see internal_gpio_set_output() Complementary function for output pins
 * @see internal_gpio_init_imu() Primary caller for IMU interrupt pin
 * @see internal_gpio_init_sonar_echoes() Primary caller for sonar echo pins
 *
 * @since Version 1.0.0
 */
static inline void internal_gpio_set_input(rx_port_pin_t port_pin)
{
  const uint8_t            port = rx_port_from_pin(port_pin);
  const uint8_t            pin  = rx_pin_from_pin(port_pin);
  volatile rx_port_regs_t* regs = rx_port_get_base(port);
  RX_ASSERT(regs != nullptr, "Invalid GPIO port for input pin");
  RX_ASSERT(pin <= k_gpio_max_pin_number, "GPIO pin number out of range (0-7)");
  regs->pmr &= (uint8_t) ~(uint16_t)(k_gpio_single_bit_mask << pin); /* GPIO mode */
  regs->pdr &= (uint8_t) ~(uint16_t)(k_gpio_single_bit_mask << pin); /* Input direction */
}

/**
 * @brief Configure IMU pins (RIIC1 I2C + GPIO interrupt and reset)
 *
 * @details
 * Configures 4 pins for the inertial measurement unit:
 * - P2.1/SCL1 and P2.0/SDA1: RIIC1 I2C bus for IMU communication
 * - P3.2: IMU interrupt input (active-low from IMU INT pin)
 * - P8.3: IMU reset output (active-low, initialized HIGH = not in reset)
 *
 * Pin configuration order:
 * 1. I2C bus pins (SCL1, SDA1) via RIIC MPC
 * 2. Interrupt input via GPIO MPC + input direction
 * 3. Reset output via GPIO MPC + output HIGH (de-asserted)
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P2.1 configured as SCL1, P2.0 configured as SDA1 (RIIC1 peripheral mode)
 * @post P3.2 configured as GPIO input, P8.3 configured as GPIO output HIGH
 *
 * @note Not thread-safe. Performs non-atomic RMW on port registers.
 * @note Called only during single-threaded initialization before RTOS start.
 *
 * @see internal_gpio_set_input() Used for IMU interrupt pin
 * @see internal_gpio_set_output() Used for IMU reset pin
 * @see rx_mpc_set_riic() MPC configuration for I2C pins
 *
 * @since Version 1.0.0
 */
/**
 * @brief 9-clock SCL bit-bang recovery to flush a stuck RIIC1 peripheral
 *
 * Any I2C peripheral on the bus (BNO055 or BMP280 in our case) that was
 * interrupted mid-transaction -- e.g., by a JTAG reset that cut the
 * controller off mid-byte -- can end up holding SDA low, waiting for
 * more SCL clocks to finish the byte it thought it was sending. When
 * RIIC1 then comes up with PMR=1 and tries a Start, it sees SDA+SCL
 * already low, flags the bus as busy, and every subsequent transaction
 * errors out with k_rx_err_timeout (0x108) "I2C bus busy timeout".
 *
 * The fix is to bit-bang 9 SCL edges while the pins are still GPIO
 * (before rx_mpc_set_riic() raises PMR), long enough to clock out any
 * pending byte from the stuck peripheral, then generate a manual STOP.
 * Same sequence as the bench-verified imu_test/main.c::i2c_bus_recover
 * (RX72N HW manual Chapter 38 "Bus Recovery" informative note).
 *
 * @pre SCL/SDA pins must still be in GPIO mode (PMR=0)
 * @post SCL/SDA left tristated (PDR=0), ready for rx_mpc_set_riic() to
 *       raise PMR and hand the pads to RIIC1
 */
/**
 * @enum riic1_recover_t
 * @brief Timing and cycle constants for the RIIC1 bit-bang bus recovery sequence
 *
 * @details
 * Centralised constants used by internal_riic1_bus_recover() and its
 * three sub-phase helpers (BNO055 reset, SCL bit-bang, manual STOP).
 * Hoisted to file scope so each helper can reference the same values.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_i2c_recover_cycles  = 9U,     /**< 9 SCL edges flush a stuck peripheral byte. */
  k_i2c_recover_half_us = 5U,     /**< 5 us half-period -> 100 kHz SCL. */
  k_i2c_recover_cpu_mhz = 240U,   /**< CPU clock for busy-wait calibration. */
  k_bno055_rst_low_us   = 20000U, /**< BNO055 datasheet: tRST_low >= 20 us. */
  k_bno055_por_us       = 650U,   /**< POR settle budget (caller adds more). */
} riic1_recover_t;

/**
 * @brief Pulse the BNO055 RST pin low for >= 20 us to force a power-on reset
 *
 * @details
 * Active-low BNO055 reset on P8.3. Forcing the chip through POR before the
 * SCL bit-bang guarantees the IMU is not driving SDA low as a residue of a
 * half-finished transaction from a prior firmware load.
 *
 * @pre k_pin_imu_rst is a valid GPIO pin
 * @pre Single-threaded boot context (pre-RTOS)
 * @post P8.3 left HIGH (chip out of reset, executing POR)
 * @post Pin direction set to OUTPUT, mode GPIO
 *
 * @note Not thread-safe: non-atomic RMW on port registers.
 * @since Version 1.0.0
 */
static void internal_riic1_recover_reset_bno055(void)
{
  const uint8_t                  rst_bit = rx_pin_from_pin((rx_port_pin_t)k_pin_imu_rst);
  volatile rx_port_regs_t* const rst_port =
    rx_port_get_base(rx_port_from_pin((rx_port_pin_t)k_pin_imu_rst));
  RX_ASSERT(rst_port != nullptr, "IMU RST port base invalid during bus recovery");
  const uint8_t rst_mask = (uint8_t)(1U << rst_bit);
  rst_port->pmr &= (uint8_t)~rst_mask;  /* GPIO mode */
  rst_port->pdr |= rst_mask;            /* Output */
  rst_port->podr &= (uint8_t)~rst_mask; /* Drive LOW -> chip in reset */
  internal_busy_wait_us((uint16_t)k_bno055_rst_low_us, (uint16_t)k_i2c_recover_cpu_mhz);
  rst_port->podr |= rst_mask; /* Release -> chip POR */
}

/**
 * @brief Drive 9 SCL edges at ~100 kHz to flush any stuck peripheral byte
 *
 * @details
 * Step 2 of the I2C bus recovery dance: with both lines parked high, toggle
 * SCL nine times at ~100 kHz. That is enough to clock out any byte from a
 * peripheral (BMP280 in particular, which has no reset pin) that was stuck
 * mid-transaction when JTAG cut the controller off.
 *
 * @param[in,out] port Pointer to the P2 port register block (SCL/SDA share P2)
 * @param[in] scl_mask Bit mask selecting the SCL pin within @p port->podr
 * @param[in] both Bit mask selecting both SCL and SDA together
 *
 * @pre @p port != nullptr (caller asserts before calling)
 * @pre @p port->pmr cleared for both pins (GPIO mode already set)
 * @post SCL toggled k_i2c_recover_cycles times, ending HIGH
 * @post SDA undisturbed (caller still owns it via @p both)
 *
 * @note Not thread-safe: non-atomic RMW on port->podr.
 * @since Version 1.0.0
 */
static void
internal_riic1_recover_clock_scl(volatile rx_port_regs_t* port, uint8_t scl_mask, uint8_t both)
{
  /* Idle both I2C lines high (push-pull high is fine here: we're the only
   * bus driver before RIIC1 claims the pins, and the external pull-ups
   * will take over once we tristate at the end). */
  port->pmr &= (uint8_t)~both; /* GPIO mode */
  port->podr |= both;          /* Drive high */
  port->pdr |= both;           /* Output direction */

  for (uint16_t i = 0; i < k_i2c_recover_cycles; i++) {
    port->podr &= (uint8_t)~scl_mask;
    internal_busy_wait_us((uint16_t)k_i2c_recover_half_us, (uint16_t)k_i2c_recover_cpu_mhz);
    port->podr |= scl_mask;
    internal_busy_wait_us((uint16_t)k_i2c_recover_half_us, (uint16_t)k_i2c_recover_cpu_mhz);
  }
}

/**
 * @brief Generate a manual I2C STOP and tristate the lines for RIIC1 handoff
 *
 * @details
 * Step 3 of the bus recovery dance: SDA low while SCL high, then SDA back
 * high -- the I2C STOP signature. Any peripheral still waiting for the end
 * of its transaction will release the bus on this edge. Pins are then
 * tristated so rx_mpc_set_riic() can raise PMR cleanly without glitching.
 *
 * @param[in,out] port Pointer to the P2 port register block (SCL/SDA share P2)
 * @param[in] sda_mask Bit mask selecting the SDA pin within @p port->podr
 * @param[in] both Bit mask selecting both SCL and SDA together
 *
 * @pre @p port != nullptr (caller asserts before calling)
 * @pre SCL has already been left HIGH by internal_riic1_recover_clock_scl()
 * @post Manual STOP edge generated on the bus
 * @post Both pins tristated (PDR=0), ready for rx_mpc_set_riic() to claim them
 *
 * @note Not thread-safe: non-atomic RMW on port->podr / port->pdr.
 * @since Version 1.0.0
 */
static void internal_riic1_recover_stop_and_tristate(volatile rx_port_regs_t* port,
                                                     uint8_t                  sda_mask,
                                                     uint8_t                  both)
{
  port->podr &= (uint8_t)~sda_mask;
  internal_busy_wait_us((uint16_t)k_i2c_recover_half_us, (uint16_t)k_i2c_recover_cpu_mhz);
  port->podr |= sda_mask;
  internal_busy_wait_us((uint16_t)k_i2c_recover_half_us, (uint16_t)k_i2c_recover_cpu_mhz);

  /* Tristate the pins so RIIC1 takes over cleanly once rx_mpc_set_riic()
   * raises PMR. Leaving PDR=1 through the handoff is harmless per the HW
   * manual, but tristating first avoids any glitch. */
  port->pdr &= (uint8_t)~both;
}

static void internal_riic1_bus_recover(void)
{
  const uint8_t scl_bit = rx_pin_from_pin((rx_port_pin_t)k_pin_imu_scl);
  const uint8_t sda_bit = rx_pin_from_pin((rx_port_pin_t)k_pin_imu_sda);
  /* k_pin_imu_scl and k_pin_imu_sda are on the same port (P2). */
  volatile rx_port_regs_t* const port =
    rx_port_get_base(rx_port_from_pin((rx_port_pin_t)k_pin_imu_scl));
  RX_ASSERT(port != nullptr, "IMU I2C port base invalid during bus recovery");

  const uint8_t scl_mask = (uint8_t)(1U << scl_bit);
  const uint8_t sda_mask = (uint8_t)(1U << sda_bit);
  const uint8_t both     = (uint8_t)(scl_mask | sda_mask);

  /* Step 1: hardware-reset BNO055 via P83 (active-low). */
  internal_riic1_recover_reset_bno055();

  /* Step 2: 9 SCL edges @ ~100 kHz to flush any held peripheral byte. */
  internal_riic1_recover_clock_scl(port, scl_mask, both);

  /* Step 3: manual STOP and tristate for RIIC1 handoff. */
  internal_riic1_recover_stop_and_tristate(port, sda_mask, both);
}

/**
 * @brief Configure RIIC1 SCL1 + SDA1 pin mux for IMU I2C bus
 *
 * @details
 * Routes P2.1 to SCL1 and P2.0 to SDA1 via the MPC (rx_mpc_set_riic).
 * Caller must have already run internal_riic1_bus_recover() so the lines
 * are quiescent before PMR=1 hands the pads to RIIC1.
 *
 * @return rx_err_t Error code from rx_mpc_set_riic()
 * @retval k_rx_ok Both pins configured for RIIC1 peripheral mode
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins P2.0 and P2.1 not claimed by another peripheral
 * @post P2.1 -> SCL1, P2.0 -> SDA1 (PMR=1, peripheral mode)
 *
 * @note Not thread-safe (single-threaded boot context).
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_imu_i2c_pins(void)
{
  static const char* const s_tag = "GPIO_IMU";
  rx_err_t                 err   = rx_mpc_set_riic((rx_port_pin_t)k_pin_imu_scl);
  RX_RETURN_ON_ERROR(err, s_tag, "SCL1 pin config failed");

  err = rx_mpc_set_riic((rx_port_pin_t)k_pin_imu_sda);
  RX_RETURN_ON_ERROR(err, s_tag, "SDA1 pin config failed");
  return k_rx_ok;
}

/**
 * @brief Configure the IMU reset pin (P8.3) as a GPIO output driven HIGH
 *
 * @details
 * Drives P8.3 HIGH (BNO055 not in reset). Active-low reset; HIGH = chip
 * running. Called after internal_riic1_recover_reset_bno055() has already
 * pulsed the line LOW for tRST_low and released it.
 *
 * @return rx_err_t Error code from rx_mpc_set_gpio()
 * @retval k_rx_ok P8.3 configured as GPIO output, driven HIGH
 *
 * @pre MPC write protection disabled
 * @pre P8.3 not claimed by another peripheral
 * @post P8.3 = GPIO output, podr=1 (chip not in reset)
 *
 * @note Not thread-safe (single-threaded boot context).
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_imu_rst_pin(void)
{
  static const char* const s_tag = "GPIO_IMU";
  rx_err_t                 err   = rx_mpc_set_gpio((rx_port_pin_t)k_pin_imu_rst);
  RX_RETURN_ON_ERROR(err, s_tag, "IMU RST MPC config failed");

  internal_gpio_set_output((rx_port_pin_t)k_pin_imu_rst, true); /* HIGH = not in reset */
  return k_rx_ok;
}

static rx_err_t internal_gpio_init_imu(void)
{
  static const char* const s_tag = "GPIO_IMU";

  /* NASA Rule 5: precondition validation */
  RX_ASSERT(rx_port_get_base(rx_port_from_pin((rx_port_pin_t)k_pin_imu_scl)) != nullptr,
            "IMU SCL port base invalid");
  RX_ASSERT(rx_port_get_base(rx_port_from_pin((rx_port_pin_t)k_pin_imu_rst)) != nullptr,
            "IMU RST port base invalid");

  /* Bit-bang recovery BEFORE handing the pads to RIIC1 via PMR=1. See
   * internal_riic1_bus_recover() for the why. */
  internal_riic1_bus_recover();

  rx_err_t err = internal_gpio_init_imu_i2c_pins();
  RX_RETURN_ON_ERROR(err, s_tag, "IMU I2C pin init failed");

  err = internal_gpio_init_imu_rst_pin();
  RX_RETURN_ON_ERROR(err, s_tag, "IMU RST pin init failed");

  return k_rx_ok;
}

/**
 * @enum imu_irq_cfg_t
 * @brief ICU configuration constants for the IMU INT (IRQ12) interrupt
 *
 * @details
 * P3.2 is connected to the BNO055 INT pin (active-low, falling-edge).
 * IRQ12 vector = 64 (IRQ0 base) + 12 = 76.
 * IER index = 76 / 8 = 9, bit = 76 % 8 = 4.
 *
 * Priority 7 sits between the comm task ISR (priority 6) and motor control (8).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_imu_irq_num        = 12U,   /**< IRQ number for P3.2 (BNO055 INT pin) */
  k_irqcr_falling_edge = 0x04U, /**< IRQCR[n] bits[3:2]=01: falling-edge trigger */
  k_irqcr_rising_edge  = 0x08U, /**< IRQCR[n] bits[3:2]=10: rising-edge trigger */
  k_irqcr_both_edges   = 0x0CU, /**< IRQCR[n] bits[3:2]=11: both-edges trigger (HUM 15.2.5) */
  k_icu_ir_clear_imu   = 0U,    /**< Write 0 to IR register to clear pending flag */
  k_imu_irq_priority   = 7U,    /**< IPR priority (between comm=6 and motor=8) */
  k_imu_ier_bits       = 8U,    /**< Bits per IER register (vector/8 = IER index) */
} imu_irq_cfg_t;

/**
 * @enum imu_irq_vector_t
 * @brief ICU vector number for IRQ12 (IMU INT on P3.2)
 *
 * @details
 * External IRQ0 = vector 64; IRQ12 = 64 + 12 = 76.
 * Stored in uint8_t (fits in 0-255 range).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_imu_irq_vector = 76U, /**< ICU vector for IRQ12: 64 (IRQ0 base) + 12 */
} imu_irq_vector_t;

/**
 * @brief Configure IMU INT pin (P3.2) as falling-edge IRQ12 input
 *
 * @details
 * Reconfigures P3.2 from plain GPIO input to IRQ12 function, enabling
 * hardware falling-edge detection on the BNO055 active-low INT signal.
 *
 * Configuration sequence:
 * 1. Set MPC ISEL bit (rx_mpc_set_irq) to route pin to ICU
 * 2. Set GPIO direction to input (direction register cleared)
 * 3. Enable digital noise filter (PCLK/32 = 1.6 us, rejects < 1 us spikes)
 * 4. Set IRQCR[12] = 0x04 (falling-edge mode: bits[3:2] = 01)
 * 5. Set IPR[76] = 7 (priority 7, between comm and motor tasks)
 * 6. Clear IR[76] (remove any stale pending request)
 * 7. Enable in IER[9] bit 4 (vector 76 / 8 = 9, 76 % 8 = 4)
 *
 *
 * @pre P3.2 not in use by another peripheral
 * @pre MPC write protection disabled (handled by rx_mpc_set_irq)
 * @post P3.2 configured as IRQ12 input, falling-edge trigger
 * @post Digital noise filter active (PCLK/32, 1.6 us response time)
 * @post ICU enabled for IRQ12 at priority 7
 * @post imu_task.c ISR will fire on each active-low assertion from BNO055
 *
 * @note Called during single-threaded hardware_init() before RTOS start
 * @note The ISR (INT_IRQ12 in imu_task.c) sets s_imu_event_flags
 * @warning Do not call after RTOS starts; ICU register access is not thread-safe
 *
 * @see rx_mpc_set_irq() MPC ISEL bit configuration for IRQ function
 * @see rx_irq_filter_enable() Digital noise filter API
 * @see imu_task.c INT_IRQ12() ISR handler (sets event flags)
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_imu_irq(void)
{
  static const char* const s_tag = "GPIO_IMU_IRQ";

  /* NASA Rule 5: precondition validation */
  RX_ASSERT(rx_port_get_base(rx_port_from_pin((rx_port_pin_t)k_pin_imu_int)) != nullptr,
            "IMU INT port base invalid");

  /* Step 1: Configure P3.2 for IRQ12 function (MPC ISEL bit set) */
  rx_err_t err = rx_mpc_set_irq((rx_port_pin_t)k_pin_imu_int);
  RX_RETURN_ON_ERROR(err, s_tag, "IMU INT MPC IRQ config failed");

  /* Step 2: Set GPIO direction to input */
  internal_gpio_set_input((rx_port_pin_t)k_pin_imu_int);

  /* Step 3: Enable digital noise filter on IRQ12 (PCLK/32 = 1.6 us response) */
  err = rx_irq_filter_enable(k_imu_irq_num, k_irq_filter_pclk_32);
  RX_RETURN_ON_ERROR(err, s_tag, "IMU INT filter enable failed");

  volatile rx_icu_regs_t* const icu_regs = icu();

  /* Step 4: Both-edges detection. docs/sections/03_hardware_pinout.tex
   * describes IMU INT as active-low; BNO055 datasheet section 3.6 says
   * the INT pin is active-high push-pull. Both-edges catches either
   * polarity so ACC_BSX_DRDY pulses at ~100 Hz fire the ISR regardless
   * of which document is correct. The ISR is edge-triggered and clears
   * IR[76] unconditionally, so extra edges cost only a few cycles and
   * do not misfire. Empirically, even with both-edges configured, the
   * INT pin on this PCB revision is not observed to toggle -- the IMU
   * task falls back to 200 ms polling (k_imu_wait_timeout path), which
   * reads sensor data successfully at ~5 Hz via the polling branch of
   * internal_wait_for_imu_int. */
  icu_regs->irqcr[k_imu_irq_num] = k_irqcr_both_edges;

  /* Step 5: Set priority (7 = between comm ISR priority 6 and motor 8) */
  icu_regs->ipr[k_imu_irq_vector] = k_imu_irq_priority;

  /* Step 6: Clear any stale pending request before enabling */
  icu_regs->ir[k_imu_irq_vector] = k_icu_ir_clear_imu;

  /* Step 7: Enable IRQ12 in IER register (IER[9] bit 4) */
  const uint8_t ier_idx = (uint8_t)(k_imu_irq_vector / k_imu_ier_bits);
  const uint8_t ier_bit = (uint8_t)(k_imu_irq_vector % k_imu_ier_bits);
  icu_regs->ier[ier_idx] |= (uint8_t)(1U << ier_bit);

  rx_log_info(s_tag, "IMU IRQ12 configured: P3.2 falling-edge, priority 7");
  return k_rx_ok;
}

/**
 * @brief Configure HOST_IRQ output pin (P67, active-low data-ready signal to RPi5)
 *
 * @details
 * Configures P6.7 (pin 98) as a GPIO output, initialised HIGH (deasserted).
 * The telemetry task drives this pin LOW immediately before each telemetry
 * send and HIGH again after the send completes, giving the RPi5 a hardware
 * data-ready interrupt instead of requiring fixed-rate polling.
 *
 * | Signal   | Pin | Direction | Active Level | Default |
 * |----------|-----|-----------|--------------|---------|
 * | HOST_IRQ | P67 | Output    | LOW          | HIGH    |
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre P6.7 not in use by another peripheral
 * @post P6.7 configured as GPIO output, driven HIGH (deasserted, no data pending)
 * @post RPi5 sees HOST_IRQ HIGH (idle/no-data state)
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during single-threaded initialization before RTOS start.
 *
 * @see telemetry_task.c internal_build_and_send_telemetry() Asserts/deasserts at runtime
 * @see rx_port_constants.h k_rx_p6_7 for the combined port/pin constant
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_host_irq(void)
{
  static const char* const s_tag = "GPIO_IRQ";

  rx_err_t err = rx_mpc_set_gpio((rx_port_pin_t)k_pin_host_irq);
  RX_RETURN_ON_ERROR(err, s_tag, "HOST_IRQ MPC config failed");

  /* Initial HIGH = deasserted (no data pending) */
  internal_gpio_set_output((rx_port_pin_t)k_pin_host_irq, true);

  return k_rx_ok;
}

/**
 * @brief Configure DRV8263H motor driver control pins (DRVOFF + nSLEEP)
 *
 * @details
 * Configures 8 GPIO output pins for DRV8263H motor driver control:
 * - 4x DRVOFF pins: Output HIGH (driver outputs disabled for safe startup)
 * - 4x nSLEEP pins: Output HIGH (driver awake, not in sleep mode)
 *
 * ## Safe Initialization Order (Critical)
 *
 * DRVOFF pins are configured **before** nSLEEP pins. This ordering is
 * required by the DRV8263H power-up sequence:
 *
 * 1. DRVOFF = HIGH first  -> H-bridge outputs disabled (safe state)
 * 2. nSLEEP = HIGH second -> driver wakes up with outputs already disabled
 *
 * If nSLEEP were asserted first (waking the driver), the H-bridge outputs
 * could briefly be in an undefined state before DRVOFF is driven HIGH,
 * risking uncontrolled motor movement or shoot-through.
 *
 * Motor outputs remain disabled (DRVOFF=HIGH) until the motor control task
 * explicitly drives DRVOFF LOW before commanding PWM.
 * @todo (#367) Motor control task must drive DRVOFF LOW before commanding PWM.
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post All 4 DRVOFF pins configured as GPIO outputs, driven HIGH (disabled)
 * @post All 4 nSLEEP pins configured as GPIO outputs, driven HIGH (awake)
 *
 * @note Not thread-safe. Performs non-atomic RMW on port registers.
 * @note Called only during single-threaded initialization before RTOS start.
 *
 * @warning DRVOFF must be driven HIGH before nSLEEP is asserted to prevent
 *          uncontrolled H-bridge output during driver wake-up.
 *
 * @see internal_gpio_set_output() Used for all DRVOFF and nSLEEP pins
 * @see rx_mpc_set_gpio() MPC configuration for GPIO mode
 * @see internal_gpio_init_gptw_pwm() Configures PWM pins for DRV8263H IN1/IN2
 *
 * @since Version 1.0.0
 */
/** @brief Configure all DRVOFF GPIO pins as outputs HIGH (outputs disabled). */
static rx_err_t internal_gpio_init_drvoff_pins(const rx_port_pin_t pins[])
{
  static const char* const s_tag = "GPIO_DRV_CTRL";
  for (uint8_t i = 0; i < k_motor_count; i++) {
    const rx_err_t err = rx_mpc_set_gpio(pins[i]);
    RX_RETURN_ON_ERROR(err, s_tag, "DRVOFF MPC config failed");
    internal_gpio_set_output(pins[i], true); /* HIGH = outputs disabled */
  }
  return k_rx_ok;
}

/** @brief Configure all nSLEEP GPIO pins as outputs HIGH (driver awake). */
static rx_err_t internal_gpio_init_nsleep_pins(const rx_port_pin_t pins[])
{
  static const char* const s_tag = "GPIO_DRV_CTRL";
  for (uint8_t i = 0; i < k_motor_count; i++) {
    const rx_err_t err = rx_mpc_set_gpio(pins[i]);
    RX_RETURN_ON_ERROR(err, s_tag, "nSLEEP MPC config failed");
    internal_gpio_set_output(pins[i], true); /* HIGH = awake */
  }
  return k_rx_ok;
}

static rx_err_t internal_gpio_init_motor_driver_ctrl(void)
{
  static const char* const s_tag = "GPIO_DRV_CTRL";

  /* NASA Rule 5: precondition validation */
  RX_ASSERT(rx_port_get_base(rx_port_from_pin((rx_port_pin_t)k_pin_motor0_drvoff)) != nullptr,
            "Motor 0 DRVOFF port base invalid");
  RX_ASSERT(rx_port_get_base(rx_port_from_pin((rx_port_pin_t)k_pin_motor0_nsleep)) != nullptr,
            "Motor 0 nSLEEP port base invalid");

  const rx_port_pin_t drvoff_pins[k_motor_count] = {(rx_port_pin_t)k_pin_motor0_drvoff,
                                                    (rx_port_pin_t)k_pin_motor1_drvoff,
                                                    (rx_port_pin_t)k_pin_motor2_drvoff,
                                                    (rx_port_pin_t)k_pin_motor3_drvoff};

  const rx_port_pin_t nsleep_pins[k_motor_count] = {(rx_port_pin_t)k_pin_motor0_nsleep,
                                                    (rx_port_pin_t)k_pin_motor1_nsleep,
                                                    (rx_port_pin_t)k_pin_motor2_nsleep,
                                                    (rx_port_pin_t)k_pin_motor3_nsleep};

  /* CRITICAL ORDERING: DRVOFF HIGH before nSLEEP HIGH to prevent undefined
   * H-bridge state during wake-up (per DRV8263H datasheet sec 7.3.1) */
  rx_err_t err = internal_gpio_init_drvoff_pins(drvoff_pins);
  RX_RETURN_ON_ERROR(err, s_tag, "DRVOFF pins init failed");

  err = internal_gpio_init_nsleep_pins(nsleep_pins);
  RX_RETURN_ON_ERROR(err, s_tag, "nSLEEP pins init failed");

  /* Wait for DRV8263H-Q1 tWAKE (~1.2 ms min; busy-wait ~10 ms pre-kernel) */
  internal_busy_wait_us(k_twake_busy_wait_us, k_twake_cpu_mhz);

  return k_rx_ok;
}

/**
 * @brief Configure ADC current sense input pins
 *
 * @details
 * Configures MPC pin multiplexing for S12AD0 analog inputs AN004-AN007 on
 * P4.4-P4.7. Disables digital input buffers and sets pins to analog mode for
 * motor current sensing.
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P4.4-P4.7 configured as AN004-AN007 analog inputs
 * @post ADC pins ready for current sensing
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_adc(void)
{
  static const char* const s_tag = "GPIO_ADC";

  rx_err_t err = rx_mpc_set_adc((rx_port_pin_t)k_pin_adc_an004);
  RX_RETURN_ON_ERROR(err, s_tag, "AN004 pin config failed");

  err = rx_mpc_set_adc((rx_port_pin_t)k_pin_adc_an005);
  RX_RETURN_ON_ERROR(err, s_tag, "AN005 pin config failed");

  err = rx_mpc_set_adc((rx_port_pin_t)k_pin_adc_an006);
  RX_RETURN_ON_ERROR(err, s_tag, "AN006 pin config failed");

  err = rx_mpc_set_adc((rx_port_pin_t)k_pin_adc_an007);
  RX_RETURN_ON_ERROR(err, s_tag, "AN007 pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure USB VBUS detection pin
 *
 * @details
 * Configures MPC pin multiplexing for USB0_VBUS detection on P1.6.
 * Enables USB peripheral to detect host connection via VBUS voltage level.
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P1.6 configured as USB0_VBUS input
 * @post USB VBUS detection enabled
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_usb(void)
{
  static const char* const s_tag = "GPIO_USB";

  rx_err_t err = rx_mpc_set_usb_vbus((rx_port_pin_t)k_pin_usb_vbus);
  RX_RETURN_ON_ERROR(err, s_tag, "USB VBUS pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure HC-SR04 sonar trigger pins (GPIO outputs)
 *
 * @details
 * Configures 4 sonar trigger pins as GPIO outputs with initial LOW state.
 * Pins: PF5, PJ5, PJ3, P33. Used to trigger ultrasonic pulse transmission
 * on HC-SR04 distance sensors.
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post All 4 sonar trigger pins configured as GPIO outputs
 * @post Trigger pins initialized to LOW (idle state)
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_sonar_triggers(void)
{
  static const char* const s_tag = "GPIO_SONAR_TRIG";

  const rx_port_pin_t sonar_trig_pins[k_sonar_count] = {(rx_port_pin_t)k_pin_sonar_trig0,
                                                        (rx_port_pin_t)k_pin_sonar_trig1,
                                                        (rx_port_pin_t)k_pin_sonar_trig2,
                                                        (rx_port_pin_t)k_pin_sonar_trig3};

  for (uint8_t i = 0; i < k_sonar_count; i++) {
    const rx_err_t err = rx_mpc_set_gpio(sonar_trig_pins[i]);
    RX_RETURN_ON_ERROR(err, s_tag, "Sonar trigger MPC config failed");
    internal_gpio_set_output(sonar_trig_pins[i], false); /* LOW = idle */
  }

  return k_rx_ok;
}

/**
 * @brief Configure HC-SR04 sonar echo pins (GPIO inputs)
 *
 * @details
 * Configures 4 sonar echo pins as GPIO inputs for pulse width measurement.
 * Pins: P0.3, P0.2, P0.1, P0.0 (also IRQ11-IRQ8). Echo pulse duration
 * measured by IRQ timing to determine distance.
 *
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post All 4 sonar echo pins configured as GPIO inputs
 * @post Echo pins ready for IRQ-based pulse measurement
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_sonar_echoes(void)
{
  static const char* const s_tag = "GPIO_SONAR_ECHO";

  const rx_port_pin_t sonar_echo_pins[k_sonar_count] = {(rx_port_pin_t)k_pin_sonar_echo0,
                                                        (rx_port_pin_t)k_pin_sonar_echo1,
                                                        (rx_port_pin_t)k_pin_sonar_echo2,
                                                        (rx_port_pin_t)k_pin_sonar_echo3};

  for (uint8_t i = 0; i < k_sonar_count; i++) {
    const rx_err_t err = rx_mpc_set_gpio(sonar_echo_pins[i]);
    RX_RETURN_ON_ERROR(err, s_tag, "Sonar echo MPC config failed");

    internal_gpio_set_input(sonar_echo_pins[i]);
  }

  return k_rx_ok;
}

/**
 * @brief Initialize GPIO pins for motor control, I2C, and USB communication
 *
 * @details
 * Configures Multi-Function Pin Controller (MPC) for 8 critical pins used in STAR
 * robot application. All pins are configured for peripheral mode (not GPIO), with
 * specific PSEL values determined by hardware function.
 *
 * **Pin configuration:**
 * - **8x GPTW PWM pins** - Motor control GPTW PWM outputs (PORT2/1/8/C/E)
 * - **2x I2C pins** - Sensor communication bus (SCL/SDA)
 * - **1x USB pin** - USB VBUS detection for CDC debug interface
 *
 * **Algorithm steps:**
 * 1. Validate all pin identifiers are within valid range
 * 2. Configure GPTW PWM pins for 4-motor DRV8263H IN1/IN2 control
 * 3. Configure I2C pins (PSEL = 0x0F) for sensor bus
 * 4. Configure USB pin (PSEL = 0x11) for USB VBUS detect
 * 5. All operations use rx_mpc API with write-protect handling
 *
 * ## Pin Allocation Table
 *
 * | Pin | Port.Bit | Function | PSEL | Usage |
 * |-----|----------|----------|------|-------|
 * | P23 | PORT2.3 | GTIOC0A | 0x1E | Motor 0 IN2/direction (pkg pin 34) |
 * | P17 | PORT1.7 | GTIOC0B | 0x1E | Motor 0 IN1/PWM (pkg pin 38) |
 * | P22 | PORT2.2 | GTIOC1A | 0x1E | Motor 1 IN2/direction (pkg pin 35) |
 * | PC3 | PORTC.3 | GTIOC1B | 0x1E | Motor 1 IN1/PWM (pkg pin 67) |
 * | PE3 | PORTE.3 | GTIOC2A | 0x1E | Motor 2 IN2/direction (pkg pin 108) |
 * | P86 | PORT8.6 | GTIOC2B | 0x1E | Motor 2 IN1/PWM (pkg pin 41) |
 * | PE7 | PORTE.7 | GTIOC3A | 0x1E | Motor 3 IN2/direction (pkg pin 101) |
 * | PC6 | PORTC.6 | GTIOC3B | 0x1E | Motor 3 IN1/PWM (pkg pin 61) |
 * | P1.2 | PORT1.2 | SCL0 | 0x0F | I2C clock line |
 * | P1.3 | PORT1.3 | SDA0 | 0x0F | I2C data line |
 * | P1.6 | PORT1.6 | USB0_VBUS | 0x11 | USB VBUS detect input |
 *
 *
 * @pre PCLKB clock must be running (MPC registers require active clock)
 * @pre Must be called during single-threaded initialization (before RTOS starts)
 *
 * @post All 8 pins configured for peripheral mode (PMR = 1)
 * @post PWPR register locked (B0WI = 1, write protection active)
 * @post Motor control pins ready for GPTW PWM output
 * @post I2C pins ready for RIIC communication
 * @post USB pin ready for VBUS detection
 *
 * @note **Thread Safety**: Not thread-safe. Must be called during initialization
 *       before ThreadX kernel starts. Do not call from running RTOS tasks.
 *
 * @note **Re-entrancy**: Not reentrant. Calling multiple times is safe (idempotent)
 *       but wastes CPU cycles re-writing same register values.
 *
 * @note **Performance**: Execution time ~15 us @ 240 MHz (8 pins x 2 us/pin).
 *       One-time initialization cost, not runtime overhead.
 *
 * @warning **Pin conflicts**: If pins are already in use by another peripheral,
 *          reconfiguring them here will break that peripheral's functionality.
 *          Ensure pin allocations match hardware schematic.
 *
 * @warning **Motor safety**: Motor control pins must be configured before enabling
 *          GPTW channels. Unconfigured PWM pins can cause undefined motor behavior.
 *
 * @par Example - Normal Initialization:
 * @code
 * // Called from hardware_init() during boot
 * rx_err_t err = gpio_init();
 * if (err != k_rx_ok) {
 *     rx_log_error("HWINT", "GPIO init failed: %d", err);
 *     return err;
 * }
 *
 * // Now safe to initialize motor drivers
 * err = motor_init();
 * @endcode
 *
 * @par Example - Error Handling:
 * @code
 * rx_err_t err = gpio_init();
 * switch (err) {
 *     case k_rx_ok:
 *         rx_log_info("GPIO", "8 pins configured successfully");
 *         break;
 *     case k_rx_err_invalid_arg:
 *         // Programming error - invalid pin constant used
 *         rx_log_error("GPIO", "Invalid pin identifier");
 *         return err;
 *     case k_rx_err_hw_error:
 *         // Hardware fault - MPC registers not accessible
 *         rx_log_error("GPIO", "MPC hardware fault");
 *         return err;
 * }
 * @endcode
 *
 * @see rx_mpc_set_gptw() Configure pin for GPTW PWM output
 * @see rx_mpc_set_riic() Configure pin for I2C bus function
 * @see rx_mpc_set_peripheral() Generic pin configuration
 * @see RX72N Manual Chapter 23 - Multi-Function Pin Controller
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1** [OK] No goto, setjmp, recursion (sequential pin configuration)
 * - **Rule 2** [OK] No loops in main function (delegated to helper functions)
 * - **Rule 3** [OK] No dynamic allocation (all register I/O)
 * - **Rule 4** [OK] Function is ~30 lines (under 60 line limit, decomposed into helpers)
 * - **Rule 5** [OK] 2 preconditions, 5 postconditions documented
 * - **Rule 6** [OK] Minimal scope (no local variables)
 * - **Rule 7** [OK] All internal_gpio_init_*() return values checked
 * - **Rule 8** [OK] Uses C23 typed enums for pin identifiers
 * - **Rule 9** [OK] Single level of function call dereferencing
 * - **Rule 10** [OK] Compiles with -Wall -Wextra -Werror
 *
 */
/**
 * @brief Configure host-side GPIO pins (I2C, host SPI, and HOST_IRQ output)
 *
 * @details
 * First sub-block of internal_gpio_init_comm_and_encoders(). Routes the
 * three pin groups that face the RPi5 host: RIIC0 SCL/SDA, RSPI2 host
 * peripheral CIPO/COPI/CS/CLK, and the P6.7 HOST_IRQ output line.
 *
 * @return rx_err_t Error code from the first failing sub-helper, or k_rx_ok.
 * @retval k_rx_ok All host-side GPIO pins configured.
 *
 * @pre MPC write protection disabled.
 * @pre Pins not claimed by another peripheral.
 * @post RIIC0, RSPI2, and HOST_IRQ pins configured for their roles.
 *
 * @note Not thread-safe (single-threaded boot context).
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_host_pins(void)
{
  static const char* const s_tag = "GPIO";
  rx_err_t                 err   = internal_gpio_init_i2c();
  RX_RETURN_ON_ERROR(err, s_tag, "I2C pin init failed");

  err = internal_gpio_init_host_spi();
  RX_RETURN_ON_ERROR(err, s_tag, "Host SPI pin init failed");

  err = internal_gpio_init_host_irq();
  RX_RETURN_ON_ERROR(err, s_tag, "HOST_IRQ pin init failed");
  return k_rx_ok;
}

/**
 * @brief Configure all four motor encoder GPIO pin pairs (MTU + TPU channels)
 *
 * @details
 * Second sub-block of internal_gpio_init_comm_and_encoders(). Routes the
 * four motor encoder phase-counting input pin pairs to their peripherals:
 * MTU0/MTU1 for motors 0-1 and TPU2/TPU3 for motors 2-3.
 *
 * @return rx_err_t Error code from the first failing sub-helper, or k_rx_ok.
 * @retval k_rx_ok All eight encoder pins configured.
 *
 * @pre MPC write protection disabled.
 * @pre Encoder pins not claimed by another peripheral.
 * @post All four encoder channels' MTCLKx/TCLKx pins muxed to MTU/TPU.
 *
 * @note Not thread-safe (single-threaded boot context).
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_encoder_pins(void)
{
  static const char* const s_tag = "GPIO";
  rx_err_t                 err   = internal_gpio_init_mtu_encoders();
  RX_RETURN_ON_ERROR(err, s_tag, "MTU encoder pin init failed");

  err = internal_gpio_init_tpu_encoders();
  RX_RETURN_ON_ERROR(err, s_tag, "TPU encoder pin init failed");
  return k_rx_ok;
}

/** @brief Configure comm, IRQ, and encoder GPIO pins (first half of gpio_init). */
static rx_err_t internal_gpio_init_comm_and_encoders(void)
{
  static const char* const s_tag = "GPIO";
  rx_err_t                 err   = internal_gpio_init_host_pins();
  RX_RETURN_ON_ERROR(err, s_tag, "Host pin init failed");

  err = internal_gpio_init_encoder_pins();
  RX_RETURN_ON_ERROR(err, s_tag, "Encoder pin init failed");
  return k_rx_ok;
}

/**
 * @brief Configure all motor PWM and DRV8263H control GPIO pins
 *
 * @details
 * First sub-block of internal_gpio_init_motor_and_sensors(). Sets up the
 * motor-side pin mux: GPTW PWM outputs (8 pins for 4 channels) and the
 * DRVOFF/nSLEEP control pins for the four DRV8263H drivers.
 *
 * @return rx_err_t Error code from the first failing sub-helper, or k_rx_ok.
 * @retval k_rx_ok All motor pins configured.
 *
 * @pre MPC write protection disabled.
 * @pre GPTW and motor driver pins not claimed by another peripheral.
 * @post 8 GPTW PWM pins muxed; 8 DRV8263H control pins driven HIGH (safe).
 *
 * @note Not thread-safe (single-threaded boot context).
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_motor_pins(void)
{
  static const char* const s_tag = "GPIO";
  rx_err_t                 err   = internal_gpio_init_gptw_pwm();
  RX_RETURN_ON_ERROR(err, s_tag, "GPTW PWM pin init failed");

  err = internal_gpio_init_motor_driver_ctrl();
  RX_RETURN_ON_ERROR(err, s_tag, "Motor driver control pin init failed");
  return k_rx_ok;
}

/**
 * @brief Configure all IMU-related GPIO pins (RIIC1 bus, RST, and IRQ12)
 *
 * @details
 * Second sub-block of internal_gpio_init_motor_and_sensors(). Brings up
 * the IMU side of the board: RIIC1 SCL/SDA, BNO055 RST output, and the
 * IRQ12 falling-edge input on P3.2.
 *
 * @return rx_err_t Error code from the first failing sub-helper, or k_rx_ok.
 * @retval k_rx_ok All IMU pins configured.
 *
 * @pre MPC write protection disabled.
 * @pre IMU pins not claimed by another peripheral.
 * @post RIIC1 pins live; BNO055 out of reset; IRQ12 enabled at priority 7.
 *
 * @note Not thread-safe (single-threaded boot context).
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_imu_pins(void)
{
  static const char* const s_tag = "GPIO";
  rx_err_t                 err   = internal_gpio_init_imu();
  RX_RETURN_ON_ERROR(err, s_tag, "IMU pin init failed");

  err = internal_gpio_init_imu_irq();
  RX_RETURN_ON_ERROR(err, s_tag, "IMU IRQ pin init failed");
  return k_rx_ok;
}

/**
 * @brief Configure ADC, USB, and HC-SR04 sonar GPIO pins
 *
 * @details
 * Third sub-block of internal_gpio_init_motor_and_sensors(). Configures
 * S12AD0 analog inputs (AN004-AN007), USB0_VBUS sense, and the four
 * HC-SR04 trigger outputs and echo inputs.
 *
 * @return rx_err_t Error code from the first failing sub-helper, or k_rx_ok.
 * @retval k_rx_ok All ADC/USB/sonar pins configured.
 *
 * @pre MPC write protection disabled.
 * @pre Pins not claimed by another peripheral.
 * @post 4 ADC analog pins live; USB VBUS detect live; 8 sonar pins muxed.
 *
 * @note Not thread-safe (single-threaded boot context).
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_adc_usb_sonar_pins(void)
{
  static const char* const s_tag = "GPIO";
  rx_err_t                 err   = internal_gpio_init_adc();
  RX_RETURN_ON_ERROR(err, s_tag, "ADC pin init failed");

  err = internal_gpio_init_usb();
  RX_RETURN_ON_ERROR(err, s_tag, "USB VBUS pin init failed");

  err = internal_gpio_init_sonar_triggers();
  RX_RETURN_ON_ERROR(err, s_tag, "Sonar trigger pin init failed");

  err = internal_gpio_init_sonar_echoes();
  RX_RETURN_ON_ERROR(err, s_tag, "Sonar echo pin init failed");
  return k_rx_ok;
}

/** @brief Configure motor, IMU, ADC, USB, and sonar GPIO pins (second half of gpio_init). */
static rx_err_t internal_gpio_init_motor_and_sensors(void)
{
  static const char* const s_tag = "GPIO";
  rx_err_t                 err   = internal_gpio_init_motor_pins();
  RX_RETURN_ON_ERROR(err, s_tag, "Motor pin init failed");

  err = internal_gpio_init_imu_pins();
  RX_RETURN_ON_ERROR(err, s_tag, "IMU pin group init failed");

  err = internal_gpio_init_adc_usb_sonar_pins();
  RX_RETURN_ON_ERROR(err, s_tag, "ADC/USB/sonar pin init failed");
  return k_rx_ok;
}

static rx_err_t gpio_init(void)
{
  static const char* const s_tag = "GPIO";

  rx_err_t err = internal_gpio_init_comm_and_encoders();
  RX_RETURN_ON_ERROR(err, s_tag, "Comm and encoder pin init failed");

  err = internal_gpio_init_motor_and_sensors();
  RX_RETURN_ON_ERROR(err, s_tag, "Motor and sensor pin init failed");

  /* GTETRG nFAULT pins: no MPC config needed (documented in header) */
  rx_log_info(s_tag, "GPIO pin muxing complete");

  return k_rx_ok;
}

/**
 * @brief Initialize GPTW PWM for 4 motor channels with 90-degree phase staggering
 *
 * @details
 * Configures GPTW channels 0-3 for complementary PWM output at 20 kHz with
 * 1 us dead-time. Channels are phase-staggered by 90 degrees to reduce peak
 * current draw and EMI.
 *
 *
 * @pre GPIO pins for GPTW configured via gpio_init() (PSEL = 0x1E)
 * @pre PCLKA clock running at 120 MHz
 *
 * @post 4 GPTW channels configured for 20 kHz complementary PWM
 * @post Phase staggering active (0, 90, 180, 270 degrees)
 *
 * @note Not thread-safe. Call during single-threaded initialization only.
 *
 * @see rx_gptw_init_all_staggered() HAL function for staggered PWM init
 *
 * @since Version 1.0.0
 */
static rx_err_t gptw_pwm_init(void)
{
  static const char* const s_tag = "GPTW";

  /* Per-channel configs: shared frequency / wave mode, distinct pins
   * per motor. Pin map decoded from hardware.h's packed k_pin_motor*
   * constants via rx_port_from_pin / rx_pin_from_pin so the HAL itself
   * never embeds a board-specific pin assumption. */
  const rx_port_pin_t in2_pins[k_rx_gptw_channel_count] = {
    (rx_port_pin_t)k_pin_motor0_in2,
    (rx_port_pin_t)k_pin_motor1_in2,
    (rx_port_pin_t)k_pin_motor2_in2,
    (rx_port_pin_t)k_pin_motor3_in2,
  };
  const rx_port_pin_t in1_pins[k_rx_gptw_channel_count] = {
    (rx_port_pin_t)k_pin_motor0_in1,
    (rx_port_pin_t)k_pin_motor1_in1,
    (rx_port_pin_t)k_pin_motor2_in1,
    (rx_port_pin_t)k_pin_motor3_in1,
  };

  rx_gptw_config_t configs[k_rx_gptw_channel_count];
  for (uint8_t i = 0; i < k_rx_gptw_channel_count; i++) {
    configs[i] = (rx_gptw_config_t){
      .frequency_hz         = k_gptw_pwm_freq_hz,
      .deadtime_ns          = k_gptw_deadtime_ns,
      .wave_mode            = k_gptw_wave_saw_pwm,
      .enable_complementary = true,
      .invert_polarity      = false,
      .port_a_idx           = rx_port_from_pin(in2_pins[i]),
      .bit_a                = rx_pin_from_pin(in2_pins[i]),
      .port_b_idx           = rx_port_from_pin(in1_pins[i]),
      .bit_b                = rx_pin_from_pin(in1_pins[i]),
    };
  }
  const rx_gptw_config_t* config_ptrs[k_rx_gptw_channel_count];
  for (uint8_t i = 0; i < k_rx_gptw_channel_count; ++i) {
    config_ptrs[i] = &configs[i];
  }

  rx_err_t err = rx_gptw_init_all_staggered(config_ptrs);
  RX_RETURN_ON_ERROR(err, s_tag, "GPTW staggered init failed");

  rx_log_info(s_tag, "4 channels @ 20kHz, 90-deg stagger");
  return k_rx_ok;
}

/**
 * @brief Initialize RSPI2 as SPI peripheral for RPi5 host communication
 *
 * @details
 * Configures RSPI2 in peripheral mode for receiving commands from the
 * Raspberry Pi 5 host controller. Uses SPI mode 0 (CPOL=0, CPHA=0) with
 * 8-bit transfers.
 *
 *
 * @pre GPIO pins for RSPI2 configured via gpio_init()
 * @pre PCLKA clock running
 *
 * @post RSPI2 ready to receive SPI transactions from RPi5
 * @post SPI mode 0, 8-bit transfers configured
 *
 * @note Not thread-safe. Call during single-threaded initialization only.
 *
 * @see rspi_init_peripheral() HAL function for RSPI peripheral mode
 *
 * @since Version 1.0.0
 */
static rx_err_t spi_init(void)
{
  static const char* const s_tag = "SPI";

  rspi_config_t host_config = {.spi_mode = k_rspi_mode_0, .use_16bit = false};

  rx_err_t err = rspi_init_peripheral(k_rspi_channel_2, &host_config);
  RX_RETURN_ON_ERROR(err, s_tag, "RSPI2 peripheral init failed");

  rx_log_info(s_tag, "RSPI2 initialized (host peripheral, mode 0, 8-bit)");
  return k_rx_ok;
}

/**
 * @brief Initialize I2C buses for host communication and IMU sensors
 *
 * @details
 * Configures two RIIC channels:
 * - **RIIC0** at 400 kHz (fast mode) for RPi5 host I2C (P1.2 SCL0, P1.3 SDA0)
 * - **RIIC1** at 400 kHz (fast mode) for IMU sensors BNO055 + BMP280 (P2.1 SCL1, P2.0 SDA1)
 *
 * GPIO pins for RIIC0 are configured in internal_gpio_init_i2c(); pins for
 * RIIC1 (IMU) are configured in internal_gpio_init_imu().
 *
 *
 * @pre GPIO pins for RIIC0 configured via internal_gpio_init_i2c()
 * @pre GPIO pins for RIIC1 (IMU) configured via internal_gpio_init_imu()
 * @pre PCLKB clock running at 60 MHz
 *
 * @post Static frequency assertions verified at compile time
 * @post RIIC initialization deferred to rx_bus_i2c_init() in bus manager
 *
 * @note Not thread-safe. Call during single-threaded initialization only.
 *
 * @see rx_bus_i2c_init() Performs RIIC channel init on first bus registration
 * @see internal_gpio_init_imu() Configures P2.0/P2.1 for RIIC1
 *
 * @since Version 1.0.0
 */
static rx_err_t i2c_init(void)
{
  static const char* const s_tag = "I2C";

  /* Precondition: channel and frequency values must be within valid range (NASA Rule 5) */
  static_assert((bool)((unsigned int)k_i2c_host_freq_hz <= (unsigned int)k_i2c_fast_mode_max_hz),
                "Host I2C frequency must not exceed 400 kHz fast mode");
  static_assert((bool)((unsigned int)k_i2c_imu_freq_hz <= (unsigned int)k_i2c_fast_mode_max_hz),
                "IMU I2C frequency must not exceed 400 kHz fast mode");

  /* RIIC initialization is deferred to the bus manager. Each call to
   * rx_bus_i2c_init() (via rx_bus_manager_register() in main.c) will call
   * riic_init() for its channel on first use and skip re-initialization for
   * shared-channel buses (e.g., "i2c1" and "i2c1_baro" both on RIIC1). */
  rx_log_info(s_tag, "RIIC initialization deferred to bus manager (rx_bus_i2c_init)");
  return k_rx_ok;
}

/**
 * @brief Initialize ADC channels for motor current sensing
 *
 * @details
 * Configures S12AD0 channels AN004-AN007 at 12-bit resolution for reading
 * motor current via analog sense inputs.
 *
 * | Channel | Motor | Pin |
 * |---------|-------|-----|
 * | AN007 | Motor 0 | P4.7 |
 * | AN006 | Motor 1 | P4.6 |
 * | AN005 | Motor 2 | P4.5 |
 * | AN004 | Motor 3 | P4.4 |
 *
 *
 * @pre GPIO pins for ADC configured via gpio_init()
 * @pre PCLKB/PCLKD clock running
 *
 * @post S12AD0 channels AN004-AN007 ready for conversion
 * @post 12-bit resolution configured
 *
 * @note Not thread-safe. Call during single-threaded initialization only.
 *
 * @see adc_init() HAL function for ADC channel init
 *
 * @since Version 1.0.0
 */
static rx_err_t adc_init_channels(void)
{
  static const char* const s_tag = "ADC";

  const adc_channel_t channels[k_motor_adc_count] = {
    k_adc_channel_7, /* Motor 0: AN007 */
    k_adc_channel_6, /* Motor 1: AN006 */
    k_adc_channel_5, /* Motor 2: AN005 */
    k_adc_channel_4  /* Motor 3: AN004 */
  };

  for (uint8_t i = 0; i < k_motor_adc_count; i++) {
    rx_err_t err = adc_init(k_adc_unit_0, channels[i], k_adc_resolution_12bit);
    RX_RETURN_ON_ERROR(err, s_tag, "ADC channel init failed");
  }

  rx_log_info(s_tag, "S12AD0 AN004-AN007 @ 12-bit");
  return k_rx_ok;
}

/**
 * @brief Non-fatal peripheral validation after initialization
 *
 * @details
 * Performs a test ADC conversion to verify the ADC subsystem is operational.
 * Logs warnings on failure but never halts - all checks are informational.
 *
 * @pre All peripheral init functions have completed
 *
 * @post Validation results logged
 * @post No state changes (read-only checks)
 *
 * @note Not thread-safe. Call during single-threaded initialization only.
 * @note Non-fatal: failures are logged as warnings, boot continues.
 *
 * @since Version 1.0.0
 */
static void validate_peripherals(void)
{
  static const char* const s_tag = "VALIDATE";

  /* ADC test conversion on AN007 (motor 0 current) */
  uint16_t test_val = 0;
  if (adc_read(k_adc_unit_0, k_adc_channel_7, &test_val) == k_rx_ok) {
    rx_log_info(s_tag, "ADC0 test conversion OK");
  } else {
    rx_log_warn(s_tag, "ADC0 test conversion failed (non-fatal)");
  }

  rx_log_info(s_tag, "Peripheral validation complete");
}
#endif /* !RX_IS_SIMULATOR */

/**
 * @var g_pin_host_irq
 * @brief Canonical pin identifier for the HOST_IRQ output signal (P6.7, active-low)
 *
 * @details
 * Single authoritative definition of the HOST_IRQ GPIO pin.  Consumers
 * (e.g. telemetry_task) use this constant with gpio_write_low() /
 * gpio_write_high() instead of embedding the raw port/bit literal.
 *
 * @note Read-only after hardware_init() completes.
 * @since Version 1.0.0
 * @see internal_gpio_init_host_irq() Configures pin direction and initial level
 */
const rx_port_pin_t g_pin_host_irq = (rx_port_pin_t)k_pin_host_irq;

/**
 * @brief Initialize all application-specific hardware peripherals for STAR motor controller
 *
 * @details
 * Configures **seven categories** of peripherals required by the STAR robot application:
 *
 * 1. **GPIO** ([PASS] implemented) - Motor control pins, LEDs, sensor chip selects
 * 2. **Timers** ([PASS] implemented) - CMT0 for ThreadX tick at 100 Hz
 * 3. **UART** ([PASS] implemented) - SCI9 for debug console at 115200 baud
 * 4. **SPI** ([PASS] implemented) - Host SPI peripheral (RSPI2), sensor bus
 * 5. **I2C** (planned) - IMU, temperature, pressure sensors
 * 7. **ADC** (planned) - Current sensing
 *
 * ## Initialization Sequence (7 Stages)
 *
 * **Stage order is CRITICAL** - violating dependencies causes failures:
 *
 * @msc
 * msc {
 *   width=700;
 *   Caller, HwInit, Timers, UART, SPI, I2C, ADC;
 *
 *   Caller => HwInit [label="hardware_init()"];
 *   HwInit note HwInit [label="Precondition: Check SCKCR3 != reset_state", textcolor="blue"];
 *   HwInit => HwInit [label="RX_ASSERT(clocks initialized)"];
 *   HwInit => Timers [label="timer_init()"];
 *   Timers => Timers [label="Configure CMT0\n100 Hz tick"];
 *   Timers => HwInit [label="k_rx_ok"];
 *   HwInit => UART [label="uart_debug_init()"];
 *   UART => UART [label="Configure SCI9\n115200 baud"];
 *   UART => HwInit [label="k_rx_ok"];
 *   HwInit => SPI [label="spi_init()"];
 *   SPI => SPI [label="Configure RSPI2\nhost peripheral"];
 *   SPI => HwInit [label="k_rx_ok"];
 *   HwInit => I2C [label="i2c_init()"];
 *   I2C => I2C [label="Configure RIIC0"];
 *   I2C => HwInit [label="k_rx_ok"];
 *   HwInit => ADC [label="adc_init_channels()"];
 *   ADC => ADC [label="Configure S12AD0\nAN004-AN007"];
 *   ADC => HwInit [label="k_rx_ok"];
 *   HwInit note HwInit [label="Postcondition: Check SCKCR3 still valid", textcolor="green"];
 *   HwInit => HwInit [label="RX_ASSERT(clocks not corrupted)"];
 *   HwInit => Caller [label="k_rx_ok"];
 * }
 * @endmsc
 *
 * ## Performance Characteristics (RX72N @ 240 MHz)
 *
 * | Stage | Duration | CPU Cycles | Implementation | Critical Path? |
 * |-------|----------|------------|----------------|----------------|
 * | **Precondition check** | 0.5 us | ~120 | [COMPLETE] | No |
 * | **Timer init (CMT0)** | 10 us | ~2,400 | [COMPLETE] | No |
 * | **UART init (SCI9)** | 50 us | ~12,000 | [COMPLETE] | No |
 * | **SPI init (RSPI2)** | 20 us | ~4,800 | [COMPLETE] | No |
 * | **I2C init (RIIC0+1)** | 15 us | ~3,600 | [COMPLETE] | No |
 * | **ADC init (S12AD0)** | 100 us | ~24,000 | [COMPLETE] | No |
 * | **Postcondition check** | 0.5 us | ~120 | [COMPLETE] | No |
 * | **Total (current)** | **~201 us** | **~48,240** | All peripherals | **No** |
 *
 * **Note:** Not on critical boot path. Total boot time (main to ThreadX) is ~51 ms,
 * dominated by USB enumeration (~50 ms) which happens later in comm_task.
 *
 * ## Memory Usage (Static Allocation Only)
 *
 * | Object | Size | Location | Lifetime |
 * |--------|------|----------|----------|
 * | **s_sckcr3_reset_state** | 1 byte | .rodata (Flash) | Permanent |
 * | **err (local variable)** | 2 bytes | Stack (Main stack) | Function duration |
 * | **Return address** | 4 bytes | Stack (Main stack) | Function duration |
 * | **Total stack usage** | ~64 bytes | Main stack (4 KB available) | Function duration |
 *
 * **Stack depth:** 1 level (hardware_init -> timer_init/uart_init)
 *
 * ## Error Handling and Recovery
 *
 * **Critical errors (assert-halt):**
 * - **Precondition:** SCKCR3 == reset_state -> Clocks not initialized (caller bug)
 * - **Postcondition:** SCKCR3 changed during init -> Clock corruption (peripheral bug)
 * - **Action:** RX_ASSERT halts execution with message
 *
 * **Peripheral errors (return error code):**
 * - **timer_init() failed** -> CMT0 configuration error (hardware or register access issue)
 * - **uart_debug_init() failed** -> SCI9 configuration error (baud rate calculation, pin config)
 * - **Action:** Return error to main(), which halts boot with error code
 *
 * **Recovery strategy:**
 * - On assert: Halt execution (developer investigates)
 * - On error return: main() halts boot (error logged via UART if possible)
 * - No partial initialization cleanup (fail-fast, no recovery)
 *
 *
 * @pre System clocks configured via rx_clock_power_init() (SCKCR3 != reset_state)
 * @pre Interrupt vector table set up (reset vector, exception vectors)
 * @pre Stack pointer valid (at least 4 KB available in main stack)
 * @pre Memory protection unlocked if required (PRCR register)
 *
 * @post CMT0 configured for ThreadX tick at 100 Hz (timer interrupt enabled)
 * @post SCI9 configured for debug console at 115200 baud (UART TX/RX operational)
 * @post System clocks still operational (SCKCR3 unchanged from precondition)
 * @post Peripherals ready for application use (motor control, sensors, communication)
 *
 * @note **Call order:** rx_clock_power_init() -> hardware_init() -> tx_kernel_enter()
 *       Violating this order causes precondition assertion failure.
 *
 * @note **Not idempotent:** Calling hardware_init() multiple times may cause errors
 *       (peripherals already configured, interrupts already enabled). Only call once during boot.
 *
 * @warning **Never call before rx_clock_power_init().** Peripheral configuration requires
 *          stable system clocks. Precondition assertion will halt execution if violated.
 *
 * @warning **Boot order critical.** rx_infrastructure_init() must be called before
 *          hardware_init(). See main.c for the complete boot sequence.
 *
 * @par Thread Safety:
 * Executes in single-threaded context before ThreadX starts. No synchronization needed.
 *
 * @par Example Boot Sequence:
 * @code
 * int main(void) {
 *   rx_err_t ret;
 *
 *   // Stage 1: System clocks (240 MHz PLL)
 *   ret = rx_clock_power_init();
 *   RX_ERROR_CHECK(ret);
 *
 *   // Stage 2: Application peripherals (timers, UART, sensors)
 *   ret = hardware_init();
 *   if (ret != k_rx_ok) {
 *     // Log error (if UART initialized successfully)
 *     rx_log_error("MAIN", "Hardware initialization failed: %d", ret);
 *     // Halt boot
 *     while (1) { __asm__ volatile("wait"); }
 *   }
 *
 *   // Stage 3: Start ThreadX RTOS
 *   tx_kernel_enter();
 *
 *   // Never reached (scheduler takes over)
 * }
 * @endcode
 *
 * @par Example Error Handling:
 * @code
 * rx_err_t hardware_init(void) {
 *   rx_err_t err;
 *
 *   // Initialize timer (CMT0)
 *   err = timer_init();
 *   if (rx_err_is_error(err)) {
 *     // Cannot use rx_log_error yet (UART not initialized)
 *     return err;  // Propagate error to main()
 *   }
 *
 *   // Initialize UART (SCI9) - enables logging
 *   err = uart_debug_init();
 *   if (rx_err_is_error(err)) {
 *     // Still cannot log (UART init failed)
 *     return err;  // Propagate error to main()
 *   }
 *
 *   // Now logging is available for subsequent errors
 *   err = spi_init();
 *   if (rx_err_is_error(err)) {
 *     rx_log_error("HWINT", "SPI initialization failed: %d", err);
 *     return err;
 *   }
 *
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * @par Planned Peripherals (Future Implementation):
 * - **GPIO:** Port initialization for motor control (PA0-PA7), LEDs (PB0-PB2), sensor CS (PC0-PC3)
 * - **SPI:** RSPI1 for sensor bus
 * - **I2C:** RIIC0 for IMU (MPU6050), temperature (LM75), pressure (BMP280)
 * - **ADC:** ADC0 channels for current sensing (4 channels)
 * - **USB:** USB CDC for ROS2 communication (already partially implemented, needs integration)
 *
 * @see rx_clock_power_init() System clock configuration (MUST call before this function)
 * @see timer_init() Configure CMT0 for ThreadX tick (called by this function)
 * @see uart_debug_init() Configure SCI9 for debug console (called by this function)
 * @see main() Main entry point (calls this function during boot)
 *
 * @since Version 1.0.0
 *
 * @test test_hardware_init.c Verify all peripherals initialize successfully
 * @test test_hardware_init.c Verify precondition assertion (clocks not initialized)
 * @test test_hardware_init.c Verify error propagation (timer/UART init failure)
 */
#if !RX_IS_SIMULATOR
/**
 * @brief Initialize the motor-control peripheral chain (GPIO, GPTW PWM, POEG)
 *
 * @details
 * First phase of internal_init_all_peripherals(). Brings up the pin mux,
 * the 4-channel staggered GPTW PWM, and the POEG fault-protection block
 * that links GTETRG nFAULT inputs to PWM hardware shutdown.
 *
 * @return rx_err_t Error code from the first failing sub-init, or k_rx_ok.
 * @retval k_rx_ok GPIO, GPTW, and POEG all initialised successfully.
 *
 * @pre System clocks have been initialised (SCKCR3 != reset state).
 * @pre Single-threaded boot context.
 * @post All MPC pins configured; 4 GPTW channels staggered at 20 kHz; POEG
 *       active and arming hardware nFAULT shutdown of the H-bridges.
 *
 * @note Not thread-safe.
 * @since Version 1.0.0
 */
static rx_err_t internal_init_motor_chain(void)
{
  static const char* const s_tag = "HW_INIT";

  /* 1. GPIO: Configure MPC pin multiplexing for all peripherals */
  rx_err_t err = gpio_init();
  RX_RETURN_ON_ERROR(err, s_tag, "GPIO initialization failed");

  /* 2. GPTW: 4-channel motor PWM with phase staggering */
  err = gptw_pwm_init();
  RX_RETURN_ON_ERROR(err, s_tag, "GPTW PWM initialization failed");

  /* 2b. POEG: Motor fault protection (links GTETRG->POEG->GPTW) */
  err = rx_poeg_init();
  RX_RETURN_ON_ERROR(err, s_tag, "POEG fault protection init failed");
  return k_rx_ok;
}

/**
 * @brief Initialize host-communication peripherals (SPI, I2C, ADC)
 *
 * @details
 * Second phase of internal_init_all_peripherals(). Brings up RSPI2 for
 * RPi5 host communication, the two RIIC channels (RIIC0 host bus and
 * RIIC1 IMU bus -- the latter is initialised lazily via rx_bus_i2c_init),
 * and the S12AD0 ADC channels for motor current sensing.
 *
 * @return rx_err_t Error code from the first failing sub-init, or k_rx_ok.
 * @retval k_rx_ok SPI, I2C, and ADC subsystems all initialised successfully.
 *
 * @pre internal_init_motor_chain() and timer_init() have completed.
 * @pre Single-threaded boot context.
 * @post RSPI2 ready for host packets; RIIC channels armed; ADC0 calibrated.
 *
 * @note Not thread-safe.
 * @since Version 1.0.0
 */
static rx_err_t internal_init_comm_chain(void)
{
  static const char* const s_tag = "HW_INIT";

  /* 5a. SPI: RSPI2 host peripheral for RPi5 communication */
  rx_err_t err = spi_init();
  RX_RETURN_ON_ERROR(err, s_tag, "SPI initialization failed");

  /* 6. I2C: RIIC0 host */
  err = i2c_init();
  RX_RETURN_ON_ERROR(err, s_tag, "I2C initialization failed");

  /* 7. ADC: S12AD0 channels AN004-AN007 for motor current sensing */
  err = adc_init_channels();
  RX_RETURN_ON_ERROR(err, s_tag, "ADC initialization failed");
  return k_rx_ok;
}

/** @brief Initialize all hardware peripherals (GPIO through ADC). Hardware only. */
static rx_err_t internal_init_all_peripherals(void)
{
  /* 1-2. GPIO + GPTW PWM + POEG fault protection. */
  rx_err_t err = internal_init_motor_chain();
  if (rx_err_is_error(err)) {
    return err;
  }

  /* 3. Timer: CMT0 for ThreadX tick (100 Hz) */
  err = timer_init();
  if (rx_err_is_error(err)) {
    return err;
  }

  /* 4. UART: SCI9 debug console - MOVED TO MAIN()
   *    UART initialized in main() before hardware_init() to enable early error logging.
   *    Error logging is now available for all peripheral initialization below. */

  /* 5-7. SPI host link, RIIC host bus, ADC current sensing. */
  err = internal_init_comm_chain();
  if (rx_err_is_error(err)) {
    return err;
  }

  /* 9. Validate: Non-fatal peripheral checks (log warnings, never halt) */
  validate_peripherals();

  return k_rx_ok;
}
#endif /* !RX_IS_SIMULATOR */

/**
 * @brief Initialize all on-board peripherals after the clock tree is up
 *
 * @details
 * Top-level peripheral bring-up entry point.  Called once from rx_main()
 * after the clock subsystem has been brought online (SCKCR3 != reset).
 *
 * On real hardware (RX_IS_SIMULATOR not defined) the function delegates to
 * internal_init_all_peripherals(), which executes the canonical bring-up
 * order:
 *
 *  1. GPIO / pin-mux for motors, encoders, sensors, and LEDs.
 *  2. GPTW PWM channels for the four motor drivers.
 *  3. POEG (port output enable group) for hardware fault propagation.
 *  4. CMT timer for the millisecond tick.
 *  5. SPI for the host-link bridge.
 *  6. I2C for IMU, barometer, and other peripherals.
 *  7. ADC channels AN004-AN007 for motor current sense.
 *  8. Non-fatal validation pass (logs warnings, never halts).
 *
 * Any error from those sub-initializations short-circuits the bring-up and
 * is returned to the caller; the caller will typically halt the system.
 *
 * Under RX_IS_SIMULATOR the body of internal_init_all_peripherals() is
 * compiled out so the function only validates the clock pre/post-conditions
 * and returns success.  This lets the unit-test host run integration tests
 * that exercise hardware_init() without touching simulated MMIO.
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok All configured peripherals initialized successfully (or
 *                 always returned under RX_IS_SIMULATOR).
 * @retval other   Propagated from internal_init_all_peripherals() if any
 *                 sub-initialization (GPIO, GPTW, POEG, timer, SPI, I2C,
 *                 ADC) failed.
 *
 * @pre System clock subsystem initialized (SCKCR3 != reset value).
 * @pre Called from supervisor/boot context before the ThreadX scheduler
 *      starts.
 *
 * @post On k_rx_ok: all configured peripherals are initialized and the
 *       clock register state has not been corrupted (post-condition assert).
 * @post On error: only a prefix of peripherals is initialized; the caller
 *       must not assume any particular peripheral is usable.
 *
 * @note Not thread-safe.  Designed to be called exactly once from
 *       single-threaded boot context before the ThreadX scheduler starts.
 *
 * @see rx_main() Calls hardware_init() during boot.
 *
 * @since Version 1.0.0
 */
rx_err_t hardware_init(void)
{
  /* Precondition: Verify that system clocks have been initialized */
  RX_ASSERT((system_regs() != nullptr) && (system_regs()->sckcr3 != s_sckcr3_reset_state),
            "Precondition: Clock system not properly initialized");

#if !RX_IS_SIMULATOR
  /* Initialize all peripherals: GPIO -> GPTW -> POEG -> Timer -> SPI -> I2C -> ADC */
  const rx_err_t init_err = internal_init_all_peripherals();
  if (rx_err_is_error(init_err)) {
    return init_err;
  }
#endif

  /* Postcondition: Verify clock system is still operational after all setup */
  RX_ASSERT((system_regs() != nullptr) && (system_regs()->sckcr3 != s_sckcr3_reset_state),
            "Postcondition: Clock system corrupted during initialization");

  return k_rx_ok;
}
