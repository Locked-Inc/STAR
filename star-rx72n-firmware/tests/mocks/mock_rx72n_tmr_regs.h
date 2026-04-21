/**
 * @file mock_rx72n_tmr_regs.h
 * @brief Mock TMR Register Definitions for Host-Side Unit Testing
 *
 * @details
 * Shadow header that replaces the real libs/rx_hal/inc/rx72n_tmr_regs.h in
 * unit-test builds. Duplicates the register types, bit enums, and SELECTB
 * source numbers, and redirects the tmr0()/tmr1()/tmr2()/tmr3() accessors
 * to RAM-based storage so tests can observe every register write.
 *
 * ## Unit-vs-Channel Storage Model
 *
 * On real hardware, TMR0 and TMR1 share a single 16-byte register block
 * with byte-interleaved fields (TMR0 at even offsets, TMR1 at odd offsets).
 * TMR2 and TMR3 share the next 16-byte block. To mirror that, each unit's
 * mock storage is a single 32-byte buffer, with the even-channel accessor
 * returning a pointer to offset 0 and the odd-channel accessor returning
 * a pointer to offset 1. Writes to `tmr0()->tcr` and `tmr1()->tcr` therefore
 * land on different bytes of the same buffer, matching the hardware.
 *
 * @see rx_tmr.h TMR HAL driver API
 * @see rx_tmr.c TMR HAL driver implementation
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#ifdef UNIT_TEST

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Register Structures (mirror real hardware layout)
 * =============================================================================
 */

/**
 * @brief Mock TMR single-channel register struct
 *
 * @details
 * Byte-interleaved layout exactly as rx_tmr_channel_regs_t in the real
 * header: even channels access the base of a 16-byte block, odd channels
 * access base+1, and the "pair" bytes correspond to the other channel's
 * fields in the same block.
 *
 * @invariant sizeof(rx_tmr_channel_regs_t) == 14
 * @since Version 1.0.0
 */
typedef struct {
  volatile uint8_t tcr;        /**< Timer Control Register            @ +0x00 */
  volatile uint8_t tcr_pair;   /**< Paired channel TCR byte           @ +0x01 */
  volatile uint8_t tcsr;       /**< Timer Control/Status              @ +0x02 */
  volatile uint8_t tcsr_pair;  /**< Paired channel TCSR byte          @ +0x03 */
  volatile uint8_t tcora;      /**< Time Constant A                   @ +0x04 */
  volatile uint8_t tcora_pair; /**< Paired channel TCORA byte         @ +0x05 */
  volatile uint8_t tcorb;      /**< Time Constant B                   @ +0x06 */
  volatile uint8_t tcorb_pair; /**< Paired channel TCORB byte         @ +0x07 */
  volatile uint8_t tcnt;       /**< Timer Counter                     @ +0x08 */
  volatile uint8_t tcnt_pair;  /**< Paired channel TCNT byte          @ +0x09 */
  volatile uint8_t tccr;       /**< Timer Counter Control             @ +0x0A */
  volatile uint8_t tccr_pair;  /**< Paired channel TCCR byte          @ +0x0B */
  volatile uint8_t tcstr;      /**< Timer Counter Start               @ +0x0C */
  volatile uint8_t tcstr_pair; /**< Paired channel TCSTR byte         @ +0x0D */
} rx_tmr_channel_regs_t;

/**
 * @brief Mock TMR cascade 16-bit register view
 * @invariant sizeof(rx_tmr_cascade_regs_t) == 12
 * @since Version 1.0.0
 */
typedef struct {
  volatile uint8_t  reserved_tcr[2];  /**< TCR bytes         @ +0x00..0x01 */
  volatile uint8_t  reserved_tcsr[2]; /**< TCSR bytes        @ +0x02..0x03 */
  volatile uint16_t tcora;            /**< 16-bit TCORA      @ +0x04       */
  volatile uint16_t tcorb;            /**< 16-bit TCORB      @ +0x06       */
  volatile uint16_t tcnt;             /**< 16-bit TCNT       @ +0x08       */
  volatile uint16_t tccr;             /**< 16-bit TCCR       @ +0x0A       */
} rx_tmr_cascade_regs_t;

/* =============================================================================
 * Channel / Bit / Enum Constants (values mirror the real header)
 * =============================================================================
 */

/** @brief Total TMR channel count (TMR0..TMR3) */
typedef enum : uint8_t {
  k_tmr_channel_count = 4,
} rx_tmr_counts_t;

/** @brief TCR bit masks */
typedef enum : uint8_t {
  k_tmr_tcr_cmieb = (1U << 7),
  k_tmr_tcr_cmiea = (1U << 6),
  k_tmr_tcr_ovie  = (1U << 5),
} rx_tmr_tcr_bits_t;

/** @brief TCR CCLR counter clear select */
typedef enum : uint8_t {
  k_tmr_tcr_cclr_disabled     = (0U << 3),
  k_tmr_tcr_cclr_cmp_match_a  = (1U << 3),
  k_tmr_tcr_cclr_cmp_match_b  = (2U << 3),
  k_tmr_tcr_cclr_external_sig = (3U << 3),
} rx_tmr_tcr_cclr_t;

/** @brief TCR masks */
typedef enum : uint8_t {
  k_tmr_tcr_cclr_mask = 0x18,
} rx_tmr_tcr_masks_t;

/** @brief TCSR bits */
typedef enum : uint8_t {
  k_tmr_tcsr_adte = (1U << 4),
} rx_tmr_tcsr_bits_t;

/** @brief TCSR OSA/OSB output select */
typedef enum : uint8_t {
  k_tmr_tcsr_output_no_change = 0x0,
  k_tmr_tcsr_output_low       = 0x1,
  k_tmr_tcsr_output_high      = 0x2,
  k_tmr_tcsr_output_toggle    = 0x3,
} rx_tmr_tcsr_output_t;

/** @brief TCSR field shifts */
typedef enum : uint8_t {
  k_tmr_tcsr_osa_shift = 0,
  k_tmr_tcsr_osb_shift = 2,
} rx_tmr_tcsr_shift_t;

/** @brief TCSR masks */
typedef enum : uint8_t {
  k_tmr_tcsr_osa_mask     = 0x03,
  k_tmr_tcsr_osb_mask     = 0x0C,
  k_tmr_tcsr_odd_reserved = 0x10,
} rx_tmr_tcsr_masks_t;

/** @brief TCCR bits */
typedef enum : uint8_t {
  k_tmr_tccr_tmris = (1U << 7),
} rx_tmr_tccr_bits_t;

/** @brief TCCR field shifts */
typedef enum : uint8_t {
  k_tmr_tccr_cks_shift = 0,
  k_tmr_tccr_css_shift = 3,
} rx_tmr_tccr_shift_t;

/** @brief TCCR masks */
typedef enum : uint8_t {
  k_tmr_tccr_cks_mask = 0x07,
  k_tmr_tccr_css_mask = 0x18,
} rx_tmr_tccr_masks_t;

/** @brief TCCR CSS clock source select */
typedef enum : uint8_t {
  k_tmr_tccr_css_external = (0U << 3),
  k_tmr_tccr_css_internal = (1U << 3),
  k_tmr_tccr_css_cascade  = (3U << 3),
} rx_tmr_tccr_css_t;

/** @brief TCCR CKS internal-clock divider values */
typedef enum : uint8_t {
  k_tmr_tccr_cks_prohibited   = 0x0,
  k_tmr_tccr_cks_pclk_div1    = 0x1,
  k_tmr_tccr_cks_pclk_div2    = 0x2,
  k_tmr_tccr_cks_pclk_div8    = 0x3,
  k_tmr_tccr_cks_pclk_div32   = 0x4,
  k_tmr_tccr_cks_pclk_div64   = 0x5,
  k_tmr_tccr_cks_pclk_div1024 = 0x6,
  k_tmr_tccr_cks_pclk_div8192 = 0x7,
} rx_tmr_tccr_cks_internal_t;

/** @brief TCCR CKS external-edge values */
typedef enum : uint8_t {
  k_tmr_tccr_cks_ext_rising  = 0x1,
  k_tmr_tccr_cks_ext_falling = 0x2,
  k_tmr_tccr_cks_ext_both    = 0x3,
} rx_tmr_tccr_cks_external_t;

/** @brief TCSTR bits */
typedef enum : uint8_t {
  k_tmr_tcstr_tcs = (1U << 0),
} rx_tmr_tcstr_bits_t;

/** @brief MSTPCRA bits for TMR units */
typedef enum : uint32_t {
  k_tmr_mstpcra_mstpa4 = (1U << 4),
  k_tmr_mstpcra_mstpa5 = (1U << 5),
} rx_tmr_mstpcra_t;

/** @brief SELECTB source numbers for TMR events */
typedef enum : uint8_t {
  k_tmr_intb_cmia0 = 3,
  k_tmr_intb_cmib0 = 4,
  k_tmr_intb_ovi0  = 5,
  k_tmr_intb_cmia1 = 6,
  k_tmr_intb_cmib1 = 7,
  k_tmr_intb_ovi1  = 8,
  k_tmr_intb_cmia2 = 9,
  k_tmr_intb_cmib2 = 10,
  k_tmr_intb_ovi2  = 11,
  k_tmr_intb_cmia3 = 12,
  k_tmr_intb_cmib3 = 13,
  k_tmr_intb_ovi3  = 14,
} rx_tmr_intb_source_t;

/* =============================================================================
 * Mock Register Storage (defined in the test translation unit)
 * =============================================================================
 */

/**
 * @enum rx_tmr_mock_sizes_t
 * @brief Buffer sizing constants for mock TMR units
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mock_tmr_unit_bytes = 32, /**< Per-unit shared buffer size (safe upper bound) */
  k_mock_tmr_unit_count = 2,  /**< Number of TMR units (unit 0 = TMR0/1, unit 1 = TMR2/3) */
} rx_tmr_mock_sizes_t;

/**
 * @brief Mock TMR unit buffers - `g_mock_tmr_unit[0]` holds TMR0/TMR1,
 *        `g_mock_tmr_unit[1]` holds TMR2/TMR3.
 */
extern uint8_t g_mock_tmr_unit[k_mock_tmr_unit_count][k_mock_tmr_unit_bytes];

/* =============================================================================
 * Inline Accessors
 * =============================================================================
 */

/** @brief Mock pointer to TMR0 (unit 0 even channel, offset 0) */
static inline volatile rx_tmr_channel_regs_t* tmr0(void)
{
  return (volatile rx_tmr_channel_regs_t*)&g_mock_tmr_unit[0][0];
}

/** @brief Mock pointer to TMR1 (unit 0 odd channel, offset 1) */
static inline volatile rx_tmr_channel_regs_t* tmr1(void)
{
  return (volatile rx_tmr_channel_regs_t*)&g_mock_tmr_unit[0][1];
}

/** @brief Mock pointer to TMR2 (unit 1 even channel, offset 0) */
static inline volatile rx_tmr_channel_regs_t* tmr2(void)
{
  return (volatile rx_tmr_channel_regs_t*)&g_mock_tmr_unit[1][0];
}

/** @brief Mock pointer to TMR3 (unit 1 odd channel, offset 1) */
static inline volatile rx_tmr_channel_regs_t* tmr3(void)
{
  return (volatile rx_tmr_channel_regs_t*)&g_mock_tmr_unit[1][1];
}

/** @brief Mock pointer to TMR01 cascade view */
static inline volatile rx_tmr_cascade_regs_t* tmr01_cascade(void)
{
  return (volatile rx_tmr_cascade_regs_t*)&g_mock_tmr_unit[0][0];
}

/** @brief Mock pointer to TMR23 cascade view */
static inline volatile rx_tmr_cascade_regs_t* tmr23_cascade(void)
{
  return (volatile rx_tmr_cascade_regs_t*)&g_mock_tmr_unit[1][0];
}

#ifdef __cplusplus
}
#endif

#endif /* UNIT_TEST */
