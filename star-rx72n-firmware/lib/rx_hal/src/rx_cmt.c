#include <stddef.h>
/* src/drivers/rx_cmt.c */

/**
 * @file rx_cmt.c
 * @brief CMT (Compare Match Timer) Driver Implementation
 * @details
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
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx_cmt.h"

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "CMT";

/* =============================================================================
 * Constants
 * =============================================================================
 */

typedef enum {
  k_cmt_max_channels = 4,
  k_cmt_div_8        = 0,
  k_cmt_div_32       = 1,
  k_cmt_div_128      = 2,
  k_cmt_div_512      = 3,
} cmt_constants_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static bool             s_cmt_initialized[k_cmt_max_channels] = {false};
static rx_cmt_callback_t s_cmt_callback[k_cmt_max_channels]   = {NULL};
static void*            s_cmt_user_data[k_cmt_max_channels]   = {NULL};

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
static volatile CMT_Channel_Type* internal_get_cmt_base(rx_cmt_channel_t channel)
{
  switch (channel) {
    case k_cmt_channel_0:
      return &CMT0;
    case k_cmt_channel_1:
      return &CMT1;
    case k_cmt_channel_2:
      return &CMT2;
    case k_cmt_channel_3:
      return &CMT3;
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
 * @return RX_OK on success, error code on failure
 */
static rx_err_t internal_calculate_cmt_params(uint32_t frequency_hz,
                                                 uint8_t* divider,
                                                 uint16_t* cmcor)
{
  const uint32_t pclkb = PCLKB_HZ;

  if (frequency_hz == 0) {
    return RX_ERR_INVALID_ARG;
  }

  /* Try each divider to find one that fits in 16-bit */
  const uint16_t dividers[] = {8, 32, 128, 512};

  for (uint8_t i = 0; i < 4; i++) {
    uint32_t period_calc = (pclkb / dividers[i]) / frequency_hz;

    /* Check if period fits in 16-bit and is reasonable */
    if (period_calc > 0 && period_calc <= 0xFFFF) {
      *divider = i;
      *cmcor   = (uint16_t)period_calc;
      return RX_OK;
    }
  }

  RX_LOG_ERROR(s_tag, "Error occurred");
  return RX_ERR_INVALID_ARG;
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

  if (channel >= k_cmt_max_channels) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  /* CMT0 is reserved for ThreadX system tick */
  if (channel == k_cmt_channel_0) {
    RX_LOG_ERROR(s_tag, "CMT0 is reserved for ThreadX");
    return RX_ERR_CONFLICT;
  }

  if (config->priority > IPR_LEVEL_MAX) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  volatile CMT_Channel_Type* cmt = internal_get_cmt_base(channel);
  if (cmt == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  /* Calculate divider and compare value */
  uint8_t  divider;
  uint16_t cmcor;
  rx_err_t err = internal_calculate_cmt_params(config->frequency_hz, &divider, &cmcor);
  if (err != RX_OK) {
    return err;
  }

  RX_LOG_INFO(s_tag, "CMT initialized");

  /* Enable CMT module (clear module stop bit) */
  SYSTEM.PRCR = 0xA50B; /* Enable writes to MSTPCR */
  SYSTEM.MSTPCRB &= ~(1 << 15); /* CMT0-CMT3 */
  SYSTEM.PRCR = 0xA500; /* Lock MSTPCR */

  /* Stop timer before configuration */
  rx_cmt_stop(channel);

  /* Configure timer control register
   * - Set clock divider
   * - Enable compare match interrupt
   */
  cmt->CMCR = (divider << 0) | /* CKS: Clock Select */
              (1 << 6);        /* CMIE: Compare Match Interrupt Enable */

  /* Set compare match value (period - 1) */
  cmt->CMCOR = cmcor - 1;

  /* Clear counter */
  cmt->CMCNT = 0;

  /* Configure interrupt */
  uint8_t vector = VECT_CMT0_CMI0 + channel;

  /* Set interrupt priority */
  ICU.IPR[vector] = config->priority;

  /* Enable interrupt in ICU */
  uint8_t ier_index = vector / 8;
  uint8_t ier_bit   = vector % 8;
  ICU.IER[ier_index] |= (1 << ier_bit);

  /* Clear interrupt flag */
  ICU.IR[vector] = 0;

  /* Save callback */
  s_cmt_callback[channel]  = config->callback;
  s_cmt_user_data[channel] = config->user_data;
  s_cmt_initialized[channel] = true;

  /* Start timer */
  rx_cmt_start(channel);

  RX_LOG_INFO(s_tag, "Info");

  return RX_OK;
}

rx_err_t rx_cmt_start(rx_cmt_channel_t channel)
{
  if (channel >= k_cmt_max_channels || !s_cmt_initialized[channel]) {
    return RX_ERR_INVALID_STATE;
  }

  /* Set corresponding bit in CMSTR register */
  switch (channel) {
    case k_cmt_channel_0:
      CMT_CTRL.CMSTR0 |= (1 << 0);
      break;
    case k_cmt_channel_1:
      CMT_CTRL.CMSTR0 |= (1 << 1);
      break;
    case k_cmt_channel_2:
      CMT_CTRL.CMSTR1 |= (1 << 0);
      break;
    case k_cmt_channel_3:
      CMT_CTRL.CMSTR1 |= (1 << 1);
      break;
    default:
      return RX_ERR_INVALID_ARG;
  }

  return RX_OK;
}

rx_err_t rx_cmt_stop(rx_cmt_channel_t channel)
{
  if (channel >= k_cmt_max_channels) {
    return RX_ERR_INVALID_ARG;
  }

  /* Clear corresponding bit in CMSTR register */
  switch (channel) {
    case k_cmt_channel_0:
      CMT_CTRL.CMSTR0 &= ~(1 << 0);
      break;
    case k_cmt_channel_1:
      CMT_CTRL.CMSTR0 &= ~(1 << 1);
      break;
    case k_cmt_channel_2:
      CMT_CTRL.CMSTR1 &= ~(1 << 0);
      break;
    case k_cmt_channel_3:
      CMT_CTRL.CMSTR1 &= ~(1 << 1);
      break;
    default:
      return RX_ERR_INVALID_ARG;
  }

  return RX_OK;
}

rx_err_t rx_cmt_get_count(rx_cmt_channel_t channel, uint16_t* count)
{
  RX_CHECK_NULL_PTR(count, s_tag, "count pointer is NULL");

  if (channel >= k_cmt_max_channels || !s_cmt_initialized[channel]) {
    return RX_ERR_INVALID_STATE;
  }

  volatile CMT_Channel_Type* cmt = internal_get_cmt_base(channel);
  if (cmt == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  *count = cmt->CMCNT;
  return RX_OK;
}

rx_err_t rx_cmt_deinit(rx_cmt_channel_t channel)
{
  if (channel >= k_cmt_max_channels) {
    return RX_ERR_INVALID_ARG;
  }

  /* Stop timer */
  rx_cmt_stop(channel);

  /* Disable interrupt */
  uint8_t vector    = VECT_CMT0_CMI0 + channel;
  uint8_t ier_index = vector / 8;
  uint8_t ier_bit   = vector % 8;
  ICU.IER[ier_index] &= ~(1 << ier_bit);

  /* Clear callback */
  s_cmt_callback[channel]  = NULL;
  s_cmt_user_data[channel] = NULL;
  s_cmt_initialized[channel] = false;

  RX_LOG_INFO(s_tag, "CMT deinitialized");

  return RX_OK;
}
