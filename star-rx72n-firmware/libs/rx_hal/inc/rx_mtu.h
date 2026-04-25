/**
 * @file rx_mtu.h
 * @brief MTU PWM Driver for RX72N Multi-Function Timer Unit
 *
 * @details
 * **Production-grade PWM driver** for the RX72N Multi-Function Timer Unit (MTU3)
 * peripheral. This module provides 16-bit PWM generation for motor control,
 * quadrature encoder input, and general-purpose timing applications.
 *
 * ## Purpose and Design Rationale
 *
 * The MTU (Multi-Function Timer Unit) is a versatile timer peripheral that
 * complements the GPTW module:
 *
 * | Feature | MTU3 | GPTW |
 * |---------|------|------|
 * | Counter Width | 16-bit | 32-bit |
 * | Channels | 8 (MTU0-4, MTU6-7) | 4 (GPTW0-3) |
 * | Outputs per Channel | Up to 4 (A/B/C/D) | 2 (A/B) |
 * | Phase Counting | Yes (encoder input) | Limited |
 * | Complementary PWM | Yes (with deadtime) | Yes |
 * | Primary Use | Encoders, PWM | Motor PWM |
 *
 * **STAR Project Usage**:
 * - MTU1/MTU2: Quadrature encoder input (see rx_mtu_encoder.h)
 * - GPTW0-3: Motor PWM output (see rx_gptw.h) - **recommended for motors**
 *
 * ## Key Features
 *
 * **PWM Generation**:
 * - 8 timer channels with independent configuration
 * - PWM mode 1: Center-aligned (triangle wave) for symmetric current ripple
 * - PWM mode 2: Edge-aligned (sawtooth wave) for simple applications
 * - Complementary outputs with configurable deadtime
 * - Up to 4 outputs per channel (A/B/C/D) on some channels
 *
 * **Encoder Interface** (MTU1/MTU2):
 * - Phase counting mode for quadrature encoders
 * - 16-bit pulse count with overflow detection
 * - See rx_mtu_encoder.h for encoder-specific functions
 *
 * **Timing Features**:
 * - Input capture for pulse width measurement
 * - Output compare for waveform generation
 * - Cascade operation for extended range
 *
 * ## Hardware Architecture
 *
 * @par Register Base Addresses:
 *
 * | Channel | Base Address | Outputs | Notes |
 * |---------|--------------|---------|-------|
 * | MTU0 | 0x000C1300 | A,B,C,D | 4 outputs |
 * | MTU1 | 0x000C1380 | A,B | Encoder input |
 * | MTU2 | 0x000C1400 | A,B | Encoder input |
 * | MTU3 | 0x000C1200 | A,B,C,D | 4 outputs, pairs with MTU4 |
 * | MTU4 | 0x000C1201 | A,B,C,D | 4 outputs, pairs with MTU3 |
 * | MTU6 | 0x000C1A00 | A,B,C,D | 4 outputs, pairs with MTU7 |
 * | MTU7 | 0x000C1A01 | A,B,C,D | 4 outputs, pairs with MTU6 |
 *
 * Note: MTU5 (U/V/W sub-channels at 0x000C1C80/90/A0) is not supported by this HAL.
 *
 * @par Clock Configuration:
 *
 * | Parameter | Value | Notes |
 * |-----------|-------|-------|
 * | Clock Source | PCLKA | Peripheral Clock A |
 * | Clock Frequency | 120 MHz | After system clock configuration |
 * | Counter Resolution | 8.33 ns | 1/120MHz |
 * | Max Period (16-bit) | 546 us | 65535 counts at 120 MHz |
 * | Min PWM Frequency | 1831 Hz | 1 / 546 us |
 *
 * ## PWM Modes
 *
 * **PWM Mode 1 (Center-Aligned)**:
 * - Counter counts up to period, then down to 0
 * - Symmetric current ripple (better for motors)
 * - Effective PWM frequency = PCLKA / (2 x period)
 *
 * **PWM Mode 2 (Edge-Aligned)**:
 * - Counter counts up to period, then resets
 * - Asymmetric ripple
 * - Effective PWM frequency = PCLKA / period
 *
 * @par PWM Frequency Calculation:
 *
 * For center-aligned (PWM mode 1):
 * @f[
 *   f_{PWM} = \frac{f_{PCLKA}}{2 \times (\text{TGRA} + 1)}
 * @f]
 *
 * For edge-aligned (PWM mode 2):
 * @f[
 *   f_{PWM} = \frac{f_{PCLKA}}{\text{TGRA} + 1}
 * @f]
 *
 * ## Performance Characteristics
 *
 * | Operation | Execution Time | Memory Usage | Notes |
 * |-----------|---------------|--------------|-------|
 * | rx_mtu_init_pwm() | ~50 us | 0 heap | One-time setup |
 * | rx_mtu_set_duty() | ~5 us | 0 | Float calculation |
 * | rx_mtu_set_duty_raw() | ~1 us | 0 | Direct register write |
 * | rx_mtu_enable_output() | ~1 us | 0 | Single register write |
 *
 * **Memory Footprint**:
 * - Static variables: ~32 bytes (initialization flags)
 * - Code size: ~1.5 KB (all functions)
 * - Stack usage: < 48 bytes (deepest call)
 * - Heap usage: 0 bytes (zero dynamic allocation)
 *
 * @par Hardware Requirements:
 *
 * | Component | Requirement | Notes |
 * |-----------|-------------|-------|
 * | MCU | Renesas RX72N | MTU3 peripheral required |
 * | Clock | PCLKA @ 120 MHz | System clock configuration |
 * | Power | 3.3V I/O | LVCMOS compatible |
 *
 * @par Module Dependencies:
 *
 * **This module depends on**:
 * - `rx_err.h` - Error code definitions
 * - `rx72n_mtu_regs.h` - MTU register definitions
 *
 * **This module is used by**:
 * - `rx_mtu_encoder.c` - Encoder interface (MTU1/MTU2)
 * - Application code for auxiliary PWM outputs
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | No goto/setjmp/recursion |
 * | 2. Fixed loop bounds | [OK] | Loops bounded by channel count (8 max) |
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
 * - This module handles ONLY MTU PWM configuration
 * - Encoder functions are in rx_mtu_encoder.h
 *
 * **Open/Closed (O)**:
 * - Extensible via rx_mtu_config_t structure
 * - New modes can be added without modifying existing code
 *
 * **Liskov Substitution (L)**:
 * - All functions return rx_err_t with consistent semantics
 * - Channel and output parameters are validated uniformly
 *
 * **Interface Segregation (I)**:
 * - PWM functions separate from encoder functions
 * - Float and raw duty cycle APIs separate
 *
 * **Dependency Inversion (D)**:
 * - Depends on abstract error codes
 * - Can be mocked for unit testing
 *
 * @see rx_gptw.h GPTW driver (preferred for motor PWM)
 * @see rx_mtu_encoder.h Encoder interface using MTU phase counting
 * @see rx72n_mtu_regs.h Register definitions
 * @see RX72N Hardware Manual Section 24 - Multi-Function Timer Unit 3
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @enum rx_mtu_channel_t
 * @brief MTU Channel Identifiers
 *
 * @details
 * Identifies the MTU timer channels available on the RX72N. Note that MTU5
 * does not exist in this MCU family. Channels are grouped for specific functions:
 *
 * ## Channel Groups and Functions
 *
 * **General Purpose PWM** (MTU0, MTU3, MTU4, MTU6, MTU7):
 * - Up to 4 outputs per channel (A, B, C, D)
 * - Complementary PWM with deadtime
 * - MTU3+MTU4 and MTU6+MTU7 can be paired for synchronized operation
 *
 * **Encoder Input** (MTU1, MTU2):
 * - Phase counting mode for quadrature encoders
 * - 16-bit pulse count with direction detection
 * - Used by rx_mtu_encoder.h for motor position feedback
 *
 * @par Channel Specifications:
 *
 * | Channel | Base Address | Outputs | Phase Count | Paired With |
 * |---------|--------------|---------|-------------|-------------|
 * | MTU0 | 0x000C1300 | A,B,C,D | No | - |
 * | MTU1 | 0x000C1380 | A,B | Yes | MTU2 |
 * | MTU2 | 0x000C1400 | A,B | Yes | MTU1 |
 * | MTU3 | 0x000C1200 | A,B,C,D | No | MTU4 |
 * | MTU4 | 0x000C1201 | A,B,C,D | No | MTU3 |
 * | MTU6 | 0x000C1A00 | A,B,C,D | No | MTU7 |
 * | MTU7 | 0x000C1A01 | A,B,C,D | No | MTU6 |
 *
 * @par STAR Project Usage:
 *
 * | Channel | Application | Configuration |
 * |---------|-------------|---------------|
 * | MTU1 | Encoder 0-1 (Motor 0, Motor 1) | Phase counting mode |
 * | MTU2 | Encoder 2-3 (Motor 2, Motor 3) | Phase counting mode |
 * | MTU0,3,4,6,7 | Available | General purpose PWM |
 *
 * @par Usage Example:
 * @code{.c}
 * // Initialize MTU0 for PWM output
 * rx_mtu_config_t config = {
 *     .frequency_hz = 10000,
 *     .deadtime_ns = 500,
 *     .enable_complementary = true,
 *     .invert_polarity = false
 * };
 *
 * rx_err_t err = rx_mtu_init_pwm(k_mtu_channel_0, &config);
 * if (err != k_rx_ok) {
 *     rx_log_error("MTU", "Init failed: %d", err);
 * }
 * @endcode
 *
 * @invariant Channel 5 is not defined (MTU5 hardware exists but is not supported by this HAL)
 * @invariant Channel values match hardware register offsets
 *
 * @note C23 typed enum with uint8_t underlying type
 * @note Value 5 is intentionally skipped (MTU5 U/V/W sub-channels not supported by this HAL)
 *
 * @see rx_mtu_encoder.h Encoder functions using MTU1/MTU2
 * @see RX72N Hardware Manual Section 24 - MTU3 channels
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mtu_channel_0 =
    0, /**< MTU0: General purpose timer. 4 outputs (A,B,C,D). Base: 0x000C1300. Standalone operation */
  k_mtu_channel_1 =
    1, /**< MTU1: Encoder input channel. 2 outputs (A,B). Base: 0x000C1380. Phase counting with MTU2 */
  k_mtu_channel_2 =
    2, /**< MTU2: Encoder input channel. 2 outputs (A,B). Base: 0x000C1400. Phase counting with MTU1 */
  k_mtu_channel_3 =
    3, /**< MTU3: General purpose timer. 4 outputs (A,B,C,D). Base: 0x000C1200. Pairs with MTU4 */
  k_mtu_channel_4 =
    4, /**< MTU4: General purpose timer. 4 outputs (A,B,C,D). Base: 0x000C1201. Pairs with MTU3 */
  k_mtu_channel_6 =
    6, /**< MTU6: General purpose timer. 4 outputs (A,B,C,D). Base: 0x000C1A00. Pairs with MTU7 */
  k_mtu_channel_7 =
    7, /**< MTU7: General purpose timer. 4 outputs (A,B,C,D). Base: 0x000C1A01. Pairs with MTU6 */
} rx_mtu_channel_t;

/**
 * @enum rx_mtu_output_t
 * @brief MTU Output Channel Selection
 *
 * @details
 * Selects which output pin to configure for PWM or compare match output.
 * MTU channels can have 2 or 4 outputs depending on the channel.
 *
 * ## Output Availability by Channel
 *
 * | Channel | Output A | Output B | Output C | Output D |
 * |---------|----------|----------|----------|----------|
 * | MTU0 | [OK] | [OK] | [OK] | [OK] |
 * | MTU1 | [OK] | [OK] | - | - |
 * | MTU2 | [OK] | [OK] | - | - |
 * | MTU3 | [OK] | [OK] | [OK] | [OK] |
 * | MTU4 | [OK] | [OK] | [OK] | [OK] |
 * | MTU6 | [OK] | [OK] | [OK] | [OK] |
 * | MTU7 | [OK] | [OK] | [OK] | [OK] |
 *
 * ## Complementary Pairs
 *
 * When complementary output is enabled:
 * - A and B form a complementary pair
 * - C and D form a complementary pair (on channels with 4 outputs)
 * - Deadtime is inserted between transitions
 *
 * @par Usage Example:
 * @code{.c}
 * // Set 50% duty on output A of MTU0
 * rx_mtu_set_duty(k_mtu_channel_0, k_mtu_output_a, 50.0f);
 *
 * // In complementary mode, output B is automatically inverted
 * // with deadtime inserted between transitions
 * @endcode
 *
 * @invariant Output C and D only valid for channels with 4 outputs
 * @invariant Output values are 0-3 (4 possible outputs)
 *
 * @note C23 typed enum with uint8_t underlying type
 * @warning Using output C/D on MTU1/MTU2 will return k_rx_err_invalid_arg
 *
 * @see rx_mtu_channel_t Channel selection
 * @see rx_mtu_set_duty() Set duty cycle on specific output
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mtu_output_a =
    0, /**< MTIOCA output pin. Available on all channels. Compare register: TGRA. Primary PWM output */
  k_mtu_output_b =
    1, /**< MTIOCB output pin. Available on all channels. Compare register: TGRB. Complementary to A when enabled */
  k_mtu_output_c =
    2, /**< MTIOCC output pin. MTU0,3,4,6,7 only. Compare register: TGRC. Secondary PWM output */
  k_mtu_output_d =
    3, /**< MTIOCD output pin. MTU0,3,4,6,7 only. Compare register: TGRD. Complementary to C when enabled */
} rx_mtu_output_t;

/**
 * @struct rx_mtu_config_t
 * @brief MTU PWM Configuration Structure
 *
 * @details
 * Contains all configuration parameters for initializing an MTU channel
 * for PWM output. This structure is passed to rx_mtu_init_pwm() to
 * configure the timer.
 *
 * ## Configuration Guidelines
 *
 * **Frequency Limits** (16-bit counter at 120 MHz):
 * - Maximum frequency: ~1.83 MHz (period = 65 counts)
 * - Minimum frequency: ~1831 Hz (period = 65535 counts)
 * - Recommended for motors: 15-25 kHz
 *
 * **Resolution Trade-off**:
 * Higher frequency -> lower resolution (fewer counts per period)
 * - 20 kHz: 6000 counts -> 0.017% resolution
 * - 100 kHz: 1200 counts -> 0.083% resolution
 *
 * @par Configuration Parameter Table:
 *
 * | Parameter | Min | Max | Default | Unit | Notes |
 * |-----------|-----|-----|---------|------|-------|
 * | frequency_hz | 1831 | 1830000 | 20000 | Hz | 16-bit limit |
 * | deadtime_ns | 0 | 65535 | 1000 | ns | 0 disables |
 * | enable_complementary | false | true | true | bool | A/B pairs |
 * | invert_polarity | false | true | false | bool | Active-low |
 *
 * @par Memory Layout:
 *
 * | Offset | Size | Field | Type | Alignment |
 * |--------|------|-------|------|-----------|
 * | 0 | 4 | frequency_hz | uint32_t | 4 |
 * | 4 | 2 | deadtime_ns | uint16_t | 2 |
 * | 6 | 1 | enable_complementary | bool | 1 |
 * | 7 | 1 | invert_polarity | bool | 1 |
 * | **Total** | **8** | - | - | **4** |
 *
 * @par Usage Example:
 * @code{.c}
 * // Standard PWM configuration for auxiliary output
 * rx_mtu_config_t config = {
 *     .frequency_hz = 10000,           // 10 kHz
 *     .deadtime_ns = 500,              // 500 ns deadtime
 *     .enable_complementary = true,    // A/B complementary
 *     .invert_polarity = false         // Active-high
 * };
 *
 * rx_err_t err = rx_mtu_init_pwm(k_mtu_channel_0, &config);
 * if (err != k_rx_ok) {
 *     rx_log_error("MTU", "Init failed: %d", err);
 * }
 * @endcode
 *
 * @invariant frequency_hz must produce period <= 65535 counts
 * @invariant Structure is copied during init (no need to keep alive)
 *
 * @note For motor control, prefer rx_gptw.h with 32-bit resolution
 * @warning Invalid configurations rejected with k_rx_err_invalid_arg
 *
 * @see rx_mtu_init_pwm() Channel initialization function
 * @see rx_gptw_config_t GPTW config (32-bit, recommended for motors)
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief PWM frequency in Hz
   * @details
   * Sets the PWM switching frequency. Limited by 16-bit counter:
   * - Min: 1831 Hz (period = 65535 at 120 MHz)
   * - Max: ~1.83 MHz (period = 65 at 120 MHz)
   *
   * @par Valid Range: [1831, 1830000] Hz
   * @par Recommended: 10000-20000 Hz for PWM applications
   */
  uint32_t frequency_hz;

  /**
   * @brief Deadtime in nanoseconds
   * @details
   * Deadtime inserted between complementary output transitions.
   * Prevents shoot-through in H-bridge configurations.
   * Set to 0 to disable deadtime insertion.
   *
   * @par Valid Range: [0, 65535] ns
   * @par Recommended: 500-1000 ns
   */
  uint16_t deadtime_ns;

  /**
   * @brief Enable complementary outputs
   * @details
   * When true, output pairs (A/B and C/D) are complementary:
   * - Output B = inverted Output A (with deadtime)
   * - Output D = inverted Output C (with deadtime)
   *
   * Required for H-bridge motor control applications.
   */
  bool enable_complementary;

  /**
   * @brief Invert PWM polarity
   * @details
   * When true, inverts the PWM output polarity:
   * - false: HIGH during duty cycle (active-high)
   * - true: LOW during duty cycle (active-low)
   */
  bool invert_polarity;
} rx_mtu_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize MTU channel for PWM output
 *
 * Configures MTU channel for PWM mode 1 (center-aligned, triangle wave).
 * Enables timer and starts PWM generation at 0% duty cycle.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[in] config PWM configuration
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel or config is invalid
 * @return k_rx_err_invalid_state if MTU module not enabled
 */
[[nodiscard]] rx_err_t rx_mtu_init_pwm(rx_mtu_channel_t channel, const rx_mtu_config_t* config);

/**
 * @brief Set PWM duty cycle
 *
 * Updates duty cycle for specified output channel.
 * Change takes effect on next PWM period for glitch-free operation.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[in] output Output channel (A/B/C/D)
 * @param[in] duty_percent Duty cycle in percent (0.0 - 100.0)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel, output, or duty is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t
rx_mtu_set_duty(rx_mtu_channel_t channel, rx_mtu_output_t output, float duty_percent);

/**
 * @brief Set PWM duty cycle (raw count value)
 *
 * Updates duty cycle using raw counter value for efficiency.
 * Useful in tight control loops where floating-point calculation overhead
 * should be avoided.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[in] output Output channel (A/B/C/D)
 * @param[in] duty_count Duty cycle count (0 - period_count)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel, output, or count is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t
rx_mtu_set_duty_raw(rx_mtu_channel_t channel, rx_mtu_output_t output, uint16_t duty_count);

/**
 * @brief Get current duty cycle
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[in] output Output channel (A/B/C/D)
 * @param[out] duty_percent Pointer to store duty cycle in percent
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if duty_percent is nullptr
 * @return k_rx_err_invalid_arg if channel or output is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t
rx_mtu_get_duty(rx_mtu_channel_t channel, rx_mtu_output_t output, float* duty_percent);

/**
 * @brief Get PWM period count
 *
 * Returns the period register value (useful for raw duty cycle calculations).
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[out] period_count Pointer to store period count
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if period_count is nullptr
 * @return k_rx_err_invalid_arg if channel is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t rx_mtu_get_period(rx_mtu_channel_t channel, uint16_t* period_count);

/**
 * @brief Enable or disable PWM output
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 * @param[in] output Output channel (A/B/C/D)
 * @param[in] enable True to enable output, false to disable
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel or output is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
[[nodiscard]] rx_err_t
rx_mtu_enable_output(rx_mtu_channel_t channel, rx_mtu_output_t output, bool enable);

/**
 * @brief Start MTU timer
 *
 * Starts the timer counter for PWM generation.
 * Timer is automatically started during init, but can be stopped/restarted.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
[[nodiscard]] rx_err_t rx_mtu_start(rx_mtu_channel_t channel);

/**
 * @brief Stop MTU timer
 *
 * Stops the timer counter. PWM outputs will hold their last state.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
[[nodiscard]] rx_err_t rx_mtu_stop(rx_mtu_channel_t channel);

/**
 * @brief Deinitialize MTU channel
 *
 * Stops timer, disables outputs, and releases resources.
 *
 * @param[in] channel MTU channel (0-4, 6-7)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
[[nodiscard]] rx_err_t rx_mtu_deinit(rx_mtu_channel_t channel);

#ifdef __cplusplus
}
#endif
