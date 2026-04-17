/**
 * @file main.c
 * @brief GPIO breakout board verification firmware for RX72N.
 *
 * @details
 * Sequentially toggles every GPIO pin exposed on TOM's breakout PCB so
 * that Analog Discovery 2 instruments can verify each header connection.
 *
 * Test pattern per pin:
 *   1. Drive HIGH for ~5 ms
 *   2. Drive LOW  for ~5 ms
 *   3. Advance to next pin
 *
 * After all pins are tested, a long quiet gap (~40x pin period) separates
 * cycles so the host script can anchor timing to the gap. The host script
 * (host/gpio_verify.py) captures AD2 digital inputs and matches transitions
 * to the known pin order.
 *
 * Implementation: polling-based with a simple nop delay loop, no RTOS.
 * Runs on HOCO 16 MHz -> PLL 192 MHz -> ICLK 96 MHz after clock_init().
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

/* ==========================================================================
 * GPIO register base addresses (RX72N Hardware Manual Ch 22)
 * ========================================================================== */
typedef enum : uintptr_t {
    k_pdr_base  = 0x0008C000U, /**< PDR[port]:  0=input, 1=output */
    k_podr_base = 0x0008C020U, /**< PODR[port]: output data */
    k_pmr_base  = 0x0008C060U, /**< PMR[port]:  0=GPIO, 1=peripheral */
} port_reg_base_t;

/* ==========================================================================
 * Port offsets from each base address.
 * ========================================================================== */
typedef enum : uint8_t {
    k_port_off_0 = 0x00U,
    k_port_off_1 = 0x01U,
    k_port_off_2 = 0x02U,
    k_port_off_3 = 0x03U,
    k_port_off_4 = 0x04U,
    k_port_off_5 = 0x05U,
    k_port_off_6 = 0x06U,
    k_port_off_7 = 0x07U,
    k_port_off_8 = 0x08U,
    k_port_off_9 = 0x09U,
    k_port_off_a = 0x0AU,
    k_port_off_b = 0x0BU,
    k_port_off_c = 0x0CU,
    k_port_off_d = 0x0DU,
    k_port_off_e = 0x0EU,
    k_port_off_f = 0x0FU,
    k_port_off_j = 0x12U,
} port_offset_t;

/* ==========================================================================
 * Pin descriptor -- one entry per testable GPIO pin.
 * ========================================================================== */
typedef struct {
    uint8_t port_off;
    uint8_t bit;
} pin_desc_t;

/* ==========================================================================
 * Complete pin table. The index into this array is the "pin index" that the
 * host script matches transitions against. BGA pin numbers shown in comments
 * correspond to TOM's breakout silkscreen (see pin_map.md).
 *
 * PJ3 and PJ5 are intentionally excluded: on RX72N they double as JTAG TMS/
 * TDO, and driving them while the E2 Lite still owns the debug interface
 * hangs the MCU.
 * ========================================================================== */
static const pin_desc_t s_pins[] = {
    /* Port 0 */
    { k_port_off_0, 0 }, { k_port_off_0, 1 }, { k_port_off_0, 2 },
    { k_port_off_0, 3 }, { k_port_off_0, 5 }, { k_port_off_0, 7 },
    /* Port 1 */
    { k_port_off_1, 2 }, { k_port_off_1, 3 }, { k_port_off_1, 4 },
    { k_port_off_1, 5 }, { k_port_off_1, 7 },
    /* Port 2 */
    { k_port_off_2, 0 }, { k_port_off_2, 1 }, { k_port_off_2, 2 },
    { k_port_off_2, 3 }, { k_port_off_2, 4 }, { k_port_off_2, 5 },
    /* Port 3 */
    { k_port_off_3, 2 }, { k_port_off_3, 3 },
    /* Port 4 */
    { k_port_off_4, 0 }, { k_port_off_4, 1 }, { k_port_off_4, 2 },
    { k_port_off_4, 3 }, { k_port_off_4, 4 }, { k_port_off_4, 5 },
    { k_port_off_4, 6 }, { k_port_off_4, 7 },
    /* Port 5 */
    { k_port_off_5, 0 }, { k_port_off_5, 1 }, { k_port_off_5, 2 },
    { k_port_off_5, 3 }, { k_port_off_5, 4 }, { k_port_off_5, 5 },
    { k_port_off_5, 6 },
    /* Port 6 */
    { k_port_off_6, 0 }, { k_port_off_6, 1 }, { k_port_off_6, 2 },
    { k_port_off_6, 3 }, { k_port_off_6, 4 }, { k_port_off_6, 5 },
    { k_port_off_6, 6 }, { k_port_off_6, 7 },
    /* Port 7 */
    { k_port_off_7, 0 }, { k_port_off_7, 1 }, { k_port_off_7, 2 },
    { k_port_off_7, 3 }, { k_port_off_7, 4 }, { k_port_off_7, 5 },
    { k_port_off_7, 6 }, { k_port_off_7, 7 },
    /* Port 8 */
    { k_port_off_8, 0 }, { k_port_off_8, 1 }, { k_port_off_8, 2 },
    { k_port_off_8, 3 }, { k_port_off_8, 6 }, { k_port_off_8, 7 },
    /* Port 9 */
    { k_port_off_9, 0 }, { k_port_off_9, 1 }, { k_port_off_9, 2 },
    { k_port_off_9, 3 },
    /* Port A */
    { k_port_off_a, 0 }, { k_port_off_a, 1 }, { k_port_off_a, 2 },
    { k_port_off_a, 3 }, { k_port_off_a, 4 }, { k_port_off_a, 5 },
    { k_port_off_a, 6 }, { k_port_off_a, 7 },
    /* Port B */
    { k_port_off_b, 0 }, { k_port_off_b, 1 }, { k_port_off_b, 2 },
    { k_port_off_b, 3 }, { k_port_off_b, 4 }, { k_port_off_b, 5 },
    /* Port C */
    { k_port_off_c, 0 }, { k_port_off_c, 1 }, { k_port_off_c, 2 },
    { k_port_off_c, 3 }, { k_port_off_c, 4 }, { k_port_off_c, 5 },
    { k_port_off_c, 6 },
    /* Port D */
    { k_port_off_d, 0 }, { k_port_off_d, 1 }, { k_port_off_d, 2 },
    { k_port_off_d, 3 }, { k_port_off_d, 4 }, { k_port_off_d, 5 },
    { k_port_off_d, 6 }, { k_port_off_d, 7 },
    /* Port E */
    { k_port_off_e, 0 }, { k_port_off_e, 1 }, { k_port_off_e, 2 },
    { k_port_off_e, 3 }, { k_port_off_e, 4 }, { k_port_off_e, 5 },
    { k_port_off_e, 6 }, { k_port_off_e, 7 },
    /* Port F */
    { k_port_off_f, 5 },
};

/* ==========================================================================
 * Timing constants. At ICLK = 96 MHz each volatile-nop loop iteration is
 * roughly 10 cycles, so 50 000 iterations ~= 5 ms per HIGH/LOW state. A
 * full 98-pin sweep is ~1 s. The gap is ~40x one pin period so the host
 * can reliably anchor time to the quiet window between cycles.
 * ========================================================================== */
typedef enum : uint32_t {
    k_num_pins   = (uint32_t)(sizeof(s_pins) / sizeof(s_pins[0])),
    k_delay_iter = 50000U,
    /* Gap must be longer than the longest intra-cycle interval between
     * two WIRED channels' edges, otherwise gpio_verify.py --auto-map can
     * anchor to the wrong gap. With sparse wiring (up to ~30 consecutive
     * unwired pins between wired ones) intra-cycle gaps can approach
     * 300 ms, so 500 delay iterations gives a ~2.5 s cycle gap that is
     * unambiguous regardless of how the probes are placed. */
    k_gap_iters  = 500U,
} timing_t;

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
 * delay -- simple busy-wait for a single pin state (HIGH or LOW).
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

extern void clock_init(void);

/* ==========================================================================
 * main -- bring up the PLL then sweep all pins forever.
 * ========================================================================== */
int main(void)
{
    clock_init();
    gpio_init_all();

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
