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
#include "rx_check.h"
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
  k_ier_bit_enable_base = 1, /**< Base value for IER bit enable shift */
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
 * @brief ICU interrupt request flag values
 */
typedef enum {
  k_icu_ir_clear = 0, /**< Clear ICU IR flag */
} icu_ir_values_t;

/**
 * @brief CMT0 compare match interrupt handler
 *
 * This is called 100 times per second and drives the ThreadX scheduler.
 * Calls _tx_timer_interrupt() to update ThreadX timers and perform
 * time-slice preemption.
 */
void cmt0_isr(void)
{
  /* Clear ICU interrupt request flag */
  icu()->ir[k_vect_cmt0_cmi0] = k_icu_ir_clear;

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
  if (cmt_ctrl() == NULL || cmt0() == NULL) {
    return k_rx_err_invalid_state;
  }

  rx_log_info("TIMER", "Initializing CMT0 for ThreadX tick");

  /* Stop CMT0 if running */
  cmt_ctrl()->cmstr0 &= ~k_cmt0_cmstr_start_bit;

  /* Configure CMT0 */
  /* CMCR: Clock = PCLK/128, interrupt enabled */
  cmt0()->cmcr = k_cmt0_cmcr_config;

  /* Set compare match value for 100 Hz tick
     * CMCOR = (60,000,000 / 128 / 100) - 1 = 4687 */
  cmt0()->cmcor = k_cmt0_compare_match;

  /* Reset counter */
  cmt0()->cmcnt = k_cmt0_counter_init;

  /* Configure interrupt controller (ICU) */
  /* Clear any pending interrupt */
  icu()->ir[k_vect_cmt0_cmi0] = k_icu_ir_clear;

  /* Set interrupt priority (3 out of 15) */
  icu()->ipr[k_vect_cmt0_cmi0] = k_cmt0_irq_priority;

  /* Enable CMT0 interrupt in ICU */
  icu()->ier[k_vect_cmt0_cmi0 / k_icu_ier_bits_per_reg] |=
    (k_ier_bit_enable_base << (k_vect_cmt0_cmi0 % k_icu_ier_bits_per_reg));

  /* Start CMT0 */
  cmt_ctrl()->cmstr0 |= k_cmt0_cmstr_start_bit;

  if ((cmt_ctrl()->cmstr0 & k_cmt0_cmstr_start_bit) == 0) {
    return k_rx_err_hw_error;
  }

  /* Enable interrupts globally (set I flag in PSW) */
  /* Enable interrupts globally via PSW I-flag
   * Note: Direct assembly required as RX GCC has no built-in for PSW manipulation */
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
  if (cmt_ctrl() == NULL) {
    return k_rx_err_invalid_state;
  }

  if ((cmt_ctrl()->cmstr0 & k_cmt0_cmstr_start_bit) == 0) {
    return k_rx_err_invalid_state;
  }

  rx_log_info("TIMER", "Stopping CMT0");

  /* Stop CMT0 */
  cmt_ctrl()->cmstr0 &= ~k_cmt0_cmstr_start_bit;

  if ((cmt_ctrl()->cmstr0 & k_cmt0_cmstr_start_bit) != 0) {
    return k_rx_err_hw_error;
  }

  return k_rx_ok;
}

/**
 * @brief Get current CMT0 counter value
 *
 * @param[out] count Pointer to store counter value
 *
 * @return k_rx_ok on success,
 *         k_rx_err_null_ptr if count is NULL
 */
rx_err_t timer_get_count(uint16_t* count)
{
  RX_CHECK_NULL_PTR(count, "TIMER", "Count pointer is NULL");

  *count = cmt0()->cmcnt;

  if (*count > k_cmt0_compare_match) {
    return k_rx_err_hw_error;
  }

  return k_rx_ok;
}
