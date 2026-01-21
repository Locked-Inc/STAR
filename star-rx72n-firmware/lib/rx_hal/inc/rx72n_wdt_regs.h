/* lib/rx_hal/inc/rx72n_wdt_regs.h */

/**
 * @file rx72n_wdt_regs.h
 * @brief RX72N WDT Watchdog Timer Register Definitions
 *
 * Register definitions for the Watchdog Timer (WDT) with all
 * bit field definitions.
 *
 * The WDT runs on the PCLKB peripheral clock and can be stopped in software,
 * making it suitable for development and debugging.
 *
 * Key Features:
 * - 14-bit down counter using PCLKB
 * - Configurable timeout: 256 to 16384 cycles
 * - Window protection (optional) to detect too-early refresh
 * - Can be stopped in software (unlike IWDT)
 * - Generates reset or NMI on timeout
 *
 * References:
 * - RX72N Group User's Manual: Hardware, Section 24 (WDT)
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_WDT_REGS_H
#define STAR_RX72N_WDT_REGS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Watchdog Timer (WDT)
 * =============================================================================
 */

/** @brief WDT base address */
typedef enum : uint32_t {
  k_wdt_base_addr = 0x00088020, /**< WDT register base address */
} rx_wdt_addresses_t;

/** @brief WDT register reserved field sizes */
typedef enum : uint8_t {
  k_wdt_reserved_after_wdtrr_bytes  = 1, /**< Reserved byte after WDTRR */
  k_wdt_reserved_after_wdtrcr_bytes = 1, /**< Reserved byte after WDTRCR */
} wdt_reserved_sizes_t;

/**
 * @brief WDT Register Map
 * @details
 * Watchdog Timer (WDT) registers for software-controllable watchdog functionality.
 * Uses PCLKB clock and can be stopped in software.
 * Base address: 0x00088020
 *
 * Note: WDT has 4 registers only (WDTRR, WDTCR, WDTSR, WDTRCR).
 * WDTCSTPR exists for IWDT but NOT for WDT.
 * Verified against RX72N Group User's Manual Hardware (R01UH0824EJ0120 Rev.1.20)
 */
typedef struct __attribute__((packed)) {
  volatile uint8_t  wdtrr; /**< Refresh Register (write 0x00 then 0xFF to refresh) */
  uint8_t           reserved0[k_wdt_reserved_after_wdtrr_bytes]; /**< Reserved */
  volatile uint16_t wdtcr;  /**< Control Register (timeout, clock, window) */
  volatile uint16_t wdtsr;  /**< Status Register (counter value, flags) */
  volatile uint8_t  wdtrcr; /**< Reset Control Register (reset/NMI select) */
  uint8_t           reserved1[k_wdt_reserved_after_wdtrcr_bytes]; /**< Reserved */
} rx_wdt_regs_t;

/**
 * @brief Get pointer to WDT registers
 * @return Volatile pointer to WDT register structure
 */
static inline volatile rx_wdt_regs_t* wdt(void)
{
  return (volatile rx_wdt_regs_t*)k_wdt_base_addr;
}

/* WDT Refresh Register (WDTRR) - Write sequence to refresh */
typedef enum : uint8_t {
  k_wdt_refresh_start = 0x00, /**< First write value */
  k_wdt_refresh_end   = 0xFF, /**< Second write value */
} wdt_refresh_sequence_t;

/* WDT Control Register (WDTCR) Bit Definitions */
/* Bit position constants for WDTCR fields */
enum : uint8_t {
  k_wdt_cr_cks_pos  = 4,  /**< Clock division ratio field position */
  k_wdt_cr_rpes_pos = 8,  /**< Window start position field position */
  k_wdt_cr_rpss_pos = 12, /**< Window end position field position */
};

typedef enum : uint16_t {
  /* Timeout Period Select (TOPS) - bits 1:0 */
  k_wdt_tops_1024  = 0x0000, /**< 1024 cycles */
  k_wdt_tops_4096  = 0x0001, /**< 4096 cycles */
  k_wdt_tops_8192  = 0x0002, /**< 8192 cycles */
  k_wdt_tops_16384 = 0x0003, /**< 16384 cycles */

  /* Clock Division Ratio (CKS) - bits 7:4 */
  k_wdt_cks_div_4    = (0x00 << k_wdt_cr_cks_pos), /**< PCLKB/4 */
  k_wdt_cks_div_64   = (0x04 << k_wdt_cr_cks_pos), /**< PCLKB/64 */
  k_wdt_cks_div_128  = (0x0F << k_wdt_cr_cks_pos), /**< PCLKB/128 */
  k_wdt_cks_div_256  = (0x05 << k_wdt_cr_cks_pos), /**< PCLKB/256 */
  k_wdt_cks_div_512  = (0x06 << k_wdt_cr_cks_pos), /**< PCLKB/512 */
  k_wdt_cks_div_2048 = (0x07 << k_wdt_cr_cks_pos), /**< PCLKB/2048 */
  k_wdt_cks_div_8192 = (0x08 << k_wdt_cr_cks_pos), /**< PCLKB/8192 */

  /* Window Start Position (RPES) - bits 9:8 */
  k_wdt_rpes_75 = (0x00 << k_wdt_cr_rpes_pos), /**< 75% (refresh allowed after 25% elapsed) */
  k_wdt_rpes_50 = (0x01 << k_wdt_cr_rpes_pos), /**< 50% */
  k_wdt_rpes_25 = (0x02 << k_wdt_cr_rpes_pos), /**< 25% */
  k_wdt_rpes_0  = (0x03 << k_wdt_cr_rpes_pos), /**< 0% (refresh allowed immediately) */

  /* Window End Position (RPSS) - bits 13:12 */
  k_wdt_rpss_100 = (0x00 << k_wdt_cr_rpss_pos), /**< 100% (no early cutoff) */
  k_wdt_rpss_75  = (0x01 << k_wdt_cr_rpss_pos), /**< 75% */
  k_wdt_rpss_50  = (0x02 << k_wdt_cr_rpss_pos), /**< 50% */
  k_wdt_rpss_25  = (0x03 << k_wdt_cr_rpss_pos), /**< 25% */
} wdt_wdtcr_bits_t;

/* WDT Status Register (WDTSR) Bit Definitions */
/* Bit position constants for WDTSR flags */
enum : uint8_t {
  k_wdt_sr_undff_pos = 14, /**< Underflow flag bit position */
  k_wdt_sr_refef_pos = 15, /**< Refresh error flag bit position */
};

typedef enum : uint16_t {
  k_wdt_sr_cntval_mask = 0x3FFF,                      /**< Down counter value (bits 13:0) */
  k_wdt_sr_undff       = (1 << k_wdt_sr_undff_pos),  /**< Underflow flag (reset occurred) */
  k_wdt_sr_refef       = (1 << k_wdt_sr_refef_pos),  /**< Refresh error flag (window violation) */
} wdt_wdtsr_bits_t;

/* WDT Reset Control Register (WDTRCR) Bit Definitions */
enum : uint8_t {
  k_wdt_rcr_rstirqs_pos = 7, /**< Reset/Interrupt select bit position */
};

typedef enum : uint8_t {
  k_wdt_rcr_rstirqs_mask = (1 << k_wdt_rcr_rstirqs_pos), /**< Reset/Interrupt Select bit mask */
  k_wdt_rstirqs_reset    = (1 << k_wdt_rcr_rstirqs_pos), /**< Generate reset on timeout */
  k_wdt_rstirqs_nmi      = 0x00,     /**< Generate NMI on timeout */
} wdt_wdtrcr_bits_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* Verify base address matches Hardware Manual */
_Static_assert(k_wdt_base_addr == 0x00088020, "WDT base address incorrect");

/* Verify register structure layout (WDT has 4 registers, 8 bytes total) */
_Static_assert(sizeof(rx_wdt_regs_t) == 8, "WDT register structure size incorrect");
_Static_assert(offsetof(rx_wdt_regs_t, wdtrr) == 0x00, "WDTRR offset incorrect");
_Static_assert(offsetof(rx_wdt_regs_t, wdtcr) == 0x02, "WDTCR offset incorrect");
_Static_assert(offsetof(rx_wdt_regs_t, wdtsr) == 0x04, "WDTSR offset incorrect");
_Static_assert(offsetof(rx_wdt_regs_t, wdtrcr) == 0x06, "WDTRCR offset incorrect");

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_WDT_REGS_H */
