/**
 * @file rx_crc_internal.h
 * @brief Internal CRC API - Software and Hardware Backend Declarations
 *
 * @details
 * Internal-only header used by rx_crc.c, rx_crc_sw.c, and rx_crc_hw.c.
 * Declares the internal dispatch functions for each backend. Application code
 * must use rx_crc.h instead.
 *
 * ## Architecture
 *
 * ```
 * rx_crc_compute()            (rx_crc.c - public dispatcher)
 *   |
 *   +--> internal_crc_sw_compute()      (rx_crc_sw.c - all 5 polys, any platform)
 *   |
 *   +--> internal_crc_hw_cpu_compute()  (rx_crc_hw.c - RX72N only, #ifdef __RX__)
 *   |
 *   +--> internal_crc_hw_dma_compute()  (rx_crc_hw.c - RX72N only, #ifdef __RX__)
 * ```
 *
 * @see rx_crc.h    Public API (use this in application code)
 * @see rx_crc_sw.c Software implementations
 * @see rx_crc_hw.c Hardware implementations
 *
 * @author Locked, Inc.
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#include "rx_crc.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Shared Constants
 * =============================================================================
 */

/**
 * @enum rx_crc_shared_constants_t
 * @brief Shared constants for CRC validation and NASA-compliant bounded loops
 *
 * @details
 * Defines loop bounds and starting indices used by all CRC implementations.
 * Satisfies NASA Power of 10 Rule 2 (bounded loops).
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_crc_len_min   = 1U,     /**< Minimum meaningful CRC data length (bytes) */
  k_crc_len_max   = 65535U, /**< Maximum CRC data length - matches DMA limit (bytes) */
  k_crc_idx_start = 0U,     /**< Standard loop start index (zero-based) */
} rx_crc_shared_constants_t;

/**
 * @enum rx_crc_dma_constants_t
 * @brief DMA backend threshold and related constants
 *
 * @details
 * The DMA backend falls back to hw_cpu for buffers below k_crc_dma_threshold
 * because DMA setup overhead exceeds the computation cost for small transfers.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_crc_dma_threshold = 128U, /**< Min bytes to use DMA (below = hw_cpu fallback) */
} rx_crc_dma_constants_t;

/* =============================================================================
 * Software Backend (rx_crc_sw.c) - All Platforms
 * =============================================================================
 */

/**
 * @brief Compute CRC using software for the given polynomial
 *
 * @details
 * Portable software implementation supporting all five polynomials. Uses
 * lookup tables for CRC-32 and CRC-32C, and bitwise algorithms for the
 * others. Always available (no hardware dependency).
 *
 * @param[in] poly CRC polynomial selection
 * @param[in] data Input data buffer (must be non-NULL)
 * @param[in] len  Number of bytes (must be >= 1)
 *
 * @return CRC result (8, 16, or 32 significant bits depending on polynomial)
 *
 * @pre data must point to at least len bytes
 * @pre len must be in [k_crc_len_min, k_crc_len_max]
 * @post No side effects (pure function, no heap allocation, no global state modified)
 * @post Return value has at most 32 significant bits; upper bits zero for CRC-8 and CRC-16
 *
 * @note Thread-safe (stateless, read-only tables)
 * @since Version 1.0.0
 */
uint32_t internal_crc_sw_compute(rx_crc_poly_t poly, const uint8_t* data, uint32_t len);

/**
 * @brief Update CRC-32/IEEE in-progress value with additional data
 *
 * @details
 * Continues an IEEE 802.3 CRC-32 from a finalized partial result. Un-finalizes
 * the input CRC, feeds the new data, then re-finalizes. Used by rx_crc32_update().
 *
 * @param[in] crc  Previous finalized CRC-32 value
 * @param[in] data Additional data buffer (must be non-NULL)
 * @param[in] len  Number of bytes (must be >= 1)
 *
 * @return Updated finalized CRC-32 value
 *
 * @pre data must point to at least len bytes, or be NULL (pass-through)
 * @pre crc must be 0U (first call) or the finalized CRC-32/IEEE value returned
 *      by a prior rx_crc32_update() / internal_crc32_sw_update() call
 * @post Returns finalized CRC-32/IEEE value; no heap or global state modified
 * @post No side effects (pure computation, read-only table access)
 *
 * @note Thread-safe (stateless, read-only table)
 * @since Version 1.0.0
 */
uint32_t internal_crc32_sw_update(uint32_t crc, const uint8_t* data, uint32_t len);

/* =============================================================================
 * Hardware Backend (rx_crc_hw.c) - RX72N Only
 * =============================================================================
 */

#ifdef __RX__

/**
 * @brief Initialize RX72N CRC peripheral (enable CRCA module clock)
 *
 * @return rx_err_t k_rx_ok on success
 *
 * @pre System clock (PCLKB) must be configured and running
 * @pre PRCR register must be writable (caller must not hold PRCR lock)
 * @post CRCA module clock enabled (MSTPCRB bit 23 cleared)
 * @post DMAC driver initialized (rx_dmaca_init() called) if not already running
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t internal_crc_hw_init(void);

/**
 * @brief Deinitialize RX72N CRC peripheral (disable CRCA module clock)
 *
 * @return rx_err_t k_rx_ok on success
 *
 * @pre internal_crc_hw_init() must have been called successfully before this
 * @pre No CRC or DMA transfers may be in progress at time of call
 * @post CRCA module clock disabled (MSTPCRB bit 23 set)
 * @post DMAC driver deinitialized (rx_dmaca_deinit() called) if owned by this module
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t internal_crc_hw_deinit(void);

/**
 * @brief Compute CRC using RX72N CRC peripheral, CPU-driven byte loop
 *
 * @details
 * Falls back to software for CRC-8/Maxim (hardware polynomial mismatch).
 * Configures CRCCR with GPS and LMS, feeds data byte-by-byte, reads CRCDOR.
 *
 * @param[in]  config     CRC configuration
 * @param[in]  data       Input data (non-NULL, len >= 1)
 * @param[in]  len        Number of bytes
 * @param[out] result_out CRC result
 *
 * @return rx_err_t k_rx_ok on success
 *
 * @pre CRC peripheral must be initialized via internal_crc_hw_init()
 * @pre config, data, and result_out must not be NULL
 * @post *result_out contains the computed CRC on k_rx_ok
 * @post CRC peripheral registers returned to idle state (CRCDOR readable)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t internal_crc_hw_cpu_compute(const rx_crc_config_t* config,
                                                   const uint8_t*         data,
                                                   uint32_t               len,
                                                   uint32_t*              result_out);

/**
 * @brief Compute CRC using RX72N CRC peripheral, DMA-driven transfer
 *
 * @details
 * Falls back to hw_cpu for:
 * - CRC-8/Maxim (hardware polynomial mismatch)
 * - len < k_crc_dma_threshold (DMA overhead not worth it)
 *
 * Configures CRCCR, then calls rx_dmaca_transfer_poll() to move src -> CRCDIR.
 *
 * @param[in]  config     CRC configuration (dma.timeout_cycles used if > 0)
 * @param[in]  data       Input data (non-NULL, len >= 1)
 * @param[in]  len        Number of bytes
 * @param[out] result_out CRC result
 *
 * @return rx_err_t
 * @retval k_rx_ok              Success
 * @retval k_rx_err_null_ptr    config, data, or result_out is NULL
 * @retval k_rx_err_invalid_arg CRC-32/CRC-32C buffer pointer or length not aligned
 *                              to k_hw_crc32_alignment_bytes
 * @retval k_rx_err_timeout     DMA timed out
 *
 * @pre CRC and DMAC peripherals must be initialized (internal_crc_hw_init() called)
 * @pre For CRC-32/CRC-32C: data pointer and len must each be aligned to
 *      k_hw_crc32_alignment_bytes (4 bytes); unaligned inputs return
 *      k_rx_err_invalid_arg before touching the peripheral
 * @post *result_out contains the final CRC with XOR applied on k_rx_ok;
 *       result_out is not written on any error return
 * @post DMA channel is idle on return (transfer complete, or aborted on timeout)
 *
 * @note NOT thread-safe. Unlike internal_crc_sw_compute and
 *       internal_crc32_sw_update (which are stateless and re-entrant), this
 *       function programs shared DMA channel hardware and writes result_out.
 *       Callers must serialize all concurrent invocations. Concurrent calls
 *       will corrupt the in-flight DMA transfer and produce wrong result_out.
 *
 * @see k_hw_crc32_alignment_bytes Alignment requirement for CRC-32/CRC-32C (defined in rx_crc.h)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t internal_crc_hw_dma_compute(const rx_crc_config_t* config,
                                                   const uint8_t*         data,
                                                   uint32_t               len,
                                                   uint32_t*              result_out);

#endif /* __RX__ */

#ifdef __cplusplus
}
#endif
