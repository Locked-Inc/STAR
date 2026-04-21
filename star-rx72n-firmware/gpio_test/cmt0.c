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
 * Register and bit references follow the RX72N Hardware Manual
 * (R01UH0824EJ0111), chapters 13 (ICU), 15 (CMT), and 11 (module stop).
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

extern void _tx_timer_interrupt(void);

/**
 * @enum cmt_addr_t
 * @brief Absolute hardware register addresses used by the CMT0 tick setup.
 */
typedef enum : uintptr_t {
    k_cmstr0_addr  = 0x00088000U, /**< CMSTR0: shared CMT0/CMT1 start register (Ch 15) */
    k_cmt0_cmcr    = 0x00088002U, /**< CMT0 CMCR: control register (Ch 15) */
    k_cmt0_cmcnt   = 0x00088004U, /**< CMT0 CMCNT: counter value (Ch 15) */
    k_cmt0_cmcor   = 0x00088006U, /**< CMT0 CMCOR: compare-match target (Ch 15) */

    k_icu_ir_base  = 0x00087000U, /**< ICU.IR[] base: IR[vect] at +vect (Ch 13) */
    k_icu_ier_base = 0x00087200U, /**< ICU.IER[] base: IER[vect/8] bit (vect%8) (Ch 13) */
    k_icu_ipr_base = 0x00087300U, /**< ICU.IPR[] base: IPR[vect] priority (Ch 13) */

    k_mstpcra_addr = 0x00080010U, /**< MSTPCRA: module stop control A (Ch 11) */
    k_prcr_addr    = 0x000803FEU, /**< PRCR: protect register for clock/module stop writes */
} cmt_addr_t;

/**
 * @enum cmt0_cfg_t
 * @brief 16-bit configuration values written to CMT0 and PRCR.
 */
typedef enum : uint16_t {
    k_cmt0_cmcor_val        = 14999U,  /**< (48 MHz / 32) / 100 Hz - 1 = 14999 for 100 Hz tick */
    k_cmt0_cmcr_val         = 0x0041U, /**< CMCR: CMIE=1 (interrupt), CKS=01 (PCKB/32) */
    k_cmstr0_ch0_bit        = 0x0001U, /**< CMSTR0 bit 0: CMT0 start/stop */
    k_prcr_unlock_prc0_prc1 = 0xA503U, /**< Write PRCR=0xA503 to enable PRC0+PRC1 writes */
    k_prcr_lock             = 0xA500U, /**< Write PRCR=0xA500 to relock PRC0+PRC1 */
} cmt0_cfg_t;

/**
 * @enum icu_cmt0_cfg_t
 * @brief ICU vector/priority/bit constants for CMT0 CMI0.
 */
typedef enum : uint8_t {
    k_vect_cmt0        = 28U, /**< ICU vector number for CMT0 CMI0 (Ch 13) */
    k_cmt0_priority    = 5U,  /**< IPR priority (0..15); 5 is low-enough for ThreadX tick */
    k_icu_ier_cmt0     = 3U,  /**< IER register index for vector 28 (28/8 = 3) */
    k_icu_ier_cmt0_bit = 4U,  /**< Bit position within IER[3] (28 % 8 = 4) */
    k_mstpa_cmt0_bit   = 15U, /**< MSTPCRA bit 15: combined CMT0/CMT1 module stop */
} icu_cmt0_cfg_t;

/** @brief Typed accessor for CMSTR0 (CMT0/1 start register). */
static inline volatile uint16_t *internal_cmstr0(void)
{
    return (volatile uint16_t *)k_cmstr0_addr;
}

/** @brief Typed accessor for CMT0.CMCR. */
static inline volatile uint16_t *internal_cmt0_cmcr(void)
{
    return (volatile uint16_t *)k_cmt0_cmcr;
}

/** @brief Typed accessor for CMT0.CMCNT. */
static inline volatile uint16_t *internal_cmt0_cmcnt(void)
{
    return (volatile uint16_t *)k_cmt0_cmcnt;
}

/** @brief Typed accessor for CMT0.CMCOR. */
static inline volatile uint16_t *internal_cmt0_cmcor(void)
{
    return (volatile uint16_t *)k_cmt0_cmcor;
}

/**
 * @brief Typed accessor for ICU.IR[vect] (one-byte interrupt request flag).
 * @param[in] vect ICU vector number (0..255).
 * @return Volatile pointer to IR[vect].
 */
static inline volatile uint8_t *internal_icu_ir(uint8_t vect)
{
    return (volatile uint8_t *)(k_icu_ir_base + vect);
}

/**
 * @brief Typed accessor for ICU.IER[idx] (vect/8 selects the byte).
 * @param[in] idx IER register index (vect / 8).
 * @return Volatile pointer to IER[idx].
 */
static inline volatile uint8_t *internal_icu_ier(uint8_t idx)
{
    return (volatile uint8_t *)(k_icu_ier_base + idx);
}

/**
 * @brief Typed accessor for ICU.IPR[vect] (one-byte priority).
 * @param[in] vect ICU vector number (0..255).
 * @return Volatile pointer to IPR[vect].
 */
static inline volatile uint8_t *internal_icu_ipr(uint8_t vect)
{
    return (volatile uint8_t *)(k_icu_ipr_base + vect);
}

/** @brief Typed accessor for MSTPCRA (module stop control A). */
static inline volatile uint32_t *internal_mstpcra(void)
{
    return (volatile uint32_t *)k_mstpcra_addr;
}

/** @brief Typed accessor for PRCR (clock/module-stop write protect). */
static inline volatile uint16_t *internal_prcr(void)
{
    return (volatile uint16_t *)k_prcr_addr;
}

/**
 * @brief CMT0 CMI0 ISR: clears IR and drives the ThreadX tick.
 *
 * @details
 * Wired by `vectors.S` into slot 28 of the relocatable vector table.
 * Marked `__attribute__((interrupt))` so GCC-RX emits the RTE prologue
 * and epilogue around the body.
 *
 * @pre Interrupts are globally enabled and this vector is wired in INTB.
 * @post `_tx_timer_interrupt()` has advanced the ThreadX system clock by
 *       exactly one tick.
 * @note Executes in ISR context on the ISP.
 */
void __attribute__((interrupt)) cmt0_isr(void)
{
    *internal_icu_ir(k_vect_cmt0) = 0U;
    _tx_timer_interrupt();
}

/**
 * @brief Program CMT0 for a 100 Hz tick and enable its interrupt.
 *
 * @details
 * 1. Releases CMT0 (and CMT1) from module-stop via MSTPCRA bit 15,
 *    bracketed by the PRCR unlock/relock protocol.
 * 2. Stops CMT0, programs CMCR/CMCOR, clears CMCNT.
 * 3. Clears any pending IR, sets IPR=5, enables the vector in IER.
 * 4. Starts CMT0 and globally enables interrupts with `SETPSW I`.
 *
 * @pre `clock_init()` has completed so PCKB = 48 MHz.
 * @pre Called from `main()` before `tx_kernel_enter()`.
 * @post CMT0 fires at exactly 100 Hz into `cmt0_isr`.
 * @post Global interrupt flag (PSW.I) is set.
 */
void cmt0_init(void)
{
    /* Release CMT0/CMT1 from module stop (MSTPCRA bit 15 = 0) under PRCR protection. */
    *internal_prcr()    = k_prcr_unlock_prc0_prc1;
    *internal_mstpcra() &= ~(uint32_t)(1UL << k_mstpa_cmt0_bit);
    *internal_prcr()    = k_prcr_lock;

    /* Stop CMT0 before reprogramming. */
    *internal_cmstr0() &= (uint16_t) ~(uint16_t)k_cmstr0_ch0_bit;

    *internal_cmt0_cmcr()  = k_cmt0_cmcr_val;
    *internal_cmt0_cmcor() = k_cmt0_cmcor_val;
    *internal_cmt0_cmcnt() = 0U;

    *internal_icu_ir(k_vect_cmt0)          = 0U;
    *internal_icu_ipr(k_vect_cmt0)         = k_cmt0_priority;
    *internal_icu_ier(k_icu_ier_cmt0)     |= (uint8_t)(1U << k_icu_ier_cmt0_bit);

    *internal_cmstr0() |= k_cmstr0_ch0_bit;

    /* Inline asm: RX has no standalone GCC builtin for SETPSW; this is the
     * idiomatic way to set PSW.I (global interrupt enable) at runtime. */
    __asm__ volatile("setpsw i");
}
