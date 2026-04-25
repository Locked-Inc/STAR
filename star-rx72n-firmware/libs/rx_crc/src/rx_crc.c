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

/**
 * @brief Initialize the CRC module
 *
 * @details
 * Sets the initialized flag and, on RX72N hardware, enables the CRC peripheral
 * clock and DMAC via internal_crc_hw_init(). On host builds, the hardware path
 * is skipped and the module enters the ready state immediately.
 *
 *
 * @pre Not already initialized
 * @pre System clock (PCLKA) must be running on RX72N
 * @post s_crc_initialized = true
 * @post CRC hardware peripheral enabled (RX72N only)
 *
 * @note Not thread-safe; call during single-threaded system init
 * @since Version 1.0.0
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

/**
 * @brief Deinitialize the CRC module
 *
 * @details
 * Clears the initialized flag and, on RX72N hardware, disables the CRC
 * peripheral clock and releases DMAC ownership via internal_crc_hw_deinit().
 * On host builds, the hardware path is skipped.
 *
 *
 * @pre Module must be initialized via rx_crc_init()
 * @pre No concurrent CRC computations in progress
 * @post s_crc_initialized = false
 * @post CRC hardware peripheral disabled (RX72N only)
 *
 * @note Not thread-safe; call during single-threaded system teardown
 * @since Version 1.0.0
 */
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

/**
 * @brief Dispatch CRC computation to the configured backend
 *
 * @details
 * Validates inputs then routes to one of three backends:
 * - software: always available; uses bitwise algorithm; only supports LSB-first
 *   (reflected) bit order; returns k_rx_err_invalid_arg for MSB-first requests
 * - hw_cpu: RX72N CRC peripheral driven byte-by-byte; falls back to software on host
 * - hw_dma: RX72N CRC peripheral via DMAC; falls back to software on host
 *
 * @note The software backend only supports k_rx_crc_bit_order_lsb_first. All
 *       software CRC implementations use the reflected (LSB-first) form.
 *       Requesting k_rx_crc_bit_order_msb_first with the software backend
 *       returns k_rx_err_invalid_arg.
 *
 *
 *
 * @pre config, data, result_out must not be NULL
 * @pre len must be in [1, k_crc_len_max]
 * @post *result_out contains the computed CRC on k_rx_ok
 * @post No state is modified (pure computation)
 *
 * @par Activity Diagram:
 * @startuml
 * start
 * :rx_crc_compute(config, data, len, result_out);
 * if (any pointer NULL?) then (yes)
 *   :return k_rx_err_null_ptr;
 *   stop
 * endif
 * if (len < 1 or len > k_crc_len_max?) then (yes)
 *   :return k_rx_err_invalid_arg;
 *   stop
 * endif
 * switch (config->backend)
 * case (software)
 *   if (bit_order != LSB-first?) then (yes)
 *     :return k_rx_err_invalid_arg;
 *     stop
 *   endif
 *   :internal_crc_sw_compute(poly, data, len);
 *   :return k_rx_ok;
 *   stop
 * case (hw_cpu / hw_dma on RX72N)
 *   if (not initialized?) then (yes)
 *     :return k_rx_err_not_initialized;
 *     stop
 *   endif
 *   :internal_crc_hw_cpu/dma_compute();
 *   :return result;
 *   stop
 * case (hw_cpu / hw_dma on host)
 *   if (bit_order != LSB-first?) then (yes)
 *     :return k_rx_err_invalid_arg;
 *     stop
 *   endif
 *   :internal_crc_sw_compute() [host fallback];
 *   :return k_rx_ok;
 *   stop
 * case (default)
 *   :return k_rx_err_invalid_arg;
 *   stop
 * endswitch
 * @enduml
 *
 * @note Thread-safe for software backend; not thread-safe for hw_cpu/hw_dma
 * @since Version 1.0.0
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

  if (config->poly > k_rx_crc_poly_crc32c || config->bit_order > k_rx_crc_bit_order_msb_first) {
    return k_rx_err_invalid_arg;
  }

  switch (config->backend) {
    case k_rx_crc_backend_software:
      if (config->bit_order != k_rx_crc_bit_order_lsb_first) {
        return k_rx_err_invalid_arg;
      }
      *result_out = internal_crc_sw_compute(config->poly, data, len);
      return k_rx_ok;

#ifdef __RX__
    case k_rx_crc_backend_hw_cpu:
      if (!s_crc_initialized) {
        return k_rx_err_not_initialized;
      }
      return internal_crc_hw_cpu_compute(config, data, len, result_out);

    case k_rx_crc_backend_hw_dma:
      if (!s_crc_initialized) {
        return k_rx_err_not_initialized;
      }
      return internal_crc_hw_dma_compute(config, data, len, result_out);
#else
    /* Host build: hardware backends not available - fall back to software */
    case k_rx_crc_backend_hw_cpu:
      [[fallthrough]]; /* hw_cpu has same software fallback as hw_dma on host */
    case k_rx_crc_backend_hw_dma:
      if (config->bit_order != k_rx_crc_bit_order_lsb_first) {
        return k_rx_err_invalid_arg;
      }
      *result_out = internal_crc_sw_compute(config->poly, data, len);
      return k_rx_ok;
#endif

    default:
      return k_rx_err_invalid_arg;
  }
}

/**
 * @brief Incremental CRC-32/IEEE update (software only -- hardware cannot seed CRC state)
 *
 * @details
 * Delegates to internal_crc32_sw_update() after null/zero-length guard checks.
 * Passes NULL or zero-length data through unchanged (returns crc unmodified).
 *
 * @note The correct seed for the first call is 0U (not 0xFFFFFFFF). Internally,
 *       the function un-finalizes by XOR with 0xFFFFFFFF, so crc=0 produces the
 *       correct CRC-32 initial accumulator state of 0xFFFFFFFF.
 *
 *
 *
 * @pre data must point to len valid bytes, or be NULL (pass-through)
 * @pre len must be > 0 for any update to occur
 * @post Returns finalized CRC-32 value covering all bytes processed so far
 * @post No state is modified (pure computation)
 *
 * @note Always uses software CRC; hardware cannot resume a partially computed CRC
 * @since Version 1.0.0
 */
uint32_t rx_crc32_update(uint32_t crc, const uint8_t* data, uint32_t len)
{
  if (data == nullptr || len == 0U) {
    return crc;
  }

  return internal_crc32_sw_update(crc, data, len);
}
