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

/* ICU Register Structure */
typedef struct {
  volatile uint8_t  IR[256];    /* 0x000-0x0FF: Interrupt Request Registers */
  volatile uint8_t  DTCER[256]; /* 0x100-0x1FF: DTC Enable Registers */
  volatile uint8_t  IER[32];    /* 0x200-0x21F: Interrupt Enable Registers */
  uint8_t           RESERVED0[192];
  volatile uint8_t  SWINTR;  /* 0x2E0: Software Interrupt Register */
  volatile uint8_t  SWINT2R; /* 0x2E1: Software Interrupt 2 Register */
  uint8_t           RESERVED1[14];
  volatile uint16_t FIR; /* 0x2F0: Fast Interrupt Register */
  uint8_t           RESERVED2[14];
  volatile uint8_t  IPR[256]; /* 0x300-0x3FF: Interrupt Priority Registers */
  volatile uint8_t  DMRSR[8]; /* 0x400-0x407: DMAC Module Start Registers */
  uint8_t           RESERVED3[248];
  volatile uint8_t  IRQCR[16]; /* 0x500-0x50F: IRQ Control Registers */
  uint8_t           RESERVED4[16];
  volatile uint8_t  IRQFLTE[2]; /* 0x520-0x521: IRQ Filter Enable Registers */
  volatile uint16_t IRQFLTC[2]; /* 0x522-0x525: IRQ Filter Clock Select Registers */
  uint8_t           RESERVED5[58];
  volatile uint32_t NMICR;  /* 0x560: NMI Control Register */
  volatile uint8_t  NMIER;  /* 0x564: NMI Enable Register */
  volatile uint8_t  NMISR;  /* 0x565: NMI Status Register */
  volatile uint8_t  NMICLR; /* 0x566: NMI Clear Register */
  volatile uint8_t  NMIFLT; /* 0x567: NMI Filter Control Register */
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
