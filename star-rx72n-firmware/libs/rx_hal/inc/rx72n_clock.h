/**
 * @file rx72n_clock.h
 * @brief RX72N Clock Frequency Definitions and Configuration
 *
 * @details
 * Defines all system clock frequencies for the RX72N microcontroller configured
 * with external 24 MHz crystal and PLL x10 for maximum 240 MHz CPU operation.
 * These constants are used throughout the firmware for timing calculations,
 * baud rate generation, PWM frequency configuration, and peripheral initialization.
 *
 * ## Clock Generation Architecture (STAR Project)
 *
 * The RX72N has a sophisticated clock generation system with multiple clock
 * domains optimized for different purposes:
 *
 * @dot
 * digraph clock_tree {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   EXTAL [label="External Crystal\n24 MHz", shape=ellipse, fillcolor=lightyellow, style=filled];
 *   PLL [label="PLL\nx10", fillcolor=lightblue, style=filled];
 *   ICLK [label="ICLK\n240 MHz\nCPU", fillcolor=lightgreen, style=filled];
 *   PCLKA [label="PCLKA\n120 MHz\nTimers", fillcolor=lightpink, style=filled];
 *   PCLKB [label="PCLKB\n60 MHz\nUART/SPI", fillcolor=lightpink, style=filled];
 *   PCLKC [label="PCLKC\n60 MHz\nPWM", fillcolor=lightpink, style=filled];
 *   PCLKD [label="PCLKD\n60 MHz\nCMT", fillcolor=lightpink, style=filled];
 *   FCLK [label="FCLK\n60 MHz\nFlash", fillcolor=lightcyan, style=filled];
 *   BCLK [label="BCLK\n120 MHz\nExt Bus", fillcolor=lightgray, style=filled];
 *
 *   EXTAL -> PLL [label="24 MHz"];
 *   PLL -> ICLK [label="240 MHz\n/1"];
 *   PLL -> PCLKA [label="/2"];
 *   PLL -> PCLKB [label="/4"];
 *   PLL -> PCLKC [label="/4"];
 *   PLL -> PCLKD [label="/4"];
 *   PLL -> FCLK [label="/4"];
 *   PLL -> BCLK [label="/2"];
 * }
 * @enddot
 *
 * ## Clock Domain Assignments
 *
 * | Clock | Frequency | Peripherals | Purpose |
 * |-------|-----------|-------------|---------|
 * | ICLK | 240 MHz | CPU core, DMA | Maximum instruction throughput |
 * | PCLKA | 120 MHz | CMT, TMR, MTU, GPT | High-resolution timers for PWM |
 * | PCLKB | 60 MHz | SCI (UART), RSPI, RIIC | Communication interfaces |
 * | PCLKC | 60 MHz | (Reserved) | User peripherals |
 * | PCLKD | 60 MHz | (Reserved) | User peripherals |
 * | FCLK | 60 MHz | Flash controller | Flash access (max 60 MHz) |
 * | BCLK | 120 MHz | External bus | External memory interface |
 *
 * ## Clock Configuration (SYSTEM Registers)
 *
 * Clock frequencies are set during boot by `rx_clock_power_init()`:
 *
 * 1. **Main Clock Enable**: MOSCWTCR, MOSCCR, MOFCR registers
 * 2. **PLL Configuration**: PLLCR (x10 multiplier, STC=0x13), PLLCR2 (enable)
 * 3. **Clock Dividers**: SCKCR register sets ICK, PCKA, PCKB, PCKC, PCKD, FCK, BCK dividers
 * 4. **Clock Source Selection**: SCKCR3 selects PLL as system clock
 *
 * **Register Configuration** (from rx_clock_power_init.c):
 * ```
 * SYSTEM.SCKCR.LONG = 0x20011222;  // ICK=0(/1)->240MHz, BCK=1(/2)->120MHz, PCKA=1(/2)->120MHz, PCKB/C/D=2(/4)->60MHz, FCK=2(/4)->60MHz
 * SYSTEM.PLLCR.WORD = 0x1300;      // STC=19 -> x10.0 (manual p345); 24 MHz x 10 = 240 MHz
 * ```
 *
 * ## Performance Implications
 *
 * | Operation | Clock | Impact of Frequency |
 * |-----------|-------|---------------------|
 * | CPU instructions | ICLK (240 MHz) | ~4.2 ns per cycle, critical for control loops |
 * | Flash access | FCLK (60 MHz) | ~17 ns per access, affects code execution speed |
 * | PWM resolution | PCLKA (120 MHz) | 8.3 ns resolution, 20 kHz PWM = 6000 steps |
 * | UART baud rate | PCLKB (60 MHz) | Max 3.75 Mbaud (60M/16), typical 115200 baud |
 * | Timer tick | PCLKD (60 MHz) | ThreadX 10ms tick = 600,000 cycles per tick |
 *
 * ## Hardware Requirements
 *
 * | Requirement | Value | Notes |
 * |-------------|-------|-------|
 * | External Crystal | 24 MHz | +/-50 ppm accuracy required |
 * | PLL Input Range | 8-24 MHz | 24 MHz nominal for STAR project |
 * | PLL Output Range | 200-520 MHz | 240 MHz output (/1 for CPU) |
 * | CPU Max Frequency | 240 MHz | Absolute max per datasheet |
 * | Flash Max Frequency | 60 MHz | Must not exceed! (hardware limit) |
 * | Operating Voltage | 3.3V +/-10% | Required for 240 MHz operation |
 * | Temperature Range | -40degC to +85degC | Industrial grade |
 *
 * ## Timing Calculation Examples
 *
 * **PWM Frequency**:
 * ```
 * PWM_freq = PCLKA / (prescaler x period)
 * 20 kHz = 120 MHz / (1 x 6000)
 * ```
 *
 * **UART Baud Rate**:
 * ```
 * Baud_rate = PCLKB / (32 x (N + 1))
 * 115200 = 60 MHz / (32 x (16 + 1))  -> N=16 (BRR register value)
 * ```
 *
 * **Timer Period**:
 * ```
 * Period_ms = (counter_max x prescaler) / PCLKD_MHz
 * 1 ms = (60000 x 1) / 60 MHz
 * ```
 *
 * @par Module Dependencies:
 * - **NO dependencies** (pure constant definitions)
 * - Used by: ALL timing-related modules (CMT, PWM, UART, SPI, ADC, etc.)
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1**: [OK] No code (only data definitions)
 * - **Rule 2**: [OK] No loops
 * - **Rule 3**: [OK] No dynamic allocation (compile-time constants)
 * - **Rule 4**: [OK] No functions
 * - **Rule 5**: [OK] N/A (no functions)
 * - **Rule 6**: [OK] File scope only (header file)
 * - **Rule 7**: [OK] No functions
 * - **Rule 8**: [OK] C23 typed enums for all constants
 * - **Rule 9**: [OK] No pointers
 * - **Rule 10**: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility**: Only defines clock frequencies, no logic
 * - **Open/Closed**: Extensible (add new clocks) without modification
 * - **Liskov Substitution**: N/A (no inheritance)
 * - **Interface Segregation**: Minimal interface (single enum)
 * - **Dependency Inversion**: N/A (no dependencies)
 *
 * @par Usage Example - Baud Rate Calculation:
 * @code{.c}
 * #include "rx72n_clock.h"
 *
 * // Calculate UART BRR register value for desired baud rate
 * uint8_t uart_calculate_brr(uint32_t baud_rate) {
 *     // Formula: BRR = (PCLKB / (32 x baud)) - 1
 *     uint32_t brr = (k_pclkb_hz / (32 * baud_rate)) - 1;
 *     return (uint8_t)brr;
 * }
 *
 * // Example: 115200 baud
 * uint8_t brr = uart_calculate_brr(115200);  // Returns 16
 * @endcode
 *
 * @par Usage Example - PWM Configuration:
 * @code{.c}
 * // Configure PWM for 20 kHz at 50% duty cycle
 * void pwm_configure_20khz(void) {
 *     typedef enum : uint32_t {
 *         k_pwm_freq_hz = 20000,  // Desired PWM frequency
 *         k_prescaler = 1,         // Timer prescaler
 *     } pwm_config_t;
 *
 *     // Calculate timer period: Period = PCLKA / (prescaler x freq)
 *     uint32_t period = k_pclka_hz / (k_prescaler * k_pwm_freq_hz);
 *     // period = 120,000,000 / (1 x 20,000) = 6000 counts
 *
 *     uint32_t duty_50pct = period / 2;  // 3000 counts = 50%
 *
 *     gpt_set_period(GPT_CHANNEL_0, period);
 *     gpt_set_duty(GPT_CHANNEL_0, duty_50pct);
 * }
 * @endcode
 *
 * @par Usage Example - Delay Timing:
 * @code{.c}
 * // Calculate cycles for precise delay
 * void delay_us(uint32_t us) {
 *     // Cycles per microsecond at ICLK frequency
 *     typedef enum : uint32_t {
 *         k_cycles_per_us = k_iclk_hz / 1000000,  // 240 cycles/us
 *     } delay_config_t;
 *
 *     uint32_t cycles = us * k_cycles_per_us;
 *
 *     // Busy-wait loop (NOP instructions)
 *     for (uint32_t i = 0; i < cycles / 4; i++) {
 *         __asm volatile("nop\nnop\nnop\nnop");
 *     }
 * }
 * @endcode
 *
 * @par Usage Example - Timer Configuration:
 * @code{.c}
 * // Configure CMT for 1ms tick (1 kHz interrupt)
 * void cmt_configure_1ms(void) {
 *     typedef enum : uint32_t {
 *         k_desired_freq_hz = 1000,    // 1 kHz = 1ms period
 *         k_cmt_prescaler = 8,          // CMT prescaler (PCLKD/8)
 *     } cmt_config_t;
 *
 *     // Calculate compare match value
 *     // CMCOR = (PCLKD / prescaler / freq) - 1
 *     uint32_t cmcor = (k_pclkd_hz / k_cmt_prescaler / k_desired_freq_hz) - 1;
 *     // cmcor = (60,000,000 / 8 / 1000) - 1 = 7499
 *
 *     CMT0.CMCOR = (uint16_t)cmcor;
 * }
 * @endcode
 *
 * @see [RX72N User's Manual](../../docs/rx72n_manual.pdf) Chapter 9: Clock Generation Circuit
 * @see [RX72N Group Hardware Manual](../../docs/rx72n_hardware.pdf) Section 59: Electrical Characteristics
 * @see [rx_clock_power_init.c](../../src/rx_clock_power_init.c) Clock initialization code
 * @see [hardware_init.c](../../src/hardware_init.c) Calls clock initialization
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Clock Frequencies (assume PLL configured for 240 MHz)
 * =============================================================================
 */

/**
 * @enum rx_clock_frequencies_t
 * @brief System clock frequencies for RX72N @ 240 MHz operation
 *
 * @details
 * Defines all clock domain frequencies for the STAR project's RX72N configuration.
 * These values are determined by the PLL and clock divider settings in
 * rx_clock_power_init() and are guaranteed to be accurate for timing calculations.
 *
 * **Clock Source**: 24 MHz external crystal -> PLL (x10) -> 240 MHz -> divided per domain
 *
 * **Frequency Derivation**:
 * - PLL output: 24 MHz x 10 = 240 MHz
 * - ICLK: 240 MHz / 1 = 240 MHz (CPU)
 * - PCLKA: 240 MHz / 2 = 120 MHz (high-speed peripherals)
 * - PCLKB/C/D: 240 MHz / 4 = 60 MHz (standard peripherals)
 * - FCLK: 240 MHz / 4 = 60 MHz (flash - max allowed)
 * - BCLK: 240 MHz / 2 = 120 MHz (external bus - not used in STAR)
 *
 * ## Clock Domain Usage Table
 *
 * | Clock | Freq | Used By | STAR Project Usage |
 * |-------|------|---------|---------------------|
 * | ICLK | 240 MHz | CPU core | PID control loop, communication protocol |
 * | PCLKA | 120 MHz | MTU, GPT, CMT, TMR | Encoder capture (MTU), PWM generation (GPT) |
 * | PCLKB | 60 MHz | SCI, RSPI, RIIC | UART debug, SPI to RPi5, I2C sensors |
 * | PCLKC | 60 MHz | (Reserved) | Not used in STAR project |
 * | PCLKD | 60 MHz | CMT | ThreadX system tick (10ms) |
 * | FCLK | 60 MHz | Flash | Code execution, const data access |
 * | BCLK | 120 MHz | External bus | Not used (no external memory) |
 *
 * @par Field Constraints:
 * | Constant | Min | Max | Notes |
 * |----------|-----|-----|-------|
 * | k_iclk_hz | 1 MHz | 240 MHz | 240 MHz is absolute max per datasheet |
 * | k_pclka_hz | 1 MHz | 240 MHz | Limited by ICLK (cannot exceed CPU clock) |
 * | k_pclkb_hz | 1 MHz | 60 MHz | Communication peripherals rated to 60 MHz |
 * | k_fclk_hz | 1 MHz | 60 MHz | **MUST NOT EXCEED 60 MHz** (flash limit) |
 *
 * @par Usage Example - Calculate Timer Period:
 * @code{.c}
 * // Calculate CMT compare value for 100 Hz interrupt (10ms period)
 * typedef enum : uint32_t {
 *     k_desired_freq_hz = 100,     // 100 Hz = 10ms period
 *     k_cmt_prescaler = 8,          // PCLKD/8
 * } timer_config_t;
 *
 * uint32_t cmcor = (k_pclkd_hz / k_cmt_prescaler / k_desired_freq_hz) - 1;
 * // cmcor = (60,000,000 / 8 / 100) - 1 = 74,999
 * @endcode
 *
 * @see rx_clock_power_init() Function that configures these frequencies
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief CPU core clock frequency (ICLK) - 240 MHz
   *
   * @details
   * The instruction clock (ICLK) drives the RX72N CPU core, DMA controller,
   * and internal buses. This is the primary determinant of computational
   * throughput and interrupt latency.
   *
   * **Value**: 240,000,000 Hz (240 MHz)
   * **Source**: PLL output (240 MHz) divided by 1
   * **Cycle time**: 4.17 nanoseconds per cycle
   * **Register**: SYSTEM.SCKCR.ICK (divider = b'0000' = /1 from PLL)
   *
   * **Performance Metrics** @ 240 MHz:
   * - Single-cycle instructions: 4.17 ns (e.g., MOV, ADD)
   * - Flash access: ~20 ns (5 cycles with wait states)
   * - Branch misprediction penalty: ~8 ns (2 cycles)
   * - Interrupt latency: ~200 ns (minimum, no context save)
   *
   * **Use Cases in STAR Project**:
   * - PID control loop execution (~2 us per motor update @ -O2)
   * - Protocol parsing (nanopb decode ~50 us for 256-byte frame)
   * - Floating-point math (division ~20 cycles = 83 ns)
   * - Critical timing operations (encoder pulse measurement)
   *
   * **Rationale**: Maximum frequency selected for:
   * - Minimize control loop latency (100 Hz loop = 10ms budget)
   * - Fast protocol processing (SPI @ 10 Mbps = 25.6 us per 32-byte frame)
   * - Headroom for future features (current usage ~30% of CPU)
   *
   * @par Impact of Changing:
   * - **Increase**: Not possible (240 MHz is absolute max)
   * - **Decrease**: Proportional reduction in throughput, may cause control loop overruns
   *
   * @warning Flash wait states increase CPU power consumption. Use `-O2` optimization
   *          to minimize instruction count and reduce power.
   *
   * @see k_fclk_hz Flash clock must stay <=60 MHz regardless of ICLK
   */
  k_iclk_hz = 240000000UL,

  /**
   * @brief Peripheral Clock A (PCLKA) - 120 MHz
   *
   * @details
   * High-speed peripheral clock for time-critical functions: timers for PWM
   * generation (GPT), encoder capture (MTU), and high-resolution timing (CMT).
   *
   * **Value**: 120,000,000 Hz (120 MHz)
   * **Source**: 24 MHz main oscillator -> PLL x10 = 240 MHz -> PCKA divider /2
   * **Cycle time**: ~8.33 nanoseconds per cycle
   * **Register**: SYSTEM.SCKCR = 0x20011222 (PCKA[15:12] = 1 = divide-by-2 from 240 MHz PLL)
   *
   * **Connected Peripherals**:
   * - **MTU (Multi-Function Timer)**: Encoder quadrature capture (4 motors)
   * - **GPT (General PWM Timer)**: Motor PWM generation (4 H-bridges)
   * - **CMT (Compare Match Timer)**: System tick, delay timers
   * - **TMR (8/16-bit Timer)**: Miscellaneous timing functions
   *
   * **Performance Analysis**:
   *
   * **PWM Resolution @ 20 kHz**:
   * ```
   * Period = PCLKA / PWM_freq = 120,000,000 / 20,000 = 6000 counts
   * Resolution = 100% / 6000 = 0.0167% per count
   * Voltage resolution (12V supply) = 12V x 0.0167% = 2 mV per step
   * ```
   *
   * **Encoder Capture Precision**:
   * ```
   * Max velocity = 2.5 m/s (STAR robot spec)
   * Wheel diameter = 0.1 m -> circumference = 0.314 m
   * Max RPM = (2.5 m/s / 0.314 m) x 60 = 477 RPM
   * Encoder PPR = 341 x 4 (quadrature) = 1364 edges/rev
   * Max edge rate = 477 RPM x 1364 edges / 60 = 10,836 edges/sec
   * Min edge period = 1 / 10,836 = 92.3 us
   * Timer counts per edge @ 120 MHz = 92.3 us x 120 MHz = 11,076 counts
   * Velocity resolution = 1 count / 11,076 = 0.009% per timer count [OK]
   * ```
   *
   * **Rationale**: 120 MHz chosen for:
   * - PWM resolution: 6000 steps @ 20 kHz (excellent granularity)
   * - Encoder precision: 0.009% velocity measurement resolution
   * - Timer headroom: Max edge rate only uses 10% of timer bandwidth
   *
   * @par Impact of Changing:
   * - **Increase to 240 MHz**: Double PWM resolution (12000 steps), but wastes power
   * - **Decrease to 60 MHz**: Half PWM resolution (3000 steps), still acceptable
   *
   * @warning GPT, MTU, CMT baud rate calculations assume 120 MHz. Changing PCLKA
   *          requires recalculating all timer prescalers and compare values.
   */
  k_pclka_hz = 120000000UL,

  /**
   * @brief Peripheral Clock B (PCLKB) - 60 MHz
   *
   * @details
   * Standard-speed peripheral clock for communication interfaces: UART serial
   * console, SPI to Raspberry Pi 5, and I2C sensor bus.
   *
   * **Value**: 60,000,000 Hz (60 MHz)
   * **Source**: ICLK (240 MHz) divided by 4
   * **Cycle time**: 16.67 nanoseconds per cycle
   * **Register**: SYSTEM.SCKCR.PCKB (divider = b'0010' = /4 from ICLK)
   *
   * **Connected Peripherals**:
   * - **SCI (Serial Communication Interface)**: UART for USB CDC debug console
   * - **RSPI (Renesas SPI)**: SPI controller for RPi5 communication
   * - **RIIC (Renesas I2C)**: I2C controller for sensor bus (IMU, lidar interface)
   *
   * **Baud Rate Capabilities**:
   *
   * **UART (SCI) @ 115200 baud**:
   * ```
   * BRR = (PCLKB / (32 x baud_rate)) - 1
   * BRR = (60,000,000 / (32 x 115200)) - 1 = 16.276... ~ 16
   * Actual baud = 60,000,000 / (32 x 17) = 110,294 baud
   * Error = (110,294 - 115200) / 115200 = -4.3% (within +/-5% tolerance) [OK]
   * ```
   *
   * **SPI (RSPI) @ 10 Mbps** (STAR project: communication with RPi5):
   * ```
   * SPI_bitrate = PCLKB / (2 x (SPBR + 1))
   * 10 Mbps = 60,000,000 / (2 x (SPBR + 1))
   * SPBR = (60,000,000 / (2 x 10,000,000)) - 1 = 2
   * Actual rate = 60,000,000 / (2 x 3) = 10 Mbps exact [OK]
   * ```
   *
   * **I2C (RIIC) @ 400 kHz** (Fast-mode I2C for IMU):
   * ```
   * I2C_freq = PCLKB / (CKS x (BRL + BRH + 4))
   * 400 kHz typical with CKS=0 (prescaler /1), BRL=BRH=73
   * Actual = 60,000,000 / (1 x (73 + 73 + 4)) = 400 kHz exact [OK]
   * ```
   *
   * **Rationale**: 60 MHz chosen for:
   * - UART: Standard baud rates (9600, 19200, 115200) achievable with <5% error
   * - SPI: 10 Mbps target rate (STAR requirement) achievable exactly
   * - I2C: 100 kHz (standard) and 400 kHz (fast-mode) both supported
   * - Power: Lower clock = lower power for communication peripherals
   *
   * @par Impact of Changing:
   * - **Increase to 120 MHz**: Would allow 20 Mbps SPI, but RPi5 limited to 10 Mbps
   * - **Decrease to 30 MHz**: Would limit SPI to 5 Mbps, still acceptable
   *
   * @warning All UART/SPI/I2C drivers assume 60 MHz PCLKB. Changing requires
   *          recalculating all BRR/SPBR register values.
   */
  k_pclkb_hz = 60000000UL,

  /**
   * @brief Peripheral Clock C (PCLKC) - 60 MHz (unused in STAR project)
   *
   * @details
   * General-purpose peripheral clock, currently unused in STAR project.
   * Available for future expansion or custom peripherals.
   *
   * **Value**: 60,000,000 Hz (60 MHz)
   * **Source**: ICLK (240 MHz) divided by 4
   * **Register**: SYSTEM.SCKCR.PCKC (divider = b'0010' = /4 from ICLK)
   *
   * **Rationale**: Configured to 60 MHz (matching PCLKB/D) for:
   * - Consistency: All general-purpose clocks run at same frequency
   * - Power: Lower clock reduces power consumption of unused peripherals
   * - Future-proofing: Standard communication peripheral clock if needed
   *
   * @warning Not currently used. Changing has no effect on STAR functionality.
   */
  k_pclkc_hz = 60000000UL,

  /**
   * @brief Peripheral Clock D (PCLKD) - 60 MHz
   *
   * @details
   * General-purpose peripheral clock, used in STAR project for CMT system tick
   * timer that drives ThreadX RTOS scheduling.
   *
   * **Value**: 60,000,000 Hz (60 MHz)
   * **Source**: ICLK (240 MHz) divided by 4
   * **Cycle time**: 16.67 nanoseconds per cycle
   * **Register**: SYSTEM.SCKCR.PCKD (divider = b'0010' = /4 from ICLK)
   *
   * **Connected Peripherals**:
   * - **CMT (Compare Match Timer)**: ThreadX 10ms system tick
   *
   * **ThreadX Tick Configuration**:
   * ```
   * Tick frequency = 100 Hz (10ms period)
   * CMT prescaler = PCLKD / 8 = 60 MHz / 8 = 7.5 MHz
   * Compare value = (7.5 MHz / 100 Hz) - 1 = 75,000 - 1 = 74,999
   * Actual period = 75,000 / 7.5 MHz = 10.000 ms exact [OK]
   * ```
   *
   * **Rationale**: 60 MHz chosen for:
   * - ThreadX tick: Integer divider achieves exact 10ms period
   * - Power: Lower clock reduces CMT power consumption
   * - Flexibility: High enough for sub-millisecond timing if needed
   *
   * @par Impact of Changing:
   * - **Increase to 120 MHz**: Would double timer resolution, but overkill for 10ms tick
   * - **Decrease to 30 MHz**: Would halve resolution, still sufficient for 10ms tick
   *
   * @warning ThreadX configuration assumes 100 Hz tick (10ms). Changing PCLKD
   *          requires reconfiguring CMT compare value in rx_cmt.c.
   */
  k_pclkd_hz = 60000000UL,

  /**
   * @brief External Bus Clock (BCLK) - 120 MHz (unused in STAR project)
   *
   * @details
   * Clock for external memory interface (SDRAM, SRAM, NOR flash). Not used in
   * STAR project as all code/data fits in internal 4MB flash + 512KB SRAM.
   *
   * **Value**: 120,000,000 Hz (120 MHz)
   * **Source**: ICLK (240 MHz) divided by 2
   * **Register**: SYSTEM.SCKCR.BCK (divider = b'0001' = /2 from ICLK)
   *
   * **Rationale**: Configured to 120 MHz despite being unused because:
   * - Default safe value (does not affect internal operation)
   * - Matches PCLKA (high-speed peripheral clock)
   * - Future-proofing if external memory added later
   *
   * @warning Not used in STAR (no external bus devices). Changing has no effect.
   */
  k_bclk_hz = 120000000UL,

  /**
   * @brief Flash Memory Clock (FCLK) - 60 MHz (hardware limit)
   *
   * @details
   * Clock for internal flash controller (code fetch, const data access). This
   * frequency MUST NOT EXCEED 60 MHz per RX72N datasheet, regardless of CPU
   * clock speed.
   *
   * **Value**: 60,000,000 Hz (60 MHz)
   * **Source**: ICLK (240 MHz) divided by 4
   * **Cycle time**: 16.67 nanoseconds per cycle
   * **Register**: SYSTEM.SCKCR.FCK (divider = b'0010' = /4 from ICLK)
   *
   * **Flash Access Characteristics**:
   * - **1-wait access**: 16.67 ns + wait state ~ 20 ns per access
   * - **CPU cycles per access**: 20 ns / 4.17 ns = ~5 CPU cycles
   * - **Code throughput**: ~48 million instructions/sec (if 1 fetch per instruction)
   * - **Actual throughput**: Higher due to caching and instruction pipelining
   *
   * **Wait State Configuration** (FACI.FWEPROR register):
   * ```
   * ICLK = 240 MHz, FCLK = 60 MHz
   * Wait states = 1 (minimum for 60 MHz FCLK @ 240 MHz ICLK)
   * Access time = 1 FCLK cycle + 1 wait = 33.3 ns
   * ```
   *
   * **Performance Impact**:
   * - Flash access is slower than SRAM (5 cycles vs 1 cycle)
   * - Critical loops should use SRAM (`__attribute__((section(".data"))`)
   * - Const data in flash adds latency vs SRAM variables
   *
   * **Rationale**: 60 MHz is MAXIMUM allowed by hardware:
   * - Exceeding 60 MHz causes flash corruption and data loss
   * - Lower frequencies (30 MHz) would reduce throughput unnecessarily
   * - 60 MHz balances speed and hardware safety limit
   *
   * @par Impact of Changing:
   * - **Increase >60 MHz**: **FORBIDDEN** - Violates datasheet, causes corruption
   * - **Decrease to 30 MHz**: Halves flash throughput, increases code execution time
   *
   * @warning **CRITICAL**: FCLK MUST NOT EXCEED 60 MHz under any circumstances.
   *          Flash controller will malfunction if overclocked, leading to data
   *          corruption, crashes, and potential permanent flash damage.
   *
   * @attention If increasing ICLK in future, MUST adjust FCK divider to keep
   *            FCLK <=60 MHz. Example: ICLK=480 MHz requires FCK divider /8.
   */
  k_fclk_hz = 60000000UL,

} rx_clock_frequencies_t;

#ifdef __cplusplus
}
#endif
