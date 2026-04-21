/**
 * @file rx72n_tmr_regs.h
 * @brief RX72N 8-Bit Timer (TMRb) Register Definitions
 *
 * @details
 * Register definitions for the TMR peripheral which provides 4 channels of
 * 8-bit timers (TMR0..TMR3) organized into two units. Each unit contains two
 * channels that may operate independently (two 8-bit counters) or cascaded
 * (one 16-bit counter, TMR01 or TMR23).
 *
 * @par Channel and Unit Layout
 * | Unit | Channels    | Module Stop Bit     | Cascade Name |
 * |------|-------------|---------------------|--------------|
 * | 0    | TMR0, TMR1  | MSTPCRA.MSTPA5      | TMR01        |
 * | 1    | TMR2, TMR3  | MSTPCRA.MSTPA4      | TMR23        |
 *
 * @par Key Features
 * - 8-bit up-counter with two compare-match registers (TCORA, TCORB)
 * - Optional 16-bit cascaded mode (even + odd channel concatenated)
 * - Counter clear on compare match A, compare match B, or external reset
 * - 8 clock sources per channel (external edge/level, PCLK dividers, cascade)
 * - Compare match A/B and overflow interrupts via SELECTB
 * - Dedicated TMO output pin per channel for hardware pulse generation
 *
 * @par Register Map Overview (Unit 0 / Unit 1)
 * | Even Ch Addr | Odd Ch Addr | Register | Size | Description               |
 * |--------------|-------------|----------|------|---------------------------|
 * | 0x00088200   | 0x00088201  | TCR      | 8    | Timer Control             |
 * | 0x00088202   | 0x00088203  | TCSR     | 8    | Timer Control/Status      |
 * | 0x00088204   | 0x00088205  | TCORA    | 8    | Time Constant A           |
 * | 0x00088206   | 0x00088207  | TCORB    | 8    | Time Constant B           |
 * | 0x00088208   | 0x00088209  | TCNT     | 8    | Timer Counter             |
 * | 0x0008820A   | 0x0008820B  | TCCR     | 8    | Timer Counter Control     |
 * | 0x0008820C   | 0x0008820D  | TCSTR    | 8    | Timer Counter Start (ELC) |
 *
 * Unit 1 uses the same layout offset +0x10 (TMR2 base 0x00088210, TMR3 0x00088211).
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp, or recursion
 * - Rule 2: N/A (no loops in register definitions)
 * - Rule 3: No dynamic memory allocation
 * - Rule 4: All accessor functions are single-statement
 * - Rule 5: N/A (hardware layer)
 * - Rule 6: Minimal scope - inline accessors only
 * - Rule 7: N/A (no return values to check)
 * - Rule 8: All constants use C23 typed enums
 * - Rule 9: No function pointers
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par Manual References
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0111 Rev.1.11)
 * - Chapter 30: 8-Bit Timer (TMRb), pages 1553-1580
 * - Chapter 15 Table 15.3: Software Configurable Interrupt B sources
 *
 * @see rx_tmr.h TMR HAL driver API
 * @see rx72n_regs.h Main register include file
 *
 * @author Locked, Inc.
 * @date 2026-04-21
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
 * TMR Base Addresses
 * =============================================================================
 */

/**
 * @enum rx_tmr_addresses_t
 * @brief TMR channel base addresses
 *
 * @details
 * Base addresses for each TMR channel (even channels at even byte offsets,
 * odd channels at the following odd byte). The cascade (16-bit) address is
 * the even-channel base; 16-bit bus accesses at those addresses transfer
 * the even channel as the upper 8 bits and the odd channel as the lower 8.
 *
 * @note Manual: Ch30.1.1 Table 30.1 - TMR Base Address Map
 * @see tmr0() tmr1() tmr2() tmr3() Accessor functions
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  k_tmr0_base_addr = 0x00088200, /**< TMR0 even channel (unit 0) */
  k_tmr1_base_addr = 0x00088201, /**< TMR1 odd channel  (unit 0) */
  k_tmr2_base_addr = 0x00088210, /**< TMR2 even channel (unit 1) */
  k_tmr3_base_addr = 0x00088211, /**< TMR3 odd channel  (unit 1) */

  /** @brief Unit 0 cascade (TMR0 upper, TMR1 lower) - 16-bit access base */
  k_tmr01_cascade_base_addr = 0x00088200,

  /** @brief Unit 1 cascade (TMR2 upper, TMR3 lower) - 16-bit access base */
  k_tmr23_cascade_base_addr = 0x00088210,

  /** @brief Byte spacing between units (TMR2 - TMR0) */
  k_tmr_unit_spacing = 0x10,
} rx_tmr_addresses_t;

/**
 * @enum rx_tmr_counts_t
 * @brief TMR channel count constant
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_channel_count = 4, /**< Total TMR channels (TMR0..TMR3) */
} rx_tmr_counts_t;

/* =============================================================================
 * TMR Channel Register Structure
 * =============================================================================
 */

/**
 * @struct rx_tmr_channel_regs_t
 * @brief TMR single-channel register block
 *
 * @details
 * Register block for one TMR channel accessed via byte strides of 2. Because
 * even and odd channels share the same 16-byte block at different parity
 * offsets, accessing TMRn directly requires striding by 2 bytes per field;
 * this structure therefore represents ONE channel with the interleaved odd
 * byte reserved.
 *
 * The layout follows the byte addressing in Table 30.1:
 *  - TMR0.TCR    @ 0x88200, TMR1.TCR    @ 0x88201
 *  - TMR0.TCSR   @ 0x88202, TMR1.TCSR   @ 0x88203
 *  - TMR0.TCORA  @ 0x88204, TMR1.TCORA  @ 0x88205
 *  - TMR0.TCORB  @ 0x88206, TMR1.TCORB  @ 0x88207
 *  - TMR0.TCNT   @ 0x88208, TMR1.TCNT   @ 0x88209
 *  - TMR0.TCCR   @ 0x8820A, TMR1.TCCR   @ 0x8820B
 *  - TMR0.TCSTR  @ 0x8820C, TMR1.TCSTR  @ 0x8820D
 *
 * Pointer returned by tmr0()/tmr2() points to the even byte; pointer returned
 * by tmr1()/tmr3() points to the odd byte. Each struct field therefore
 * accesses the channel's own byte.
 *
 * @invariant sizeof(rx_tmr_channel_regs_t) == 14 (spans byte 0 through 12)
 *
 * @see tmr0() tmr1() tmr2() tmr3() Accessor functions
 * @see rx_tmr_cascade_regs_t 16-bit cascade view
 * @since Version 1.0.0
 */
typedef struct {
  volatile uint8_t tcr;        /**< Timer Control Register (TCR) @ +0x00 */
  volatile uint8_t tcr_pair;   /**< Paired channel TCR             @ +0x01 */
  volatile uint8_t tcsr;       /**< Timer Control/Status (TCSR)    @ +0x02 */
  volatile uint8_t tcsr_pair;  /**< Paired channel TCSR            @ +0x03 */
  volatile uint8_t tcora;      /**< Time Constant A (TCORA)        @ +0x04 */
  volatile uint8_t tcora_pair; /**< Paired channel TCORA           @ +0x05 */
  volatile uint8_t tcorb;      /**< Time Constant B (TCORB)        @ +0x06 */
  volatile uint8_t tcorb_pair; /**< Paired channel TCORB           @ +0x07 */
  volatile uint8_t tcnt;       /**< Timer Counter (TCNT)           @ +0x08 */
  volatile uint8_t tcnt_pair;  /**< Paired channel TCNT            @ +0x09 */
  volatile uint8_t tccr;       /**< Timer Counter Control (TCCR)   @ +0x0A */
  volatile uint8_t tccr_pair;  /**< Paired channel TCCR            @ +0x0B */
  volatile uint8_t tcstr;      /**< Timer Counter Start (TCSTR)    @ +0x0C */
  volatile uint8_t tcstr_pair; /**< Paired channel TCSTR           @ +0x0D */
} rx_tmr_channel_regs_t;

/**
 * @struct rx_tmr_cascade_regs_t
 * @brief TMR cascaded 16-bit register view (TMR01 or TMR23)
 *
 * @details
 * When two TMR channels in the same unit are cascaded, TCORA, TCORB, TCNT,
 * and TCCR may be accessed as 16-bit values with the even channel forming
 * the upper byte and the odd channel forming the lower byte.
 *
 * Only TCORA/TCORB/TCNT/TCCR have defined 16-bit semantics (manual Table
 * 30.4); the other bytes are kept per-channel via the per-channel register
 * block accessed through tmr0()/tmr1()/tmr2()/tmr3().
 *
 * @invariant sizeof(rx_tmr_cascade_regs_t) == 12
 *
 * @see tmr01_cascade() tmr23_cascade() Accessor functions
 * @since Version 1.0.0
 */
typedef struct {
  volatile uint8_t  reserved_tcr[2];  /**< TCR bytes   (per-channel only)    @ +0x00..0x01 */
  volatile uint8_t  reserved_tcsr[2]; /**< TCSR bytes  (per-channel only)    @ +0x02..0x03 */
  volatile uint16_t tcora;            /**< 16-bit cascaded TCORA             @ +0x04 */
  volatile uint16_t tcorb;            /**< 16-bit cascaded TCORB             @ +0x06 */
  volatile uint16_t tcnt;             /**< 16-bit cascaded TCNT              @ +0x08 */
  volatile uint16_t tccr;             /**< 16-bit cascaded TCCR              @ +0x0A */
} rx_tmr_cascade_regs_t;

/* =============================================================================
 * Inline Accessor Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to TMR0 channel register block (unit 0 even channel)
 * @return Volatile pointer to TMR0 register block at 0x00088200
 * @pre TMR unit 0 module clock enabled (MSTPCRA.MSTPA5 = 0)
 * @post Returned pointer is valid for the lifetime of the program
 * @note Thread Safety: Safe - returns a constant hardware address
 * @see k_tmr0_base_addr
 * @since Version 1.0.0
 */
static inline volatile rx_tmr_channel_regs_t* tmr0(void)
{
  return (volatile rx_tmr_channel_regs_t*)k_tmr0_base_addr;
}

/**
 * @brief Get pointer to TMR1 channel register block (unit 0 odd channel)
 * @return Volatile pointer to TMR1 register block at 0x00088201
 * @pre TMR unit 0 module clock enabled (MSTPCRA.MSTPA5 = 0)
 * @post Returned pointer is valid for the lifetime of the program
 * @note Thread Safety: Safe - returns a constant hardware address
 * @see k_tmr1_base_addr
 * @since Version 1.0.0
 */
static inline volatile rx_tmr_channel_regs_t* tmr1(void)
{
  return (volatile rx_tmr_channel_regs_t*)k_tmr1_base_addr;
}

/**
 * @brief Get pointer to TMR2 channel register block (unit 1 even channel)
 * @return Volatile pointer to TMR2 register block at 0x00088210
 * @pre TMR unit 1 module clock enabled (MSTPCRA.MSTPA4 = 0)
 * @post Returned pointer is valid for the lifetime of the program
 * @note Thread Safety: Safe - returns a constant hardware address
 * @see k_tmr2_base_addr
 * @since Version 1.0.0
 */
static inline volatile rx_tmr_channel_regs_t* tmr2(void)
{
  return (volatile rx_tmr_channel_regs_t*)k_tmr2_base_addr;
}

/**
 * @brief Get pointer to TMR3 channel register block (unit 1 odd channel)
 * @return Volatile pointer to TMR3 register block at 0x00088211
 * @pre TMR unit 1 module clock enabled (MSTPCRA.MSTPA4 = 0)
 * @post Returned pointer is valid for the lifetime of the program
 * @note Thread Safety: Safe - returns a constant hardware address
 * @see k_tmr3_base_addr
 * @since Version 1.0.0
 */
static inline volatile rx_tmr_channel_regs_t* tmr3(void)
{
  return (volatile rx_tmr_channel_regs_t*)k_tmr3_base_addr;
}

/**
 * @brief Get pointer to TMR01 cascaded 16-bit register view
 * @return Volatile pointer to TMR01 cascade block at 0x00088200
 * @pre TMR unit 0 module clock enabled
 * @pre Cascade mode configured via TMR1.TCCR (CSS/CKS = overflow of TMR0)
 * @post Returned pointer is valid for the lifetime of the program
 * @note Thread Safety: Safe - returns a constant hardware address
 * @since Version 1.0.0
 */
static inline volatile rx_tmr_cascade_regs_t* tmr01_cascade(void)
{
  return (volatile rx_tmr_cascade_regs_t*)k_tmr01_cascade_base_addr;
}

/**
 * @brief Get pointer to TMR23 cascaded 16-bit register view
 * @return Volatile pointer to TMR23 cascade block at 0x00088210
 * @pre TMR unit 1 module clock enabled
 * @post Returned pointer is valid for the lifetime of the program
 * @note Thread Safety: Safe - returns a constant hardware address
 * @since Version 1.0.0
 */
static inline volatile rx_tmr_cascade_regs_t* tmr23_cascade(void)
{
  return (volatile rx_tmr_cascade_regs_t*)k_tmr23_cascade_base_addr;
}

/* =============================================================================
 * TCR - Timer Control Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_tmr_tcr_bits_t
 * @brief TCR bit positions and single-bit masks
 *
 * @par Bit Layout (8-bit)
 * | Bits  | Field       | R/W | Description                                |
 * |-------|-------------|-----|--------------------------------------------|
 * | 7     | CMIEB       | R/W | TCORB compare match interrupt enable       |
 * | 6     | CMIEA       | R/W | TCORA compare match interrupt enable       |
 * | 5     | OVIE        | R/W | TCNT overflow interrupt enable             |
 * | 4:3   | CCLR[1:0]   | R/W | Counter clear source select                |
 * | 2:0   | -           | R/W | Reserved (must be written 0)               |
 *
 * @note Manual: Ch30.2.4 - Timer Control Register (TCR)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tcr_cmieb = (1U << 7), /**< TCORB compare match interrupt enable */
  k_tmr_tcr_cmiea = (1U << 6), /**< TCORA compare match interrupt enable */
  k_tmr_tcr_ovie  = (1U << 5), /**< Overflow interrupt enable            */
} rx_tmr_tcr_bits_t;

/**
 * @enum rx_tmr_tcr_cclr_t
 * @brief TCR CCLR[1:0] counter clear source
 *
 * @note Manual: Ch30.2.4 Table - CCLR field
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tcr_cclr_disabled     = (0U << 3), /**< 00b: Counter clear disabled   */
  k_tmr_tcr_cclr_cmp_match_a  = (1U << 3), /**< 01b: Clear on TCORA match     */
  k_tmr_tcr_cclr_cmp_match_b  = (2U << 3), /**< 10b: Clear on TCORB match     */
  k_tmr_tcr_cclr_external_sig = (3U << 3), /**< 11b: Clear on external reset  */
} rx_tmr_tcr_cclr_t;

/**
 * @enum rx_tmr_tcr_masks_t
 * @brief TCR register field masks
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tcr_cclr_mask = 0x18, /**< CCLR[1:0] field mask (bits 4:3) */
} rx_tmr_tcr_masks_t;

/* =============================================================================
 * TCSR - Timer Control/Status Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_tmr_tcsr_bits_t
 * @brief TCSR bit positions (even channels TMR0/TMR2)
 *
 * @par Bit Layout (8-bit, even channels)
 * | Bits  | Field       | R/W | Description                                |
 * |-------|-------------|-----|--------------------------------------------|
 * | 7:5   | -           | R   | Undefined at reset                         |
 * | 4     | ADTE        | R/W | A/D trigger enable (TMR0/TMR2 only)        |
 * | 3:2   | OSB[1:0]    | R/W | TMO output select on TCORB compare match   |
 * | 1:0   | OSA[1:0]    | R/W | TMO output select on TCORA compare match   |
 *
 * @note For odd channels (TMR1/TMR3), bit 4 is reserved and reads as 1.
 * @note Manual: Ch30.2.6 - Timer Control/Status Register (TCSR)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tcsr_adte = (1U << 4), /**< A/D trigger enable (even channels only) */
} rx_tmr_tcsr_bits_t;

/**
 * @enum rx_tmr_tcsr_output_t
 * @brief TCSR OSA/OSB output select values (identical encoding)
 *
 * @details
 * Controls the TMO pin behavior on compare match. Both OSA and OSB use this
 * 2-bit encoding; shift the value left by 0 for OSA and 2 for OSB.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tcsr_output_no_change = 0x0, /**< 00b: No change                  */
  k_tmr_tcsr_output_low       = 0x1, /**< 01b: Drive low on compare match */
  k_tmr_tcsr_output_high      = 0x2, /**< 10b: Drive high on compare match*/
  k_tmr_tcsr_output_toggle    = 0x3, /**< 11b: Toggle on compare match    */
} rx_tmr_tcsr_output_t;

/**
 * @enum rx_tmr_tcsr_shift_t
 * @brief TCSR output-select field shifts
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tcsr_osa_shift = 0, /**< OSA[1:0] starts at bit 0 */
  k_tmr_tcsr_osb_shift = 2, /**< OSB[1:0] starts at bit 2 */
} rx_tmr_tcsr_shift_t;

/**
 * @enum rx_tmr_tcsr_masks_t
 * @brief TCSR field masks
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tcsr_osa_mask     = 0x03, /**< OSA[1:0] field mask (bits 1:0) */
  k_tmr_tcsr_osb_mask     = 0x0C, /**< OSB[1:0] field mask (bits 3:2) */
  k_tmr_tcsr_odd_reserved = 0x10, /**< Odd-channel bit 4 reserved, reads as 1 */
} rx_tmr_tcsr_masks_t;

/* =============================================================================
 * TCCR - Timer Counter Control Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_tmr_tccr_bits_t
 * @brief TCCR bit positions
 *
 * @par Bit Layout (8-bit)
 * | Bits  | Field       | R/W | Description                                |
 * |-------|-------------|-----|--------------------------------------------|
 * | 7     | TMRIS       | R/W | External reset detection (0=edge, 1=level) |
 * | 6:5   | -           | R/W | Reserved (write 0)                         |
 * | 4:3   | CSS[1:0]    | R/W | Clock source select                        |
 * | 2:0   | CKS[2:0]    | R/W | Clock divider select                       |
 *
 * @note Manual: Ch30.2.5 - Timer Counter Control Register (TCCR)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tccr_tmris = (1U << 7), /**< Timer reset detection condition */
} rx_tmr_tccr_bits_t;

/**
 * @enum rx_tmr_tccr_shift_t
 * @brief TCCR field shifts
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tccr_cks_shift = 0, /**< CKS[2:0] starts at bit 0 */
  k_tmr_tccr_css_shift = 3, /**< CSS[1:0] starts at bit 3 */
} rx_tmr_tccr_shift_t;

/**
 * @enum rx_tmr_tccr_masks_t
 * @brief TCCR field masks
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tccr_cks_mask = 0x07, /**< CKS[2:0] field mask (bits 2:0) */
  k_tmr_tccr_css_mask = 0x18, /**< CSS[1:0] field mask (bits 4:3) */
} rx_tmr_tccr_masks_t;

/**
 * @enum rx_tmr_tccr_css_t
 * @brief TCCR CSS[1:0] clock source select (per Table 30.5)
 *
 * @par Source Meaning
 * | Value  | Channel  | Source                                                  |
 * |--------|----------|---------------------------------------------------------|
 * | 00b    | any      | External count clock on TMCI pin (edge selected by CKS) |
 * | 01b    | any      | Internal clock (PCLK divider selected by CKS)           |
 * | 10b    | any      | Setting prohibited                                      |
 * | 11b    | TMR0/2   | Count on odd-channel TCNT overflow (cascade lower->up)  |
 * | 11b    | TMR1/3   | Count on even-channel TCORA compare match (cascade)     |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tccr_css_external = (0U << 3), /**< External count clock on TMCI pin */
  k_tmr_tccr_css_internal = (1U << 3), /**< Internal PCLK source (use CKS)   */
  k_tmr_tccr_css_cascade  = (3U << 3), /**< Cascade from paired channel     */
} rx_tmr_tccr_css_t;

/**
 * @enum rx_tmr_tccr_cks_internal_t
 * @brief TCCR CKS[2:0] divider values when CSS = internal clock
 *
 * @par Internal Clock Dividers (Table 30.5)
 * | CKS  | Source          |
 * |------|-----------------|
 * | 000b | Clock prohibited|
 * | 001b | PCLK            |
 * | 010b | PCLK/2          |
 * | 011b | PCLK/8          |
 * | 100b | PCLK/32         |
 * | 101b | PCLK/64         |
 * | 110b | PCLK/1024       |
 * | 111b | PCLK/8192       |
 *
 * @note Manual table order differs from register encoding. Values below
 * match the exact bit pattern written to CKS[2:0].
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tccr_cks_prohibited   = 0x0, /**< 000b: clock input prohibited */
  k_tmr_tccr_cks_pclk_div1    = 0x1, /**< 001b: PCLK / 1               */
  k_tmr_tccr_cks_pclk_div2    = 0x2, /**< 010b: PCLK / 2               */
  k_tmr_tccr_cks_pclk_div8    = 0x3, /**< 011b: PCLK / 8               */
  k_tmr_tccr_cks_pclk_div32   = 0x4, /**< 100b: PCLK / 32              */
  k_tmr_tccr_cks_pclk_div64   = 0x5, /**< 101b: PCLK / 64              */
  k_tmr_tccr_cks_pclk_div1024 = 0x6, /**< 110b: PCLK / 1024          */
  k_tmr_tccr_cks_pclk_div8192 = 0x7, /**< 111b: PCLK / 8192          */
} rx_tmr_tccr_cks_internal_t;

/**
 * @enum rx_tmr_tccr_cks_external_t
 * @brief TCCR CKS[2:0] edge selection when CSS = external clock
 *
 * @par External Edge Selection (Table 30.5)
 * | CKS  | Edge                             |
 * |------|----------------------------------|
 * | 000b | Clock prohibited                 |
 * | 001b | Count at rising edge             |
 * | 010b | Count at falling edge            |
 * | 011b | Count at both rising and falling |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tccr_cks_ext_rising  = 0x1, /**< 001b: rising-edge count         */
  k_tmr_tccr_cks_ext_falling = 0x2, /**< 010b: falling-edge count        */
  k_tmr_tccr_cks_ext_both    = 0x3, /**< 011b: both-edge count           */
} rx_tmr_tccr_cks_external_t;

/* =============================================================================
 * TCSTR - Timer Counter Start Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_tmr_tcstr_bits_t
 * @brief TCSTR bit positions
 *
 * @par Bit Layout (8-bit)
 * | Bits  | Field | R/W | Description                                |
 * |-------|-------|-----|--------------------------------------------|
 * | 7:1   | -     | R/W | Reserved                                   |
 * | 0     | TCS   | R/W | Count state from ELC (0=stopped, 1=running)|
 *
 * @note Manual: Ch30.2.7 - Timer Counter Start Register (TCSTR)
 * @note TCS is only valid when ELC event controller drives the channel;
 * software should write 0 to stop and leave 1 for ELC to set.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_tcstr_tcs = (1U << 0), /**< Timer counter status (ELC) */
} rx_tmr_tcstr_bits_t;

/* =============================================================================
 * Module Stop Control Bits
 * =============================================================================
 */

/**
 * @enum rx_tmr_mstpcra_t
 * @brief MSTPCRA bits that control the TMR module stop state
 *
 * @details
 * Unit 0 (TMR0/TMR1) is gated by MSTPA5.
 * Unit 1 (TMR2/TMR3) is gated by MSTPA4.
 *
 * @note Manual: Ch11 Module Stop Control Register A (MSTPCRA)
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_tmr_mstpcra_mstpa4 = (1U << 4), /**< Unit 1 (TMR2/TMR3) stop */
  k_tmr_mstpcra_mstpa5 = (1U << 5), /**< Unit 0 (TMR0/TMR1) stop */
} rx_tmr_mstpcra_t;

/* =============================================================================
 * SELECTB Interrupt Source Numbers
 * =============================================================================
 */

/**
 * @enum rx_tmr_intb_source_t
 * @brief TMR Software Configurable Interrupt B source numbers
 *
 * @details
 * TMR compare-match and overflow interrupts are routed through ICU SELECTB.
 * These source numbers are written into an SLIBRn register (n in 128..207)
 * to map a TMR event onto the corresponding interrupt vector.
 *
 * @note Manual: Ch15 Table 15.3 - Software Configurable Interrupt B sources
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_tmr_intb_cmia0 = 3,  /**< TMR0 TCORA compare match */
  k_tmr_intb_cmib0 = 4,  /**< TMR0 TCORB compare match */
  k_tmr_intb_ovi0  = 5,  /**< TMR0 TCNT overflow       */
  k_tmr_intb_cmia1 = 6,  /**< TMR1 TCORA compare match */
  k_tmr_intb_cmib1 = 7,  /**< TMR1 TCORB compare match */
  k_tmr_intb_ovi1  = 8,  /**< TMR1 TCNT overflow       */
  k_tmr_intb_cmia2 = 9,  /**< TMR2 TCORA compare match */
  k_tmr_intb_cmib2 = 10, /**< TMR2 TCORB compare match */
  k_tmr_intb_ovi2  = 11, /**< TMR2 TCNT overflow       */
  k_tmr_intb_cmia3 = 12, /**< TMR3 TCORA compare match */
  k_tmr_intb_cmib3 = 13, /**< TMR3 TCORB compare match */
  k_tmr_intb_ovi3  = 14, /**< TMR3 TCNT overflow       */
} rx_tmr_intb_source_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

static_assert(k_tmr0_base_addr == 0x00088200, "TMR0 base address incorrect");
static_assert(k_tmr1_base_addr == 0x00088201, "TMR1 base address incorrect");
static_assert(k_tmr2_base_addr == 0x00088210, "TMR2 base address incorrect");
static_assert(k_tmr3_base_addr == 0x00088211, "TMR3 base address incorrect");
static_assert((k_tmr2_base_addr - k_tmr0_base_addr) == k_tmr_unit_spacing,
              "TMR unit spacing incorrect");

static_assert(sizeof(rx_tmr_channel_regs_t) == 14, "TMR channel register block size incorrect");
static_assert(offsetof(rx_tmr_channel_regs_t, tcr) == 0x00, "TCR offset incorrect");
static_assert(offsetof(rx_tmr_channel_regs_t, tcsr) == 0x02, "TCSR offset incorrect");
static_assert(offsetof(rx_tmr_channel_regs_t, tcora) == 0x04, "TCORA offset incorrect");
static_assert(offsetof(rx_tmr_channel_regs_t, tcorb) == 0x06, "TCORB offset incorrect");
static_assert(offsetof(rx_tmr_channel_regs_t, tcnt) == 0x08, "TCNT offset incorrect");
static_assert(offsetof(rx_tmr_channel_regs_t, tccr) == 0x0A, "TCCR offset incorrect");
static_assert(offsetof(rx_tmr_channel_regs_t, tcstr) == 0x0C, "TCSTR offset incorrect");

static_assert(sizeof(rx_tmr_cascade_regs_t) == 12, "TMR cascade register block size incorrect");
static_assert(offsetof(rx_tmr_cascade_regs_t, tcora) == 0x04, "Cascade TCORA offset incorrect");
static_assert(offsetof(rx_tmr_cascade_regs_t, tcorb) == 0x06, "Cascade TCORB offset incorrect");
static_assert(offsetof(rx_tmr_cascade_regs_t, tcnt) == 0x08, "Cascade TCNT offset incorrect");
static_assert(offsetof(rx_tmr_cascade_regs_t, tccr) == 0x0A, "Cascade TCCR offset incorrect");

static_assert(k_tmr_tcr_cmieb == 0x80, "TCR CMIEB bit value incorrect");
static_assert(k_tmr_tcr_cmiea == 0x40, "TCR CMIEA bit value incorrect");
static_assert(k_tmr_tcr_ovie == 0x20, "TCR OVIE bit value incorrect");
static_assert(k_tmr_tcr_cclr_cmp_match_a == 0x08, "TCR CCLR match A incorrect");
static_assert(k_tmr_tcr_cclr_cmp_match_b == 0x10, "TCR CCLR match B incorrect");

static_assert(k_tmr_tccr_tmris == 0x80, "TCCR TMRIS bit value incorrect");
static_assert(k_tmr_tccr_css_internal == 0x08, "TCCR CSS internal value incorrect");
static_assert(k_tmr_tccr_css_cascade == 0x18, "TCCR CSS cascade value incorrect");

static_assert(k_tmr_tcstr_tcs == 0x01, "TCSTR TCS bit value incorrect");
static_assert(k_tmr_mstpcra_mstpa5 == 0x20, "MSTPCRA.MSTPA5 bit value incorrect");
static_assert(k_tmr_mstpcra_mstpa4 == 0x10, "MSTPCRA.MSTPA4 bit value incorrect");

#ifdef __cplusplus
}
#endif
