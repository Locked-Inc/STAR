/**
 * @file rx_crc_hw.c
 * @brief RX72N Hardware CRC Backend Implementation
 *
 * @details
 * Implements internal_crc_hw_init(), internal_crc_hw_deinit(),
 * internal_crc_hw_cpu_compute(), and internal_crc_hw_dma_compute() for the
 * RX72N CRC Calculator peripheral (base 0x00088280, MSTPCRB bit 23).
 *
 * ## GPS (General Purpose Subsystem) Mapping (CRCCR bits [2:0])
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

/**
 * @brief Alignment requirement for CRC-32 and CRC-32C DMA transfers
 */
typedef enum : uint8_t {
  k_hw_crc32_alignment_bytes = 4U, /**< CRC-32/CRC-32C require 4-byte aligned buffer and length */
} rx_crc_hw_alignment_t;

/* =============================================================================
 * Module State
 * =============================================================================
 */

/**
 * @var s_dmaca_owned
 * @brief True when this module called rx_dmaca_init() successfully
 *
 * @details
 * Set to true when rx_dmaca_init() returns k_rx_ok in internal_crc_hw_init().
 * Remains false when rx_dmaca_init() returns k_rx_err_invalid_state (DMAC was
 * already initialized by a third party). internal_crc_hw_deinit() calls
 * rx_dmaca_deinit() only when this flag is true, preventing double-deinit.
 *
 * @note Not thread-safe; only mutated during single-threaded init/deinit.
 * @since Version 1.0.0
 */
static bool s_dmaca_owned = false;

/**
 * @var s_dma_fallback_count
 * @brief Count of DMA transfer failures that triggered CPU fallback
 *
 * @details
 * Incremented each time rx_dmaca_transfer_poll() returns an error and the
 * hw_dma backend falls back to the CPU loop. Non-zero values indicate DMA
 * reliability issues (timeout, misconfiguration) that are otherwise silent.
 *
 * @note Not thread-safe: rx_dmaca_transfer_poll() is itself single-channel
 *       and not thread-safe, so concurrent access cannot occur in practice.
 * @warning Do not modify directly; read via debug/telemetry only.
 * @since Version 1.0.0
 */
static uint32_t s_dma_fallback_count = 0U;

/**
 * @var s_tag
 * @brief Log tag for CRC hardware module
 * @details Identifies log messages from this module
 * @note Read-only after initialization
 * @since Version 1.0.0
 */
static const char* const s_tag = "CRC";

/* =============================================================================
 * Private Helpers
 * =============================================================================
 */

/**
 * @brief Return the final XOR value for the given polynomial
 *
 *
 * @pre poly is a valid rx_crc_poly_t value within the defined enum range
 * @pre Called only for hardware-supported polynomials (CRC-16 through CRC-32C)
 * @post Returns 0xFFFFFFFF for CRC-32/CRC-32C, 0 otherwise
 * @post No side effects; does not modify any global or peripheral state
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
 *
 * @pre CRC peripheral module clock must be enabled
 * @pre config must not be NULL; config->poly and config->bit_order must be valid enum values
 * @post CRCCR configured; accumulator reset to initial value for the polynomial
 * @post Only CRCCR and the CRC accumulator (via DORCLR) are modified; no other registers changed
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

/**
 * @brief Enable CRC peripheral clock and initialize DMAC for hw_dma backend
 *
 * @details
 * Unlocks PRCR, clears MSTPCRB bit 23 to enable the CRC peripheral clock,
 * then re-locks PRCR. Calls rx_dmaca_init() to prepare the DMA channel;
 * if DMAC was already initialized (k_rx_err_invalid_state), the CRC module
 * records that it does not own DMAC and will not deinit it on teardown.
 *
 *
 * @pre System registers (SYSTEM) must be accessible
 * @pre CRC registers must be accessible
 * @post MSTPCRB bit 23 cleared (CRC clock enabled)
 * @post s_dmaca_owned set to true only if rx_dmaca_init() returned k_rx_ok
 *
 * @par Sequence Diagram:
 * @startuml
 * participant "caller" as C
 * participant "internal_crc_hw_init" as I
 * participant "PRCR + MSTPCRB" as R
 * participant "rx_dmaca_init" as D
 * C -> I: call
 * I -> R: PRCR = unlock_prc1
 * I -> R: MSTPCRB &= ~bit23 (enable CRC clock)
 * I -> R: PRCR = lock
 * I -> D: rx_dmaca_init()
 * alt k_rx_ok
 *   D --> I: k_rx_ok
 *   I -> I: s_dmaca_owned = true
 * else k_rx_err_invalid_state
 *   D --> I: k_rx_err_invalid_state
 *   note right of I: already owned -- s_dmaca_owned stays false\ndeinit will not call rx_dmaca_deinit()
 * else unexpected error
 *   D --> I: other error
 *   I -> R: PRCR = unlock_prc1
 *   I -> R: MSTPCRB |= bit23 (rollback CRC clock)
 *   I -> R: PRCR = lock
 *   I --> C: propagate error
 *   return
 * end
 * I --> C: k_rx_ok
 * @enduml
 *
 * @note Not thread-safe; call during single-threaded system init
 * @since Version 1.0.0
 */
rx_err_t internal_crc_hw_init(void)
{
  RX_CHECK_NULL_PTR(system_regs(), s_tag, "System registers not accessible");
  RX_CHECK_NULL_PTR(crc_regs(), s_tag, "CRC registers not accessible");

  /* Unlock protection and enable CRC module clock (clear MSTPCRB bit 23) */
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcrb &= ~((uint32_t)k_hw_bit_one << k_hw_mstpb_crc_bit);
  *prcr_reg() = k_rx_prcr_lock;

  /* Initialize DMA driver; treat k_rx_err_invalid_state as OK (already initialized),
   * propagate any unexpected error so callers know DMA is unavailable.
   * Track ownership: only deinit DMAC on teardown if we initialized it here.
   * On unexpected failure, roll back CRC clock to leave no partial state. */
  rx_err_t dma_ret = rx_dmaca_init();
  if (dma_ret == k_rx_ok) {
    s_dmaca_owned = true;
  } else if (dma_ret != k_rx_err_invalid_state) {
    /* Roll back: disable CRC clock before returning the error */
    *prcr_reg() = k_rx_prcr_unlock_prc1;
    system_regs()->mstpcrb |= ((uint32_t)k_hw_bit_one << k_hw_mstpb_crc_bit);
    *prcr_reg() = k_rx_prcr_lock;
    return dma_ret;
  }

  return k_rx_ok;
}

/**
 * @brief Disable CRC peripheral clock and release DMAC if owned
 *
 * @details
 * Calls rx_dmaca_deinit() only if s_dmaca_owned is true (this module called
 * rx_dmaca_init() successfully). Then unlocks PRCR, sets MSTPCRB bit 23 to
 * stop the CRC peripheral clock, and re-locks PRCR.
 *
 *
 * @pre System registers (SYSTEM) must be accessible
 * @pre internal_crc_hw_init() must have been called successfully
 * @post MSTPCRB bit 23 set (CRC clock disabled)
 * @post s_dmaca_owned cleared to false
 *
 * @note Not thread-safe; call during single-threaded system teardown
 * @since Version 1.0.0
 */
rx_err_t internal_crc_hw_deinit(void)
{
  RX_CHECK_NULL_PTR(system_regs(), s_tag, "System registers not accessible");

  /* Deinitialize DMA driver only if this module initialized it */
  if (s_dmaca_owned) {
    (void)rx_dmaca_deinit();
    s_dmaca_owned = false;
  }

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

/**
 * @brief Compute CRC using RX72N CRC peripheral, CPU-driven byte loop
 *
 * @details
 * Configures CRCCR with GPS (polynomial) and LMS (bit order) bits, sets DORCLR
 * to reset the accumulator, then feeds data byte-by-byte to CRCDIR.
 * CRC-8/Maxim (GPS=0x00) maps to the wrong hardware polynomial (0x07 vs 0x31),
 * so it falls back to internal_crc_sw_compute() automatically.
 *
 *
 *
 * @pre CRC peripheral clock must be enabled (internal_crc_hw_init() called)
 * @pre data must point to valid readable memory of len bytes
 * @post *result_out contains the final CRC with XOR applied
 * @post CRCDOR accumulator is in indeterminate state after read
 *
 * @note CRC-8/Maxim always uses software fallback (hardware poly mismatch)
 * @since Version 1.0.0
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
    /* Software CRC-8/Maxim only supports LSB-first bit order */
    if (config->bit_order != k_rx_crc_bit_order_lsb_first) {
      return k_rx_err_invalid_arg;
    }
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

/**
 * @brief Compute CRC using RX72N CRC peripheral, DMA-driven transfer
 *
 * @details
 * Configures CRCCR then uses rx_dmaca_transfer_poll() to move src to CRCDIR
 * without CPU intervention. Falls back to internal_crc_hw_cpu_compute() for:
 * - CRC-8/Maxim (hardware polynomial mismatch)
 * - len < k_crc_dma_threshold (DMA overhead not justified)
 * - DMA transfer failure (s_dma_fallback_count incremented for observability)
 *
 *
 *
 * @pre CRC and DMAC peripherals must be initialized (internal_crc_hw_init() called)
 * @pre data must point to valid readable memory of len bytes
 * @post *result_out contains the final CRC with XOR applied (on k_rx_ok)
 * @post s_dma_fallback_count incremented only on transient DMA failure (not on invalid_arg)
 *
 * @par Activity Diagram:
 * @startuml
 * start
 * :internal_crc_hw_dma_compute(config, data, len, result_out);
 * if (any pointer NULL?) then (yes)
 *   :return k_rx_err_null_ptr;
 *   stop
 * endif
 * if (poly==crc8 or len < DMA threshold?) then (yes)
 *   :internal_crc_hw_cpu_compute() [direct];
 *   stop
 * endif
 * note right
 *   No alignment check: DMAC uses 8-bit transfers
 *   (k_dmtmd_sz_8bit), so any byte count is valid
 * end note
 * :internal_configure_crccr() -- GPS + LMS + DORCLR;
 * :build rx_dmaca_config_t (src->CRCDIR, timeout);
 * :rx_dmaca_transfer_poll(&dma_cfg);
 * if (err == k_rx_err_invalid_arg?) then (yes)
 *   :return k_rx_err_invalid_arg (bad config);
 *   stop
 * endif
 * if (other DMA failure (timeout)?) then (yes)
 *   if (s_dma_fallback_count < UINT32_MAX?) then (yes)
 *     :s_dma_fallback_count++;
 *   endif
 *   :internal_crc_hw_cpu_compute() [fallback];
 *   stop
 * endif
 * :*result_out = CRCDOR XOR final_xor;
 * :return k_rx_ok;
 * stop
 * @enduml
 *
 * @note Transient DMA failures fall back transparently to the CPU path; result is identical
 * @since Version 1.0.0
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

  /* No alignment requirement: DMAC is configured for 8-bit (k_dmtmd_sz_8bit)
   * transfers in internal_configure_channel(), so CRCDIR receives one byte at
   * a time regardless of the CRC polynomial selected. */
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
  if (err == k_rx_err_invalid_arg) {
    /* Bad config (e.g. timeout_cycles out of range): surface to caller, do not fall back */
    return err;
  }
  if (err != k_rx_ok) {
    /* Transient DMA failure (timeout): record for observability (saturating), fall back to CPU */
    if (s_dma_fallback_count < UINT32_MAX) {
      s_dma_fallback_count++;
    }
    return internal_crc_hw_cpu_compute(config, data, len, result_out);
  }

  *result_out = crc_regs()->crcdor ^ internal_final_xor(config->poly);
  return k_rx_ok;
}

#endif /* __RX__ */
