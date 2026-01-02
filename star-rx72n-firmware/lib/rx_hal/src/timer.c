/* lib/rx_hal/src/timer.c */

/**
 * @file timer.c
 * @brief System Tick Timer for ThreadX
 *
 * Uses CMT0 (Compare Match Timer 0) to generate periodic interrupts
 * for ThreadX system tick at 100 Hz.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdint.h>

#include "hardware.h"
#include "rx72n_regs.h"
#include "tx_api.h"

/* =============================================================================
 * CMT0 Configuration Constants
 * =============================================================================
 */

/**
 * @brief CMT0 timer configuration for 100 Hz system tick
 *
 * Clock Configuration:
 * - PCLKB = 60 MHz (peripheral clock)
 * - Divider = 128
 * - Target frequency = 100 Hz (10ms tick)
 * - CMCOR = (PCLKB / divider / frequency) - 1
 *         = (60,000,000 / 128 / 100) - 1
 *         = 4687
 */
typedef enum {
  k_cmt0_compare_match = 4687, /**< Compare match value for 100 Hz */
  k_cmt0_irq_priority  = 3,    /**< Interrupt priority (0-15, higher = more urgent) */
} cmt0_config_t;

/** @brief CMT0 register configuration values */
typedef enum {
  k_cmt0_cmcr_config = 0x0042, /**< CMCR: CKS[1:0]=10 (PCLK/128), CMIE=1 (interrupt enable) */
} cmt0_cmcr_values_t;

/** @brief CMSTR0 register bit positions */
typedef enum {
  k_cmt0_cmstr_start_bit = 0x01, /**< CMT0 start bit in CMSTR0 */
} cmt0_cmstr_bits_t;

/** @brief ICU IER register calculation constants */
typedef enum {
  k_cmt0_ier_bits_per_reg = 8, /**< Bits per IER register */
} cmt0_ier_constants_t;

/** @brief Timer counter initial value */
typedef enum {
  k_cmt0_counter_init = 0, /**< Counter initial/clear value */
} cmt0_counter_values_t;

/* ThreadX timer interrupt handler (defined in ThreadX port) */
extern void _tx_timer_interrupt(void);

/* =============================================================================
 * CMT0 Interrupt Handler
 * =============================================================================
 */

/**
 * @brief CMT0 compare match interrupt handler
 *
 * This is called 100 times per second and drives the ThreadX scheduler.
 * Calls _tx_timer_interrupt() to update ThreadX timers and perform
 * time-slice preemption.
 */
void cmt0_isr(void)
{
  /* Clear interrupt flag (write 0 to BSR bit) */
  CMT0.cmcr; /* Read to clear interrupt flag */

  /* Call ThreadX timer interrupt handler */
  _tx_timer_interrupt();
}

/* =============================================================================
 * CMT0 Initialization
 * =============================================================================
 */

/**
 * @brief Initialize CMT0 for ThreadX system tick
 *
 * Configures CMT0 to generate interrupts at TX_TIMER_TICKS_PER_SECOND (100 Hz).
 *
 * Configuration:
 * - Clock: PCLKB (60 MHz)
 * - Divider: /128 (468.75 kHz)
 * - Compare: 4687 (100 Hz)
 * - Priority: 3
 *
 * @return k_rx_ok on success
 */
rx_err_t timer_init(void)
{
  rx_log_info("TIMER", "Initializing CMT0 for ThreadX tick");

  /* Stop CMT0 if running */
  CMT_CTRL.cmstr0 &= ~k_cmt0_cmstr_start_bit;

  /* Configure CMT0 */
  /* CMCR: Clock = PCLK/128, interrupt enabled */
  CMT0.cmcr = k_cmt0_cmcr_config;

  /* Set compare match value for 100 Hz tick
     * CMCOR = (60,000,000 / 128 / 100) - 1 = 4687 */
  CMT0.cmcor = k_cmt0_compare_match;

  /* Reset counter */
  CMT0.cmcnt = k_cmt0_counter_init;

  /* Configure interrupt controller (ICU) */
  /* Clear any pending interrupt */
  ICU.ir[k_vect_cmt0_cmi0] = k_cmt0_counter_init;

  /* Set interrupt priority (3 out of 15) */
  ICU.ipr[k_vect_cmt0_cmi0] = k_cmt0_irq_priority;

  /* Enable CMT0 interrupt in ICU */
  ICU.ier[k_vect_cmt0_cmi0 / k_cmt0_ier_bits_per_reg] |=
    (1 << (k_vect_cmt0_cmi0 % k_cmt0_ier_bits_per_reg));

  /* Start CMT0 */
  CMT_CTRL.cmstr0 |= k_cmt0_cmstr_start_bit;

  /* Enable interrupts globally (set I flag in PSW) */
  __asm__ volatile("setpsw i");

  rx_log_info("TIMER", "CMT0 initialized successfully");

  return k_rx_ok;
}

/**
 * @brief Stop CMT0 timer
 *
 * @return k_rx_ok on success
 */
rx_err_t timer_stop(void)
{
  rx_log_info("TIMER", "Stopping CMT0");

  /* Stop CMT0 */
  CMT_CTRL.cmstr0 &= ~k_cmt0_cmstr_start_bit;

  return k_rx_ok;
}

/**
 * @brief Get current CMT0 counter value
 *
 * @param[out] count Pointer to store counter value
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_pointer if count is NULL
 */
rx_err_t timer_get_count(uint16_t* count)
{
  RX_CHECK_NULL_PTR(count, "TIMER", "Count pointer is NULL");

  *count = CMT0.cmcnt;

  return k_rx_ok;
}
