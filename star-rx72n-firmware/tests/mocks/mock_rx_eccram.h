/**
 * @file mock_rx_eccram.h
 * @brief Mock rx_eccram public API for unit tests that consume the ECCRAM driver
 *
 * @details
 * Provides a stub implementation of the rx_eccram public API so that other
 * modules' unit tests can be built without linking against the full rx_eccram
 * driver (which depends on hardware-specific register mocks). Each stubbed
 * function records its invocation count and the last argument it received,
 * and returns a configurable rx_err_t so tests can exercise error paths.
 *
 * @par Intended Use:
 * Include this header (and link mock_rx_eccram.c) in test targets that exercise
 * higher-level components which call rx_eccram_* at runtime. Tests that
 * exercise the ECCRAM driver itself use the real rx_eccram.c plus the
 * register-level mocks in mock_rx72n_eccram_regs.h.
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../../libs/rx_hal/inc/rx_eccram.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct mock_rx_eccram_state_t
 * @brief Instrumentation state for mock_rx_eccram
 *
 * @details
 * Aggregates call counters, last-seen arguments, and programmable return
 * values for every stubbed rx_eccram_* function. Tests configure the
 * return fields before the unit-under-test call, then inspect the count
 * and captured argument fields afterwards.
 *
 * @since Version 1.0.0
 */
typedef struct {
  uint32_t         init_calls;     /**< Count of rx_eccram_init() invocations */
  rx_eccram_mode_t init_last_mode; /**< Mode argument from most recent init */
  rx_err_t         init_return;    /**< Programmable return for rx_eccram_init() */

  uint32_t           get_status_calls;  /**< Count of rx_eccram_get_error_status() */
  rx_err_t           get_status_return; /**< Programmable return */
  rx_eccram_status_t get_status_fill;   /**< Value written through out pointer */

  uint32_t clear_errors_calls;  /**< Count of rx_eccram_clear_errors() */
  rx_err_t clear_errors_return; /**< Programmable return */

  uint32_t               register_isr_calls;  /**< Count of rx_eccram_register_error_isr() */
  rx_eccram_on_1bit_fn_t last_on_1bit;        /**< 1-bit callback from most recent call */
  rx_eccram_on_2bit_fn_t last_on_2bit;        /**< 2-bit callback from most recent call */
  void*                  last_ctx;            /**< ctx from most recent call */
  rx_err_t               register_isr_return; /**< Programmable return */

  uint32_t  region_start_calls;  /**< Count of rx_eccram_region_start() */
  uintptr_t region_start_return; /**< Programmable return (default 0x00FF8000) */

  uint32_t  region_end_calls;  /**< Count of rx_eccram_region_end() */
  uintptr_t region_end_return; /**< Programmable return (default 0x00FFFFFF) */
} mock_rx_eccram_state_t;

/**
 * @brief Reset mock state to defaults (all counters zero, returns = k_rx_ok)
 *
 * @pre  None (safe to call before any mock usage).
 * @post All counters zeroed.
 * @post All *_return fields set to k_rx_ok.
 * @post region_start_return = 0x00FF8000, region_end_return = 0x00FFFFFF.
 *
 * @since Version 1.0.0
 */
void mock_rx_eccram_reset(void);

/**
 * @brief Retrieve a mutable pointer to the mock state
 *
 * @return Pointer to the internal state; tests configure return values and
 *         inspect counters/captured arguments through this pointer.
 *
 * @note Never returns nullptr.
 * @since Version 1.0.0
 */
mock_rx_eccram_state_t* mock_rx_eccram_state(void);

#ifdef __cplusplus
}
#endif
