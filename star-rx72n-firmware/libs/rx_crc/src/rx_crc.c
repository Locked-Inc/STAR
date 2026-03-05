/**
 * @file rx_crc.c
 * @brief CRC Public API Dispatcher
 *
 * @details
 * Implements the public rx_crc_* API. Routes to software, hw_cpu, or hw_dma
 * backends via rx_crc_compute(). On host builds (no __RX__), hardware backends
 * fall back to software automatically.
 *
 * @see rx_crc.h       Public API
 * @see rx_crc_sw.c    Software backend
 * @see rx_crc_hw.c    Hardware backend (RX72N only)
 *
 * @author Locked, Inc.
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>

#include "rx_crc_internal.h"

/* =============================================================================
 * Module State
 * =============================================================================
 */

/**
 * @brief Tracks whether CRC hardware has been initialized
 * @warning Not thread-safe; call rx_crc_init() single-threaded during init
 */
static bool s_crc_initialized = false;

/* =============================================================================
 * Public API - Lifecycle
 * =============================================================================
 */

rx_err_t rx_crc_init(void)
{
  if (s_crc_initialized) {
    return k_rx_err_invalid_state;
  }

#ifdef __RX__
  rx_err_t err = internal_crc_hw_init();
  if (err != k_rx_ok) {
    return err;
  }
#endif

  s_crc_initialized = true;
  return k_rx_ok;
}

rx_err_t rx_crc_deinit(void)
{
  if (!s_crc_initialized) {
    return k_rx_err_invalid_state;
  }

#ifdef __RX__
  rx_err_t err = internal_crc_hw_deinit();
  if (err != k_rx_ok) {
    return err;
  }
#endif

  s_crc_initialized = false;
  return k_rx_ok;
}

/* =============================================================================
 * Public API - Computation
 * =============================================================================
 */

rx_err_t rx_crc_compute(const rx_crc_config_t* config,
                        const uint8_t*         data,
                        uint32_t               len,
                        uint32_t*              result_out)
{
  if (config == nullptr || data == nullptr || result_out == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (len < k_crc_len_min || len > k_crc_len_max) {
    return k_rx_err_invalid_arg;
  }

  switch (config->backend) {
    case k_rx_crc_backend_software:
      *result_out = internal_crc_sw_compute(config->poly, data, len);
      return k_rx_ok;

#ifdef __RX__
    case k_rx_crc_backend_hw_cpu:
      return internal_crc_hw_cpu_compute(config, data, len, result_out);

    case k_rx_crc_backend_hw_dma:
      return internal_crc_hw_dma_compute(config, data, len, result_out);
#else
    /* Host build: hardware backends not available - fall back to software */
    case k_rx_crc_backend_hw_cpu:
    case k_rx_crc_backend_hw_dma:
      *result_out = internal_crc_sw_compute(config->poly, data, len);
      return k_rx_ok;
#endif

    default:
      return k_rx_err_invalid_arg;
  }
}

/**
 * @brief Incremental CRC-32/IEEE update (software only - HW cannot seed CRC state)
 */
uint32_t rx_crc32_update(uint32_t crc, const uint8_t* data, uint32_t len)
{
  if (data == nullptr || len == 0U) {
    return crc;
  }

  return internal_crc32_sw_update(crc, data, len);
}
