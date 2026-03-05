/**
 * @file rx_crc_hw.c
 * @brief RX72N Hardware CRC Backend Implementation
 *
 * @details
 * Implements internal_crc_hw_init(), internal_crc_hw_deinit(),
 * internal_crc_hw_cpu_compute(), and internal_crc_hw_dma_compute() for the
 * RX72N CRC Calculator peripheral (base 0x00088280, MSTPCRB bit 23).
 *
 * ## GPS Mapping (CRCCR bits [2:0])
 *
 * | rx_crc_poly_t              | GPS  | Notes                           |
 * |----------------------------|------|---------------------------------|
 * | k_rx_crc_poly_crc8  (0x00) | 0x00 | Wrong poly for Maxim -> SW only |
 * | k_rx_crc_poly_crc16 (0x01) | 0x01 | CRC-16/IBM                      |
 * | k_rx_crc_poly_crc_ccitt (0x02) | 0x02 | CRC-CCITT/Kermit            |
 * | k_rx_crc_poly_crc32 (0x03) | 0x03 | CRC-32/IEEE 802.3               |
 * | k_rx_crc_poly_crc32c (0x04) | 0x04 | CRC-32C/Castagnoli             |
 *
 * @see rx_crc_internal.h Internal declarations
 * @see rx72n_crc_regs.h   CRC peripheral register layout
 * @see rx_dmaca.h         DMA driver
 *
 * @author Locked, Inc.
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_crc_internal.h"

#ifdef __RX__

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_dmaca.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Private Constants
 * =============================================================================
 */

/**
 * @brief CRCCR configuration bits for hw_cpu compute
 *
 * @details
 * LMS (bit 6) = 1: LSB-first (reflected). Required for IEEE 802.3 / Kermit
 * compatibility. DORCLR (bit 7) = 1: clear accumulator before computation.
 */
typedef enum : uint8_t {
  k_hw_crccr_lms    = k_crc_crccr_lms,    /**< Bit 6: LSB-first mode */
  k_hw_crccr_dorclr = k_crc_crccr_dorclr, /**< Bit 7: clear output register */
} rx_crc_hw_crccr_t;

/**
 * @brief Final XOR constants for CRC result finalization
 */
typedef enum : uint32_t {
  k_hw_crc32_final_xor = 0xFFFFFFFFU, /**< IEEE 802.3 / CRC-32C final XOR */
  k_hw_crc16_final_xor = 0x00000000U, /**< CRC-16/IBM: no final XOR */
  k_hw_ccitt_final_xor = 0x00000000U, /**< CRC-CCITT/Kermit: no final XOR */
} rx_crc_hw_final_xor_t;

/**
 * @brief Module stop bit position for MSTPCRB
 */
typedef enum : uint8_t {
  k_hw_mstpb_crc_bit = k_mstpb_crc, /**< Bit 23: CRC module stop in MSTPCRB */
  k_hw_bit_one       = 1U,          /**< Single bit value for shift operations */
} rx_crc_hw_mstpcrb_t;

/* =============================================================================
 * Private Helpers
 * =============================================================================
 */

/**
 * @brief Return the final XOR value for the given polynomial
 *
 * @param[in] poly CRC polynomial
 * @return Final XOR value to apply after reading CRCDOR
 *
 * @pre poly is a valid rx_crc_poly_t
 * @post Returns 0xFFFFFFFF for CRC-32/CRC-32C, 0 otherwise
 * @since Version 1.0.0
 */
static uint32_t internal_final_xor(rx_crc_poly_t poly)
{
  if (poly == k_rx_crc_poly_crc32 || poly == k_rx_crc_poly_crc32c) {
    return k_hw_crc32_final_xor;
  }
  return k_hw_crc16_final_xor;
}

/**
 * @brief Configure CRC peripheral control register and clear accumulator
 *
 * @details
 * Sets GPS from poly enum value (valid for CRC-16 through CRC-32C), sets LMS
 * from config->bit_order, and sets DORCLR to reset the CRC accumulator.
 *
 * @param[in] config CRC configuration (poly and bit_order used)
 *
 * @pre CRC peripheral module clock must be enabled
 * @post CRCCR configured; accumulator reset to initial value for the polynomial
 * @since Version 1.0.0
 */
static void internal_configure_crccr(const rx_crc_config_t* config)
{
  uint8_t lms = (config->bit_order == k_rx_crc_bit_order_lsb_first) ? (uint8_t)k_hw_crccr_lms : 0U;

  /* GPS = poly enum value (0x01..0x04 maps directly to CRCCR GPS field) */
  uint8_t gps = (uint8_t)config->poly;

  crc_regs()->crccr = (uint8_t)(gps | lms | (uint8_t)k_hw_crccr_dorclr);
}

/* =============================================================================
 * Lifecycle
 * =============================================================================
 */

rx_err_t internal_crc_hw_init(void)
{
  RX_CHECK_NULL_PTR(system_regs(), "CRC", "System registers not accessible");
  RX_CHECK_NULL_PTR(crc_regs(), "CRC", "CRC registers not accessible");

  /* Unlock protection and enable CRC module clock (clear MSTPCRB bit 23) */
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcrb &= ~((uint32_t)k_hw_bit_one << k_hw_mstpb_crc_bit);
  *prcr_reg() = k_rx_prcr_lock;

  return k_rx_ok;
}

rx_err_t internal_crc_hw_deinit(void)
{
  RX_CHECK_NULL_PTR(system_regs(), "CRC", "System registers not accessible");

  /* Re-enable module stop (set MSTPCRB bit 23) */
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcrb |= ((uint32_t)k_hw_bit_one << k_hw_mstpb_crc_bit);
  *prcr_reg() = k_rx_prcr_lock;

  return k_rx_ok;
}

/* =============================================================================
 * CPU-Driven Hardware CRC
 * =============================================================================
 */

rx_err_t internal_crc_hw_cpu_compute(const rx_crc_config_t* config,
                                     const uint8_t*         data,
                                     uint32_t               len,
                                     uint32_t*              result_out)
{
  if (config == nullptr || data == nullptr || result_out == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* CRC-8/Maxim: hardware GPS=0x00 uses poly 0x07, not 0x31 - must use SW */
  if (config->poly == k_rx_crc_poly_crc8) {
    *result_out = internal_crc_sw_compute(config->poly, data, len);
    return k_rx_ok;
  }

  internal_configure_crccr(config);

  /* Feed data byte-by-byte (NASA Rule 2: bounded by k_crc_len_max) */
  volatile uint8_t* crcdir_byte = (volatile uint8_t*)&crc_regs()->crcdir;
  for (uint32_t i = k_crc_idx_start; i < k_crc_len_max; i++) {
    if (i >= len) {
      break;
    }
    *crcdir_byte = data[i];
  }

  *result_out = crc_regs()->crcdor ^ internal_final_xor(config->poly);
  return k_rx_ok;
}

/* =============================================================================
 * DMA-Driven Hardware CRC
 * =============================================================================
 */

rx_err_t internal_crc_hw_dma_compute(const rx_crc_config_t* config,
                                     const uint8_t*         data,
                                     uint32_t               len,
                                     uint32_t*              result_out)
{
  if (config == nullptr || data == nullptr || result_out == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* CRC-8/Maxim or small buffer: fall back to CPU-driven path */
  if (config->poly == k_rx_crc_poly_crc8 || len < k_crc_dma_threshold) {
    return internal_crc_hw_cpu_compute(config, data, len, result_out);
  }

  internal_configure_crccr(config);

  /* Use config timeout if non-zero, otherwise use library default */
  uint32_t timeout = (config->dma.timeout_cycles != 0U) ? config->dma.timeout_cycles
                                                        : (uint32_t)k_dma_crc_timeout_cycles;

  /* Destination: byte address of CRCDIR register (fixed, no increment) */
  const rx_dmaca_config_t dma_cfg = {
    .channel        = (uint8_t)k_dma_channel_crc,
    .src            = data,
    .len            = len,
    .dst_addr       = (uintptr_t)((volatile uint8_t*)&crc_regs()->crcdir),
    .timeout_cycles = timeout,
  };

  rx_err_t err = rx_dmaca_transfer_poll(&dma_cfg);
  if (err != k_rx_ok) {
    return err;
  }

  *result_out = crc_regs()->crcdor ^ internal_final_xor(config->poly);
  return k_rx_ok;
}

#endif /* __RX__ */
