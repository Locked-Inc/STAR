/* lib/rx_core/inc/rx_err.h */

/**
 * @file rx_err.h
 * @brief Error Code Definitions for RX72N Firmware
 *
 * @details
 * Comprehensive error code system for the STAR RX72N motor controller firmware.
 * Provides type-safe error handling with categorized error codes following
 * C23 typed enum standards for embedded safety-critical systems.
 *
 * ## Architecture Design
 *
 * This module implements a centralized error handling system for safety-critical
 * embedded requirements:
 * - **Type-safe:** Uses C23 typed enums (uint16_t underlying type)
 * - **Zero overhead:** Inline helper functions compile to single instructions
 * - **Extensible:** Category-based organization supports future error codes
 * - **Traceable:** Each error code maps to specific failure conditions
 *
 * ## Error Code Organization
 *
 * Error codes are organized into non-overlapping ranges to facilitate debugging
 * and logging. The hex representation makes category identification trivial:
 *
 * | Range       | Category      | Purpose                                |
 * |-------------|---------------|----------------------------------------|
 * | 0x000       | Success       | Operation completed successfully       |
 * | 0x100-0x1FF | Generic       | General-purpose errors (null, timeout) |
 * | 0x200-0x2FF | Hardware      | Peripheral and GPIO errors             |
 * | 0x300-0x3FF | RTOS          | ThreadX and synchronization errors     |
 * | 0x400-0x4FF | Communication | SPI, UART, I2C, protocol errors        |
 * | 0x500-0x5FF | Validation    | Input validation and checksum errors   |
 *
 * ## Design Rationale
 *
 * **Why signed int32_t for rx_err_t but unsigned uint16_t for enum?**
 * - rx_err_t uses int32_t to allow negative error codes (future compatibility)
 * - Enum uses uint16_t to minimize memory usage (most values < 0xFFFF)
 * - Implicit conversion is safe: uint16_t -> int32_t always preserves value
 *
 * **Why category-based ranges instead of linear numbering?**
 * - Enables fast error categorization in logging (e.g., "0x2xx = hardware")
 * - Supports future expansion within categories without renumbering
 * - Makes error codes human-readable in hex dumps and debuggers
 *
 * **Why inline helpers instead of macros?**
 * - Type safety: compiler checks parameter types
 * - Debuggable: can step into functions, set breakpoints
 * - No macro pitfalls: no multiple evaluation, proper scoping
 *
 * ## Performance Characteristics
 *
 * All operations are zero-cost abstractions:
 * - rx_err_from_threadx(): Single comparison + conditional move (1-2 cycles)
 * - rx_err_is_ok(): Single comparison (1 cycle)
 * - rx_err_is_error(): Single comparison (1 cycle)
 *
 * ## Memory Usage
 *
 * - rx_err_t: 4 bytes per error variable
 * - rx_err_codes_t: 2 bytes per enum value
 * - Helper functions: 0 bytes (inlined by compiler at -O1+)
 *
 * ## Thread Safety
 *
 * All functions in this module are inherently thread-safe (pure functions).
 * Error codes are read-only constants after compilation.
 *
 * @par Module Dependencies:
 * - stdint.h: Fixed-width integer types
 * - stdbool.h: Boolean type for helper functions
 *
 * @see rx_check.h Validation macros that return rx_err_t codes
 * @see rx_error_handler.h Error logging and handling infrastructure
 * @see docs/sections/06_nasa_power_of_10.tex NASA compliance documentation
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion (pure constant definitions)
 * - Rule 2: [OK] N/A (no loops)
 * - Rule 3: [OK] No dynamic allocation (compile-time constants only)
 * - Rule 4: [OK] All functions < 10 lines (simple inline helpers)
 * - Rule 5: [OK] Inline functions have implicit pre/post conditions
 * - Rule 6: [OK] All identifiers have minimal scope
 * - Rule 7: [OK] N/A (no function calls to check)
 * - Rule 8: [OK] Uses C23 typed enums (k_rx_err_codes_t : uint16_t)
 * - Rule 9: [OK] N/A (no function pointers)
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Only error code definitions, no handling logic
 * - **O (Open/Closed):** Extensible via new enum values, closed for modification
 * - **L (Liskov Substitution):** rx_err_t can substitute for int32_t in APIs
 * - **I (Interface Segregation):** Minimal interface (3 helper functions)
 * - **D (Dependency Inversion):** No dependencies on concrete implementations
 *
 * @author STAR Team
 * @date 2026-01-27
 * @version 1.0.0
 * @copyright MIT License
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Error Type Definition
 * =============================================================================
 */

/**
 * @typedef rx_err_t
 * @brief Error code type for RX72N firmware
 *
 * @details
 * Signed 32-bit integer type used for all function return values that
 * indicate success or failure. This type provides a consistent error
 * handling interface across the entire firmware codebase.
 *
 * ## Type Selection Rationale
 *
 * **Why int32_t instead of enum?**
 * - Allows future negative error codes (e.g., -1 for EOF-like conditions)
 * - Compatible with standard POSIX error conventions
 * - Sufficient range for all error categories (only using 0x000-0x5FF currently)
 * - Efficient on 32-bit ARM Cortex-R4 (native register width)
 *
 * **Why signed instead of unsigned?**
 * - Enables use of negative values for special conditions
 * - Matches C standard library conventions (errno, etc.)
 * - Prevents accidental unsigned wraparound bugs
 *
 * ## Value Semantics
 *
 * - **0 (k_rx_ok):** Success - operation completed as requested
 * - **Non-zero:** Error - operation failed, consult specific error code
 * - **Positive values:** Standard error codes (0x100-0x5FF)
 * - **Negative values:** Reserved for future use
 *
 * ## Usage Pattern
 *
 * @par Standard Error Checking:
 * @code{.c}
 * rx_err_t err = some_function();
 * if (err != k_rx_ok) {
 *     // Handle error
 *     rx_log_error("TAG", "Operation failed: 0x%X", err);
 *     return err;  // Propagate error up call stack
 * }
 * @endcode
 *
 * @par Early Return Pattern:
 * @code{.c}
 * rx_err_t init_subsystem(void) {
 *     rx_err_t err;
 *
 *     err = init_hardware();
 *     if (err != k_rx_ok) return err;
 *
 *     err = init_rtos();
 *     if (err != k_rx_ok) return err;
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Switch-Based Error Handling:
 * @code{.c}
 * rx_err_t err = spi_transfer(data, len);
 * switch (err) {
 *     case k_rx_ok:
 *         // Success path
 *         break;
 *     case k_rx_err_timeout:
 *         // Retry logic
 *         retry_transfer();
 *         break;
 *     case k_rx_err_nack:
 *         // Device not responding
 *         reset_device();
 *         break;
 *     default:
 *         // Unexpected error
 *         log_error(err);
 *         break;
 * }
 * @endcode
 *
 * @par Macro-Based Error Propagation:
 * @code{.c}
 * #define RX_RETURN_ON_ERROR(err, tag, msg) \
 *     do { \
 *         rx_err_t _err = (err); \
 *         if (_err != k_rx_ok) { \
 *             rx_log_error((tag), (msg)); \
 *             return _err; \
 *         } \
 *     } while (0)
 *
 * rx_err_t init_system(void) {
 *     RX_RETURN_ON_ERROR(init_gpio(), "INIT", "GPIO init failed");
 *     RX_RETURN_ON_ERROR(init_spi(), "INIT", "SPI init failed");
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @note This type is used by ALL functions that can fail. Consistent usage
 *       across the codebase enables systematic error handling and logging.
 *
 * @warning Never cast arbitrary integers to rx_err_t. Only use predefined
 *          k_rx_* constants or values returned from error conversion functions.
 *
 * @attention Functions returning rx_err_t MUST document all possible return
 *            values using @retval tags in their Doxygen documentation.
 *
 * @par Memory Layout:
 * - Size: 4 bytes
 * - Alignment: 4 bytes (native word on ARM Cortex-R4)
 * - Signedness: Signed (two's complement)
 * - Range: -2,147,483,648 to 2,147,483,647 (only 0-0x5FF currently used)
 *
 * @par Performance:
 * - Return value overhead: 0 cycles (returned in register r0)
 * - Comparison overhead: 1 cycle (CMP r0, #0)
 * - No heap allocation required
 *
 * @see rx_err_codes_t Enumeration of all defined error codes
 * @see rx_err_is_ok() Helper to check for success
 * @see rx_err_is_error() Helper to check for failure
 * @see rx_err_from_threadx() Convert ThreadX codes to rx_err_t
 *
 * @since Version 1.0.0
 */
typedef int32_t rx_err_t;

/* =============================================================================
 * Error Codes (Following Style Guide: Enums > Const > Macros)
 * =============================================================================
 */

/**
 * @enum rx_err_codes_t
 * @brief Comprehensive error code enumeration for RX72N firmware
 *
 * @details
 * Type-safe error codes using C23 typed enum syntax with explicit uint16_t
 * underlying type. All error codes are compile-time constants organized into
 * logical categories for systematic error handling and debugging.
 *
 * ## Error Code Categories
 *
 * The error code space is partitioned into non-overlapping ranges:
 *
 * | Range       | Category      | Count | Purpose                                     |
 * |-------------|---------------|-------|---------------------------------------------|
 * | 0x000       | Success       | 1     | Operation completed successfully            |
 * | 0x100-0x1FF | Generic       | 15    | General-purpose errors (null, timeout, etc.)|
 * | 0x200-0x2FF | Hardware      | 9     | Peripheral, GPIO, and sensor errors         |
 * | 0x300-0x3FF | RTOS          | 6     | ThreadX and synchronization errors          |
 * | 0x400-0x4FF | Communication | 8     | SPI, UART, I2C, and protocol errors         |
 * | 0x500-0x5FF | Validation    | 4     | Input validation and checksum errors        |
 *
 * ## Error Category Flow
 *
 * Typical error propagation through the system:
 *
 * @dot
 * digraph error_flow {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   hardware [label="Hardware Layer\n(0x2xx)", fillcolor=lightblue, style=filled];
 *   driver [label="Driver Layer\n(0x4xx)", fillcolor=lightgreen, style=filled];
 *   rtos [label="RTOS Layer\n(0x3xx)", fillcolor=lightyellow, style=filled];
 *   validation [label="Validation\n(0x5xx)", fillcolor=lightpink, style=filled];
 *   generic [label="Generic Errors\n(0x1xx)", fillcolor=lightgray, style=filled];
 *   app [label="Application", fillcolor=white];
 *
 *   hardware -> driver [label="hw_error"];
 *   driver -> app [label="comm_error"];
 *   rtos -> app [label="rtos_error"];
 *   validation -> driver [label="invalid_arg"];
 *   generic -> app [label="timeout/busy"];
 * }
 * @enddot
 *
 * ## Error Handling Patterns
 *
 * @par Pattern 1: Direct Comparison
 * @code{.c}
 * rx_err_t err = motor_start();
 * if (err == k_rx_ok) {
 *     // Success
 * } else if (err == k_rx_err_estop) {
 *     // Emergency stop active - cannot start
 * } else {
 *     // Other error - log and handle
 * }
 * @endcode
 *
 * @par Pattern 2: Category Check
 * @code{.c}
 * rx_err_t err = spi_transfer();
 * if ((err & 0xF00) == 0x400) {
 *     // Communication error category
 *     restart_communication();
 * } else if ((err & 0xF00) == 0x200) {
 *     // Hardware error category
 *     reset_hardware();
 * }
 * @endcode
 *
 * @par Pattern 3: Error Logging with Context
 * @code{.c}
 * rx_err_t err = init_subsystem();
 * if (err != k_rx_ok) {
 *     rx_log_error("INIT", "Subsystem init failed: 0x%03X", err);
 *     // Hex dump shows category: 0x2xx = hardware, 0x4xx = comm, etc.
 * }
 * @endcode
 *
 * @par Usage Example - Complete Error Handling:
 * @code{.c}
 * #include "rx_err.h"
 * #include "rx_log.h"
 *
 * rx_err_t initialize_motor_controller(void) {
 *     rx_err_t err;
 *
 *     // Initialize hardware
 *     err = init_gpio();
 *     if (err != k_rx_ok) {
 *         if (err == k_rx_err_gpio_conflict) {
 *             rx_log_error("MOTOR", "GPIO conflict - check pin assignments");
 *         }
 *         return err;
 *     }
 *
 *     // Initialize SPI for RPi5 communication
 *     err = init_spi();
 *     if (err != k_rx_ok) {
 *         if (err == k_rx_err_spi_error) {
 *             rx_log_error("SPI", "SPI initialization failed");
 *         }
 *         return err;
 *     }
 *
 *     // Create RTOS resources
 *     err = create_motor_thread();
 *     if (err != k_rx_ok) {
 *         if (err == k_rx_err_rtos_thread_create) {
 *             rx_log_error("MOTOR", "Thread creation failed - out of memory?");
 *         }
 *         return err;
 *     }
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @invariant All error codes except k_rx_ok are non-zero
 * @invariant Error code ranges never overlap between categories
 * @invariant Error codes are compile-time constants (never modified at runtime)
 *
 * @note Uses C23 typed enum syntax `: uint16_t` for guaranteed size and ABI stability
 * @note Error codes are designed to be human-readable in hex (category visible)
 * @note All error codes fit in uint16_t (0x000-0x5FF) for memory efficiency
 *
 * @warning Do not cast arbitrary integers to this enum - only use defined constants
 * @warning Do not modify error codes at runtime - they are compile-time constants
 *
 * @see rx_err_t Type alias for function return values
 * @see rx_err_is_ok() Helper to check for success
 * @see rx_err_is_error() Helper to check for failure
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Success - operation completed as requested
   * @details
   * The only non-error value. Indicates successful completion of an operation
   * with no errors, warnings, or exceptional conditions. All outputs are valid
   * and postconditions are satisfied.
   *
   * @par Value: 0x000
   * @par Usage: Return this when function succeeds completely
   * @par Example:
   * @code{.c}
   * rx_err_t validate_input(uint8_t value) {
   *     if (value < 100) {
   *         return k_rx_ok;  // Valid input
   *     }
   *     return k_rx_err_out_of_range;
   * }
   * @endcode
   */
  k_rx_ok = 0,

  /* =========================================================================
   * Generic Errors (0x100 - 0x1FF)
   * General-purpose errors not specific to hardware or communication
   * =========================================================================
   */

  /**
   * @brief Generic unspecified failure
   * @details
   * Used when a function fails but the specific error type is unknown or
   * doesn't fit other categories. Prefer more specific error codes when possible.
   *
   * @par Value: 0x101
   * @par When to use: Last resort when no specific error applies
   * @par Recovery: Log error, inspect code path to determine root cause
   * @see Use more specific errors like k_rx_err_invalid_arg when applicable
   */
  k_rx_fail = 0x101,

  /**
   * @brief Out of memory - allocation failed
   * @details
   * Memory allocation failed (heap, stack, or pool). In safety-critical code
   * with zero dynamic allocation (NASA Rule 3), this indicates static buffer
   * exhaustion (e.g., ring buffer full, pool depleted).
   *
   * @par Value: 0x102
   * @par Cause: Static buffer full, RTOS pool exhausted, stack overflow imminent
   * @par Recovery: Release resources, reduce load, increase buffer size
   * @par Prevention: Size buffers based on worst-case analysis
   * @warning In zero-allocation systems, this indicates design issue (buffers too small)
   */
  k_rx_err_no_mem = 0x102,

  /**
   * @brief Invalid function argument
   * @details
   * Caller provided parameter outside valid range, nullptr where non-NULL
   * required, or logically inconsistent arguments (e.g., buffer_size > max_size).
   *
   * @par Value: 0x103
   * @par Cause: Parameter validation failed in function preconditions
   * @par Recovery: Fix caller code, add validation at call site
   * @par Prevention: Use RX_CHECK_NULL_PTR and RX_CHECK_RANGE macros
   * @see k_rx_err_null_ptr More specific error for nullptr pointers
   * @see k_rx_err_range_check_failed More specific for range violations
   */
  k_rx_err_invalid_arg = 0x103,

  /**
   * @brief Invalid state for requested operation
   * @details
   * Function called when module is in wrong state. For example, calling
   * motor_start() when motor is already running, or spi_transfer() before
   * spi_init() completed successfully.
   *
   * @par Value: 0x104
   * @par Cause: State machine in unexpected state for requested operation
   * @par Recovery: Check module state, call initialization functions first
   * @par Prevention: Document valid state transitions, enforce with state checks
   * @see k_rx_err_not_initialized Specific case of invalid state (not initialized)
   */
  k_rx_err_invalid_state = 0x104,

  /**
   * @brief Invalid size parameter
   * @details
   * Size parameter outside valid range. Examples: buffer size exceeds maximum,
   * packet length too short, array size is zero.
   *
   * @par Value: 0x105
   * @par Cause: Size parameter failed validation (too large, too small, misaligned)
   * @par Recovery: Adjust size parameter to valid range
   * @par Prevention: Document valid size ranges in function documentation
   * @see k_rx_err_invalid_arg More general argument validation error
   */
  k_rx_err_invalid_size = 0x105,

  /**
   * @brief Requested item not found
   * @details
   * Search, lookup, or query operation failed to find requested item.
   * Examples: device not found on bus, configuration key missing, motor ID invalid.
   *
   * @par Value: 0x106
   * @par Cause: Item does not exist in collection, device not on bus, ID not registered
   * @par Recovery: Verify item exists, check ID is correct, enumerate available items
   * @par Prevention: Validate IDs before use, document valid ID ranges
   */
  k_rx_err_not_found = 0x106,

  /**
   * @brief Feature or operation not supported
   * @details
   * Requested functionality not implemented in this build or hardware configuration.
   * Examples: hardware feature not available on this MCU variant, optional module
   * not compiled in, operation not supported by peripheral.
   *
   * @par Value: 0x107
   * @par Cause: Feature not implemented, hardware limitation, compile-time config
   * @par Recovery: Use alternative approach, check hardware capabilities
   * @par Prevention: Document supported features, provide capability query API
   */
  k_rx_err_not_supported = 0x107,

  /**
   * @brief Operation timed out
   * @details
   * Operation exceeded allowed time limit. Examples: peripheral not ready within
   * timeout, RTOS wait timeout expired, communication timeout (no response).
   *
   * @par Value: 0x108
   * @par Cause: Peripheral hung, device not responding, deadlock, excessive load
   * @par Recovery: Retry operation, reset peripheral, increase timeout
   * @par Prevention: Tune timeouts based on empirical testing, implement watchdogs
   * @see k_rx_err_hw_timeout Hardware-specific timeout
   */
  k_rx_err_timeout = 0x108,

  /**
   * @brief Resource busy - operation cannot proceed
   * @details
   * Requested resource is currently in use and cannot service request.
   * Examples: SPI bus busy, mutex locked by other thread, DMA channel active.
   *
   * @par Value: 0x109
   * @par Cause: Resource locked, peripheral busy, exclusive access required
   * @par Recovery: Wait for resource, retry later, queue request
   * @par Prevention: Implement resource arbitration, use RTOS synchronization
   * @see k_rx_err_would_block Non-blocking operation would block
   */
  k_rx_err_busy = 0x109,

  /**
   * @brief No application data available (control frame received)
   * @details
   * Operation succeeded but no application-layer data was returned. Distinct from
   * k_rx_err_timeout - receive was successful but the frame was a control frame
   * (ACK/NACK/PING/PONG/RESET) not intended for application consumption.
   *
   * @par Value: 0x10A
   * @par Cause: Received control frame instead of data frame
   * @par Recovery: This is normal - caller should continue polling
   * @par Prevention: Not an error condition - expected behavior
   * @see k_rx_err_timeout No data due to timeout
   */
  k_rx_err_no_data = 0x10A,

  /**
   * @brief Non-blocking operation would block
   * @details
   * Non-blocking function called but operation would block waiting for resource.
   * Differs from k_rx_err_busy: this indicates caller requested non-blocking mode
   * but operation requires waiting.
   *
   * @par Value: 0x10B
   * @par Cause: Non-blocking mode requested but data/resource not immediately available
   * @par Recovery: Retry later, use blocking mode, or poll until ready
   * @par Prevention: Check resource availability before calling non-blocking API
   * @see k_rx_err_busy Resource busy in blocking mode
   */
  k_rx_err_would_block = 0x10B,

  /**
   * @brief Item already exists - cannot create
   * @details
   * Attempted to create resource that already exists. Examples: thread already
   * created, device already registered, configuration already initialized.
   *
   * @par Value: 0x10C
   * @par Cause: Duplicate creation attempt, resource already allocated
   * @par Recovery: Use existing resource, or delete and recreate
   * @par Prevention: Check existence before creation, track resource state
   */
  k_rx_err_exists = 0x10C,

  /**
   * @brief Container empty - no data available
   * @details
   * Attempted to retrieve data from empty container. Examples: ring buffer empty,
   * queue empty, receive FIFO empty.
   *
   * @par Value: 0x10D
   * @par Cause: No data available, all items consumed
   * @par Recovery: Wait for data, check count before reading
   * @par Prevention: Check count/size before attempting read
   * @see k_rx_err_would_block Non-blocking read from empty container
   */
  k_rx_err_empty = 0x10D,

  /**
   * @brief Operation cancelled by request
   * @details
   * Operation was cancelled before completion, either by explicit cancel request
   * or by system shutdown. No side effects occurred (atomic cancellation).
   *
   * @par Value: 0x10E
   * @par Cause: User cancel request, system shutdown, higher priority task
   * @par Recovery: Clean up resources, update state to reflect cancellation
   * @par Prevention: Implement graceful cancellation support
   */
  k_rx_err_cancelled = 0x10E,

  /**
   * @brief Module not initialized
   * @details
   * Function called before module initialization completed. All modules must be
   * initialized via their init() function before use.
   *
   * @par Value: 0x10F
   * @par Cause: init() not called, init() failed, module deinitialized
   * @par Recovery: Call module_init() before using module functions
   * @par Prevention: Document initialization requirements, enforce with state checks
   * @see k_rx_err_invalid_state More general state error
   */
  k_rx_err_not_initialized = 0x10F,

  /**
   * @brief Emergency stop active - operation forbidden
   * @details
   * Emergency stop triggered, all motor operations halted. System in safe state,
   * requires manual reset before normal operation can resume.
   *
   * @par Value: 0x110
   * @par Cause: Emergency stop button pressed, safety fault detected
   * @par Recovery: Clear fault condition, call reset function, acknowledge e-stop
   * @par Prevention: Implement proper safety interlocks
   * @warning This is a safety-critical error - do not bypass e-stop checks
   */
  k_rx_err_estop = 0x110,

  /* =========================================================================
   * Hardware Errors (0x200 - 0x2FF)
   * Peripheral, GPIO, and hardware interface errors
   * =========================================================================
   */

  /**
   * @brief Hardware initialization failed
   * @details
   * Hardware peripheral failed to initialize. Examples: peripheral not responding,
   * clock configuration failed, register write verification failed.
   *
   * @par Value: 0x201
   * @par Cause: Hardware fault, incorrect configuration, missing hardware
   * @par Recovery: Check hardware connections, verify clock settings, reset MCU
   * @par Prevention: Validate hardware presence before init
   */
  k_rx_err_hw_init_failed = 0x201,

  /**
   * @brief Hardware not ready for operation
   * @details
   * Hardware peripheral exists but not ready for requested operation. Examples:
   * PLL not locked, ADC not calibrated, peripheral in reset state.
   *
   * @par Value: 0x202
   * @par Cause: Peripheral needs time to stabilize, calibration incomplete
   * @par Recovery: Wait for ready flag, poll status register, retry after delay
   * @par Prevention: Implement proper hardware sequencing and delays
   */
  k_rx_err_hw_not_ready = 0x202,

  /**
   * @brief Hardware operation timed out
   * @details
   * Hardware peripheral failed to complete operation within timeout. Distinct from
   * k_rx_err_timeout (generic) - this is hardware-specific.
   *
   * @par Value: 0x203
   * @par Cause: Peripheral hung, bus arbitration lost, external device not responding
   * @par Recovery: Reset peripheral, check external hardware, increase timeout
   * @par Prevention: Tune hardware timeouts, implement watchdog recovery
   */
  k_rx_err_hw_timeout = 0x203,

  /**
   * @brief Generic hardware error
   * @details
   * Unspecified hardware error detected. Examples: fault flag set, error interrupt,
   * hardware self-test failure.
   *
   * @par Value: 0x204
   * @par Cause: Hardware fault condition, transient error, EMI/noise
   * @par Recovery: Read hardware error registers, reset peripheral, retry operation
   * @par Prevention: Implement hardware error monitoring and logging
   */
  k_rx_err_hw_error = 0x204,

  /**
   * @brief GPIO pin conflict detected
   * @details
   * GPIO pin already in use by another peripheral or function. Pin multiplexing
   * conflict must be resolved before pin can be used.
   *
   * @par Value: 0x205
   * @par Cause: Multiple peripherals mapped to same pin, incorrect MPC settings
   * @par Recovery: Reconfigure pin mux, use alternative pin, disable conflicting peripheral
   * @par Prevention: Document pin assignments, validate at init time
   * @see docs/sections/03_hardware_pinout.tex Complete pin assignment table
   */
  k_rx_err_gpio_conflict = 0x205,

  /**
   * @brief Invalid GPIO port number
   * @details
   * GPIO port number outside valid range for RX72N (valid: 0-K, excluding reserved).
   *
   * @par Value: 0x206
   * @par Cause: Incorrect port constant, typo in pin definition
   * @par Recovery: Use valid port number (PORT0-PORTK), check hardware manual
   * @par Prevention: Use enum constants from rx_port_constants.h
   */
  k_rx_err_gpio_invalid_port = 0x206,

  /**
   * @brief Invalid GPIO pin number
   * @details
   * GPIO pin number outside valid range (0-7 for most ports on RX72N).
   *
   * @par Value: 0x207
   * @par Cause: Incorrect pin constant, array index out of bounds
   * @par Recovery: Use valid pin number (0-7), check port definition
   * @par Prevention: Use enum constants from rx_gpio_constants.h
   */
  k_rx_err_gpio_invalid_pin = 0x207,

  /**
   * @brief Sensor measurement out of valid range
   * @details
   * Sensor reading outside physically possible or configured range. Examples:
   * ADC overflow, temperature sensor disconnected, ultrasonic no echo.
   *
   * @par Value: 0x208
   * @par Cause: Sensor fault, value exceeds limits, sensor disconnected
   * @par Recovery: Check sensor connections, validate sensor state, clamp to range
   * @par Prevention: Implement sensor health monitoring
   */
  k_rx_err_out_of_range = 0x208,

  /**
   * @brief Hardware register block not mapped in memory
   * @details
   * Attempted to access peripheral whose registers are not mapped at expected
   * address. Indicates hardware configuration error or unsupported MCU variant.
   *
   * @par Value: 0x209
   * @par Cause: Wrong MCU variant, peripheral not present, incorrect memory map
   * @par Recovery: Verify MCU part number, check datasheet, disable peripheral
   * @par Prevention: Conditional compilation based on MCU variant
   */
  k_rx_err_hw_unmapped = 0x209,

  /* =========================================================================
   * RTOS Errors (0x300 - 0x3FF)
   * ThreadX and synchronization errors
   * =========================================================================
   */

  /**
   * @brief Generic RTOS error
   * @details
   * Unspecified ThreadX error occurred. Check ThreadX status code for details.
   *
   * @par Value: 0x301
   * @par Cause: ThreadX API returned non-success status
   * @par Recovery: Examine ThreadX status, consult ThreadX documentation
   * @par Prevention: Validate ThreadX API parameters
   * @see rx_err_from_threadx() Convert ThreadX codes to rx_err_t
   */
  k_rx_err_rtos_error = 0x301,

  /**
   * @brief ThreadX error (alias for k_rx_err_rtos_error)
   * @details
   * Identical to k_rx_err_rtos_error, provided for explicit ThreadX naming.
   *
   * @par Value: 0x301
   */
  k_rx_err_threadx = 0x301,

  /**
   * @brief ThreadX thread creation failed
   * @details
   * tx_thread_create() failed. Common causes: out of memory, invalid priority,
   * invalid stack.
   *
   * @par Value: 0x302
   * @par Cause: Insufficient memory, stack too small, invalid parameters
   * @par Recovery: Increase heap size, validate thread parameters
   * @par Prevention: Size memory pools based on worst-case thread count
   */
  k_rx_err_rtos_thread_create = 0x302,

  /**
   * @brief Semaphore operation failed
   * @details
   * Semaphore create, get, or put operation failed. Check ThreadX error code.
   *
   * @par Value: 0x303
   * @par Cause: Invalid semaphore handle, timeout, semaphore not created
   * @par Recovery: Verify semaphore initialized, check timeout value
   * @par Prevention: Initialize all RTOS objects before use
   */
  k_rx_err_rtos_semaphore = 0x303,

  /**
   * @brief Mutex operation failed
   * @details
   * Mutex create, get, or put operation failed. Possible deadlock or priority
   * inversion if using priority inheritance.
   *
   * @par Value: 0x304
   * @par Cause: Invalid mutex handle, timeout, mutex already owned
   * @par Recovery: Check mutex state, verify not already locked
   * @par Prevention: Use timeouts, avoid nested mutex acquisition
   */
  k_rx_err_rtos_mutex = 0x304,

  /**
   * @brief Queue operation failed
   * @details
   * Queue create, send, or receive operation failed.
   *
   * @par Value: 0x305
   * @par Cause: Queue full, queue empty, invalid queue handle
   * @par Recovery: Increase queue size, implement flow control
   * @par Prevention: Size queues based on worst-case message rate
   */
  k_rx_err_rtos_queue = 0x305,

  /**
   * @brief Timer operation failed
   * @details
   * Timer create, activate, or deactivate operation failed.
   *
   * @par Value: 0x306
   * @par Cause: Invalid timer handle, timer already active
   * @par Recovery: Check timer state before operation
   * @par Prevention: Track timer state explicitly
   */
  k_rx_err_rtos_timer = 0x306,

  /* =========================================================================
   * Communication Errors (0x400 - 0x4FF)
   * SPI, UART, I2C, and protocol errors
   * =========================================================================
   */

  /**
   * @brief Generic communication error
   * @details
   * Unspecified communication error on SPI, UART, or I2C bus.
   *
   * @par Value: 0x401
   * @par Cause: Bus error, framing error, data corruption
   * @par Recovery: Reset bus, retry transfer, check connections
   * @par Prevention: Implement error detection (CRC, checksums)
   */
  k_rx_err_comm_error = 0x401,

  /**
   * @brief SPI communication error
   * @details
   * SPI transfer failed. Examples: CIPO timeout, bus error, chip select conflict.
   *
   * @par Value: 0x402
   * @par Cause: No response on CIPO, mode mismatch, clock issue
   * @par Recovery: Check SPI mode, verify clock frequency, check CS timing
   * @par Prevention: Validate SPI configuration before transfer
   */
  k_rx_err_spi_error = 0x402,

  /**
   * @brief UART communication error
   * @details
   * UART error detected. Examples: framing error, parity error, overrun.
   *
   * @par Value: 0x403
   * @par Cause: Baud rate mismatch, noise, buffer overrun
   * @par Recovery: Clear error flags, adjust baud rate, reduce data rate
   * @par Prevention: Use hardware flow control, size buffers adequately
   */
  k_rx_err_uart_error = 0x403,

  /**
   * @brief I2C communication error
   * @details
   * I2C (RIIC) error detected. Examples: bus busy, arbitration lost, NACK.
   *
   * @par Value: 0x404
   * @par Cause: Device not responding, wrong address, bus contention
   * @par Recovery: Reset I2C bus, verify device address, check pull-ups
   * @par Prevention: Implement bus recovery, validate addresses
   */
  k_rx_err_i2c_error = 0x404,

  /**
   * @brief CRC validation failed
   * @details
   * Received data CRC does not match calculated CRC. Data corruption detected.
   *
   * @par Value: 0x405
   * @par Cause: Data corruption, EMI, faulty peripheral
   * @par Recovery: Request retransmission, check signal integrity
   * @par Prevention: Use shielded cables, implement retry logic
   * @see k_rx_err_checksum_mismatch Simpler checksum validation
   */
  k_rx_err_crc_mismatch = 0x405,

  /**
   * @brief Protocol error detected
   * @details
   * Communication protocol violation. Examples: invalid packet format, unexpected
   * sequence number, protocol state machine error.
   *
   * @par Value: 0x406
   * @par Cause: Protocol version mismatch, malformed packet, out-of-sequence
   * @par Recovery: Reset protocol state machine, request resynchronization
   * @par Prevention: Validate all protocol fields, implement version negotiation
   */
  k_rx_err_protocol_error = 0x406,

  /**
   * @brief NACK (Not Acknowledged) received
   * @details
   * Peripheral responded with NACK (negative acknowledgment). Device refusing
   * operation or not ready.
   *
   * @par Value: 0x407
   * @par Cause: Device busy, invalid command, device not ready
   * @par Recovery: Retry after delay, check device state
   * @par Prevention: Poll device ready status before commands
   */
  k_rx_err_nack = 0x407,

  /**
   * @brief Resource conflict in communication
   * @details
   * Multiple controllers attempting bus access, or conflicting operation in progress.
   *
   * @par Value: 0x408
   * @par Cause: Bus arbitration lost, conflicting DMA channels
   * @par Recovery: Implement bus arbitration, retry after backoff
   * @par Prevention: Use RTOS mutexes for shared buses
   */
  k_rx_err_conflict = 0x408,

  /**
   * @brief Retransmission limit exceeded
   * @details
   * Automatic retransmission exhausted all retry attempts without receiving ACK.
   * Frame delivery cannot be confirmed.
   *
   * @par Value: 0x409
   * @par Cause: Remote side not responding, persistent CRC failures, link down
   * @par Recovery: Reset communication, check physical connection, increase retries
   * @par Prevention: Tune timeout/retry parameters for link quality
   * @see rx_spi_comm_set_auto_retransmit() Configure retry parameters
   */
  k_rx_err_retry_limit = 0x409,

  /* =========================================================================
   * Validation Errors (0x500 - 0x5FF)
   * Input validation and checksum errors
   * =========================================================================
   */

  /**
   * @brief Generic validation failed
   * @details
   * Data validation or consistency check failed.
   *
   * @par Value: 0x501
   * @par Cause: Invalid data format, constraint violation, consistency check failed
   * @par Recovery: Reject invalid data, request valid input
   * @par Prevention: Validate all inputs at system boundaries
   */
  k_rx_err_validation_failed = 0x501,

  /**
   * @brief Checksum mismatch detected
   * @details
   * Simple checksum validation failed. Less robust than CRC but faster.
   *
   * @par Value: 0x502
   * @par Cause: Data corruption, transmission error
   * @par Recovery: Request retransmission
   * @par Prevention: Use CRC for critical data
   * @see k_rx_err_crc_mismatch More robust CRC validation
   */
  k_rx_err_checksum_mismatch = 0x502,

  /**
   * @brief Range check failed
   * @details
   * Value outside configured or physically possible range.
   *
   * @par Value: 0x503
   * @par Cause: Value exceeds min/max limits
   * @par Recovery: Clamp to valid range, reject input
   * @par Prevention: Document and enforce valid ranges
   */
  k_rx_err_range_check_failed = 0x503,

  /**
   * @brief Null pointer detected
   * @details
   * Required pointer parameter is nullptr. Subset of k_rx_err_invalid_arg.
   *
   * @par Value: 0x504
   * @par Cause: Caller passed NULL where non-NULL required
   * @par Recovery: Fix caller code to pass valid pointer
   * @par Prevention: Use RX_CHECK_NULL_PTR macro at function entry
   * @see k_rx_err_invalid_arg More general argument validation
   */
  k_rx_err_null_ptr = 0x504,

} rx_err_codes_t;

/* =============================================================================
 * Error Code Helpers
 * =============================================================================
 */

/**
 * @brief Convert ThreadX status code to rx_err_t
 *
 * @details
 * Converts ThreadX API return values (UINT status codes) to rx_err_t error codes.
 * ThreadX uses 0 for success (TX_SUCCESS) and non-zero for various error conditions.
 * This function performs a simple binary conversion: success -> k_rx_ok, any error -> k_rx_err_rtos_error.
 *
 * ## Algorithm
 *
 * 1. Compare tx_status against TX_SUCCESS (0)
 * 2. If equal: return k_rx_ok (success)
 * 3. If not equal: return k_rx_err_rtos_error (generic RTOS error)
 *
 * ## ThreadX Status Codes Mapped
 *
 * | ThreadX Status     | Value | rx_err_t Result      | Meaning                           |
 * |--------------------|-------|----------------------|-----------------------------------|
 * | TX_SUCCESS         | 0     | k_rx_ok (0x000)      | Operation successful              |
 * | TX_DELETED         | 0x01  | k_rx_err_rtos_error  | Object was deleted                |
 * | TX_NO_MEMORY       | 0x10  | k_rx_err_rtos_error  | Memory allocation failed          |
 * | TX_PTR_ERROR       | 0x03  | k_rx_err_rtos_error  | Invalid pointer                   |
 * | TX_WAIT_ERROR      | 0x04  | k_rx_err_rtos_error  | Invalid wait option               |
 * | TX_SIZE_ERROR      | 0x05  | k_rx_err_rtos_error  | Invalid size                      |
 * | TX_CALLER_ERROR    | 0x13  | k_rx_err_rtos_error  | Invalid caller context            |
 * | TX_NOT_AVAILABLE   | 0x1D  | k_rx_err_rtos_error  | Resource not available            |
 * | *(all other)*      | *     | k_rx_err_rtos_error  | Generic RTOS error                |
 *
 * @note This is a simplified conversion. For finer-grained error mapping, inspect
 *       the ThreadX status code directly and map to specific rx_err_t codes.
 *
 * @param[in] tx_status ThreadX status code returned from ThreadX API call
 *   - Type: UINT (32-bit unsigned integer in ThreadX)
 *   - Valid range: Any value (typically 0x00-0x1D for defined ThreadX errors)
 *   - Special values:
 *     - 0 (TX_SUCCESS): Indicates successful operation
 *     - Non-zero: Various ThreadX error conditions
 *
 * @return rx_err_t error code
 * @retval k_rx_ok (0x000) If tx_status == 0 (TX_SUCCESS), operation succeeded
 * @retval k_rx_err_rtos_error (0x301) If tx_status != 0, ThreadX error occurred
 *
 * @par Usage Example - Basic Conversion:
 * @code{.c}
 * #include "rx_err.h"
 * #include "tx_api.h"
 *
 * TX_THREAD my_thread;
 * CHAR thread_stack[1024];
 *
 * rx_err_t create_worker_thread(void) {
 *     UINT tx_status = tx_thread_create(
 *         &my_thread,                    // Thread control block
 *         "Worker",                      // Thread name
 *         worker_thread_entry,           // Entry function
 *         0,                             // Entry input
 *         thread_stack,                  // Stack start
 *         sizeof(thread_stack),          // Stack size
 *         16,                            // Priority
 *         16,                            // Preemption threshold
 *         TX_NO_TIME_SLICE,              // Time slice
 *         TX_AUTO_START                  // Auto start
 *     );
 *
 *     // Convert ThreadX status to rx_err_t
 *     rx_err_t err = rx_err_from_threadx(tx_status);
 *     if (err != k_rx_ok) {
 *         rx_log_error("RTOS", "Thread creation failed: TX status 0x%02X", tx_status);
 *         return err;
 *     }
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Usage Example - Inline in Error Check:
 * @code{.c}
 * rx_err_t init_rtos_resources(void) {
 *     // Create semaphore and convert result inline
 *     rx_err_t err = rx_err_from_threadx(
 *         tx_semaphore_create(&my_sem, "DataReady", 0)
 *     );
 *
 *     if (err != k_rx_ok) {
 *         return err;  // Propagate RTOS error
 *     }
 *
 *     // Continue with other RTOS operations...
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @par Performance:
 * - Execution time: ~2 cycles @ 240 MHz (CMP + CMOV)
 * - Stack usage: 0 bytes (inlined by compiler)
 * - Code size: 0 bytes when inlined (optimized to single instruction)
 *
 * @note Thread Safety: Pure function, inherently thread-safe (no shared state)
 * @note Re-entrancy: Fully reentrant (no side effects)
 * @note Memory: No dynamic allocation, no static variables
 *
 * @pre tx_status contains valid ThreadX status code (any UINT value accepted)
 * @post Return value is either k_rx_ok or k_rx_err_rtos_error (no other values possible)
 *
 * @invariant Result is always one of two values: k_rx_ok or k_rx_err_rtos_error
 *
 * @see k_rx_err_rtos_error Error code returned for all non-success ThreadX status
 * @see k_rx_ok Success code returned for TX_SUCCESS
 * @see tx_thread_create() Example ThreadX API that returns UINT status
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion
 * - Rule 2: [OK] N/A (no loops)
 * - Rule 3: [OK] No dynamic allocation
 * - Rule 4: [OK] Function is 1 line (excluding documentation)
 * - Rule 5: [OK] 1 precondition, 1 postcondition, 1 invariant
 * - Rule 6: [OK] Minimal scope (inline function)
 * - Rule 7: [OK] No function calls to check
 * - Rule 8: [OK] Uses typed enum constants
 * - Rule 9: [OK] No function pointers
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @since Version 1.0.0
 */
static inline rx_err_t rx_err_from_threadx(uint32_t tx_status)
{
  return (tx_status == 0) ? k_rx_ok : k_rx_err_rtos_error;
}

/**
 * @brief Check if error code represents success
 *
 * @details
 * Tests whether an rx_err_t value indicates successful operation (k_rx_ok).
 * This is the preferred way to check for success in STAR firmware, providing
 * explicit intent and type safety.
 *
 * ## Algorithm
 *
 * 1. Compare err against k_rx_ok (0)
 * 2. Return true if equal (success), false otherwise (error)
 *
 * ## Comparison to Alternatives
 *
 * | Method                  | Type Safe | Explicit Intent | Performance |
 * |-------------------------|-----------|-----------------|-------------|
 * | rx_err_is_ok(err)       | [OK]         | [OK]               | 1 cycle     |
 * | err == k_rx_ok          | [OK]         | [OK]               | 1 cycle     |
 * | !err                    | [X]         | [X]               | 1 cycle     |
 * | err == 0                | [X]         | [X]               | 1 cycle     |
 *
 * **Recommended:** Use `rx_err_is_ok(err)` for explicit intent and self-documenting code.
 *
 * @param[in] err Error code to check
 *   - Type: rx_err_t (int32_t)
 *   - Valid range: Any rx_err_t value (typically 0x000-0x5FF or 0)
 *   - Special values:
 *     - k_rx_ok (0): Success condition
 *     - Any non-zero: Error condition
 *
 * @return Boolean success indicator
 * @retval true If err == k_rx_ok (0), operation was successful
 * @retval false If err != k_rx_ok (any non-zero value), operation failed
 *
 * @par Usage Example - Basic Success Check:
 * @code{.c}
 * rx_err_t err = initialize_motor();
 * if (rx_err_is_ok(err)) {
 *     // Success path - motor initialized
 *     start_control_loop();
 * } else {
 *     // Error path - log and handle failure
 *     rx_log_error("MOTOR", "Init failed: 0x%X", err);
 *     enter_safe_state();
 * }
 * @endcode
 *
 * @par Usage Example - Assert Success:
 * @code{.c}
 * // Critical operation that must succeed
 * rx_err_t err = init_critical_subsystem();
 * assert(rx_err_is_ok(err) && "Critical init failed");  // NASA Rule 5
 * @endcode
 *
 * @par Usage Example - Conditional Compilation:
 * @code{.c}
 * #if DEBUG_LEVEL >= 2
 *     rx_err_t err = optional_debug_feature();
 *     if (!rx_err_is_ok(err)) {
 *         rx_log_warn("DEBUG", "Optional feature unavailable");
 *     }
 * #endif
 * @endcode
 *
 * @par Performance:
 * - Execution time: 1 cycle @ 240 MHz (single CMP instruction)
 * - Stack usage: 0 bytes (inlined by compiler)
 * - Code size: 0 bytes when inlined (optimized to single comparison)
 *
 * @note Thread Safety: Pure function, inherently thread-safe (no shared state)
 * @note Re-entrancy: Fully reentrant (no side effects, no mutable state)
 * @note Memory: No allocation, no static variables
 *
 * @pre err contains valid rx_err_t value (any int32_t accepted, but typically 0x000-0x5FF)
 * @post Return value is deterministic boolean based solely on input
 *
 * @invariant Result is always boolean (true or false, no other values)
 *
 * @see rx_err_is_error() Inverse check (returns true if error, false if success)
 * @see k_rx_ok Success constant (0x000)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion
 * - Rule 2: [OK] N/A (no loops)
 * - Rule 3: [OK] No dynamic allocation
 * - Rule 4: [OK] Function is 1 line
 * - Rule 5: [OK] 1 precondition, 1 postcondition, 1 invariant
 * - Rule 6: [OK] Minimal scope (inline function)
 * - Rule 7: [OK] No function calls to check
 * - Rule 8: [OK] Uses typed enum constant k_rx_ok
 * - Rule 9: [OK] No function pointers
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @since Version 1.0.0
 */
static inline bool rx_err_is_ok(rx_err_t err)
{
  return (err == k_rx_ok);
}

/**
 * @brief Check if error code represents failure
 *
 * @details
 * Tests whether an rx_err_t value indicates a failure condition (any non-zero value).
 * This is the logical inverse of rx_err_is_ok() and is used when error-checking
 * logic is more naturally expressed as "if error occurred" rather than "if success".
 *
 * ## Algorithm
 *
 * 1. Compare err against k_rx_ok (0)
 * 2. Return true if not equal (error), false otherwise (success)
 *
 * ## When to Use This vs rx_err_is_ok()
 *
 * **Use rx_err_is_error()** when error handling is the primary focus:
 * @code{.c}
 * rx_err_t err = risky_operation();
 * if (rx_err_is_error(err)) {
 *     // Handle error first (primary concern)
 *     handle_error(err);
 *     return;
 * }
 * // Continue with normal path
 * @endcode
 *
 * **Use rx_err_is_ok()** when success path is the primary focus:
 * @code{.c}
 * rx_err_t err = initialize_system();
 * if (rx_err_is_ok(err)) {
 *     // Success is the expected/happy path
 *     continue_normal_operation();
 * } else {
 *     // Error is exceptional
 *     handle_error(err);
 * }
 * @endcode
 *
 * @param[in] err Error code to check
 *   - Type: rx_err_t (int32_t)
 *   - Valid range: Any rx_err_t value (typically 0x000-0x5FF or 0)
 *   - Special values:
 *     - k_rx_ok (0): Success condition (returns false)
 *     - Any non-zero: Error condition (returns true)
 *
 * @return Boolean error indicator
 * @retval true If err != k_rx_ok (non-zero), operation failed
 * @retval false If err == k_rx_ok (0), operation was successful
 *
 * @par Usage Example - Early Return on Error:
 * @code{.c}
 * rx_err_t initialize_peripherals(void) {
 *     rx_err_t err;
 *
 *     err = init_gpio();
 *     if (rx_err_is_error(err)) {
 *         return err;  // Early return on error
 *     }
 *
 *     err = init_spi();
 *     if (rx_err_is_error(err)) {
 *         return err;  // Propagate error immediately
 *     }
 *
 *     err = init_i2c();
 *     if (rx_err_is_error(err)) {
 *         return err;
 *     }
 *
 *     return k_rx_ok;  // All initialization succeeded
 * }
 * @endcode
 *
 * @par Usage Example - Error Logging:
 * @code{.c}
 * rx_err_t err = send_command(cmd);
 * if (rx_err_is_error(err)) {
 *     // Log error with details
 *     rx_log_error("COMM", "Command send failed: 0x%03X", err);
 *
 *     // Category-specific handling
 *     if ((err & 0xF00) == 0x400) {
 *         reset_communication();
 *     }
 * }
 * @endcode
 *
 * @par Usage Example - Error Accumulation:
 * @code{.c}
 * uint32_t error_count = 0;
 * for (uint8_t i = 0; i < k_sensor_count; i++) {
 *     rx_err_t err = read_sensor(i);
 *     if (rx_err_is_error(err)) {
 *         error_count++;
 *         rx_log_warn("SENSOR", "Sensor %u failed: 0x%X", i, err);
 *     }
 * }
 *
 * if (error_count > k_max_allowed_failures) {
 *     enter_degraded_mode();
 * }
 * @endcode
 *
 * @par Performance:
 * - Execution time: 1 cycle @ 240 MHz (single CMP instruction)
 * - Stack usage: 0 bytes (inlined by compiler)
 * - Code size: 0 bytes when inlined (optimized to single comparison)
 *
 * @note Thread Safety: Pure function, inherently thread-safe (no shared state)
 * @note Re-entrancy: Fully reentrant (no side effects, no mutable state)
 * @note Memory: No allocation, no static variables
 *
 * @pre err contains valid rx_err_t value (any int32_t accepted)
 * @post Return value is deterministic boolean based solely on input
 *
 * @invariant Result is always boolean (true or false)
 * @invariant Exactly inverse of rx_err_is_ok(): rx_err_is_error(e) == !rx_err_is_ok(e)
 *
 * @see rx_err_is_ok() Inverse check (returns true if success, false if error)
 * @see k_rx_ok Success constant (0x000)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, recursion
 * - Rule 2: [OK] N/A (no loops)
 * - Rule 3: [OK] No dynamic allocation
 * - Rule 4: [OK] Function is 1 line
 * - Rule 5: [OK] 1 precondition, 1 postcondition, 2 invariants
 * - Rule 6: [OK] Minimal scope (inline function)
 * - Rule 7: [OK] No function calls to check
 * - Rule 8: [OK] Uses typed enum constant k_rx_ok
 * - Rule 9: [OK] No function pointers
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @since Version 1.0.0
 */
static inline bool rx_err_is_error(rx_err_t err)
{
  return (err != k_rx_ok);
}

#ifdef __cplusplus
}
#endif
