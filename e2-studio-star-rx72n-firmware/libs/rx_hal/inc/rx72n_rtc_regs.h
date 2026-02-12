/* SPDX-License-Identifier: MIT */
/**
 * @file rx72n_rtc_regs.h
 * @brief RX72N Real-Time Clock (RTC) Register Definitions
 *
 * @details
 * Minimal RTC register definitions for sub-clock oscillator disable functionality.
 * The RTC module is **not used** in the STAR project. These definitions exist
 * solely to properly disable the sub-clock oscillator during system initialization.
 *
 * @par Why This File Exists (Design Rationale)
 * The RX72N sub-clock oscillator (32.768 kHz) consumes power even when not
 * actively used. To fully disable it, the RTC module's RCR3.RTCEN bit must be
 * cleared to 0. This file provides the minimum register definitions needed for
 * this operation without including the full RTC implementation.
 *
 * @par CRITICAL ADDRESS VERIFICATION (2026-01-29)
 * All register addresses verified against RX72N Hardware Manual Ch33.
 * Previous implementation had **CRITICAL BUG** - wrong struct offsets:
 * - Old code: RCR3 at offset 0x1A (0x0008C41A = RMONAR register!)
 * - Correct:  RCR3 at offset 0x26 (0x0008C426 per manual)
 *
 * @par Key Register Addresses (from Ch33)
 * | Register | Address    | Offset | Description                    |
 * |----------|------------|--------|--------------------------------|
 * | R64CNT   | 0x0008C400 | 0x00   | 64-Hz Counter                  |
 * | RSECCNT  | 0x0008C402 | 0x02   | Second Counter (BCD)           |
 * | RMINCNT  | 0x0008C404 | 0x04   | Minute Counter (BCD)           |
 * | RHRCNT   | 0x0008C406 | 0x06   | Hour Counter (BCD)             |
 * | RWKCNT   | 0x0008C408 | 0x08   | Day-of-Week Counter            |
 * | RDAYCNT  | 0x0008C40A | 0x0A   | Day Counter (BCD)              |
 * | RMONCNT  | 0x0008C40C | 0x0C   | Month Counter (BCD)            |
 * | RYRCNT   | 0x0008C40E | 0x0E   | Year Counter (BCD, 16-bit)     |
 * | RSECAR   | 0x0008C410 | 0x10   | Second Alarm                   |
 * | ...      | ...        | ...    | (alarm registers)              |
 * | RYRAREN  | 0x0008C41E | 0x1E   | Year Alarm Enable              |
 * | (gap)    | 0x0008C420 | 0x20   | Reserved (2 bytes)             |
 * | RCR1     | 0x0008C422 | 0x22   | RTC Control Register 1         |
 * | RCR2     | 0x0008C424 | 0x24   | RTC Control Register 2         |
 * | RCR3     | 0x0008C426 | 0x26   | **RTC Control Register 3**     |
 * | RCR4     | 0x0008C428 | 0x28   | RTC Control Register 4         |
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] N/A (no loops)
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] Single-statement accessor
 * - Rule 5: [OK] Static assertions verify addresses
 * - Rule 6: [OK] Minimal scope
 * - Rule 7: [OK] N/A
 * - Rule 8: [OK] All constants use C23 typed enums
 * - Rule 9: [OK] No function pointers
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par Manual References
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 33: Realtime Clock (RTCd), pages 1616-1670
 * - Section 33.2.11: RTC Control Register 3 (RCR3), page 1637
 *
 * @note This is an intentionally minimal implementation
 * @warning Do not expand this file unless RTC functionality is actually needed
 *
 * @see rx72n_system_regs.h SOSCCR register for sub-clock oscillator control
 * @see rx_clock_power_init.c Clock initialization implementation
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * RTC Register Addresses
 * =============================================================================
 */

/**
 * @enum rtc_addresses_t
 * @brief RTC module register addresses
 *
 * @details
 * Addresses for RTC registers verified against RX72N Hardware Manual Ch33.
 * Only addresses needed for sub-clock disable are defined.
 *
 * @note Addresses verified 2026-01-29 against manual section 33.2
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief RTC register base address (0x0008C400)
   * @details Manual ref: Ch33 section 33.2.1 (R64CNT at 0x0008C400)
   */
  k_rtc_base_addr = 0x0008C400U,

  /**
   * @brief RTC Control Register 3 (RCR3) address
   * @details
   * Direct address for RCR3 at 0x0008C426 (offset 0x26 from base).
   * Manual ref: Ch33 section 33.2.11
   * @note This is the ONLY RTC register actively used in STAR project
   */
  k_rtc_rcr3_addr = 0x0008C426U,

  /**
   * @brief RTC Control Register 1 (RCR1) address
   * @details Manual ref: Ch33 section 33.2.9, address 0x0008C422
   */
  k_rtc_rcr1_addr = 0x0008C422U,

  /**
   * @brief RTC Control Register 2 (RCR2) address
   * @details Manual ref: Ch33 section 33.2.10, address 0x0008C424
   */
  k_rtc_rcr2_addr = 0x0008C424U,

  /**
   * @brief RTC Control Register 4 (RCR4) address
   * @details Manual ref: Ch33 section 33.2.12, address 0x0008C428
   */
  k_rtc_rcr4_addr = 0x0008C428U,
} rtc_addresses_t;

/**
 * @enum rtc_offsets_t
 * @brief RTC register offsets from base address
 *
 * @details
 * Offsets verified against RX72N Hardware Manual Ch33 register table.
 * Used for static assertions to verify struct layout.
 *
 * @note Offsets verified 2026-01-29 against manual
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rtc_offset_r64cnt  = 0x00U, /**< 64-Hz Counter offset */
  k_rtc_offset_rseccnt = 0x02U, /**< Second Counter offset (note: 0x02, not 0x01) */
  k_rtc_offset_rmincnt = 0x04U, /**< Minute Counter offset */
  k_rtc_offset_rhrcnt  = 0x06U, /**< Hour Counter offset */
  k_rtc_offset_rwkcnt  = 0x08U, /**< Day-of-Week Counter offset */
  k_rtc_offset_rdaycnt = 0x0AU, /**< Day Counter offset */
  k_rtc_offset_rmoncnt = 0x0CU, /**< Month Counter offset */
  k_rtc_offset_ryrcnt  = 0x0EU, /**< Year Counter offset (16-bit) */
  k_rtc_offset_rsecar  = 0x10U, /**< Second Alarm offset */
  k_rtc_offset_rminar  = 0x12U, /**< Minute Alarm offset */
  k_rtc_offset_rhrar   = 0x14U, /**< Hour Alarm offset */
  k_rtc_offset_rwkar   = 0x16U, /**< Day-of-Week Alarm offset */
  k_rtc_offset_rdayar  = 0x18U, /**< Day Alarm offset */
  k_rtc_offset_rmonar  = 0x1AU, /**< Month Alarm offset */
  k_rtc_offset_ryrar   = 0x1CU, /**< Year Alarm offset (16-bit) */
  k_rtc_offset_ryraren = 0x1EU, /**< Year Alarm Enable offset */
  k_rtc_offset_rcr1    = 0x22U, /**< RTC Control Register 1 offset */
  k_rtc_offset_rcr2    = 0x24U, /**< RTC Control Register 2 offset */
  k_rtc_offset_rcr3    = 0x26U, /**< RTC Control Register 3 offset **KEY** */
  k_rtc_offset_rcr4    = 0x28U, /**< RTC Control Register 4 offset */
  k_rtc_offset_rfrh    = 0x2AU, /**< Frequency Register H offset */
  k_rtc_offset_rfrl    = 0x2CU, /**< Frequency Register L offset */
  k_rtc_offset_radj    = 0x2EU, /**< Time Error Adjustment offset */
} rtc_offsets_t;

/* =============================================================================
 * RCR3 Register Bits
 * =============================================================================
 */

/**
 * @enum rcr3_bits_t
 * @brief RTC Control Register 3 (RCR3) Bit Definitions
 *
 * @details
 * Controls sub-clock oscillator input to the RTC module. This is the
 * only RTC register actively used in the STAR project.
 *
 * @par Register Layout (8-bit @ 0x0008C426)
 * | Bit | Field | R/W | Description                                       |
 * |-----|-------|-----|---------------------------------------------------|
 * | 0   | RTCEN | R/W | Sub-Clock Oscillator Control for RTC              |
 * | 7:1 | -     | R   | Reserved (read as 0)                              |
 *
 * @par RTCEN Bit Behavior
 * - **0 (k_rcr3_rtcen_disable)**: Sub-clock oscillator stopped for RTC.
 * - **1 (k_rcr3_rtcen_enable)**: Sub-clock oscillator running for RTC.
 *
 * @note Manual ref: Ch33 section 33.2.11 (page 1637)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Disable sub-clock input to RTC (RTCEN=0)
   * @details Use this value to disable RTC and save power.
   * This is the STAR project default configuration.
   */
  k_rcr3_rtcen_disable = 0x00U,

  /**
   * @brief Enable sub-clock input to RTC (RTCEN=1)
   * @details Use this value to enable RTC timekeeping.
   * Not used in STAR project.
   */
  k_rcr3_rtcen_enable = 0x01U,

  /**
   * @brief RTCEN bit position (bit 0)
   */
  k_rcr3_rtcen_bit = 0x01U,
} rcr3_bits_t;

/* =============================================================================
 * Register Accessor Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to RTC Control Register 3 (RCR3)
 *
 * @details
 * Returns a volatile pointer to RCR3 at address 0x0008C426.
 * This is the only RTC register used in STAR project (for sub-clock disable).
 *
 * @par Address Verification
 * - Manual: Ch33 section 33.2.11 specifies RCR3 at 0x0008C426
 * - This accessor uses the direct address, NOT struct offset
 * - Static assertion verifies address at compile time
 *
 * @return Volatile pointer to RCR3 register (8-bit)
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre RTC module stop bit (MSTPCRC.MSTPC27) should be cleared for access
 * @post Pointer is valid for the lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 * @note This function is always inlined for zero overhead
 *
 * @par Example (Sub-Clock Disable):
 * @code{.c}
 * // During system initialization
 * *rtc_rcr3_reg() = k_rcr3_rtcen_disable;  // Disable sub-clock to RTC
 * @endcode
 *
 * @see k_rtc_rcr3_addr Address constant
 * @see rcr3_bits_t RCR3 control bits
 * @since Version 1.0.0
 */
static inline volatile uint8_t* rtc_rcr3_reg(void)
{
  return (volatile uint8_t*)k_rtc_rcr3_addr;
}

/**
 * @brief Get pointer to RTC Control Register 1 (RCR1)
 *
 * @details
 * Returns a volatile pointer to RCR1 at address 0x0008C422.
 * Not used in STAR project but provided for completeness.
 *
 * @return Volatile pointer to RCR1 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* rtc_rcr1_reg(void)
{
  return (volatile uint8_t*)k_rtc_rcr1_addr;
}

/**
 * @brief Get pointer to RTC Control Register 2 (RCR2)
 *
 * @details
 * Returns a volatile pointer to RCR2 at address 0x0008C424.
 * Not used in STAR project but provided for completeness.
 *
 * @return Volatile pointer to RCR2 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* rtc_rcr2_reg(void)
{
  return (volatile uint8_t*)k_rtc_rcr2_addr;
}

/**
 * @brief Get pointer to RTC Control Register 4 (RCR4)
 *
 * @details
 * Returns a volatile pointer to RCR4 at address 0x0008C428.
 * Not used in STAR project but provided for completeness.
 *
 * @return Volatile pointer to RCR4 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* rtc_rcr4_reg(void)
{
  return (volatile uint8_t*)k_rtc_rcr4_addr;
}

/* =============================================================================
 * Static Assertions - Address Verification
 * =============================================================================
 */

/**
 * @name Address Verification
 * @brief Compile-time verification of register addresses
 * @{
 */

/* Verify RCR3 address is correct (critical for sub-clock disable) */
static_assert(k_rtc_rcr3_addr == 0x0008C426U, "RCR3 address must be 0x0008C426 per Ch33 manual");

/* Verify base address */
static_assert(k_rtc_base_addr == 0x0008C400U,
              "RTC base address must be 0x0008C400 per Ch33 manual");

/* Verify RCR3 offset from base */
static_assert(k_rtc_rcr3_addr - k_rtc_base_addr == k_rtc_offset_rcr3,
              "RCR3 offset must be 0x26 from base");

/* Verify other control register addresses */
static_assert(k_rtc_rcr1_addr == 0x0008C422U, "RCR1 address must be 0x0008C422 per Ch33 manual");

static_assert(k_rtc_rcr2_addr == 0x0008C424U, "RCR2 address must be 0x0008C424 per Ch33 manual");

static_assert(k_rtc_rcr4_addr == 0x0008C428U, "RCR4 address must be 0x0008C428 per Ch33 manual");

/* Verify control register spacing (each 2 bytes apart) */
static_assert(k_rtc_rcr2_addr - k_rtc_rcr1_addr == 2U, "RCR1 to RCR2 spacing must be 2 bytes");

static_assert(k_rtc_rcr3_addr - k_rtc_rcr2_addr == 2U, "RCR2 to RCR3 spacing must be 2 bytes");

static_assert(k_rtc_rcr4_addr - k_rtc_rcr3_addr == 2U, "RCR3 to RCR4 spacing must be 2 bytes");

/** @} */

#ifdef __cplusplus
}
#endif
