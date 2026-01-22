/* lib/rx_hal/inc/rx72n_iwdt_regs.h */

/**
 * @file rx72n_iwdt_regs.h
 * @brief RX72N IWDT Watchdog Timer Register Definitions
 *
 * Register definitions for the Independent Watchdog Timer (IWDT) with all
 * bit field definitions.
 *
 * The IWDT runs on a dedicated 120 kHz oscillator, independent of the main
 * system clock. If the main clock fails, the IWDT can still reset the chip.
 *
 * Key Features:
 * - 14-bit down counter with dedicated clock source
 * - Configurable timeout: 128ms to 16384ms
 * - Window protection (optional) to detect too-early refresh
 * - Generates reset or NMI on timeout
 *
 * References:
 * - RX72N Group User's Manual: Hardware, Section 25 (IWDT)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_IWDT_REGS_H
#define STAR_RX72N_IWDT_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Independent Watchdog Timer (IWDT)
 * =============================================================================
 */

/** @brief IWDT base address */
typedef enum : uint32_t {
  k_iwdt_base_addr = 0x00088030, /**< IWDT register base address */
} rx_iwdt_addresses_t;

/** @brief IWDT register reserved field sizes */
typedef enum : uint8_t {
  k_iwdt_reserved_after_iwdtrr_bytes  = 1, /**< Reserved byte after IWDTRR */
  k_iwdt_reserved_after_iwdtrcr_bytes = 1, /**< Reserved byte after IWDTRCR */
} iwdt_reserved_sizes_t;

/**
 * @brief IWDT Register Map
 * @details
 * Independent Watchdog Timer (IWDT) registers for system watchdog functionality.
 * Uses dedicated 120 kHz oscillator, independent of main system clock.
 * Base address: 0x00088030
 */
typedef struct __attribute__((packed)) {
  volatile uint8_t  iwdtrr; /**< Refresh Register (write 0x00 then 0xFF to refresh) */
  uint8_t           reserved0[k_iwdt_reserved_after_iwdtrr_bytes]; /**< Reserved */
  volatile uint16_t iwdtcr;  /**< Control Register (timeout, clock, window) */
  volatile uint16_t iwdtsr;  /**< Status Register (counter value, flags) */
  volatile uint8_t  iwdtrcr; /**< Reset Control Register (reset/NMI select) */
  uint8_t           reserved1[k_iwdt_reserved_after_iwdtrcr_bytes]; /**< Reserved */
  volatile uint8_t  iwdtcstpr; /**< Count Stop Control Register (sleep mode) */
} rx_iwdt_regs_t;

/**
 * @brief Get pointer to IWDT registers
 * @return Volatile pointer to IWDT register structure
 */
static inline volatile rx_iwdt_regs_t* iwdt(void)
{
  return (volatile rx_iwdt_regs_t*)k_iwdt_base_addr;
}

/* IWDT Refresh Register (IWDTRR) - Write sequence to refresh */
typedef enum : uint8_t {
  k_iwdt_refresh_start = 0x00, /**< First write value */
  k_iwdt_refresh_end   = 0xFF, /**< Second write value */
} iwdt_refresh_sequence_t;

/** @brief IWDT Control Register (IWDTCR) Bit Shift Positions */
typedef enum : uint8_t {
  k_iwdt_iwdtcr_cks_shift  = 4,  /**< Clock Division Ratio (CKS) bit shift */
  k_iwdt_iwdtcr_rpes_shift = 8,  /**< Window Start Position (RPES) bit shift */
  k_iwdt_iwdtcr_rpss_shift = 12, /**< Window End Position (RPSS) bit shift */
} iwdt_iwdtcr_shifts_t;

/* IWDT Control Register (IWDTCR) Bit Definitions */
typedef enum : uint16_t {
  /* Timeout Period Select (TOPS) - bits 1:0 */
  k_iwdt_tops_1024  = 0x0000, /**< 1024 cycles (~8.5ms at 120kHz) */
  k_iwdt_tops_4096  = 0x0001, /**< 4096 cycles (~34ms) */
  k_iwdt_tops_8192  = 0x0002, /**< 8192 cycles (~68ms) */
  k_iwdt_tops_16384 = 0x0003, /**< 16384 cycles (~137ms) */

  /* Clock Division Ratio (CKS) - bits 7:4 */
  k_iwdt_cks_div_1   = (0x00 << k_iwdt_iwdtcr_cks_shift),  /**< IWDTCLK/1 */
  k_iwdt_cks_div_16  = (0x02 << k_iwdt_iwdtcr_cks_shift),  /**< IWDTCLK/16 */
  k_iwdt_cks_div_32  = (0x03 << k_iwdt_iwdtcr_cks_shift),  /**< IWDTCLK/32 */
  k_iwdt_cks_div_64  = (0x04 << k_iwdt_iwdtcr_cks_shift),  /**< IWDTCLK/64 */
  k_iwdt_cks_div_128 = (0x0F << k_iwdt_iwdtcr_cks_shift),  /**< IWDTCLK/128 (~1s at 16384 cycles) */
  k_iwdt_cks_div_256 = (0x05 << k_iwdt_iwdtcr_cks_shift),  /**< IWDTCLK/256 */

  /* Window Start Position (RPES) - bits 9:8 */
  k_iwdt_rpes_75 = (0x00 << k_iwdt_iwdtcr_rpes_shift), /**< 75% (refresh allowed after 25% elapsed) */
  k_iwdt_rpes_50 = (0x01 << k_iwdt_iwdtcr_rpes_shift), /**< 50% */
  k_iwdt_rpes_25 = (0x02 << k_iwdt_iwdtcr_rpes_shift), /**< 25% */
  k_iwdt_rpes_0  = (0x03 << k_iwdt_iwdtcr_rpes_shift), /**< 0% (refresh allowed immediately) */

  /* Window End Position (RPSS) - bits 13:12 */
  k_iwdt_rpss_100 = (0x00 << k_iwdt_iwdtcr_rpss_shift), /**< 100% (no early cutoff) */
  k_iwdt_rpss_75  = (0x01 << k_iwdt_iwdtcr_rpss_shift), /**< 75% */
  k_iwdt_rpss_50  = (0x02 << k_iwdt_iwdtcr_rpss_shift), /**< 50% */
  k_iwdt_rpss_25  = (0x03 << k_iwdt_iwdtcr_rpss_shift), /**< 25% */
} iwdt_iwdtcr_bits_t;

/* IWDT Status Register (IWDTSR) Bit Definitions */
typedef enum : uint16_t {
  k_iwdt_sr_cntval_mask   = 0x3FFF, /**< Down counter value (bits 13:0) */
  k_iwdt_sr_undff_shift   = 14,     /**< Underflow flag bit position */
  k_iwdt_sr_refef_shift   = 15,     /**< Refresh error flag bit position */
  k_iwdt_sr_undff         = (1U << k_iwdt_sr_undff_shift), /**< Underflow flag (reset occurred) */
  k_iwdt_sr_refef         = (1U << k_iwdt_sr_refef_shift), /**< Refresh error flag (window violation) */
} iwdt_iwdtsr_bits_t;

/* IWDT Reset Control Register (IWDTRCR) Bit Definitions */
typedef enum : uint8_t {
  k_iwdt_rstirqs_pos      = 7, /**< Reset/Interrupt Select bit position */
  k_iwdt_rcr_rstirqs_mask = (1U << k_iwdt_rstirqs_pos), /**< Reset/Interrupt Select bit mask */
  k_iwdt_rstirqs_reset    = (1U << k_iwdt_rstirqs_pos), /**< Generate reset on timeout */
  k_iwdt_rstirqs_nmi      = 0x00, /**< Generate NMI on timeout */
} iwdt_iwdtrcr_bits_t;

/** @brief IWDTCSTPR bit field positions */
typedef enum : uint8_t {
  k_iwdt_cstpr_slcstp_pos = 7, /**< Sleep Mode Count Stop bit position */
} iwdt_iwdtcstpr_shifts_t;

/* IWDT Count Stop Control Register (IWDTCSTPR) Bit Definitions */
typedef enum : uint8_t {
  k_iwdt_cstpr_slcstp_mask = (1U << k_iwdt_cstpr_slcstp_pos), /**< Sleep Mode Count Stop bit mask */
  k_iwdt_slcstp_stop       = (1U << k_iwdt_cstpr_slcstp_pos), /**< Stop counting during sleep */
  k_iwdt_slcstp_continue   = 0x00, /**< Continue counting during sleep */
} iwdt_iwdtcstpr_bits_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* Verify base address matches Hardware Manual */
_Static_assert(k_iwdt_base_addr == 0x00088030, "IWDT base address incorrect");

/* Verify register structure layout */
_Static_assert(sizeof(rx_iwdt_regs_t) == 9, "IWDT register structure size incorrect");
_Static_assert(offsetof(rx_iwdt_regs_t, iwdtrr) == 0x00, "IWDTRR offset incorrect");
_Static_assert(offsetof(rx_iwdt_regs_t, iwdtcr) == 0x02, "IWDTCR offset incorrect");
_Static_assert(offsetof(rx_iwdt_regs_t, iwdtsr) == 0x04, "IWDTSR offset incorrect");
_Static_assert(offsetof(rx_iwdt_regs_t, iwdtrcr) == 0x06, "IWDTRCR offset incorrect");
_Static_assert(offsetof(rx_iwdt_regs_t, iwdtcstpr) == 0x08, "IWDTCSTPR offset incorrect");

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_IWDT_REGS_H */
