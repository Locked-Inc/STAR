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

/**
 * @brief SCI Register Map
 * @details
 * Serial Communication Interface (SCI) registers for UART communication.
 * Base addresses:
 * - SCI0: 0x0008A000
 * - SCI1: 0x0008A020
 * - SCI2: 0x0008A040
 * - SCI5: 0x0008A0A0
 * - SCI6: 0x0008A0C0
 */
typedef struct {
  volatile uint8_t smr;  /**< Serial Mode Register (data length, parity, stop bits) */
  volatile uint8_t brr;  /**< Bit Rate Register (baud rate divisor) */
  volatile uint8_t scr;  /**< Serial Control Register (TX/RX enable, interrupts) */
  volatile uint8_t tdr;  /**< Transmit Data Register (data to send) */
  volatile uint8_t ssr;  /**< Serial Status Register (TX empty, RX full, errors) */
  volatile uint8_t rdr;  /**< Receive Data Register (received data) */
  volatile uint8_t scmr; /**< Smart Card Mode Register (smart card settings) */
  volatile uint8_t semr; /**< Serial Extended Mode Register (noise filter, etc.) */
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
