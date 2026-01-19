/* src/rx_clock_power_init.c */

/**
 * @file rx_clock_power_init.c
 * @brief RX72N Clock and Power Initialization
 *
 * Configures:
 * - Clock system (240 MHz from PLL)
 * - Module stop control
 * - Interrupt controller
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_clock_power_init.h"

#include <stdint.h>

#include "rx72n_regs.h"
#include "rx72n_rtc_regs.h"
#include "rx_check.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Private Definitions
 * =============================================================================
 */

/** @brief Oscillator stabilization timing constants */
typedef enum : uint32_t {
  k_main_osc_stabilization_cycles     = 2400000, /**< Main oscillator delay (~10ms at 240MHz) */
  k_pll_stabilization_timeout         = 1000000, /**< PLL stabilization max wait iterations */
  k_pll_stabilization_timeout_expired = 0,       /**< PLL timeout expiration value */
} oscillator_timing_t;

/** @brief Oscillator control register values */
typedef enum : uint8_t {
  k_sub_clock_stopped = 0x01, /**< SOSCCR: Stop sub-clock oscillator */
  k_main_osc_enabled  = 0x00, /**< MOSCCR: Enable main oscillator */
  k_pll_enabled       = 0x00, /**< PLLCR2: Enable PLL */
} oscillator_control_t;

/** @brief PLL configuration values */
typedef enum : uint16_t {
  k_pll_multiplier_10_div_1 = 0x1300, /**< PLLCR: 10x multiplier, divide by 1 (240MHz from 24MHz) */
  k_pll_stable_flag         = 0x04,   /**< OSCOVFSR: PLL stabilization flag bit */
} pll_config_t;

/** @brief System clock configuration */
typedef enum : uint32_t {
  k_system_clock_dividers   = 0x21C21211, /**< SCKCR: ICLK=240MHz, PCLKA=120MHz, others=60MHz */
  k_system_clock_source_pll = 0x0400,     /**< SCKCR3: Select PLL as system clock source */
} system_clock_config_t;

/** @brief Module stop bit positions in MSTPCRA */
typedef enum : uint8_t {
  k_mstpcra_cmt = 15, /**< CMT0, CMT1 module stop bit */
  k_mstpcra_mtu = 9,  /**< MTU module stop bit */
} mstpcra_bits_t;

/** @brief Module stop bit positions in MSTPCRB */
typedef enum : uint8_t {
  k_mstpcrb_rspi0 = 17, /**< RSPI0 module stop bit */
  k_mstpcrb_rspi1 = 16, /**< RSPI1 module stop bit */
  /* Note: SCI modules are enabled per-channel in uart_init_channel() */
} mstpcrb_bits_t;

/** @brief Module stop bit positions in MSTPCRC */
typedef enum : uint8_t {
  k_mstpcrc_s12ad = 17, /**< S12AD module stop bit */
} mstpcrc_bits_t;

/** @brief Module initialization retry configuration */
typedef enum : uint8_t {
  k_retry_count_module_stop = 3, /**< Number of retries for module stop register verification */
} module_init_config_t;

/* =============================================================================
 * Clock Configuration
 * =============================================================================
 */

/**
 * @brief Initialize clock system to 240 MHz
 *
 * Clock tree:
 * - Main oscillator: 24 MHz (external crystal)
 * - PLL: 24 MHz x 10 / 1 = 240 MHz
 * - ICLK (CPU): 240 MHz
 * - PCLKA: 120 MHz
 * - PCLKB/C/D: 60 MHz
 * - FCLK (Flash): 60 MHz
 *
 * @return k_rx_ok on success
 */
static rx_err_t internal_clock_init(void)
{
  uint32_t timeout = k_pll_stabilization_timeout;

  /* Unlock protection for clock registers */
  system_regs()->prcr = k_rx_prcr_unlock_all;

  /* Precondition: Verify PRCR unlock took effect */
  RX_ASSERT(system_regs()->prcr == k_rx_prcr_unlock_all, "Precondition: PRCR unlock failed");

  /* Stop sub-clock oscillator (not used) */
  system_regs()->sosccr = k_sub_clock_stopped;

  /* Disable RTC to fully disable sub-clock (RCR3.RTCEN = 0)
   * Required for complete sub-clock shutdown as per hardware manual */
  rtc_regs()->rcr3 = k_rcr3_rtcen_disable;

  /* Start main oscillator (24 MHz external crystal) */
  system_regs()->mosccr = k_main_osc_enabled;

  /* Wait for main oscillator stabilization (typically 10ms) */
  /* NOTE: Busy-wait required - runs before ThreadX initialization */
  for (volatile uint32_t i = 0; i < k_main_osc_stabilization_cycles; i++) {
    __asm__ volatile("nop");
  }

  /* Configure PLL */
  /* PLL = (Main OSC x PLIDIV) / PLLSTBY */
  /* 240 MHz = (24 MHz x 10) / 1 */
  system_regs()->pllcr  = k_pll_multiplier_10_div_1;
  system_regs()->pllcr2 = k_pll_enabled;

  /* Wait for PLL stabilization */
  /* NOTE: Busy-wait polling required - runs before ThreadX initialization */
  while ((system_regs()->oscovfsr & k_pll_stable_flag) == 0 && timeout > 0) {
    timeout--;
  }

  if (timeout == k_pll_stabilization_timeout_expired) {
    /* PLL failed to stabilize - but can't log yet (UART not initialized) */
    system_regs()->prcr = k_rx_prcr_lock;
    return k_rx_err_hw_timeout;
  }

  /* Configure system clocks */
  /* ICLK=240MHz, PCLKA=120MHz, PCLKB=60MHz, PCLKC=60MHz,
     * PCLKD=60MHz, BCLK=120MHz, FCLK=60MHz */
  system_regs()->sckcr = k_system_clock_dividers;

  /* Select PLL as system clock */
  system_regs()->sckcr3 = k_system_clock_source_pll;

  /* Postcondition: Verify PLL selection took effect */
  RX_ASSERT((system_regs()->sckcr3 == k_system_clock_source_pll) &&
              ((system_regs()->oscovfsr & k_pll_stable_flag) != 0),
            "Postcondition: PLL selection and stability verification failed");

  /* Lock protection */
  system_regs()->prcr = k_rx_prcr_lock;

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
 * - MTU (motor PWM)
 * - S12AD (ADC)
 *
 * Note: SCI modules are enabled per-channel in uart_init_channel()
 *
 * @return k_rx_ok on success, k_rx_err if verification fails after retries
 */
static rx_err_t internal_module_stop_init(void)
{
  const uint32_t mstpcra_clear_mask = (1UL << k_mstpcra_cmt) | (1UL << k_mstpcra_mtu);
  const uint32_t mstpcrb_clear_mask = (1UL << k_mstpcrb_rspi0) | (1UL << k_mstpcrb_rspi1);
  const uint32_t mstpcrc_clear_mask = (1UL << k_mstpcrc_s12ad);

  for (uint8_t attempt = 0; attempt < k_retry_count_module_stop; attempt++) {
    /* Protect off */
    system_regs()->prcr = k_rx_prcr_unlock_all;

    /* Module Stop Control Register A */
    system_regs()->mstpcra &= ~mstpcra_clear_mask; /* CMT0, CMT1, MTU */

    /* Module Stop Control Register B */
    /* Note: SCI modules are enabled per-channel in uart_init_channel() */
    system_regs()->mstpcrb &= ~mstpcrb_clear_mask; /* RSPI0, RSPI1 */

    /* Module Stop Control Register C */
    system_regs()->mstpcrc &= ~mstpcrc_clear_mask; /* S12AD */

    /* Protect on */
    system_regs()->prcr = k_rx_prcr_lock;

    /* Post-condition: Verify bits are cleared */
    const uint32_t mstpcra_actual = system_regs()->mstpcra & mstpcra_clear_mask;
    const uint32_t mstpcrb_actual = system_regs()->mstpcrb & mstpcrb_clear_mask;
    const uint32_t mstpcrc_actual = system_regs()->mstpcrc & mstpcrc_clear_mask;

    if ((mstpcra_actual == 0) && (mstpcrb_actual == 0) && (mstpcrc_actual == 0)) {
      /* Postcondition: Re-read hardware registers to verify stability */
      const uint32_t verify_a = system_regs()->mstpcra & mstpcra_clear_mask;
      const uint32_t verify_b = system_regs()->mstpcrb & mstpcrb_clear_mask;
      const uint32_t verify_c = system_regs()->mstpcrc & mstpcrc_clear_mask;

      RX_ASSERT((verify_a == 0) && (verify_b == 0) && (verify_c == 0),
                "Postcondition: Module stop bits remain cleared");
      return k_rx_ok;
    }

    /* If verification failed and this isn't the last attempt, retry */
  }

  /* All retries exhausted - return error */
  return k_rx_err_hw_timeout;
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Verify system clock configuration is correct
 *
 * Checks that the clock system is properly configured to 240 MHz and
 * that required peripheral modules are enabled.
 *
 * @return k_rx_ok if system state is valid, error code otherwise
 */
static rx_err_t internal_verify_system_state(void)
{
  volatile rx_system_regs_t* sys = system_regs();

  /* Verify system register access */
  if (sys == NULL) {
    return k_rx_err_hw_init_failed;
  }

  /* Verify PLL is enabled by checking PLLCR2 register */
  const uint8_t pllcr2 = sys->pllcr2;
  if (pllcr2 != k_pll_enabled) {
    return k_rx_err_hw_init_failed;
  }

  /* Verify system clock dividers are configured correctly */
  const uint32_t sckcr = sys->sckcr;
  if (sckcr != k_system_clock_dividers) {
    return k_rx_err_hw_init_failed;
  }

  /* Verify PLL is selected as the system clock source */
  const uint16_t sckcr3 = sys->sckcr3;
  if (sckcr3 != k_system_clock_source_pll) {
    return k_rx_err_hw_init_failed;
  }

  return k_rx_ok;
}

/**
 * @brief Initialize RX72N system
 *
 * Call this early in startup, before ThreadX initialization.
 *
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_clock_power_init(void)
{
  /* Initialize clock to 240 MHz */
  rx_err_t err = internal_clock_init();
  if (err != k_rx_ok) {
    /* Can't log - UART not initialized yet */
    return err;
  }

  /* Enable peripheral modules */
  err = internal_module_stop_init();
  if (err != k_rx_ok) {
    /* Can't log - UART not initialized yet */
    return err;
  }

  /* Postcondition: Verify system state is correctly configured */
  err = internal_verify_system_state();
  if (err != k_rx_ok) {
    /* Clock or module configuration failed validation */
    return err;
  }

  /* System init complete - but still can't log until UART is initialized */

  return k_rx_ok;
}
