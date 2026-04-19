/**
 * @file main.c
 * @brief PWM driven via rx_motor library on RX72N Motor 0 (re-routed to P23/P17).
 *
 * @details
 * This test exercises the production STAR motor driver path -- rx_motor_init()
 * -> rx_gptw_init_pwm() -> internal MPC/PMR configuration -- then patches the
 * pin routing so GTIOC0A/GTIOC0B come out on P23 (pin 34) and P17 (pin 38) of
 * Tom's breakout board instead of the default PE5/PE2.
 *
 * Pin mapping (scope):
 *   - Tom header pin 34 -> P23 / GTIOC0A -> "IN2" (scope Ch2, blue)
 *   - Tom header pin 38 -> P17 / GTIOC0B -> "IN1" (scope Ch1, orange)
 *
 * After initialization we drive the motor through a -100..+100..-100 sweep
 * so the scope sees the speed PWM on whichever output is active (IN1 for
 * forward, IN2 for reverse) with the other pin held at 0%.
 *
 * nSLEEP / DRVOFF: driven HIGH / LOW respectively on convenient GPIOs so any
 * attached DRV8263 would run; with no DRV present these are harmless.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "rx_motor.h"
#include "rx_gptw.h"
#include "rx72n_regs.h"
#include "rx72n_gptw_regs.h"

extern void clock_init(void);

/* ==========================================================================
 * Direct register helpers for MPC/PORT patch-up
 * ========================================================================== */
#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))

#define PWPR      0x0008C11FU
#define P17PFS    0x0008C14FU  /* Port 1 Pin 7  -> GTIOC0B (pin 38) */
#define P23PFS    0x0008C153U  /* Port 2 Pin 3  -> GTIOC0A (pin 34) */
#define PE2PFS    0x0008C17AU  /* Port E Pin 2  -> default GTIOC0B */
#define PE5PFS    0x0008C17DU  /* Port E Pin 5  -> default GTIOC0A */

#define PORT1_PMR  0x0008C061U
#define PORT1_PDR  0x0008C001U
#define PORT2_PMR  0x0008C062U
#define PORT2_PDR  0x0008C002U
#define PORTE_PMR  0x0008C06EU
#define PORTE_PDR  0x0008C00EU

/* nSLEEP / DRVOFF helper GPIOs.
 *   nSLEEP -> PA7 (pin 88), driven HIGH
 *   DRVOFF -> PA6 (pin 89), driven LOW
 * No DRV attached -- these pins just park at known levels. */
#define PORTA_PMR  0x0008C06AU
#define PORTA_PDR  0x0008C00AU
#define PORTA_PODR 0x0008C02AU

/* Heartbeat LED on PA4 so we can see firmware is alive with a 3rd scope probe. */
#define PA4_MASK  (1U << 4)
#define PA7_MASK  (1U << 7)
#define PA6_MASK  (1U << 6)

#define PSEL_GPTW 0x14U

/* ==========================================================================
 * Low-level delay used in the duty sweep.
 * ========================================================================== */
static inline void delay_loops(uint32_t n)
{
    for (volatile uint32_t i = 0U; i < n; i++) {
        __asm__ volatile("nop");
    }
}

/* ==========================================================================
 * reroute_motor0_to_p23_p17()
 *
 * The HAL's rx_gptw_init_pwm() wires GTIOC0A/GTIOC0B to PE5/PE2 via MPC PSEL
 * = 0x14 and sets PORTE.PMR bits 5,2.  This function undoes that and instead
 * routes GPTW0 to P23/P17 (Tom board scope pins 34 / 38).
 *
 * Sequence:
 *   1. Unlock MPC PWPR (B0WI=0, then PFSWE=1).
 *   2. Return PE5/PE2 to GPIO (PMR=0, PFS=0).
 *   3. Program P23PFS and P17PFS with PSEL = 0x14 (GPTW alt function).
 *   4. Set PORT2.PMR bit 3 (P23) and PORT1.PMR bit 7 (P17).
 *   5. Re-lock MPC (PWPR = B0WI).
 *
 * GPTW counter / GTIOR are untouched -- the peripheral keeps producing PWM;
 * we're only changing which pad the signal reaches.
 * ========================================================================== */
static void reroute_motor0_to_p23_p17(void)
{
    /* 1. Unlock PFS writes. */
    REG8(PWPR) = 0x00U;   /* clear B0WI */
    REG8(PWPR) = 0x40U;   /* set PFSWE  */

    /* 2. Demote PE5/PE2 to GPIO (safe state -- tri-state by default). */
    REG8(PORTE_PMR) &= (uint8_t)~((1U << 5) | (1U << 2));
    REG8(PE5PFS) = 0x00U;
    REG8(PE2PFS) = 0x00U;

    /* 3. Program Motor 0 pins on P23 / P17. */
    REG8(P23PFS) = PSEL_GPTW;
    REG8(P17PFS) = PSEL_GPTW;

    /* 4. Re-lock PFS. */
    REG8(PWPR) = 0x00U;
    REG8(PWPR) = 0x80U;

    /* 5. Enable peripheral mode on the new pins. */
    REG8(PORT2_PMR) |= (uint8_t)(1U << 3); /* P23 -> GTIOC0A */
    REG8(PORT1_PMR) |= (uint8_t)(1U << 7); /* P17 -> GTIOC0B */
}

/* ==========================================================================
 * Helper GPIOs: park nSLEEP=HIGH, DRVOFF=LOW, heartbeat LED on PA4.
 * ========================================================================== */
static void setup_drv_helper_gpios(void)
{
    /* All three are GPIO outputs. PMR=0 already (reset default); set PDR=1. */
    REG8(PORTA_PDR)  |= (uint8_t)(PA7_MASK | PA6_MASK | PA4_MASK);
    /* nSLEEP HIGH, DRVOFF LOW, LED LOW */
    REG8(PORTA_PODR) |= (uint8_t)PA7_MASK;
    REG8(PORTA_PODR) &= (uint8_t)~PA6_MASK;
    REG8(PORTA_PODR) &= (uint8_t)~PA4_MASK;
}

static void toggle_heartbeat(void)
{
    REG8(PORTA_PODR) ^= (uint8_t)PA4_MASK;
}

/* ==========================================================================
 * Duty sweep:  -100..+100..-100  in steps of 5, ~40 ms per step.
 * ========================================================================== */
static void run_sweep_forever(rx_motor_handle_t* motor)
{
    float duty = -100.0F;
    int   dir  = +1;                /* +1 = increasing, -1 = decreasing */

    for (;;) {
        (void)rx_motor_set_duty(motor, duty);
        toggle_heartbeat();
        delay_loops(400000U);       /* ~40 ms at 96 MHz w/ O0, loose tuning */

        duty += (float)dir * 5.0F;
        if (duty >= 100.0F) {
            duty = 100.0F;
            dir  = -1;
        } else if (duty <= -100.0F) {
            duty = -100.0F;
            dir  = +1;
        }
    }
}

/* ==========================================================================
 * main
 * ========================================================================== */
int main(void)
{
    clock_init();                   /* HOCO *12 -> PLL 192 MHz, PCKA = 96 MHz */

    setup_drv_helper_gpios();       /* nSLEEP=HI, DRVOFF=LO, LED GPIO output */

    /* STAR Motor 0 (DRV8263H IN/IN): 20 kHz PWM, 1 us dead-time, active high. */
    static rx_motor_handle_t motor0 = {};
    const rx_motor_config_t  cfg    = {
        .channel      = k_gptw_channel_0,
        .output_a     = k_gptw_output_a,  /* GTIOC0A -> IN2 */
        .output_b     = k_gptw_output_b,  /* GTIOC0B -> IN1 */
        .pwm_freq_hz  = 20000U,
        .dead_time_ns = 1000U,
        .invert_pwm   = false,
    };

    rx_err_t err = rx_motor_init(&motor0, &cfg);
    if (err != k_rx_ok) {
        /* Park heartbeat LED solid on as an error indicator, then spin. */
        REG8(PORTA_PODR) |= (uint8_t)PA4_MASK;
        for (;;) { __asm__ volatile("nop"); }
    }

    /* rx_motor_init -> rx_gptw_init_pwm -> internal_configure_mpc() configured
     * PE5/PE2.  Re-route to P23/P17 for Tom's breakout header. */
    reroute_motor0_to_p23_p17();

    /* Quick sanity kick: 50% forward so the scope has an immediate target. */
    (void)rx_motor_set_duty(&motor0, 50.0F);

    /* Now loop forever sweeping -100..+100..-100 so the scope can see both
     * forward and reverse duty patterns on the two pads. */
    run_sweep_forever(&motor0);
    return 0;
}

/* =============================================================================
 * Stubs -- symbols referenced by rx_check.h / rx_log.h / vectors.S that this
 * minimal firmware doesn't actually exercise.  Keeps the linker happy without
 * dragging in the ThreadX RTOS, CMT0 driver, or framed-UART log backend.
 * ============================================================================= */

/* uart_debug_* are called by internal_rx_fatal_error() in rx_check.h.  No UART
 * is wired up in this test, so just no-op.  If a fatal assert fires we'll spin
 * in the while(1) wait at the end of internal_rx_fatal_error() anyway. */
void uart_debug_putc(char c)                        { (void)c; }
void uart_debug_puts(const char* s)                 { (void)s; }
void uart_debug_putint(int32_t v)                   { (void)v; }
void uart_debug_putuint(uint32_t v)                 { (void)v; }
void uart_debug_puthex(uint32_t v, uint8_t digits)  { (void)v; (void)digits; }

/* rx_log_uart_* are called by rx_log_error/warn/info macros on hardware.
 * Replace with no-ops to avoid pulling in rx_log_uart.c (ThreadX dependency). */
void rx_log_uart_putc(char c)                       { (void)c; }
void rx_log_uart_puts(const char* s)                { (void)s; }
void rx_log_uart_putint(int32_t v)                  { (void)v; }
void rx_log_uart_putuint(uint32_t v)                { (void)v; }
void rx_log_uart_puthex(uint32_t v, uint8_t digits) { (void)v; (void)digits; }

/* ThreadX / CMT0 symbols referenced by vectors.S. */
void _tx_thread_context_save(void)    {}
void _tx_thread_context_restore(void) {}
void cmt0_isr(void)                   {}
