/* lib/rx_hal/src/rx_gptw.c */

/**
 * @file rx_gptw.c
 * @brief GPTW PWM Driver Implementation for Motor Control
 * @details
 * General PWM Timer driver for brushed DC motors on RX72N.
 *
 * Supported PWM Modes:
 *
 * 1. Sawtooth-wave PWM (edge-aligned):
 *    - Counter: 0 -> GTPR -> 0 (reset)
 *    - Period = PCLKA / frequency
 *    - For 20kHz: period = 120MHz / 20kHz = 6000 counts (~12.5-bit resolution)
 *
 * 2. Triangle-wave PWM (center-aligned):
 *    - Counter: 0 -> GTPR -> 0 (up/down)
 *    - Period = PCLKA / (2 * frequency)
 *    - For 20kHz: period = 120MHz / 40kHz = 3000 counts (~11.5-bit resolution)
 *    - Lower EMI, reduced current ripple, optimal for H-bridge
 *
 * @see RX72N Hardware Manual Section 26.2.12 (GTCR register)
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
 * @brief Check if wave mode is triangle (center-aligned)
 *
 * @param[in] mode Waveform mode
 *
 * @return true if triangle mode, false if sawtooth
 */
static inline bool internal_is_triangle_mode(const rx_gptw_wave_mode_t mode)
{
  return (mode == k_gptw_wave_tri_pwm1) || (mode == k_gptw_wave_tri_pwm2) ||
         (mode == k_gptw_wave_tri_pwm3);
}

/**
 * @brief Get GTCR mode bits for wave mode
 *
 * @param[in] mode Waveform mode
 *
 * @return GTCR.MD bits for the specified mode
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
 * @param[in] frequency_hz Desired PWM frequency in Hz
 * @param[in] wave_mode    Waveform mode (affects period calculation)
 * @param[out] period      Pointer to store period value
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if period is NULL
 * @return k_rx_err_invalid_arg if frequency or wave_mode is invalid
 */
static rx_err_t internal_calculate_period(const uint32_t            frequency_hz,
                                          const rx_gptw_wave_mode_t wave_mode,
                                          uint32_t*                 period)
{
  /* Pre-condition: validate pointer (NASA Power of 10 Rule 5) */
  RX_CHECK_NULL_PTR(period, s_tag, "period pointer is NULL");

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
   * PCLKA = 120 MHz
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
  system_regs()->mstpcrc &= ~((uint32_t)k_gptw_bit_set << k_mstpc_gptw);
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
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if gptw or config is NULL
 * @return k_rx_err_invalid_arg if period is out of range
 */
static rx_err_t internal_configure_gptw_hardware(volatile rx_gptw_channel_regs_t* gptw,
                                                 const rx_gptw_config_t*          config,
                                                 const uint32_t                   period)
{
  /* Pre-condition: validate pointers (NASA Power of 10 Rule 5) */
  RX_CHECK_NULL_PTR(gptw, s_tag, "gptw pointer is NULL");
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  /* Pre-condition: validate period range */
  if ((period < k_gptw_period_min) || (period > s_gptw_period_max)) {
    rx_log_error(s_tag, "Period out of range");
    return k_rx_err_invalid_arg;
  }

  /* Unlock write protection for this channel */
  gptw->gtwp = k_gptw_gtwp_unlock;

  /* Configure control register
   * - PCLKA/1 (120 MHz)
   * - Waveform mode from config (sawtooth or triangle)
   */
  const uint32_t gtcr_mode = internal_get_gtcr_mode(config->wave_mode);
  gptw->gtcr               = k_gptw_gtcr_tpcs_1 | gtcr_mode;

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

  return k_rx_ok;
}

static rx_err_t internal_prepare_gptw_pwm_init(const rx_gptw_channel_t           channel,
                                               const rx_gptw_config_t*           config,
                                               volatile rx_gptw_channel_regs_t** gptw_out,
                                               uint32_t*                         period_out)
{
  rx_err_t err;

  if ((int32_t)channel >= (int32_t)k_gptw_max_channels) {
    rx_log_error(s_tag, "Invalid GPTW channel");
    return k_rx_err_invalid_arg;
  }

  *gptw_out = internal_get_gptw_base(channel);
  if (*gptw_out == NULL) {
    return k_rx_err_invalid_arg;
  }

  err = internal_calculate_period(config->frequency_hz, config->wave_mode, period_out);
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
  const uint32_t period     = s_gptw_period[channel_value];
  const uint32_t duty_count = (uint32_t)((duty_percent * period) / (float)k_gptw_duty_divisor);

  return rx_gptw_set_duty_raw(channel_value, output_value, duty_count);
}

rx_err_t
rx_gptw_set_duty_raw(const rx_gptw_channel_t channel, rx_gptw_output_t output, uint32_t duty_count)
{
  if (((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
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

  if (((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
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

  if (((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  *period_count = s_gptw_period[channel];
  return k_rx_ok;
}

rx_err_t
rx_gptw_enable_output(const rx_gptw_channel_t channel, const rx_gptw_output_t output, bool enable)
{
  if (((int32_t)channel >= (int32_t)k_gptw_max_channels) || !s_gptw_initialized[channel]) {
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
  if ((int32_t)channel >= (int32_t)k_gptw_max_channels) {
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
  if ((int32_t)channel >= (int32_t)k_gptw_max_channels) {
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

/* =============================================================================
 * Phase Staggering Implementation
 * =============================================================================
 */

/**
 * @brief Phase divisors for staggered PWM initialization
 * @details Values represent the divisor used to calculate counter offset from period.
 * k_phase_divisor_none=1 for 0-degree (no division needed, offset=0).
 * Division by these values yields the phase offset in counts.
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
 * @param[in] channel GPTW channel (must be k_gptw_channel_0 to k_gptw_channel_3)
 * @param[in] period PWM period in counts (must be > 0)
 * @param[in] is_triangle True if triangle mode (center-aligned)
 * @param[out] init_count Pointer to store initial counter value (must be non-NULL)
 * @param[out] init_dir_up Pointer to store initial direction (true=up, false=down)
 *                         Only valid for triangle mode. (must be non-NULL)
 *
 * @note If any validation fails, outputs are set to safe defaults (0, true)
 *       and the function returns early.
 */
static void internal_calculate_phase_offset(const rx_gptw_channel_t channel,
                                            const uint32_t          period,
                                            const bool              is_triangle,
                                            uint32_t*               init_count,
                                            bool*                   init_dir_up)
{
  /* Pre-condition: validate output pointers (NASA Power of 10 Rule 5) */
  if (init_count == NULL || init_dir_up == NULL) {
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
 * @brief Configure a single GPTW channel for staggered PWM
 *
 * Performs hardware configuration, MPC setup, pin configuration, and phase
 * staggering for one GPTW channel.
 *
 * @param[in] channel     GPTW channel to configure
 * @param[in] config      Configuration parameters
 * @param[in] period      Pre-calculated period value
 * @param[in] is_triangle True if using triangle wave mode
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if gptw base is NULL
 * @return Propagated error codes from internal_configure_gptw_hardware or internal_configure_mpc
 */
static rx_err_t internal_configure_channel_staggered(const rx_gptw_channel_t channel,
                                                     const rx_gptw_config_t* config,
                                                     const uint32_t          period,
                                                     const bool              is_triangle)
{
  rx_err_t err;

  /* Pre-condition: validate channel range (NASA Power of 10 Rule 5) */
  if ((int32_t)channel >= (int32_t)k_gptw_max_channels) {
    return k_rx_err_invalid_arg;
  }

  volatile rx_gptw_channel_regs_t* gptw = internal_get_gptw_base(channel);

  /* Pre-condition: validate gptw pointer */
  if (gptw == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Base hardware configuration */
  err = internal_configure_gptw_hardware(gptw, config, period);
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

rx_err_t rx_gptw_init_all_staggered(const rx_gptw_config_t* config)
{
  rx_err_t err;
  uint32_t period;

  /* Pre-condition: validate config pointer (NASA Power of 10 Rule 5) */
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

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
  if (gptw_common() != NULL) {
    gptw_common()->gtstpa = k_gptw_all_channels_mask; /* Stop Ch0-3 */
  }

  /* Calculate period once (assumes same frequency for all) */
  err = internal_calculate_period(config->frequency_hz, config->wave_mode, &period);
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
  if (gptw_common() != NULL) {
    rx_log_info(s_tag, "Global start of GPTW channels");
    gptw_common()->gtstra = k_gptw_all_channels_mask; /* Start Ch0-3 */
  } else {
    return k_rx_err_hw_error;
  }

  return k_rx_ok;
}
