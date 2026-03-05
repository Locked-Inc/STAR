/**
 * @file mock_rx_crc.c
 * @brief Mock CRC Implementation for Host-Side Testing
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_crc.h"

#include <stdbool.h>
#include <stddef.h>

#include "rx_crc.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief CRC-8 Maxim constants
 */
typedef enum : uint8_t {
  k_crc8_maxim_poly = 0x8C, /**< Reversed polynomial 0x31 */
  k_bits_per_byte   = 8,    /**< Number of bits in a byte */
  k_crc8_lsb_mask   = 0x01, /**< Mask for LSB */
  k_shift_one_bit   = 1,    /**< Shift by one bit position */
} crc8_constants_t;

/* =============================================================================
 * Mock State
 * =============================================================================
 */

static bool     s_override_enabled = false;
static uint32_t s_mock_crc_value   = 0U;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_crc8_set_return_value(uint8_t crc)
{
  s_mock_crc_value = (uint32_t)crc;
}

void mock_crc8_set_override(bool enable)
{
  s_override_enabled = enable;
}

/* =============================================================================
 * CRC-8 Implementation
 * =============================================================================
 */

rx_err_t rx_crc_compute(const rx_crc_config_t* config,
                        const uint8_t*         data,
                        uint32_t               len,
                        uint32_t*              result_out)
{
  if (config == nullptr || data == nullptr || result_out == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (len == 0U) {
    return k_rx_err_invalid_arg;
  }

  if (s_override_enabled) {
    *result_out = s_mock_crc_value;
    return k_rx_ok;
  }

  /* Real CRC-8/Maxim computation (only polynomial used in this mock context) */
  uint8_t crc = 0;

  for (uint32_t i = 0U; i < len; ++i) {
    crc ^= data[i];

    for (uint8_t j = 0; j < k_bits_per_byte; ++j) {
      if ((crc & k_crc8_lsb_mask) != 0U) {
        crc = (uint8_t)((crc >> k_shift_one_bit) ^ k_crc8_maxim_poly);
      } else {
        crc >>= k_shift_one_bit;
      }
    }
  }

  *result_out = (uint32_t)crc;
  return k_rx_ok;
}

rx_err_t rx_crc_init(void)
{
  return k_rx_ok;
}

rx_err_t rx_crc_deinit(void)
{
  return k_rx_ok;
}

uint32_t rx_crc32_update(uint32_t crc, const uint8_t* data, uint32_t len)
{
  (void)crc;
  (void)data;
  (void)len;
  return 0U;
}
