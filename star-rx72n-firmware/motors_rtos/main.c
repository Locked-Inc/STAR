/**
 * @file main.c
 * @brief motors_rtos -- motors + encoders under Azure RTOS ThreadX.
 *
 * @details
 * Two-task ThreadX bring-up of libs/rx_motor + libs/rx_encoder, proving they
 * coexist with the scheduler.  Mirrors encoder_test's open-loop spin + count
 * logic but splits the work across two cooperating tasks:
 *
 *   motor_task    (priority 10) -- every 500 ms, advances a duty-sweep state
 *                                   machine.  All 4 motors take the same
 *                                   duty, with per-motor direction_sign applied
 *                                   so FL/BL spin the same wheel direction as
 *                                   FR/BR despite the reversed wiring.
 *
 *   encoder_task  (priority  9) -- every 100 ms, reads all 4 encoder TCNTs,
 *                                   prints a one-line CSV over SCI9 (115200,
 *                                   /dev/ttyACM0 on the host), and toggles the
 *                                   PA7/D9 heartbeat LED.  Higher priority
 *                                   than motor_task so the console stays
 *                                   responsive even when motor_task is awake.
 *
 * Boot sequence (main):
 *   1. clock_init()         -- PLL 240 MHz / PCLKA 120 / PCLKB 60
 *   2. sci9_debug_init()    -- console up; banner printed immediately
 *   3. motor_drv_gpio_init  -- DRVOFF high, nSLEEP high, tWAKE 10 ms, DRVOFF low
 *   4. motor_pwm_init       -- rx_motor_init x4 at 20 kHz, 1 us deadtime
 *   5. encoders_init        -- MTU1/MTU2/TPU1/TPU2 phase counting mode 1
 *   6. cmt0_init            -- 100 Hz ThreadX tick + setpsw i
 *   7. tx_kernel_enter      -- hands off forever
 *
 * Per memory project_motor_pwm_hal_bugs.md: the production rx_mpc / rx_gptw
 * stack has latent issues writing PFS for MTCLK/TCLK encoder pins, so the
 * encoders_init() routine here mirrors encoder_test's direct-register workaround
 * rather than calling rx_mpc_set_mtu_encoder() / rx_mpc_set_tpu_encoder().
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "tx_api.h"

#include "rx_motor.h"
#include "rx_gptw.h"
#include "rx_mpc.h"
#include "rx_port_constants.h"
#include "rx_mtu_encoder.h"
#include "rx_encoder_tpu.h"
#include "rx72n_mtu_regs.h"
#include "rx72n_tpu_regs.h"
#include "rx72n_system_regs.h"
#include "rx72n_icu_regs.h"
#include "rx_register_protection.h"

/* Symbols defined in sibling files */
extern void clock_init(void);
extern void cmt0_init(void);
extern void sci9_debug_init(void);
extern void sci9_debug_putc(char c);
extern void sci9_debug_puts(const char *s);
extern void sci9_debug_puthex16(uint16_t v);
extern void sci9_debug_puthex32(uint32_t v);

/* ==========================================================================
 * Direct port-register access helpers (same pattern as encoder_test).
 * ========================================================================== */
#define REG8(a) (*(volatile uint8_t *)(uintptr_t)(a))

typedef enum : uintptr_t {
    k_port_pdr_base  = 0x0008C000U,
    k_port_podr_base = 0x0008C020U,
    k_port_pmr_base  = 0x0008C060U,
} port_reg_base_t;

static inline volatile uint8_t *port_pdr(uint8_t port)
{
    return &REG8(k_port_pdr_base + port);
}
static inline volatile uint8_t *port_podr(uint8_t port)
{
    return &REG8(k_port_podr_base + port);
}
static inline volatile uint8_t *port_pmr(uint8_t port)
{
    return &REG8(k_port_pmr_base + port);
}

static inline void gpio_set_output(uint8_t port, uint8_t pin)
{
    *port_pmr(port) &= (uint8_t) ~(1U << pin);
    *port_pdr(port) |= (uint8_t)(1U << pin);
}
static inline void gpio_write(uint8_t port, uint8_t pin, bool high)
{
    if (high) {
        *port_podr(port) |= (uint8_t)(1U << pin);
    } else {
        *port_podr(port) &= (uint8_t) ~(1U << pin);
    }
}
static inline void gpio_toggle(uint8_t port, uint8_t pin)
{
    *port_podr(port) ^= (uint8_t)(1U << pin);
}

/* ==========================================================================
 * Status LED map (matches encoder_test, matches memory project_star_pcb_leds.md)
 * ========================================================================== */
typedef enum : uint8_t {
    k_port_7   = 7,
    k_port_a   = 10,
    k_port_b   = 11,
    k_port_e   = 14,
    k_port_6   = 6,

    k_bit_led_heartbeat = 7, /**< PA7 -- D9, toggled 10 Hz by encoder_task */
    k_bit_led_init_done = 0, /**< PB0 -- D10, solid once main() finishes init */
    k_bit_led_motor_run = 1, /**< P71 -- D11, toggles on every motor_task step */
    k_bit_led_enc_tick  = 2, /**< P72 -- D12, toggles per encoder sample */
    k_bit_led_motor_alive = 1, /**< PB1 -- D13, proves motor_task reached loop body */
} status_led_pins_t;

static void leds_init(void)
{
    gpio_set_output(k_port_a, k_bit_led_heartbeat);
    gpio_write(k_port_a, k_bit_led_heartbeat, false);

    gpio_set_output(k_port_b, k_bit_led_init_done);
    gpio_write(k_port_b, k_bit_led_init_done, false);

    gpio_set_output(k_port_7, k_bit_led_motor_run);
    gpio_write(k_port_7, k_bit_led_motor_run, false);

    gpio_set_output(k_port_7, k_bit_led_enc_tick);
    gpio_write(k_port_7, k_bit_led_enc_tick, false);

    gpio_set_output(k_port_b, k_bit_led_motor_alive);
    gpio_write(k_port_b, k_bit_led_motor_alive, false);
}

/* ==========================================================================
 * Per-motor wiring (mirrors encoder_test / src/inc/hardware_config.h).
 *
 * | Idx | Wheel | GPTW | IN1  | IN2  | DRVOFF | nSLEEP | Encoder |
 * |-----|-------|------|------|------|--------|--------|---------|
 * |  0  | FL    | 0    | P17  | P23  | P61    | P60    | MTU1    |
 * |  1  | FR    | 1    | PC3  | P22  | P63    | P62    | MTU2    |
 * |  2  | BR    | 2    | P86  | PE3  | PE0    | P64    | TPU1    |
 * |  3  | BL    | 3    | PC6  | PE7  | PE2    | PE1    | TPU2    |
 *
 * direction_sign compensates for the physical wiring: M0 (FL) and M3 (BL)
 * are reversed on this PCB revision, so +50% duty for "forward" applies as
 * -50% to those channels.
 * ========================================================================== */
typedef struct {
    rx_gptw_channel_t gptw_channel;
    rx_port_pin_t     in2_pin;
    rx_port_pin_t     in1_pin;
    uint8_t           drvoff_port;
    uint8_t           drvoff_pin;
    uint8_t           nsleep_port;
    uint8_t           nsleep_pin;
    bool              is_tpu;
    uint8_t           timer_channel;
    int8_t            direction_sign;
} motor_wiring_t;

typedef enum : uint8_t {
    k_motor_count = 4,
} motor_const_t;

static const motor_wiring_t k_motors[k_motor_count] = {
    /* M0 = FL, wired backwards */
    {.gptw_channel = k_gptw_channel_0, .in2_pin = k_rx_p2_3, .in1_pin = k_rx_p1_7,
     .drvoff_port = k_port_6, .drvoff_pin = 1, .nsleep_port = k_port_6, .nsleep_pin = 0,
     .is_tpu = false, .timer_channel = 1, .direction_sign = -1},
    /* M1 = FR */
    {.gptw_channel = k_gptw_channel_1, .in2_pin = k_rx_p2_2, .in1_pin = k_rx_pc_3,
     .drvoff_port = k_port_6, .drvoff_pin = 3, .nsleep_port = k_port_6, .nsleep_pin = 2,
     .is_tpu = false, .timer_channel = 2, .direction_sign = +1},
    /* M2 = BR */
    {.gptw_channel = k_gptw_channel_2, .in2_pin = k_rx_pe_3, .in1_pin = k_rx_p8_6,
     .drvoff_port = k_port_e, .drvoff_pin = 0, .nsleep_port = k_port_6, .nsleep_pin = 4,
     .is_tpu = true,  .timer_channel = 1, .direction_sign = +1},
    /* M3 = BL, wired backwards */
    {.gptw_channel = k_gptw_channel_3, .in2_pin = k_rx_pe_7, .in1_pin = k_rx_pc_6,
     .drvoff_port = k_port_e, .drvoff_pin = 2, .nsleep_port = k_port_e, .nsleep_pin = 1,
     .is_tpu = true,  .timer_channel = 2, .direction_sign = -1},
};

/* ==========================================================================
 * Bounded busy-wait (~1 ms per unit at 240 MHz ICLK, -O1, volatile loop).
 * Only used in main() before tx_kernel_enter() for the DRV8263 tWAKE window;
 * after that, every delay goes through tx_thread_sleep().
 * ========================================================================== */
typedef enum : uint32_t {
    k_iters_per_ms = 40000U,
} delay_const_t;

static void busy_wait_ms(uint32_t ms)
{
    for (uint32_t m = 0; m < ms; m++) {
        for (volatile uint32_t i = 0; i < k_iters_per_ms; i++) {
            __asm__ volatile("nop");
        }
    }
}

/* ==========================================================================
 * DRV8263H power-up: DRVOFF high first, nSLEEP high, 10 ms tWAKE, DRVOFF low.
 * ========================================================================== */
static void motor_drv_gpio_init(void)
{
    for (uint8_t i = 0; i < k_motor_count; i++) {
        gpio_set_output(k_motors[i].drvoff_port, k_motors[i].drvoff_pin);
        gpio_write(k_motors[i].drvoff_port, k_motors[i].drvoff_pin, true);
    }
    for (uint8_t i = 0; i < k_motor_count; i++) {
        gpio_set_output(k_motors[i].nsleep_port, k_motors[i].nsleep_pin);
        gpio_write(k_motors[i].nsleep_port, k_motors[i].nsleep_pin, true);
    }
    busy_wait_ms(10);
    for (uint8_t i = 0; i < k_motor_count; i++) {
        gpio_write(k_motors[i].drvoff_port, k_motors[i].drvoff_pin, false);
    }
}

/* ==========================================================================
 * GPTW PWM init via rx_motor lib (same config as motor_spin_test).
 * ========================================================================== */
typedef enum : uint32_t {
    k_pwm_freq_hz  = 20000U,
    k_deadtime_ns  = 1000U,
} pwm_const_t;

static bool motor_pwm_init(rx_motor_handle_t handles[k_motor_count])
{
    for (uint8_t i = 0; i < k_motor_count; i++) {
        const rx_motor_config_t cfg = {
            .channel      = k_motors[i].gptw_channel,
            .output_a     = k_gptw_output_a,
            .output_b     = k_gptw_output_b,
            .pwm_freq_hz  = k_pwm_freq_hz,
            .dead_time_ns = k_deadtime_ns,
            .invert_pwm   = false,
            .port_a_idx   = (uint8_t)rx_port_from_pin(k_motors[i].in2_pin),
            .bit_a        = (uint8_t)rx_pin_from_pin(k_motors[i].in2_pin),
            .port_b_idx   = (uint8_t)rx_port_from_pin(k_motors[i].in1_pin),
            .bit_b        = (uint8_t)rx_pin_from_pin(k_motors[i].in1_pin),
        };
        if (rx_motor_init(&handles[i], &cfg) != k_rx_ok) {
            return false;
        }
    }
    return true;
}

/* ==========================================================================
 * Encoder init -- direct-register, bypassing rx_mpc / rx_mtu_start which
 * don't actually write PFS or TSTRA on this HAL revision.  Copied verbatim
 * from the known-good encoder_test/main.c encoders_init() routine.
 * ========================================================================== */
#define REG8_AT(a)  (*(volatile uint8_t  *)(uintptr_t)(a))
#define REG16_AT(a) (*(volatile uint16_t *)(uintptr_t)(a))

static bool encoders_init(void)
{
    /* Step 1: release MTU (MSTPA9) + TPU (MSTPA13) module clocks. */
    *prcr_reg() = k_rx_prcr_unlock_all;
    system_regs()->mstpcra &= ~(uint32_t)((1UL << 9) | (uint32_t)k_tpu_mstpcra_mstpa13);
    *prcr_reg() = k_rx_prcr_lock;

    /* Step 2 + 3: MPC pin sequence PMR=0, write PFS, PMR=1 for every
     * encoder phase input. */
    volatile uint8_t *pwpr  = (volatile uint8_t *)0x0008C11FU;
    volatile uint8_t *p2pmr = (volatile uint8_t *)0x0008C062U;
    volatile uint8_t *papmr = (volatile uint8_t *)0x0008C06AU;
    volatile uint8_t *pbpmr = (volatile uint8_t *)0x0008C06BU;
    volatile uint8_t *pcpmr = (volatile uint8_t *)0x0008C06CU;

    *p2pmr &= (uint8_t)~(uint8_t)((1U << 4) | (1U << 5));
    *papmr &= (uint8_t)~(uint8_t)((1U << 1) | (1U << 3));
    *pcpmr &= (uint8_t)~(uint8_t)((1U << 0) | (1U << 2) | (1U << 5));
    *pbpmr &= (uint8_t)~(uint8_t)(1U << 3);

    *pwpr = 0x00U;
    *pwpr = 0x40U;
    REG8_AT(0x0008C140U + 2U * 8U + 4U)  = 0x02U; /* P24 MTCLKA */
    REG8_AT(0x0008C140U + 2U * 8U + 5U)  = 0x02U; /* P25 MTCLKB */
    REG8_AT(0x0008C140U + 10U * 8U + 1U) = 0x02U; /* PA1 MTCLKC */
    REG8_AT(0x0008C140U + 12U * 8U + 5U) = 0x02U; /* PC5 MTCLKD */
    REG8_AT(0x0008C140U + 12U * 8U + 2U) = 0x03U; /* PC2 TCLKA  */
    REG8_AT(0x0008C140U + 10U * 8U + 3U) = 0x04U; /* PA3 TCLKB  */
    REG8_AT(0x0008C140U + 12U * 8U + 0U) = 0x03U; /* PC0 TCLKC  */
    REG8_AT(0x0008C140U + 11U * 8U + 3U) = 0x04U; /* PB3 TCLKD  */
    *pwpr = 0x00U;
    *pwpr = 0x80U;

    *p2pmr |= (uint8_t)((1U << 4) | (1U << 5));
    *papmr |= (uint8_t)((1U << 1) | (1U << 3));
    *pcpmr |= (uint8_t)((1U << 0) | (1U << 2) | (1U << 5));
    *pbpmr |= (uint8_t)(1U << 3);

    /* Phase counting mode 1 on MTU1 + MTU2 */
    volatile rx_mtu_channel_regs_t *m1 = mtu1();
    m1->tcr = 0;  m1->tmdr = 0x04U;
    m1->tiorh = 0; m1->tiorl = 0;
    m1->tier = 0;  m1->tsr = 0;  m1->tcnt = 0;
    volatile rx_mtu_channel_regs_t *m2 = mtu2();
    m2->tcr = 0;  m2->tmdr = 0x04U;
    m2->tiorh = 0; m2->tiorl = 0;
    m2->tier = 0;  m2->tsr = 0;  m2->tcnt = 0;

    /* Phase counting mode 1 on TPU1 + TPU2 */
    volatile rx_tpu_regs_t *t1 = tpu1();
    t1->tcr = 0;  t1->tmdr = 0x04U;  t1->tcnt = 0;
    volatile rx_tpu_regs_t *t2 = tpu2();
    t2->tcr = 0;  t2->tmdr = 0x04U;  t2->tcnt = 0;

    /* Clear TPU noise filters from any prior-run leftover state */
    tpu_control()->nfcr[1] = 0;
    tpu_control()->nfcr[2] = 0;

    /* Start all four counters */
    mtu_tstra()->tstr   |= (uint8_t)(k_mtu_tstr_cst1 | k_mtu_tstr_cst2);
    tpu_control()->tstr |= (uint8_t)(k_tpu_tstr_cst1 | k_tpu_tstr_cst2);

    return true;
}

static uint16_t encoder_read_raw(uint8_t motor_idx)
{
    static const uintptr_t k_tcnt_addr[k_motor_count] = {
        0x000C1386U,       /* MTU1 */
        0x000C1406U,       /* MTU2 */
        0x00088120U + 6U,  /* TPU1 */
        0x00088130U + 6U,  /* TPU2 */
    };
    return REG16_AT(k_tcnt_addr[motor_idx]);
}

/* ==========================================================================
 * Console helpers -- thin wrappers around sci9_debug_* for readability.
 * ========================================================================== */
static void print_s16_dec(int16_t v)
{
    char buf[8];
    uint8_t  i       = 0;
    uint32_t u;
    bool     negative = v < 0;

    u = negative ? (uint32_t)(-(int32_t)v) : (uint32_t)v;
    if (u == 0U) {
        sci9_debug_putc('0');
        return;
    }
    if (negative) {
        sci9_debug_putc('-');
    }
    while (u != 0U) {
        buf[i++] = (char)('0' + (u % 10U));
        u /= 10U;
    }
    while (i-- > 0U) {
        sci9_debug_putc(buf[i]);
    }
}

/* ==========================================================================
 * Duty sweep state machine -- 21 duty steps from -100 to +100 in 10% steps,
 * reversed at the endpoints.  motor_task advances one step per wakeup.
 * ========================================================================== */
typedef enum : uint8_t {
    k_sweep_step_count = 21U, /**< -100, -90, ..., 0, ..., +100 */
} sweep_const_t;

static const int8_t k_sweep_table[k_sweep_step_count] = {
    -100, -90, -80, -70, -60, -50, -40, -30, -20, -10,
       0,
     +10, +20, +30, +40, +50, +60, +70, +80, +90, +100,
};

/* ==========================================================================
 * ThreadX objects
 * ========================================================================== */
typedef enum : uint16_t {
    k_motor_task_stack_sz   = 2048U,
    k_encoder_task_stack_sz = 2048U,
} rtos_stack_t;

typedef enum : uint8_t {
    k_encoder_task_priority = 9U,  /**< higher priority: prints CSV, keeps console responsive */
    k_motor_task_priority   = 10U, /**< lower priority: sweeps duty, preempted by encoder */
    k_motor_sleep_ticks     = 50U, /**< 50 * 10 ms = 500 ms */
    k_encoder_sleep_ticks   = 10U, /**< 10 * 10 ms = 100 ms */
} rtos_prio_t;

static TX_THREAD s_motor_tcb;
static TX_THREAD s_encoder_tcb;
static uint8_t   s_motor_stack[k_motor_task_stack_sz];
static uint8_t   s_encoder_stack[k_encoder_task_stack_sz];

/* Motor handles initialised in main() before the scheduler starts. */
static rx_motor_handle_t s_motors[k_motor_count];

/* Most recent motor duty -- written by motor_task, read by encoder_task for
 * CSV logging.  Signed volatile byte is atomic on RX so no mutex needed. */
static volatile int8_t s_current_duty = 0;

/* Diagnostic counters -- help identify *where* motor_task is making progress.
 * All three are volatile so the compiler can't optimise them away. */
static volatile uint32_t g_motor_task_entries     = 0U; /**< bumped once on task entry */
static volatile uint32_t g_motor_task_loops       = 0U; /**< bumped each iteration BEFORE set_duty */
static volatile uint32_t g_motor_task_loops_after = 0U; /**< bumped each iteration AFTER  set_duty */
static volatile uint32_t g_motor_create_status    = 0xFFFFFFFFU; /**< tx_thread_create return for motor_task */
static volatile uint32_t g_encoder_create_status  = 0xFFFFFFFFU; /**< tx_thread_create return for encoder_task */

extern volatile uint32_t g_cmt0_isr_count; /**< from cmt0.c -- verifies CMT0 tick ISR is firing */

/* ThreadX kernel internals -- used by Option 1/1b diagnostic in encoder_task.
 * tx_thread_sleep.c:89-118 has four TX_CALLER_ERROR (0x13) return paths:
 *   :89   thread_ptr == NULL          -> observe via cur
 *   :99   system_state != 0           -> observe via ss  (ISR wrapping issue)
 *   :111  thread_ptr == timer thread  -> observe via cur
 *   :132  preempt_disable != 0        -> observe via pd  (already ruled out: pd=0) */
extern UINT       _tx_thread_preempt_disable;
extern ULONG      _tx_thread_system_state;
extern TX_THREAD *_tx_thread_current_ptr;

/* ==========================================================================
 * motor_task -- step the duty sweep across all 4 motors every 500 ms.
 * ========================================================================== */
static void motor_task_entry(ULONG arg)
{
    (void)arg;

    g_motor_task_entries = 1U;
    gpio_write(k_port_b, k_bit_led_motor_alive, true);

    /* Explicit 0 duty on entry for a clean baseline before the sweep. */
    for (uint8_t i = 0; i < k_motor_count; i++) {
        (void)rx_motor_set_duty(&s_motors[i], 0.0F);
    }

    uint8_t step = 0U;

    for (;;) {
        gpio_toggle(k_port_7, k_bit_led_motor_run);

        const int8_t duty = k_sweep_table[step];
        s_current_duty = duty;

        for (uint8_t i = 0; i < k_motor_count; i++) {
            const float signed_duty = (float)(int16_t)duty *
                                      (float)(int16_t)k_motors[i].direction_sign;
            (void)rx_motor_set_duty(&s_motors[i], signed_duty);
        }

        step = (uint8_t)((step + 1U) % k_sweep_step_count);

        (void)tx_thread_sleep((ULONG)k_motor_sleep_ticks);
    }
}

/* ==========================================================================
 * encoder_task -- 10 Hz CSV dump + heartbeat LED.
 *
 * CSV header (printed once at task start): t_ms,duty,m0,m1,m2,m3
 * Each subsequent line: uptime,duty,cnt0,cnt1,cnt2,cnt3
 * ========================================================================== */
static void encoder_task_entry(ULONG arg)
{
    (void)arg;

    sci9_debug_puts("t_ms,cmt0_cnt,mot_state,m_ent,duty,m0,m1,m2,m3\n");

    uint32_t t_ms = 0U;

    for (;;) {
        gpio_toggle(k_port_a, k_bit_led_heartbeat);
        gpio_toggle(k_port_7, k_bit_led_enc_tick);

        const uint16_t c0 = encoder_read_raw(0U);
        const uint16_t c1 = encoder_read_raw(1U);
        const uint16_t c2 = encoder_read_raw(2U);
        const uint16_t c3 = encoder_read_raw(3U);


        print_s16_dec((int16_t)(t_ms & 0x7FFFU));
        sci9_debug_putc(',');
        print_s16_dec((int16_t)(g_cmt0_isr_count & 0x7FFFU));
        sci9_debug_putc(',');
        print_s16_dec((int16_t)s_motor_tcb.tx_thread_state);
        sci9_debug_putc(',');
        print_s16_dec((int16_t)g_motor_task_entries);
        sci9_debug_putc(',');
        print_s16_dec((int16_t)s_current_duty);
        sci9_debug_putc(',');
        print_s16_dec((int16_t)c0);
        sci9_debug_putc(',');
        print_s16_dec((int16_t)c1);
        sci9_debug_putc(',');
        print_s16_dec((int16_t)c2);
        sci9_debug_putc(',');
        print_s16_dec((int16_t)c3);
        sci9_debug_putc('\n');

        /* Option 1b diagnostic: capture all TX_CALLER_ERROR discriminators
         * BEFORE sleep plus its return value AFTER.  ss and cur distinguish
         * the three remaining TX_CALLER_ERROR paths in tx_thread_sleep.c. */
        const UINT        pd  = _tx_thread_preempt_disable;
        const ULONG       ss  = _tx_thread_system_state;
        const TX_THREAD  *cur = _tx_thread_current_ptr;
        const UINT        ret = tx_thread_sleep((ULONG)k_encoder_sleep_ticks);
        sci9_debug_puts("pd=");
        print_s16_dec((int16_t)pd);
        sci9_debug_puts(" ss=");
        sci9_debug_puthex32((uint32_t)ss);
        sci9_debug_puts(" cur=");
        sci9_debug_puthex32((uint32_t)(uintptr_t)cur);
        sci9_debug_puts(" ret=");
        sci9_debug_puthex16((uint16_t)ret);
        sci9_debug_putc('\n');
        t_ms += 100U;
    }
}

/* ==========================================================================
 * ThreadX application-define -- create both tasks (hardware already init'd).
 * ========================================================================== */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;

    g_encoder_create_status = (uint32_t)tx_thread_create(
        &s_encoder_tcb, "encoder", encoder_task_entry, 0U,
        s_encoder_stack, (ULONG)k_encoder_task_stack_sz,
        (UINT)k_encoder_task_priority,
        (UINT)k_encoder_task_priority,
        TX_NO_TIME_SLICE, TX_AUTO_START);

    g_motor_create_status = (uint32_t)tx_thread_create(
        &s_motor_tcb, "motor", motor_task_entry, 0U,
        s_motor_stack, (ULONG)k_motor_task_stack_sz,
        (UINT)k_motor_task_priority,
        (UINT)k_motor_task_priority,
        TX_NO_TIME_SLICE, TX_AUTO_START);
}

/* ==========================================================================
 * Fatal error trap -- park all four motors and halt with the FAIL LED lit.
 * ========================================================================== */
static void error_park(const char *msg)
{
    sci9_debug_puts("FATAL ");
    sci9_debug_puts(msg);
    sci9_debug_puts("\n");

    /* Best-effort motor safe: attempt a 0-duty write (may fail if rx_motor
     * isn't initialised, but that's OK since GPTW would be idle in that case). */
    for (uint8_t i = 0; i < k_motor_count; i++) {
        (void)rx_motor_set_duty(&s_motors[i], 0.0F);
    }

    for (;;) {
        __asm__ volatile("nop");
    }
}

/* ==========================================================================
 * main() -- runs ONCE on reset, init everything, hand off to ThreadX forever.
 * ========================================================================== */
int main(void)
{
    clock_init();
    leds_init();
    sci9_debug_init();

    sci9_debug_puts("\n\n");
    sci9_debug_puts("motors_rtos: ThreadX + rx_motor + rx_encoder bring-up\n");
    sci9_debug_puts("clock=240MHz pclkb=60MHz tick=100Hz\n");

    motor_drv_gpio_init();
    sci9_debug_puts("drv8263 armed\n");

    if (!motor_pwm_init(s_motors)) {
        error_park("motor_pwm_init");
    }
    sci9_debug_puts("motors 0..3 init ok\n");

    if (!encoders_init()) {
        error_park("encoders_init");
    }
    sci9_debug_puts("encoders 0..3 init ok\n");

    gpio_write(k_port_b, k_bit_led_init_done, true);
    sci9_debug_puts("entering scheduler\n");

    cmt0_init();
    tx_kernel_enter();

    /* tx_kernel_enter() never returns; fall-through trap if it somehow does. */
    for (;;) {
        __asm__ volatile("nop");
    }
    return 0;
}
