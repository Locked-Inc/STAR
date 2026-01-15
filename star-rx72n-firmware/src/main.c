/* src/main.c */

/**
 * @file main.c
 *
 */

#include "rx_err.h"

static void internal_check_porf(void) { /* TODO */ }
static void internal_check_iwdtrf(void) { /* TODO */ }
static void internal_check_wdtrf(void) { /* TODO */ }
static void internal_check_swrf(void) { /* TODO */ }
static void internal_check_lvd0rf(void) { /* TODO */ }

static void internal_check_startup_flags(void)
{
  /* TODO: All of these methods were left returning void, should fix once we know what to do with them */
  internal_check_porf();
  internal_check_iwdtrf();
  internal_check_wdtrf();
  internal_check_swrf();
  internal_check_lvd0rf();
}

/**
 * @brief Main entry point
 *
 * Initializes hardware and enters ThreadX kernel.
 * Never returns after entering ThreadX.
 *
 * @return Should never return (ThreadX scheduler takes over)
 */
int main(void)
{
  rx_err_t ret = k_rx_ok;

  /* Check:
   *  PORF (Power-On Reset Detect Flag),
   *  IWDTRF (Independent Watchdog Timer Reset Detect Flag)
   *  WDTRF (Watchdog Timer Reset Detect Flag)
   *  SWRF (Software Reset Detect Flag)
   *  CWSF (Cold/Warm Start Determination Flag)
   *  LVD0RF (Voltage-Monitoring 0 Reset Detect Flag)
   */
  internal_check_startup_flags(); /* TODO: Left as returning void, should fix once we know what to do with this */

  // initalize_hardware();

  /* Should never reach here, ThreadX scheduler failed to start if it does */
  while (1) {
    __asm__ volatile("wait"); /* Wait for sleep/idle */
  }
  return 0;
}