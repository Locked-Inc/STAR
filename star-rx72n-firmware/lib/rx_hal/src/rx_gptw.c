/* lib/rx_hal/src/rx_gptw.c */

/**
 * @file rx_gptw.c
 * @brief GPTW PWM Driver Implementation for Motor Control
 * @details
 * General PWM Timer driver for brushed DC motors on RX72N.
 *
 * PWM Mode (Sawtooth Wave - Edge-Aligned):
 * - Counter counts up from 0 to GTPR (period)
 * - GTCCRA/GTCCRB control duty cycle
 * - Efficient for H-bridge motor control
 *
 * For 20kHz PWM with PCLKA=120MHz:
 * - Period = PCLKA / frequency = 120MHz / 20kHz = 6000
 * - Resolution = 6000 counts (approximately 12.5-bit)
 * - Duty cycle range: 0-6000
 *
 * @warning Base addresses derived from hirakuni45/RX framework.
 * Verify against RX72N Hardware Manual before production use.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

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
  k_gptw_period_min        = 10, /**< Minimum valid period */
  k_gptw_period_zero       = 0,  /**< Zero period value */
  k_gptw_deadtime_disabled = 0,  /**< No dead time */
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
static uint32_t s_gptw_period[k_gptw_max_channels] = {0};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get GPTW channel base address
 *
 * @param[in] channel GPTW channel
 *
 * @return Pointer to GPTW register base, or NULL if invalid
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
static rx_err_t internal_calculate_period(const uint32_t frequency_hz, uint32_t* period)
{
  /* For PWM mode (sawtooth wave):
   * Period = PCLKA / frequency
   * PCLKA = 120 MHz
   */
  const uint32_t pclka = k_pclka_hz;

  if (frequency_hz == k_gptw_period_zero) {
    return k_rx_err_invalid_arg;
  }

  const uint32_t period_calc = pclka / frequency_hz;

  /* Check if period fits in 32-bit register */
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
 * Sets up Port E alternate functions for GPTW outputs.
 *
 * @param[in] channel GPTW channel to configure
 *
 * @return k_rx_ok on success
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
 * @param[in] channel GPTW channel
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
 * Unlocks system protection and clears the GPTW module stop bit.
 */
static void internal_enable_gptw_module_clock(void)
{
  system_regs()->prcr = k_rx_prcr_unlock_prc1_prc3;
  system_regs()->mstpcrc &= ~(1UL << k_mstpc_gptw);
  system_regs()->prcr = k_rx_prcr_lock;
}

/**
 * @brief Configure GPTW hardware registers
 *
 * Sets up GPTW control, I/O, period, duty cycle, buffer, and dead time registers.
 *
 * @param[in] gptw   Pointer to GPTW channel registers
 * @param[in] config Configuration parameters
 * @param[in] period Calculated period value
 */
static void internal_configure_gptw_hardware(volatile rx_gptw_channel_regs_t* gptw,
                                             const rx_gptw_config_t*          config,
                                             const uint32_t                   period)
{
  /* Unlock write protection for this channel */
  gptw->gtwp = k_gptw_gtwp_unlock;

  /* Configure control register
   * - PCLKA/1 (120 MHz)
   * - Sawtooth wave continuous mode
   */
  gptw->gtcr = k_gptw_gtcr_tpcs_1 | k_gptw_gtcr_md_saw_cont;

  /* Configure I/O control register
   * - Initial low, toggle on compare match for both outputs
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

  /* Enable buffer operation for glitch-free updates */
  gptw->gtber = k_gptw_gtber_ccra_buf | k_gptw_gtber_ccrb_buf;

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
}

static rx_err_t internal_prepare_gptw_pwm_init(const rx_gptw_channel_t           channel,
                                               const rx_gptw_config_t*           config,
                                               volatile rx_gptw_channel_regs_t** gptw_out,
                                               uint32_t*                         period_out)
{
  rx_err_t err;

  if (((int32_t)channel < (int32_t)k_gptw_channel_0) ||
      ((int32_t)channel >= (int32_t)k_gptw_max_channels)) {
    rx_log_error(s_tag, "Invalid GPTW channel");
    return k_rx_err_invalid_arg;
  }

  *gptw_out = internal_get_gptw_base(channel);
  if (*gptw_out == NULL) {
    return k_rx_err_invalid_arg;
  }

  err = internal_calculate_period(config->frequency_hz, period_out);
  if (err != k_rx_ok) {
    return err;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_gptw_init_pwm(const rx_gptw_channel_t channel, const rx_gptw_config_t* config)
{
  volatile rx_gptw_channel_regs_t* gptw;
  uint32_t                         period;
  rx_err_t                         err;

  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  err = internal_prepare_gptw_pwm_init(channel, config, &gptw, &period);
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
  internal_configure_gptw_hardware(gptw, config, period);

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

rx_err_t rx_gptw_set_duty(const rx_gptw_channel_t channel,
                          const rx_gptw_output_t  output,
                          const float             duty_percent)
{
  if (((int32_t)channel < (int32_t)k_gptw_channel_0) ||
      ((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if (duty_percent < (float)k_gptw_duty_min || duty_percent > (float)k_gptw_duty_max) {
    rx_log_error(s_tag, "Invalid duty cycle");
    return k_rx_err_invalid_arg;
  }

  /* Convert percentage to count value */
  const uint32_t period     = s_gptw_period[channel];
  const uint32_t duty_count = (uint32_t)((duty_percent * period) / (float)k_gptw_duty_divisor);

  return rx_gptw_set_duty_raw(channel, output, duty_count);
}

rx_err_t
rx_gptw_set_duty_raw(const rx_gptw_channel_t channel, rx_gptw_output_t output, uint32_t duty_count)
{
  if (((int32_t)channel < (int32_t)k_gptw_channel_0) ||
      ((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Clamp to period */
  const uint32_t period = s_gptw_period[channel];
  if (duty_count > period) {
    duty_count = period;
  }

  /* Update duty cycle (buffered, takes effect on next period) */
  switch (output) {
    case k_gptw_output_a:
      gptw->gtccra = duty_count;
      break;
    case k_gptw_output_b:
      gptw->gtccrb = duty_count;
      break;
    default:
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

rx_err_t rx_gptw_get_duty(const rx_gptw_channel_t channel,
                          const rx_gptw_output_t  output,
                          float*                  duty_percent)
{
  RX_CHECK_NULL_PTR(duty_percent, s_tag, "duty_percent pointer is NULL");

  if (((int32_t)channel < (int32_t)k_gptw_channel_0) ||
      ((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  const volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == NULL) {
    return k_rx_err_invalid_arg;
  }

  const uint32_t period     = s_gptw_period[channel];
  uint32_t       duty_count = 0;

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

  *duty_percent = (float)(duty_count * (float)k_gptw_duty_max) / period;

  return k_rx_ok;
}

rx_err_t rx_gptw_get_period(const rx_gptw_channel_t channel, uint32_t* period_count)
{
  RX_CHECK_NULL_PTR(period_count, s_tag, "period_count pointer is NULL");

  if (((int32_t)channel < (int32_t)k_gptw_channel_0) ||
      ((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  *period_count = s_gptw_period[channel];
  return k_rx_ok;
}

rx_err_t
rx_gptw_enable_output(const rx_gptw_channel_t channel, const rx_gptw_output_t output, bool enable)
{
  if (((int32_t)channel < (int32_t)k_gptw_channel_0) ||
      ((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == NULL) {
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

rx_err_t rx_gptw_start(const rx_gptw_channel_t channel)
{
  if (((int32_t)channel < (int32_t)k_gptw_channel_0) ||
      ((int32_t)channel >= (int32_t)k_gptw_max_channels)) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Unlock and set CST bit in GTCR */
  gptw->gtwp = k_gptw_gtwp_unlock;
  gptw->gtcr |= k_gptw_gtcr_cst;
  gptw->gtwp = k_gptw_gtwp_lock;

  return k_rx_ok;
}

rx_err_t rx_gptw_stop(const rx_gptw_channel_t channel)
{
  if (((int32_t)channel < (int32_t)k_gptw_channel_0) ||
      ((int32_t)channel >= (int32_t)k_gptw_max_channels)) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);
  if (gptw == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Unlock and clear CST bit in GTCR */
  gptw->gtwp = k_gptw_gtwp_unlock;
  gptw->gtcr &= ~k_gptw_gtcr_cst;
  gptw->gtwp = k_gptw_gtwp_lock;

  return k_rx_ok;
}

rx_err_t rx_gptw_deinit(const rx_gptw_channel_t channel)
{
  rx_err_t err;

  if (((int32_t)channel < (int32_t)k_gptw_channel_0) ||
      ((int32_t)channel >= (int32_t)k_gptw_max_channels)) {
    return k_rx_err_invalid_arg;
  }

  /* Stop timer */
  err = rx_gptw_stop(channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Disable outputs */
  err = rx_gptw_enable_output(channel, k_gptw_output_a, false);
  if (err != k_rx_ok) {
    return err;
  }
  err = rx_gptw_enable_output(channel, k_gptw_output_b, false);
  if (err != k_rx_ok) {
    return err;
  }

  /* Mark as uninitialized */
  s_gptw_initialized[channel] = false;
  s_gptw_period[channel]      = k_gptw_period_zero;

  rx_log_info(s_tag, "GPTW deinitialized");

  return k_rx_ok;
}
