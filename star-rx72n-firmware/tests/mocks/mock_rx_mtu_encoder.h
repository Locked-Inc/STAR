/* tests/mocks/mock_rx_mtu_encoder.h */

/**
 * @file mock_rx_mtu_encoder.h
 * @brief Mock MTU Encoder Hardware for Unit Testing
 *
 * @details
 * Mock implementation of MTU timer and system registers for encoder driver testing.
 * Provides counter simulation, overflow handling, and call tracking for comprehensive
 * unit testing without real hardware.
 *
 * Features:
 * - Simulates MTU counter behavior for phase counting mode
 * - Supports overflow/underflow simulation
 * - Tracks all MTU function calls
 * - No actual hardware access (runs on host)
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_MTU_ENCODER_H
#define MOCK_RX_MTU_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_mtu3a.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief Mock configuration constants
 */
typedef enum {
  k_mock_encoder_max_channels = 7, /**< Maximum MTU channels for encoders */
} mock_encoder_constants_t;

/* =============================================================================
 * Mock State Structure
 * =============================================================================
 */

/**
 * @brief Mock encoder hardware state for a single channel
 */
typedef struct {
  bool     initialized; /**< Channel is initialized */
  bool     running;     /**< Timer is running */
  uint16_t tcnt;        /**< Simulated timer counter value */
  uint8_t  tcr;         /**< Timer control register */
  uint8_t  tmdr;        /**< Timer mode register */
  uint8_t  tiorh;       /**< Timer I/O control register H */
  uint8_t  tiorl;       /**< Timer I/O control register L */
  uint32_t start_count; /**< Number of rx_mtu_start calls */
  uint32_t stop_count;  /**< Number of rx_mtu_stop calls */
} mock_encoder_channel_t;

/**
 * @brief Global mock encoder state
 */
typedef struct {
  bool                   initialized;                           /**< Mock is initialized */
  mock_encoder_channel_t channels[k_mock_encoder_max_channels]; /**< Channel states */
  uint32_t               mstpcra;           /**< Module stop control register A */
  uint16_t               prcr;              /**< Protection register */
  bool                   inject_init_error; /**< Inject init failure */
} mock_encoder_state_t;

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

/**
 * @brief Global mock instance for simple usage
 */
extern mock_encoder_state_t g_mock_encoder_state;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock encoder hardware state
 *
 * Resets all mock state to default values.
 */
void mock_encoder_init(void);

/**
 * @brief Deinitialize mock encoder hardware state
 */
void mock_encoder_deinit(void);

/**
 * @brief Reset mock state for a new test
 *
 * Clears call counts and channel states.
 */
void mock_encoder_reset(void);

/* =============================================================================
 * Counter Simulation Functions
 * =============================================================================
 */

/**
 * @brief Set simulated counter value for a channel
 *
 * @param[in] channel MTU channel
 * @param[in] count   Counter value to set (0-65535)
 */
void mock_encoder_set_counter(rx_mtu_channel_t channel, uint16_t count);

/**
 * @brief Get current simulated counter value
 *
 * @param[in] channel MTU channel
 *
 * @return Current counter value
 */
uint16_t mock_encoder_get_counter(rx_mtu_channel_t channel);

/**
 * @brief Advance counter by specified counts (simulating rotation)
 *
 * @param[in] channel MTU channel
 * @param[in] delta   Counts to add (can be negative for reverse)
 *
 * @note This properly wraps at 16-bit boundaries
 */
void mock_encoder_advance_counter(rx_mtu_channel_t channel, int32_t delta);

/* =============================================================================
 * State Query Functions
 * =============================================================================
 */

/**
 * @brief Check if channel is initialized
 *
 * @param[in] channel MTU channel
 *
 * @return true if initialized, false otherwise
 */
bool mock_encoder_is_initialized(rx_mtu_channel_t channel);

/**
 * @brief Check if timer is running
 *
 * @param[in] channel MTU channel
 *
 * @return true if running, false otherwise
 */
bool mock_encoder_is_running(rx_mtu_channel_t channel);

/**
 * @brief Get timer mode register value
 *
 * @param[in] channel MTU channel
 *
 * @return TMDR register value
 */
uint8_t mock_encoder_get_tmdr(rx_mtu_channel_t channel);

/**
 * @brief Get start call count for channel
 *
 * @param[in] channel MTU channel
 *
 * @return Number of rx_mtu_start calls
 */
uint32_t mock_encoder_get_start_count(rx_mtu_channel_t channel);

/**
 * @brief Get stop call count for channel
 *
 * @param[in] channel MTU channel
 *
 * @return Number of rx_mtu_stop calls
 */
uint32_t mock_encoder_get_stop_count(rx_mtu_channel_t channel);

/* =============================================================================
 * Error Injection Functions
 * =============================================================================
 */

/**
 * @brief Set error injection for initialization
 *
 * @param[in] inject_error true to inject initialization failure
 */
void mock_encoder_set_init_error(bool inject_error);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RX_MTU_ENCODER_H */
