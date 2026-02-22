/* lib/rx_crc/src/rx_dmaca.c */

/**
 * @file    rx_dmaca.c
 * @brief   DMACA driver for RX72N — Phase 1 (polling / software-trigger mode)
 *
 * Implements the API declared in rx_dmaca.h.
 *
 * Hardware references:
 *   RX72N Hardware Manual §18  (DMACA)
 *   RX72N Hardware Manual §46  (CRCA)
 *   RX Family Register Access  iodefine.h / r_bsp header set
 *
 * Design constraints honoured:
 *   • 8-bit and 32-bit transfers only — 16-bit is prohibited for CRCDIR.
 *   • Destination address is NEVER incremented for CRCDIR use.
 *   • All completion loops are bounded (NASA Rule 2).
 *   • No dynamic memory allocation (NASA Rule 10).
 *   • Every public function validates its arguments and returns rx_err_t.
 * @author STAR Team
 * @date 2026-02-22
 * @copyright Copyright (c) 2026 STAR Project - MIT License
 */



#include "rx_dmaca.h"
#include <stddef.h>          /* NULL */

/* Renesas BSP register-map header (adjust path for your project layout). */
#include "platform.h"        /* includes iodefine.h, r_bsp, etc.          */

/* =========================================================================
 * Internal helpers — register access
 *
 * The RX72N iodefine.h exposes DMAC channels as an array:
 *   DMAC0 … DMAC7  or  DMAC[0] … DMAC[7]  (BSP-version-dependent)
 *
 * We use a small inline accessor to keep channel selection uniform.
 * ========================================================================= */

/** Return a pointer to the volatile DMAC channel register block. */
static volatile struct st_dmac0 * dmac_ch(uint8_t ch)
{
    /* iodefine.h places consecutive DMAC structs at known offsets.
     * The cast arithmetic below matches the RX72N memory map:
     *   DMAC0 @ 0x00082000, stride = 0x40 bytes per channel.          */
    return (volatile struct st_dmac0 *)(0x00082000UL + ((uint32_t)ch * 0x40UL));
}

/* =========================================================================
 * Module-level state
 * ========================================================================= */

/** Set to true after rx_dmaca_init() completes successfully. */
static bool s_dmaca_initialised = false;

/**
 * Track which channels have been configured but not yet completed.
 * Bit N = 1 means channel N has an active or pending configuration.
 */
static uint8_t s_channel_configured = 0U;

/* =========================================================================
 * Argument validation helpers
 * ========================================================================= */

static rx_err_t validate_channel(uint8_t channel)
{
    if (channel >= DMACA_NUM_CHANNELS) {
        return k_rx_err_param;
    }
    return k_rx_ok;
}

/* =========================================================================
 * Public API implementation
 * ========================================================================= */

rx_err_t rx_dmaca_init(void)
{
    // DMAC is not affected by MSTP (11.4 in User's Manual)
    /* Enable DMACA module-level operation (DMST bit in DMAST register). */
    DMAC.DMAST.BIT.DMST = 1;

    s_dmaca_initialised  = true;
    s_channel_configured = 0U;

    return k_rx_ok;
}

rx_err_t rx_dmaca_configure(uint8_t channel, const dmaca_config_t *p_cfg)
{
    rx_err_t err;

    /* ---- Argument checks ------------------------------------------------ */
    err = validate_channel(channel);
    if (err != k_rx_ok) {
        return err;
    }

    if (p_cfg == NULL) {
        return k_rx_err_param;
    }

    if ((p_cfg->transfer_count == 0U) ||
        (p_cfg->transfer_count > DMACA_MAX_TRANSFER_COUNT)) {
        return k_rx_err_param;
    }

    /*
     * Reject 16-bit (word) transfers explicitly.
     * CRCDIR does not support 16-bit bus access.  Any value other than
     * k_dmaca_size_byte (0) or k_dmaca_size_longword (2) is invalid.
     */
    if ((p_cfg->data_size != k_dmaca_size_byte) &&
        (p_cfg->data_size != k_dmaca_size_longword)) {
        return k_rx_err_param;
    }

    if ((p_cfg->p_src == NULL) || (p_cfg->p_dst == NULL)) {
        return k_rx_err_param;
    }

    /* ---- Stop the channel before reconfiguring -------------------------- */
    volatile struct st_dmac0 *ch = dmac_ch(channel);
    ch->DMCNT.BIT.DTE = 0;   /* Disable transfer enable */

    /* ---- Source address ------------------------------------------------- */
    ch->DMSAR = (uint32_t)(uintptr_t)p_cfg->p_src;

    /* ---- Destination address -------------------------------------------- */
    ch->DMDAR = (uint32_t)(uintptr_t)p_cfg->p_dst;

    /* ---- Transfer count ------------------------------------------------- */
    ch->DMCRA = (uint32_t)p_cfg->transfer_count;   /* Block / normal count  */
    ch->DMCRB = 0U;                                 /* Repeat count (unused) */

    /* ---- Transfer mode register (DMTMD) --------------------------------- *
     *
     *  Bits [15:14]  MD[1:0]  Transfer mode  00=Normal 01=Block 10=Repeat
     *  Bits [13:12]  DTS[1:0] Repeat area    00=dest   01=source (unused)
     *  Bits [11:10]  (reserved)
     *  Bits  [9: 8]  SZ[1:0]  Data size      00=8-bit  10=32-bit
     *  Bits  [7: 2]  DCTG[5:0] Activation    000000=SW 000001=INT …
     *  Bits  [1: 0]  (reserved)
     *
     * We build the register word manually for clarity.                    */
    {
        uint16_t dmtmd = 0U;

        /* Transfer mode */
        switch (p_cfg->transfer_mode) {
            case k_dmaca_mode_normal: dmtmd |= (0U << 14U); break;
            case k_dmaca_mode_block:  dmtmd |= (1U << 14U); break;
            case k_dmaca_mode_repeat: dmtmd |= (2U << 14U); break;
            default:                  dmtmd |= (1U << 14U); break; /* block */
        }

        /* Data size — only byte or longword accepted (validated above). */
        if (p_cfg->data_size == k_dmaca_size_longword) {
            dmtmd |= (2U << 8U);   /* SZ = 10b → 32-bit */
        } else {
            dmtmd |= (0U << 8U);   /* SZ = 00b → 8-bit  */
        }

        ch->DMTMD.WORD = dmtmd;
    }

    /* ---- Address mode register (DMAMD) ---------------------------------- *
     *
     *  Bits [15:14]  SM[1:0]  Source mode       00=fixed 10=inc 11=dec
     *  Bits [13:12]  SARA[1:0] Src repeat area  (unused in block mode)
     *  Bits  [9: 8]  DM[1:0]  Dest mode         00=fixed 10=inc 11=dec
     *  Bits  [5: 4]  DARA[1:0] Dst repeat area  (unused)             */
    {
        uint16_t dmamd = 0U;

        /* Source address mode */
        switch (p_cfg->src_addr_mode) {
            case k_dmaca_addr_fixed:     dmamd |= (0U << 14U); break;
            case k_dmaca_addr_increment: dmamd |= (2U << 14U); break;
            case k_dmaca_addr_decrement: dmamd |= (3U << 14U); break;
            default:                     dmamd |= (2U << 14U); break; /*increment*/
        }

        /* Destination address mode
         * CRITICAL: must be fixed (00b) when targeting CRCDIR.
         * Incrementing the destination would write to reserved memory. */
        dmamd |= (0U << 8U); // A switch case could have been used here, but it's better to ensure it will always be fixed.

        ch->DMAMD.WORD = dmamd;
    }

    /* ---- Interrupt / status register (DMINT) ---------------------------- *
     * Phase 1 uses polling; disable all DMA interrupts.                  */
    ch->DMINT.BYTE = 0x00U;

    /* ---- Clear any stale status flags (DMSTS) --------------------------- */
    ch->DMSTS.BYTE = 0x00U;

    /* Mark channel as configured. */
    s_channel_configured |= (uint8_t)(1U << channel);

    return k_rx_ok;
}

rx_err_t rx_dmaca_start(uint8_t channel)
{
    rx_err_t err;

    err = validate_channel(channel);
    if (err != k_rx_ok) {
        return err;
    }

    if ((s_channel_configured & (uint8_t)(1U << channel)) == 0U) {
        /* Channel has not been configured. */
        return k_rx_err_param;
    }

    volatile struct st_dmac0 *ch = dmac_ch(channel);

    /* Enable DMA transfer for this channel. */
    ch->DMCNT.BIT.DTE = 1;

    /* Issue software request (SWREQ pulse → triggers the block transfer). */
    ch->DMREQ.BIT.SWREQ = 1;

    return k_rx_ok;
}

rx_err_t rx_dmaca_wait(uint8_t channel, uint32_t timeout_cycles)
{
    rx_err_t err;

    err = validate_channel(channel);
    if (err != k_rx_ok) {
        return err;
    }

    volatile struct st_dmac0 *ch = dmac_ch(channel);

    /*
     * Bounded polling loop — NASA Rule 2 compliance.
     *
     * We poll the ACT bit (transfer active flag) in DMSTS.
     * ACT = 1: DMA is running or waiting for bus.
     * ACT = 0: Transfer complete (or not yet started).
     *
     * Secondary check: DTE auto-clears to 0 after block completion.
     *
     * The timeout counter counts loop iterations, not exact CPU cycles,
     * but each iteration reads a volatile peripheral register (≥ 4 cycles
     * at 60 MHz PCLKB / 240 MHz ICLK), so the bound is conservative.
     */
    uint32_t remaining = timeout_cycles;

    while (ch->DMSTS.BIT.ACT == 1U)
    {
        if (remaining == 0U) {
            /* Timeout: forcibly disable channel before returning. */
            ch->DMCNT.BIT.DTE = 0;
            s_channel_configured &= (uint8_t)(~(1U << channel));
            return k_rx_err_timeout;
        }
        remaining--;
    }

    /* Transfer ended normally — clear the configured flag. */
    s_channel_configured &= (uint8_t)(~(1U << channel));

    return k_rx_ok;
}

void rx_dmaca_abort(uint8_t channel)
{
    if (channel >= DMACA_NUM_CHANNELS) {
        return;
    }

    volatile struct st_dmac0 *ch = dmac_ch(channel);
    ch->DMCNT.BIT.DTE = 0;
    s_channel_configured &= (uint8_t)(~(1U << channel));
}

rx_err_t rx_dmaca_transfer_blocking(uint8_t channel, const dmaca_config_t *p_cfg)
{
    rx_err_t err;

    err = rx_dmaca_configure(channel, p_cfg);
    if (err != k_rx_ok) {
        return err;
    }

    err = rx_dmaca_start(channel);
    if (err != k_rx_ok) {
        return err;
    }

    /*
     * Scale timeout to the configured block size.
     *
     * Worst-case per-transfer: (Cr + Cw) = (1 + 4) = 5 PCLKB cycles
     * At 240/60 MHz (ICLK/PCLKB ratio = 4): 5 × 4 = 20 ICLK iterations
     * Add 25 % bus contention margin for EXDMAC priority preemption.
     *
     * Minimum floor = RX_DMACA_POLL_TIMEOUT_CYCLES to cover DMA startup
     * overhead and very small blocks where the formula underestimates.
     */
    uint32_t scaled = ((uint32_t)p_cfg->transfer_count * 20U * 5U) / 4U;
    uint32_t timeout = (scaled > RX_DMACA_POLL_TIMEOUT_CYCLES)
                       ? scaled
                       : RX_DMACA_POLL_TIMEOUT_CYCLES;

    return rx_dmaca_wait(channel, timeout);
}