/* lib/rx_hal/inc/rx72n_system_regs.h */

/**
 * @file rx72n_system_regs.h
 * @brief RX72N System Control Register Definitions
 *
 * System control registers for clock, power, and module stop control.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_SYSTEM_REGS_H
#define STAR_RX72N_SYSTEM_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * System Control Registers
 * =============================================================================
 */

/** @brief System register base address */
typedef enum {
  k_system_base_addr = 0x00080000, /**< System register base address */
} rx_system_addresses_t;

/** @brief System register reserved field sizes */
typedef enum {
  k_system_reserved_after_syscr1_bytes   = 2, /**< Reserved bytes after SYSCR1 */
  k_system_reserved_after_sbycr_bytes    = 2, /**< Reserved bytes after SBYCR */
  k_system_reserved_after_prcr_bytes     = 2, /**< Reserved bytes after PRCR */
  k_system_reserved_after_pllcr2_bytes   = 5, /**< Reserved bytes after PLLCR2 */
  k_system_reserved_after_bckcr_bytes    = 1, /**< Reserved byte after BCKCR */
  k_system_reserved_after_hococr2_bytes  = 4, /**< Reserved bytes after HOCOCR2 */
  k_system_reserved_after_oscovfsr_bytes = 3, /**< Reserved bytes after OSCOVFSR */
} system_reserved_sizes_t;

/**
 * @brief System Control Register Map
 * @details
 * System control registers for clock configuration, power management,
 * and module stop control.
 * Base address: 0x00080000
 */
typedef struct {
  volatile uint16_t syscr0; /**< System Control Register 0 */
  volatile uint16_t syscr1; /**< System Control Register 1 */
  uint8_t           reserved0[k_system_reserved_after_syscr1_bytes]; /**< Reserved */
  volatile uint16_t sbycr;                                          /**< Standby Control Register */
  uint8_t           reserved1[k_system_reserved_after_sbycr_bytes]; /**< Reserved */
  volatile uint16_t prcr;                                           /**< Protection Register */
  uint8_t           reserved1a[k_system_reserved_after_prcr_bytes]; /**< Reserved */
  volatile uint32_t mstpcra; /**< Module Stop Control Register A */
  volatile uint32_t mstpcrb; /**< Module Stop Control Register B */
  volatile uint32_t mstpcrc; /**< Module Stop Control Register C */
  volatile uint32_t mstpcrd; /**< Module Stop Control Register D */
  volatile uint32_t sckcr;   /**< System Clock Control Register */
  volatile uint16_t sckcr2;  /**< System Clock Control Register 2 */
  volatile uint16_t sckcr3;  /**< System Clock Control Register 3 */
  volatile uint16_t pllcr;   /**< PLL Control Register */
  volatile uint8_t  pllcr2;  /**< PLL Control Register 2 */
  uint8_t           reserved2[k_system_reserved_after_pllcr2_bytes]; /**< Reserved */
  volatile uint8_t  bckcr; /**< External Bus Clock Control Register */
  uint8_t           reserved3[k_system_reserved_after_bckcr_bytes]; /**< Reserved */
  volatile uint8_t  mosccr;  /**< Main Clock Oscillator Control */
  volatile uint8_t  sosccr;  /**< Sub-Clock Oscillator Control */
  volatile uint8_t  lococr;  /**< Low-Speed On-Chip Oscillator Control */
  volatile uint8_t  ilococr; /**< IWDT-Dedicated On-Chip Oscillator Control */
  volatile uint8_t  hococr;  /**< High-Speed On-Chip Oscillator Control */
  volatile uint8_t  hococr2; /**< High-Speed On-Chip Oscillator Control 2 */
  uint8_t           reserved4[k_system_reserved_after_hococr2_bytes]; /**< Reserved */
  volatile uint8_t  oscovfsr; /**< Oscillation Stabilization Flag */
  uint8_t           reserved5[k_system_reserved_after_oscovfsr_bytes]; /**< Reserved */
  volatile uint8_t  ostdcr; /**< Oscillation Stop Detection Control */
  volatile uint8_t  ostdsr; /**< Oscillation Stop Detection Status */
} rx_system_regs_t;

/**
 * @brief Get pointer to system registers
 * @return Volatile pointer to system register structure
 * @note Named system_regs() instead of system() to avoid naming conflicts
 */
static inline volatile rx_system_regs_t* system_regs(void)
{
  return (volatile rx_system_regs_t*)k_system_base_addr;
}

/* Module stop bits for MSTPCRA register */
typedef enum {
  k_mstpa_cmt23 = 14, /**< CMT2/CMT3 module stop bit in MSTPCRA */
} rx_module_stop_bits_a_t;

/* Module stop bits for MSTPCRB register */
typedef enum {
  k_mstpb_usb0 = 19, /**< USB0 module stop bit in MSTPCRB */
  k_mstpb_crc  = 23, /**< CRC module stop bit in MSTPCRB */
} rx_module_stop_bits_b_t;

/* =============================================================================
 * Reset Status Registers (RSTSR)
 * =============================================================================
 */

/**
 * @brief Reset status register addresses
 * @details
 * IMPORTANT: RSTSR registers are NOT contiguous in memory!
 * - RSTSR0/1 are adjacent at 0x0008C290-0x0008C291
 * - RSTSR2 is at a separate address 0x000800C0
 * Verified against RX72N Group User's Manual Hardware (R01UH0824EJ0120 Rev.1.20)
 */
typedef enum {
  k_rstsr0_addr = 0x0008C290, /**< Reset Status Register 0 address (page 286) */
  k_rstsr1_addr = 0x0008C291, /**< Reset Status Register 1 address (page 288) */
  k_rstsr2_addr = 0x000800C0, /**< Reset Status Register 2 address (page 289) */
} rx_rstsr_addresses_t;

/**
 * @brief Reset Status Registers 0 and 1 (contiguous)
 * @details
 * RSTSR0 and RSTSR1 are adjacent in memory at 0x0008C290-0x0008C291.
 */
typedef struct {
  volatile uint8_t rstsr0; /**< Reset Status Register 0 (voltage, power-on) */
  volatile uint8_t rstsr1; /**< Reset Status Register 1 (cold/warm start) */
} rx_rstsr01_regs_t;

/**
 * @brief Get pointer to RSTSR0/RSTSR1 registers
 * @return Volatile pointer to RSTSR0/1 register structure
 */
static inline volatile rx_rstsr01_regs_t* rstsr01(void)
{
  return (volatile rx_rstsr01_regs_t*)k_rstsr0_addr;
}

/**
 * @brief Get pointer to RSTSR2 register
 * @return Volatile pointer to RSTSR2 register
 * @note RSTSR2 is at a separate address from RSTSR0/1
 */
static inline volatile uint8_t* rstsr2(void)
{
  return (volatile uint8_t*)k_rstsr2_addr;
}

/* RSTSR0 bit definitions (page 286) */
typedef enum {
  k_rstsr0_porf    = (1 << 0), /**< Power-On Reset Detect Flag */
  k_rstsr0_lvd0rf  = (1 << 1), /**< Voltage-Monitoring 0 Reset Detect Flag */
  k_rstsr0_lvd1rf  = (1 << 2), /**< Voltage-Monitoring 1 Reset Detect Flag */
  k_rstsr0_lvd2rf  = (1 << 3), /**< Voltage-Monitoring 2 Reset Detect Flag */
  k_rstsr0_dpsrstf = (1 << 7), /**< Deep Software Standby Reset Flag */
} rstsr0_bits_t;

/* RSTSR1 bit definitions (page 288) */
typedef enum {
  k_rstsr1_cwsf = (1 << 0), /**< Cold/Warm Start Determination Flag */
} rstsr1_bits_t;

/* RSTSR2 bit definitions (page 289) */
typedef enum {
  k_rstsr2_iwdtrf = (1 << 0), /**< Independent Watchdog Timer Reset Detect Flag */
  k_rstsr2_wdtrf  = (1 << 1), /**< Watchdog Timer Reset Detect Flag */
  k_rstsr2_swrf   = (1 << 2), /**< Software Reset Detect Flag */
} rstsr2_bits_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_SYSTEM_REGS_H */
