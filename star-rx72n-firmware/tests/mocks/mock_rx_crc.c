/* tests/mocks/mock_rx_crc.c */

/**
 * @file mock_rx_crc.c
 * @brief Mock CRC Implementation for Host-Side Testing
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_crc.h"

#include <stdbool.h>
#include <stddef.h>

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief CRC-8 Maxim constants
 */
typedef enum {
  k_crc8_maxim_poly  = 0x8C, /**< Reversed polynomial 0x31 */
  k_bits_per_byte    = 8,
  k_crc8_lsb_mask    = 0x01,
} crc8_constants_t;

/* =============================================================================
 * Mock State
 * =============================================================================
 */

static bool    s_override_enabled = false;
static uint8_t s_mock_crc_value   = 0;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_crc8_set_return_value(uint8_t crc)
{
  s_mock_crc_value = crc;
}

void mock_crc8_set_override(bool enable)
{
  s_override_enabled = enable;
}

/* =============================================================================
 * CRC-8 Implementation
 * =============================================================================
 */

uint8_t rx_crc8_maxim(const uint8_t* data, uint32_t length)
{
  if (s_override_enabled) {
    return s_mock_crc_value;
  }

  if (data == NULL || length == 0) {
    return 0;
  }

  uint8_t crc = 0;

  for (uint32_t i = 0; i < length; ++i) {
    crc ^= data[i];

    for (uint8_t j = 0; j < k_bits_per_byte; ++j) {
      if ((crc & k_crc8_lsb_mask) != 0) {
        crc = (uint8_t)((crc >> 1) ^ k_crc8_maxim_poly);
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}
