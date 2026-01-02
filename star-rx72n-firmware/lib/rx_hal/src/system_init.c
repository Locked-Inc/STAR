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

#include "hardware.h"
#include "rx72n_regs.h"

/* =============================================================================
 * Private Definitions
 * =============================================================================
 */

/** @brief Protection register unlock/lock values */
typedef enum {
  k_prcr_unlock = 0xA50F, /**< Unlock protection (PRC0-PRC3, PRKEY) */
  k_prcr_lock   = 0xA500, /**< Lock protection (PRKEY only) */
} protection_register_t;

/** @brief Oscillator stabilization timing constants */
typedef enum {
  k_main_osc_stabilization_cycles     = 2400000, /**< Main oscillator delay (~10ms at 240MHz) */
  k_pll_stabilization_timeout         = 1000000, /**< PLL stabilization max wait iterations */
  k_pll_stabilization_timeout_expired = 0,       /**< PLL timeout expiration value */
} oscillator_timing_t;

/** @brief Oscillator control register values */
typedef enum {
  k_sub_clock_stopped = 0x01, /**< SOSCCR: Stop sub-clock oscillator */
  k_main_osc_enabled  = 0x00, /**< MOSCCR: Enable main oscillator */
  k_pll_enabled       = 0x00, /**< PLLCR2: Enable PLL */
} oscillator_control_t;

/** @brief PLL configuration values */
typedef enum {
  k_pll_multiplier_30_div_2 = 0x1D01, /**< PLLCR: 30x multiplier, divide by 2 (240MHz from 16MHz) */
  k_pll_stable_flag         = 0x04,   /**< OSCOVFSR: PLL stabilization flag bit */
} pll_config_t;

/** @brief System clock configuration */
typedef enum {
  k_system_clock_dividers   = 0x21C21211, /**< SCKCR: ICLK=240MHz, PCLKA=120MHz, others=60MHz */
  k_system_clock_source_pll = 0x0400,     /**< SCKCR3: Select PLL as system clock source */
} system_clock_config_t;

/* =============================================================================
 * Clock Configuration
 * =============================================================================
 */

/**
 * @brief Initialize clock system to 240 MHz
 *
 * Clock tree:
 * - Main oscillator: 16 MHz (external crystal)
 * - PLL: 16 MHz x 30 / 2 = 240 MHz
 * - ICLK (CPU): 240 MHz
 * - PCLKA: 120 MHz
 * - PCLKB/C/D: 60 MHz
 * - FCLK (Flash): 60 MHz
 *
 * @return k_rx_ok on success
 */
static rx_err_t clock_init(void)
{
  /* Unlock protection for clock registers */
  SYSTEM.prcr = k_prcr_unlock;

  /* Stop sub-clock oscillator (not used) */
  SYSTEM.sosccr = k_sub_clock_stopped;

  /* Start main oscillator (16 MHz external crystal) */
  SYSTEM.mosccr = k_main_osc_enabled;

  /* Wait for main oscillator stabilization (typically 10ms) */
  /* NOTE: Busy-wait required - runs before ThreadX initialization */
  for (volatile uint32_t i = 0; i < k_main_osc_stabilization_cycles; i++) {
    __asm__ volatile("nop");
  }

  /* Configure PLL */
  /* PLL = (Main OSC x PLIDIV) / PLLSTBY */
  /* 240 MHz = (16 MHz x 30) / 2 */
  SYSTEM.pllcr  = k_pll_multiplier_30_div_2;
  SYSTEM.pllcr2 = k_pll_enabled;

  /* Wait for PLL stabilization */
  /* NOTE: Busy-wait polling required - runs before ThreadX initialization */
  uint32_t timeout = k_pll_stabilization_timeout;
  while ((SYSTEM.oscovfsr & k_pll_stable_flag) == 0 && timeout > 0) {
    timeout--;
  }

  if (timeout == k_pll_stabilization_timeout_expired) {
    /* PLL failed to stabilize - but can't log yet (UART not initialized) */
    SYSTEM.prcr = k_prcr_lock;
    return k_rx_err_hw_timeout;
  }

  /* Configure system clocks */
  /* ICLK=240MHz, PCLKA=120MHz, PCLKB=60MHz, PCLKC=60MHz,
     * PCLKD=60MHz, BCLK=120MHz, FCLK=60MHz */
  SYSTEM.sckcr = k_system_clock_dividers;

  /* Select PLL as system clock */
  SYSTEM.sckcr3 = k_system_clock_source_pll;

  /* Lock protection */
  SYSTEM.prcr = k_prcr_lock;

  return k_rx_ok;
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
 *
 * @return k_rx_ok on success
 */
static rx_err_t module_stop_init(void)
{
  /* Protect off */
  SYSTEM.prcr = 0xA50F;

  /* Module Stop Control Register A */
  SYSTEM.mstpcra &= ~((1 << 15) | /* CMT0, CMT1 */
                      (1 << 9)    /* MTU */
  );

  /* Module Stop Control Register B */
  SYSTEM.mstpcrb &= ~((1 << 26) | /* SCI5 */
                      (1 << 17) | /* RSPI0 */
                      (1 << 16)   /* RSPI1 */
  );

  /* Module Stop Control Register C */
  SYSTEM.mstpcrc &= ~((1 << 17) /* S12AD */
  );

  /* Protect on */
  SYSTEM.prcr = 0xA500;

  return k_rx_ok;
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize RX72N system
 *
 * Call this early in startup, before ThreadX initialization.
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t system_init(void)
{
  /* Initialize clock to 240 MHz */
  rx_err_t err = clock_init();
  if (err != k_rx_ok) {
    /* Can't log - UART not initialized yet */
    return err;
  }

  /* Enable peripheral modules */
  err = module_stop_init();
  if (err != k_rx_ok) {
    /* Can't log - UART not initialized yet */
    return err;
  }

  /* System init complete - but still can't log until UART is initialized */

  return k_rx_ok;
}
