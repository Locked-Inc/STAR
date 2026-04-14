/**
 * @file main.c
 * @brief Minimal bare-metal RX72N test: blink P54 LED.
 *
 * @details
 * No RTOS. No BSP. No external clock config -- runs on the default clock
 * selected by OFS1 in option-setting memory. On a factory chip with OFS1
 * left at 0xFFFFFFFF this is the LOCO at 240 kHz, NOT HOCO at 16 MHz.
 * A small loop counter gives a visible blink even at 240 kHz.
 *
 * Register addresses from RX72N Hardware Manual (r01uh0824ej0111), Ch 22.
 *   PORT5 PDR  (direction)   = 0x0008C000 + 5 = 0x0008C005
 *   PORT5 PODR (output data) = 0x0008C020 + 5 = 0x0008C025
 *
 * After reset, PMR5 = 0x00 (all GPIO, not peripheral) -- no MPC unlock needed.
 *
 * Built with -O0 so the volatile delay loop isn't optimised out.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

#define PORT5_PDR   (*(volatile uint8_t *)0x0008C005U)
#define PORT5_PODR  (*(volatile uint8_t *)0x0008C025U)

#define P54_MASK    ((uint8_t)(1U << 4U))   /* Pin 4 of Port 5 */

/*
 * Tuned for LOCO = 240 kHz (factory-default internal clock).
 * 30000 iterations gives roughly a 1.5 s period -- half the previous
 * 60000-count version, so it is obvious when a new image was flashed.
 */
#define DELAY_COUNT 30000U

static void delay(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0U; i < count; i++) {
        __asm__ __volatile__("nop");
    }
}

int main(void)
{
    PORT5_PDR |= P54_MASK;           /* P54 = output */

    for (;;) {
        PORT5_PODR = P54_MASK;       /* LED on  */
        delay(DELAY_COUNT);
        PORT5_PODR = 0U;             /* LED off */
        delay(DELAY_COUNT);
    }
}
