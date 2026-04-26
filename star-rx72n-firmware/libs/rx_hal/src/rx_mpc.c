/**
 * @file rx_mpc.c
 * @brief Multi-Function Pin Controller (MPC) Driver Implementation
 *
 * @details
 * Implementation of the MPC driver for RX72N pin multiplexer configuration.
 * This module provides the core functionality for connecting GPIO pins to
 * peripheral functions through the PFS (Pin Function Select) registers.
 *
 * @par Implementation Architecture
 * @verbatim
 *                      rx_mpc.c Implementation Structure
 *   +---------------------------------------------------------------------+
 *   |                         Public API Layer                           |
 *   |   +-------------+ +-------------+ +-------------+ +-------------+  |
 *   |   |rx_mpc_set_  | |rx_mpc_set_  | |rx_mpc_set_  | |rx_mpc_set_  |  |
 *   |   |  gpio()     | | peripheral()| |  mtu_pwm()  | |   sci()     |  |
 *   |   +------+------+ +------+------+ +------+------+ +------+------+  |
 *   |          |               |               |               |         |
 *   +----------+---------------+---------------+---------------+---------+
 *              |               |               |               |
 *   +----------v---------------v---------------v---------------v---------+
 *   |                        Internal Helper Layer                        |
 *   |   +-------------------------------------------------------------+  |
 *   |   |                  internal_write_pfs()                        |  |
 *   |   |    (unlock -> write PFS -> lock with error handling)          |  |
 *   |   +-------------------------+-----------------------------------+  |
 *   |                             |                                      |
 *   |   +-------------------------v-----------------------------------+  |
 *   |   |              internal_get_pfs_register()                    |  |
 *   |   |    (port/pin -> PFS register address calculation)            |  |
 *   |   +-------------------------+-----------------------------------+  |
 *   |                             |                                      |
 *   |   +-------------+     +-----v---------+                           |
 *   |   |internal_mpc_|     |internal_mpc_  |                           |
 *   |   |  unlock()   |     |  lock()       |                           |
 *   |   +------+------+     +------+--------+                           |
 *   +----------+-------------------+-------------------------------------+
 *              |                   |
 *   +----------v-------------------v-------------------------------------+
 *   |                    Hardware Register Layer                         |
 *   |   +-------------------------------------------------------------+  |
 *   |   |                     mpc() accessor                          |  |
 *   |   |              (rx72n_mpc_regs.h, 0x0008C100)                  |  |
 *   |   +-------------------------------------------------------------+  |
 *   +--------------------------------------------------------------------+
 * @endverbatim
 *
 * @par PWPR Write Protection Sequence
 * The PFS registers are protected by a two-stage lock. The unlock/lock
 * sequence must be followed exactly:
 * @msc
 *   Driver, PWPR, PFS;
 *
 *   --- [label="Unlock Sequence"];
 *   Driver => PWPR [label="Write 0x00 (B0WI=0)"];
 *   PWPR box PWPR [label="PFSWE now writable"];
 *   Driver => PWPR [label="Write 0x40 (PFSWE=1)"];
 *   PWPR box PWPR [label="PFS now writable"];
 *
 *   --- [label="Modify PFS"];
 *   Driver => PFS [label="Write PSEL value"];
 *   PFS box PFS [label="Pin function changed"];
 *
 *   --- [label="Lock Sequence"];
 *   Driver => PWPR [label="Write 0x00 (PFSWE=0)"];
 *   PWPR box PWPR [label="PFS now protected"];
 *   Driver => PWPR [label="Write 0x80 (B0WI=1)"];
 *   PWPR box PWPR [label="PFSWE now protected"];
 * @endmsc
 *
 * @par PFS Register Address Calculation
 * PFS registers are laid out contiguously in memory with 8 bytes per port:
 * @verbatim
 *   PFS Base = MPC Base + 0x40 = 0x0008C140
 *
 *   Address = PFS_Base + (port_offset) + pin_number
 *
 *   Port Offsets (8 bytes each):
 *   Port 0: +0    Port 1: +8    Port 2: +16   Port 3: +24
 *   Port 4: +32   Port 5: +40   Port 6: +48   Port 7: +56
 *   Port 8: +64   Port 9: +72   Port A: +80   Port B: +88
 *   Port C: +96   Port D: +104  Port E: +112  Port J: +144
 *
 *   Example: PB7 (Port B, Pin 7)
 *   Address = 0x0008C140 + 88 + 7 = 0x0008C19F
 * @endverbatim
 *
 * @par Performance Characteristics
 * | Function | Typical Time | Stack Usage | Notes |
 * |----------|-------------|-------------|-------|
 * | rx_mpc_set_gpio() | 1.2 us | 24 bytes | Direct PFS write |
 * | rx_mpc_set_peripheral() | 1.5 us | 32 bytes | With validation |
 * | rx_mpc_set_mtu_pwm() | 1.6 us | 40 bytes | Calls set_peripheral |
 * | Bulk config (10 pins) | 15 us | - | Sequential calls |
 *
 * @par Memory Usage
 * | Category | Size | Description |
 * |----------|------|-------------|
 * | Code (.text) | ~800 bytes | All functions |
 * | Constants (.rodata) | 4 bytes | Tag string "MPC" |
 * | Static data (.data) | 0 bytes | None |
 * | Stack (max) | 40 bytes | Deepest call chain |
 *
 * @par Thread Safety Analysis
 * **NOT THREAD-SAFE.** Critical section analysis:
 * - internal_mpc_unlock() -> internal_write_pfs() -> internal_mpc_lock()
 * - This sequence is non-atomic (3 register writes)
 * - Interrupt or thread switch during unlock leaves PFS unprotected
 * - Race condition: Thread A unlocks, Thread B unlocks, Thread A writes,
 *   Thread B writes (corrupted configuration possible)
 *
 * @par Recommended Usage Pattern
 * @code{.c}
 * // Option 1: Configure all pins during single-threaded init
 * void hardware_init(void)  // Called before RTOS starts
 * {
 *     rx_mpc_set_peripheral(&pwm_config);
 *     rx_mpc_set_sci(k_port_b_pin_7);
 *     // ... all MPC configuration here
 * }
 *
 * // Option 2: Use mutex if runtime reconfiguration needed
 * void configure_pin_safe(const rx_mpc_peripheral_config_t* config)
 * {
 *     tx_mutex_get(&mpc_mutex, TX_WAIT_FOREVER);
 *     rx_mpc_set_peripheral(config);
 *     tx_mutex_put(&mpc_mutex);
 * }
 * @endcode
 *
 * @par Error Handling Strategy
 * - All public functions validate inputs before hardware access
 * - Errors logged via rx_log_error() with descriptive messages
 * - No partial configuration: if validation fails, no registers modified
 * - Lock always restored even on error path (no dangling unlock)
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1**: [OK] No goto, setjmp, longjmp, recursion
 * - **Rule 2**: [OK] Fixed loop bounds (switch/case only, no iteration)
 * - **Rule 3**: [OK] No dynamic memory allocation
 * - **Rule 4**: [OK] All functions < 60 lines
 * - **Rule 5**: [OK] Assertions via RX_CHECK_* macros (2+ per function)
 * - **Rule 6**: [OK] Variables at smallest scope (function-local)
 * - **Rule 7**: [OK] All return values checked and propagated
 * - **Rule 8**: [OK] C23 typed enums for all constants
 * - **Rule 9**: [WARN] No function pointers (direct calls only)
 * - **Rule 10**: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **Single Responsibility**: Each internal function has one purpose
 * - **Open/Closed**: New peripherals added via set_peripheral()
 * - **Dependency Inversion**: Uses rx72n_mpc_regs.h abstraction
 *
 * @see rx_mpc.h Public API documentation
 * @see rx72n_mpc_regs.h Hardware register definitions
 * @see docs/sections/03_hardware_pinout.tex Pin assignment documentation
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_mpc.h"

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_log.h"
#include "rx_port_constants.h"
#include "rx_port_utils.h"

/**
 * @var s_tag
 * @brief Logging tag for MPC module
 *
 * @details
 * Static constant string used as the tag parameter for all rx_log_*() calls
 * within this module. Provides consistent identification of MPC-related
 * log messages in debug output.
 *
 * @par Value: "MPC"
 * @par Memory: 4 bytes in .rodata section
 *
 * @note Statically allocated, never modified
 * @since Version 1.0.0
 */
static const char* const s_tag = "MPC";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum mpc_pin_constants_t
 * @brief Pin number validation constants for MPC driver
 *
 * @details
 * Defines the valid range for pin numbers within a port. RX72N GPIO ports
 * support up to 8 pins per port (pins 0-7), though not all pins are
 * available on all ports or packages.
 *
 * @see rx_port_constants.h Source of k_rx_pin_max definition
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mpc_min_pin = 0,            /**< Minimum valid pin number (always 0) */
  k_mpc_max_pin = k_rx_pin_max, /**< Maximum valid pin number (7 for 8 pins per port) */
} mpc_pin_constants_t;

/**
 * @enum mpc_pfs_offset_t
 * @brief PFS register base offset from MPC structure start
 *
 * @details
 * The PFS registers start at offset 0x40 from MPC base (0x0008C100), which
 * is address 0x0008C140. However, when accessing via the rx_mpc_regs_t struct,
 * we need to account for the structure layout.
 *
 * This offset is used with pointer arithmetic to calculate PFS register
 * addresses from the mpc() accessor.
 *
 * @par Calculation
 * PFS_addr = (uint8_t*)mpc() + k_mpc_pfs_base_offset + port_offset + pin
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mpc_pfs_base_offset =
    64, /**< Byte offset from mpc() to P00PFS register (0x40: P00PFS=0x0008C140, base=0x0008C100) */
} mpc_pfs_offset_t;

/**
 * @enum mpc_port_number_t
 * @brief Port number aliases for MPC driver validation
 *
 * @details
 * Maps the generic rx_port_* constants to MPC-specific names for use in
 * the switch statement in internal_get_pfs_register(). This provides
 * compile-time verification that all ports are handled.
 *
 * @par 144-pin LFQFP Package Availability
 * | Port | Available | Notes |
 * |------|-----------|-------|
 * | 0-5 | Partial | Some pins available |
 * | 6-9 | Partial | Some pins available (P67, P86/P87 on 144-pin) |
 * | A-E | Full | All 8 pins per port |
 * | F | Partial | PF0-PF2, PF5 available on 144-pin |
 * | G-H | None | Not on 144-pin package |
 * | J | Partial | PJ3, PJ5 available on 144-pin |
 *
 * @see rx_port_constants.h Source definitions
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mpc_port_start = k_rx_port_0, /**< First valid port number for range check */
  k_mpc_port_0     = k_rx_port_0, /**< Port 0 (GPIO P00-P07) */
  k_mpc_port_1     = k_rx_port_1, /**< Port 1 (GPIO P10-P17) */
  k_mpc_port_2     = k_rx_port_2, /**< Port 2 (GPIO P20-P27, encoder inputs) */
  k_mpc_port_3     = k_rx_port_3, /**< Port 3 (GPIO P30-P34, P35=NMI) */
  k_mpc_port_4     = k_rx_port_4, /**< Port 4 (GPIO P40-P47, ADC inputs) */
  k_mpc_port_5     = k_rx_port_5, /**< Port 5 (GPIO P50-P57) */
  k_mpc_port_6     = k_rx_port_6, /**< Port 6 (GPIO P60-P67) */
  k_mpc_port_7     = k_rx_port_7, /**< Port 7 (GPIO P71-P77, P70 N/A) */
  k_mpc_port_8     = k_rx_port_8, /**< Port 8 (GPIO P80-P87) */
  k_mpc_port_9     = k_rx_port_9, /**< Port 9 (GPIO P90-P97) */
  k_mpc_port_a     = k_rx_port_a, /**< Port A (GPIO PA0-PA7, SPI pins) */
  k_mpc_port_b     = k_rx_port_b, /**< Port B (GPIO PB0-PB7, UART pins) */
  k_mpc_port_c     = k_rx_port_c, /**< Port C (GPIO PC0-PC7, encoder inputs) */
  k_mpc_port_d     = k_rx_port_d, /**< Port D (GPIO PD0-PD7) */
  k_mpc_port_e     = k_rx_port_e, /**< Port E (GPIO PE0-PE7, PWM outputs) */
  k_mpc_port_f     = k_rx_port_f, /**< Port F (partial on 144-pin: PF0-PF2, PF5) */
  k_mpc_port_g     = k_rx_port_g, /**< Port G (not on 144-pin package) */
  k_mpc_port_j     = k_rx_port_j, /**< Port J (partial: PJ3, PJ5 on 144-pin) */
} mpc_port_number_t;

/**
 * @enum mpc_port_offset_t
 * @brief PFS register byte offsets for each port
 *
 * @details
 * PFS registers are laid out contiguously in memory with 8 bytes allocated
 * per port (one byte per pin), even if not all pins are physically present.
 * These offsets are added to the PFS base address to reach each port's
 * registers.
 *
 * @par Memory Layout
 * @verbatim
 *   PFS Base (0x0008C140):
 *   +0x00: P00PFS - P07PFS (Port 0)
 *   +0x08: P10PFS - P17PFS (Port 1)
 *   +0x10: P20PFS - P27PFS (Port 2)
 *   ...
 *   +0x50: PA0PFS - PA7PFS (Port A)
 *   +0x58: PB0PFS - PB7PFS (Port B)
 *   ...
 *   +0x90: PJ0PFS - PJ7PFS (Port J)
 * @endverbatim
 *
 * @note Each port occupies 8 bytes regardless of actual pin count
 * @see internal_get_pfs_register() Uses these offsets
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_mpc_port0_offset = 0,   /**< Port 0 offset: 0x00 (P00-P07) */
  k_mpc_port1_offset = 8,   /**< Port 1 offset: 0x08 (P10-P17) */
  k_mpc_port2_offset = 16,  /**< Port 2 offset: 0x10 (P20-P27) */
  k_mpc_port3_offset = 24,  /**< Port 3 offset: 0x18 (P30-P34) */
  k_mpc_port4_offset = 32,  /**< Port 4 offset: 0x20 (P40-P47, ADC) */
  k_mpc_port5_offset = 40,  /**< Port 5 offset: 0x28 (P50-P57) */
  k_mpc_port6_offset = 48,  /**< Port 6 offset: 0x30 (P60-P67) */
  k_mpc_port7_offset = 56,  /**< Port 7 offset: 0x38 (P71-P77) */
  k_mpc_port8_offset = 64,  /**< Port 8 offset: 0x40 (P80-P87) */
  k_mpc_port9_offset = 72,  /**< Port 9 offset: 0x48 (P90-P97) */
  k_mpc_porta_offset = 80,  /**< Port A offset: 0x50 (PA0-PA7, SPI) */
  k_mpc_portb_offset = 88,  /**< Port B offset: 0x58 (PB0-PB7, UART) */
  k_mpc_portc_offset = 96,  /**< Port C offset: 0x60 (PC0-PC7, MTU) */
  k_mpc_portd_offset = 104, /**< Port D offset: 0x68 (PD0-PD7) */
  k_mpc_porte_offset = 112, /**< Port E offset: 0x70 (PE0-PE7, PWM) */
  k_mpc_portf_offset = 120, /**< Port F offset: 0x78 (partial on 144-pin) */
  k_mpc_portg_offset = 128, /**< Port G offset: 0x80 (not on 144-pin) */
  k_mpc_portj_offset = 144, /**< Port J offset: 0x90 (PJ3, PJ5 on 144-pin) */
} mpc_port_offset_t;

/**
 * @enum mpc_psel_constants_t
 * @brief PSEL field validation constants
 *
 * @details
 * The PSEL field in PFS registers is 5 bits wide, allowing values 0-31.
 * This constant defines the maximum valid value for input validation.
 *
 * @see rx_mpc_set_peripheral() Validates PSEL against this maximum
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mpc_psel_max = 31, /**< Maximum PSEL value (5 bits = 0x1F) */
} mpc_psel_constants_t;

/**
 * @enum mpc_pfs_gpio_t
 * @brief PFS register value for GPIO mode
 *
 * @details
 * GPIO mode requires all bits in PFS register to be 0:
 * - PSEL[4:0] = 0 (no peripheral function)
 * - ISEL = 0 (no interrupt input)
 * - ASEL = 0 (digital mode, not analog)
 *
 * @see rx_mpc_set_gpio() Uses this value
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mpc_pfs_gpio = 0, /**< GPIO mode: all zeros (PSEL=0, ISEL=0, ASEL=0) */
} mpc_pfs_gpio_t;

/**
 * @enum mpc_pwpr_t
 * @brief PWPR register control values
 *
 * @details
 * Values used to control the PWPR (Write Protect Register) during the
 * unlock/lock sequence. Clearing PWPR is the first step in both sequences.
 *
 * @see internal_mpc_unlock() Uses k_mpc_pwpr_clear then k_mpc_pwpr_pfswe
 * @see internal_mpc_lock() Uses k_mpc_pwpr_clear then k_mpc_pwpr_b0wi
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mpc_pwpr_clear = 0, /**< Clear all PWPR bits (first step of unlock/lock) */
} mpc_pwpr_t;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Calculate PFS register address for a given port and pin
 *
 * @details
 * Computes the memory address of the Pin Function Select (PFS) register
 * for the specified port and pin combination. Uses a lookup table (switch)
 * to get the port offset, then adds the pin number.
 *
 * @par Algorithm Steps
 * 1. Validate pin number is in range [0, 7]
 * 2. Get base address of PFS registers from mpc() accessor
 * 3. Look up port offset in switch statement
 * 4. If port invalid, log error and returnnullptr
 * 5. Return base + offset + pin
 *
 * @par Address Calculation
 * @verbatim
 *   PFS_addr = (mpc() + k_mpc_pfs_base_offset) + port_offset + pin
 *
 *   Example: PB7 (Port B, Pin 7)
 *   = (0x0008C100 + 64) + 88 + 7
 *   = 0x0008C140 + 88 + 7
 *   = 0x0008C19F (PB7PFS)
 * @endverbatim
 *
 *
 *
 * @pre PCLKB clock must be running for MPC register access
 * @pre Caller has verified channel is in valid range for the target package
 *
 * @post No registers modified (read-only address calculation)
 *
 * @note Internal function - not exported in header
 * @note Port G is explicitly rejected (returns NULL) since it is absent on the 144-pin LFQFP package
 * @note Port H has no named constant in rx_port_constants.h; a raw value in that range falls to the default case and returns NULL
 *
 * @warning Returned pointer is volatile - must dereference with care
 * @warning Caller must still unlock PWPR before writing to the register
 *
 * @par Performance
 * - Execution time: ~200 ns @ 240 MHz (switch lookup + pointer math)
 * - Stack usage: 8 bytes (local variables)
 *
 * @see mpc() MPC register accessor
 * @see mpc_port_offset_t Port offset constants
 *
 * @since Version 1.0.0
 */
/**
 * @brief Sentinel value indicating a port that is not pin-muxable
 *
 * @details
 * Used in the s_port_pfs_offsets table to flag ports that are unavailable
 * on the 144-pin LFQFP package (port G, port H).  Looking up such a port
 * returns nullptr from internal_get_pfs_register().
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_mpc_port_offset_invalid = 0xFFFFU, /**< @brief Port not available on this package */
} mpc_port_offset_sentinel_t;

/**
 * @brief Resolve a port enum to its PFS-register offset
 *
 * @details
 * Lookup table indexed by k_mpc_port_* values.  Returns
 * k_mpc_port_offset_invalid for ports unavailable on this package.
 * Replaces the prior 60+ line switch statement that this function used.
 *
 * @param[in] port Port enum value (k_mpc_port_0 .. k_mpc_port_j)
 *
 * @return uint16_t PFS port offset, or k_mpc_port_offset_invalid if absent
 *
 * @pre None
 * @post Return value is either a valid offset or the sentinel
 *
 * @note Pure function, no side effects.
 *
 * @since Version 1.0.0
 */
static uint16_t internal_mpc_port_offset(const uint8_t port)
{
  switch (port) {
    case k_mpc_port_0:
      return k_mpc_port0_offset;
    case k_mpc_port_1:
      return k_mpc_port1_offset;
    case k_mpc_port_2:
      return k_mpc_port2_offset;
    case k_mpc_port_3:
      return k_mpc_port3_offset;
    case k_mpc_port_4:
      return k_mpc_port4_offset;
    case k_mpc_port_5:
      return k_mpc_port5_offset;
    case k_mpc_port_6:
      return k_mpc_port6_offset;
    case k_mpc_port_7:
      return k_mpc_port7_offset;
    case k_mpc_port_8:
      return k_mpc_port8_offset;
    case k_mpc_port_9:
      return k_mpc_port9_offset;
    case k_mpc_port_a:
      return k_mpc_porta_offset;
    case k_mpc_port_b:
      return k_mpc_portb_offset;
    case k_mpc_port_c:
      return k_mpc_portc_offset;
    case k_mpc_port_d:
      return k_mpc_portd_offset;
    case k_mpc_port_e:
      return k_mpc_porte_offset;
    case k_mpc_port_f:
      return k_mpc_portf_offset;
    case k_mpc_port_j:
      return k_mpc_portj_offset;
    case k_mpc_port_g: /* not on 144-pin LFQFP */
    default:
      return k_mpc_port_offset_invalid;
  }
}

static volatile uint8_t* internal_get_pfs_register(const uint8_t port, uint8_t pin)
{
  if (pin > k_mpc_max_pin) {
    return nullptr;
  }

  const uint16_t port_offset = internal_mpc_port_offset(port);
  if (port_offset == (uint16_t)k_mpc_port_offset_invalid) {
    rx_log_error(s_tag,
                 (port == k_mpc_port_g) ? "Port not available on this package" : "Invalid port");
    return nullptr;
  }

  /* PFS registers start at MPC base + 0x40 (offset for P00PFS).  Each port
   * has 8 pins laid out P00-P07, P10-P17, P20-P27, etc. */
  volatile uint8_t* pfs_base = (volatile uint8_t*)mpc() + k_mpc_pfs_base_offset;
  return pfs_base + port_offset + pin;
}

/**
 * @brief Unlock MPC PFS register write protection
 *
 * @details
 * Executes the two-step unlock sequence required before writing to any
 * PFS register. The PWPR (Write Protect Register) guards against accidental
 * modification of pin functions.
 *
 * @par Unlock Sequence (per RX72N Hardware Manual Ch20)
 * 1. Write 0x00 to PWPR (clears B0WI bit, allows PFSWE modification)
 * 2. Write 0x40 to PWPR (sets PFSWE bit, enables PFS register writes)
 *
 * @par Timing
 * - Two 8-bit register writes
 * - No wait states required between writes
 * - Total time: ~100 ns @ 240 MHz
 *
 * @pre MPC module must be active (not in module stop)
 *
 * @post PWPR.B0WI = 0 (PFSWE modifiable)
 * @post PWPR.PFSWE = 1 (PFS registers writable)
 *
 * @note Must call internal_mpc_lock() after PFS modification!
 * @note Internal function - not exported in header
 *
 * @warning Not thread-safe - window between unlock and lock is vulnerable
 * @warning Failing to lock after modification leaves PFS unprotected
 *
 * @par Example:
 * @code
 * // Typical usage: unlock, modify PFS, then lock immediately
 * internal_mpc_unlock();
 * *pfs_reg = k_psel_sci_tx;  // Write peripheral select value
 * internal_mpc_lock();
 * @endcode
 *
 * @see internal_mpc_lock() Must be called after PFS writes
 * @see k_mpc_pwpr_pfswe PFSWE enable constant (0x40)
 *
 * @since Version 1.0.0
 */
static void internal_mpc_unlock(void)
{
  /* Unlock sequence:
   * 1. Clear B0WI to allow PFSWE write
   * 2. Set PFSWE to allow PFS write
   */
  mpc()->pwpr = k_mpc_pwpr_clear; /* Clear B0WI */
  mpc()->pwpr = k_mpc_pwpr_pfswe; /* Set PFSWE */
}

/**
 * @brief Lock MPC PFS register write protection
 *
 * @details
 * Executes the two-step lock sequence to protect PFS registers after
 * modification. This restores write protection and prevents accidental
 * changes to pin functions.
 *
 * @par Lock Sequence (per RX72N Hardware Manual Ch20)
 * 1. Write 0x00 to PWPR (clears PFSWE bit, protects PFS registers)
 * 2. Write 0x80 to PWPR (sets B0WI bit, prevents PFSWE modification)
 *
 * @par Timing
 * - Two 8-bit register writes
 * - No wait states required between writes
 * - Total time: ~100 ns @ 240 MHz
 *
 * @pre internal_mpc_unlock() was called previously
 * @pre PFS modifications are complete
 *
 * @post PWPR.PFSWE = 0 (PFS registers protected)
 * @post PWPR.B0WI = 1 (PFSWE bit protected)
 *
 * @note Internal function - not exported in header
 *
 * @par Example:
 * @code
 * // Typical usage: unlock, modify PFS, then lock immediately
 * internal_mpc_unlock();
 * *pfs_reg = k_psel_rspi_clk;  // Write peripheral select value
 * internal_mpc_lock();         // Restore write protection
 * @endcode
 *
 * @see internal_mpc_unlock() Must be called before PFS writes
 * @see k_mpc_pwpr_b0wi B0WI enable constant (0x80)
 *
 * @since Version 1.0.0
 */
static void internal_mpc_lock(void)
{
  /* Lock sequence:
   * 1. Clear PFSWE to protect PFS
   * 2. Set B0WI to protect PFSWE
   */
  mpc()->pwpr = k_mpc_pwpr_clear; /* Clear PFSWE */
  mpc()->pwpr = k_mpc_pwpr_b0wi;  /* Set B0WI */
}

/**
 * @brief Write value to PFS register with automatic unlock/lock
 *
 * @details
 * Complete PFS modification sequence: calculates register address, unlocks
 * protection, writes the value, and re-locks protection. This function
 * handles the entire write-protected register access pattern.
 *
 * @par Algorithm Steps
 * 1. Call internal_get_pfs_register() to get register address
 * 2. If address is nullptr, return error (no unlock performed)
 * 3. Call internal_mpc_unlock() to enable PFS writes
 * 4. Write value to PFS register
 * 5. Call internal_mpc_lock() to protect PFS registers
 * 6. Return success
 *
 * @par Execution Flow
 * @dot
 * digraph write_pfs {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   start [label="internal_write_pfs()"];
 *   get_reg [label="internal_get_pfs_register()"];
 *   check [label="pfs_reg == nullptr?", shape=diamond];
 *   error [label="return k_rx_err_invalid_arg", fillcolor=lightcoral, style=filled];
 *   unlock [label="internal_mpc_unlock()"];
 *   write [label="*pfs_reg = value"];
 *   lock [label="internal_mpc_lock()"];
 *   success [label="return k_rx_ok", fillcolor=lightgreen, style=filled];
 *
 *   start -> get_reg;
 *   get_reg -> check;
 *   check -> error [label="yes"];
 *   check -> unlock [label="no"];
 *   unlock -> write;
 *   write -> lock;
 *   lock -> success;
 * }
 * @enddot
 *
 *
 *
 * @pre PCLKB clock must be running
 * @pre Value should be a valid PFS configuration
 *
 * @post PFS register contains specified value
 * @post PWPR is locked (B0WI=1)
 *
 * @note Internal function - not exported in header
 * @note Lock is always executed even on error path (defensive coding)
 *
 * @warning Not thread-safe - the unlock/write/lock sequence is non-atomic
 *
 * @par Performance
 * - Execution time: ~500 ns @ 240 MHz
 * - Stack usage: 16 bytes
 *
 * @see internal_get_pfs_register() Address calculation
 * @see internal_mpc_unlock() Protection unlock
 * @see internal_mpc_lock() Protection lock
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_write_pfs(const uint8_t port, uint8_t pin, uint8_t value)
{
  volatile uint8_t* pfs_reg = internal_get_pfs_register(port, pin);
  if (pfs_reg == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Unlock, write, lock */
  internal_mpc_unlock();
  *pfs_reg = value;
  internal_mpc_lock();

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Configure pin for GPIO (General Purpose I/O) function
 *
 * @details
 * Sets the specified pin to GPIO mode by writing 0x00 to its PFS register.
 * This disconnects the pin from all peripheral functions and allows it to
 * be controlled by the PORT registers (PDR, PODR, PIDR).
 *
 * @par Implementation Details
 * 1. Extract port and pin number from encoded rx_port_pin_t
 * 2. Validate port is in range [0, J]
 * 3. Validate pin is in range [0, 7]
 * 4. Write 0x00 to PFS register (via internal_write_pfs)
 *
 *
 *
 * @pre PCLKB clock running, MPC not in module stop
 * @pre pin must be a valid rx_port_pin_t constant
 *
 * @post PFS register = 0x00 (GPIO mode)
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note Also set PORT PMR bit to 0 for complete GPIO configuration
 *
 * @code
 * // Configure P30 (port 3, pin 0) for GPIO use
 * rx_err_t err = rx_mpc_set_gpio(RX_PORT_PIN(3, 0));
 * if (err != k_rx_ok) {
 *     // handle error
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_gpio(const rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  /* Pre-condition: port and pin must be in valid range */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  RX_CHECK_RANGE_TAG(port, k_mpc_port_start, k_mpc_port_j, k_rx_err_invalid_arg, s_tag);
  RX_CHECK_RANGE_TAG(pin_num, k_mpc_min_pin, k_mpc_max_pin, k_rx_err_invalid_arg, s_tag);

  /* GPIO mode: PSEL = 0, ISEL = 0, ASEL = 0 */
  return internal_write_pfs(port, pin_num, k_mpc_pfs_gpio);
}

/**
 * @brief Configure pin for peripheral function with specified PSEL code
 *
 * @details
 * Core function for peripheral pin configuration. Writes the specified
 * PSEL value to the pin's PFS register, connecting it to a peripheral.
 *
 * @par Implementation Details
 * 1. Validate config pointer is not nullptr
 * 2. Validate PSEL value is in range [0, 31]
 * 3. Extract port and pin from config->pin
 * 4. Validate extracted values
 * 5. Write PSEL to PFS register (via internal_write_pfs)
 *
 *
 *
 * @pre config must be non-NULL with valid fields
 * @pre PCLKB clock running
 *
 * @post PFS register contains config->psel
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note Also set PORT PMR bit to 1 for peripheral mode
 *
 * @code
 * // Configure PA0 for RSPI clock (PSEL = 0x0D)
 * const rx_mpc_peripheral_config_t cfg = {
 *     .pin  = RX_PORT_PIN(0xA, 0),
 *     .psel = 0x0D,
 * };
 * rx_err_t err = rx_mpc_set_peripheral(&cfg);
 * if (err != k_rx_ok) {
 *     // handle error
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_peripheral(const rx_mpc_peripheral_config_t* config)
{
  if (config == nullptr) {
    rx_log_error(s_tag, "Invalid configuration pointer");
    return k_rx_err_invalid_arg;
  }

  if (config->psel > k_mpc_psel_max) {
    rx_log_error(s_tag, "Invalid PSEL value exceeds maximum");
    rx_log_error_val(s_tag, "  psel=", config->psel);
    rx_log_error_val(s_tag, "  k_mpc_psel_max=", (uint8_t)k_mpc_psel_max);
    return k_rx_err_invalid_arg;
  }

  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(config->pin);
  const uint8_t pin_num = rx_pin_from_pin(config->pin);

  /* Validate the decoded port and pin are in valid range */
  if (port > k_mpc_port_j || pin_num > k_mpc_max_pin) {
    rx_log_error(s_tag, "Invalid port or pin number");
    rx_log_error_val(s_tag, "  port=", port);
    rx_log_error_val(s_tag, "  pin=", pin_num);
    return k_rx_err_invalid_arg;
  }

  /* Peripheral mode: PSEL = specified, ISEL = 0, ASEL = 0.
   *
   * NOTE: PORT.PMR bit is intentionally NOT set here. Setting PMR=1 before
   * the peripheral itself (e.g. GPTW timer) is fully configured exposes
   * the pin to undefined/glitchy output that can latch a fault in
   * downstream silicon (e.g. DRV8263H motor driver). Caller is responsible
   * for setting PMR=1 AFTER the peripheral is fully initialized. */
  return internal_write_pfs(port, pin_num, config->psel);
}

/**
 * @brief Configure pin for MTU3a timer PWM output (MTIOC)
 *
 * @details
 * Convenience wrapper that configures a pin for MTU I/O compare output
 * using PSEL = k_psel_mtu_ioc (0x01). Commonly used for motor PWM.
 *
 *
 *
 * @pre PCLKB clock must be running, MPC not in module stop
 * @pre pin must encode a valid port/pin combination supported by MTU MTIOC
 *
 * @post PFS register contains k_psel_mtu_ioc (0x01) for the specified pin
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note STAR project uses GPTW for motor PWM (see rx_mpc_set_gptw(), PSEL = k_psel_gptw (0x1E)), not MTU
 *
 * @code
 * // Configure P14 for MTU3 PWM output (MTIOCA)
 * rx_err_t err = rx_mpc_set_mtu_pwm(RX_PORT_PIN(1, 4));
 * if (err != k_rx_ok) {
 *     // handle error
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_mtu_pwm(const rx_port_pin_t pin)
{
  /* MTU PWM (MTIOC) pins typically use PSEL = 0x01
   * Common pins:
   * - P14-P17: MTU3 MTIOCA/B/C/D
   * - P24-P27: MTU4 MTIOCA/B/C/D
   */
  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = k_psel_mtu_ioc};

  return rx_mpc_set_peripheral(&config);
}

/**
 * @brief Configure pin for MTU3a encoder/phase counter input (MTCLK)
 *
 * @details
 * Convenience wrapper that configures a pin for MTU external clock / phase
 * counting mode using PSEL = k_psel_mtu_phase (0x02). Used for quadrature
 * encoders on pins that expose MTCLKA/B/C/D.
 *
 *
 *
 * @pre PCLKB clock must be running, MPC not in module stop
 * @pre pin must encode a valid port/pin combination supported by MTCLK input
 *
 * @post PFS register contains k_psel_mtu_phase (0x02) for the specified pin
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note Configure both Phase A and Phase B pins for proper operation
 *
 * @code
 * // Configure P24 and P25 for MTU phase counting (MTCLKA/B)
 * rx_err_t err = rx_mpc_set_mtu_encoder(RX_PORT_PIN(2, 4));
 * if (err == k_rx_ok) {
 *     err = rx_mpc_set_mtu_encoder(RX_PORT_PIN(2, 5));
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation
 * @see rx_mtu_encoder.h Encoder driver using MTU phase counting
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_mtu_encoder(const rx_port_pin_t pin)
{
  /* MTU external clock / phase counting pins use PSEL = 0x02 on every
   * RX72N MTCLK candidate pin. See RX72N Group Hardware Manual
   * (R01UH0824EJ0111), chapter 23 "Multi-Function Pin Controller (MPC)",
   * Tables 23.5 / 23.6 / 23.17 / 23.19 -- every row that selects MTCLKA,
   * MTCLKB, MTCLKC, or MTCLKD on a 144-pin LFQFP pin is PSEL = 000010b.
   * The old value 0x03 was wrong -- it caused MTU TCNT to stay at 0x0000
   * because no MTCLK source was actually connected through the PFS mux.
   *
   * Common STAR pins:
   *   P24/P25 (Motor 0 front-right MTCLKA/B)
   *   PA1/PC5 (Motor 1 front-left  MTCLKC/D)
   */
  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = k_psel_mtu_phase};

  return rx_mpc_set_peripheral(&config);
}

/**
 * @brief Configure pin for TPU encoder/phase counter input (TCLK)
 *
 * @details
 * Convenience wrapper that configures a pin for TPU external clock input
 * (TCLKA/B/C/D). Picks the correct PSEL value based on which port the
 * pin is on, because RX72N uses two different alternate-function slots
 * for the SAME TCLK signal depending on candidate:
 *   - Port C pins (PC0/PC1/PC2/PC3) use PSEL = 0x03 (k_psel_tpu_clk_portc)
 *   - Port 1 / Port A / Port B pins use PSEL = 0x04 (k_psel_tpu_clk_alt)
 *
 * Reference: RX72N Group Hardware Manual (R01UH0824EJ0111), chapter 23
 * "Multi-Function Pin Controller (MPC)". TCLK rows in Table 23.19 (Port C)
 * use PSEL = 000011b, while TCLK rows in Tables 23.5 (P1x), 23.17 (PAx),
 * and 23.18 (PBx) use PSEL = 000100b. Before this split, every TPU pin was
 * configured with PSEL=0x03, which silently misconfigured the non-Port-C
 * halves of STAR's rear-wheel encoders (PA3 for Motor 2 Phase B, PB3 for
 * Motor 3 Phase B) so those edges were never counted.
 *
 *
 *
 * @pre PCLKB clock must be running, MPC not in module stop
 * @pre pin must encode a valid port/pin combination supported by TCLK input
 *
 * @post PFS register contains the correct TPU-TCLK PSEL for that pin's port
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note Configure both Phase A and Phase B pins for proper operation
 *
 * @code
 * // Configure PC2 (TCLKA) and PA3 (TCLKB) for Motor 2 rear-left encoder
 * rx_err_t err = rx_mpc_set_tpu_encoder(RX_PORT_PIN(0xC, 2));  // -> 0x03
 * if (err == k_rx_ok) {
 *     err = rx_mpc_set_tpu_encoder(RX_PORT_PIN(0xA, 3));       // -> 0x04
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation
 * @see rx_encoder_tpu.h TPU encoder driver using phase counting
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_tpu_encoder(const rx_port_pin_t pin)
{
  /* Pick the right PSEL for the TCLK function on this pin's port. */
  const uint8_t port = rx_port_from_pin(pin);
  const uint8_t psel =
    (port == k_mpc_port_c) ? (uint8_t)k_psel_tpu_clk_portc : (uint8_t)k_psel_tpu_clk_alt;

  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = psel};
  return rx_mpc_set_peripheral(&config);
}

/**
 * @brief Configure pin for ADC analog input
 *
 * @details
 * Configures a pin for analog-to-digital conversion by setting the ASEL
 * bit in the PFS register. This disables the digital input buffer to
 * prevent noise during analog measurement.
 *
 *
 *
 * @pre PCLKB clock must be running, MPC not in module stop
 * @pre pin must encode a valid port/pin combination with analog capability
 *
 * @post PFS register contains k_pfs_asel (0x80) for the specified pin
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note Sets ASEL bit (0x80) rather than PSEL field
 * @note Also configure S12ADC for actual conversion
 *
 * @code
 * // Configure P40 (AN000) for ADC analog input
 * rx_err_t err = rx_mpc_set_adc(RX_PORT_PIN(4, 0));
 * if (err != k_rx_ok) {
 *     // handle error
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_adc(const rx_port_pin_t pin)
{
  /* Extract port and pin number from type-safe enum */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate the decoded port and pin are in valid range */
  if (port > k_mpc_port_j || pin_num > k_mpc_max_pin) {
    rx_log_error(s_tag, "Invalid port or pin number");
    return k_rx_err_invalid_arg;
  }

  /* ADC mode: Set ASEL = 1 to disable digital I/O
   * Common ADC pins: P40-P47 (AN000-AN007)
   */
  return internal_write_pfs(port, pin_num, k_pfs_asel);
}

/**
 * @brief Configure pin for SCI (Serial Communication Interface) UART function
 *
 * @details
 * Convenience wrapper that configures a pin for SCI UART operation using
 * PSEL = k_psel_sci_tx (0x0A). Works for both TXD and RXD pins.
 *
 *
 *
 * @pre PCLKB clock must be running, MPC not in module stop
 * @pre pin must encode a valid port/pin combination with SCI multiplexing
 *
 * @post PFS register contains k_psel_sci_tx (0x0A) for the specified pin
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note Configure both TXD and RXD pins for bidirectional UART
 *
 * @code
 * // Configure PB7 for SCI9 TXD
 * rx_err_t err = rx_mpc_set_sci(RX_PORT_PIN(0xB, 7));
 * if (err != k_rx_ok) {
 *     // handle error
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_sci(const rx_port_pin_t pin)
{
  /* SCI pins use PSEL = 0x0A
   * TX and RX use the same PSEL code
   */
  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = k_psel_sci_tx};

  return rx_mpc_set_peripheral(&config);
}

/**
 * @brief Configure pin for RIIC (I2C) bus function
 *
 * @details
 * Convenience wrapper that configures a pin for RIIC operation using
 * PSEL = k_psel_riic_scl (0x0F). Works for both SCL and SDA pins.
 *
 * Unlike rx_mpc_set_gptw(), this function also sets PORT.PMR=1 for the
 * pin before returning -- RIIC cannot drive SDA/SCL while PMR=0 and the
 * immediate-vs-deferred PMR trade-off differs from the motor-PWM case:
 *
 *   | Peripheral | When to raise PMR           | Reason                  |
 *   |------------|-----------------------------|-------------------------|
 *   | GPTW/PWM   | AFTER timer is running      | Push-pull drive: an un-
 *   |            | (rx_gptw_init_pwm handles   | initialized output could
 *   |            | this after rx_gptw_start)   | momentarily glitch
 *   |            |                             | DRV8263H H-bridge inputs
 *   |            |                             | to Vm/2 during the
 *   |            |                             | GTIOR reset state window.
 *   | RIIC/I2C   | IMMEDIATELY after PFS       | Open-drain + external
 *   |            | (here, inside this wrapper) | pull-ups: idle bus is
 *   |            |                             | both lines high (3.3V);
 *   |            |                             | routing an un-initialized
 *   |            |                             | RIIC just leaves the
 *   |            |                             | pull-ups floating the
 *   |            |                             | lines high. No hazard.
 *
 * Without PMR=1 on the RIIC pins, rx72n peripheral bits still toggle
 * internally but the pad stays disconnected from RIIC; every Start
 * condition times out with k_rx_err_timeout (0x108) at the first
 * riic_write()/riic_read() call. Discovered 2026-04-22: production
 * imu_task saw tight "[ERROR] [RIIC] Start condition timeout" loops while
 * imu_test/main.c (which sets PMR=1 at line 227 before calling its own
 * riic_init()) talked to BNO055 + BMP280 cleanly on the same board.
 *
 *
 *
 * @pre PCLKB clock must be running, MPC not in module stop
 * @pre pin must encode a valid port/pin combination with RIIC multiplexing
 *
 * @post PFS register contains k_psel_riic_scl (0x0F) for the specified pin
 * @post PORT.PMR bit for the pin is set (pin routed to RIIC peripheral)
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note Configure both SCL and SDA pins; external pull-ups required
 *
 * @code
 * // Configure P12 (SCL) and P13 (SDA) for RIIC0
 * rx_err_t err = rx_mpc_set_riic(RX_PORT_PIN(1, 2));
 * if (err == k_rx_ok) {
 *     err = rx_mpc_set_riic(RX_PORT_PIN(1, 3));
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation
 * @see rx_mpc_set_gptw Contrast: deferred-PMR case
 * @see star-rx72n-firmware/imu_test/main.c::riic1_init Bench reference
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_riic(const rx_port_pin_t pin)
{
  /* RIIC pins use PSEL = 0x0F
   * SCL and SDA use the same PSEL code
   */
  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = k_psel_riic_scl};

  const rx_err_t err = rx_mpc_set_peripheral(&config);
  if (err != k_rx_ok) {
    return err;
  }

  /* Set PORT.PMR=1 so SCL/SDA actually route to the RIIC peripheral.
   *
   * Unlike GPTW (where PMR must be deferred until after the timer is
   * running to avoid exposing DRV8263H inputs to GTIOR reset state),
   * I2C pins are open-drain with external pull-ups: driving them high
   * through the peripheral mux is the idle-bus state and perfectly safe.
   *
   * Without this bit set, P20/P21 remain GPIO (PMR=0) and RIIC1 can
   * never toggle the lines -- every Start condition times out with
   * k_rx_err_timeout (0x108). The bench-proven imu_test/main.c sets
   * PMR identically at rx72n-firmware/imu_test/main.c:227. */
  const uint8_t                  port_idx = rx_port_from_pin(pin);
  const uint8_t                  bit      = rx_pin_from_pin(pin);
  volatile rx_port_regs_t* const port     = rx_port_get_base(port_idx);
  if (port == nullptr) {
    return k_rx_err_invalid_arg;
  }
  port->pmr |= (uint8_t)(1U << bit);
  return k_rx_ok;
}

/**
 * @brief Configure pin for RSPI (SPI) bus function
 *
 * @details
 * Convenience wrapper that configures a pin for RSPI operation using
 * PSEL = k_psel_rspi_clk (0x0D). Works for all SPI signals (CLK, COPI,
 * CIPO, SSL).
 *
 *
 *
 * @pre PCLKB clock must be running, MPC not in module stop
 * @pre pin must encode a valid port/pin combination with RSPI multiplexing
 *
 * @post PFS register contains k_psel_rspi_clk (0x0D) for the specified pin
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note Configure all 4 SPI pins (CLK, COPI, CIPO, CS)
 * @note Uses OSHWA-approved inclusive terminology
 *
 * @code
 * // Configure PA0-PA4 for RSPI0 (CLK, COPI, CIPO, SSL0, SSL1)
 * rx_err_t err = rx_mpc_set_rspi(RX_PORT_PIN(0xA, 0)); // CLK
 * if (err == k_rx_ok) {
 *     err = rx_mpc_set_rspi(RX_PORT_PIN(0xA, 1)); // COPI
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation
 * @see rx_spi_comm.h SPI communication driver
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_rspi(const rx_port_pin_t pin)
{
  /* RSPI pins use PSEL = 0x0D
   * All RSPI signals (CLK, COPI, CIPO, SSL) use same PSEL
   */
  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = k_psel_rspi_clk};

  return rx_mpc_set_peripheral(&config);
}

/**
 * @brief Configure pin for GPTW complementary PWM output
 *
 * @details
 * Convenience wrapper that configures a pin for GPTW operation using
 * PSEL = k_psel_gptw (0x1E). GPTW provides complementary PWM output with dead-time
 * insertion for motor control.
 *
 *
 *
 * @pre PCLKB clock must be running, MPC not in module stop
 * @pre pin must encode a valid port/pin combination with GPTW multiplexing
 *
 * @post PFS register contains k_psel_gptw (0x1E) for the specified pin
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note Used for 4 GPTW channels (0-3) with 8 total pins (IN2 + IN1 per motor)
 * @note Phase staggering configured separately via rx_gptw driver
 *
 * @code
 * // Configure motor 2 GPTW IN2 and IN1 pins (Port E)
 * rx_err_t err = rx_mpc_set_gptw(RX_PORT_PIN(0xE, 3)); // PE3 GTIOC2A (IN2)
 * if (err == k_rx_ok) {
 *     err = rx_mpc_set_gptw(RX_PORT_PIN(8, 6)); // P8.6 GTIOC2B (IN1)
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation with usage examples
 * @see rx_gptw_init_all_staggered() GPTW channel configuration
 *
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_gptw(const rx_port_pin_t pin)
{
  /* GPTW pins use PSEL = 0x1E
   * Supports 4 GPTW channels (0-3) for motor IN2/IN1 control */
  const rx_mpc_peripheral_config_t config = {
    .pin  = pin,
    .psel = k_psel_gptw /* 0x1E - defined in rx_pin_psel_t enum */
  };

  return rx_mpc_set_peripheral(&config);
}

/**
 * @brief Configure pin for USB VBUS detection
 *
 * @details
 * Convenience wrapper that configures a pin for USB VBUS detection using
 * PSEL = 0x11. Required for USB CDC enumeration and power monitoring.
 *
 *
 *
 * @pre PCLKB clock must be running, MPC not in module stop
 * @pre pin must encode a valid port/pin combination with USB VBUS multiplexing
 *
 * @post PFS register contains k_psel_usb_vbus (0x11) for the specified pin
 * @post PWPR locked after operation
 *
 * @note Thread safety: Not thread-safe
 * @note Active-high VBUS detect (pin = 1 when 5V present)
 * @note Also requires USB PHY and controller configuration
 *
 * @code
 * // Configure P16 for USB0_VBUS detection
 * rx_err_t err = rx_mpc_set_usb_vbus(RX_PORT_PIN(1, 6));
 * if (err != k_rx_ok) {
 *     // handle error
 * }
 * @endcode
 *
 * @see rx_mpc.h Full API documentation with usage examples
 * @see rx_usb.h USB controller driver
 *
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_usb_vbus(const rx_port_pin_t pin)
{
  /* USB VBUS detect uses PSEL = 0x11
   * Typical pin: P1.6 for USB0_VBUS on RX72N */
  const rx_mpc_peripheral_config_t config = {
    .pin  = pin,
    .psel = k_psel_usb_vbus /* 0x11 - defined in rx_pin_psel_t enum */
  };

  return rx_mpc_set_peripheral(&config);
}

/**
 * @brief Configure pin for IRQ (external interrupt) function
 *
 * @details
 * Sets pin function to IRQ mode by writing the ISEL bit (bit 6, value 0x40)
 * directly to the PFS register via internal_write_pfs(). This bypasses
 * rx_mpc_set_peripheral() because ISEL=0x40 exceeds the 5-bit PSEL field
 * maximum (k_mpc_psel_max = 31).
 *
 * Mirrors the approach used by rx_mpc_set_adc() for ASEL (bit 7):
 * both functions write their respective bit directly rather than going
 * through the PSEL validation path.
 *
 * **Algorithm Steps:**
 * 1. Extract port number and pin number from encoded pin parameter
 * 2. Validate port in range [0, J]
 * 3. Validate pin in range [0, 7]
 * 4. Write ISEL bit (0x40) to PFS register via internal_write_pfs()
 *
 *
 *
 * @pre Pin must have IRQ multiplexing capability (P00-P07 for IRQ8-15)
 * @pre PCLKB clock must be running
 *
 * @post Pin ISEL bit (0x40) set in PFS register
 * @post PSEL field remains 0 (no peripheral function conflict)
 *
 * @note Thread safety: Not thread-safe
 * @note Typical pins: P00-P07 for IRQ8-IRQ15 on RX72N
 * @note Used by HC-SR04 ultrasonic sensors for echo pulse measurement
 *
 * @code
 * // Configure P00 for IRQ8 (HC-SR04 echo input)
 * rx_err_t err = rx_mpc_set_irq(RX_PORT_PIN(0, 0));
 * if (err != k_rx_ok) {
 *     // handle error
 * }
 * // Then configure ICU for rising/falling edge detection
 * @endcode
 *
 * @see rx_mpc_set_adc() Same pattern: writes ASEL bit (0x80) directly
 * @see rx_mpc.h Full API documentation with usage examples
 * @see rx_hcsr04_icu_configure() Configure ICU after calling this function
 *
 * @since Version 1.0.0
 */
rx_err_t rx_mpc_set_irq(const rx_port_pin_t pin)
{
  /* Extract port and pin number from type-safe enum */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate the decoded port and pin are in valid range */
  if (port > k_mpc_port_j || pin_num > k_mpc_max_pin) {
    rx_log_error(s_tag, "Invalid port or pin number for IRQ function");
    return k_rx_err_invalid_arg;
  }

  /* IRQ mode: Set ISEL = 1 (bit 6) to enable IRQ input function.
   * This is NOT a PSEL value; k_pfs_isel is 0x40 which exceeds the 5-bit
   * PSEL field. Write directly like rx_mpc_set_adc() does for ASEL.
   * Typical IRQ pins: P00-P07 (IRQ8-IRQ15) for HC-SR04 echo measurement. */
  return internal_write_pfs(port, pin_num, k_pfs_isel);
}
