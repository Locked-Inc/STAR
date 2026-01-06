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

#include <stddef.h>
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
  k_sci0_base_addr  = 0x0008A000, /**< SCI0 register base address */
  k_sci1_base_addr  = 0x0008A020, /**< SCI1 register base address */
  k_sci2_base_addr  = 0x0008A040, /**< SCI2 register base address */
  k_sci3_base_addr  = 0x0008A060, /**< SCI3 register base address */
  k_sci4_base_addr  = 0x0008A080, /**< SCI4 register base address */
  k_sci5_base_addr  = 0x0008A0A0, /**< SCI5 register base address */
  k_sci6_base_addr  = 0x0008A0C0, /**< SCI6 register base address */
  k_sci7_base_addr  = 0x0008A0E0, /**< SCI7 register base address */
  k_sci8_base_addr  = 0x0008A100, /**< SCI8 register base address */
  k_sci9_base_addr  = 0x0008A120, /**< SCI9 register base address (Debug UART) */
  k_sci10_base_addr = 0x000D0000, /**< SCI10 register base address */
  k_sci11_base_addr = 0x000D0020, /**< SCI11 register base address */
  k_sci12_base_addr = 0x000D0040, /**< SCI12 register base address */
} rx_sci_addresses_t;

/**
 * @brief SCI Register Map
 * @details
 * Serial Communication Interface (SCI) registers for UART communication.
 * Base addresses (standard region 0x0008Axxx):
 * - SCI0: 0x0008A000
 * - SCI1: 0x0008A020
 * - SCI2: 0x0008A040
 * - SCI3: 0x0008A060
 * - SCI4: 0x0008A080
 * - SCI5: 0x0008A0A0
 * - SCI6: 0x0008A0C0
 * - SCI7: 0x0008A0E0
 * - SCI8: 0x0008A100
 * - SCI9: 0x0008A120 (Debug UART - CY7C65213 USB bridge)
 *
 * Extended region (0x000D0xxx):
 * - SCI10: 0x000D0000
 * - SCI11: 0x000D0020
 * - SCI12: 0x000D0040
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
 * @brief Get pointer to SCI3 registers
 * @return Volatile pointer to SCI3 register structure
 */
static inline volatile rx_sci_regs_t* sci3(void)
{
  return (volatile rx_sci_regs_t*)k_sci3_base_addr;
}

/**
 * @brief Get pointer to SCI4 registers
 * @return Volatile pointer to SCI4 register structure
 */
static inline volatile rx_sci_regs_t* sci4(void)
{
  return (volatile rx_sci_regs_t*)k_sci4_base_addr;
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

/**
 * @brief Get pointer to SCI7 registers
 * @return Volatile pointer to SCI7 register structure
 */
static inline volatile rx_sci_regs_t* sci7(void)
{
  return (volatile rx_sci_regs_t*)k_sci7_base_addr;
}

/**
 * @brief Get pointer to SCI8 registers
 * @return Volatile pointer to SCI8 register structure
 */
static inline volatile rx_sci_regs_t* sci8(void)
{
  return (volatile rx_sci_regs_t*)k_sci8_base_addr;
}

/**
 * @brief Get pointer to SCI9 registers (Debug UART)
 * @return Volatile pointer to SCI9 register structure
 */
static inline volatile rx_sci_regs_t* sci9(void)
{
  return (volatile rx_sci_regs_t*)k_sci9_base_addr;
}

/**
 * @brief Get pointer to SCI10 registers
 * @return Volatile pointer to SCI10 register structure
 */
static inline volatile rx_sci_regs_t* sci10(void)
{
  return (volatile rx_sci_regs_t*)k_sci10_base_addr;
}

/**
 * @brief Get pointer to SCI11 registers
 * @return Volatile pointer to SCI11 register structure
 */
static inline volatile rx_sci_regs_t* sci11(void)
{
  return (volatile rx_sci_regs_t*)k_sci11_base_addr;
}

/**
 * @brief Get pointer to SCI12 registers
 * @return Volatile pointer to SCI12 register structure
 */
static inline volatile rx_sci_regs_t* sci12(void)
{
  return (volatile rx_sci_regs_t*)k_sci12_base_addr;
}

/* =============================================================================
 * Multi-Channel Support
 * =============================================================================
 */

/**
 * @brief SCI channel count
 */
typedef enum {
  k_sci_channel_max = 13, /**< Total SCI channels (0-12) */
} sci_channel_limits_t;

#if defined(USE_MOCK_SCI_REGS)
volatile rx_sci_regs_t* sci_get_channel(uint8_t channel);
#else
/**
 * @brief Get SCI register base for a channel
 * @param[in] channel SCI channel number (0-12)
 * @return Pointer to SCI registers, or NULL if invalid channel
 */
static inline volatile rx_sci_regs_t* sci_get_channel(uint8_t channel)
{
  switch (channel) {
    case 0:
      return sci0();
    case 1:
      return sci1();
    case 2:
      return sci2();
    case 3:
      return sci3();
    case 4:
      return sci4();
    case 5:
      return sci5();
    case 6:
      return sci6();
    case 7:
      return sci7();
    case 8:
      return sci8();
    case 9:
      return sci9();
    case 10:
      return sci10();
    case 11:
      return sci11();
    case 12:
      return sci12();
    default:
      return NULL; /* Invalid channel */
  }
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_SCI_REGS_H */
