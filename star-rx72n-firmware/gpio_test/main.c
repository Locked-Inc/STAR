/**
 * @file main.c
 * @brief GPIO breakout verification firmware for RX72N, ThreadX edition.
 *
 * @details
 * Mirrors the structure that works in blinky_rtos: one ThreadX thread that
 * drives the pins with a busy-wait delay (NOT tx_thread_sleep, which stalls
 * when the scheduler/tick isn't ticking for any reason). A single 100 Hz
 * CMT0 tick keeps the kernel happy even though we do not use it for
 * sleep-based timing.
 *
 * Test pattern per pin:
 *   1. Drive HIGH for ~5 ms (busy-wait loop)
 *   2. Drive LOW  for ~5 ms
 *   3. Advance to next pin
 *
 * After all pins are tested a long quiet gap (~2.3 s) separates cycles so
 * the host script can anchor capture timing to the gap.
 *
 * Clock path:
 *   clock_init() brings us from HOCO 16 MHz -> PLL 192 MHz -> ICLK 96 MHz
 *   / PCKB 48 MHz. CMT0 is then programmed at PCKB/32 for 100 Hz tick.
 *
 * PJ3 and PJ5 are excluded: on RX72N they double as JTAG TMS/TDO and driving
 * them while the E2 Lite has the debug interface latched hangs the MCU.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include "tx_api.h"

/* ==========================================================================
 * GPIO register base addresses (RX72N Hardware Manual Ch 22)
 * ========================================================================== */
typedef enum : uintptr_t {
    k_pdr_base  = 0x0008C000U,
    k_podr_base = 0x0008C020U,
    k_pmr_base  = 0x0008C060U,
} port_reg_base_t;

/* ==========================================================================
 * Port offsets (PDR[k_port_off_x] = PDR_base + offset)
 * ========================================================================== */
typedef enum : uint8_t {
    k_port_off_0 = 0x00U, k_port_off_1 = 0x01U, k_port_off_2 = 0x02U,
    k_port_off_3 = 0x03U, k_port_off_4 = 0x04U, k_port_off_5 = 0x05U,
    k_port_off_6 = 0x06U, k_port_off_7 = 0x07U, k_port_off_8 = 0x08U,
    k_port_off_9 = 0x09U, k_port_off_a = 0x0AU, k_port_off_b = 0x0BU,
    k_port_off_c = 0x0CU, k_port_off_d = 0x0DU, k_port_off_e = 0x0EU,
    k_port_off_f = 0x0FU,
} port_offset_t;

typedef struct {
    uint8_t port_off;
    uint8_t bit;
} pin_desc_t;

/* ==========================================================================
 * Pin table. Array index = firmware sweep index.
 * ========================================================================== */
static const pin_desc_t s_pins[] = {
    { k_port_off_0, 0 }, { k_port_off_0, 1 }, { k_port_off_0, 2 },
    { k_port_off_0, 3 }, { k_port_off_0, 5 }, { k_port_off_0, 7 },
    { k_port_off_1, 2 }, { k_port_off_1, 3 }, { k_port_off_1, 4 },
    { k_port_off_1, 5 }, { k_port_off_1, 7 },
    { k_port_off_2, 0 }, { k_port_off_2, 1 }, { k_port_off_2, 2 },
    { k_port_off_2, 3 }, { k_port_off_2, 4 }, { k_port_off_2, 5 },
    { k_port_off_3, 2 }, { k_port_off_3, 3 },
    { k_port_off_4, 0 }, { k_port_off_4, 1 }, { k_port_off_4, 2 },
    { k_port_off_4, 3 }, { k_port_off_4, 4 }, { k_port_off_4, 5 },
    { k_port_off_4, 6 }, { k_port_off_4, 7 },
    { k_port_off_5, 0 }, { k_port_off_5, 1 }, { k_port_off_5, 2 },
    { k_port_off_5, 3 }, { k_port_off_5, 4 }, { k_port_off_5, 5 },
    { k_port_off_5, 6 },
    { k_port_off_6, 0 }, { k_port_off_6, 1 }, { k_port_off_6, 2 },
    { k_port_off_6, 3 }, { k_port_off_6, 4 }, { k_port_off_6, 5 },
    { k_port_off_6, 6 }, { k_port_off_6, 7 },
    { k_port_off_7, 0 }, { k_port_off_7, 1 }, { k_port_off_7, 2 },
    { k_port_off_7, 3 }, { k_port_off_7, 4 }, { k_port_off_7, 5 },
    { k_port_off_7, 6 }, { k_port_off_7, 7 },
    { k_port_off_8, 0 }, { k_port_off_8, 1 }, { k_port_off_8, 2 },
    { k_port_off_8, 3 }, { k_port_off_8, 6 }, { k_port_off_8, 7 },
    { k_port_off_9, 0 }, { k_port_off_9, 1 }, { k_port_off_9, 2 },
    { k_port_off_9, 3 },
    { k_port_off_a, 0 }, { k_port_off_a, 1 }, { k_port_off_a, 2 },
    { k_port_off_a, 3 }, { k_port_off_a, 4 }, { k_port_off_a, 5 },
    { k_port_off_a, 6 }, { k_port_off_a, 7 },
    { k_port_off_b, 0 }, { k_port_off_b, 1 }, { k_port_off_b, 2 },
    { k_port_off_b, 3 }, { k_port_off_b, 4 }, { k_port_off_b, 5 },
    { k_port_off_c, 0 }, { k_port_off_c, 1 }, { k_port_off_c, 2 },
    { k_port_off_c, 3 }, { k_port_off_c, 4 }, { k_port_off_c, 5 },
    { k_port_off_c, 6 },
    { k_port_off_d, 0 }, { k_port_off_d, 1 }, { k_port_off_d, 2 },
    { k_port_off_d, 3 }, { k_port_off_d, 4 }, { k_port_off_d, 5 },
    { k_port_off_d, 6 }, { k_port_off_d, 7 },
    { k_port_off_e, 0 }, { k_port_off_e, 1 }, { k_port_off_e, 2 },
    { k_port_off_e, 3 }, { k_port_off_e, 4 }, { k_port_off_e, 5 },
    { k_port_off_e, 6 }, { k_port_off_e, 7 },
    { k_port_off_f, 5 },
};

/* ==========================================================================
 * Timing constants. Busy-wait delay at ICLK = 96 MHz. The actual pin
 * period drifts a little with optimisation level; gpio_verify.py measures
 * it empirically from the capture.
 * ========================================================================== */
typedef enum : uint32_t {
    k_num_pins   = (uint32_t)(sizeof(s_pins) / sizeof(s_pins[0])),
    k_delay_iter = 50000U,
    k_gap_iters  = 500U,
} timing_t;

/* ==========================================================================
 * ThreadX objects
 * ========================================================================== */
typedef enum : uint8_t {
    k_sweep_priority = 10U,
} rtos_prio_t;

typedef enum : uint16_t {
    k_sweep_stack_sz = 512U,
} rtos_stack_t;

static TX_THREAD s_sweep_tcb;
static uint8_t   s_sweep_stack[k_sweep_stack_sz];

/* ==========================================================================
 * Inline accessors
 * ========================================================================== */
static inline volatile uint8_t *pdr(uint8_t port_off)
{
    return (volatile uint8_t *)(k_pdr_base + port_off);
}
static inline volatile uint8_t *podr(uint8_t port_off)
{
    return (volatile uint8_t *)(k_podr_base + port_off);
}
static inline volatile uint8_t *pmr(uint8_t port_off)
{
    return (volatile uint8_t *)(k_pmr_base + port_off);
}

/* ==========================================================================
 * Busy-wait delay. Used in place of tx_thread_sleep because the 100 Hz
 * ThreadX tick is too coarse for ~5 ms pin pulses, and because blinky_rtos
 * has proven this pattern stable.
 * ========================================================================== */
static void delay(void)
{
    for (volatile uint32_t i = 0U; i < k_delay_iter; i++) {
        __asm__ volatile("nop");
    }
}

/* ==========================================================================
 * gpio_init_all -- configure every pin in s_pins as GPIO output, driven LOW.
 * ========================================================================== */
static void gpio_init_all(void)
{
    for (uint32_t i = 0U; i < k_num_pins; i++) {
        uint8_t off  = s_pins[i].port_off;
        uint8_t mask = (uint8_t)(1U << s_pins[i].bit);
        *pmr(off)  &= (uint8_t)~mask;
        *pdr(off)  |= mask;
        *podr(off) &= (uint8_t)~mask;
    }
}

/* ==========================================================================
 * sweep_task -- forever loop that pulses each pin in s_pins order.
 * ========================================================================== */
static void sweep_task_entry(ULONG arg)
{
    (void)arg;

    for (;;) {
        for (uint32_t i = 0U; i < k_num_pins; i++) {
            uint8_t off  = s_pins[i].port_off;
            uint8_t mask = (uint8_t)(1U << s_pins[i].bit);
            *podr(off) |= mask;
            delay();
            *podr(off) &= (uint8_t)~mask;
            delay();
        }
        for (uint32_t g = 0U; g < k_gap_iters; g++) {
            delay();
        }
    }
}

/* ==========================================================================
 * tx_application_define -- called by ThreadX from tx_kernel_enter().
 * Configures pins and starts the sweep task.
 * ========================================================================== */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;

    gpio_init_all();

    (void)tx_thread_create(&s_sweep_tcb, "gpio_sweep",
                           sweep_task_entry, 0U,
                           s_sweep_stack, k_sweep_stack_sz,
                           k_sweep_priority, k_sweep_priority,
                           TX_NO_TIME_SLICE, TX_AUTO_START);
}

/* ==========================================================================
 * External init hooks
 * ========================================================================== */
extern void clock_init(void);
extern void cmt0_init(void);

/* ==========================================================================
 * main -- bring up the PLL, arm the CMT0 tick, then hand off to ThreadX.
 * Order matters: clock_init FIRST so PCKB = 48 MHz before cmt0_init reads
 * from CMT registers; cmt0_init BEFORE tx_kernel_enter as in blinky_rtos.
 * ========================================================================== */
int main(void)
{
    clock_init();
    cmt0_init();
    tx_kernel_enter();
    return 0;
}
