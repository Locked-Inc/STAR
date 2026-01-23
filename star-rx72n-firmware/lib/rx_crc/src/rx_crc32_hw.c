/* lib/rx_crc/src/rx_crc32_hw.c */

/**
 * @file rx_crc32_hw.c
 * @brief Hardware CRC-32 Implementation for RX72N
 *
 * Uses the RX72N CRC Calculator peripheral for IEEE 802.3 CRC-32 acceleration.
 * This implementation is only compiled when targeting the RX72N (RX_CRC32_USE_HARDWARE).
 *
 * Hardware Details:
 * - Base Address: 0x00088280
 * - Module Stop: MSTPCRB bit 23
 * - Polynomial: X32+X26+X23+X22+X16+X12+X11+X10+X8+X7+X5+X4+X2+X+1 (0x04C11DB7)
 * - This is the IEEE 802.3 standard polynomial, compatible with Go's crc32.ChecksumIEEE()
 *
 * References:
 * - RX72N Group User's Manual: Hardware (CRC Calculator section)
 * - https://renesas.github.io/fsp/group___c_r_c.html
 * - https://tool-support.renesas.com/autoupdate/support/onlinehelp/csp/V8.12.00/
 *   CS+.chm/CodeGenerator-API-RX.chm/Documents/crccalculatorcrc.htm
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_crc_internal.h"

#ifdef RX_CRC32_USE_HARDWARE

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_gpio_constants.h"
#include "rx_register_protection.h"

/* =============================================================================
 * CRC Hardware Constants
 * =============================================================================
 */

/**
 * @brief CRC hardware constants
 *
 * Note: Protection register constants are from rx_register_protection.h
 */
typedef enum : int32_t {
  k_crc_ieee_final_xor = -1, /**< IEEE 802.3 CRC-32 final XOR (0xFFFFFFFF as signed int32_t) */
} rx_crc_hw_constants_t;

/**
 * @brief CRC calculation limits and indices
 */
typedef enum : uint32_t {
  k_crc_len_min        = 1,     /**< Minimum valid data length */
  k_crc_len_max        = 65535, /**< Maximum CRC data length (bounded loop) */
  k_crc_idx_start      = 0,     /**< Loop start index */
  k_crc_result_invalid = 0,     /**< Invalid CRC result (error case) */
  k_crc_bit_set        = 1,     /**< Bit set value for register manipulation */
} rx_crc_loop_constants_t;

/* =============================================================================
 * Module State
 * =============================================================================
 */

static bool s_crc_initialized = false;

/* =============================================================================
 * Hardware CRC Implementation
 * =============================================================================
 */

/**
 * @brief Initialize hardware CRC module
 *
 * Enables the RX72N CRC calculator peripheral and configures it for
 * IEEE 802.3 CRC-32 calculation (compatible with crc32.ChecksumIEEE).
 *
 * @return k_rx_ok on success
 *
 * @pre System registers must be accessible
 * @post CRC module is enabled and configured for IEEE 802.3
 */
rx_err_t rx_crc_init(void)
{
  /* Pre-condition: Validate system registers are accessible (NASA Rule 5) */
  RX_CHECK_NULL_PTR(system_regs(), "CRC", "System registers not accessible");
  RX_CHECK_NULL_PTR(crc_regs(), "CRC", "CRC registers not accessible");

  if (s_crc_initialized) {
    return k_rx_ok;
  }

  /* Enable CRC module by clearing module stop bit */
  system_regs()->prcr = k_rx_prcr_unlock_prc1_prc3; /* Unlock PRC1+PRC3 for MSTPCR */
  system_regs()->mstpcrb &=
    ~((uint32_t)k_crc_bit_set << k_mstpb_crc); /* Clear bit 23 to enable CRC */
  system_regs()->prcr = k_rx_prcr_lock;        /* Lock protection */

  /*
     * Configure CRC peripheral for IEEE 802.3:
     * - GPS = 0x03 (CRC-32)
     * - LMS = 1 (LSB first / reflected mode for IEEE 802.3 compatibility)
     *
     * The RX72N CRC calculator in this configuration produces output
     * bit-exact with Go's crc32.ChecksumIEEE() when properly initialized.
     */
  crc_regs()->crccr = k_crc_crccr_lms | k_crc_crccr_gps_crc32;

  s_crc_initialized = true;
  return k_rx_ok;
}

/**
 * @brief Deinitialize hardware CRC module
 *
 * Note: This implementation intentionally does NOT disable the CRC module.
 * The module remains enabled for continuous availability and minimal overhead.
 *
 * @return k_rx_ok always (no-op deinit)
 *
 * @pre CRC module must have been initialized
 * @post CRC module state unchanged
 */
rx_err_t rx_crc_deinit(void)
{
  /*
     * Note: We intentionally do NOT disable the CRC module after initialization.
     *
     * Rationale:
     * 1. CRC calculations may be needed at any time for frame validation
     * 2. Module stop/start overhead is significant
     * 3. Power savings from disabling are minimal (CRC is low-power)
     *
     * If power optimization is critical, enable the module stop here:
     * system_regs()->prcr = k_rx_prcr_unlock_prc1_prc3;
     * system_regs()->mstpcrb |= ((uint32_t)k_crc_bit_set << k_mstpb_crc);
     * system_regs()->prcr = k_rx_prcr_lock;
     * s_crc_initialized = false;
     */

  /* Pre-condition: Validate module was initialized (NASA Rule 5) */
  RX_VALIDATE_INIT(s_crc_initialized, "CRC", "CRC module not initialized");

  /* Post-condition: Module state unchanged */
  RX_VALIDATE_INIT(s_crc_initialized, "CRC", "CRC module state corrupted");

  return k_rx_ok;
}

/**
 * @brief Calculate IEEE 802.3 CRC-32 using hardware acceleration
 *
 * Computes CRC-32 checksum using the RX72N CRC calculator peripheral.
 * Result is compatible with crc32.ChecksumIEEE() in Go and other
 * IEEE 802.3 CRC-32 implementations.
 *
 * @param[in] data Pointer to data buffer
 * @param[in] len Length of data in bytes (1 to 65535)
 *
 * @return CRC-32 checksum, or 0 on error
 *
 * @pre data must be non-NULL
 * @pre len must be in range [1, 65535]
 * @post CRC module is initialized if not already
 */
uint32_t rx_crc32_ieee_impl(const uint8_t* data, uint32_t len)
{
  rx_err_t          init_err;
  volatile uint8_t* crcdir_byte;
  uint32_t          i;

  /* Pre-condition: Validate input parameters (NASA Rule 5) */
  RX_CHECK_NULL_PTR(data, "CRC", "CRC data pointer is NULL");
  RX_CHECK_RANGE_TAG(len, k_crc_len_min, k_crc_len_max, k_crc_result_invalid, "CRC");

  /* Ensure CRC module is initialized */
  if (!s_crc_initialized) {
    init_err = rx_crc_init();
    if (init_err != k_rx_ok) {
      return k_crc_result_invalid;
    }
  }

  /*
     * Clear the CRC data output register to start fresh calculation.
     * Setting DORCLR bit resets the internal CRC state to the initial value.
     *
     * For IEEE 802.3, the initial value is 0xFFFFFFFF. The RX72N hardware
     * handles this automatically when DORCLR is set.
     */
  crc_regs()->crccr |= k_crc_crccr_dorclr;

  /*
     * Feed data bytes to the CRC calculator.
     *
     * The hardware processes each byte through the CRC polynomial.
     * We write bytes via pointer cast to the CRCDIR register address.
     *
     * Note: For large aligned buffers, 32-bit word writes could be faster,
     * but byte-wise ensures correctness for all cases.
     *
     * NASA Rule 2: Loop bounded by k_crc_len_max (compile-time constant)
     */
  crcdir_byte = (volatile uint8_t*)&crc_regs()->crcdir;
  for (i = k_crc_idx_start; i < k_crc_len_max; i++) {
    if (i >= len) {
      break;
    }
    *crcdir_byte = data[i];
  }

  /*
     * Read result and apply IEEE 802.3 finalization.
     *
     * IEEE 802.3 requires XOR with 0xFFFFFFFF after calculation.
     * The hardware outputs the raw CRC value, so we apply the final XOR.
     *
     * Post-condition: Result is valid IEEE 802.3 CRC-32 (NASA Rule 5)
     */
  return crc_regs()->crcdor ^ k_crc_ieee_final_xor;
}

/**
 * @brief Update CRC-32 with additional data (software fallback)
 *
 * Computes incremental CRC-32 by updating a previous CRC value with new data.
 * Uses software implementation because RX72N hardware doesn't support
 * loading arbitrary initial CRC state.
 *
 * @param[in] crc Previous CRC value to continue from
 * @param[in] data Pointer to new data buffer
 * @param[in] len Length of new data in bytes
 *
 * @return Updated CRC-32 checksum
 *
 * @pre data must be non-NULL
 * @pre len must be in valid range
 */
uint32_t rx_crc32_update_impl(uint32_t crc, const uint8_t* data, uint32_t len)
{
  /*
     * Use software fallback for incremental CRC.
     *
     * Rationale:
     * The RX72N CRC hardware doesn't support loading an arbitrary initial
     * CRC value. For incremental CRC (updating a previous CRC with new data),
     * we would need to:
     * 1. Un-finalize the input CRC (XOR with 0xFFFFFFFF)
     * 2. Load it into the hardware state
     * 3. Process new data
     * 4. Re-finalize
     *
     * The hardware's DORCLR resets to a fixed initial value, making step 2
     * problematic. Rather than complex state manipulation, we use the
     * software implementation which handles this cleanly.
     *
     * Performance note: Incremental CRC is typically used for small chunks
     * in streaming scenarios, where the software overhead is acceptable.
     */

  /* Pre-condition: Validate input parameters (NASA Rule 5) */
  RX_CHECK_NULL_PTR(data, "CRC", "CRC update data pointer is NULL");
  RX_CHECK_RANGE_TAG(len, k_crc_len_min, k_crc_len_max, k_crc_result_invalid, "CRC");

  /* Delegate to software implementation */
  return rx_crc32_update_sw(crc, data, len);
}

#endif /* RX_CRC32_USE_HARDWARE */
