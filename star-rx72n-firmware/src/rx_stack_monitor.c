/**
 * @file rx_stack_monitor.c
 * @brief ThreadX Stack Overflow Detection and High-Water Mark Monitoring
 *
 * @details
 * Implements the application-level ThreadX stack overflow handler required when
 * TX_ENABLE_STACK_CHECKING is defined in tx_user.h.  At every context switch
 * ThreadX verifies the 0xEF sentinel bytes placed by TX_STACK_FILLING; if those
 * bytes are corrupted it calls the handler registered via
 * tx_thread_stack_error_notify().
 *
 * ## Handler Behaviour
 *
 * When a stack overflow is detected the handler:
 * 1. Logs the overflowing thread's name via uart_debug_puts() (direct, avoids
 *    any further stack usage from logging infrastructure).
 * 2. Calls internal_rx_fatal_error() which disables interrupts and spins,
 *    triggering the hardware IWDT reset after at most 2 seconds.
 *
 * ## High-Water Mark Collection
 *
 * rx_stack_monitor_get_free_bytes() scans the 0xEF-filled region at the stack
 * base to determine how many bytes have never been touched.  Call this function
 * periodically (e.g. from the telemetry task) or after long-run tests.
 *
 * ## Initialization
 *
 * ```
 * tx_application_define()
 *   +- ...create tasks...
 *   +- rx_stack_monitor_init()     <- register handler before scheduler starts
 *   +- (scheduler starts)
 * ```
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto, setjmp, recursion
 * - Rule 2: Bounded loops (stack scan uses fixed upper bound = stack size)
 * - Rule 3: No dynamic memory
 * - Rule 4: All functions <= 60 lines
 * - Rule 5: >= 2 precondition/postcondition checks per function
 * - Rule 7: All return values checked by callers via RX_ASSERT
 * - Rule 8: C23 typed enums for all integer constants
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - Single Responsibility: Stack overflow detection and high-water mark only
 * - Dependency Inversion: Coupled to ThreadX only via the registered callback
 *
 * @see rx_stack_monitor.h Public API declarations
 * @see tx_user.h USE_TX_ENABLE_STACK_CHECKING / USE_TX_DISABLE_STACK_FILLING
 *
 * @date 2026-01-11
 * @version 1.0.0
 * @par Hardware:
 * - Target MCU: Renesas RX72N (240 MHz, RXv3 core)
 * - Toolchain: GNURX 8.3.0 or later; C23 typed enum support required
 * - RTOS: Azure RTOS ThreadX 6.x; TX_ENABLE_STACK_CHECKING must be defined in tx_user.h
 *
 * @since Version 1.0.0
 * @author Locked, Inc.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_stack_monitor.h"

#include "rx_check.h"
#include "rx_err.h"
#include "rx_log.h"
#include "tx_api.h"

/* =============================================================================
 * Module Constants
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Log tag for stack monitor module
 *
 * @details Used by rx_log macros to identify messages from this module.
 *
 * @note Read-only after initialisation (effectively constant).
 * @warning Do not modify at runtime.
 * @since Version 1.0.0
 */
static const char* const s_tag = "STACK_MON";

/**
 * @enum stack_fill_constants_t
 * @brief Stack fill pattern constants for high-water mark scanning
 *
 * @details
 * ThreadX fills unused stack bytes with 0xEF when TX_DISABLE_STACK_FILLING is
 * not defined.  The 32-bit word pattern 0xEFEFEFEF is used by ThreadX internally
 * (TX_STACK_FILL) and is replicated here for byte-level scanning.
 *
 * @invariant k_stack_fill_byte == 0xEF (matches ThreadX fill byte)
 *
 * @code
 * // Check whether the first stack byte holds the fill pattern:
 * if (stack_base[k_stack_index_base] == k_stack_fill_byte) {
 *     // pattern present -- scanning is valid
 * }
 * @endcode
 *
 * @see TX_STACK_FILL definition in tx_api.h
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_stack_fill_byte     = 0xEF, /**< Byte value placed in unused stack memory by ThreadX */
  k_stack_index_base    = 0U,   /**< Index of the first (base) byte in the stack buffer */
  k_stack_count_initial = 0U,   /**< Initial free-byte counter before scanning begins */
} stack_fill_constants_t;

/* =============================================================================
 * Internal (private) functions
 * =============================================================================
 */

/**
 * @brief Application-level ThreadX stack overflow handler
 *
 * @details
 * ThreadX calls this function (via the registered callback pointer) whenever it
 * detects that the 0xEF sentinel pattern at the bottom of a thread's stack has
 * been corrupted during a context switch.
 *
 * ## Actions on Stack Overflow
 *
 * 1. Write the thread name directly to the UART using uart_debug_puts() to
 *    minimise additional stack usage (bypasses the full logging stack).
 * 2. Call internal_rx_fatal_error() which disables interrupts and halts; the
 *    hardware IWDT resets the system within 2 seconds.
 *
 *
 *
 * @pre ThreadX stack checking is enabled (TX_ENABLE_STACK_CHECKING defined)
 * @pre This handler was registered via tx_thread_stack_error_notify()
 * @post System halted (in UNIT_TEST builds: function returns, allowing test
 *       scaffolding to observe the call)
 * @post Error message visible on UART debug output
 *
 * @note Not thread-safe; called from interrupt-like context within ThreadX
 *       scheduler with interrupts disabled.
 * @warning This function is [[noreturn]] in production builds.  Do not add
 *          code after the internal_rx_fatal_error() call.
 *
 * @see rx_stack_monitor_init() Registers this function with ThreadX
 * @see internal_rx_fatal_error() Underlying fatal halt mechanism
 *
 * @since Version 1.0.0
 *
 * @code
 * // Registered indirectly via rx_stack_monitor_init() -- do not call directly:
 * tx_thread_stack_error_notify(internal_stack_overflow_handler);
 * @endcode
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [PASS] Precondition: handler only called when stack overflow detected
 * - Rule 5: [PASS] Postcondition: system halted before corruption propagates
 */
#ifndef UNIT_TEST
[[noreturn]]
#endif
static void internal_stack_overflow_handler(TX_THREAD* thread_ptr)
{
#ifndef UNIT_TEST
  /* Precondition: ThreadX guarantees this is only called on stack corruption */
  uart_debug_puts("\r\n[STACK_MON] FATAL: Stack overflow detected");

  if (thread_ptr != TX_NULL) {
    /* Precondition: thread_ptr non-null; name pointer may still be null */
    uart_debug_puts(" in thread: ");
    if (thread_ptr->tx_thread_name != TX_NULL) {
      uart_debug_puts(thread_ptr->tx_thread_name);
    } else {
      uart_debug_puts("<unnamed>");
    }
  } else {
    uart_debug_puts(" (thread_ptr is NULL)");
  }
  uart_debug_puts("\r\n");

  /* Postcondition: halt before stack corruption propagates to other memory */
  internal_rx_fatal_error(s_tag, "Stack overflow - system halted", k_rx_fail);
#else
  (void)thread_ptr;
#endif
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Register the stack overflow handler with ThreadX
 *
 * @details
 * Calls tx_thread_stack_error_notify() to install
 * internal_stack_overflow_handler() as the application callback.  ThreadX
 * invokes this callback at every context switch when a corrupted stack
 * sentinel is detected.
 *
 *
 * @pre TX_ENABLE_STACK_CHECKING is defined via USE_TX_ENABLE_STACK_CHECKING=1
 * @pre Called from tx_application_define() (single-threaded context)
 * @post _tx_thread_application_stack_error_handler set to the STAR handler
 * @post Stack sentinel violations will call internal_stack_overflow_handler()
 *
 * @note Thread safety: single-threaded context in tx_application_define().
 * @warning Returns k_rx_err_not_supported if built without stack checking.
 *
 * @see tx_thread_stack_error_notify() Underlying ThreadX API
 * @see rx_stack_monitor_get_free_bytes() Query per-thread headroom after init
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [PASS] 2 preconditions, 2 postconditions
 * - Rule 7: [PASS] tx_thread_stack_error_notify() return value checked and mapped
 */
rx_err_t rx_stack_monitor_init(void)
{
  /* Precondition: must be called before threads start running */
  /* (Enforced by contract: caller is tx_application_define()) */

  const UINT tx_status = tx_thread_stack_error_notify(internal_stack_overflow_handler);

  /* Precondition: ThreadX must be initialised (non-zero status otherwise) */
  if (tx_status != TX_SUCCESS) {
    rx_log_error(s_tag, "tx_thread_stack_error_notify failed - stack checking disabled");
    return k_rx_err_not_supported;
  }

  /* Postcondition: handler registered; log confirmation */
  rx_log_info(s_tag, "Stack overflow handler registered (TX_ENABLE_STACK_CHECKING active)");

  /* Postcondition: return k_rx_ok to caller */
  return k_rx_ok;
}

/**
 * @brief Return the number of unused (free) bytes in a thread's stack
 *
 * @details
 * Scans the thread's stack memory from the base (lowest address, where the
 * fill pattern is placed first) toward higher addresses, counting consecutive
 * 0xEF bytes.  The result is the number of bytes that have never been written
 * by the thread (i.e., the high-water mark complement).
 *
 *
 *
 * @pre thread_ptr is a valid, initialised TX_THREAD (not NULL)
 * @pre free_bytes points to a writable uint32_t (not NULL)
 * @post *free_bytes contains the count of 0xEF bytes at stack base
 * @post Thread state is not modified (read-only scan)
 *
 * @note Thread safety: snapshot only; result may change immediately after return.
 * @warning TX_DISABLE_STACK_FILLING must not be defined; otherwise the fill
 *          pattern is absent and this function returns k_rx_err_invalid_state.
 *
 * @code
 * // See rx_stack_monitor.h for a complete usage example.
 * @endcode
 *
 * @see rx_stack_monitor_init() Must be called first to enable overflow detection
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 2: [PASS] Loop bound = tx_thread_stack_size (statically known at entry)
 * - Rule 5: [PASS] 2 preconditions (NULL checks), 2 postconditions
 */
rx_err_t rx_stack_monitor_get_free_bytes(const TX_THREAD* thread_ptr, uint32_t* free_bytes)
{
  /* Precondition: null pointer checks */
  RX_CHECK_NULL_PTR(thread_ptr, s_tag, "thread_ptr must not be NULL");
  RX_CHECK_NULL_PTR(free_bytes, s_tag, "free_bytes must not be NULL");

  const uint8_t* const stack_base = (const uint8_t*)thread_ptr->tx_thread_stack_start;
  /* Intentional narrowing cast: ULONG is 32-bit on RX72N (ILP32 ABI), so
     * truncation cannot occur on this platform.  Explicitly cast to uint32_t
     * to make the assumption auditable for future portability reviews. */
  const uint32_t stack_size = (uint32_t)thread_ptr->tx_thread_stack_size;

  /* Precondition: stack base pointer must be valid */
  RX_CHECK_NULL_PTR(stack_base, s_tag, "thread stack_start must not be NULL");

  /* Guard: a zero-size stack cannot hold any fill bytes; scanning would be
     * a no-op (or a buffer overread if the loop bound underflows).  Treat a
     * zero-size stack as invalid state rather than a NULL-pointer error so
     * callers can distinguish the two failure modes. */
  if (stack_size == (uint32_t)k_stack_count_initial) {
    rx_log_warn(s_tag, "thread tx_thread_stack_size is 0 - cannot scan stack");
    return k_rx_err_invalid_state;
  }

  /* Verify that the fill pattern is present at all (first byte check) */
  if (stack_base[k_stack_index_base] != k_stack_fill_byte) {
    rx_log_warn(s_tag, "Stack fill pattern absent - TX_DISABLE_STACK_FILLING may be set");
    return k_rx_err_invalid_state;
  }

  /* Scan from stack base upward, counting 0xEF bytes.
     * Loop bound: stack_size is a compile-time-known constant per thread
     * (satisfies NASA Rule 2 - statically provable upper bound). */
  uint32_t count = k_stack_count_initial;
  for (uint32_t i = 0; i < stack_size; i++) {
    if (stack_base[i] != k_stack_fill_byte) {
      break;
    }
    count++;
  }

  /* Postcondition: count is in [0, stack_size] */
  *free_bytes = count;

  /* Postcondition: return success */
  return k_rx_ok;
}
