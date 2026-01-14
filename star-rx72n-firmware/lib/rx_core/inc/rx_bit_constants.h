/* lib/rx_core/inc/rx_bit_constants.h */

/**
 * @file rx_bit_constants.h
 * @brief Bit and Byte Size Constants
 *
 * Centralized constants for bit manipulation, data encoding, and protocol
 * operations. These are fundamental constants used across the protocol stack.
 *
 * Note: Hardware-specific constants (e.g., register bit positions, dividers)
 * should NOT use these constants - they belong in peripheral-specific headers.
 *
 * @date 2026-01-07
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_BIT_CONSTANTS_H
#define STAR_RX_BIT_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Bit and Byte Size Constants
 * =============================================================================
 */

/**
 * @brief Fundamental bit and byte size constants
 *
 * Used for protocol encoding, data serialization, FEC, HARQ, and other
 * data manipulation operations.
 *
 * Example usage:
 * @code
 * // Calculate bytes needed for N bits
 * uint32_t byte_count = (bit_count + k_rx_bits_per_byte - 1) / k_rx_bits_per_byte;
 *
 * // Extract byte from 32-bit word
 * uint8_t byte = (word >> (k_rx_bits_per_byte * byte_index)) & 0xFF;
 * @endcode
 */
typedef enum {
  k_rx_bits_per_byte   = 8,  /**< Bits per byte (universal) */
  k_rx_bits_per_word16 = 16, /**< Bits per 16-bit word */
  k_rx_bits_per_word32 = 32, /**< Bits per 32-bit word */
  k_rx_bits_per_word64 = 64, /**< Bits per 64-bit word */
} rx_bit_sizes_t;

/**
 * @brief Bit manipulation constants
 *
 * Common bit shift and mask values for protocol operations.
 */
typedef enum {
  k_rx_shift_byte   = 8,  /**< Shift for byte extraction (8 bits) */
  k_rx_shift_word16 = 16, /**< Shift for 16-bit word extraction */
  k_rx_shift_word24 = 24, /**< Shift for accessing byte 3 in 32-bit word */
} rx_bit_shifts_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_BIT_CONSTANTS_H */
