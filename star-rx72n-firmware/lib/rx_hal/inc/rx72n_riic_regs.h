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

/** @brief RIIC base addresses */
typedef enum : uint32_t {
  k_riic0_base_addr = 0x00088300, /**< RIIC0 register base address */
  k_riic1_base_addr = 0x00088320, /**< RIIC1 register base address */
  k_riic2_base_addr = 0x00088340, /**< RIIC2 register base address */
} rx_riic_addresses_t;

/**
 * @brief RIIC Register Map
 * @details
 * I2C Bus Interface (RIIC) registers for I2C controller/peripheral communication.
 * Base addresses:
 * - RIIC0: 0x00088300
 * - RIIC1: 0x00088320
 * - RIIC2: 0x00088340
 */
typedef struct {
  volatile uint8_t iccr1; /**< I2C Bus Control Register 1 (enable, reset) */
  volatile uint8_t iccr2; /**< I2C Bus Control Register 2 (start, stop, restart) */
  volatile uint8_t icmr1; /**< I2C Bus Mode Register 1 (clock settings) */
  volatile uint8_t icmr2; /**< I2C Bus Mode Register 2 (timeout settings) */
  volatile uint8_t icmr3; /**< I2C Bus Mode Register 3 (ACK/NACK control) */
  volatile uint8_t icfer; /**< I2C Bus Function Enable Register (features) */
  volatile uint8_t icser; /**< I2C Bus Status Enable Register (status detect) */
  volatile uint8_t icier; /**< I2C Bus Interrupt Enable Register (interrupts) */
  volatile uint8_t icsr1; /**< I2C Bus Status Register 1 (ACK status) */
  volatile uint8_t icsr2; /**< I2C Bus Status Register 2 (TX/RX status) */
  volatile uint8_t sarl0; /**< Peripheral Address Register L0 (7-bit address) */
  volatile uint8_t saru0; /**< Peripheral Address Register U0 (10-bit extension) */
  volatile uint8_t sarl1; /**< Peripheral Address Register L1 */
  volatile uint8_t saru1; /**< Peripheral Address Register U1 */
  volatile uint8_t sarl2; /**< Peripheral Address Register L2 */
  volatile uint8_t saru2; /**< Peripheral Address Register U2 */
  volatile uint8_t icbrl; /**< I2C Bus Bit Rate Register L (low period) */
  volatile uint8_t icbrh; /**< I2C Bus Bit Rate Register H (high period) */
  volatile uint8_t icdrt; /**< I2C Bus Transmit Data Register */
  volatile uint8_t icdrr; /**< I2C Bus Receive Data Register */
} rx_riic_regs_t;

/**
 * @brief Get pointer to RIIC0 registers
 * @return Volatile pointer to RIIC0 register structure
 */
static inline volatile rx_riic_regs_t* riic0(void)
{
  return (volatile rx_riic_regs_t*)k_riic0_base_addr;
}

/**
 * @brief Get pointer to RIIC1 registers
 * @return Volatile pointer to RIIC1 register structure
 */
static inline volatile rx_riic_regs_t* riic1(void)
{
  return (volatile rx_riic_regs_t*)k_riic1_base_addr;
}

/**
 * @brief Get pointer to RIIC2 registers
 * @return Volatile pointer to RIIC2 register structure
 */
static inline volatile rx_riic_regs_t* riic2(void)
{
  return (volatile rx_riic_regs_t*)k_riic2_base_addr;
}

/* RIIC Control Register 1 (ICCR1) Bit Definitions */
typedef enum : uint8_t {
  k_riic_iccr1_ice      = (1 << 7), /**< I2C Bus Interface Enable */
  k_riic_iccr1_iicrst   = (1 << 6), /**< I2C Bus Interface Internal Reset */
  k_riic_iccr1_clk_mask = 0x0F,     /**< Clock Select Mask (bits 0-3) */
} riic_iccr1_bits_t;

/* RIIC Control Register 2 (ICCR2) Bit Definitions */
typedef enum : uint8_t {
  k_riic_iccr2_bbsy = (1 << 7), /**< Bus Busy Detection Flag */
  k_riic_iccr2_mst  = (1 << 6), /**< Controller Mode */
  k_riic_iccr2_trx  = (1 << 5), /**< Transmit/Receive Mode (1=TX, 0=RX) */
  k_riic_iccr2_sp   = (1 << 3), /**< Stop Condition Issue Request */
  k_riic_iccr2_rs   = (1 << 2), /**< Restart Condition Issue Request */
  k_riic_iccr2_st   = (1 << 1), /**< Start Condition Issue Request */
} riic_iccr2_bits_t;

/* RIIC Status Register 1 (ICSR1) Bit Definitions */
typedef enum : uint8_t {
  k_riic_icsr1_ackbr = (1 << 0), /**< ACK Bit Receive Flag */
} riic_icsr1_bits_t;

/* RIIC Status Register 2 (ICSR2) Bit Definitions */
typedef enum : uint8_t {
  k_riic_icsr2_nackf = (1 << 4), /**< NACK Detection Flag */
  k_riic_icsr2_stop  = (1 << 3), /**< Stop Condition Detection Flag */
  k_riic_icsr2_start = (1 << 2), /**< Start Condition Detection Flag */
  k_riic_icsr2_tdre  = (1 << 7), /**< Transmit Data Empty Flag */
  k_riic_icsr2_rdrf  = (1 << 1), /**< Receive Data Full Flag */
} riic_icsr2_bits_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* Verify base addresses match Hardware Manual */
_Static_assert(k_riic0_base_addr == 0x00088300, "RIIC0 base address incorrect");
_Static_assert(k_riic1_base_addr == 0x00088320, "RIIC1 base address incorrect");
_Static_assert(k_riic2_base_addr == 0x00088340, "RIIC2 base address incorrect");

/* Verify register structure layout */
_Static_assert(sizeof(rx_riic_regs_t) == 20, "RIIC register structure size incorrect");
_Static_assert(offsetof(rx_riic_regs_t, iccr1) == 0x00, "ICCR1 offset incorrect");
_Static_assert(offsetof(rx_riic_regs_t, iccr2) == 0x01, "ICCR2 offset incorrect");
_Static_assert(offsetof(rx_riic_regs_t, icdrt) == 0x12, "ICDRT offset incorrect");
_Static_assert(offsetof(rx_riic_regs_t, icdrr) == 0x13, "ICDRR offset incorrect");

/* Verify channel spacing (0x20 bytes between channels) */
_Static_assert((k_riic1_base_addr - k_riic0_base_addr) == 0x20, "RIIC0 to RIIC1 spacing incorrect");
_Static_assert((k_riic2_base_addr - k_riic1_base_addr) == 0x20, "RIIC1 to RIIC2 spacing incorrect");

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_RIIC_REGS_H */
