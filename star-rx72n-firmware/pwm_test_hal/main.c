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
 * The STAR HAL previously contained four bugs that this test first
 * exposed: (1) wrong module-stop bit (MSTPCRC.MSTPC6 instead of
 * MSTPCRA.MSTPA7), (2) missing GTUDDTYC write (counter defaulted to
 * down-count), (3) default single-buffer GTBER (duty writes swallowed),
 * (4) hard-coded 120 MHz PCKA (real PCKA = 96 MHz). Those bugs are now
 * fixed in rx_gptw.c / rx72n_gptw_regs.h / rx72n_clock.h, so the only
 * board-specific adjustment left here is the PE5/PE2 -> P23/P17 re-pin.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdbool.h>
#include <stdint.h>

#include "rx_gptw.h"

extern void clock_init(void);

#define REG8(a)  (*(volatile uint8_t  *)(a))

/* MPC pin function select */
#define PWPR       0x0008C11FU
#define P17PFS     0x0008C14FU /* pin 38 on Tom board */
#define P23PFS     0x0008C153U /* pin 34 on Tom board */
#define PE2PFS     0x0008C172U /* HAL default GTIOC0B -- we disable this */
#define PE5PFS     0x0008C175U /* HAL default GTIOC0A -- we disable this */

#define PWPR_UNLOCK_STAGE1 0x00U /* clear B0WI */
#define PWPR_UNLOCK_STAGE2 0x40U /* set PFSWE */
#define PWPR_LOCK          0x80U /* set B0WI */

/* P17/P23 use PSEL = 0b011110 = 0x1E for GTIOC0A/B per RX72N HW manual
 * Ch.23 Table 23.4 (P17) and Table 23.6 (P23). 0x14 is the correct PSEL
 * for GTIOC on the Port E family (PE5/PE2), but Ports 1/2 use a
 * different encoding. */
#define PFS_GPTW_P17_P23  0x1EU
#define PFS_DISABLED      0x00U

/* PORT peripheral mode / direction */
#define PORT1_PMR  0x0008C061U
#define PORT2_PMR  0x0008C062U
#define PORTE_PMR  0x0008C06EU

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
    REG8(P23PFS) = PFS_GPTW_P17_P23;
    REG8(P17PFS) = PFS_GPTW_P17_P23;

    /* Re-protect PFS. */
    REG8(PWPR) = PWPR_UNLOCK_STAGE1;
    REG8(PWPR) = PWPR_LOCK;

    /* Switch P23/P17 to peripheral mode. */
    REG8(PORT2_PMR) |= (uint8_t)(1U << 3); /* P23 */
    REG8(PORT1_PMR) |= (uint8_t)(1U << 7); /* P17 */
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

    /* Step 2: Call the HAL to configure GPTW0 as 20 kHz sawtooth PWM with
     * 0% initial duty. The HAL:
     *   - releases the module-stop bit (MSTPCRA.MSTPA7, fixed from MSTPCRC.MSTPC6)
     *   - computes GTPR = 96M/20k - 1 = 4799 (fixed from 120 MHz assumption)
     *   - writes GTUDDTYC = UD|UDF for up-count (fixed: was defaulting to 0)
     *   - writes GTBER = 0 for immediate GTCCRA/B updates (fixed: was single-buf)
     *   - configures MPC for PE5/PE2 and starts the timer.
     * After this call we only need to re-pin outputs to our board (step 3)
     * and set the desired duty (step 4). */
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

    /* Step 3: Re-route the GPTW0 outputs from PE5/PE2 (HAL default) to the
     * P23/P17 pins Tom's breakout board exposes. This is legitimate
     * board-specific routing, not a bug workaround. */
    repin_gtioc0_to_p23_p17();

    /* Step 4: Set 50% duty on both outputs. With GTBER=0 these writes take
     * effect immediately. */
    (void)rx_gptw_set_duty_raw(k_gptw_channel_0, k_gptw_output_a, k_pwm_duty_50pct);
    (void)rx_gptw_set_duty_raw(k_gptw_channel_0, k_gptw_output_b, k_pwm_duty_50pct);

    /* Step 5: Static 50% duty forever (diagnostic).  PA7 heartbeat. */
    REG8(PORTA_PDR) |= PA7_MASK;
    for (;;) {
        REG8(PORTA_PODR) ^= PA7_MASK;
        delay_loops(200000U);
    }
    (void)run_forever;

    return 0;
}
