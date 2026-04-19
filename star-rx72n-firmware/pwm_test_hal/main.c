/**
 * @file main.c
 * @brief HAL-driven 20 kHz PWM on GPTW0 channel 0 with outputs on P23/P17.
 *
 * @details
 * Standalone firmware that exercises the STAR libs/rx_hal/src/rx_gptw.c HAL
 * directly to drive Tom's breakout-board PWM pins:
 *
 *   - P23 / GTIOC0A -> Tom header pin 34 ("IN2", scope Ch2 blue)
 *   - P17 / GTIOC0B -> Tom header pin 38 ("IN1", scope Ch1 orange)
 *
 * These are NOT the HAL's default PE5/PE2 routing. The HAL configures MPC
 * internally for PE5/PE2, so after rx_gptw_init_pwm() we undo that routing
 * and re-pin GTIOC0A/B on P23/P17.
 *
 * Clock path (from pwm_test's clock.c):
 *   - HOCO 16 MHz x 12 = 192 MHz PLL
 *   - ICLK 96 MHz, PCKA 96 MHz
 *
 * IMPORTANT: the STAR HAL is hard-coded for PCKA = 120 MHz. With our actual
 * PCKA = 96 MHz, the HAL writes GTPR = 120M/20k = 6000 which gives real
 * PWM frequency 96M/6000 = 16 kHz. After init we overwrite GTPR/GTPBR to
 * 4799 (96M/20k) so the measured output is a true 20 kHz.
 *
 * Module-stop bit quirk: STAR's rx72n_gptw_regs.h declares GPTW at
 * MSTPCRC bit 6 (k_mstpc_gptw = 6) but per the Renesas RX72N iodefine.h,
 * GPTW is controlled by MSTPCRA bit 7 on RX72N. We clear MSTPA7 ourselves
 * BEFORE calling rx_gptw_init_pwm() so the module is already running; the
 * HAL's bogus MSTPC6 write becomes a harmless no-op on some unrelated bit.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdbool.h>
#include <stdint.h>

#include "rx_gptw.h"

extern void clock_init(void);

#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

/* System / module stop */
#define PRCR       0x000803FEU
#define MSTPCRA    0x00080010U /* bit 7 = GPTW on RX72N (per Renesas iodefine) */
#define PRCR_UNLOCK_PRC1 0xA502U
#define PRCR_LOCK        0xA500U

/* MPC pin function select */
#define PWPR       0x0008C11FU
#define P17PFS     0x0008C14FU /* pin 38 on Tom board */
#define P23PFS     0x0008C153U /* pin 34 on Tom board */
#define PE2PFS     0x0008C172U /* HAL default GTIOC0B -- we disable this */
#define PE5PFS     0x0008C175U /* HAL default GTIOC0A -- we disable this */

#define PWPR_UNLOCK_STAGE1 0x00U /* clear B0WI */
#define PWPR_UNLOCK_STAGE2 0x40U /* set PFSWE */
#define PWPR_LOCK          0x80U /* set B0WI */

#define PFS_GPTW     0x14U
#define PFS_DISABLED 0x00U

/* PORT peripheral mode / direction */
#define PORT1_PMR  0x0008C061U
#define PORT2_PMR  0x0008C062U
#define PORTE_PMR  0x0008C06EU

/* GPTW0 base for post-init GTPR override (channel 0 is at 0x000C2000) */
#define GPTW0_GTPR   0x000C2064U /* Cycle Setting Register */
#define GPTW0_GTPBR  0x000C2068U /* Cycle Setting Buffer Register */
#define GPTW0_GTWP   0x000C2000U /* Write Protection Register */
#define GPTW_GTWP_UNLOCK 0x0000A500U
#define GPTW_GTWP_LOCK   0x0000A501U

/* Heartbeat LED on PA7 */
#define PORTA_PDR  0x0008C00AU
#define PORTA_PODR 0x0008C02AU
#define PA7_MASK   0x80U

/* PWM constants for 20 kHz at PCKA = 96 MHz */
typedef enum : uint32_t {
    k_pwm_pcka_hz      = 96000000U,
    k_pwm_freq_hz      = 20000U,
    k_pwm_period_cnt   = 4800U, /* GTPR+1 */
    k_pwm_gtpr         = 4799U, /* PCKA/freq - 1 */
    k_pwm_duty_50pct   = 2400U, /* half of period */
    k_duty_step        = 48U,   /* ~1% of period */
    k_duty_hold_loops  = 40000U,
} pwm_const_t;

static inline void delay_loops(uint32_t n)
{
    for (volatile uint32_t i = 0U; i < n; i++) {
        __asm__ volatile("nop");
    }
}

/* Release GPTW module stop via the real RX72N bit (MSTPCRA.MSTPA7). The HAL's
 * internal_enable_gptw_module_clock writes to MSTPCRC.MSTPC6, which appears to
 * be a transcription error for this chip. We do this before calling the HAL so
 * the HAL's bogus write is a no-op on some unrelated bit. */
static void enable_gptw_module_mstpa7(void)
{
    REG16(PRCR)     = PRCR_UNLOCK_PRC1;
    REG32(MSTPCRA) &= ~(1UL << 7);
    REG16(PRCR)     = PRCR_LOCK;
}

/* The HAL's rx_gptw_init_pwm configures MPC for PE5/PE2 (HAL's default
 * Motor 0 pins). We need GTIOC0A/B on P23/P17 instead. This function:
 *   1. Disables PE5/PE2 peripheral output (clear PMR + clear PFS).
 *   2. Enables P23/P17 as GTIOC0A/B (PFS = 0x14, PMR = 1).
 * Must be called AFTER rx_gptw_init_pwm() returns. */
static void repin_gtioc0_to_p23_p17(void)
{
    /* Undo HAL's peripheral mode on PE5/PE2 first, before changing PFS. */
    REG8(PORTE_PMR) &= (uint8_t)~((1U << 5) | (1U << 2));

    /* Enable PFS writes. */
    REG8(PWPR) = PWPR_UNLOCK_STAGE1;
    REG8(PWPR) = PWPR_UNLOCK_STAGE2;

    /* Disable PE5/PE2 routing and enable P23/P17 routing. */
    REG8(PE5PFS) = PFS_DISABLED;
    REG8(PE2PFS) = PFS_DISABLED;
    REG8(P23PFS) = PFS_GPTW;
    REG8(P17PFS) = PFS_GPTW;

    /* Re-protect PFS. */
    REG8(PWPR) = PWPR_UNLOCK_STAGE1;
    REG8(PWPR) = PWPR_LOCK;

    /* Switch P23/P17 to peripheral mode. */
    REG8(PORT2_PMR) |= (uint8_t)(1U << 3); /* P23 */
    REG8(PORT1_PMR) |= (uint8_t)(1U << 7); /* P17 */
}

/* Override GTPR/GTPBR post-init to compensate for the HAL's hard-coded
 * 120 MHz PCLKA assumption. Our actual PCKA is 96 MHz, so we need
 * GTPR = 96M/20k - 1 = 4799 for a true 20 kHz waveform. */
static void fix_gtpr_for_96mhz_pcka(void)
{
    REG32(GPTW0_GTWP) = GPTW_GTWP_UNLOCK;
    REG32(GPTW0_GTPR)  = k_pwm_gtpr;
    REG32(GPTW0_GTPBR) = k_pwm_gtpr;
    REG32(GPTW0_GTWP) = GPTW_GTWP_LOCK;
}

/* Drive the PA7 heartbeat LED and sweep the PWM duty cycle 0..100..0 forever.
 * Uses rx_gptw_set_duty_raw for speed (avoids float dispatch). */
static void run_forever(void)
{
    REG8(PORTA_PDR)  |= PA7_MASK;
    REG8(PORTA_PODR) &= (uint8_t)~PA7_MASK;

    uint32_t duty      = 0U;
    uint32_t direction = 1U;

    for (;;) {
        /* Same duty on both A and B (both outputs mirror each other for scope
         * visibility). For a real H-bridge one output would be fixed direction
         * and the other the variable-duty PWM. */
        (void)rx_gptw_set_duty_raw(k_gptw_channel_0, k_gptw_output_a, duty);
        (void)rx_gptw_set_duty_raw(k_gptw_channel_0, k_gptw_output_b, duty);

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

int main(void)
{
    /* Step 1: HOCO x12 -> 192 MHz PLL, PCKA = 96 MHz. */
    clock_init();

    /* Step 2: Pre-enable the GPTW module using the correct MSTPCRA.MSTPA7 bit.
     * The HAL will also try to enable it via MSTPCRC.MSTPC6 (bogus for RX72N)
     * but that write is harmless on whatever unrelated bit it touches. */
    enable_gptw_module_mstpa7();

    /* Step 3: Call the HAL to configure GPTW0 as 20 kHz sawtooth PWM with
     * 0% initial duty. The HAL will compute GTPR = 120M/20k = 6000 based on
     * its assumed PCKA; we fix that in step 5. The HAL also configures MPC
     * for PE5/PE2 and starts the timer. */
    const rx_gptw_config_t config = {
        .frequency_hz         = k_pwm_freq_hz,
        .deadtime_ns          = 0U,
        .wave_mode            = k_gptw_wave_saw_pwm,
        .enable_complementary = false,
        .invert_polarity      = false,
    };
    rx_err_t err = rx_gptw_init_pwm(k_gptw_channel_0, &config);
    if (err != k_rx_ok) {
        /* Fatal: flash LED fast forever so we know something went wrong. */
        REG8(PORTA_PDR) |= PA7_MASK;
        for (;;) {
            REG8(PORTA_PODR) ^= PA7_MASK;
            delay_loops(8000U);
        }
    }

    /* Step 4: Re-route the GPTW0 outputs from PE5/PE2 to P23/P17. */
    repin_gtioc0_to_p23_p17();

    /* Step 5: Fix GTPR/GTPBR for our real 96 MHz PCKA so we actually run at
     * 20 kHz instead of the HAL-assumed 16 kHz (120->96 MHz difference). */
    fix_gtpr_for_96mhz_pcka();

    /* Step 6: Set initial 50% duty on both outputs. Use raw counts because
     * the HAL's cached period (s_gptw_period) still reflects 6000, which
     * would make rx_gptw_set_duty percentages miscalculate. */
    (void)rx_gptw_set_duty_raw(k_gptw_channel_0, k_gptw_output_a, k_pwm_duty_50pct);
    (void)rx_gptw_set_duty_raw(k_gptw_channel_0, k_gptw_output_b, k_pwm_duty_50pct);

    /* Step 7: Ensure timer is running (HAL already started it, but idempotent). */
    (void)rx_gptw_start(k_gptw_channel_0);

    /* Step 8: Sweep duty forever so a scope sees a visible triangle envelope. */
    run_forever();

    return 0;
}
