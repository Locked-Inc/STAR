/* lib/rx_hal/inc/rx72n_rspi_regs.h */

/**
 * @file rx72n_rspi_regs.h
 * @brief RX72N RSPI SPI Register Definitions
 *
 * Register definitions for the Renesas Serial Peripheral Interface (RSPI)
 * used for SPI communication to the Raspberry Pi 5.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_RSPI_REGS_H
#define STAR_RX72N_RSPI_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Renesas Serial Peripheral Interface (RSPI) - For SPI to RPi5
 * =============================================================================
 */

typedef struct {
  volatile uint8_t  SPCR;   /* SPI Control Register */
  volatile uint8_t  SSLP;   /* SPI Slave Select Polarity Register */
  volatile uint8_t  SPPCR;  /* SPI Pin Control Register */
  volatile uint8_t  SPSR;   /* SPI Status Register */
  volatile uint32_t SPDR;   /* SPI Data Register */
  volatile uint8_t  SPSCR;  /* SPI Sequence Control Register */
  volatile uint8_t  SPSSR;  /* SPI Sequence Status Register */
  volatile uint8_t  SPBR;   /* SPI Bit Rate Register */
  volatile uint8_t  SPDCR;  /* SPI Data Control Register */
  volatile uint8_t  SPCKD;  /* SPI Clock Delay Register */
  volatile uint8_t  SSLND;  /* SPI Slave Select Negation Delay Register */
  volatile uint8_t  SPND;   /* SPI Next-Access Delay Register */
  volatile uint8_t  SPCR2;  /* SPI Control Register 2 */
  volatile uint16_t SPCMD0; /* SPI Command Register 0 */
} rx_rspi_regs_t;

#define RSPI0_BASE ((rx_rspi_regs_t*)0x000D0000)
#define RSPI1_BASE ((rx_rspi_regs_t*)0x000D0100)
#define RSPI2_BASE ((rx_rspi_regs_t*)0x000D0200)

#define RSPI0 (*RSPI0_BASE)
#define RSPI1 (*RSPI1_BASE)
#define RSPI2 (*RSPI2_BASE)

/* RSPI Control Register (SPCR) Bit Definitions */
typedef enum {
  k_rspi_spcr_sprie = (1 << 7), /* Receive Interrupt Enable */
  k_rspi_spcr_spe   = (1 << 6), /* SPI Function Enable */
  k_rspi_spcr_sptie = (1 << 5), /* Transmit Interrupt Enable */
  k_rspi_spcr_speie = (1 << 4), /* Error Interrupt Enable */
  k_rspi_spcr_mstr  = (1 << 3), /* Controller/Peripheral Mode (1=Controller, 0=Peripheral) */
  k_rspi_spcr_modfe = (1 << 2), /* Mode Fault Error Detection Enable */
  k_rspi_spcr_txmd  = (1 << 1), /* Transmit Only Mode */
  k_rspi_spcr_spms  = (1 << 0), /* SPI Mode Select */
} rspi_spcr_bits_t;

/* RSPI Status Register (SPSR) Bit Definitions */
typedef enum {
  k_rspi_spsr_sprf  = (1 << 7), /* Receive Buffer Full Flag */
  k_rspi_spsr_sptef = (1 << 5), /* Transmit Buffer Empty Flag */
  k_rspi_spsr_perf  = (1 << 3), /* Parity Error Flag */
  k_rspi_spsr_modf  = (1 << 2), /* Mode Fault Error Flag */
  k_rspi_spsr_idlnf = (1 << 1), /* Idle Flag */
  k_rspi_spsr_ovrf  = (1 << 0), /* Overrun Error Flag */
} rspi_spsr_bits_t;

/* RSPI Pin Control Register (SPPCR) Bit Definitions */
typedef enum {
  k_rspi_sppcr_moife = (1 << 6), /* COPI Idle Fixed Value Enable */
  k_rspi_sppcr_moifv = (1 << 5), /* COPI Idle Fixed Value */
  k_rspi_sppcr_splp  = (1 << 0), /* Loopback Mode */
} rspi_sppcr_bits_t;

/* RSPI Data Control Register (SPDCR) Bit Definitions */
typedef enum {
  k_rspi_spdcr_sprdtd = (1 << 5), /* Receive Data Ready Detection */
  k_rspi_spdcr_splw   = (1 << 4), /* Word Access Mode (1=Word, 0=Byte) */
} rspi_spdcr_bits_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_RSPI_REGS_H */
