/**
 * @file main.c
 * @brief 20 kHz PWM on GPTW0 using Renesas iodefine.h register structs.
 *
 * @details
 * "FIT-style" bring-up: instead of hand-rolled `REG32(0x000C202C) = X` writes
 * (which in pwm_test/main.c went silent on Tom's PCB), this file uses the
 * authoritative Renesas BSP register definitions from
 *   r_bsp/r_bsp_vx.xx/r_bsp/mcu/rx72n/register_access/gnuc/iodefine.h
 *
 * That header is the same one every Renesas FIT driver builds against (the
 * public rx-driver-package has NO dedicated GPTW FIT driver -- the only code
 * in the whole package that writes GPTW registers is r_aeropoint_rx, and it
 * does so through this same iodefine with statements like
 *   MSTP(GPTW) = 0;
 *   GPTW1.GTCR.LONG = ...;
 *   R_MPC_Write(...);
 * so that is the "reference driver" pattern we mimic here.
 *
 * ### Pin map (Tom's PCB breakout -> GPTW0)
 *   - Tom header pin 34 -> P23 / GTIOC0A  (scope Ch2, blue,  "IN2")
 *   - Tom header pin 38 -> P17 / GTIOC0B  (scope Ch1, orange,"IN1")
 *
 * ### Clocking (clock.c)
 *   - HOCO 16 MHz x 12 = 192 MHz PLL
 *   - ICLK 96 MHz, PCKA 96 MHz (GPTW source)
 *
 * ### PWM
 *   - Saw-wave mode (GTCR.MD = 000)
 *   - Period  = 96 MHz / 20 kHz = 4800 counts -> GTPR = 4799
 *   - GTIOCA / GTIOCB driven with complementary duty for scope verification
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdbool.h>
#include <stdint.h>

#include "iodefine.h"

extern void clock_init(void);

/* -------------------------------------------------------------------------
 * PWM configuration
 * ------------------------------------------------------------------------- */
enum {
    k_pwm_pcka_hz     = 96000000U,
    k_pwm_freq_hz     = 20000U,
    k_pwm_period_cnt  = 4800U,         /* PCKA / freq */
    k_pwm_gtpr        = 4799U,         /* period - 1 */
    k_pwm_duty_50pct  = 2400U,
    k_duty_step       = 48U,
    k_duty_hold_loops = 40000U,

    /* PSEL value for GPTW alternate function on Port 1 / Port 2 pins.
     * Matches the PSEL used by r_aeropoint_rx's GPTW pin init (0x1E there is
     * Port D specific; 0x14 is the documented GTIOCnA/B select for the P1x
     * and P2x pins in the RX72N MPC tables -- same value used by STAR's
     * rx_gptw.c HAL and by pwm_test/main.c today). */
    k_psel_gptw       = 0x14U,
};

static inline void delay_loops(uint32_t n)
{
    for (volatile uint32_t i = 0U; i < n; ++i) {
        __asm__ volatile("nop");
    }
}

/* -------------------------------------------------------------------------
 * Step 1: Release GPTW from module stop via MSTPCRA.MSTPA7.
 *
 * iodefine.h provides the canonical MSTP(GPTW) macro which resolves to
 * SYSTEM.MSTPCRA.BIT.MSTPA7 on RX72N. Writing 0 wakes the module.
 * This is the exact sequence used by r_aeropoint_rx's hardware init.
 * ------------------------------------------------------------------------- */
static void release_gptw_from_module_stop(void)
{
    /* Unlock PRC1 (module stop protection) via PRCR.
     * PRCR requires the key 0xA5 in the upper byte. */
    SYSTEM.PRCR.WORD = 0xA502U;   /* PRKEY=0xA5, PRC1=1 */
    MSTP(GPTW) = 0U;              /* Expands to SYSTEM.MSTPCRA.BIT.MSTPA7 = 0 */
    SYSTEM.PRCR.WORD = 0xA500U;   /* Re-lock */
}

/* -------------------------------------------------------------------------
 * Step 2: Route GTIOC0A -> P23, GTIOC0B -> P17 via MPC.
 *
 * Equivalent to what r_mpc_rx would do via R_MPC_Write() -- same register
 * sequence, direct. Port 1 / Port 2 pins route GPTW signals with PSEL=0x14.
 * ------------------------------------------------------------------------- */
static void route_gtioc0_to_p23_p17(void)
{
    /* Unlock MPC PFS writes: clear B0WI, then set PFSWE. */
    MPC.PWPR.BIT.B0WI  = 0U;
    MPC.PWPR.BIT.PFSWE = 1U;

    /* PSEL = 0x14 selects GPTW alternate function. */
    MPC.P23PFS.BYTE = (uint8_t)k_psel_gptw;  /* P23 -> GTIOC0A */
    MPC.P17PFS.BYTE = (uint8_t)k_psel_gptw;  /* P17 -> GTIOC0B */

    /* Re-lock PFS writes. */
    MPC.PWPR.BIT.PFSWE = 0U;
    MPC.PWPR.BIT.B0WI  = 1U;

    /* Switch the pads to peripheral mode so the GPTW output drives the pin.
     * We deliberately leave PDR alone -- the peripheral-mode pad is driven
     * by GTIOCnA/B's OAE/OBE bits from GTIOR. */
    PORT2.PMR.BIT.B3 = 1U;   /* P23 */
    PORT1.PMR.BIT.B7 = 1U;   /* P17 */
}

/* -------------------------------------------------------------------------
 * Step 3: Configure GPTW0 as a 20 kHz saw-PWM with complementary A/B.
 *
 * All register writes use iodefine.h bitfields. Addresses / bit positions
 * come straight from the Renesas BSP header so we cannot mistype offsets.
 * ------------------------------------------------------------------------- */
static void configure_gptw0_pwm_20khz(void)
{
    /* Unlock GPTW0 write protection. PRKEY=0xA5, WP bits cleared. */
    GPTW0.GTWP.LONG = 0x0000A500U;

    /* Stop counter before configuring. GTCR.CST = 0 halts the counter. */
    GPTW0.GTCR.LONG = 0x00000000U;

    /* Force count direction = up (UD=1) with UDF=1 to accept the write.
     * Without UDF, the UD bit write is ignored per the hardware manual. */
    GPTW0.GTUDDTYC.LONG = 0x00000003U;

    /* Clear counter. */
    GPTW0.GTCNT = 0U;

    /* Period: 4799 -> 20 kHz at 96 MHz PCKA. */
    GPTW0.GTPR  = (uint32_t)k_pwm_gtpr;
    GPTW0.GTPBR = (uint32_t)k_pwm_gtpr;

    /* Initial 50% duty on both channels for scope visibility.
     * GTCCRB = period - GTCCRA produces a mirror-image waveform on B. */
    GPTW0.GTCCRA = (uint32_t)k_pwm_duty_50pct;
    GPTW0.GTCCRB = (uint32_t)(k_pwm_period_cnt - k_pwm_duty_50pct);

    /* GTIOR configuration using iodefine bitfields.
     *
     * Per RX72N HW manual 26.2.14:
     *   GTIOA/GTIOB = 0b01001 (0x09):
     *     - Initial output LOW
     *     - Low  at compare match with GTCCRA/B while counting up
     *     - High at cycle end (GTCNT reaches GTPR)
     *   -> produces an active-high sawtooth PWM pulse of width GTCCRx.
     * OAE / OBE = 1 to actually drive the pad.
     *
     * Everything else zeroed (no dead time, no noise filter, no force-low). */
    GPTW0.GTIOR.LONG = 0U;
    GPTW0.GTIOR.BIT.GTIOA = 0x09U;
    GPTW0.GTIOR.BIT.OAE   = 1U;
    GPTW0.GTIOR.BIT.GTIOB = 0x09U;
    GPTW0.GTIOR.BIT.OBE   = 1U;

    /* Enable single-buffer operation on GTCCRA/B and GTPR so writes
     * propagate on the next cycle boundary without glitching. Matches the
     * STAR HAL's tested configuration. */
    GPTW0.GTBER.LONG = 0x00050005U;

    /* Select sawtooth PWM mode (MD=000), PCKA/1 (TPCS=0000), start (CST=1). */
    GPTW0.GTCR.BIT.MD   = 0U;
    GPTW0.GTCR.BIT.TPCS = 0U;
    GPTW0.GTCR.BIT.CST  = 1U;

    /* Lock GPTW0 write protection. */
    GPTW0.GTWP.LONG = 0x0000A501U;
}

/* Sweep GTCCRA/B in a triangle so the scope sees a 0..100..0% envelope. */
static void run_pwm_sweep_forever(void)
{
    /* PA7 heartbeat LED so we can see firmware is alive even if PWM is dead. */
    PORTA.PDR.BIT.B7  = 1U;
    PORTA.PODR.BIT.B7 = 0U;

    uint32_t duty      = 0U;
    uint32_t direction = 1U;

    for (;;) {
        /* Buffered write -- propagates on next cycle boundary via GTCCRC. */
        GPTW0.GTCCRA = duty;
        GPTW0.GTCCRB = (uint32_t)k_pwm_period_cnt - duty;

        PORTA.PODR.BIT.B7 ^= 1U;

        delay_loops(k_duty_hold_loops);

        if (direction != 0U) {
            duty += (uint32_t)k_duty_step;
            if (duty >= (uint32_t)k_pwm_period_cnt) {
                duty      = (uint32_t)k_pwm_period_cnt;
                direction = 0U;
            }
        } else {
            if (duty <= (uint32_t)k_duty_step) {
                duty      = 0U;
                direction = 1U;
            } else {
                duty -= (uint32_t)k_duty_step;
            }
        }
    }
}

int main(void)
{
    clock_init();
    release_gptw_from_module_stop();
    route_gtioc0_to_p23_p17();
    configure_gptw0_pwm_20khz();
    run_pwm_sweep_forever();
    return 0;
}

/* -------------------------------------------------------------------------
 * Stubs for symbols referenced by vectors.S (ThreadX ISRs, CMT0) that this
 * minimal firmware doesn't use. Prevents linker errors without dragging in
 * the full ThreadX library or a real CMT0 driver.
 * ------------------------------------------------------------------------- */
void _tx_thread_context_save(void)    {}
void _tx_thread_context_restore(void) {}
void cmt0_isr(void)                   {}
