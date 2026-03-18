/**
 * @file rx_stdbit.h
 * @brief C23 Bit Manipulation Polyfill for RX72N Firmware
 *
 * @details
 * Provides the C23 `<stdbit.h>` bit utility interface with automatic fallback
 * to compiler builtins when the native header is unavailable. Offers portable,
 * safe bit manipulation replacing ad-hoc bitwise tricks and compiler-specific
 * intrinsics.
 *
 * ## Header Selection Strategy
 *
 * The header uses a two-tier fallback:
 * 1. **Native C23 `<stdbit.h>`** - Used when `__STDC_VERSION_STDBIT_H__` is
 *    defined (glibc 2.39+, gcc-14+, or glibc 2.38 with gcc-13)
 * 2. **GCC/Clang builtins** - `__builtin_clz()`, `__builtin_ctz()`,
 *    `__builtin_popcount()` family (covers GNURX and host compilers)
 *
 * ## Supported Operations
 *
 * | Function | Operation | Description |
 * |----------|-----------|-------------|
 * | `stdc_leading_zeros_ui(x)` | CLZ | Count leading zero bits |
 * | `stdc_trailing_zeros_ui(x)` | CTZ | Count trailing zero bits |
 * | `stdc_count_ones_ui(x)` | POPCOUNT | Count set bits |
 * | `stdc_has_single_bit_ui(x)` | Power-of-2 | Check if exactly one bit set |
 * | `stdc_bit_width_ui(x)` | Bit width | Minimum bits to represent value |
 * | `stdc_bit_ceil_ui(x)` | Round up | Smallest power of 2 >= x |
 *
 * All functions are provided for `unsigned int` (`_ui`) operands. Additional
 * width-specific variants (`_uc`, `_us`, `_ul`, `_ull`) are available when
 * using the native `<stdbit.h>`.
 *
 * ## Usage Example
 *
 * @code{.c}
 * #include "rx_stdbit.h"
 *
 * // Count leading zeros in a 32-bit CRC register
 * unsigned int lz = stdc_leading_zeros_ui(crc_value);
 *
 * // Check if buffer size is a power of 2
 * if (stdc_has_single_bit_ui(buf_size)) {
 *     // Can use mask instead of modulo
 *     index &= buf_size - 1;
 * }
 *
 * // Find minimum bits needed to represent an error code
 * unsigned int width = stdc_bit_width_ui(error_code);
 * @endcode
 *
 * ## Hardware Requirements
 *
 * | Requirement | Value | Notes |
 * |-------------|-------|-------|
 * | **CPU** | Any (RX72N, ARM, x86) | Platform-agnostic |
 * | **Compiler** | GCC >= 3.4, Clang >= 3.0, or C23 | Builtin or native support |
 * | **RAM** | 0 bytes | Inline operations, no state |
 * | **Flash** | 0 bytes | Inlined by compiler |
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 [YES]:** No goto, setjmp/longjmp, recursion
 * - **Rule 2 [N/A]:** No loops
 * - **Rule 3 [YES]:** No dynamic allocation
 * - **Rule 4 [YES]:** Functions are short (1-3 lines each)
 * - **Rule 5 [N/A]:** Utility functions with well-defined inputs
 * - **Rule 6 [YES]:** Data at smallest scope
 * - **Rule 7 [N/A]:** Return values are the purpose of these functions
 * - **Rule 8 [YES]:** Minimal macro use (type-generic dispatch only)
 * - **Rule 9 [YES]:** No pointers
 * - **Rule 10 [YES]:** Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Provides ONLY bit manipulation utilities
 * - **O (Open/Closed):** Extensible via additional stdc_* functions
 * - **L (Liskov Substitution):** Drop-in replacement for compiler builtins
 * - **I (Interface Segregation):** Minimal focused API
 * - **D (Dependency Inversion):** No dependencies beyond compiler builtins
 *
 * @author Locked, Inc.
 * @date 2026-03-18
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @see rx_stdckdint.h for C23 checked integer arithmetic polyfill
 * @see rx_bit_constants.h for bit/byte size constants
 *
 * @version 1.0.0
 * @since Version 1.1.0
 */

#pragma once

/* ============================================================================
 * Tier 1: Native C23 <stdbit.h>
 *
 * When the compiler + libc provides the standard header, use it directly.
 * Detection: __STDC_VERSION_STDBIT_H__ is defined per C23 7.18.
 * This covers glibc 2.39+ with gcc-14+, or glibc 2.38+ with gcc-13.
 * ============================================================================
 */
#if defined(__STDC_VERSION_STDBIT_H__) || __has_include(<stdbit.h>)

#include <stdbit.h>

/* Only define polyfills if the native header did NOT define the macro. */
#if defined(__STDC_VERSION_STDBIT_H__)
/* Native header fully available -- nothing more to do. */
#else
/* stdbit.h exists but may be incomplete; fall through to Tier 2. */
#undef _RX_STDBIT_USE_POLYFILL
#define _RX_STDBIT_USE_POLYFILL 1
#endif

#else
#define _RX_STDBIT_USE_POLYFILL 1
#endif /* __has_include(<stdbit.h>) */

/* ============================================================================
 * Tier 2: GCC/Clang builtin polyfill
 *
 * Provides the most commonly used stdbit.h functions using compiler builtins.
 * Available in GCC >= 3.4 and Clang >= 3.0.
 *
 * These inline functions target `unsigned int` (32-bit on both RX72N and x86).
 * The _ui suffix matches the C23 naming convention for unsigned int variants.
 * ============================================================================
 */
#if defined(_RX_STDBIT_USE_POLYFILL) && (defined(__GNUC__) || defined(__clang__))

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Count leading zero bits in an unsigned int
 *
 * @details
 * Returns the number of consecutive zero bits starting from the most
 * significant bit. Returns `sizeof(unsigned int) * CHAR_BIT` for input 0.
 *
 * @param[in] value Unsigned integer to examine
 *
 * @return Number of leading zero bits (0 to 32 on 32-bit systems)
 *
 * @par Example:
 * @code{.c}
 * stdc_leading_zeros_ui(0x00FF0000U); // Returns 8
 * stdc_leading_zeros_ui(0U);          // Returns 32
 * stdc_leading_zeros_ui(1U);          // Returns 31
 * @endcode
 *
 * @since Version 1.1.0
 */
static inline unsigned int stdc_leading_zeros_ui(unsigned int value)
{
  if (value == 0U) {
    return (unsigned int)(sizeof(unsigned int) * CHAR_BIT);
  }
  return (unsigned int)__builtin_clz(value);
}

/**
 * @brief Count trailing zero bits in an unsigned int
 *
 * @details
 * Returns the number of consecutive zero bits starting from the least
 * significant bit. Returns `sizeof(unsigned int) * CHAR_BIT` for input 0.
 *
 * @param[in] value Unsigned integer to examine
 *
 * @return Number of trailing zero bits (0 to 32 on 32-bit systems)
 *
 * @par Example:
 * @code{.c}
 * stdc_trailing_zeros_ui(0x00000100U); // Returns 8
 * stdc_trailing_zeros_ui(0U);           // Returns 32
 * stdc_trailing_zeros_ui(1U);           // Returns 0
 * @endcode
 *
 * @since Version 1.1.0
 */
static inline unsigned int stdc_trailing_zeros_ui(unsigned int value)
{
  if (value == 0U) {
    return (unsigned int)(sizeof(unsigned int) * CHAR_BIT);
  }
  return (unsigned int)__builtin_ctz(value);
}

/**
 * @brief Count the number of set (1) bits in an unsigned int
 *
 * @details
 * Returns the population count (Hamming weight) of the value.
 *
 * @param[in] value Unsigned integer to examine
 *
 * @return Number of set bits (0 to 32 on 32-bit systems)
 *
 * @par Example:
 * @code{.c}
 * stdc_count_ones_ui(0x0FU); // Returns 4
 * stdc_count_ones_ui(0U);    // Returns 0
 * stdc_count_ones_ui(~0U);   // Returns 32
 * @endcode
 *
 * @since Version 1.1.0
 */
static inline unsigned int stdc_count_ones_ui(unsigned int value)
{
  return (unsigned int)__builtin_popcount(value);
}

/**
 * @brief Check if exactly one bit is set (power of 2 test)
 *
 * @details
 * Returns true if the value is a power of two (exactly one bit set).
 * Returns false for 0 (zero bits set).
 *
 * @param[in] value Unsigned integer to test
 *
 * @return true if value is a power of 2, false otherwise
 *
 * @par Example:
 * @code{.c}
 * stdc_has_single_bit_ui(256U); // Returns true (2^8)
 * stdc_has_single_bit_ui(255U); // Returns false
 * stdc_has_single_bit_ui(0U);   // Returns false
 * @endcode
 *
 * @since Version 1.1.0
 */
static inline bool stdc_has_single_bit_ui(unsigned int value)
{
  return (value != 0U) && ((value & (value - 1U)) == 0U);
}

/**
 * @brief Compute the minimum number of bits needed to represent a value
 *
 * @details
 * Returns floor(log2(value)) + 1 for nonzero values, or 0 for input 0.
 * Equivalent to `sizeof(unsigned int) * CHAR_BIT - stdc_leading_zeros_ui(value)`.
 *
 * @param[in] value Unsigned integer to measure
 *
 * @return Bit width (0 to 32 on 32-bit systems)
 *
 * @par Example:
 * @code{.c}
 * stdc_bit_width_ui(255U); // Returns 8
 * stdc_bit_width_ui(256U); // Returns 9
 * stdc_bit_width_ui(0U);   // Returns 0
 * stdc_bit_width_ui(1U);   // Returns 1
 * @endcode
 *
 * @since Version 1.1.0
 */
static inline unsigned int stdc_bit_width_ui(unsigned int value)
{
  return (unsigned int)(sizeof(unsigned int) * CHAR_BIT)
         - stdc_leading_zeros_ui(value);
}

/**
 * @brief Compute the smallest power of 2 greater than or equal to a value
 *
 * @details
 * Returns the smallest power of two that is >= value. Returns 1 for input 0
 * or 1. Behavior is undefined if the result cannot be represented (i.e.,
 * value > 2^(N-1) where N is the bit width of unsigned int).
 *
 * @param[in] value Unsigned integer
 *
 * @return Smallest power of 2 >= value
 *
 * @par Example:
 * @code{.c}
 * stdc_bit_ceil_ui(200U); // Returns 256
 * stdc_bit_ceil_ui(256U); // Returns 256
 * stdc_bit_ceil_ui(1U);   // Returns 1
 * stdc_bit_ceil_ui(0U);   // Returns 1
 * @endcode
 *
 * @since Version 1.1.0
 */
static inline unsigned int stdc_bit_ceil_ui(unsigned int value)
{
  if (value <= 1U) {
    return 1U;
  }
  unsigned int width = stdc_bit_width_ui(value - 1U);
  return 1U << width;
}

/**
 * @def stdc_leading_zeros(x)
 * @brief Type-generic macro for counting leading zeros
 *
 * @details
 * Routes to `stdc_leading_zeros_ui()` for `unsigned int` arguments.
 * When using the native `<stdbit.h>`, this macro handles all unsigned types.
 *
 * @since Version 1.1.0
 */
#define stdc_leading_zeros(x)  stdc_leading_zeros_ui((unsigned int)(x))

/**
 * @def stdc_trailing_zeros(x)
 * @brief Type-generic macro for counting trailing zeros
 * @since Version 1.1.0
 */
#define stdc_trailing_zeros(x) stdc_trailing_zeros_ui((unsigned int)(x))

/**
 * @def stdc_count_ones(x)
 * @brief Type-generic macro for population count
 * @since Version 1.1.0
 */
#define stdc_count_ones(x)     stdc_count_ones_ui((unsigned int)(x))

/**
 * @def stdc_has_single_bit(x)
 * @brief Type-generic macro for power-of-2 test
 * @since Version 1.1.0
 */
#define stdc_has_single_bit(x) stdc_has_single_bit_ui((unsigned int)(x))

/**
 * @def stdc_bit_width(x)
 * @brief Type-generic macro for bit width calculation
 * @since Version 1.1.0
 */
#define stdc_bit_width(x)      stdc_bit_width_ui((unsigned int)(x))

/**
 * @def stdc_bit_ceil(x)
 * @brief Type-generic macro for power-of-2 ceiling
 * @since Version 1.1.0
 */
#define stdc_bit_ceil(x)       stdc_bit_ceil_ui((unsigned int)(x))

#endif /* _RX_STDBIT_USE_POLYFILL && __GNUC__ */
