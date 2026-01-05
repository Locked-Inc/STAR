/* lib/rx_hal/src/rx_mtu3a.c */

/**
 * @file rx_mtu3a.c
 * @brief MTU3a PWM Driver Implementation for Motor Control
 *
 * Multi-Function Timer Unit PWM driver for brushed DC motors.
 *
 * PWM Mode 1 (Triangle Wave - Center-Aligned):
 * - Counter counts up to TGRA, then down to 0
 * - TGRB/TGRD control duty cycle
 * - Provides symmetric PWM with lower harmonics
 * - Better for motor control than edge-aligned PWM
 *
 * For 20kHz PWM with PCLKA=120MHz:
 * - Period = PCLKA / (2 * frequency) = 120MHz / (2 * 20kHz) = 3000
 * - Resolution = 3000 counts (approximately 12-bit)
 * - Duty cycle range: 0-3000
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_mtu3a.h"

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "MTU3A";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief MTU general constants */
typedef enum {
  k_mtu_max_channels = 7, /**< MTU0-MTU4, MTU6-MTU7 (sparse indexing) */
} mtu_constants_t;

/** @brief System protection register values */
typedef enum {
  k_mtu_prcr_unlock = 0xA50B, /**< Enable writes to MSTPCR */
  k_mtu_prcr_lock   = 0xA500, /**< Disable writes to MSTPCR */
} mtu_prcr_values_t;

/** @brief MTU module stop bit positions in MSTPCRA */
typedef enum {
  k_mtu_mstpa_mtu0_4 = 9, /**< MTU0-MTU4 module stop bit */
  k_mtu_mstpa_mtu6_7 = 8, /**< MTU6-MTU7 module stop bit */
} mtu_module_stop_bits_t;

/** @brief Period calculation constants */
typedef enum {
  k_mtu_period_divisor = 2,      /**< Triangle wave period divisor */
  k_mtu_period_max     = 0xFFFF, /**< Maximum valid period (16-bit) */
  k_mtu_period_min     = 10,     /**< Minimum valid period */
  k_mtu_period_zero    = 0,      /**< Zero period value */
} mtu_period_constants_t;

/** @brief TIOR register shift positions */
typedef enum {
  k_mtu_tior_low_shift  = 0, /**< Low nibble shift (MTIOCA/MTIOCC) */
  k_mtu_tior_high_shift = 4, /**< High nibble shift (MTIOCB/MTIOCD) */
} mtu_tior_shift_t;

/** @brief TIOR register mask values */
typedef enum {
  k_mtu_tior_low_mask  = 0xF0, /**< Mask for low nibble */
  k_mtu_tior_high_mask = 0x0F, /**< Mask for high nibble */
} mtu_tior_mask_t;

/** @brief TIOR output disabled value */
typedef enum {
  k_mtu_tior_disabled = 0x00, /**< Output disabled */
} mtu_tior_disabled_t;

/** @brief Duty cycle calculation constants */
typedef enum {
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
 * @brief Get MTU channel base address
 *
 * @param[in] channel MTU channel
 *
 * @return Pointer to MTU register base, or NULL if invalid
 */
static volatile void* internal_get_mtu_base(rx_mtu_channel_t channel)
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
      return NULL;
  }
}

/**
 * @brief Calculate period register value from frequency
 *
 * @param[in] frequency_hz Desired PWM frequency in Hz
 * @param[out] period Pointer to store period value
 *
 * @return k_rx_ok on success, k_rx_err_invalid_arg if frequency too high/low
 */
static rx_err_t internal_calculate_period(uint32_t frequency_hz, uint16_t* period)
{
  /* For PWM mode 1 (triangle wave):
   * Period = PCLKA / (2 * frequency)
   * PCLKA = 120 MHz
   */
  const uint32_t pclka = k_pclka_hz;

  if (frequency_hz == k_mtu_period_zero) {
    return k_rx_err_invalid_arg;
  }

  uint32_t period_calc = pclka / (k_mtu_period_divisor * frequency_hz);

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
 * @return Pointer to TGR register, or NULL if invalid
 */
static volatile uint16_t* internal_get_tgr_register(volatile rx_mtu_channel_regs_t* mtu,
                                                    rx_mtu_output_t                 output)
{
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
      return NULL;
  }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_mtu_init_pwm(rx_mtu_channel_t channel, const rx_mtu_config_t* config)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  if ((int32_t)channel >= k_mtu_max_channels) {
    rx_log_error(s_tag, "Invalid MTU channel");
    return k_rx_err_invalid_arg;
  }

  volatile rx_mtu_channel_regs_t* mtu =
    (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Calculate period from frequency */
  uint16_t period;
  rx_err_t err = internal_calculate_period(config->frequency_hz, &period);
  if (err != k_rx_ok) {
    return err;
  }

  rx_log_info(s_tag, "Initializing MTU");

  /* Enable MTU module (clear module stop bit) */
  system_regs()->prcr = k_mtu_prcr_unlock;

  if (channel <= k_mtu_channel_4) {
    system_regs()->mstpcra &= ~(1UL << k_mtu_mstpa_mtu0_4);
  } else {
    system_regs()->mstpcra &= ~(1UL << k_mtu_mstpa_mtu6_7);
  }

  system_regs()->prcr = k_mtu_prcr_lock;

  /* Stop timer before configuration */
  rx_mtu_stop(channel);

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
  rx_mtu_start(channel);

  rx_log_info(s_tag, "MTU initialized successfully");

  return k_rx_ok;
}

rx_err_t rx_mtu_set_duty(rx_mtu_channel_t channel, rx_mtu_output_t output, float duty_percent)
{
  if ((int32_t)channel >= k_mtu_max_channels || !s_mtu_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if (duty_percent < (float)k_mtu_duty_min || duty_percent > (float)k_mtu_duty_max) {
    rx_log_error(s_tag, "Invalid duty cycle");
    return k_rx_err_invalid_arg;
  }

  /* Convert percentage to count value */
  uint16_t period     = s_mtu_period[channel];
  uint16_t duty_count = (uint16_t)((duty_percent * period) / (float)k_mtu_duty_divisor);

  return rx_mtu_set_duty_raw(channel, output, duty_count);
}

rx_err_t rx_mtu_set_duty_raw(rx_mtu_channel_t channel, rx_mtu_output_t output, uint16_t duty_count)
{
  if ((int32_t)channel >= k_mtu_max_channels || !s_mtu_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_mtu_channel_regs_t* mtu =
    (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Clamp to period */
  uint16_t period = s_mtu_period[channel];
  if (duty_count > period) {
    duty_count = period;
  }

  /* Get TGR register for this output */
  volatile uint16_t* tgr = internal_get_tgr_register(mtu, output);
  if (tgr == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Update duty cycle (buffered, takes effect on next period) */
  *tgr = duty_count;

  return k_rx_ok;
}

rx_err_t rx_mtu_get_duty(rx_mtu_channel_t channel, rx_mtu_output_t output, float* duty_percent)
{
  RX_CHECK_NULL_PTR(duty_percent, s_tag, "duty_percent pointer is NULL");

  if ((int32_t)channel >= k_mtu_max_channels || !s_mtu_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_mtu_channel_regs_t* mtu =
    (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  volatile uint16_t* tgr = internal_get_tgr_register(mtu, output);
  if (tgr == NULL) {
    return k_rx_err_invalid_arg;
  }

  uint16_t period     = s_mtu_period[channel];
  uint16_t duty_count = *tgr;

  *duty_percent = (float)(duty_count * (float)k_mtu_duty_max) / period;

  return k_rx_ok;
}

rx_err_t rx_mtu_get_period(rx_mtu_channel_t channel, uint16_t* period_count)
{
  RX_CHECK_NULL_PTR(period_count, s_tag, "period_count pointer is NULL");

  if ((int32_t)channel >= k_mtu_max_channels || !s_mtu_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  *period_count = s_mtu_period[channel];
  return k_rx_ok;
}

rx_err_t rx_mtu_enable_output(rx_mtu_channel_t channel, rx_mtu_output_t output, bool enable)
{
  if ((int32_t)channel >= k_mtu_max_channels || !s_mtu_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_mtu_channel_regs_t* mtu =
    (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Enable/disable output by modifying TIOR registers */
  uint8_t tior_value = enable ? k_mtu_tior_init_low : k_mtu_tior_disabled;

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

rx_err_t rx_mtu_start(rx_mtu_channel_t channel)
{
  if ((int32_t)channel >= k_mtu_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Set corresponding bit in TSTR register */
  switch (channel) {
    case k_mtu_channel_0:
      mtu_tstr()->tstr |= k_mtu_tstr_cst0;
      break;
    case k_mtu_channel_1:
      mtu_tstr()->tstr |= k_mtu_tstr_cst1;
      break;
    case k_mtu_channel_2:
      mtu_tstr()->tstr |= k_mtu_tstr_cst2;
      break;
    case k_mtu_channel_3:
      mtu_tstr()->tstr |= k_mtu_tstr_cst3;
      break;
    case k_mtu_channel_4:
      mtu_tstr()->tstr |= k_mtu_tstr_cst4;
      break;
    case k_mtu_channel_6:
    case k_mtu_channel_7:
      /* MTU6/7 have separate start control - simplified */
      rx_log_warn(s_tag, "MTU6/7 start not fully implemented");
      break;
    default:
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

rx_err_t rx_mtu_stop(rx_mtu_channel_t channel)
{
  if ((int32_t)channel >= k_mtu_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Clear corresponding bit in TSTR register */
  switch (channel) {
    case k_mtu_channel_0:
      mtu_tstr()->tstr &= ~k_mtu_tstr_cst0;
      break;
    case k_mtu_channel_1:
      mtu_tstr()->tstr &= ~k_mtu_tstr_cst1;
      break;
    case k_mtu_channel_2:
      mtu_tstr()->tstr &= ~k_mtu_tstr_cst2;
      break;
    case k_mtu_channel_3:
      mtu_tstr()->tstr &= ~k_mtu_tstr_cst3;
      break;
    case k_mtu_channel_4:
      mtu_tstr()->tstr &= ~k_mtu_tstr_cst4;
      break;
    case k_mtu_channel_6:
    case k_mtu_channel_7:
      /* MTU6/7 have separate start control */
      break;
    default:
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

rx_err_t rx_mtu_deinit(rx_mtu_channel_t channel)
{
  if ((int32_t)channel >= k_mtu_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Stop timer */
  rx_mtu_stop(channel);

  /* Disable all outputs */
  rx_mtu_enable_output(channel, k_mtu_output_a, false);
  rx_mtu_enable_output(channel, k_mtu_output_b, false);
  rx_mtu_enable_output(channel, k_mtu_output_c, false);
  rx_mtu_enable_output(channel, k_mtu_output_d, false);

  /* Mark as uninitialized */
  s_mtu_initialized[channel] = false;
  s_mtu_period[channel]      = k_mtu_period_zero;

  rx_log_info(s_tag, "Info");

  return k_rx_ok;
}
