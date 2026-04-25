/**
 * @file rx_encoder_tpu.c
 * @brief TPU Quadrature Encoder Driver Implementation (Phase Counting Mode)
 *
 * @details
 * Hardware-accelerated quadrature encoder position tracking using the RX72N
 * TPU peripheral in phase counting mode for rear wheel encoders (motors 2-3).
 * The algorithm mirrors rx_mtu_encoder.c: 4x decoding, 16-bit overflow
 * detection, 32-bit accumulated count, and velocity calculation.
 *
 * ## Architecture
 *
 * ```
 * motor_control_task.c  (250 Hz PID loop)
 *         |
 * rx_encoder_tpu.c      (THIS MODULE: overflow + velocity)
 *         |
 * rx_tpu.c              (HAL: register access)
 *         |
 * rx72n_tpu_regs.h      (memory-mapped I/O)
 * ```
 *
 * ## Overflow Detection Algorithm
 *
 * Same as MTU encoder - see rx_mtu_encoder.c for detailed derivation.
 * Uses signed 16-bit delta with half-range threshold (32768) to distinguish
 * forward wraps from reverse motion.
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto, setjmp, recursion
 * - Rule 3: No dynamic memory (static arrays only)
 * - Rule 4: All functions < 60 lines
 * - Rule 5: 2+ validation checks per function
 * - Rule 7: All return values checked
 * - Rule 8: C23 typed enums, no magic numbers
 * - Rule 10: -Wall -Wextra -Werror clean
 *
 * @see rx_encoder_tpu.h Public API
 * @see rx_mtu_encoder.c MTU encoder (front wheels, same algorithm)
 * @see rx_tpu.h TPU HAL driver
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_encoder_tpu.h"

#include <math.h>

#include "rx_check.h"
#include "rx_log.h"
#include "rx_tpu.h"

static const char* const s_tag = "TPU_ENCODER";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief TPU encoder channel mapping constants */
typedef enum : uint8_t {
  k_tpu_enc_max_channels = 6, /**< Max TPU channel index + 1 */
} tpu_enc_channel_limits_t;

/** @brief 16-bit counter overflow detection constants */
typedef enum : uint32_t {
  k_tpu_enc_counter_max  = 65536,  /**< 16-bit counter range */
  k_tpu_enc_counter_half = 32768,  /**< Half-range for wrap detection */
  k_tpu_enc_16bit_mask   = 0xFFFF, /**< 16-bit mask */
} tpu_enc_counter_t;

/** @brief Encoder parameter limits */
typedef enum : uint16_t {
  k_tpu_enc_min_counts_per_rev = 1,   /**< Minimum valid counts per rev */
  k_tpu_enc_count_reset        = 0,   /**< Counter reset value */
  k_tpu_enc_degrees_per_rev    = 360, /**< Degrees in one revolution */
  k_tpu_enc_turns_multiplier   = 2,   /**< +-720 deg range check */
} tpu_enc_params_t;

/** @brief Velocity calculation limits */
typedef enum : uint16_t {
  k_tpu_enc_max_velocity_rps = 100, /**< Max realistic velocity (6000 RPM) */
} tpu_enc_velocity_t;

/* Float constants (enums can't hold floats) */
static const float k_tpu_enc_initial_position_deg = 0.0F;
static const float k_tpu_enc_min_delta_time_s     = 0.0F;
static const float k_tpu_enc_max_delta_time_s     = 10.0F;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/** @brief Per-channel initialization flag */
static bool s_initialized[k_tpu_enc_max_channels] = {false};

/** @brief Per-channel encoder state (accumulated count, position) */
static rx_encoder_state_t s_state[k_tpu_enc_max_channels] = {};

/** @brief Per-channel counts per revolution */
static uint16_t s_counts_per_rev[k_tpu_enc_max_channels] = {};

/** @brief Per-channel direction inversion flag */
static bool s_invert_direction[k_tpu_enc_max_channels] = {false};

/** @brief Per-channel last total count for velocity delta */
static int32_t s_last_count[k_tpu_enc_max_channels] = {};

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Check if a TPU channel index is valid for encoder use
 *
 *
 *
 * @since Version 1.0.0
 */
static bool internal_is_valid_channel(const rx_tpu_channel_t channel)
{
  return (bool)(channel == k_tpu_channel_1 || channel == k_tpu_channel_2 ||
                channel == k_tpu_channel_4 || channel == k_tpu_channel_5);
}

/**
 * @brief Update encoder state from a raw 16-bit counter reading
 *
 * @details
 * Implements the 16-bit overflow detection algorithm. Calculates the
 * signed delta from the previous reading, detects wraps using the
 * half-range threshold, applies direction inversion, and accumulates
 * into the 32-bit total count. Also derives revolutions and degrees.
 *
 *
 *
 * @pre s_initialized[channel] == true
 * @post state filled with updated values
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_update_state(rx_encoder_state_t*    state,
                                                  const rx_tpu_channel_t channel,
                                                  const uint16_t         current_count)
{
  if (!internal_is_valid_channel(channel) || state == nullptr) {
    return k_rx_err_invalid_arg;
  }

  if (!s_initialized[channel]) {
    rx_log_error(s_tag, "Encoder not initialized");
    return k_rx_err_invalid_state;
  }

  const uint16_t last_raw = s_state[channel].last_raw_count;

  /* Calculate delta with 16-bit wraparound handling */
  int32_t delta;
  if (current_count >= last_raw) {
    delta = current_count - last_raw;
  } else {
    delta = (int32_t)k_tpu_enc_counter_max - (int32_t)last_raw + (int32_t)current_count;
  }

  /* Detect reverse wraparound */
  if (delta > (int32_t)k_tpu_enc_counter_half) {
    delta = delta - (int32_t)k_tpu_enc_counter_max;
  }

  /* Apply direction inversion */
  if (s_invert_direction[channel]) {
    delta = -delta;
  }

  /* Accumulate into 32-bit count */
  s_state[channel].total_count += delta;
  s_state[channel].last_raw_count = current_count;

  /* Derive revolutions and position */
  const uint16_t cpr = s_counts_per_rev[channel];
  if (cpr < k_tpu_enc_min_counts_per_rev) {
    rx_log_error(s_tag, "counts_per_rev corrupted (zero)");
    return k_rx_err_invalid_state;
  }

  s_state[channel].revolutions = s_state[channel].total_count / cpr;

  const int32_t remainder = s_state[channel].total_count % cpr;
  s_state[channel].position_deg =
    ((float)remainder * (float)k_tpu_enc_degrees_per_rev) / (float)cpr;

  /* Post-condition: position sanity check (remainder = total % cpr, so |pos| < 360 always) */

  *state = s_state[channel];
  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_tpu_encoder_init(const rx_tpu_encoder_config_t* config)
{
  RX_VALIDATE_PTR(config, s_tag, "config pointer is nullptr");

  const rx_tpu_channel_t channel = config->channel;

  if (!internal_is_valid_channel(channel)) {
    rx_log_error(s_tag, "Invalid TPU channel for encoder");
    return k_rx_err_invalid_arg;
  }

  if (config->counts_per_rev < k_tpu_enc_min_counts_per_rev) {
    rx_log_error(s_tag, "counts_per_rev must be >= 1");
    return k_rx_err_invalid_arg;
  }

  if (s_initialized[channel]) {
    rx_log_error(s_tag, "Channel already initialized");
    return k_rx_err_invalid_state;
  }

  /* Configure TPU HAL for phase counting mode 4 (4x resolution) */
  const rx_tpu_config_t tpu_config = {
    .channel    = channel,
    .phase_mode = k_tpu_phase_mode_4,
  };

  rx_err_t err = rx_tpu_init_phase_count(&tpu_config);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "TPU HAL init failed");
    return err;
  }

  /* Start the counter (must succeed: channel was just initialized above) */
  (void)rx_tpu_start(channel);

  /* Initialize software state */
  s_state[channel].total_count    = 0;
  s_state[channel].last_raw_count = 0;
  s_state[channel].revolutions    = 0;
  s_state[channel].position_deg   = k_tpu_enc_initial_position_deg;

  s_counts_per_rev[channel]   = config->counts_per_rev;
  s_invert_direction[channel] = config->invert_direction;
  s_last_count[channel]       = 0;
  s_initialized[channel]      = true;

  rx_log_info(s_tag, "TPU encoder initialized");

  return k_rx_ok;
}

/**
 * @brief Read the raw 16-bit hardware counter for a TPU encoder channel
 *
 * @details
 * Bypasses overflow accumulation and returns the current TPU TCNT register
 * directly via rx_tpu_read_count().  Useful for diagnostic dumps and
 * low-level wraparound detection.  Validates the channel index and the
 * per-channel initialized flag before touching hardware.
 *
 *
 *
 * @pre count != nullptr.
 * @pre Channel was successfully initialized via rx_tpu_encoder_init().
 *
 * @post On k_rx_ok: *count holds the latched TPU counter value.
 * @post No TPU register write side effects.
 *
 * @note Not thread-safe with rx_tpu_encoder_reset() / _set_count() on the
 *       same channel.  Concurrent reads on different channels are safe.
 *
 * @see rx_tpu_encoder_read_count()    Cooked count with overflow tracking.
 * @see rx_tpu_encoder_read_velocity() Velocity computation from delta.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_encoder_read_raw(const rx_tpu_channel_t channel, uint16_t* count)
{
  RX_VALIDATE_PTR(count, s_tag, "count pointer is nullptr");

  if (!internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  if (!s_initialized[channel]) {
    rx_log_error(s_tag, "Encoder not initialized");
    return k_rx_err_invalid_state;
  }

  return rx_tpu_read_count(channel, count);
}

/**
 * @brief Read the cooked encoder state with overflow tracking
 *
 * @details
 * Reads the raw TPU counter, then folds it through internal_update_state(),
 * which extends the 16-bit hardware counter into a 32-bit signed total
 * count by detecting wraparound between successive reads.  Position in
 * degrees and revolution count are derived from total_count and
 * counts_per_rev.
 *
 *
 *
 * @pre state != nullptr.
 * @pre Channel was successfully initialized via rx_tpu_encoder_init().
 *
 * @post On k_rx_ok: *state reflects the latest TPU position; internal
 *       last_raw_count cache is updated for next overflow detection.
 * @post On error paths: *state is unspecified.
 *
 * @note Not thread-safe with rx_tpu_encoder_reset() / _set_count() on the
 *       same channel because both update internal cached state.
 *
 * @see rx_tpu_encoder_read_raw()      Raw TCNT (no overflow tracking).
 * @see rx_tpu_encoder_read_velocity() Velocity computation.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_encoder_read_count(const rx_tpu_channel_t channel, rx_encoder_state_t* state)
{
  RX_VALIDATE_PTR(state, s_tag, "state pointer is nullptr");

  if (!internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  if (!s_initialized[channel]) {
    rx_log_error(s_tag, "Encoder not initialized");
    return k_rx_err_invalid_state;
  }

  /* Read current 16-bit counter from TPU HAL (register read; must succeed) */
  uint16_t current_count = 0;
  (void)(rx_tpu_read_count(channel, &current_count));

  return internal_update_state(state, channel, current_count);
}

/**
 * @brief Compute encoder angular velocity in revolutions/second
 *
 * @details
 * Reads the latest cooked count, computes the delta against the cached
 * s_last_count[channel] from the previous call, divides by counts_per_rev,
 * and divides by delta_time_s to get rev/s.  Updates the cached previous
 * count so successive calls produce successive deltas.  Logs a warning
 * (but still returns k_rx_ok) when |velocity| exceeds
 * k_tpu_enc_max_velocity_rps, indicating a likely encoder fault.
 *
 *
 *
 * @pre velocity_rps != nullptr.
 * @pre Channel was successfully initialized via rx_tpu_encoder_init().
 * @pre delta_time_s matches the period since the previous call to keep
 *      the velocity self-consistent.
 *
 * @post On k_rx_ok: *velocity_rps holds the computed value;
 *       s_last_count[channel] advanced to the current total_count.
 * @post On error: *velocity_rps and cached state are unspecified.
 *
 * @note Not thread-safe with itself or other encoder mutators on the same
 *       channel because of the shared s_last_count[] cache.
 *
 * @see rx_tpu_encoder_read_count() Underlying cooked-count reader.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_encoder_read_velocity(float*                 velocity_rps,
                                      const float            delta_time_s,
                                      const rx_tpu_channel_t channel)
{
  RX_VALIDATE_PTR(velocity_rps, s_tag, "velocity_rps pointer is nullptr");

  const bool dt_too_small = (bool)(delta_time_s <= k_tpu_enc_min_delta_time_s);
  const bool dt_too_large = (bool)(delta_time_s > k_tpu_enc_max_delta_time_s);
  if ((bool)((int)dt_too_small | (int)dt_too_large)) {
    rx_log_error(s_tag, "Invalid delta time for velocity calculation");
    return k_rx_err_invalid_arg;
  }

  if (!internal_is_valid_channel(channel)) {
    rx_log_error(s_tag, "Invalid channel");
    return k_rx_err_invalid_arg;
  }

  if (!s_initialized[channel]) {
    rx_log_error(s_tag, "Encoder not initialized");
    return k_rx_err_invalid_state;
  }

  /* Read current count via overflow handler (register read; must succeed) */
  uint16_t raw_count = 0;
  (void)rx_tpu_read_count(channel, &raw_count);

  rx_encoder_state_t state;
  const rx_err_t     err = internal_update_state(&state, channel, raw_count);
  if (err != k_rx_ok) {
    return err;
  }

  /* Calculate velocity from count delta */
  const int32_t delta_count = state.total_count - s_last_count[channel];
  s_last_count[channel]     = state.total_count;

  /* cpr was validated by internal_update_state above; post-condition guard */
  const uint16_t cpr = s_counts_per_rev[channel];

  const float delta_revs = (float)delta_count / (float)cpr;
  *velocity_rps          = delta_revs / delta_time_s;

  /* Warn on unrealistic velocity */
  if (fabsf(*velocity_rps) > k_tpu_enc_max_velocity_rps) {
    rx_log_warn(s_tag, "Unrealistic velocity - possible encoder fault");
  }

  return k_rx_ok;
}

/**
 * @brief Reset both hardware and software encoder state to zero
 *
 * @details
 * Calls rx_tpu_reset_count() to clear the TPU TCNT register, then zeros
 * the cached software state (total_count, last_raw_count, revolutions,
 * position_deg, s_last_count).  Used after carriage homing / index pulses
 * or when re-initializing a faulted encoder.
 *
 *
 *
 * @pre Channel was successfully initialized via rx_tpu_encoder_init().
 *
 * @post On k_rx_ok: TPU TCNT register == 0; cached software state == 0.
 *       Subsequent reads will report position_deg == 0 with revolutions
 *       == 0.
 * @post On error: state unchanged.
 *
 * @note Not thread-safe with concurrent reads on the same channel.
 *
 * @see rx_tpu_encoder_set_count() Reset to a non-zero starting count.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_encoder_reset(const rx_tpu_channel_t channel)
{
  if (!internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  if (!s_initialized[channel]) {
    rx_log_error(s_tag, "Encoder not initialized");
    return k_rx_err_invalid_state;
  }

  /* Reset TPU hardware counter (register write; must succeed) */
  (void)(rx_tpu_reset_count(channel));

  /* Reset software state */
  s_state[channel].total_count    = 0;
  s_state[channel].last_raw_count = 0;
  s_state[channel].revolutions    = 0;
  s_state[channel].position_deg   = k_tpu_enc_initial_position_deg;
  s_last_count[channel]           = 0;

  return k_rx_ok;
}

/**
 * @brief Force the cached encoder count to a specific 32-bit value
 *
 * @details
 * Software-only counterpart to rx_tpu_encoder_reset(): does NOT touch the
 * TPU TCNT register.  Sets the cached total_count, last_raw_count,
 * revolutions, and position_deg from the supplied count and the channel's
 * counts_per_rev.  Useful for tests, calibration, and resuming from a
 * known offset.  Detects a corrupted counts_per_rev (< minimum) and
 * returns k_rx_err_invalid_state.
 *
 *
 *
 * @pre Channel was successfully initialized via rx_tpu_encoder_init().
 *
 * @post On k_rx_ok: cached state reflects count; revolutions =
 *       count / counts_per_rev; position_deg derived accordingly.
 * @post Hardware TCNT register is NOT updated.
 *
 * @note Not thread-safe with concurrent reads on the same channel.
 *
 * @see rx_tpu_encoder_reset() Hardware-and-software reset to zero.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_encoder_set_count(const int32_t count, const rx_tpu_channel_t channel)
{
  if (!internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  if (!s_initialized[channel]) {
    rx_log_error(s_tag, "Encoder not initialized");
    return k_rx_err_invalid_state;
  }

  /* Set software state */
  s_state[channel].total_count    = count;
  s_state[channel].last_raw_count = (uint16_t)((uint32_t)count & k_tpu_enc_16bit_mask);
  s_last_count[channel]           = count;

  /* Recalculate position */
  const uint16_t cpr = s_counts_per_rev[channel];
  if (cpr < k_tpu_enc_min_counts_per_rev) {
    rx_log_error(s_tag, "counts_per_rev corrupted");
    return k_rx_err_invalid_state;
  }

  s_state[channel].revolutions = count / cpr;

  const int32_t remainder = count % cpr;
  s_state[channel].position_deg =
    ((float)remainder * (float)k_tpu_enc_degrees_per_rev) / (float)cpr;

  return k_rx_ok;
}

/**
 * @brief Deinitialize a TPU encoder channel and release its resources
 *
 * @details
 * Calls rx_tpu_deinit() to stop the TPU counter and reset its mode, then
 * clears the per-channel s_initialized flag and counts_per_rev.
 * Subsequent reads on this channel will return k_rx_err_invalid_state
 * until rx_tpu_encoder_init() is called again.
 *
 *
 *
 * @pre Channel was previously initialized via rx_tpu_encoder_init().
 *
 * @post On k_rx_ok: TPU counter stopped, s_initialized[channel] == false,
 *       s_counts_per_rev[channel] == 0.
 * @post On error: state unchanged.
 *
 * @note Not thread-safe with concurrent reads on the same channel.
 *       Typically called only at shutdown or before re-initialization.
 *
 * @see rx_tpu_encoder_init() Required to re-arm after deinit.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_encoder_deinit(const rx_tpu_channel_t channel)
{
  if (!internal_is_valid_channel(channel)) {
    return k_rx_err_invalid_arg;
  }

  if (!s_initialized[channel]) {
    rx_log_error(s_tag, "Deinit called on uninitialized channel");
    return k_rx_err_invalid_state;
  }

  /* Deinitialize TPU HAL (stops counter, resets mode) */
  (void)rx_tpu_deinit(channel);

  s_initialized[channel]    = false;
  s_counts_per_rev[channel] = 0;

  rx_log_info(s_tag, "TPU encoder deinitialized");

  return k_rx_ok;
}

#ifdef UNIT_TEST
/**
 * @brief Corrupt counts_per_rev to 0 for a channel (test-only)
 *
 * @details
 * Bypasses the init validation to set s_counts_per_rev[channel] = 0, enabling
 * tests to exercise the runtime cpr-corrupted guard paths in
 * internal_update_state(), rx_tpu_encoder_read_velocity(), and
 * rx_tpu_encoder_set_count(). Available only in UNIT_TEST builds.
 *
 *
 * @pre channel must be a valid index (1, 2, 4, or 5)
 * @post s_counts_per_rev[channel] == 0
 *
 * @note Test-only; not compiled into production firmware
 * @since Version 1.0.0
 */
void rx_tpu_encoder_test_corrupt_cpr(rx_tpu_channel_t channel)
{
  if ((uint8_t)channel < k_tpu_enc_max_channels) {
    s_counts_per_rev[channel] = 0U;
  }
}
#endif /* UNIT_TEST */
