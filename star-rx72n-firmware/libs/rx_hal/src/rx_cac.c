/**
 * @file rx_cac.c
 * @brief Clock Frequency Accuracy Measurement Circuit (CAC) Driver Implementation
 *
 * @details
 * Driver for the RX72N Clock Frequency Accuracy Measurement Circuit (CAC) per
 * Renesas RX72N Group User's Manual (R01UH0824EJ0111) Chapter 10 "Clock
 * Frequency Accuracy Measurement Circuit".
 *
 * The CAC compares a measurement-target clock against a reference clock, counts
 * target-clock edges between reference-clock edges, and compares the accumulated
 * count against CAULVR / CALLVR window bounds. A frequency deviation sets
 * CASTR.FERRF; when CAICR.FERRIE is enabled the failure is routed through
 * GROUPBL0 (vector 110, bit 26) to the CPU.
 *
 * @par Bring-up Sequence (RX72N HW Manual Ch10.3)
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
 * @see RX72N HW Manual R01UH0824EJ0111 Chapter 10
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
 * Internal Translation Helpers
 * =============================================================================
 */

/**
 * @brief Translate rx_cac_clock_t into a CACR1.FMCS bit pattern
 * @param[in] clock Caller-supplied clock selector
 * @return uint8_t CACR1 FMCS field value (already shifted)
 * @pre clock is a valid rx_cac_clock_t enumerator
 * @post Returned value covers only the CACR1 FMCS bits
 * @since Version 1.0.0
 */
static uint8_t internal_fmcs_bits(rx_cac_clock_t clock)
{
  switch (clock) {
    case k_cac_clock_main:
      return k_cac_cacr1_fmcs_main;
    case k_cac_clock_sub:
      return k_cac_cacr1_fmcs_sub;
    case k_cac_clock_hoco:
      return k_cac_cacr1_fmcs_hoco;
    case k_cac_clock_loco:
      return k_cac_cacr1_fmcs_loco;
    case k_cac_clock_pclkb:
      return k_cac_cacr1_fmcs_pclkb;
    case k_cac_clock_iwdtclk:
      return k_cac_cacr1_fmcs_iwdtclk;
    case k_cac_clock_uclk:
      return k_cac_cacr1_fmcs_uclk;
    case k_cac_clock_clkout25:
      return k_cac_cacr1_fmcs_clkout25;
    default:
      return k_cac_cacr1_fmcs_main;
  }
}

/**
 * @brief Translate rx_cac_clock_t into a CACR2.RSCS bit pattern
 * @param[in] clock Reference clock selector (main..iwdtclk only)
 * @return uint8_t CACR2 RSCS field value (already shifted)
 * @pre clock in [k_cac_clock_main .. k_cac_clock_iwdtclk]
 * @post Returned value covers only the CACR2 RSCS bits
 * @since Version 1.0.0
 */
static uint8_t internal_rscs_bits(rx_cac_clock_t clock)
{
  switch (clock) {
    case k_cac_clock_main:
      return k_cac_cacr2_rscs_main;
    case k_cac_clock_sub:
      return k_cac_cacr2_rscs_sub;
    case k_cac_clock_hoco:
      return k_cac_cacr2_rscs_hoco;
    case k_cac_clock_loco:
      return k_cac_cacr2_rscs_loco;
    case k_cac_clock_pclkb:
      return k_cac_cacr2_rscs_pclkb;
    case k_cac_clock_iwdtclk:
      return k_cac_cacr2_rscs_iwdtclk;
    default:
      return k_cac_cacr2_rscs_main;
  }
}

/**
 * @brief Translate rx_cac_target_div_t into a CACR1.TCSS bit pattern
 * @param[in] div Target-clock divider selector
 * @return uint8_t CACR1 TCSS field value (already shifted)
 * @since Version 1.0.0
 */
static uint8_t internal_tcss_bits(rx_cac_target_div_t div)
{
  switch (div) {
    case k_cac_target_div_1:
      return k_cac_cacr1_tcss_div_1;
    case k_cac_target_div_4:
      return k_cac_cacr1_tcss_div_4;
    case k_cac_target_div_8:
      return k_cac_cacr1_tcss_div_8;
    case k_cac_target_div_32:
      return k_cac_cacr1_tcss_div_32;
    default:
      return k_cac_cacr1_tcss_div_1;
  }
}

/**
 * @brief Translate rx_cac_ref_div_t into a CACR2.RCDS bit pattern
 * @param[in] div Reference-clock divider selector
 * @return uint8_t CACR2 RCDS field value (already shifted)
 * @since Version 1.0.0
 */
static uint8_t internal_rcds_bits(rx_cac_ref_div_t div)
{
  switch (div) {
    case k_cac_ref_div_32:
      return k_cac_cacr2_rcds_div_32;
    case k_cac_ref_div_128:
      return k_cac_cacr2_rcds_div_128;
    case k_cac_ref_div_1024:
      return k_cac_cacr2_rcds_div_1024;
    case k_cac_ref_div_8192:
      return k_cac_cacr2_rcds_div_8192;
    default:
      return k_cac_cacr2_rcds_div_32;
  }
}

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
 * @param[in] config Validated configuration struct (never nullptr)
 * @pre config != nullptr and all fields validated by caller
 * @pre CACR0.CFME == 0 (writes to CAULVR/CALLVR require measurement halted)
 * @post Registers hold the requested configuration and CASTR flags are cleared
 * @since Version 1.0.0
 */
static void internal_apply_config(const rx_cac_config_t* config)
{
  volatile rx_cac_regs_t* regs      = cac();
  uint8_t                 cacr1_val = (uint8_t)(internal_fmcs_bits(config->measured_clock) |
                                internal_tcss_bits(config->target_div) | k_cac_cacr1_edges_rising);
  uint8_t                 cacr2_val =
    (uint8_t)(internal_rscs_bits(config->reference_clock) |
              internal_rcds_bits(config->reference_div) | k_cac_cacr2_rps_internal);
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
  internal_module_start();
  volatile rx_cac_regs_t* regs = cac();
  regs->cacr0                  = k_cac_cacr0_cfme_stop;
  internal_apply_config(config);
  s_initialized = true;
  return k_rx_ok;
}

rx_err_t rx_cac_start(void)
{
  if (!s_initialized) {
    rx_log_error(s_tag, "rx_cac_start: not initialized");
    return k_rx_err_not_initialized;
  }
  volatile rx_cac_regs_t* regs = cac();
  regs->cacr0                  = k_cac_cacr0_cfme_start;
  if ((regs->cacr0 & k_cac_cacr0_cfme_mask) != k_cac_cacr0_cfme_start) {
    rx_log_error(s_tag, "rx_cac_start: CFME readback failed");
    return k_rx_err_hw_error;
  }
  return k_rx_ok;
}

rx_err_t rx_cac_stop(void)
{
  if (!s_initialized) {
    rx_log_error(s_tag, "rx_cac_stop: not initialized");
    return k_rx_err_not_initialized;
  }
  volatile rx_cac_regs_t* regs = cac();
  regs->cacr0                  = k_cac_cacr0_cfme_stop;
  if ((regs->cacr0 & k_cac_cacr0_cfme_mask) != 0U) {
    rx_log_error(s_tag, "rx_cac_stop: CFME did not clear");
    return k_rx_err_hw_error;
  }
  return k_rx_ok;
}

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
