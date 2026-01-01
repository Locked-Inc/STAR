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

/* System Control Register (SYSTEM) - Base: 0x00080000 */
typedef struct {
  volatile uint16_t SYSCR0; /* 0x00 - System Control Register 0 */
  volatile uint16_t SYSCR1; /* 0x02 - System Control Register 1 */
  uint8_t           RESERVED0[2];
  volatile uint16_t SBYCR; /* 0x06 - Standby Control Register */
  uint8_t           RESERVED1[2];
  volatile uint16_t PRCR; /* 0x0A - Protection Register */
  uint8_t           RESERVED1A[2];
  volatile uint32_t MSTPCRA; /* 0x0C - Module Stop Control Register A */
  volatile uint32_t MSTPCRB; /* 0x10 - Module Stop Control Register B */
  volatile uint32_t MSTPCRC; /* 0x14 - Module Stop Control Register C */
  volatile uint32_t MSTPCRD; /* 0x18 - Module Stop Control Register D */
  volatile uint32_t SCKCR;   /* 0x1C - System Clock Control Register */
  volatile uint16_t SCKCR2;  /* 0x20 - System Clock Control Register 2 */
  volatile uint16_t SCKCR3;  /* 0x22 - System Clock Control Register 3 */
  volatile uint16_t PLLCR;   /* 0x24 - PLL Control Register */
  volatile uint8_t  PLLCR2;  /* 0x26 - PLL Control Register 2 */
  uint8_t           RESERVED2[5];
  volatile uint8_t  BCKCR; /* 0x2C - External Bus Clock Control Register */
  uint8_t           RESERVED3[1];
  volatile uint8_t  MOSCCR;  /* 0x2E - Main Clock Oscillator Control */
  volatile uint8_t  SOSCCR;  /* 0x2F - Sub-Clock Oscillator Control */
  volatile uint8_t  LOCOCR;  /* 0x30 - Low-Speed On-Chip Oscillator Control */
  volatile uint8_t  ILOCOCR; /* 0x31 - High-Speed On-Chip Oscillator Control */
  volatile uint8_t  HOCOCR;  /* 0x32 - High-Speed On-Chip Oscillator Control */
  volatile uint8_t  HOCOCR2; /* 0x33 - High-Speed On-Chip Oscillator Control 2 */
  uint8_t           RESERVED4[4];
  volatile uint8_t  OSCOVFSR; /* 0x38 - Oscillation Stabilization Flag */
  uint8_t           RESERVED5[3];
  volatile uint8_t  OSTDCR; /* 0x3C - Oscillation Stop Detection Control */
  volatile uint8_t  OSTDSR; /* 0x3D - Oscillation Stop Detection Status */
} rx_system_regs_t;

#define SYSTEM_BASE ((rx_system_regs_t*)0x00080000)
#define SYSTEM      (*SYSTEM_BASE)

/* Module stop bits for MSTPCRB register */
typedef enum {
  k_mstpb_usb0 = 19, /**< USB0 module stop bit in MSTPCRB */
  k_mstpb_crc  = 23, /**< CRC module stop bit in MSTPCRB */
} rx_module_stop_bits_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_SYSTEM_REGS_H */
