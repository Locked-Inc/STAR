/**
 * @file clock.c
 * @brief RX72N clock init for the STAR PCB bench test: MOSC 24 MHz crystal
 *        -> PLL 240 MHz -> ICLK=120 / PCKA=120 / PCKB=60 / FCLK=60 MHz.
 *
 * @details
 * Self-contained reduced version of src/rx_clock_power_init.c, stripped of
 * the logging, asserts, simulator branches, and PPLL/USB clock paths the
 * uart test does not need. Configures only the main PLL.
 *
 * Path: EXTAL 24 MHz -> MOSC -> PLL (x10 / 1) = 240 MHz -> SCKCR=0x21C21211.
 * After this routine returns, ICLK is 120 MHz (half of production's 240 MHz
 * -- the bench test runs the CPU at reduced speed to cut power dissipation)
 * and PCLKA (the SCI/GPTW source for this test) is 120 MHz, matching
 * k_pclka_hz in libs/rx_hal/inc/rx72n_clock.h.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

#include "rx72n_regs.h"
#include "rx_register_protection.h"

/* ==========================================================================
 * Register addresses not exposed by rx_system_regs_t
 * ========================================================================== */
typedef enum : uintptr_t {
  k_packcr_addr = 0x00080044U, /**< Peripheral clock source: 0=PLL */
} clock_extra_addrs_t;

/* ==========================================================================
 * Bit / value constants (mirrors src/rx_clock_power_init.c)
 * ========================================================================== */
typedef enum : uint16_t {
  /*
   * PLLCR layout:
   *   bits[1:0]  PLIDIV    : 00 = /1
   *   bit [4]    PLLSRCSEL : 0 = MOSC, 1 = HOCO
   *   bits[13:8] STC       : multiplier = (STC + 1) / 2
   *
   * MOSC 24 MHz / 1 * 10 = 240 MHz. STC = (10 * 2) - 1 = 19 = 0x13.
   * PLLSRCSEL = 0 (MOSC) -> bit 4 clear.
   *
   * Production rx_clock_power_init.c uses k_pll_multiplier_10_div_1 = 0x1300.
   */
  k_pllcr_mosc_x10 = 0x1300U,
} clock_word_constants_t;

typedef enum : uint32_t {
  /*
   * SCKCR dividers (bench-test config, ICLK half-speed vs production):
   *   FCK=/4 (60), ICK=/2 (120), PSTOP0=1, PSTOP1=1, BCK=/4 (60),
   *   PCKA=/2 (120), PCKB=/4 (60), PCKC=/2 (120), PCKD=/2 (120).
   */
  k_sckcr_value = 0x21C21211U,
} clock_sckcr_t;

typedef enum : uint8_t {
  k_sub_clock_stopped = 0x01U, /**< SOSCCR: stop sub-clock oscillator */
  k_main_osc_enabled  = 0x00U, /**< MOSCCR: 0 = MOSC running */
  k_pll_enabled       = 0x00U, /**< PLLCR2: 0 = PLL running */
  k_oscovfsr_moovf    = 0x01U, /**< OSCOVFSR bit 0: MOSC stable */
  k_oscovfsr_plovf    = 0x04U, /**< OSCOVFSR bit 2: PLL stable */
} clock_byte_constants_t;

typedef enum : uint32_t {
  /* Bounded poll counts (NASA Rule 2). We poll the hardware ready flag
   * (OSCOVFSR.MOOVF / .PLOVF) so the actual wait is hardware-determined --
   * roughly 10 ms physical time regardless of CPU clock. The upper bound
   * just prevents an infinite spin if the crystal is missing. ~500k iters
   * at LOCO 240 kHz is ~3 s, which is still much shorter than the prior
   * fixed 10-second busy-wait. */
  k_main_osc_poll_max  = 500000U,
  k_pll_lock_poll_max  = 500000U,
} clock_timeout_t;

/* ==========================================================================
 * clock_init -- bring the chip from POR defaults to PLL 240 / PCKA 120 MHz.
 * ========================================================================== */
void clock_init(void)
{
  volatile rx_system_regs_t *sys = system_regs();

  /* Unlock PRC0 (clock) + PRC1 (module stop) write protection. */
  *prcr_reg() = k_rx_prcr_unlock_prc0_prc1;

  /* Stop sub-clock oscillator -- not used on STAR. */
  sys->sosccr = k_sub_clock_stopped;

  /* Start MOSC (24 MHz external crystal). */
  sys->mosccr = k_main_osc_enabled;

  /* Poll the hardware MOSC-stable flag. Exits as soon as the crystal has
   * settled (typically ~10 ms physical time) instead of waiting a fixed
   * 2.4M cycles, which on the LOCO 240 kHz boot clock is ~10 seconds. */
  for (volatile uint32_t i = 0; i < k_main_osc_poll_max; i++) {
    if ((sys->oscovfsr & k_oscovfsr_moovf) != 0U) {
      break;
    }
  }

  /* Configure and enable PLL: 24 MHz x 10 / 1 = 240 MHz. */
  sys->pllcr  = k_pllcr_mosc_x10;
  sys->pllcr2 = k_pll_enabled;

  /* Wait for PLL lock (bounded). If we time out, the crystal is missing
   * or not driving -- DO NOT switch SCKCR3 to PLL or the chip will halt
   * on a dead clock. Stay on LOCO so the LEDs still respond and the user
   * can see we got past clock_init. */
  bool pll_locked = false;
  for (volatile uint32_t i = 0; i < k_pll_lock_poll_max; i++) {
    if ((sys->oscovfsr & k_oscovfsr_plovf) != 0U) {
      pll_locked = true;
      break;
    }
  }

  if (pll_locked) {
    /* MANDATORY: raise flash wait state BEFORE switching to >120 MHz. */
    *memwait_reg() = k_memwait_one_wait;

    /* Apply system clock dividers. */
    sys->sckcr = k_sckcr_value;

    /* Route UCLK from main PLL (UPLLSEL=0). */
    *((volatile uint16_t *)k_packcr_addr) = 0x0000U;

    /* Switch system clock source to PLL. */
    sys->sckcr3 = k_sckcr3_cksel_pll;
  }

  /* Re-lock PRCR. */
  *prcr_reg() = k_rx_prcr_lock;
}
