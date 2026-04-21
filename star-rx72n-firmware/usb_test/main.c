/**
 * @file main.c
 * @brief Minimal USB device bring-up test for RX72N (rx_usb bypass version).
 *
 * @details
 * Boots clocks, starts CMT0 ticker, enters ThreadX.  The heartbeat task calls
 * usb_min_init() (a 200-line hand-coded USB device that bypasses libs/rx_usb)
 * and then blinks PA7 forever while waiting for the host to enumerate.
 *
 * If the host can read the device + config descriptors and assign an
 * address, we know the RX72N USB hardware setup is correct and the bug
 * is somewhere inside libs/rx_usb.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

#include "tx_api.h"

extern void clock_init(void);
extern void cmt0_init(void);
extern void usb_min_init(void);

/*
 * Multi-LED diagnostic encoding (same six LEDs as blinky):
 *   PA7  -- heartbeat (always toggles, proves the firmware loop is alive)
 *   PB0  -- isr_count > 0 (the USB0 USBI ISR has fired at least once)
 *   P71  -- setup_count > 0 (a SETUP packet has reached handle_setup)
 *   P72  -- dvst_count > 0 (a bus-reset / device-state event has fired)
 *
 * If only PA7 toggles -> ISR isn't firing at all.
 * If PA7 + PB0       -> ISR fires but no SETUP packets.
 * If PA7 + PB0 + P72 -> bus reset arrives but no SETUP yet.
 * If PA7 + PB0 + P71 + P72 -> SETUP is being processed by us.
 */
typedef enum : uintptr_t {
    k_porta_pdr_addr  = 0x0008C00AU,
    k_porta_podr_addr = 0x0008C02AU,
    k_portb_pdr_addr  = 0x0008C00BU,
    k_portb_podr_addr = 0x0008C02BU,
    k_port7_pdr_addr  = 0x0008C007U,
    k_port7_podr_addr = 0x0008C027U,
} gpio_addrs_t;

typedef enum : uint8_t {
    k_pa7_mask = (uint8_t)(1U << 7U),
    k_pb0_mask = (uint8_t)(1U << 0U),
    k_pb5_mask = (uint8_t)(1U << 5U),
    k_p71_mask = (uint8_t)(1U << 1U),
    k_p72_mask = (uint8_t)(1U << 2U),
} gpio_bits_t;

static inline volatile uint8_t *porta_pdr(void)
{
    return (volatile uint8_t *)k_porta_pdr_addr;
}
static inline volatile uint8_t *porta_podr(void)
{
    return (volatile uint8_t *)k_porta_podr_addr;
}
static inline volatile uint8_t *portb_pdr(void)
{
    return (volatile uint8_t *)k_portb_pdr_addr;
}
static inline volatile uint8_t *portb_podr(void)
{
    return (volatile uint8_t *)k_portb_podr_addr;
}
static inline volatile uint8_t *port7_pdr(void)
{
    return (volatile uint8_t *)k_port7_pdr_addr;
}
static inline volatile uint8_t *port7_podr(void)
{
    return (volatile uint8_t *)k_port7_podr_addr;
}

extern volatile uint32_t g_usb_isr_count;
extern volatile uint32_t g_usb_setup_count;
extern volatile uint32_t g_usb_dvst_count;
extern volatile uint16_t g_usb_last_intsts;
extern volatile uint16_t g_usb_last_breq;

/* ==========================================================================
 * ThreadX task config
 * ========================================================================== */
typedef enum : uint16_t {
    k_usb_task_stack_sz = 2048U,
} usb_task_stack_t;

typedef enum : uint8_t {
    k_usb_task_priority = 10U,
    k_blink_period_ticks = 25U, /**< 25 ticks * 10 ms = 250 ms = 2 Hz */
} usb_task_cfg_t;

static TX_THREAD s_usb_tcb;
static uint8_t   s_usb_stack[k_usb_task_stack_sz];

static void usb_heartbeat_task(ULONG arg)
{
    (void)arg;

    /* All four diagnostic LEDs as outputs, all off at start. */
    *porta_pdr() |= k_pa7_mask;
    *portb_pdr() |= (uint8_t)(k_pb0_mask | k_pb5_mask);
    *port7_pdr() |= (uint8_t)(k_p71_mask | k_p72_mask);
    *porta_podr() &= (uint8_t)~k_pa7_mask;
    *portb_podr() &= (uint8_t)~(k_pb0_mask | k_pb5_mask);
    *port7_podr() &= (uint8_t)~(k_p71_mask | k_p72_mask);

    usb_min_init();

    for (;;) {
        *porta_podr() ^= k_pa7_mask;
        *portb_podr() ^= k_pb5_mask;
        if (g_usb_isr_count > 0U) {
            *portb_podr() |= k_pb0_mask;
        }
        if (g_usb_setup_count > 0U) {
            *port7_podr() |= k_p71_mask;
        }
        if (g_usb_dvst_count > 0U) {
            *port7_podr() |= k_p72_mask;
        }

        tx_thread_sleep((ULONG)k_blink_period_ticks);
    }
}

void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;

    (void)tx_thread_create(&s_usb_tcb,
                           "usb_min",
                           usb_heartbeat_task,
                           0U,
                           s_usb_stack,
                           (ULONG)k_usb_task_stack_sz,
                           k_usb_task_priority,
                           k_usb_task_priority,
                           TX_NO_TIME_SLICE,
                           TX_AUTO_START);
}

/**
 * @brief Quick visual heartbeat: flash PA7 N times at LOCO speed.
 * Proves firmware is alive BEFORE clock_init() or ThreadX.
 */
static void early_blink(uint8_t count)
{
    *porta_pdr()  |= k_pa7_mask;
    *portb_pdr()  |= k_pb5_mask;
    *porta_podr() &= (uint8_t)~k_pa7_mask;
    *portb_podr() &= (uint8_t)~k_pb5_mask;
    for (uint8_t n = 0U; n < count; n++) {
        *porta_podr() |= k_pa7_mask;
        *portb_podr() |= k_pb5_mask;
        for (volatile uint32_t d = 0U; d < 20000U; d++) { __asm__ volatile("nop"); }
        *porta_podr() &= (uint8_t)~k_pa7_mask;
        *portb_podr() &= (uint8_t)~k_pb5_mask;
        for (volatile uint32_t d = 0U; d < 20000U; d++) { __asm__ volatile("nop"); }
    }
}

int main(void)
{
    /* 1 blink = firmware entry (still on LOCO 240 kHz) */
    early_blink(1U);

    clock_init();

    /* 2 blinks = clock_init() returned OK (PLL running) */
    early_blink(2U);

    cmt0_init();

    /* 3 blinks = about to enter ThreadX */
    early_blink(3U);

    tx_kernel_enter(); /* Never returns */
    return 0;
}
