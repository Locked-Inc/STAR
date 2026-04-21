/**
 * @file rx_tmr.c
 * @brief TMR HAL Driver Implementation (8-bit + 16-bit cascade)
 *
 * @details
 * Implementation of the TMR hardware abstraction layer for the RX72N
 * 8-bit Timer peripheral. Provides per-channel init/start/stop, counter
 * readout, microsecond period programming, and SELECTB ISR-callback
 * registration.
 *
 * ## Initialisation Sequence
 *
 * @verbatim
 *   rx_tmr_init(&config)
 *     |
 *     +-- 1. Validate config pointer (NASA Rule 5)
 *     +-- 2. Validate channel / clock / clear / outputs
 *     +-- 3. In cascade mode, require even channel
 *     +-- 4. Check not already initialised
 *     +-- 5. Enable unit module clock (MSTPA5 or MSTPA4, PRCR-gated)
 *     +-- 6. Stop counter (TCCR = 0)
 *     +-- 7. Program TCR  (interrupt enables + CCLR)
 *     +-- 8. Program TCSR (output select A/B)
 *     +-- 9. Preload TCCR clock bits into internal state (start writes them)
 *     +-- 10. Clear TCNT to 0
 *     +-- 11. Mark channel initialised
 *     +-- 12. In cascade mode, also configure paired odd channel
 * @endverbatim
 *
 * ## Channel Register Access
 *
 * Because even and odd TMR channels share a 16-byte block at different
 * parity byte offsets, the driver uses rx72n_tmr_regs.h inline accessors
 * (tmr0()..tmr3()) that each return a volatile pointer landing on the
 * correct byte. Each struct field then accesses only the target channel's
 * own byte.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto / recursion / setjmp
 * - Rule 2: All loops bounded
 * - Rule 3: No dynamic memory
 * - Rule 4: Functions kept short (decomposed helpers)
 * - Rule 5: >= 2 validation checks per public function
 * - Rule 6: Variables at smallest scope
 * - Rule 7: All return values checked / explicitly cast (void)
 * - Rule 8: C23 typed enums for constants
 * - Rule 9: Function pointer permitted for ISR DIP
 * - Rule 10: -Wall -Wextra -Werror clean
 *
 * @see rx_tmr.h Public API
 * @see rx72n_tmr_regs.h Register structures and bit definitions
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_tmr.h"

#include <stddef.h>
#include <stdint.h>

#ifdef UNIT_TEST
#include "mock_rx72n_regs.h"
#else
#include "rx72n_regs.h"
#endif
#include "rx_check.h"
#include "rx_log.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Module-private Constants
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Logging tag for the TMR module
 * @details 4 bytes in .rodata.
 * @since Version 1.0.0
 */
static const char* s_tag = "TMR";

/**
 * @enum tmr_pclk_hz_t
 * @brief Peripheral clock rate for internal-clock period calculations
 *
 * @details
 * TMR counts on PCLKB, which is configured by the system clock init to
 * 60 MHz for the RX72N STAR platform. Period calculations use this
 * constant rather than a runtime clock query to keep the helper pure.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_tmr_pclkb_hz = 60000000U, /**< 60 MHz PCLKB on RX72N STAR */
} tmr_pclk_hz_t;

/**
 * @enum tmr_period_limits_t
 * @brief Compile-time period / divisor limits
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_tmr_us_per_sec      = 1000000U, /**< Microseconds per second */
  k_tmr_max_tcora       = 0xFFU,    /**< 8-bit TCORA maximum value */
  k_tmr_max_tcora_16bit = 0xFFFFU,  /**< 16-bit cascade TCORA maximum value */
} tmr_period_limits_t;

/**
 * @enum tmr_channel_count_t
 * @brief Array sizing constant for per-channel state
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_channel_count_internal = 4, /**< TMR0..TMR3 */
} tmr_channel_count_t;

/* =============================================================================
 * Module-private State
 * =============================================================================
 */

/**
 * @var s_tmr_initialized
 * @brief Per-channel init flags indexed by rx_tmr_channel_t
 * @warning Do not modify directly - use rx_tmr_init() / rx_tmr_deinit()
 * @since Version 1.0.0
 */
static bool s_tmr_initialized[k_tmr_channel_count_internal] = {false, false, false, false};

/**
 * @var s_tmr_mode
 * @brief Per-channel operating mode (independent vs cascade)
 * @since Version 1.0.0
 */
static rx_tmr_mode_t s_tmr_mode[k_tmr_channel_count_internal] = {
  k_tmr_mode_8bit_independent,
  k_tmr_mode_8bit_independent,
  k_tmr_mode_8bit_independent,
  k_tmr_mode_8bit_independent,
};

/**
 * @var s_tmr_clock_bits
 * @brief Cached TCCR clock bits (CSS|CKS) applied at rx_tmr_start()
 *
 * @details
 * rx_tmr_init() computes the TCCR clock-field value but does NOT write it
 * immediately because that would start the counter. rx_tmr_start() OR's
 * these bits into TCCR when requested. rx_tmr_stop() clears the clock
 * field so the counter halts.
 *
 * @since Version 1.0.0
 */
static uint8_t s_tmr_clock_bits[k_tmr_channel_count_internal] = {0U, 0U, 0U, 0U};

/**
 * @var s_tmr_clock_source
 * @brief Cached clock-source enum per channel (needed by set_period_us)
 * @since Version 1.0.0
 */
static rx_tmr_clock_source_t s_tmr_clock_source[k_tmr_channel_count_internal] = {
  k_tmr_clock_pclk_div_1,
  k_tmr_clock_pclk_div_1,
  k_tmr_clock_pclk_div_1,
  k_tmr_clock_pclk_div_1,
};

/**
 * @var s_tmr_callback
 * @brief Per-channel compare-match / overflow callback pointers
 * @since Version 1.0.0
 */
static rx_tmr_isr_callback_t s_tmr_callback[k_tmr_channel_count_internal] = {
  NULL,
  NULL,
  NULL,
  NULL,
};

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Get register pointer for a TMR channel
 * @param[in] channel Validated channel identifier (0..3)
 * @return Volatile pointer to the channel's register struct; NULL if invalid.
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE volatile rx_tmr_channel_regs_t* internal_tmr_get_regs(rx_tmr_channel_t channel)
{
  /* GCOVR_EXCL_BR_START(validate_config restricts channel to 0..3) */
  switch (channel) {
    case k_tmr_channel_0:
      return tmr0();
    case k_tmr_channel_1:
      return tmr1();
    case k_tmr_channel_2:
      return tmr2();
    case k_tmr_channel_3:
      return tmr3();
    default:                                        /* GCOVR_EXCL_LINE */
      RX_ASSERT(false, "TMR channel out of range"); /* GCOVR_EXCL_LINE */
      return NULL;                                  /* GCOVR_EXCL_LINE */
  }
  /* GCOVR_EXCL_BR_STOP */
}

/**
 * @brief Return the MSTPCRA bit mask for a TMR channel's unit
 * @param[in] channel Validated channel identifier
 * @return MSTPA5 for unit 0 (TMR0/1), MSTPA4 for unit 1 (TMR2/3), else 0.
 * @since Version 1.0.0
 */
static uint32_t internal_tmr_unit_mstp_bit(rx_tmr_channel_t channel)
{
  /* Public callers pre-validate channel in 0..3 via internal_tmr_validate_config, so
   * anything not in unit 0 (TMR0/1) must belong to unit 1 (TMR2/3). */
  if ((channel == k_tmr_channel_0) || (channel == k_tmr_channel_1)) {
    return k_tmr_mstpcra_mstpa5;
  }
  return (uint32_t)k_tmr_mstpcra_mstpa4;
}

/**
 * @brief Enable the module clock for the channel's TMR unit
 * @param[in] channel Validated channel identifier
 * @pre PRCR register accessible
 * @post MSTPCRA bit cleared for the unit
 * @since Version 1.0.0
 */
static void internal_tmr_enable_module_clock(rx_tmr_channel_t channel)
{
  const uint32_t mstp_bit = internal_tmr_unit_mstp_bit(channel);

  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcra &= ~mstp_bit;
  *prcr_reg() = k_rx_prcr_lock;
}

/**
 * @brief Map a clock source to TCCR CSS|CKS bits
 * @param[in]  source Validated clock-source enumeration value
 * @param[out] bits   Receives the composed TCCR[4:0] bits
 * @return k_rx_ok on success, k_rx_err_invalid_arg on unknown value.
 * @since Version 1.0.0
 */
static rx_err_t internal_tmr_clock_bits(rx_tmr_clock_source_t source, uint8_t* bits)
{
  /* GCOVR_EXCL_BR_START(validate_config blocks out-of-range clock_source) */
  switch (source) {
    case k_tmr_clock_pclk_div_1:
      *bits = (uint8_t)(k_tmr_tccr_css_internal | k_tmr_tccr_cks_pclk_div1);
      return k_rx_ok;
    case k_tmr_clock_pclk_div_2:
      *bits = (uint8_t)(k_tmr_tccr_css_internal | k_tmr_tccr_cks_pclk_div2);
      return k_rx_ok;
    case k_tmr_clock_pclk_div_8:
      *bits = (uint8_t)(k_tmr_tccr_css_internal | k_tmr_tccr_cks_pclk_div8);
      return k_rx_ok;
    case k_tmr_clock_pclk_div_32:
      *bits = (uint8_t)(k_tmr_tccr_css_internal | k_tmr_tccr_cks_pclk_div32);
      return k_rx_ok;
    case k_tmr_clock_pclk_div_64:
      *bits = (uint8_t)(k_tmr_tccr_css_internal | k_tmr_tccr_cks_pclk_div64);
      return k_rx_ok;
    case k_tmr_clock_pclk_div_1024:
      *bits = (uint8_t)(k_tmr_tccr_css_internal | k_tmr_tccr_cks_pclk_div1024);
      return k_rx_ok;
    case k_tmr_clock_pclk_div_8192:
      *bits = (uint8_t)(k_tmr_tccr_css_internal | k_tmr_tccr_cks_pclk_div8192);
      return k_rx_ok;
    case k_tmr_clock_external_rising:
      *bits = (uint8_t)(k_tmr_tccr_css_external | k_tmr_tccr_cks_ext_rising);
      return k_rx_ok;
    case k_tmr_clock_external_falling:
      *bits = (uint8_t)(k_tmr_tccr_css_external | k_tmr_tccr_cks_ext_falling);
      return k_rx_ok;
    case k_tmr_clock_external_both:
      *bits = (uint8_t)(k_tmr_tccr_css_external | k_tmr_tccr_cks_ext_both);
      return k_rx_ok;
    default: /* GCOVR_EXCL_LINE -- validate_config blocks out-of-range clock_source */
      return k_rx_err_invalid_arg; /* GCOVR_EXCL_LINE */
  }
  /* GCOVR_EXCL_BR_STOP */
}

/**
 * @brief Return the integer divider for an internal clock source
 * @param[in] source Internal clock-source enumeration value
 * @return PCLK divider (1, 2, 8, 32, 64, 1024, 8192) or 0 for external.
 * @since Version 1.0.0
 */
static uint32_t internal_tmr_clock_divider(rx_tmr_clock_source_t source)
{
  switch (source) {
    case k_tmr_clock_pclk_div_1:
      return 1U;
    case k_tmr_clock_pclk_div_2:
      return 2U;
    case k_tmr_clock_pclk_div_8:
      return 8U;
    case k_tmr_clock_pclk_div_32:
      return 32U;
    case k_tmr_clock_pclk_div_64:
      return 64U;
    case k_tmr_clock_pclk_div_1024:
      return 1024U;
    case k_tmr_clock_pclk_div_8192:
      return 8192U;
    default:
      return 0U; /* External or invalid */
  }
}

/**
 * @brief Compose TCR value from config
 * @param[in] config Validated configuration
 * @return TCR value with CCLR field + interrupt enables set per config.
 * @since Version 1.0.0
 */
static uint8_t internal_tmr_compose_tcr(const rx_tmr_config_t* config)
{
  uint8_t tcr = 0U;

  if ((config->irq_mask & k_tmr_irq_cmp_match_a) != 0U) {
    tcr |= k_tmr_tcr_cmiea;
  }
  if ((config->irq_mask & k_tmr_irq_cmp_match_b) != 0U) {
    tcr |= k_tmr_tcr_cmieb;
  }
  if ((config->irq_mask & k_tmr_irq_overflow) != 0U) {
    tcr |= k_tmr_tcr_ovie;
  }

  /* GCOVR_EXCL_BR_START(validate_config blocks out-of-range counter_clear) */
  switch (config->counter_clear) {
    case k_tmr_clear_disabled:
      tcr |= k_tmr_tcr_cclr_disabled;
      break;
    case k_tmr_clear_cmp_match_a:
      tcr |= k_tmr_tcr_cclr_cmp_match_a;
      break;
    case k_tmr_clear_cmp_match_b:
      tcr |= k_tmr_tcr_cclr_cmp_match_b;
      break;
    case k_tmr_clear_external_reset:
      tcr |= k_tmr_tcr_cclr_external_sig;
      break;
    default: /* GCOVR_EXCL_LINE -- validate_config blocks out-of-range counter_clear */
      break; /* GCOVR_EXCL_LINE */
  }
  /* GCOVR_EXCL_BR_STOP */
  return tcr;
}

/**
 * @brief Compose TCSR value from config (OSA/OSB fields)
 * @param[in] config Validated configuration
 * @return TCSR value with OSA/OSB populated; ADTE left at 0.
 * @since Version 1.0.0
 */
static uint8_t internal_tmr_compose_tcsr(const rx_tmr_config_t* config)
{
  const uint8_t osa = (uint8_t)(((uint8_t)config->output_a) << k_tmr_tcsr_osa_shift);
  const uint8_t osb = (uint8_t)(((uint8_t)config->output_b) << k_tmr_tcsr_osb_shift);
  return (uint8_t)(osa | osb);
}

/**
 * @brief Validate enum fields of a configuration
 * @param[in] config Non-null configuration
 * @return k_rx_ok if all enum fields are in range.
 * @since Version 1.0.0
 */
static rx_err_t internal_tmr_validate_config(const rx_tmr_config_t* config)
{
  if ((uint8_t)config->channel >= k_tmr_channel_count_internal) {
    return k_rx_err_invalid_arg;
  }
  if ((config->mode != k_tmr_mode_8bit_independent) && (config->mode != k_tmr_mode_16bit_cascade)) {
    return k_rx_err_invalid_arg;
  }
  if ((uint8_t)config->counter_clear > k_tmr_clear_external_reset) {
    return k_rx_err_invalid_arg;
  }
  if ((uint8_t)config->output_a > k_tmr_output_toggle) {
    return k_rx_err_invalid_arg;
  }
  if ((uint8_t)config->output_b > k_tmr_output_toggle) {
    return k_rx_err_invalid_arg;
  }
  if ((uint8_t)config->clock_source > k_tmr_clock_external_both) {
    return k_rx_err_invalid_arg;
  }
  /* Cascade mode requires an even channel */
  if (config->mode == k_tmr_mode_16bit_cascade) {
    if ((config->channel != k_tmr_channel_0) && (config->channel != k_tmr_channel_2)) {
      return k_rx_err_invalid_arg;
    }
  }
  return k_rx_ok;
}

/**
 * @brief Write register fields for a single channel during init
 * @param[in] channel Channel to program (even or odd)
 * @param[in] config  Source configuration
 * @since Version 1.0.0
 */
static void internal_tmr_program_channel(rx_tmr_channel_t channel, const rx_tmr_config_t* config)
{
  volatile rx_tmr_channel_regs_t* regs = internal_tmr_get_regs(channel);

  regs->tccr  = 0U; /* Stop counter before programming */
  regs->tcr   = internal_tmr_compose_tcr(config);
  regs->tcsr  = internal_tmr_compose_tcsr(config);
  regs->tcora = k_tmr_max_tcora;
  regs->tcorb = k_tmr_max_tcora;
  regs->tcnt  = 0U;
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

rx_err_t rx_tmr_init(const rx_tmr_config_t* config)
{
  /* Pre-condition 1: non-null config */
  RX_CHECK_NULL_PTR(config, s_tag, "Config pointer is NULL");

  /* Pre-condition 2: enum ranges */
  rx_err_t err = internal_tmr_validate_config(config);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Invalid TMR config");
    return err;
  }

  const uint8_t idx = (uint8_t)config->channel;

  /* Pre-condition 3: not already initialised */
  if (s_tmr_initialized[idx]) {
    rx_log_error(s_tag, "Channel already initialised");
    return k_rx_err_invalid_state;
  }

  /* Compose clock bits (validated via enum range above) */
  uint8_t clock_bits = 0U;
  err                = internal_tmr_clock_bits(config->clock_source, &clock_bits);
  if (err != k_rx_ok) { /* GCOVR_EXCL_LINE -- validate_config blocks out-of-range clock_source */
    return err;         /* GCOVR_EXCL_LINE */
  } /* GCOVR_EXCL_BR_LINE */

  /* 1. Enable unit module clock */
  internal_tmr_enable_module_clock(config->channel);

  /* 2. Program the primary channel */
  internal_tmr_program_channel(config->channel, config);

  /* 3. For cascade mode, program the paired odd channel */
  if (config->mode == k_tmr_mode_16bit_cascade) {
    const rx_tmr_channel_t odd =
      (config->channel == k_tmr_channel_0) ? k_tmr_channel_1 : k_tmr_channel_3;

    /* Even channel counts on its own selected clock source; odd channel
     * counts on the even channel's TCORA compare match via CSS = cascade. */
    rx_tmr_config_t odd_cfg = *config;
    odd_cfg.channel         = odd;
    internal_tmr_program_channel(odd, &odd_cfg);

    volatile rx_tmr_channel_regs_t* odd_regs = internal_tmr_get_regs(odd);
    odd_regs->tccr                           = k_tmr_tccr_css_cascade;

    s_tmr_mode[(uint8_t)odd]         = k_tmr_mode_16bit_cascade;
    s_tmr_initialized[(uint8_t)odd]  = true;
    s_tmr_clock_bits[(uint8_t)odd]   = k_tmr_tccr_css_cascade;
    s_tmr_clock_source[(uint8_t)odd] = config->clock_source;
  }

  /* 4. Cache state for the primary channel */
  s_tmr_mode[idx]         = config->mode;
  s_tmr_clock_bits[idx]   = clock_bits;
  s_tmr_clock_source[idx] = config->clock_source;
  s_tmr_initialized[idx]  = true;

  rx_log_info_val(s_tag, "TMR channel initialised", (uint32_t)config->channel);
  return k_rx_ok;
}

rx_err_t rx_tmr_start(rx_tmr_channel_t channel)
{
  if ((uint8_t)channel >= k_tmr_channel_count_internal) {
    return k_rx_err_invalid_arg;
  }
  const uint8_t idx = (uint8_t)channel;
  RX_VALIDATE_INIT(s_tmr_initialized[idx], s_tag, "Channel not initialised");

  volatile rx_tmr_channel_regs_t* regs = internal_tmr_get_regs(channel);
  regs->tccr                           = s_tmr_clock_bits[idx];

  /* In cascade mode the paired odd channel also needs its cascade bits
   * kept after a stop/start cycle. Cascade is only valid on even channels
   * (enforced by validate_config), so the ternary below is exhaustive. */
  if (s_tmr_mode[idx] == k_tmr_mode_16bit_cascade) {
    volatile rx_tmr_channel_regs_t* odd_regs = (channel == k_tmr_channel_0) ? tmr1() : tmr3();
    odd_regs->tccr                           = (uint8_t)k_tmr_tccr_css_cascade;
  }
  return k_rx_ok;
}

rx_err_t rx_tmr_stop(rx_tmr_channel_t channel)
{
  if ((uint8_t)channel >= k_tmr_channel_count_internal) {
    return k_rx_err_invalid_arg;
  }
  const uint8_t idx = (uint8_t)channel;
  RX_VALIDATE_INIT(s_tmr_initialized[idx], s_tag, "Channel not initialised");

  volatile rx_tmr_channel_regs_t* regs = internal_tmr_get_regs(channel);
  regs->tccr                           = 0U;

  if (s_tmr_mode[idx] == k_tmr_mode_16bit_cascade) {
    volatile rx_tmr_channel_regs_t* odd_regs = (channel == k_tmr_channel_0) ? tmr1() : tmr3();
    odd_regs->tccr                           = 0U;
  }
  return k_rx_ok;
}

rx_err_t rx_tmr_read(rx_tmr_channel_t channel, uint16_t* value)
{
  RX_CHECK_NULL_PTR(value, s_tag, "Value pointer is NULL");
  if ((uint8_t)channel >= k_tmr_channel_count_internal) {
    return k_rx_err_invalid_arg;
  }
  const uint8_t idx = (uint8_t)channel;
  RX_VALIDATE_INIT(s_tmr_initialized[idx], s_tag, "Channel not initialised");

  if (s_tmr_mode[idx] == k_tmr_mode_16bit_cascade) {
    const uint8_t          upper = internal_tmr_get_regs(channel)->tcnt;
    const rx_tmr_channel_t odd   = (channel == k_tmr_channel_0) ? k_tmr_channel_1 : k_tmr_channel_3;
    const uint8_t          lower = internal_tmr_get_regs(odd)->tcnt;
    *value                       = (uint16_t)(((uint16_t)upper << 8U) | (uint16_t)lower);
  } else {
    *value = (uint16_t)internal_tmr_get_regs(channel)->tcnt;
  }
  return k_rx_ok;
}

rx_err_t rx_tmr_set_period_us(rx_tmr_channel_t channel, uint32_t period_us)
{
  if ((uint8_t)channel >= k_tmr_channel_count_internal) {
    return k_rx_err_invalid_arg;
  }
  const uint8_t idx = (uint8_t)channel;
  RX_VALIDATE_INIT(s_tmr_initialized[idx], s_tag, "Channel not initialised");

  if (period_us == 0U) {
    return k_rx_err_invalid_arg;
  }

  const uint32_t divider = internal_tmr_clock_divider(s_tmr_clock_source[idx]);
  if (divider == 0U) {
    /* External clock selected - cannot compute period */
    return k_rx_err_invalid_arg;
  }

  /* tick_hz = PCLKB / divider; ticks = period_us * tick_hz / 1e6
   * compare = ticks - 1 (TCORA fires when TCNT reaches compare value) */
  const uint64_t ticks = ((uint64_t)period_us * (uint64_t)k_tmr_pclkb_hz) /
                         ((uint64_t)divider * (uint64_t)k_tmr_us_per_sec);
  if (ticks == 0U) {
    return k_rx_err_invalid_arg;
  }
  const uint64_t compare = ticks - 1U;

  const uint32_t max_compare =
    (s_tmr_mode[idx] == k_tmr_mode_16bit_cascade) ? k_tmr_max_tcora_16bit : k_tmr_max_tcora;
  if (compare > (uint64_t)max_compare) {
    rx_log_error_val(s_tag, "Period too large for divider", period_us);
    return k_rx_err_invalid_arg;
  }

  if (s_tmr_mode[idx] == k_tmr_mode_16bit_cascade) {
    const rx_tmr_channel_t odd = (channel == k_tmr_channel_0) ? k_tmr_channel_1 : k_tmr_channel_3;
    internal_tmr_get_regs(channel)->tcora = (uint8_t)((compare >> 8U) & 0xFFU);
    internal_tmr_get_regs(odd)->tcora     = (uint8_t)(compare & 0xFFU);
  } else {
    internal_tmr_get_regs(channel)->tcora = (uint8_t)(compare & 0xFFU);
  }
  return k_rx_ok;
}

rx_err_t rx_tmr_register_compare_match_isr(rx_tmr_channel_t channel, rx_tmr_isr_callback_t callback)
{
  if ((uint8_t)channel >= k_tmr_channel_count_internal) {
    return k_rx_err_invalid_arg;
  }
  s_tmr_callback[(uint8_t)channel] = callback;
  return k_rx_ok;
}

rx_err_t rx_tmr_deinit(rx_tmr_channel_t channel)
{
  if ((uint8_t)channel >= k_tmr_channel_count_internal) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_tmr_channel_regs_t* regs = internal_tmr_get_regs(channel);
  regs->tccr                           = 0U;
  regs->tcr                            = 0U;
  regs->tcsr                           = 0U;
  regs->tcora                          = 0U;
  regs->tcorb                          = 0U;
  regs->tcnt                           = 0U;

  const uint8_t idx = (uint8_t)channel;
  if (s_tmr_mode[idx] == k_tmr_mode_16bit_cascade) {
    const rx_tmr_channel_t odd = (channel == k_tmr_channel_0) ? k_tmr_channel_1 : k_tmr_channel_3;
    volatile rx_tmr_channel_regs_t* odd_regs = internal_tmr_get_regs(odd);
    odd_regs->tccr                           = 0U;
    odd_regs->tcr                            = 0U;
    odd_regs->tcsr                           = 0U;
    odd_regs->tcora                          = 0U;
    odd_regs->tcorb                          = 0U;
    odd_regs->tcnt                           = 0U;
    s_tmr_initialized[(uint8_t)odd]          = false;
    s_tmr_mode[(uint8_t)odd]                 = k_tmr_mode_8bit_independent;
    s_tmr_clock_bits[(uint8_t)odd]           = 0U;
    s_tmr_callback[(uint8_t)odd]             = NULL;
  }

  s_tmr_initialized[idx] = false;
  s_tmr_mode[idx]        = k_tmr_mode_8bit_independent;
  s_tmr_clock_bits[idx]  = 0U;
  s_tmr_callback[idx]    = NULL;
  return k_rx_ok;
}

/* =============================================================================
 * ISR Dispatch Hooks (SELECTB)
 * =============================================================================
 */

/**
 * @brief Internal dispatcher invoked by the SELECTB vector shim
 *
 * @details
 * Called from the low-level vector when a TMR CMIA/CMIB/OVI interrupt
 * fires. Clears the source flag in the TMR (compare-match sets no
 * persistent status bit, so the hardware does it automatically; this
 * function exists so the test harness can synthesise ISR invocations)
 * and invokes the registered user callback.
 *
 * @param[in] channel TMR channel whose interrupt fired (0..3)
 * @since Version 1.0.0
 */
void rx_tmr_isr_dispatch(rx_tmr_channel_t channel)
{
  if ((uint8_t)channel >= k_tmr_channel_count_internal) {
    return;
  }
  rx_tmr_isr_callback_t cb = s_tmr_callback[(uint8_t)channel];
  if (cb != NULL) {
    cb(channel);
  }
}
