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
 * @brief Bit-manipulation constants shared by all CRC algorithms
 */
typedef enum : uint8_t {
  k_bits_per_byte = 8U, /**< Number of bits in a byte (inner loop bound) */
  k_shift_one_bit = 1U, /**< Right-shift by one bit position per iteration */
} crc_common_constants_t;

/**
 * @brief CRC-8/Maxim constants
 */
typedef enum : uint8_t {
  k_crc8_maxim_poly = 0x8CU, /**< Reversed polynomial 0x31 */
  k_crc8_lsb_mask   = 0x01U, /**< Mask for LSB */
} crc8_constants_t;

/**
 * @brief CRC-32/IEEE 802.3 constants
 */
typedef enum : uint32_t {
  k_crc32_poly     = 0xEDB88320U, /**< Reflected CRC-32/IEEE polynomial (0x04C11DB7 reflected) */
  k_crc32_xor_mask = 0xFFFFFFFFU, /**< Initial and final XOR mask (IEEE 802.3) */
  k_crc32_lsb_mask = 0x00000001U, /**< Mask for LSB check in bitwise accumulator loop */
} crc32_constants_t;

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

/**
 * @brief Incremental CRC-32/IEEE 802.3 accumulator (matches rx_crc_sw.c)
 *
 * @details
 * Un-finalizes the running CRC by XOR with k_crc32_xor_mask, processes each
 * byte with the reflected polynomial k_crc32_poly (0xEDB88320), then
 * re-finalizes with a second XOR. Passes the input crc through unchanged when
 * data is NULL or len is 0.
 *
 * This implementation is byte-identical to internal_crc32_sw_update() in
 * rx_crc_sw.c, allowing host-side tests to verify incremental CRC results
 * without linking the real hardware backend.
 *
 * @param[in] crc  Running CRC state; use 0U for the first chunk
 * @param[in] data Input data pointer; NULL causes pass-through (crc returned unchanged)
 * @param[in] len  Number of bytes to process; 0 causes pass-through
 *
 * @return uint32_t Updated finalized CRC-32 accumulator
 * @retval crc (unchanged) if data is NULL or len is 0
 * @retval finalized CRC-32/IEEE value covering all bytes processed so far
 *
 * @pre data must point to at least len valid bytes, or be NULL
 * @pre crc must be 0U for the first call, or the return value of a prior call
 * @post Returns finalized CRC-32; no internal state retained across calls
 * @post No side effects; pure computation, no mock state modified
 *
 * @note Not thread-safe; call only from the test thread
 *
 * @since Version 1.0.0
 */
uint32_t rx_crc32_update(uint32_t crc, const uint8_t* data, uint32_t len)
{
  if (data == nullptr || len == 0U) {
    return crc;
  }

  /* Un-finalize previous CRC, process bytes, re-finalize (matches rx_crc_sw.c) */
  uint32_t work = crc ^ (uint32_t)k_crc32_xor_mask;

  for (uint32_t i = 0U; i < len; i++) {
    work ^= (uint32_t)data[i];
    for (uint8_t b = 0U; b < k_bits_per_byte; b++) {
      if ((work & (uint32_t)k_crc32_lsb_mask) != 0U) {
        work = (work >> k_shift_one_bit) ^ (uint32_t)k_crc32_poly;
      } else {
        work >>= k_shift_one_bit;
      }
    }
  }

  return work ^ (uint32_t)k_crc32_xor_mask;
}
