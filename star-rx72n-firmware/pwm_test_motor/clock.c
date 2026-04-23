/**
 * @file clock.c
 * @brief RX72N clock initialisation: HOCO 16 MHz -> PLL 192 -> ICLK 96 / PCKB 48.
 *
 * @details
 * This test sources the PLL from the internal HOCO (16 MHz) instead of
 * the STAR PCB's 24 MHz external crystal on MOSC/EXTAL. The choice is
 * deliberate (isolate the PLL bring-up from crystal startup), not because
 * the board lacks a crystal -- it has one.
 *
 * Target clocks:
 *   - PLL   = HOCO 16 MHz x 12 = 192 MHz
 *   - ICLK  = 192 / 2  =  96 MHz  (CPU)
 *   - PCKA  = 192 / 2  =  96 MHz
 *   - PCKB  = 192 / 4  =  48 MHz  (CMT0 ThreadX tick)
 *   - PCKC  = 192 / 2  =  96 MHz
 *   - PCKD  = 192 / 2  =  96 MHz
 *   - FCLK  = 192 / 4  =  48 MHz  (flash)
 *   - BCLK  = 192 / 4  =  48 MHz
 *   - UCLK  = 192 / 4  =  48 MHz  (not used, but set for completeness)
 *
 * Path: HOCO 16 MHz -> PLIDIV /1 -> STC x12 -> 192 MHz PLL output.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

#include "rx72n_regs.h"
#include "rx_register_protection.h"

/* ==========================================================================
 * Register addresses not exposed by the rx_system_regs_t struct
 * ========================================================================== */
typedef enum : uintptr_t {
    k_packcr_addr = 0x00080044U, /**< Peripheral clock source: 0=PLL, 1=PPLL */
} clock_extra_addrs_t;

/* ==========================================================================
 * Clock configuration constants
 * ========================================================================== */
typedef enum : uint16_t {
    /*
     * PLLCR layout (16 bits):
     *   bits [1:0]  PLIDIV  : 00=/1, 01=/2, 10=/3
     *   bit  [4]    PLLSRCSEL: 0=MOSC, 1=HOCO
     *   bits [13:8] STC     : multiply = (STC + 1) / 2
     *
     * HOCO 16 MHz / 1 * 12 = 192 MHz PLL output.
     * STC = (12 * 2) - 1 = 23 = 0x17.
     * PLLSRCSEL = 1 (HOCO) -> bit 4 set.
     */
    k_pllcr_hoco_x12 = 0x1710U,

    /*
     * SCKCR2: bits [7:4] UCK = /(N+1), bits [3:0] reserved (must be 0x1).
     * UCK = 0x3 -> /4 -> 192 MHz / 4 = 48 MHz exact.
     */
    k_sckcr2_uck_div4 = 0x0031U,

    /* PACKCR: bit 0 UPLLSEL -- 0 routes UCLK divider input from main PLL. */
    k_packcr_pll_source = 0x0000U,
} clock_constants_word_t;

typedef enum : uint32_t {
    /*
     * SCKCR dividers:
     *   FCK=/4 (48), ICK=/2 (96), PSTOP1=1, PSTOP0=1, BCK=/4 (48),
     *   PCKA=/2 (96), PCKB=/4 (48), PCKC=/2 (96), PCKD=/2 (96).
     */
    k_sckcr_value = 0x21C21211U,
} clock_sckcr_t;

typedef enum : uint8_t {
    k_hococr_running   = 0x00U, /**< HOCOCR: 0 = HOCO running */
    k_pllcr2_enable    = 0x00U, /**< PLLCR2: 0 = PLL running (inverted logic) */
    k_memwait_one      = 0x01U, /**< MEMWAIT: 1 wait state for ICLK > 120 MHz */
    k_oscovfsr_hcovf   = 0x08U, /**< OSCOVFSR bit 3: HOCO stable */
    k_oscovfsr_plovf   = 0x04U, /**< OSCOVFSR bit 2: PLL stable */
} clock_byte_constants_t;

/* ==========================================================================
 * Inline accessors
 * ========================================================================== */
static inline volatile uint16_t *packcr_reg(void)
{
    return (volatile uint16_t *)k_packcr_addr;
}

/* ==========================================================================
 * clock_init -- bring the chip from LOCO 240 kHz to PLL 192/96/48 MHz.
 *
 * Uses HOCO (16 MHz internal oscillator) as the PLL source.
 * No external crystal required.
 *
 * Must run before any peripheral that depends on PCKB is touched.
 * Called from main() before cmt0_init() and before tx_kernel_enter().
 * ========================================================================== */
void clock_init(void)
{
    volatile rx_system_regs_t *sys = system_regs();

    /* Step 0: unlock PRC0 (clock) + PRC1 (module stop) */
    *prcr_reg() = k_rx_prcr_unlock_prc0_prc1;

    /* Step 1: start HOCO if not already running. */
    sys->hococr = k_hococr_running;

    /* Step 2: wait for HOCO to stabilise. */
    while ((sys->oscovfsr & k_oscovfsr_hcovf) == 0U) {
        __asm__ volatile("nop");
    }

    /* Step 3: program PLL: HOCO 16 MHz / 1 * 12 = 192 MHz, and enable. */
    sys->pllcr  = k_pllcr_hoco_x12;
    sys->pllcr2 = k_pllcr2_enable;

    /* Step 4: wait for PLL to stabilise. */
    while ((sys->oscovfsr & k_oscovfsr_plovf) == 0U) {
        __asm__ volatile("nop");
    }

    /* Step 5: MANDATORY -- raise flash wait state before ICLK exceeds 120 MHz. */
    *memwait_reg() = k_memwait_one;

    /* Step 6: program all bus dividers. */
    sys->sckcr = k_sckcr_value;

    /* Step 7: select main PLL as the UCLK divider source (UPLLSEL=0). */
    *packcr_reg() = k_packcr_pll_source;

    /* Step 8: program the USB clock divider. */
    sys->sckcr2 = k_sckcr2_uck_div4;

    /* Step 9: switch the system clock source to PLL. */
    sys->sckcr3 = k_sckcr3_cksel_pll;

    /* Step 10: relock PRCR. */
    *prcr_reg() = k_rx_prcr_lock;
}
