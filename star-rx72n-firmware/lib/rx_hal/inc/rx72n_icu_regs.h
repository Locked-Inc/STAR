/* lib/rx_hal/inc/rx72n_icu_regs.h */

/**
 * @file rx72n_icu_regs.h
 * @brief RX72N ICU Interrupt Controller Register Definitions
 *
 * Register definitions for the Interrupt Controller Unit (ICU) with interrupt
 * vector numbers and priority levels.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_ICU_REGS_H
#define STAR_RX72N_ICU_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Interrupt Controller (ICU)
 * =============================================================================
 */

/** @brief ICU register reserved field sizes */
typedef enum {
  k_icu_reserved_after_ier_bytes     = 192, /**< Reserved bytes after IER */
  k_icu_reserved_after_swint2r_bytes = 14,  /**< Reserved bytes after SWINT2R */
  k_icu_reserved_after_fir_bytes     = 14,  /**< Reserved bytes after FIR */
  k_icu_reserved_after_dmrsr_bytes   = 248, /**< Reserved bytes after DMRSR */
  k_icu_reserved_after_irqcr_bytes   = 16,  /**< Reserved bytes after IRQCR */
  k_icu_reserved_after_irqfltc_bytes = 58,  /**< Reserved bytes after IRQFLTC */
} icu_reserved_sizes_t;

/** @brief ICU register array sizes */
typedef enum {
  k_icu_ir_count     = 256, /**< Number of Interrupt Request Registers */
  k_icu_dtcer_count  = 256, /**< Number of DTC Enable Registers */
  k_icu_ier_count    = 32,  /**< Number of Interrupt Enable Registers */
  k_icu_ipr_count    = 256, /**< Number of Interrupt Priority Registers */
  k_icu_dmrsr_count  = 8,   /**< Number of DMAC Module Start Registers */
  k_icu_irqcr_count  = 16,  /**< Number of IRQ Control Registers */
  k_icu_irqflte_count = 2,  /**< Number of IRQ Filter Enable Registers */
  k_icu_irqfltc_count = 2,  /**< Number of IRQ Filter Clock Select Registers */
} icu_array_sizes_t;

/**
 * @brief ICU Register Map
 * @details
 * Interrupt Controller Unit (ICU) registers for managing interrupts.
 * Supports 256 interrupt sources with configurable priorities (0-15).
 * Base address: 0x00087000
 */
typedef struct {
  volatile uint8_t  ir[k_icu_ir_count];       /**< Interrupt Request Registers */
  volatile uint8_t  dtcer[k_icu_dtcer_count]; /**< DTC Enable Registers */
  volatile uint8_t  ier[k_icu_ier_count];     /**< Interrupt Enable Registers */
  uint8_t           reserved0[k_icu_reserved_after_ier_bytes]; /**< Reserved */
  volatile uint8_t  swintr;  /**< Software Interrupt Register */
  volatile uint8_t  swint2r; /**< Software Interrupt 2 Register */
  uint8_t           reserved1[k_icu_reserved_after_swint2r_bytes]; /**< Reserved */
  volatile uint16_t fir; /**< Fast Interrupt Register */
  uint8_t           reserved2[k_icu_reserved_after_fir_bytes]; /**< Reserved */
  volatile uint8_t  ipr[k_icu_ipr_count];    /**< Interrupt Priority Registers */
  volatile uint8_t  dmrsr[k_icu_dmrsr_count]; /**< DMAC Module Start Registers */
  uint8_t           reserved3[k_icu_reserved_after_dmrsr_bytes]; /**< Reserved */
  volatile uint8_t  irqcr[k_icu_irqcr_count]; /**< IRQ Control Registers */
  uint8_t           reserved4[k_icu_reserved_after_irqcr_bytes]; /**< Reserved */
  volatile uint8_t  irqflte[k_icu_irqflte_count]; /**< IRQ Filter Enable Registers */
  volatile uint16_t irqfltc[k_icu_irqfltc_count]; /**< IRQ Filter Clock Select Registers */
  uint8_t           reserved5[k_icu_reserved_after_irqfltc_bytes]; /**< Reserved */
  volatile uint32_t nmicr;  /**< NMI Control Register */
  volatile uint8_t  nmier;  /**< NMI Enable Register */
  volatile uint8_t  nmisr;  /**< NMI Status Register */
  volatile uint8_t  nmiclr; /**< NMI Clear Register */
  volatile uint8_t  nmiflt; /**< NMI Filter Control Register */
} rx_icu_regs_t;

#define ICU_BASE_ADDR ((rx_icu_regs_t*)0x00087000)
#define ICU           (*ICU_BASE_ADDR)

/* Vector numbers */
typedef enum {
  k_vect_cmt0_cmi0 = 28, /**< CMT0 compare match interrupt */
  k_vect_cmt1_cmi1 = 29, /**< CMT1 compare match interrupt */
} rx_cmt_interrupt_vector_t;

/* Interrupt Priority Levels (0 = disabled, 1-15 = priority) */
typedef enum {
  k_ipr_level_disable = 0,  /**< Interrupt disabled */
  k_ipr_level_min     = 1,  /**< Minimum enabled priority */
  k_ipr_level_max     = 15, /**< Maximum priority */
} rx_interrupt_priority_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_ICU_REGS_H */
