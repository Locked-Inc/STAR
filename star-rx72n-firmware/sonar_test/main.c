/**
 * @file main.c
 * @brief HC-SR04 bring-up test on the production STAR PCB.
 *
 * @details
 * Bare-metal, polled-echo smoke test for all four HC-SR04 sonars. Mirrors
 * imu_test/ in structure: uses the rx_hal struct accessors (cmt1(), port0(),
 * portf(), portj(), port3(), system_regs(), prcr_reg()) so every register
 * address is compiler-verified via static_assert in the header, with NO
 * hand-rolled addresses in this file. No ThreadX, no rx_hcsr04 lib
 * dependency.
 *
 * Sequence per sonar (standard HC-SR04):
 *   1. Drive TRIG low for 5 us (settle).
 *   2. Drive TRIG high for 12 us.
 *   3. Drive TRIG low, start the echo timer.
 *   4. Poll ECHO until it goes HIGH (or ~1 ms timeout -> NO_RESPONSE).
 *   5. Poll ECHO until it goes LOW (or ~40 ms timeout -> OUT_OF_RANGE).
 *   6. distance_cm = echo_us / 58.
 *
 * Pin map (from src/inc/hardware_config.h):
 *   S0: TRIG=PF5  ECHO=P03
 *   S1: TRIG=PJ5  ECHO=P02
 *   S2: TRIG=PJ3  ECHO=P01
 *   S3: TRIG=P33  ECHO=P00
 *
 * Microsecond timing is derived from CMT1 running at PCLKB/128. With
 * PCLKB=60 MHz after clock_init(), the CMT1 tick rate is 468.75 kHz, so
 * one tick = 2.133 us. 16-bit CMCNT rolls over at ~139 ms -- longer than
 * the 40 ms HC-SR04 max, so a single measurement cannot wrap.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "rx72n_system_regs.h"      /* system_regs(), prcr_reg() */
#include "rx72n_cmt_regs.h"         /* cmt1(), cmt_ctrl(), k_rx72n_cmstr0_cmt1_enable */
#include "rx72n_port_regs.h"        /* port0(), port3(), portf(), portj() */
#include "rx_register_protection.h" /* k_rx_prcr_unlock_prc1, k_rx_prcr_lock */
#include "hardware_config.h"        /* k_sonar_{0..3}_{trig,echo}_{pin,port} */

extern void clock_init(void);
extern void sci9_debug_init(void);
extern void sci9_debug_puts(const char *s);
extern void sci9_debug_puthex16(uint16_t v);

/* ========================================================================= */
/* Timing constants                                                          */
/* ========================================================================= */
typedef enum : uint16_t {
    /* CMCR: CKS=10 -> PCLKB/128 -> 468.75 kHz (2.133 us/tick) at PCLKB=60 MHz.
     * CMIE=0 -- we poll CMCNT, no ISR. bit 7 reserved (read as 1). */
    k_cmt1_cmcr_val  = 0x0082U,
    k_cmt1_cmcor_val = 0xFFFFU, /**< Free-running to full 16-bit range */
} cmt1_cfg_t;

/* CMT unit 0 (CMT0+CMT1) module-stop bit. The rx_hal header names the CMT2/3
 * bit (k_mstpa_cmt23 = 14) but not the CMT0/1 one, so we name it locally. */
typedef enum : uint8_t {
    k_mstpa_cmt01 = 15U,
} cmt_mstp_t;

/* 2.133 us/tick expressed as a fixed-point (x100) to avoid float math.
 *   us = (ticks * 213) / 100
 * Max ticks in 40 ms window = 40000 us / 2.133 = 18750 -> 18750*213 = 3.99M,
 * fits comfortably in uint32_t. */
typedef enum : uint32_t {
    k_ticks_to_us_num = 213U,
    k_ticks_to_us_den = 100U,
    /* HC-SR04: echo pulse in us = distance_cm * 58. Invert for the read:
     *   distance_cm = echo_us / 58. */
    k_us_per_cm = 58U,
    /* ~1 ms (500 ticks) to wait for ECHO to rise after TRIG. Datasheet
     * quotes 450 us internal burst + propagation delay. */
    k_echo_rise_timeout_ticks = 500U,
    /* ~40 ms (18750 ticks) echo high-width cap -> "out of range". */
    k_echo_high_timeout_ticks = 18750U,
    /* 60 ms quiet time between sonars so reflected bursts don't cross-trigger. */
    k_settle_delay_ms = 60U,
    /* HC-SR04 trigger pulse shape (datasheet: >=10 us high). */
    k_trig_settle_us   = 5U,
    k_trig_pulse_us    = 12U,
    /* 200 ms pause at end of every 4-sonar sweep -> ~5 Hz update. */
    k_sweep_gap_ms     = 200U,
    /* 5 s window so the operator can attach cat /dev/ttyACM0 after flash. */
    k_startup_delay_ms = 5000U,
    /* Microseconds per millisecond -- keeps busy_wait_ms out of magic-number lint. */
    k_us_per_ms        = 1000U,
} sonar_timing_t;

/* sci9_debug_putu32_col scratch buffer sizes. 11 digits covers UINT32_MAX,
 * +1 for NUL. Padded output tolerates up to width=10 + 11 digits + NUL. */
typedef enum : uint8_t {
    k_dec_digits_max = 11U, /**< ceil(log10(UINT32_MAX)) = 10, +1 safety */
    k_dec_buf_size   = 12U, /**< k_dec_digits_max + NUL */
    k_dec_out_size   = 13U, /**< padded output buffer (width <= 10 + digits + NUL) */
    k_dec_base       = 10U, /**< decimal base for digit extraction */
} dec_printer_sizes_t;

/* ========================================================================= */
/* Sonar pin table                                                           */
/* ========================================================================= */
typedef struct {
    volatile rx_port_regs_t *trig_port;
    uint8_t                  trig_bit;
    volatile rx_port_regs_t *echo_port;
    uint8_t                  echo_bit;
    const char              *label;
} sonar_pins_t;

typedef enum : uint8_t {
    k_sonar_id_0 = 0U, /**< Front-left  (PF5/P03) */
    k_sonar_id_1 = 1U, /**< Front-right (PJ5/P02) */
    k_sonar_id_2 = 2U, /**< Back-left   (PJ3/P01) */
    k_sonar_id_3 = 3U, /**< Back-right  (P33/P00) */
} sonar_id_t;

/* Runtime-initialized -- port*() accessors aren't constant expressions. */
static sonar_pins_t s_sonars[k_sonar_count];

static void sonar_table_init(void)
{
    s_sonars[k_sonar_id_0] = (sonar_pins_t){ portf(), (uint8_t)k_sonar_0_trig_pin,
                                             port0(), (uint8_t)k_sonar_0_echo_pin,
                                             "S0 FL (PF5/P03)" };
    s_sonars[k_sonar_id_1] = (sonar_pins_t){ portj(), (uint8_t)k_sonar_1_trig_pin,
                                             port0(), (uint8_t)k_sonar_1_echo_pin,
                                             "S1 FR (PJ5/P02)" };
    s_sonars[k_sonar_id_2] = (sonar_pins_t){ portj(), (uint8_t)k_sonar_2_trig_pin,
                                             port0(), (uint8_t)k_sonar_2_echo_pin,
                                             "S2 BL (PJ3/P01)" };
    s_sonars[k_sonar_id_3] = (sonar_pins_t){ port3(), (uint8_t)k_sonar_3_trig_pin,
                                             port0(), (uint8_t)k_sonar_3_echo_pin,
                                             "S3 BR (P33/P00)" };
}

/* ========================================================================= */
/* GPIO helpers                                                              */
/* ========================================================================= */
static inline void gpio_configure_output(volatile rx_port_regs_t *p, uint8_t bit)
{
    uint8_t mask = (uint8_t)(1U << bit);
    p->pmr  &= (uint8_t)~mask; /* GPIO mode */
    p->podr &= (uint8_t)~mask; /* Drive low initially */
    p->pdr  |= mask;           /* Output */
}

static inline void gpio_configure_input(volatile rx_port_regs_t *p, uint8_t bit)
{
    uint8_t mask = (uint8_t)(1U << bit);
    p->pmr &= (uint8_t)~mask;  /* GPIO mode */
    p->pdr &= (uint8_t)~mask;  /* Input */
}

static inline void gpio_write(volatile rx_port_regs_t *p, uint8_t bit, bool high)
{
    uint8_t mask = (uint8_t)(1U << bit);
    if (high) {
        p->podr |= mask;
    } else {
        p->podr &= (uint8_t)~mask;
    }
}

static inline bool gpio_read(volatile rx_port_regs_t *p, uint8_t bit)
{
    uint8_t mask  = (uint8_t)(1U << bit);
    uint8_t level = (uint8_t)(p->pidr & mask);
    bool    high  = (bool)(level != 0U);
    return high;
}

/* ========================================================================= */
/* Microsecond timer via CMT1                                                */
/* ========================================================================= */
static void cmt1_us_timer_init(void)
{
    /* Release CMT unit 0 (CMT0+CMT1 share MSTPA15) from module stop.
     * PRCR.PRC1 must be 1 to write MSTPCRA. */
    *prcr_reg() = k_rx_prcr_unlock_prc1;
    system_regs()->mstpcra &= ~(uint32_t)(1UL << (uint32_t)k_mstpa_cmt01);
    *prcr_reg() = k_rx_prcr_lock;

    /* Stop CMT1 while we configure it, then start. */
    cmt_ctrl()->cmstr0 &= (uint16_t)~k_rx72n_cmstr0_cmt1_enable;
    cmt1()->cmcr  = k_cmt1_cmcr_val;
    cmt1()->cmcor = k_cmt1_cmcor_val;
    cmt1()->cmcnt = 0U;
    cmt_ctrl()->cmstr0 |= k_rx72n_cmstr0_cmt1_enable;
}

static inline uint16_t cmt1_ticks(void)
{
    return cmt1()->cmcnt;
}

/* Handles single 16-bit wraparound -- caller's window must be < 139 ms. */
static inline uint16_t ticks_elapsed(uint16_t start, uint16_t now)
{
    return (uint16_t)(now - start);
}

static inline uint32_t ticks_to_us(uint32_t ticks)
{
    return (ticks * k_ticks_to_us_num) / k_ticks_to_us_den;
}

/* ========================================================================= */
/* Busy-wait (CMT1-accurate)                                                 */
/* ========================================================================= */
static void busy_wait_us(uint32_t us)
{
    /* Convert us -> ticks at 2.133 us/tick:
     *   ticks = us * 100 / 213 (rounded down). */
    uint32_t ticks = (us * k_ticks_to_us_den) / k_ticks_to_us_num;
    if (ticks == 0U) {
        ticks = 1U;
    }
    uint16_t start = cmt1_ticks();
    while ((uint32_t)ticks_elapsed(start, cmt1_ticks()) < ticks) {
        /* spin */
    }
}

static void busy_wait_ms(uint32_t ms)
{
    for (uint32_t m = 0U; m < ms; m++) {
        busy_wait_us(k_us_per_ms);
    }
}

/* ========================================================================= */
/* Unsigned-decimal printer (right-aligned in width-char column)             */
/* ========================================================================= */
static void sci9_debug_putu32_col(uint32_t v, uint8_t width)
{
    char    buf[k_dec_buf_size] = {0};
    uint8_t n                   = 0U;
    if (v == 0U) {
        buf[n++] = '0';
    } else {
        while (v > 0U && n < (uint8_t)(sizeof(buf) - 1U)) {
            buf[n++] = (char)('0' + (v % (uint32_t)k_dec_base));
            v /= (uint32_t)k_dec_base;
        }
    }
    char    out[k_dec_out_size] = {0};
    uint8_t idx                 = 0U;
    while (idx + n < width && idx < (uint8_t)(sizeof(out) - 1U)) {
        out[idx++] = ' ';
    }
    while (n > 0U && idx < (uint8_t)(sizeof(out) - 1U)) {
        out[idx++] = buf[--n];
    }
    out[idx] = '\0';
    sci9_debug_puts(out);
}

/* ========================================================================= */
/* HC-SR04 measurement                                                       */
/* ========================================================================= */
typedef enum : uint8_t {
    k_sonar_ok           = 0U,
    k_sonar_no_response  = 1U,
    k_sonar_out_of_range = 2U,
} sonar_status_t;

typedef struct {
    sonar_status_t status;
    uint32_t       pulse_us;
    uint32_t       dist_cm;
} sonar_result_t;

static sonar_result_t sonar_measure(const sonar_pins_t *s)
{
    sonar_result_t r = { k_sonar_no_response, 0U, 0U };

    /* Step 1: ensure TRIG starts low. */
    gpio_write(s->trig_port, s->trig_bit, false);
    busy_wait_us(k_trig_settle_us);

    /* Step 2: HIGH pulse (datasheet minimum 10 us, we use 12 us with margin). */
    gpio_write(s->trig_port, s->trig_bit, true);
    busy_wait_us(k_trig_pulse_us);
    gpio_write(s->trig_port, s->trig_bit, false);

    /* Step 3: wait for ECHO rising edge (with ~1 ms bound). */
    uint16_t t0   = cmt1_ticks();
    uint16_t tnow = t0;
    bool     rose = false;
    while ((uint32_t)ticks_elapsed(t0, tnow) < k_echo_rise_timeout_ticks) {
        if (gpio_read(s->echo_port, s->echo_bit)) {
            rose = true;
            break;
        }
        tnow = cmt1_ticks();
    }
    if (!rose) {
        r.status = k_sonar_no_response;
        return r;
    }

    /* Step 4: measure ECHO high width (with ~40 ms bound). */
    uint16_t t_rise = cmt1_ticks();
    tnow = t_rise;
    bool fell = false;
    while ((uint32_t)ticks_elapsed(t_rise, tnow) < k_echo_high_timeout_ticks) {
        if (!gpio_read(s->echo_port, s->echo_bit)) {
            fell = true;
            break;
        }
        tnow = cmt1_ticks();
    }
    if (!fell) {
        r.status = k_sonar_out_of_range;
        return r;
    }

    uint32_t ticks = (uint32_t)ticks_elapsed(t_rise, tnow);
    r.pulse_us     = ticks_to_us(ticks);
    r.dist_cm      = r.pulse_us / k_us_per_cm;
    r.status       = k_sonar_ok;
    return r;
}

/* ========================================================================= */
/* Output formatting                                                         */
/* ========================================================================= */
typedef enum : uint8_t {
    k_col_width_dist_cm  = 4U,
    k_col_width_pulse_us = 6U,
} output_cols_t;

static void print_result(const sonar_pins_t *s, const sonar_result_t *r)
{
    sci9_debug_puts("  ");
    sci9_debug_puts(s->label);
    sci9_debug_puts("  ");

    switch (r->status) {
        case k_sonar_ok:
            sci9_debug_puts("dist=");
            sci9_debug_putu32_col(r->dist_cm, k_col_width_dist_cm);
            sci9_debug_puts(" cm   (pulse=");
            sci9_debug_putu32_col(r->pulse_us, k_col_width_pulse_us);
            sci9_debug_puts(" us)\n");
            break;
        case k_sonar_no_response:
            sci9_debug_puts("NO_RESPONSE (echo never rose within 1 ms)\n");
            break;
        case k_sonar_out_of_range:
            sci9_debug_puts("OUT_OF_RANGE (echo stuck high past 40 ms)\n");
            break;
        default:
            sci9_debug_puts("BAD_STATUS\n");
            break;
    }
}

/* ========================================================================= */
/* Setup / banner                                                            */
/* ========================================================================= */
static void configure_all_pins(void)
{
    sci9_debug_puts("Configuring sonar pins:\n");
    for (uint8_t i = 0U; i < k_sonar_count; i++) {
        const sonar_pins_t *s = &s_sonars[i];
        gpio_configure_output(s->trig_port, s->trig_bit);
        gpio_configure_input(s->echo_port, s->echo_bit);
        sci9_debug_puts("  ");
        sci9_debug_puts(s->label);
        sci9_debug_puts("\n");
    }
}

static void print_banner(void)
{
    sci9_debug_puts("\n=== SONAR_TEST start ===\n");
    sci9_debug_puts("CMT1 us-timer: PCLKB/128 = 468.75 kHz (2.133 us/tick)\n");
}

static void print_legend(void)
{
    sci9_debug_puts("\nExpected: ~2 cm (min) .. ~400 cm (max), 0 = no echo.\n");
    sci9_debug_puts("If a sonar prints NO_RESPONSE every iter -> TRIG or ECHO wiring bad.\n");
    sci9_debug_puts("If it prints OUT_OF_RANGE every iter    -> nothing in front / stuck echo.\n");
    sci9_debug_puts("----------------------------------------------------------------\n\n");
}

static void run_one_sweep(uint16_t iter)
{
    sci9_debug_puts("iter=");
    sci9_debug_puthex16(iter);
    sci9_debug_puts("\n");

    for (uint8_t i = 0U; i < k_sonar_count; i++) {
        const sonar_pins_t *s = &s_sonars[i];
        sonar_result_t      r = sonar_measure(s);
        print_result(s, &r);
        busy_wait_ms(k_settle_delay_ms);
    }

    sci9_debug_puts("\n");
}

/* ========================================================================= */
/* main                                                                      */
/* ========================================================================= */
int main(void)
{
    clock_init();
    sci9_debug_init();
    sci9_debug_puts("\n[sonar_test] clock+uart up\n");

    sonar_table_init();
    cmt1_us_timer_init();
    sci9_debug_puts("[sonar_test] CMT1 us-timer up, cmcnt=0x");
    sci9_debug_puthex16(cmt1()->cmcnt);
    sci9_debug_puts("\n");

    busy_wait_ms(k_startup_delay_ms);
    sci9_debug_puts("[sonar_test] 5s startup delay done\n");

    print_banner();
    configure_all_pins();
    print_legend();

    uint16_t iter = 0U;
    for (;;) {
        run_one_sweep(iter);
        busy_wait_ms(k_sweep_gap_ms);
        iter++;
    }
}
