/**
 * @file rx_stack_monitor.h
 * @brief ThreadX Stack Overflow Detection and High-Water Mark Monitoring
 *
 * @details
 * Provides the application-level ThreadX stack overflow handler and related
 * stack monitoring utilities for safety-critical RX72N firmware.
 *
 * ## Motivation
 *
 * ThreadX optionally checks the 0xEF sentinel pattern (filled by
 * TX_STACK_FILLING) at every context switch when TX_ENABLE_STACK_CHECKING is
 * defined.  If the pattern is corrupted the RTOS calls the application handler
 * registered with tx_thread_stack_error_notify().  Without that handler the
 * default ThreadX behaviour is a spin-loop that is invisible at the system
 * level and gives no diagnostic information.
 *
 * This module:
 * 1. Registers a named application handler that logs the overflowing thread
 *    name before performing a controlled safety halt.
 * 2. Exposes rx_stack_monitor_get_free_bytes() so long-run tests can collect
 *    per-thread high-water marks.
 *
 * ## Integration
 *
 * Call rx_stack_monitor_init() from tx_application_define() after all threads
 * are created:
 *
 * @code
 * void tx_application_define(void *first_unused_memory)
 * {
 *     // ... create threads ...
 *     rx_err_t err = rx_stack_monitor_init();
 *     RX_ASSERT(err == k_rx_ok, "rx_stack_monitor_init must succeed");
 * }
 * @endcode
 *
 * ## Design Constraints
 *
 * - No dynamic memory (NASA Rule 3 / RX72N constraint).
 * - The overflow handler is marked [[noreturn]] for target builds so the
 *   compiler can omit unreachable code after it.
 * - In UNIT_TEST builds the [[noreturn]] attribute is suppressed and the
 *   function returns, allowing test scaffolding to verify the handler is
 *   called without crashing the test runner.
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto, setjmp, recursion
 * - Rule 3: No dynamic memory
 * - Rule 5: Precondition / postcondition checks in every public function
 * - Rule 7: All return values checked by callers
 * - Rule 8: C23 typed enums for all integer constants
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - Single Responsibility: Handles only stack overflow detection
 * - Dependency Inversion: Registered via ThreadX callback pointer, not
 *   direct coupling
 *
 * @see tx_thread_stack_error_notify() ThreadX API used to register handler
 * @see rx_stack_monitor_init() Register the overflow handler
 * @see rx_stack_monitor_get_free_bytes() Query per-thread free stack bytes
 *
 * @date 2026-01-11
 * @version 1.0.0
 * @par Platform:
 * - Target MCU: Renesas RX72N (240 MHz, RXv3 core)
 * - Toolchain: GNURX 8.3.0 or later; C23 typed enum support required
 * - RTOS: Azure RTOS ThreadX 6.x; TX_ENABLE_STACK_CHECKING must be defined in tx_user.h
 *
 * @since Version 1.0.0
 * @author Locked, Inc.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"
#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Register the stack overflow handler with ThreadX
 *
 * @details
 * Calls tx_thread_stack_error_notify() to register
 * internal_stack_overflow_handler() as the application callback invoked by
 * ThreadX whenever a corrupted stack sentinel is detected at a context switch.
 *
 * Must be called from tx_application_define(), after all threads are created
 * and before the scheduler starts running those threads.
 *
 * ## Integration Example
 *
 * @code
 * void tx_application_define(void *first_unused_memory)
 * {
 *     // ... create threads, buses, etc. ...
 *
 *     rx_err_t err = rx_stack_monitor_init();
 *     RX_ASSERT(err == k_rx_ok, "Stack monitor init must succeed");
 * }
 * @endcode
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Handler registered successfully
 * @retval k_rx_err_not_supported TX_ENABLE_STACK_CHECKING not defined in
 *         tx_user.h (ThreadX returns TX_FEATURE_NOT_ENABLED)
 *
 * @pre TX_ENABLE_STACK_CHECKING is defined (set via USE_TX_ENABLE_STACK_CHECKING=1)
 * @pre Called from tx_application_define() context (before scheduler start)
 * @post _tx_thread_application_stack_error_handler points to the STAR handler
 * @post Any subsequent stack sentinel corruption triggers the STAR handler
 *
 * @note Thread safety: Must be called before threads are running (single-
 *       threaded context in tx_application_define()).
 * @warning If this function returns k_rx_err_not_supported the firmware
 *          was built without TX_ENABLE_STACK_CHECKING.  Ensure
 *          USE_TX_ENABLE_STACK_CHECKING=1 in tx_user.h.
 *
 * @see tx_thread_stack_error_notify() Underlying ThreadX registration API
 * @see rx_stack_monitor_get_free_bytes() Query stack headroom after init
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_stack_monitor_init(void);

/**
 * @brief Return the number of unused (free) bytes in a thread's stack
 *
 * @details
 * Scans the thread's stack memory for the 0xEF fill pattern placed by
 * ThreadX stack filling (enabled when TX_DISABLE_STACK_FILLING is NOT
 * defined).  The count of consecutive 0xEF bytes from the stack base toward
 * the stack pointer is returned as the free-byte estimate.
 *
 * This value represents the **minimum** remaining headroom since the scan
 * starts from the low-address end of the stack (the direction into which the
 * RX stack grows).
 *
 * ## High-Water Mark Collection Example
 *
 * @code
 * // After a long-run test, collect stack usage for every thread:
 * TX_THREAD *t = _tx_thread_created_ptr;
 * while (t != TX_NULL) {
 *     uint32_t free_bytes;
 *     rx_err_t err = rx_stack_monitor_get_free_bytes(t, &free_bytes);
 *     if (err == k_rx_ok) {
 *         rx_log_info("STACK", "Thread %s: %lu B free of %lu B",
 *                     t->tx_thread_name,
 *                     (unsigned long)free_bytes,
 *                     (unsigned long)t->tx_thread_stack_size);
 *     }
 *     t = t->tx_thread_created_next;
 *     if (t == _tx_thread_created_ptr) { break; }
 * }
 * @endcode
 *
 * @param[in]  thread_ptr  Pointer to the ThreadX thread control block.
 *                         The thread control block must be fully initialized
 *                         and tx_thread_stack_size must be > 0.  Stack
 *                         sentinel bytes are valid regardless of the thread's
 *                         suspended or sleeping state.
 * @param[out] free_bytes  Set to the count of 0xEF-filled bytes remaining in
 *                         the stack (i.e. bytes never written by the thread).
 *                         Valid only when k_rx_ok is returned.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok  High-water mark computed successfully
 * @retval k_rx_err_null_ptr  thread_ptr or free_bytes is NULL
 * @retval k_rx_err_invalid_state  Stack fill pattern not present; stack
 *         checking may be disabled or the stack completely overrun
 *
 * @pre thread_ptr must be a valid, initialized TX_THREAD (not NULL)
 * @pre free_bytes must point to a valid uint32_t storage location (not NULL)
 * @post *free_bytes contains the number of 0xEF-filled bytes at stack base
 * @post Thread state is not modified (read-only scan)
 *
 * @note Not thread-safe with respect to the scanned thread: the result is
 *       only a snapshot and may change immediately after the call returns.
 * @note TX_DISABLE_STACK_FILLING must NOT be defined; otherwise the fill
 *       pattern is absent and the function returns k_rx_err_invalid_state.
 *
 * @see rx_stack_monitor_init() Must be called first to enable overflow detection
 * @see TX_THREAD#tx_thread_stack_start ThreadX stack base pointer
 * @see TX_THREAD#tx_thread_stack_size ThreadX total stack size
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_stack_monitor_get_free_bytes(const TX_THREAD* thread_ptr,
                                                       uint32_t*        free_bytes);

#ifdef __cplusplus
}
#endif
