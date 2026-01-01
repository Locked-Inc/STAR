/* src/hardware/timer.c */

/**
 * @file timer.c
 * @brief System Tick Timer for ThreadX
 *
 * Uses CMT0 (Compare Match Timer 0) to generate periodic interrupts
 * for ThreadX system tick at 100 Hz.
 */

#include <stdint.h>

#include "hardware.h"
#include "rx72n_regs.h"
#include "tx_api.h"
#include "tx_user.h"

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
  CMT0.CMCR; /* Read to clear interrupt flag */

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
 * - Priority: 5
 *
 * @return k_rx_ok on success
 */
rx_err_t timer_init(void)
{
  RX_LOG_INFO("TIMER", "Initializing CMT0 for ThreadX tick");

  /* Stop CMT0 if running */
  CMT_CTRL.CMSTR0 &= ~0x01;

  /* Configure CMT0 */
  /* CMCR: Clock = PCLK/128, interrupt enabled */
  CMT0.CMCR = 0x0042; /* CKS[1:0]=10 (PCLK/128), CMIE=1 (interrupt enable) */

  /* Set compare match value for 100 Hz tick
     * CMCOR = (60,000,000 / 128 / 100) - 1 = 4687 */
  CMT0.CMCOR = TX_RX72N_CMT_CMCOR;

  /* Reset counter */
  CMT0.CMCNT = 0;

  /* Configure interrupt controller (ICU) */
  /* Clear any pending interrupt */
  ICU.IR[VECT_CMT0_CMI0] = 0;

  /* Set interrupt priority (5 out of 15) */
  ICU.IPR[VECT_CMT0_CMI0] = TX_RX72N_CMT_PRIORITY;

  /* Enable CMT0 interrupt in ICU */
  ICU.IER[VECT_CMT0_CMI0 / 8] |= (1 << (VECT_CMT0_CMI0 % 8));

  /* Start CMT0 */
  CMT_CTRL.CMSTR0 |= 0x01;

  /* Enable interrupts globally (set I flag in PSW) */
  __asm__ volatile("setpsw i");

  RX_LOG_INFO("TIMER", "CMT0 initialized successfully");

  return k_rx_ok;
}

/**
 * @brief Stop CMT0 timer
 *
 * @return k_rx_ok on success
 */
rx_err_t timer_stop(void)
{
  RX_LOG_INFO("TIMER", "Stopping CMT0");

  /* Stop CMT0 */
  CMT_CTRL.CMSTR0 &= ~0x01;

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

  *count = CMT0.CMCNT;

  return k_rx_ok;
}
