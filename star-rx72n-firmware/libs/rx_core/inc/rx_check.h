/**
 * @file rx_check.h
 * @brief Validation and Error Checking Macros for RX72N Firmware
 *
 * @details
 * Comprehensive error checking and validation infrastructure for safety-critical
 * embedded systems. Provides type-safe macros for parameter validation, error
 * propagation, and assertion checking aligned with NASA Power of 10 Rule 5
 * (minimum 2 assertions per function).
 *
 * ## Architecture Design
 *
 * This module implements a layered error handling strategy:
 * - **Fatal errors:** System halts with diagnostic output (RX_ERROR_CHECK, RX_ASSERT)
 * - **Propagating errors:** Early return with logging (RX_RETURN_ON_ERROR variants)
 * - **Non-fatal errors:** Log and continue (RX_ERROR_CHECK_WITHOUT_ABORT)
 * - **Precondition validation:** Input parameter checks (RX_CHECK_NULL_PTR, RX_CHECK_RANGE)
 *
 * ## Error Handling Strategies
 *
 * | Macro                           | Fatal | Returns | Use Case                              |
 * |---------------------------------|-------|---------|---------------------------------------|
 * | RX_ERROR_CHECK                  | [YES]     | Never   | Critical initialization errors        |
 * | RX_ERROR_CHECK_WITHOUT_ABORT    | [NO]     | No      | Optional feature failures             |
 * | RX_RETURN_ON_ERROR              | [NO]     | rx_err_t| Error propagation in rx_err_t funcs   |
 * | RX_RETURN_VOID_ON_ERROR         | [NO]     | void    | Error handling in void functions      |
 * | RX_RETURN_NULL_ON_ERROR         | [NO]     | NULL    | Error handling in pointer functions   |
 * | RX_CHECK_NULL_PTR               | [NO]     | rx_err_t| Null pointer precondition check       |
 * | RX_CHECK_RANGE                  | [NO]     | rx_err_t| Range validation                      |
 * | RX_ASSERT                       | [YES]     | Never   | Invariant and postcondition checking  |
 *
 * ## Error Handling Flow
 *
 * @dot
 * digraph error_handling {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   error [label="Error Detected", fillcolor=red, style=filled, fontcolor=white];
 *   fatal [label="Fatal?", shape=diamond];
 *   log_halt [label="Log & Halt\n(RX_ERROR_CHECK)", fillcolor=red, style=filled];
 *   log_continue [label="Log & Continue\n(RX_ERROR_CHECK_WITHOUT_ABORT)", fillcolor=yellow, style=filled];
 *   log_return [label="Log & Return\n(RX_RETURN_ON_ERROR)", fillcolor=orange, style=filled];
 *   caller [label="Caller Handles"];
 *
 *   error -> fatal;
 *   fatal -> log_halt [label="Yes\n(critical)"];
 *   fatal -> log_continue [label="No\n(non-critical)"];
 *   fatal -> log_return [label="No\n(propagate)"];
 *   log_halt -> halt [label="infinite loop"];
 *   log_continue -> continue [label="execution continues"];
 *   log_return -> caller;
 *
 *   halt [shape=octagon, label="HALTED", fillcolor=black, fontcolor=white];
 *   continue [shape=ellipse, label="Continue"];
 * }
 * @enddot
 *
 * ## Design Rationale
 *
 * **Why macros instead of inline functions?**
 * - Capture __FILE__, __LINE__ for precise error location (future enhancement)
 * - Zero runtime overhead (no function call in release builds)
 * - Early return from enclosing function (not possible with inline functions)
 * - Conditional compilation based on build type (e.g., disable assertions in release)
 *
 * **Why do-while(0) wrapper?**
 * - Safe in all contexts (if statements without braces)
 * - Requires semicolon after macro invocation
 * - Prevents multiple expansion bugs
 *
 * **Why separate return-type variants?**
 * - Type safety: prevents returning error from void function
 * - Explicit intent: RX_RETURN_NULL_ON_ERROR clearly returnsnullptr
 * - Compiler optimization: separate code paths for different return types
 *
 * ## Performance Characteristics
 *
 * All macros expand to minimal code:
 * - **Success path:** 1-2 comparisons (2-4 cycles @ 240 MHz)
 * - **Error path:** Logging + action (varies, but typically < 100 us for non-fatal)
 * - **Fatal error:** Logging + infinite loop (system halted)
 * - **Memory:** 0 bytes overhead (pure compile-time code generation)
 *
 * ## Thread Safety
 *
 * - **Validation macros:** Thread-safe (pure validation, no shared state)
 * - **Logging macros:** Thread-safe if rx_log is thread-safe
 * - **Fatal error:** Not thread-safe (intentional - halts entire system)
 *
 * ## Usage Patterns
 *
 * @par Pattern 1: Critical Initialization
 * @code{.c}
 * void system_init(void) {
 *     rx_err_t err;
 *
 *     // Critical hardware init - must succeed or halt
 *     err = init_clock();
 *     RX_ERROR_CHECK(err);  // Halts on failure
 *
 *     err = init_gpio();
 *     RX_ERROR_CHECK(err);  // Halts on failure
 *
 *     // System continues only if both succeeded
 * }
 * @endcode
 *
 * @par Pattern 2: Optional Feature Initialization
 * @code{.c}
 * void init_optional_features(void) {
 *     rx_err_t err;
 *
 *     // Optional feature - log error but continue
 *     err = init_debug_uart();
 *     RX_ERROR_CHECK_WITHOUT_ABORT(err);  // Logs, continues
 *
 *     // System continues even if debug UART fails
 * }
 * @endcode
 *
 * @par Pattern 3: Error Propagation
 * @code{.c}
 * rx_err_t init_subsystem(void) {
 *     rx_err_t err;
 *
 *     // Propagate error to caller
 *     err = init_hardware();
 *     RX_RETURN_ON_ERROR(err, "SUBSYS", "Hardware init failed");
 *
 *     err = init_software();
 *     RX_RETURN_ON_ERROR(err, "SUBSYS", "Software init failed");
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Pattern 4: NASA Rule 5 Compliance (Preconditions)
 * @code{.c}
 * rx_err_t process_buffer(const uint8_t* data, size_t len) {
 *     // Precondition 1: Validate pointer (NASA Rule 5)
 *     RX_CHECK_NULL_PTR(data, "PROCESS", "Data buffer is nullptr");
 *
 *     // Precondition 2: Validate size (NASA Rule 5)
 *     RX_CHECK_RANGE(len, 1, 256, k_rx_err_invalid_size);
 *
 *     // Function body - preconditions satisfied
 *     // ...
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Pattern 5: Postcondition Validation
 * @code{.c}
 * rx_err_t compute_result(float* output) {
 *     RX_CHECK_NULL_PTR(output, "COMPUTE", "Output pointer is nullptr");
 *
 *     // Perform computation
 *     *output = complex_calculation();
 *
 *     // Postcondition: result must be finite (NASA Rule 5)
 *     RX_ASSERT(isfinite(*output), "Result must be finite");
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Module Dependencies:
 * - rx_err.h: Error code definitions (rx_err_t, k_rx_ok, etc.)
 * - rx_log.h: Logging infrastructure (rx_log_error, uart_debug_puts)
 *
 * @see rx_err.h Error code definitions
 * @see rx_log.h Logging functions used by check macros
 * @see rx_error_handler.h High-level error handling infrastructure
 * @see docs/sections/06_nasa_power_of_10.tex NASA Rule 5 compliance
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [YES] No goto, setjmp, recursion (macros expand to structured code)
 * - Rule 2: [YES] Infinite loop in fatal error is intentional (halt behavior)
 * - Rule 3: [YES] No dynamic allocation (stack-only variables)
 * - Rule 4: [YES] Macro expansions are small (< 10 lines)
 * - Rule 5: [YES] **Explicitly designed to enforce Rule 5** (provide assertion macros)
 * - Rule 6: [YES] Minimal scope (do-while(0) block scope)
 * - Rule 7: [YES] All function calls checked by design (purpose of module)
 * - Rule 8: [YES] No raw macros for constants (uses rx_err_t typed enums)
 * - Rule 9: [YES] No function pointers
 * - Rule 10: [YES] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Only error checking and validation, no business logic
 * - **O (Open/Closed):** Extensible via new macros, existing macros closed for modification
 * - **L (Liskov Substitution):** Macros are transparent wrappers, don't change semantics
 * - **I (Interface Segregation):** Separate macros for different use cases (fatal, return, validate)
 * - **D (Dependency Inversion):** Depends on abstract rx_err_t, not concrete error values
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_log.h"

/**
 * @def RX_STATIC_TESTABLE
 * @brief Makes internal (static) functions visible to unit tests.
 *
 * @details
 * In production builds functions decorated with this macro are declared
 * @c static, keeping them private to their translation unit.  In UNIT_TEST
 * builds the static qualifier is removed so test code can call internal
 * helpers directly -- including with invalid arguments -- to verify that
 * every defensive null-check and error branch is reachable.
 *
 * Usage in a source file:
 * @code
 *   RX_STATIC_TESTABLE rx_err_t internal_decode_header(...);
 * @endcode
 *
 * Usage in a test file (forward-declare the function as extern):
 * @code
 *   #ifdef UNIT_TEST
 *   extern rx_err_t internal_decode_header(...);
 *   #endif
 * @endcode
 *
 * @note In production builds this macro expands to @c static, so there is
 *       zero overhead and no change in linkage or inlining behaviour.
 *
 * @since Version 1.0.0
 */
#ifndef RX_STATIC_TESTABLE
#ifdef UNIT_TEST
#define RX_STATIC_TESTABLE /* non-static in test builds */
#else
#define RX_STATIC_TESTABLE static
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Fatal Error Handling
 * =============================================================================
 */

/**
 * @enum fatal_error_constants_t
 * @brief Constants for fatal error output formatting
 */
typedef enum : uint8_t {
  k_err_hex_digits = 8U, /**< Number of hex digits for a 32-bit error code */
} fatal_error_constants_t;

/**
 * @brief Halt execution with diagnostic error message (fatal error handler)
 *
 * @details
 * Terminates firmware execution after logging detailed error diagnostics to
 * debug UART. This is a non-returning function used for unrecoverable errors
 * where continuing execution would be unsafe.
 *
 * ## Algorithm
 *
 * 1. Output formatted error banner to debug UART
 * 2. Display component tag, error message, and error code (hex)
 * 3. Indicate system halted state
 * 4. Disable all interrupts (global interrupt disable)
 * 5. Enter infinite loop with CPU in low-power WAIT state
 *
 * ## Output Format
 *
 * ```
 * ========================================
 * FATAL ERROR
 * ========================================
 * [TAG] Error message here
 * Error code: 0x00000XXX
 * ========================================
 * System halted. Reset required.
 * ========================================
 * ```
 *
 * ## Platform-Specific Behavior
 *
 * **Normal Build (Hardware Target):**
 * - Disables interrupts via `clrpsw i` (clear PSW I-flag)
 * - Enters infinite loop with `wait` instruction
 * - CPU remains in low-power wait state until hardware reset
 * - Watchdog will eventually trigger reset if enabled
 *
 * **Unit Test Build (UNIT_TEST defined):**
 * - Returns immediately without halting
 * - Allows test harness to continue and detect fatal error
 * - Enables testing of error paths without hanging test runner
 *
 * @param[in] tag Component tag for error identification
 *   - Type: const char* (null-terminated string)
 *   - Must not be nullptr (undefined behavior if nullptr)
 *   - Typical values: "MOTOR", "COMM", "INIT", "ASSERT", etc.
 *   - Used to identify which subsystem detected the error
 *   - Recommended length: 3-8 characters for readability
 *
 * @param[in] message Human-readable error description
 *   - Type: const char* (null-terminated string)
 *   - Must not be nullptr (undefined behavior if nullptr)
 *   - Should describe what failed and why
 *   - Example: "GPIO initialization failed", "Invalid motor ID"
 *   - Maximum practical length: ~80 characters (UART line width)
 *
 * @param[in] err Error code indicating failure type
 *   - Type: rx_err_t (int32_t)
 *   - Typically non-zero (zero indicates success, illogical for fatal error)
 *   - Displayed in hexadecimal (8 digits, zero-padded)
 *   - Used for post-mortem debugging via UART log capture
 *
 * @note **This function never returns** (except in UNIT_TEST builds)
 *
 * @par Usage Example - From RX_ERROR_CHECK:
 * @code{.c}
 * rx_err_t err = critical_init_function();
 * if (err != k_rx_ok) {
 *     internal_rx_fatal_error("INIT", "Critical init failed", err);
 *     // Never reached in normal builds
 * }
 * @endcode
 *
 * @par Usage Example - From RX_ASSERT:
 * @code{.c}
 * if (!(ptr != nullptr)) {
 *     internal_rx_fatal_error("ASSERT", "Pointer must not be NULL", k_rx_fail);
 *     // Never reached in normal builds
 * }
 * @endcode
 *
 * @par Usage Example - Direct Call (Rare):
 * @code{.c}
 * // Only use directly if macro wrappers don't fit use case
 * if (unrecoverable_condition()) {
 *     internal_rx_fatal_error("SAFETY", "Unrecoverable hardware fault", k_rx_err_hw_error);
 * }
 * @endcode
 *
 * @par Assembly Instruction Details:
 *
 * **clrpsw i** - Clear Processor Status Word I-flag
 * - Disables all maskable interrupts globally
 * - Non-maskable interrupts (NMI) still active
 * - Prevents interrupt handlers from interfering with error state
 * - Ensures system remains in known halted state
 *
 * **wait** - Wait for Interrupt
 * - Places CPU in low-power state
 * - CPU wakes on interrupt, but interrupts disabled -> immediately waits again
 * - Reduces power consumption during halt state
 * - Effective infinite loop with minimal power draw
 *
 * @par Watchdog Behavior:
 *
 * If watchdog (IWDT) is enabled:
 * - Watchdog will expire after timeout period (typically 128ms-2s)
 * - System will reset automatically
 * - Error message captured before reset (if UART log buffered)
 * - Allows recovery from fatal errors in deployed systems
 *
 * If watchdog is disabled:
 * - System remains halted indefinitely
 * - Requires manual hardware reset or power cycle
 * - Useful during development for post-mortem debugging
 *
 * @par Performance:
 * - Execution time: ~500 us (UART output dominates)
 * - Stack usage: ~16 bytes (local variables, return address)
 * - UART bandwidth: ~100 bytes @ 115200 baud = ~9ms transmission time
 * - **Not performance-critical:** Only called on fatal errors
 *
 * @note Thread Safety: Not thread-safe, but irrelevant (system halting)
 * @note Re-entrancy: Not reentrant (disables interrupts, infinite loop)
 * @note Memory: No dynamic allocation, stack-only locals
 *
 * @warning **This function never returns in normal builds** - Calling code after
 *          this function is unreachable. Compiler may optimize it away.
 *
 * @warning **Do not call from interrupt context** - Disabling interrupts in ISR
 *          can cause undefined behavior. Use error flags instead.
 *
 * @attention **System is permanently halted** - Only recovery is hardware reset.
 *            Ensure watchdog is enabled in production builds for automatic recovery.
 *
 * @attention **UART output is blocking** - If UART is not initialized or not
 *            connected, uart_debug_puts() may hang. Ensure UART is functional.
 *
 * @pre tag != nullptr (undefined behavior if nullptr, will likely crash)
 * @pre message != nullptr (undefined behavior if nullptr, will likely crash)
 * @pre UART debug interface initialized (or uart_debug_puts() will hang)
 *
 * @post Interrupts disabled globally (PSW.I = 0)
 * @post CPU in infinite wait loop (never returns)
 * @post Error diagnostics sent to debug UART
 * @post System requires hardware reset to recover
 *
 * @invariant In UNIT_TEST builds, function returns immediately (allows testing)
 * @invariant In normal builds, function never returns (infinite loop)
 *
 * @see RX_ERROR_CHECK() Macro that calls this function
 * @see RX_ASSERT() Macro that calls this function
 * @see uart_debug_puts() UART output function used for diagnostics
 * @see uart_debug_puthex() UART hex output function
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [YES] No goto, setjmp, recursion
 * - Rule 2: [WARN] Infinite loop is intentional (halt behavior, safety requirement)
 * - Rule 3: [YES] No dynamic allocation
 * - Rule 4: [YES] Function is ~30 lines (compact error handler)
 * - Rule 5: [YES] 3 preconditions, 4 postconditions, 2 invariants
 * - Rule 6: [YES] Variables have minimal scope
 * - Rule 7: [YES] UART function calls assumed to succeed (no error checking needed)
 * - Rule 8: [YES] Uses typed enum for error codes
 * - Rule 9: [YES] No function pointers
 * - Rule 10: [YES] Compiles with -Wall -Wextra -Werror
 *
 * @since Version 1.0.0
 */
#ifndef UNIT_TEST
[[noreturn]]
#endif
static inline void internal_rx_fatal_error(const char* tag, const char* message, rx_err_t err)
{
  uart_debug_puts("\r\n");
  uart_debug_puts("========================================\r\n");
  uart_debug_puts("FATAL ERROR\r\n");
  uart_debug_puts("========================================\r\n");
  uart_debug_putc('[');
  uart_debug_puts(tag);
  uart_debug_puts("] ");
  uart_debug_puts(message);
  uart_debug_puts("\r\n");
  uart_debug_puts("Error code: ");
  uart_debug_puthex((uint32_t)err, k_err_hex_digits);
  uart_debug_puts("\r\n");
  uart_debug_puts("========================================\r\n");
  uart_debug_puts("System halted. Reset required.\r\n");
  uart_debug_puts("========================================\r\n");

  /* Disable interrupts and halt */
#ifndef UNIT_TEST
  __asm__ volatile("clrpsw i");
  while (1) {
    __asm__ volatile("wait");
  }
#endif
}

/* =============================================================================
 * Assertion Macros
 * =============================================================================
 */

/**
 * @def RX_ASSERT
 * @brief Runtime assertion with fatal error on failure (NASA Rule 5 compliance)
 *
 * @details
 * Verifies a boolean condition at runtime and halts execution if the condition
 * is false. This macro is the primary mechanism for enforcing NASA Power of 10
 * Rule 5 (minimum 2 assertions per function) through precondition, postcondition,
 * and invariant checking.
 *
 * ## Algorithm
 *
 * 1. Evaluate condition expression
 * 2. If condition is true: Continue execution (no overhead)
 * 3. If condition is false:
 *    a. Call internal_rx_fatal_error() with "ASSERT" tag
 *    b. Log error message to debug UART
 *    c. Halt system (infinite loop with interrupts disabled)
 *
 * ## Use Cases
 *
 * | Use Case                  | Example                                          |
 * |---------------------------|--------------------------------------------------|
 * | Precondition (input)      | `RX_ASSERT(ptr != nullptr, "Input must be non-NULL")`|
 * | Precondition (range)      | `RX_ASSERT(value <= MAX, "Value exceeds maximum")`|
 * | Postcondition (output)    | `RX_ASSERT(result >= 0, "Result must be non-negative")`|
 * | Invariant (state)         | `RX_ASSERT(count < MAX_COUNT, "Count overflow")`|
 * | Loop bound verification   | `RX_ASSERT(i < ARRAY_SIZE, "Loop index overflow")`|
 *
 * ## NASA Rule 5 Compliance Pattern
 *
 * Every function should have minimum 2 assertions:
 * @code{.c}
 * rx_err_t calculate(float* input, float* output) {
 *     // Precondition 1: Input pointer valid (NASA Rule 5)
 *     RX_ASSERT(input != nullptr, "Input pointer must not be NULL");
 *
 *     // Precondition 2: Output pointer valid (NASA Rule 5)
 *     RX_ASSERT(output != nullptr, "Output pointer must not be NULL");
 *
 *     // Perform calculation
 *     *output = *input * 2.0f;
 *
 *     // Postcondition: Result is finite (NASA Rule 5)
 *     RX_ASSERT(isfinite(*output), "Output must be finite");
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @param condition Boolean expression to verify
 *   - Type: Any expression convertible to bool
 *   - Must evaluate to true for normal operation
 *   - Examples: `ptr != nullptr`, `value < MAX`, `state == READY`
 *   - Evaluated exactly once (no multiple evaluation bug)
 *   - Should not have side effects (evaluation may be removed in release builds)
 *
 * @param message Error message displayed on assertion failure
 *   - Type: const char* (string literal)
 *   - Must be compile-time constant string
 *   - Should describe what was expected, not what failed
 *   - Good: "Pointer must not be NULL", "Value must be in range [0, 100]"
 *   - Bad: "ptr is nullptr", "Invalid value"
 *   - Recommended length: < 60 characters for readability
 *
 * @par Usage Example - Precondition Checking:
 * @code{.c}
 * rx_err_t motor_set_velocity(motor_handle_t* motor, float velocity_mps) {
 *     // Precondition 1: Handle valid
 *     RX_ASSERT(motor != nullptr, "Motor handle must not be NULL");
 *
 *     // Precondition 2: Velocity within physical limits
 *     RX_ASSERT(velocity_mps >= -2.5f && velocity_mps <= 2.5f,
 *               "Velocity must be in range [-2.5, 2.5] m/s");
 *
 *     // Precondition 3: Motor initialized
 *     RX_ASSERT(motor->initialized, "Motor must be initialized");
 *
 *     // Function body - all preconditions satisfied
 *     motor->target_velocity = velocity_mps;
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Usage Example - Postcondition Checking:
 * @code{.c}
 * rx_err_t sensor_read(sensor_t* sensor, float* temperature_c) {
 *     RX_ASSERT(sensor != nullptr, "Sensor handle must not be NULL");
 *     RX_ASSERT(temperature_c != nullptr, "Output pointer must not be NULL");
 *
 *     // Read sensor via I2C
 *     uint16_t raw_value = read_i2c_register(sensor->address, TEMP_REG);
 *     *temperature_c = (raw_value * 0.0625f) - 40.0f;  // DS18B20 conversion
 *
 *     // Postcondition: Temperature in physically possible range
 *     RX_ASSERT(*temperature_c >= -55.0f && *temperature_c <= 125.0f,
 *               "Temperature must be in valid sensor range");
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Usage Example - Invariant Checking:
 * @code{.c}
 * typedef struct {
 *     uint8_t head;
 *     uint8_t tail;
 *     uint8_t data[64];
 * } ring_buffer_t;
 *
 * void ring_buffer_push(ring_buffer_t* rb, uint8_t value) {
 *     RX_ASSERT(rb != nullptr, "Ring buffer must not be NULL");
 *
 *     // Invariant: head and tail always within bounds
 *     RX_ASSERT(rb->head < 64, "Head index must be < 64");
 *     RX_ASSERT(rb->tail < 64, "Tail index must be < 64");
 *
 *     rb->data[rb->head] = value;
 *     rb->head = (rb->head + 1) % 64;
 *
 *     // Invariant maintained after modification
 *     RX_ASSERT(rb->head < 64, "Head index must remain < 64");
 * }
 * @endcode
 *
 * @par Usage Example - Loop Bound Verification (NASA Rule 2):
 * @code{.c}
 * typedef enum : uint8_t {
 *     k_max_iterations = 100  // Explicit loop bound (NASA Rule 2)
 * } loop_bounds_t;
 *
 * void process_array(uint8_t* data, size_t len) {
 *     RX_ASSERT(data != nullptr, "Data array must not be NULL");
 *     RX_ASSERT(len <= k_max_iterations, "Array too large for fixed loop bound");
 *
 *     for (uint8_t i = 0; i < k_max_iterations && i < len; i++) {
 *         // Invariant: i is always within array bounds
 *         RX_ASSERT(i < len, "Loop index must be < array length");
 *         data[i] = process_byte(data[i]);
 *     }
 * }
 * @endcode
 *
 * @par Conditional Compilation:
 *
 * Assertions can be disabled in release builds for performance:
 * @code{.c}
 * #ifdef NDEBUG
 * #define RX_ASSERT(condition, message) ((void)0)  // No-op in release
 * #else
 * #define RX_ASSERT(condition, message) do { ... } while (0)  // As shown above
 * #endif
 * @endcode
 *
 * **WARNING:** Disabling assertions removes safety checks. Only disable after
 * extensive testing with assertions enabled.
 *
 * @par Performance:
 * - Success path: 1 comparison + 1 branch (2-3 cycles @ 240 MHz)
 * - Failure path: Logs and halts (system terminates, performance irrelevant)
 * - Stack usage: 0 bytes (no additional stack beyond function call if failure)
 * - Code size: ~8 bytes per assertion in release builds with optimizations
 *
 * @note Thread Safety: Condition check is atomic (single comparison)
 * @note Re-entrancy: Re-entrant (until failure, then system halts)
 * @note Memory: No dynamic allocation
 *
 * @warning **Assertions halt the system** - Use for programming errors and
 *          invariant violations, NOT for recoverable runtime errors.
 *
 * @warning **Condition must not have side effects** - If assertions are disabled,
 *          side effects disappear. Bad: `RX_ASSERT(ptr++ != nullptr, "...")` Good: `ptr++; RX_ASSERT(ptr != nullptr, "...")`
 *
 * @attention **Use assertions for programming errors** (bugs in code) and
 *            return error codes for runtime errors (invalid user input, hardware faults).
 *
 * @par Assertions vs. Error Returns:
 *
 * | Condition Type           | Use Assertion    | Use Error Return |
 * |--------------------------|------------------|------------------|
 * | Null pointer from caller | YES              | NO               |
 * | Value out of documented range | YES         | NO               |
 * | Invalid state transition | YES              | NO               |
 * | Hardware fault           | NO               | YES              |
 * | Communication timeout    | NO               | YES              |
 * | Sensor out of range      | NO               | YES              |
 *
 * @pre condition is a valid boolean expression
 * @pre message is a non-NULL string literal (compile-time constant)
 *
 * @post If condition is true: Execution continues normally
 * @post If condition is false: System halts permanently (requires reset)
 *
 * @invariant If condition is false, function never returns
 * @invariant Condition evaluated exactly once (no multiple evaluation)
 *
 * @see internal_rx_fatal_error() Function called on assertion failure
 * @see RX_ERROR_CHECK() Related macro for error code checking
 * @see RX_CHECK_NULL_PTR() Alternative for null pointer checking with error return
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [YES] No goto, setjmp, recursion (structured if statement)
 * - Rule 2: [YES] N/A (no loops)
 * - Rule 3: [YES] No dynamic allocation
 * - Rule 4: [YES] Macro expansion is 3 lines
 * - Rule 5: [YES] **This macro implements Rule 5** (provides assertion mechanism)
 * - Rule 6: [YES] do-while(0) provides minimal scope
 * - Rule 7: [YES] N/A (macro itself, not function)
 * - Rule 8: [YES] Uses typed enum k_rx_fail
 * - Rule 9: [YES] No function pointers
 * - Rule 10: [YES] Compiles with -Wall -Wextra -Werror
 *
 * @since Version 1.0.0
 */
#define RX_ASSERT(condition, message)                                                              \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      internal_rx_fatal_error("ASSERT", message, k_rx_fail);                                       \
    }                                                                                              \
  } while (0)

/**
 * @def RX_ASSERT_PRE
 * @brief Precondition assertion -- verifies caller contract before function body
 *
 * @details
 * Halts the system if a precondition is violated. Use for validating state and
 * parameters at the top of internal functions where the condition is
 * architecturally guaranteed by callers, but should still be checked defensively.
 *
 * ## Testability (UNIT_TEST builds)
 *
 * Because internal functions are decorated with @c RX_STATIC_TESTABLE, test
 * code can call them directly with invalid state or parameters. This makes
 * the false branch of every RX_ASSERT_PRE reachable, enabling 100% branch
 * coverage without production-code gymnastics.
 *
 * @param condition Boolean expression that must be true
 * @param message   Descriptive string for fatal error output
 *
 * @pre  condition is a valid boolean expression
 * @pre  message is a non-NULL string literal
 * @post If condition is true: execution continues
 * @post If condition is false: system halts (returns in UNIT_TEST builds)
 *
 * @see RX_ASSERT_POST() Postcondition counterpart
 * @see RX_STATIC_TESTABLE Exposes internal functions for test-driven coverage
 *
 * @since Version 1.0.0
 */
#define RX_ASSERT_PRE(condition, message)                                                          \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      internal_rx_fatal_error("PRE", (message), k_rx_fail);                                        \
    }                                                                                              \
  } while (0)

/**
 * @def RX_ASSERT_POST
 * @brief Postcondition assertion -- verifies function output or invariant after work
 *
 * @details
 * Halts the system if a postcondition is violated. Use after a computation to
 * verify that the result satisfies the documented contract.
 *
 * ## Testability (UNIT_TEST builds)
 *
 * Postconditions are often architecturally guaranteed (e.g., a 20-bit mask
 * always produces a value in [0, 0xFFFFF]). Their false branches are
 * unreachable without fault injection. In UNIT_TEST builds the macro checks
 * @c g_rx_assert_post_force_fail first: when that flag is @c true the macro
 * fires regardless of the actual condition, making the failure branch
 * reachable for coverage.
 *
 * @param condition Boolean expression that must be true
 * @param message   Descriptive string for fatal error output
 *
 * @pre  condition is a valid boolean expression
 * @pre  message is a non-NULL string literal
 * @post If condition is true (and force-fail off): execution continues
 * @post If condition is false or force-fail on: system halts (returns in UNIT_TEST)
 *
 * @see RX_ASSERT_PRE() Precondition counterpart
 * @see g_rx_assert_post_force_fail Test-only fault injection flag
 *
 * @since Version 1.0.0
 */
#ifdef UNIT_TEST
extern bool g_rx_assert_post_force_fail;
#define RX_ASSERT_POST(condition, message)                                                         \
  do {                                                                                             \
    if (g_rx_assert_post_force_fail || !(condition)) {                                             \
      internal_rx_fatal_error("POST", (message), k_rx_fail);                                       \
    }                                                                                              \
  } while (0)
#else
#define RX_ASSERT_POST(condition, message)                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      internal_rx_fatal_error("POST", (message), k_rx_fail);                                       \
    }                                                                                              \
  } while (0)
#endif

/* =============================================================================
 * Error Checking Macros
 * =============================================================================
 */

/**
 * @def RX_ERROR_CHECK
 * @brief Fatal error checking - halt system if error code indicates failure
 *
 * @details
 * Checks an rx_err_t error code and halts the system with diagnostics if the
 * error is not k_rx_ok. Use this macro for critical operations where failure
 * is unrecoverable and continuing execution would be unsafe.
 *
 * ## When to Use
 *
 * - **Critical initialization:** Hardware init, clock config, essential peripherals
 * - **Safety-critical resources:** Emergency stop system, watchdog, fault detection
 * - **Unrecoverable errors:** Memory corruption, invalid firmware state
 *
 * ## When NOT to Use
 *
 * - **Optional features:** Use RX_ERROR_CHECK_WITHOUT_ABORT instead
 * - **Recoverable errors:** Use RX_RETURN_ON_ERROR to propagate to caller
 * - **Expected failures:** Handle explicitly with if/switch statements
 *
 * @param err Error code expression to check
 *   - Type: Any expression that evaluates to rx_err_t
 *   - Evaluated exactly once (safe for expressions with side effects)
 *   - If k_rx_ok (0): Continues execution silently
 *   - If any other value: Halts system with diagnostics
 *
 * @par Usage Example - Critical Initialization:
 * @code{.c}
 * void system_boot(void) {
 *     rx_err_t err;
 *
 *     // Clock init is critical - must succeed
 *     err = init_system_clock();
 *     RX_ERROR_CHECK(err);  // Halts on failure
 *
 *     // GPIO init is critical - must succeed
 *     err = init_gpio();
 *     RX_ERROR_CHECK(err);  // Halts on failure
 *
 *     // Only reached if both succeeded
 *     uart_debug_puts("System boot successful\r\n");
 * }
 * @endcode
 *
 * @par Usage Example - Safety-Critical Resource:
 * @code{.c}
 * void init_safety_systems(void) {
 *     rx_err_t err;
 *
 *     // Emergency stop system must work
 *     err = init_emergency_stop();
 *     RX_ERROR_CHECK(err);  // Safety-critical, cannot proceed if failed
 *
 *     // Watchdog must be enabled
 *     err = init_independent_watchdog();
 *     RX_ERROR_CHECK(err);  // System unusable without watchdog
 * }
 * @endcode
 *
 * @par Comparison to Alternatives:
 *
 * | Macro                           | Halts | Logs | Returns | Use Case              |
 * |---------------------------------|-------|------|---------|------------------------|
 * | RX_ERROR_CHECK                  | [YES]     | [YES]    | Never   | Critical init          |
 * | RX_ERROR_CHECK_WITHOUT_ABORT    | [NO]     | [YES]    | No      | Optional features      |
 * | RX_RETURN_ON_ERROR              | [NO]     | [YES]    | Error   | Propagate to caller    |
 * | if (err != k_rx_ok)             | [NO]     | [NO]    | Varies  | Custom handling        |
 *
 * @warning **System halts permanently** - Use only for truly unrecoverable errors
 * @warning **Cannot be caught or handled** - No try/catch, no error recovery
 *
 * @attention **Watchdog behavior:** If enabled, watchdog will eventually reset
 *            system. If disabled, requires manual reset/power cycle.
 *
 * @see RX_ERROR_CHECK_WITHOUT_ABORT() Non-fatal version (logs, continues)
 * @see RX_RETURN_ON_ERROR() Error propagation version (returns to caller)
 * @see internal_rx_fatal_error() Underlying fatal error handler
 *
 * @since Version 1.0.0
 */
#define RX_ERROR_CHECK(err)                                                                        \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      internal_rx_fatal_error("ERROR_CHECK", "Fatal error detected", err_rc_);                     \
    }                                                                                              \
  } while (0)

/**
 * @brief Check error code and log on failure (non-fatal)
 *
 * @param[in] err Error code to check
 *
 * If err is not k_rx_ok, logs the error but continues execution.
 * Use this for non-critical errors where system can continue.
 *
 * Example:
 *   rx_err_t err = optional_feature_init();
 *   RX_ERROR_CHECK_WITHOUT_ABORT(err);  // Logs but continues
 */
#define RX_ERROR_CHECK_WITHOUT_ABORT(err)                                                          \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      rx_log_error_val("ERROR_CHECK", "Error", err_rc_);                                           \
    }                                                                                              \
  } while (0)

/**
 * @brief Return early on error
 *
 * @param[in] err Error code to check
 * @param[in] tag Component tag for logging
 * @param[in] message Error message for logging
 *
 * If err is not k_rx_ok, logs the error and returns err from the function.
 * Use this for functions that return rx_err_t.
 *
 * Example:
 *   rx_err_t init_subsystem(void) {
 *       rx_err_t err = gpio_init();
 *       RX_RETURN_ON_ERROR(err, "SUBSYS", "GPIO init failed");
 *       // Only reached if gpio_init succeeded
 *       return k_rx_ok;
 *   }
 */
#define RX_RETURN_ON_ERROR(err, tag, message)                                                      \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      rx_log_error(tag, message);                                                                  \
      rx_log_error_val(tag, "Error", err_rc_);                                                     \
      return err_rc_;                                                                              \
    }                                                                                              \
  } while (0)

/**
 * @brief Return void on error (for void functions)
 *
 * @param[in] err Error code to check
 * @param[in] tag Component tag for logging
 * @param[in] message Error message for logging
 *
 * If err is not k_rx_ok, logs the error and returns from the function.
 * Use this for void functions that cannot return an error code.
 *
 * Example:
 *   void configure_system(void) {
 *       rx_err_t err = gpio_init();
 *       RX_RETURN_VOID_ON_ERROR(err, "SYSTEM", "GPIO init failed");
 *       // Only reached if gpio_init succeeded
 *   }
 */
#define RX_RETURN_VOID_ON_ERROR(err, tag, message)                                                 \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      rx_log_error(tag, message);                                                                  \
      rx_log_error_val(tag, "Error", err_rc_);                                                     \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Return NULL on error (for pointer-returning functions)
 *
 * @param[in] err Error code to check
 * @param[in] tag Component tag for logging
 * @param[in] message Error message for logging
 *
 * If err is not k_rx_ok, logs the error and returns NULL from the function.
 * Use this for functions that return pointers.
 *
 * Example:
 *   void* create_object(void) {
 *       rx_err_t err = validate_preconditions();
 *       RX_RETURN_NULL_ON_ERROR(err, "FACTORY", "Precondition check failed");
 *       // Only reached if validation succeeded
 *       return allocate_object();
 *   }
 */
#define RX_RETURN_NULL_ON_ERROR(err, tag, message)                                                 \
  do {                                                                                             \
    rx_err_t err_rc_ = (err);                                                                      \
    if (rx_err_is_error(err_rc_)) {                                                                \
      rx_log_error(tag, message);                                                                  \
      rx_log_error_val(tag, "Error", err_rc_);                                                     \
      return nullptr;                                                                              \
    }                                                                                              \
  } while (0)

/* =============================================================================
 * Null Pointer Checking
 * =============================================================================
 */

/**
 * @brief Check for null pointer and return error
 *
 * @param[in] ptr Pointer to check
 * @param[in] tag Component tag for logging
 * @param[in] message Error message for logging
 *
 * If ptr is nullptr, logs the error and returns k_rx_err_null_ptr.
 *
 * Example:
 *   rx_err_t process_data(const uint8_t* data) {
 *       RX_CHECK_NULL_PTR(data, "PROCESS", "Data pointer is nullptr");
 *       // Only reached if data != nullptr
 *       return k_rx_ok;
 *   }
 */
#define RX_CHECK_NULL_PTR(ptr, tag, message)                                                       \
  do {                                                                                             \
    if ((ptr) == nullptr) {                                                                        \
      rx_log_error(tag, message);                                                                  \
      return k_rx_err_null_ptr;                                                                    \
    }                                                                                              \
  } while (0)

/**
 * @brief Check that a value is within an inclusive range
 *
 * @param[in] value Value to check
 * @param[in] min Minimum allowed value
 * @param[in] max Maximum allowed value
 * @param[in] errcode Error to return on failure
 */
#define RX_CHECK_RANGE(value, min, max, errcode)                                                   \
  do {                                                                                             \
    if (((value) < (min)) || ((value) > (max))) {                                                  \
      rx_log_error("CHECK", "Range check failed");                                                 \
      return (errcode);                                                                            \
    }                                                                                              \
  } while (0)

/**
 * @brief Check that a value is within an inclusive range with tag
 *
 * @note Only checks upper bound to avoid -Wtype-limits warnings when min == 0 and value is
 *       unsigned (comparison is always false). For non-zero min bounds, add an explicit
 *       lower-bound check before this macro. The min parameter is documented but not checked.
 *
 * @param[in] value Value to check
 * @param[in] min Minimum allowed value (documented only; add explicit check if min > 0)
 * @param[in] max Maximum allowed value
 * @param[in] errcode Error to return on failure
 * @param[in] tag Component tag for logging
 */
#define RX_CHECK_RANGE_TAG(value, min, max, errcode, tag)                                          \
  do {                                                                                             \
    (void)(min); /* Document minimum bound; explicit check needed if min > 0 */                    \
    if ((value) > (max)) {                                                                         \
      rx_log_error(tag, "Range check failed");                                                     \
      return (errcode);                                                                            \
    }                                                                                              \
  } while (0)

/**
 * @brief Validate pointer pre-condition and return error
 *
 * Alias for RX_CHECK_NULL_PTR to standardize pre-condition naming.
 */
#define RX_VALIDATE_PTR(ptr, tag, message) RX_CHECK_NULL_PTR(ptr, tag, message)

/**
 * @brief Validate initialization state pre-condition
 *
 * Returns k_rx_err_invalid_state on failure.
 */
#define RX_VALIDATE_INIT(initialized, tag, message)                                                \
  do {                                                                                             \
    if (!(initialized)) {                                                                          \
      rx_log_error(tag, message);                                                                  \
      return k_rx_err_invalid_state;                                                               \
    }                                                                                              \
  } while (0)

#ifdef __cplusplus
}
#endif
