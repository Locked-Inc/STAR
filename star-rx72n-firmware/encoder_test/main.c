/**
 * @file main.c
 * @brief Closed-loop encoder verification test.
 *
 * @details
 * Drives all 4 motors at +50 percent duty for ~3 seconds while sampling each
 * encoder's hardware count. Stops motors, then dumps the count deltas via
 * SCI9 UART so the host (cat /dev/ttyACM0 @ 115200) can capture and verify
 * each encoder produced ticks.
 *
 * Expected output on /dev/ttyACM0:
 *   ENCODER_TEST start
 *   init clock ok
 *   init leds ok
 *   init uart ok
 *   init drv gpio ok
 *   init pwm ok
 *   init enc ok
 *   before m0=0xXXXX m1=0xXXXX m2=0xXXXX m3=0xXXXX
 *   spinning motors at +50pct for 3 seconds
 *   after  m0=0xXXXX m1=0xXXXX m2=0xXXXX m3=0xXXXX
 *   motors stopped
 *   delta m0=0xXXXX m1=0xXXXX m2=0xXXXX m3=0xXXXX
 *   summary FAIL m0,m3 (or PASS)
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "rx_motor.h"
#include "rx_gptw.h"
#include "rx_mpc.h"
#include "rx_port_constants.h"
#include "rx_mtu_encoder.h"
#include "rx_encoder_tpu.h"
#include "rx72n_mtu_regs.h"
#include "rx72n_tpu_regs.h"
#include "rx72n_system_regs.h"
#include "rx_register_protection.h"

extern void clock_init(void);
extern void sci9_debug_init(void);
extern void sci9_debug_puts(const char *s);
extern void sci9_debug_puthex16(uint16_t v);

/* ==========================================================================
 * Direct port-register access (same pattern as motor_spin_test).
 * ========================================================================== */
#define REG8(a) (*(volatile uint8_t *)(uintptr_t)(a))

typedef enum : uintptr_t {
  k_port_pdr_base  = 0x0008C000U,
  k_port_podr_base = 0x0008C020U,
  k_port_pmr_base  = 0x0008C060U,
} port_reg_base_t;

static inline volatile uint8_t *port_pdr(uint8_t port)  { return &REG8(k_port_pdr_base  + port); }
static inline volatile uint8_t *port_podr(uint8_t port) { return &REG8(k_port_podr_base + port); }
static inline volatile uint8_t *port_pmr(uint8_t port)  { return &REG8(k_port_pmr_base  + port); }

/* ==========================================================================
 * Status LEDs (active-high, same map as motor_spin_test).
 * ========================================================================== */
typedef enum : uint8_t {
  k_port_a   = 10,
  k_port_b   = 11,
  k_port_7   = 7,
  k_bit_led0 = 7, /* PA7 D9  -- heartbeat */
  k_bit_led1 = 0, /* PB0 D10 -- after init */
  k_bit_led2 = 1, /* P71 D11 -- motor PWM running */
  k_bit_led3 = 2, /* P72 D12 -- encoders all ticked */
  k_bit_led4 = 1, /* PB1 D13 -- test PASS */
  k_bit_led5 = 2, /* PB2 D14 -- test FAIL or init error */
} status_led_pins_t;

static inline void gpio_set_output(uint8_t port, uint8_t pin)
{
  *port_pmr(port) &= (uint8_t) ~(1U << pin);
  *port_pdr(port) |= (uint8_t)(1U << pin);
}
static inline void gpio_write(uint8_t port, uint8_t pin, bool high)
{
  if (high) { *port_podr(port) |= (uint8_t)(1U << pin); }
  else      { *port_podr(port) &= (uint8_t) ~(1U << pin); }
}
static inline void led(uint8_t port, uint8_t bit, bool on) { gpio_write(port, bit, on); }

static void leds_init(void)
{
  gpio_set_output(k_port_a, k_bit_led0); led(k_port_a, k_bit_led0, false);
  gpio_set_output(k_port_b, k_bit_led1); led(k_port_b, k_bit_led1, false);
  gpio_set_output(k_port_7, k_bit_led2); led(k_port_7, k_bit_led2, false);
  gpio_set_output(k_port_7, k_bit_led3); led(k_port_7, k_bit_led3, false);
  gpio_set_output(k_port_b, k_bit_led4); led(k_port_b, k_bit_led4, false);
  gpio_set_output(k_port_b, k_bit_led5); led(k_port_b, k_bit_led5, false);
}

/* ==========================================================================
 * Per-motor wiring + per-encoder timer-channel mapping. Mirrors
 * src/inc/hardware_config.h but kept local so the test can compile
 * without depending on the application include tree.
 * ==========================================================================
 *
 * NOTE: The back-wheel Hall encoder harness is cross-wired vs silk. BR's
 * encoder cable physically lands on TPU2 pins (PC0/PB3) and BL's lands on
 * TPU1 pins (PC2/PA3). The Encoder-unit column below reflects the actual
 * hardware wiring, so spinning wheel BR increments enc2 (reading TPU2).
 * Matches production (src/tasks/motor_control_task.c:2214-2216).
 *
 * | Idx | Wheel | GPTW | IN1 (PWM) | IN2 (dir) | DRVOFF | nSLEEP | Encoder unit |
 * |-----|-------|------|-----------|-----------|--------|--------|--------------|
 * |  0  | FL    | 0    | P17       | P23       | P61    | P60    | MTU1         |
 * |  1  | FR    | 1    | PC3       | P22       | P63    | P62    | MTU2         |
 * |  2  | BR    | 2    | P86       | PE3       | PE0    | P64    | TPU2         |
 * |  3  | BL    | 3    | PC6       | PE7       | PE2    | PE1    | TPU1         |
 */
typedef struct {
  rx_gptw_channel_t gptw_channel;
  rx_port_pin_t     in2_pin;
  rx_port_pin_t     in1_pin;
  uint8_t           drvoff_port;
  uint8_t           drvoff_pin;
  uint8_t           nsleep_port;
  uint8_t           nsleep_pin;
  bool              is_tpu;         /* false = MTU1/2, true = TPU1/2 */
  uint8_t           timer_channel;  /* MTU 1/2 or TPU 1/2 */
  int8_t            direction_sign; /* +1 = positive duty drives wheel forward;
                                       -1 = motor wired backwards (M0 FL, M3 BL) */
} motor_wiring_t;

typedef enum : uint8_t {
  k_motor_count = 4,
  k_port_6      = 6,
  k_port_e      = 14,
} test_constants_t;

static const motor_wiring_t k_motors[k_motor_count] = {
  /* M0 = FL (front-left). Wired backwards. */
  {.gptw_channel = k_gptw_channel_0, .in2_pin = k_rx_p2_3, .in1_pin = k_rx_p1_7,
   .drvoff_port = k_port_6, .drvoff_pin = 1, .nsleep_port = k_port_6, .nsleep_pin = 0,
   .is_tpu = false, .timer_channel = 1, .direction_sign = -1},
  /* M1 = FR. */
  {.gptw_channel = k_gptw_channel_1, .in2_pin = k_rx_p2_2, .in1_pin = k_rx_pc_3,
   .drvoff_port = k_port_6, .drvoff_pin = 3, .nsleep_port = k_port_6, .nsleep_pin = 2,
   .is_tpu = false, .timer_channel = 2, .direction_sign = +1},
  /* M2 = BR. Harness: Hall cable lands on TPU2 pins (PC0/PB3). */
  {.gptw_channel = k_gptw_channel_2, .in2_pin = k_rx_pe_3, .in1_pin = k_rx_p8_6,
   .drvoff_port = k_port_e, .drvoff_pin = 0, .nsleep_port = k_port_6, .nsleep_pin = 4,
   .is_tpu = true,  .timer_channel = 2, .direction_sign = +1},
  /* M3 = BL. Wired backwards (matches FL on left side). Harness: Hall cable
   * lands on TPU1 pins (PC2/PA3). */
  {.gptw_channel = k_gptw_channel_3, .in2_pin = k_rx_pe_7, .in1_pin = k_rx_pc_6,
   .drvoff_port = k_port_e, .drvoff_pin = 2, .nsleep_port = k_port_e, .nsleep_pin = 1,
   .is_tpu = true,  .timer_channel = 1, .direction_sign = -1},
};

/* ==========================================================================
 * Bounded busy-wait helper (NASA Rule 2). ~1 ms = 240k iters at 240 MHz -O1.
 * ==========================================================================
 */
typedef enum : uint32_t {
  /* Empirically calibrated: 40000 iters/ms at 240 MHz with -O1 volatile loop
   * (~6 cycles per iteration). 240000 was 6x too slow. */
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
 * DRV8263H DRVOFF/nSLEEP power-up sequence (same as motor_spin_test).
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
  busy_wait_ms(10); /* tWAKE */
  for (uint8_t i = 0; i < k_motor_count; i++) {
    gpio_write(k_motors[i].drvoff_port, k_motors[i].drvoff_pin, false);
  }
}

/* ==========================================================================
 * GPTW PWM init (per-motor, via the lib).
 * ========================================================================== */
static bool motor_pwm_init(rx_motor_handle_t handles[k_motor_count])
{
  for (uint8_t i = 0; i < k_motor_count; i++) {
    const rx_motor_config_t cfg = {
      .channel      = k_motors[i].gptw_channel,
      .output_a     = k_gptw_output_a,
      .output_b     = k_gptw_output_b,
      .pwm_freq_hz  = 20000U,
      .dead_time_ns = 1000U,
      .invert_pwm   = false,
      .port_a_idx   = (uint8_t)rx_port_from_pin(k_motors[i].in2_pin),
      .bit_a        = (uint8_t)rx_pin_from_pin(k_motors[i].in2_pin),
      .port_b_idx   = (uint8_t)rx_port_from_pin(k_motors[i].in1_pin),
      .bit_b        = (uint8_t)rx_pin_from_pin(k_motors[i].in1_pin),
    };
    if (rx_motor_init(&handles[i], &cfg) != k_rx_ok) { return false; }
  }
  return true;
}

/* ==========================================================================
 * Encoder init -- MTU for FL/FR (idx 0, 1), TPU for BR/BL (idx 2, 3).
 * ========================================================================== */
typedef enum : uint16_t {
  k_counts_per_rev = 1364, /* 341 PPR x 4 (matches production) */
} enc_const_t;

/* Encoder phase A/B input pins. Reflects the cross-wired back-wheel harness:
 * BR's Hall cable is on TPU2 pins (PC0/PB3) and BL's is on TPU1 pins
 * (PC2/PA3). Matches production (motor_control_task.c:2214-2216).
 *   Encoder 0 (FL,  MTU1): P24 / P25
 *   Encoder 1 (FR,  MTU2): PA1 / PC5
 *   Encoder 2 (BR,  TPU2): PC0 / PB3
 *   Encoder 3 (BL,  TPU1): PC2 / PA3
 */
typedef struct { uint8_t port; uint8_t bit; bool is_tpu; } enc_pin_t;
static const enc_pin_t k_enc_pins[] = {
  {2,  4, false}, {2,  5, false}, /* enc0: P24 / P25 (MTU1 MTCLKA/B)       -- FL */
  {10, 1, false}, {12, 5, false}, /* enc1: PA1 / PC5 (MTU2 MTCLKC/D)       -- FR */
  {12, 0, true},  {11, 3, true},  /* enc2: PC0 / PB3 (TPU2 TCLKC/D, harness) -- BR */
  {12, 2, true},  {10, 3, true},  /* enc3: PC2 / PA3 (TPU1 TCLKA/B, harness) -- BL */
};

/* Direct-register encoder init -- the lib's rx_mpc_set_*_encoder doesn't
 * actually write PFS, and rx_mtu_start doesn't actually set TSTRA. Bypass
 * both. Verified against RX72N HW manual chapter 25 (MTU3a) + chapter 27
 * (TPUa) + chapter 23 (MPC PFS tables 23.4 / 23.6 / 23.10 / 23.14 / 23.16).
 *
 * MTU1 (enc0 FL): base 0x000C1380, TCR/TMDR/TCNT at +0/+1/+6, TSTRA bit 1
 * MTU2 (enc1 FR): base 0x000C1400, same offsets,                TSTRA bit 2
 * TPU2 (enc2 BR): base 0x00088130, TCR/TMDR/TCNT at +0/+1/+6, TSTR  bit 2 (harness)
 * TPU1 (enc3 BL): base 0x00088120, same offsets,                TSTR  bit 1 (harness)
 *
 * Phase counting mode 1 = TMDR=0x04. TCR=0x00 (external clock). PSEL=0x02
 * for MTCLK[A-D] on port 2/A/C; TCLK[A-D] on port C/A/C/B.
 */

#define REG8_AT(a)  (*(volatile uint8_t  *)(uintptr_t)(a))
#define REG16_AT(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG32_AT(a) (*(volatile uint32_t *)(uintptr_t)(a))

/* MTU/TPU encoder init using the production accessors from
 * rx72n_{mtu,tpu,system}_regs.h instead of hardcoded hex addresses.
 * Keeps PFS writes raw because the audited rx_mpc helpers stop
 * short of writing PFS for MTCLK/TCLK pins (HAL bug, see memory
 * project_motor_pwm_hal_bugs.md). */
static bool encoders_init(void)
{
  /* Step 1: enable MTU (MSTPA9) + TPU (MSTPA13) module clocks. */
  *prcr_reg() = k_rx_prcr_unlock_all;
  system_regs()->mstpcra &= ~(uint32_t)((1UL << 9) | (uint32_t)k_tpu_mstpcra_mstpa13);
  *prcr_reg() = k_rx_prcr_lock;

  /* Step 2 + 3: RX72N MPC pin setup requires the sequence PMR=0, write PFS,
   * PMR=1. MTCLKA/MTCLKB on P24/P25 take PSEL=0x02 (the production header's
   * old k_psel_mtu_phase=0x03 was WRONG -- 0x02 is the MTCLK PSEL on RX72N).
   * For TPU TCLK keep our verified per-pin values. */
  volatile uint8_t *pwpr  = (volatile uint8_t *)0x0008C11FU;
  volatile uint8_t *p2pmr = (volatile uint8_t *)0x0008C062U;
  volatile uint8_t *papmr = (volatile uint8_t *)0x0008C06AU;
  volatile uint8_t *pbpmr = (volatile uint8_t *)0x0008C06BU;
  volatile uint8_t *pcpmr = (volatile uint8_t *)0x0008C06CU;

  /* clear PMR for all encoder input pins so PFS writes take effect */
  *p2pmr &= (uint8_t)~(uint8_t)((1U << 4) | (1U << 5));
  *papmr &= (uint8_t)~(uint8_t)((1U << 1) | (1U << 3));
  *pcpmr &= (uint8_t)~(uint8_t)((1U << 0) | (1U << 2) | (1U << 5));
  *pbpmr &= (uint8_t)~(uint8_t)(1U << 3);

  /* PFS writes */
  *pwpr = 0x00U; *pwpr = 0x40U;
  REG8_AT(0x0008C140U + 2U * 8U + 4U)  = 0x02U; /* P24 MTCLKA -- enc0 A (MTU PSEL=0x02, RX72N HW manual R01UH0824EJ0111 Table 23.6) */
  REG8_AT(0x0008C140U + 2U * 8U + 5U)  = 0x02U; /* P25 MTCLKB -- enc0 B */
  REG8_AT(0x0008C140U + 10U * 8U + 1U) = 0x02U; /* PA1 MTCLKC -- enc1 A */
  REG8_AT(0x0008C140U + 12U * 8U + 5U) = 0x02U; /* PC5 MTCLKD -- enc1 B */
  REG8_AT(0x0008C140U + 12U * 8U + 2U) = 0x03U; /* PC2 TCLKA  -- enc3 A (TPU1, reads BL per harness) */
  REG8_AT(0x0008C140U + 10U * 8U + 3U) = 0x04U; /* PA3 TCLKB  -- enc3 B (TPU1, reads BL per harness) */
  REG8_AT(0x0008C140U + 12U * 8U + 0U) = 0x03U; /* PC0 TCLKC  -- enc2 A (TPU2, reads BR per harness) */
  REG8_AT(0x0008C140U + 11U * 8U + 3U) = 0x04U; /* PB3 TCLKD  -- enc2 B (TPU2, reads BR per harness) */
  *pwpr = 0x00U; *pwpr = 0x80U;

  /* set PMR=1 to route the pad to the peripheral */
  *p2pmr |= (uint8_t)((1U << 4) | (1U << 5));
  *papmr |= (uint8_t)((1U << 1) | (1U << 3));
  *pcpmr |= (uint8_t)((1U << 0) | (1U << 2) | (1U << 5));
  *pbpmr |= (uint8_t)(1U << 3);

  /* Step 4: phase counting mode 1 on MTU1 + MTU2 via accessors. */
  volatile rx_mtu_channel_regs_t* m1 = mtu1();
  m1->tcr   = 0;
  m1->tmdr  = 0x04U;  /* phase counting mode 1 */
  m1->tiorh = 0;
  m1->tiorl = 0;
  m1->tier  = 0;
  m1->tsr   = 0;
  m1->tcnt  = 0;
  volatile rx_mtu_channel_regs_t* m2 = mtu2();
  m2->tcr   = 0;
  m2->tmdr  = 0x04U;
  m2->tiorh = 0;
  m2->tiorl = 0;
  m2->tier  = 0;
  m2->tsr   = 0;
  m2->tcnt  = 0;

  /* Step 5: configure TPU1 + TPU2 for phase counting mode 1 via accessors. */
  volatile rx_tpu_regs_t* t1 = tpu1();
  t1->tcr  = 0;
  t1->tmdr = 0x04U;
  t1->tcnt = 0;
  volatile rx_tpu_regs_t* t2 = tpu2();
  t2->tcr  = 0;
  t2->tmdr = 0x04U;
  t2->tcnt = 0;

  /* Step 6: disable noise filter on TPU1/TPU2 explicitly (HW reset clears,
   * but ensure leftover state from a prior run is gone). */
  tpu_control()->nfcr[1] = 0;
  tpu_control()->nfcr[2] = 0;

  /* Step 7: start counters. */
  mtu_tstra()->tstr     |= (uint8_t)(k_mtu_tstr_cst1 | k_mtu_tstr_cst2);
  tpu_control()->tstr   |= (uint8_t)(k_tpu_tstr_cst1 | k_tpu_tstr_cst2);

  return true;
}

static bool encoder_read_raw(uint8_t motor_idx, uint16_t *out_count)
{
  /* Direct register read -- bypass the lib (rx_encoder_read_raw works
   * but for symmetry with the bypass init). */
  static const uintptr_t k_tcnt_addr[k_motor_count] = {
    0x000C1386U, /* MTU1 */
    0x000C1406U, /* MTU2 */
    0x00088120U + 6U, /* TPU1 */
    0x00088130U + 6U, /* TPU2 */
  };
  *out_count = REG16_AT(k_tcnt_addr[motor_idx]);
  return true;
}

/* ==========================================================================
 * UART helpers -- print 4 motor counts on one line.
 * ========================================================================== */
static void print_counts(const char *label, uint16_t c[k_motor_count])
{
  sci9_debug_puts(label);
  sci9_debug_puts(" m0=");
  sci9_debug_puthex16(c[0]);
  sci9_debug_puts(" m1=");
  sci9_debug_puthex16(c[1]);
  sci9_debug_puts(" m2=");
  sci9_debug_puthex16(c[2]);
  sci9_debug_puts(" m3=");
  sci9_debug_puthex16(c[3]);
  sci9_debug_puts("\n");
}

static void error_park(const char *msg)
{
  sci9_debug_puts("FATAL ");
  sci9_debug_puts(msg);
  sci9_debug_puts("\n");
  led(k_port_b, k_bit_led5, true);
  for (;;) { __asm__ volatile("nop"); }
}

/* ==========================================================================
 * Test sequence
 * ========================================================================== */
typedef enum : uint32_t {
  k_spin_ms     = 3000U, /* drive motors this long */
  k_settle_ms   = 500U,  /* let everything stop */
  k_min_delta   = 50U,   /* minimum ticks expected per encoder over k_spin_ms */
} test_timing_t;

int main(void)
{
  clock_init();
  leds_init();
  led(k_port_b, k_bit_led1, true);

  sci9_debug_init();
  motor_drv_gpio_init();

  static rx_motor_handle_t s_motors[k_motor_count];
  if (!motor_pwm_init(s_motors)) { error_park("motor_pwm_init"); }
  if (!encoders_init())          { error_park("encoders_init"); }

  /* === Sanity check: can MTU1 count AT ALL using its internal clock?
   * This bypasses the encoder pin routing entirely. Set TCR.TPSC=000
   * (PCLK/1) and TMDR=0 (normal mode). At 60 MHz PCLKB the counter should
   * wrap nearly continuously. If TCNT stays at 0x0000 here too, MTU1
   * itself isn't running -- not a pin routing issue. */
  REG8_AT(0x000C1280U) &= (uint8_t)~(uint8_t)((1U << 1) | (1U << 2)); /* stop MTU1, MTU2 */
  REG8_AT(0x000C1380U) = 0x00U;  /* TCR  = TPSC=000 (PCLK/1), CKEG=0, CCLR=0 */
  REG8_AT(0x000C1381U) = 0x00U;  /* TMDR = normal mode */
  REG16_AT(0x000C1386U) = 0;     /* TCNT = 0 */
  REG8_AT(0x000C1280U) |= (uint8_t)(1U << 1); /* start MTU1 */
  busy_wait_ms(10);
  uint16_t internal_cnt = REG16_AT(0x000C1386U);
  sci9_debug_puts("MTU1 internal-clock test (10 ms PCLK/1): TCNT=0x");
  sci9_debug_puthex16(internal_cnt);
  sci9_debug_puts(internal_cnt != 0 ? " ALIVE\n" : " DEAD\n");
  /* Restore MTU1+MTU2 to phase counting mode for the spin loop. */
  REG8_AT(0x000C1280U) &= (uint8_t)~(uint8_t)((1U << 1) | (1U << 2));
  REG8_AT(0x000C1380U) = 0x00U;
  REG8_AT(0x000C1381U) = 0x04U;
  REG16_AT(0x000C1386U) = 0;
  REG8_AT(0x000C1400U) = 0x00U;
  REG8_AT(0x000C1401U) = 0x04U;
  REG16_AT(0x000C1406U) = 0;
  REG8_AT(0x000C1280U) |= (uint8_t)((1U << 1) | (1U << 2));

  /* Loop the test forever so any capture window catches a full cycle.
   * Each iteration: print banner, sample, spin 3s, sample, stop, summarize,
   * idle 2s, repeat. */
  uint16_t iter = 0;
  for (;;) {
    sci9_debug_puts("\n=== ENCODER_TEST iter=");
    sci9_debug_puthex16(iter);
    sci9_debug_puts(" ===\n");

    uint16_t before[k_motor_count] = {0};
    for (uint8_t i = 0; i < k_motor_count; i++) {
      if (!encoder_read_raw(i, &before[i])) { error_park("encoder_read_raw before"); }
    }
    print_counts("before", before);

    /* Dump MTU1 + TSTRA + a few port registers using production accessors. */
    {
      volatile rx_mtu_channel_regs_t* m = mtu1();
      sci9_debug_puts("regs: TCR=");  sci9_debug_puthex16(m->tcr);
      sci9_debug_puts(" TMDR=");      sci9_debug_puthex16(m->tmdr);
      sci9_debug_puts(" TIORH=");     sci9_debug_puthex16(m->tiorh);
      sci9_debug_puts(" TIER=");      sci9_debug_puthex16(m->tier);
      sci9_debug_puts(" TSR=");       sci9_debug_puthex16(m->tsr);
      sci9_debug_puts(" TCNT=");      sci9_debug_puthex16(m->tcnt);
      sci9_debug_puts(" TSTRA=");     sci9_debug_puthex16(mtu_tstra()->tstr);
      sci9_debug_puts(" P24PFS=");    sci9_debug_puthex16(*(volatile uint8_t*)0x0008C154U);
      sci9_debug_puts(" P25PFS=");    sci9_debug_puthex16(*(volatile uint8_t*)0x0008C155U);
      sci9_debug_puts(" P2PMR=");     sci9_debug_puthex16(*(volatile uint8_t*)0x0008C062U);
      sci9_debug_puts(" P2PIDR=");    sci9_debug_puthex16(*(volatile uint8_t*)0x0008C042U);
      sci9_debug_puts("\n");
    }
    sci9_debug_puts("spinning motors (per-motor sign applied) for 3s, sampling each second\n");
    led(k_port_7, k_bit_led2, true);
    for (uint8_t i = 0; i < k_motor_count; i++) {
      const float duty = 50.0F * (float)k_motors[i].direction_sign;
      (void)rx_motor_set_duty(&s_motors[i], duty);
    }

    /* Burst-read MTU1.TCNT in a tight loop right after zeroing it -- if
     * MTU is counting fast (TSR.TCFV is set in our dump => overflow has
     * fired at least once), the values will move quickly across this loop,
     * even though the periodic 1-second sample lands at 0. */
    mtu1()->tcnt = 0;
    sci9_debug_puts("burst MTU1.TCNT:");
    for (uint8_t s = 0; s < 8; s++) {
      busy_wait_ms(1);
      uint16_t c = mtu1()->tcnt;
      sci9_debug_puts(" ");
      sci9_debug_puthex16(c);
    }
    sci9_debug_puts("\n");

    /* Sample 3 times during the spin so we can see if counters move at all. */
    volatile uint8_t *p2_pidr = (volatile uint8_t *)0x0008C042U;
    volatile uint8_t *pa_pidr = (volatile uint8_t *)0x0008C04AU;
    volatile uint8_t *pc_pidr = (volatile uint8_t *)0x0008C04CU;
    uint16_t mid[k_motor_count] = {0};
    for (uint8_t s = 0; s < 3; s++) {
      busy_wait_ms(k_spin_ms / 3U);
      for (uint8_t i = 0; i < k_motor_count; i++) {
        (void)encoder_read_raw(i, &mid[i]);
      }
      sci9_debug_puts("mid   ");
      sci9_debug_puthex16(mid[0]); sci9_debug_puts(" ");
      sci9_debug_puthex16(mid[1]); sci9_debug_puts(" ");
      sci9_debug_puthex16(mid[2]); sci9_debug_puts(" ");
      sci9_debug_puthex16(mid[3]);
      sci9_debug_puts(" pidr2="); sci9_debug_puthex16(*p2_pidr);
      sci9_debug_puts(" pidrA="); sci9_debug_puthex16(*pa_pidr);
      sci9_debug_puts(" pidrC="); sci9_debug_puthex16(*pc_pidr);
      sci9_debug_puts("\n");
    }

    uint16_t after[k_motor_count] = {0};
    for (uint8_t i = 0; i < k_motor_count; i++) {
      if (!encoder_read_raw(i, &after[i])) { error_park("encoder_read_raw after"); }
    }
    print_counts("after ", after);

    for (uint8_t i = 0; i < k_motor_count; i++) {
      (void)rx_motor_set_duty(&s_motors[i], 0.0F);
    }
    led(k_port_7, k_bit_led2, false);
    sci9_debug_puts("motors stopped\n");
    busy_wait_ms(k_settle_ms);

    uint16_t delta[k_motor_count];
    for (uint8_t i = 0; i < k_motor_count; i++) {
      delta[i] = (uint16_t)(after[i] - before[i]);
    }
    print_counts("delta ", delta);

    bool pass = true;
    for (uint8_t i = 0; i < k_motor_count; i++) {
      int32_t signed_delta = (int32_t)(int16_t)delta[i];
      uint32_t mag = (uint32_t)((signed_delta < 0) ? -signed_delta : signed_delta);
      if (mag < k_min_delta) {
        pass = false;
        sci9_debug_puts("encoder FAIL idx=");
        sci9_debug_puthex16((uint16_t)i);
        sci9_debug_puts("\n");
      }
    }
    sci9_debug_puts(pass ? "summary PASS\n" : "summary FAIL\n");
    if (pass) { led(k_port_b, k_bit_led4, true); }
    else      { led(k_port_b, k_bit_led5, true); }

    /* Idle 2s then repeat. */
    busy_wait_ms(2000);
    iter++;
  }
}
