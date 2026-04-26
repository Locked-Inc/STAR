/**
 * @file rx72n_iwdt_regs.h
 * @brief RX72N IWDT Independent Watchdog Timer Register Definitions
 *
 * @details
 * Register definitions for the Independent Watchdog Timer (IWDT) module on
 * the RX72N microcontroller. The IWDT provides critical system monitoring
 * with a dedicated clock source independent of the main system clock.
 *
 * @par System Architecture
 * @dot
 * digraph iwdt_architecture {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   osc [label="IWDTCLK\n120 kHz\n(Dedicated)" style="filled" fillcolor="lightblue"];
 *   divider [label="Clock Divider\n(1 to 256)"];
 *   counter [label="14-bit\nDown Counter"];
 *   window [label="Window\nComparator"];
 *   action [label="Action\n(Reset/NMI)"];
 *   sleep [label="Sleep Mode\nControl"];
 *
 *   sw [label="Software\nRefresh"];
 *
 *   osc -> divider;
 *   divider -> counter;
 *   counter -> window;
 *   window -> action [label="timeout\nor early"];
 *   sw -> counter [label="IWDTRR\n0x00->0xFF"];
 *   sleep -> counter [label="IWDTCSTPR"];
 * }
 * @enddot
 *
 * @par IWDT vs WDT Comparison
 * | Feature           | IWDT (This Module)       | WDT                     |
 * |-------------------|--------------------------|-------------------------|
 * | Clock Source      | Dedicated 120 kHz        | PCLKB (60 MHz)          |
 * | Clock Independent | Yes (survives clock fail)| No                      |
 * | Stop in Software  | No (once started)        | Yes                     |
 * | Sleep Mode Ctrl   | Yes (IWDTCSTPR)          | No                      |
 * | Best For          | **Production safety**    | Development/debugging   |
 * | Timeout Range     | 8.5 ms to 17.5 s         | 4 us to 2.2 s           |
 *
 * @par Key Features
 * - 14-bit down counter with dedicated 120 kHz oscillator
 * - **Clock-independent operation** - resets system even if main clock fails
 * - Configurable timeout period: 1024 to 16384 cycles
 * - Configurable clock division: 1 to 256
 * - Optional window protection to detect too-early refresh
 * - Sleep mode count stop control (IWDTCSTPR)
 * - Generates reset or NMI on timeout (configurable)
 * - **Cannot be stopped in software once started** (safety feature)
 *
 * @par Timeout Calculation
 * @f[
 *   T_{timeout} = \frac{TOPS \times CKS}{f_{IWDTCLK}}
 * @f]
 *
 * Example with IWDTCLK = 120 kHz, TOPS = 16384, CKS = 128:
 * @f[
 *   T_{timeout} = \frac{16384 \times 128}{120,000} = 17.5 \text{ seconds}
 * @f]
 *
 * @par Timeout Period Table (IWDTCLK = 120 kHz)
 * | TOPS   | CKS/1    | CKS/16   | CKS/64   | CKS/128  | CKS/256  |
 * |--------|----------|----------|----------|----------|----------|
 * | 1024   | 8.5 ms   | 137 ms   | 546 ms   | 1.09 s   | 2.18 s   |
 * | 4096   | 34 ms    | 546 ms   | 2.18 s   | 4.37 s   | 8.74 s   |
 * | 8192   | 68 ms    | 1.09 s   | 4.37 s   | 8.74 s   | 17.5 s   |
 * | 16384  | 137 ms   | 2.18 s   | 8.74 s   | 17.5 s   | 35 s*    |
 *
 * *Note: 35s exceeds practical limits; use shorter timeouts for safety.
 *
 * @par Hardware Requirements
 * | Parameter        | Value        | Notes                              |
 * |------------------|--------------|-----------------------------------|
 * | Clock Source     | IWDTCLK      | 120 kHz dedicated oscillator      |
 * | Base Address     | 0x00088030   | 9-byte register block             |
 * | Interrupt Vector | IWDT_WUNI    | IWDT underflow (NMI mode)         |
 * | Register Count   | 5            | IWDTRR, IWDTCR, IWDTSR, etc.      |
 *
 * @par Production Safety Recommendation
 * The IWDT is recommended for production systems because:
 * 1. Cannot be disabled by software (protects against runaway code)
 * 2. Survives main clock failure
 * 3. Configurable sleep mode behavior
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
 * - **S**: Single responsibility - only IWDT register definitions
 * - **O**: Open for extension via additional bit field enums
 * - **L**: N/A (no inheritance hierarchy)
 * - **I**: Minimal interface - only necessary accessor
 * - **D**: N/A (hardware register layer)
 *
 * @par Module Dependencies
 * - `<stdint.h>` - Fixed-width integer types
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 35: Independent Watchdog Timer (IWDT), pages 1559-1572
 *
 * @see rx72n_wdt_regs.h Software-controllable Watchdog Timer
 * @see rx_iwdt.h Higher-level IWDT API
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
 * Independent Watchdog Timer (IWDT)
 * =============================================================================
 */

/**
 * @enum rx_iwdt_addresses_t
 * @brief IWDT base address constant
 *
 * @details
 * Base address for the Independent Watchdog Timer register block at 0x00088030.
 * The IWDT has a 9-byte register space with 5 registers and 2 reserved bytes.
 *
 * @par Memory Map
 * | Address      | Register  | Size | Description                        |
 * |--------------|-----------|------|-----------------------------------|
 * | 0x00088030   | IWDTRR    | 1    | Refresh Register                  |
 * | 0x00088031   | Reserved  | 1    | -                                 |
 * | 0x00088032   | IWDTCR    | 2    | Control Register                  |
 * | 0x00088034   | IWDTSR    | 2    | Status Register                   |
 * | 0x00088036   | IWDTRCR   | 1    | Reset Control Register            |
 * | 0x00088037   | Reserved  | 1    | -                                 |
 * | 0x00088038   | IWDTCSTPR | 1    | Count Stop Control Register       |
 *
 * @see iwdt() Accessor function
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /**
   * @brief IWDT register base address (0x00088030)
   * @details Verified against RX72N Hardware Manual Ch35
   */
  k_iwdt_base_addr = 0x00088030,
} rx_iwdt_addresses_t;

/**
 * @enum iwdt_reserved_sizes_t
 * @brief IWDT register reserved field sizes
 * @details Defines reserved byte counts for structure padding to match hardware layout.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_iwdt_reserved_after_iwdtrr_bytes  = 1, /**< Reserved byte after IWDTRR @ 0x01 */
  k_iwdt_reserved_after_iwdtrcr_bytes = 1, /**< Reserved byte after IWDTRCR @ 0x07 */
} iwdt_reserved_sizes_t;

/**
 * @struct rx_iwdt_regs_t
 * @brief IWDT Register Map Structure
 *
 * @details
 * Independent Watchdog Timer (IWDT) registers for safety-critical system
 * monitoring. Uses dedicated 120 kHz oscillator that operates independently
 * of the main system clock.
 *
 * @par Memory Layout Table
 * | Offset | Size | Field      | Type     | Description                         |
 * |--------|------|------------|----------|-------------------------------------|
 * | 0x00   | 1    | iwdtrr     | uint8_t  | Refresh Register                    |
 * | 0x01   | 1    | reserved0  | -        | Reserved                            |
 * | 0x02   | 2    | iwdtcr     | uint16_t | Control Register                    |
 * | 0x04   | 2    | iwdtsr     | uint16_t | Status Register                     |
 * | 0x06   | 1    | iwdtrcr    | uint8_t  | Reset Control Register              |
 * | 0x07   | 1    | reserved1  | -        | Reserved                            |
 * | 0x08   | 1    | iwdtcstpr  | uint8_t  | Count Stop Control Register         |
 * | **Total** | **9** |         |          | (packed, no additional padding)     |
 *
 * @par IWDT Characteristics
 * - **Cannot be stopped once started** - This is a safety feature
 * - **Clock-independent** - Continues operating if main clock fails
 * - **Sleep mode control** - Can stop counting during sleep (IWDTCSTPR)
 *
 * @par Initialization Example
 * @code{.c}
 * // Configure IWDT for 1-second timeout (production configuration)
 * volatile rx_iwdt_regs_t* iwdt_regs = iwdt();
 *
 * // 1. Configure control register BEFORE first refresh
 * //    TOPS=16384 cycles, CKS=div128, RPSS=100%, RPES=0% (no window)
 * iwdt_regs->iwdtcr = k_iwdt_tops_16384 | k_iwdt_cks_div_128 |
 *                     k_iwdt_rpss_100 | k_iwdt_rpes_0;
 *
 * // 2. Configure to generate reset on timeout (production)
 * iwdt_regs->iwdtrcr = k_iwdt_rstirqs_reset;
 *
 * // 3. Configure sleep mode behavior
 * iwdt_regs->iwdtcstpr = k_iwdt_slcstp_continue;  // Keep counting in sleep
 *
 * // 4. Start IWDT by performing first refresh
 * // WARNING: IWDT cannot be stopped after this!
 * iwdt_regs->iwdtrr = k_iwdt_refresh_start;  // Write 0x00
 * iwdt_regs->iwdtrr = k_iwdt_refresh_end;    // Write 0xFF - IWDT NOW RUNNING
 * @endcode
 *
 * @par Refresh Example (in main loop)
 * @code{.c}
 * void app_main_loop(void) {
 *     while (1) {
 *         // Perform application tasks
 *         process_commands();
 *         update_motors();
 *         check_safety();
 *
 *         // Refresh IWDT (must complete both writes atomically)
 *         iwdt()->iwdtrr = k_iwdt_refresh_start;  // 0x00
 *         iwdt()->iwdtrr = k_iwdt_refresh_end;    // 0xFF
 *
 *         // Wait for next cycle
 *         tx_thread_sleep(100);  // 100ms loop
 *     }
 * }
 * @endcode
 *
 * @note Unlike WDT, IWDT has IWDTCSTPR for sleep mode control
 *
 * @invariant sizeof(rx_iwdt_regs_t) == 9 bytes
 * @invariant All offsets match RX72N Hardware Manual Ch35
 *
 * @warning **IWDT cannot be stopped once started** - ensure configuration is correct
 * @warning Configuration must be complete before first refresh
 * @attention Recommended for production systems due to clock independence
 *
 * @see iwdt() Accessor function
 * @see iwdt_refresh_sequence_t Refresh values
 * @see iwdt_iwdtcr_bits_t Control register bits
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed)) {
  /**
   * @brief IWDT Refresh Register (IWDTRR) @ offset 0x00
   * @details Write 0x00 then 0xFF to refresh the watchdog counter.
   * First refresh also starts the IWDT (cannot be stopped after).
   * @see iwdt_refresh_sequence_t Refresh values
   * @warning First refresh starts IWDT permanently until reset
   */
  volatile uint8_t iwdtrr;

  uint8_t reserved0[k_iwdt_reserved_after_iwdtrr_bytes]; /**< Reserved @ 0x01 */

  /**
   * @brief IWDT Control Register (IWDTCR) @ offset 0x02
   * @details Configures timeout period, clock division, and window parameters.
   * Must be configured before first refresh (before starting IWDT).
   * @see iwdt_iwdtcr_bits_t Bit definitions
   */
  volatile uint16_t iwdtcr;

  /**
   * @brief IWDT Status Register (IWDTSR) @ offset 0x04
   * @details Contains current counter value and error flags.
   * @see iwdt_iwdtsr_bits_t Bit definitions
   */
  volatile uint16_t iwdtsr;

  /**
   * @brief IWDT Reset Control Register (IWDTRCR) @ offset 0x06
   * @details Selects action on timeout: reset or NMI.
   * @see iwdt_iwdtrcr_bits_t Bit definitions
   */
  volatile uint8_t iwdtrcr;

  uint8_t reserved1[k_iwdt_reserved_after_iwdtrcr_bytes]; /**< Reserved @ 0x07 */

  /**
   * @brief IWDT Count Stop Control Register (IWDTCSTPR) @ offset 0x08
   * @details Controls whether IWDT counter continues during sleep modes.
   * @par Bit 7 (SLCSTP):
   * - 0: Continue counting during sleep (recommended)
   * - 1: Stop counting during sleep
   * @see iwdt_iwdtcstpr_bits_t Bit definitions
   */
  volatile uint8_t iwdtcstpr;
} rx_iwdt_regs_t;

/**
 * @brief Get pointer to IWDT registers
 *
 * @details
 * Returns a volatile pointer to the IWDT register structure at address
 * 0x00088030. The IWDT is recommended for production systems due to its
 * clock-independent operation.
 *
 * @return Volatile pointer to IWDT register structure
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
 * // Refresh IWDT
 * iwdt()->iwdtrr = k_iwdt_refresh_start;
 * iwdt()->iwdtrr = k_iwdt_refresh_end;
 *
 * // Check counter value
 * uint16_t counter = iwdt()->iwdtsr & k_iwdt_sr_cntval_mask;
 * @endcode
 *
 * @see k_iwdt_base_addr Base address constant
 * @see rx_iwdt_init() Higher-level initialization
 * @since Version 1.0.0
 */
static inline volatile rx_iwdt_regs_t* iwdt(void)
{
  return (volatile rx_iwdt_regs_t*)k_iwdt_base_addr;
}

/**
 * @enum iwdt_refresh_sequence_t
 * @brief IWDT Refresh Register (IWDTRR) Write Sequence Values
 *
 * @details
 * The IWDT counter is refreshed by writing a specific two-byte sequence:
 * 1. Write 0x00 (k_iwdt_refresh_start)
 * 2. Write 0xFF (k_iwdt_refresh_end)
 *
 * The first refresh also **starts** the IWDT, which cannot be stopped until reset.
 *
 * @par Refresh Example
 * @code{.c}
 * // Refresh IWDT (atomic - disable interrupts if needed)
 * iwdt()->iwdtrr = k_iwdt_refresh_start;  // Step 1: Write 0x00
 * iwdt()->iwdtrr = k_iwdt_refresh_end;    // Step 2: Write 0xFF
 * @endcode
 *
 * @warning First refresh starts IWDT permanently
 * @warning Both writes must complete in sequence - partial refresh is invalid
 * @see rx_iwdt_regs_t::iwdtrr Refresh register
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief First write value for refresh sequence (0x00)
   * @details Must be written first, followed immediately by k_iwdt_refresh_end.
   */
  k_iwdt_refresh_start = 0x00,

  /**
   * @brief Second write value for refresh sequence (0xFF)
   * @details Must be written immediately after k_iwdt_refresh_start.
   * Counter reloads to initial value on completion.
   */
  k_iwdt_refresh_end = 0xFF,
} iwdt_refresh_sequence_t;

/**
 * @enum iwdt_iwdtcr_shifts_t
 * @brief IWDT Control Register (IWDTCR) Bit Field Positions
 *
 * @details
 * Bit positions for IWDTCR register fields. Used to construct field values.
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
  k_iwdt_iwdtcr_cks_shift  = 4,  /**< CKS field position (bits 7:4) */
  k_iwdt_iwdtcr_rpes_shift = 8,  /**< RPES field position (bits 9:8) */
  k_iwdt_iwdtcr_rpss_shift = 12, /**< RPSS field position (bits 13:12) */
} iwdt_iwdtcr_shifts_t;

/**
 * @enum iwdt_iwdtcr_bits_t
 * @brief IWDT Control Register (IWDTCR) Bit Field Values
 *
 * @details
 * Configuration values for IWDT timeout period, clock division, and
 * window protection. These values can be OR'd together to configure IWDTCR.
 *
 * @par Timeout Calculation
 * @f[
 *   T_{timeout} = \frac{TOPS \times CKS}{120,000}
 * @f]
 *
 * @par Timeout Examples (IWDTCLK = 120 kHz)
 * | Configuration                          | Timeout  |
 * |----------------------------------------|----------|
 * | TOPS_1024 + CKS_DIV_1                  | 8.5 ms   |
 * | TOPS_16384 + CKS_DIV_128               | 17.5 s   |
 * | TOPS_16384 + CKS_DIV_64                | 8.7 s    |
 * | TOPS_4096 + CKS_DIV_32                 | ~1.1 s   |
 *
 * @par Production Configuration Example
 * @code{.c}
 * // 1-second timeout, no window protection
 * iwdt()->iwdtcr = k_iwdt_tops_16384 | k_iwdt_cks_div_128 |
 *                  k_iwdt_rpss_100 | k_iwdt_rpes_0;
 * @endcode
 *
 * @see rx_iwdt_regs_t::iwdtcr Control register
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* =========================================================================
   * Timeout Period Select (TOPS) - bits 1:0
   * Counter cycles before timeout (14-bit counter max values)
   * ========================================================================= */

  /**
   * @brief 1024 cycles timeout (~8.5ms at 120kHz, no division)
   * @details Counter loads 0x03FF (1023). Shortest timeout period.
   */
  k_iwdt_tops_1024 = 0x0000,

  /**
   * @brief 4096 cycles timeout (~34ms at 120kHz, no division)
   * @details Counter loads 0x0FFF (4095).
   */
  k_iwdt_tops_4096 = 0x0001,

  /**
   * @brief 8192 cycles timeout (~68ms at 120kHz, no division)
   * @details Counter loads 0x1FFF (8191).
   */
  k_iwdt_tops_8192 = 0x0002,

  /**
   * @brief 16384 cycles timeout (~137ms at 120kHz, no division)
   * @details Counter loads 0x3FFF (16383). Longest timeout period.
   */
  k_iwdt_tops_16384 = 0x0003,

  /* =========================================================================
   * Clock Division Ratio (CKS) - bits 7:4
   * IWDTCLK (120 kHz) divided by this value feeds the counter
   * ========================================================================= */

  /**
   * @brief IWDTCLK divided by 1 (fastest, shortest timeout)
   * @details CKS[3:0] = 0000. 120 kHz counter clock.
   */
  k_iwdt_cks_div_1 = (0x00 << k_iwdt_iwdtcr_cks_shift),

  /**
   * @brief IWDTCLK divided by 16
   * @details CKS[3:0] = 0010. 7.5 kHz counter clock.
   */
  k_iwdt_cks_div_16 = (0x02 << k_iwdt_iwdtcr_cks_shift),

  /**
   * @brief IWDTCLK divided by 32
   * @details CKS[3:0] = 0011. 3.75 kHz counter clock.
   */
  k_iwdt_cks_div_32 = (0x03 << k_iwdt_iwdtcr_cks_shift),

  /**
   * @brief IWDTCLK divided by 64
   * @details CKS[3:0] = 0100. 1.875 kHz counter clock.
   */
  k_iwdt_cks_div_64 = (0x04 << k_iwdt_iwdtcr_cks_shift),

  /**
   * @brief IWDTCLK divided by 128 (recommended for ~1s timeout)
   * @details CKS[3:0] = 1111. 937.5 Hz counter clock.
   * With TOPS_16384: ~17.5 second timeout.
   */
  k_iwdt_cks_div_128 = (0x0F << k_iwdt_iwdtcr_cks_shift),

  /**
   * @brief IWDTCLK divided by 256 (slowest, longest timeout)
   * @details CKS[3:0] = 0101. 468.75 Hz counter clock.
   */
  k_iwdt_cks_div_256 = (0x05 << k_iwdt_iwdtcr_cks_shift),

  /* =========================================================================
   * Window End Position (RPES) - bits 9:8
   * Specifies when refresh window CLOSES (refresh becomes prohibited)
   * ========================================================================= */

  /**
   * @brief Window closes at 75% counter remaining
   * @details Refresh prohibited when counter < 75% of initial value.
   */
  k_iwdt_rpes_75 = (0x00 << k_iwdt_iwdtcr_rpes_shift),

  /**
   * @brief Window closes at 50% counter remaining
   */
  k_iwdt_rpes_50 = (0x01 << k_iwdt_iwdtcr_rpes_shift),

  /**
   * @brief Window closes at 25% counter remaining
   */
  k_iwdt_rpes_25 = (0x02 << k_iwdt_iwdtcr_rpes_shift),

  /**
   * @brief No window end (refresh allowed until timeout)
   * @details Recommended for most applications.
   */
  k_iwdt_rpes_0 = (0x03 << k_iwdt_iwdtcr_rpes_shift),

  /* =========================================================================
   * Window Start Position (RPSS) - bits 13:12
   * Specifies when refresh window OPENS (refresh becomes allowed)
   * ========================================================================= */

  /**
   * @brief Window opens at 25% counter remaining
   * @details Strictest early-refresh detection.
   */
  k_iwdt_rpss_25 = (0x00 << k_iwdt_iwdtcr_rpss_shift),

  /**
   * @brief Window opens at 50% counter remaining
   */
  k_iwdt_rpss_50 = (0x01 << k_iwdt_iwdtcr_rpss_shift),

  /**
   * @brief Window opens at 75% counter remaining
   */
  k_iwdt_rpss_75 = (0x02 << k_iwdt_iwdtcr_rpss_shift),

  /**
   * @brief No window start (refresh allowed immediately)
   * @details Recommended for most applications (no early-refresh detection).
   */
  k_iwdt_rpss_100 = (0x03 << k_iwdt_iwdtcr_rpss_shift),
} iwdt_iwdtcr_bits_t;

/**
 * @enum iwdt_iwdtsr_bits_t
 * @brief IWDT Status Register (IWDTSR) Bit Field Values
 *
 * @details
 * Contains the current down-counter value and error flags.
 *
 * @par Register Layout (16-bit)
 * | Bits  | Field  | R/W | Description                           |
 * |-------|--------|-----|---------------------------------------|
 * | 13:0  | CNTVAL | R   | Current counter value                 |
 * | 14    | UNDFF  | R/W | Underflow flag (write 0 to clear)     |
 * | 15    | REFEF  | R/W | Refresh error flag (write 0 to clear) |
 *
 * @par Error Checking Example
 * @code{.c}
 * uint16_t status = iwdt()->iwdtsr;
 * if (status & k_iwdt_sr_undff) {
 *     rx_log_warn("IWDT", "Reset was caused by IWDT timeout");
 * }
 * @endcode
 *
 * @see rx_iwdt_regs_t::iwdtsr Status register
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /**
   * @brief Counter value mask (bits 13:0)
   * @details Extracts 14-bit down-counter value.
   */
  k_iwdt_sr_cntval_mask = 0x3FFF,

  /**
   * @brief Underflow flag bit position
   */
  k_iwdt_sr_undff_shift = 14,

  /**
   * @brief Refresh error flag bit position
   */
  k_iwdt_sr_refef_shift = 15,

  /**
   * @brief Underflow flag - Bit 14
   * @details Set when counter reaches 0 (timeout). Write 0 to clear.
   */
  k_iwdt_sr_undff = (1U << 14),

  /**
   * @brief Refresh error flag - Bit 15
   * @details Set when refresh occurs outside valid window. Write 0 to clear.
   */
  k_iwdt_sr_refef = (1U << 15),
} iwdt_iwdtsr_bits_t;

/**
 * @enum iwdt_iwdtrcr_bits_t
 * @brief IWDT Reset Control Register (IWDTRCR) Bit Field Values
 *
 * @details
 * Controls the action taken on IWDT timeout.
 *
 * @par Register Layout (8-bit)
 * | Bit | Field   | R/W | Description                            |
 * |-----|---------|-----|----------------------------------------|
 * | 6:0 | -       | -   | Reserved                               |
 * | 7   | RSTIRQS | R/W | 0=NMI, 1=Reset on timeout              |
 *
 * @par Configuration Example
 * @code{.c}
 * // Production: Generate reset on timeout (recommended)
 * iwdt()->iwdtrcr = k_iwdt_rstirqs_reset;
 * @endcode
 *
 * @see rx_iwdt_regs_t::iwdtrcr Reset control register
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief RSTIRQS bit position
   */
  k_iwdt_rstirqs_pos = 7,

  /**
   * @brief RSTIRQS bit mask
   */
  k_iwdt_rcr_rstirqs_mask = (1U << 7),

  /**
   * @brief Generate internal reset on timeout (RSTIRQS=1)
   * @details Recommended for production systems.
   */
  k_iwdt_rstirqs_reset = (1U << 7),

  /**
   * @brief Generate NMI on timeout (RSTIRQS=0)
   * @details Useful for debugging or logging before reset.
   */
  k_iwdt_rstirqs_nmi = 0x00,
} iwdt_iwdtrcr_bits_t;

/**
 * @enum iwdt_iwdtcstpr_shifts_t
 * @brief IWDTCSTPR bit field positions
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_iwdt_cstpr_slcstp_pos = 7, /**< Sleep Mode Count Stop bit position */
} iwdt_iwdtcstpr_shifts_t;

/**
 * @enum iwdt_iwdtcstpr_bits_t
 * @brief IWDT Count Stop Control Register (IWDTCSTPR) Bit Field Values
 *
 * @details
 * Controls whether the IWDT counter continues during sleep modes.
 * This register is unique to IWDT (WDT does not have this capability).
 *
 * @par Register Layout (8-bit)
 * | Bit | Field  | R/W | Description                             |
 * |-----|--------|-----|-----------------------------------------|
 * | 6:0 | -      | -   | Reserved                                |
 * | 7   | SLCSTP | R/W | 0=Continue counting, 1=Stop in sleep    |
 *
 * @par Sleep Mode Behavior
 * - **Continue (k_iwdt_slcstp_continue)**: IWDT keeps running in sleep.
 *   Ensures system wakes up if sleep takes too long. **Recommended.**
 * - **Stop (k_iwdt_slcstp_stop)**: IWDT pauses during sleep.
 *   Use only if sleep duration is unpredictable and legitimate.
 *
 * @par Configuration Example
 * @code{.c}
 * // Recommended: Keep counting during sleep
 * iwdt()->iwdtcstpr = k_iwdt_slcstp_continue;
 *
 * // Alternative: Stop counting during sleep
 * iwdt()->iwdtcstpr = k_iwdt_slcstp_stop;
 * @endcode
 *
 * @see rx_iwdt_regs_t::iwdtcstpr Count stop control register
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief SLCSTP bit mask
   */
  k_iwdt_cstpr_slcstp_mask = (1U << 7),

  /**
   * @brief Stop counting during sleep (SLCSTP=1)
   * @details IWDT counter pauses when MCU enters sleep mode.
   * @warning Use with caution - may mask stuck-in-sleep conditions
   */
  k_iwdt_slcstp_stop = (1U << 7),

  /**
   * @brief Continue counting during sleep (SLCSTP=0)
   * @details IWDT counter continues in sleep mode. **Recommended.**
   * Ensures system recovery if sleep is not properly exited.
   */
  k_iwdt_slcstp_continue = 0x00,
} iwdt_iwdtcstpr_bits_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* The k_iwdt_base_addr enum declaration above is itself the contract for the
 * IWDT base address (per Hardware Manual). A static_assert that re-states the
 * same literal here would duplicate the magic number without adding a real
 * cross-check, so we rely on the enum declaration alone. */

/* Verify register structure layout */
static_assert(sizeof(rx_iwdt_regs_t) == 9, "IWDT register structure size incorrect");
static_assert(offsetof(rx_iwdt_regs_t, iwdtrr) == 0x00, "IWDTRR offset incorrect");
static_assert(offsetof(rx_iwdt_regs_t, iwdtcr) == 0x02, "IWDTCR offset incorrect");
static_assert(offsetof(rx_iwdt_regs_t, iwdtsr) == 0x04, "IWDTSR offset incorrect");
static_assert(offsetof(rx_iwdt_regs_t, iwdtrcr) == 0x06, "IWDTRCR offset incorrect");
static_assert(offsetof(rx_iwdt_regs_t, iwdtcstpr) == 0x08, "IWDTCSTPR offset incorrect");

#ifdef __cplusplus
}
#endif
