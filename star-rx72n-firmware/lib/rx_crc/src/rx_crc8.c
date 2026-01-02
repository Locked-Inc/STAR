/* lib/rx_crc/src/rx_crc8.c */

/**
 * @file rx_crc8.c
 * @brief Dallas/Maxim CRC-8 helper for OneWire peripherals.
 *
 * Polynomial: x^8 + x^5 + x^4 + 1 (0x31), reflected form 0x8C with LSB-first bit
 * processing per Maxim's application notes.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stddef.h>

#include "rx_crc.h"

/**
 * @brief Constants for the Maxim CRC-8 algorithm (LSB-first).
 */
typedef enum {
  k_crc8_initial_value = 0x00,
  k_crc8_bit_mask      = 0x01,
  k_crc8_polynomial    = 0x8C, /**< Reflected form of x^8 + x^5 + x^4 + 1 */
  k_crc8_bits_per_byte = 8,
} rx_crc8_constants_t;

/**
 * @brief Dallas/Maxim CRC-8 implementation.
 *
 * Deliberately bitwise (no lookup table) because OneWire payloads are tiny
 * (ROM codes, scratchpads). A table would cost 256 bytes of flash without
 * meaningful speedup for these short buffers.
 *
 * Matches Maxim application notes AN27/AN187 used for ROM code and scratchpad
 * verification on DS18B20/DS2431/etc. Returns 0 when data is NULL or len is
 * zero to mirror the CRC-32 helpers' behavior.
 *
 * @param[in] data Input buffer
 * @param[in] len  Number of bytes in buffer
 * @return CRC-8 value
 */
uint8_t rx_crc8_maxim(const uint8_t* data, uint32_t len)
{
  uint8_t crc = k_crc8_initial_value;

  if ((data == NULL) || (len == 0)) {
    return 0;
  }

  for (uint32_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < k_crc8_bits_per_byte; ++bit) {
      if ((crc & k_crc8_bit_mask) != 0U) {
        crc = (uint8_t)((crc >> 1U) ^ k_crc8_polynomial);
      } else {
        crc >>= 1U;
      }
    }
  }

  return crc;
}
