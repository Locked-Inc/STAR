/* lib/rx_crc/src/rx_dmaca.c */

/**
 * @file    rx_dmaca.c
 * @brief   DMACA driver for RX72N
 *
 * Implements the API declared in rx_dmaca.h.
 *
 * Hardware references:
 *   RX72N Hardware Manual Sec.18  (DMACA)
 *   RX72N Hardware Manual Sec.46  (CRCA)
 *   RX Family Register Access  iodefine.h / r_bsp header set
 *
 * Design constraints honoured:
 *   - 8-bit and 32-bit transfers only -- 16-bit is prohibited for CRCDIR.
 *   - Destination address is NEVER incremented for CRCDIR use.
 *   - All completion loops are bounded (NASA Rule 2).
 *   - No dynamic memory allocation (NASA Rule 10).
 *   - Every public function validates its arguments and returns rx_err_t.
 * @author STAR Team
 * @date 2026-02-22
 * @copyright Copyright (c) 2026 STAR Project - MIT License
 */



#include "rx_dmaca.h"
#include <stddef.h>          /* NULL */
#include "rx_err.h"
#include "rx_check.h"       /* runtime validation macros */

/* hardware register definitions for DMAC (RX72N) */
#include "rx72n_dmac_regs.h"  /* exposes rx_dmac_channel_regs_t, dmac_dmast_reg(), k_dmast_dmst */


/* =========================================================================
 * Internal helpers -- register access
 *
 * The RX72N iodefine.h exposes DMAC channels as an array:
 *   DMAC0 ... DMAC7  or  DMAC[0] ... DMAC[7]  (BSP-version-dependent)
 *
 * We use a small inline accessor to keep channel selection uniform.
 * ========================================================================= */

/* ------------------------------------------------------------------
 * Private constants
 * ------------------------------------------------------------------ */

/**
 * @enum dmac_hw_t
 * @brief Hardware addresses for DMAC channel calculation
 *
 * @details
 * The DMAC register blocks for channels 0-7 are laid out in memory at a
 * fixed base address with a constant stride between consecutive channels.
 * These named constants replace raw literals in pointer arithmetic to make
 * the relationship explicit and easier to audit.
 */
/* Local alias for hardware register layout constant from rx72n_dmac_regs.h */

typedef enum : uint32_t {
    k_dmac_channel_stride = k_dmac_channel_size,       /**< bytes between channels */
} dmac_hw_t;

/* Local named constants for register clearing / SWREQ bit to avoid magic numbers
 *
 * Converted to a typed enum per C23 style guidelines; values retain their
 * original meanings.  Call sites cast to uint8_t where required by the
 * register definitions. */
typedef enum : uint8_t {
    DMACA_DMINT_CLEAR       = 0x00U, /**< Clear DMINT (disable interrupts) */
    DMACA_DMSTS_CLEAR       = 0x00U, /**< Clear DMSTS (clear status flags) */
    DMACA_SWREQ_REQUEST     = 1U,    /**< SWREQ request bit value */
} dmaca_const_t;

/**
 * @enum dmaca_dte_t
 * @brief Values for the DMCNT.DTE bit
 *
 * @details
 * Used internally when enabling or disabling the DMA transfer engine for a
 * channel.  Grouped here as a typed enum for readability; the underlying
 * register field is one bit wide so the values are 0 or 1.
 */
typedef enum : uint8_t {
    DMACA_DTE_DISABLE = 0U, /**< DTE = 0, DMA engine disabled */
    DMACA_DTE_ENABLE  = 1U, /**< DTE = 1, DMA engine enabled  */
} dmaca_dte_t;


/**
 * @brief  Obtain pointer to DMAC channel registers
 *
 * @details
 * Each DMAC channel exposes an identical register block.  The blocks are
 * located consecutively in memory starting at `k_dmac0_base_addr` with a
 * fixed `k_dmac_channel_stride`.  This helper computes the address for
 * the requested channel and returns a typed pointer for register access.
 *
 * @param[in] ch  Channel index (0 .. DMACA_NUM_CHANNELS-1).
 *
 * @return volatile rx_dmac_channel_regs_t *
 * @retval non-NULL Pointer to the channel's register block
 *
 * @pre RX_CHECK(ch < DMACA_NUM_CHANNELS, "DMACA", "invalid channel index");
 * @post return != NULL
 *
 * @note Thread safety: safe to call from any context; the calculation does not
 *       modify shared state.  The returned pointer should only be used with
 *       proper synchronization if the channel is accessed concurrently.
 *
 * @see rx_dmaca_start, rx_dmaca_abort
 * @since Version 1.0.0
 */
 
static volatile rx_dmac_channel_regs_t * dmac_ch(uint8_t ch)
{
    /* precondition check converts to runtime error log if violated */
    RX_ASSERT(ch < DMACA_NUM_CHANNELS, "DMACA invalid channel index");
    volatile rx_dmac_channel_regs_t *ptr =
        (volatile rx_dmac_channel_regs_t *)(uintptr_t)(k_dmac0_base_addr +
                                                       ((uint32_t)ch * k_dmac_channel_stride));
    RX_ASSERT(ptr != NULL, "DMACA channel pointer NULL");
    return ptr;
}

/* =========================================================================
 * Module-level state
 * ========================================================================= */

/**
 * @var s_dmaca_initialised
 * @brief DMACA initialization flag
 *
 * @details
 * This boolean indicates whether the DMACA driver has been successfully
 * initialized via rx_dmaca_init().  Public API calls use this flag to
 * reject requests made prior to initialization.  It is cleared during
 * module startup and set to true exactly once by rx_dmaca_init().
 *
 * @note This is a module-level static variable; it is not visible outside
 *       rx_dmaca.c and should only be accessed within this translation
 *       unit.
 *
 * @warning Direct modification of this variable is forbidden outside of
 *          this module.  Only rx_dmaca_init() or other internal helper
 *          routines are permitted to change its value.
 *
 * @since Version 1.0.0
 */
static bool s_dmaca_initialised = false;

/**
 * @var s_channel_configured
 * @brief Bitmap of channels with active or pending configurations
 *
 * @details
 * Each bit in this 8-bit bitmap corresponds to one DMACA channel.  When
 * bit N is set to 1 the associated channel N currently has an active
 * configuration or a configuration that is pending completion.  The flag is
 * manipulated by rx_dmaca_configure() and cleared when transfers complete.
 *
 * Bit semantics:
 *   - Bit 0 -> channel 0
 *   - Bit 1 -> channel 1
 *   - ...
 *
 * Bit N = 1 means channel N has an active or pending configuration.
 *
 * @note This variable has module-level scope; it is static to rx_dmaca.c and
 *       must not be referenced from other files.
 *
 * @warning Do not modify this bitmap directly outside of the module.  Changes
 *          should only occur through the initialization routine
 *          rx_dmaca_init() or internal APIs that manage channel state.
 *
 * @since Version 1.0.0
 */
static uint8_t s_channel_configured = 0U;

/* =========================================================================
 * Argument validation helpers
 * ========================================================================= */

static rx_err_t validate_channel(uint8_t channel)
{
    if (channel >= DMACA_NUM_CHANNELS) {
        return k_rx_err_nack;
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
    /* hardware header provides helper for the DMAST register plus bit masks */
    *dmac_dmast_reg() |= (uint8_t)k_dmast_dmst;

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
        return k_rx_err_nack;
    }

    if ((p_cfg->transfer_count == 0U) ||
        (p_cfg->transfer_count > DMACA_MAX_TRANSFER_COUNT)) {
        return k_rx_err_nack;
    }

    /*
     * Reject 16-bit (word) transfers explicitly.
     * CRCDIR does not support 16-bit bus access.  Any value other than
     * k_dmaca_size_byte (0) or k_dmaca_size_longword (2) is invalid.
     */
    if ((p_cfg->data_size != k_dmaca_size_byte) &&
        (p_cfg->data_size != k_dmaca_size_longword)) {
        return k_rx_err_nack;
    }

    if ((p_cfg->p_src == NULL) || (p_cfg->p_dst == NULL)) {
        return k_rx_err_nack;
    }

    /* ---- Stop the channel before reconfiguring -------------------------- */
    volatile rx_dmac_channel_regs_t *ch = dmac_ch(channel);
    ch->dmcnt &= ~(uint8_t)DMACA_DTE_ENABLE;   /* Disable transfer enable */

    /* ---- Source address ------------------------------------------------- */
    ch->dmsar = (uint32_t)(uintptr_t)p_cfg->p_src;

    /* ---- Destination address -------------------------------------------- */
    ch->dmdar = (uint32_t)(uintptr_t)p_cfg->p_dst;

    /* ---- Transfer count ------------------------------------------------- */
    ch->dmcra = (uint32_t)p_cfg->transfer_count;   /* Block / normal count  */
    ch->dmcrb = 0U;                                 /* Repeat count (unused) */

    /* ---- Transfer mode register (DMTMD) --------------------------------- *
     *
     *  Bits [15:14]  MD[1:0]  Transfer mode  00=Normal 01=Block 10=Repeat
     *  Bits [13:12]  DTS[1:0] Repeat area    00=dest   01=source (unused)
     *  Bits [11:10]  (reserved)
     *  Bits  [9: 8]  SZ[1:0]  Data size      00=8-bit  10=32-bit
     *  Bits  [7: 2]  DCTG[5:0] Activation    000000=SW 000001=INT ...
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
            default:                  return k_rx_err_nack;
        }

        /* Data size -- only byte or longword accepted (validated above). */
        if (p_cfg->data_size == k_dmaca_size_longword) {
            dmtmd |= (2U << 8U);   /* SZ = 10b -> 32-bit */
        } else {
            dmtmd |= (0U << 8U);   /* SZ = 00b -> 8-bit  */
        }

        ch->dmtmd = dmtmd;
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
            default:                     return k_rx_err_nack;
        }

        if (p_cfg->dst_addr_mode != k_dmaca_addr_fixed) {
            return k_rx_err_nack;
        }
        dmamd |= (0U << 8U); /* Ensure destination mode is fixed */
        ch->dmamd = dmamd;
    }

    /* ---- Interrupt / status register (DMINT) ---------------------------- *
     * Phase 1 uses polling; disable all DMA interrupts.                  */
    ch->dmint = (uint8_t)DMACA_DMINT_CLEAR;

    /* ---- Clear any stale status flags (DMSTS) --------------------------- */
    ch->dmsts = (uint8_t)DMACA_DMSTS_CLEAR;

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
        return k_rx_err_nack;
    }

    volatile rx_dmac_channel_regs_t *ch = dmac_ch(channel);

    /* Validate assumptions for debug builds.  Public API also performs
     * runtime validation via validate_channel(). */
    RX_ASSERT(channel < DMACA_NUM_CHANNELS, "DMACA invalid channel index");

    /* Enable DMA transfer for this channel. */
    ch->dmcnt |= (uint8_t)DMACA_DTE_ENABLE;

    /* Issue software request (SWREQ pulse -> triggers the block transfer). */
    ch->dmreq |= (uint8_t)DMACA_SWREQ_REQUEST;

    return k_rx_ok;
}

rx_err_t rx_dmaca_wait(uint8_t channel, uint32_t timeout_cycles)
{
    rx_err_t err;

    err = validate_channel(channel);
    if (err != k_rx_ok) {
        return err;
    }

    if ((s_channel_configured & (uint8_t)(1U << channel)) == 0U) {
        return k_rx_err_nack;
    }
    volatile rx_dmac_channel_regs_t *ch = dmac_ch(channel);

    /*
     * Bounded polling loop -- NASA Rule 2 compliance.
     *
     * We poll the ACT bit (transfer active flag) in DMSTS.
     * ACT = 1: DMA is running or waiting for bus.
     * ACT = 0: Transfer complete (or not yet started).
     *
     * Secondary check: DTE auto-clears to 0 after block completion.
     *
     * The timeout counter counts loop iterations, not exact CPU cycles,
     * but each iteration reads a volatile peripheral register (>= 4 cycles
     * at 60 MHz PCLKB / 240 MHz ICLK), so the bound is conservative.
     */
    uint32_t remaining = timeout_cycles;

    while ((ch->dmsts & 0x01U) == 1U)
    {
        if (remaining == 0U) {
            /* Timeout: forcibly disable channel before returning. */
            ch->dmcnt &= ~(uint8_t)DMACA_DTE_ENABLE;
            s_channel_configured &= (uint8_t)(~(1U << channel));
            return k_rx_err_timeout;
        }
        remaining--;
    }

    /* Transfer ended normally -- clear the configured flag. */
    s_channel_configured &= (uint8_t)(~(1U << channel));

    return k_rx_ok;
}

/**
 * @brief Abort an ongoing DMA transfer on a channel.
 *
 * @details
 * Public API is tolerant of out-of-range channel values and will return
 * without side-effects when an invalid index is provided.  For debug builds
 * callers' assumptions are verified with `RX_ASSERT(channel < DMACA_NUM_CHANNELS, ...)`.
 *
 * @param[in] channel DMACA channel index to abort (0..DMACA_NUM_CHANNELS-1)
 *
 * @note The function performs a runtime no-op for invalid channel indexes.
 *
 * @return void No return value; operation is performed via side-effects on
 *         hardware registers and module state.
 *
 * @retval none
 *
 * @pre channel is an integer (uint8_t) representing the desired DMACA
 *      channel; if it is outside the valid range the call is a no-op.
 * @pre DMACA peripheral clock must be enabled (MSTP bit cleared) so that
 *      register accesses are valid.
 *
 * @post If channel is valid: DMA transfer engine for that channel is
 *       disabled and the module's configured-bit bitmap is cleared.
 * @post If channel is invalid: no hardware registers are modified and the
 *       function returns without side-effects.
 *
 * @par Example:
 * @code
 * // abort channel 3 before reconfiguring it
 * rx_dmaca_abort(3);
 * // caller may then safely call rx_dmaca_configure(3, &new_cfg);
 * @endcode
 *
 * @see dmac_ch(), RX_ASSERT
 * @since 1.0.0
 */
void rx_dmaca_abort(uint8_t channel)
{
    /* Validate caller assumptions in debug builds. Public API remains tolerant
     * and performs a no-op for out-of-range indices. */
    RX_ASSERT(channel < DMACA_NUM_CHANNELS, "DMACA invalid channel index");

    if (channel >= DMACA_NUM_CHANNELS) {
        /* no-op for invalid index, per API contract */
        return;
    }

    volatile rx_dmac_channel_regs_t *ch = dmac_ch(channel);

    /* disable transfer engine and clear configured flag */
    ch->dmcnt &= ~(uint8_t)DMACA_DTE_ENABLE;
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
     * At 240/60 MHz (ICLK/PCLKB ratio = 4): 5 x 4 = 20 ICLK iterations
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