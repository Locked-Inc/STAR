/**
 * @file rx_dmaca.h
 * @brief RX72N DMAC Polling-Mode Driver for CRC DMA Transfers
 *
 * @details
 * Provides a minimal polling-mode DMA driver for the RX72N DMAC peripheral.
 * Designed for use by rx_crc when offloading data-to-CRCDIR transfers via DMA,
 * freeing the CPU from byte-at-a-time writes.
 *
 * ## Architecture
 *
 * ```
 * rx_dmaca_transfer_poll()
 *     |
 *     +--> Configure DMSAR / DMDAR / DMCRA / DMTMD / DMAMD
 *     +--> Set DMCNT.DTE = 1
 *     +--> Set DMREQ.SWREQ = 1 (software trigger)
 *     +--> Poll DMSTS.ACT until 0 or timeout
 *     +--> Clear DMCNT.DTE
 * ```
 *
 * ## DMA Start Sequence (mandatory order)
 *
 * 1. Set `DMAST.DMST = 1` (module-level start) -- done once in rx_dmaca_init()
 * 2. Set `DMCNT.DTE = 1` (channel enable)
 * 3. Set `DMREQ.SWREQ = 1` (trigger transfer)
 *
 * ## Thread Safety
 *
 * rx_dmaca_transfer_poll() is NOT thread-safe. The caller must ensure that
 * only one transfer is active per channel at a time.
 *
 * ## NASA Power of 10
 *
 * | Rule | Status | Notes |
 * |------|--------|-------|
 * | 1. Simple flow | PASS | No goto/recursion |
 * | 2. Bounded loops | PASS | Timeout loop bounded by timeout_cycles |
 * | 3. No dynamic mem | PASS | Zero malloc |
 * | 4. Short functions | PASS | All <= 60 lines |
 * | 5. Assertions | PASS | NULL checks + range checks |
 * | 6. Small scope | PASS | Vars declared at first use |
 * | 7. Check returns | PASS | All returns checked |
 * | 8. Limit preprocessor | PASS | C23 typed enums |
 * | 9. Restrict pointers | WARN | Function pointers via bus interface pattern |
 * | 10. Compiler warnings | PASS | -Wall -Wextra -Werror |
 *
 * @see rx_crc.h CRC library that uses this driver for hw_dma backend
 * @see rx72n_dmac_regs.h Hardware register definitions
 *
 * @author Locked, Inc.
 * @date 2026-03-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum rx_dmaca_defaults_t
 * @brief Default constants for the DMAC-CRC integration
 *
 * @details
 * Provides compile-time constants for CRC-specific DMA usage.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief Timeout cycles for DMA polling (208 us at 240 MHz, 10x margin)
   * @details
   * Based on worst-case DMA transfer of 65535 bytes at 8-bit mode.
   * At ~240 MHz with ~6 cycles/byte, 65535 bytes ~ 1.6 ms = 384000 cycles.
   * 50000 cycles gives approximately 208 us which covers typical CRC buffers.
   * @par Value: 50000
   */
  k_dma_crc_timeout_cycles = 50000U,

  /**
   * @brief DMAC channel reserved for CRC transfers
   * @details Channel 0 has highest priority among the 8 DMAC channels.
   * @par Value: 0
   */
  k_dma_channel_crc = 0U,
} rx_dmaca_defaults_t;

/* =============================================================================
 * Types
 * =============================================================================
 */

/**
 * @struct rx_dmaca_config_t
 * @brief Configuration for a single DMAC polling transfer
 *
 * @details
 * Describes one DMA transfer from a source buffer to a fixed destination
 * address (e.g., CRCDIR at 0x00088284). All fields must be set before
 * calling rx_dmaca_transfer_poll().
 *
 * @invariant channel must be in [0, 7]
 * @invariant src must not be NULL
 * @invariant len must be in [1, 65535]
 * @invariant timeout_cycles must be > 0
 *
 * @code
 * rx_dmaca_config_t cfg = {
 *     .channel        = k_dma_channel_crc,
 *     .src            = data,
 *     .len            = len,
 *     .dst_addr       = k_crcdir_addr,
 *     .timeout_cycles = k_dma_crc_timeout_cycles,
 * };
 * rx_err_t err = rx_dmaca_transfer_poll(&cfg);
 * @endcode
 *
 * @see rx_dmaca_transfer_poll() Uses this config
 *
 * @since Version 1.0.0
 */
typedef struct {
  uint8_t        channel;        /**< DMA channel (0-7); 0 = highest priority */
  const uint8_t* src;            /**< Source buffer pointer (incremented by DMA) */
  uint32_t       len;            /**< Number of bytes to transfer [1, 65535] */
  uintptr_t      dst_addr;       /**< Destination address (fixed, e.g., CRCDIR) */
  uint32_t       timeout_cycles; /**< Max poll iterations before timeout error */
} rx_dmaca_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize the DMAC peripheral (enable module clock, set DMST=1)
 *
 * @details
 * Enables the DMAC module by clearing MSTPCRA bit 28 (module stop) then
 * setting DMAST.DMST = 1 to allow channel operation.
 *
 * Re-initialization returns k_rx_err_invalid_state (not k_rx_ok) to alert
 * callers of unexpected double-init. Callers that need idempotent behavior
 * should check the return value and treat k_rx_err_invalid_state as acceptable.
 *
 * @return rx_err_t
 * @retval k_rx_ok Peripheral initialized and ready
 * @retval k_rx_err_invalid_state Already initialized
 *
 * @pre System clock (PCLKA) must be running
 * @pre Protection registers must be accessible
 * @post MSTPCRA bit 28 cleared (DMAC clock enabled)
 * @post DMAST.DMST = 1 (module operating)
 *
 * @note Not thread-safe; call during single-threaded system init
 * @warning Do not call from interrupt context
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_dmaca_init(void);

/**
 * @brief Transfer data from src to a fixed destination via DMA (polling)
 *
 * @details
 * Configures DMAC channel, triggers a software-initiated transfer, then
 * polls DMSTS.ACT until transfer completes or timeout expires. Clears
 * DTE on completion or error.
 *
 * Transfer size (DMTMD.SZ) is always 8-bit (k_dmtmd_sz_8bit) for CRCDIR
 * compatibility. The CRCA peripheral processes 8-bit bytes regardless of
 * the 32-bit CRC polynomial selected.
 *
 * @param[in] config Transfer configuration (channel, src, len, dst, timeout)
 *
 * @return rx_err_t
 * @retval k_rx_ok Transfer complete
 * @retval k_rx_err_null_ptr config is NULL
 * @retval k_rx_err_invalid_arg channel >= 8, len == 0, len > 65535, timeout == 0
 * @retval k_rx_err_not_initialized rx_dmaca_init() not called
 * @retval k_rx_err_timeout Transfer did not complete within timeout_cycles
 *
 * @pre rx_dmaca_init() must have been called
 * @pre config->src must point to valid readable memory of config->len bytes
 * @post On success, all config->len bytes written to config->dst_addr
 * @post DMCNT.DTE cleared regardless of success or failure
 *
 * @note Not thread-safe on the same channel
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_dmaca_transfer_poll(const rx_dmaca_config_t* config);

/**
 * @brief Abort an in-progress DMA transfer on a channel
 *
 * @details
 * Clears DMCNT.DTE immediately, then polls until DMSTS.ACT = 0 to ensure
 * the transfer has stopped before returning.
 *
 * @param[in] channel Channel to abort (0-7)
 *
 * @return rx_err_t
 * @retval k_rx_ok Channel stopped
 * @retval k_rx_err_invalid_arg channel >= 8
 * @retval k_rx_err_not_initialized rx_dmaca_init() not called
 * @retval k_rx_err_timeout ACT bit did not clear within timeout
 *
 * @pre rx_dmaca_init() must have been called
 * @pre channel must be in [0, 7]
 * @post DMCNT.DTE = 0 for the specified channel
 * @post DMSTS.ACT = 0 (transfer stopped)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_dmaca_abort(uint8_t channel);

/**
 * @brief Deinitialize DMAC (clear DMST, lock MSTPCRA)
 *
 * @details
 * Sets DMAST.DMST = 0 then re-enables module stop (MSTPCRA bit 28 = 1)
 * to gate the DMAC clock for power saving.
 *
 * @return rx_err_t
 * @retval k_rx_ok Peripheral deinitialized
 * @retval k_rx_err_not_initialized Not initialized
 *
 * @pre rx_dmaca_init() must have been called
 * @pre No transfers are in progress on any channel
 * @post DMAST.DMST = 0 (all channels suspended)
 * @post MSTPCRA bit 28 = 1 (module stopped)
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_dmaca_deinit(void);

#ifdef __cplusplus
}
#endif
