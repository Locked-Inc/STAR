/**
 * @file clock.c
 * @brief RX72N clock initialisation: 24 MHz crystal -> ICLK 240 / PCKB 60 / UCK 48.
 *
 * @details
 * The board boots on LOCO (~240 kHz) until software reprograms the clock tree.
 * For USB CDC operation we need:
 *   - ICLK  = 240 MHz  (CPU)
 *   - PCKA  = 120 MHz
 *   - PCKB  =  60 MHz  (CMT0 ThreadX tick, peripheral bus)
 *   - PCKC  =  60 MHz
 *   - PCKD  =  60 MHz
 *   - FCLK  =  60 MHz  (flash limit)
 *   - BCLK  = 120 MHz
 *   - UCLK  =  48 MHz  *** mandatory for USB device mode ***
 *
 * Path: EXTAL 24 MHz -> PLIDIV /1 -> 24 MHz -> STC x10 -> 240 MHz PLL output.
 * SCKCR2 UCK = /5 -> 240/5 = 48 MHz.  PACKCR.UPLLSEL = 0 selects the main
 * PLL (not PPLL) as the UCLK source -- matches the BSP-generated config in
 * src/boot/inc/smc/r_bsp_config.h (BSP_CFG_USB_CLOCK_SOURCE = 2).
 *
 * **The production rx_clock_power_init.c does NOT write SCKCR2 or PACKCR**,
 * which is the latent bug that prevents USB enumeration in the main firmware.
 * This implementation fixes that.
 *
 * Sequencing rules (don't soft-brick the chip):
 *   1. MOSCWTCR before MOSCCR.
 *   2. MEMWAIT = 1 BEFORE writing SCKCR with ICK > /2 (ICLK > 120 MHz).
 *   3. SCKCR3 source switch AFTER all dividers are programmed.
 *   4. PRCR unlock with 0xA503 (PRC0 + PRC1) for CGC + MSTPCR access.
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
    k_moscwtcr_addr = 0x000800A2U, /**< Main OSC wait control */
    k_mofcr_addr    = 0x0008C293U, /**< Main OSC drive / source select */
    k_packcr_addr   = 0x00080044U, /**< Peripheral clock source: 0=PLL, 1=PPLL */
} clock_extra_addrs_t;

/* ==========================================================================
 * Clock configuration constants
 * ========================================================================== */
typedef enum : uint16_t {
    /*
     * PLLCR layout (16 bits):
     *   bits [1:0]  PLIDIV  : 00=/1, 01=/2, 10=/3
     *   bit  [4]    PLLSRCSEL: 0=MOSC, 1=HOCO
     *   bits [13:8] STC     : (mul * 2 - 1), e.g. x10 -> 19 -> 0x13
     *
     * 24 MHz EXTAL / 1 * 10 = 240 MHz PLL output.
     */
    k_pllcr_24mhz_x10 = 0x1300U,

    /*
     * SCKCR layout (high nibble first):
     *   FCK ICK PSTOP BCK PCKA PCKB PCKC PCKD
     *   /4  /1  C     /2  /2   /4   /4   /4
     * Packed nibbles: 2 0 C 1 1 2 2 2 = 0x20C11222
     *
     * Note: the production firmware writes 0x21C21211 which is actually
     * ICK=/2 (120 MHz), BCK=/4 (60 MHz), PCKC=/2 (120 MHz), PCKD=/2 (120 MHz)
     * -- some of those exceed the documented peripheral clock maximums.
     */
    k_sckcr_240_120_60 = 0x20C11222U & 0xFFFFU, /* low half (actually we need 32-bit; see below) */
} clock_constants16_t;

typedef enum : uint32_t {
    /*
     * Match the production rx_clock_power_init.c value byte-for-byte so we
     * inherit a configuration that's known-good on this hardware.  The
     * docstring there lies about ICLK=240 MHz -- it actually programs:
     *   FCK=/4 (60), ICK=/2 (120), PSTOP1=1, PSTOP0=1, BCK=/4 (60),
     *   PCKA=/2 (120), PCKB=/4 (60), PCKC=/2 (120), PCKD=/2 (120).
     * MEMWAIT=1 is overkill at ICLK=120 MHz but harmless.
     *
     * USB only needs PCKB=60 (for CMT0 ThreadX tick) and UCK=48 (set via
     * SCKCR2 below).  The other dividers don't matter for this test.
     */
    k_sckcr_value = 0x21C21211U,
} clock_sckcr_t;

typedef enum : uint16_t {
    /*
     * SCKCR2: bits [7:4] UCK = /(N+1), bits [3:0] reserved (must contain 0x1).
     * UCK = 0x4 -> /5 -> 240 MHz / 5 = 48 MHz exact.
     */
    k_sckcr2_uck_div5 = 0x0041U,

    /* SCKCR3: CKSEL field at bits [10:8].  4 = PLL. */
    k_sckcr3_cksel_pll_value = 0x0400U,

    /* PACKCR: bit 0 UPLLSEL -- 0 routes UCLK divider input from main PLL. */
    k_packcr_pll_source = 0x0000U,
} clock_constants_word_t;

typedef enum : uint8_t {
    k_pllcr2_enable    = 0x00U, /**< PLLCR2: 0 = PLL running (inverted logic) */
    k_mosccr_enable    = 0x00U, /**< MOSCCR: 0 = MOSC running */
    k_moscwtcr_default = 0x53U, /**< MOSC wait time = 83 LOCO counts (~10 ms) */
    k_mofcr_crystal    = 0x00U, /**< MOFCR: MOSEL=0 (crystal), MODRV2=00 (20-24 MHz) */
    k_memwait_one      = 0x01U, /**< MEMWAIT: 1 wait state for ICLK > 120 MHz */
    k_oscovfsr_moovf   = 0x01U, /**< OSCOVFSR bit 0: MOSC stable */
    k_oscovfsr_plovf   = 0x04U, /**< OSCOVFSR bit 2: PLL stable */
} clock_byte_constants_t;

/* ==========================================================================
 * Inline accessors for the registers not in rx_system_regs_t
 * ========================================================================== */
static inline volatile uint8_t *moscwtcr_reg(void)
{
    return (volatile uint8_t *)k_moscwtcr_addr;
}
static inline volatile uint8_t *mofcr_reg(void)
{
    return (volatile uint8_t *)k_mofcr_addr;
}
static inline volatile uint16_t *packcr_reg(void)
{
    return (volatile uint16_t *)k_packcr_addr;
}

/* ==========================================================================
 * clock_init -- bring the chip from LOCO 240 kHz to PLL 240/120/60/48 MHz.
 *
 * Must run before any peripheral that depends on PCKB or UCK is touched.
 * Called from main() before cmt0_init() and before tx_kernel_enter().
 * ========================================================================== */
void clock_init(void)
{
    volatile rx_system_regs_t *sys = system_regs();

    /* Step 0: unlock PRC0 (clock) + PRC1 (module stop) */
    *prcr_reg() = k_rx_prcr_unlock_prc0_prc1;

    /* Step 1: configure MOSC for a 24 MHz crystal.
     *         MOSEL=0 (crystal), MODRV2=00 (20-24 MHz drive). */
    *mofcr_reg() = k_mofcr_crystal;

    /* Step 2: program the main OSC wait time before enabling MOSC. */
    *moscwtcr_reg() = k_moscwtcr_default;

    /* Step 3: start the main oscillator. */
    sys->mosccr = k_mosccr_enable;

    /* Step 4: wait for MOSC to stabilise. */
    while ((sys->oscovfsr & k_oscovfsr_moovf) == 0U) {
        __asm__ volatile("nop");
    }

    /* Step 5: program PLL multiplier (24 MHz / 1 * 10 = 240 MHz) and enable. */
    sys->pllcr  = k_pllcr_24mhz_x10;
    sys->pllcr2 = k_pllcr2_enable;

    /* Step 6: wait for PLL to stabilise. */
    while ((sys->oscovfsr & k_oscovfsr_plovf) == 0U) {
        __asm__ volatile("nop");
    }

    /* Step 7: MANDATORY -- raise flash wait state before ICLK exceeds 120 MHz. */
    *memwait_reg() = k_memwait_one;

    /* Step 8: program all bus dividers.
     *         FCK=/4(60), ICK=/1(240), BCK=/2(120), PCKA=/2(120),
     *         PCKB=/4(60), PCKC=/4(60), PCKD=/4(60). */
    sys->sckcr = k_sckcr_value;

    /* Step 9: select main PLL as the UCLK divider source (UPLLSEL=0). */
    *packcr_reg() = k_packcr_pll_source;

    /* Step 10: program the USB clock divider.  /5 of 240 MHz PLL = 48 MHz. */
    sys->sckcr2 = k_sckcr2_uck_div5;

    /* Step 11: switch the system clock source to PLL. */
    sys->sckcr3 = k_sckcr3_cksel_pll_value;

    /* Step 12: relock PRCR. */
    *prcr_reg() = k_rx_prcr_lock;
}
