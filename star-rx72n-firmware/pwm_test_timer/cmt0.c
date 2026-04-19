/**
 * @file cmt0.c
 * @brief CMT0 100 Hz tick source with user-defined hook; PCKB = 48 MHz.
 *
 * @details
 * After clock_init() PCKB = 48 MHz.  PCKB/32 = 1.5 MHz, divided by 15000
 * = 100 Hz exactly.  CMCOR = 14999, CMCR.CKS = 01 (PCKB/32), CMIE = 1.
 *
 * Vector 28 (CMT0 CMI0) is wired in vectors.S to cmt0_isr below, which is
 * marked __attribute__((interrupt)) so GCC generates the RTE epilogue.
 *
 * The ISR calls cmt0_tick_hook() every tick. The app (main.c) provides a
 * strong override of this weak default to advance the PWM duty sweep.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

/* Weak default tick hook -- main.c provides a strong override. */
__attribute__((weak)) void cmt0_tick_hook(void) { }

/* ==========================================================================
 * Hardware addresses (CMT0 / ICU / module stop)
 * ========================================================================== */
typedef enum : uintptr_t {
    k_cmstr0_addr = 0x00088000U, /**< CMT start register (shared CH0/CH1) */
    k_cmt0_cmcr   = 0x00088002U, /**< CMT0 control register */
    k_cmt0_cmcnt  = 0x00088004U, /**< CMT0 counter */
    k_cmt0_cmcor  = 0x00088006U, /**< CMT0 compare match register */

    k_icu_ir_base  = 0x00087000U,
    k_icu_ier_base = 0x00087200U,
    k_icu_ipr_base = 0x00087300U,

    k_mstpcra_addr = 0x00080010U, /**< Module stop control register A */
    k_prcr_addr    = 0x000803FEU, /**< Protection register */
} cmt_addr_t;

/* ==========================================================================
 * CMT0 configuration (PCKB = 48 MHz)
 * ========================================================================== */
typedef enum : uint16_t {
    k_cmt0_cmcor_val = 14999U,  /**< 48 MHz / 32 / 100 Hz - 1 */
    k_cmt0_cmcr_val  = 0x0041U, /**< CMIE=1, CKS=01 (PCKB/32) */
    k_cmstr0_ch0_bit = 0x0001U,

    k_prcr_unlock_prc0_prc1 = 0xA503U,
    k_prcr_lock             = 0xA500U,
} cmt0_cfg_t;

typedef enum : uint8_t {
    k_vect_cmt0       = 28U,
    k_cmt0_priority   = 5U,
    k_icu_ier_cmt0    = 3U,   /**< IER index for vector 28 (28/8) */
    k_icu_ier_cmt0_bit = 4U,  /**< Bit position within IER[3] (28%8) */
    k_mstpa_cmt0_bit  = 15U,  /**< MSTPCRA bit 15 = CMT0/CMT1 */
} icu_cmt0_cfg_t;

/* ==========================================================================
 * Inline accessors
 * ========================================================================== */
static inline volatile uint16_t *cmstr0(void)
{
    return (volatile uint16_t *)k_cmstr0_addr;
}
static inline volatile uint16_t *cmt0_cmcr(void)
{
    return (volatile uint16_t *)k_cmt0_cmcr;
}
static inline volatile uint16_t *cmt0_cmcnt(void)
{
    return (volatile uint16_t *)k_cmt0_cmcnt;
}
static inline volatile uint16_t *cmt0_cmcor(void)
{
    return (volatile uint16_t *)k_cmt0_cmcor;
}
static inline volatile uint8_t *icu_ir(uint8_t v)
{
    return (volatile uint8_t *)(k_icu_ir_base + v);
}
static inline volatile uint8_t *icu_ier(uint8_t i)
{
    return (volatile uint8_t *)(k_icu_ier_base + i);
}
static inline volatile uint8_t *icu_ipr(uint8_t v)
{
    return (volatile uint8_t *)(k_icu_ipr_base + v);
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
 * CMT0 ISR -- calls the app-defined tick hook at 100 Hz.
 * ========================================================================== */
void __attribute__((interrupt)) cmt0_isr(void)
{
    *icu_ir(k_vect_cmt0) = 0U;
    cmt0_tick_hook();
}

/* ==========================================================================
 * cmt0_init -- enable CMT0 module and arm the 100 Hz tick.
 * Called from main() after clock_init() and before tx_kernel_enter().
 * ========================================================================== */
void cmt0_init(void)
{
    /* Release CMT0/CMT1 from module stop */
    *prcr() = k_prcr_unlock_prc0_prc1;
    *mstpcra() &= ~(uint32_t)(1UL << k_mstpa_cmt0_bit);
    *prcr() = k_prcr_lock;

    /* Stop CMT0 before configuring */
    *cmstr0() &= (uint16_t) ~(uint16_t)k_cmstr0_ch0_bit;

    /* Program counter, compare value, and control register */
    *cmt0_cmcr()  = k_cmt0_cmcr_val;
    *cmt0_cmcor() = k_cmt0_cmcor_val;
    *cmt0_cmcnt() = 0U;

    /* Wire up the ICU: clear pending, set priority, enable */
    *icu_ir(k_vect_cmt0)  = 0U;
    *icu_ipr(k_vect_cmt0) = k_cmt0_priority;
    *icu_ier(k_icu_ier_cmt0) |= (uint8_t)(1U << k_icu_ier_cmt0_bit);

    /* Start CMT0 */
    *cmstr0() |= k_cmstr0_ch0_bit;

    /* Globally enable interrupts */
    __asm__ volatile("setpsw i");
}
