/**
 * @file main.c
 * @brief USB CDC bring-up test for RX72N.
 *
 * @details
 * Boots RX72N from LOCO 240 kHz to PLL 240 MHz (ICLK 240, PCKB 60, UCK 48),
 * starts the ThreadX 100 Hz tick on CMT0, then enters ThreadX.  Inside
 * tx_application_define() we create one task that:
 *
 *   1. Calls rx_usb_init() to bring up the existing 3-port CDC-ACM
 *      composite stack from libs/rx_usb.
 *   2. Waits for the host to enumerate and configure the protocol port.
 *   3. Writes a counter line every second on /dev/ttyACM0.
 *
 * If everything works the host sees three new /dev/ttyACM* devices appear
 * with VID/PID 045B:0235 (Renesas test allocation, set in rx_usb_cdc.c).
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include <string.h>

#include "rx_err.h"
#include "rx_usb.h"
#include "tx_api.h"

extern void clock_init(void);
extern void cmt0_init(void);

/* PORTA accessor for visual heartbeat LED on PA7 (same LED used by blinky). */
typedef enum : uintptr_t {
    k_porta_pdr_addr  = 0x0008C00AU,
    k_porta_podr_addr = 0x0008C02AU,
} gpio_addrs_t;

typedef enum : uint8_t {
    k_pa7_mask = (uint8_t)(1U << 7U),
} gpio_bits_t;

static inline volatile uint8_t *porta_pdr(void)
{
    return (volatile uint8_t *)k_porta_pdr_addr;
}
static inline volatile uint8_t *porta_podr(void)
{
    return (volatile uint8_t *)k_porta_podr_addr;
}

/* ==========================================================================
 * ThreadX task
 * ========================================================================== */
typedef enum : uint16_t {
    k_usb_task_stack_sz = 2048U, /**< Bytes of stack for the USB heartbeat task */
} usb_task_stack_t;

typedef enum : uint8_t {
    k_usb_task_priority = 10U,
    k_heartbeat_period_ticks = 100U, /**< 100 ticks * 10 ms = 1 s */
} usb_task_cfg_t;

static TX_THREAD s_usb_tcb;
static uint8_t   s_usb_stack[k_usb_task_stack_sz];

/* ==========================================================================
 * Heartbeat task: drives /dev/ttyACM0 once enumerated.
 *
 * Toggles the PA7 LED every iteration so we have a visual confirmation that
 * the firmware is running even when USB enumeration fails.  If the LED is
 * blinking but no /dev/ttyACMx appears, the bug is in USB.  If the LED is
 * dark or stuck, the bug is in clock/CMT/ThreadX/init.
 * ========================================================================== */
static void usb_heartbeat_task(ULONG arg)
{
    (void)arg;

    /* PA7 as output -- the GPIO bit for the visual heartbeat. */
    *porta_pdr() |= k_pa7_mask;
    *porta_podr() &= (uint8_t)~k_pa7_mask;

    /* Bring up USB hardware (calls tx_thread_sleep internally so this MUST
     * run from a task, not from main() before tx_kernel_enter()). */
    rx_usb_config_t cfg = { .callback = (rx_usb_callback_t)0, .ctx = (void *)0 };
    (void)rx_usb_init(&cfg);

    /* LED on means rx_usb_init returned (clock + USB hw init both finished).
     * If it gets here the firmware ran past clock_init, cmt0_init, ThreadX
     * boot, and the entire rx_usb stack init. */
    *porta_podr() |= k_pa7_mask;

    /* Wait for the host to send SET_CONFIGURATION on the protocol port.
     * While waiting, blink PA7 at ~5 Hz so we can see we're alive but
     * stuck in enumeration. */
    while (!rx_usb_is_configured(k_usb_port_proto)) {
        *porta_podr() ^= k_pa7_mask;
        tx_thread_sleep(10U);
    }

    /* LED solid on once enumeration completes. */
    *porta_podr() |= k_pa7_mask;

    /* Steady-state: print a counter line every second. */
    static const char banner[] = "STAR USB CDC heartbeat\r\n";
    (void)rx_usb_write(k_usb_port_proto,
                       (const uint8_t *)banner,
                       (uint32_t)(sizeof banner - 1U));

    uint32_t tick = 0U;
    char     line[32];
    for (;;) {
        /* Tiny formatter: "tick=NNNNN\r\n" without pulling in printf. */
        line[0]  = 't';
        line[1]  = 'i';
        line[2]  = 'c';
        line[3]  = 'k';
        line[4]  = '=';
        uint32_t value = tick;
        char     digits[10];
        uint8_t  d = 0U;
        if (value == 0U) {
            digits[d++] = '0';
        }
        else {
            while (value != 0U && d < (uint8_t)sizeof digits) {
                digits[d++] = (char)('0' + (value % 10U));
                value /= 10U;
            }
        }
        uint8_t out_len = 5U;
        while (d > 0U) {
            d--;
            line[out_len++] = digits[d];
        }
        line[out_len++] = '\r';
        line[out_len++] = '\n';
        (void)rx_usb_write(k_usb_port_proto, (const uint8_t *)line, (uint32_t)out_len);

        tx_thread_sleep((ULONG)k_heartbeat_period_ticks);
        tick++;
    }
}

/* ==========================================================================
 * tx_application_define -- ThreadX entry point after tx_kernel_enter().
 * ========================================================================== */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;

    (void)tx_thread_create(&s_usb_tcb,
                           "usb_hb",
                           usb_heartbeat_task,
                           0U,
                           s_usb_stack,
                           (ULONG)k_usb_task_stack_sz,
                           k_usb_task_priority,
                           k_usb_task_priority,
                           TX_NO_TIME_SLICE,
                           TX_AUTO_START);
}

/* ==========================================================================
 * main -- clock + tick + ThreadX kernel.
 * ========================================================================== */
int main(void)
{
    clock_init();
    cmt0_init();
    tx_kernel_enter(); /* Never returns */
    return 0;
}
