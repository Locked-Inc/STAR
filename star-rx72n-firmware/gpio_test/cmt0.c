/**
 * @file cmt0.c
 * @brief CMT0 100 Hz tick source for ThreadX with PCKB = 48 MHz.
 *
 * @details
 * After clock_init() PCKB = 48 MHz. PCKB/32 = 1.5 MHz, divided by 15000
 * gives 100 Hz exactly. CMCOR = 14999, CMCR.CKS = 01 (PCKB/32), CMIE = 1.
 *
 * Vector 28 (CMT0 CMI0) is wired in vectors.S to cmt0_isr below, which is
 * marked __attribute__((interrupt)) so GCC generates the RTE epilogue.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

extern void _tx_timer_interrupt(void);

typedef enum : uintptr_t {
    k_cmstr0_addr = 0x00088000U,
    k_cmt0_cmcr   = 0x00088002U,
    k_cmt0_cmcnt  = 0x00088004U,
    k_cmt0_cmcor  = 0x00088006U,

    k_icu_ir_base  = 0x00087000U,
    k_icu_ier_base = 0x00087200U,
    k_icu_ipr_base = 0x00087300U,

    k_mstpcra_addr = 0x00080010U,
    k_prcr_addr    = 0x000803FEU,
} cmt_addr_t;

typedef enum : uint16_t {
    k_cmt0_cmcor_val        = 14999U,
    k_cmt0_cmcr_val         = 0x0041U,
    k_cmstr0_ch0_bit        = 0x0001U,
    k_prcr_unlock_prc0_prc1 = 0xA503U,
    k_prcr_lock             = 0xA500U,
} cmt0_cfg_t;

typedef enum : uint8_t {
    k_vect_cmt0        = 28U,
    k_cmt0_priority    = 5U,
    k_icu_ier_cmt0     = 3U,
    k_icu_ier_cmt0_bit = 4U,
    k_mstpa_cmt0_bit   = 15U,
} icu_cmt0_cfg_t;

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

void __attribute__((interrupt)) cmt0_isr(void)
{
    *icu_ir(k_vect_cmt0) = 0U;
    _tx_timer_interrupt();
}

void cmt0_init(void)
{
    /* Release CMT0/CMT1 from module stop */
    *prcr() = k_prcr_unlock_prc0_prc1;
    *mstpcra() &= ~(uint32_t)(1UL << k_mstpa_cmt0_bit);
    *prcr() = k_prcr_lock;

    *cmstr0() &= (uint16_t) ~(uint16_t)k_cmstr0_ch0_bit;

    *cmt0_cmcr()  = k_cmt0_cmcr_val;
    *cmt0_cmcor() = k_cmt0_cmcor_val;
    *cmt0_cmcnt() = 0U;

    *icu_ir(k_vect_cmt0)  = 0U;
    *icu_ipr(k_vect_cmt0) = k_cmt0_priority;
    *icu_ier(k_icu_ier_cmt0) |= (uint8_t)(1U << k_icu_ier_cmt0_bit);

    *cmstr0() |= k_cmstr0_ch0_bit;
    __asm__ volatile("setpsw i");
}
