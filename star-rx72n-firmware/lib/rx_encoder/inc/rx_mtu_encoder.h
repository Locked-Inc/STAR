/* lib/rx_encoder/inc/rx_mtu_encoder.h */

/**
 * @file rx_mtu_encoder.h
 * @brief MTU Encoder Driver for RX72N (Phase Counting Mode)
 * @details
 * Quadrature encoder interface using MTU phase counting mode.
 *
 * The MTU can directly count quadrature encoder signals without CPU intervention.
 * This provides hardware-based encoder counting with:
 * - 4x decoding (count on all edges of both channels)
 * - 16-bit counter (0-65535)
 * - Automatic overflow/underflow handling
 * - No CPU overhead
 *
 * Phase Counting Mode Features:
 * - MTCLKA: Phase A input
 * - MTCLKB: Phase B input
 * - Counter increments/decrements based on phase relationship
 * - 4x resolution (counts all edges)
 *
 * For 341 PPR encoders with 4x decoding:
 * - 1364 counts per revolution
 * - Max speed before overflow: (65536 / 1364) = 48 revolutions between reads
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_MTU_ENCODER_H
#define STAR_RX72N_MTU_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_mtu3a.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief Encoder configuration
 */
typedef struct {
  rx_mtu_channel_t channel;          /**< MTU channel for encoder */
  uint16_t         counts_per_rev;   /**< Encoder counts per revolution (after 4x) */
  bool             invert_direction; /**< Invert count direction */
} rx_encoder_config_t;

/**
 * @brief Encoder state
 */
typedef struct {
  int32_t  total_count;    /**< Total accumulated count (with overflow handling) */
  uint16_t last_raw_count; /**< Last raw counter value for overflow detection */
  int32_t  revolutions;    /**< Number of full revolutions */
  float    position_deg;   /**< Position in degrees */
} rx_encoder_state_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize MTU channel for encoder counting
 *
 * Configures MTU in phase counting mode for quadrature encoder.
 * Sets counter to 0 and starts counting automatically.
 *
 * @param[in] config Encoder configuration
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if config is NULL
 * @return k_rx_err_invalid_arg if channel or counts_per_rev is invalid
 */
rx_err_t rx_encoder_init(const rx_encoder_config_t* config);

/**
 * @brief Read raw encoder count
 *
 * Returns the current 16-bit counter value.
 * Does not handle overflow - use rx_encoder_read_count() for overflow handling.
 *
 * @param[in] channel MTU channel
 * @param[out] count Pointer to store raw count (0-65535)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if count is NULL
 * @return k_rx_err_invalid_arg if channel is invalid
 * @return k_rx_err_invalid_state if encoder not initialized
 */
rx_err_t rx_encoder_read_raw(rx_mtu_channel_t channel, uint16_t* count);

/**
 * @brief Read encoder count with overflow handling
 *
 * Reads counter and updates accumulated count handling 16-bit overflow.
 * Call this frequently (at least every 48 revolutions for 341 PPR encoder).
 *
 * @param[in] channel MTU channel
 * @param[out] state Pointer to encoder state structure
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if state is NULL
 * @return k_rx_err_invalid_arg if channel is invalid
 * @return k_rx_err_invalid_state if encoder not initialized
 */
rx_err_t rx_encoder_read_count(rx_mtu_channel_t channel, rx_encoder_state_t* state);

/**
 * @brief Read encoder velocity
 *
 * Calculates velocity based on count change over time period.
 * Assumes periodic calls at known rate (e.g., 250Hz control loop).
 *
 * @param[in] channel MTU channel
 * @param[in] delta_time_s Time since last call in seconds
 * @param[out] velocity_rps Pointer to store velocity in revolutions per second
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if velocity_rps is NULL
 * @return k_rx_err_invalid_arg if channel or delta_time_s is invalid
 * @return k_rx_err_invalid_state if encoder not initialized
 */
rx_err_t
rx_encoder_read_velocity(rx_mtu_channel_t channel, float delta_time_s, float* velocity_rps);

/**
 * @brief Reset encoder count to zero
 *
 * Resets hardware counter and accumulated count.
 *
 * @param[in] channel MTU channel
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 * @return k_rx_err_invalid_state if encoder not initialized
 */
rx_err_t rx_encoder_reset(rx_mtu_channel_t channel);

/**
 * @brief Set encoder count value
 *
 * Sets both hardware counter and accumulated count.
 * Useful for homing or calibration.
 *
 * @param[in] channel MTU channel
 * @param[in] count Count value to set
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 * @return k_rx_err_invalid_state if encoder not initialized
 */
rx_err_t rx_encoder_set_count(rx_mtu_channel_t channel, int32_t count);

/**
 * @brief Deinitialize encoder
 *
 * Stops counter and releases resources.
 *
 * @param[in] channel MTU channel
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t rx_encoder_deinit(rx_mtu_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MTU_ENCODER_H */
