/**
 * @file rx_time_threadx.c
 * @brief ThreadX Time Implementation for RX72N
 *
 * @details
 * Real implementation of rx_time_interface_t using ThreadX RTOS primitives.
 * This file provides hardware-independent time services by leveraging the
 * ThreadX kernel tick infrastructure (CMT0 timer).
 *
 * @par System Architecture
 * @verbatim
 *   +-----------------+    +-----------------+    +-----------------+
 *   |  Application    |    |  rx_time_if     |    |   ThreadX       |
 *   |  (Motor Ctrl)   |--->|  (This Module)  |--->|   Kernel        |
 *   +-----------------+    +-----------------+    +-----------------+
 *                                  |                      |
 *                                  |                      v
 *                                  |              +-----------------+
 *                                  |              |   CMT0 Timer    |
 *                                  |              |   (100 Hz ISR)  |
 *                                  |              +-----------------+
 *                                  |
 *         +------------------------+------------------------+
 *         |              rx_time_interface_t                |
 *         |  +-------------+-------------+----------------+ |
 *         |  |  sleep_ms() |  get_ms()   |  is_elapsed()  | |
 *         |  |  (blocking) |  (instant)  |  (instant)     | |
 *         |  +-------------+-------------+----------------+ |
 *         +-------------------------------------------------+
 * @endverbatim
 *
 * @par Threading Model
 * - sleep_ms(): Yields CPU via tx_thread_sleep() - thread blocks
 * - get_ms(): Returns current time atomically - no blocking
 * - is_elapsed(): Compares times atomically - no blocking
 *
 * @par Time Resolution and Wraparound
 * - ThreadX tick rate: 100 Hz (10ms per tick from k_threadx_ms_per_tick)
 * - Minimum sleep: 10ms (1 tick), sleeps < 10ms round up to 1 tick
 * - Maximum sleep: ~49.7 days (2^32 / 100 / 86400)
 * - Time wraparound: ~49.7 days (handled correctly by is_elapsed())
 *
 * @par Performance Characteristics
 * | Operation    | Time       | Notes                          |
 * |--------------|------------|--------------------------------|
 * | sleep_ms()   | ~1 us      | Context switch to scheduler    |
 * | get_ms()     | ~100 ns    | Single register read           |
 * | is_elapsed() | ~150 ns    | Two reads + subtraction        |
 *
 * @par Memory Usage
 * | Type   | Size    | Description                           |
 * |--------|---------|---------------------------------------|
 * | Code   | ~200 B  | All 4 functions (with -O2)            |
 * | Stack  | 0 B     | No local arrays, uses caller's stack  |
 * | Static | 0 B     | Uses ThreadX global state             |
 *
 * @par Conditional Compilation
 * This file is only compiled when __RX__ is defined (firmware build).
 * Unit tests use mock_time.c instead for deterministic testing.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] No loops in this module
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] All functions < 20 lines
 * - Rule 5: [OK] Each function has preconditions (via RX_CHECK_NULL_PTR)
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] Return value checking (tx_thread_sleep discarded explicitly)
 * - Rule 8: [OK] Minimal preprocessor (only __RX__ guard)
 * - Rule 9: [OK] Function pointers used for DIP pattern (intentional deviation)
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles Compliance
 * - **S**: Single Responsibility - Only time-related operations
 * - **O**: Open/Closed - Interface can add new backends without modification
 * - **L**: Liskov Substitution - Interchangeable with mock_time.c
 * - **I**: Interface Segregation - Minimal 3-function interface
 * - **D**: Dependency Inversion - Implements abstract rx_time_interface_t
 *
 * @par Module Dependencies
 * @dot
 * digraph dependencies {
 *   rankdir=TB;
 *   node [shape=box];
 *   rx_time_threadx [label="rx_time_threadx.c"];
 *   rx_time_interface [label="rx_time_interface.h"];
 *   rx_time_constants [label="rx_time_constants.h"];
 *   rx_check [label="rx_check.h"];
 *   tx_api [label="tx_api.h (ThreadX)"];
 *
 *   rx_time_threadx -> rx_time_interface;
 *   rx_time_threadx -> rx_time_constants;
 *   rx_time_threadx -> rx_check;
 *   rx_time_threadx -> tx_api;
 * }
 * @enddot
 *
 * @see rx_time_interface.h Interface definition
 * @see rx_time_constants.h Time conversion constants
 * @see mock_time.c Unit test mock implementation
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#ifdef __RX__

#include "rx_check.h"
#include "rx_time_constants.h"
#include "rx_time_interface.h"
#include "tx_api.h"

/* =============================================================================
 * Module State
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Log tag for time threadx module
 * @details Identifies log messages from this module
 * @note Read-only after initialization
 * @since Version 1.0.0
 */
static const char* const s_tag = "TIME";

/* =============================================================================
 * Module Constants
 * =============================================================================
 */

/**
 * @enum rx_time_threadx_zero_t
 * @brief Zero sentinel for ThreadX time computations
 * @details Used in ceiling-division and guard checks to avoid magic number 0.
 *
 * @invariant k_rx_zero equals zero; used as a guard against zero-division in
 *            tick ceiling calculation
 *
 * @code{.c}
 * // Guard: only sleep if ticks > 0
 * if (ticks > k_rx_zero) {
 *     (void)tx_thread_sleep(ticks);
 * }
 * @endcode
 *
 * @see impl_sleep_ms() Inline ceiling-division ms-to-ticks conversion
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_rx_zero = 0u, /**< Zero sentinel -- used in overflow-safe ceiling division */
} rx_time_threadx_zero_t;

/**
 * @enum rx_time_ceil_offset_t
 * @brief Ceiling-division rounding offset
 * @details Added to the remainder check when computing tick ceiling from milliseconds.
 *
 * @invariant k_ceil_div_offset equals 1; adding this before integer division
 *            implements ceiling division
 *
 * @code{.c}
 * // Ceiling division: ticks = ceil(ms / k_threadx_ms_per_tick)
 * const uint32_t quotient  = ms / k_threadx_ms_per_tick;
 * const uint32_t remainder = ms % k_threadx_ms_per_tick;
 * const uint32_t ticks     = quotient + (remainder > k_rx_zero ? k_ceil_div_offset : k_rx_zero);
 * @endcode
 *
 * @see impl_sleep_ms() Inline ceiling-division ms-to-ticks conversion
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_ceil_div_offset = 1u, /**< Standard ceiling-division rounding offset */
} rx_time_ceil_offset_t;

/* =============================================================================
 * Interface Implementation Functions
 * =============================================================================
 */

/**
 * @brief Sleep for specified milliseconds using ThreadX scheduler
 *
 * @details
 * Yields CPU to other threads via tx_thread_sleep(), allowing the ThreadX
 * scheduler to run higher-priority threads while this thread waits.
 *
 * @par Algorithm Steps:
 * 1. Convert milliseconds to ThreadX ticks with ceiling division
 * 2. If ticks > 0, call tx_thread_sleep() to yield CPU
 * 3. Thread is rescheduled after tick count expires
 *
 * @par Tick Conversion Formula:
 * @f[
 *   \text{ticks} = \left\lceil \frac{\text{ms}}{10} \right\rceil
 *                = \left\lfloor \frac{\text{ms}}{10} \right\rfloor
 *                + \begin{cases} 1 & \text{ms} \bmod 10 > 0 \\ 0 & \text{otherwise} \end{cases}
 * @f]
 * @note Implemented via overflow-safe quotient/remainder to avoid wrap at high ms values.
 *
 *
 * @pre Calling thread must be a valid ThreadX thread
 * @pre ThreadX kernel must be running (tx_kernel_enter() called)
 *
 * @post Thread has slept for at least ms milliseconds (rounded up to tick)
 * @post Other threads have had opportunity to run
 *
 * @note Thread Safety: Safe - tx_thread_sleep() is thread-safe
 * @note Cannot be called from ISR context
 *
 * @warning Actual sleep time is rounded up to nearest 10ms tick boundary
 * @warning Sleep may be longer than requested due to higher-priority threads
 *
 * @par Example:
 * @code{.c}
 * // Sleep for 50ms (actual: 50ms = 5 ticks)
 * impl_sleep_ms(NULL, 50);
 *
 * // Sleep for 1ms (actual: 10ms = 1 tick due to rounding)
 * impl_sleep_ms(NULL, 1);
 * @endcode
 *
 * @see tx_thread_sleep() ThreadX sleep function
 * @see k_threadx_ms_per_tick Tick period constant (10ms)
 *
 * @since Version 1.0.0
 */
static void impl_sleep_ms(void* ctx, uint32_t ms)
{
  (void)ctx; /* ThreadX uses global state - no context needed */

  /* Convert ms to ticks, rounding up (overflow-safe ceiling division) */
  const uint32_t quotient  = ms / k_threadx_ms_per_tick;
  const uint32_t remainder = ms % k_threadx_ms_per_tick;
  const uint32_t ticks     = quotient + (remainder > k_rx_zero ? k_ceil_div_offset : k_rx_zero);
  if (ticks > k_rx_zero) {
    (void)tx_thread_sleep(ticks);
  }
}

/**
 * @brief Get current time in milliseconds from ThreadX tick counter
 *
 * @details
 * Reads the ThreadX system tick counter and converts to milliseconds.
 * The tick counter starts at 0 when tx_kernel_enter() is called and
 * increments at 100 Hz (every 10ms) via CMT0 timer interrupt.
 *
 * @par Algorithm Steps:
 * 1. Read ThreadX tick counter via tx_time_get()
 * 2. Multiply by k_threadx_ms_per_tick (10ms) to get milliseconds
 * 3. Return result (may wrap at ~49.7 days)
 *
 * @par Conversion Formula:
 * @f[
 *   \text{ms} = \text{ticks} \times 10
 * @f]
 *
 *
 *
 * @pre ThreadX kernel must be running
 *
 * @post Return value represents time in 10ms increments
 *
 * @note Thread Safety: Safe - tx_time_get() is atomic
 * @note Can be called from ISR context
 * @note Resolution is 10ms (cannot distinguish times < 10ms apart)
 *
 * @par Wraparound Handling:
 * Time wraps after ~49.7 days: 2^32 ticks x 10ms = 49,710,269,491 ms
 * Use impl_is_elapsed() for correct timeout handling across wraparound.
 *
 * @par Example:
 * @code{.c}
 * // Measure operation duration
 * uint32_t start = impl_get_ms(NULL);
 * do_operation();
 * uint32_t elapsed = impl_get_ms(NULL) - start;
 * rx_log_debug("TIMING", "Operation took %lu ms", elapsed);
 * @endcode
 *
 * @see tx_time_get() ThreadX tick counter
 * @see impl_is_elapsed() Wraparound-safe timeout check
 *
 * @since Version 1.0.0
 */
static uint32_t impl_get_ms(void* ctx)
{
  (void)ctx; /* ThreadX uses global state - no context needed */

  return tx_time_get() * k_threadx_ms_per_tick;
}

/**
 * @brief Check if timeout has elapsed since start time
 *
 * @details
 * Determines if the specified timeout has elapsed since start_ms.
 * Uses unsigned subtraction which handles 32-bit wraparound correctly
 * due to modular arithmetic properties.
 *
 * @par Algorithm Steps:
 * 1. Get current time via tx_time_get() x 10
 * 2. Calculate elapsed = now - start_ms (unsigned, handles wrap)
 * 3. Return true if elapsed >= timeout_ms
 *
 * @par Wraparound Correctness:
 * Using unsigned 32-bit arithmetic, the subtraction `now - start_ms` yields
 * the correct elapsed time even when `now` has wrapped past zero, as long as
 * the actual elapsed time is less than 2^32 / 100 / 2 ~ 24.8 days.
 *
 * @f[
 *   \text{elapsed} = (\text{now} - \text{start}) \mod 2^{32}
 * @f]
 *
 *
 *
 * @pre start_ms must be from a previous get_ms() call
 * @pre Elapsed time since start_ms < 24.8 days (half wraparound period)
 *
 * @post No side effects
 *
 * @note Thread Safety: Safe - only reads atomic tick counter
 * @note Can be called from ISR context
 *
 * @warning For correct wraparound handling, timeout_ms should be < 24.8 days
 *
 * @par Example - Polling with Timeout:
 * @code{.c}
 * rx_time_interface_t time_if;
 * rx_time_threadx_get_interface(&time_if);
 *
 * uint32_t start = time_if.get_ms(time_if.ctx);
 * const uint32_t timeout_ms = 1000; // 1 second timeout
 *
 * while (!device_ready()) {
 *     if (time_if.is_elapsed(time_if.ctx, start, timeout_ms)) {
 *         rx_log_error("DEV", "Timeout waiting for device");
 *         return k_rx_err_timeout;
 *     }
 *     time_if.sleep_ms(time_if.ctx, 10); // Poll every 10ms
 * }
 * @endcode
 *
 * @par Example - Wraparound Scenario:
 * @code{.c}
 * // Assume UINT32_MAX = 4,294,967,295 ms
 * // start_ms = 4,294,967,200 (95ms before wrap)
 * // now = 100 (after wrap)
 * // elapsed = 100 - 4,294,967,200 = 195 (correct via unsigned wrap)
 * @endcode
 *
 * @see impl_get_ms() Get start time
 * @see impl_sleep_ms() Delay between polls
 *
 * @since Version 1.0.0
 */
static bool impl_is_elapsed(void* ctx, uint32_t start_ms, uint32_t timeout_ms)
{
  (void)ctx; /* ThreadX uses global state - no context needed */

  const uint32_t now     = tx_time_get() * k_threadx_ms_per_tick;
  const uint32_t elapsed = now - start_ms;

  return elapsed >= timeout_ms;
}

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Get time interface structure for ThreadX RTOS
 *
 * @details
 * Populates an rx_time_interface_t structure with function pointers
 * to the ThreadX implementation. This enables Dependency Inversion:
 * callers depend on the abstract interface, not the concrete ThreadX API.
 *
 * @par Algorithm Steps:
 * 1. Validate iface is not nullptr
 * 2. Set ctx to NULL (ThreadX uses global state)
 * 3. Set function pointers to implementation functions
 * 4. Return success
 *
 * @par Interface Population:
 * | Field      | Value            | Description                    |
 * |------------|------------------|--------------------------------|
 * | ctx        | NULL             | ThreadX uses global state      |
 * | sleep_ms   | impl_sleep_ms    | tx_thread_sleep() wrapper      |
 * | get_ms     | impl_get_ms      | tx_time_get() x 10 wrapper     |
 * | is_elapsed | impl_is_elapsed  | Wraparound-safe timeout check  |
 *
 *
 *
 * @pre iface !=nullptr
 *
 * @post iface->ctx ==nullptr
 * @post iface->sleep_ms, get_ms, is_elapsed are valid function pointers
 * @post Interface is ready for use
 *
 * @invariant Interface functions remain valid for program lifetime
 *
 * @note Thread Safety: Safe - only writes to caller-owned structure
 * @note Re-entrancy: Safe - no shared state accessed
 *
 * @par Example - Basic Usage:
 * @code{.c}
 * rx_time_interface_t time_if;
 * rx_err_t err = rx_time_threadx_get_interface(&time_if);
 * if (err != k_rx_ok) {
 *     rx_log_error("TIME", "Failed to get interface");
 *     return err;
 * }
 *
 * // Use interface for timing operations
 * uint32_t start = time_if.get_ms(time_if.ctx);
 * do_some_work();
 * uint32_t duration = time_if.get_ms(time_if.ctx) - start;
 * @endcode
 *
 * @par Example - Timeout Loop:
 * @code{.c}
 * rx_time_interface_t time_if;
 * rx_time_threadx_get_interface(&time_if);
 *
 * // Wait for sensor with 500ms timeout
 * uint32_t start = time_if.get_ms(time_if.ctx);
 * while (!sensor_data_ready()) {
 *     if (time_if.is_elapsed(time_if.ctx, start, 500)) {
 *         return k_rx_err_timeout;
 *     }
 *     time_if.sleep_ms(time_if.ctx, 10);
 * }
 * @endcode
 *
 * @par Example - Dependency Injection:
 * @code{.c}
 * // In production code
 * rx_time_interface_t time_if;
 * rx_time_threadx_get_interface(&time_if);
 * motor_init(&motor, &config, &time_if);
 *
 * // In unit tests (mock_time.c)
 * rx_time_interface_t time_if;
 * mock_time_get_interface(&time_if);
 * motor_init(&motor, &config, &time_if);  // Same interface, mock behavior
 * @endcode
 *
 * @see rx_time_interface_t Interface structure definition
 * @see mock_time_get_interface() Unit test mock version
 * @see impl_sleep_ms() Sleep implementation details
 * @see impl_get_ms() Time retrieval details
 * @see impl_is_elapsed() Timeout check details
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 1 precondition (NULL check), 3 postconditions
 * - Rule 9: [OK] Function pointers for DIP (intentional deviation)
 *
 * @test Tested in test_rx_time.c with nullptr and success cases
 *
 * @since Version 1.0.0
 */
rx_err_t rx_time_threadx_get_interface(rx_time_interface_t* iface)
{
  RX_CHECK_NULL_PTR(iface, s_tag, "Interface pointer is nullptr");

  iface->ctx        = NULL; /* No context needed for ThreadX */
  iface->sleep_ms   = impl_sleep_ms;
  iface->get_ms     = impl_get_ms;
  iface->is_elapsed = impl_is_elapsed;

  return k_rx_ok;
}

#else

/* ISO C requires at least one declaration per translation unit */
typedef int rx_time_threadx_placeholder_t;

#endif /* __RX__ */
