/* lib/rx_hal/inc/rx72n_sci_regs.h */

/**
 * @file rx72n_sci_regs.h
 * @brief RX72N SCI UART Register Definitions
 *
 * Register definitions for the Serial Communication Interface (SCI) used for
 * UART and debug communication.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_SCI_REGS_H
#define STAR_RX72N_SCI_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Serial Communication Interface (SCI) - For UART/Debug
 * =============================================================================
 */

typedef struct {
  volatile uint8_t SMR;  /* Serial Mode Register */
  volatile uint8_t BRR;  /* Bit Rate Register */
  volatile uint8_t SCR;  /* Serial Control Register */
  volatile uint8_t TDR;  /* Transmit Data Register */
  volatile uint8_t SSR;  /* Serial Status Register */
  volatile uint8_t RDR;  /* Receive Data Register */
  volatile uint8_t SCMR; /* Smart Card Mode Register */
  volatile uint8_t SEMR; /* Serial Extended Mode Register */
} rx_sci_regs_t;

#define SCI0_BASE ((rx_sci_regs_t*)0x0008A000)
#define SCI1_BASE ((rx_sci_regs_t*)0x0008A020)
#define SCI2_BASE ((rx_sci_regs_t*)0x0008A040)
#define SCI5_BASE ((rx_sci_regs_t*)0x0008A0A0)
#define SCI6_BASE ((rx_sci_regs_t*)0x0008A0C0)

#define SCI0 (*SCI0_BASE)
#define SCI1 (*SCI1_BASE)
#define SCI2 (*SCI2_BASE)
#define SCI5 (*SCI5_BASE)
#define SCI6 (*SCI6_BASE)

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_SCI_REGS_H */
