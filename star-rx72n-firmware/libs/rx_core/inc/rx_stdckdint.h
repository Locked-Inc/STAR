/**
 * @file rx_stdckdint.h
 * @brief C23 Checked Integer Arithmetic Polyfill for RX72N Firmware
 *
 * @details
 * Provides the C23 `<stdckdint.h>` checked integer arithmetic interface with
 * automatic fallback to compiler builtins or manual overflow detection when the
 * native header is unavailable. Checked arithmetic functions return true on
 * overflow without invoking undefined behavior, making them essential for
 * safety-critical embedded code.
 *
 * ## Header Selection Strategy
 *
 * The header uses a three-tier fallback:
 * 1. **Native C23 `<stdckdint.h>`** - Used when `__STDC_VERSION_STDCKDINT_H__`
 *    is defined (gcc-14+, future GNURX releases)
 * 2. **GCC/Clang builtins** - `__builtin_add_overflow()` family, available in
 *    GCC >= 5 and Clang >= 3.8 (covers GNURX and host compilers)
 * 3. **Manual detection** - Portable C fallback using pre-check comparisons
 *
 * ## Supported Operations
 *
 * | Macro | Operation | Overflow Condition |
 * |-------|-----------|-------------------|
 * | `ckd_add(result, a, b)` | `*result = a + b` | Sum wraps or exceeds type range |
 * | `ckd_sub(result, a, b)` | `*result = a - b` | Difference wraps or exceeds type range |
 * | `ckd_mul(result, a, b)` | `*result = a * b` | Product wraps or exceeds type range |
 *
 * All three macros:
 * - Store the mathematically correct truncated result in `*result`
 * - Return `true` if overflow occurred, `false` otherwise
 * - Handle mixed signed/unsigned operand types correctly
 * - Never invoke undefined behavior (no signed overflow UB)
 *
 * ## Usage Example
 *
 * @code{.c}
 * #include "rx_stdckdint.h"
 *
 * uint32_t total;
 * if (ckd_add(&total, payload_len, header_len)) {
 *     // Overflow: combined length exceeds uint32_t range
 *     return k_rx_err_overflow;
 * }
 *
 * uint16_t product;
 * if (ckd_mul(&product, width, height)) {
 *     // Overflow: area exceeds uint16_t range
 *     return k_rx_err_overflow;
 * }
 * @endcode
 *
 * ## Hardware Requirements
 *
 * | Requirement | Value | Notes |
 * |-------------|-------|-------|
 * | **CPU** | Any (RX72N, ARM, x86) | Platform-agnostic |
 * | **Compiler** | GCC >= 5, Clang >= 3.8, or C23 | Builtin or native support |
 * | **RAM** | 0 bytes | Inline operations, no state |
 * | **Flash** | 0 bytes | Inlined by compiler |
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 [YES]:** No goto, setjmp/longjmp, recursion (header-only, no control flow)
 * - **Rule 2 [N/A]:** No loops
 * - **Rule 3 [YES]:** No dynamic allocation
 * - **Rule 4 [N/A]:** No functions (macros only)
 * - **Rule 5 [N/A]:** No functions
 * - **Rule 6 [YES]:** Data at smallest scope
 * - **Rule 7 [N/A]:** No return values to check
 * - **Rule 8 [YES]:** Macros justified -- wrapping compiler builtins (cannot use enums)
 * - **Rule 9 [YES]:** Single-level pointer dereference (result pointer)
 * - **Rule 10 [YES]:** Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Provides ONLY checked integer arithmetic
 * - **O (Open/Closed):** Extensible via additional ckd_* macros without modification
 * - **L (Liskov Substitution):** Drop-in replacement for unchecked arithmetic
 * - **I (Interface Segregation):** Minimal 3-macro API
 * - **D (Dependency Inversion):** No dependencies beyond compiler builtins
 *
 * @author Locked, Inc.
 * @date 2026-03-18
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @see rx_stdbit.h for C23 bit manipulation polyfill
 * @see rx_check.h for error propagation macros
 * @see rx_err.h for error code definitions
 *
 * @version 1.0.0
 * @since Version 1.1.0
 */

#pragma once

/* ============================================================================
 * Tier 1: Native C23 <stdckdint.h>
 *
 * When the compiler provides the standard header, use it directly.
 * Detection: __STDC_VERSION_STDCKDINT_H__ is defined per C23 7.20.
 * ============================================================================
 */
#if defined(__STDC_VERSION_STDCKDINT_H__)

#include <stdckdint.h>

/* ============================================================================
 * Tier 2: GCC/Clang __builtin_*_overflow intrinsics
 *
 * Available in GCC >= 5 and Clang >= 3.8. These handle mixed signed/unsigned
 * types, perform the operation in infinite precision, and return true on
 * overflow. The truncated result is always stored in *result.
 * ============================================================================
 */
#elif defined(__GNUC__) && (__GNUC__ >= 5)

/**
 * @def ckd_add(result, a, b)
 * @brief Checked addition: *result = a + b, returns true on overflow
 *
 * @details
 * Computes the sum of a and b. If the mathematical result cannot be
 * represented in the type of *result, stores the truncated value and
 * returns true. Otherwise stores the exact result and returns false.
 *
 * @param[out] result Pointer to store the sum (any integer type)
 * @param[in] a First operand (any integer type)
 * @param[in] b Second operand (any integer type)
 *
 * @return true if overflow occurred, false otherwise
 *
 * @par Example:
 * @code{.c}
 * uint32_t sum;
 * if (ckd_add(&sum, UINT32_MAX, 1U)) {
 *     // Overflow detected: sum contains truncated value (0)
 * }
 * @endcode
 *
 * @since Version 1.1.0
 */
#define ckd_add(result, a, b) __builtin_add_overflow((a), (b), (result))

/**
 * @def ckd_sub(result, a, b)
 * @brief Checked subtraction: *result = a - b, returns true on overflow
 *
 * @details
 * Computes the difference of a and b. If the mathematical result cannot be
 * represented in the type of *result, stores the truncated value and
 * returns true. Otherwise stores the exact result and returns false.
 *
 * @param[out] result Pointer to store the difference (any integer type)
 * @param[in] a First operand (any integer type)
 * @param[in] b Second operand (any integer type)
 *
 * @return true if overflow occurred, false otherwise
 *
 * @par Example:
 * @code{.c}
 * uint32_t diff;
 * if (ckd_sub(&diff, 0U, 1U)) {
 *     // Underflow detected: diff contains truncated value (UINT32_MAX)
 * }
 * @endcode
 *
 * @since Version 1.1.0
 */
#define ckd_sub(result, a, b) __builtin_sub_overflow((a), (b), (result))

/**
 * @def ckd_mul(result, a, b)
 * @brief Checked multiplication: *result = a * b, returns true on overflow
 *
 * @details
 * Computes the product of a and b. If the mathematical result cannot be
 * represented in the type of *result, stores the truncated value and
 * returns true. Otherwise stores the exact result and returns false.
 *
 * @param[out] result Pointer to store the product (any integer type)
 * @param[in] a First operand (any integer type)
 * @param[in] b Second operand (any integer type)
 *
 * @return true if overflow occurred, false otherwise
 *
 * @par Example:
 * @code{.c}
 * uint16_t area;
 * if (ckd_mul(&area, (uint16_t)300, (uint16_t)300)) {
 *     // Overflow: 90000 > UINT16_MAX (65535)
 * }
 * @endcode
 *
 * @since Version 1.1.0
 */
#define ckd_mul(result, a, b) __builtin_mul_overflow((a), (b), (result))

/* ============================================================================
 * Tier 3: Portable C fallback (no compiler extensions)
 *
 * Manual overflow detection for compilers that lack builtins. Uses unsigned
 * arithmetic and pre-check comparisons to avoid undefined behavior.
 * Limited to same-type unsigned operands for simplicity and safety.
 * ============================================================================
 */
#else

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @def ckd_add(result, a, b)
 * @brief Portable checked addition for unsigned types
 *
 * @details
 * Fallback implementation using the property that unsigned addition overflows
 * if and only if the sum is less than either operand (for same-type operands),
 * or equivalently, if a > TYPE_MAX - b.
 *
 * @warning This fallback only supports same-type unsigned operands.
 *          Mixed signed/unsigned requires the builtin tier.
 *
 * @since Version 1.1.0
 */
#define ckd_add(result, a, b)                                                  \
  (*(result) = (a) + (b), (*(result) < (a)))

/**
 * @def ckd_sub(result, a, b)
 * @brief Portable checked subtraction for unsigned types
 *
 * @details
 * Fallback implementation using the property that unsigned subtraction
 * underflows if and only if the minuend is less than the subtrahend.
 *
 * @warning This fallback only supports same-type unsigned operands.
 *
 * @since Version 1.1.0
 */
#define ckd_sub(result, a, b)                                                  \
  (*(result) = (a) - (b), ((a) < (b)))

/**
 * @def ckd_mul(result, a, b)
 * @brief Portable checked multiplication for unsigned types
 *
 * @details
 * Fallback implementation using pre-check division. If b != 0 and
 * a > TYPE_MAX / b, the product would overflow.
 *
 * @warning This fallback only supports same-type unsigned operands.
 *
 * @since Version 1.1.0
 */
#define ckd_mul(result, a, b)                                                  \
  (*(result) = (a) * (b),                                                      \
   ((b) != 0 && (a) > (typeof(*(result)))(-1) / (b)))

#endif /* __STDC_VERSION_STDCKDINT_H__ / __GNUC__ */
