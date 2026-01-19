/* lib/rx_hal/src/rx_cmt.c */

/**
 * @file rx_cmt.c
 * @brief CMT (Compare Match Timer) Driver Implementation
 *
 * Periodic interrupt generation using Compare Match Timer.
 *
 * CMT Operation:
 * - 16-bit up-counter
 * - Counts from 0 to CMCOR (compare match register)
 * - Generates interrupt on match
 * - Auto-reloads to 0 and continues
 *
 * Clock Selection (CMCR.CKS):
 * - 00: PCLKB/8
 * - 01: PCLKB/32
 * - 10: PCLKB/128
 * - 11: PCLKB/512
 *
 * For 250Hz with PCLKB=60MHz:
 * - Option 1: /8 divider = 7.5MHz, period = 30000 (exceeds 16-bit)
 * - Option 2: /32 divider = 1.875MHz, period = 7500
 * - Option 3: /128 divider = 468.75kHz, period = 1875 (RECOMMENDED)
 * - Option 4: /512 divider = 117.1875kHz, period = 468.75
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_cmt.h"

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_log.h"
#include "rx_register_protection.h"

static const char* s_tag = "CMT";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief CMT channel and divider constants */
typedef enum : uint8_t {
  k_cmt_max_channels = 4, /**< CMT0, CMT1, CMT2, CMT3 */
  k_cmt_div_8        = 0, /**< PCLKB/8 divider setting */
  k_cmt_div_32       = 1, /**< PCLKB/32 divider setting */
  k_cmt_div_128      = 2, /**< PCLKB/128 divider setting */
  k_cmt_div_512      = 3, /**< PCLKB/512 divider setting */
  k_cmt_num_dividers = 4, /**< Number of available dividers */
} cmt_constants_t;

/** @brief CMT divider values (actual divisor values) */
typedef enum : uint16_t {
  k_cmt_divider_val_8   = 8,   /**< Divide by 8 */
  k_cmt_divider_val_32  = 32,  /**< Divide by 32 */
  k_cmt_divider_val_128 = 128, /**< Divide by 128 */
  k_cmt_divider_val_512 = 512, /**< Divide by 512 */
} cmt_divider_values_t;

/** @brief CMT module stop bit positions in MSTPCRB */
typedef enum : uint8_t {
  k_cmt_mstpb_cmt = 15, /**< CMT0-CMT3 module stop bit */
} cmt_module_stop_bits_t;

/** @brief CMCR register bit positions */
typedef enum : uint8_t {
  k_cmt_cmcr_cks_shift = 0, /**< CKS (clock select) bit shift */
  k_cmt_cmcr_cmie_pos  = 6, /**< CMIE (interrupt enable) bit position */
} cmt_cmcr_bits_t;

/** @brief Period calculation constants */
typedef enum : uint16_t {
  k_cmt_period_min = 0,      /**< Minimum valid period */
  k_cmt_period_max = 0xFFFF, /**< Maximum valid period (16-bit) */
  k_cmt_period_adj = 1,      /**< Period adjustment (period - 1) */
} cmt_period_constants_t;

/** @brief CMSTR register bit positions */
typedef enum : uint8_t {
  k_cmt_cmstr_str0 = 0, /**< CMT0/CMT2 start bit */
  k_cmt_cmstr_str1 = 1, /**< CMT1/CMT3 start bit */
} cmt_cmstr_bits_t;

/** @brief IER register calculation constants */

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static bool              s_cmt_initialized[k_cmt_max_channels] = {false};
static rx_cmt_callback_t s_cmt_callback[k_cmt_max_channels]    = {NULL};
static void*             s_cmt_user_data[k_cmt_max_channels]   = {NULL};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get CMT channel base address
 *
 * @param[in] channel CMT channel
 *
 * @return Pointer to CMT register base, or NULL if invalid
 */
static volatile rx_cmt_channel_regs_t* internal_get_cmt_base(rx_cmt_channel_t channel)
{
  switch (channel) {
    case k_cmt_channel_0:
      return cmt0();
    case k_cmt_channel_1:
      return cmt1();
    case k_cmt_channel_2:
      return cmt2();
    case k_cmt_channel_3:
      return cmt3();
    default:
      return NULL;
  }
}

/**
 * @brief Calculate CMT divider and compare value from frequency
 *
 * @param[in] frequency_hz Desired frequency in Hz
 * @param[out] divider Pointer to store divider setting
 * @param[out] cmcor Pointer to store compare match value
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t
internal_calculate_cmt_params(uint32_t frequency_hz, uint8_t* divider, uint16_t* cmcor)
{
  const uint32_t pclkb = k_pclkb_hz;

  if (frequency_hz == 0) {
    return k_rx_err_invalid_arg;
  }

  /* Try each divider to find one that fits in 16-bit */
  static const uint16_t dividers[] = {k_cmt_divider_val_8,
                                      k_cmt_divider_val_32,
                                      k_cmt_divider_val_128,
                                      k_cmt_divider_val_512};

  for (uint8_t i = 0; i < k_cmt_num_dividers; i++) {
    uint32_t period_calc = (pclkb / dividers[i]) / frequency_hz;

    /* Check if period fits in 16-bit and is reasonable */
    if (period_calc > k_cmt_period_min && period_calc <= k_cmt_period_max) {
      *divider = i;
      *cmcor   = (uint16_t)period_calc;
      return k_rx_ok;
    }
  }

  rx_log_error(s_tag, "Error occurred");
  return k_rx_err_invalid_arg;
}

/* =============================================================================
 * Interrupt Handlers
 * =============================================================================
 */

/**
 * @brief CMT1 interrupt handler
 */
void cmt1_isr(void)
{
  /* Call user callback if registered */
  if (s_cmt_callback[k_cmt_channel_1] != NULL) {
    s_cmt_callback[k_cmt_channel_1](s_cmt_user_data[k_cmt_channel_1]);
  }

  /* Clear interrupt flag (read-modify-write not needed, auto-cleared) */
}

/**
 * @brief CMT2 interrupt handler
 */
void cmt2_isr(void)
{
  if (s_cmt_callback[k_cmt_channel_2] != NULL) {
    s_cmt_callback[k_cmt_channel_2](s_cmt_user_data[k_cmt_channel_2]);
  }
}

/**
 * @brief CMT3 interrupt handler
 */
void cmt3_isr(void)
{
  if (s_cmt_callback[k_cmt_channel_3] != NULL) {
    s_cmt_callback[k_cmt_channel_3](s_cmt_user_data[k_cmt_channel_3]);
  }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_cmt_init(rx_cmt_channel_t channel, const rx_cmt_config_t* config)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  if ((int32_t)channel >= k_cmt_max_channels) {
    rx_log_error(s_tag, "Error occurred");
    return k_rx_err_invalid_arg;
  }

  /* CMT0 is reserved for ThreadX system tick */
  if (channel == k_cmt_channel_0) {
    rx_log_error(s_tag, "CMT0 is reserved for ThreadX");
    return k_rx_err_conflict;
  }

  if (config->priority > k_ipr_level_max) {
    rx_log_error(s_tag, "Error occurred");
    return k_rx_err_invalid_arg;
  }

  volatile rx_cmt_channel_regs_t* cmt = internal_get_cmt_base(channel);
  if (cmt == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Calculate divider and compare value */
  uint8_t        divider;
  uint16_t       cmcor;
  const rx_err_t err = internal_calculate_cmt_params(config->frequency_hz, &divider, &cmcor);
  if (err != k_rx_ok) {
    return err;
  }

  rx_log_info(s_tag, "CMT initialized");

  /* Enable CMT module (clear module stop bit) */
  system_regs()->prcr = k_rx_prcr_unlock_prc1_prc3;
  system_regs()->mstpcrb &= ~(1UL << k_cmt_mstpb_cmt);
  system_regs()->prcr = k_rx_prcr_lock;

  /* Stop timer before configuration */
  rx_cmt_stop(channel);

  /* Configure timer control register
   * - Set clock divider
   * - Enable compare match interrupt
   */
  cmt->cmcr = (divider << k_cmt_cmcr_cks_shift) | /* CKS: Clock Select */
              (1 << k_cmt_cmcr_cmie_pos);         /* CMIE: Compare Match Interrupt Enable */

  /* Set compare match value (period - 1) */
  cmt->cmcor = cmcor - k_cmt_period_adj;

  /* Clear counter */
  cmt->cmcnt = 0;

  /* Configure interrupt */
  const uint8_t vector = k_vect_cmt0_cmi0 + channel;

  /* Set interrupt priority */
  icu()->ipr[vector] = config->priority;

  /* Enable interrupt in ICU */
  uint8_t       ier_index = vector / k_icu_ier_bits_per_reg;
  const uint8_t ier_bit   = vector % k_icu_ier_bits_per_reg;
  icu()->ier[ier_index] |= (1 << ier_bit);

  /* Clear interrupt flag */
  icu()->ir[vector] = 0;

  /* Save callback */
  s_cmt_callback[channel]    = config->callback;
  s_cmt_user_data[channel]   = config->user_data;
  s_cmt_initialized[channel] = true;

  /* Start timer */
  rx_cmt_start(channel);

  rx_log_info(s_tag, "Info");

  return k_rx_ok;
}

rx_err_t rx_cmt_start(rx_cmt_channel_t channel)
{
  if ((int32_t)channel >= k_cmt_max_channels || !s_cmt_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  /* Set corresponding bit in CMSTR register */
  switch (channel) {
    case k_cmt_channel_0:
      cmt_ctrl()->cmstr0 |= (1 << k_cmt_cmstr_str0);
      break;
    case k_cmt_channel_1:
      cmt_ctrl()->cmstr0 |= (1 << k_cmt_cmstr_str1);
      break;
    case k_cmt_channel_2:
      cmt_ctrl()->cmstr1 |= (1 << k_cmt_cmstr_str0);
      break;
    case k_cmt_channel_3:
      cmt_ctrl()->cmstr1 |= (1 << k_cmt_cmstr_str1);
      break;
    default:
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

rx_err_t rx_cmt_stop(rx_cmt_channel_t channel)
{
  if ((int32_t)channel >= k_cmt_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Clear corresponding bit in CMSTR register */
  switch (channel) {
    case k_cmt_channel_0:
      cmt_ctrl()->cmstr0 &= ~(1 << k_cmt_cmstr_str0);
      break;
    case k_cmt_channel_1:
      cmt_ctrl()->cmstr0 &= ~(1 << k_cmt_cmstr_str1);
      break;
    case k_cmt_channel_2:
      cmt_ctrl()->cmstr1 &= ~(1 << k_cmt_cmstr_str0);
      break;
    case k_cmt_channel_3:
      cmt_ctrl()->cmstr1 &= ~(1 << k_cmt_cmstr_str1);
      break;
    default:
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

rx_err_t rx_cmt_get_count(rx_cmt_channel_t channel, uint16_t* count)
{
  RX_CHECK_NULL_PTR(count, s_tag, "count pointer is NULL");

  if ((int32_t)channel >= k_cmt_max_channels || !s_cmt_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_cmt_channel_regs_t* cmt = internal_get_cmt_base(channel);
  if (cmt == NULL) {
    return k_rx_err_invalid_arg;
  }

  *count = cmt->cmcnt;
  return k_rx_ok;
}

rx_err_t rx_cmt_deinit(rx_cmt_channel_t channel)
{
  if ((int32_t)channel >= k_cmt_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Stop timer */
  rx_cmt_stop(channel);

  /* Disable interrupt */
  const uint8_t vector    = k_vect_cmt0_cmi0 + channel;
  uint8_t       ier_index = vector / k_icu_ier_bits_per_reg;
  const uint8_t ier_bit   = vector % k_icu_ier_bits_per_reg;
  icu()->ier[ier_index] &= ~(1 << ier_bit);

  /* Clear callback */
  s_cmt_callback[channel]    = NULL;
  s_cmt_user_data[channel]   = NULL;
  s_cmt_initialized[channel] = false;

  rx_log_info(s_tag, "CMT deinitialized");

  return k_rx_ok;
}
