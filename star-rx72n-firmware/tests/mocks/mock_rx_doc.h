/**
 * @file mock_rx_doc.h
 * @brief Mock DOC Register Definitions for Unit Testing
 *
 * @details
 * Wraps the real rx72n_doc_regs.h but substitutes the doc() inline accessor
 * with one that returns a pointer to a mock register area. Source files that
 * use DOC registers select this header via `#ifdef UNIT_TEST` guards:
 * `#include "mock_rx_doc.h"` in test builds, `#include "rx72n_doc_regs.h"` in
 * production builds.
 *
 * ## Memory Simulation
 *
 * The mock allocates an `rx_doc_regs_t` structure in normal host memory
 * (`g_mock_doc_regs`) so that reads/writes to DOCR, DODIR, and DODSR behave
 * like plain C struct member accesses. Tests may additionally arm special
 * semantics via the mock_rx_doc_* helpers -- most notably, auto-setting DOPCF
 * on DODSR write to emulate the DOC's compare/add/subtract trigger behavior
 * without real hardware.
 *
 * ## System Register Mock
 *
 * Because rx_doc.c also pokes MSTPCRB.MSTPB6 (under PRCR.PRC1 unlock) to
 * clock-gate the DOC peripheral, this header also provides a mock
 * system_regs() accessor backed by `g_mock_doc_system_regs` and links with
 * the existing mock_rx72n_system_regs.h for PRCR / rx_system_regs_t type
 * definitions.
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

/* Pull in rx_system_regs_t, PRCR mock, and prcr_reg() accessor. */
#include "mock_rx72n_system_regs.h"

/* rx_doc_mode_t is used by mock_rx_doc_trigger_operation(). */
#include "../../libs/rx_hal/inc/rx_doc.h"

/* Rename the real doc() inline accessor so the mock can override it. */
#define doc real_doc

/* Bring in all type/enum/struct definitions from the real header. */
#include "../../libs/rx_hal/inc/rx72n_doc_regs.h"

/* Restore the name so our mock accessor can own it below. */
#undef doc

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @var g_mock_doc_regs
 * @brief Mock DOC register area (allocated in mock_rx_doc.c)
 *
 * @details
 * Ordinary C struct standing in for the memory-mapped DOC register block at
 * 0x0008B080. Tests may read/write fields directly or via doc().
 *
 * @since Version 1.0.0
 */
extern rx_doc_regs_t g_mock_doc_regs;

/**
 * @var g_mock_doc_system_regs
 * @brief Mock system-register block used by the DOC driver test suite
 *
 * @details
 * Backs the mock system_regs() accessor below. Only the `mstpcrb` field is
 * exercised by rx_doc.c today, but the full rx_system_regs_t layout is
 * preserved for future DOC features (e.g., interrupt wiring through
 * GROUPBL0).
 *
 * @since Version 1.0.0
 */
extern rx_system_regs_t g_mock_doc_system_regs;

/**
 * @brief Reset mock DOC state to a clean, post-reset-equivalent configuration
 *
 * @details
 * Zeros DOCR, DODIR, DODSR; disables the flag-auto-update mode; clears the
 * access counter. Also resets g_mock_doc_system_regs.mstpcrb to 0xFFFFFFFF
 * (simulating all modules stopped after power-on reset) and sets g_mock_prcr
 * to the locked state (0xA500). Call from each test's setUp() for
 * deterministic runs.
 *
 * @post g_mock_doc_regs is zeroed
 * @post g_mock_doc_system_regs.mstpcrb == 0xFFFFFFFF (all stopped)
 * @post g_mock_prcr == 0xA500 (locked)
 * @post mock_rx_doc_auto_flag_enabled() returns false
 * @post mock_rx_doc_get_access_count() returns 0
 * @since Version 1.0.0
 */
void mock_rx_doc_reset(void);

/**
 * @brief Enable or disable automatic DOPCF updates on DODSR writes
 *
 * @details
 * When enabled, the mock auto-sets DOPCF the next time the driver writes
 * DODSR, simulating what real DOC hardware does in one of the four modes:
 *
 * - k_rx_doc_mode_compare:     DOPCF set when DODSR == DODIR
 * - k_rx_doc_mode_compare_neq: DOPCF set when DODSR != DODIR
 * - k_rx_doc_mode_add:         DODSR becomes (DODSR_before + DODIR) mod 65536;
 *                              DOPCF set on carry past 0xFFFF
 * - k_rx_doc_mode_subtract:    DODSR becomes (DODSR_before - DODIR) mod 65536;
 *                              DOPCF set on borrow below 0x0000
 *
 * When disabled (the default), DODSR retains whatever the driver wrote and
 * DOPCF keeps whatever a test loaded via mock_rx_doc_set_dopcf().
 *
 * Because DODSR writes go through the mock's doc() accessor, the test must
 * manually call mock_rx_doc_trigger_operation() with the configured mode and
 * the value the driver just wrote to emulate one hardware cycle.
 *
 * @param[in] enabled true to enable, false to disable
 * @post mock_rx_doc_auto_flag_enabled() returns @p enabled
 * @since Version 1.0.0
 */
void mock_rx_doc_set_auto_flag(bool enabled);

/**
 * @brief Query whether auto-flag mode is currently enabled
 * @return true if DODSR writes trigger emulated DOC semantics, false otherwise
 * @since Version 1.0.0
 */
bool mock_rx_doc_auto_flag_enabled(void);

/**
 * @brief Explicitly set or clear DOPCF in the mock
 *
 * @details
 * Bypasses auto-flag mode entirely: directly OR's in or masks out bit 5 of
 * DOCR so a test can pre-load the flag before a call to rx_doc_compare()
 * or its add/subtract siblings.
 *
 * @param[in] set true to set DOPCF (bit 5), false to clear
 * @post (doc()->docr & k_doc_docr_dopcf_mask) reflects @p set
 * @since Version 1.0.0
 */
void mock_rx_doc_set_dopcf(bool set);

/**
 * @brief Apply one cycle of emulated DOC behavior to the mock registers
 *
 * @details
 * Call this from a test after the driver-under-test has written DODSR and
 * you want the mock to "execute" the configured operation. Supplies the
 * value the driver wrote and the pre-trigger DODSR value so the emulator
 * can compute the post-trigger DODSR and DOPCF.
 *
 * For compare modes the comparison is DODIR vs @p dodsr_written; for add and
 * subtract the pre-trigger DODSR is taken from @p dodsr_before.
 *
 * @param[in] mode          Which operation the driver was configured for
 * @param[in] dodsr_before  DODSR value just before the driver wrote DODSR
 * @param[in] dodsr_written Value the driver just wrote to DODSR
 *
 * @pre Mock is in auto-flag mode (mock_rx_doc_set_auto_flag(true))
 * @post DOPCF reflects one cycle of DOC behavior for @p mode
 * @post For add/subtract modes, DODSR reflects the arithmetic result
 *
 * @since Version 1.0.0
 */
void mock_rx_doc_trigger_operation(rx_doc_mode_t mode,
                                   uint16_t      dodsr_before,
                                   uint16_t      dodsr_written);

/**
 * @brief Get number of times the mock doc() accessor has been invoked
 * @return Call count since mock_rx_doc_reset()
 * @since Version 1.0.0
 */
uint32_t mock_rx_doc_get_access_count(void);

/**
 * @brief Mock doc() accessor -- returns pointer to the mock register area
 *
 * @details
 * Not `static inline`: the call-counter increment is an observable side
 * effect, so the mock is defined in mock_rx_doc.c and exported here as an
 * ordinary prototype. Every TU under test that calls doc() sees the same
 * storage (g_mock_doc_regs).
 *
 * @return Volatile pointer to g_mock_doc_regs (never NULL)
 *
 * @pre mock_rx_doc_reset() has been called at least once (recommended)
 * @post Pointer remains valid for the lifetime of the test
 *
 * @since Version 1.0.0
 */
volatile rx_doc_regs_t* doc(void);

/**
 * @brief Mock system_regs() accessor used by rx_doc.c's clock-gate code
 *
 * @details
 * Returns a pointer to g_mock_doc_system_regs. rx_doc.c only reads/writes
 * mstpcrb through this accessor, but the whole rx_system_regs_t layout is
 * preserved for completeness.
 *
 * @return Non-NULL volatile pointer to g_mock_doc_system_regs
 * @since Version 1.0.0
 */
static inline volatile rx_system_regs_t* system_regs(void)
{
  return &g_mock_doc_system_regs;
}

#ifdef __cplusplus
}
#endif
