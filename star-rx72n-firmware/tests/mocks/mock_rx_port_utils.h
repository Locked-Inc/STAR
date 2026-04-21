/**
 * @file mock_rx_port_utils.h
 * @brief Mock PORT utility functions for unit testing
 *
 * @details
 * Replaces the real rx_port_utils.h to provide RAM-based port register
 * access for host-side testing. Includes mock_rx72n_port_regs.h
 * instead of the real hardware version.
 *
 * @author Locked, Inc.
 * @date 2026-03-03
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: No goto or recursion in table-driven lookup
 * - Rule 4: rx_port_get_base is a single concise function (under 60 lines)
 * - Rule 6: Variables at smallest scope (no file-scope state)
 * - Rule 9: Function pointers used for table-driven port accessor lookup
 *
 * @par SOLID Principles:
 * - **L (Liskov Substitution):** Mock rx_port_get_base substitutes for the
 *   real hardware version, enabling host-side unit testing
 * - **D (Dependency Inversion):** Production code depends on this abstract
 *   interface rather than direct hardware register addresses
 */

#pragma once

#include "mock_rx72n_port_regs.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mock version of rx_port_get_base -- maps port enum to RAM struct
 *
 * @details
 * Translates a port number (using k_rx_port_* constants from
 * rx_port_constants.h) into a volatile pointer to the corresponding entry
 * in the g_mock_port_regs RAM array. This allows production code that calls
 * rx_port_get_base() to operate on mock registers during host-side testing
 * without any source changes.
 *
 * @param[in] port Port number using k_rx_port_* constants (valid range 0-18;
 *                 ports 0-16 map to ports 0-9/A-F/G; index 17 (Port H) is
 *                 reserved and returns nullptr; index 18 (0x12, k_rx_port_j)
 *                 maps to port J)
 *
 * @return Volatile pointer to the mock port register struct for the given port
 * @retval non-NULL Valid pointer to g_mock_port_regs entry for recognized ports
 * @retval nullptr  Port number not recognized (out of range or reserved index)
 *
 * @pre port must be a valid k_rx_port_* constant from rx_port_constants.h
 * @pre g_mock_port_regs array must be defined (in mock_drv8263_port.c)
 *
 * @post Returned pointer (when non-NULL) is valid for the lifetime of the test
 * @post No side effects on any port register contents (pure lookup)
 *
 * @note Thread-safe; performs read-only pointer computation with no shared
 *       mutable state
 *
 * @par NASA Rule 5 Deviation (Precondition Assertion):
 * This function does NOT assert(port < k_port_table_size) because it IS the
 * bounds-validation function for GPIO port numbers. Production callers that
 * pass invalid ports rely on the nullptr return value to detect errors.
 * Adding an assert() here would abort test processes that intentionally supply
 * out-of-range ports to verify error handling (e.g., test_init_invalid_drvoff_port).
 * The explicit bounds check `if (port >= k_port_table_size) { return nullptr; }`
 * at the start of the function body fulfills the Rule 5 intent.
 *
 * @see rx_port_constants.h Port number definitions (k_rx_port_0 through
 *      k_rx_port_j)
 * @see rx_port_regs_t Port register structure layout
 * @see g_mock_port_regs Global mock register array
 *
 * @code
 * typedef enum : uint8_t { k_example_pin_0 = 0 } example_pin_t;
 * volatile rx_port_regs_t* regs = rx_port_get_base(k_rx_port_0);
 * if (regs != nullptr) {
 *   enum : uint8_t { k_bit_one = 1 };
 *   regs->podr |= (uint8_t)(k_bit_one << k_example_pin_0);
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */

/**
 * @typedef port_accessor_fn_t
 * @brief Function pointer type for port accessor functions
 * @details Used by the table-driven lookup in rx_port_get_base() to map
 *          k_rx_port_* constants to their corresponding port register structs.
 * @see rx_port_get_base() Table-driven lookup that uses this type
 * @since Version 1.0.0
 */
typedef volatile rx_port_regs_t* (*port_accessor_fn_t)(void);

static inline volatile rx_port_regs_t* rx_port_get_base(uint8_t port)
{
  /** @brief Covers port constants 0-18 (k_rx_port_j = 0x12 = 18) */
  enum : uint8_t { k_port_table_size = 19 };

  /**
   * @brief Table mapping k_rx_port_* constants to mock port accessor functions
   * @note Read-only after initialization; entries indexed by k_rx_port_* values.
   *       Unassigned slots (e.g., reserved port index 17) contain nullptr and
   *       cause rx_port_get_base() to return nullptr safely.
   * @warning Direct modification is forbidden; the table is const and populated
   *          at compile time. Use the port accessor functions (port0..portj) via
   *          rx_port_get_base() rather than indexing this table directly.
   */
  static const port_accessor_fn_t s_port_accessors[k_port_table_size] = {
    [k_rx_port_0] = port0,
    [k_rx_port_1] = port1,
    [k_rx_port_2] = port2,
    [k_rx_port_3] = port3,
    [k_rx_port_4] = port4,
    [k_rx_port_5] = port5,
    [k_rx_port_6] = port6,
    [k_rx_port_7] = port7,
    [k_rx_port_8] = port8,
    [k_rx_port_9] = port9,
    [k_rx_port_a] = porta,
    [k_rx_port_b] = portb,
    [k_rx_port_c] = portc,
    [k_rx_port_d] = portd,
    [k_rx_port_e] = porte,
    [k_rx_port_f] = portf,
    [k_rx_port_g] = portg,
    [k_rx_port_j] = portj,
  };

  if (port >= k_port_table_size) {
    return nullptr;
  }

  const port_accessor_fn_t fn = s_port_accessors[port];
  return (fn != nullptr) ? fn() : nullptr;
}

#ifdef __cplusplus
}
#endif
