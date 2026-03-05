/* star-rx72n-firmware/libs/rx_core/inc/rx_bit_constants.h */

/**
 * @file rx_bit_constants.h
 * @brief Fundamental Bit and Byte Size Constants for Protocol and Data Manipulation
 *
 * @details
 * ## Overview
 * Centralized repository of typed enum constants for bit/byte sizes and shift operations
 * used throughout the STAR firmware. Provides self-documenting, type-safe constants for:
 * - Protocol encoding/decoding (Protocol Buffers, ASCII frames, CRC)
 * - Data serialization and deserialization
 * - Bit manipulation (extraction, packing, masking)
 * - Endianness conversion (big-endian <-> little-endian)
 * - Buffer size calculations
 *
 * **Design Philosophy:**
 * - **No magic numbers**: All numeric literals replaced with named typed enums (NASA Rule 8)
 * - **Type safety**: C23 typed enums with explicit underlying types (uint8_t)
 * - **Self-documenting**: Constants have descriptive names (k_rx_bits_per_byte vs. literal 8)
 * - **Searchable**: grep for "k_rx_shift_byte" finds all byte-shift operations
 * - **Hardware-agnostic**: These are universal constants, not hardware-specific
 *
 * ## Scope and Usage
 *
 * **INCLUDE in:**
 * - Protocol encoding/decoding modules (rx_frame, rx_nanopb, rx_crc)
 * - Communication layers (rx_spi_comm, rx_usb_comm, rx_comm_manager)
 * - Utility functions for data packing/unpacking
 * - Unit conversion functions
 *
 * **EXCLUDE from (use peripheral-specific headers instead):**
 * - Hardware register bit positions -> Use rx72n_*_regs.h (e.g., USBE.SYSCFG.DRPD = bit 5)
 * - Clock divider calculations -> Use rx72n_clock.h (e.g., PCLKA_DIV = 2)
 * - Peripheral-specific shifts -> Define in peripheral header (e.g., ADC channel shift)
 *
 * ## Common Usage Patterns
 *
 * ### 1. Bit-to-Byte Conversion (Ceiling Division)
 * @code{.c}
 * // Calculate bytes needed to store N bits (round up)
 * uint32_t bit_count = 100;  // 100 bits
 * uint32_t byte_count = (bit_count + k_rx_bits_per_byte - 1) / k_rx_bits_per_byte;
 * // Result: (100 + 8 - 1) / 8 = 107 / 8 = 13 bytes (ceiling division)
 * @endcode
 *
 * ### 2. Byte Extraction from Multi-Byte Word
 * @code{.c}
 * // Extract individual bytes from 32-bit word (little-endian)
 * uint32_t word = 0x12345678;
 * uint8_t byte0 = (word >> (k_rx_shift_byte * 0)) & 0xFF;  // 0x78 (LSB)
 * uint8_t byte1 = (word >> (k_rx_shift_byte * 1)) & 0xFF;  // 0x56
 * uint8_t byte2 = (word >> (k_rx_shift_byte * 2)) & 0xFF;  // 0x34
 * uint8_t byte3 = (word >> k_rx_shift_word24) & 0xFF;      // 0x12 (MSB)
 * @endcode
 *
 * ### 3. Big-Endian 16-bit Packing
 * @code{.c}
 * // Pack uint16_t into big-endian byte array
 * uint16_t value = 0xABCD;
 * uint8_t buffer[2];
 * buffer[0] = (value >> k_rx_shift_byte) & 0xFF;  // 0xAB (high byte)
 * buffer[1] = (value >> 0) & 0xFF;                // 0xCD (low byte)
 * @endcode
 *
 * ### 4. Buffer Size Calculation for Bit Fields
 * @code{.c}
 * // Allocate buffer for protocol with bit-stuffing
 * const uint32_t k_payload_bits = 512;  // 512 bits of data
 * const uint32_t k_overhead_bits = 32;  // CRC-32
 * const uint32_t k_total_bits = k_payload_bits + k_overhead_bits;
 *
 * // Calculate bytes needed (ceiling division)
 * const uint32_t k_buffer_size =
 *     (k_total_bits + k_rx_bits_per_byte - 1) / k_rx_bits_per_byte;
 * uint8_t buffer[k_buffer_size];  // 68 bytes
 * @endcode
 *
 * ## Hardware Requirements
 *
 * | Requirement | Value | Notes |
 * |-------------|-------|-------|
 * | **CPU** | Any (RX, ARM, x86) | Universal constants, platform-agnostic |
 * | **Compiler** | C23 or later | Requires typed enum support `: uint8_t` |
 * | **Dependencies** | stdint.h | For uint8_t underlying type |
 * | **RAM** | 0 bytes | Compile-time constants (no storage) |
 * | **Flash** | 0 bytes | Inlined by compiler, no code generated |
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 [YES]:** No goto, setjmp/longjmp, recursion (header-only, no control flow)
 * - **Rule 2 [N/A]:** No loops (header-only)
 * - **Rule 3 [YES]:** No dynamic allocation (compile-time constants)
 * - **Rule 4 [N/A]:** No functions (header-only)
 * - **Rule 5 [N/A]:** No functions (header-only)
 * - **Rule 6 [YES]:** Data at smallest scope (file-scoped enums)
 * - **Rule 7 [N/A]:** No functions (header-only)
 * - **Rule 8 [YES]:** No macros except include guards (uses typed enums instead)
 * - **Rule 9 [YES]:** No pointers (value types only)
 * - **Rule 10 [YES]:** Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Module provides ONLY universal bit/byte constants
 * - **O (Open/Closed):** Extensible (add new enums) without modifying existing code
 * - **L (Liskov Substitution):** N/A (no functions, only data)
 * - **I (Interface Segregation):** Minimal API (2 enums), each with focused purpose
 * - **D (Dependency Inversion):** No dependencies (standalone header)
 *
 * @par Module Dependencies:
 * ```
 * rx_bit_constants.h
 *   +--> stdint.h (uint8_t for underlying enum type)
 *
 * Used by:
 *   +--> lib/rx_frame/ (protocol framing, byte packing)
 *   +--> lib/rx_crc/ (CRC-32 byte-wise operations)
 *   +--> lib/rx_nanopb/ (Protocol Buffer serialization)
 *   +--> lib/rx_comm_manager/ (communication buffer management)
 *   +--> lib/rx_hal/src/uart.c (data serialization)
 * ```
 *
 * @author STAR Team
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @see stdint.h for standard integer types
 * @see rx_gpio_constants.h for GPIO pin constants
 * @see rx_port_constants.h for port/pin mapping constants
 * @see rx_time_constants.h for time conversion constants
 *
 * @version 1.0.0
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Bit and Byte Size Constants
 * =============================================================================
 */

/**
 * @enum rx_bit_sizes_t
 * @brief Fundamental bit and byte size constants for data type sizes
 *
 * @details
 * Defines universal constants for bit counts in standard data types (byte, word16, word32, word64).
 * These constants eliminate magic numbers in protocol encoding, buffer calculations, and bit
 * manipulation operations.
 *
 * **Underlying Type:** uint8_t (8-bit unsigned) - sufficient for all values (max = 64)
 * **Guaranteed Sizes:** All values are compile-time constants (no runtime overhead)
 *
 * ## Usage Patterns
 *
 * ### Ceiling Division for Bit-to-Byte Conversion
 * @code{.c}
 * // Calculate bytes needed for N bits (always round up)
 * uint32_t bit_count = 100;  // Example: 100 bits
 * uint32_t byte_count = (bit_count + k_rx_bits_per_byte - 1) / k_rx_bits_per_byte;
 * // Result: (100 + 7) / 8 = 13 bytes (ceiling of 100/8 = 12.5)
 * @endcode
 *
 * ### Byte Indexing in Multi-Byte Words
 * @code{.c}
 * // Extract byte N from 32-bit word
 * uint32_t word = 0xDEADBEEF;
 * uint8_t byte_index = 2;  // Want byte 2 (0xBE)
 * uint8_t byte = (word >> (k_rx_bits_per_byte * byte_index)) & 0xFF;
 * // Result: (0xDEADBEEF >> 16) & 0xFF = 0xBE
 * @endcode
 *
 * ### Protocol Buffer Sizing
 * @code{.c}
 * // Allocate buffer for protocol with CRC-32
 * const uint32_t k_payload_bytes = 64;
 * const uint32_t k_crc_bits = k_rx_bits_per_word32;  // CRC-32 = 32 bits
 * const uint32_t k_crc_bytes = k_crc_bits / k_rx_bits_per_byte;  // 4 bytes
 * const uint32_t k_total_bytes = k_payload_bytes + k_crc_bytes;  // 68 bytes
 * uint8_t buffer[k_total_bytes];
 * @endcode
 *
 * ## Value Justification Table
 *
 * | Constant | Value | Rationale | Standard Reference |
 * |----------|-------|-----------|-------------------|
 * | k_rx_bits_per_byte | 8 | Universal (ANSI, ISO/IEC, POSIX) | C99 Sec.3.6: CHAR_BIT = 8 |
 * | k_rx_bits_per_word16 | 16 | uint16_t guaranteed size | C99 Sec.7.18.1.1 exact-width types |
 * | k_rx_bits_per_word32 | 32 | uint32_t guaranteed size | C99 Sec.7.18.1.1 exact-width types |
 * | k_rx_bits_per_word64 | 64 | uint64_t guaranteed size | C99 Sec.7.18.1.1 exact-width types |
 *
 * @par Historical Note:
 * While some legacy systems used 7-bit bytes (e.g., CDC 6600) or 9-bit bytes (PDP-10),
 * **all modern systems use 8-bit bytes** as mandated by POSIX.1-2008 and C99. The RX72N
 * CHAR_BIT is guaranteed to be 8.
 *
 * @par Usage Example: Endianness Conversion
 * @code{.c}
 * #include "rx_bit_constants.h"
 *
 * // Convert little-endian uint32_t to big-endian byte array
 * void uint32_to_big_endian(uint32_t value, uint8_t* buffer) {
 *   buffer[0] = (value >> (k_rx_bits_per_byte * 3)) & 0xFF;  // MSB
 *   buffer[1] = (value >> (k_rx_bits_per_byte * 2)) & 0xFF;
 *   buffer[2] = (value >> (k_rx_bits_per_byte * 1)) & 0xFF;
 *   buffer[3] = (value >> (k_rx_bits_per_byte * 0)) & 0xFF;  // LSB
 * }
 *
 * // Convert big-endian byte array to little-endian uint32_t
 * uint32_t big_endian_to_uint32(const uint8_t* buffer) {
 *   return ((uint32_t)buffer[0] << (k_rx_bits_per_byte * 3)) |  // MSB
 *          ((uint32_t)buffer[1] << (k_rx_bits_per_byte * 2)) |
 *          ((uint32_t)buffer[2] << (k_rx_bits_per_byte * 1)) |
 *          ((uint32_t)buffer[3] << (k_rx_bits_per_byte * 0));   // LSB
 * }
 * @endcode
 *
 * @see rx_bit_shifts_t for corresponding shift values
 * @see k_rx_shift_byte equivalent to (1 * k_rx_bits_per_byte)
 * @see k_rx_shift_word16 equivalent to (2 * k_rx_bits_per_byte)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rx_bits_per_byte =
    8, /**< Bits per byte. Universal constant (CHAR_BIT = 8 per C99). Used for byte extraction, buffer sizing, bit-to-byte conversion. Range: Always 8 on modern systems. */
  k_rx_bits_per_word16 =
    16, /**< Bits per 16-bit word (uint16_t). Guaranteed by C99 exact-width types. Used for protocol fields, endianness conversion, CRC-16 operations. Range: Always 16. */
  k_rx_bits_per_word32 =
    32, /**< Bits per 32-bit word (uint32_t). Guaranteed by C99 exact-width types. Used for CRC-32, hash values, register access, Protocol Buffer fixed32. Range: Always 32. */
  k_rx_bits_per_word64 =
    64, /**< Bits per 64-bit word (uint64_t). Guaranteed by C99 exact-width types. Used for timestamps, large counters, Protocol Buffer fixed64. Range: Always 64. */
} rx_bit_sizes_t;

/**
 * @enum rx_bit_shifts_t
 * @brief Bit shift constants for byte and word extraction from multi-byte values
 *
 * @details
 * Provides named constants for common bit-shift operations used in protocol encoding,
 * endianness conversion, and byte-wise data access. These constants make code self-
 * documenting and eliminate magic shift values.
 *
 * **Underlying Type:** uint8_t (8-bit unsigned) - sufficient for all values (max = 24)
 * **Relationship to rx_bit_sizes_t:**
 * - k_rx_shift_byte = k_rx_bits_per_byte (both = 8)
 * - k_rx_shift_word16 = k_rx_bits_per_word16 (both = 16)
 * - k_rx_shift_word24 = 3 x k_rx_bits_per_byte (3 bytes = 24 bits)
 *
 * ## Usage Patterns
 *
 * ### Byte Extraction from 32-bit Word (Little-Endian)
 * @code{.c}
 * uint32_t word = 0x12345678;  // Little-endian representation
 *
 * // Extract individual bytes
 * uint8_t byte0 = (word >> 0) & 0xFF;                 // 0x78 (LSB, byte 0)
 * uint8_t byte1 = (word >> k_rx_shift_byte) & 0xFF;   // 0x56 (byte 1)
 * uint8_t byte2 = (word >> k_rx_shift_word16) & 0xFF; // 0x34 (byte 2)
 * uint8_t byte3 = (word >> k_rx_shift_word24) & 0xFF; // 0x12 (MSB, byte 3)
 * @endcode
 *
 * ### Big-Endian 32-bit Packing
 * @code{.c}
 * // Pack uint32_t into big-endian byte array
 * uint32_t value = 0xAABBCCDD;
 * uint8_t buffer[4];
 *
 * buffer[0] = (value >> k_rx_shift_word24) & 0xFF;  // 0xAA (MSB first)
 * buffer[1] = (value >> k_rx_shift_word16) & 0xFF;  // 0xBB
 * buffer[2] = (value >> k_rx_shift_byte) & 0xFF;    // 0xCC
 * buffer[3] = (value >> 0) & 0xFF;                  // 0xDD (LSB last)
 * @endcode
 *
 * ### Big-Endian 16-bit Packing/Unpacking
 * @code{.c}
 * // Pack uint16_t -> big-endian buffer
 * uint16_t value = 0xABCD;
 * uint8_t buffer[2];
 * buffer[0] = (value >> k_rx_shift_byte) & 0xFF;  // 0xAB (high byte)
 * buffer[1] = (value >> 0) & 0xFF;                // 0xCD (low byte)
 *
 * // Unpack big-endian buffer -> uint16_t
 * uint16_t reconstructed = ((uint16_t)buffer[0] << k_rx_shift_byte) | buffer[1];
 * // Result: (0xAB << 8) | 0xCD = 0xABCD
 * @endcode
 *
 * ## Why Not Use rx_bit_sizes_t Directly?
 *
 * **Semantic Clarity:**
 * - `k_rx_shift_byte` **clearly indicates** "shift to access next byte"
 * - `k_rx_bits_per_byte` **ambiguously suggests** "number of bits in a byte"
 * - Using k_rx_shift_byte makes the **intent explicit** (shift operation)
 *
 * **Example:**
 * @code{.c}
 * // CLEAR INTENT: Using k_rx_shift_byte
 * uint8_t byte1 = (word >> k_rx_shift_byte) & 0xFF;  // [OK] Obvious: shift by 1 byte
 *
 * // AMBIGUOUS INTENT: Using k_rx_bits_per_byte
 * uint8_t byte1 = (word >> k_rx_bits_per_byte) & 0xFF;  // [X] Less clear: shifting? counting?
 * @endcode
 *
 * ## Value Justification Table
 *
 * | Constant | Value | Derivation | Use Case |
 * |----------|-------|------------|----------|
 * | k_rx_shift_byte | 8 | 1 byte x 8 bits | Access byte N in multi-byte word |
 * | k_rx_shift_word16 | 16 | 2 bytes x 8 bits | Access upper 16 bits of 32-bit word |
 * | k_rx_shift_word24 | 24 | 3 bytes x 8 bits | Access MSB (byte 3) of 32-bit word |
 *
 * @par Usage Example: CRC-32 Byte-Wise Processing
 * @code{.c}
 * #include "rx_bit_constants.h"
 *
 * // Extract bytes from CRC-32 for transmission (big-endian)
 * void crc32_to_bytes(uint32_t crc, uint8_t* buffer) {
 *   buffer[0] = (crc >> k_rx_shift_word24) & 0xFF;  // Byte 3 (MSB)
 *   buffer[1] = (crc >> k_rx_shift_word16) & 0xFF;  // Byte 2
 *   buffer[2] = (crc >> k_rx_shift_byte) & 0xFF;    // Byte 1
 *   buffer[3] = (crc >> 0) & 0xFF;                  // Byte 0 (LSB)
 * }
 *
 * // Reconstruct CRC-32 from big-endian byte array
 * uint32_t bytes_to_crc32(const uint8_t* buffer) {
 *   return ((uint32_t)buffer[0] << k_rx_shift_word24) |  // MSB
 *          ((uint32_t)buffer[1] << k_rx_shift_word16) |
 *          ((uint32_t)buffer[2] << k_rx_shift_byte) |
 *          ((uint32_t)buffer[3] << 0);                    // LSB
 * }
 * @endcode
 *
 * @par Usage Example: Protocol Field Extraction
 * @code{.c}
 * // Extract 16-bit field from Protocol Buffer varint
 * uint32_t varint = 0x12345678;
 * uint16_t field = (varint >> k_rx_shift_word16) & 0xFFFF;
 * // Result: 0x1234 (upper 16 bits)
 * @endcode
 *
 * @see rx_bit_sizes_t for related bit count constants
 * @see k_rx_bits_per_byte equivalent value to k_rx_shift_byte (both = 8)
 * @see k_rx_bits_per_word16 equivalent value to k_rx_shift_word16 (both = 16)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rx_shift_byte =
    8, /**< Shift amount to access next byte (8 bits = 1 byte). Use for extracting byte N from multi-byte word. Example: byte1 = (word >> k_rx_shift_byte) & 0xFF. Equivalent to k_rx_bits_per_byte but semantically clearer for shift operations. */
  k_rx_shift_word16 =
    16, /**< Shift amount to access upper 16 bits of 32-bit word (16 bits = 2 bytes). Use for extracting high word from dword. Example: high_word = (dword >> k_rx_shift_word16) & 0xFFFF. Also used for big-endian 16-bit packing/unpacking. */
  k_rx_shift_word24 =
    24, /**< Shift amount to access byte 3 (MSB) of 32-bit word (24 bits = 3 bytes). Use for extracting most significant byte. Example: msb = (word >> k_rx_shift_word24) & 0xFF. Critical for big-endian 32-bit encoding. */
} rx_bit_shifts_t;

#ifdef __cplusplus
}
#endif
