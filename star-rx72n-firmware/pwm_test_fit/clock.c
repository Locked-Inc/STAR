/**
 * @file clock.c
 * @brief RX72N clock init using Renesas iodefine.h register structs.
 *
 * HOCO 16 MHz -> PLL 192 -> ICLK 96 MHz, PCKA 96 MHz, PCKB 48 MHz.
 * Identical register writes to pwm_test/clock.c but via iodefine.h instead
 * of STAR's rx72n_regs.h. This lets us share the same iodefine with the
 * GPTW-bringup code in main.c and guarantees addresses/bitfields come
 * straight from the authoritative Renesas BSP.
 *
 * No external crystal -- HOCO is the PLL source.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

#include "iodefine.h"

/* PRCR key/enable constants (PRCR is a 16-bit write-only control register
 * and cannot be expressed via the iodefine bitfield union on writes). */
#define PRCR_KEY_UNLOCK_PRC0_PRC1   ((uint16_t)0xA503U)
#define PRCR_KEY_LOCK               ((uint16_t)0xA500U)

/* PACKCR is not modeled as a standalone field in iodefine; address from manual. */
#define PACKCR_ADDR                 (0x00080044U)

/* MEMWAIT address and value (used for ICLK > 120 MHz). Our ICLK is 96 MHz
 * so this write is defensive; replicate pwm_test/clock.c for determinism. */
#define MEMWAIT_ADDR                (0x0008101CU)

void clock_init(void)
{
    volatile uint16_t * const prcr   = (volatile uint16_t *)&SYSTEM.PRCR.WORD;
    volatile uint8_t  * const memwait= (volatile uint8_t  *)MEMWAIT_ADDR;
    volatile uint16_t * const packcr = (volatile uint16_t *)PACKCR_ADDR;

    /* Unlock PRC0 (clock) + PRC1 (module stop). */
    *prcr = PRCR_KEY_UNLOCK_PRC0_PRC1;

    /* Start HOCO. */
    SYSTEM.HOCOCR.BYTE = 0x00U;
    while (SYSTEM.OSCOVFSR.BIT.HCOVF == 0U) { __asm__ volatile("nop"); }

    /* PLL: HOCO 16 MHz / 1 * 12 = 192 MHz (PLIDIV=00, PLLSRCSEL=1, STC=23). */
    SYSTEM.PLLCR.WORD  = 0x1710U;
    SYSTEM.PLLCR2.BYTE = 0x00U;
    while (SYSTEM.OSCOVFSR.BIT.PLOVF == 0U) { __asm__ volatile("nop"); }

    /* MEMWAIT = 1 (required for ICLK > 120 MHz; harmless for us). */
    *memwait = 0x01U;

    /* SCKCR: FCK=/4, ICK=/2, PSTOP1=1, PSTOP0=1, BCK=/4,
     *        PCKA=/2 (96 MHz), PCKB=/4 (48 MHz), PCKC=/2, PCKD=/2. */
    SYSTEM.SCKCR.LONG  = 0x21C21211U;

    /* UPLLSEL = 0 : route UCLK divider input from main PLL. */
    *packcr = 0x0000U;

    /* SCKCR2: UCK = /4 (48 MHz), bits [3:0] reserved = 1. */
    SYSTEM.SCKCR2.WORD = 0x0031U;

    /* SCKCR3: select PLL as system clock source. */
    SYSTEM.SCKCR3.WORD = 0x0400U;

    /* Relock PRCR. */
    *prcr = PRCR_KEY_LOCK;
}
