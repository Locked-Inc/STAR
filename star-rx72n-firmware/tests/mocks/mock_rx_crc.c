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
  k_crc8_init_value = 0x00U, /**< Initial CRC-8/Maxim accumulator value */
} crc8_constants_t;

/**
 * @brief CRC-32/IEEE 802.3 constants
 */
typedef enum : uint32_t {
  k_crc32_poly     = 0xEDB88320U, /**< Reflected CRC-32/IEEE polynomial (0x04C11DB7 reflected) */
  k_crc32_xor_mask = 0xFFFFFFFFU, /**< Initial and final XOR mask (IEEE 802.3) */
  k_crc32_lsb_mask = 0x00000001U, /**< Mask for LSB check in bitwise accumulator loop */
  k_crc_len_max =
    65535U, /**< Maximum CRC data length (matches k_crc_len_max in rx_crc_internal.h) */
} crc32_constants_t;

/* =============================================================================
 * Mock State
 * =============================================================================
 */

static bool     s_override_enabled = false;
static uint32_t s_mock_crc_value   = 0U;
static bool     s_is_initialized   = false;

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
 * CRC Implementation (CRC-8/Maxim only; other polys return k_rx_err_invalid_arg)
 * =============================================================================
 */

/**
 * @brief Mock CRC computation -- supports CRC-8/Maxim only; all other polys error
 *
 * @details
 * Validates all pointer and length arguments, then either returns a preset value
 * (when override is enabled via mock_crc8_set_override()) or runs the real
 * CRC-8/Maxim bitwise algorithm so test assertions can verify actual CRC math.
 * Any polynomial other than k_rx_crc_poly_crc8 returns k_rx_err_invalid_arg.
 *
 * @param[in]  config     CRC configuration; must not be NULL; only poly field checked
 * @param[in]  data       Source bytes; must not be NULL
 * @param[in]  len        Byte count [1, k_crc_len_max]
 * @param[out] result_out Computed or preset CRC value; valid only on k_rx_ok
 *
 * @return rx_err_t
 * @retval k_rx_ok              CRC computed; *result_out is valid
 * @retval k_rx_err_null_ptr    config, data, or result_out is NULL
 * @retval k_rx_err_invalid_arg len == 0, len > k_crc_len_max, or poly != k_rx_crc_poly_crc8
 *
 * @pre config, data, result_out must not be NULL
 * @pre len must be in [1, k_crc_len_max]
 * @post *result_out set to preset value (override) or computed CRC-8/Maxim
 * @post s_mock_crc_value and s_override_enabled are read-only; not modified
 *
 * @note Not thread-safe; call only from the test thread
 * @since Version 1.0.0
 */
rx_err_t rx_crc_compute(const rx_crc_config_t* config,
                        const uint8_t*         data,
                        uint32_t               len,
                        uint32_t*              result_out)
{
  if (config == nullptr || data == nullptr || result_out == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (len == 0U || len > (uint32_t)k_crc_len_max || config->poly != k_rx_crc_poly_crc8) {
    return k_rx_err_invalid_arg;
  }

  if (s_override_enabled) {
    *result_out = s_mock_crc_value;
    return k_rx_ok;
  }

  /* CRC-8/Maxim computation - only supported polynomial for this mock */
  uint8_t crc = (uint8_t)k_crc8_init_value;

  for (uint32_t i = 0U; i < k_crc_len_max; ++i) {
    if (i >= len) {
      break;
    }
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

/**
 * @brief Mock CRC init -- tracks initialization state; rejects double-init
 *
 * @details
 * Sets s_is_initialized to true. Returns k_rx_err_invalid_state if already
 * initialized, mirroring the real rx_crc_init() behavior so tests that check
 * double-init error handling work against the mock without linking hardware code.
 *
 * @return rx_err_t
 * @retval k_rx_ok              Module marked as initialized
 * @retval k_rx_err_invalid_state Already initialized (call rx_crc_deinit() first)
 *
 * @pre s_is_initialized must be false
 * @pre No CRC computations should be in progress
 * @post s_is_initialized = true
 * @post Mock is now ready to accept rx_crc_compute() calls
 *
 * @note Not thread-safe; call only from the test thread
 * @since Version 1.0.0
 */
rx_err_t rx_crc_init(void)
{
  if (s_is_initialized) {
    return k_rx_err_invalid_state;
  }
  s_is_initialized = true;
  return k_rx_ok;
}

/**
 * @brief Mock CRC deinit -- clears initialization state; rejects deinit-without-init
 *
 * @details
 * Clears s_is_initialized to false. Returns k_rx_err_invalid_state if not
 * currently initialized, mirroring the real rx_crc_deinit() behavior.
 * Does NOT clear mock override state (s_override_enabled, s_mock_crc_value)
 * so test presets survive a deinit/reinit cycle.
 *
 * @return rx_err_t
 * @retval k_rx_ok              Module marked as deinitialized
 * @retval k_rx_err_invalid_state Not initialized (call rx_crc_init() first)
 *
 * @pre s_is_initialized must be true
 * @pre No CRC computations should be in progress
 * @post s_is_initialized = false
 * @post Mock override state (s_override_enabled, s_mock_crc_value) unchanged
 *
 * @note Not thread-safe; call only from the test thread
 * @since Version 1.0.0
 */
rx_err_t rx_crc_deinit(void)
{
  if (!s_is_initialized) {
    return k_rx_err_invalid_state;
  }
  s_is_initialized = false;
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
  if (data == nullptr || len == 0U || len > k_crc_len_max) {
    return crc;
  }

  /* Un-finalize previous CRC, process bytes, re-finalize (matches rx_crc_sw.c) */
  uint32_t work = crc ^ (uint32_t)k_crc32_xor_mask;

  for (uint32_t i = 0U; i < k_crc_len_max; ++i) {
    if (i >= len) {
      break;
    }
    work ^= (uint32_t)data[i];
    for (uint8_t b = 0U; b < k_bits_per_byte; ++b) {
      if ((work & (uint32_t)k_crc32_lsb_mask) != 0U) {
        work = (work >> k_shift_one_bit) ^ (uint32_t)k_crc32_poly;
      } else {
        work >>= k_shift_one_bit;
      }
    }
  }

  return work ^ (uint32_t)k_crc32_xor_mask;
}
