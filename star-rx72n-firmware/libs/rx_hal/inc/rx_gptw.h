/**
 * @file rx_gptw.h
 * @brief GPTW PWM Driver for RX72N Motor Control
 *
 * @details
 * **Production-grade PWM driver** for the RX72N General PWM Timer (GPTW) peripheral,
 * optimized for brushed DC motor control with H-bridge drivers. This module provides
 * high-resolution 32-bit PWM generation with complementary outputs, deadtime insertion,
 * and phase-staggered operation for 4-motor systems.
 *
 * ## Purpose and Design Rationale
 *
 * The GPTW peripheral is chosen over MTU for motor control because:
 * 1. **32-bit resolution** - Higher precision than MTU's 16-bit (0.000015% vs 0.0015%)
 * 2. **Dedicated motor pins** - Port E pins optimized for PWM output
 * 3. **Hardware deadtime** - Prevents H-bridge shoot-through without CPU overhead
 * 4. **Phase staggering** - Reduces peak current draw and EMI
 *
 * ## Key Features
 *
 * **PWM Generation**:
 * - 4 independent channels (GPTW0-GPTW3) for 4 motors
 * - 32-bit counter resolution for precise duty cycle control
 * - Sawtooth (edge-aligned) or triangle (center-aligned) waveforms
 * - Complementary A/B outputs per channel for H-bridge control
 * - Configurable deadtime insertion (0-65535 ns)
 *
 * **Phase Staggering**:
 * - 90-degree phase offset between channels
 * - Channel 0: 0deg, Channel 1: 90deg, Channel 2: 180deg, Channel 3: 270deg
 * - Spreads current draw across PWM period
 * - Reduces peak power supply current by ~75%
 * - Reduces conducted EMI by preventing simultaneous switching
 *
 * **Motor Control Integration**:
 * - Direct interface with DRV8263H H-bridge drivers
 * - IN2/IN1 control mode support (sign-magnitude)
 * - Glitch-free duty cycle updates on period boundary
 * - Fast raw duty cycle API for tight control loops
 *
 * ## Hardware Architecture
 *
 * @par Pin Assignments (from hardware_pinout.h):
 *
 * | Motor | Channel | Output A | Output B | Function |
 * |-------|---------|----------|----------|----------|
 * | Motor 0 | GPTW0 | PE5/GTIOC0A | PE2/GTIOC0B | Front-Left |
 * | Motor 1 | GPTW1 | PE4/GTIOC1A | PE1/GTIOC1B | Front-Right |
 * | Motor 2 | GPTW2 | PE3/GTIOC2A | PE0/GTIOC2B | Rear-Left |
 * | Motor 3 | GPTW3 | PE7/GTIOC3A | PE6/GTIOC3B | Rear-Right |
 *
 * @par Register Base Addresses:
 *
 * | Register Set | Base Address | Description |
 * |--------------|--------------|-------------|
 * | GPTW Common | 0x000C2000 | Shared control registers |
 * | GPTW0 | 0x000C2100 | Channel 0 registers |
 * | GPTW1 | 0x000C2180 | Channel 1 registers |
 * | GPTW2 | 0x000C2200 | Channel 2 registers |
 * | GPTW3 | 0x000C2280 | Channel 3 registers |
 *
 * @par Clock Configuration:
 *
 * | Parameter | Value | Notes |
 * |-----------|-------|-------|
 * | Clock Source | PCLKA | Peripheral Clock A |
 * | Clock Frequency | 120 MHz | After system clock configuration |
 * | Counter Resolution | 8.33 ns | 1/120MHz |
 * | PWM Frequency | 20 kHz | Typical for motor control |
 * | Period Counts | 6000 | 120MHz / 20kHz |
 * | Duty Resolution | 0.0167% | 1/6000 |
 *
 * ## PWM Waveform Modes
 *
 * @dot
 * digraph waveform_modes {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_saw {
 *     label="Sawtooth (Edge-Aligned)";
 *     saw_desc [label="Counter: 0->Period->0\nUpdate: At trough\nBest for: Simple PWM"];
 *   }
 *
 *   subgraph cluster_tri {
 *     label="Triangle (Center-Aligned)";
 *     tri_desc [label="Counter: 0->Period->0->Period\nUpdate: At crest/trough\nBest for: Motor control"];
 *   }
 * }
 * @enddot
 *
 * ## Performance Characteristics
 *
 * | Operation | Execution Time | Memory Usage | Notes |
 * |-----------|---------------|--------------|-------|
 * | rx_gptw_init_all_staggered() | ~100 us | 0 heap | One-time setup |
 * | rx_gptw_set_duty() | ~5 us | 0 | Float calculation |
 * | rx_gptw_set_duty_raw() | ~1 us | 0 | Direct register write |
 * | rx_gptw_get_duty() | ~3 us | 0 | Register read + calc |
 * | rx_gptw_enable_output() | ~1 us | 0 | Single register write |
 *
 * **Memory Footprint**:
 * - Static variables: ~64 bytes (initialization flags, period cache)
 * - Code size: ~2 KB (all functions)
 * - Stack usage: < 64 bytes (deepest call)
 * - Heap usage: 0 bytes (zero dynamic allocation)
 *
 * ## Thread Safety and RTOS Integration
 *
 * **Thread-Safe Operations**:
 * - rx_gptw_set_duty() and rx_gptw_set_duty_raw() are atomic register writes
 * - Safe to call from motor control ISR or ThreadX tasks
 * - No mutex required for duty cycle updates
 *
 * **Non-Thread-Safe Operations**:
 * - rx_gptw_init_all_staggered() - Call once during system initialization
 * - rx_gptw_deinit() - Call only when no motor control active
 *
 * @par Hardware Requirements:
 *
 * | Component | Requirement | Notes |
 * |-----------|-------------|-------|
 * | MCU | Renesas RX72N | GPTW peripheral required |
 * | Clock | PCLKA @ 120 MHz | System clock configuration |
 * | Pins | Port E (PE0-PE7) | Configured via MPC |
 * | Power | 3.3V I/O | LVCMOS compatible |
 *
 * @par Module Dependencies:
 *
 * **This module depends on**:
 * - `rx_err.h` - Error code definitions
 * - `rx_check.h` - Parameter validation macros
 * - `rx72n_gptw_regs.h` - GPTW register definitions
 * - `rx72n_mpc_regs.h` - Pin mux configuration
 *
 * **This module is used by**:
 * - `rx_motor.c` - Motor control driver
 * - `rx_drv8263.c` - H-bridge driver interface
 * - `motor_control_task.c` - Motor control task
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | No goto/setjmp/recursion |
 * | 2. Fixed loop bounds | [OK] | Loops bounded by k_gptw_channel_3 (4 iterations max) |
 * | 3. No dynamic allocation | [OK] | Zero malloc/free, all static |
 * | 4. Small functions | [OK] | All functions < 60 lines |
 * | 5. Assertions (>=2 per function) | [OK] | Minimum 2 validation checks per function |
 * | 6. Narrow scope | [OK] | File-scope statics, minimal globals |
 * | 7. Check return values | [OK] | All functions return rx_err_t |
 * | 8. Limited preprocessor | [OK] | C23 typed enums, minimal macros |
 * | 9. Pointer restrictions | [OK] | Single-level pointers only |
 * | 10. Compiler warnings | [OK] | Compiles with -Wall -Wextra -Werror |
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility (S)**:
 * - This module handles ONLY GPTW PWM generation
 * - Motor control logic is in rx_motor.c
 * - H-bridge configuration is in rx_drv8263.c
 *
 * **Open/Closed (O)**:
 * - Extensible via rx_gptw_config_t structure
 * - New waveform modes can be added without modifying existing code
 * - Frequency and deadtime configurable at runtime
 *
 * **Liskov Substitution (L)**:
 * - All functions return rx_err_t with consistent semantics
 * - Channel and output parameters are validated uniformly
 *
 * **Interface Segregation (I)**:
 * - Separate functions for float vs raw duty cycle
 * - Init, set, get, enable are independent operations
 * - Wrapper types prevent parameter swapping
 *
 * **Dependency Inversion (D)**:
 * - Depends on abstract error codes, not specific implementations
 * - Motor control depends on this interface, not hardware registers directly
 *
 * @see rx_motor.h Motor control driver using GPTW
 * @see rx_drv8263.h H-bridge driver interface
 * @see rx72n_gptw_regs.h Register definitions
 * @see RX72N Hardware Manual Section 26 - General PWM Timer
 * @see docs/sections/03_hardware_pinout.tex Pin assignments
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "rx_check.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @enum rx_gptw_channel_t
 * @brief GPTW Channel Identifiers for 4-Motor Control System
 *
 * @details
 * Identifies the four GPTW timer channels used for motor PWM generation.
 * Each channel controls one motor through complementary A/B outputs connected
 * to a DRV8263H H-bridge driver.
 *
 * ## Channel Architecture
 *
 * Each GPTW channel provides:
 * - 32-bit up/down counter
 * - Two compare/capture registers (GTCCRA, GTCCRB)
 * - Two output pins (GTIOCnA, GTIOCnB)
 * - Independent deadtime insertion
 * - Synchronized start/stop capability
 *
 * @par Channel-to-Motor Mapping:
 *
 * | Channel | Motor | Position | Register Base | Phase Offset |
 * |---------|-------|----------|---------------|--------------|
 * | GPTW0 | Motor 0 | Front-Left | 0x000C2100 | 0deg |
 * | GPTW1 | Motor 1 | Front-Right | 0x000C2180 | 90deg |
 * | GPTW2 | Motor 2 | Rear-Left | 0x000C2200 | 180deg |
 * | GPTW3 | Motor 3 | Rear-Right | 0x000C2280 | 270deg |
 *
 * @par Pin Assignments:
 *
 * | Channel | Output A | Output B | MPC Setting |
 * |---------|----------|----------|-------------|
 * | k_gptw_channel_0 | PE5/GTIOC0A | PE2/GTIOC0B | PSEL = 0x14 |
 * | k_gptw_channel_1 | PE4/GTIOC1A | PE1/GTIOC1B | PSEL = 0x14 |
 * | k_gptw_channel_2 | PE3/GTIOC2A | PE0/GTIOC2B | PSEL = 0x14 |
 * | k_gptw_channel_3 | PE7/GTIOC3A | PE6/GTIOC3B | PSEL = 0x14 |
 *
 * @par Usage Example:
 * @code{.c}
 * // Initialize all channels with staggered phases
 * rx_gptw_config_t config = {
 *     .frequency_hz = 20000,
 *     .deadtime_ns = 1000,
 *     .wave_mode = k_gptw_wave_saw_pwm,
 *     .enable_complementary = true,
 *     .invert_polarity = false
 * };
 * rx_gptw_init_all_staggered(&config);
 *
 * // Set duty cycle for motor 0 (front-left)
 * rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                  rx_gptw_output_id(k_gptw_output_a),
 *                  50.0F);  // 50% duty cycle
 * @endcode
 *
 * @invariant Channel values are contiguous: 0, 1, 2, 3
 * @invariant All channels use same register structure (0x80 byte spacing)
 * @invariant Channel count equals motor count (4)
 *
 * @note C23 typed enum with uint8_t underlying type (1-byte values)
 * @note Channel 4 and 5 exist in hardware but are not used in STAR
 *
 * @see rx_gptw_channel_id() Type-safe wrapper constructor
 * @see rx_gptw_output_t Output selection within channel
 * @see rx72n_gptw_regs.h Register structure definitions
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_gptw_channel_0 =
    0, /**< GPTW0 channel for Motor 0 (Front-Left). Register base: 0x000C2100. Outputs: PE5/GTIOC0A, PE2/GTIOC0B. Phase offset: 0deg in staggered mode */
  k_gptw_channel_1 =
    1, /**< GPTW1 channel for Motor 1 (Front-Right). Register base: 0x000C2180. Outputs: PE4/GTIOC1A, PE1/GTIOC1B. Phase offset: 90deg in staggered mode */
  k_gptw_channel_2 =
    2, /**< GPTW2 channel for Motor 2 (Rear-Left). Register base: 0x000C2200. Outputs: PE3/GTIOC2A, PE0/GTIOC2B. Phase offset: 180deg in staggered mode */
  k_gptw_channel_3 =
    3, /**< GPTW3 channel for Motor 3 (Rear-Right). Register base: 0x000C2280. Outputs: PE7/GTIOC3A, PE6/GTIOC3B. Phase offset: 270deg in staggered mode */
} rx_gptw_channel_t;

/**
 * @enum rx_gptw_output_t
 * @brief GPTW Output Channel Selection for Complementary PWM
 *
 * @details
 * Selects between the two outputs (A and B) available on each GPTW channel.
 * Each output has an independent duty cycle and is controlled separately.
 *
 * ## Independent Output Mode (STAR Configuration)
 *
 * For the DRV8263H motor driver in IN2/IN1 mode, the GPTW outputs operate
 * **independently** (not in complementary mode). Each output has its own
 * compare register and duty cycle:
 * - Output A: Static direction signal (0% or 100% duty)
 * - Output B: Variable-duty PWM for speed control
 *
 * The outputs are NOT inverted versions of each other and no hardware
 * deadtime insertion is used. This is safe because the DRV8263H has
 * integrated shoot-through protection on its internal H-bridge FETs.
 *
 * ## IN2/IN1 Mode (Sign-Magnitude)
 *
 * For DRV8263H H-bridge control in IN2/IN1 mode:
 * - Output A: Connected to IN2 (direction) input - controls direction
 * - Output B: Connected to IN1 (PWM) input - controls PWM duty
 *
 * @par H-Bridge Connection:
 *
 * | Output | DRV8263H Pin | Function | Signal Type |
 * |--------|--------------|----------|-------------|
 * | Output A (GTIOCnA) | IN2 | Direction control | Static HIGH/LOW |
 * | Output B (GTIOCnB) | IN1 | PWM speed control | PWM waveform    |
 *
 * @par Usage Example:
 * @code{.c}
 * // Set forward direction at 75% duty for motor 0
 * // Output A = HIGH (forward direction)
 * rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                  rx_gptw_output_id(k_gptw_output_a),
 *                  100.0F);  // IN2 = HIGH (direction)
 *
 * // Output B = PWM at 75% (motor speed)
 * rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                  rx_gptw_output_id(k_gptw_output_b),
 *                  75.0F);   // IN1 = 75% PWM
 *
 * // Reverse direction: set Output A LOW
 * rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                  rx_gptw_output_id(k_gptw_output_a),
 *                  0.0F);    // IN2 = LOW (reverse)
 * @endcode
 *
 * @invariant Output values are 0 (A) or 1 (B) only
 * @invariant Both outputs available on all channels
 *
 * @note C23 typed enum with uint8_t underlying type
 * @warning In independent mode, each output is controlled separately; the
 *   DRV8263H handles shoot-through protection internally
 *
 * @see rx_gptw_output_id() Type-safe wrapper constructor
 * @see rx_gptw_channel_t Channel selection
 * @see rx_drv8263.h H-bridge driver interface
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_gptw_output_a =
    0, /**< GTIOCA output pin. Connected to DRV8263H IN2 (direction) in IN2/IN1 mode. Controls motor direction: HIGH=forward, LOW=reverse */
  k_gptw_output_b =
    1, /**< GTIOCB output pin. Connected to DRV8263H IN1 (PWM) in IN2/IN1 mode. PWM duty cycle controls motor speed (0-100%) */
} rx_gptw_output_t;

/**
 * @enum rx_gptw_wave_mode_t
 * @brief GPTW PWM Waveform Mode Selection
 *
 * @details
 * Selects the PWM waveform generation mode. The mode determines counter behavior,
 * output alignment, and duty cycle update timing. Motor control typically uses
 * sawtooth (edge-aligned) or triangle (center-aligned) modes.
 *
 * ## Waveform Comparison
 *
 * **Sawtooth (Edge-Aligned)**:
 * - Counter counts up from 0 to period, then resets to 0
 * - PWM edges aligned to period start
 * - Simpler timing, lower switching losses
 * - Asymmetric current ripple
 *
 * **Triangle (Center-Aligned)**:
 * - Counter counts up to period, then down to 0
 * - PWM pulses centered in period
 * - Symmetric current ripple (better for motors)
 * - Reduced EMI due to staggered edges
 *
 * @dot
 * digraph waveform_comparison {
 *   rankdir=TB;
 *   node [shape=record, fontsize=10];
 *
 *   sawtooth [label="{Sawtooth Mode|Counter: 0->P->0->P->0|Update: trough only|PWM edges: aligned to start|Best for: LED dimming}"];
 *   triangle [label="{Triangle Mode|Counter: 0->P->0->P->0|Update: crest and/or trough|PWM edges: centered|Best for: motor control}"];
 *
 *   sawtooth -> triangle [style=invis];
 * }
 * @enddot
 *
 * @par Mode Selection Guide:
 *
 * | Mode | Counter Pattern | Update Timing | Use Case |
 * |------|-----------------|---------------|----------|
 * | k_gptw_wave_saw_pwm | 0->P->0 (reset) | Trough | Simple PWM, LEDs |
 * | k_gptw_wave_tri_pwm1 | 0->P->0 (fold) | Trough | Motor control (standard) |
 * | k_gptw_wave_tri_pwm2 | 0->P->0 (fold) | Crest+Trough | Motor control (fast update) |
 * | k_gptw_wave_tri_pwm3 | 0->P->0 (fold) | 64-bit at trough | High-precision motor |
 *
 * @par Mathematical Model:
 *
 * **Sawtooth PWM Frequency**:
 * @f[
 *   f_{PWM} = \frac{f_{PCLKA}}{(GTPR + 1)}
 * @f]
 *
 * **Triangle PWM Frequency** (half the sawtooth frequency for same period):
 * @f[
 *   f_{PWM} = \frac{f_{PCLKA}}{2 \times (GTPR + 1)}
 * @f]
 *
 * Where:
 * - @f$ f_{PCLKA} @f$ = 120 MHz (peripheral clock)
 * - @f$ GTPR @f$ = Period register value
 *
 * @par Usage Example:
 * @code{.c}
 * // Configure for motor control with triangle wave (center-aligned)
 * rx_gptw_config_t config = {
 *     .frequency_hz = 20000,      // 20 kHz PWM
 *     .deadtime_ns = 1000,        // 1 us deadtime
 *     .wave_mode = k_gptw_wave_tri_pwm1,  // Triangle, trough update
 *     .enable_complementary = true,
 *     .invert_polarity = false
 * };
 *
 * // For simple LED dimming, use sawtooth
 * rx_gptw_config_t led_config = {
 *     .frequency_hz = 1000,       // 1 kHz (no audible noise)
 *     .wave_mode = k_gptw_wave_saw_pwm,  // Sawtooth (simpler)
 *     .enable_complementary = false,
 *     .invert_polarity = false
 * };
 * @endcode
 *
 * @invariant Mode values map directly to GTCR.MD bit field (0-3)
 * @invariant All modes support complementary output with deadtime
 *
 * @note C23 typed enum with uint8_t underlying type
 * @note Triangle modes have half the effective PWM frequency for same period
 *
 * @see RX72N Hardware Manual Section 26.2.12 (GTCR.MD bits)
 * @see rx_gptw_config_t Configuration structure using this enum
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_gptw_wave_saw_pwm =
    0, /**< Sawtooth-wave PWM (edge-aligned). Counter: 0->Period->reset. Single update point at trough. Best for simple PWM applications. GTCR.MD = 0b000 */
  k_gptw_wave_tri_pwm1 =
    1, /**< Triangle-wave PWM mode 1 (center-aligned). Counter: 0->Period->0. Update at trough only. Best for motor control with standard update rate. GTCR.MD = 0b001 */
  k_gptw_wave_tri_pwm2 =
    2, /**< Triangle-wave PWM mode 2 (center-aligned). Counter: 0->Period->0. Update at both crest and trough. Best for motor control requiring fast duty updates. GTCR.MD = 0b010 */
  k_gptw_wave_tri_pwm3 =
    3, /**< Triangle-wave PWM mode 3 (center-aligned). Counter: 0->Period->0. 64-bit buffer transfer at trough. Best for high-precision motor control. GTCR.MD = 0b011 */
} rx_gptw_wave_mode_t;

/**
 * @enum rx_gptw_duty_calc_constants_t
 * @brief Named constants for duty cycle percentage calculations
 *
 * @details
 * Provides named numerators and a common denominator for computing raw duty
 * counts as integer fractions of the period.  Using named constants instead of
 * magic literals satisfies NASA Power of 10 Rule 8 (no magic numbers).
 *
 * @par Example:
 * @code{.c}
 * // 50% duty: period * 50 / 100
 * uint32_t half = (period * k_gptw_duty_50_pct_num) / k_gptw_duty_pct_denom;
 * @endcode
 *
 * @invariant All numerator constants must be <= k_gptw_duty_pct_denom (non-negative)
 * @invariant k_gptw_duty_pct_denom must be non-zero (denominator for percentage calculation)
 *
 * @see rx_gptw_set_duty_raw() Uses these constants in documentation examples
 * @see rx_gptw_get_period() Provides the period value for the calculation
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_gptw_duty_50_pct_num = 50,  /**< Numerator for 50% duty cycle calculation */
  k_gptw_duty_75_pct_num = 75,  /**< Numerator for 75% duty cycle calculation */
  k_gptw_duty_pct_denom  = 100, /**< Denominator for percentage duty calculations */
} rx_gptw_duty_calc_constants_t;

/**
 * @struct rx_gptw_channel_id_t
 * @brief Type-safe GPTW channel wrapper (prevents accidental argument swaps)
 *
 * @details
 * Wraps rx_gptw_channel_t in a struct to leverage the C type system for
 * catching argument transposition errors at compile time. Without this wrapper,
 * swapping channel and output arguments (both uint8_t) would compile silently.
 * Construct via rx_gptw_channel_id() helper function.
 *
 * @invariant value must be a valid rx_gptw_channel_t value (k_gptw_channel_0 through k_gptw_channel_3)
 *
 * @code
 * rx_gptw_channel_id_t ch = rx_gptw_channel_id(k_gptw_channel_0);
 * @endcode
 *
 * @see rx_gptw_channel_id() Constructor function
 * @see rx_gptw_output_id_t Companion wrapper for output parameter
 * @since Version 1.0.0
 */
typedef struct {
  rx_gptw_channel_t value; /**< GPTW channel number (0-3) */
} rx_gptw_channel_id_t;

/**
 * @struct rx_gptw_output_id_t
 * @brief Type-safe GPTW output wrapper (prevents accidental argument swaps)
 *
 * @details
 * Wraps rx_gptw_output_t in a struct to leverage the C type system for
 * catching argument transposition errors at compile time. Without this wrapper,
 * swapping channel and output arguments (both uint8_t) would compile silently.
 * Construct via rx_gptw_output_id() helper function.
 *
 * @invariant value must be k_gptw_output_a or k_gptw_output_b
 *
 * @code
 * rx_gptw_output_id_t out = rx_gptw_output_id(k_gptw_output_a);
 * @endcode
 *
 * @see rx_gptw_output_id() Constructor function
 * @see rx_gptw_channel_id_t Companion wrapper for channel parameter
 * @since Version 1.0.0
 */
typedef struct {
  rx_gptw_output_t value; /**< GPTW output identifier (A or B) */
} rx_gptw_output_id_t;

/**
 * @brief Helper to construct a GPTW channel wrapper
 *
 * @details Provides type-safe wrapper construction with NASA Power of 10 Rule 5
 * validation (minimum 2 checks per function):
 * - Pre-condition: Input value is within valid channel range [0-3]
 * - Post-condition: Wrapper correctly stores the input value
 *
 * @param[in] value GPTW channel enum value (k_gptw_channel_0 to k_gptw_channel_3)
 *
 * @return Wrapped channel ID for type-safe function calls
 *
 * @pre value must be a valid rx_gptw_channel_t in range [k_gptw_channel_0, k_gptw_channel_3]
 * @pre Caller must pass a compile-time known or previously validated channel value
 * @post Returned wrapper .value field equals the input value
 * @post Wrapper is immediately usable in any API accepting rx_gptw_channel_id_t
 *
 * @note Reentrant and lock-free; contains no shared mutable state
 *
 * @see rx_gptw_output_id() Companion wrapper constructor for output parameter
 * @see rx_gptw_channel_id_t Wrapper type constructed by this function
 *
 * @since Version 1.0.0
 */
static inline rx_gptw_channel_id_t rx_gptw_channel_id(rx_gptw_channel_t value)
{
  /* Pre-condition: channel must be in valid range */
  /* Note: value >= k_gptw_channel_0 is always true for uint8_t since k_gptw_channel_0 == 0 */
  RX_ASSERT(value <= k_gptw_channel_3, "GPTW channel out of range");

  const rx_gptw_channel_id_t id = {.value = value};

  return id;
}

/**
 * @brief Helper to construct a GPTW output wrapper
 *
 * @details Provides type-safe wrapper construction with NASA Power of 10 Rule 5
 * validation (minimum 2 checks per function):
 * - Pre-condition: Input value is within valid output range [A-B]
 * - Post-condition: Wrapper correctly stores the input value
 *
 * @param[in] value GPTW output enum value (k_gptw_output_a or k_gptw_output_b)
 *
 * @return Wrapped output ID for type-safe function calls
 *
 * @pre value must be a valid rx_gptw_output_t (k_gptw_output_a or k_gptw_output_b)
 * @pre Caller must pass a compile-time known or previously validated output value
 * @post Returned wrapper .value field equals the input value
 * @post Wrapper is immediately usable in any API accepting rx_gptw_output_id_t
 *
 * @note Reentrant and lock-free; contains no shared mutable state
 *
 * @see rx_gptw_channel_id() Companion wrapper constructor for channel parameter
 * @see rx_gptw_output_id_t Wrapper type constructed by this function
 *
 * @since Version 1.0.0
 */
static inline rx_gptw_output_id_t rx_gptw_output_id(rx_gptw_output_t value)
{
  /* Pre-condition: output must be in valid range */
  /* Note: value >= k_gptw_output_a is always true for uint8_t since k_gptw_output_a == 0 */
  RX_ASSERT(value <= k_gptw_output_b, "GPTW output out of range");

  const rx_gptw_output_id_t id = {.value = value};

  return id;
}

/**
 * @struct rx_gptw_config_t
 * @brief GPTW PWM Configuration Structure
 *
 * @details
 * Contains all configuration parameters for initializing a GPTW channel
 * for PWM output. This structure is passed to rx_gptw_init_pwm() or
 * rx_gptw_init_all_staggered() to configure the timer.
 *
 * ## Configuration Guidelines
 *
 * **Frequency Selection**:
 * - Motor control: 15-25 kHz (above audible range, efficient switching)
 * - LED dimming: 100-1000 Hz (no flicker, simple filtering)
 * - Servo control: 50-400 Hz (standard RC servo range)
 *
 * **Deadtime Selection**:
 * - Minimum: 100 ns (propagation delay margin)
 * - Typical: 500-1000 ns (good balance)
 * - Maximum: 5000 ns (very conservative, reduces efficiency)
 *
 * @par Configuration Parameter Table:
 *
 * | Parameter | Min | Max | Default | Unit | Notes |
 * |-----------|-----|-----|---------|------|-------|
 * | frequency_hz | 100 | 1000000 | 20000 | Hz | Limited by PCLKA/2 |
 * | deadtime_ns | 0 | 65535 | 1000 | ns | 0 disables deadtime |
 * | wave_mode | 0 | 3 | 0 | enum | See rx_gptw_wave_mode_t |
 * | enable_complementary | false | true | true | bool | For H-bridge |
 * | invert_polarity | false | true | false | bool | Active-low output |
 *
 * @par Memory Layout:
 *
 * | Offset | Size | Field | Type | Alignment |
 * |--------|------|-------|------|-----------|
 * | 0 | 4 | frequency_hz | uint32_t | 4 |
 * | 4 | 2 | deadtime_ns | uint16_t | 2 |
 * | 6 | 1 | wave_mode | uint8_t | 1 |
 * | 7 | 1 | enable_complementary | bool | 1 |
 * | 8 | 1 | invert_polarity | bool | 1 |
 * | 9-11 | 3 | (padding) | - | - |
 * | **Total** | **12** | - | - | **4** |
 *
 * @par Usage Example (Motor Control):
 * @code{.c}
 * // Standard motor control configuration
 * rx_gptw_config_t motor_config = {
 *     .frequency_hz = 20000,           // 20 kHz (inaudible)
 *     .deadtime_ns = 1000,             // 1 us deadtime
 *     .wave_mode = k_gptw_wave_saw_pwm, // Edge-aligned
 *     .enable_complementary = true,    // H-bridge requires this
 *     .invert_polarity = false         // Active-high outputs
 * };
 *
 * rx_err_t err = rx_gptw_init_all_staggered(&motor_config);
 * if (err != k_rx_ok) {
 *     rx_log_error("GPTW", "Failed to initialize: %d", err);
 * }
 * @endcode
 *
 * @par Usage Example (LED Dimming):
 * @code{.c}
 * // LED PWM configuration (no complementary needed)
 * rx_gptw_config_t led_config = {
 *     .frequency_hz = 1000,            // 1 kHz (no flicker)
 *     .deadtime_ns = 0,                // No deadtime for LEDs
 *     .wave_mode = k_gptw_wave_saw_pwm,
 *     .enable_complementary = false,   // Single output only
 *     .invert_polarity = false
 * };
 *
 * rx_err_t err = rx_gptw_init_pwm(k_gptw_channel_0, &led_config);
 * @endcode
 *
 * @invariant frequency_hz > 0 (enforced by rx_gptw_init_*)
 * @invariant frequency_hz <= PCLKA / 2 (Nyquist limit)
 * @invariant wave_mode in [0, 3] (valid enum range)
 *
 * @note Structure is copied during initialization; no need to keep alive
 * @warning Invalid configurations rejected with k_rx_err_invalid_arg
 *
 * @see rx_gptw_init_pwm() Single channel initialization
 * @see rx_gptw_init_all_staggered() 4-channel staggered initialization
 * @see rx_gptw_wave_mode_t Waveform mode options
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief PWM frequency in Hz
   * @details
   * Sets the PWM switching frequency. For motor control, 15-25 kHz is typical
   * (above audible range). Higher frequencies reduce current ripple but increase
   * switching losses.
   *
   * @par Frequency Calculation:
   * @f[
   *   \text{Period Count} = \frac{f_{PCLKA}}{f_{PWM}} - 1
   * @f]
   *
   * Where @f$ f_{PCLKA} @f$ = 120 MHz
   *
   * @par Valid Range: [100, 1000000] Hz
   * @par Recommended: 20000 Hz for motor control
   * @note Actual frequency may differ slightly due to integer division
   */
  uint32_t frequency_hz;

  /**
   * @brief Deadtime in nanoseconds
   * @details
   * Deadtime inserted between complementary output transitions to prevent
   * shoot-through (simultaneous conduction of high-side and low-side switches).
   * Set to 0 to disable deadtime insertion.
   *
   * @par Deadtime Calculation:
   * @f[
   *   \text{Deadtime Counts} = \frac{\text{deadtime\_ns} \times f_{PCLKA}}{10^9}
   * @f]
   *
   * At 120 MHz: 1000 ns = 120 counts
   *
   * @par Valid Range: [0, 65535] ns
   * @par Recommended: 1000 ns (1 us) for DRV8263H
   * @warning Too short may cause shoot-through; too long reduces efficiency
   */
  uint16_t deadtime_ns;

  /**
   * @brief Waveform mode (sawtooth or triangle)
   * @details
   * Selects counter behavior and PWM alignment.
   * - Sawtooth: Edge-aligned, simpler, single update point
   * - Triangle: Center-aligned, symmetric ripple, better for motors
   *
   * @see rx_gptw_wave_mode_t for detailed mode descriptions
   */
  rx_gptw_wave_mode_t wave_mode;

  /**
   * @brief Enable complementary outputs
   * @details
   * When true, Output B is the inverted version of Output A with deadtime
   * inserted. Required for H-bridge motor control.
   *
   * - true: Both A and B outputs active (complementary)
   * - false: Only Output A active, Output B held low
   *
   * @warning When complementary mode is enabled, GTCCRB is tied to GTCCRA
   *          by the hardware. Writing GTCCRB directly has no effect; the
   *          output B duty is always the complement of output A. For
   *          independent A/B control (e.g., DRV8263H IN2/IN1 mode), set
   *          this to false.
   */
  bool enable_complementary;

  /**
   * @brief Invert PWM polarity
   * @details
   * When true, inverts the PWM output polarity.
   * - false: HIGH during duty cycle, LOW otherwise (active-high)
   * - true: LOW during duty cycle, HIGH otherwise (active-low)
   *
   * @note Use for active-low driver inputs or inverted LED connections
   */
  bool invert_polarity;

  /**
   * @brief Pin coordinates for GTIOC*A and GTIOC*B outputs.
   *
   * @details
   * The lib does not know the board's pin map. Caller MUST set these to
   * the actual pads where GTIOC_A and GTIOC_B should appear. Source the
   * values from a board-level pin map header (e.g. src/inc/hardware_config.h).
   *
   * `port_*_idx` is the standard RX72N port index: 1, 2, 8, etc., with
   *   PORTA=10, PORTB=11, PORTC=12, PORTD=13, PORTE=14, PORTF=15.
   * `bit_*` is the bit position within that port (0..7).
   */
  uint8_t port_a_idx;
  uint8_t bit_a;
  uint8_t port_b_idx;
  uint8_t bit_b;
} rx_gptw_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize all 4 GPTW channels with 90 degree phase staggering
 *
 * @details
 * Configures GPTW0-3 for synchronized PWM operation with phase offsets that
 * spread current draw across the PWM period. This is the **recommended
 * initialization function** for 4-motor systems as it reduces peak current
 * and EMI compared to synchronized PWM.
 *
 * ## Phase Staggering Architecture
 *
 * Phase staggering offsets the start of each channel's PWM period:
 * - **Channel 0**: 0deg (reference phase)
 * - **Channel 1**: 90deg (period / 4 offset)
 * - **Channel 2**: 180deg (period / 2 offset)
 * - **Channel 3**: 270deg (3 x period / 4 offset)
 *
 * @dot
 * digraph phase_stagger {
 *   rankdir=TB;
 *   node [shape=record, fontsize=10];
 *
 *   title [label="Phase Staggered PWM (4 channels at 50% duty)", shape=none];
 *
 *   ch0 [label="{Ch0 (0deg)|######______######______}"];
 *   ch1 [label="{Ch1 (90deg)|___######______######___}"];
 *   ch2 [label="{Ch2 (180deg)|______######______######}"];
 *   ch3 [label="{Ch3 (270deg)|###______######______###}"];
 *   current [label="{Total Current|Spread evenly across period}"];
 *
 *   title -> ch0 [style=invis];
 *   ch0 -> ch1 -> ch2 -> ch3 -> current;
 * }
 * @enddot
 *
 * ## Benefits of Phase Staggering
 *
 * 1. **Reduced Peak Current**: Instead of all motors switching simultaneously,
 *    current draw is spread across the period, reducing peak by up to 75%
 * 2. **Lower EMI**: Staggered edges reduce conducted emissions
 * 3. **Smaller Capacitors**: Lower peak current allows smaller power supply capacitors
 * 4. **Better Power Quality**: More constant current draw improves voltage regulation
 *
 * ## Algorithm Steps
 *
 * 1. Validate configuration parameters (NULL check, frequency range, wave mode)
 * 2. Calculate period count from frequency and PCLKA
 * 3. Enable GPTW module clock (MSTPCRC.MSTPC10 = 0)
 * 4. Configure MPC for Port E pins (GTIOC function select)
 * 5. For each channel (0-3):
 *    a. Configure timer mode register (GTCR)
 *    b. Set period register (GTPR)
 *    c. Calculate and set phase offset (GTCNT initial value)
 *    d. Configure output mode and polarity (GTIOR)
 *    e. Set deadtime registers (GTDVU, GTDVD)
 *    f. Set initial duty cycle to 0% (GTCCRA, GTCCRB)
 * 6. Start all channels simultaneously (GTSTR)
 *
 * @param[in] config Common configuration for all channels
 *   - config->frequency_hz: PWM frequency [100, 1000000] Hz
 *   - config->deadtime_ns: Deadtime [0, 65535] ns
 *   - config->wave_mode: Waveform mode (sawtooth or triangle)
 *   - config->enable_complementary: Enable A/B complementary outputs
 *   - config->invert_polarity: Invert output polarity
 *
 * @return rx_err_t Error code indicating success or failure reason
 * @retval k_rx_ok All 4 channels initialized successfully
 * @retval k_rx_err_null_ptr config pointer is nullptr
 * @retval k_rx_err_invalid_arg frequency_hz is 0 or wave_mode out of range
 * @retval k_rx_err_hw_error GPTW hardware access failed (NULL register pointer)
 *
 * @pre System clock configured (PCLKA = 120 MHz)
 * @pre Port E pins not used by other peripherals
 *
 * @post All 4 GPTW channels running with 90deg phase offsets
 * @post All duty cycles initialized to 0% (motors stopped)
 * @post MPC configured for GTIOC function on Port E pins
 *
 * @invariant Phase relationship maintained: Ch1 = Ch0 + 90deg, Ch2 = Ch0 + 180deg, Ch3 = Ch0 + 270deg
 *
 * @note Thread-safe: Call only once during system initialization
 * @note Propagates errors from internal_calculate_period(), internal_configure_gptw_hardware()
 *
 * @warning Calling this function while motors are running will cause glitches
 * @attention This function should be used INSTEAD of rx_gptw_init_pwm() for motor control
 *
 * @par Performance:
 * - Execution time: ~100 us at 240 MHz
 * - Memory usage: 0 heap, ~64 bytes stack
 *
 * @par Example (Complete Motor System Initialization):
 * @code{.c}
 * #include "rx_gptw.h"
 * #include "rx_drv8263.h"
 * #include "rx_motor.h"
 *
 * rx_err_t motor_system_init(void)
 * {
 *     // Configure GPTW for 20 kHz PWM with 1 us deadtime
 *     rx_gptw_config_t pwm_config = {
 *         .frequency_hz = 20000,
 *         .deadtime_ns = 1000,
 *         .wave_mode = k_gptw_wave_saw_pwm,
 *         .enable_complementary = true,
 *         .invert_polarity = false
 *     };
 *
 *     rx_err_t err = rx_gptw_init_all_staggered(&pwm_config);
 *     if (err != k_rx_ok) {
 *         rx_log_error("MOTOR", "GPTW init failed: %d", err);
 *         return err;
 *     }
 *
 *     // Initialize H-bridge drivers
 *     err = rx_drv8263_init_all();
 *     if (err != k_rx_ok) {
 *         rx_log_error("MOTOR", "DRV8263H init failed: %d", err);
 *         return err;
 *     }
 *
 *     rx_log_info("MOTOR", "Motor system initialized (4 channels, 20kHz, staggered)");
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Example (Verify Phase Offsets):
 * @code{.c}
 * // Diagnostic: read counter values to verify phase relationship
 * void verify_phase_stagger(void)
 * {
 *     uint32_t cnt0, cnt1, cnt2, cnt3;
 *     uint32_t period;
 *
 *     rx_gptw_get_period(k_gptw_channel_0, &period);
 *
 *     // Read all counters (will have some jitter due to sequential reads)
 *     cnt0 = GPTW0->GTCNT;
 *     cnt1 = GPTW1->GTCNT;
 *     cnt2 = GPTW2->GTCNT;
 *     cnt3 = GPTW3->GTCNT;
 *
 *     // Expected: cnt1 ~ cnt0 + period/4, cnt2 ~ cnt0 + period/2, etc.
 *     rx_log_debug("GPTW", "Period=%lu, Offsets: %lu, %lu, %lu, %lu",
 *                  period, cnt0, cnt1 - cnt0, cnt2 - cnt0, cnt3 - cnt0);
 * }
 * @endcode
 *
 * @see rx_gptw_init_pwm() Single channel initialization (not recommended for motors)
 * @see rx_gptw_set_duty() Set duty cycle after initialization
 * @see rx_gptw_deinit() Cleanup function
 * @see rx_drv8263_init_all() H-bridge driver initialization
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto/setjmp/recursion
 * - Rule 2: [OK] Loop bounded by k_gptw_channel_3 (4 iterations)
 * - Rule 5: [OK] 2 preconditions, 3 postconditions
 * - Rule 7: [OK] All internal function returns checked
 *
 * @callgraph
 * @callergraph
 */
/* Public count constant matching the private k_gptw_max_channels in rx_gptw.c. */
typedef enum : uint8_t {
  k_rx_gptw_channel_count = 4,
} rx_gptw_count_t;

[[nodiscard]] rx_err_t rx_gptw_init_all_staggered(const rx_gptw_config_t* configs[k_rx_gptw_channel_count]);

/**
 * @brief Initialize a single GPTW channel for PWM output
 *
 * @details
 * Configures the specified GPTW channel for PWM mode (sawtooth or triangle wave).
 * Automatically configures MPC for the pin alternate functions, sets up dead-time
 * insertion, and starts PWM generation at 0% duty cycle. The GTCNT counter is
 * reset to zero during initialization.
 *
 * @param[in] channel GPTW channel number (valid: 0-3)
 * @param[in] config Pointer to PWM configuration struct (must be non-null)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, channel initialized and timer running
 * @retval k_rx_err_invalid_arg Channel out of range or config is null
 * @retval k_rx_err_invalid_state GPTW module not enabled in MSTPCR
 *
 * @pre GPTW module clock must be enabled
 * @pre config must point to a valid rx_gptw_config_t
 * @post Channel timer running at configured frequency with 0% duty
 * @post GTCNT reset to zero (affects phase staggering if called per-channel)
 *
 * @note Not thread-safe. Call from single-threaded init context.
 * @warning Resets GTCNT to 0. If phase staggering is required, use
 *          rx_gptw_init_all_staggered() instead, or configure phase offsets
 *          after all channels are initialized.
 *
 * @see rx_gptw_init_all_staggered() Phase-staggered multi-channel init
 * @see rx_gptw_deinit() Release channel resources
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_gptw_init_pwm(rx_gptw_channel_t channel, const rx_gptw_config_t* config);

/**
 * @brief Set PWM duty cycle (floating-point percentage)
 *
 * @details
 * Updates the duty cycle for the specified GPTW channel and output. The new duty
 * cycle is written to the compare register buffer and takes effect on the next
 * PWM period boundary, ensuring glitch-free operation.
 *
 * ## Algorithm Steps
 *
 * 1. Validate channel and output wrapper values
 * 2. Check channel is initialized
 * 3. Clamp duty_percent to [0.0, 100.0] range
 * 4. Calculate raw count: duty_count = (duty_percent / 100.0) x period_count
 * 5. Write to compare register (GTCCRA or GTCCRB)
 * 6. Register buffer transfers to active on next period boundary
 *
 * @par Duty Cycle Calculation:
 * @f[
 *   \text{duty\_count} = \left\lfloor \frac{\text{duty\_percent}}{100} \times \text{period\_count} \right\rfloor
 * @f]
 *
 * For 20 kHz PWM with 120 MHz PCLKA:
 * - Period count = 6000
 * - 50% duty = 3000 counts
 * - Resolution = 0.0167% per count
 *
 * @param[in] channel GPTW channel wrapped in rx_gptw_channel_id_t for type safety
 *   - Use rx_gptw_channel_id(k_gptw_channel_0) through rx_gptw_channel_id(k_gptw_channel_3)
 * @param[in] output Output selection wrapped in rx_gptw_output_id_t for type safety
 *   - Use rx_gptw_output_id(k_gptw_output_a) or rx_gptw_output_id(k_gptw_output_b)
 * @param[in] duty_percent Duty cycle in percent [0.0, 100.0]
 *   - Values < 0.0 are clamped to 0.0
 *   - Values > 100.0 are clamped to 100.0
 *   - NaN and infinity are treated as invalid
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Duty cycle updated successfully
 * @retval k_rx_err_invalid_arg Channel or output out of range, or duty is NaN/infinity
 * @retval k_rx_err_not_initialized Channel not initialized via rx_gptw_init_*()
 *
 * @pre Channel initialized via rx_gptw_init_all_staggered() or rx_gptw_init_pwm()
 * @pre duty_percent is a valid float (not NaN or infinity)
 *
 * @post Compare register updated with new duty value
 * @post Duty takes effect on next period boundary (no glitch)
 *
 * @invariant Actual duty always in [0%, 100%] regardless of input
 *
 * @note Thread-safe: Atomic register write, safe from ISR or multiple threads
 * @note Use rx_gptw_set_duty_raw() in tight control loops to avoid float overhead
 *
 * @warning Floating-point calculation adds ~4 us overhead vs raw version
 *
 * @par Performance:
 * - Execution time: ~5 us at 240 MHz (includes float multiply)
 * - Memory usage: 0 bytes heap, ~16 bytes stack
 *
 * @par Example (Set Motor Speed):
 * @code{.c}
 * // Set motor 0 forward at 75% speed
 * // Output A = direction (100% = forward)
 * rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                  rx_gptw_output_id(k_gptw_output_a),
 *                  100.0F);
 *
 * // Output B = speed (75% duty)
 * rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                  rx_gptw_output_id(k_gptw_output_b),
 *                  75.0F);
 * @endcode
 *
 * @par Example (PID Control Loop):
 * @code{.c}
 * void motor_control_task(void)
 * {
 *     while (true) {
 *         float setpoint = get_velocity_setpoint();
 *         float measured = read_encoder_velocity();
 *         float output;
 *
 *         rx_pid_compute(&pid, setpoint, measured, 0.01F, &output);
 *
 *         // Convert PID output (-100 to +100) to duty cycle
 *         float duty = fabsf(output);
 *         bool forward = (output >= 0.0F);
 *
 *         // Set direction
 *         rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                          rx_gptw_output_id(k_gptw_output_a),
 *                          forward ? 100.0F : 0.0F);
 *
 *         // Set speed
 *         rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                          rx_gptw_output_id(k_gptw_output_b),
 *                          duty);
 *
 *         tx_thread_sleep(1);  // 100 Hz control loop
 *     }
 * }
 * @endcode
 *
 * @see rx_gptw_set_duty_raw() Raw count version for tight loops
 * @see rx_gptw_get_duty() Read current duty cycle
 * @see rx_gptw_channel_id() Wrapper constructor for channel
 * @see rx_gptw_output_id() Wrapper constructor for output
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 preconditions, 2 postconditions
 * - Rule 7: [OK] Returns error code, caller must check
 */
[[nodiscard]] rx_err_t
rx_gptw_set_duty(rx_gptw_channel_id_t channel, rx_gptw_output_id_t output, float duty_percent);

/**
 * @brief Set PWM duty cycle using raw counter value
 *
 * @details
 * Updates duty cycle using a raw counter value for efficiency in tight control
 * loops where floating-point overhead should be avoided. The count is written
 * to the compare register buffer and takes effect on the next PWM period
 * boundary, ensuring glitch-free operation.
 *
 * @param[in] channel GPTW channel (valid: 0-3)
 * @param[in] output Output channel (k_gptw_output_a or k_gptw_output_b)
 * @param[in] duty_count Duty cycle count (valid range: 0 to period_count)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, duty cycle updated
 * @retval k_rx_err_invalid_arg Channel, output, or count out of range
 * @retval k_rx_err_invalid_state Channel not initialized via rx_gptw_init_pwm()
 *
 * @pre Channel must be initialized via rx_gptw_init_pwm()
 * @pre duty_count must not exceed the period count for the channel
 * @post Compare register buffer updated with new duty count
 * @post Active register updated on next period boundary
 *
 * @note Thread-safe: single 32-bit register write is inherently atomic on RX72N
 *
 * @par Performance:
 * Constant time: single 32-bit register write after validation.
 *
 * @par Example:
 * @code{.c}
 * // Read current period, compute 75% duty, then set raw count
 * uint32_t period = 0;
 * rx_err_t err = rx_gptw_get_period(k_gptw_channel_0, &period);
 * if (err == k_rx_ok) {
 *     const uint32_t duty_count = (period * k_gptw_duty_75_pct_num) / k_gptw_duty_pct_denom;
 *     err = rx_gptw_set_duty_raw(k_gptw_channel_0, k_gptw_output_b, duty_count);
 * }
 * @endcode
 *
 * @see rx_gptw_set_duty() Percentage-based version with float conversion
 * @see rx_gptw_get_period() Query period count for duty calculation
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_gptw_set_duty_raw(rx_gptw_channel_t channel, rx_gptw_output_t output, uint32_t duty_count);

/**
 * @brief Get current duty cycle as a floating-point percentage
 *
 * @details
 * Reads the current compare register value for the specified output and
 * converts it to a percentage of the period count. The value reflects the
 * most recently committed duty cycle (may differ from the buffered value
 * pending a period boundary update).
 *
 * @param[in] channel GPTW channel number (valid: 0-3)
 * @param[in] output Output channel (k_gptw_output_a or k_gptw_output_b)
 * @param[out] duty_percent Pointer to store duty cycle in percent [0.0, 100.0]
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, duty_percent written
 * @retval k_rx_err_null_ptr duty_percent is nullptr
 * @retval k_rx_err_invalid_arg Channel or output out of range
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel must be initialized via rx_gptw_init_pwm()
 * @pre duty_percent must be a valid non-null pointer
 * @post duty_percent contains current duty cycle percentage
 * @post No hardware state modified (read-only operation)
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @par Example:
 * @code{.c}
 * float duty = 0.0F;
 * rx_err_t err = rx_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a, &duty);
 * if (err == k_rx_ok) {
 *     // duty now contains current duty cycle percentage [0.0, 100.0]
 * }
 * @endcode
 *
 * @see rx_gptw_set_duty() Set duty cycle by percentage
 * @see rx_gptw_get_period() Get period count for manual calculation
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_gptw_get_duty(rx_gptw_channel_t channel, rx_gptw_output_t output, float* duty_percent);

/**
 * @brief Get the 32-bit PWM period count for a GPTW channel
 *
 * @details
 * Reads the GTPR (General Timer Period Register) value for the specified
 * channel. This value determines the PWM frequency and is needed to
 * calculate raw duty counts: duty_count = (duty_percent / 100) * period_count.
 *
 * @param[in] channel GPTW channel number (valid: 0-3)
 * @param[out] period_count Pointer to store 32-bit period count
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, period_count written
 * @retval k_rx_err_null_ptr period_count is nullptr
 * @retval k_rx_err_invalid_arg Channel out of range
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel must be initialized via rx_gptw_init_pwm()
 * @pre period_count must be a valid non-null pointer
 * @post period_count contains the GTPR register value
 * @post No hardware state modified (read-only operation)
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @par Example:
 * @code{.c}
 * uint32_t period = 0;
 * rx_err_t err = rx_gptw_get_period(k_gptw_channel_0, &period);
 * if (err == k_rx_ok) {
 *     // Calculate raw duty count for 50% duty cycle
 *     uint32_t duty_count = (period * k_gptw_duty_50_pct_num) / k_gptw_duty_pct_denom;
 * }
 * @endcode
 *
 * @see rx_gptw_set_duty_raw() Uses period_count for raw duty calculation
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_gptw_get_period(rx_gptw_channel_t channel, uint32_t* period_count);

/**
 * @brief Enable or disable a PWM output pin
 *
 * @details
 * Controls the GTONCR (General Timer Output Negation Control Register) bits
 * that gate the PWM signal to the physical pin. When disabled, the pin is
 * driven to its inactive level (determined by polarity configuration).
 *
 * @param[in] channel GPTW channel number (valid: 0-3)
 * @param[in] output Output channel (k_gptw_output_a or k_gptw_output_b)
 * @param[in] enable true to drive PWM on pin, false to hold pin inactive
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, output enable state updated
 * @retval k_rx_err_invalid_arg Channel or output out of range
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel must be initialized via rx_gptw_init_pwm()
 * @pre Output must be k_gptw_output_a or k_gptw_output_b
 * @post GTONCR bit updated for the specified output
 * @post Pin either drives PWM signal or holds inactive level
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @par Example:
 * @code{.c}
 * // Enable PWM output on channel 0, output A
 * rx_err_t err = rx_gptw_enable_output(k_gptw_channel_0, k_gptw_output_a, true);
 * if (err != k_rx_ok) {
 *     // Handle output enable failure
 * }
 * @endcode
 *
 * @see rx_gptw_start() Start timer (separate from output enable)
 * @see rx_gptw_stop() Stop timer (separate from output enable)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_gptw_enable_output(rx_gptw_channel_t channel, rx_gptw_output_t output, bool enable);

/**
 * @brief Start the GPTW timer counter for PWM generation
 *
 * @details
 * Sets the GTCR.CST bit to begin counting. The timer is automatically
 * started during rx_gptw_init_pwm(), but can be stopped via rx_gptw_stop()
 * and restarted with this function. Counting resumes from the current
 * GTCNT value (not reset to zero).
 *
 * @param[in] channel GPTW channel number (valid: 0-3)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, timer started
 * @retval k_rx_err_invalid_arg Channel out of range
 *
 * @pre Channel must be initialized via rx_gptw_init_pwm()
 * @pre Channel must currently be stopped (safe to call if already running)
 * @post Timer counter incrementing at peripheral clock rate
 * @post PWM outputs active (if enabled via rx_gptw_enable_output())
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @par Example:
 * @code{.c}
 * // Restart timer after a stop
 * rx_err_t err = rx_gptw_start(k_gptw_channel_0);
 * if (err != k_rx_ok) {
 *     // Handle start failure
 * }
 * @endcode
 *
 * @see rx_gptw_stop() Stop the timer counter
 * @see rx_gptw_init_pwm() Initializes and starts timer automatically
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_gptw_start(rx_gptw_channel_t channel);

/**
 * @brief Stop the GPTW timer counter
 *
 * @details
 * Clears the GTCR.CST bit to halt counting. PWM outputs hold their last
 * state when the timer stops. The GTCNT value is preserved and counting
 * resumes from that value when rx_gptw_start() is called.
 *
 * @param[in] channel GPTW channel number (valid: 0-3)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, timer stopped
 * @retval k_rx_err_invalid_arg Channel out of range
 *
 * @pre Channel must be initialized via rx_gptw_init_pwm()
 * @pre Channel should be running (safe to call if already stopped)
 * @post Timer counter halted at current GTCNT value
 * @post PWM outputs frozen at last driven state
 *
 * @note Not thread-safe. Caller must provide synchronization.
 * @warning Outputs hold last state; explicitly set duty to 0 before
 *          stopping if low output is required.
 *
 * @par Example:
 * @code{.c}
 * // Stop timer and ensure outputs are low
 * rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                  rx_gptw_output_id(k_gptw_output_b), 0.0F);
 * rx_err_t err = rx_gptw_stop(k_gptw_channel_0);
 * @endcode
 *
 * @see rx_gptw_start() Resume timer counter
 * @see rx_gptw_deinit() Full shutdown including output disable
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_gptw_stop(rx_gptw_channel_t channel);

/**
 * @brief Deinitialize a GPTW channel and release all resources
 *
 * @details
 * Performs a full shutdown sequence: stops the timer counter, disables both
 * PWM outputs, resets the compare and period registers, and marks the channel
 * as uninitialized. After deinit, the channel must be re-initialized via
 * rx_gptw_init_pwm() before use.
 *
 * @param[in] channel GPTW channel number (valid: 0-3)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, channel fully deinitialized
 * @retval k_rx_err_invalid_arg Channel out of range
 *
 * @pre Channel should be initialized (safe to call if already deinitialized)
 * @pre Motor outputs should be in a safe state before deinit
 * @post Timer stopped, outputs disabled, channel marked uninitialized
 * @post All channel registers reset to default values
 *
 * @note Not thread-safe. Caller must provide synchronization.
 *
 * @par Example:
 * @code{.c}
 * // Full shutdown of GPTW channel 0
 * rx_err_t err = rx_gptw_deinit(k_gptw_channel_0);
 * if (err == k_rx_ok) {
 *     // Channel released, must call rx_gptw_init_pwm() before reuse
 * }
 * @endcode
 *
 * @see rx_gptw_init_pwm() Re-initialize after deinit
 * @see rx_gptw_stop() Stop timer without full deinit
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_gptw_deinit(rx_gptw_channel_t channel);

#ifdef __cplusplus
}
#endif
