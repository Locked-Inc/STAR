/**
 * @file mock_rx_cac.h
 * @brief Mock CAC Register Definitions for Unit Testing
 *
 * @details
 * Wraps the real rx72n_cac_regs.h and overrides the inline cac() accessor so
 * unit tests can read and write a host-side mock register bank instead of
 * touching the CAC peripheral at physical address 0x0008B000.
 *
 * Source files under test select this header via `#ifdef UNIT_TEST` guards:
 * - Production: `#include "rx72n_cac_regs.h"` -> real hardware address
 * - Test:       `#include "mock_rx_cac.h"`    -> &g_mock_cac_regs
 *
 * Additional helper functions are provided to reset mock state between tests
 * and to count accesses, allowing tests to assert that a driver actually
 * wrote/read the expected registers.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto / setjmp / recursion
 * - Rule 2: [OK] No loops
 * - Rule 3: [OK] Zero dynamic memory allocation
 * - Rule 4: [OK] All functions < 10 lines
 * - Rule 5: N/A (declarations only)
 * - Rule 8: [OK] C23 typed enums for constants
 * - Rule 9: [OK] Single-level pointers
 * - Rule 10: [OK] -Wall -Wextra -Werror clean
 *
 * @see rx72n_cac_regs.h Real register definitions
 * @see mock_rx_cac.c Backing storage and helper implementations
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Rename the real cac() accessor so this header can substitute its mock. */
#define cac real_cac
#include "../../libs/rx_hal/inc/rx72n_cac_regs.h"
#undef cac

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Host-side mock of the CAC register block
 * @details Defined once in mock_rx_cac.c, exposed here so multiple test files
 * can all observe the same backing storage.
 * @since Version 1.0.0
 */
extern rx_cac_regs_t g_mock_cac_regs;

/**
 * @brief Mock accessor replacing the real cac() inline
 * @return Volatile pointer to the mock register bank
 * @post Returned pointer is non-null and stable for the life of the test
 * @since Version 1.0.0
 */
static inline volatile rx_cac_regs_t* cac(void)
{
  return (volatile rx_cac_regs_t*)&g_mock_cac_regs;
}

/**
 * @brief Reset the mock CAC register bank to its power-on state
 * @details Clears every byte in g_mock_cac_regs and zeros the accessor counters.
 * Call from setUp() in each Unity test function to isolate tests.
 * @post All mock register fields == 0
 * @post All access counters == 0
 * @since Version 1.0.0
 */
void mock_rx_cac_reset(void);

/**
 * @brief Test hook that clears the CAC driver's initialization flag
 * @details Implemented in rx_cac.c under the UNIT_TEST guard. Called from
 * setUp() to ensure every Unity test starts with the driver in the
 * not-initialized state.
 * @post rx_cac_init() may be called again without returning k_rx_err_invalid_state
 * @since Version 1.0.0
 */
void rx_cac_test_reset(void);

#ifdef __cplusplus
}
#endif
