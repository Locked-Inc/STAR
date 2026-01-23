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
#include "rx_register_protection.h"

static const char* s_tag = "MTU_ENCODER";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief Encoder configuration and hardware constants
 */
typedef enum : uint32_t {
  k_encoder_max_channels       = 8,    /**< Maximum MTU channels for encoders (max index 7) */
  k_tmdr_phase_counting_mode_1 = 0x04, /**< Phase counting mode 1 (4x decoding) */

  /* 16-bit counter limits */
  k_encoder_counter_max  = 65536,  /**< 16-bit counter maximum value */
  k_encoder_counter_half = 32768,  /**< Half of counter for wraparound detection */
  k_encoder_16bit_mask   = 0xFFFF, /**< Bitmask for 16-bit values */

  /* Module stop control bits */
  k_mtu_mstpcra_mtu0_4_bit = 9, /**< MSTPCRA bit for MTU0-MTU4 */
  k_mtu_mstpcra_mtu6_7_bit = 8, /**< MSTPCRA bit for MTU6-MTU7 */

  /* Timer control defaults */
  k_tcr_external_clock_no_prescaler = 0x00, /**< External clock, no prescaler */
  k_tior_disabled                   = 0x00, /**< I/O control disabled */
} encoder_constants_t;

/**
 * @brief Encoder calculation constants
 */
typedef enum : uint16_t {
  k_turns_multiplier       = 2,   /**< Multiplier for turns range validation (±720°) */
  k_degrees_per_revolution = 360, /**< Degrees in one full revolution */
} encoder_calc_constants_t;

/**
 * @brief Encoder initialization values
 */
typedef enum : uint16_t {
  k_encoder_count_reset        = 0,     /**< Counter reset value */
  k_encoder_initial_count      = 0,     /**< Initial total count */
  k_encoder_initial_rev        = 0,     /**< Initial revolution count */
  k_encoder_min_counts_per_rev = 1,     /**< Minimum valid counts per revolution */
  k_encoder_max_counts_per_rev = 65535, /**< Maximum valid counts per revolution */
} encoder_init_values_t;

/**
 * @brief Encoder velocity limits
 */
typedef enum : uint16_t {
  k_max_realistic_velocity_rps = 100, /**< Maximum realistic velocity (6000 RPM) */
} encoder_velocity_limits_t;

/* Floating-point constants (enums can't hold floats) */
static const float k_encoder_initial_position_deg = 0.0f; /**< Initial position in degrees */
static const float k_min_delta_time_s = 0.0f;  /**< Minimum delta time for velocity calculation */
static const float k_max_delta_time_s = 10.0f; /**< Maximum delta time for velocity calculation */

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
static rx_err_t internal_verify_timer_counting(const volatile rx_mtu_channel_regs_t* mtu);
static rx_err_t internal_initialize_encoder_state(rx_mtu_channel_t           channel,
                                                  const rx_encoder_config_t* config);
static rx_err_t internal_update_state_from_count(rx_encoder_state_t* state,
                                                 rx_mtu_channel_t    channel,
                                                 uint16_t            current_count);

static bool internal_is_valid_channel(rx_mtu_channel_t channel);

/**
 * @brief Get MTU channel base address
 *
 * @param[in] channel MTU channel
 *
 * @return Pointer to MTU register base, or NULL if invalid
 */
static volatile rx_mtu_channel_regs_t* internal_get_mtu_base(const rx_mtu_channel_t channel)
{
  switch (channel) {
    case k_mtu_channel_0:
      return (volatile rx_mtu_channel_regs_t*)mtu0();
    case k_mtu_channel_1:
      return (volatile rx_mtu_channel_regs_t*)mtu1();
    case k_mtu_channel_2:
      return (volatile rx_mtu_channel_regs_t*)mtu2();
    case k_mtu_channel_3:
      return (volatile rx_mtu_channel_regs_t*)mtu3();
    case k_mtu_channel_4:
      return (volatile rx_mtu_channel_regs_t*)mtu4();
    case k_mtu_channel_6:
      return (volatile rx_mtu_channel_regs_t*)mtu6();
    case k_mtu_channel_7:
      return (volatile rx_mtu_channel_regs_t*)mtu7();
    default:
      return NULL;
  }
}

static bool internal_is_valid_channel(const rx_mtu_channel_t channel)
{
  return internal_get_mtu_base(channel) != NULL;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_encoder_init(const rx_encoder_config_t* config)
{
  rx_err_t                        err = k_rx_ok;
  volatile rx_mtu_channel_regs_t* mtu;

  /* Validate inputs */
  RX_VALIDATE_PTR(config, s_tag, "config pointer is NULL");

  const rx_mtu_channel_t channel = config->channel;

  if (config->counts_per_rev < k_encoder_min_counts_per_rev ||
      config->counts_per_rev > k_encoder_max_counts_per_rev) {
    rx_log_error(s_tag, "Invalid counts per revolution");
    return k_rx_err_invalid_arg;
  }

  mtu = internal_get_mtu_base(channel);
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

rx_err_t rx_encoder_read_raw(const rx_mtu_channel_t channel, uint16_t* count)
{
  volatile rx_mtu_channel_regs_t* mtu;

  RX_VALIDATE_PTR(count, s_tag, "count pointer is NULL");

  mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_encoder_initialized[channel], s_tag, "Encoder not initialized");

  *count = mtu->tcnt;
  return k_rx_ok;
}

rx_err_t rx_encoder_read_count(const rx_mtu_channel_t channel, rx_encoder_state_t* state)
{
  volatile rx_mtu_channel_regs_t* mtu;

  RX_VALIDATE_PTR(state, s_tag, "state pointer is NULL");

  mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_encoder_initialized[channel], s_tag, "Encoder not initialized");

  /* Read current counter value */
  const uint16_t current_count = mtu->tcnt;

  return internal_update_state_from_count(state, channel, current_count);
}

rx_err_t rx_encoder_read_velocity(float*                 velocity_rps,
                                  const float            delta_time_s,
                                  const rx_mtu_channel_t channel)
{
  volatile rx_mtu_channel_regs_t* mtu;
  rx_encoder_state_t              state;
  rx_err_t                        err;

  RX_VALIDATE_PTR(velocity_rps, s_tag, "velocity_rps pointer is NULL");

  /* Runtime validation to catch accidental parameter swaps */
  if (delta_time_s <= k_min_delta_time_s || delta_time_s > k_max_delta_time_s) {
    rx_log_error(s_tag, "Invalid delta time for velocity calculation");
    return k_rx_err_invalid_arg;
  }

  if (!internal_is_valid_channel(channel)) {
    rx_log_error(s_tag, "Invalid channel - possible parameter swap");
    return k_rx_err_invalid_arg;
  }

  mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_encoder_initialized[channel], s_tag, "Encoder not initialized");

  /* Read current count */
  err = internal_update_state_from_count(&state, channel, mtu->tcnt);
  if (err != k_rx_ok) {
    return err;
  }

  /* Calculate velocity based on count change */
  const int32_t delta_count = state.total_count - s_last_count[channel];
  s_last_count[channel]     = state.total_count;

  /* Convert to revolutions per second */
  const uint16_t counts_per_rev = s_counts_per_rev[channel];

  /* Guard division: Validate counts_per_rev is within acceptable range */
  if (counts_per_rev < k_encoder_min_counts_per_rev ||
      counts_per_rev > k_encoder_max_counts_per_rev) {
    rx_log_error(s_tag, "counts_per_rev out of valid range");
    return k_rx_err_invalid_state;
  }

  const float delta_revs = (float)delta_count / counts_per_rev;
  *velocity_rps          = delta_revs / delta_time_s;

  /* Post-condition: Validate velocity is realistic */
  if (fabsf(*velocity_rps) > k_max_realistic_velocity_rps) {
    rx_log_warn(s_tag, "Unrealistic velocity detected - possible encoder failure");
    /* Don't return error, just warn - allow for brief overspeed conditions */
  }

  return k_rx_ok;
}

rx_err_t rx_encoder_reset(const rx_mtu_channel_t channel)
{
  volatile rx_mtu_channel_regs_t* mtu;

  mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_encoder_initialized[channel], s_tag, "Encoder not initialized");

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

rx_err_t rx_encoder_set_count(const int32_t count, const rx_mtu_channel_t channel)
{
  volatile rx_mtu_channel_regs_t* mtu;

  mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return k_rx_err_invalid_arg;
  }

  RX_VALIDATE_INIT(s_encoder_initialized[channel], s_tag, "Encoder not initialized");

  /* Set hardware counter (limited to 16-bit) */
  mtu->tcnt = (uint16_t)(count & k_encoder_16bit_mask);

  /* Set software state */
  s_encoder_state[channel].total_count    = count;
  s_encoder_state[channel].last_raw_count = (uint16_t)(count & k_encoder_16bit_mask);
  s_last_count[channel]                   = count;

  /* Recalculate position */
  const uint16_t counts_per_rev = s_counts_per_rev[channel];
  if (counts_per_rev < k_encoder_min_counts_per_rev ||
      counts_per_rev > k_encoder_max_counts_per_rev) {
    rx_log_error(s_tag, "counts_per_rev out of valid range - state corrupted");
    return k_rx_err_invalid_state;
  }
  s_encoder_state[channel].revolutions = count / counts_per_rev;

  const int32_t remainder_counts = count % counts_per_rev;
  s_encoder_state[channel].position_deg =
    (float)(remainder_counts * k_degrees_per_revolution) / counts_per_rev;

  return k_rx_ok;
}

rx_err_t rx_encoder_deinit(const rx_mtu_channel_t channel)
{
  if (!internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  if (!s_encoder_initialized[channel]) {
    rx_log_error(s_tag, "Deinit called on uninitialized channel");
    return k_rx_err_invalid_state;
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
static rx_err_t internal_enable_mtu_module(const rx_mtu_channel_t channel)
{
  volatile rx_mtu_channel_regs_t* mtu;

  /* Pre-condition: channel must be within valid MTU channel enum range */
  if (channel > k_mtu_channel_7) {
    rx_log_error(s_tag, "Channel exceeds maximum MTU channel");
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition: channel must map to a valid MTU base address */
  mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    rx_log_error(s_tag, "Invalid MTU channel - no base address");
    return k_rx_err_invalid_arg;
  }

  /* Enable MTU module (clear module stop bit) */
  system_regs()->prcr = k_rx_prcr_unlock_all; /* Enable writes to MSTPCR (0xA50F) */

  if (channel <= k_mtu_channel_4) {
    system_regs()->mstpcra &= ~(1 << k_mtu_mstpcra_mtu0_4_bit); /* MTU0-MTU4 */
  } else {
    system_regs()->mstpcra &= ~(1 << k_mtu_mstpcra_mtu6_7_bit); /* MTU6-MTU7 */
  }

  system_regs()->prcr = k_rx_prcr_lock; /* Lock MSTPCR (0xA500) */

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
  if (mtu == NULL) {
    return k_rx_err_null_ptr;
  }

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

  /* Post-condition: Verify configuration was latched (NASA Rule 5)
   * Read back TCR and TMDR to ensure hardware accepted the writes */
  if (mtu->tcr != k_tcr_external_clock_no_prescaler) {
    rx_log_error(s_tag, "TCR configuration not latched");
    return k_rx_err_hw_init_failed;
  }

  if (mtu->tmdr != k_tmdr_phase_counting_mode_1) {
    rx_log_error(s_tag, "TMDR configuration not latched");
    return k_rx_err_hw_init_failed;
  }

  return k_rx_ok;
}

/**
 * @brief Verify timer configuration after initialization (NASA Rule 5: min 2 checks)
 *
 * Since encoder counting verification requires encoder motion which may not be
 * available at initialization time, this function validates the timer configuration
 * instead of actual counting.
 *
 * @param[in] mtu Pointer to MTU channel registers
 *
 * @return k_rx_ok if timer configuration is valid
 * @return k_rx_err_null_ptr if mtu is NULL
 * @return k_rx_err_hw_init_failed if timer mode is incorrectly configured
 */
static rx_err_t internal_verify_timer_counting(const volatile rx_mtu_channel_regs_t* mtu)
{
  /* Pre-condition: Validate input pointer (Rule 5 check 1) */
  if (mtu == NULL) {
    return k_rx_err_null_ptr;
  }

  /* Post-condition: Verify timer mode register is configured for phase counting (Rule 5 check 2)
   * TMDR should be set to k_tmdr_phase_counting_mode_1 (0x04) after initialization.
   * This confirms the hardware accepted our configuration. */
  if (mtu->tmdr != k_tmdr_phase_counting_mode_1) {
    rx_log_error(s_tag, "Timer mode register not configured for phase counting");
    return k_rx_err_hw_init_failed;
  }

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
static rx_err_t internal_initialize_encoder_state(const rx_mtu_channel_t     channel,
                                                  const rx_encoder_config_t* config)
{
  if (config == NULL || !internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  /* Validate counts_per_rev is within acceptable range */
  if (config->counts_per_rev < k_encoder_min_counts_per_rev ||
      config->counts_per_rev > k_encoder_max_counts_per_rev) {
    rx_log_error(s_tag, "counts_per_rev out of valid range");
    return k_rx_err_invalid_arg;
  }

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

static rx_err_t internal_update_state_from_count(rx_encoder_state_t*    state,
                                                 const rx_mtu_channel_t channel,
                                                 const uint16_t         current_count)
{
  if (!internal_is_valid_channel(channel) || state == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition: Ensure encoder is initialized (NASA Rule 5 compliance) */
  if (!s_encoder_initialized[channel]) {
    rx_log_error(s_tag, "Encoder not initialized");
    return k_rx_err_invalid_state;
  }

  const uint16_t last_count = s_encoder_state[channel].last_raw_count;

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
  if (delta > (int32_t)k_encoder_counter_half) {
    /* Large positive delta means we actually went backwards */
    delta = delta - (int32_t)k_encoder_counter_max;
  }

  /* Invert direction if configured */
  if (s_invert_direction[channel]) {
    delta = -delta;
  }

  /* Update total count */
  s_encoder_state[channel].total_count += delta;
  s_encoder_state[channel].last_raw_count = current_count;

  /* Calculate revolutions and position */
  const uint16_t counts_per_rev = s_counts_per_rev[channel];

  /* Guard division: Validate counts_per_rev is within acceptable range (NASA Rule 5) */
  if (counts_per_rev < k_encoder_min_counts_per_rev ||
      counts_per_rev > k_encoder_max_counts_per_rev) {
    rx_log_error(s_tag, "counts_per_rev out of valid range");
    return k_rx_err_invalid_state;
  }

  s_encoder_state[channel].revolutions = s_encoder_state[channel].total_count / counts_per_rev;

  const int32_t remainder_counts = s_encoder_state[channel].total_count % counts_per_rev;
  s_encoder_state[channel].position_deg =
    (float)(remainder_counts * k_degrees_per_revolution) / counts_per_rev;

  /* Post-condition: Validate position is within reasonable range
   * Note: Position can be negative for backward counts, so we check absolute value.
   * Position beyond ±720° would indicate a calculation error. */
  if (fabsf(s_encoder_state[channel].position_deg) >
      (k_turns_multiplier * k_degrees_per_revolution)) {
    rx_log_error(s_tag, "Position calculation overflow - exceeds ±720°");
    return k_rx_err_out_of_range;
  }

  /* Copy to output */
  *state = s_encoder_state[channel];

  return k_rx_ok;
}
