/**
 * @file mock_rx_mtu_encoder.c
 * @brief Mock MTU Encoder Hardware Implementation for Unit Testing
 *
 * @details
 * Provides mock implementation of MTU timer registers and rx_mtu functions
 * for host-side encoder driver testing. Simulates hardware behavior including
 * counter values, register access, and start/stop operations.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

/* Include mock regs header FIRST to define mock types before real headers */
#include "mock_rx_mtu_encoder.h"

#include <stddef.h>

#include "mock_rx_mtu_regs.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief Counter wrap constant
 */
typedef enum : uint32_t {
  k_counter_max  = 65536,   /**< 16-bit counter maximum + 1 */
  k_counter_mask = 0xFFFFU, /**< 16-bit mask */
} counter_constants_t;

/**
 * @brief Channel index mapping constants
 */
typedef enum : int32_t {
  k_ch_idx_0       = 0,  /**< Array index for channel 0 */
  k_ch_idx_1       = 1,  /**< Array index for channel 1 */
  k_ch_idx_2       = 2,  /**< Array index for channel 2 */
  k_ch_idx_3       = 3,  /**< Array index for channel 3 */
  k_ch_idx_4       = 4,  /**< Array index for channel 4 */
  k_ch_idx_6       = 5,  /**< Array index for channel 6 */
  k_ch_idx_7       = 6,  /**< Array index for channel 7 */
  k_ch_idx_invalid = -1, /**< Invalid channel sentinel */
} channel_index_t;

/* =============================================================================
 * Global Mock Instance and Registers
 * =============================================================================
 */

mock_encoder_state_t g_mock_encoder_state;

/* Global mock registers (referenced by mock_rx_mtu_regs.h inline accessors) */
rx_mtu_channel_regs_t g_mock_mtu_regs[k_mock_mtu_max_channels];
rx_system_regs_t      g_mock_system_regs;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Convert channel enum to array index
 *
 * @param[in] channel MTU channel
 *
 * @return Array index (0-6), or -1 if invalid
 */
static int32_t internal_channel_to_index(rx_mtu_channel_t channel)
{
  switch (channel) {
    case k_mtu_channel_0:
      return k_ch_idx_0;
    case k_mtu_channel_1:
      return k_ch_idx_1;
    case k_mtu_channel_2:
      return k_ch_idx_2;
    case k_mtu_channel_3:
      return k_ch_idx_3;
    case k_mtu_channel_4:
      return k_ch_idx_4;
    case k_mtu_channel_6:
      return k_ch_idx_6;
    case k_mtu_channel_7:
      return k_ch_idx_7;
    default:
      return k_ch_idx_invalid;
  }
}

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_encoder_init(void)
{
  uint8_t* enc_ptr = (uint8_t*)&g_mock_encoder_state;
  for (size_t i = 0; i < sizeof(g_mock_encoder_state); i++) {
    enc_ptr[i] = 0;
  }

  uint8_t* reg_ptr = (uint8_t*)g_mock_mtu_regs;
  for (size_t i = 0; i < sizeof(g_mock_mtu_regs); i++) {
    reg_ptr[i] = 0;
  }

  uint8_t* sys_ptr = (uint8_t*)&g_mock_system_regs;
  for (size_t i = 0; i < sizeof(g_mock_system_regs); i++) {
    sys_ptr[i] = 0;
  }

  g_mock_encoder_state.initialized = true;
}

void mock_encoder_deinit(void)
{
  g_mock_encoder_state.initialized = false;
}

void mock_encoder_reset(void)
{
  mock_encoder_init();
}

/* =============================================================================
 * Counter Simulation Functions
 * =============================================================================
 */

void mock_encoder_set_counter(rx_mtu_channel_t channel, uint16_t count)
{
  int32_t idx = internal_channel_to_index(channel);

  if (idx >= 0) {
    g_mock_mtu_regs[idx].tcnt               = count;
    g_mock_encoder_state.channels[idx].tcnt = count;
  }
}

uint16_t mock_encoder_get_counter(rx_mtu_channel_t channel)
{
  int32_t idx = internal_channel_to_index(channel);

  if (idx >= 0) {
    return g_mock_mtu_regs[idx].tcnt;
  }
  return 0;
}

void mock_encoder_advance_counter(rx_mtu_channel_t channel, int32_t delta)
{
  int32_t idx = internal_channel_to_index(channel);

  if (idx >= 0) {
    int32_t       current = (int32_t)g_mock_mtu_regs[idx].tcnt;
    int32_t       new_val = current + delta;
    const int32_t max     = (int32_t)k_counter_max;
    /* Handle 16-bit wraparound with bounded modulo arithmetic */
    new_val                                 = ((new_val % max) + max) % max;
    g_mock_mtu_regs[idx].tcnt               = (uint16_t)new_val;
    g_mock_encoder_state.channels[idx].tcnt = (uint16_t)new_val;
  }
}

/* =============================================================================
 * State Query Functions
 * =============================================================================
 */

bool mock_encoder_is_initialized(rx_mtu_channel_t channel)
{
  int32_t idx = internal_channel_to_index(channel);

  if (idx >= 0) {
    return g_mock_encoder_state.channels[idx].initialized;
  }
  return false;
}

bool mock_encoder_is_running(rx_mtu_channel_t channel)
{
  int32_t idx = internal_channel_to_index(channel);

  if (idx >= 0) {
    return g_mock_encoder_state.channels[idx].running;
  }
  return false;
}

uint8_t mock_encoder_get_tmdr(rx_mtu_channel_t channel)
{
  int32_t idx = internal_channel_to_index(channel);

  if (idx >= 0) {
    return g_mock_mtu_regs[idx].tmdr;
  }
  return 0;
}

uint32_t mock_encoder_get_start_count(rx_mtu_channel_t channel)
{
  int32_t idx = internal_channel_to_index(channel);

  if (idx >= 0) {
    return g_mock_encoder_state.channels[idx].start_count;
  }
  return 0;
}

uint32_t mock_encoder_get_stop_count(rx_mtu_channel_t channel)
{
  int32_t idx = internal_channel_to_index(channel);

  if (idx >= 0) {
    return g_mock_encoder_state.channels[idx].stop_count;
  }
  return 0;
}

/* =============================================================================
 * Error Injection Functions
 * =============================================================================
 */

void mock_encoder_set_init_error(bool inject_error)
{
  g_mock_encoder_state.inject_init_error = inject_error;
}

/* =============================================================================
 * Mock rx_mtu Functions (Replace Real Implementations)
 * =============================================================================
 */

rx_err_t rx_mtu_start(rx_mtu_channel_t channel)
{
  int32_t idx = internal_channel_to_index(channel);

  if (idx < 0) {
    return k_rx_err_invalid_arg;
  }

  g_mock_encoder_state.channels[idx].running = true;
  g_mock_encoder_state.channels[idx].start_count++;
  return k_rx_ok;
}

rx_err_t rx_mtu_stop(rx_mtu_channel_t channel)
{
  int32_t idx = internal_channel_to_index(channel);

  if (idx < 0) {
    return k_rx_err_invalid_arg;
  }

  g_mock_encoder_state.channels[idx].running = false;
  g_mock_encoder_state.channels[idx].stop_count++;
  return k_rx_ok;
}
