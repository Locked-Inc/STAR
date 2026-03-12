/**
 * @file rx_log.c
 * @brief Runtime log backend selection for the RX72N logging system
 *
 * @details
 * Implements the runtime backend selector for the rx_log system. Callers use
 * rx_log_set_backend() once at startup (before any RTOS tasks run) to choose
 * which physical interfaces receive log output.  The LOG_PUTS / LOG_PUTC /
 * LOG_PUTINT / LOG_PUTHEX macros in rx_log.h then dispatch to UART, USB, or
 * both based on the active bitmask returned by rx_log_get_backend().
 *
 * ## Thread Safety
 * rx_log_set_backend() is **NOT** thread-safe.  It must be called once from
 * a single context (typically tx_application_define()) before the scheduler
 * starts.  rx_log_get_backend() is read-only and safe to call from any
 * context once the backend has been set.
 *
 * @see rx_log.h   Public logging API and LOG_* dispatch macros
 * @see rx_log_usb.c  USB CDC log backend implementation
 *
 * @author Locked, Inc.
 * @date 2026-03-11
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_log.h"

/* =============================================================================
 * Static State
 * =============================================================================
 */

/**
 * @var s_log_backend
 * @brief Active log output backend bitmask
 *
 * @details
 * Stores the current rx_log_backend_t bitmask selected by rx_log_set_backend().
 * Default is UART only (k_log_backend_uart) so that early boot messages appear
 * on the debug UART before USB CDC is enumerated.
 *
 * @note Intentionally NOT volatile: the value is written once at startup from a
 * single context and then only read.  If future use requires concurrent writes,
 * callers must add external synchronization.
 * @warning Direct mutation outside of rx_log_set_backend() is forbidden.
 * @since Version 1.0.0
 */
static rx_log_backend_t s_log_backend = k_log_backend_uart;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Set the active log output backend(s)
 *
 * @details
 * Validates the bitmask and atomically (from a single-context perspective)
 * stores it.  Any bit outside the defined mask (bits 2-7) is rejected to
 * catch caller mistakes early.
 *
 * @param[in] backend  Bitmask of rx_log_backend_t values (0x00..0x03)
 *
 * @return rx_err_t
 * @retval k_rx_ok              Backend stored successfully
 * @retval k_rx_err_invalid_arg Unknown bits set in backend
 *
 * @pre Must be called before any RTOS tasks are started
 * @pre backend must only contain bits defined in rx_log_backend_t
 * @post s_log_backend updated if validation passes
 * @post s_log_backend unchanged if validation fails
 *
 * @note Not thread-safe. Call once at startup.
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [PASS] 2 preconditions, 2 postconditions
 */
rx_err_t rx_log_set_backend(rx_log_backend_t backend)
{
  /* Precondition: reject any bit outside the known mask */
  if ((backend & (rx_log_backend_t)(~(uint8_t)k_log_backend_both)) != k_log_backend_none) {
    return k_rx_err_invalid_arg;
  }

  s_log_backend = backend;
  return k_rx_ok;
}

/**
 * @brief Return the currently active log backend bitmask
 *
 * @details
 * Reads the backend bitmask previously stored by rx_log_set_backend().
 * Called by the LOG_PUTS / LOG_PUTC / LOG_PUTINT / LOG_PUTHEX macros on
 * every log invocation to select the output interface(s).
 *
 * @return rx_log_backend_t  Active backend bitmask (k_log_backend_none if no logging)
 *
 * @pre  s_log_backend has been set at least once (defaults to k_log_backend_uart at startup)
 * @pre  No concurrent write to s_log_backend is in progress (not thread-safe)
 * @post Returns the current value of s_log_backend; global state is not modified
 * @post Returned value is a valid rx_log_backend_t bitmask (bits 0..1 only)
 *
 * @note Read-only after startup -- safe to call from any context once initialized.
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [PASS] 2 preconditions, 2 postconditions
 */
rx_log_backend_t rx_log_get_backend(void)
{
  return s_log_backend;
}
