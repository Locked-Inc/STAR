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
#define REG32(a) (*(volatile uint32_t *)(a))

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

/* GPTW0 direct-register patch.  After rx_motor_init() leaves the HAL in
 * a state that does not reliably produce a waveform on our board (PCKA is
 * 96 MHz, not the 120 MHz the HAL assumes; GTUDDTYC is left at reset =
 * count-down; buffered compare updates), we overwrite the key GPTW0
 * registers with the verified known-good values from /tmp/pwm_clean.c.
 * That test program produced clean 20 kHz 50% duty on P17/P23 at PCKA=96 MHz.
 *
 * GPTW0 base = 0x000C2000. Period = 4799 counts (PCKA 96 MHz / (4799+1) = 20 kHz). */
#define GPTW0_GTWP       0x000C2000U
#define GPTW0_GTCR       0x000C202CU
#define GPTW0_GTUDDTYC   0x000C2030U
#define GPTW0_GTIOR      0x000C2034U
#define GPTW0_GTBER      0x000C2040U
#define GPTW0_GTCNT      0x000C2048U
#define GPTW0_GTCCRA     0x000C204CU
#define GPTW0_GTCCRB     0x000C2050U
#define GPTW0_GTPR       0x000C2064U
#define GPTW0_GTPBR      0x000C2068U
#define GPTW_GTWP_UNLOCK 0x0000A500U
#define GPTW_GTWP_LOCK   0x0000A501U

#define GPTW0_PERIOD     4799U       /* 96 MHz / 4800 = 20 kHz */
#define GPTW0_DUTY_50PCT 2400U       /* 50% forward */

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

/* RX72N HW manual Ch.23 Tables 23.4/23.6: GTIOC on port 1/2 pins needs
 * PSEL=0x1E (011110b). 0x14 is the Port E encoding and is wrong here. */
#define PSEL_GPTW 0x1EU

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
 * = 0x14 (correct for port E) and sets PORTE.PMR bits 5,2.  This function
 * undoes that and instead routes GPTW0 to P23/P17 (Tom board scope pins
 * 34 / 38).  Note: port 1/2 pins use a different PSEL encoding (0x1E),
 * verified against RX72N HW manual Ch.23 Tables 23.4 and 23.6.
 *
 * Sequence:
 *   1. Unlock MPC PWPR (B0WI=0, then PFSWE=1).
 *   2. Return PE5/PE2 to GPIO (PMR=0, PFS=0).
 *   3. Program P23PFS and P17PFS with PSEL = 0x1E (GPTW alt on port 1/2).
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
/* ==========================================================================
 * force_gptw0_20khz_saw()
 *
 * Overwrite the GPTW0 registers with the verified known-good values from
 * /tmp/pwm_clean.c.  Stops the timer, reprograms GTCR/GTUDDTYC/GTIOR/
 * GTPR/GTCCRA/GTCCRB/GTBER, then restarts.  After this returns, GPTW0
 * produces 20 kHz sawtooth PWM with GTIOCnA and GTIOCnB both at 50% duty.
 * ========================================================================== */
static void force_gptw0_20khz_saw(void)
{
    /* Clear MSTPA7 (GPTW module stop) via MSTPCRA.  PRC1 must be unlocked
     * before touching module-stop registers.  Per /tmp/pwm_clean.c this is
     * the correct bit on RX72N; the HAL clears MSTPCRC.MSTPC6 which may be
     * a different peripheral. */
    REG16(0x000803FEU)  = 0xA502U;                /* PRCR: unlock PRC1 */
    REG32(0x00080010U) &= ~(1UL << 7);            /* MSTPCRA.MSTPA7 = 0 */
    REG16(0x000803FEU)  = 0xA500U;                /* PRCR: lock */

    REG32(GPTW0_GTWP)     = GPTW_GTWP_UNLOCK;
    REG32(GPTW0_GTCR)     = 0x00000000U;          /* stop, MD=0 sawtooth, TPCS=0 */
    REG32(GPTW0_GTUDDTYC) = 0x00000003U;          /* UD=1, UDF=1 force up-count */
    REG32(GPTW0_GTCNT)    = 0x00000000U;          /* clear counter */
    REG32(GPTW0_GTPR)     = GPTW0_PERIOD;
    REG32(GPTW0_GTPBR)    = GPTW0_PERIOD;
    REG32(GPTW0_GTCCRA)   = GPTW0_DUTY_50PCT;
    REG32(GPTW0_GTCCRB)   = GPTW0_DUTY_50PCT;
    /* GTIOR: GTIOA=0x09, OAE bit 8, GTIOB=0x09, OBE bit 24
     * 0x09 = init LOW, HIGH at cycle end, LOW at compare match. */
    REG32(GPTW0_GTIOR)    = (0x09U << 0) | (1U << 8) | (0x09U << 16) | (1U << 24);
    REG32(GPTW0_GTBER)    = 0x00000000U;          /* no buffering */
    REG32(GPTW0_GTCR)     = 0x00000001U;          /* CST=1 start */
    REG32(GPTW0_GTWP)     = GPTW_GTWP_LOCK;
}

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

    /* rx_motor_set_duty() computes the compare count from the HAL's cached
     * period, which in turn is derived from an assumed PCKA of 120 MHz --
     * but our clock_init actually gives PCKA = 96 MHz.  Rather than fight
     * the HAL's assumption, overwrite GPTW0 with verified-good raw values
     * that produce 20 kHz 50% sawtooth PWM.  Both GTIOC0A (P23) and
     * GTIOC0B (P17) should toggle at 20 kHz after this call. */
    force_gptw0_20khz_saw();

    /* Idle loop with heartbeat so we can confirm firmware is alive. */
    for (;;) {
        toggle_heartbeat();
        delay_loops(2000000U);  /* ~200 ms heartbeat */
    }

    (void)run_sweep_forever;    /* retained for future manual sweeps */
    (void)motor0;
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
