/**
 * @file cmt0.c
 * @brief CMT0 ThreadX tick source for the PLL-clocked motors_rtos test.
 *
 * @details
 * Drives ThreadX's 100 Hz system tick via CMT0 CMI0 (vector 28).  This
 * variant targets the PLL clock tree (PCLKB = 60 MHz), not the LOCO
 * default used by blinky_rtos -- motors_rtos calls clock_init() before
 * tx_kernel_enter() so CMT0 sees the full 60 MHz PCLKB.
 *
 * Math: PCLKB / 32 = 60 MHz / 32 = 1.875 MHz; CMCOR = 18749 -> 100 Hz.
 *
 * The ISR must:
 *   1. Clear the ICU IR flag (otherwise the IRQ re-fires immediately).
 *   2. Call _tx_timer_interrupt() so ThreadX advances its clock.
 *
 * vectors.S wires slot 28 to `_cmt0_isr` -- GCC prefixes this file's
 * cmt0_isr symbol with an underscore on link, matching that reference.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

/* ==========================================================================
 * Register addresses (RX72N HW manual R01UH0824EJ0111, Ch 15 CMT + Ch 13 ICU)
 * ========================================================================== */
typedef enum : uintptr_t {
    k_cmstr0_addr = 0x00088000U, /**< CMT start register (CH0/CH1 share) */
    k_cmt0_cmcr   = 0x00088002U, /**< CMT0 control register */
    k_cmt0_cmcnt  = 0x00088004U, /**< CMT0 counter */
    k_cmt0_cmcor  = 0x00088006U, /**< CMT0 compare match register */

    k_icu_ir_base  = 0x00087000U,
    k_icu_ier_base = 0x00087200U,
    k_icu_ipr_base = 0x00087300U,

    k_mstpcra_addr = 0x00080010U, /**< MSTPCRA (bit 15 = CMT0/CMT1 unit) */
    k_prcr_addr    = 0x000803FEU, /**< PRCR protection register */
} cmt0_hw_addr_t;

typedef enum : uint16_t {
    /*
     * CMCR layout:
     *   bits[1:0] CKS : 00=PCLK/8, 01=PCLK/32, 10=PCLK/128, 11=PCLK/512
     *   bit [6]   CMIE: 0=disabled, 1=enabled
     */
    k_cmt0_cmcr_val  = 0x0041U, /**< CKS=01 (PCLKB/32), CMIE=1 */
    k_cmt0_cmcor_val = 18749U,  /**< 60 MHz / 32 / 100 Hz - 1 */
    k_cmstr0_ch0_bit = 0x0001U,
} cmt0_cfg_t;

typedef enum : uint8_t {
    k_vect_cmt0         = 28U,
    k_cmt0_priority     = 3U,
    k_icu_ier_cmt0      = 3U, /**< IER[28 / 8] = IER[3] */
    k_icu_ier_cmt0_bit  = 4U, /**< 28 % 8 = 4 */
    k_mstpcra_cmt0_bit  = 15U,
} cmt0_icu_cfg_t;

typedef enum : uint16_t {
    k_prcr_unlock_prc1 = 0xA502U, /**< PRKEY=A5, PRC1=1 (CGC access) */
    k_prcr_lock        = 0xA500U,
} cmt0_prcr_t;

/* ==========================================================================
 * Register accessors
 * ========================================================================== */
static inline volatile uint16_t *cmstr0(void)    { return (volatile uint16_t *)k_cmstr0_addr; }
static inline volatile uint16_t *cmt0_cmcr(void) { return (volatile uint16_t *)k_cmt0_cmcr; }
static inline volatile uint16_t *cmt0_cmcnt(void){ return (volatile uint16_t *)k_cmt0_cmcnt; }
static inline volatile uint16_t *cmt0_cmcor(void){ return (volatile uint16_t *)k_cmt0_cmcor; }

static inline volatile uint8_t *icu_ir(uint8_t vect)
{
    return (volatile uint8_t *)(k_icu_ir_base + vect);
}
static inline volatile uint8_t *icu_ier(uint8_t idx)
{
    return (volatile uint8_t *)(k_icu_ier_base + idx);
}
static inline volatile uint8_t *icu_ipr(uint8_t vect)
{
    return (volatile uint8_t *)(k_icu_ipr_base + vect);
}

static inline volatile uint32_t *mstpcra(void)
{
    return (volatile uint32_t *)k_mstpcra_addr;
}
static inline volatile uint16_t *prcr(void)
{
    return (volatile uint16_t *)k_prcr_addr;
}


/* ==========================================================================
 * CMT0 ISR (wired at vectors.S slot 28).  GCC's __attribute__((interrupt))
 * generates the RTE-terminated prologue/epilogue the RX expects.
 * ========================================================================== */
/* Exposed to main.c for the diagnostic CSV column -- counts how many times
 * the CMT0 compare-match ISR has fired.  Incremented by the assembly ISR
 * in vectors.S (which wraps _tx_timer_interrupt in context_save/restore so
 * that timer-based task wakeups actually cause a context switch).  See the
 * `_cmt0_isr:` block in vectors.S for the full ISR implementation. */
volatile uint32_t g_cmt0_isr_count = 0U;

/* ==========================================================================
 * cmt0_init -- call once, before tx_kernel_enter().
 * ========================================================================== */
void cmt0_init(void)
{
    /* Release CMT0 from module stop (MSTPCRA bit 15 controls the CMT0/CMT1 unit). */
    *prcr() = k_prcr_unlock_prc1;
    *mstpcra() &= ~(uint32_t)(1UL << (uint32_t)k_mstpcra_cmt0_bit);
    *prcr() = k_prcr_lock;

    /* Stop CMT0, configure, then start. */
    *cmstr0()    &= (uint16_t)~(uint16_t)k_cmstr0_ch0_bit;
    *cmt0_cmcr()  = (uint16_t)k_cmt0_cmcr_val;
    *cmt0_cmcor() = (uint16_t)k_cmt0_cmcor_val;
    *cmt0_cmcnt() = 0U;

    /* Direct-address writes -- bypass any macro/accessor doubt.  Addresses
     * verified against libs/rx_hal/inc/rx72n_icu_regs.h: IR/IER/IPR are
     * uint8_t arrays at offsets 0x000/0x200/0x300 from ICU base 0x00087000.
     *   IR[28]  = 0x0008_701C
     *   IER[3]  = 0x0008_7203, bit 4 = vector 28
     *   IPR[28] = 0x0008_731C
     * Priority raised from 3 to 10 to match production (rx72n_icu_regs.h
     * example doc). */
    /* ICU setup for CMT0_CMI0 (vector 28).  Direct-address writes document
     * each register explicitly; indices verified against
     * libs/rx_hal/inc/rx72n_icu_regs.h (@0x000 IR, @0x200 IER, @0x300 IPR).
     *
     * IMPORTANT -- RX72N IPR INDEX IS *NOT* THE VECTOR NUMBER.
     * The HAL's example code `icu_regs->ipr[28] = 10` at
     * rx72n_icu_regs.h:318 is WRONG for CMT0_CMI0.  IPR[] on RX72N is
     * sparse-mapped per a hardware lookup table in the RX72N HW manual
     * section 14 (ICU).  Vector 28 (CMT0_CMI0) maps to **IPR[4]**, not
     * IPR[28].  Writes to IPR[28] (0x8731C) are silently dropped -- reads
     * return 0 regardless of what was written.  Confirmed empirically:
     * writing 10 to IPR[4] made the ISR fire (g_cmt0_isr_count advanced
     * from 0 to 604+ in ~6 sec); writing the same to IPR[28] had zero
     * effect.  See also eclipse-threadx/rtos-docs and Renesas community
     * threads referenced in the matching RCA session notes.
     *
     * We intentionally keep BOTH writes for now: IPR[4] is the real
     * priority register; IPR[28] is a no-op we leave in so future readers
     * can trivially see the wrong-address trap that cost us a session,
     * with this comment serving as the "why".  Remove the IPR[28] write
     * after this test is known-stable on hardware. */
    *(volatile uint8_t *)0x0008701CU = 0U;   /* IR[28]  -- clear pending */
    *(volatile uint8_t *)0x00087304U = 10U;  /* IPR[4]  -- real priority register for vector 28 on RX72N */
    *(volatile uint8_t *)0x0008731CU = 10U;  /* IPR[28] -- NO-OP, intentionally kept as a tombstone (see comment above) */
    *(volatile uint8_t *)0x00087203U |= (uint8_t)(1U << 4); /* IER[3] bit 4 enable */

    *cmstr0() |= (uint16_t)k_cmstr0_ch0_bit;

    /* Globally enable interrupts -- ThreadX expects PSW.I = 1 before kernel entry. */
    __asm__ volatile("setpsw i");
}
