/* src/hardware/system_init.c */

/**
 * @file system_init.c
 * @brief RX72N System Initialization
 *
 * Configures:
 * - Clock system (240 MHz from PLL)
 * - Module stop control
 * - Interrupt controller
 */

#include <stdint.h>

#include "rx72n_regs.h"

/* =============================================================================
 * Clock Configuration
 * =============================================================================
 */

/**
 * @brief Initialize clock system to 240 MHz
 *
 * Clock tree:
 * - Main oscillator: 16 MHz (external crystal)
 * - PLL: 16 MHz × 30 / 2 = 240 MHz
 * - ICLK (CPU): 240 MHz
 * - PCLKA: 120 MHz
 * - PCLKB/C/D: 60 MHz
 * - FCLK (Flash): 60 MHz
 */
void clock_init(void)
{
  /* Protect off for clock registers */
  SYSTEM.PRCR = 0xA50F;

  /* Stop sub-clock oscillator (not used) */
  SYSTEM.SOSCCR = 0x01;

  /* Start main oscillator (16 MHz external crystal) */
  SYSTEM.MOSCCR = 0x00; /* Enable main oscillator */

  /* Wait for main oscillator stabilization (typically 10ms) */
  /* Simple delay loop - should be ~2.4M cycles at default 240MHz */
  for (volatile uint32_t i = 0; i < 2400000; i++) {
    __asm__ volatile("nop");
  }

  /* Configure PLL */
  /* PLL = (Main OSC × PLIDIV) / PLLSTBY */
  /* 240 MHz = (16 MHz × 30) / 2 */
  SYSTEM.PLLCR  = 0x1D01; /* PLIDIV=30-1=29(0x1D), STC=1 (div by 2) */
  SYSTEM.PLLCR2 = 0x00;   /* Enable PLL */

  /* Wait for PLL stabilization */
  while ((SYSTEM.OSCOVFSR & 0x04) == 0) {
    /* Wait for PLL stable */
  }

  /* Configure system clocks */
  /* ICLK=240MHz, PCLKA=120MHz, PCLKB=60MHz, PCLKC=60MHz,
     * PCLKD=60MHz, BCLK=120MHz, FCLK=60MHz */
  SYSTEM.SCKCR = 0x21C21211;

  /* Select PLL as system clock */
  SYSTEM.SCKCR3 = 0x0400; /* CKSEL[2:0] = 100b (PLL) */

  /* Protect on */
  SYSTEM.PRCR = 0xA500;
}

/* =============================================================================
 * Module Stop Control
 * =============================================================================
 */

/**
 * @brief Enable peripheral modules
 *
 * Disables module stop for peripherals we'll use:
 * - CMT0 (system tick timer)
 * - SCI5 (UART debug)
 * - MTU (motor PWM)
 * - S12AD (ADC)
 */
void module_stop_init(void)
{
  /* Protect off */
  SYSTEM.PRCR = 0xA50F;

  /* Module Stop Control Register A */
  SYSTEM.MSTPCRA &= ~((1 << 15) | /* CMT0, CMT1 */
                      (1 << 9)    /* MTU */
  );

  /* Module Stop Control Register B */
  SYSTEM.MSTPCRB &= ~((1 << 26) | /* SCI5 */
                      (1 << 17) | /* RSPI0 */
                      (1 << 16)   /* RSPI1 */
  );

  /* Module Stop Control Register C */
  SYSTEM.MSTPCRC &= ~((1 << 17) /* S12AD */
  );

  /* Protect on */
  SYSTEM.PRCR = 0xA500;
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize RX72N system
 *
 * Call this early in startup, before ThreadX initialization.
 */
void system_init(void)
{
  /* Initialize clock to 240 MHz */
  clock_init();

  /* Enable peripheral modules */
  module_stop_init();
}
