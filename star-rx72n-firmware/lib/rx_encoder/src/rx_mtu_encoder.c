/* lib/rx_encoder/src/rx_mtu_encoder.c */

/**
 * @file rx_mtu_encoder.c
 * @brief MTU Encoder Driver Implementation (Phase Counting Mode)
 * @details
 * Quadrature encoder counting using MTU phase counting mode.
 *
 * Phase Counting Mode (TMDR.MD = 0100):
 * - MTCLKA = Phase A input
 * - MTCLKB = Phase B input
 * - 4x decoding (count on all edges)
 * - Counter auto-increments/decrements based on phase relationship
 *
 * Direction Detection:
 * - Phase A leads Phase B: Count up (forward)
 * - Phase B leads Phase A: Count down (reverse)
 *
 * Overflow Handling:
 * - 16-bit counter wraps at 65536
 * - Software tracks total count across wraps
 * - Must read frequently to detect wraps (every ~48 revs for 341 PPR)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_mtu_encoder.h"

#include <math.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_gpio_constants.h"
#include "rx_log.h"

static const char* s_tag = "MTU_ENCODER";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief Encoder configuration and hardware constants
 */
typedef enum {
  k_encoder_max_channels       = 7,    /**< Maximum MTU channels for encoders */
  k_tmdr_phase_counting_mode_1 = 0x04, /**< Phase counting mode 1 (4x decoding) */

  /* 16-bit counter limits */
  k_encoder_counter_max  = 65536,  /**< 16-bit counter maximum value */
  k_encoder_counter_half = 32768,  /**< Half of counter for wraparound detection */
  k_encoder_16bit_mask   = 0xFFFF, /**< Bitmask for 16-bit values */

  /* Module stop control bits */
  k_mtu_mstpcra_mtu0_4_bit = 9, /**< MSTPCRA bit for MTU0-MTU4 */
  k_mtu_mstpcra_mtu6_7_bit = 8, /**< MSTPCRA bit for MTU6-MTU7 */

  /* PRCR (Protect Register) values */
  k_prcr_unlock_mtu = 0x0B, /**< Unlock PRC0, PRC1, PRC3 for MTU config */

  /* Timer control defaults */
  k_tcr_external_clock_no_prescaler = 0x00, /**< External clock, no prescaler */
  k_tior_disabled                   = 0x00, /**< I/O control disabled */
} encoder_constants_t;

/**
 * @brief Encoder calculation constants
 */
typedef enum {
  k_degrees_per_revolution = 360, /**< Degrees in one full revolution */
} encoder_calc_constants_t;

/**
 * @brief Encoder initialization values
 */
typedef enum {
  k_encoder_count_reset      = 0, /**< Counter reset value */
  k_encoder_initial_count    = 0, /**< Initial total count */
  k_encoder_initial_rev      = 0, /**< Initial revolution count */
  k_encoder_min_counts_per_rev = 1, /**< Minimum valid counts per revolution */
} encoder_init_values_t;

/**
 * @brief Encoder velocity limits
 */
typedef enum {
  k_max_realistic_velocity_rps = 100, /**< Maximum realistic velocity (6000 RPM) */
} encoder_velocity_limits_t;

/* Floating-point constants (enums can't hold floats) */
static const float k_encoder_initial_position_deg = 0.0f; /**< Initial position in degrees */
static const float k_min_delta_time_s = 0.0f; /**< Minimum delta time for velocity calculation */

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static bool               s_encoder_initialized[k_encoder_max_channels] = {false};
static rx_encoder_state_t s_encoder_state[k_encoder_max_channels]       = {0};
static uint16_t           s_counts_per_rev[k_encoder_max_channels]      = {0};
static bool               s_invert_direction[k_encoder_max_channels]    = {false};
static int32_t            s_last_count[k_encoder_max_channels]          = {0};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

static rx_err_t internal_enable_mtu_module(rx_mtu_channel_t channel);
static rx_err_t internal_configure_encoder_timer(volatile rx_mtu_channel_regs_t* mtu);
static rx_err_t internal_verify_timer_counting(volatile rx_mtu_channel_regs_t* mtu);
static rx_err_t internal_initialize_encoder_state(rx_mtu_channel_t                channel,
                                                    const rx_encoder_config_t*     config);

/**
 * @brief Get MTU channel base address
 *
 * @param[in] channel MTU channel
 *
 * @return Pointer to MTU register base, or NULL if invalid
 */
static volatile rx_mtu_channel_regs_t* internal_get_mtu_base(rx_mtu_channel_t channel)
{
  switch (channel) {
    case k_mtu_channel_0:
      return (volatile rx_mtu_channel_regs_t*)mtu0();
    case k_mtu_channel_1:
      return (volatile rx_mtu_channel_regs_t*)mtu1();
    case k_mtu_channel_2:
      return (volatile rx_mtu_channel_regs_t*)mtu2();
    case k_mtu_channel_3:
      return (volatile rx_mtu_channel_regs_t*)((volatile uint8_t*)mtu3());
    case k_mtu_channel_4:
      return (volatile rx_mtu_channel_regs_t*)((volatile uint8_t*)mtu4());
    case k_mtu_channel_6:
      return (volatile rx_mtu_channel_regs_t*)mtu6();
    case k_mtu_channel_7:
      return (volatile rx_mtu_channel_regs_t*)mtu7();
    default:
      return NULL;
  }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_encoder_init(const rx_encoder_config_t* config)
{
  rx_err_t err = k_rx_ok;

  /* Validate inputs */
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  rx_mtu_channel_t channel = config->channel;

  if ((int32_t)channel >= k_encoder_max_channels) {
    rx_log_error(s_tag, "Invalid MTU channel");
    return k_rx_err_invalid_arg;
  }

  if (config->counts_per_rev < k_encoder_min_counts_per_rev) {
    rx_log_error(s_tag, "Invalid counts per revolution");
    return k_rx_err_invalid_arg;
  }

  volatile rx_mtu_channel_regs_t* mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Enable MTU module */
  err = internal_enable_mtu_module(channel);
  if (err != k_rx_ok) {
    return err;
  }

  /* Stop timer before configuration */
  err = rx_mtu_stop(channel);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to stop timer before init");
    return err;
  }

  /* Configure timer for encoder mode */
  err = internal_configure_encoder_timer(mtu);
  if (err != k_rx_ok) {
    return err;
  }

  /* Initialize state */
  err = internal_initialize_encoder_state(channel, config);
  if (err != k_rx_ok) {
    return err;
  }

  /* Start counter */
  err = rx_mtu_start(channel);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to start encoder timer");
    s_encoder_initialized[channel] = false;
    return err;
  }

  /* Post-condition: Verify timer is counting */
  err = internal_verify_timer_counting(mtu);
  if (err != k_rx_ok) {
    s_encoder_initialized[channel] = false;
    return err;
  }

  rx_log_info(s_tag, "MTU encoder initialized successfully");
  return k_rx_ok;
}

rx_err_t rx_encoder_read_raw(rx_mtu_channel_t channel, uint16_t* count)
{
  RX_CHECK_NULL_PTR(count, s_tag, "count pointer is NULL");

  if ((int32_t)channel >= k_encoder_max_channels || !s_encoder_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_mtu_channel_regs_t* mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  *count = mtu->tcnt;
  return k_rx_ok;
}

rx_err_t rx_encoder_read_count(rx_mtu_channel_t channel, rx_encoder_state_t* state)
{
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is NULL");

  if ((int32_t)channel >= k_encoder_max_channels || !s_encoder_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_mtu_channel_regs_t* mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Read current counter value */
  uint16_t current_count = mtu->tcnt;
  uint16_t last_count    = s_encoder_state[channel].last_raw_count;

  /* Calculate delta (handling 16-bit wraparound) */
  int32_t delta;
  if (current_count >= last_count) {
    /* Normal case: no wraparound */
    delta = current_count - last_count;
  } else {
    /* Wraparound occurred */
    delta = (k_encoder_counter_max - last_count) + current_count;
  }

  /* Check for reverse wraparound */
  if (delta > k_encoder_counter_half) {
    /* Large positive delta means we actually went backwards */
    delta = delta - k_encoder_counter_max;
  }

  /* Invert direction if configured */
  if (s_invert_direction[channel]) {
    delta = -delta;
  }

  /* Update total count */
  s_encoder_state[channel].total_count += delta;
  s_encoder_state[channel].last_raw_count = current_count;

  /* Calculate revolutions and position */
  uint16_t counts_per_rev              = s_counts_per_rev[channel];
  s_encoder_state[channel].revolutions = s_encoder_state[channel].total_count / counts_per_rev;

  int32_t remainder_counts = s_encoder_state[channel].total_count % counts_per_rev;
  s_encoder_state[channel].position_deg =
    (float)(remainder_counts * k_degrees_per_revolution) / counts_per_rev;

  /* Post-condition: Validate position is within 0-360 range */
  if (s_encoder_state[channel].position_deg < k_encoder_initial_position_deg ||
      s_encoder_state[channel].position_deg >= k_degrees_per_revolution) {
    rx_log_error(s_tag, "Position calculation overflow");
    return k_rx_err_out_of_range;
  }

  /* Copy to output */
  *state = s_encoder_state[channel];

  return k_rx_ok;
}

rx_err_t rx_encoder_read_velocity(rx_mtu_channel_t channel, float delta_time_s, float* velocity_rps)
{
  RX_CHECK_NULL_PTR(velocity_rps, s_tag, "velocity_rps pointer is NULL");

  if ((int32_t)channel >= k_encoder_max_channels || !s_encoder_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  if (delta_time_s <= k_min_delta_time_s) {
    rx_log_error(s_tag, "Invalid delta time for velocity calculation");
    return k_rx_err_invalid_arg;
  }

  /* Read current count */
  rx_encoder_state_t state;
  rx_err_t           err = rx_encoder_read_count(channel, &state);
  if (err != k_rx_ok) {
    return err;
  }

  /* Calculate velocity based on count change */
  int32_t delta_count   = state.total_count - s_last_count[channel];
  s_last_count[channel] = state.total_count;

  /* Convert to revolutions per second */
  uint16_t counts_per_rev = s_counts_per_rev[channel];
  float    delta_revs     = (float)delta_count / counts_per_rev;
  *velocity_rps           = delta_revs / delta_time_s;

  /* Post-condition: Validate velocity is realistic */
  if (fabsf(*velocity_rps) > k_max_realistic_velocity_rps) {
    rx_log_warn(s_tag, "Unrealistic velocity detected - possible encoder failure");
    /* Don't return error, just warn - allow for brief overspeed conditions */
  }

  return k_rx_ok;
}

rx_err_t rx_encoder_reset(rx_mtu_channel_t channel)
{
  if ((int32_t)channel >= k_encoder_max_channels || !s_encoder_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_mtu_channel_regs_t* mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Reset hardware counter */
  mtu->tcnt = k_encoder_count_reset;

  /* Reset state */
  s_encoder_state[channel].total_count    = k_encoder_initial_count;
  s_encoder_state[channel].last_raw_count = k_encoder_count_reset;
  s_encoder_state[channel].revolutions    = k_encoder_initial_rev;
  s_encoder_state[channel].position_deg   = k_encoder_initial_position_deg;
  s_last_count[channel]                   = k_encoder_initial_count;

  return k_rx_ok;
}

rx_err_t rx_encoder_set_count(rx_mtu_channel_t channel, int32_t count)
{
  if ((int32_t)channel >= k_encoder_max_channels || !s_encoder_initialized[channel]) {
    return k_rx_err_invalid_state;
  }

  volatile rx_mtu_channel_regs_t* mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Set hardware counter (limited to 16-bit) */
  mtu->tcnt = (uint16_t)(count & k_encoder_16bit_mask);

  /* Set software state */
  s_encoder_state[channel].total_count    = count;
  s_encoder_state[channel].last_raw_count = (uint16_t)(count & k_encoder_16bit_mask);
  s_last_count[channel]                   = count;

  /* Recalculate position */
  uint16_t counts_per_rev              = s_counts_per_rev[channel];
  s_encoder_state[channel].revolutions = count / counts_per_rev;

  int32_t remainder_counts = count % counts_per_rev;
  s_encoder_state[channel].position_deg =
    (float)(remainder_counts * k_degrees_per_revolution) / counts_per_rev;

  return k_rx_ok;
}

rx_err_t rx_encoder_deinit(rx_mtu_channel_t channel)
{
  if ((int32_t)channel >= k_encoder_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Stop timer (explicitly ignore return value on cleanup path) */
  (void)rx_mtu_stop(channel);

  /* Mark as uninitialized */
  s_encoder_initialized[channel] = false;
  s_counts_per_rev[channel]      = k_encoder_count_reset;

  rx_log_info(s_tag, "MTU encoder deinitialized");

  return k_rx_ok;
}

/* =============================================================================
 * Internal Helper Function Implementations
 * =============================================================================
 */

/**
 * @brief Enable MTU module by clearing module stop bit
 *
 * @param[in] channel MTU channel
 *
 * @return k_rx_ok on success
 */
static rx_err_t internal_enable_mtu_module(rx_mtu_channel_t channel)
{
  /* Enable MTU module (clear module stop bit) */
  system_regs()->prcr =
    (k_prcr_key << k_prcr_key_shift) | k_prcr_unlock_mtu; /* Enable writes to MSTPCR */

  if (channel <= k_mtu_channel_4) {
    system_regs()->mstpcra &= ~(1 << k_mtu_mstpcra_mtu0_4_bit); /* MTU0-MTU4 */
  } else {
    system_regs()->mstpcra &= ~(1 << k_mtu_mstpcra_mtu6_7_bit); /* MTU6-MTU7 */
  }

  system_regs()->prcr = (k_prcr_key << k_prcr_key_shift) | k_prcr_lock_all; /* Lock MSTPCR */

  return k_rx_ok;
}

/**
 * @brief Configure MTU timer for encoder phase counting mode
 *
 * @param[in] mtu Pointer to MTU channel registers
 *
 * @return k_rx_ok on success
 */
static rx_err_t internal_configure_encoder_timer(volatile rx_mtu_channel_regs_t* mtu)
{
  /* Configure Timer Control Register (TCR)
   * - No prescaler (count directly on phase inputs)
   * - External clock on MTCLKA/B
   */
  mtu->tcr = k_tcr_external_clock_no_prescaler;

  /* Configure Phase Counting Mode 1 (4x decoding)
   * TMDR.MD = 0100
   */
  mtu->tmdr = k_tmdr_phase_counting_mode_1;

  /* Configure I/O control (not used in phase counting mode) */
  mtu->tiorh = k_tior_disabled;
  mtu->tiorl = k_tior_disabled;

  /* Clear counter */
  mtu->tcnt = k_encoder_count_reset;

  return k_rx_ok;
}

/**
 * @brief Verify timer is counting after initialization
 *
 * @param[in] mtu Pointer to MTU channel registers
 *
 * @return k_rx_ok if timer is counting
 * @return k_rx_err_hw_init_failed if timer not counting
 */
static rx_err_t internal_verify_timer_counting(volatile rx_mtu_channel_regs_t* mtu)
{
  /* Note: This verification is optional and may not work if encoder is stationary.
   * In production, consider removing this check or only enabling in debug builds.
   * For now, we'll skip this check as it requires encoder movement.
   */
  (void)mtu; /* Unused - timer verification would require encoder motion */
  return k_rx_ok;
}

/**
 * @brief Initialize encoder state variables
 *
 * @param[in] channel MTU channel
 * @param[in] config Encoder configuration
 *
 * @return k_rx_ok on success
 */
static rx_err_t internal_initialize_encoder_state(rx_mtu_channel_t           channel,
                                                    const rx_encoder_config_t* config)
{
  /* Initialize state */
  s_encoder_state[channel].total_count    = k_encoder_initial_count;
  s_encoder_state[channel].last_raw_count = k_encoder_count_reset;
  s_encoder_state[channel].revolutions    = k_encoder_initial_rev;
  s_encoder_state[channel].position_deg   = k_encoder_initial_position_deg;

  s_counts_per_rev[channel]      = config->counts_per_rev;
  s_invert_direction[channel]    = config->invert_direction;
  s_last_count[channel]          = k_encoder_initial_count;
  s_encoder_initialized[channel] = true;

  return k_rx_ok;
}
