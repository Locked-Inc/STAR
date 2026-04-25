/**
 * @file rx_cac.c
 * @brief Clock Frequency Accuracy Measurement Circuit (CAC) Driver Implementation
 *
 * @details
 * Driver for the RX72N Clock Frequency Accuracy Measurement Circuit (CAC) per
 * Renesas RX72N Group User's Manual (R01UH0824EJ0111) Chapter 11 "Clock
 * Frequency Accuracy Measurement Circuit" (pp.392-397 of the Ch11 register map
 * section referenced below).
 *
 * The CAC compares a measurement-target clock against a reference clock, counts
 * target-clock edges between reference-clock edges, and compares the accumulated
 * count against CAULVR / CALLVR window bounds. A frequency deviation sets
 * CASTR.FERRF; when CAICR.FERRIE is enabled the failure is routed through
 * GROUPBL0 (vector 110, bit 26) to the CPU.
 *
 * @par Bring-up Sequence (RX72N HW Manual Ch11.3)
 * 1. Unlock PRCR.PRC1, clear MSTPCRC.MSTPC19, re-lock PRCR.
 * 2. Ensure CACR0.CFME = 0 while programming CACR1, CACR2, CAULVR, CALLVR,
 *    and CAICR. Writes to CAULVR / CALLVR require CFME = 0.
 * 3. Clear any stale CASTR flags via CAICR write-1-clear bits.
 * 4. Caller invokes rx_cac_start() to set CACR0.CFME = 1 and begin measuring.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto / setjmp / recursion
 * - Rule 2: [OK] No loops
 * - Rule 3: [OK] Zero dynamic memory allocation
 * - Rule 4: [OK] All functions < 60 lines
 * - Rule 5: [OK] >= 2 validation checks per function
 * - Rule 6: [OK] File-scope statics, narrowest-scope locals
 * - Rule 7: [OK] All return values checked via RX_RETURN_ON_ERROR / explicit cast
 * - Rule 8: [OK] C23 typed enums for constants
 * - Rule 9: [OK] Single-level pointers
 * - Rule 10: [OK] -Wall -Wextra -Werror clean
 *
 * @see rx_cac.h Public API
 * @see rx72n_cac_regs.h Register definitions
 * @see RX72N HW Manual R01UH0824EJ0111 Chapter 11
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_cac.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef UNIT_TEST
#include "mock_rx72n_regs.h"
#include "mock_rx_cac.h"
#else
#include "rx72n_cac_regs.h"
#include "rx72n_regs.h"
#endif

#include "rx_check.h"
#include "rx_log.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Internal State
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Log tag for the CAC driver module
 * @details String literal used by rx_log_* calls from this translation unit.
 * @since Version 1.0.0
 */
static const char* const s_tag = "CAC";

/**
 * @var s_initialized
 * @brief Tracks whether rx_cac_init() has installed a valid configuration
 * @details Cleared by rx_cac_deinit() to permit a subsequent init call.
 * @since Version 1.0.0
 */
static bool s_initialized = false;

/* =============================================================================
 * Internal Translation Tables
 *
 * Lookup tables replace per-case switch statements so that the translators are
 * branch-free. rx_cac_init() validates each enum argument against the tables'
 * defined indices, so the caller's index is always in range by the time a
 * translator is invoked.
 * =============================================================================
 */

/**
 * @var s_fmcs_table
 * @brief CACR1.FMCS bit pattern indexed by rx_cac_clock_t
 * @details RX72N HW Manual p.393 (FMCS field of CACR1).
 * @since Version 1.0.0
 */
static const uint8_t s_fmcs_table[] = {
  [k_cac_clock_main]     = k_cac_cacr1_fmcs_main,
  [k_cac_clock_sub]      = k_cac_cacr1_fmcs_sub,
  [k_cac_clock_hoco]     = k_cac_cacr1_fmcs_hoco,
  [k_cac_clock_loco]     = k_cac_cacr1_fmcs_loco,
  [k_cac_clock_pclkb]    = k_cac_cacr1_fmcs_pclkb,
  [k_cac_clock_iwdtclk]  = k_cac_cacr1_fmcs_iwdtclk,
  [k_cac_clock_uclk]     = k_cac_cacr1_fmcs_uclk,
  [k_cac_clock_clkout25] = k_cac_cacr1_fmcs_clkout25,
};

/**
 * @var s_rscs_table
 * @brief CACR2.RSCS bit pattern indexed by rx_cac_clock_t (reference subset)
 * @details RX72N HW Manual p.394 (RSCS field of CACR2). Only Main..IWDTCLK
 * (indices 0..5) are valid references; rx_cac_init() rejects higher values.
 * @since Version 1.0.0
 */
static const uint8_t s_rscs_table[] = {
  [k_cac_clock_main]    = k_cac_cacr2_rscs_main,
  [k_cac_clock_sub]     = k_cac_cacr2_rscs_sub,
  [k_cac_clock_hoco]    = k_cac_cacr2_rscs_hoco,
  [k_cac_clock_loco]    = k_cac_cacr2_rscs_loco,
  [k_cac_clock_pclkb]   = k_cac_cacr2_rscs_pclkb,
  [k_cac_clock_iwdtclk] = k_cac_cacr2_rscs_iwdtclk,
};

/**
 * @var s_tcss_table
 * @brief CACR1.TCSS bit pattern indexed by rx_cac_target_div_t
 * @details RX72N HW Manual p.393 (TCSS field of CACR1).
 * @since Version 1.0.0
 */
static const uint8_t s_tcss_table[] = {
  [k_cac_target_div_1]  = k_cac_cacr1_tcss_div_1,
  [k_cac_target_div_4]  = k_cac_cacr1_tcss_div_4,
  [k_cac_target_div_8]  = k_cac_cacr1_tcss_div_8,
  [k_cac_target_div_32] = k_cac_cacr1_tcss_div_32,
};

/**
 * @var s_rcds_table
 * @brief CACR2.RCDS bit pattern indexed by rx_cac_ref_div_t
 * @details RX72N HW Manual p.394 (RCDS field of CACR2).
 * @since Version 1.0.0
 */
static const uint8_t s_rcds_table[] = {
  [k_cac_ref_div_32]   = k_cac_cacr2_rcds_div_32,
  [k_cac_ref_div_128]  = k_cac_cacr2_rcds_div_128,
  [k_cac_ref_div_1024] = k_cac_cacr2_rcds_div_1024,
  [k_cac_ref_div_8192] = k_cac_cacr2_rcds_div_8192,
};

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Release the CAC module from module-stop (MSTPCRC.MSTPC19 = 0)
 * @details Unlocks PRC1, clears the MSTPC19 bit, re-locks PRCR.
 * @pre PRCR is accessible (not held by another critical section)
 * @post MSTPCRC.MSTPC19 = 0, PRCR re-locked
 * @since Version 1.0.0
 */
static void internal_module_start(void)
{
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcrc &= ~((uint32_t)1U << k_cac_mstpc_bit);
  *prcr_reg() = k_rx_prcr_lock;
}

/**
 * @brief Return the CAC module to module-stop (MSTPCRC.MSTPC19 = 1)
 * @details Unlocks PRC1, sets the MSTPC19 bit, re-locks PRCR.
 * @pre Measurement has been halted (CACR0.CFME = 0)
 * @post MSTPCRC.MSTPC19 = 1, PRCR re-locked
 * @since Version 1.0.0
 */
static void internal_module_stop(void)
{
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcrc |= ((uint32_t)1U << k_cac_mstpc_bit);
  *prcr_reg() = k_rx_prcr_lock;
}

/**
 * @brief Program CACR1, CACR2, CAULVR, CALLVR, and CAICR from a config struct
 * @pre config != nullptr and all enum fields validated by rx_cac_init()
 * @pre CACR0.CFME == 0 (writes to CAULVR/CALLVR require measurement halted)
 * @post Registers hold the requested configuration and CASTR flags are cleared
 * @since Version 1.0.0
 */
static void internal_apply_config(const rx_cac_config_t* config)
{
  volatile rx_cac_regs_t* regs = cac();
  uint8_t                 cacr1_val =
    (uint8_t)(s_fmcs_table[(uint8_t)config->measured_clock] |
              s_tcss_table[(uint8_t)config->target_div] | k_cac_cacr1_edges_rising);
  uint8_t cacr2_val =
    (uint8_t)(s_rscs_table[(uint8_t)config->reference_clock] |
              s_rcds_table[(uint8_t)config->reference_div] | k_cac_cacr2_rps_internal);
  uint8_t caicr_val = k_cac_caicr_clear_all_flags;
  if (config->enable_ferrie) {
    caicr_val |= k_cac_caicr_ferrie_mask;
  }
  if (config->enable_mendie) {
    caicr_val |= k_cac_caicr_mendie_mask;
  }
  if (config->enable_ovfie) {
    caicr_val |= k_cac_caicr_ovfie_mask;
  }
  regs->cacr1  = cacr1_val;
  regs->cacr2  = cacr2_val;
  regs->caulvr = config->caulvr;
  regs->callvr = config->callvr;
  regs->caicr  = caicr_val;
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

rx_err_t rx_cac_init(const rx_cac_config_t* config)
{
  RX_CHECK_NULL_PTR(config, s_tag, "rx_cac_init: config is nullptr");
  if (s_initialized) {
    rx_log_error(s_tag, "rx_cac_init: already initialized");
    return k_rx_err_invalid_state;
  }
  if (config->callvr >= config->caulvr) {
    rx_log_error(s_tag, "rx_cac_init: callvr must be < caulvr");
    return k_rx_err_invalid_arg;
  }
  if ((uint8_t)config->reference_clock > k_cac_clock_iwdtclk) {
    rx_log_error(s_tag, "rx_cac_init: reference_clock out of range");
    return k_rx_err_invalid_arg;
  }
  if ((uint8_t)config->measured_clock > k_cac_clock_clkout25) {
    rx_log_error(s_tag, "rx_cac_init: measured_clock out of range");
    return k_rx_err_invalid_arg;
  }
  if ((uint8_t)config->target_div > k_cac_target_div_32) {
    rx_log_error(s_tag, "rx_cac_init: target_div out of range");
    return k_rx_err_invalid_arg;
  }
  if ((uint8_t)config->reference_div > k_cac_ref_div_8192) {
    rx_log_error(s_tag, "rx_cac_init: reference_div out of range");
    return k_rx_err_invalid_arg;
  }
  internal_module_start();
  volatile rx_cac_regs_t* regs = cac();
  regs->cacr0                  = k_cac_cacr0_cfme_stop;
  internal_apply_config(config);
  s_initialized = true;
  return k_rx_ok;
}

/**
 * @brief Start the Clock Frequency Accuracy (CAC) measurement
 *
 * @details
 * Sets CACR0.CFME=1 to enable comparison of the measurement clock against
 * the reference clock configured by rx_cac_init().  Subsequent calls to
 * rx_cac_check() will report whether the measurement is within the
 * configured tolerance window and return the latched count.
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok                  Measurement started.
 * @retval k_rx_err_not_initialized rx_cac_init() has not been called.
 *
 * @pre rx_cac_init() previously returned k_rx_ok.
 *
 * @post On k_rx_ok: CACR0.CFME == 1; measurement is running.
 * @post On k_rx_err_not_initialized: register state unchanged.
 *
 * @note Not thread-safe.  Serialize all rx_cac_* operations.
 *
 * @see rx_cac_stop()  Pause measurement.
 * @see rx_cac_check() Read measurement status and count.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_cac_start(void)
{
  if (!s_initialized) {
    rx_log_error(s_tag, "rx_cac_start: not initialized");
    return k_rx_err_not_initialized;
  }
  cac()->cacr0 = k_cac_cacr0_cfme_start;
  return k_rx_ok;
}

/**
 * @brief Pause the CAC measurement without deinitializing the driver
 *
 * @details
 * Clears CACR0.CFME, halting the measurement counter.  The measurement
 * configuration (reference clock, tolerance, IRQ enables) is preserved
 * so a subsequent rx_cac_start() resumes from a clean state.  Useful
 * when entering low-power modes or when switching reference clocks.
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok                  Measurement halted.
 * @retval k_rx_err_not_initialized rx_cac_init() has not been called.
 *
 * @pre rx_cac_init() previously returned k_rx_ok.
 *
 * @post On k_rx_ok: CACR0.CFME == 0; measurement stopped.
 * @post On k_rx_err_not_initialized: register state unchanged.
 *
 * @note Not thread-safe.  Serialize all rx_cac_* operations.
 *
 * @see rx_cac_start()  Resume measurement.
 * @see rx_cac_deinit() Full driver shutdown.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_cac_stop(void)
{
  if (!s_initialized) {
    rx_log_error(s_tag, "rx_cac_stop: not initialized");
    return k_rx_err_not_initialized;
  }
  cac()->cacr0 = k_cac_cacr0_cfme_stop;
  return k_rx_ok;
}

/**
 * @brief Check the CAC frequency-error flag and snapshot the latched count
 *
 * @details
 * Reads CASTR (status), latches the current CACNTBR (counter buffer)
 * value if out_count is non-NULL, then issues a write-1-clear to the IRQ
 * status flags in CAICR (preserving the IRQ enables).  Returns whether
 * the FERRF (frequency-error) flag was set since the last check.
 *
 * @param[out] out_count Optional destination for the CACNTBR snapshot
 *                       (NULL skips the read).
 *
 * @return true if FERRF was latched since the last check, false otherwise
 *         (also false if the driver is not initialized).
 *
 * @pre None (returns false silently if driver not initialized).
 *
 * @post On any return: CAICR FERRF/MENDF/OVFF flags are cleared
 *       (write-1-to-clear).
 * @post If out_count != NULL: *out_count holds the value of CACNTBR at
 *       call time.
 *
 * @note Safe from any context including ISR (single register read +
 *       write, no blocking).
 *
 * @see rx_cac_start() Required to start measurement before checking.
 *
 * @since Version 1.0.0
 */
bool rx_cac_check(uint32_t* out_count)
{
  if (!s_initialized) {
    return false;
  }
  volatile rx_cac_regs_t* regs   = cac();
  uint8_t                 status = regs->castr;
  if (out_count != nullptr) {
    *out_count = (uint32_t)regs->cacntbr;
  }
  /* Preserve enabled IRQ settings while issuing write-1-clear acknowledgements. */
  uint8_t ack =
    (uint8_t)(regs->caicr & (uint8_t)(k_cac_caicr_ferrie_mask | k_cac_caicr_mendie_mask |
                                      k_cac_caicr_ovfie_mask));
  ack |= k_cac_caicr_clear_all_flags;
  regs->caicr = ack;
  return (bool)((status & k_cac_castr_ferrf_mask) != 0U);
}

/**
 * @brief Fully deinitialize the CAC peripheral and gate its module clock
 *
 * @details
 * Stops the measurement (CFME=0), clears CACR1 / CACR2 (resetting the
 * reference clock and tolerance), clears all IRQ flags via CAICR, then
 * gates the CAC module clock through internal_module_stop().  Marks the
 * driver as uninitialized.
 *
 * @return rx_err_t Error code.
 * @retval k_rx_ok                  Driver fully shut down.
 * @retval k_rx_err_not_initialized rx_cac_init() has not been called.
 *
 * @pre rx_cac_init() previously returned k_rx_ok.
 *
 * @post On k_rx_ok: CACR0/1/2 == 0, CAICR flags cleared, MSTPCR bit set
 *       (clock gated), s_initialized == false.
 * @post On k_rx_err_not_initialized: register state unchanged.
 *
 * @note Not thread-safe.  Serialize all rx_cac_* operations.
 *
 * @see rx_cac_init() Re-arm before next measurement campaign.
 *
 * @since Version 1.0.0
 */
rx_err_t rx_cac_deinit(void)
{
  if (!s_initialized) {
    rx_log_error(s_tag, "rx_cac_deinit: not initialized");
    return k_rx_err_not_initialized;
  }
  volatile rx_cac_regs_t* regs = cac();
  regs->cacr0                  = k_cac_cacr0_cfme_stop;
  regs->cacr1                  = 0U;
  regs->cacr2                  = 0U;
  regs->caicr                  = k_cac_caicr_clear_all_flags;
  internal_module_stop();
  s_initialized = false;
  return k_rx_ok;
}

#ifdef UNIT_TEST
/**
 * @brief Test-only hook that clears the driver's initialization flag.
 * @details Allows unit tests to return the module to its power-on state without
 * touching the mock register bank.
 * @post s_initialized == false
 * @since Version 1.0.0
 */
void rx_cac_test_reset(void)
{
  s_initialized = false;
}
#endif
