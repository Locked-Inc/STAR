/* lib/rx_hal/inc/rx_cmt.h */

/**
 * @file rx_cmt.h
 * @brief CMT (Compare Match Timer) Driver for RX72N
 * @details
 * Compare Match Timer driver for periodic interrupts.
 *
 * The RX72N has 4 CMT channels (CMT0-CMT3):
 * - CMT0: Used by ThreadX for system tick (100Hz)
 * - CMT1: Available for motor control loop (250Hz)
 * - CMT2-CMT3: Available for other uses
 *
 * CMT Features:
 * - 16-bit counter with compare match
 * - Clock source: PCLKB/8, /32, /128, /512
 * - Generates periodic interrupts on compare match
 * - Auto-reloads counter for continuous operation
 *
 * For 250Hz with PCLKB=60MHz and /128 divider:
 * - Counter clock = 60MHz/128 = 468.75kHz
 * - Period = 468750/250 = 1875 counts
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_CMT_H
#define STAR_RX72N_CMT_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief CMT channel identifiers
 */
typedef enum {
  k_cmt_channel_0 = 0, /**< CMT0 (used by ThreadX) */
  k_cmt_channel_1 = 1, /**< CMT1 (motor control) */
  k_cmt_channel_2 = 2, /**< CMT2 (available) */
  k_cmt_channel_3 = 3, /**< CMT3 (available) */
} rx_cmt_channel_t;

/**
 * @brief CMT interrupt callback function type
 *
 * @param[in] user_data User data provided during init
 */
typedef void (*rx_cmt_callback_t)(void* user_data);

/**
 * @brief CMT configuration
 */
typedef struct {
  uint32_t          frequency_hz; /**< Desired interrupt frequency in Hz */
  rx_cmt_callback_t callback;     /**< Interrupt callback function */
  void*             user_data;    /**< User data passed to callback */
  uint8_t           priority;     /**< Interrupt priority (0-15) */
} rx_cmt_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize CMT channel for periodic interrupts
 *
 * Configures CMT channel for specified frequency and enables interrupts.
 * Timer starts immediately after init.
 *
 * @param[in] channel CMT channel (1-3, CMT0 reserved for ThreadX)
 * @param[in] config CMT configuration
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if config is NULL
 * @return k_rx_err_invalid_arg if channel, frequency, or priority is invalid
 * @return k_rx_err_conflict if channel is CMT0 (reserved)
 */
rx_err_t rx_cmt_init(rx_cmt_channel_t channel, const rx_cmt_config_t* config);

/**
 * @brief Start CMT timer
 *
 * Starts the timer counter and interrupt generation.
 * Timer is automatically started during init.
 *
 * @param[in] channel CMT channel
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_cmt_start(rx_cmt_channel_t channel);

/**
 * @brief Stop CMT timer
 *
 * Stops the timer counter and interrupt generation.
 * Counter value is preserved.
 *
 * @param[in] channel CMT channel
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t rx_cmt_stop(rx_cmt_channel_t channel);

/**
 * @brief Get CMT counter value
 *
 * @param[in] channel CMT channel
 * @param[out] count Pointer to store counter value
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if count is NULL
 * @return k_rx_err_invalid_arg if channel is invalid
 * @return k_rx_err_invalid_state if channel not initialized
 */
rx_err_t rx_cmt_get_count(rx_cmt_channel_t channel, uint16_t* count);

/**
 * @brief Deinitialize CMT channel
 *
 * Stops timer, disables interrupts, and releases resources.
 *
 * @param[in] channel CMT channel
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if channel is invalid
 */
rx_err_t rx_cmt_deinit(rx_cmt_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_CMT_H */
