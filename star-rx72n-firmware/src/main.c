/* src/main.c */

/**
 * @file main.c
 *
 */

#include "rx_check.h"
#include "rx_err.h"
#include "system_init.h"

/* =============================================================================
 * Main Return Codes
 * =============================================================================
 */

/**
 * @brief Main function return codes
 *
 * Note: main() should never return in this firmware as ThreadX takes over.
 * These codes exist for completeness and static analysis tools.
 */
typedef enum {
  k_main_ret_success = 0, /**< Successful completion (should never be reached) */
} main_ret_t;

/* =============================================================================
 * Startup Flag Check Helpers
 * =============================================================================
 */

/**
 * @brief Check Power-On Reset Detect Flag (PORF)
 * @return true if PORF check passes (normal condition), false on unexpected state
 */
static bool internal_check_porf(void)
{
  /* TODO: Implement actual PORF flag check from RSTSR0 register */
  /* For now, return true to indicate normal startup */
  return true;
}

/**
 * @brief Check Independent Watchdog Timer Reset Detect Flag (IWDTRF)
 * @return true if no IWDT reset detected (normal), false if IWDT caused reset
 */
static bool internal_check_iwdtrf(void)
{
  /* TODO: Implement actual IWDTRF flag check from RSTSR2 register */
  /* For now, return true to indicate no watchdog reset */
  return true;
}

/**
 * @brief Check Watchdog Timer Reset Detect Flag (WDTRF)
 * @return true if no WDT reset detected (normal), false if WDT caused reset
 */
static bool internal_check_wdtrf(void)
{
  /* TODO: Implement actual WDTRF flag check from RSTSR2 register */
  return true;
}

/**
 * @brief Check Software Reset Detect Flag (SWRF)
 * @return true if check passes, false on unexpected state
 */
static bool internal_check_swrf(void)
{
  /* TODO: Implement actual SWRF flag check from RSTSR2 register */
  return true;
}

/**
 * @brief Check Voltage-Monitoring 0 Reset Detect Flag (LVD0RF)
 * @return true if no voltage-monitoring reset detected, false if LVD0 caused reset
 */
static bool internal_check_lvd0rf(void)
{
  /* TODO: Implement actual LVD0RF flag check from RSTSR0 register */
  return true;
}

/**
 * @brief Check all startup flags and return status
 *
 * Validates critical reset status flags to determine boot condition.
 * Asserts on critical failures to catch unexpected reset conditions early.
 *
 * @return k_rx_ok if all startup checks pass, error code otherwise
 */
static rx_err_t internal_check_startup_flags(void)
{
  bool porf_ok   = internal_check_porf();
  bool iwdtrf_ok = internal_check_iwdtrf();
  bool wdtrf_ok  = internal_check_wdtrf();
  bool swrf_ok   = internal_check_swrf();
  bool lvd0rf_ok = internal_check_lvd0rf();

  /* Assert critical startup conditions:
   * - PORF should indicate clean power-on (critical for proper initialization)
   * - IWDTRF should be clear (watchdog reset indicates prior firmware issue) */
  RX_ASSERT(porf_ok, "Power-On Reset flag check failed - unexpected startup condition");
  RX_ASSERT(iwdtrf_ok, "Independent Watchdog Timer reset detected - prior execution fault");

  /* Return error if any startup flag indicates abnormal condition */
  if (!porf_ok || !iwdtrf_ok || !wdtrf_ok || !swrf_ok || !lvd0rf_ok) {
    return k_rx_err_hw_init_failed;
  }

  return k_rx_ok;
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
  rx_err_t startup_ret;
  rx_err_t ret;

  /* Check:
   *  PORF (Power-On Reset Detect Flag),
   *  IWDTRF (Independent Watchdog Timer Reset Detect Flag)
   *  WDTRF (Watchdog Timer Reset Detect Flag)
   *  SWRF (Software Reset Detect Flag)
   *  CWSF (Cold/Warm Start Determination Flag)
   *  LVD0RF (Voltage-Monitoring 0 Reset Detect Flag)
   */
  startup_ret = internal_check_startup_flags();
  RX_ERROR_CHECK(startup_ret);

  ret = system_init(); /* TODO: Change the name `system_init` to something more explicit, `system` seems wrong */
  RX_ERROR_CHECK(ret);

  /* TODO: Add hardware_init() implementation and call it here once peripheral setup is defined */

  /* Should never reach here, ThreadX scheduler failed to start if it does */
  while (1) {
    __asm__ volatile("wait"); /* Wait for sleep/idle */
  }

  /* Unreachable: ThreadX scheduler takes over before reaching this point.
   * If execution reaches here, the system is in an undefined state. */
  __builtin_unreachable();

  return k_main_ret_success;
}