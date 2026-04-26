/**
 * @file rx72n_wdt_regs.h
 * @brief RX72N WDT Watchdog Timer Register Definitions
 *
 * @details
 * Register definitions for the Watchdog Timer (WDT) module on the RX72N
 * microcontroller. The WDT provides system monitoring to detect software
 * runaway conditions and reset the system if necessary.
 *
 * @par System Architecture
 * @dot
 * digraph wdt_architecture {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   pclkb [label="PCLKB\n(60 MHz)"];
 *   divider [label="Clock Divider\n(4 to 8192)"];
 *   counter [label="14-bit\nDown Counter"];
 *   window [label="Window\nComparator"];
 *   action [label="Action\n(Reset/NMI)"];
 *
 *   sw [label="Software\nRefresh"];
 *
 *   pclkb -> divider;
 *   divider -> counter;
 *   counter -> window;
 *   window -> action [label="timeout\nor early"];
 *   sw -> counter [label="WDTRR\n0x00->0xFF"];
 * }
 * @enddot
 *
 * @par WDT vs IWDT Comparison
 * | Feature           | WDT (This Module)        | IWDT                    |
 * |-------------------|--------------------------|-------------------------|
 * | Clock Source      | PCLKB (60 MHz)           | Dedicated 120 kHz       |
 * | Stop in Software  | Yes                      | No (once started)       |
 * | Debug/Sleep Ctrl  | No                       | Yes (IWDTCSTPR)         |
 * | Best For          | Development, debugging   | Production safety       |
 * | Timeout Range     | 4 us to 2.2 s            | 8.5 ms to 17.5 s        |
 *
 * @par Key Features
 * - 14-bit down counter using PCLKB peripheral clock
 * - Configurable timeout period: 1024 to 16384 cycles
 * - Configurable clock division: 4 to 8192
 * - Optional window protection to detect too-early refresh
 * - Can be stopped in software (unlike IWDT)
 * - Generates reset or NMI on timeout (configurable)
 *
 * @par Timeout Calculation
 * @f[
 *   T_{timeout} = \frac{TOPS \times CKS}{f_{PCLKB}}
 * @f]
 *
 * Example with PCLKB = 60 MHz, TOPS = 16384, CKS = 8192:
 * @f[
 *   T_{timeout} = \frac{16384 \times 8192}{60,000,000} = 2.24 \text{ seconds}
 * @f]
 *
 * @par Timeout Period Table (PCLKB = 60 MHz)
 * | TOPS   | CKS/4    | CKS/64   | CKS/128  | CKS/8192 |
 * |--------|----------|----------|----------|----------|
 * | 1024   | 17 us    | 1.1 ms   | 2.2 ms   | 140 ms   |
 * | 4096   | 68 us    | 4.4 ms   | 8.7 ms   | 559 ms   |
 * | 8192   | 137 us   | 8.7 ms   | 17.5 ms  | 1.12 s   |
 * | 16384  | 273 us   | 17.5 ms  | 34.9 ms  | 2.24 s   |
 *
 * @par Window Protection
 * The WDT supports window protection to detect both late refresh (timeout)
 * and early refresh (software error). The refresh window is defined by:
 * - **RPSS**: Window start position (when refresh becomes allowed)
 * - **RPES**: Window end position (when refresh becomes prohibited)
 *
 * @startuml
 * robust "Refresh Window" as RW
 * robust "Counter" as CNT
 *
 * @0
 * CNT is "Max (16384)"
 * RW is "Prohibited"
 *
 * @100
 * RW is "Allowed (RPSS=75%)"
 *
 * @250
 * RW is "Prohibited (RPES=50%)"
 *
 * @400
 * CNT is "0 (Timeout)"
 * @enduml
 *
 * @par Hardware Requirements
 * | Parameter        | Value        | Notes                              |
 * |------------------|--------------|-----------------------------------|
 * | Clock Source     | PCLKB        | 60 MHz in STAR configuration      |
 * | Base Address     | 0x00088020   | 8-byte register block             |
 * | Interrupt Vector | WDT_WOVI     | Watchdog overflow (NMI mode)      |
 * | Register Count   | 4            | WDTRR, WDTCR, WDTSR, WDTRCR       |
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] N/A (no loops in register definitions)
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] All accessor functions are single-statement
 * - Rule 5: [OK] N/A (hardware layer)
 * - Rule 6: [OK] Minimal scope
 * - Rule 7: [OK] N/A (no return values to check)
 * - Rule 8: [OK] All constants use C23 typed enums
 * - Rule 9: [OK] No function pointers
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single responsibility - only WDT register definitions
 * - **O**: Open for extension via additional bit field enums
 * - **L**: N/A (no inheritance hierarchy)
 * - **I**: Minimal interface - only necessary accessor
 * - **D**: N/A (hardware register layer)
 *
 * @par Module Dependencies
 * - `<stddef.h>` - offsetof macro for static assertions
 * - `<stdint.h>` - Fixed-width integer types
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 34: Watchdog Timer (WDT), pages 1547-1558
 *
 * @see rx72n_iwdt_regs.h Independent Watchdog Timer (production use)
 * @see rx_wdt.h Higher-level WDT API
 * @see rx72n_regs.h Main register include file
 *
 * @author Locked, Inc.
 * @date 2026-01-28
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Watchdog Timer (WDT)
 * =============================================================================
 */

/**
 * @enum rx_wdt_addresses_t
 * @brief WDT base address constant
 *
 * @details
 * Base address for the Watchdog Timer register block at 0x00088020.
 * The WDT has an 8-byte register space with 4 registers and 2 reserved bytes.
 *
 * @par Memory Map
 * | Address      | Register | Size | Description                       |
 * |--------------|----------|------|-----------------------------------|
 * | 0x00088020   | WDTRR    | 1    | Refresh Register                  |
 * | 0x00088021   | Reserved | 1    | -                                 |
 * | 0x00088022   | WDTCR    | 2    | Control Register                  |
 * | 0x00088024   | WDTSR    | 2    | Status Register                   |
 * | 0x00088026   | WDTRCR   | 1    | Reset Control Register            |
 * | 0x00088027   | Reserved | 1    | -                                 |
 *
 * @see wdt() Accessor function
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /**
   * @brief WDT register base address (0x00088020)
   * @details Verified against RX72N Hardware Manual Ch34
   */
  k_wdt_base_addr = 0x00088020,
} rx_wdt_addresses_t;

/**
 * @enum wdt_reserved_sizes_t
 * @brief WDT register reserved field sizes
 * @details Defines reserved byte counts for structure padding to match hardware layout.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_wdt_reserved_after_wdtrr_bytes  = 1, /**< Reserved byte after WDTRR @ 0x01 */
  k_wdt_reserved_after_wdtrcr_bytes = 1, /**< Reserved byte after WDTRCR @ 0x07 */
} wdt_reserved_sizes_t;

/**
 * @struct rx_wdt_regs_t
 * @brief WDT Register Map Structure
 *
 * @details
 * Watchdog Timer (WDT) registers for software-controllable watchdog functionality.
 * Uses PCLKB clock and can be stopped in software (unlike IWDT).
 *
 * @par Memory Layout Table
 * | Offset | Size | Field     | Type     | Description                          |
 * |--------|------|-----------|----------|--------------------------------------|
 * | 0x00   | 1    | wdtrr     | uint8_t  | Refresh Register                     |
 * | 0x01   | 1    | reserved0 | -        | Reserved                             |
 * | 0x02   | 2    | wdtcr     | uint16_t | Control Register                     |
 * | 0x04   | 2    | wdtsr     | uint16_t | Status Register                      |
 * | 0x06   | 1    | wdtrcr    | uint8_t  | Reset Control Register               |
 * | 0x07   | 1    | reserved1 | -        | Reserved                             |
 * | **Total** | **8** |        |          | (packed, no additional padding)      |
 *
 * @par WDT Refresh Sequence
 * The watchdog is refreshed by writing a specific sequence to WDTRR:
 * 1. Write 0x00 to WDTRR
 * 2. Write 0xFF to WDTRR
 *
 * This two-step sequence prevents accidental refresh from single-byte errors.
 *
 * @par Initialization Example
 * @code{.c}
 * // Configure WDT for 2-second timeout with window protection
 * volatile rx_wdt_regs_t* wdt_regs = wdt();
 *
 * // 1. Configure control register (before starting)
 * //    TOPS=16384 cycles, CKS=div8192, RPSS=100%, RPES=0%
 * wdt_regs->wdtcr = k_wdt_tops_16384 | k_wdt_cks_div_8192 |
 *                   k_wdt_rpss_100 | k_wdt_rpes_0;
 *
 * // 2. Configure to generate reset on timeout
 * wdt_regs->wdtrcr = k_wdt_rstirqs_reset;
 *
 * // 3. Start WDT by performing first refresh
 * wdt_regs->wdtrr = k_wdt_refresh_start;  // Write 0x00
 * wdt_regs->wdtrr = k_wdt_refresh_end;    // Write 0xFF
 * @endcode
 *
 * @par Refresh Example (in main loop)
 * @code{.c}
 * void app_main_loop(void) {
 *     while (1) {
 *         // Perform application tasks
 *         process_commands();
 *         update_motors();
 *
 *         // Refresh watchdog (must complete both writes atomically)
 *         wdt()->wdtrr = k_wdt_refresh_start;  // 0x00
 *         wdt()->wdtrr = k_wdt_refresh_end;    // 0xFF
 *
 *         // Wait for next cycle
 *         tx_thread_sleep(10);
 *     }
 * }
 * @endcode
 *
 * @note WDT has 4 registers only. WDTCSTPR (count stop) exists for IWDT but NOT for WDT.
 *
 * @invariant sizeof(rx_wdt_regs_t) == 8 bytes
 * @invariant All offsets match RX72N Hardware Manual Ch34
 *
 * @warning Refresh must complete both writes (0x00 then 0xFF) atomically
 * @warning Do not refresh outside the valid window if window protection is enabled
 *
 * @see wdt() Accessor function
 * @see wdt_refresh_sequence_t Refresh values
 * @see wdt_wdtcr_bits_t Control register bits
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed)) {
  /**
   * @brief WDT Refresh Register (WDTRR) @ offset 0x00
   * @details Write 0x00 then 0xFF to refresh the watchdog counter.
   * Both writes must complete within the valid window if window protection is enabled.
   * @see wdt_refresh_sequence_t Refresh values
   * @warning Single byte writes are invalid - must write both values in sequence
   */
  volatile uint8_t wdtrr;

  uint8_t reserved0[k_wdt_reserved_after_wdtrr_bytes]; /**< Reserved @ 0x01 */

  /**
   * @brief WDT Control Register (WDTCR) @ offset 0x02
   * @details Configures timeout period, clock division, and window parameters.
   * Must be configured before starting the WDT (before first refresh).
   * @par Bit Fields:
   * - Bits 1:0 (TOPS): Timeout period selection
   * - Bits 7:4 (CKS): Clock division ratio
   * - Bits 9:8 (RPES): Window end position
   * - Bits 13:12 (RPSS): Window start position
   * @see wdt_wdtcr_bits_t Bit definitions
   */
  volatile uint16_t wdtcr;

  /**
   * @brief WDT Status Register (WDTSR) @ offset 0x04
   * @details Contains current counter value and error flags.
   * @par Bit Fields:
   * - Bits 13:0 (CNTVAL): Current down-counter value (read-only)
   * - Bit 14 (UNDFF): Underflow flag (timeout occurred)
   * - Bit 15 (REFEF): Refresh error flag (window violation)
   * @see wdt_wdtsr_bits_t Bit definitions
   */
  volatile uint16_t wdtsr;

  /**
   * @brief WDT Reset Control Register (WDTRCR) @ offset 0x06
   * @details Selects action on timeout: reset or NMI.
   * @par Bit 7 (RSTIRQS):
   * - 0: Generate NMI on timeout
   * - 1: Generate internal reset on timeout
   * @see wdt_wdtrcr_bits_t Bit definitions
   */
  volatile uint8_t wdtrcr;

  uint8_t reserved1[k_wdt_reserved_after_wdtrcr_bytes]; /**< Reserved @ 0x07 */
} rx_wdt_regs_t;

/**
 * @brief Get pointer to WDT registers
 *
 * @details
 * Returns a volatile pointer to the WDT register structure at address
 * 0x00088020. The WDT is a software-controllable watchdog timer that
 * uses the PCLKB peripheral clock.
 *
 * @return Volatile pointer to WDT register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre None (hardware register always accessible)
 * @post Pointer is valid for the lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 * @note This function is always inlined for zero overhead
 *
 * @par Example:
 * @code{.c}
 * // Refresh watchdog
 * wdt()->wdtrr = k_wdt_refresh_start;
 * wdt()->wdtrr = k_wdt_refresh_end;
 *
 * // Check counter value
 * uint16_t counter = wdt()->wdtsr & k_wdt_sr_cntval_mask;
 * @endcode
 *
 * @see k_wdt_base_addr Base address constant
 * @see rx_wdt_init() Higher-level initialization
 * @since Version 1.0.0
 */
static inline volatile rx_wdt_regs_t* wdt(void)
{
  return (volatile rx_wdt_regs_t*)k_wdt_base_addr;
}

/**
 * @enum wdt_refresh_sequence_t
 * @brief WDT Refresh Register (WDTRR) Write Sequence Values
 *
 * @details
 * The WDT counter is refreshed by writing a specific two-byte sequence:
 * 1. Write 0x00 (k_wdt_refresh_start)
 * 2. Write 0xFF (k_wdt_refresh_end)
 *
 * This sequence must complete within the valid refresh window if window
 * protection is enabled, otherwise a refresh error (REFEF) will occur.
 *
 * @par Refresh Example
 * @code{.c}
 * // Refresh watchdog (atomic - disable interrupts if needed)
 * wdt()->wdtrr = k_wdt_refresh_start;  // Step 1: Write 0x00
 * wdt()->wdtrr = k_wdt_refresh_end;    // Step 2: Write 0xFF
 * @endcode
 *
 * @warning Both writes must complete in sequence - partial refresh is invalid
 * @warning Do not insert other operations between the two writes
 * @see rx_wdt_regs_t::wdtrr Refresh register
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief First write value for refresh sequence (0x00)
   * @details Must be written first, followed immediately by k_wdt_refresh_end.
   */
  k_wdt_refresh_start = 0x00,

  /**
   * @brief Second write value for refresh sequence (0xFF)
   * @details Must be written immediately after k_wdt_refresh_start.
   * Counter reloads to initial value on completion.
   */
  k_wdt_refresh_end = 0xFF,
} wdt_refresh_sequence_t;

/**
 * @enum wdt_wdtcr_pos_t
 * @brief WDT Control Register (WDTCR) Bit Field Positions
 *
 * @details
 * Bit positions for WDTCR register fields. Used to construct field values.
 *
 * @par Register Layout (16-bit)
 * | Bits   | Field | Description                        |
 * |--------|-------|------------------------------------|
 * | 1:0    | TOPS  | Timeout period selection           |
 * | 3:2    | -     | Reserved                           |
 * | 7:4    | CKS   | Clock division ratio               |
 * | 9:8    | RPES  | Window end position                |
 * | 11:10  | -     | Reserved                           |
 * | 13:12  | RPSS  | Window start position              |
 * | 15:14  | -     | Reserved                           |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_wdt_cr_cks_pos  = 4,  /**< CKS field position (bits 7:4) */
  k_wdt_cr_rpes_pos = 8,  /**< RPES field position (bits 9:8) */
  k_wdt_cr_rpss_pos = 12, /**< RPSS field position (bits 13:12) */
} wdt_wdtcr_pos_t;

/**
 * @enum wdt_wdtcr_bits_t
 * @brief WDT Control Register (WDTCR) Bit Field Values
 *
 * @details
 * Configuration values for WDT timeout period, clock division, and
 * window protection. These values can be OR'd together to configure WDTCR.
 *
 * @par Timeout Period Calculation
 * @f[
 *   T_{timeout} = \frac{TOPS \times CKS}{f_{PCLKB}}
 * @f]
 *
 * @par Timeout Examples (PCLKB = 60 MHz)
 * | Configuration                          | Timeout  |
 * |----------------------------------------|----------|
 * | TOPS_1024 + CKS_DIV_4                  | 17 us    |
 * | TOPS_16384 + CKS_DIV_8192              | 2.24 s   |
 * | TOPS_4096 + CKS_DIV_2048               | 140 ms   |
 *
 * @par Window Protection
 * Window protection prevents both late refresh (timeout) and early refresh
 * (software error). Configure RPSS for start and RPES for end of window.
 *
 * @par Configuration Example
 * @code{.c}
 * // 2-second timeout, no window protection
 * wdt()->wdtcr = k_wdt_tops_16384 | k_wdt_cks_div_8192 |
 *                k_wdt_rpss_100 | k_wdt_rpes_0;
 *
 * // 140ms timeout with 25%-75% refresh window
 * wdt()->wdtcr = k_wdt_tops_4096 | k_wdt_cks_div_2048 |
 *                k_wdt_rpss_75 | k_wdt_rpes_25;
 * @endcode
 *
 * @warning Only 6 CKS settings are valid - others are prohibited
 * @see rx_wdt_regs_t::wdtcr Control register
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* =========================================================================
   * Timeout Period Select (TOPS) - bits 1:0
   * Counter cycles before timeout (14-bit counter max values)
   * ========================================================================= */

  /**
   * @brief 1024 cycles timeout (counter loads 0x03FF)
   * @details Shortest timeout period. Use with larger CKS for practical timeouts.
   */
  k_wdt_tops_1024 = 0x0000,

  /**
   * @brief 4096 cycles timeout (counter loads 0x0FFF)
   */
  k_wdt_tops_4096 = 0x0001,

  /**
   * @brief 8192 cycles timeout (counter loads 0x1FFF)
   */
  k_wdt_tops_8192 = 0x0002,

  /**
   * @brief 16384 cycles timeout (counter loads 0x3FFF)
   * @details Longest timeout period. Combined with CKS_DIV_8192 gives ~2.24s at 60MHz.
   */
  k_wdt_tops_16384 = 0x0003,

  /* =========================================================================
   * Clock Division Ratio (CKS) - bits 7:4
   * PCLKB divided by this value feeds the counter
   * NOTE: Only 6 valid settings per Manual Table 34.2
   * ========================================================================= */

  /**
   * @brief PCLKB divided by 4 (fastest, shortest timeout)
   * @details CKS[3:0] = 0001. At 60 MHz: 15 MHz counter clock.
   */
  k_wdt_cks_div_4 = (0x01 << k_wdt_cr_cks_pos),

  /**
   * @brief PCLKB divided by 64
   * @details CKS[3:0] = 0100. At 60 MHz: 937.5 kHz counter clock.
   */
  k_wdt_cks_div_64 = (0x04 << k_wdt_cr_cks_pos),

  /**
   * @brief PCLKB divided by 128
   * @details CKS[3:0] = 1111. At 60 MHz: 468.75 kHz counter clock.
   */
  k_wdt_cks_div_128 = (0x0F << k_wdt_cr_cks_pos),

  /**
   * @brief PCLKB divided by 512
   * @details CKS[3:0] = 0110. At 60 MHz: 117.19 kHz counter clock.
   */
  k_wdt_cks_div_512 = (0x06 << k_wdt_cr_cks_pos),

  /**
   * @brief PCLKB divided by 2048
   * @details CKS[3:0] = 0111. At 60 MHz: 29.3 kHz counter clock.
   */
  k_wdt_cks_div_2048 = (0x07 << k_wdt_cr_cks_pos),

  /**
   * @brief PCLKB divided by 8192 (slowest, longest timeout)
   * @details CKS[3:0] = 1000. At 60 MHz: 7.32 kHz counter clock.
   */
  k_wdt_cks_div_8192 = (0x08 << k_wdt_cr_cks_pos),

  /* =========================================================================
   * Window End Position (RPES) - bits 9:8
   * Specifies when refresh window CLOSES (refresh becomes prohibited)
   * Value = counter percentage below which refresh is not allowed
   * ========================================================================= */

  /**
   * @brief Window closes at 75% counter remaining
   * @details Refresh prohibited when counter < 75% of initial value.
   * Provides early detection of runaway code.
   */
  k_wdt_rpes_75 = (0x00 << k_wdt_cr_rpes_pos),

  /**
   * @brief Window closes at 50% counter remaining
   */
  k_wdt_rpes_50 = (0x01 << k_wdt_cr_rpes_pos),

  /**
   * @brief Window closes at 25% counter remaining
   */
  k_wdt_rpes_25 = (0x02 << k_wdt_cr_rpes_pos),

  /**
   * @brief No window end (refresh always allowed until timeout)
   * @details Use with RPSS_100 to disable window protection entirely.
   */
  k_wdt_rpes_0 = (0x03 << k_wdt_cr_rpes_pos),

  /* =========================================================================
   * Window Start Position (RPSS) - bits 13:12
   * Specifies when refresh window OPENS (refresh becomes allowed)
   * Value = counter percentage below which refresh is allowed
   * ========================================================================= */

  /**
   * @brief Window opens at 25% counter remaining
   * @details Refresh allowed only when counter < 25% of initial value.
   * Strictest early-refresh detection.
   */
  k_wdt_rpss_25 = (0x00 << k_wdt_cr_rpss_pos),

  /**
   * @brief Window opens at 50% counter remaining
   */
  k_wdt_rpss_50 = (0x01 << k_wdt_cr_rpss_pos),

  /**
   * @brief Window opens at 75% counter remaining
   */
  k_wdt_rpss_75 = (0x02 << k_wdt_cr_rpss_pos),

  /**
   * @brief No window start (refresh allowed immediately)
   * @details Use with RPES_0 to disable window protection entirely.
   */
  k_wdt_rpss_100 = (0x03 << k_wdt_cr_rpss_pos),
} wdt_wdtcr_bits_t;

/**
 * @enum wdt_wdtsr_pos_t
 * @brief WDT Status Register (WDTSR) Bit Field Positions
 * @details Bit positions for status flags in the WDTSR register.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_wdt_sr_undff_pos = 14, /**< Underflow flag bit position */
  k_wdt_sr_refef_pos = 15, /**< Refresh error flag bit position */
} wdt_wdtsr_pos_t;

/**
 * @enum wdt_wdtsr_bits_t
 * @brief WDT Status Register (WDTSR) Bit Field Values
 *
 * @details
 * Contains the current down-counter value and error flags. The counter value
 * can be read at any time; flags indicate timeout or window violation.
 *
 * @par Register Layout (16-bit)
 * | Bits  | Field  | R/W | Description                           |
 * |-------|--------|-----|---------------------------------------|
 * | 13:0  | CNTVAL | R   | Current counter value                 |
 * | 14    | UNDFF  | R/W | Underflow flag (write 0 to clear)     |
 * | 15    | REFEF  | R/W | Refresh error flag (write 0 to clear) |
 *
 * @par Reading Counter Example
 * @code{.c}
 * // Get current counter value
 * uint16_t counter = wdt()->wdtsr & k_wdt_sr_cntval_mask;
 *
 * // Calculate remaining time (example with CKS_DIV_8192, PCLKB=60MHz)
 * // Time per count = 8192 / 60MHz = 136.5 us
 * float remaining_ms = counter * 0.1365f;
 * @endcode
 *
 * @par Error Checking Example
 * @code{.c}
 * // Check for errors after reset
 * uint16_t status = wdt()->wdtsr;
 * if (status & k_wdt_sr_undff) {
 *     // Watchdog timeout occurred (system was reset)
 *     rx_log_warn("WDT", "Watchdog reset detected");
 *     wdt()->wdtsr = ~k_wdt_sr_undff;  // Clear flag
 * }
 * if (status & k_wdt_sr_refef) {
 *     // Window violation occurred
 *     rx_log_error("WDT", "Watchdog window violation");
 *     wdt()->wdtsr = ~k_wdt_sr_refef;  // Clear flag
 * }
 * @endcode
 *
 * @see rx_wdt_regs_t::wdtsr Status register
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Counter value mask (bits 13:0)
   * @details Extracts 14-bit down-counter value. Value decrements from
   * initial (TOPS value - 1) toward 0. Refresh reloads to initial value.
   */
  k_wdt_sr_cntval_mask = 0x3FFF,

  /**
   * @brief Underflow flag - Bit 14
   * @details Set when counter reaches 0 (timeout). In reset mode, this flag
   * indicates the reset was caused by WDT. Write 0 to clear.
   */
  k_wdt_sr_undff = (1U << k_wdt_sr_undff_pos),

  /**
   * @brief Refresh error flag - Bit 15
   * @details Set when refresh occurs outside the valid window (too early).
   * Indicates software error - code refreshed before window opened.
   * Write 0 to clear.
   */
  k_wdt_sr_refef = (1U << k_wdt_sr_refef_pos),
} wdt_wdtsr_bits_t;

/**
 * @enum wdt_wdtrcr_bits_t
 * @brief WDT Reset Control Register (WDTRCR) Bit Field Values
 *
 * @details
 * Controls the action taken on WDT timeout or window violation.
 * Choice between generating an internal reset or Non-Maskable Interrupt (NMI).
 *
 * @par Register Layout (8-bit)
 * | Bit | Field   | R/W | Description                            |
 * |-----|---------|-----|----------------------------------------|
 * | 6:0 | -       | -   | Reserved                               |
 * | 7   | RSTIRQS | R/W | 0=NMI, 1=Reset on timeout              |
 *
 * @par Mode Selection
 * - **Reset Mode (k_wdt_rstirqs_reset)**: Recommended for production.
 *   Ensures system recovers even if NMI handler is corrupted.
 * - **NMI Mode (k_wdt_rstirqs_nmi)**: Useful for debugging.
 *   Allows saving diagnostic info before reset.
 *
 * @par Configuration Example
 * @code{.c}
 * // Production: Generate reset on timeout
 * wdt()->wdtrcr = k_wdt_rstirqs_reset;
 *
 * // Debug: Generate NMI to capture state before reset
 * wdt()->wdtrcr = k_wdt_rstirqs_nmi;
 * @endcode
 *
 * @see rx_wdt_regs_t::wdtrcr Reset control register
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief RSTIRQS bit position
   */
  k_wdt_rcr_rstirqs_pos = 7,

  /**
   * @brief RSTIRQS bit mask
   */
  k_wdt_rcr_rstirqs_mask = (1U << k_wdt_rcr_rstirqs_pos),

  /**
   * @brief Generate internal reset on timeout (RSTIRQS=1)
   * @details Recommended for production. System resets automatically
   * when watchdog times out, ensuring recovery.
   */
  k_wdt_rstirqs_reset = (1U << k_wdt_rcr_rstirqs_pos),

  /**
   * @brief Generate NMI on timeout (RSTIRQS=0)
   * @details Useful for debugging. NMI handler can log diagnostics
   * before manually resetting. Requires NMI handler implementation.
   */
  k_wdt_rstirqs_nmi = 0x00,
} wdt_wdtrcr_bits_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* Verify base address matches Hardware Manual */
static_assert(k_wdt_base_addr == 0x00088020, "WDT base address incorrect");

/* Verify register structure layout (WDT has 4 registers, 8 bytes total) */
static_assert(sizeof(rx_wdt_regs_t) == 8, "WDT register structure size incorrect");
static_assert(offsetof(rx_wdt_regs_t, wdtrr) == 0x00, "WDTRR offset incorrect");
static_assert(offsetof(rx_wdt_regs_t, wdtcr) == 0x02, "WDTCR offset incorrect");
static_assert(offsetof(rx_wdt_regs_t, wdtsr) == 0x04, "WDTSR offset incorrect");
static_assert(offsetof(rx_wdt_regs_t, wdtrcr) == 0x06, "WDTRCR offset incorrect");

#ifdef __cplusplus
}
#endif
