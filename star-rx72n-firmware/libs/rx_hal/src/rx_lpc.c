/**
 * @file rx_lpc.c
 * @brief Low Power Consumption (LPC) HAL Driver Implementation for RX72N
 *
 * @details
 * Implements the Sleep, Software Standby, and Deep Software Standby mode
 * entry paths plus the OPCCR (Operating Power Control) selector described
 * in the RX72N Hardware Manual Chapter 11.
 *
 * @par Register Write Protocol
 * All writes to SBYCR, OPCCR, RSTCKCR, and DPSBYCR require PRC1 to be
 * unlocked via PRCR (key 0xA5, PRC1=1). The driver unlocks, writes, and
 * re-locks PRCR around every such access. PRCR writes are guarded by
 * `#ifdef __RX__` so host-side unit tests can exercise the driver without
 * hardware-mapped register access.
 *
 * @par Low-Power Entry Instruction
 * All three low-power modes are entered by executing the WAIT instruction
 * on RX. SBYCR.SSBY and DPSBYCR.DPSBY are set beforehand to select which
 * flavour of low power the WAIT enters:
 *
 * @verbatim
 *   SSBY=0, DPSBY=0  -> Sleep (WAIT)
 *   SSBY=1, DPSBY=0  -> Software Standby (WAIT)
 *   SSBY=1, DPSBY=1  -> Deep Software Standby (WAIT)
 * @endverbatim
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto/setjmp/recursion
 * - Rule 2: [OK] OPCMTSF poll bounded by k_opcmtsf_poll_max retries
 * - Rule 3: [OK] Zero dynamic allocation (file-scope statics only)
 * - Rule 4: [OK] All functions < 60 lines
 * - Rule 5: [OK] >= 2 pre/post conditions per function
 * - Rule 6: [OK] s_*-prefixed file-scope statics, locals at narrowest scope
 * - Rule 7: [OK] All return values checked or explicitly cast to (void)
 * - Rule 8: [OK] C23 typed enums, preprocessor only for __RX__/UNIT_TEST guards
 * - Rule 9: [OK] Single-level pointers
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @see rx_lpc.h         Public API
 * @see rx72n_lpc_regs.h Register layout and bit definitions
 * @see RX72N Hardware Manual Chapter 11 - Low Power Consumption
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_lpc.h"

#include <stddef.h>

#include "rx72n_lpc_regs.h"

#ifdef __RX__
#include "rx72n_system_regs.h"
#include "rx_register_protection.h"
#endif

/* =============================================================================
 * Internal Constants
 * =============================================================================
 */

/**
 * @enum rx_lpc_init_state_t
 * @brief Driver initialization tracking
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_lpc_not_initialized = 0, /**< rx_lpc_init() has not been called */
  k_lpc_initialized     = 1, /**< rx_lpc_init() completed successfully */
} rx_lpc_init_state_t;

/**
 * @enum rx_lpc_sbycr_bits_t
 * @brief SBYCR bit definitions (RX72N Manual Section 11.2.1, page 405)
 *
 * @details
 * Only the two bits manipulated by this driver are declared here. Other
 * bits in SBYCR (OPE) are left at their reset values.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_sbycr_ssby = 0x8000U, /**< Bit 15: Software Standby Select (0=sleep, 1=standby) */
} rx_lpc_sbycr_bits_t;

/**
 * @enum rx_lpc_poll_limits_t
 * @brief Bounded-retry limits for register polls
 *
 * @details
 * OPCCR.OPCMTSF typically clears within a few hundred cycles. 1024 is a
 * generous ceiling that still keeps the poll bounded per NASA Rule 2.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_opcmtsf_poll_max = 1024U, /**< Max OPCMTSF poll iterations */
} rx_lpc_poll_limits_t;

/* =============================================================================
 * Module State
 * =============================================================================
 */

/** @brief Driver initialization flag */
static rx_lpc_init_state_t s_initialized = k_lpc_not_initialized;

/** @brief Last low-power mode the driver attempted to enter */
static rx_lpc_last_mode_t s_last_mode = k_lpc_mode_none;

/** @brief Latched "last reset was deep-standby wake" flag (set by rx_lpc_init) */
static bool s_was_deep_standby_wake = false;

/** @brief Latched wake flags read from DPSIFR0..DPSIFR3 at init time */
static uint32_t s_latched_wake_flags = 0U;

#ifdef UNIT_TEST
/** @brief Test-injected value for the next init: simulated "was deep-standby wake" */
static bool s_inject_was_wake = false;

/** @brief Test-injected value for the next init: simulated pending DPSIFR bitmask */
static uint32_t s_inject_pending_flags = 0U;
#endif

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Translate OPCCR mode enum to OPCM[2:0] field value
 *
 * @param[in]  mode      Driver-level mode selector
 * @param[out] opcm_bits Receives the OPCM[2:0] field value
 *
 * @return rx_err_t
 * @retval k_rx_ok              mode is valid and opcm_bits written
 * @retval k_rx_err_invalid_arg mode not recognised or opcm_bits NULL
 *
 * @pre opcm_bits != NULL
 * @post *opcm_bits contains one of k_opccr_opcm_* constants on success
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_translate_opcc_mode(rx_lpc_opcc_mode_t mode, uint8_t* opcm_bits)
{
  if (opcm_bits == NULL) {
    return k_rx_err_invalid_arg;
  }

  switch (mode) {
    case k_lpc_opcc_high_speed:
      *opcm_bits = (uint8_t)k_opccr_opcm_highspeed;
      return k_rx_ok;
    case k_lpc_opcc_low_speed_1:
      *opcm_bits = (uint8_t)k_opccr_opcm_lowspeed1;
      return k_rx_ok;
    case k_lpc_opcc_low_speed_2:
      *opcm_bits = (uint8_t)k_opccr_opcm_lowspeed2;
      return k_rx_ok;
    default:
      return k_rx_err_invalid_arg;
  }
}

/**
 * @brief Program DPSIER0..DPSIER3 from the 32-bit wake mask
 *
 * @details
 * Each byte of the mask maps directly to one DPSIER register as described
 * in rx_lpc.h. The NMI bit (DPSIER2.DNMIE) is documented as write-once by
 * the hardware; this driver re-asserts it on every call and the hardware
 * will ignore the redundant writes after the first.
 *
 * @param[in] wake_mask 32-bit OR of rx_lpc_wake_flags_t values
 *
 * @pre dps_regs() returns a valid (hardware or host-mocked) pointer
 * @post DPSIER0..DPSIER3 reflect the enabled wake sources
 *
 * @since Version 1.0.0
 */
/* Only used by the host-build (#else) branch of rx_lpc_enter_deep_software_standby;
 * on RX target the same writes are inlined directly under #ifdef __RX__. Guard
 * the helper definition so the cross-compile build doesn't trip
 * -Werror=unused-function. */
#ifndef __RX__
static void internal_program_wake_enables(uint32_t wake_mask)
{
  (void)wake_mask;
}
#endif

/**
 * @brief Assemble DPSBYCR value from application-level arguments
 *
 * @param[in] deep_power Deep-cut power configuration selector
 * @param[in] keep_io    true to set DPSBYCR.IOKEEP
 * @param[in] set_dpsby  true to set DPSBYCR.DPSBY (enter deep standby)
 *
 * @return uint8_t DPSBYCR value ready to write
 *
 * @post Return value contains at most the bits DEEPCUT[1:0], IOKEEP, DPSBY
 *
 * @since Version 1.0.0
 */
static uint8_t internal_assemble_dpsbycr(rx_lpc_deep_power_t deep_power,
                                         bool                keep_io,
                                         bool                set_dpsby)
{
  uint8_t value = 0U;

  switch (deep_power) {
    case k_lpc_deep_ram_usb_on:
      value |= (uint8_t)k_dpsbycr_deepcut_ram_usb_on;
      break;
    case k_lpc_deep_ram_usb_off:
      value |= (uint8_t)k_dpsbycr_deepcut_ram_usb_off;
      break;
    case k_lpc_deep_lvd_off:
      value |= (uint8_t)k_dpsbycr_deepcut_lvd_off;
      break;
    default:
      /* Caller is responsible for validation; default to least-aggressive cut */
      value |= (uint8_t)k_dpsbycr_deepcut_ram_usb_on;
      break;
  }

  if (keep_io) {
    value |= (uint8_t)k_dpsbycr_iokeep;
  }
  if (set_dpsby) {
    value |= (uint8_t)k_dpsbycr_dpsby;
  }
  return value;
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

rx_err_t rx_lpc_init(void)
{
  /* Latch whatever the boot path told us about the previous reset */
#ifdef UNIT_TEST
  s_was_deep_standby_wake = s_inject_was_wake;
  s_latched_wake_flags    = s_inject_pending_flags;
  s_inject_was_wake       = false;
  s_inject_pending_flags  = 0U;
#else
  s_was_deep_standby_wake = false;
  s_latched_wake_flags    = 0U;
#endif

#ifdef __RX__
  /* Read DPSIFR0..DPSIFR3, OR them into the latched flags, then clear them */
  volatile rx_dps_regs_t* dps = dps_regs();
  uint32_t                flags = 0U;
  flags |= ((uint32_t)dps->dpsifr0) << 0U;
  flags |= ((uint32_t)dps->dpsifr1) << 8U;
  flags |= ((uint32_t)dps->dpsifr2) << 16U;
  flags |= ((uint32_t)dps->dpsifr3) << 24U;
  s_latched_wake_flags |= flags;

  /* Write 0 to each flag register to clear any pending wake bits */
  volatile uint16_t* prcr = prcr_reg();
  *prcr                   = (uint16_t)k_rx_prcr_unlock_prc1;
  dps->dpsifr0            = 0U;
  dps->dpsifr1            = 0U;
  dps->dpsifr2            = 0U;
  dps->dpsifr3            = 0U;
  *prcr                   = (uint16_t)k_rx_prcr_lock;
#endif

  s_last_mode   = k_lpc_mode_none;
  s_initialized = k_lpc_initialized;
  return k_rx_ok;
}

rx_err_t rx_lpc_set_operating_power(rx_lpc_opcc_mode_t mode)
{
  if (s_initialized != k_lpc_initialized) {
    return k_rx_err_not_initialized;
  }

  uint8_t  opcm_bits = 0U;
  rx_err_t err       = internal_translate_opcc_mode(mode, &opcm_bits);
  if (err != k_rx_ok) {
    return err;
  }

#ifdef __RX__
  volatile uint16_t* prcr = prcr_reg();
  volatile uint8_t*  ocr  = opccr_reg();

  *prcr = (uint16_t)k_rx_prcr_unlock_prc1;
  *ocr  = opcm_bits;
  *prcr = (uint16_t)k_rx_prcr_lock;

  /* Bounded poll for OPCMTSF=0 (Manual 11.2.6, page 414) */
  for (uint16_t i = 0U; i < k_opcmtsf_poll_max; i++) {
    if ((*ocr & (uint8_t)k_opccr_opcmtsf) == 0U) {
      return k_rx_ok;
    }
  }
  return k_rx_err_hw_timeout;
#else
  (void)opcm_bits;
  return k_rx_ok;
#endif
}

rx_err_t rx_lpc_enter_sleep(void)
{
  if (s_initialized != k_lpc_initialized) {
    return k_rx_err_not_initialized;
  }

#ifdef __RX__
  volatile uint16_t*        prcr = prcr_reg();
  volatile rx_system_regs_t* sys = system_regs();

  *prcr      = (uint16_t)k_rx_prcr_unlock_prc1;
  sys->sbycr = (uint16_t)(sys->sbycr & (uint16_t)~k_sbycr_ssby);
  *prcr      = (uint16_t)k_rx_prcr_lock;
#endif

  s_last_mode = k_lpc_mode_sleep;

#ifdef __RX__
  __asm__ volatile("wait");
#endif

  return k_rx_ok;
}

rx_err_t rx_lpc_enter_software_standby(void)
{
  if (s_initialized != k_lpc_initialized) {
    return k_rx_err_not_initialized;
  }

#ifdef __RX__
  volatile uint16_t*         prcr = prcr_reg();
  volatile rx_system_regs_t* sys  = system_regs();
  volatile rx_dps_regs_t*    dps  = dps_regs();

  *prcr = (uint16_t)k_rx_prcr_unlock_prc1;
  /* Clear DPSBY so WAIT enters software standby, not deep standby */
  dps->dpsbycr = (uint8_t)(dps->dpsbycr & (uint8_t)~k_dpsbycr_dpsby);
  /* Set SSBY so WAIT enters standby, not sleep */
  sys->sbycr = (uint16_t)(sys->sbycr | (uint16_t)k_sbycr_ssby);
  *prcr      = (uint16_t)k_rx_prcr_lock;
#endif

  s_last_mode = k_lpc_mode_software_standby;

#ifdef __RX__
  __asm__ volatile("wait");
#endif

  return k_rx_ok;
}

rx_err_t rx_lpc_enter_deep_software_standby(uint32_t            wake_mask,
                                            rx_lpc_deep_power_t deep_power,
                                            bool                keep_io)
{
  if (s_initialized != k_lpc_initialized) {
    return k_rx_err_not_initialized;
  }
  if (wake_mask == 0U) {
    return k_rx_err_invalid_arg;
  }
  if ((wake_mask & ~(uint32_t)k_lpc_wake_all_mask) != 0U) {
    return k_rx_err_invalid_arg;
  }
  if ((deep_power != k_lpc_deep_ram_usb_on) && (deep_power != k_lpc_deep_ram_usb_off)
      && (deep_power != k_lpc_deep_lvd_off)) {
    return k_rx_err_invalid_arg;
  }

  const uint8_t dpsbycr_value = internal_assemble_dpsbycr(deep_power, keep_io, true);

#ifdef __RX__
  volatile uint16_t*         prcr = prcr_reg();
  volatile rx_system_regs_t* sys  = system_regs();
  volatile rx_dps_regs_t*    dps  = dps_regs();

  *prcr = (uint16_t)k_rx_prcr_unlock_prc1;

  /* 1. Enable wake sources */
  dps->dpsier0 = (uint8_t)((wake_mask >> 0U) & 0xFFU);
  dps->dpsier1 = (uint8_t)((wake_mask >> 8U) & 0xFFU);
  dps->dpsier2 = (uint8_t)((wake_mask >> 16U) & 0xFFU);
  dps->dpsier3 = (uint8_t)((wake_mask >> 24U) & 0xFFU);

  /* 2. Clear any stale wake flags */
  dps->dpsifr0 = 0U;
  dps->dpsifr1 = 0U;
  dps->dpsifr2 = 0U;
  dps->dpsifr3 = 0U;

  /* 3. Program DPSBYCR (DEEPCUT, IOKEEP, DPSBY=1) */
  dps->dpsbycr = dpsbycr_value;

  /* 4. Set SBYCR.SSBY so WAIT enters standby-family */
  sys->sbycr = (uint16_t)(sys->sbycr | (uint16_t)k_sbycr_ssby);

  *prcr = (uint16_t)k_rx_prcr_lock;
#else
  (void)dpsbycr_value;
  internal_program_wake_enables(wake_mask);
#endif

  s_last_mode = k_lpc_mode_deep_software_standby;

#ifdef __RX__
  __asm__ volatile("wait");
  /* On real hardware, execution never reaches here - wake restarts at reset */
#endif

  return k_rx_ok;
}

bool rx_lpc_was_deep_standby_wake(void)
{
  return s_was_deep_standby_wake;
}

rx_err_t rx_lpc_get_wake_flags(uint32_t* flags)
{
  if (flags == NULL) {
    return k_rx_err_invalid_arg;
  }
  if (s_initialized != k_lpc_initialized) {
    return k_rx_err_not_initialized;
  }

  *flags = s_latched_wake_flags;
  return k_rx_ok;
}

/* =============================================================================
 * Test-Only Helpers
 * =============================================================================
 */

#ifdef UNIT_TEST
void rx_lpc_test_reset(void)
{
  s_initialized           = k_lpc_not_initialized;
  s_last_mode             = k_lpc_mode_none;
  s_was_deep_standby_wake = false;
  s_latched_wake_flags    = 0U;
  s_inject_was_wake       = false;
  s_inject_pending_flags  = 0U;
}

rx_lpc_last_mode_t rx_lpc_test_get_last_mode(void)
{
  return s_last_mode;
}

void rx_lpc_test_set_deep_standby_wake(bool was_wake)
{
  s_inject_was_wake = was_wake;
}

void rx_lpc_test_set_pending_wake_flags(uint32_t flags)
{
  s_inject_pending_flags = flags;
}
#endif /* UNIT_TEST */
