/**
 * @file rx72n_cmt_regs.h
 * @brief RX72N Compare Match Timer (CMT) Register Definitions
 *
 * @details
 * Register definitions for the Compare Match Timer (CMT) peripheral on the
 * RX72N microcontroller. CMT provides simple periodic interrupt generation
 * used for RTOS system tick and general timing purposes.
 *
 * @par CMT System Architecture
 * @verbatim
 *                         RX72N CMT Timer Architecture
 *   +-------------------------------------------------------------------------+
 *   |                              CMT Module                                 |
 *   |  +-----------------------------------------------------------------+   |
 *   |  |                     Clock Source (PCLKB)                        |   |
 *   |  |                          60 MHz                                 |   |
 *   |  +-------------------------------+---------------------------------+   |
 *   |                                  |                                     |
 *   |         +------------------------+------------------------+            |
 *   |         |                        |                        |            |
 *   |         v                        v                        v            |
 *   |  +-------------+          +-------------+          +-------------+     |
 *   |  | CMT0 (Unit0)|          | CMT1 (Unit0)|          | CMT2 (Unit1)|     |
 *   |  | /8/32/128/512|         | /8/32/128/512|         | /8/32/128/512|    |
 *   |  |             |          |             |          |             |     |
 *   |  | CMCNT[16]   |          | CMCNT[16]   |          | CMCNT[16]   |     |
 *   |  |    ^        |          |    ^        |          |    ^        |     |
 *   |  |    | Count  |          |    | Count  |          |    | Count  |     |
 *   |  |    v        |          |    v        |          |    v        |     |
 *   |  | CMCOR[16]   |          | CMCOR[16]   |          | CMCOR[16]   |     |
 *   |  | (Compare)   |          | (Compare)   |          | (Compare)   |     |
 *   |  +------+------+          +------+------+          +------+------+     |
 *   |         |                        |                        |            |
 *   |         | Match                  | Match                  | Match      |
 *   |         v                        v                        v            |
 *   +---------+------------------------+------------------------+------------+
 *             |                        |                        |
 *             v                        v                        v
 *       +----------+            +----------+            +----------+
 *       | CMI0 IRQ |            | CMI1 IRQ |            | CMI2 IRQ |
 *       | (Vec 28) |            | (Vec 29) |            | (Vec 30) |
 *       +----------+            +----------+            +----------+
 *             |
 *             v
 *       +--------------+
 *       | ThreadX Tick |
 *       |   100 Hz     |
 *       +--------------+
 *
 *   Memory Map (Verified against RX72N Manual Ch31):
 *   +------------+----------------------------------------------+
 *   |  Address   |  Register                                    |
 *   +------------+----------------------------------------------+
 *   | 0x00088000 |  CMSTR0 (Start Register Unit 0: CMT0/1)      |
 *   | 0x00088002 |  CMT0.CMCR (Control Register)                |
 *   | 0x00088004 |  CMT0.CMCNT (Counter)                        |
 *   | 0x00088006 |  CMT0.CMCOR (Compare Match Register)         |
 *   | 0x00088008 |  CMT1.CMCR                                   |
 *   | 0x0008800A |  CMT1.CMCNT                                  |
 *   | 0x0008800C |  CMT1.CMCOR                                  |
 *   | 0x00088010 |  CMSTR1 (Start Register Unit 1: CMT2/3)      |
 *   | 0x00088012 |  CMT2.CMCR                                   |
 *   | 0x00088014 |  CMT2.CMCNT                                  |
 *   | 0x00088016 |  CMT2.CMCOR                                  |
 *   | 0x00088018 |  CMT3.CMCR                                   |
 *   | 0x0008801A |  CMT3.CMCNT                                  |
 *   | 0x0008801C |  CMT3.CMCOR                                  |
 *   +------------+----------------------------------------------+
 * @endverbatim
 *
 * @par Timer Period Calculation
 * The CMT generates an interrupt when CMCNT matches CMCOR. The timer period
 * is calculated as:
 *
 * @f[
 * T_{period} = \frac{(CMCOR + 1) \times DIV}{f_{PCLKB}}
 * @f]
 *
 * Where:
 * - @f$ T_{period} @f$ = Timer period in seconds
 * - @f$ CMCOR @f$ = Compare match register value (0-65535)
 * - @f$ DIV @f$ = Clock divider (8, 32, 128, or 512)
 * - @f$ f_{PCLKB} @f$ = Peripheral clock B frequency (60 MHz)
 *
 * @par STAR Project ThreadX Configuration (100 Hz tick)
 * @f[
 * CMCOR = \frac{f_{PCLKB}}{DIV \times f_{tick}} - 1
 *       = \frac{60,000,000}{8 \times 100} - 1 = 74,999
 * @f]
 *
 * @par Example: CMT0 Initialization for 100 Hz ThreadX Tick
 * @code
 * #include "rx72n_cmt_regs.h"
 *
 * // Stop CMT0 before configuration
 * cmt_ctrl()->cmstr0 &= ~k_rx72n_cmstr0_cmt0_enable;
 *
 * // Configure CMT0: PCLKB/8 = 7.5 MHz, interrupt enable
 * cmt0()->cmcr = 0x0040;  // CKS=00 (PCLKB/8), CMIE=1
 *
 * // Set compare match value for 100 Hz: 7.5 MHz / 100 - 1 = 74999
 * cmt0()->cmcor = 74999;
 *
 * // Clear counter
 * cmt0()->cmcnt = 0;
 *
 * // Configure interrupt in ICU (priority 5, vector 28)
 * // ... (see rx72n_icu_regs.h for ICU configuration)
 *
 * // Start CMT0
 * cmt_ctrl()->cmstr0 |= k_rx72n_cmstr0_cmt0_enable;
 * @endcode
 *
 * @par NASA Power of 10 Compliance
 * - Rule 3: No dynamic memory - all register access via static inline functions
 * - Rule 5: Compile-time assertions verify all register layout assumptions
 * - Rule 8: Uses typed enums (C23) for all constants, no magic numbers
 * - Rule 10: Clean build with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - Single Responsibility: CMT register definitions only, no driver logic
 * - Open/Closed: Accessors can be used with any CMT configuration
 * - Interface Segregation: Minimal API - just register access functions
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware, Chapter 31 (Compare Match Timer)
 *
 * @par Verification Status
 * [PASS] VERIFIED (2026-01-28) - All register addresses and offsets verified
 * against RX72N Manual Chapter 31.
 *
 * @see rx_cmt.h Higher-level CMT driver API
 * @see rx72n_icu_regs.h ICU interrupt configuration for CMI interrupts
 *
 * @date 2026-01-28
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup cmt_regs CMT Register Definitions
 * @brief Compare Match Timer (CMT) hardware register interface
 * @{
 */

/* =============================================================================
 * Compare Match Timer (CMT) - For ThreadX System Tick
 * =============================================================================
 */

/**
 * @enum rx_cmt_addresses_t
 * @brief CMT base addresses (verified against RX72N Hardware Manual Ch31)
 *
 * @details
 * The RX72N has 4 CMT channels organized into 2 units:
 * - Unit 0: CMT0 and CMT1 (controlled by CMSTR0)
 * - Unit 1: CMT2 and CMT3 (controlled by CMSTR1)
 *
 * @par Memory Layout
 * @verbatim
 *   Address      | Content
 *   -------------+---------------------
 *   0x00088000   | CMSTR0 (Unit 0 start)
 *   0x00088002   | CMT0 registers (6 bytes)
 *   0x00088008   | CMT1 registers (6 bytes)
 *   0x00088010   | CMSTR1 (Unit 1 start)
 *   0x00088012   | CMT2 registers (6 bytes)
 *   0x00088018   | CMT3 registers (6 bytes)
 * @endverbatim
 *
 * @note CMSTR registers are at base, channel registers follow at +2 offset.
 */
typedef enum : uintptr_t {
  k_cmt_ctrl_base_addr  = 0x00088000, /**< CMSTR0 base: Unit 0 start register (CMT0/CMT1) */
  k_cmt1_ctrl_base_addr = 0x00088010, /**< CMSTR1 base: Unit 1 start register (CMT2/CMT3) */
  k_cmt0_base_addr      = 0x00088002, /**< CMT0 registers (CMCR, CMCNT, CMCOR) */
  k_cmt1_base_addr      = 0x00088008, /**< CMT1 registers (CMCR, CMCNT, CMCOR) */
  k_cmt2_base_addr      = 0x00088012, /**< CMT2 registers (CMCR, CMCNT, CMCOR) */
  k_cmt3_base_addr      = 0x00088018, /**< CMT3 registers (CMCR, CMCNT, CMCOR) */
} rx_cmt_addresses_t;

/**
 * @enum rx_cmt_layout_t
 * @brief CMT register layout constants used for compile-time verification
 *
 * @details
 * Physical byte sizes and register offsets for CMT structures.
 * Used exclusively in static_assert expressions so no magic numbers
 * appear in code.
 *
 * @see rx_cmt_channel_regs_t
 * @see rx_cmt_control_regs_t
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cmt_channel_regs_size_bytes = 6,    /**< Channel struct: 3 x 16-bit registers */
  k_cmt_control_regs_size_bytes = 4,    /**< Control struct: 2 x 16-bit registers */
  k_cmt_cmcr_offset             = 0x00, /**< CMCR  byte offset from channel base */
  k_cmt_cmcnt_offset            = 0x02, /**< CMCNT byte offset from channel base */
  k_cmt_cmcor_offset            = 0x04, /**< CMCOR byte offset from channel base */
  k_cmt_cmstr_reg_size_bytes    = 2,    /**< CMSTR register width (one 16-bit word) */
} rx_cmt_layout_t;

/**
 * @struct rx_cmt_channel_regs_t
 * @brief CMT Channel Register Map (6 bytes per channel)
 *
 * @details
 * Compare Match Timer (CMT) channel registers for periodic interrupt generation.
 * Each channel is a 16-bit up-counter that generates an interrupt when CMCNT
 * matches CMCOR, then resets to 0 and continues counting.
 *
 * @par Register Layout (6 bytes)
 * @verbatim
 *   Offset | Size | Register | Description
 *   -------+------+----------+------------------------------
 *   0x00   |  2   | CMCR     | Control: clock div, IRQ enable
 *   0x02   |  2   | CMCNT    | Counter: current count value
 *   0x04   |  2   | CMCOR    | Compare: match value (period)
 * @endverbatim
 *
 * @par CMCR Register Bit Layout (RX72N Manual Ch31, p.1583)
 * @verbatim
 *   Bit | Name | Description
 *   ----+------+------------------------------------
 *   15-8|  -   | Reserved
 *    7  |  -   | Reserved (write value should be 1)
 *    6  | CMIE | Compare Match Interrupt Enable
 *   5-2 |  -   | Reserved
 *   1-0 | CKS  | Clock Select (00=PCLK/8, 01=/32, 10=/128, 11=/512)
 * @endverbatim
 *
 * @par Timer Operation
 * 1. Configure CMCR with clock divider and interrupt enable
 * 2. Set CMCOR to desired period value
 * 3. Clear CMCNT to 0
 * 4. Enable timer via CMSTRn register
 * 5. Counter increments at PCLKB/DIV rate
 * 6. When CMCNT == CMCOR, interrupt fires and CMCNT resets to 0
 *
 * @invariant CMCNT <= CMCOR during normal operation
 *
 * @see rx_cmt_control_regs_t For start/stop control (CMSTR0/1)
 */
typedef struct {
  /**
   * @brief Compare Match Timer Control Register
   *
   * @details
   * Controls clock source divider and interrupt enable.
   *
   * | Bits | Name | R/W | Description |
   * |------|------|-----|-------------|
   * | 7    |  -   | R/W | Reserved; write value should be 1 (manual p.1583) |
   * | 6    | CMIE | R/W | 1 = Enable compare match interrupt |
   * | 1-0  | CKS  | R/W | Clock select: 00=/8, 01=/32, 10=/128, 11=/512 |
   */
  volatile uint16_t cmcr;

  /**
   * @brief Compare Match Timer Counter Register
   *
   * @details
   * 16-bit up-counter incremented by selected clock. Cleared to 0
   * on compare match and when timer is stopped.
   *
   * - Read: Current counter value
   * - Write: Set counter value (typically write 0 before starting)
   */
  volatile uint16_t cmcnt;

  /**
   * @brief Compare Match Timer Constant Register
   *
   * @details
   * Compare match value. When CMCNT equals this value, an interrupt
   * is generated (if CMIE=1) and CMCNT resets to 0.
   *
   * Period calculation: @f$ T = \frac{(CMCOR + 1) \times DIV}{f_{PCLKB}} @f$
   *
   * | CMCOR Value | Period @ PCLK/8 (60MHz) |
   * |-------------|-------------------------|
   * | 74999       | 10 ms (100 Hz)          |
   * | 7499        | 1 ms (1000 Hz)          |
   * | 749         | 100 us (10 kHz)         |
   */
  volatile uint16_t cmcor;
} rx_cmt_channel_regs_t;

/**
 * @struct rx_cmt_control_regs_t
 * @brief CMT Start/Stop Control Register Map
 *
 * @details
 * Controls starting and stopping of CMT channels. Each unit has a
 * separate start register controlling two channels.
 *
 * @par Memory Layout
 * @verbatim
 *   Address    | Register | Controls
 *   -----------+----------+---------------
 *   0x00088000 | CMSTR0   | CMT0, CMT1
 *   0x00088010 | CMSTR1   | CMT2, CMT3
 * @endverbatim
 *
 * @note CMSTR0 and CMSTR1 are NOT contiguous! CMSTR1 is at +0x10 from CMSTR0.
 *       This struct is only for documentation; use cmt_ctrl() for CMSTR0.
 *
 * @par Usage Example
 * @code
 * // Start CMT0
 * cmt_ctrl()->cmstr0 |= k_rx72n_cmstr0_cmt0_enable;
 *
 * // Stop CMT0
 * cmt_ctrl()->cmstr0 &= ~k_rx72n_cmstr0_cmt0_enable;
 * @endcode
 */
typedef struct {
  /**
   * @brief Compare Match Timer Start Register 0
   * @details Controls CMT0 and CMT1 operation.
   * - Bit 0 (STR0): 1 = Start CMT0, 0 = Stop CMT0
   * - Bit 1 (STR1): 1 = Start CMT1, 0 = Stop CMT1
   */
  volatile uint16_t cmstr0;

  /**
   * @brief Compare Match Timer Start Register 1
   * @details Controls CMT2 and CMT3 operation.
   * - Bit 0 (STR2): 1 = Start CMT2, 0 = Stop CMT2
   * - Bit 1 (STR3): 1 = Start CMT3, 0 = Stop CMT3
   *
   * @warning This field is NOT at the expected offset! CMSTR1 is at
   *          0x00088010, not 0x00088002. Access via separate function.
   */
  volatile uint16_t cmstr1;
} rx_cmt_control_regs_t;

/**
 * @enum rx_cmt_cmstr0_bits_t
 * @brief CMSTR0 register bit definitions (CMT Unit 0 start control)
 *
 * @details
 * Bit field definitions for Compare Match Timer Start Register 0.
 * Controls starting and stopping of CMT0 and CMT1 channels.
 *
 * @par Register Layout (16-bit, only bits 0-1 used)
 * @verbatim
 *   Bit | Name | Description
 *   ----+------+--------------------------------
 *    0  | STR0 | 1 = Start CMT0, 0 = Stop CMT0
 *    1  | STR1 | 1 = Start CMT1, 0 = Stop CMT1
 *   2-15|  -   | Reserved (write 0)
 * @endverbatim
 */
typedef enum : uint8_t {
  k_rx72n_cmstr0_cmt0_enable = 0x01, /**< Bit 0: Start/stop CMT0 counter */
  k_rx72n_cmstr0_cmt1_enable = 0x02, /**< Bit 1: Start/stop CMT1 counter */
} rx_cmt_cmstr0_bits_t;

/**
 * @enum rx_cmt_cmstr1_bits_t
 * @brief CMSTR1 register bit definitions (CMT Unit 1 start control)
 *
 * @details
 * Bit field definitions for Compare Match Timer Start Register 1.
 * Controls starting and stopping of CMT2 and CMT3 channels.
 *
 * @note CMSTR1 is at address 0x00088010, which is 16 bytes after CMSTR0.
 *
 * @par Register Layout (16-bit, only bits 0-1 used)
 * @verbatim
 *   Bit | Name | Description
 *   ----+------+--------------------------------
 *    0  | STR2 | 1 = Start CMT2, 0 = Stop CMT2
 *    1  | STR3 | 1 = Start CMT3, 0 = Stop CMT3
 *   2-15|  -   | Reserved (write 0)
 * @endverbatim
 */
typedef enum : uint8_t {
  k_rx72n_cmstr1_cmt2_enable = 0x01, /**< Bit 0: Start/stop CMT2 counter */
  k_rx72n_cmstr1_cmt3_enable = 0x02, /**< Bit 1: Start/stop CMT3 counter */
} rx_cmt_cmstr1_bits_t;

/* =============================================================================
 * Inline Register Accessor Functions
 * =============================================================================
 * These functions provide type-safe access to CMT hardware registers.
 * Using functions instead of macros enables:
 * - Type checking at compile time
 * - Debugger-friendly register access
 * - Consistent volatile semantics
 */

/**
 * @brief Get pointer to CMT0 registers
 *
 * @details
 * Returns a volatile pointer to CMT channel 0 registers at address 0x00088002.
 * CMT0 is typically used for the ThreadX system tick (100 Hz).
 *
 * @return Volatile pointer to CMT0 register structure
 *
 * @par Example
 * @code
 * // Configure CMT0 for 100 Hz interrupt with PCLK/8 divider
 * cmt0()->cmcr = 0x0040;   // CKS=00 (PCLKB/8), CMIE=1
 * cmt0()->cmcor = 74999;   // 60MHz/8/100Hz - 1
 * cmt0()->cmcnt = 0;       // Clear counter
 * @endcode
 *
 * @see cmt_ctrl() For start/stop control
 */
static inline volatile rx_cmt_channel_regs_t* cmt0(void)
{
  return (volatile rx_cmt_channel_regs_t*)k_cmt0_base_addr;
}

/**
 * @brief Get pointer to CMT1 registers
 *
 * @details
 * Returns a volatile pointer to CMT channel 1 registers at address 0x00088008.
 * CMT1 is available for general-purpose timing.
 *
 * @return Volatile pointer to CMT1 register structure
 */
static inline volatile rx_cmt_channel_regs_t* cmt1(void)
{
  return (volatile rx_cmt_channel_regs_t*)k_cmt1_base_addr;
}

/**
 * @brief Get pointer to CMT2 registers
 *
 * @details
 * Returns a volatile pointer to CMT channel 2 registers at address 0x00088012.
 * CMT2 is part of Unit 1 (controlled by CMSTR1).
 *
 * @return Volatile pointer to CMT2 register structure
 */
static inline volatile rx_cmt_channel_regs_t* cmt2(void)
{
  return (volatile rx_cmt_channel_regs_t*)k_cmt2_base_addr;
}

/**
 * @brief Get pointer to CMT3 registers
 *
 * @details
 * Returns a volatile pointer to CMT channel 3 registers at address 0x00088018.
 * CMT3 is part of Unit 1 (controlled by CMSTR1).
 *
 * @return Volatile pointer to CMT3 register structure
 */
static inline volatile rx_cmt_channel_regs_t* cmt3(void)
{
  return (volatile rx_cmt_channel_regs_t*)k_cmt3_base_addr;
}

/**
 * @brief Get pointer to CMT control registers (CMSTR0)
 *
 * @details
 * Returns a volatile pointer to CMT control register structure at address
 * 0x00088000. Provides access to CMSTR0 for starting/stopping CMT0 and CMT1.
 *
 * @warning CMSTR1 (for CMT2/3) is at 0x00088010, not adjacent to CMSTR0.
 *          The rx_cmt_control_regs_t struct has incorrect cmstr1 offset.
 *          For CMSTR1 access, use: `*(volatile uint16_t*)0x00088010`
 *
 * @return Volatile pointer to CMT control register structure
 *
 * @par Example
 * @code
 * // Start CMT0
 * cmt_ctrl()->cmstr0 |= k_rx72n_cmstr0_cmt0_enable;
 *
 * // Stop CMT0 and CMT1
 * cmt_ctrl()->cmstr0 = 0;
 * @endcode
 */
static inline volatile rx_cmt_control_regs_t* cmt_ctrl(void)
{
  return (volatile rx_cmt_control_regs_t*)k_cmt_ctrl_base_addr;
}

/**
 * @brief Get pointer to CMT Unit 1 control register (CMSTR1)
 *
 * @details
 * Returns a volatile pointer to CMSTR1 at address 0x00088010. CMSTR1 is NOT
 * contiguous with CMSTR0 -- it is 0x10 bytes after CMSTR0. This accessor
 * provides correct access to CMSTR1 for starting/stopping CMT2 and CMT3.
 *
 * @warning Do NOT use cmt_ctrl()->cmstr1 to access CMSTR1 -- the struct field
 *          cmstr1 sits at offset +2 from CMSTR0 (address 0x00088002), which is
 *          CMT0.CMCR, not CMSTR1.
 *
 * @return Volatile pointer to CMT Unit 1 control register structure
 *
 * @par Example
 * @code
 * // Start CMT2
 * cmt_ctrl1()->cmstr0 |= k_rx72n_cmstr1_cmt2_enable;
 *
 * // Stop CMT3
 * cmt_ctrl1()->cmstr0 &= ~k_rx72n_cmstr1_cmt3_enable;
 * @endcode
 *
 * @see cmt_ctrl() For CMSTR0 (CMT0/CMT1 control)
 */
static inline volatile rx_cmt_control_regs_t* cmt_ctrl1(void)
{
  return (volatile rx_cmt_control_regs_t*)k_cmt1_ctrl_base_addr;
}

/* =============================================================================
 * Static Assertions - Compile-time verification of register layout
 * =============================================================================
 * These assertions verify our register structures match the RX72N hardware
 * exactly. If any assertion fails, the struct definition must be corrected.
 *
 * Reference: RX72N Group User's Manual: Hardware, Chapter 31, Section 21.2
 * =============================================================================
 */

/**
 * @defgroup cmt_static_assert CMT Static Assertions
 * @brief Compile-time verification of CMT register layout
 * @details
 * These static assertions are verified at compile time to ensure the
 * C structure definitions match the actual hardware register layout.
 * A failed assertion indicates a bug in the register definitions that
 * must be fixed before the code can compile.
 * @{
 */

/* Verify CMT channel register structure size (3 x 16-bit = 6 bytes) */
static_assert(sizeof(rx_cmt_channel_regs_t) == k_cmt_channel_regs_size_bytes,
              "CMT channel register structure size mismatch");

/* Verify CMT channel register offsets */
static_assert(offsetof(rx_cmt_channel_regs_t, cmcr)  == k_cmt_cmcr_offset,
              "CMT CMCR register offset incorrect");
static_assert(offsetof(rx_cmt_channel_regs_t, cmcnt) == k_cmt_cmcnt_offset,
              "CMT CMCNT register offset incorrect");
static_assert(offsetof(rx_cmt_channel_regs_t, cmcor) == k_cmt_cmcor_offset,
              "CMT CMCOR register offset incorrect");

/* Verify CMT control register structure size (2 x 16-bit = 4 bytes) */
static_assert(sizeof(rx_cmt_control_regs_t) == k_cmt_control_regs_size_bytes,
              "CMT control register structure size mismatch");

/* Verify address relationships: each channel immediately follows prior struct */
static_assert(k_cmt0_base_addr - k_cmt_ctrl_base_addr == k_cmt_cmstr_reg_size_bytes,
              "CMT0 must start one 16-bit CMSTR0 word after unit 0 base");
static_assert(k_cmt1_base_addr - k_cmt0_base_addr == k_cmt_channel_regs_size_bytes,
              "CMT0 to CMT1 spacing must equal one channel struct");
static_assert(k_cmt2_base_addr - k_cmt1_ctrl_base_addr == k_cmt_cmstr_reg_size_bytes,
              "CMT2 must start one 16-bit CMSTR1 word after unit 1 base");
static_assert(k_cmt3_base_addr - k_cmt2_base_addr == k_cmt_channel_regs_size_bytes,
              "CMT2 to CMT3 spacing must equal one channel struct");

/** @} */ /* End of cmt_static_assert group */

/** @} */ /* End of cmt_regs group */

#ifdef __cplusplus
}
#endif
