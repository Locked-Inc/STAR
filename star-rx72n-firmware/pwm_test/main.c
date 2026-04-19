/**
 * @file main.c
 * @brief Minimal 20 kHz PWM output on GPTW0 for AD2 scope verification.
 *
 * @details
 * No motor, no H-bridge — just drives the Motor 0 PWM pins with a
 * sawtooth-PWM duty sweep so the AD2 scope sees a clean 0-100% triangle
 * envelope on the logic-level outputs.
 *
 * Tom's PCB breakout mapping (see gpio_test/pin_map.md):
 *   - Tom header pin 34 -> P23 / GTIOC0A -> "IN2" (scope Ch2, blue)
 *   - Tom header pin 38 -> P17 / GTIOC0B -> "IN1" (scope Ch1, orange)
 *
 * Clocks (from clock.c):
 *   - PLL = HOCO 16 MHz * 12 = 192 MHz
 *   - PCKA = 192 / 2 = 96 MHz (GPTW source)
 *
 * PWM:
 *   - Saw-wave mode (GTCR.MD=000)
 *   - Period = 96 MHz / 20 kHz = 4800 counts -> GTPR = 4799
 *   - GTCCRA = duty (0 .. GTPR) for GTIOCA (P23)
 *   - GTCCRB = GTPR - duty for GTIOCB (P17) -- complementary
 *
 * Observable on the scope:
 *   - 50 us period (20 kHz) on both channels
 *   - 3.3 V logic levels (digital, not motor-driven)
 *   - Duty cycle sweeps 0 -> 100% -> 0% every ~4 seconds
 *   - Ch1 (P17) and Ch2 (P23) are mirror images
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

extern void clock_init(void);

#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

/* ==========================================================================
 * Register addresses
 * ========================================================================== */

/* System / module stop */
#define PRCR      0x000803FEU
#define MSTPCRA   0x00080010U /* bit 7 = GPTW (per Renesas iodefine.h -- MSTPCRC.MSTPC6 is actually ECCRAM) */

/* MPC pin function select */
#define PWPR      0x0008C11FU
#define P17PFS    0x0008C14FU /* pin 38 on Tom board */
#define P23PFS    0x0008C153U /* pin 34 on Tom board */

/* PORT 1 (P17) and PORT 2 (P23) peripheral mode */
#define PORT1_PMR  0x0008C061U
#define PORT2_PMR  0x0008C062U
#define PORT1_PDR  0x0008C001U
#define PORT2_PDR  0x0008C002U
#define PORT1_PODR 0x0008C021U
#define PORT2_PODR 0x0008C022U

/* GPTW0 channel registers (base 0x000C2000) */
#define GPTW0_BASE 0x000C2000U
#define GTWP      (GPTW0_BASE + 0x00U) /* 32b: write protection */
#define GTCR      (GPTW0_BASE + 0x2CU) /* 32b: MD, TPCS, CST */
#define GTUDDTYC  (GPTW0_BASE + 0x30U) /* 32b: count direction */
#define GTIOR     (GPTW0_BASE + 0x34U) /* 32b: GTIOCA/B output config */
#define GTINTAD   (GPTW0_BASE + 0x38U)
#define GTST      (GPTW0_BASE + 0x3CU)
#define GTBER     (GPTW0_BASE + 0x40U) /* buffer enable */
#define GTCNT     (GPTW0_BASE + 0x48U) /* counter */
#define GTCCRA    (GPTW0_BASE + 0x4CU) /* duty A */
#define GTCCRB    (GPTW0_BASE + 0x50U) /* duty B */
#define GTPR      (GPTW0_BASE + 0x64U) /* period */
#define GTPBR     (GPTW0_BASE + 0x68U) /* period buffer */

/* Heartbeat LED on PA7 (pin 88) so we see firmware is alive. */
#define PORTA_PDR  0x0008C00AU
#define PORTA_PODR 0x0008C02AU
#define PA7_MASK   0x80U

/* ==========================================================================
 * PWM configuration constants
 * ========================================================================== */
typedef enum : uint32_t {
    k_pwm_pcka_hz     = 96000000U,
    k_pwm_freq_hz     = 20000U,
    k_pwm_period_cnt  = 4800U,        /* 96 MHz / 20 kHz */
    k_pwm_gtpr        = 4799U,        /* period - 1 */
    k_duty_step       = 48U,          /* ~1% of period */
    k_duty_hold_loops = 40000U,       /* delay between duty steps */
    k_psel_gtiocxa    = 0x06U,        /* GTIOC0A, GTIOC0B alt function */
    k_psel_gtiocxb    = 0x06U,
} pwm_const_t;

static inline void delay_loops(uint32_t n)
{
    for (volatile uint32_t i = 0U; i < n; i++) {
        __asm__ volatile("nop");
    }
}

/* ==========================================================================
 * Bring GPTW0 out of module stop and wire P23/P17 to GTIOC0A/GTIOC0B.
 * ========================================================================== */
static void gptw0_pin_and_module_setup(void)
{
    /* Unlock PRC1 (module stop), clear MSTPCRA.MSTPA7 (GPTW), lock. */
    REG16(PRCR) = 0xA502U;
    REG32(MSTPCRA) &= ~(1UL << 7);
    REG16(PRCR) = 0xA500U;

    /* Enable PFS writes via MPC PWPR. */
    REG8(PWPR) = 0x00U;   /* clear B0WI */
    REG8(PWPR) = 0x40U;   /* set PFSWE */

    /* P23 -> GTIOC0A, P17 -> GTIOC0B.  PSEL = 0x14 for GPTW per rx_mpc.h. */
    REG8(P23PFS) = 0x14U;
    REG8(P17PFS) = 0x14U;

    /* Re-protect PFS. */
    REG8(PWPR) = 0x00U;
    REG8(PWPR) = 0x80U;

    /* Switch pins to peripheral mode (PMR=1). PDR stays input side
     * because the GTIOC output enable in GTIOR drives the pad. */
    REG8(PORT2_PMR) |= (uint8_t)(1U << 3); /* P23 */
    REG8(PORT1_PMR) |= (uint8_t)(1U << 7); /* P17 */
}

/* ==========================================================================
 * Configure GPTW0 as 20 kHz saw-wave PWM with complementary A/B outputs.
 * ========================================================================== */
static void gptw0_init_pwm(void)
{
    /* Unlock GTWP write-protection (key 0xA500 in upper bits, WP bits = 0). */
    REG32(GTWP)    = 0x0000A500U;

    /* Stop counter before configuring. */
    REG32(GTCR)    = 0x00000000U;
    REG32(GTUDDTYC)= 0x00000003U;        /* UD=1 (count up) + UDF=1 (forced update) -- without UDF, UD write is ignored */
    REG32(GTCNT)   = 0x00000000U;

    /* Period: 4800 cycles at 96 MHz = 20 kHz. GTPR = period - 1. */
    REG32(GTPR)    = (uint32_t)k_pwm_gtpr;
    REG32(GTPBR)   = (uint32_t)k_pwm_gtpr;

    /* Start at 50% duty for initial visibility. */
    REG32(GTCCRA)  = (uint32_t)(k_pwm_period_cnt / 2U);
    REG32(GTCCRB)  = (uint32_t)(k_pwm_period_cnt - (k_pwm_period_cnt / 2U));

    /* GTIOR per rx72n_gptw_regs.h:
     *   GTIOA[4:0] = 0x09  -> initial LOW, high at cycle end, low at CMA
     *   OAE bit 8  = 1     -> output enable on GTIOCA
     *   GTIOB[20:16] = 0x09 same pattern
     *   OBE bit 24 = 1     -> output enable on GTIOCB
     * (Complementary waveform is produced by setting GTCCRB = period-duty.)
     */
    /* 0x06 = initial low, toggle on compare match.  Per Renesas iodefine
     * reference this is the canonical simple-PWM encoding; matches the
     * FIT/FSP docs (our rx72n_gptw_regs.h uses 0x09 which is a different
     * saw-wave variant; we try 0x06 first). */
    const uint32_t gtior =
        (0x06U << 0)  | (1U << 8)  |   /* GTIOCA: init low, toggle@CMP, OAE */
        (0x06U << 16) | (1U << 24);    /* GTIOCB: init low, toggle@CMP, OBE */
    REG32(GTIOR)   = gtior;

    /* Enable single-buffer mode for GTCCRA/GTCCRB (HAL requirement). */
    REG32(GTBER)   = 0x00050005U;

    /* Start counter: MD=0 saw-wave, TPCS=0 (PCLKA/1), CST=1. */
    REG32(GTCR)    = (0U << 16) | (0U << 24) | (1U << 0);
}

/* Debug helper: bring out two GPIOs (P17 and P23 as GPIO override) that
 * mirror whether GTCNT is nonzero and whether GTCR CST bit reads back.
 * WARNING: this re-purposes the PWM pins so scope will see GPIO-style
 * toggling not PWM.  Use only for debug diagnosis. */
static void gptw_diag_dump(void)
{
    volatile uint32_t cnt_a = REG32(GTCNT);
    for (volatile uint32_t d = 0; d < 1000U; d++) { __asm__ volatile("nop"); }
    volatile uint32_t cnt_b = REG32(GTCNT);
    volatile uint32_t cst   = REG32(GTCR) & 1U;
    volatile uint32_t pmr1  = REG8(PORT1_PMR) & (1U << 7);
    volatile uint32_t pmr2  = REG8(PORT2_PMR) & (1U << 3);

    /* Force P17/P23 back to GPIO mode so we can encode diagnostics on them. */
    REG8(PORT1_PMR) &= (uint8_t)~(1U << 7);
    REG8(PORT2_PMR) &= (uint8_t)~(1U << 3);
    REG8(PORT1_PDR) |= (1U << 7);
    REG8(PORT2_PDR) |= (1U << 3);

    /* Encode diagnostics on Ch1 (P17) and Ch2 (P23):
     *   - 5 pulses = GTCNT advanced (counter running)
     *   - 3 pulses = GTCNT frozen
     *   - 7 pulses = CST readback = 1
     *   - 1 pulse  = CST readback = 0
     *   Sequence: cnt-state pulses, pause, cst pulses, pause, pmr pulses. */
    uint32_t counts_advanced = (cnt_b != cnt_a);
    uint32_t pulses_a = counts_advanced ? 5U : 3U;
    uint32_t pulses_b = cst ? 7U : 1U;
    uint32_t pulses_c = (pmr1 != 0U && pmr2 != 0U) ? 4U : 2U;

    for (uint32_t round = 0; round < 3U; round++) {
        uint32_t n = (round == 0U) ? pulses_a : (round == 1U) ? pulses_b : pulses_c;
        for (uint32_t i = 0; i < n; i++) {
            REG8(PORT1_PODR) |= (1U << 7);
            REG8(PORT2_PODR) |= (1U << 3);
            for (volatile uint32_t d = 0; d < 30000U; d++) { __asm__ volatile("nop"); }
            REG8(PORT1_PODR) &= (uint8_t)~(1U << 7);
            REG8(PORT2_PODR) &= (uint8_t)~(1U << 3);
            for (volatile uint32_t d = 0; d < 30000U; d++) { __asm__ volatile("nop"); }
        }
        for (volatile uint32_t d = 0; d < 400000U; d++) { __asm__ volatile("nop"); }
    }
}

/* ==========================================================================
 * LED + PWM duty sweep forever.  Produces a visible 0..100% triangle on the
 * scope so you can watch the duty cycle move.
 * ========================================================================== */
static void run_forever(void)
{
    /* PA7 heartbeat LED. */
    REG8(PORTA_PDR)  |= PA7_MASK;
    REG8(PORTA_PODR) &= (uint8_t)~PA7_MASK;

    uint32_t duty      = 0U;
    uint32_t direction = 1U; /* 1 = up, 0 = down */

    for (;;) {
        /* Update duty on both channels.  CCRB = period - CCRA keeps them
         * complementary (visible as mirror-image waveforms on the scope). */
        REG32(GTCCRA) = duty;
        REG32(GTCCRB) = (uint32_t)k_pwm_period_cnt - duty;

        /* Toggle LED every loop so we can see the firmware is alive. */
        REG8(PORTA_PODR) ^= PA7_MASK;

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

/* Slow toggle on PA7 LED so AD2 can see how far we got.
 * Probe a 3rd channel to PA7 / pin 88 and count pulses. */
static void dbg_pulse(uint32_t n)
{
    REG8(PORTA_PDR)  |= PA7_MASK;
    for (uint32_t i = 0; i < n; i++) {
        REG8(PORTA_PODR) |= PA7_MASK;
        for (volatile uint32_t d = 0; d < 20000U; d++) { __asm__ volatile("nop"); }
        REG8(PORTA_PODR) &= (uint8_t)~PA7_MASK;
        for (volatile uint32_t d = 0; d < 20000U; d++) { __asm__ volatile("nop"); }
    }
    for (volatile uint32_t d = 0; d < 100000U; d++) { __asm__ volatile("nop"); }
}

int main(void)
{
    /* No clock_init -- stay at LOCO.  After PWM init, read GTCNT twice with
     * a delay; if counter advanced the GPTW clock is running.  Then force
     * pins back to GPIO and encode the result as pulses on P17/P23 so
     * scope can see: 5 pulses = counter running, 3 = frozen. */
    gptw0_pin_and_module_setup();
    gptw0_init_pwm();

    /* Read GTCNT twice to see if counter ticks. */
    volatile uint32_t a = REG32(GTCNT);
    for (volatile uint32_t d = 0; d < 10000U; d++) { __asm__ volatile("nop"); }
    volatile uint32_t b = REG32(GTCNT);
    uint32_t running = (b != a) ? 1U : 0U;

    /* Pull pins back to GPIO to emit a diag pulse pattern. */
    REG8(PORT1_PMR) &= (uint8_t)~(1U << 7);
    REG8(PORT2_PMR) &= (uint8_t)~(1U << 3);
    REG8(PORT1_PDR) |= (1U << 7);
    REG8(PORT2_PDR) |= (1U << 3);

    /* First N pulses encode the answer:
     *   5 pulses  -> GTCNT advanced (counter ticking)
     *   3 pulses  -> GTCNT frozen (counter stopped)
     * Then a long gap, then 2 pulses signalling "end of diag". */
    uint32_t n = running ? 5U : 3U;
    for (uint32_t i = 0; i < n; i++) {
        REG8(PORT1_PODR) |= (1U << 7);
        REG8(PORT2_PODR) |= (1U << 3);
        for (volatile uint32_t d = 0; d < 50000U; d++) { __asm__ volatile("nop"); }
        REG8(PORT1_PODR) &= (uint8_t)~(1U << 7);
        REG8(PORT2_PODR) &= (uint8_t)~(1U << 3);
        for (volatile uint32_t d = 0; d < 50000U; d++) { __asm__ volatile("nop"); }
    }
    for (;;) {
        /* Idle with pins LOW. */
    }
    return 0;
}

/* =============================================================================
 * Stubs for symbols referenced by vectors.S (ThreadX ISRs, CMT0) that this
 * minimal firmware doesn't use. Prevents linker errors without dragging in
 * the full ThreadX library or a real CMT0 driver.
 * ============================================================================= */
void _tx_thread_context_save(void)    {}
void _tx_thread_context_restore(void) {}
void cmt0_isr(void)                   {}
