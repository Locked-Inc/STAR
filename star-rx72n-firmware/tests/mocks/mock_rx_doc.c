/**
 * @file mock_rx_doc.c
 * @brief Mock DOC Register / System-Register Implementation for Unit Testing
 *
 * @details
 * Backs the declarations in mock_rx_doc.h. Provides:
 *
 * - Storage for g_mock_doc_regs, g_mock_doc_system_regs, and g_mock_prcr
 * - A doc() accessor that increments a call counter on every invocation
 * - Helpers for tests to arm / inspect the mock (reset, set_auto_flag,
 *   set_dopcf, trigger_operation, get_access_count, auto_flag_enabled)
 *
 * ## DOC Hardware Emulation
 *
 * In auto-flag mode (see mock_rx_doc_set_auto_flag()), tests can have the
 * mock compute DOPCF and DODSR in software whenever they call
 * mock_rx_doc_trigger_operation(). Behavior emulated:
 *
 * - compare / compare_neq: DOPCF = (written == DODIR) / (written != DODIR)
 * - add:                    DODSR = (dodsr_before + DODIR) & 0xFFFF;
 *                           DOPCF set on carry past 0xFFFF
 * - subtract:               DODSR = (dodsr_before - DODIR) & 0xFFFF;
 *                           DOPCF set on borrow below 0x0000
 *
 * The mock never touches DOPCF unless the test explicitly asks it to, which
 * keeps the driver's flag-clear path (write DOPCFCL = 1) fully testable as a
 * plain struct write.
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "mock_rx_doc.h"

/* =============================================================================
 * Mock Storage
 * =============================================================================
 */

/** @brief Storage for the DOC register block mock. */
rx_doc_regs_t g_mock_doc_regs;

/** @brief Storage for the system-register block mock used by rx_doc.c. */
rx_system_regs_t g_mock_doc_system_regs;

/**
 * @brief Storage for the PRCR mock used by the mock_rx72n_system_regs.h
 *        prcr_reg() inline accessor.
 *
 * @details
 * mock_rx72n_system_regs.h declares `extern volatile uint16_t g_mock_prcr;`
 * and defines prcr_reg() to return its address. Defining the variable here
 * keeps the DOC test fully self-contained (no link-time dependency on the
 * IWDT mock suite's mock_rx_system_regs.c).
 */
/* NOLINTNEXTLINE(readability-magic-numbers) -- 0xA500 is the PRCR
 * locked-state default, defined as k_mock_doc_prcr_locked below; we
 * can't forward-reference the enum here. */
volatile uint16_t g_mock_prcr = 0xA500U;

/* =============================================================================
 * Internal Constants
 * =============================================================================
 */

/**
 * @enum mock_doc_mstpcrb_defaults_t
 * @brief Default reset values for the mocked MSTPCRB register
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief All module-stop bits set: every peripheral stopped after reset */
  k_mock_doc_mstpcrb_all_stopped = 0xFFFFFFFFU,
} mock_doc_mstpcrb_defaults_t;

/**
 * @enum mock_doc_prcr_defaults_t
 * @brief Default reset values for the mocked PRCR register
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /** @brief PRKEY=0xA5, all PRC bits cleared (locked state) */
  k_mock_doc_prcr_locked = 0xA500U,
} mock_doc_prcr_defaults_t;

/**
 * @enum mock_doc_arith_limits_t
 * @brief Arithmetic boundary values for the 16-bit add/subtract emulation
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief 17-bit mask used to detect carry past 0xFFFF in 32-bit add */
  k_mock_doc_carry_bit = 0x10000U,
  /** @brief 16-bit mask used to wrap the 32-bit add/subtract result */
  k_mock_doc_u16_mask = 0xFFFFU,
} mock_doc_arith_limits_t;

/* =============================================================================
 * Internal State
 * =============================================================================
 */

/** @brief Number of times doc() has been called since mock_rx_doc_reset(). */
static uint32_t s_access_count = 0U;

/** @brief Whether mock_rx_doc_trigger_operation() should modify DOPCF/DODSR. */
static bool s_auto_flag_enabled = false;

/* =============================================================================
 * Public Mock API
 * =============================================================================
 */

void mock_rx_doc_reset(void)
{
  /* C99 compound-literal zero-init -- avoids cert-msc24-c (memset insecure)
   * and gives the compiler freedom to pick the optimal zero-fill. */
  g_mock_doc_regs                = (rx_doc_regs_t){0};
  g_mock_doc_system_regs         = (rx_system_regs_t){0};
  g_mock_doc_system_regs.mstpcrb = k_mock_doc_mstpcrb_all_stopped;
  g_mock_prcr                    = k_mock_doc_prcr_locked;
  s_access_count                 = 0U;
  s_auto_flag_enabled            = false;
}

void mock_rx_doc_set_auto_flag(bool enabled)
{
  s_auto_flag_enabled = enabled;
}

bool mock_rx_doc_auto_flag_enabled(void)
{
  return s_auto_flag_enabled;
}

void mock_rx_doc_set_dopcf(bool set)
{
  if (set) {
    g_mock_doc_regs.docr |= k_doc_docr_dopcf_mask;
  } else {
    g_mock_doc_regs.docr &= (uint8_t)~k_doc_docr_dopcf_mask;
  }
}

void mock_rx_doc_trigger_operation(rx_doc_mode_t mode,
                                   uint16_t      dodsr_before,
                                   uint16_t      dodsr_written)
{
  if (!s_auto_flag_enabled) {
    return;
  }

  bool flag = false;
  switch (mode) {
    case k_rx_doc_mode_compare:
      flag = (bool)(g_mock_doc_regs.dodir == dodsr_written);
      break;
    case k_rx_doc_mode_compare_neq:
      flag = (bool)(g_mock_doc_regs.dodir != dodsr_written);
      break;
    case k_rx_doc_mode_add: {
      const uint32_t sum    = (uint32_t)dodsr_before + (uint32_t)g_mock_doc_regs.dodir;
      flag                  = (bool)((sum & k_mock_doc_carry_bit) != 0U);
      g_mock_doc_regs.dodsr = (uint16_t)(sum & k_mock_doc_u16_mask);
      (void)dodsr_written;
      break;
    }
    case k_rx_doc_mode_subtract: {
      const int32_t diff    = (int32_t)dodsr_before - (int32_t)g_mock_doc_regs.dodir;
      flag                  = (bool)(diff < 0);
      g_mock_doc_regs.dodsr = (uint16_t)((uint32_t)diff & k_mock_doc_u16_mask);
      (void)dodsr_written;
      break;
    }
    default:
      return;
  }

  mock_rx_doc_set_dopcf(flag);
}

uint32_t mock_rx_doc_get_access_count(void)
{
  return s_access_count;
}

/* =============================================================================
 * Mock Accessor -- Required by rx_doc.c
 * =============================================================================
 */

volatile rx_doc_regs_t* doc(void)
{
  s_access_count++;
  return (volatile rx_doc_regs_t*)&g_mock_doc_regs;
}
