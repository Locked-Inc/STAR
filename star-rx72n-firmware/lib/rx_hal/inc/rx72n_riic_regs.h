/* lib/rx_hal/inc/rx72n_riic_regs.h */

/**
 * @file rx72n_riic_regs.h
 * @brief RX72N RIIC I2C Register Definitions
 *
 * Register definitions for the I2C Bus Interface (RIIC) used for I2C/SMBUS
 * communication.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_RIIC_REGS_H
#define STAR_RX72N_RIIC_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * I2C Bus Interface (RIIC) - For I2C/SMBUS Communication
 * =============================================================================
 */

typedef struct {
  volatile uint8_t ICCR1; /* I2C Bus Control Register 1 */
  volatile uint8_t ICCR2; /* I2C Bus Control Register 2 */
  volatile uint8_t ICMR1; /* I2C Bus Mode Register 1 */
  volatile uint8_t ICMR2; /* I2C Bus Mode Register 2 */
  volatile uint8_t ICMR3; /* I2C Bus Mode Register 3 */
  volatile uint8_t ICFER; /* I2C Bus Function Enable Register */
  volatile uint8_t ICSER; /* I2C Bus Status Enable Register */
  volatile uint8_t ICIER; /* I2C Bus Interrupt Enable Register */
  volatile uint8_t ICSR1; /* I2C Bus Status Register 1 */
  volatile uint8_t ICSR2; /* I2C Bus Status Register 2 */
  volatile uint8_t SARL0; /* Slave Address Register L0 */
  volatile uint8_t SARU0; /* Slave Address Register U0 */
  volatile uint8_t SARL1; /* Slave Address Register L1 */
  volatile uint8_t SARU1; /* Slave Address Register U1 */
  volatile uint8_t SARL2; /* Slave Address Register L2 */
  volatile uint8_t SARU2; /* Slave Address Register U2 */
  volatile uint8_t ICBRL; /* I2C Bus Bit Rate Register L */
  volatile uint8_t ICBRH; /* I2C Bus Bit Rate Register H */
  volatile uint8_t ICDRT; /* I2C Bus Transmit Data Register */
  volatile uint8_t ICDRR; /* I2C Bus Receive Data Register */
} rx_riic_regs_t;

#define RIIC0_BASE ((rx_riic_regs_t*)0x00088300)
#define RIIC1_BASE ((rx_riic_regs_t*)0x00088320)
#define RIIC2_BASE ((rx_riic_regs_t*)0x00088340)

#define RIIC0 (*RIIC0_BASE)
#define RIIC1 (*RIIC1_BASE)
#define RIIC2 (*RIIC2_BASE)

/* RIIC Control Register 1 (ICCR1) Bit Definitions */
typedef enum {
  k_riic_iccr1_ice      = (1 << 7), /* I2C Bus Interface Enable */
  k_riic_iccr1_iicrst   = (1 << 6), /* I2C Bus Interface Internal Reset */
  k_riic_iccr1_clk_mask = 0x0F,     /* Clock Select Mask (bits 0-3) */
} riic_iccr1_bits_t;

/* RIIC Control Register 2 (ICCR2) Bit Definitions */
typedef enum {
  k_riic_iccr2_bbsy = (1 << 7), /* Bus Busy Detection Flag */
  k_riic_iccr2_mst  = (1 << 6), /* Controller Mode */
  k_riic_iccr2_trx  = (1 << 5), /* Transmit/Receive Mode (1=TX, 0=RX) */
  k_riic_iccr2_sp   = (1 << 3), /* Stop Condition Issue Request */
  k_riic_iccr2_rs   = (1 << 2), /* Restart Condition Issue Request */
  k_riic_iccr2_st   = (1 << 1), /* Start Condition Issue Request */
} riic_iccr2_bits_t;

/* RIIC Status Register 1 (ICSR1) Bit Definitions */
typedef enum {
  k_riic_icsr1_ackbr = (1 << 0), /* ACK Bit Receive Flag */
} riic_icsr1_bits_t;

/* RIIC Status Register 2 (ICSR2) Bit Definitions */
typedef enum {
  k_riic_icsr2_nackf = (1 << 4), /* NACK Detection Flag */
  k_riic_icsr2_stop  = (1 << 3), /* Stop Condition Detection Flag */
  k_riic_icsr2_start = (1 << 2), /* Start Condition Detection Flag */
  k_riic_icsr2_tdre  = (1 << 7), /* Transmit Data Empty Flag */
  k_riic_icsr2_rdrf  = (1 << 1), /* Receive Data Full Flag */
} riic_icsr2_bits_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_RIIC_REGS_H */
