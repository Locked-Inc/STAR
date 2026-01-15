/* src/main.c */

/**
 * @file main.c
 *
 */

#include "app_main_task.h"
#include "hardware_init.h"
#include "rx72n_system_regs.h"
#include "rx_check.h"
#include "rx_clock_power_init.h"
#include "rx_err.h"
#include "tx_api.h"

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
 *
 * Reads RSTSR0 register to check if a power-on reset occurred.
 * PORF=1 indicates power-on reset was detected (normal on fresh boot).
 *
 * @return true if PORF check passes (power-on reset detected), false otherwise
 */
static bool internal_check_porf(void)
{
  volatile rx_rstsr01_regs_t* regs = rstsr01();

  /* Precondition: Register pointer must be valid (compile-time constant address) */
  RX_ASSERT(regs != NULL, "RSTSR01 register pointer is NULL");

  uint8_t rstsr0_val = regs->rstsr0;

  /* PORF=1 indicates power-on reset occurred, which is expected on normal startup */
  bool porf_set = (rstsr0_val & k_rstsr0_porf) != 0;

  /* PORF may not be set on warm boots - this is valid, so log but don't halt */
  if (!porf_set) {
    /* Warm boot or software reset - both acceptable */
    rx_log_info("MAIN", "Power-on reset flag not set - warm boot detected");
  }

  return porf_set;
}

/**
 * @brief Helper function to check RSTSR2 flag state
 *
 * Reads RSTSR2 register and checks if a specific flag is clear.
 * Used by internal_check_iwdtrf, internal_check_wdtrf, and internal_check_swrf.
 *
 * @param[in] flag_mask The bit mask for the flag to check
 * @return true if flag is clear (0), false if flag is set (1)
 */
static bool internal_check_rstsr2_flag_clear(uint8_t flag_mask)
{
  /* Precondition: flag_mask must be non-zero */
  RX_ASSERT(flag_mask != 0, "Precondition: flag_mask must not be zero");

  volatile uint8_t* reg = rstsr2();

  /* Precondition: Register pointer must be valid (compile-time constant address) */
  RX_ASSERT(reg != NULL, "RSTSR2 register pointer is NULL");

  uint8_t rstsr2_val = *reg;

  /* Compute result reflecting actual register state */
  bool result = (rstsr2_val & flag_mask) == 0;

  return result;
}

/**
 * @brief Check Independent Watchdog Timer Reset Detect Flag (IWDTRF)
 *
 * Reads RSTSR2 register to check if an IWDT reset occurred.
 * IWDTRF=1 indicates the previous execution was terminated by watchdog timeout.
 *
 * @return true if no IWDT reset detected (IWDTRF=0, normal), false if IWDT caused reset
 */
static bool internal_check_iwdtrf(void)
{
  return internal_check_rstsr2_flag_clear(k_rstsr2_iwdtrf);
}

/**
 * @brief Check Watchdog Timer Reset Detect Flag (WDTRF)
 *
 * Reads RSTSR2 register to check if a WDT reset occurred.
 * WDTRF=1 indicates the previous execution was terminated by WDT timeout.
 *
 * @return true if no WDT reset detected (WDTRF=0, normal), false if WDT caused reset
 */
static bool internal_check_wdtrf(void)
{
  return internal_check_rstsr2_flag_clear(k_rstsr2_wdtrf);
}

/**
 * @brief Check Software Reset Detect Flag (SWRF)
 *
 * Reads RSTSR2 register to check if a software reset occurred.
 * SWRF=1 indicates a software reset was executed.
 *
 * @return true if no software reset detected (SWRF=0), false if software reset occurred
 * @note Software reset is not necessarily an error - depends on application context
 */
static bool internal_check_swrf(void)
{
  return internal_check_rstsr2_flag_clear(k_rstsr2_swrf);
}

/**
 * @brief Check Voltage-Monitoring 0 Reset Detect Flag (LVD0RF)
 *
 * Reads RSTSR0 register to check if a voltage monitoring reset occurred.
 * LVD0RF=1 indicates voltage dropped below threshold causing reset.
 *
 * @return true if no voltage-monitoring reset detected (LVD0RF=0), false if LVD0 caused reset
 */
static bool internal_check_lvd0rf(void)
{
  volatile rx_rstsr01_regs_t* regs = rstsr01();

  /* Precondition: Register pointer must be valid */
  RX_ASSERT(regs != NULL, "RSTSR01 register pointer is NULL");

  uint8_t rstsr0_val  = regs->rstsr0;
  uint8_t rstsr0_val2 = regs->rstsr0;

  RX_ASSERT(rstsr0_val == rstsr0_val2, "Inconsistent RSTSR01 read");

  /* LVD0RF=0 means no voltage-monitoring reset (normal condition) */
  bool lvd0rf_clear = (rstsr0_val & k_rstsr0_lvd0rf) == 0;

  return lvd0rf_clear;
}

/**
 * @brief Check Cold/Warm Start Determination Flag (CWSF)
 *
 * Reads RSTSR1 register to determine startup type.
 * CWSF=0 indicates cold start (power-on or voltage drop).
 * CWSF=1 indicates warm start (software reset, watchdog, etc.).
 *
 * @return true always - both cold and warm starts are acceptable
 * @note This function reads RSTSR1 but both cold and warm starts are acceptable
 */
static bool internal_check_cwsf(void)
{
  volatile rx_rstsr01_regs_t* regs = rstsr01();

  /* Precondition: Register pointer must be valid */
  RX_ASSERT(regs != NULL, "RSTSR01 register pointer is NULL");

  uint8_t rstsr1_val = regs->rstsr1;

  /* Extract CWSF bit value for validation */
  uint8_t cwsf_raw = (rstsr1_val & k_rstsr1_cwsf);

  /* Postcondition: Verify CWSF bit value is either 0 or k_rstsr1_cwsf */
  RX_ASSERT((cwsf_raw == 0) || (cwsf_raw == k_rstsr1_cwsf),
            "Postcondition: CWSF bit value invalid");

  /* Return actual CWSF state: true for warm start (CWSF=1), false for cold start (CWSF=0) */
  return (cwsf_raw == k_rstsr1_cwsf);
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
  (void)internal_check_porf();
  bool iwdtrf_ok = internal_check_iwdtrf();
  bool wdtrf_ok  = internal_check_wdtrf();
  bool swrf_ok   = internal_check_swrf();
  bool lvd0rf_ok = internal_check_lvd0rf();

  (void)internal_check_cwsf();

  /* Assert critical startup conditions (fail-fast, system halts on failure):
   * - IWDTRF should be clear (watchdog reset indicates prior firmware issue) */
  RX_ASSERT(iwdtrf_ok, "Independent Watchdog Timer reset detected - prior execution fault");

  /* Return error if any non-critical startup flag indicates abnormal condition.
   * Note: porf_ok and iwdtrf_ok are not checked here as RX_ASSERT above
   * halts execution if they fail (fail-fast behavior for critical flags). */
  if (!wdtrf_ok || !swrf_ok || !lvd0rf_ok) {
    return k_rx_err_hw_init_failed;
  }

  return k_rx_ok;
}

/**
 * @brief ThreadX Application Definition Callback
 *
 * Called by ThreadX kernel at startup to allow the application to create
 * threads and other kernel objects.
 *
 * @param[in] first_unused_memory Pointer to first unused memory for kernel objects
 */
void tx_application_define(void* first_unused_memory)
{
  (void)first_unused_memory; /* Unused parameter */

  /* Create application threads */
  RX_ERROR_CHECK(app_main_task_create());
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
  rx_err_t ret;

  /* Check startup flags (via internal_check_startup_flags):
   *  PORF (Power-On Reset Detect Flag) - asserted, halts on failure
   *  IWDTRF (Independent Watchdog Timer Reset Detect Flag) - asserted, halts on failure
   *  WDTRF (Watchdog Timer Reset Detect Flag)
   *  SWRF (Software Reset Detect Flag)
   *  LVD0RF (Voltage-Monitoring 0 Reset Detect Flag)
   *  CWSF (Cold/Warm Start Determination Flag)
   */
  ret = internal_check_startup_flags();
  RX_ERROR_CHECK(ret);

  /* Initialize system clocks and power management */
  ret = rx_clock_power_init();
  RX_ERROR_CHECK(ret);

  /* Initialize application-specific hardware (motors, sensors, etc.) */
  ret = hardware_init();
  RX_ERROR_CHECK(ret);

  /* Start the ThreadX scheduler - should never return */
  tx_kernel_enter();

  /* Should never reach here, ThreadX scheduler failed to start if it does */
  while (1) {
    __asm__ volatile("wait"); /* Wait for sleep/idle */
  }

  /* Unreachable: ThreadX scheduler takes over before reaching this point.
   * If execution reaches here, the system is in an undefined state. */
  __builtin_unreachable();

  return k_main_ret_success;
}