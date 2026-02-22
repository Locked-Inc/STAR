/* lib/rx_crc/inc/rx_dmaca.h */

/**
 * @file    rx_dmaca.h
 * @brief   DMACA driver for RX72N
 *
 * Supports 8-bit and 32-bit block transfers only.
 * 16-bit transfers are explicitly prohibited: CRCDIR does not support them.
 *
 * NASA Power of 10 compliance:
 *   Rule 2  – All DMA completion loops use bounded timeouts (no infinite waits).
 *   Rule 6  – All return values are checked by callers.
 *   Rule 10 – No dynamic memory allocation; all state in caller-supplied structs.
* @author STAR Team
 * @date 2026-02-22
 * @copyright Copyright (c) 2026 STAR Project - MIT License
 */

#ifndef RX_DMACA_H
#define RX_DMACA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "rx_err.h"   /* rx_err_t, k_rx_ok, k_rx_err_timeout, k_rx_err_param */

/* =========================================================================
 * Public constants
 * ========================================================================= */

/** Maximum transfer count per DMA block (hardware limit: 10-bit counter). */
#define DMACA_MAX_TRANSFER_COUNT  (1024U)

/** Number of independently-configured DMACA channels on the RX72N. */
#define DMACA_NUM_CHANNELS        (8U)

/** Physical address of CRCDIR (CRC Data Input Register). */
#define CRCDIR_PHYS_ADDR          (0x00088284UL)

/* =========================================================================
 * Types
 * ========================================================================= */

/**
 * DMA transfer data width.
 *
 * NOTE: k_dmaca_size_word (16-bit) is intentionally absent.
 *       CRCDIR does not accept 16-bit accesses; attempting them produces
 *       undefined CRC results.  See RX72N HW Manual §46.2.
 */
typedef enum {
    k_dmaca_size_byte      = 0U,   /**< 8-bit  — DMTMD.SZ[1:0] = 00b */
    k_dmaca_size_longword  = 2U,   /**< 32-bit — DMTMD.SZ[1:0] = 10b */
} dmaca_data_size_t;

/** Address update mode for source or destination. */
typedef enum {
    k_dmaca_addr_fixed     = 0U,   /**< DMAMD.xM[1:0] = 00b  (register, FIFO) */
    k_dmaca_addr_increment = 2U,   /**< DMAMD.xM[1:0] = 10b  (normal buffer)  */
    k_dmaca_addr_decrement = 3U,   /**< DMAMD.xM[1:0] = 11b  (reverse buffer) */
} dmaca_addr_mode_t;

/** DMA transfer mode. */
typedef enum {
    k_dmaca_mode_normal = 0U,      /**< Normal  (1 unit per activation) */
    k_dmaca_mode_block  = 1U,      /**< Block   (full block per activation) — use for CRC */
    k_dmaca_mode_repeat = 2U,      /**< Repeat  (auto-reload source/dest) */
} dmaca_transfer_mode_t;

/** Activation source for the DMA transfer. */
//typedef enum {
    //k_dmaca_act_software = 0U,     /**< SWREQ in DMREQ  — no peripheral trigger */
    //k_dmaca_act_peripheral        /**< Hardware trigger from interrupt source */
//} dmaca_activation_t;

/**
 * Complete configuration for one DMA channel transfer.
 * Fill this struct, then call rx_dmaca_configure() followed by rx_dmaca_start().
 */
typedef struct {
    void                  *p_src;           /**< Source buffer address */
    void                  *p_dst;           /**< Destination address (fixed for CRCDIR) */
    uint32_t               transfer_count;  /**< Number of data units (1–1024) */
    dmaca_data_size_t      data_size;       /**< k_dmaca_size_byte or k_dmaca_size_longword ONLY */
    dmaca_transfer_mode_t  transfer_mode;   /**< Use k_dmaca_mode_block for CRC */
    dmaca_addr_mode_t      src_addr_mode;   /**< Typically k_dmaca_addr_increment */
    dmaca_addr_mode_t      dst_addr_mode;   /**< k_dmaca_addr_fixed for CRCDIR */
    //dmaca_activation_t     activation;      /**< k_dmaca_act_software for polling mode */
} dmaca_config_t;

/* =========================================================================
 * Timeout configuration  (NASA Rule 2 — bounded loops)
 * ========================================================================= */

/**
 * Worst-case execution time budget for DMA completion polling.
 *
 * Derivation (8-bit, 256-byte block):
 *   Per-transfer cost = (Cr + Cw) = (1 + 4) = 5 PCLKB cycles
 *   At 240 MHz ICLK / 60 MHz PCLKB: 5 × 4 = 20 ICLK cycles per byte
 *   256 bytes × 20 cycles = 5,120 CPU cycles
 *   Bus contention margin (EXDMAC priority preemption): +25 % → ~6,400
 *   Round up to nearest power of two with margin: 8,192
 *
 * Override with -DRX_DMACA_POLL_TIMEOUT_CYCLES=<n> in your build if you
 * have longer blocks or tighter real-time budgets.
 */
#ifndef RX_DMACA_POLL_TIMEOUT_CYCLES
#  define RX_DMACA_POLL_TIMEOUT_CYCLES  (8192U)
#endif

/* =========================================================================
 * API
 * ========================================================================= */

/**
 * @brief  Enable DMACA module clock and perform one-time hardware init.
 *
 * Must be called once before any other rx_dmaca_* function.
 *
 * @return k_rx_ok on success.
 */
rx_err_t rx_dmaca_init(void);

/**
 * @brief  Configure a single DMACA channel for a transfer.
 *
 * Does not start the transfer; call rx_dmaca_start() afterwards.
 *
 * @param[in] channel  Channel index 0–7.
 * @param[in] p_cfg    Pointer to filled dmaca_config_t.  Must not be NULL.
 *
 * @return k_rx_ok           Configuration accepted.
 * @return k_rx_err_param    channel ≥ DMACA_NUM_CHANNELS, p_cfg is NULL,
 *                           transfer_count is 0 or > 1024, or an unsupported
 *                           data_size was requested (e.g. 16-bit).
 */
rx_err_t rx_dmaca_configure(uint8_t channel, const dmaca_config_t *p_cfg);

/**
 * @brief  Arm and activate a previously configured channel.
 *
 * For software-triggered (k_dmaca_act_software) channels this also issues
 * the SWREQ pulse, so the transfer begins immediately.
 *
 * @param[in] channel  Channel index 0–7.
 *
 * @return k_rx_ok        Transfer started.
 * @return k_rx_err_param Channel not previously configured.
 */
rx_err_t rx_dmaca_start(uint8_t channel);

/**
 * @brief  Poll until the channel is idle or the timeout expires.
 *
 * Implements NASA Rule 2: the spin loop has a hard upper bound derived from
 * the worst-case bus latency for the configured block size.
 *
 * @param[in] channel        Channel index 0–7.
 * @param[in] timeout_cycles Maximum number of polling iterations before
 *                           declaring a timeout.  Use
 *                           RX_DMACA_POLL_TIMEOUT_CYCLES or a custom value
 *                           scaled to your block size.
 *
 * @return k_rx_ok           Transfer completed normally (ACT = 0, DTE = 0).
 * @return k_rx_err_timeout  Transfer did not complete within timeout_cycles.
 *                           The channel is forcibly disabled before returning.
 * @return k_rx_err_param    Invalid channel index.
 */
rx_err_t rx_dmaca_wait(uint8_t channel, uint32_t timeout_cycles);

/**
 * @brief  Unconditionally abort and disable a channel.
 *
 * Safe to call even if the channel is idle.
 *
 * @param[in] channel  Channel index 0–7.
 */
void rx_dmaca_abort(uint8_t channel);

/**
 * @brief  Convenience wrapper: configure, start, and poll to completion.
 *
 * Equivalent to calling rx_dmaca_configure() + rx_dmaca_start() +
 * rx_dmaca_wait() in sequence, with the timeout scaled to the transfer size.
 *
 * @param[in] channel  Channel index 0–7.
 * @param[in] p_cfg    Transfer configuration.
 *
 * @return k_rx_ok, k_rx_err_param, or k_rx_err_timeout (see above).
 */
rx_err_t rx_dmaca_transfer_blocking(uint8_t channel, const dmaca_config_t *p_cfg);

#ifdef __cplusplus
}
#endif

#endif /* RX_DMACA_H */