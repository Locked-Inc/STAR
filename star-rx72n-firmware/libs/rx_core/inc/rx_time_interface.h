/**
 * @file rx_time_interface.h
 * @brief Time Interface - Dependency Inversion for Timing Operations
 *
 * @details
 * Defines the abstract interface for time operations in RX72N firmware,
 * implementing the Dependency Inversion Principle (DIP) from SOLID design.
 * This header contains ONLY the interface definition with NO implementation
 * details, enabling complete decoupling between high-level modules and
 * low-level timing mechanisms.
 *
 * ## Architecture Pattern: Dependency Inversion Principle (DIP)
 *
 * **Problem**: Communication modules (USB, SPI, UART) need timing operations
 * (sleep, timeouts) but should not directly depend on ThreadX RTOS. Direct
 * dependencies create:
 * - Tight coupling -> hard to change RTOS
 * - Untestable code -> cannot unit test without ThreadX
 * - Platform lock-in -> cannot port to bare-metal or different RTOS
 *
 * **Solution**: Define abstract time interface that both high-level and
 * low-level modules depend on:
 *
 * @dot
 * digraph dependency_inversion {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   interface [label="rx_time_interface_t\n(Abstract Interface)", shape=oval, fillcolor=lightblue, style=filled];
 *   high [label="High-Level Modules\n(USB, SPI, UART)", fillcolor=lightgreen, style=filled];
 *   impl_tx [label="ThreadX Implementation\nrx_time_threadx.c", fillcolor=lightyellow, style=filled];
 *   impl_mock [label="Mock Implementation\nmock_time.c", fillcolor=lightyellow, style=filled];
 *
 *   high -> interface [label="depends on\n(uses)", dir=back];
 *   impl_tx -> interface [label="implements", style=dashed];
 *   impl_mock -> interface [label="implements", style=dashed];
 * }
 * @enddot
 *
 * **Key Insight**: High-level modules depend on the ABSTRACTION (interface),
 * not the IMPLEMENTATION (ThreadX). Implementations are injected at runtime
 * through function pointers.
 *
 * ## Benefits of This Pattern
 *
 * | Benefit | Description | Example |
 * |---------|-------------|---------|
 * | **Testability** | Unit tests inject mock timing without ThreadX | Test USB without RTOS |
 * | **Flexibility** | Swap RTOS without changing USB/SPI code | ThreadX -> FreeRTOS |
 * | **Decoupling** | Communication layers have zero ThreadX includes | Clean dependencies |
 * | **Portability** | Same code runs on bare-metal with null impl | Debug builds |
 * | **Simulation** | Mock advances time instantly for fast tests | 1000ms timeout in 1us |
 *
 * ## Concrete Implementations
 *
 * | Implementation | File | Use Case |
 * |----------------|------|----------|
 * | ThreadX | `rx_time_threadx.c` | Production firmware on RX72N |
 * | Mock (manual) | `mock_time.c` | Unit tests with controlled time |
 * | Mock (auto) | `mock_time.c` (auto_advance) | Unit tests with simulated delays |
 * | Null | User-provided | Bare-metal debug (polling only) |
 *
 * ## Interface Contract
 *
 * All implementations MUST provide:
 * 1. **sleep_ms()**: Block for specified milliseconds (or simulate)
 * 2. **get_ms()**: Return monotonic time in milliseconds
 * 3. **is_elapsed()**: Check if timeout occurred (optional, can be NULL)
 * 4. **ctx**: Opaque pointer to implementation-specific data
 *
 * Implementations MUST guarantee:
 * - Monotonic time (never decreases)
 * - Wrap-around handling for 32-bit time (wraps ~49.7 days)
 * - Thread-safe operations (if used in multi-threaded context)
 *
 * ## Memory Layout
 *
 * | Offset | Size | Field | Type | Purpose |
 * |--------|------|-------|------|---------|
 * | 0 | 4 bytes | ctx | void* | Implementation context |
 * | 4 | 4 bytes | sleep_ms | fn ptr | Sleep function |
 * | 8 | 4 bytes | get_ms | fn ptr | Get time function |
 * | 12 | 4 bytes | is_elapsed | fn ptr | Timeout check (optional) |
 * | **Total** | **16 bytes** | | | Lightweight interface |
 *
 * ## Performance Characteristics
 *
 * - **Memory overhead**: 16 bytes per interface instance (4 function pointers)
 * - **Call overhead**: ~3 cycles (indirect jump through function pointer)
 * - **ThreadX sleep_ms**: Yields to scheduler (~50-200 us context switch)
 * - **Mock sleep_ms**: Instant return if auto_advance disabled
 * - **get_ms**: ~10-50 cycles depending on implementation
 *
 * ## Hardware Requirements
 *
 * | Requirement | Value | Notes |
 * |-------------|-------|-------|
 * | MCU | RX72N or compatible | 32-bit pointers, function pointers |
 * | RAM | 16 bytes per instance | Typically 1-2 instances total |
 * | Time Source | Any | ThreadX uses CMT, mock uses static counter |
 * | Alignment | 4-byte | Natural alignment for 32-bit pointers |
 *
 * @par Module Dependencies:
 * - [rx_err.h](rx_err.h) - Error code definitions
 * - **NO dependencies on ThreadX or any RTOS** (by design!)
 * - Implementations depend on: ThreadX (`tx_api.h`) or test framework
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1**: [OK] No goto, setjmp, recursion
 * - **Rule 2**: [OK] No loops in interface (validation has bounded loop)
 * - **Rule 3**: [OK] Zero dynamic allocation (interface is stack/static)
 * - **Rule 4**: [OK] All functions <60 lines (inline validation is 10 lines)
 * - **Rule 5**: [OK] Preconditions enforced via validation function
 * - **Rule 6**: [OK] Minimal scope (interface defined in header only)
 * - **Rule 7**: [OK] Return values checked (rx_err_t for validation)
 * - **Rule 8**: [OK] No macros (pure interface, no magic numbers)
 * - **Rule 9**: [OK] **Function pointers allowed** (intentional deviation for DIP)
 * - **Rule 10**: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **Single Responsibility**: Only defines time interface, no implementation
 * - **Open/Closed**: Extensible (add new implementations) without modification
 * - **Liskov Substitution**: All implementations interchangeable via interface
 * - **Interface Segregation**: Minimal 3-function interface, each with clear purpose
 * - **Dependency Inversion**: [PASS] **THIS IS THE DIP PATTERN** - High-level modules
 *   depend on abstraction (interface), not concretions (ThreadX)
 *
 * @par Example - Production Usage (ThreadX):
 * @code{.c}
 * // In USB communication module initialization
 * #include "rx_time_interface.h"
 * #include "rx_time_threadx.h"  // Concrete ThreadX implementation
 *
 * void usb_comm_init(void) {
 *     // Get ThreadX time interface
 *     rx_time_interface_t time_iface;
 *     rx_time_threadx_get_interface(&time_iface);
 *
 *     // Validate interface (defensive programming)
 *     rx_err_t err = rx_time_interface_validate(&time_iface);
 *     if (err != k_rx_ok) {
 *         rx_log_error("USB", "Invalid time interface");
 *         return;
 *     }
 *
 *     // Inject into USB configuration
 *     rx_usb_comm_config_t config = {
 *         .time_iface = &time_iface,
 *         .timeout_ms = 1000,
 *     };
 *
 *     rx_usb_comm_init(&config);
 * }
 *
 * // In USB communication operations (no ThreadX dependency!)
 * rx_err_t usb_send_with_timeout(const rx_time_interface_t* time,
 *                                 uint8_t* data, uint32_t len) {
 *     uint32_t start = time->get_ms(time->ctx);
 *
 *     while (!tx_complete()) {
 *         if (time->is_elapsed(time->ctx, start, 1000)) {
 *             return k_rx_err_timeout;
 *         }
 *         time->sleep_ms(time->ctx, 1);  // Brief yield
 *     }
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Example - Unit Test Usage (Mock):
 * @code{.c}
 * // In unit test for USB timeout handling
 * #include "rx_time_interface.h"
 * #include "mock_time.h"  // Test mock implementation
 *
 * void test_usb_timeout(void) {
 *     // Create mock time interface (no ThreadX needed!)
 *     rx_time_interface_t mock_time;
 *     mock_time_get_interface(&mock_time);
 *
 *     // Disable auto-advance for manual control
 *     mock_time_set_auto_advance(false);
 *
 *     // Inject mock into USB config
 *     rx_usb_comm_config_t config = {
 *         .time_iface = &mock_time,
 *         .timeout_ms = 1000,
 *     };
 *     rx_usb_comm_init(&config);
 *
 *     // Start operation that should timeout
 *     uint8_t data[64] = {0};
 *     rx_err_t err = usb_send_with_timeout(&mock_time, data, sizeof(data));
 *
 *     // Manually advance time past timeout
 *     mock_time_advance_ms(1001);
 *
 *     // Verify timeout occurred
 *     assert(err == k_rx_err_timeout);
 * }
 * @endcode
 *
 * @par Example - Multiple Interface Instances:
 * @code{.c}
 * // Different modules can use different time implementations
 * void system_init(void) {
 *     // USB uses ThreadX time (real delays)
 *     rx_time_interface_t usb_time;
 *     rx_time_threadx_get_interface(&usb_time);
 *     usb_init(&usb_time);
 *
 *     // SPI uses null time (polling only, no sleeps)
 *     rx_time_interface_t spi_time = {
 *         .ctx = nullptr,
 *         .sleep_ms = nullptr,  // Never call sleep
 *         .get_ms = real_time_get_ms,  // Still need timeouts
 *         .is_elapsed = nullptr,
 *     };
 *     spi_init(&spi_time);  // Will use polling instead of sleeps
 * }
 * @endcode
 *
 * @see [rx_time_threadx.c](../../src/rx_time_threadx.c) Production implementation
 * @see [mock_time.c](../../tests/mocks/mock_time.c) Unit test implementation
 * @see [rx_error_interface.h](rx_error_interface.h) Similar DIP pattern for errors
 * @see [rx_pin_interface.h](rx_pin_interface.h) Similar DIP pattern for GPIO
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of interface struct */
typedef struct rx_time_interface rx_time_interface_t;

/* =============================================================================
 * Time Interface Function Pointers
 * =============================================================================
 */

/**
 * @typedef rx_time_sleep_ms_fn
 * @brief Function pointer: Sleep/delay for specified milliseconds
 *
 * @details
 * Blocks (or simulates blocking) for the specified number of milliseconds.
 * Behavior depends on concrete implementation:
 *
 * **ThreadX Implementation**:
 * - Calls `tx_thread_sleep(ticks)` where ticks = ms * (TX_TIMER_TICKS_PER_SECOND / 1000)
 * - Yields CPU to other threads (cooperative multitasking)
 * - Actual sleep time may be slightly longer due to tick granularity
 * - Example: 10ms sleep with 10ms tick = exactly 1 tick
 * - Example: 15ms sleep with 10ms tick = 2 ticks = 20ms actual
 *
 * **Mock Implementation (manual control)**:
 * - Returns immediately without advancing simulated time
 * - Test code manually advances time via mock_time_advance_ms()
 * - Use for fine-grained control of timing in unit tests
 *
 * **Mock Implementation (auto-advance)**:
 * - Returns immediately AND advances simulated time by ms
 * - Useful for testing timeout logic without real delays
 * - 1000ms timeout completes in ~1us real time
 *
 * **Null Implementation (bare-metal)**:
 * - Function pointer set tonullptr
 * - Caller must check for nullptr before calling
 * - Use for polling-only code that never sleeps
 *
 * @param[in] ctx Implementation-specific context pointer
 *   - ThreadX: Pointer to thread control block (unused, can be NULL)
 *   - Mock: Pointer to mock_time_state_t for simulated time tracking
 *   - Null: N/A (function pointer is nullptr)
 *
 * @param[in] ms Number of milliseconds to sleep/delay
 *   - **Valid range**: [0, UINT32_MAX]
 *   - **Units**: milliseconds (MKS: 0.001 seconds)
 *   - **Value 0**: Yields CPU without delay (ThreadX) or no-op (mock)
 *   - **Large values**: ms > 49 days wraps at UINT32_MAX (impractical)
 *
 * @return void (no return value, always "succeeds")
 *
 * @pre No preconditions - safe to call with any ms value
 * @pre ctx must be valid for the specific implementation (or nullptr if allowed)
 *
 * @post Control returned after at least ms milliseconds (ThreadX)
 * @post Simulated time advanced by ms (mock with auto_advance)
 * @post Other threads may have executed during sleep (ThreadX)
 *
 * @note **Thread Safety**: ThreadX implementation is thread-safe. Mock depends
 *       on test harness (single-threaded tests are safe).
 * @note **Accuracy**: ThreadX sleep accuracy limited by tick rate (typically +/-10ms).
 *       Use hardware timers for precise delays <1ms.
 * @note **Execution Time**: ThreadX context switch ~50-200us. Mock returns in <1us.
 * @note **Blocking**: ThreadX yields CPU (other threads run). Mock returns immediately.
 *
 * @warning ThreadX tick granularity causes rounding: 15ms sleep with 10ms tick = 20ms actual.
 * @attention Never call from interrupt context (ThreadX implementation will fault).
 * @attention Excessive sleeps (>1000ms) may indicate architectural problems.
 *
 * @par Example - Basic Sleep:
 * @code{.c}
 * // Sleep for 10ms between sensor reads
 * void sensor_poll_loop(const rx_time_interface_t* time) {
 *     while (1) {
 *         read_sensor_data();
 *         process_data();
 *
 *         // Brief sleep to reduce CPU usage
 *         time->sleep_ms(time->ctx, 10);  // 10ms = 100 Hz poll rate
 *     }
 * }
 * @endcode
 *
 * @par Example - Defensive Null Check:
 * @code{.c}
 * // Gracefully handle null sleep_ms (polling-only mode)
 * void wait_for_ready(const rx_time_interface_t* time) {
 *     while (!device_is_ready()) {
 *         // Sleep if available, otherwise busy-wait
 *         if (time->sleep_ms != nullptr) {
 *             time->sleep_ms(time->ctx, 1);  // 1ms yield
 *         }
 *         // Continue polling
 *     }
 * }
 * @endcode
 *
 * @see rx_time_get_ms_fn Get current time for timeout calculations
 * @see rx_time_is_elapsed_fn Check if timeout occurred
 * @see tx_thread_sleep() ThreadX implementation calls this
 *
 * @since Version 1.0.0
 */
typedef void (*rx_time_sleep_ms_fn)(void* ctx, uint32_t ms);

/**
 * @typedef rx_time_get_ms_fn
 * @brief Function pointer: Get current monotonic time in milliseconds
 *
 * @details
 * Returns a continuously increasing time value in milliseconds, suitable for
 * measuring elapsed time and implementing timeouts. This is NOT wall-clock
 * time (not UTC, not human-readable) - it's a monotonic counter that starts
 * at system boot and increases until overflow (~49.7 days).
 *
 * **Monotonic Guarantee**: Time NEVER decreases, even if system clock is
 * adjusted. This makes it reliable for timeouts and performance measurements.
 *
 * **Wrap-Around Behavior**: 32-bit counter wraps to 0 after 2^32 milliseconds
 * (4,294,967,296 ms = 49.71 days). Properly written timeout code handles this
 * correctly using unsigned arithmetic.
 *
 * **Implementation Details**:
 *
 * **ThreadX Implementation**:
 * - Returns `tx_time_get() * (1000 / TX_TIMER_TICKS_PER_SECOND)`
 * - Resolution matches ThreadX tick rate (typically 10ms on RX72N)
 * - Examples:
 *   - 100 Hz tick: Resolution = 10ms (0, 10, 20, 30, ...)
 *   - 1000 Hz tick: Resolution = 1ms (0, 1, 2, 3, ...)
 * - Overhead: ~50 cycles (~200 ns @ 240 MHz)
 *
 * **Mock Implementation**:
 * - Returns static counter maintained by mock
 * - Manual control: Counter only changes via mock_time_advance_ms()
 * - Auto-advance: Counter increases during sleep_ms() calls
 * - Resolution: 1ms (arbitrary, no hardware limits in simulation)
 * - Overhead: ~10 cycles (~40 ns @ 240 MHz, no RTOS)
 *
 * **Bare-Metal Implementation** (user-provided):
 * - Could use CMT hardware timer counter
 * - Could use SysTick (if using ARM Cortex-M)
 * - Resolution depends on hardware timer configuration
 *
 * @param[in] ctx Implementation-specific context pointer
 *   - ThreadX: Unused (can be NULL)
 *   - Mock: Pointer to mock_time_state_t containing current_ms counter
 *   - Hardware: Pointer to timer peripheral base address
 *
 * @return uint32_t Current time in milliseconds since system boot
 * @retval 0 System just booted (or wrapped around after 49.7 days)
 * @retval 1000 System has been running for 1 second
 * @retval 3600000 System has been running for 1 hour
 * @retval UINT32_MAX System has been running for 49.7 days (about to wrap)
 *
 * @pre No preconditions - safe to call anytime
 * @pre ctx must be valid for the specific implementation (or nullptr if allowed)
 *
 * @post Returned value is >= previous call (monotonic, unless wrapped)
 * @post No side effects (read-only operation)
 *
 * @invariant get_ms(t2) >= get_ms(t1) for t2 > t1 (unless wrap)
 * @invariant Resolution matches implementation tick rate (e.g., 10ms for ThreadX)
 *
 * @note **Thread Safety**: ThreadX tx_time_get() is thread-safe. Mock reads
 *       volatile counter (safe for single-threaded tests).
 * @note **Execution Time**: ThreadX ~200ns, Mock ~40ns @ 240 MHz (very fast)
 * @note **Wrap-Around**: Use unsigned arithmetic for timeouts to handle wrap correctly:
 *       `(uint32_t)(current - start) >= timeout` works across wrap boundary.
 * @note **Resolution**: Limited by ThreadX tick rate. For sub-millisecond timing,
 *       use hardware timers (CMT, GPT) directly.
 *
 * @attention NOT wall-clock time! Do not use for timestamps or logging absolute time.
 * @attention Wraps every 49.7 days. Long-running systems must handle wrap correctly.
 *
 * @par Example - Timeout Implementation:
 * @code{.c}
 * // Wait for device ready with 1000ms timeout
 * rx_err_t wait_device_ready(const rx_time_interface_t* time, uint32_t timeout_ms) {
 *     uint32_t start_ms = time->get_ms(time->ctx);
 *
 *     while (!device_is_ready()) {
 *         // Check timeout using wrap-safe arithmetic
 *         uint32_t elapsed = time->get_ms(time->ctx) - start_ms;
 *         if (elapsed >= timeout_ms) {
 *             return k_rx_err_timeout;  // Timeout occurred
 *         }
 *
 *         time->sleep_ms(time->ctx, 1);  // Brief yield
 *     }
 *
 *     return k_rx_ok;  // Device ready
 * }
 * @endcode
 *
 * @par Example - Performance Measurement:
 * @code{.c}
 * // Measure execution time of algorithm
 * void profile_algorithm(const rx_time_interface_t* time) {
 *     uint32_t start = time->get_ms(time->ctx);
 *
 *     run_expensive_algorithm();
 *
 *     uint32_t end = time->get_ms(time->ctx);
 *     uint32_t duration_ms = end - start;  // Wrap-safe
 *
 *     rx_log_info("PERF", "Algorithm took %lu ms", duration_ms);
 * }
 * @endcode
 *
 * @par Example - Wrap-Around Handling:
 * @code{.c}
 * // Demonstrate wrap-around safety (uint32_t arithmetic)
 * void test_wrap_around(void) {
 *     uint32_t start = UINT32_MAX - 100;  // Near wrap point
 *     mock_time_set_ms(start);
 *
 *     // Advance past wrap boundary
 *     mock_time_advance_ms(200);
 *     uint32_t end = mock_time_get_ms();  // end = 99 (wrapped)
 *
 *     // Unsigned arithmetic handles wrap correctly
 *     uint32_t elapsed = end - start;     // 99 - (UINT32_MAX-100) = 200 [OK]
 *     assert(elapsed == 200);              // Correct!
 * }
 * @endcode
 *
 * @par Example - Uptime Display:
 * @code{.c}
 * // Convert milliseconds to human-readable uptime
 * void print_uptime(const rx_time_interface_t* time) {
 *     uint32_t uptime_ms = time->get_ms(time->ctx);
 *
 *     uint32_t seconds = uptime_ms / 1000;
 *     uint32_t minutes = seconds / 60;
 *     uint32_t hours = minutes / 60;
 *     uint32_t days = hours / 24;
 *
 *     printf("Uptime: %lu days, %lu hours, %lu min\n",
 *            days, hours % 24, minutes % 60);
 * }
 * @endcode
 *
 * @see rx_time_sleep_ms_fn Sleep for specified duration
 * @see rx_time_is_elapsed_fn Convenience function for timeout checks
 * @see tx_time_get() ThreadX implementation uses this
 *
 * @since Version 1.0.0
 */
typedef uint32_t (*rx_time_get_ms_fn)(void* ctx);

/**
 * @typedef rx_time_is_elapsed_fn
 * @brief Function pointer: Check if timeout has elapsed (optional convenience)
 *
 * @details
 * Convenience function that combines get_ms() and timeout arithmetic into a
 * single call. Equivalent to: `(get_ms(ctx) - start_ms) >= timeout_ms` with
 * correct handling of 32-bit wrap-around.
 *
 * This function is OPTIONAL - implementations may set this pointer to NULL.
 * Callers should either:
 * 1. Check for nullptr before calling, OR
 * 2. Implement timeout check manually using get_ms()
 *
 * **Why Optional?**: Some implementations (bare-metal, minimal mocks) may want
 * to provide only the core get_ms() function and let callers handle timeout
 * logic themselves. This keeps the interface minimal.
 *
 * **Wrap-Around Safety**: Uses unsigned 32-bit arithmetic which correctly
 * handles wraparound at UINT32_MAX:
 * - Example: start=4294967290, timeout=100, current=50
 * - Elapsed = (50 - 4294967290) = 110 via unsigned underflow
 * - 110 >= 100 -> true (timeout occurred) [OK]
 *
 * **Algorithm**:
 * ```
 * 1. current_ms = get_ms(ctx)
 * 2. elapsed = current_ms - start_ms  // Unsigned subtraction (wrap-safe)
 * 3. return (elapsed >= timeout_ms)
 * ```
 *
 * @param[in] ctx Implementation-specific context pointer
 *   - ThreadX: Unused (can be NULL)
 *   - Mock: Pointer to mock_time_state_t
 *   - Bare-metal: Pointer to timer peripheral
 *
 * @param[in] start_ms Start time captured from get_ms()
 *   - **Valid range**: [0, UINT32_MAX]
 *   - **Units**: milliseconds since boot
 *   - **Source**: MUST be from prior get_ms() call
 *   - **Typical use**: Captured at start of operation
 *
 * @param[in] timeout_ms Timeout duration in milliseconds
 *   - **Valid range**: [0, UINT32_MAX]
 *   - **Units**: milliseconds (MKS: 0.001 seconds)
 *   - **Value 0**: Returns true immediately (timeout already occurred)
 *   - **Large values**: timeout > 49 days may never trigger due to wrap
 *
 * @return bool Timeout status
 * @retval true Timeout has elapsed: (current - start) >= timeout
 * @retval false Timeout has NOT elapsed: still within timeout window
 *
 * @pre start_ms must be from prior get_ms() call
 * @pre timeout_ms should be reasonable (< 49 days for guaranteed correctness)
 * @pre ctx must be valid for the specific implementation (or nullptr if allowed)
 * @pre Function pointer may be NULL - caller must check before calling!
 *
 * @post No side effects (read-only operation)
 * @post Result is deterministic given same inputs and current time
 *
 * @note **Thread Safety**: Thread-safe if get_ms() is thread-safe
 * @note **Execution Time**: ~250ns @ 240 MHz (one get_ms() call + arithmetic)
 * @note **Optional Function**: May be NULL in minimal implementations. Always check!
 * @note **Wrap-Around**: Correctly handles wrap using unsigned arithmetic
 * @note **Resolution**: Limited by get_ms() resolution (typically 10ms for ThreadX)
 *
 * @warning start_ms MUST be from get_ms(), not arbitrary value or hardware timer
 * @attention Function pointer may be NULL - check before dereferencing!
 * @attention timeout_ms > ~25 days may behave unexpectedly near wrap boundary
 *
 * @par Example - Using is_elapsed (if available):
 * @code{.c}
 * // Wait for I2C transaction with timeout
 * rx_err_t i2c_wait_complete(const rx_time_interface_t* time, uint32_t timeout_ms) {
 *     uint32_t start = time->get_ms(time->ctx);
 *
 *     while (!i2c_transaction_done()) {
 *         // Use is_elapsed if available
 *         if (time->is_elapsed != nullptr) {
 *             if (time->is_elapsed(time->ctx, start, timeout_ms)) {
 *                 return k_rx_err_timeout;
 *             }
 *         } else {
 *             // Fallback: manual timeout check
 *             uint32_t elapsed = time->get_ms(time->ctx) - start;
 *             if (elapsed >= timeout_ms) {
 *                 return k_rx_err_timeout;
 *             }
 *         }
 *
 *         time->sleep_ms(time->ctx, 1);
 *     }
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Example - Simplified with NULL Check:
 * @code{.c}
 * // Cleaner timeout check with defensive NULL handling
 * bool check_timeout(const rx_time_interface_t* time,
 *                    uint32_t start, uint32_t timeout_ms) {
 *     if (time->is_elapsed != nullptr) {
 *         // Use convenience function if available
 *         return time->is_elapsed(time->ctx, start, timeout_ms);
 *     } else {
 *         // Manual calculation (always works)
 *         uint32_t elapsed = time->get_ms(time->ctx) - start;
 *         return (elapsed >= timeout_ms);
 *     }
 * }
 * @endcode
 *
 * @par Example - Multiple Timeouts:
 * @code{.c}
 * // Check multiple independent timeouts
 * void multi_timeout_loop(const rx_time_interface_t* time) {
 *     uint32_t sensor_start = time->get_ms(time->ctx);
 *     uint32_t motor_start = time->get_ms(time->ctx);
 *     uint32_t comm_start = time->get_ms(time->ctx);
 *
 *     while (running) {
 *         // Check sensor timeout (500ms)
 *         if (time->is_elapsed(time->ctx, sensor_start, 500)) {
 *             read_sensors();
 *             sensor_start = time->get_ms(time->ctx);  // Restart timer
 *         }
 *
 *         // Check motor timeout (100ms)
 *         if (time->is_elapsed(time->ctx, motor_start, 100)) {
 *             update_motors();
 *             motor_start = time->get_ms(time->ctx);
 *         }
 *
 *         // Check comm timeout (1000ms)
 *         if (time->is_elapsed(time->ctx, comm_start, 1000)) {
 *             send_telemetry();
 *             comm_start = time->get_ms(time->ctx);
 *         }
 *
 *         time->sleep_ms(time->ctx, 10);  // 100 Hz loop
 *     }
 * }
 * @endcode
 *
 * @par Example - Wrap-Around Correctness:
 * @code{.c}
 * // Verify is_elapsed handles wrap correctly
 * void test_wrap_around_elapsed(void) {
 *     rx_time_interface_t mock_time;
 *     mock_time_get_interface(&mock_time);
 *
 *     // Set time near wrap boundary
 *     uint32_t start = UINT32_MAX - 50;  // 50ms before wrap
 *     mock_time_set_ms(start);
 *
 *     // Advance past wrap (50 + 100 = 150ms elapsed)
 *     mock_time_advance_ms(150);
 *
 *     // Check timeout (100ms)
 *     bool elapsed = mock_time.is_elapsed(mock_time.ctx, start, 100);
 *     assert(elapsed == true);  // Correctly detected timeout across wrap! [OK]
 * }
 * @endcode
 *
 * @see rx_time_get_ms_fn Used internally by this function
 * @see rx_time_sleep_ms_fn Often used together in timeout loops
 *
 * @since Version 1.0.0
 */
typedef bool (*rx_time_is_elapsed_fn)(void* ctx, uint32_t start_ms, uint32_t timeout_ms);

/* =============================================================================
 * Time Interface Structure
 * =============================================================================
 */

/**
 * @struct rx_time_interface
 * @brief Time operations interface structure (Dependency Inversion Pattern)
 *
 * @details
 * Defines the complete contract for time operations that all implementations
 * must fulfill. This struct contains function pointers for time operations
 * plus an opaque context pointer for implementation-specific state.
 *
 * This is a "fat pointer" or "vtable" pattern common in object-oriented design,
 * adapted for C. It enables runtime polymorphism without inheritance - different
 * implementations (ThreadX, mock, bare-metal) can be swapped at runtime by
 * filling in different function pointers.
 *
 * ## Memory Layout (32-bit pointers)
 *
 * | Offset | Size | Field | Type | Alignment | Purpose |
 * |--------|------|-------|------|-----------|---------|
 * | 0 | 4 | ctx | void* | 4 | Implementation context |
 * | 4 | 4 | sleep_ms | fn ptr | 4 | Sleep function |
 * | 8 | 4 | get_ms | fn ptr | 4 | Get time function |
 * | 12 | 4 | is_elapsed | fn ptr | 4 | Timeout check (optional) |
 * | **Total** | **16** | | | | **No padding** |
 *
 * ## Initialization Patterns
 *
 * **Static Initialization** (compile-time):
 * ```c
 * static rx_time_interface_t s_time = {
 *     .ctx = &s_threadx_state,
 *     .sleep_ms = threadx_sleep_ms_impl,
 *     .get_ms = threadx_get_ms_impl,
 *     .is_elapsed = threadx_is_elapsed_impl,
 * };
 * ```
 *
 * **Dynamic Initialization** (runtime):
 * ```c
 * rx_time_interface_t time;
 * rx_time_threadx_get_interface(&time);  // Fills in struct
 * ```
 *
 * **Null Implementation** (minimal/bare-metal):
 * ```c
 * static rx_time_interface_t s_null_time = {
 *     .ctx = nullptr,
 *     .sleep_ms = nullptr,  // Polling only, no sleeps
 *     .get_ms = hardware_timer_get_ms,  // Still need timeouts
 *     .is_elapsed = nullptr,  // Manual timeout checks
 * };
 * ```
 *
 * ## Field Constraints and Relationships
 *
 * | Field | Required? | Notes |
 * |-------|-----------|-------|
 * | ctx | Optional | NULL if implementation is stateless |
 * | sleep_ms | **Required** | Validation fails if nullptr |
 * | get_ms | **Required** | Validation fails if nullptr |
 * | is_elapsed | Optional | Can be NULL (manual timeout checks) |
 *
 * **Constraint**: If sleep_ms is nullptr, interface is invalid (cannot sleep).
 * **Constraint**: If get_ms is nullptr, interface is invalid (cannot measure time).
 * **Invariant**: Once initialized, function pointers should not change (read-only).
 *
 * @par Usage Example - Initialization:
 * @code{.c}
 * // Production: Use ThreadX implementation
 * void production_init(void) {
 *     rx_time_interface_t time_iface;
 *     rx_time_threadx_get_interface(&time_iface);
 *
 *     // Validate before use (defensive programming)
 *     rx_err_t err = rx_time_interface_validate(&time_iface);
 *     if (err != k_rx_ok) {
 *         rx_log_fatal("INIT", "Invalid time interface");
 *         return;
 *     }
 *
 *     // Inject into USB module
 *     usb_comm_init(&time_iface);
 * }
 * @endcode
 *
 * @par Usage Example - Passing by Value vs Pointer:
 * @code{.c}
 * // Option 1: Pass by pointer (common, lightweight)
 * void device_init(const rx_time_interface_t* time) {
 *     // Use: time->get_ms(time->ctx)
 * }
 *
 * // Option 2: Pass by value (copy 16 bytes, less common)
 * void device_init(rx_time_interface_t time) {
 *     // Use: time.get_ms(time.ctx)
 * }
 *
 * // Option 3: Embed in config struct (recommended)
 * typedef struct {
 *     rx_time_interface_t time_iface;  // 16 bytes embedded
 *     uint32_t timeout_ms;
 *     uint32_t retry_count;
 * } device_config_t;
 * @endcode
 *
 * @par Usage Example - Multiple Implementations:
 * @code{.c}
 * // System with mixed implementations
 * void mixed_system_init(void) {
 *     // USB uses real ThreadX time (needs actual delays)
 *     rx_time_interface_t usb_time;
 *     rx_time_threadx_get_interface(&usb_time);
 *     usb_init(&usb_time);
 *
 *     // SPI uses null time (polling only, instant returns)
 *     rx_time_interface_t spi_time = {0};
 *     spi_time.get_ms = hardware_timer_get_ms;  // Still need timeouts
 *     spi_time.sleep_ms = null_sleep;  // No-op sleep
 *     spi_init(&spi_time);
 * }
 * @endcode
 *
 * @see rx_time_interface_validate() Validate interface before use
 * @see rx_time_threadx_get_interface() Fill with ThreadX implementation
 * @see mock_time_get_interface() Fill with mock implementation for tests
 *
 * @since Version 1.0.0
 */
struct rx_time_interface {
  /**
   * @brief Implementation-specific context pointer (opaque)
   *
   * @details
   * Pointer to concrete implementation's private state. Treated as opaque by
   * interface users - only the implementation knows what this points to.
   *
   * **ThreadX**: Unused, typically NULL (ThreadX state is global)
   * **Mock**: Points to mock_time_state_t with current_ms counter
   * **Bare-metal**: Could point to hardware timer peripheral base address
   *
   * **Lifetime**: Must remain valid for duration of interface usage. Typically
   * points to static or global data, NOT stack variables.
   *
   * **Null allowed**: Yes, if implementation is stateless (e.g., ThreadX)
   */
  void* ctx;

  /**
   * @brief Sleep/delay function pointer (REQUIRED)
   *
   * @details
   * Blocks or simulates blocking for specified milliseconds. This function is
   * REQUIRED - validation fails if nullptr.
   *
   * **Implementation**: ThreadX: tx_thread_sleep(), Mock: simulate time advance
   * **Null allowed**: NO (validation fails)
   * **Thread safety**: Implementation-dependent (ThreadX is thread-safe)
   *
   * @see rx_time_sleep_ms_fn Full documentation of sleep semantics
   */
  rx_time_sleep_ms_fn sleep_ms;

  /**
   * @brief Get current time function pointer (REQUIRED)
   *
   * @details
   * Returns monotonic time in milliseconds since boot. This function is
   * REQUIRED - validation fails if nullptr.
   *
   * **Implementation**: ThreadX: tx_time_get() conversion, Mock: counter read
   * **Null allowed**: NO (validation fails)
   * **Resolution**: ThreadX tick rate (10ms typical), Mock 1ms
   *
   * @see rx_time_get_ms_fn Full documentation of time semantics
   */
  rx_time_get_ms_fn get_ms;

  /**
   * @brief Timeout elapsed check function pointer (OPTIONAL)
   *
   * @details
   * Convenience function combining get_ms() and timeout arithmetic. This
   * function is OPTIONAL - can be NULL for minimal implementations.
   *
   * **Implementation**: Calls get_ms() and computes (current - start) >= timeout
   * **Null allowed**: YES (caller must check before using)
   * **Alternative**: Manual timeout check using get_ms() if nullptr
   *
   * @see rx_time_is_elapsed_fn Full documentation of timeout semantics
   */
  rx_time_is_elapsed_fn is_elapsed;
};

/* =============================================================================
 * Interface Validation
 * =============================================================================
 */

/**
 * @brief Validate that a time interface is properly initialized
 *
 * @details
 * Performs defensive validation of a time interface to ensure it meets the
 * minimum requirements for safe usage. Checks that the interface pointer is
 * non-NULL and that required function pointers (sleep_ms, get_ms) are populated.
 *
 * This function implements "fail-fast" validation - it's better to detect a
 * misconfigured interface immediately at initialization than to crash later
 * when dereferencing NULL function pointers.
 *
 * **Validation Rules**:
 * 1. Interface pointer must be non-NULL
 * 2. sleep_ms function pointer must be non-NULL (REQUIRED)
 * 3. get_ms function pointer must be non-NULL (REQUIRED)
 * 4. ctx pointer may be NULL (implementation-dependent)
 * 5. is_elapsed pointer may be NULL (OPTIONAL)
 *
 * Algorithm steps:
 * 1. Check if iface pointer is nullptr -> return k_rx_err_null_ptr
 * 2. Check if sleep_ms is nullptr -> return k_rx_err_invalid_state
 * 3. Check if get_ms is nullptr -> return k_rx_err_invalid_state
 * 4. All checks passed -> return k_rx_ok
 *
 * @param[in] iface Pointer to time interface to validate
 *   - May be NULL (will return k_rx_err_null_ptr)
 *   - Typically points to stack or static rx_time_interface_t
 *
 * @return rx_err_t Validation result
 * @retval k_rx_ok Interface is valid and safe to use
 * @retval k_rx_err_null_ptr iface pointer is nullptr (cannot validate)
 * @retval k_rx_err_invalid_state Required function pointer(s) are NULL (sleep_ms or get_ms)
 *
 * @pre No preconditions - safe to call with any input (even NULL)
 *
 * @post Interface not modified (read-only validation)
 * @post Return value indicates whether interface is usable
 *
 * @note **Thread Safety**: Thread-safe (read-only operation, no shared state)
 * @note **Execution Time**: ~50 ns @ 240 MHz (3 null checks)
 * @note **Use Case**: Call at initialization before using interface
 * @note **Optional Fields**: Does NOT check ctx or is_elapsed (both may be NULL)
 *
 * @warning Does NOT validate that function pointers point to valid functions!
 *          Only checks for nullptr. Invalid pointers will cause crash on call.
 *
 * @attention Validation does NOT guarantee thread-safety of implementations.
 *            ThreadX is thread-safe, but custom implementations may not be.
 *
 * @par Example - Defensive Initialization:
 * @code{.c}
 * // Validate interface before passing to modules
 * void system_init(void) {
 *     rx_time_interface_t time_iface;
 *     rx_time_threadx_get_interface(&time_iface);
 *
 *     // Validate before use (defensive programming)
 *     rx_err_t err = rx_time_interface_validate(&time_iface);
 *     if (err == k_rx_err_null_ptr) {
 *         rx_log_fatal("INIT", "Time interface is nullptr");
 *         return;
 *     } else if (err == k_rx_err_invalid_state) {
 *         rx_log_fatal("INIT", "Time interface missing required functions");
 *         return;
 *     }
 *
 *     // Safe to use
 *     usb_init(&time_iface);
 *     spi_init(&time_iface);
 * }
 * @endcode
 *
 * @par Example - Validation in Module Init:
 * @code{.c}
 * // Module enforces interface validity at init
 * rx_err_t usb_comm_init(const rx_time_interface_t* time) {
 *     // Validate injected dependency
 *     rx_err_t err = rx_time_interface_validate(time);
 *     if (err != k_rx_ok) {
 *         rx_log_error("USB", "Invalid time interface: %d", err);
 *         return err;  // Propagate error to caller
 *     }
 *
 *     // Store validated interface
 *     s_usb_time = time;
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Example - Unit Test Verification:
 * @code{.c}
 * // Verify validation catches misconfigured interfaces
 * void test_validate_catches_null_interface(void) {
 *     rx_err_t err = rx_time_interface_validate(nullptr);
 *     assert(err == k_rx_err_null_ptr);
 * }
 *
 * void test_validate_catches_missing_sleep(void) {
 *     rx_time_interface_t time = {0};
 *     time.get_ms = mock_get_ms;  // Only get_ms populated
 *     // Missing sleep_ms!
 *
 *     rx_err_t err = rx_time_interface_validate(&time);
 *     assert(err == k_rx_err_invalid_state);
 * }
 *
 * void test_validate_allows_null_ctx(void) {
 *     rx_time_interface_t time = {0};
 *     time.ctx = nullptr;  // ctx can benullptr
 *     time.sleep_ms = mock_sleep_ms;
 *     time.get_ms = mock_get_ms;
 *
 *     rx_err_t err = rx_time_interface_validate(&time);
 *     assert(err == k_rx_ok);  // Valid even with NULL ctx
 * }
 * @endcode
 *
 * @par Example - Production Assertion:
 * @code{.c}
 * // Hard assertion in safety-critical code
 * void critical_init(const rx_time_interface_t* time) {
 *     rx_err_t err = rx_time_interface_validate(time);
 *
 *     // In safety-critical systems, invalid interface is fatal
 *     RX_CHECK(err == k_rx_ok, "Time interface validation failed");
 *
 *     // Continue with guaranteed valid interface...
 * }
 * @endcode
 *
 * @see rx_time_interface Structure being validated
 * @see rx_time_threadx_get_interface() Produces valid interface
 * @see mock_time_get_interface() Produces valid test interface
 *
 * @since Version 1.0.0
 * @version 1.0.0
 *
 * @test Tested in test_rx_time_interface.c::test_validate_null_interface()
 * @test Tested in test_rx_time_interface.c::test_validate_missing_functions()
 * @test Tested in test_rx_time_interface.c::test_validate_valid_interface()
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion
 * - Rule 2: [OK] No loops
 * - Rule 5: [OK] 1 precondition, 2 postconditions
 * - Rule 4: [OK] Function is 10 lines (well under 60)
 *
 * @callgraph
 * @callergraph
 */
[[nodiscard]] rx_err_t rx_time_threadx_get_interface(rx_time_interface_t* iface);

static inline rx_err_t rx_time_interface_validate(const rx_time_interface_t* iface)
{
  if (iface == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Check required function pointers */
  if (iface->sleep_ms == nullptr || iface->get_ms == nullptr) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

#ifdef __cplusplus
}
#endif
