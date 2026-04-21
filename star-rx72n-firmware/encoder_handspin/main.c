/**
 * @file main.c
 * @brief Encoder hand-spin verification -- no motors, just live TCNT prints.
 *
 * @details
 * Initializes the four encoders (MTU1, MTU2, TPU1, TPU2) via direct
 * register writes (bypasses the broken HAL). Motors are NEVER enabled.
 * Then prints all four 16-bit TCNT registers every 250 ms. Spin a wheel
 * by hand and the corresponding count should change.
 *
 * Expected on /dev/ttyACM0 (115200 8N1):
 *   ENCODER_HANDSPIN start
 *   m0=0xXXXX m1=0xXXXX m2=0xXXXX m3=0xXXXX
 *   ...
 *
 * Pin map (mirrors src/inc/hardware_config.h):
 *   m0 (FL,  MTU1) phase A=P24 (MTCLKA), phase B=P25 (MTCLKB)
 *   m1 (FR,  MTU2) phase A=PA1 (MTCLKC), phase B=PC5 (MTCLKD)
 *   m2 (BR,  TPU1) phase A=PC2 (TCLKA),  phase B=PA3 (TCLKB)
 *   m3 (BL,  TPU2) phase A=PC0 (TCLKC),  phase B=PB3 (TCLKD)
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include <stdbool.h>

extern void clock_init(void);
extern void sci9_debug_init(void);
extern void sci9_debug_puts(const char *s);
extern void sci9_debug_puthex16(uint16_t v);

#define REG8(a)  (*(volatile uint8_t  *)(uintptr_t)(a))
#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

typedef enum : uint32_t {
  k_iters_per_ms     = 40000U,
  k_print_period_ms  = 250U,
} timing_t;

typedef enum : uint8_t {
  k_motor_count = 4,
} count_t;

static const uintptr_t k_tcnt_addr[k_motor_count] = {
  0x000C1386U,        /* MTU1 TCNT (enc0 FL) */
  0x000C1406U,        /* MTU2 TCNT (enc1 FR) */
  0x00088120U + 6U,   /* TPU1 TCNT (enc2 BR) */
  0x00088130U + 6U,   /* TPU2 TCNT (enc3 BL) */
};

static void busy_wait_ms(uint32_t ms)
{
  for (uint32_t m = 0; m < ms; m++) {
    for (volatile uint32_t i = 0; i < k_iters_per_ms; i++) {
      __asm__ volatile("nop");
    }
  }
}

static void encoders_init(void)
{
  /* Step 1: enable MTU0-4 (MSTPCRA bit 9) and TPU0-5 (MSTPCRA bit 13). */
  volatile uint16_t *prcr    = (volatile uint16_t *)0x000803FEU;
  volatile uint32_t *mstpcra = (volatile uint32_t *)0x00080010U;
  *prcr     = 0xA502U;
  *mstpcra &= ~(uint32_t)((1UL << 9) | (1UL << 13));
  *prcr     = 0xA500U;

  /* Step 2: MPC PFS writes for all 8 encoder input pins, PSEL=0x02. */
  volatile uint8_t *pwpr = (volatile uint8_t *)0x0008C11FU;
  *pwpr = 0x00U; *pwpr = 0x40U;
  REG8(0x0008C140U + 2U * 8U + 4U)  = 0x02U; /* P24 */
  REG8(0x0008C140U + 2U * 8U + 5U)  = 0x02U; /* P25 */
  REG8(0x0008C140U + 10U * 8U + 1U) = 0x02U; /* PA1 */
  REG8(0x0008C140U + 12U * 8U + 5U) = 0x02U; /* PC5 */
  /* TPU TCLK PSEL is per-pin per the RX72N manual (NOT 0x02 like MTU).
   * Verified against PCnPFS, PAnPFS, PBnPFS tables in chapter 23. */
  REG8(0x0008C140U + 12U * 8U + 2U) = 0x03U; /* PC2 TCLKA  -- enc2 A (port C TCLK = 0x03) */
  REG8(0x0008C140U + 10U * 8U + 3U) = 0x04U; /* PA3 TCLKB  -- enc2 B (port A TCLKB = 0x04) */
  REG8(0x0008C140U + 12U * 8U + 0U) = 0x03U; /* PC0 TCLKC  -- enc3 A (port C TCLK = 0x03) */
  REG8(0x0008C140U + 11U * 8U + 3U) = 0x04U; /* PB3 TCLKD  -- enc3 B (port B TCLK = 0x04) */
  *pwpr = 0x00U; *pwpr = 0x80U;

  /* Step 3: PORT.PMR=1 on each input pin. */
  REG8(0x0008C062U) |= (uint8_t)((1U << 4) | (1U << 5));         /* P24, P25 */
  REG8(0x0008C06AU) |= (uint8_t)((1U << 1) | (1U << 3));         /* PA1, PA3 */
  REG8(0x0008C06CU) |= (uint8_t)((1U << 0) | (1U << 2) | (1U << 5)); /* PC0, PC2, PC5 */
  REG8(0x0008C06BU) |= (uint8_t)(1U << 3);                       /* PB3 */

  /* Step 4: MTU1 + MTU2 phase counting mode 1. */
  REG8(0x000C1380U)  = 0x00U;
  REG8(0x000C1381U)  = 0x04U;
  REG16(0x000C1386U) = 0U;
  REG8(0x000C1400U)  = 0x00U;
  REG8(0x000C1401U)  = 0x04U;
  REG16(0x000C1406U) = 0U;

  /* Step 5: TPU1 + TPU2 phase counting mode 1. */
  REG8(0x00088120U + 0U) = 0x00U;
  REG8(0x00088120U + 1U) = 0x04U;
  REG16(0x00088120U + 6U) = 0U;
  REG8(0x00088130U + 0U) = 0x00U;
  REG8(0x00088130U + 1U) = 0x04U;
  REG16(0x00088130U + 6U) = 0U;

  /* Step 6: start counters. */
  REG8(0x000C1290U) |= (uint8_t)((1U << 1) | (1U << 2)); /* MTU TSTRA: MTU1+MTU2 */
  REG8(0x00088100U) |= (uint8_t)((1U << 1) | (1U << 2)); /* TPU TSTR:  TPU1+TPU2 */
}

int main(void)
{
  clock_init();
  sci9_debug_init();
  sci9_debug_puts("\nENCODER_HANDSPIN start (motors OFF; spin wheels by hand)\n");

  encoders_init();
  sci9_debug_puts("encoders init ok; printing TCNT every 250ms\n");

  for (;;) {
    sci9_debug_puts("m0=");
    sci9_debug_puthex16(REG16(k_tcnt_addr[0]));
    sci9_debug_puts(" m1=");
    sci9_debug_puthex16(REG16(k_tcnt_addr[1]));
    sci9_debug_puts(" m2=");
    sci9_debug_puthex16(REG16(k_tcnt_addr[2]));
    sci9_debug_puts(" m3=");
    sci9_debug_puthex16(REG16(k_tcnt_addr[3]));
    sci9_debug_puts("\n");
    busy_wait_ms(k_print_period_ms);
  }
}
