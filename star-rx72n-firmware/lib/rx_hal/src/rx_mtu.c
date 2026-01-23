/* lib/rx_hal/src/rx_mtu.c */

/**
 * @file rx_mtu.c
 * @brief MTU PWM Driver Implementation
 *
 * Multi-Function Timer Unit (MTU) PWM driver.
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
 * @brief Get MTU channel base address
 *
 * @param[in] channel MTU channel
 *
 * @return Pointer to MTU register base, or NULL if invalid
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
      return NULL;
  }
}

static bool internal_is_valid_channel(const rx_mtu_channel_t channel)
{
  return internal_get_mtu_base(channel) != NULL;
}

static rx_err_t internal_clear_tstr_bit(volatile rx_mtu_tstr_regs_t* tstr, const uint8_t mask)
{
  if (tstr == NULL) {
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
 * @param[in] frequency_hz Desired PWM frequency in Hz
 * @param[out] period Pointer to store period value
 *
 * @return k_rx_ok on success, k_rx_err_invalid_arg if frequency too high/low
 */
static rx_err_t internal_calculate_period(const uint32_t frequency_hz, uint16_t* period)
{
  uint32_t period_calc;

  /* For PWM mode 1 (triangle wave):
   * Period = PCLKA / (2 * frequency)
   * PCLKA = 120 MHz
   */
  const uint32_t pclka = k_pclka_hz;

  if (period == NULL) {
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
 * @return Pointer to TGR register, or NULL if invalid
 */
static volatile uint16_t* internal_get_tgr_register(volatile rx_mtu_channel_regs_t* mtu,
                                                    const rx_mtu_output_t           output)
{
  if (mtu == NULL) {
    return NULL;
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
      return NULL;
  }
}

static rx_err_t internal_set_duty_raw_mtu(volatile rx_mtu_channel_regs_t* mtu,
                                          const rx_mtu_output_t           output,
                                          const uint16_t                  duty_count)
{
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  volatile uint16_t* tgr = internal_get_tgr_register(mtu, output);
  if (tgr == NULL) {
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

rx_err_t rx_mtu_init_pwm(const rx_mtu_channel_t channel, const rx_mtu_config_t* config)
{
  volatile rx_mtu_channel_regs_t* mtu;
  uint16_t                        period;
  rx_err_t                        err;

  RX_VALIDATE_PTR(config, s_tag, "config pointer is NULL");

  mtu = (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Calculate period from frequency */
  err = internal_calculate_period(config->frequency_hz, &period);
  if (err != k_rx_ok) {
    return err;
  }

  rx_log_info(s_tag, "Initializing MTU");

  /* Enable MTU module (clear module stop bit) */
  system_regs()->prcr = k_rx_prcr_unlock_prc1_prc3;

  if (channel <= k_mtu_channel_4) {
    system_regs()->mstpcra &= ~(k_mtu_bit_one << k_mtu_mstpa_mtu0_4);
  } else {
    system_regs()->mstpcra &= ~(k_mtu_bit_one << k_mtu_mstpa_mtu6_7);
  }

  system_regs()->prcr = k_rx_prcr_lock;

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
  if (mtu == NULL) {
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
  if (mtu == NULL) {
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

  RX_VALIDATE_PTR(duty_percent, s_tag, "duty_percent pointer is NULL");

  mtu = (volatile rx_mtu_channel_regs_t*)internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_mtu_initialized[channel], s_tag, "MTU channel not initialized");

  tgr = internal_get_tgr_register(mtu, output);
  if (tgr == NULL) {
    return k_rx_err_invalid_arg;
  }

  period     = s_mtu_period[channel];
  duty_count = *tgr;

  *duty_percent = (float)(duty_count * (float)k_mtu_duty_max) / period;

  return k_rx_ok;
}

rx_err_t rx_mtu_get_period(const rx_mtu_channel_t channel, uint16_t* period_count)
{
  RX_VALIDATE_PTR(period_count, s_tag, "period_count pointer is NULL");

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
  if (mtu == NULL) {
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
