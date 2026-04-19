/**
 * @file rx_sync.c
 * @brief Multi-Motor Velocity Synchronization Layer Implementation
 *
 * @details
 * Implements minimum-velocity tracking with EMA filtering.  The algorithm
 * mirrors matlab/firmware_match_sim.m exactly -- if this code disagrees with
 * the simulation, the bug is here, not in the simulation.
 *
 * Algorithm per control cycle (rx_sync_update):
 *   1. For each motor i:
 *        v_ema[i] = alpha * v_measured[i] + (1 - alpha) * v_ema[i]
 *   2. Fault detection:
 *        if v_ema[i] < fault_thresh_frac * v_commanded for fault_ticks ticks:
 *            faulted[i] = true
 *        else: fault_timer[i] = 0
 *   3. v_sync = min(v_ema[i] for non-faulted i)
 *        fallback to v_commanded if all motors faulted
 *   4. rx_sync_get_setpoint(i) returns min(v_commanded, v_sync)
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include "rx_sync.h"

#include <string.h>

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

static inline float internal_minf(float a, float b)
{
  return (a < b) ? a : b;
}

static inline float internal_maxf(float a, float b)
{
  return (a > b) ? a : b;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

rx_err_t rx_sync_init(rx_sync_handle_t* h, const rx_sync_config_t* cfg)
{
  if (h == nullptr || cfg == nullptr) {
    return k_rx_err_null_ptr;
  }
  if (cfg->ema_alpha <= 0.0f || cfg->ema_alpha >= 1.0f) {
    return k_rx_err_invalid_arg;
  }
  if (cfg->fault_thresh_frac <= 0.0f || cfg->fault_thresh_frac >= 1.0f) {
    return k_rx_err_invalid_arg;
  }
  if (cfg->fault_ticks == 0U) {
    return k_rx_err_invalid_arg;
  }

  (void)memset(h, 0, sizeof(*h));
  h->config      = *cfg;
  h->v_sync      = 0.0f;
  h->initialized = true;

  return k_rx_ok;
}

rx_err_t rx_sync_update(rx_sync_handle_t* h,
                        const float       v_measured[k_sync_num_motors],
                        float             v_commanded,
                        float             dt_s)
{
  if (h == nullptr || v_measured == nullptr) {
    return k_rx_err_null_ptr;
  }
  if (!h->initialized) {
    return k_rx_err_not_initialized;
  }
  if (dt_s <= 0.0f || v_commanded < 0.0f) {
    return k_rx_err_invalid_arg;
  }

  const float alpha       = h->config.ema_alpha;
  const float one_m_alpha = 1.0f - alpha;
  const float fault_thr   = h->config.fault_thresh_frac * v_commanded;

  /* Step 1: update EMA for each motor */
  for (uint8_t i = 0U; i < (uint8_t)k_sync_num_motors; i++) {
    const float v_m = internal_maxf(0.0f, v_measured[i]);
    h->v_ema[i]     = alpha * v_m + one_m_alpha * h->v_ema[i];
  }

  /* Step 2: fault detection */
  for (uint8_t i = 0U; i < (uint8_t)k_sync_num_motors; i++) {
    if (h->v_ema[i] < fault_thr) {
      if (h->fault_timer[i] < h->config.fault_ticks + 1U) {
        h->fault_timer[i]++;
      }
    } else {
      h->fault_timer[i] = 0U;
      h->faulted[i]     = false;
    }
    if (h->fault_timer[i] >= h->config.fault_ticks) {
      h->faulted[i] = true;
    }
  }

  /* Step 3: compute v_sync = min of non-faulted motors */
  float v_sync    = v_commanded; /* fallback if all faulted */
  bool  any_valid = false;

  for (uint8_t i = 0U; i < (uint8_t)k_sync_num_motors; i++) {
    if (!h->faulted[i]) {
      if (!any_valid || h->v_ema[i] < v_sync) {
        v_sync    = h->v_ema[i];
        any_valid = true;
      }
    }
  }

  h->v_sync = v_sync;
  return k_rx_ok;
}

float rx_sync_get_setpoint(const rx_sync_handle_t* h, uint8_t motor_id, float v_commanded)
{
  if (h == nullptr || !h->initialized || motor_id >= (uint8_t)k_sync_num_motors) {
    return 0.0f;
  }
  return internal_minf(v_commanded, h->v_sync);
}

rx_err_t rx_sync_reset(rx_sync_handle_t* h)
{
  if (h == nullptr) {
    return k_rx_err_null_ptr;
  }
  if (!h->initialized) {
    return k_rx_err_not_initialized;
  }

  const rx_sync_config_t saved_cfg = h->config;
  (void)memset(h, 0, sizeof(*h));
  h->config      = saved_cfg;
  h->initialized = true;

  return k_rx_ok;
}
