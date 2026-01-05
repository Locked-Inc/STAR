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

/** @brief SCI base addresses */
typedef enum {
  k_sci0_base_addr = 0x0008A000, /**< SCI0 register base address */
  k_sci1_base_addr = 0x0008A020, /**< SCI1 register base address */
  k_sci2_base_addr = 0x0008A040, /**< SCI2 register base address */
  k_sci5_base_addr = 0x0008A0A0, /**< SCI5 register base address */
  k_sci6_base_addr = 0x0008A0C0, /**< SCI6 register base address */
} rx_sci_addresses_t;

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

/**
 * @brief Get pointer to SCI0 registers
 * @return Volatile pointer to SCI0 register structure
 */
static inline volatile rx_sci_regs_t* sci0(void)
{
  return (volatile rx_sci_regs_t*)k_sci0_base_addr;
}

/**
 * @brief Get pointer to SCI1 registers
 * @return Volatile pointer to SCI1 register structure
 */
static inline volatile rx_sci_regs_t* sci1(void)
{
  return (volatile rx_sci_regs_t*)k_sci1_base_addr;
}

/**
 * @brief Get pointer to SCI2 registers
 * @return Volatile pointer to SCI2 register structure
 */
static inline volatile rx_sci_regs_t* sci2(void)
{
  return (volatile rx_sci_regs_t*)k_sci2_base_addr;
}

/**
 * @brief Get pointer to SCI5 registers
 * @return Volatile pointer to SCI5 register structure
 */
static inline volatile rx_sci_regs_t* sci5(void)
{
  return (volatile rx_sci_regs_t*)k_sci5_base_addr;
}

/**
 * @brief Get pointer to SCI6 registers
 * @return Volatile pointer to SCI6 register structure
 */
static inline volatile rx_sci_regs_t* sci6(void)
{
  return (volatile rx_sci_regs_t*)k_sci6_base_addr;
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_SCI_REGS_H */
