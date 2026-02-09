/* lib/rx_hal/src/rx_mtu.c */

/**
 * @file rx_mtu.c
 * @brief MTU PWM Driver Implementation for RX72N
 *
 * @details
 * **Production-grade implementation** of the Multi-Function Timer Unit (MTU)
 * peripheral driver for PWM generation on RX72N. The MTU provides 16-bit
 * timer resolution with multiple outputs per channel.
 *
 * ## Module Architecture
 *
 * The MTU peripheral is organized as:
 * - **MTU0-MTU4**: First group, controlled by MSTPCRA.MSTPA9 and TSTRA
 * - **MTU6-MTU7**: Second group, controlled by MSTPCRA.MSTPA8 and TSTRB
 * - **Note**: MTU5 exists but is excluded from general PWM use
 *
 * ## PWM Mode 1 (Triangle Wave - Center-Aligned)
 *
 * This driver configures MTU in PWM Mode 1 for center-aligned PWM:
 * - Counter counts up from 0 to TGRA (period), then down to 0
 * - Compare matches on TGRB/TGRC/TGRD control duty cycle
 * - Symmetric PWM pulses reduce harmonic content
 * - Lower EMI compared to edge-aligned PWM
 *
 * @par PWM Timing Diagram:
 *
 * ```
 *              TGRA (period)
 *                  |
 *       /\        /\        /\
 *      /  \      /  \      /  \
 *     /    \    /    \    /    \
 *    /      \  /      \  /      \
 *   0--------0--------0--------0
 *        |  |    |  |
 *      TGRB  TGRB  (compare match points)
 * ```
 *
 * ## Frequency and Resolution
 *
 * @par Calculation:
 * @f[
 *   \text{Period} = \frac{f_{PCLKA}}{2 \times f_{PWM}}
 * @f]
 *
 * @par Example: 20kHz PWM with PCLKA=120MHz:
 *
 * | Parameter | Value | Notes |
 * |-----------|-------|-------|
 * | Period | 3000 counts | 120MHz / (2 × 20kHz) |
 * | Resolution | ~11.5 bits | log2(3000) ≈ 11.5 |
 * | Duty range | 0-3000 | 0-100% in integer counts |
 * | Step size | 0.033% | 100% / 3000 |
 *
 * ## Register Base Addresses
 *
 * | Channel | Base Address | TSTR Register |
 * |---------|--------------|---------------|
 * | MTU0 | 0x000C1290 | TSTRA (bit 0) |
 * | MTU1 | 0x000C1290 | TSTRA (bit 1) |
 * | MTU2 | 0x000C1292 | TSTRA (bit 2) |
 * | MTU3 | 0x000C1200 | TSTRA (bit 6) |
 * | MTU4 | 0x000C1200 | TSTRA (bit 7) |
 * | MTU6 | 0x000C1A00 | TSTRB (bit 6) |
 * | MTU7 | 0x000C1A00 | TSTRB (bit 7) |
 *
 * ## Static Variables
 *
 * | Variable | Type | Purpose |
 * |----------|------|---------|
 * | s_mtu_initialized[] | bool[8] | Track which channels are initialized |
 * | s_mtu_period[] | uint16_t[8] | Cache period values for duty calculation |
 * | s_tag | const char* | Log tag "MTU" |
 *
 * @par NASA Power of 10 Compliance:
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | ✓ | No goto/setjmp/recursion |
 * | 2. Fixed loop bounds | ✓ | No unbounded loops |
 * | 3. No dynamic allocation | ✓ | All static, zero malloc |
 * | 4. Small functions | ✓ | All functions < 60 lines |
 * | 5. Assertions | ✓ | RX_VALIDATE_PTR, RX_VALIDATE_INIT |
 * | 6. Narrow scope | ✓ | Static variables, internal functions |
 * | 7. Check return values | ✓ | All rx_err_t returns checked |
 * | 8. Limited preprocessor | ✓ | C23 typed enums for constants |
 * | 9. Pointer restrictions | ✓ | Single-level pointers |
 * | 10. Compiler warnings | ✓ | Clean with -Wall -Wextra -Werror |
 *
 * @par SOLID Principles:
 *
 * **Single Responsibility**: This file handles ONLY MTU PWM generation.
 * Motor control logic and encoder reading are in separate modules.
 *
 * @see rx_mtu.h Public API documentation
 * @see rx72n_mtu_regs.h Register structure definitions
 * @see RX72N Hardware Manual Chapter 24 - Multi-Function Timer Unit
 *
 * @author STAR Team
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#include "rx_mtu.h"

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_log.h"
#include "rx_register_protection.h"

static const char* s_tag = "MTU";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief MTU general constants */
typedef enum : uint8_t {
  k_mtu_max_channels = 8, /**< MTU0-MTU4, MTU6-MTU7 (sparse indexing, max index is 7) */
} mtu_constants_t;

/** @brief MTU module stop bit positions in MSTPCRA */
typedef enum : uint8_t {
  k_mtu_mstpa_mtu0_4 = 9, /**< MTU0-MTU4 module stop bit */
  k_mtu_mstpa_mtu6_7 = 8, /**< MTU6-MTU7 module stop bit */
} mtu_module_stop_bits_t;

/** @brief Bit manipulation constants */
typedef enum : uint32_t {
  k_mtu_bit_one = 1UL, /**< Single bit value for shifts */
} mtu_bit_constants_t;

/** @brief Period calculation constants */
typedef enum : uint16_t {
  k_mtu_period_divisor = 2,      /**< Triangle wave period divisor */
  k_mtu_period_max     = 0xFFFF, /**< Maximum valid period (16-bit) */
  k_mtu_period_min     = 10,     /**< Minimum valid period */
  k_mtu_period_zero    = 0,      /**< Zero period value */
} mtu_period_constants_t;

/** @brief TIOR register shift positions */
typedef enum : uint8_t {
  k_mtu_tior_low_shift  = 0, /**< Low nibble shift (MTIOCA/MTIOCC) */
  k_mtu_tior_high_shift = 4, /**< High nibble shift (MTIOCB/MTIOCD) */
} mtu_tior_shift_t;

/** @brief TIOR register mask values */
typedef enum : uint8_t {
  k_mtu_tior_low_mask  = 0xF0, /**< Mask for low nibble */
  k_mtu_tior_high_mask = 0x0F, /**< Mask for high nibble */
} mtu_tior_mask_t;

/** @brief TIOR output disabled value */
typedef enum : uint8_t {
  k_mtu_tior_disabled = 0x00, /**< Output disabled */
} mtu_tior_disabled_t;

/** @brief Duty cycle calculation constants */
typedef enum : uint8_t {
  k_mtu_duty_min     = 0,   /**< Minimum duty cycle (0%) */
  k_mtu_duty_max     = 100, /**< Maximum duty cycle (100%) */
  k_mtu_duty_divisor = 100, /**< Divisor for percentage conversion */
} mtu_duty_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/* Track initialized channels */
static bool     s_mtu_initialized[k_mtu_max_channels] = {false};
static uint16_t s_mtu_period[k_mtu_max_channels]      = {0};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get MTU channel register base address
 *
 * @details
 * Maps an MTU channel enumeration to its hardware register base address.
 * The MTU peripheral has sparse channel numbering (0-4, 6-7 with no 5).
 *
 * ## Memory Map
 *
 * | Channel | Base Address | Description |
 * |---------|--------------|-------------|
 * | MTU0 | 0x000C1290 | 16-bit timer with 4 outputs |
 * | MTU1 | 0x000C1290 | 16-bit timer with 2 outputs |
 * | MTU2 | 0x000C1292 | 16-bit timer with 2 outputs |
 * | MTU3 | 0x000C1200 | 16-bit timer with 4 outputs |
 * | MTU4 | 0x000C1200 | 16-bit timer with 4 outputs |
 * | MTU6 | 0x000C1A00 | 16-bit timer with 4 outputs |
 * | MTU7 | 0x000C1A00 | 16-bit timer with 4 outputs |
 *
 * @param[in] channel MTU channel identifier
 *   - Valid: k_mtu_channel_0 through k_mtu_channel_7 (excluding 5)
 *   - Invalid values return NULL
 *
 * @return Pointer to MTU register base
 * @retval Non-NULL Pointer to register structure for valid channels
 * @retval NULL Invalid channel (including k_mtu_channel_5)
 *
 * @pre Module clock must be enabled before accessing returned pointer
 *
 * @post Returned pointer valid for hardware access
 * @post No hardware state modified
 *
 * @note Thread-safe: Returns constant address
 * @note Returns void* to accommodate different register structure types
 *
 * @see mtu0(), mtu1(), etc. Underlying accessor functions
 * @see rx72n_mtu_regs.h Register structure definitions
 *
 * @since Version 1.0.0
 */
static volatile void* internal_get_mtu_base(const rx_mtu_channel_t channel)
{
  switch (channel) {
    case k_mtu_channel_0:
      return (volatile void*)mtu0();
    case k_mtu_channel_1:
      return (volatile void*)mtu1();
    case k_mtu_channel_2:
      return (volatile void*)mtu2();
    case k_mtu_channel_3:
      return (volatile void*)mtu3();
    case k_mtu_channel_4:
      return (volatile void*)mtu4();
    case k_mtu_channel_6:
      return (volatile void*)mtu6();
    case k_mtu_channel_7:
      return (volatile void*)mtu7();
    default:
      return nullptr;
  }
}

/**
 * @brief Check if MTU channel is valid
 *
 * @details
 * Validates an MTU channel by checking if it has a valid register base address.
 * MTU channels 0-4 and 6-7 are valid; channel 5 does not exist.
 *
 * @param[in] channel MTU channel to validate
 *
 * @return bool Validation result
 * @retval true Channel is valid (0-4, 6-7)
 * @retval false Channel is invalid (5 or out of range)
 *
 * @note Thread-safe: Pure function
 * @note Delegates to internal_get_mtu_base() for actual validation
 *
 * @see internal_get_mtu_base() Underlying validation logic
 *
 * @since Version 1.0.0
 */
static bool internal_is_valid_channel(const rx_mtu_channel_t channel)
{
  return internal_get_mtu_base(channel) != nullptr;
}

static rx_err_t internal_clear_tstr_bit(volatile rx_mtu_tstr_regs_t* tstr, const uint8_t mask)
{
  if (tstr == nullptr) {
    return k_rx_err_invalid_arg;
  }

  tstr->tstr &= (uint8_t)~mask;
  if ((tstr->tstr & mask) != 0) {
    return k_rx_err_hw_error;
  }

  return k_rx_ok;
}

/**
 * @brief Calculate period register value from frequency
 *
 * @details
 * Converts desired PWM frequency to the TGRA (period) register value for
 * PWM Mode 1 (triangle wave, center-aligned).
 *
 * ## Calculation
 *
 * @f[
 *   \text{period} = \frac{f_{PCLKA}}{2 \times f_{PWM}}
 * @f]
 *
 * Where:
 * - @f$ f_{PCLKA} @f$ = 120 MHz
 * - Divisor of 2 accounts for up/down counting in triangle mode
 *
 * ## Valid Frequency Range
 *
 * | Frequency | Period | Status |
 * |-----------|--------|--------|
 * | < 915 Hz | > 65535 | Rejected (overflow) |
 * | 915 Hz - 6 MHz | 10-65535 | Valid |
 * | > 6 MHz | < 10 | Rejected (too small) |
 *
 * @param[in] frequency_hz Desired PWM frequency in Hz
 *   - Valid range: ~915 Hz to ~6 MHz
 *   - Zero frequency rejected
 * @param[out] period Pointer to store calculated period value
 *   - Must be non-NULL
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, period contains valid 16-bit value
 * @retval k_rx_err_null_ptr period pointer is nullptr
 * @retval k_rx_err_invalid_arg Frequency=0, too high, or too low
 *
 * @pre PCLKA configured to 120 MHz
 * @pre period points to valid uint16_t storage
 *
 * @post *period contains value in range [10, 65535]
 *
 * @note 16-bit resolution limits frequency range compared to 32-bit GPTW
 *
 * @see k_pclka_hz PCLKA clock frequency
 * @see k_mtu_period_max Maximum 16-bit period value
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_calculate_period(const uint32_t frequency_hz, uint16_t* period)
{
  uint32_t period_calc;

  /* For PWM mode 1 (triangle wave):
   * Period = PCLKA / (2 * frequency)
   * PCLKA = 120 MHz
   */
  const uint32_t pclka = k_pclka_hz;

  if (period == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (frequency_hz == k_mtu_period_zero) {
    return k_rx_err_invalid_arg;
  }

  period_calc = pclka / (k_mtu_period_divisor * frequency_hz);

  /* Check if period fits in 16-bit register */
  if (period_calc > k_mtu_period_max) {
    rx_log_error(s_tag, "Frequency too low");
    return k_rx_err_invalid_arg;
  }

  if (period_calc < k_mtu_period_min) {
    rx_log_error(s_tag, "Frequency too high");
    return k_rx_err_invalid_arg;
  }

  *period = (uint16_t)period_calc;
  return k_rx_ok;
}

/**
 * @brief Get TGR register pointer for output channel
 *
 * @param[in] mtu MTU base pointer
 * @param[in] output Output channel
 *
 * @return Pointer to TGR register, or nullptr if invalid
 */
static volatile uint16_t* internal_get_tgr_register(volatile rx_mtu_channel_regs_t* mtu,
                                                    const rx_mtu_output_t           output)
{
  if (mtu == nullptr) {
    return nullptr;
  }

  switch (output) {
    case k_mtu_output_a:
      return &mtu->tgra;
    case k_mtu_output_b:
      return &mtu->tgrb;
    case k_mtu_output_c:
      return &mtu->tgrc;
    case k_mtu_output_d:
      return &mtu->tgrd;
    default:
      return nullptr;
  }
}

static rx_err_t internal_set_duty_raw_mtu(volatile rx_mtu_channel_regs_t* mtu,
                                          const rx_mtu_output_t           output,
                                          const uint16_t                  duty_count)
{
  if (mtu == nullptr) {
    return k_rx_err_invalid_arg;
  }

  volatile uint16_t* tgr = internal_get_tgr_register(mtu, output);
  if (tgr == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Update duty cycle (buffered, takes effect on next period) */
  *tgr = duty_count;
  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize MTU channel for PWM operation
 *
 * @details
 * Configures an MTU channel for PWM Mode 1 (triangle wave, center-aligned)
 * operation. Performs complete hardware setup including module clock enable,
 * timer configuration, and automatic start.
 *
 * ## Initialization Sequence
 *
 * 1. Validate config pointer
 * 2. Get channel register base address
 * 3. Calculate period from frequency
 * 4. Enable MTU module clock (clear MSTPA bit)
 * 5. Stop timer if running
 * 6. Configure TCR (clock source, clear condition)
 * 7. Configure TMDR (PWM Mode 1)
 * 8. Configure TIORH/TIORL (output behavior)
 * 9. Set TGRA (period), clear TGRB/TGRC/TGRD (duty = 0)
 * 10. Clear TCNT counter
 * 11. Store period and set initialized flag
 * 12. Start timer
 *
 * ## PWM Mode 1 Configuration
 *
 * | Register | Value | Effect |
 * |----------|-------|--------|
 * | TCR | TPSC=1, CCLR=TGRA | PCLKA/1, clear on TGRA match |
 * | TMDR | MD=PWM1 | Triangle wave mode |
 * | TIORH/L | Init low | Outputs toggle on compare match |
 *
 * @param[in] channel MTU channel to initialize (0-4, 6-7)
 * @param[in] config PWM configuration parameters
 *   - frequency_hz: Desired PWM frequency [915 Hz - 6 MHz]
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Channel initialized and running
 * @retval k_rx_err_null_ptr config is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel or frequency
 *
 * @pre System clock configured (PCLKA = 120 MHz)
 * @pre config pointer valid with valid frequency
 *
 * @post MTU channel configured and timer running
 * @post All duty cycles initialized to 0%
 * @post s_mtu_initialized[channel] = true
 *
 * @note Timer starts automatically
 * @note Call once during system initialization
 *
 * @warning Not thread-safe: Configure before starting motor tasks
 *
 * @par Example:
 * @code{.c}
 * rx_mtu_config_t config = {
 *     .frequency_hz = 20000  // 20 kHz
 * };
 * rx_err_t err = rx_mtu_init_pwm(k_mtu_channel_0, &config);
 * @endcode
 *
 * @see rx_mtu.h Complete API documentation
 * @see rx_mtu_deinit() Cleanup function
 *
 * @since Version 1.0.0
 *
 * @callgraph
 */
rx_err_t rx_mtu_init_pwm(const rx_mtu_channel_t channel, const rx_mtu_config_t* config)
{
  volatile rx_mtu_channel_regs_t* mtu;
  uint16_t                        period;
  rx_err_t                        err;

  RX_VALIDATE_PTR(config, s_tag, "config pointer is nullptr");

  mtu = (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Calculate period from frequency */
  err = internal_calculate_period(config->frequency_hz, &period);
  if (err != k_rx_ok) {
    return err;
  }

  rx_log_info(s_tag, "Initializing MTU");

  /* Enable MTU module (clear module stop bit) */
  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;

  if (channel <= k_mtu_channel_4) {
    system_regs()->mstpcra &= ~(k_mtu_bit_one << k_mtu_mstpa_mtu0_4);
  } else {
    system_regs()->mstpcra &= ~(k_mtu_bit_one << k_mtu_mstpa_mtu6_7);
  }

  *prcr_reg() = k_rx_prcr_lock;

  /* Stop timer before configuration */
  err = rx_mtu_stop(channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure timer control register
   * - PCLKA/1 (120 MHz)
   * - Clear counter on TGRA compare match
   */
  mtu->tcr = k_mtu_tcr_tpsc_1 | k_mtu_tcr_cclr_tgra;

  /* Configure PWM mode 1 (triangle wave, center-aligned) */
  mtu->tmdr = k_mtu_tmdr_md_pwm1;

  /* Configure I/O control registers for PWM outputs
   * TIORH: Controls MTIOCA (output A) and MTIOCB (output B)
   * TIORL: Controls MTIOCC (output C) and MTIOCD (output D)
   *
   * For PWM mode 1:
   * - Initial low, high on up-count compare, low on down-count compare
   */
  mtu->tiorh = (k_mtu_tior_init_low << k_mtu_tior_low_shift) | /* MTIOCA */
               (k_mtu_tior_init_low << k_mtu_tior_high_shift); /* MTIOCB */
  mtu->tiorl = (k_mtu_tior_init_low << k_mtu_tior_low_shift) | /* MTIOCC */
               (k_mtu_tior_init_low << k_mtu_tior_high_shift); /* MTIOCD */

  /* Set period (TGRA = top of triangle wave) */
  mtu->tgra = period;

  /* Set initial duty cycle to 0% for all outputs */
  mtu->tgrb = k_mtu_period_zero; /* MTIOCB duty */
  mtu->tgrc = k_mtu_period_zero; /* MTIOCC duty */
  mtu->tgrd = k_mtu_period_zero; /* MTIOCD duty */

  /* Clear counter */
  mtu->tcnt = k_mtu_period_zero;

  /* Save period for duty cycle calculations */
  s_mtu_period[channel]      = period;
  s_mtu_initialized[channel] = true;

  /* Start timer */
  err = rx_mtu_start(channel);
  if (err != k_rx_ok) {
    return err;
  }

  rx_log_info(s_tag, "MTU initialized successfully");

  return k_rx_ok;
}

rx_err_t
rx_mtu_set_duty(const rx_mtu_channel_t channel, rx_mtu_output_t output, const float duty_percent)
{
  volatile rx_mtu_channel_regs_t* mtu;
  uint16_t                        period;
  uint16_t                        duty_count;

  mtu = (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == nullptr) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_mtu_initialized[channel], s_tag, "MTU channel not initialized");

  if (duty_percent < (float)k_mtu_duty_min || duty_percent > (float)k_mtu_duty_max) {
    rx_log_error(s_tag, "Invalid duty cycle");
    return k_rx_err_invalid_arg;
  }

  /* Convert percentage to count value */
  period     = s_mtu_period[channel];
  duty_count = (uint16_t)((duty_percent * period) / (float)k_mtu_duty_divisor);

  return internal_set_duty_raw_mtu(mtu, output, duty_count);
}

rx_err_t
rx_mtu_set_duty_raw(const rx_mtu_channel_t channel, rx_mtu_output_t output, uint16_t duty_count)
{
  volatile rx_mtu_channel_regs_t* mtu;
  uint16_t                        period;

  mtu = (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == nullptr) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_mtu_initialized[channel], s_tag, "MTU channel not initialized");

  /* Clamp to period */
  period = s_mtu_period[channel];
  if (duty_count > period) {
    duty_count = period;
  }

  return internal_set_duty_raw_mtu(mtu, output, duty_count);
}

rx_err_t
rx_mtu_get_duty(const rx_mtu_channel_t channel, const rx_mtu_output_t output, float* duty_percent)
{
  volatile rx_mtu_channel_regs_t* mtu;
  const volatile uint16_t*        tgr;
  uint16_t                        period;
  uint16_t                        duty_count;

  RX_VALIDATE_PTR(duty_percent, s_tag, "duty_percent pointer is nullptr");

  mtu = (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == nullptr) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_mtu_initialized[channel], s_tag, "MTU channel not initialized");

  tgr = internal_get_tgr_register(mtu, output);
  if (tgr == nullptr) {
    return k_rx_err_invalid_arg;
  }

  period     = s_mtu_period[channel];
  duty_count = *tgr;

  *duty_percent = (float)(duty_count * (float)k_mtu_duty_max) / period;

  return k_rx_ok;
}

/**
 * @brief Get PWM period count for MTU channel
 *
 * @details
 * Returns the cached period value (TGRA) from initialization. Useful for
 * calculating raw duty counts from percentages without floating-point.
 *
 * @param[in] channel MTU channel to query
 * @param[out] period_count Pointer to store period count value
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, period_count contains valid value
 * @retval k_rx_err_null_ptr period_count is nullptr
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_not_initialized Channel not initialized
 *
 * @see rx_mtu.h Complete API documentation
 *
 * @since Version 1.0.0
 */
rx_err_t rx_mtu_get_period(const rx_mtu_channel_t channel, uint16_t* period_count)
{
  RX_VALIDATE_PTR(period_count, s_tag, "period_count pointer is nullptr");

  if (!internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_mtu_initialized[channel], s_tag, "MTU channel not initialized");

  *period_count = s_mtu_period[channel];
  return k_rx_ok;
}

rx_err_t
rx_mtu_enable_output(const rx_mtu_channel_t channel, rx_mtu_output_t output, const bool enable)
{
  volatile rx_mtu_channel_regs_t* mtu;
  uint8_t                         tior_value;

  mtu = (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == nullptr) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_mtu_initialized[channel], s_tag, "MTU channel not initialized");

  /* Enable/disable output by modifying TIOR registers */
  tior_value = enable ? k_mtu_tior_init_low : k_mtu_tior_disabled;

  switch (output) {
    case k_mtu_output_a:
      mtu->tiorh = (mtu->tiorh & k_mtu_tior_low_mask) | (tior_value << k_mtu_tior_low_shift);
      break;
    case k_mtu_output_b:
      mtu->tiorh = (mtu->tiorh & k_mtu_tior_high_mask) | (tior_value << k_mtu_tior_high_shift);
      break;
    case k_mtu_output_c:
      mtu->tiorl = (mtu->tiorl & k_mtu_tior_low_mask) | (tior_value << k_mtu_tior_low_shift);
      break;
    case k_mtu_output_d:
      mtu->tiorl = (mtu->tiorl & k_mtu_tior_high_mask) | (tior_value << k_mtu_tior_high_shift);
      break;
    default:
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Start MTU counter (begin PWM generation)
 *
 * @details
 * Sets the Count Start (CSTn) bit in the appropriate TSTR register to
 * begin counter operation. Channels 0-4 use TSTRA, channels 6-7 use TSTRB.
 *
 * ## TSTR Bit Mapping
 *
 * | Channel | Register | Bit |
 * |---------|----------|-----|
 * | MTU0 | TSTRA | CST0 |
 * | MTU1 | TSTRA | CST1 |
 * | MTU2 | TSTRA | CST2 |
 * | MTU3 | TSTRA | CST3 |
 * | MTU4 | TSTRA | CST4 |
 * | MTU6 | TSTRB | CST6 |
 * | MTU7 | TSTRB | CST7 |
 *
 * @param[in] channel MTU channel to start
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Timer started successfully
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_not_initialized Channel not initialized
 *
 * @pre Channel initialized via rx_mtu_init_pwm()
 *
 * @post Timer counting, PWM output active
 *
 * @note Timer automatically started during init
 * @note Idempotent: Safe to call when already running
 *
 * @see rx_mtu_stop() Stop the timer
 *
 * @since Version 1.0.0
 */
rx_err_t rx_mtu_start(const rx_mtu_channel_t channel)
{
  if (!internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_mtu_initialized[channel], s_tag, "MTU channel not initialized");

  /* Set corresponding bit in TSTR register */
  switch (channel) {
    case k_mtu_channel_0:
      mtu_tstra()->tstr |= k_mtu_tstr_cst0;
      break;
    case k_mtu_channel_1:
      mtu_tstra()->tstr |= k_mtu_tstr_cst1;
      break;
    case k_mtu_channel_2:
      mtu_tstra()->tstr |= k_mtu_tstr_cst2;
      break;
    case k_mtu_channel_3:
      mtu_tstra()->tstr |= k_mtu_tstr_cst3;
      break;
    case k_mtu_channel_4:
      mtu_tstra()->tstr |= k_mtu_tstr_cst4;
      break;
    case k_mtu_channel_6:
      mtu_tstrb()->tstr |= k_mtu_tstr_cst6;
      break;
    case k_mtu_channel_7:
      mtu_tstrb()->tstr |= k_mtu_tstr_cst7;
      break;
    default:
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Stop MTU counter (halt PWM generation)
 *
 * @details
 * Clears the Count Start (CSTn) bit in the appropriate TSTR register to
 * halt counter operation. Counter value and output state are preserved.
 *
 * @param[in] channel MTU channel to stop
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Timer stopped successfully
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_hw_error Failed to clear TSTR bit (hardware issue)
 *
 * @pre None (can be called on uninitialized channel)
 *
 * @post Timer stopped, counter preserved
 * @post Output pins hold current state
 *
 * @note Does NOT check initialization flag (safe for use during init)
 * @note Idempotent: Safe to call when already stopped
 *
 * @warning Outputs may remain HIGH - disable outputs for safe state
 *
 * @see rx_mtu_start() Resume the timer
 * @see rx_mtu_enable_output() Disable outputs
 *
 * @since Version 1.0.0
 */
rx_err_t rx_mtu_stop(const rx_mtu_channel_t channel)
{
  rx_err_t err;

  if (!internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  /* Clear corresponding bit in TSTR register */
  switch (channel) {
    case k_mtu_channel_0:
      err = internal_clear_tstr_bit(mtu_tstra(), k_mtu_tstr_cst0);
      break;
    case k_mtu_channel_1:
      err = internal_clear_tstr_bit(mtu_tstra(), k_mtu_tstr_cst1);
      break;
    case k_mtu_channel_2:
      err = internal_clear_tstr_bit(mtu_tstra(), k_mtu_tstr_cst2);
      break;
    case k_mtu_channel_3:
      err = internal_clear_tstr_bit(mtu_tstra(), k_mtu_tstr_cst3);
      break;
    case k_mtu_channel_4:
      err = internal_clear_tstr_bit(mtu_tstra(), k_mtu_tstr_cst4);
      break;
    case k_mtu_channel_6:
      err = internal_clear_tstr_bit(mtu_tstrb(), k_mtu_tstr_cst6);
      break;
    case k_mtu_channel_7:
      err = internal_clear_tstr_bit(mtu_tstrb(), k_mtu_tstr_cst7);
      break;
    default:
      return k_rx_err_invalid_arg;
  }

  return err;
}

rx_err_t rx_mtu_deinit(const rx_mtu_channel_t channel)
{
  rx_err_t err;

  if (!internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  /* Stop timer */
  err = rx_mtu_stop(channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Disable all outputs */
  err = rx_mtu_enable_output(channel, k_mtu_output_a, false);
  if (err != k_rx_ok) {
    return err;
  }
  err = rx_mtu_enable_output(channel, k_mtu_output_b, false);
  if (err != k_rx_ok) {
    return err;
  }
  err = rx_mtu_enable_output(channel, k_mtu_output_c, false);
  if (err != k_rx_ok) {
    return err;
  }
  err = rx_mtu_enable_output(channel, k_mtu_output_d, false);
  if (err != k_rx_ok) {
    return err;
  }

  /* Mark as uninitialized */
  s_mtu_initialized[channel] = false;
  s_mtu_period[channel]      = k_mtu_period_zero;

  rx_log_info(s_tag, "MTU channel deinitialized");

  return k_rx_ok;
}
