#include <stddef.h>
/* src/drivers/rx_mtu_encoder.c */

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
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_log.h"
#include "rx_mtu_encoder.h"

static const char* s_tag = "MTU_ENCODER";

/* =============================================================================
 * Constants
 * =============================================================================
 */

typedef enum {
  k_encoder_max_channels       = 7,
  k_tmdr_phase_counting_mode_1 = 0x04, /* Phase counting mode 1 (4x decoding) */
} encoder_constants_t;

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

/**
 * @brief Get MTU channel base address
 *
 * @param[in] channel MTU channel
 *
 * @return Pointer to MTU register base, or NULL if invalid
 */
static volatile MTU_Channel_Type* internal_get_mtu_base(rx_mtu_channel_t channel)
{
  switch (channel) {
    case k_mtu_channel_0:
      return (volatile MTU_Channel_Type*)MTU0_BASE;
    case k_mtu_channel_1:
      return (volatile MTU_Channel_Type*)MTU1_BASE;
    case k_mtu_channel_2:
      return (volatile MTU_Channel_Type*)MTU2_BASE;
    case k_mtu_channel_3:
      return (volatile MTU_Channel_Type*)((volatile uint8_t*)MTU3_BASE);
    case k_mtu_channel_4:
      return (volatile MTU_Channel_Type*)((volatile uint8_t*)MTU4_BASE);
    case k_mtu_channel_6:
      return (volatile MTU_Channel_Type*)MTU6_BASE;
    case k_mtu_channel_7:
      return (volatile MTU_Channel_Type*)MTU7_BASE;
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
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  rx_mtu_channel_t channel = config->channel;

  if ((int32_t)channel >= k_encoder_max_channels) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  if (config->counts_per_rev == 0) {
    RX_LOG_ERROR(s_tag, "Invalid counts per revolution");
    return RX_ERR_INVALID_ARG;
  }

  volatile MTU_Channel_Type* mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  RX_LOG_INFO(s_tag, "Info");

  /* Enable MTU module (clear module stop bit) */
  SYSTEM.PRCR = 0xA50B; /* Enable writes to MSTPCR */

  if (channel <= k_mtu_channel_4) {
    SYSTEM.MSTPCRA &= ~(1 << 9); /* MTU0-MTU4 */
  } else {
    SYSTEM.MSTPCRA &= ~(1 << 8); /* MTU6-MTU7 */
  }

  SYSTEM.PRCR = 0xA500; /* Lock MSTPCR */

  /* Stop timer before configuration */
  rx_mtu_stop(channel);

  /* Configure Timer Control Register (TCR)
   * - No prescaler (count directly on phase inputs)
   * - External clock on MTCLKA/B
   */
  mtu->TCR = 0x00; /* External clock, no prescaler */

  /* Configure Phase Counting Mode 1 (4x decoding)
   * TMDR.MD = 0100
   */
  mtu->TMDR = k_tmdr_phase_counting_mode_1;

  /* Configure I/O control (not used in phase counting mode) */
  mtu->TIORH = 0x00;
  mtu->TIORL = 0x00;

  /* Clear counter */
  mtu->TCNT = 0;

  /* Initialize state */
  s_encoder_state[channel].total_count    = 0;
  s_encoder_state[channel].last_raw_count = 0;
  s_encoder_state[channel].revolutions    = 0;
  s_encoder_state[channel].position_deg   = 0.0f;

  s_counts_per_rev[channel]      = config->counts_per_rev;
  s_invert_direction[channel]    = config->invert_direction;
  s_last_count[channel]          = 0;
  s_encoder_initialized[channel] = true;

  /* Start counter */
  rx_mtu_start(channel);

  RX_LOG_INFO(s_tag, "Info");

  return RX_OK;
}

rx_err_t rx_encoder_read_raw(rx_mtu_channel_t channel, uint16_t* count)
{
  RX_CHECK_NULL_PTR(count, s_tag, "count pointer is NULL");

  if ((int32_t)channel >= k_encoder_max_channels || !s_encoder_initialized[channel]) {
    return RX_ERR_INVALID_STATE;
  }

  volatile MTU_Channel_Type* mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  *count = mtu->TCNT;
  return RX_OK;
}

rx_err_t rx_encoder_read_count(rx_mtu_channel_t channel, rx_encoder_state_t* state)
{
  RX_CHECK_NULL_PTR(state, s_tag, "state pointer is NULL");

  if ((int32_t)channel >= k_encoder_max_channels || !s_encoder_initialized[channel]) {
    return RX_ERR_INVALID_STATE;
  }

  volatile MTU_Channel_Type* mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  /* Read current counter value */
  uint16_t current_count = mtu->TCNT;
  uint16_t last_count    = s_encoder_state[channel].last_raw_count;

  /* Calculate delta (handling 16-bit wraparound) */
  int32_t delta;
  if (current_count >= last_count) {
    /* Normal case: no wraparound */
    delta = current_count - last_count;
  } else {
    /* Wraparound occurred */
    delta = (65536 - last_count) + current_count;
  }

  /* Check for reverse wraparound */
  if (delta > 32768) {
    /* Large positive delta means we actually went backwards */
    delta = delta - 65536;
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

  int32_t remainder_counts              = s_encoder_state[channel].total_count % counts_per_rev;
  s_encoder_state[channel].position_deg = (float)(remainder_counts * 360.0f) / counts_per_rev;

  /* Copy to output */
  *state = s_encoder_state[channel];

  return RX_OK;
}

rx_err_t rx_encoder_read_velocity(rx_mtu_channel_t channel, float delta_time_s, float* velocity_rps)
{
  RX_CHECK_NULL_PTR(velocity_rps, s_tag, "velocity_rps pointer is NULL");

  if ((int32_t)channel >= k_encoder_max_channels || !s_encoder_initialized[channel]) {
    return RX_ERR_INVALID_STATE;
  }

  if (delta_time_s <= 0.0f) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  /* Read current count */
  rx_encoder_state_t state;
  rx_err_t           err = rx_encoder_read_count(channel, &state);
  if (err != RX_OK) {
    return err;
  }

  /* Calculate velocity based on count change */
  int32_t delta_count   = state.total_count - s_last_count[channel];
  s_last_count[channel] = state.total_count;

  /* Convert to revolutions per second */
  uint16_t counts_per_rev = s_counts_per_rev[channel];
  float    delta_revs     = (float)delta_count / counts_per_rev;
  *velocity_rps           = delta_revs / delta_time_s;

  return RX_OK;
}

rx_err_t rx_encoder_reset(rx_mtu_channel_t channel)
{
  if ((int32_t)channel >= k_encoder_max_channels || !s_encoder_initialized[channel]) {
    return RX_ERR_INVALID_STATE;
  }

  volatile MTU_Channel_Type* mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  /* Reset hardware counter */
  mtu->TCNT = 0;

  /* Reset state */
  s_encoder_state[channel].total_count    = 0;
  s_encoder_state[channel].last_raw_count = 0;
  s_encoder_state[channel].revolutions    = 0;
  s_encoder_state[channel].position_deg   = 0.0f;
  s_last_count[channel]                   = 0;

  return RX_OK;
}

rx_err_t rx_encoder_set_count(rx_mtu_channel_t channel, int32_t count)
{
  if ((int32_t)channel >= k_encoder_max_channels || !s_encoder_initialized[channel]) {
    return RX_ERR_INVALID_STATE;
  }

  volatile MTU_Channel_Type* mtu = internal_get_mtu_base(channel);
  if (mtu == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  /* Set hardware counter (limited to 16-bit) */
  mtu->TCNT = (uint16_t)(count & 0xFFFF);

  /* Set software state */
  s_encoder_state[channel].total_count    = count;
  s_encoder_state[channel].last_raw_count = (uint16_t)(count & 0xFFFF);
  s_last_count[channel]                   = count;

  /* Recalculate position */
  uint16_t counts_per_rev              = s_counts_per_rev[channel];
  s_encoder_state[channel].revolutions = count / counts_per_rev;

  int32_t remainder_counts              = count % counts_per_rev;
  s_encoder_state[channel].position_deg = (float)(remainder_counts * 360.0f) / counts_per_rev;

  return RX_OK;
}

rx_err_t rx_encoder_deinit(rx_mtu_channel_t channel)
{
  if ((int32_t)channel >= k_encoder_max_channels) {
    return RX_ERR_INVALID_ARG;
  }

  /* Stop timer */
  rx_mtu_stop(channel);

  /* Mark as uninitialized */
  s_encoder_initialized[channel] = false;
  s_counts_per_rev[channel]      = 0;

  RX_LOG_INFO(s_tag, "MTU encoder deinitialized");

  return RX_OK;
}
