/**
 * @file rx_gptw.c
 * @brief GPTW PWM Driver Implementation for Motor Control
 *
 * @details
 * **Production-grade implementation** of the General PWM Timer (GPTW) driver for
 * RX72N motor control applications. This file contains all internal helper
 * functions and public API implementations for GPTW peripheral management.
 *
 * ## Module Architecture
 *
 * The implementation is structured in three layers:
 * 1. **Constants & Configuration**: Typed enums for all magic numbers
 * 2. **Internal Helpers**: Register access, calculations, hardware configuration
 * 3. **Public API**: User-facing functions documented in rx_gptw.h
 *
 * ## Supported PWM Modes
 *
 * **1. Sawtooth-wave PWM (edge-aligned)**:
 * - Counter: 0 -> GTPR -> 0 (reset at top)
 * - Period = PCLKA / frequency
 * - For 20kHz: period = 120MHz / 20kHz = 6000 counts (~12.5-bit resolution)
 * - Single update point at counter reset
 * - Best for: Simple PWM, LED dimming
 *
 * **2. Triangle-wave PWM (center-aligned)**:
 * - Counter: 0 -> GTPR -> 0 (up/down counting)
 * - Period = PCLKA / (2 x frequency)
 * - For 20kHz: period = 120MHz / 40kHz = 3000 counts (~11.5-bit resolution)
 * - Update at crest and/or trough (mode dependent)
 * - Lower EMI due to symmetric switching edges
 * - Reduced current ripple in motor windings
 * - Best for: Motor control with H-bridge
 *
 * ## Implementation Notes
 *
 * **Register Access Pattern**:
 * All GPTW register modifications follow this sequence:
 * 1. Unlock write protection (GTWP = 0xA500_0000)
 * 2. Modify registers
 * 3. Lock write protection (GTWP = 0x0000_0001)
 *
 * **Phase Staggering Implementation**:
 * For 4-motor systems, channels are initialized with counter offsets:
 * - Ch0: GTCNT = 0 (0deg reference)
 * - Ch1: GTCNT = period/4 (90deg offset)
 * - Ch2: GTCNT = period/2 (180deg offset)
 * - Ch3: GTCNT = 3xperiod/4 (270deg offset)
 *
 * ## Memory Map
 *
 * @par GPTW Register Base Addresses:
 *
 * | Module | Base Address | Size | Description |
 * |--------|--------------|------|-------------|
 * | GPTW Common | 0x000C2000 | 0x100 | Shared start/stop registers |
 * | GPTW0 | 0x000C2100 | 0x80 | Channel 0 registers |
 * | GPTW1 | 0x000C2180 | 0x80 | Channel 1 registers |
 * | GPTW2 | 0x000C2200 | 0x80 | Channel 2 registers |
 * | GPTW3 | 0x000C2280 | 0x80 | Channel 3 registers |
 *
 * @par Module Clock:
 * - MSTPCRA.MSTPA7 controls GPTW module clock on RX72N
 * - Must be cleared (= 0) before register access
 * - Protected by PRCR register lock (PRC1, key 0xA502)
 *
 * @par Clock Assumption:
 * This driver assumes PCKA = 96 MHz, which matches the STAR firmware
 * clock_init: HOCO 16 MHz x12 PLL = 192 MHz, SCKCR=0x21C21211 giving
 * PCKA = PLL/2 = 96 MHz. At 20 kHz PWM this yields GTPR = 96M/20k - 1 = 4799.
 * If the board ever moves to a different clock plan the HAL must be
 * updated (k_pclka_hz in rx72n_clock.h).
 *
 * ## Static Variables
 *
 * | Variable | Type | Purpose |
 * |----------|------|---------|
 * | s_gptw_initialized[] | bool[4] | Track which channels are initialized |
 * | s_gptw_period[] | uint32_t[4] | Cache period values for duty calculation |
 * | s_tag | const char* | Log tag "GPTW" |
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | [OK] | No goto/setjmp/recursion in any function |
 * | 2. Fixed loop bounds | [OK] | Loops bounded by k_gptw_max_channels (4) |
 * | 3. No dynamic allocation | [OK] | All variables static or stack, zero malloc |
 * | 4. Small functions | [OK] | All functions < 60 lines |
 * | 5. Assertions | [OK] | RX_CHECK_NULL_PTR, switch validation, range checks |
 * | 6. Narrow scope | [OK] | Internal functions use static, minimal globals |
 * | 7. Check return values | [OK] | All rx_err_t returns propagated or handled |
 * | 8. Limited preprocessor | [OK] | C23 typed enums, macros only for RX_CHECK |
 * | 9. Pointer restrictions | [OK] | Single-level pointers, no function pointers |
 * | 10. Compiler warnings | [OK] | Builds clean with -Wall -Wextra -Werror |
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility**: This file handles ONLY GPTW hardware abstraction.
 * Motor control logic, PID calculations, and H-bridge configuration are in
 * separate modules.
 *
 * **Open/Closed**: New waveform modes can be added by extending the
 * rx_gptw_wave_mode_t enum and internal_get_gtcr_mode() without modifying
 * existing code paths.
 *
 * @see rx_gptw.h Public API documentation
 * @see rx72n_gptw_regs.h Register structure definitions
 * @see RX72N Hardware Manual Chapter 26 - General PWM Timer (GPTW)
 * @see docs/sections/03_hardware_pinout.tex Pin assignments
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#ifdef __RX__

#include "rx_gptw.h"

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_log.h"
#include "rx_register_protection.h"

static const char* s_tag = "GPTW";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief GPTW general constants */
typedef enum : uint8_t {
  k_gptw_max_channels        = 4, /**< GPTW0-GPTW3 */
  k_gptw_outputs_per_channel = 2, /**< GTIOCA and GTIOCB */
} gptw_constants_t;

/** @brief Period calculation constants */
static const uint32_t s_gptw_period_max    = 0xFFFFFFFFUL;  /**< Maximum valid period (32-bit) */
static const uint64_t s_gptw_ns_per_second = 1000000000ULL; /**< Nanoseconds per second */

typedef enum : uint16_t {
  k_gptw_period_divisor_saw = 1,  /**< Sawtooth: period = PCLKA / freq */
  k_gptw_period_divisor_tri = 2,  /**< Triangle: period = PCLKA / (2 * freq) */
  k_gptw_period_min         = 10, /**< Minimum valid period */
  k_gptw_period_zero        = 0,  /**< Zero period value */
  k_gptw_deadtime_disabled  = 0,  /**< No dead time */
} gptw_period_constants_t;

/** @brief Duty cycle calculation constants */
typedef enum : uint8_t {
  k_gptw_duty_min     = 0,   /**< Minimum duty cycle (0%) */
  k_gptw_duty_max     = 100, /**< Maximum duty cycle (100%) */
  k_gptw_duty_divisor = 100, /**< Divisor for percentage conversion */
} gptw_duty_constants_t;

/** @brief MPC configuration constants */
typedef enum : uint8_t {
  k_mpc_pwpr_b0wi_clear = 0x00, /**< Clear B0WI to enable PFSWE write */
  k_mpc_pwpr_pfswe_set  = 0x40, /**< Set PFSWE to enable PFS write */
  k_mpc_pwpr_lock       = 0x80, /**< Set B0WI to lock PFS */
} mpc_pwpr_constants_t;

/** @brief MPC PFS register offset constants */
typedef enum : uint16_t {
  k_mpc_pfs_base_offset = 0x40, /**< Base offset for PFS registers */
  k_mpc_port_e_index    = 0x0E, /**< Port E index */
  k_mpc_pfs_port_stride = 8,    /**< Bytes between port PFS groups */
} mpc_pfs_offset_constants_t;

/** @brief Single-bit value for shift operations */
typedef enum : uint8_t {
  k_gptw_bit_set = 1, /**< Value for single bit in shift operations */
} gptw_bit_constants_t;

/** @brief GPTW channel mask for start/stop operations */
typedef enum : uint8_t {
  k_gptw_all_channels_mask = 0x0F, /**< Bitmask for all 4 GPTW channels (Ch0-3) */
} gptw_channel_mask_t;

/** @brief GPTW count direction values for GTUDDTYC register */
typedef enum : uint32_t {
  k_gptw_gtuddtyc_ud_down = 0, /**< Count direction down (UD bit = 0) */
} gptw_direction_constants_t;

/** @brief Port E pin indices for GPTW motor outputs */
typedef enum : uint8_t {
  k_porte_gptw0_a = 5, /**< PE5/GTIOC0A - Motor 0 phase */
  k_porte_gptw0_b = 2, /**< PE2/GTIOC0B - Motor 0 enable */
  k_porte_gptw1_a = 4, /**< PE4/GTIOC1A - Motor 1 phase */
  k_porte_gptw1_b = 1, /**< PE1/GTIOC1B - Motor 1 enable */
  k_porte_gptw2_a = 3, /**< PE3/GTIOC2A - Motor 2 phase */
  k_porte_gptw2_b = 0, /**< PE0/GTIOC2B - Motor 2 enable */
  k_porte_gptw3_a = 7, /**< PE7/GTIOC3A - Motor 3 phase */
  k_porte_gptw3_b = 6, /**< PE6/GTIOC3B - Motor 3 enable */
} porte_gptw_pin_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief Track initialized channels */
static bool s_gptw_initialized[k_gptw_max_channels] = {false};
/** @brief Period values for each channel */
static uint32_t s_gptw_period[k_gptw_max_channels] = {};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get GPTW channel register base address
 *
 * @details
 * Maps a GPTW channel enumeration value to its corresponding hardware register
 * base address. Each GPTW channel (0-3) has a dedicated register block at a
 * fixed memory offset.
 *
 * ## Memory Map
 *
 * | Channel | Base Address | Offset from GPTW0 |
 * |---------|--------------|-------------------|
 * | k_gptw_channel_0 | 0x000C2100 | 0x000 |
 * | k_gptw_channel_1 | 0x000C2180 | 0x080 |
 * | k_gptw_channel_2 | 0x000C2200 | 0x100 |
 * | k_gptw_channel_3 | 0x000C2280 | 0x180 |
 *
 * @param[in] channel GPTW channel identifier
 *   - Valid range: k_gptw_channel_0 to k_gptw_channel_3
 *   - Values outside this range returnnullptr
 *
 * @return Pointer to GPTW channel register structure
 * @retval Non-NULL Pointer to rx_gptw_channel_regs_t for valid channels
 * @retval NULL Invalid channel value (outside 0-3 range)
 *
 * @pre Module clock must be enabled before accessing returned pointer
 *
 * @post Returned pointer is valid for hardware register access
 * @post No hardware state modified by this function
 *
 * @note Thread-safe: Returns constant address, no state modification
 * @note Caller must check for nullptr before using returned pointer
 *
 * @par Performance:
 * - Execution time: ~10 cycles (switch statement)
 * - Memory: 0 bytes heap, 0 bytes stack allocation
 *
 * @see gptw0(), gptw1(), gptw2(), gptw3() Underlying accessor functions
 * @see rx72n_gptw_regs.h Register structure definition
 *
 * @since Version 1.0.0
 */
static volatile rx_gptw_channel_regs_t* internal_get_gptw_base(const rx_gptw_channel_t channel)
{
  switch (channel) {
    case k_gptw_channel_0:
      return gptw0();
    case k_gptw_channel_1:
      return gptw1();
    case k_gptw_channel_2:
      return gptw2();
    case k_gptw_channel_3:
      return gptw3();
    default:
      return nullptr;
  }
}

/**
 * @brief Check if wave mode is triangle (center-aligned)
 *
 * @details
 * Determines whether the specified waveform mode uses triangle wave (center-aligned)
 * PWM generation. Triangle modes count up to period, then down to zero, resulting
 * in symmetric PWM pulses centered within each period.
 *
 * ## Mode Classification
 *
 * | Mode | Type | Returns |
 * |------|------|---------|
 * | k_gptw_wave_saw_pwm | Sawtooth (edge-aligned) | false |
 * | k_gptw_wave_tri_pwm1 | Triangle (center-aligned) | true |
 * | k_gptw_wave_tri_pwm2 | Triangle (center-aligned) | true |
 * | k_gptw_wave_tri_pwm3 | Triangle (center-aligned) | true |
 *
 * @param[in] mode Waveform mode enumeration value
 *   - Valid values: Any rx_gptw_wave_mode_t enum value
 *   - Invalid values return false (sawtooth assumed)
 *
 * @return bool Mode classification result
 * @retval true Mode is triangle wave (k_gptw_wave_tri_pwm1/2/3)
 * @retval false Mode is sawtooth wave or invalid value
 *
 * @pre mode should be a valid rx_gptw_wave_mode_t value
 *
 * @post No state modification
 *
 * @note Pure function: Same input always produces same output
 * @note Thread-safe: No shared state access
 * @note Inlined for performance in period calculation loop
 *
 * @par Performance:
 * - Execution time: ~3 cycles (two comparisons)
 * - Inlined at call site, no function call overhead
 *
 * @see rx_gptw_wave_mode_t Waveform mode definitions
 * @see internal_calculate_period() Primary user of this function
 *
 * @since Version 1.0.0
 */
static inline bool internal_is_triangle_mode(const rx_gptw_wave_mode_t mode)
{
  return (mode == k_gptw_wave_tri_pwm1) || (mode == k_gptw_wave_tri_pwm2) ||
         (mode == k_gptw_wave_tri_pwm3);
}

/**
 * @brief Get GTCR mode bits for wave mode
 *
 * @details
 * Converts a waveform mode enumeration to the corresponding GTCR.MD register
 * bit field value. The MD field (bits 18:16) in GTCR controls the timer's
 * counting mode and PWM generation behavior.
 *
 * ## GTCR.MD Field Mapping
 *
 * | Input Mode | GTCR.MD Value | Binary | Description |
 * |------------|---------------|--------|-------------|
 * | k_gptw_wave_saw_pwm | k_gptw_gtcr_md_saw_pwm | 0b000 | Sawtooth PWM |
 * | k_gptw_wave_tri_pwm1 | k_gptw_gtcr_md_tri_pwm1 | 0b001 | Triangle PWM 1 |
 * | k_gptw_wave_tri_pwm2 | k_gptw_gtcr_md_tri_pwm2 | 0b010 | Triangle PWM 2 |
 * | k_gptw_wave_tri_pwm3 | k_gptw_gtcr_md_tri_pwm3 | 0b011 | Triangle PWM 3 |
 *
 * @param[in] mode Waveform mode to convert
 *   - Valid range: Any rx_gptw_wave_mode_t enum value
 *   - Invalid values trigger RX_ASSERT and return sawtooth mode
 *
 * @return uint32_t GTCR register value with MD bits set appropriately
 * @retval k_gptw_gtcr_md_saw_pwm For sawtooth mode or invalid input (fallback)
 * @retval k_gptw_gtcr_md_tri_pwm1 For triangle PWM mode 1
 * @retval k_gptw_gtcr_md_tri_pwm2 For triangle PWM mode 2
 * @retval k_gptw_gtcr_md_tri_pwm3 For triangle PWM mode 3
 *
 * @pre mode should be validated before calling (caller's responsibility)
 * @pre GTCR write protection must be unlocked before writing returned value
 *
 * @post No state modification (pure function)
 * @post Returned value is ready for GTCR register write
 *
 * @invariant Invalid modes always return sawtooth as safe fallback
 *
 * @note Assert triggers on invalid mode in debug builds
 * @note Returns safe default (sawtooth) on invalid mode in release builds
 * @note Thread-safe: Pure function with no shared state
 *
 * @warning RX_ASSERT fires if mode is not a valid enum value
 *
 * @par Performance:
 * - Execution time: ~10 cycles (switch statement)
 * - Memory: 0 bytes heap, 0 bytes stack
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] RX_ASSERT validates unexpected conditions
 *
 * @see rx_gptw_wave_mode_t Input enum definition
 * @see RX72N Hardware Manual Section 26.2.12 GTCR register description
 *
 * @since Version 1.0.0
 */
static uint32_t internal_get_gtcr_mode(const rx_gptw_wave_mode_t mode)
{
  switch (mode) {
    case k_gptw_wave_tri_pwm1:
      return k_gptw_gtcr_md_tri_pwm1;
    case k_gptw_wave_tri_pwm2:
      return k_gptw_gtcr_md_tri_pwm2;
    case k_gptw_wave_tri_pwm3:
      return k_gptw_gtcr_md_tri_pwm3;
    case k_gptw_wave_saw_pwm:
      return k_gptw_gtcr_md_saw_pwm;
    default:
      /* Should never reach here if wave_mode was validated upstream (NASA Rule 5) */
      RX_ASSERT(false, "Unexpected wave mode in internal_get_gtcr_mode");
      return k_gptw_gtcr_md_saw_pwm;
  }
}

/**
 * @brief Calculate period register value from frequency
 *
 * @details
 * Converts a desired PWM frequency to the GTPR (period register) count value.
 * The calculation differs based on waveform mode:
 *
 * ## Mathematical Formulas
 *
 * **Sawtooth (edge-aligned) mode:**
 * @f[
 *   \text{period} = \frac{f_{PCLKA}}{f_{PWM}}
 * @f]
 *
 * **Triangle (center-aligned) mode:**
 * @f[
 *   \text{period} = \frac{f_{PCLKA}}{2 \times f_{PWM}}
 * @f]
 *
 * Where:
 * - @f$ f_{PCLKA} @f$ = 120 MHz (peripheral clock A)
 * - @f$ f_{PWM} @f$ = desired PWM frequency
 *
 * ## Algorithm Steps
 *
 * 1. Validate period output pointer (RX_CHECK_NULL_PTR)
 * 2. Validate wave_mode via exhaustive switch statement
 * 3. Determine divisor based on mode (1 for sawtooth, 2 for triangle)
 * 4. Check frequency_hz is non-zero (prevent division by zero)
 * 5. Calculate period using 64-bit arithmetic to prevent overflow
 * 6. Validate period is within hardware limits [10, 0xFFFFFFFF]
 * 7. Store result and return success
 *
 * ## Example Calculations
 *
 * | Frequency | Mode | Period Count | Resolution |
 * |-----------|------|--------------|------------|
 * | 20 kHz | Sawtooth | 6000 | 0.0167% |
 * | 20 kHz | Triangle | 3000 | 0.0333% |
 * | 100 Hz | Sawtooth | 1,200,000 | 0.000083% |
 * | 1 MHz | Sawtooth | 120 | 0.833% |
 *
 * @param[in] frequency_hz Desired PWM frequency in Hz
 *   - Valid range: [1, 12000000] Hz (practical upper limit)
 *   - Frequencies resulting in period < 10 are rejected
 *   - Zero frequency is rejected with k_rx_err_invalid_arg
 * @param[in] wave_mode Waveform mode selection
 *   - Valid values: k_gptw_wave_saw_pwm, k_gptw_wave_tri_pwm1/2/3
 *   - Invalid values rejected with k_rx_err_invalid_arg
 * @param[out] period Pointer to store calculated period count
 *   - Must be non-NULL
 *   - Receives value in range [10, 0xFFFFFFFF]
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, period contains valid count
 * @retval k_rx_err_null_ptr period pointer is nullptr
 * @retval k_rx_err_invalid_arg wave_mode invalid, frequency=0, or period out of range
 *
 * @pre PCLKA configured to 120 MHz (system clock setup complete)
 * @pre period pointer must point to valid uint32_t storage
 *
 * @post On success, *period contains valid count [10, 0xFFFFFFFF]
 * @post On failure, *period is unchanged
 *
 * @invariant period >= k_gptw_period_min (10) on success
 * @invariant Calculation uses 64-bit math to prevent overflow
 *
 * @note Thread-safe: No shared state modified
 * @note Uses RX_CHECK_NULL_PTR macro for pointer validation
 *
 * @warning Very low frequencies (< 1 Hz) may overflow 32-bit period
 * @warning Very high frequencies (> 12 MHz) produce periods < 10 (rejected)
 *
 * @par Performance:
 * - Execution time: ~50 cycles (64-bit division)
 * - Memory: 0 bytes heap, ~16 bytes stack
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 preconditions (NULL check, wave_mode validation)
 * - Rule 7: [OK] Returns error code, no silent failures
 *
 * @see k_pclka_hz PCLKA clock frequency constant
 * @see internal_is_triangle_mode() Determines divisor selection
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_calculate_period(const uint32_t            frequency_hz,
                                          const rx_gptw_wave_mode_t wave_mode,
                                          uint32_t*                 period)
{
  /* Pre-condition: validate pointer (NASA Power of 10 Rule 5) */
  RX_CHECK_NULL_PTR(period, s_tag, "period pointer is nullptr");

  /* Pre-condition: validate wave_mode using exhaustive switch (catches invalid/negative values) */
  switch (wave_mode) {
    case k_gptw_wave_saw_pwm:
    case k_gptw_wave_tri_pwm1:
    case k_gptw_wave_tri_pwm2:
    case k_gptw_wave_tri_pwm3:
      break;
    default:
      rx_log_error(s_tag, "Invalid wave mode");
      return k_rx_err_invalid_arg;
  }

  /* Period calculation depends on waveform mode:
   * - Sawtooth: Period = PCLKA / frequency
   * - Triangle: Period = PCLKA / (2 * frequency), because counter goes up then down
   * PCLKA = 96 MHz on STAR (HOCO x12 PLL / 2); see rx72n_clock.h k_pclka_hz.
   */
  const uint32_t pclka = k_pclka_hz;
  const uint32_t divisor =
    internal_is_triangle_mode(wave_mode) ? k_gptw_period_divisor_tri : k_gptw_period_divisor_saw;

  if (frequency_hz == k_gptw_period_zero) {
    rx_log_error(s_tag, "Frequency is zero");
    return k_rx_err_invalid_arg;
  }

  /* Use 64-bit math to prevent overflow in divisor * frequency_hz */
  const uint64_t denominator = (uint64_t)divisor * (uint64_t)frequency_hz;
  const uint32_t period_calc = (uint32_t)(pclka / denominator);

  /* Check if period fits in valid range */
  if (period_calc > s_gptw_period_max) {
    rx_log_error(s_tag, "Frequency too low");
    return k_rx_err_invalid_arg;
  }

  if (period_calc < k_gptw_period_min) {
    rx_log_error(s_tag, "Frequency too high");
    return k_rx_err_invalid_arg;
  }

  *period = period_calc;
  return k_rx_ok;
}

/**
 * @brief Configure MPC for GPTW output pins
 *
 * @details
 * Configures the Multi-Function Pin Controller (MPC) to route GPTW timer outputs
 * to the appropriate Port E pins. Each GPTW channel uses two pins for
 * complementary PWM outputs (GTIOCA and GTIOCB).
 *
 * ## MPC Configuration Sequence
 *
 * 1. Clear PWPR.B0WI to enable PFSWE bit access
 * 2. Set PWPR.PFSWE to enable PFS register writes
 * 3. Write PSEL value (0x14) to select GPTW function for each pin
 * 4. Set PWPR.B0WI to lock PFS registers
 *
 * ## Pin Assignments
 *
 * | Channel | Output A (GTIOCnA) | Output B (GTIOCnB) | PSEL |
 * |---------|--------------------|--------------------|------|
 * | GPTW0 | PE5 | PE2 | 0x14 |
 * | GPTW1 | PE4 | PE1 | 0x14 |
 * | GPTW2 | PE3 | PE0 | 0x14 |
 * | GPTW3 | PE7 | PE6 | 0x14 |
 *
 * @param[in] channel GPTW channel to configure MPC for
 *   - Valid range: k_gptw_channel_0 to k_gptw_channel_3
 *   - Invalid channels return k_rx_err_invalid_arg
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok MPC configuration successful
 * @retval k_rx_err_invalid_arg Invalid channel value
 *
 * @pre MPC module accessible (no separate clock enable required)
 * @pre Pins not in use by other peripherals
 *
 * @post Port E PFS registers configured for GPTW function
 * @post MPC write protection re-enabled
 *
 * @invariant MPC write protection always re-locked on exit (success or failure)
 *
 * @note Modifies global MPC registers, not thread-safe
 * @note Must be called before enabling port pin peripheral mode
 *
 * @warning Overwrites any previous pin function assignment
 * @warning MPC must be unlocked before calling; function handles locking
 *
 * @par Performance:
 * - Execution time: ~20 cycles (register writes)
 * - Memory: 0 bytes heap, ~8 bytes stack
 *
 * @see internal_configure_port_pins() Configure PMR/PDR after MPC
 * @see RX72N Hardware Manual Chapter 23 - Multi-Function Pin Controller
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_configure_mpc(const rx_gptw_channel_t channel)
{
  /* MPC Pin Function Select values for GPTW on Port E
   * Each channel uses 2 pins (GTIOCA and GTIOCB)
   *
   * Pin mapping (from hardware_pinout.h):
   * - GPTW0: PE5/GTIOC0A, PE2/GTIOC0B
   * - GPTW1: PE4/GTIOC1A, PE1/GTIOC1B
   * - GPTW2: PE3/GTIOC2A, PE0/GTIOC2B
   * - GPTW3: PE7/GTIOC3A, PE6/GTIOC3B
   */

  /* Unlock MPC write protection */
  mpc()->pwpr = k_mpc_pwpr_b0wi_clear; /* Clear B0WI */
  mpc()->pwpr = k_mpc_pwpr_pfswe_set;  /* Set PFSWE to enable PFS write */

  /* Configure PFS for the appropriate pins
   * PSEL value for GPTW is 0x14 (verify against HW manual)
   * Note: Port E PFS registers not in rx_mpc_regs_t structure,
   * so we use base + offset calculation */
  volatile uint8_t* pe_pfs = (volatile uint8_t*)((uintptr_t)mpc() + k_mpc_pfs_base_offset +
                                                 k_mpc_port_e_index * k_mpc_pfs_port_stride);

  switch (channel) {
    case k_gptw_channel_0:
      /* PE5/GTIOC0A and PE2/GTIOC0B */
      pe_pfs[k_porte_gptw0_a] = k_pfs_psel_gptw;
      pe_pfs[k_porte_gptw0_b] = k_pfs_psel_gptw;
      break;
    case k_gptw_channel_1:
      /* PE4/GTIOC1A and PE1/GTIOC1B */
      pe_pfs[k_porte_gptw1_a] = k_pfs_psel_gptw;
      pe_pfs[k_porte_gptw1_b] = k_pfs_psel_gptw;
      break;
    case k_gptw_channel_2:
      /* PE3/GTIOC2A and PE0/GTIOC2B */
      pe_pfs[k_porte_gptw2_a] = k_pfs_psel_gptw;
      pe_pfs[k_porte_gptw2_b] = k_pfs_psel_gptw;
      break;
    case k_gptw_channel_3:
      /* PE7/GTIOC3A and PE6/GTIOC3B */
      pe_pfs[k_porte_gptw3_a] = k_pfs_psel_gptw;
      pe_pfs[k_porte_gptw3_b] = k_pfs_psel_gptw;
      break;
    default:
      mpc()->pwpr = k_mpc_pwpr_lock; /* Lock before returning error */
      return k_rx_err_invalid_arg;
  }

  /* Lock MPC write protection */
  mpc()->pwpr = k_mpc_pwpr_lock;

  return k_rx_ok;
}

/**
 * @brief Configure Port E pins as peripheral outputs
 *
 * @details
 * Configures the Port E GPIO pins for GPTW peripheral output mode. Sets both the
 * Peripheral Mode Register (PMR) and Port Direction Register (PDR) to enable
 * GPTW timer outputs.
 *
 * ## Register Configuration
 *
 * For each pin used by the specified channel:
 * - **PMR bit = 1**: Enable peripheral mode (not GPIO)
 * - **PDR bit = 1**: Set pin direction to output
 *
 * ## Pin Mapping
 *
 * | Channel | Output A Pin | Output B Pin |
 * |---------|--------------|--------------|
 * | GPTW0 | PE5 (bit 5) | PE2 (bit 2) |
 * | GPTW1 | PE4 (bit 4) | PE1 (bit 1) |
 * | GPTW2 | PE3 (bit 3) | PE0 (bit 0) |
 * | GPTW3 | PE7 (bit 7) | PE6 (bit 6) |
 *
 * @param[in] channel GPTW channel whose pins to configure
 *   - Valid range: k_gptw_channel_0 to k_gptw_channel_3
 *   - Invalid channels result in no action (no error)
 *
 * @pre MPC configured for GPTW function via internal_configure_mpc()
 * @pre Port E GPIO registers accessible
 *
 * @post PMR bits set for both channel pins
 * @post PDR bits set for both channel pins
 *
 * @note No return value - silent no-op on invalid channel
 * @note Modifies Port E registers, not thread-safe
 * @note Uses OR-equals to preserve other pin configurations
 *
 * @par Performance:
 * - Execution time: ~10 cycles (register read-modify-write)
 * - Memory: 0 bytes heap, 0 bytes stack
 *
 * @see internal_configure_mpc() Must be called before this function
 * @see porte() Port E register accessor
 *
 * @since Version 1.0.0
 */
static void internal_configure_port_pins(const rx_gptw_channel_t channel)
{
  /* Set pins as output and enable peripheral function
   * PMR = 1 (peripheral mode), PDR = 1 (output)
   */
  switch (channel) {
    case k_gptw_channel_0:
      porte()->pmr |= (k_gptw_bit_set << k_porte_gptw0_a) | (k_gptw_bit_set << k_porte_gptw0_b);
      porte()->pdr |= (k_gptw_bit_set << k_porte_gptw0_a) | (k_gptw_bit_set << k_porte_gptw0_b);
      break;
    case k_gptw_channel_1:
      porte()->pmr |= (k_gptw_bit_set << k_porte_gptw1_a) | (k_gptw_bit_set << k_porte_gptw1_b);
      porte()->pdr |= (k_gptw_bit_set << k_porte_gptw1_a) | (k_gptw_bit_set << k_porte_gptw1_b);
      break;
    case k_gptw_channel_2:
      porte()->pmr |= (k_gptw_bit_set << k_porte_gptw2_a) | (k_gptw_bit_set << k_porte_gptw2_b);
      porte()->pdr |= (k_gptw_bit_set << k_porte_gptw2_a) | (k_gptw_bit_set << k_porte_gptw2_b);
      break;
    case k_gptw_channel_3:
      porte()->pmr |= (k_gptw_bit_set << k_porte_gptw3_a) | (k_gptw_bit_set << k_porte_gptw3_b);
      porte()->pdr |= (k_gptw_bit_set << k_porte_gptw3_a) | (k_gptw_bit_set << k_porte_gptw3_b);
      break;
    default:
      break;
  }
}

/**
 * @brief Enable GPTW module clock
 *
 * @details
 * Enables the GPTW peripheral by clearing its module stop bit in MSTPCRA.
 * On RX72N, GPTW is controlled by MSTPCRA bit 7 (MSTPA7), per the
 * RX72N Hardware Manual Chapter 11 and Renesas iodefine.h. When set, the
 * module clock is stopped and registers are inaccessible.
 *
 * ## Register Access Sequence
 *
 * 1. **Unlock PRCR**: Write 0xA502 (PRC1) to enable writing to MSTPCR group
 * 2. **Clear MSTPA7**: AND-mask to clear bit 7 in MSTPCRA (0x00080010)
 * 3. **Lock PRCR**: Write 0xA500 to re-enable protection
 *
 * ## Module Stop Control
 *
 * | Bit | Value | Effect |
 * |-----|-------|--------|
 * | MSTPCRA.MSTPA7 = 1 | Module stopped | Registers inaccessible, low power |
 * | MSTPCRA.MSTPA7 = 0 | Module running | Registers accessible, clock active |
 *
 * @pre System clock configured
 *
 * @post GPTW module clock enabled
 * @post GPTW registers accessible
 * @post PRCR protection re-enabled
 *
 * @invariant PRCR always re-locked after modification
 *
 * @note Not thread-safe: Modifies system-wide registers
 * @note Idempotent: Safe to call multiple times
 *
 * @warning Must be called before any GPTW register access
 * @warning Modifies system protection register (PRCR)
 *
 * @par Performance:
 * - Execution time: ~15 cycles (3 register writes)
 * - Memory: 0 bytes heap, 0 bytes stack
 *
 * @see k_mstpa_gptw GPTW module stop bit position (MSTPA7)
 * @see k_rx_prcr_unlock_prc1 PRCR unlock value for MSTPCR group
 * @see RX72N Hardware Manual Chapter 11 - Low Power Consumption
 *
 * @since Version 1.0.0
 */
static void internal_enable_gptw_module_clock(void)
{
  /* RX72N: GPTW = MSTPCRA bit 7 (MSTPA7), NOT MSTPCRC bit 6 as earlier HAL
   * code claimed. Use PRC1 unlock (0xA502) which covers MSTPCR group. */
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcra &= ~((uint32_t)k_gptw_bit_set << k_mstpa_gptw);
  *prcr_reg() = k_rx_prcr_lock;
}

/**
 * @brief Configure GPTW hardware registers
 *
 * @details
 * Performs the core hardware configuration of a GPTW channel. This function
 * writes all necessary registers to set up PWM operation including control mode,
 * I/O configuration, period, duty cycle initialization, buffer mode, and deadtime.
 *
 * ## Register Configuration Sequence
 *
 * 1. **Unlock write protection**: GTWP = 0xA500_0000
 * 2. **Configure control register (GTCR)**:
 *    - Set clock source to PCLKA/1 (120 MHz)
 *    - Set waveform mode (sawtooth or triangle)
 * 3. **Configure I/O control (GTIOR)**:
 *    - Output A: Initial low, toggle on compare match, enable
 *    - Output B: Initial low, toggle on compare match, enable
 * 4. **Set period register (GTPR)**: PWM cycle count
 * 5. **Initialize duty cycles (GTCCRA, GTCCRB)**: Both to 0
 * 6. **Clear counter (GTCNT)**: Start at 0
 * 7. **Enable buffer operation (GTBER)**: Glitch-free updates
 * 8. **Configure deadtime (GTDVU, GTDVD, GTDTCR)**: If requested
 * 9. **Lock write protection**: GTWP = 0x0000_0001
 *
 * ## Deadtime Calculation
 *
 * @f[
 *   \text{deadtime\_counts} = \frac{\text{deadtime\_ns} \times f_{PCLKA}}{10^9}
 * @f]
 *
 * For 1000 ns at 96 MHz: deadtime_counts = 96
 *
 * @param[in] gptw Pointer to GPTW channel register structure
 *   - Must be non-NULL
 *   - Must point to valid GPTW register base (from internal_get_gptw_base)
 * @param[in] config Configuration parameters structure
 *   - Must be non-NULL
 *   - wave_mode determines GTCR.MD field
 *   - deadtime_ns determines deadtime register values
 * @param[in] period Pre-calculated period register value
 *   - Valid range: [k_gptw_period_min, s_gptw_period_max]
 *   - Obtained from internal_calculate_period()
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Configuration successful
 * @retval k_rx_err_null_ptr gptw or config is nullptr
 * @retval k_rx_err_invalid_arg period outside valid range [10, 0xFFFFFFFF]
 *
 * @pre gptw points to valid, unlocked GPTW registers
 * @pre config contains validated parameters
 * @pre period calculated and validated by internal_calculate_period()
 *
 * @post GPTW channel configured for PWM operation
 * @post Duty cycles initialized to 0% (motor stopped)
 * @post Buffer mode enabled for glitch-free updates
 * @post Write protection re-enabled
 *
 * @invariant Write protection always re-locked on exit
 * @invariant Duty cycles always start at 0 (safe state)
 *
 * @note Not thread-safe: Direct hardware register access
 * @note Timer is NOT started by this function
 *
 * @warning gptw pointer must be valid - no runtime bounds checking
 * @warning Caller responsible for stopping timer before calling
 *
 * @par Performance:
 * - Execution time: ~100 cycles (multiple register writes)
 * - Memory: 0 bytes heap, ~16 bytes stack
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 3 preconditions (NULL checks, period range)
 *
 * @see internal_calculate_period() Period value calculation
 * @see rx72n_gptw_regs.h Register structure definition
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_configure_gptw_hardware(volatile rx_gptw_channel_regs_t* gptw,
                                                 const rx_gptw_config_t*          config,
                                                 const uint32_t                   period)
{
  /* Pre-condition: validate pointers (NASA Power of 10 Rule 5) */
  RX_CHECK_NULL_PTR(gptw, s_tag, "gptw pointer is nullptr");
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is nullptr");

  /* Pre-condition: validate period range */
  if ((period < k_gptw_period_min) || (period > s_gptw_period_max)) {
    rx_log_error(s_tag, "Period out of range");
    return k_rx_err_invalid_arg;
  }

  /* Unlock write protection for this channel */
  gptw->gtwp = k_gptw_gtwp_unlock;

  /* Configure control register
   * - PCLKA/1 (96 MHz on STAR)
   * - Waveform mode from config (sawtooth or triangle)
   * NOTE: CST (bit 0) left clear; caller (rx_gptw_start) sets it.
   */
  const uint32_t gtcr_mode = internal_get_gtcr_mode(config->wave_mode);
  gptw->gtcr               = k_gptw_gtcr_tpcs_1 | gtcr_mode;

  /* Force UP-count direction. Reset value of GTUDDTYC is 0 (UD=0 = DOWN).
   * With GTIOR = 0x09 (compare-match-up event drives output LOW), a
   * down-counter never raises the compare-match-up event and the output
   * latches HIGH at end-of-cycle. UD=1 + UDF=1 forces up-count immediately. */
  gptw->gtuddtyc = k_gptw_gtuddtyc_ud | k_gptw_gtuddtyc_udf;

  /* Configure I/O control register
   * - Initial low, high at end-of-cycle, low at compare match (sawtooth up)
   * - Enable both outputs
   */
  gptw->gtior =
    k_gptw_gtior_oa_init_low | k_gptw_gtior_oae | k_gptw_gtior_ob_init_low | k_gptw_gtior_obe;

  /* Set period (GTPR = PWM cycle) */
  gptw->gtpr = period;

  /* Set initial duty cycle to 0% for both outputs */
  gptw->gtccra = k_gptw_period_zero;
  gptw->gtccrb = k_gptw_period_zero;

  /* Clear counter */
  gptw->gtcnt = k_gptw_period_zero;

  /* Disable all buffer operation on GTCCRA/GTCCRB/GTPR.
   * Rationale: with single-buffer enabled (CCRA=0x01<<16, CCRB=0x01<<18)
   * the hardware copies GTCCRC->GTCCRA / GTCCRE->GTCCRB on each period
   * boundary. GTCCRC/E default to 0, so any duty written via
   * rx_gptw_set_duty_raw() gets clobbered to 0 on the next overflow --
   * outputs stick at initial state. With GTBER=0, direct writes to
   * GTCCRA/GTCCRB take effect immediately.
   * If double-buffered duty updates are ever needed, a separate opt-in
   * API should enable buffering deliberately. */
  gptw->gtber = 0U;

  /* Configure dead time if requested */
  if (config->deadtime_ns > k_gptw_deadtime_disabled) {
    /* Calculate dead time count: deadtime_ns * (PCLKA / 1e9) */
    const uint32_t deadtime_count =
      (uint32_t)((uint64_t)config->deadtime_ns * k_pclka_hz / s_gptw_ns_per_second);
    gptw->gtdvu  = deadtime_count;
    gptw->gtdvd  = deadtime_count;
    gptw->gtdtcr = k_gptw_gtdtcr_tde; /* Enable dead time */
  }

  /* Lock write protection */
  gptw->gtwp = k_gptw_gtwp_lock;

  return k_rx_ok;
}

/**
 * @brief Validate channel and compute period for PWM initialization
 *
 * @details
 * Shared pre-initialization helper used by rx_gptw_init_pwm(). Performs
 * two actions in a single function to reduce code duplication:
 * 1. Validate channel range and resolve register base pointer
 * 2. Calculate the GTPR period value from frequency and wave mode
 *
 * @param[in]  channel    GPTW channel to initialize
 *   - Valid range: k_gptw_channel_0 to k_gptw_channel_3
 * @param[in]  config     Configuration structure (caller guarantees non-NULL)
 *   - config->frequency_hz and config->wave_mode are used
 * @param[out] gptw_out   Pointer-to-pointer to receive the channel register base
 *   - Must be non-NULL; *gptw_out set on success
 * @param[out] period_out Pointer to receive the calculated period count
 *   - Must be non-NULL; *period_out set on success
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Validation and period calculation succeeded
 * @retval k_rx_err_invalid_arg channel out of range or register base unresolvable
 * @retval k_rx_err_invalid_arg frequency_hz is zero, wave_mode invalid, or period out of range
 *
 * @pre gptw_out and period_out must be non-NULL (caller's responsibility)
 * @pre config must be non-NULL (caller's responsibility)
 *
 * @post *gptw_out points to valid GPTW channel registers on success
 * @post *period_out contains a value in [k_gptw_period_min, s_gptw_period_max] on success
 *
 * @note Pure helper: does not modify hardware registers or module state
 * @note Not thread-safe: relies on config content stability during call
 *
 * @see internal_calculate_period() Period calculation details
 * @see rx_gptw_init_pwm() Primary caller
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_prepare_gptw_pwm_init(const rx_gptw_channel_t           channel,
                                               const rx_gptw_config_t*           config,
                                               volatile rx_gptw_channel_regs_t** gptw_out,
                                               uint32_t*                         period_out)
{
  if ((int32_t)channel >= (int32_t)k_gptw_max_channels) {
    rx_log_error(s_tag, "Invalid GPTW channel");
    return k_rx_err_invalid_arg;
  }

  *gptw_out = internal_get_gptw_base(channel);
  if (*gptw_out == nullptr) {
    return k_rx_err_invalid_arg;
  }

  rx_err_t err = internal_calculate_period(config->frequency_hz, config->wave_mode, period_out);
  if (err != k_rx_ok) {
    return err;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize a single GPTW channel for PWM operation
 *
 * @details
 * Configures one GPTW channel for PWM generation. This function performs complete
 * hardware setup including module clock enable, MPC configuration, port pin setup,
 * timer configuration, and automatic timer start. For 4-motor systems, prefer
 * rx_gptw_init_all_staggered() instead.
 *
 * ## Initialization Sequence
 *
 * 1. Validate config pointer (RX_CHECK_NULL_PTR)
 * 2. Validate channel range [0-3]
 * 3. Get channel register base address
 * 4. Calculate period from frequency and wave mode
 * 5. Enable GPTW module clock (MSTPCRC.MSTPC10)
 * 6. Stop timer if already running
 * 7. Configure GPTW hardware registers (GTCR, GTIOR, GTPR, etc.)
 * 8. Configure MPC for Port E alternate function
 * 9. Configure port pins (PMR, PDR)
 * 10. Store period value and set initialized flag
 * 11. Start timer
 *
 * @param[in] channel GPTW channel to initialize
 *   - Valid range: k_gptw_channel_0 to k_gptw_channel_3
 *   - Invalid channels return k_rx_err_invalid_arg
 * @param[in] config PWM configuration parameters
 *   - Must be non-NULL
 *   - See rx_gptw_config_t for field descriptions
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Channel initialized and running successfully
 * @retval k_rx_err_null_ptr config pointer is nullptr
 * @retval k_rx_err_invalid_arg channel out of range, frequency=0, or invalid wave_mode
 * @retval k_rx_err_hw_error Failed to get register base address
 *
 * @pre System clock configured (PCLKA = 120 MHz)
 * @pre Port E pins not used by other peripherals
 *
 * @post GPTW channel configured and running
 * @post Duty cycle initialized to 0%
 * @post MPC and port pins configured
 *
 * @invariant s_gptw_initialized[channel] == true on success
 * @invariant s_gptw_period[channel] contains valid period on success
 *
 * @note Not thread-safe: Call once during system initialization
 * @note Timer starts automatically - use rx_gptw_stop() to halt if needed
 *
 * @warning Calling while channel is running will cause glitches
 * @attention For 4-motor systems, use rx_gptw_init_all_staggered() instead
 *
 * @par Performance:
 * - Execution time: ~50 us at 240 MHz
 * - Memory: 0 bytes heap, ~32 bytes stack
 *
 * @par Example (Single Channel for LED PWM):
 * @code{.c}
 * rx_gptw_config_t led_config = {
 *     .frequency_hz = 1000,
 *     .deadtime_ns = 0,
 *     .wave_mode = k_gptw_wave_saw_pwm,
 *     .enable_complementary = false,
 *     .invert_polarity = false
 * };
 *
 * rx_err_t err = rx_gptw_init_pwm(k_gptw_channel_0, &led_config);
 * if (err == k_rx_ok) {
 *     rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                      rx_gptw_output_id(k_gptw_output_a), 50.0f);
 * }
 * @endcode
 *
 * @see rx_gptw_init_all_staggered() Preferred for 4-motor systems
 * @see rx_gptw_deinit() Cleanup function
 * @see rx_gptw_config_t Configuration structure
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] nullptr check, channel range check
 * - Rule 7: [OK] All internal function returns checked
 *
 * @callgraph
 */
rx_err_t rx_gptw_init_pwm(const rx_gptw_channel_t channel, const rx_gptw_config_t* config)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is nullptr");

  volatile rx_gptw_channel_regs_t* gptw   = nullptr;
  uint32_t                         period = 0U;
  rx_err_t err = internal_prepare_gptw_pwm_init(channel, config, &gptw, &period);
  if (err != k_rx_ok) {
    return err;
  }

  rx_log_info(s_tag, "Initializing GPTW");

  /* Enable GPTW module clock */
  internal_enable_gptw_module_clock();

  /* Stop timer before configuration */
  err = rx_gptw_stop(channel);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to stop GPTW");
    return err;
  }

  /* Configure GPTW hardware registers */
  err = internal_configure_gptw_hardware(gptw, config, period);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to configure GPTW hardware");
    return err;
  }

  /* Configure MPC for Port E alternate functions */
  err = internal_configure_mpc(channel);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to configure MPC");
    return err;
  }

  /* Configure port pins */
  internal_configure_port_pins(channel);

  /* Save period for duty cycle calculations */
  s_gptw_period[channel]      = period;
  s_gptw_initialized[channel] = true;

  /* Start timer */
  err = rx_gptw_start(channel);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to start GPTW");
    return err;
  }

  rx_log_info(s_tag, "GPTW initialized successfully");

  return k_rx_ok;
}

/**
 * @brief Set PWM duty cycle using floating-point percentage
 *
 * @details
 * Converts floating-point duty percentage [0.0, 100.0] to raw count value and
 * delegates to rx_gptw_set_duty_raw() for hardware register update. The compare
 * register (GTCCRA or GTCCRB) is written and the change takes effect on the next
 * PWM period boundary (glitch-free via buffer mode).
 *
 * @par Algorithm:
 * @f[
 *   \text{duty\_count} = \left\lfloor \frac{\text{duty\_percent} \times \text{period}}{100} \right\rfloor
 * @f]
 *
 * @param[in] channel      Typed channel identifier (use rx_gptw_channel_id())
 * @param[in] output       Typed output identifier (use rx_gptw_output_id())
 * @param[in] duty_percent Duty cycle [0.0, 100.0] percent
 *   - 0.0 = always-off, 100.0 = always-on
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Duty cycle updated successfully
 * @retval k_rx_err_invalid_state Channel out of range or not initialized
 * @retval k_rx_err_invalid_arg duty_percent outside [0.0, 100.0] or invalid output
 *
 * @pre Channel must be initialized via rx_gptw_init_*()
 * @pre duty_percent must be in [0.0, 100.0]
 *
 * @post Compare register written; change takes effect next period boundary
 * @post Motor output drive updated on next PWM cycle
 *
 * @note Not thread-safe: concurrent writes to same output cause undefined behavior
 *
 * @code{.c}
 * rx_gptw_set_duty(rx_gptw_channel_id(k_gptw_channel_0),
 *                  rx_gptw_output_id(k_gptw_output_a), 75.0f);
 * @endcode
 *
 * @see rx_gptw_set_duty_raw() Set duty using raw count value
 * @see rx_gptw_get_duty() Read current duty cycle
 *
 * @since Version 1.0.0
 */
rx_err_t rx_gptw_set_duty(const rx_gptw_channel_id_t channel,
                          const rx_gptw_output_id_t  output,
                          const float                duty_percent)
{
  const rx_gptw_channel_t channel_value = channel.value;
  const rx_gptw_output_t  output_value  = output.value;

  if (((int32_t)channel_value >= (int32_t)k_gptw_max_channels) ||
      !s_gptw_initialized[channel_value]) {
    return k_rx_err_invalid_state;
  }

  if (duty_percent < (float)k_gptw_duty_min || duty_percent > (float)k_gptw_duty_max) {
    rx_log_error(s_tag, "Invalid duty cycle");
    return k_rx_err_invalid_arg;
  }

  /* Convert percentage to count value */
  const uint32_t period       = s_gptw_period[channel_value];
  const float    period_f     = (float)period;
  const float    duty_count_f = (duty_percent * period_f) / (float)k_gptw_duty_divisor;
  const uint32_t duty_count   = (uint32_t)duty_count_f;

  return rx_gptw_set_duty_raw(channel_value, output_value, duty_count);
}

/**
 * @brief Set PWM duty cycle using a raw 32-bit timer count
 *
 * @details
 * Directly writes duty_count to the compare register (GTCCRA or GTCCRB).
 * Buffer mode (enabled during init via GTBER) ensures the update takes effect
 * on the next PWM period boundary without glitches. Values exceeding the period
 * are clamped to period (100% duty).
 *
 * @par Register Access:
 * - Output A: Writes to GTCCRA
 * - Output B: Writes to GTCCRB
 *
 * @param[in] channel    GPTW channel (k_gptw_channel_0 to k_gptw_channel_3)
 * @param[in] output     Output to update (k_gptw_output_a or k_gptw_output_b)
 * @param[in] duty_count Raw count [0, period]; values > period clamped to period
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Duty count written successfully
 * @retval k_rx_err_invalid_state Channel out of range or not initialized
 * @retval k_rx_err_invalid_arg Could not resolve register base or invalid output
 *
 * @pre Channel must be initialized via rx_gptw_init_*()
 * @pre duty_count should be in [0, period] for meaningful duty; higher values clamped
 *
 * @post GTCCRA or GTCCRB written with clamped duty_count
 * @post Change takes effect at next PWM period boundary
 *
 * @note Not thread-safe: concurrent writes to the same register cause undefined behavior
 * @note Values exceeding period are silently clamped to period
 *
 * @code{.c}
 * uint32_t period;
 * rx_gptw_get_period(k_gptw_channel_0, &period);
 * rx_gptw_set_duty_raw(k_gptw_channel_0, k_gptw_output_a, period / 2U); // 50%
 * @endcode
 *
 * @see rx_gptw_set_duty() Set duty using floating-point percentage
 * @see rx_gptw_get_period() Retrieve period count for duty calculation
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_gptw_set_duty_raw(const rx_gptw_channel_t channel, rx_gptw_output_t output, uint32_t duty_count)
{
  if (((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Clamp to period */
  const uint32_t period = s_gptw_period[channel];
  if (duty_count > period) {
    duty_count = period;
  }

  /* GTCCRA/GTCCRB are protected by GTWP.WP on RX72N. Unlock, write, re-lock.
   * With GTBER=0 (default after our fixed init), the written value takes
   * effect immediately rather than being double-buffered to the next period. */
  gptw->gtwp = k_gptw_gtwp_unlock;
  switch (output) {
    case k_gptw_output_a:
      gptw->gtccra = duty_count;
      break;
    case k_gptw_output_b:
      gptw->gtccrb = duty_count;
      break;
    default:
      gptw->gtwp = k_gptw_gtwp_lock;
      return k_rx_err_invalid_arg;
  }
  gptw->gtwp = k_gptw_gtwp_lock;

  return k_rx_ok;
}

/**
 * @brief Read the current PWM duty cycle as a floating-point percentage
 *
 * @details
 * Reads the current compare register value (GTCCRA or GTCCRB) from hardware
 * and converts it to a percentage using the cached period from s_gptw_period[].
 *
 * @par Algorithm:
 * @f[
 *   \text{duty\_percent} = \frac{\text{duty\_count} \times 100}{\text{period}}
 * @f]
 *
 * @param[in]  channel      GPTW channel (k_gptw_channel_0 to k_gptw_channel_3)
 * @param[in]  output       Output to read (k_gptw_output_a or k_gptw_output_b)
 * @param[out] duty_percent Pointer to store percentage result [0.0, 100.0]
 *   - Must be non-NULL
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Percentage written to *duty_percent
 * @retval k_rx_err_null_ptr duty_percent is nullptr
 * @retval k_rx_err_invalid_state Channel out of range or not initialized
 * @retval k_rx_err_invalid_arg Invalid output or register pointer unresolvable
 *
 * @pre Channel must be initialized via rx_gptw_init_*()
 * @pre duty_percent must point to valid float storage
 *
 * @post *duty_percent contains duty cycle in [0.0, 100.0] on success
 * @post Hardware registers unchanged
 *
 * @note Returns actual register value; may differ from the last set value if
 *       buffer transfer has not yet occurred at period boundary
 * @note Not thread-safe: concurrent rx_gptw_set_duty calls may cause torn reads
 *
 * @code{.c}
 * float duty;
 * rx_err_t err = rx_gptw_get_duty(k_gptw_channel_0, k_gptw_output_a, &duty);
 * @endcode
 *
 * @see rx_gptw_set_duty() Set duty using floating-point percentage
 * @see rx_gptw_get_period() Retrieve period count
 *
 * @since Version 1.0.0
 */
rx_err_t rx_gptw_get_duty(const rx_gptw_channel_t channel,
                          const rx_gptw_output_t  output,
                          float*                  duty_percent)
{
  RX_CHECK_NULL_PTR(duty_percent, s_tag, "duty_percent pointer is nullptr");

  if (((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  const volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == nullptr) {
    return k_rx_err_invalid_arg;
  }

  const uint32_t period     = s_gptw_period[channel];
  uint32_t       duty_count = k_gptw_period_zero;

  switch (output) {
    case k_gptw_output_a:
      duty_count = gptw->gtccra;
      break;
    case k_gptw_output_b:
      duty_count = gptw->gtccrb;
      break;
    default:
      return k_rx_err_invalid_arg;
  }

  *duty_percent = ((float)duty_count * (float)k_gptw_duty_max) / (float)period;

  return k_rx_ok;
}

/**
 * @brief Get PWM period count for GPTW channel
 *
 * @details
 * Implementation of rx_gptw_get_period() - see rx_gptw.h for complete documentation.
 *
 * Returns the cached period value from s_gptw_period[], which matches the
 * GTPR register value set during initialization.
 *
 * @param[in] channel GPTW channel to query
 *   - Valid range: k_gptw_channel_0 to k_gptw_channel_3
 * @param[out] period_count Pointer to store period count value
 *   - Must be non-NULL
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, period_count contains valid value
 * @retval k_rx_err_null_ptr period_count is nullptr
 * @retval k_rx_err_invalid_state Channel not initialized
 *
 * @pre Channel initialized via rx_gptw_init_*()
 * @pre period_count points to valid uint32_t storage
 *
 * @post *period_count contains period value on success
 *
 * @note Thread-safe: Reads from cached value, not hardware
 * @note Useful for calculating raw duty counts from percentages
 *
 * @see rx_gptw.h Complete API documentation
 */
rx_err_t rx_gptw_get_period(const rx_gptw_channel_t channel, uint32_t* period_count)
{
  RX_CHECK_NULL_PTR(period_count, s_tag, "period_count pointer is nullptr");

  if (((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  *period_count = s_gptw_period[channel];
  return k_rx_ok;
}

/**
 * @brief Enable or disable GPTW output pin
 *
 * @details
 * Implementation of rx_gptw_enable_output() - see rx_gptw.h for complete documentation.
 *
 * Controls the Output Enable bits (OAE/OBE) in the GTIOR register. When disabled,
 * the corresponding output pin is held in its initial state (typically LOW).
 *
 * ## Register Modification
 *
 * | Output | Bit Modified | Effect |
 * |--------|--------------|--------|
 * | Output A | GTIOR.OAE | Enable/disable GTIOCnA pin |
 * | Output B | GTIOR.OBE | Enable/disable GTIOCnB pin |
 *
 * @param[in] channel GPTW channel (0-3)
 * @param[in] output Output to control (k_gptw_output_a or k_gptw_output_b)
 * @param[in] enable true to enable PWM output, false to disable (hold LOW)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Output enable state changed successfully
 * @retval k_rx_err_invalid_state Channel not initialized
 * @retval k_rx_err_invalid_arg Invalid channel or output value
 *
 * @pre Channel initialized via rx_gptw_init_*()
 *
 * @post GTIOR.OAE or GTIOR.OBE modified
 * @post Write protection re-enabled
 *
 * @note Not thread-safe: Modifies hardware registers
 * @note Timer continues running even when output disabled
 *
 * @warning Disable outputs before motor emergency stop to ensure safe state
 *
 * @see rx_gptw.h Complete API documentation
 */
rx_err_t
rx_gptw_enable_output(const rx_gptw_channel_t channel, const rx_gptw_output_t output, bool enable)
{
  if (((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Unlock for modification */
  gptw->gtwp = k_gptw_gtwp_unlock;

  /* Modify GTIOR to enable/disable output */
  switch (output) {
    case k_gptw_output_a:
      if (enable) {
        gptw->gtior |= k_gptw_gtior_oae;
      } else {
        gptw->gtior &= ~k_gptw_gtior_oae;
      }
      break;
    case k_gptw_output_b:
      if (enable) {
        gptw->gtior |= k_gptw_gtior_obe;
      } else {
        gptw->gtior &= ~k_gptw_gtior_obe;
      }
      break;
    default:
      gptw->gtwp = k_gptw_gtwp_lock;
      return k_rx_err_invalid_arg;
  }

  gptw->gtwp = k_gptw_gtwp_lock;

  return k_rx_ok;
}

/**
 * @brief Start GPTW counter (begin PWM generation)
 *
 * @details
 * Implementation of rx_gptw_start() - see rx_gptw.h for complete documentation.
 *
 * Sets the Count Start (CST) bit in GTCR to begin counter operation. The timer
 * will count according to the configured waveform mode (sawtooth or triangle)
 * and generate PWM output on the enabled pins.
 *
 * ## Register Access
 *
 * 1. Unlock write protection (GTWP)
 * 2. Set GTCR.CST bit
 * 3. Lock write protection (GTWP)
 *
 * @param[in] channel GPTW channel to start
 *   - Valid range: k_gptw_channel_0 to k_gptw_channel_3
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Timer started successfully
 * @retval k_rx_err_invalid_arg Invalid channel value
 *
 * @pre Channel configured via rx_gptw_init_*() (recommended)
 *
 * @post Timer counting actively
 * @post PWM output generated on enabled pins
 *
 * @note Does NOT require channel to be "initialized" (no flag check)
 * @note Timer automatically started during rx_gptw_init_*()
 * @note Idempotent: Safe to call when already running
 *
 * @see rx_gptw_stop() Stop the timer
 * @see rx_gptw.h Complete API documentation
 */
rx_err_t rx_gptw_start(const rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= (int32_t)k_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Unlock and set CST bit in GTCR */
  gptw->gtwp = k_gptw_gtwp_unlock;
  gptw->gtcr |= k_gptw_gtcr_cst;
  gptw->gtwp = k_gptw_gtwp_lock;

  return k_rx_ok;
}

/**
 * @brief Stop GPTW counter (halt PWM generation)
 *
 * @details
 * Implementation of rx_gptw_stop() - see rx_gptw.h for complete documentation.
 *
 * Clears the Count Start (CST) bit in GTCR to halt counter operation. The timer
 * counter value (GTCNT) is preserved and PWM outputs hold their current state.
 *
 * ## Register Access
 *
 * 1. Unlock write protection (GTWP)
 * 2. Clear GTCR.CST bit
 * 3. Lock write protection (GTWP)
 *
 * @param[in] channel GPTW channel to stop
 *   - Valid range: k_gptw_channel_0 to k_gptw_channel_3
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Timer stopped successfully
 * @retval k_rx_err_invalid_arg Invalid channel value
 *
 * @pre None (can be called on uninitialized channel)
 *
 * @post Timer stopped (GTCR.CST = 0)
 * @post Counter value preserved (GTCNT unchanged)
 * @post Output pins hold last state
 *
 * @note Does NOT require channel to be "initialized" (no flag check)
 * @note Idempotent: Safe to call when already stopped
 * @note For safety shutdown, also disable outputs via rx_gptw_enable_output()
 *
 * @warning Outputs may remain HIGH when stopped - disable outputs for safe state
 *
 * @see rx_gptw_start() Resume the timer
 * @see rx_gptw_enable_output() Disable outputs for safe state
 * @see rx_gptw.h Complete API documentation
 */
rx_err_t rx_gptw_stop(const rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= (int32_t)k_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Unlock and clear CST bit in GTCR */
  gptw->gtwp = k_gptw_gtwp_unlock;
  gptw->gtcr &= ~k_gptw_gtcr_cst;
  gptw->gtwp = k_gptw_gtwp_lock;

  return k_rx_ok;
}

/* =============================================================================
 * Phase Staggering Implementation
 * =============================================================================
 */

/**
 * @enum gptw_phase_divisor_t
 * @brief Phase divisors for staggered PWM initialization
 *
 * @details
 * Divisor constants used to calculate the initial GTCNT counter value for each
 * GPTW channel during staggered initialization. Each channel is offset by
 * 90 degrees from the previous one to spread switching edges across the PWM period.
 *
 * For a given period P the offsets are:
 * - Ch0: 0         (0/4 of P)
 * - Ch1: P / 4     (quarter divisor)
 * - Ch2: P / 2     (half divisor)
 * - Ch3: 3 * P / 4 (three-quarter: multiply by 3, then divide by 4)
 *
 * @invariant All divisor values are >= 1, preventing division-by-zero in
 *            internal_calculate_phase_offset()
 *
 * @code{.c}
 * // Example: derive 90-degree offset for a period of 3000 counts
 * uint32_t offset_ch1 = 3000U / k_phase_divisor_quarter; // == 750
 * uint32_t offset_ch3 = (3000U * 3U) / k_phase_divisor_three_quarter; // == 2250
 * @endcode
 *
 * @see internal_calculate_phase_offset() Uses these divisors for GTCNT seeding
 * @see rx_gptw_init_all_staggered() Top-level staggered initialization function
 */
typedef enum : uint8_t {
  k_phase_divisor_none    = 1, /**< 0 deg: no offset (safe default, never causes div-by-zero) */
  k_phase_divisor_quarter = 4, /**< 90 deg = period/4 */
  k_phase_divisor_half    = 2, /**< 180 deg = period/2 */
  k_phase_divisor_three_quarter = 4, /**< 270 deg = 3*period/4 (multiplied by 3 then divided) */
} gptw_phase_divisor_t;

/**
 * @brief Calculate initial counter value and direction for phase staggering
 *
 * @details
 * Computes the initial counter value (GTCNT) and counting direction for a GPTW
 * channel to achieve the desired phase offset. Phase staggering spreads PWM edges
 * across the period to reduce peak current draw and EMI.
 *
 * ## Phase Offset Table (Sawtooth Mode)
 *
 * | Channel | Phase | Counter Init | Direction |
 * |---------|-------|--------------|-----------|
 * | Ch0 | 0deg | 0 | Up (n/a) |
 * | Ch1 | 90deg | period/4 | Up (n/a) |
 * | Ch2 | 180deg | period/2 | Up (n/a) |
 * | Ch3 | 270deg | 3xperiod/4 | Up (n/a) |
 *
 * ## Phase Offset Table (Triangle Mode)
 *
 * | Channel | Phase | Counter Init | Direction |
 * |---------|-------|--------------|-----------|
 * | Ch0 | 0deg | 0 | Up |
 * | Ch1 | 90deg | period/2 | Up |
 * | Ch2 | 180deg | period | Down |
 * | Ch3 | 270deg | period/2 | Down |
 *
 * @param[in] channel GPTW channel for phase calculation
 *   - Valid range: k_gptw_channel_0 to k_gptw_channel_3
 *   - Invalid values result in default outputs (0, true)
 * @param[in] period PWM period in counter counts
 *   - Must be > 0 to avoid division by zero
 *   - Zero period results in default outputs (0, true)
 * @param[in] is_triangle true for triangle (center-aligned), false for sawtooth
 * @param[out] init_count Pointer to store initial counter value
 *   - Must be non-NULL
 *   - Range: [0, period]
 * @param[out] init_dir_up Pointer to store initial count direction
 *   - Must be non-NULL
 *   - true = counting up, false = counting down
 *   - Only meaningful for triangle mode
 *
 * @pre Output pointers must be non-NULL
 * @pre period should be > 0 for valid calculations
 *
 * @post *init_count contains phase offset value
 * @post *init_dir_up contains initial direction (triangle mode)
 * @post Safe defaults (0, true) on any validation failure
 *
 * @note Returns void - validation failures produce safe defaults silently
 * @note For sawtooth mode, direction is ignored (always up)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] nullptr checks, period validation
 * - Rule 2: [OK] No loops (direct calculation)
 *
 * @see internal_configure_channel_staggered() Calls this function
 * @see gptw_phase_divisor_t Divisor constants
 */
static void internal_calculate_phase_offset(const rx_gptw_channel_t channel,
                                            const uint32_t          period,
                                            const bool              is_triangle,
                                            uint32_t*               init_count,
                                            bool*                   init_dir_up)
{
  /* Pre-condition: validate output pointers (NASA Power of 10 Rule 5) */
  if (init_count == nullptr || init_dir_up == nullptr) {
    /* Cannot proceed without valid output pointers - caller error */
    return;
  }

  /* Pre-condition: validate period is non-zero to avoid division by zero */
  if (period == 0) {
    *init_count  = 0;
    *init_dir_up = true;
    return;
  }

  /* Pre-condition: validate channel is in valid range */
  if ((int32_t)channel >= (int32_t)k_gptw_max_channels) {
    *init_count  = 0;
    *init_dir_up = true;
    return;
  }

  *init_dir_up = true; /* Default to counting up */

  if (!is_triangle) {
    /* Edge-Aligned (Sawtooth) Staggering:
     * Ch0 (0 deg):   0
     * Ch1 (90 deg):  Period / 4
     * Ch2 (180 deg): Period / 2
     * Ch3 (270 deg): 3 * Period / 4
     */
    switch (channel) {
      case k_gptw_channel_0:
        *init_count = 0;
        break;
      case k_gptw_channel_1:
        *init_count = period / k_phase_divisor_quarter;
        break;
      case k_gptw_channel_2:
        *init_count = period / k_phase_divisor_half;
        break;
      case k_gptw_channel_3:
        *init_count = (period * 3) / k_phase_divisor_three_quarter;
        break;
      default:
        *init_count = 0;
        break;
    }
  } else {
    /* Center-Aligned (Triangle) Staggering:
     * Ch0 (0 deg):   0, Up
     * Ch1 (90 deg):  Period/2, Up
     * Ch2 (180 deg): Period, Down (Wait: Period is Top)
     *                Note: At Period, it turns around.
     *                For 180 shift, we want the "center" of the OFF pulse to correspond
     *                to the "center" of Ch0 ON pulse?
     *                Standard 3-phase shift logic applied to 4-phase:
     *                0 deg: 0 (Valley) -> Up
     *                90 deg: Period/2 -> Up
     *                180 deg: Period (Peak) -> Down (naturally acts as peak)
     *                270 deg: Period/2 -> Down
     */
    switch (channel) {
      case k_gptw_channel_0:
        *init_count  = 0;
        *init_dir_up = true;
        break;
      case k_gptw_channel_1:
        *init_count  = period / k_phase_divisor_half;
        *init_dir_up = true;
        break;
      case k_gptw_channel_2:
        *init_count  = period;
        *init_dir_up = false; /* Start at top, go down */
        break;
      case k_gptw_channel_3:
        *init_count  = period / k_phase_divisor_half;
        *init_dir_up = false; /* Start at mid, go down */
        break;
      default:
        *init_count = 0;
        break;
    }
  }
}

/**
 * @brief Configure a single GPTW channel for staggered PWM operation
 *
 * @details
 * Performs complete hardware configuration for one GPTW channel including
 * base timer setup, MPC pin function selection, port configuration, and
 * phase staggering initialization.
 *
 * ## Configuration Sequence
 *
 * 1. Validate channel range [0-3]
 * 2. Get channel register base address
 * 3. Configure base hardware (GTCR, GTIOR, GTPR, etc.)
 * 4. Configure MPC for Port E alternate function
 * 5. Configure port pins (PMR, PDR)
 * 6. Calculate phase offset for this channel
 * 7. Set initial counter value (GTCNT)
 * 8. For triangle mode: seed initial count direction
 * 9. Update initialization tracking state
 *
 * ## Phase Staggering
 *
 * Counter is initialized to a phase-offset value:
 * - Ch0: 0deg (reference, GTCNT=0)
 * - Ch1: 90deg (GTCNT=period/4)
 * - Ch2: 180deg (GTCNT=period/2)
 * - Ch3: 270deg (GTCNT=3xperiod/4)
 *
 * @param[in] channel GPTW channel to configure
 *   - Valid range: k_gptw_channel_0 to k_gptw_channel_3
 * @param[in] config Configuration parameters (frequency, deadtime, mode)
 *   - Must be non-NULL (caller's responsibility)
 * @param[in] period Pre-calculated period register value
 *   - Obtained from internal_calculate_period()
 * @param[in] is_triangle true if using triangle wave mode
 *   - Determines direction seeding requirement
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Channel configured successfully
 * @retval k_rx_err_invalid_arg Channel out of range or nullptr gptw base
 * @retval Others Propagated from internal_configure_gptw_hardware, internal_configure_mpc
 *
 * @pre Module clock enabled
 * @pre Timer stopped
 * @pre config pointer valid (caller validates)
 *
 * @post Channel fully configured with phase offset
 * @post s_gptw_initialized[channel] = true
 * @post s_gptw_period[channel] = period
 * @post Timer NOT started (caller starts all channels together)
 *
 * @note Not thread-safe: modifies hardware registers and shared state arrays
 *       (s_gptw_initialized, s_gptw_period); caller must ensure exclusive access
 * @note Timer is NOT started by this function
 * @note Called from rx_gptw_init_all_staggered() in a loop
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] Channel range validation
 * - Rule 7: [OK] All internal function returns checked
 *
 * @see rx_gptw_init_all_staggered() Parent function
 * @see internal_calculate_phase_offset() Phase calculation
 */
static rx_err_t internal_configure_channel_staggered(const rx_gptw_channel_t channel,
                                                     const rx_gptw_config_t* config,
                                                     const uint32_t          period,
                                                     const bool              is_triangle)
{
  /* Pre-condition: validate channel range (NASA Power of 10 Rule 5) */
  if ((int32_t)channel >= (int32_t)k_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);

  /* Pre-condition: validate gptw pointer */
  if (gptw == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Base hardware configuration */
  rx_err_t err = internal_configure_gptw_hardware(gptw, config, period);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure MPC and Pins */
  err = internal_configure_mpc(channel);
  if (err != k_rx_ok) {
    return err;
  }
  internal_configure_port_pins(channel);

  /* Apply Phase Staggering */
  uint32_t init_count = 0;
  bool     dir_up     = true;

  internal_calculate_phase_offset(channel, period, is_triangle, &init_count, &dir_up);

  /* Modify counter value (unlocked by configure function, but we need to unlock again
   * since internal_configure_gptw_hardware locks it at the end)
   */
  gptw->gtwp  = k_gptw_gtwp_unlock;
  gptw->gtcnt = init_count;

  /* Handle Direction Seeding for Triangle Mode */
  if (is_triangle) {
    /* Use GTUDDTYC to force initial direction
     * UD (bit 0): 0=Down, 1=Up
     * UDF (bit 1): 1=Force
     * Sequence: Set Force -> Clear Force (latch direction)
     * Note: Manual verification confirmed UD=bit0, UDF=bit1.
     */
    uint32_t ud_val = dir_up ? k_gptw_gtuddtyc_ud : k_gptw_gtuddtyc_ud_down;

    /* Force direction */
    gptw->gtuddtyc = k_gptw_gtuddtyc_udf | ud_val;

    /* Clear force bit to allow normal counting operation */
    gptw->gtuddtyc = ud_val;
  }

  gptw->gtwp = k_gptw_gtwp_lock;

  /* Update internal state */
  s_gptw_period[channel]      = period;
  s_gptw_initialized[channel] = true;

  return k_rx_ok;
}

/**
 * @brief Initialize all 4 GPTW channels with 90-degree phase staggering
 *
 * @details
 * Implementation of rx_gptw_init_all_staggered() - see rx_gptw.h for complete
 * documentation with diagrams, examples, and NASA compliance notes.
 *
 * ## Implementation Overview
 *
 * 1. **Validate inputs**: NULL check, frequency > 0, valid wave_mode
 * 2. **Enable module clock**: Clear MSTPCRC.MSTPC10
 * 3. **Stop all channels**: Atomic stop via GTSTPA register
 * 4. **Calculate period**: Common period for all channels
 * 5. **Configure each channel**: Loop calls internal_configure_channel_staggered()
 * 6. **Start all channels**: Atomic start via GTSTRA register
 *
 * ## Phase Staggering Benefits
 *
 * - Reduces peak current by spreading PWM edges across period
 * - Lowers EMI from simultaneous switching transients
 * - Allows smaller power supply capacitors
 *
 * @param[in] config Pointer to common configuration for all channels
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All 4 channels initialized and running
 * @retval k_rx_err_null_ptr config is nullptr
 * @retval k_rx_err_invalid_arg Invalid frequency or wave_mode
 * @retval k_rx_err_hw_error GPTW common registers inaccessible
 *
 * @see rx_gptw.h Complete API documentation with diagrams and examples
 *
 * @callgraph
 */
rx_err_t rx_gptw_init_all_staggered(const rx_gptw_config_t* config)
{
  /* Pre-condition: validate config pointer (NASA Power of 10 Rule 5) */
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is nullptr");

  /* Pre-condition: validate frequency is non-zero */
  if (config->frequency_hz == 0) {
    rx_log_error(s_tag, "frequency_hz is zero");
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition: validate wave_mode is within allowed values */
  switch (config->wave_mode) {
    case k_gptw_wave_saw_pwm:
    case k_gptw_wave_tri_pwm1:
    case k_gptw_wave_tri_pwm2:
    case k_gptw_wave_tri_pwm3:
      break;
    default:
      rx_log_error(s_tag, "Invalid wave_mode");
      return k_rx_err_invalid_arg;
  }

  rx_log_info(s_tag, "Initializing all GPTW channels (staggered)");

  /* Enable module clock (enables all channels) */
  internal_enable_gptw_module_clock();

  /* Stop all channels atomically to ensure clean config */
  if (gptw_common() != nullptr) {
    gptw_common()->gtstpa = k_gptw_all_channels_mask; /* Stop Ch0-3 */
  }

  /* Calculate period once (assumes same frequency for all) */
  uint32_t period;
  rx_err_t err = internal_calculate_period(config->frequency_hz, config->wave_mode, &period);
  if (err != k_rx_ok) {
    return err;
  }

  const bool is_triangle = internal_is_triangle_mode(config->wave_mode);

  /* Configure each channel using helper function */
  for (uint8_t i = 0; i < k_gptw_max_channels; i++) {
    rx_gptw_channel_t channel = (rx_gptw_channel_t)i;

    err = internal_configure_channel_staggered(channel, config, period, is_triangle);
    if (err != k_rx_ok) {
      return err;
    }
  }

  /* Start all channels atomically */
  if (gptw_common() != nullptr) {
    rx_log_info(s_tag, "Global start of GPTW channels");
    gptw_common()->gtstra = k_gptw_all_channels_mask; /* Start Ch0-3 */
  } else {
    return k_rx_err_hw_error;
  }

  return k_rx_ok;
}

/**
 * @brief Deinitialize a GPTW channel
 *
 * @details
 * Stops the timer, disables both PWM outputs (A and B), and resets
 * the counter to zero. Call only when motor control is no longer active.
 *
 * @param[in] channel GPTW channel (0-3)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_invalid_arg Invalid channel number
 *
 * @pre Motor using this channel must be stopped before calling
 * @pre channel must be in range [k_gptw_channel_0, k_gptw_channel_3]
 *
 * @post Timer counter stopped (GTCR.CST = 0)
 * @post Both outputs disabled (GTIOR.OAE = 0, GTIOR.OBE = 0)
 * @post Counter register zeroed (GTCNT = k_gptw_period_zero)
 *
 * @invariant Write protection re-locked (GTWP) on all exit paths
 *
 * @note Not thread-safe: modifies hardware registers directly
 * @note Does not clear s_gptw_initialized[] or s_gptw_period[]
 *
 * @warning Ensure motor is at rest before calling to prevent abrupt stop
 *
 * @code{.c}
 * rx_err_t err = rx_gptw_deinit(k_gptw_channel_0);
 * @endcode
 *
 * @see rx_gptw_init_all() Re-initialize after deinit
 * @see rx_gptw_stop() Stop without full teardown
 *
 * @since Version 1.0.0
 */
rx_err_t rx_gptw_deinit(const rx_gptw_channel_t channel)
{
  if ((int32_t)channel >= (int32_t)k_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Unlock write protection */
  gptw->gtwp = k_gptw_gtwp_unlock;

  /* Stop counter */
  gptw->gtcr &= ~k_gptw_gtcr_cst;

  /* Disable both outputs (OAE and OBE) */
  gptw->gtior &= ~(k_gptw_gtior_oae | k_gptw_gtior_obe);

  /* Reset counter to zero */
  gptw->gtcnt = k_gptw_period_zero;

  /* Re-lock write protection */
  gptw->gtwp = k_gptw_gtwp_lock;

  return k_rx_ok;
}

#endif /* __RX__ */
