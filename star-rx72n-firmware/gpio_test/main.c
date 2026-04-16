/**
 * @file main.c
 * @brief GPIO breakout board verification firmware for RX72N.
 *
 * @details
 * Sequentially toggles every GPIO pin exposed on TOM's breakout PCB so
 * that Analog Discovery 2 instruments can verify each header connection.
 *
 * Test pattern per pin:
 *   1. Drive HIGH for 50 ms (5 ThreadX ticks at 100 Hz)
 *   2. Drive LOW  for 50 ms
 *   3. Advance to next pin
 *
 * After all pins are tested, a 1-second gap separates cycles.
 * The host script (host/gpio_verify.py) captures AD2 digital inputs
 * and matches transitions to the known pin order.
 *
 * All 100 GPIO pins from the breakout board pin_map.md are included,
 * ordered by port number then bit number for deterministic iteration.
 *
 * Clock: HOCO 16 MHz -> PLL 192 MHz -> ICLK 96 MHz / PCKB 48 MHz.
 * Tick:  CMT0 at 100 Hz (10 ms/tick).
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include "tx_api.h"

/* ==========================================================================
 * GPIO register base addresses (RX72N Hardware Manual Ch 22)
 *
 * Registers are grouped by TYPE, not by port:
 *   PDR[port_offset]  = 0x0008C000 + offset  (direction: 0=in, 1=out)
 *   PODR[port_offset] = 0x0008C020 + offset  (output data)
 *   PIDR[port_offset] = 0x0008C040 + offset  (input data, read-only)
 *   PMR[port_offset]  = 0x0008C060 + offset  (mode: 0=GPIO, 1=peripheral)
 * ========================================================================== */
typedef enum : uintptr_t {
    k_port_pdr_base  = 0x0008C000U,
    k_port_podr_base = 0x0008C020U,
    k_port_pidr_base = 0x0008C040U,
    k_port_pmr_base  = 0x0008C060U,
} port_reg_base_t;

/* ==========================================================================
 * Port offsets from base address.
 * Ports 0-9 map to offsets 0x00-0x09, A-F to 0x0A-0x0F, J to 0x12.
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
 * Inline register accessors -- take a port offset, return register pointer.
 * ========================================================================== */
static inline volatile uint8_t *port_pdr(uint8_t offset)
{
    return (volatile uint8_t *)(k_port_pdr_base + offset);
}

static inline volatile uint8_t *port_podr(uint8_t offset)
{
    return (volatile uint8_t *)(k_port_podr_base + offset);
}

static inline volatile uint8_t *port_pidr(uint8_t offset)
{
    return (volatile uint8_t *)(k_port_pidr_base + offset);
}

static inline volatile uint8_t *port_pmr(uint8_t offset)
{
    return (volatile uint8_t *)(k_port_pmr_base + offset);
}

/* ==========================================================================
 * Pin descriptor -- one entry per testable GPIO pin.
 * ========================================================================== */
typedef struct {
    uint8_t port_offset; /**< Port register offset (k_port_off_*) */
    uint8_t bit;         /**< Bit number 0-7 within the port */
} pin_desc_t;

/* ==========================================================================
 * Complete pin table -- every GPIO on TOM's breakout board.
 *
 * Ordered by port number, then bit number.  The index into this array is
 * the "pin index" that the host script uses to identify each pin.
 *
 * BGA pin numbers from pin_map.md are in the trailing comment.
 * ========================================================================== */
static const pin_desc_t s_pins[] = {
    /* Port 0 */
    { k_port_off_0, 0 },  /*  0: P00  BGA  8 */
    { k_port_off_0, 1 },  /*  1: P01  BGA  7 */
    { k_port_off_0, 2 },  /*  2: P02  BGA  6 */
    { k_port_off_0, 3 },  /*  3: P03  BGA  4 */
    { k_port_off_0, 5 },  /*  4: P05  BGA  2 */
    { k_port_off_0, 7 },  /*  5: P07  BGA144 */

    /* Port 1 */
    { k_port_off_1, 2 },  /*  6: P12  BGA 45 */
    { k_port_off_1, 3 },  /*  7: P13  BGA 44 */
    { k_port_off_1, 4 },  /*  8: P14  BGA 43 */
    { k_port_off_1, 5 },  /*  9: P15  BGA 42 */
    { k_port_off_1, 7 },  /* 10: P17  BGA 38 */

    /* Port 2 */
    { k_port_off_2, 0 },  /* 11: P20  BGA 37 */
    { k_port_off_2, 1 },  /* 12: P21  BGA 36 */
    { k_port_off_2, 2 },  /* 13: P22  BGA 35 */
    { k_port_off_2, 3 },  /* 14: P23  BGA 34 */
    { k_port_off_2, 4 },  /* 15: P24  BGA 33 */
    { k_port_off_2, 5 },  /* 16: P25  BGA 32 */

    /* Port 3 */
    { k_port_off_3, 2 },  /* 17: P32  BGA 27 */
    { k_port_off_3, 3 },  /* 18: P33  BGA 26 */

    /* Port 4 */
    { k_port_off_4, 0 },  /* 19: P40  BGA141 */
    { k_port_off_4, 1 },  /* 20: P41  BGA139 */
    { k_port_off_4, 2 },  /* 21: P42  BGA138 */
    { k_port_off_4, 3 },  /* 22: P43  BGA137 */
    { k_port_off_4, 4 },  /* 23: P44  BGA136 */
    { k_port_off_4, 5 },  /* 24: P45  BGA135 */
    { k_port_off_4, 6 },  /* 25: P46  BGA134 */
    { k_port_off_4, 7 },  /* 26: P47  BGA133 */

    /* Port 5 */
    { k_port_off_5, 0 },  /* 27: P50  BGA 56 */
    { k_port_off_5, 1 },  /* 28: P51  BGA 55 */
    { k_port_off_5, 2 },  /* 29: P52  BGA 54 */
    { k_port_off_5, 3 },  /* 30: P53  BGA 53 */
    { k_port_off_5, 4 },  /* 31: P54  BGA 52 */
    { k_port_off_5, 5 },  /* 32: P55  BGA 51 */
    { k_port_off_5, 6 },  /* 33: P56  BGA 50 */

    /* Port 6 */
    { k_port_off_6, 0 },  /* 34: P60  BGA117 */
    { k_port_off_6, 1 },  /* 35: P61  BGA115 */
    { k_port_off_6, 2 },  /* 36: P62  BGA114 */
    { k_port_off_6, 3 },  /* 37: P63  BGA113 */
    { k_port_off_6, 4 },  /* 38: P64  BGA112 */
    { k_port_off_6, 5 },  /* 39: P65  BGA100 */
    { k_port_off_6, 6 },  /* 40: P66  BGA 99 */
    { k_port_off_6, 7 },  /* 41: P67  BGA 98 */

    /* Port 7 */
    { k_port_off_7, 0 },  /* 42: P70  BGA104 */
    { k_port_off_7, 1 },  /* 43: P71  BGA 86 */
    { k_port_off_7, 2 },  /* 44: P72  BGA 85 */
    { k_port_off_7, 3 },  /* 45: P73  BGA 77 */
    { k_port_off_7, 4 },  /* 46: P74  BGA 72 */
    { k_port_off_7, 5 },  /* 47: P75  BGA 71 */
    { k_port_off_7, 6 },  /* 48: P76  BGA 69 */
    { k_port_off_7, 7 },  /* 49: P77  BGA 68 */

    /* Port 8 */
    { k_port_off_8, 0 },  /* 50: P80  BGA 65 */
    { k_port_off_8, 1 },  /* 51: P81  BGA 64 */
    { k_port_off_8, 2 },  /* 52: P82  BGA 63 */
    { k_port_off_8, 3 },  /* 53: P83  BGA 58 */
    { k_port_off_8, 6 },  /* 54: P86  BGA 41 */
    { k_port_off_8, 7 },  /* 55: P87  BGA 39 */

    /* Port 9 */
    { k_port_off_9, 0 },  /* 56: P90  BGA131 */
    { k_port_off_9, 1 },  /* 57: P91  BGA129 */
    { k_port_off_9, 2 },  /* 58: P92  BGA128 */
    { k_port_off_9, 3 },  /* 59: P93  BGA127 */

    /* Port A */
    { k_port_off_a, 0 },  /* 60: PA0  BGA 97 */
    { k_port_off_a, 1 },  /* 61: PA1  BGA 96 */
    { k_port_off_a, 2 },  /* 62: PA2  BGA 95 */
    { k_port_off_a, 3 },  /* 63: PA3  BGA 94 */
    { k_port_off_a, 4 },  /* 64: PA4  BGA 92 */
    { k_port_off_a, 5 },  /* 65: PA5  BGA 90 */
    { k_port_off_a, 6 },  /* 66: PA6  BGA 89 */
    { k_port_off_a, 7 },  /* 67: PA7  BGA 88 */

    /* Port B */
    { k_port_off_b, 0 },  /* 68: PB0  BGA 87 */
    { k_port_off_b, 1 },  /* 69: PB1  BGA 84 */
    { k_port_off_b, 2 },  /* 70: PB2  BGA 83 */
    { k_port_off_b, 3 },  /* 71: PB3  BGA 82 */
    { k_port_off_b, 4 },  /* 72: PB4  BGA 81 */
    { k_port_off_b, 5 },  /* 73: PB5  BGA 80 */

    /* Port C */
    { k_port_off_c, 0 },  /* 74: PC0  BGA 75 */
    { k_port_off_c, 1 },  /* 75: PC1  BGA 73 */
    { k_port_off_c, 2 },  /* 76: PC2  BGA 70 */
    { k_port_off_c, 3 },  /* 77: PC3  BGA 67 */
    { k_port_off_c, 4 },  /* 78: PC4  BGA 66 */
    { k_port_off_c, 5 },  /* 79: PC5  BGA 62 */
    { k_port_off_c, 6 },  /* 80: PC6  BGA 61 */

    /* Port D */
    { k_port_off_d, 0 },  /* 81: PD0  BGA126 */
    { k_port_off_d, 1 },  /* 82: PD1  BGA125 */
    { k_port_off_d, 2 },  /* 83: PD2  BGA124 */
    { k_port_off_d, 3 },  /* 84: PD3  BGA123 */
    { k_port_off_d, 4 },  /* 85: PD4  BGA122 */
    { k_port_off_d, 5 },  /* 86: PD5  BGA121 */
    { k_port_off_d, 6 },  /* 87: PD6  BGA120 */
    { k_port_off_d, 7 },  /* 88: PD7  BGA119 */

    /* Port E */
    { k_port_off_e, 0 },  /* 89: PE0  BGA111 */
    { k_port_off_e, 1 },  /* 90: PE1  BGA110 */
    { k_port_off_e, 2 },  /* 91: PE2  BGA109 */
    { k_port_off_e, 3 },  /* 92: PE3  BGA108 */
    { k_port_off_e, 4 },  /* 93: PE4  BGA107 */
    { k_port_off_e, 5 },  /* 94: PE5  BGA106 */
    { k_port_off_e, 6 },  /* 95: PE6  BGA102 */
    { k_port_off_e, 7 },  /* 96: PE7  BGA101 */

    /* Port F */
    { k_port_off_f, 5 },  /* 97: PF5  BGA  9 */

    /* Port J */
    { k_port_off_j, 3 },  /* 98: PJ3  BGA 13 */
    { k_port_off_j, 5 },  /* 99: PJ5  BGA 11 */
};

/* ==========================================================================
 * Timing constants (ThreadX ticks at 100 Hz = 10 ms/tick)
 * ========================================================================== */
typedef enum : uint32_t {
    k_num_pins       = (uint32_t)(sizeof(s_pins) / sizeof(s_pins[0])),
    k_high_ticks     = 5U,   /**< 50 ms HIGH per pin */
    k_low_ticks      = 5U,   /**< 50 ms LOW  per pin */
    k_cycle_gap_ticks = 100U, /**< 1 s gap between full cycles */
} timing_t;

/* ==========================================================================
 * ThreadX objects
 * ========================================================================== */
typedef enum : uint8_t {
    k_test_priority = 10U,
} rtos_prio_t;

typedef enum : uint16_t {
    k_test_stack_sz = 512U,
} rtos_stack_t;

static TX_THREAD s_test_tcb;
static uint8_t   s_test_stack[k_test_stack_sz];

/* ==========================================================================
 * gpio_init_all -- configure every pin in s_pins as GPIO output, driven LOW.
 *
 * After reset PMR = 0x00 (GPIO mode) so we only need to set PDR (direction).
 * We batch per-port to minimise register writes.
 * ========================================================================== */
static void gpio_init_all(void)
{
    uint32_t i;
    for (i = 0U; i < k_num_pins; i++) {
        uint8_t off = s_pins[i].port_offset;
        uint8_t mask = (uint8_t)(1U << s_pins[i].bit);

        /* Ensure GPIO mode (clear PMR bit -- should already be 0 after reset) */
        *port_pmr(off) &= (uint8_t)~mask;

        /* Set as output */
        *port_pdr(off) |= mask;

        /* Drive LOW initially */
        *port_podr(off) &= (uint8_t)~mask;
    }
}

/* ==========================================================================
 * gpio_set / gpio_clear -- drive a single pin HIGH or LOW.
 * ========================================================================== */
static inline void gpio_set(const pin_desc_t *pin)
{
    *port_podr(pin->port_offset) |= (uint8_t)(1U << pin->bit);
}

static inline void gpio_clear(const pin_desc_t *pin)
{
    *port_podr(pin->port_offset) &= (uint8_t) ~(uint8_t)(1U << pin->bit);
}

/* ==========================================================================
 * test_task -- sequential GPIO toggle, runs forever.
 *
 * Each pin: HIGH 50 ms -> LOW 50 ms.
 * After all pins: 1 s gap, then repeat.
 * Total cycle time: 100 pins * 100 ms + 1 s gap = 11 s per cycle.
 * ========================================================================== */
static void test_task_entry(ULONG arg)
{
    (void)arg;

    for (;;) {
        uint32_t i;
        for (i = 0U; i < k_num_pins; i++) {
            gpio_set(&s_pins[i]);
            (void)tx_thread_sleep(k_high_ticks);

            gpio_clear(&s_pins[i]);
            (void)tx_thread_sleep(k_low_ticks);
        }

        /* Gap between cycles so the host can detect cycle boundaries */
        (void)tx_thread_sleep(k_cycle_gap_ticks);
    }
}

/* ==========================================================================
 * tx_application_define -- called by ThreadX after tx_kernel_enter().
 * Configures GPIO pins and creates the test thread.
 * ========================================================================== */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;

    gpio_init_all();

    (void)tx_thread_create(&s_test_tcb, "gpio_test",
                           test_task_entry, 0U,
                           s_test_stack, k_test_stack_sz,
                           k_test_priority, k_test_priority,
                           TX_NO_TIME_SLICE, TX_AUTO_START);
}

/* ==========================================================================
 * External init functions
 * ========================================================================== */
extern void clock_init(void);
extern void cmt0_init(void);

/* ==========================================================================
 * main -- initialise clock + tick, then hand off to ThreadX.
 * ========================================================================== */
int main(void)
{
    clock_init();
    cmt0_init();
    tx_kernel_enter();
    return 0;
}
