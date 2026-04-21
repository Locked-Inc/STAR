/**
 * @file main.c
 * @brief SCI9 UART verification -- prints "TICK 0xNNNN\n" every second.
 *
 * @details
 * Boots, initializes the clock and SCI9 (TXD9 = PB7), then loops
 * forever printing a hex tick counter with a 1-second cadence so the
 * host can confirm a continuous stream on /dev/ttyACM0 @ 115200 8N1.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

extern void clock_init(void);
extern void sci9_debug_init(void);
extern void sci9_debug_puts(const char *s);
extern void sci9_debug_puthex16(uint16_t v);

typedef enum : uint32_t {
  k_iters_per_ms = 240000U,
  k_tick_period_ms = 200U, /* 5 ticks/sec so a short capture catches several */
} timing_t;

static void busy_wait_ms(uint32_t ms)
{
  for (uint32_t m = 0; m < ms; m++) {
    for (volatile uint32_t i = 0; i < k_iters_per_ms; i++) {
      __asm__ volatile("nop");
    }
  }
}

int main(void)
{
  clock_init();
  sci9_debug_init();

  sci9_debug_puts("\nUART_TEST start (115200 8N1, TXD9=PB7)\n");

  uint16_t count = 0;
  for (;;) {
    sci9_debug_puts("TICK ");
    sci9_debug_puthex16(count);
    sci9_debug_puts("\n");
    busy_wait_ms(k_tick_period_ms);
    count++;
  }
}
