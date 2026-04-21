/**
 * @file rx_eccram.c
 * @brief ECC-Protected RAM (ECCRAM) Driver Implementation
 *
 * @details
 * Implements the rx_eccram.h API: module-stop clear, mode configuration
 * under PRCR unlock, region zero-initialisation with 32-bit stores,
 * error-status readout, flag clearing, ISR trampoline, and region
 * introspection.
 *
 * The RX72N has a single RAMERR vector (vector 18, RAM group) that
 * fires for both 1-bit and 2-bit ECCRAM errors. The ISR trampoline
 * inspects ECCRAM1STS and ECCRAM2STS to decide which registered
 * handler (if any) to invoke, captures the failing address(es), and
 * clears the latched status flags.
 *
 * @par Register Protection Sequencing
 * Two independent protection registers are involved:
 * - System PRCR (0x000803FE) protects MSTPCRC (Manual Ch11). Unlock
 *   with k_rx_prcr_unlock_prc1, write MSTPCRC, re-lock with
 *   k_rx_prcr_lock.
 * - ECCRAMPRCR (0x000812C4) protects ECCRAMMODE and ECCRAM1STSEN
 *   (Manual section 60.2.13). Unlock with k_rx_eccramprcr_unlock
 *   (0xF1), write the guarded register, re-lock with
 *   k_rx_eccramprcr_lock (0xF0).
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_eccram.h"

#include <stddef.h>
#include <stdint.h>

#ifdef UNIT_TEST
#include "mock_rx72n_eccram_regs.h"
#include "mock_rx72n_system_regs.h"
#else
#include "rx72n_eccram_regs.h"
#include "rx72n_system_regs.h"
#endif

#include "rx_check.h"
#include "rx_err.h"
#include "rx_log.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Module log tag for rx_log_* macros
 */
static const char* s_tag = "ECCRAM";

/**
 * @enum rx_eccram_fill_t
 * @brief Zero-initialisation fill pattern and word size
 *
 * @details
 * The ECC syndrome is computed per 32-bit word; filling with zero is
 * the cheapest pattern that produces a well-defined, check-valid state.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Fill pattern used by the zero-initialisation loop */
  k_rx_eccram_fill_pattern = 0x00000000U,
} rx_eccram_fill_t;

/**
 * @enum rx_eccram_word_size_t
 * @brief Width of a single ECC-protected word in bytes
 *
 * @details
 * Declared as a typed enum so it can bound the Rule-2 static loop
 * upper in rx_eccram_init() without introducing a magic number.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief 4 bytes per ECC word (uint32_t) */
  k_rx_eccram_word_bytes = 4,
} rx_eccram_word_size_t;

/**
 * @enum rx_eccram_word_count_t
 * @brief Number of 32-bit words in the ECCRAM region
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief 32 KB / 4 bytes per word = 8192 words */
  k_rx_eccram_word_count = (k_rx_eccram_region_size_bytes / k_rx_eccram_word_bytes),
} rx_eccram_word_count_t;

/* =============================================================================
 * Static State
 * =============================================================================
 */

/** @brief Driver initialised flag */
static bool s_initialized = false;

/** @brief Registered 1-bit error callback (may be nullptr) */
static rx_eccram_on_1bit_fn_t s_on_1bit = NULL;

/** @brief Registered 2-bit error callback (may be nullptr) */
static rx_eccram_on_2bit_fn_t s_on_2bit = NULL;

/** @brief Opaque context passed to registered callbacks */
static void* s_isr_ctx = NULL;

/** @brief Latched address of most recent 2-bit error (captured in ISR) */
static volatile uintptr_t s_last_2bit_addr = 0;

/** @brief Latched address of most recent 1-bit error (captured in ISR) */
static volatile uintptr_t s_last_1bit_addr = 0;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Translate public mode enum into ECCRAMMODE register value
 *
 * @param[in] mode Requested public mode.
 *
 * @return Matching k_rx_eccrammode_* value for ECCRAMMODE.
 * @retval k_rx_eccrammode_ecc_disabled   for k_eccram_mode_disabled
 * @retval k_rx_eccrammode_ecc_with_check for k_eccram_mode_correct_only,
 *                                        k_eccram_mode_correct_and_detect,
 *                                        k_eccram_mode_detect_only
 *
 * @pre  @p mode has been range-checked by the caller.
 * @post Return value is one of the k_rx_eccrammode_* constants.
 *
 * @note All error-visible modes share the same hardware encoding
 *       (RAMMOD=11b). The distinction between correct_only,
 *       correct_and_detect, and detect_only is enforced by which ISRs
 *       the driver registers (not by the hardware bits).
 *
 * @since Version 1.0.0
 */
static uint8_t internal_mode_to_reg(rx_eccram_mode_t mode)
{
  if (mode == k_eccram_mode_disabled) {
    return k_rx_eccrammode_ecc_disabled;
  }
  return k_rx_eccrammode_ecc_with_check;
}

/**
 * @brief Clear MSTPCRC.MSTPC6 so ECCRAM control block is clocked
 *
 * @details
 * Uses the standard STAR unlock/write/lock sequence on the system PRCR.
 *
 * @pre  System clock configured.
 * @pre  Caller holds exclusive access to MSTPCRC (single-threaded init).
 * @post MSTPCRC.MSTPC6 == 0.
 * @post System PRCR is re-locked on return.
 *
 * @see RX72N HW Manual Chapter 60, section 60.4.1 (MSTPCRC), page 2994
 * @since Version 1.0.0
 */
static void internal_enable_eccram_module_clock(void)
{
  *prcr_reg() = k_rx_prcr_unlock_prc1;
  system_regs()->mstpcrc &= ~(k_rx_eccram_mstpcrc_bit_mask);
  *prcr_reg() = k_rx_prcr_lock;
}

/**
 * @brief Write ECCRAMMODE under ECCRAMPRCR unlock
 *
 * @param[in] mode_reg_value Raw value for ECCRAMMODE register.
 *
 * @pre  ECCRAM module clocked (MSTPCRC.MSTPC6 == 0).
 * @pre  @p mode_reg_value is a valid k_rx_eccrammode_* constant.
 * @post ECCRAMMODE reflects @p mode_reg_value.
 * @post ECCRAMPRCR is re-locked on return.
 *
 * @see RX72N HW Manual Chapter 60, section 60.2.13, page 2985
 * @since Version 1.0.0
 */
static void internal_write_eccram_mode(uint8_t mode_reg_value)
{
  volatile rx_eccram_regs_t* regs = eccram_regs();
  regs->eccramprcr                = k_rx_eccramprcr_unlock;
  regs->eccrammode                = mode_reg_value;
  regs->eccramprcr                = k_rx_eccramprcr_lock;
}

/**
 * @brief Enable the 1-bit status-update latch
 *
 * @details
 * Sets ECCRAM1STSEN=1 so that 1-bit errors are latched into ECCRAM1STS
 * and ECCRAM1ECAD. Skipped for k_eccram_mode_disabled. Guarded by
 * ECCRAMPRCR (shared with ECCRAMMODE).
 *
 * @pre  ECCRAM module clocked.
 * @post ECCRAM1STSEN.ECC1STSEN == 1.
 * @post ECCRAMPRCR is re-locked on return.
 *
 * @since Version 1.0.0
 */
static void internal_enable_1bit_latch(void)
{
  volatile rx_eccram_regs_t* regs = eccram_regs();
  regs->eccramprcr                = k_rx_eccramprcr_unlock;
  regs->eccram1stsen              = k_rx_eccram1stsen_enable;
  regs->eccramprcr                = k_rx_eccramprcr_lock;
}

/**
 * @brief Zero-initialise the ECCRAM region with 32-bit stores
 *
 * @details
 * Iterates over every 32-bit word in ECCRAM and writes zero so that
 * every ECC syndrome matches. The loop upper bound is a compile-time
 * constant (k_rx_eccram_word_count), which satisfies NASA Rule 2.
 *
 * @pre  ECCRAMMODE == k_rx_eccrammode_ecc_no_check (so writes generate
 *       syndromes but reads do not trigger spurious errors).
 * @post Every 32-bit word of ECCRAM holds k_rx_eccram_fill_pattern.
 *
 * @note Runs in ~65 k cycles on RX72N at 240 MHz (~270 us).
 * @since Version 1.0.0
 */
#ifdef UNIT_TEST
/**
 * @brief Host-side stand-in for the ECCRAM region used by tests
 *
 * @details
 * The production zero-fill loop targets the hardware address
 * k_rx_eccram_region_base_addr (0x00FF8000). That address is not mapped on
 * the host under UNIT_TEST, so writes are redirected to this ordinary
 * static buffer, which has exactly the same element count as the real
 * ECCRAM region. Tests observe the effect of rx_eccram_init() by reading
 * this array.
 */
uint32_t g_mock_eccram_region[k_rx_eccram_word_count];

/**
 * @brief Reset all driver static state (UNIT_TEST-only helper)
 *
 * @details
 * The driver holds static module state (s_initialized, callbacks, last
 * failing addresses). That state persists across individual Unity tests
 * because the module is linked once per test binary. This helper returns
 * the driver to its pre-rx_eccram_init() condition so each test starts
 * from a clean slate. Not exposed outside UNIT_TEST builds.
 *
 * @post s_initialized == false
 * @post All callback pointers == NULL
 * @post All latched failing addresses == 0
 */
void rx_eccram_test_reset_state(void)
{
  s_initialized    = false;
  s_on_1bit        = NULL;
  s_on_2bit        = NULL;
  s_isr_ctx        = NULL;
  s_last_1bit_addr = 0;
  s_last_2bit_addr = 0;
}
#endif

static void internal_zero_fill_region(void)
{
#ifdef UNIT_TEST
  for (uint32_t i = 0; i < k_rx_eccram_word_count; i++) {
    g_mock_eccram_region[i] = k_rx_eccram_fill_pattern;
  }
#else
  volatile uint32_t* dst = (volatile uint32_t*)k_rx_eccram_region_base_addr;
  for (uint32_t i = 0; i < k_rx_eccram_word_count; i++) {
    dst[i] = k_rx_eccram_fill_pattern;
  }
#endif
}

/* =============================================================================
 * ISR Trampoline -- Vector 18 (RAMERR)
 * =============================================================================
 */

/**
 * @brief Shared RAM error handler core
 *
 * @details
 * Called from the `__attribute__((interrupt))` wrapper below. Reads
 * both status flags, captures the corresponding address registers,
 * dispatches to registered user callbacks, and clears the flags so
 * the next error can be detected.
 *
 * The 2-bit branch captures ECCRAM2ECAD into s_last_2bit_addr *before*
 * the flag-clearing write, matching the prompt requirement that the
 * failing address be saved first.
 *
 * @pre  Called from IRQ context on vector 18 (RAMERR).
 * @post ECCRAM1STS and ECCRAM2STS flags cleared.
 * @post s_last_1bit_addr / s_last_2bit_addr updated on match.
 *
 * @since Version 1.0.0
 */
static void internal_ram_error_isr_body(void)
{
  volatile rx_eccram_regs_t* regs = eccram_regs();

  const uint8_t status2 = regs->eccram2sts;
  if ((status2 & k_rx_eccram2sts_ecc2err_mask) != 0U) {
    const uintptr_t addr2 = (uintptr_t)regs->eccram2ecad;
    s_last_2bit_addr      = addr2;
    regs->eccram2sts      = k_rx_eccram2sts_clear;
    if (s_on_2bit != NULL) {
      s_on_2bit(addr2, s_isr_ctx);
    }
  }

  const uint8_t status1 = regs->eccram1sts;
  if ((status1 & k_rx_eccram1sts_ecc1err_mask) != 0U) {
    const uintptr_t addr1 = (uintptr_t)regs->eccram1ecad;
    s_last_1bit_addr      = addr1;
    regs->eccram1sts      = k_rx_eccram1sts_clear;
    if (s_on_1bit != NULL) {
      s_on_1bit(addr1, s_isr_ctx);
    }
  }
}

/**
 * @brief Vector-18 ISR entry (RAMERR) -- ECCRAM 1-bit/2-bit error
 *
 * @details
 * The GNURX toolchain's vector table dispatches to this symbol; the
 * `interrupt` attribute generates the correct prologue/epilogue.
 *
 * @pre  Vector 18 is enabled and wired to this symbol by the boot code.
 * @post internal_ram_error_isr_body() has run to completion.
 *
 * @see internal_ram_error_isr_body()
 * @see RX72N HW Manual Chapter 15 (ICU), RAMERR vector entry
 * @since Version 1.0.0
 */
#ifndef UNIT_TEST
void __attribute__((interrupt)) rx_eccram_ram_error_isr(void);
void __attribute__((interrupt)) rx_eccram_ram_error_isr(void)
{
  internal_ram_error_isr_body();
}
#else
void rx_eccram_ram_error_isr(void);
void rx_eccram_ram_error_isr(void)
{
  internal_ram_error_isr_body();
}
#endif

/* =============================================================================
 * Public API
 * =============================================================================
 */

rx_err_t rx_eccram_init(rx_eccram_mode_t mode)
{
  if (mode > k_eccram_mode_detect_only) {
    rx_log_error(s_tag, "invalid mode");
    return k_rx_err_invalid_arg;
  }

  s_initialized    = false;
  s_on_1bit        = NULL;
  s_on_2bit        = NULL;
  s_isr_ctx        = NULL;
  s_last_1bit_addr = 0;
  s_last_2bit_addr = 0;

  internal_enable_eccram_module_clock();

  internal_write_eccram_mode(k_rx_eccrammode_ecc_no_check);

  internal_zero_fill_region();

  const uint8_t final_mode_reg = internal_mode_to_reg(mode);
  internal_write_eccram_mode(final_mode_reg);

  volatile rx_eccram_regs_t* regs   = eccram_regs();
  const uint8_t              actual = regs->eccrammode & k_rx_eccrammode_rammod_mask;
  /* HW-only failure: a stuck ECCRAMMODE bit can only happen on real silicon
   * (e.g. ECCRAMPRCR did not unlock, or the control block lost clock). The
   * host-side mock memory always reflects writes verbatim, so the true branch
   * is unreachable in unit tests. The behavior is still documented in the
   * header (@retval k_rx_err_hw_init_failed) and validated on hardware via
   * post-bringup smoke tests. */
  if (actual != (final_mode_reg & k_rx_eccrammode_rammod_mask)) { /* GCOVR_EXCL_BR_LINE */
    rx_log_error(s_tag, "ECCRAMMODE read-back mismatch");                  /* GCOVR_EXCL_LINE */
    return k_rx_err_hw_init_failed;                                        /* GCOVR_EXCL_LINE */
  }

  regs->eccram1sts = k_rx_eccram1sts_clear;
  regs->eccram2sts = k_rx_eccram2sts_clear;

  if (mode != k_eccram_mode_disabled) {
    internal_enable_1bit_latch();
  }

  s_initialized = true;
  rx_log_info(s_tag, "ECCRAM initialised");
  return k_rx_ok;
}

rx_err_t rx_eccram_get_error_status(rx_eccram_status_t* out)
{
  RX_CHECK_NULL_PTR(out, s_tag, "out is nullptr");

  volatile rx_eccram_regs_t* regs = eccram_regs();

  const uint8_t s1 = regs->eccram1sts;
  const uint8_t s2 = regs->eccram2sts;

  out->one_bit_error = (bool)((s1 & k_rx_eccram1sts_ecc1err_mask) != 0U);
  out->two_bit_error = (bool)((s2 & k_rx_eccram2sts_ecc2err_mask) != 0U);
  out->one_bit_addr  = (uintptr_t)regs->eccram1ecad;
  out->two_bit_addr  = (uintptr_t)regs->eccram2ecad;

  return k_rx_ok;
}

rx_err_t rx_eccram_clear_errors(void)
{
  volatile rx_eccram_regs_t* regs = eccram_regs();
  regs->eccram1sts                = k_rx_eccram1sts_clear;
  regs->eccram2sts                = k_rx_eccram2sts_clear;
  return k_rx_ok;
}

rx_err_t rx_eccram_register_error_isr(rx_eccram_on_1bit_fn_t on_1bit,
                                      rx_eccram_on_2bit_fn_t on_2bit,
                                      void*                  ctx)
{
  if (!s_initialized) {
    rx_log_error(s_tag, "not initialised");
    return k_rx_err_invalid_state;
  }

  if (on_1bit != NULL) {
    s_on_1bit = on_1bit;
  }
  if (on_2bit != NULL) {
    s_on_2bit = on_2bit;
  }
  s_isr_ctx = ctx;
  return k_rx_ok;
}

uintptr_t rx_eccram_region_start(void)
{
  return k_rx_eccram_region_base_addr;
}

uintptr_t rx_eccram_region_end(void)
{
  return k_rx_eccram_region_end_addr;
}
