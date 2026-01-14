/* src/main.c */

/**
 * @file main.c
 *
 */

#include "rx_err.h"

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

  /* Should never reach here, ThreadX scheduler failed to start if it does */
  while (1) {
    __asm__ volatile("wait"); /* Wait for sleep/idle */
  }
  return 0;
}