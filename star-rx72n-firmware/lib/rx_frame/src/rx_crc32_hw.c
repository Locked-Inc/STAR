/* lib/rx_frame/src/rx_crc32_hw.c */

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
#include "rx_gpio_constants.h"

/* =============================================================================
 * CRC Hardware Constants
 * =============================================================================
 */

/** @brief IEEE 802.3 CRC-32 final XOR value */
static const uint32_t k_crc_ieee_final_xor = 0xFFFFFFFF;

/**
 * @brief CRC module-specific PRCR unlock value
 *
 * Note: k_prcr_key, k_prcr_key_shift, and k_prcr_lock_all are from rx_gpio_constants.h
 */
typedef enum {
  k_prcr_unlock_crc = 0x0B, /**< Unlock PRC0, PRC1, PRC3 for CRC config */
} rx_crc_prcr_constants_t;

/* =============================================================================
 * Module State
 * =============================================================================
 */

static bool s_crc_initialized = false;

/* =============================================================================
 * Hardware CRC Implementation
 * =============================================================================
 */

rx_err_t rx_crc_init(void)
{
  if (s_crc_initialized) {
    return k_rx_ok;
  }

  /* Enable CRC module by clearing module stop bit */
  SYSTEM.PRCR =
    (k_prcr_key << k_prcr_key_shift) | k_prcr_unlock_crc; /* Unlock protection for MSTPCR */
  SYSTEM.MSTPCRB &= ~(1UL << MSTPB_CRC);                  /* Clear bit 23 to enable CRC */
  SYSTEM.PRCR = (k_prcr_key << k_prcr_key_shift) | k_prcr_lock_all; /* Lock protection */

  /*
     * Configure CRC peripheral for IEEE 802.3:
     * - GPS = 0x03 (CRC-32)
     * - LMS = 1 (LSB first / reflected mode for IEEE 802.3 compatibility)
     *
     * The RX72N CRC calculator in this configuration produces output
     * bit-exact with Go's crc32.ChecksumIEEE() when properly initialized.
     */
  CRC.CRCCR = k_crc_crccr_lms | k_crc_crccr_gps_crc32;

  s_crc_initialized = true;
  return k_rx_ok;
}

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
     * SYSTEM.PRCR = 0xA50B;
     * SYSTEM.MSTPCRB |= (1UL << MSTPB_CRC);
     * SYSTEM.PRCR = 0xA500;
     * s_crc_initialized = false;
     */

  return k_rx_ok;
}

uint32_t rx_crc32_ieee_impl(const uint8_t* data, uint32_t len)
{
  if (data == NULL || len == 0) {
    return 0;
  }

  /* Ensure CRC module is initialized */
  if (!s_crc_initialized) {
    rx_crc_init();
  }

  /*
     * Clear the CRC data output register to start fresh calculation.
     * Setting DORCLR bit resets the internal CRC state to the initial value.
     *
     * For IEEE 802.3, the initial value is 0xFFFFFFFF. The RX72N hardware
     * handles this automatically when DORCLR is set.
     */
  CRC.CRCCR |= k_crc_crccr_dorclr;

  /*
     * Feed data bytes to the CRC calculator.
     *
     * The hardware processes each byte through the CRC polynomial.
     * We write bytes via pointer cast to the CRCDIR register address.
     *
     * Note: For large aligned buffers, 32-bit word writes could be faster,
     * but byte-wise ensures correctness for all cases.
     */
  volatile uint8_t* crcdir_byte = (volatile uint8_t*)&CRC.CRCDIR;
  for (uint32_t i = 0; i < len; i++) {
    *crcdir_byte = data[i];
  }

  /*
     * Read result and apply IEEE 802.3 finalization.
     *
     * IEEE 802.3 requires XOR with 0xFFFFFFFF after calculation.
     * The hardware outputs the raw CRC value, so we apply the final XOR.
     */
  return CRC.CRCDOR ^ k_crc_ieee_final_xor;
}

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
  return rx_crc32_update_sw(crc, data, len);
}

#endif /* RX_CRC32_USE_HARDWARE */
