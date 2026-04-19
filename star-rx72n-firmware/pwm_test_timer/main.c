/**
 * @file main.c
 * @brief 20 kHz PWM on GPTW0 with CMT0-driven duty sweep (no busy-wait).
 *
 * @details
 * Same GPTW0 setup as pwm_test/ -- raw-register config of P23/GTIOC0A and
 * P17/GTIOC0B as a saw-wave 20 kHz PWM with complementary A/B duty.
 *
 * The difference: the duty-cycle sweep cadence is driven by the CMT0 100 Hz
 * tick ISR (cmt0.c) instead of a busy-wait delay loop.  main() configures
 * the hardware, starts CMT0, then sleeps in a WAIT loop while the ISR
 * updates GPTW0.GTCCRA / GTCCRB directly.
 *
 * Cadence: CMT0 fires at 100 Hz.  Duty advances by k_duty_step (~1% of
 * period, 48 counts) per tick.  Full 0 -> 100 -> 0 sweep takes ~2 s each
 * way = 4 s round trip.
 *
 * Tom's PCB breakout mapping (see gpio_test/pin_map.md):
 *   - Tom header pin 34 -> P23 / GTIOC0A -> "IN2" (scope Ch2, blue)
 *   - Tom header pin 38 -> P17 / GTIOC0B -> "IN1" (scope Ch1, orange)
 *
 * Clocks:
 *   - PCKA = 96 MHz (GPTW source)
 *   - PCKB = 48 MHz (CMT0 source)
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

extern void clock_init(void);
extern void cmt0_init(void);

#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

/* ==========================================================================
 * Register addresses (mirrors pwm_test/main.c)
 * ========================================================================== */

/* System / module stop */
#define PRCR      0x000803FEU
#define MSTPCRA   0x00080010U

/* MPC pin function select */
#define PWPR      0x0008C11FU
#define P17PFS    0x0008C14FU
#define P23PFS    0x0008C153U

/* PORT 1 (P17) and PORT 2 (P23) peripheral mode */
#define PORT1_PMR  0x0008C061U
#define PORT2_PMR  0x0008C062U

/* GPTW0 channel registers (base 0x000C2000) */
#define GPTW0_BASE 0x000C2000U
#define GTWP      (GPTW0_BASE + 0x00U)
#define GTCR      (GPTW0_BASE + 0x2CU)
#define GTUDDTYC  (GPTW0_BASE + 0x30U)
#define GTIOR     (GPTW0_BASE + 0x34U)
#define GTST      (GPTW0_BASE + 0x3CU)
#define GTBER     (GPTW0_BASE + 0x40U)
#define GTCNT     (GPTW0_BASE + 0x48U)
#define GTCCRA    (GPTW0_BASE + 0x4CU)
#define GTCCRB    (GPTW0_BASE + 0x50U)
#define GTPR      (GPTW0_BASE + 0x64U)
#define GTPBR     (GPTW0_BASE + 0x68U)

/* Heartbeat LED on PA7 (pin 88). */
#define PORTA_PDR  0x0008C00AU
#define PORTA_PODR 0x0008C02AU
#define PA7_MASK   0x80U

/* ==========================================================================
 * PWM configuration constants
 * ========================================================================== */
typedef enum : uint32_t {
    k_pwm_pcka_hz     = 96000000U,
    k_pwm_freq_hz     = 20000U,
    k_pwm_period_cnt  = 4800U,   /* 96 MHz / 20 kHz */
    k_pwm_gtpr        = 4799U,   /* period - 1 */
    k_duty_step       = 48U,     /* ~1% of period per CMT0 tick */
    k_heartbeat_div   = 50U,     /* toggle LED every 50 ticks = 2 Hz */
} pwm_const_t;

/* ==========================================================================
 * Globals -- updated by CMT0 ISR, read by nothing (writes are side-effecting
 * to hardware registers).  Marked volatile to defeat LTO and show intent.
 * ========================================================================== */
static volatile uint32_t g_duty      = 0U;
static volatile uint32_t g_direction = 1U;  /* 1 = up, 0 = down */
static volatile uint32_t g_tick_cnt  = 0U;

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
    REG8(PWPR) = 0x00U;
    REG8(PWPR) = 0x40U;

    /* P23 -> GTIOC0A, P17 -> GTIOC0B.  PSEL = 0x14 for GPTW. */
    REG8(P23PFS) = 0x14U;
    REG8(P17PFS) = 0x14U;

    /* Re-protect PFS. */
    REG8(PWPR) = 0x00U;
    REG8(PWPR) = 0x80U;

    /* Switch pins to peripheral mode. */
    REG8(PORT2_PMR) |= (uint8_t)(1U << 3); /* P23 */
    REG8(PORT1_PMR) |= (uint8_t)(1U << 7); /* P17 */
}

/* ==========================================================================
 * Configure GPTW0 as 20 kHz saw-wave PWM with complementary A/B outputs.
 * ========================================================================== */
static void gptw0_init_pwm(void)
{
    REG32(GTWP)     = 0x0000A500U;
    REG32(GTCR)     = 0x00000000U;
    REG32(GTUDDTYC) = 0x00000003U;
    REG32(GTCNT)    = 0x00000000U;

    REG32(GTPR)     = (uint32_t)k_pwm_gtpr;
    REG32(GTPBR)    = (uint32_t)k_pwm_gtpr;

    /* Start at 50% duty (CMT0 ISR will start sweeping from here). */
    REG32(GTCCRA)   = (uint32_t)(k_pwm_period_cnt / 2U);
    REG32(GTCCRB)   = (uint32_t)(k_pwm_period_cnt - (k_pwm_period_cnt / 2U));

    const uint32_t gtior =
        (0x06U << 0)  | (1U << 8)  |   /* GTIOCA: init low, toggle@CMP, OAE */
        (0x06U << 16) | (1U << 24);    /* GTIOCB: init low, toggle@CMP, OBE */
    REG32(GTIOR)    = gtior;

    REG32(GTBER)    = 0x00050005U;

    /* Start counter: MD=0 saw-wave, TPCS=0 (PCLKA/1), CST=1. */
    REG32(GTCR)     = (0U << 16) | (0U << 24) | (1U << 0);
}

/* ==========================================================================
 * CMT0 tick hook -- called at 100 Hz from CMT0 ISR in cmt0.c.
 * Advances the PWM duty sweep by one step and toggles the heartbeat LED
 * at a slower cadence.
 *
 * NOTE: this runs in ISR context.  Writes to GTCCRA/GTCCRB go straight to
 * the GPTW compare-match registers; GPTW's single-buffer mode (GTBER=0x50005)
 * latches the new value at the next period boundary so the PWM output never
 * glitches mid-cycle.
 * ========================================================================== */
void cmt0_tick_hook(void)
{
    /* Advance duty. */
    if (g_direction != 0U) {
        g_duty += (uint32_t)k_duty_step;
        if (g_duty >= (uint32_t)k_pwm_period_cnt) {
            g_duty      = (uint32_t)k_pwm_period_cnt;
            g_direction = 0U;
        }
    } else {
        if (g_duty <= (uint32_t)k_duty_step) {
            g_duty      = 0U;
            g_direction = 1U;
        } else {
            g_duty -= (uint32_t)k_duty_step;
        }
    }

    /* Push to GPTW compare-match registers (buffered latch at next period). */
    REG32(GTCCRA) = g_duty;
    REG32(GTCCRB) = (uint32_t)k_pwm_period_cnt - g_duty;

    /* Heartbeat LED every k_heartbeat_div ticks (= 1 Hz full cycle). */
    g_tick_cnt++;
    if ((g_tick_cnt % (uint32_t)k_heartbeat_div) == 0U) {
        REG8(PORTA_PODR) ^= PA7_MASK;
    }
}

int main(void)
{
    clock_init();                    /* HOCO x12 -> 192 MHz PLL */
    gptw0_pin_and_module_setup();    /* MSTPA7 + MPC + PMR */
    gptw0_init_pwm();                /* 20 kHz sawtooth, A/B complementary */

    /* Drive heartbeat LED low initially. */
    REG8(PORTA_PDR)  |= PA7_MASK;
    REG8(PORTA_PODR) &= (uint8_t)~PA7_MASK;

    /* Arm 100 Hz tick -- enables CMT0, sets IPR=5, IER bit, setpsw i. */
    cmt0_init();

    /* All work happens in cmt0_tick_hook() via vector 28.  Sleep here. */
    for (;;) {
        __asm__ volatile("wait");
    }
    return 0;
}

/* =============================================================================
 * Stubs for SWINT-path symbols referenced by vectors.S (ThreadX ISRs).
 * SWINT is never raised in this firmware, but the vector table references
 * these symbols so they must resolve at link time.
 * ============================================================================= */
void _tx_thread_context_save(void)    {}
void _tx_thread_context_restore(void) {}
