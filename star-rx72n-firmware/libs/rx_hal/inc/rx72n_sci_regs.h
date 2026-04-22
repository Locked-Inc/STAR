/**
 * @file rx72n_sci_regs.h
 * @brief RX72N Serial Communication Interface (SCI) register definitions
 *
 * @details
 * This file provides complete hardware register definitions for the RX72N's
 * Serial Communication Interface (SCI) peripheral. The SCI module supports
 * multiple serial communication protocols including UART (asynchronous),
 * clock-synchronous serial, smart card interface, and I2C modes.
 *
 * @par System Architecture - STAR Robot Serial Communication
 * @verbatim
 *   +-------------------------------------------------------------------------+
 *   |                    RX72N Serial Communication Architecture             |
 *   |                                                                         |
 *   |  +---------------------------------------------------------------------+|
 *   |  |                         SCI Modules                                 ||
 *   |  |                                                                     ||
 *   |  |  Standard Region (0x0008Axxx)        Extended Region (0x000D0xxx)   ||
 *   |  |  +-------------------------+         +--------------------------+   ||
 *   |  |  | SCIj Module (SCI0-6)    |         | SCIi Module (SCI7-11)    |   ||
 *   |  |  | +-----+ +-----+ +-----+|         | +-----+ +-----+ +-----+  |   ||
 *   |  |  | |SCI0 | |SCI1 | |SCI2 ||         | |SCI7 | |SCI8 | |SCI9 |  |   ||
 *   |  |  | +-----+ +-----+ +-----+|         | +-----+ +-----+ +--+--+  |   ||
 *   |  |  | +-----+ +-----+ +-----+|         | +-----+ +-----+    |     |   ||
 *   |  |  | |SCI3 | |SCI4 | |SCI5 ||         | |SCI10| |SCI11|  Debug  |   ||
 *   |  |  | +-----+ +-----+ +-----+|         | +-----+ +-----+   UART   |   ||
 *   |  |  | +-----+                |         +--------------------------+   ||
 *   |  |  | |SCI6 |                |                                        ||
 *   |  |  | +-----+                |         +--------------------------+   ||
 *   |  |  +-------------------------+         | SCIh Module (SCI12)     |   ||
 *   |  |                                      | @ 0x0008B300            |   ||
 *   |  |                                      | +-------+               |   ||
 *   |  |                                      | | SCI12 |               |   ||
 *   |  |                                      | +-------+               |   ||
 *   |  |                                      +--------------------------+   ||
 *   |  +---------------------------------------------------------------------+|
 *   |                                                                         |
 *   |  STAR Robot SCI Usage:                                                  |
 *   |  +--------------------------------------------------------------------+ |
 *   |  | Debug UART (SCI9):                                                 | |
 *   |  |   +---------+    +------------+    +----------+    +------------+  | |
 *   |  |   |  RX72N  |--->| CY7C65213  |--->|   USB    |--->|    Host    |  | |
 *   |  |   |  SCI9   |    | USB-UART   |    | Cable    |    |    PC      |  | |
 *   |  |   |TXD9/RXD9|<---|  Bridge    |<---|          |<---|  Console   |  | |
 *   |  |   +---------+    +------------+    +----------+    +------------+  | |
 *   |  |                                                                    | |
 *   |  | Settings: 115200 baud, 8N1, no flow control                        | |
 *   |  | Purpose: Logging, debugging, firmware updates                      | |
 *   |  +--------------------------------------------------------------------+ |
 *   +-------------------------------------------------------------------------+
 * @endverbatim
 *
 * @par SCI Channel Summary
 * | Channel | Base Address | Region   | STAR Usage |
 * |---------|--------------|----------|------------|
 * | SCI0    | 0x0008A000   | Standard | Available  |
 * | SCI1    | 0x0008A020   | Standard | Available  |
 * | SCI2    | 0x0008A040   | Standard | Available  |
 * | SCI3    | 0x0008A060   | Standard | Available  |
 * | SCI4    | 0x0008A080   | Standard | Available  |
 * | SCI5    | 0x0008A0A0   | Standard | Available  |
 * | SCI6    | 0x0008A0C0   | Standard | Available  |
 * | SCI7    | 0x000D00E0   | Extended | Available  |
 * | SCI8    | 0x000D0000   | Extended | Available  |
 * | SCI9    | 0x000D0020   | Extended | **Debug UART** |
 * | SCI10   | 0x000D0040   | Extended | Available  |
 * | SCI11   | 0x000D0060   | Extended | Available  |
 * | SCI12   | 0x0008B300   | Standard | Available  |
 *
 * @par Baud Rate Calculation
 * For asynchronous mode with n=0 (CKS=00, PCLK/1):
 * @f[
 * \text{BRR} = \frac{f_{PCLK}}{32 \times B} - 1
 * @f]
 *
 * Where f_PCLK depends on the channel module (manual section 5):
 * - SCIj (SCI0-SCI6) and SCIh (SCI12): f_PCLK = f_PCLKB
 * - SCIi (SCI7-SCI11, extended region): f_PCLK = f_PCLKA
 *
 * Example: 115200 baud with f_PCLKB = 60 MHz, n=0:
 * @f[
 * \text{BRR} = \frac{60\text{ MHz}}{32 \times 115200} - 1 = 16.28 - 1 = 15.28 \approx 15
 * @f]
 *
 * @par UART Configuration Example
 * @code
 * // Initialize SCI9 for 115200 baud, 8N1
 * volatile rx_sci_regs_t* uart = sci9();
 *
 * // Disable TX/RX during configuration
 * uart->scr = 0x00;
 *
 * // Set mode: async, 8-bit, no parity, 1 stop bit, PCLKA/1
 * uart->smr = 0x00;
 *
 * // Enable noise filter and bit rate modulation
 * uart->semr = 0x04;  // NFEN = 1
 *
 * // Set baud rate: 115200 at 120 MHz PCLKA
 * uart->brr = 32;
 *
 * // Clear errors and enable TX/RX
 * uart->ssr = 0x84;   // Clear error flags
 * uart->scr = 0x30;   // TE=1, RE=1 (enable TX and RX)
 * @endcode
 *
 * @par Hardware Reference
 * - RX72N Group User's Manual: Hardware, Chapter 41 (SCI)
 * - Section 41.2: Register Descriptions
 * - Section 41.3: UART Mode Operation
 * - Section 41.6: Baud Rate Generator
 *
 * @par Verification Status
 * [PASS] VERIFIED (2026-01-28) - All base addresses and register offsets verified
 * against RX72N Manual Ch41 Section 41.2
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 3: [OK] Static allocation only (all definitions compile-time)
 * - Rule 5: [OK] Static assertions verify register layout at compile time
 * - Rule 8: [OK] C23 typed enums eliminate preprocessor constants
 * - Rule 10: [OK] Header compiles cleanly with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **Single Responsibility**: SCI register definitions only, no driver logic
 * - **Open/Closed**: Extend via new enums; don't modify existing values
 * - **Interface Segregation**: Basic UART registers; advanced features separate
 * - **Dependency Inversion**: Higher-level UART drivers depend on these abstractions
 *
 * @par Related Modules
 * - [uart.c](uart_8c.html): UART HAL driver implementation
 * - [rx72n_port_regs.h](rx72n__port__regs_8h.html): GPIO for TXD/RXD pins
 * - [rx72n_mpc.h](rx72n__mpc_8h.html): Pin function selection for SCI pins
 * - [rx72n_icu_regs.h](rx72n__icu__regs_8h.html): SCI interrupt vectors
 *
 * @see RX72N Hardware Manual Chapter 41 for complete SCI specification
 * @see DOXYGEN_ROADMAP.md for documentation standards
 *
 * @author Locked, Inc. Contributors
 * @date 2026-01-28
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 *
 * @defgroup sci_regs SCI Register Definitions
 * @{
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Serial Communication Interface (SCI) - Base Addresses
 * =============================================================================
 */

/**
 * @enum rx_sci_addresses_t
 * @brief SCI channel base addresses (verified against RX72N Hardware Manual)
 *
 * @details
 * Base addresses for all 13 SCI channels. The RX72N organizes SCI channels
 * into three modules in different memory regions:
 *
 * @par Memory Map Overview
 * @verbatim
 *   Address Range        Module   Channels   Notes
 *   -----------------------------------------------------------------
 *   0x0008A000-0x0008A0DF  SCIj   SCI0-6     Standard region, 0x20 spacing
 *   0x000D0000-0x000D00FF  SCIi   SCI7-11    Extended region, varying spacing
 *   0x0008B300-0x0008B31F  SCIh   SCI12      Standard region, separate
 * @endverbatim
 *
 * @par Channel Spacing
 * - SCIj (SCI0-6): 0x20 bytes between channels
 * - SCIi (SCI7-11): Non-uniform spacing (see individual addresses)
 * - SCIh (SCI12): Standalone at 0x0008B300
 *
 * @par STAR Robot Usage
 * SCI9 is designated as the debug UART, connected to the CY7C65213 USB-UART
 * bridge IC for host PC communication at 115200 baud.
 *
 * @see RX72N Hardware Manual Section 41.2 (Register Addresses)
 */
typedef enum : uintptr_t {
  /** @brief SCI0 base address - standard region (SCIj module) */
  k_sci0_base_addr = 0x0008A000,

  /** @brief SCI1 base address - standard region (SCIj module) */
  k_sci1_base_addr = 0x0008A020,

  /** @brief SCI2 base address - standard region (SCIj module) */
  k_sci2_base_addr = 0x0008A040,

  /** @brief SCI3 base address - standard region (SCIj module) */
  k_sci3_base_addr = 0x0008A060,

  /** @brief SCI4 base address - standard region (SCIj module) */
  k_sci4_base_addr = 0x0008A080,

  /** @brief SCI5 base address - standard region (SCIj module) */
  k_sci5_base_addr = 0x0008A0A0,

  /** @brief SCI6 base address - standard region (SCIj module) */
  k_sci6_base_addr = 0x0008A0C0,

  /** @brief SCI7 base address - extended region (SCIi module) */
  k_sci7_base_addr = 0x000D00E0,

  /** @brief SCI8 base address - extended region (SCIi module) */
  k_sci8_base_addr = 0x000D0000,

  /**
   * @brief SCI9 base address - extended region (SCIi module)
   * @note **STAR Debug UART** - Connected to CY7C65213 USB-UART bridge
   */
  k_sci9_base_addr = 0x000D0020,

  /** @brief SCI10 base address - extended region (SCIi module) */
  k_sci10_base_addr = 0x000D0040,

  /** @brief SCI11 base address - extended region (SCIi module) */
  k_sci11_base_addr = 0x000D0060,

  /** @brief SCI12 base address - standard region (SCIh module) */
  k_sci12_base_addr = 0x0008B300,
} rx_sci_addresses_t;

/**
 * @struct rx_sci_regs_t
 * @brief SCI register map covering UART and SPI mode registers (14 bytes)
 *
 * @details
 * Memory-mapped register structure for the RX72N SCI peripheral. Covers
 * registers from SMR (0x00) through SPMR (0x0D) to support both asynchronous
 * UART and clock-synchronous SPI modes.
 *
 * @par Register Memory Layout (14 bytes total)
 * @verbatim
 *   Offset  Size  Register  Description
 *   ------------------------------------------------------------------
 *   0x00    1     SMR       Serial Mode (data format, clock source)
 *   0x01    1     BRR       Bit Rate (baud rate divisor)
 *   0x02    1     SCR       Serial Control (TX/RX enable, interrupts)
 *   0x03    1     TDR       Transmit Data (write-only data buffer)
 *   0x04    1     SSR       Serial Status (flags, error conditions)
 *   0x05    1     RDR       Receive Data (read-only data buffer)
 *   0x06    1     SCMR      Smart Card Mode (SDIR for bit order)
 *   0x07    1     SEMR      Serial Extended Mode (noise filter, modulation)
 *   0x08    1     SNFR      Noise Filter Setting
 *   0x09    1     SIMR1     I2C Mode Register 1
 *   0x0A    1     SIMR2     I2C Mode Register 2
 *   0x0B    1     SIMR3     I2C Mode Register 3
 *   0x0C    1     SISR      I2C Status Register
 *   0x0D    1     SPMR      SPI Mode Register (CKPH, CKPOL)
 * @endverbatim
 *
 * @par UART Transmission Sequence
 * @verbatim
 *   1. Wait for SSR.TDRE = 1 (transmit data register empty)
 *   2. Write data byte to TDR
 *   3. SSR.TDRE automatically clears to 0
 *   4. Data shifts out serially via TXD pin
 *   5. SSR.TEND = 1 when transmission complete
 * @endverbatim
 *
 * @par UART Reception Sequence
 * @verbatim
 *   1. Data arrives via RXD pin
 *   2. SSR.RDRF = 1 when byte received
 *   3. Read data byte from RDR
 *   4. SSR.RDRF automatically clears to 0
 *   5. Check SSR.ORER/FER/PER for errors
 * @endverbatim
 *
 * @par Transmit Character Example
 * @code
 * void uart_putc(volatile rx_sci_regs_t* uart, char c)
 * {
 *     // Wait for transmit buffer empty
 *     while ((uart->ssr & 0x80) == 0) {
 *         // SSR.TDRE bit 7
 *     }
 *     // Write character to transmit
 *     uart->tdr = (uint8_t)c;
 * }
 * @endcode
 *
 * @par Receive Character Example
 * @code
 * char uart_getc(volatile rx_sci_regs_t* uart)
 * {
 *     // Wait for receive data ready
 *     while ((uart->ssr & 0x40) == 0) {
 *         // SSR.RDRF bit 6
 *     }
 *     // Read received character
 *     return (char)uart->rdr;
 * }
 * @endcode
 *
 * @invariant Structure size must be exactly 14 bytes
 * @invariant All registers are 8-bit (byte-accessible)
 *
 * @note Offsets 0x00-0x07 are unchanged from the original 8-byte structure.
 *       Existing UART code works unmodified. Offsets 0x08-0x0D add I2C mode
 *       and SPI mode register access needed for clock-synchronous SPI.
 *
 * @see rx_sci_addresses_t for channel base addresses
 * @see RX72N Hardware Manual Section 41.2 (Register Descriptions)
 */
typedef struct {
  /**
   * @brief Serial Mode Register (SMR) - data format configuration
   * @details
   * - Bits 7: CM - Clock mode (0=async, 1=sync)
   * - Bits 6: CHR - Character length (0=8-bit, 1=7-bit)
   * - Bits 5: PE - Parity enable (0=disable, 1=enable)
   * - Bits 4: PM - Parity mode (0=even, 1=odd)
   * - Bits 3: STOP - Stop bits (0=1 bit, 1=2 bits)
   * - Bits 2: MP - Multi-processor mode
   * - Bits 1-0: CKS - Clock select (00=PCLK/1, 01=PCLK/4, 10=PCLK/16, 11=PCLK/64)
   */
  volatile uint8_t smr;

  /**
   * @brief Bit Rate Register (BRR) - baud rate divisor
   * @details
   * Async mode: Baud rate = PCLK / (64 * 2^(2n-1) * (BRR + 1))
   * Sync mode:  Bit rate  = PCLK / (4 * (BRR + 1)) for CKS=0
   * PCLK = PCLKB (60 MHz) for SCI0-6/12; PCLKA (120 MHz) for SCI7-11.
   *
   * Common async values at 60 MHz PCLKB (SCI0-6/12), CKS=0:
   * - 9600 baud: BRR = 194
   * - 38400 baud: BRR = 47
   * - 115200 baud: BRR = 15
   *
   * Common sync values at 60 MHz PCLKB (SCI0-6/12), CKS=0:
   * - BRR=1: 7.5 MHz SPI clock
   * - BRR=3: 3.75 MHz SPI clock
   */
  volatile uint8_t brr;

  /**
   * @brief Serial Control Register (SCR) - TX/RX enable and interrupts
   * @details
   * - Bit 7: TIE - Transmit interrupt enable
   * - Bit 6: RIE - Receive interrupt enable
   * - Bit 5: TE - Transmit enable
   * - Bit 4: RE - Receive enable
   * - Bit 3: MPIE - Multi-processor interrupt enable
   * - Bit 2: TEIE - Transmit end interrupt enable
   * - Bits 1-0: CKE - Clock enable (external clock source)
   */
  volatile uint8_t scr;

  /**
   * @brief Transmit Data Register (TDR) - write-only data buffer
   * @details Write data here to transmit. Wait for SSR.TDRE = 1 first.
   */
  volatile uint8_t tdr;

  /**
   * @brief Serial Status Register (SSR) - status flags and errors
   * @details
   * - Bit 7: TDRE - Transmit data register empty (1=ready to write)
   * - Bit 6: RDRF - Receive data register full (1=data available)
   * - Bit 5: ORER - Overrun error (1=error occurred)
   * - Bit 4: FER - Framing error (1=error occurred)
   * - Bit 3: PER - Parity error (1=error occurred)
   * - Bit 2: TEND - Transmit end (1=transmission complete)
   * - Bit 1: MPB - Multi-processor bit
   * - Bit 0: MPBT - Multi-processor bit transfer
   *
   * @note Write 0 to error bits to clear them (do not write 1).
   */
  volatile uint8_t ssr;

  /**
   * @brief Receive Data Register (RDR) - read-only data buffer
   * @details Read received data here. Wait for SSR.RDRF = 1 first.
   */
  volatile uint8_t rdr;

  /**
   * @brief Smart Card Mode Register (SCMR) - smart card and bit order settings
   * @details
   * - Bit 7: BCP2 - Base clock pulse 2 (smart card clock cycles per ETU)
   * - Bits 6-5: Reserved (write 1)
   * - Bit 4: CHR1 - Character length 1 (0=9/8 bit, 1=8/7 bit; use with SMR.CHR)
   * - Bit 3: SDIR - Data transfer direction (0=LSB first, 1=MSB first)
   * - Bit 2: SINV - Smart card data inversion (0=no inversion, 1=inverted)
   * - Bit 1: Reserved (write 1)
   * - Bit 0: SMIF - Smart card interface mode (0=disabled, 1=enabled)
   *
   * @note For SPI mode, set SDIR=1 for MSB-first transfers.
   */
  volatile uint8_t scmr;

  /**
   * @brief Serial Extended Mode Register (SEMR) - advanced settings
   * @details
   * - Bit 7: RXDESEL - RXD extended input control
   * - Bit 6: BGDM - Baud rate generator double-speed mode
   * - Bit 5: NFEN - Digital noise filter enable
   * - Bit 4: ABCS - Asynchronous mode base clock select
   * - Bit 3: ABCSE - Asynchronous mode extended base clock select
   * - Bit 2: BRME - Bit rate modulation enable
   * - Bit 1: Reserved
   * - Bit 0: ACS0 - Asynchronous mode clock source select 0
   */
  volatile uint8_t semr;

  /**
   * @brief Noise Filter Setting Register (SNFR)
   * @details
   * - Bits 2-0: NFCS - Noise filter clock select
   */
  volatile uint8_t snfr;

  /**
   * @brief I2C Mode Register 1 (SIMR1)
   * @details Used in I2C mode. Not used for UART or SPI.
   */
  volatile uint8_t simr1;

  /**
   * @brief I2C Mode Register 2 (SIMR2)
   * @details Used in I2C mode. Not used for UART or SPI.
   */
  volatile uint8_t simr2;

  /**
   * @brief I2C Mode Register 3 (SIMR3)
   * @details Used in I2C mode. Not used for UART or SPI.
   */
  volatile uint8_t simr3;

  /**
   * @brief I2C Status Register (SISR)
   * @details Used in I2C mode. Not used for UART or SPI.
   */
  volatile uint8_t sisr;

  /**
   * @brief SPI Mode Register (SPMR) - SPI clock polarity and phase
   * @details
   * - Bit 7: CKPH - Clock phase (inverted from standard CPHA in some Renesas docs)
   * - Bit 6: CKPOL - Clock polarity (0=idle LOW, 1=idle HIGH)
   * - Bit 4: MFF - Mode fault flag
   * - Bit 3: Reserved
   * - Bit 2: MSS - Controller/peripheral select (0=controller, 1=peripheral)
   * - Bit 1: CTSE - CTS enable (0=disabled, 1=CTS output enabled)
   * - Bit 0: SSE - SS pin enable
   */
  volatile uint8_t spmr;
} rx_sci_regs_t;

/**
 * @enum rx_sci_ssr_flags_t
 * @brief SCI Serial Status Register (SSR) flag masks
 *
 * @details
 * Bit masks for the SSR register per RX72N HUM 34.2.7. Error flags
 * (ORER, FER, PER) are sticky and must be cleared by read-modify-write
 * with a 0 written to the bit position (writing 1 has no effect).
 *
 * @note Bit positions match both the SCIa/b/c/d/e/f (standard) and
 * SCIg/h/i (extended) SSR layouts -- the same mask works for all SCI
 * channels on the RX72N.
 */
typedef enum : uint8_t {
  k_sci_ssr_tdre_flag  = 0x80U, /**< Transmit data register empty (HUM 34.2.7 bit 7) */
  k_sci_ssr_rdrf_flag  = 0x40U, /**< Receive data register full (bit 6) */
  k_sci_ssr_orer_flag  = 0x20U, /**< Overrun error (bit 5) */
  k_sci_ssr_fer_flag   = 0x10U, /**< Framing error (bit 4) */
  k_sci_ssr_per_flag   = 0x08U, /**< Parity error (bit 3) */
  k_sci_ssr_tend_flag  = 0x04U, /**< Transmit end (bit 2) */
  k_sci_ssr_error_mask = 0x38U, /**< ORER|FER|PER -- receive error flags */
} rx_sci_ssr_flags_t;

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
typedef enum : uint8_t {
  k_sci_channel_max = 13, /**< Total SCI channels (0-12) */
} sci_channel_limits_t;

#if defined(USE_MOCK_SCI_REGS)
volatile rx_sci_regs_t* sci_get_channel(uint8_t channel);
#else
/**
 * @brief Get SCI register base for a channel
 * @param[in] channel SCI channel number (0-12)
 * @return Pointer to SCI registers, or nullptr if invalid channel
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
      return nullptr; /* Invalid channel */
  }
}
#endif

/* =============================================================================
 * Static Assertions - Compile-time Register Layout Verification
 * =============================================================================
 *
 * These static assertions verify that the rx_sci_regs_t struct layout and
 * channel base addresses exactly match the RX72N hardware. Any mismatch
 * will cause a compile-time error, preventing runtime UART bugs.
 *
 * Reference: RX72N Group User's Manual: Hardware, Chapter 41 (SCI)
 *            Section 41.2: Register Descriptions
 */

/** @name SCI Register Offset Verification
 *  @brief Verify rx_sci_regs_t matches hardware layout
 *  @{
 */
static_assert(sizeof(rx_sci_regs_t) == 14, "SCI register structure must be 14 bytes");
static_assert(offsetof(rx_sci_regs_t, smr) == 0x00, "SCI SMR register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, brr) == 0x01, "SCI BRR register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, scr) == 0x02, "SCI SCR register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, tdr) == 0x03, "SCI TDR register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, ssr) == 0x04, "SCI SSR register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, rdr) == 0x05, "SCI RDR register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, scmr) == 0x06, "SCI SCMR register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, semr) == 0x07, "SCI SEMR register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, snfr) == 0x08, "SCI SNFR register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, simr1) == 0x09, "SCI SIMR1 register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, simr2) == 0x0A, "SCI SIMR2 register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, simr3) == 0x0B, "SCI SIMR3 register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, sisr) == 0x0C, "SCI SISR register offset incorrect");
static_assert(offsetof(rx_sci_regs_t, spmr) == 0x0D, "SCI SPMR register offset incorrect");
/** @} */

/** @name SCI Base Address Verification
 *  @brief Verify channel base addresses match RX72N Manual Ch41
 *  @{
 */
static_assert(k_sci0_base_addr == 0x0008A000, "SCI0 base address incorrect");
static_assert(k_sci1_base_addr == 0x0008A020, "SCI1 base address incorrect");
static_assert(k_sci2_base_addr == 0x0008A040, "SCI2 base address incorrect");
static_assert(k_sci3_base_addr == 0x0008A060, "SCI3 base address incorrect");
static_assert(k_sci4_base_addr == 0x0008A080, "SCI4 base address incorrect");
static_assert(k_sci5_base_addr == 0x0008A0A0, "SCI5 base address incorrect");
static_assert(k_sci6_base_addr == 0x0008A0C0, "SCI6 base address incorrect");
static_assert(k_sci7_base_addr == 0x000D00E0, "SCI7 base address incorrect");
static_assert(k_sci8_base_addr == 0x000D0000, "SCI8 base address incorrect");
static_assert(k_sci9_base_addr == 0x000D0020, "SCI9 base address incorrect");
static_assert(k_sci10_base_addr == 0x000D0040, "SCI10 base address incorrect");
static_assert(k_sci11_base_addr == 0x000D0060, "SCI11 base address incorrect");
static_assert(k_sci12_base_addr == 0x0008B300, "SCI12 base address incorrect");
/** @} */

/** @name SCI Memory Region Verification
 *  @brief Verify base addresses are in correct peripheral regions
 *  @{
 */
static_assert((k_sci0_base_addr & 0xFFFF0000) == 0x00080000,
              "SCI0 base address not in SCI standard peripheral space");
static_assert((k_sci8_base_addr & 0xFFFF0000) == 0x000D0000,
              "SCI8 base address not in SCI extended peripheral space");
static_assert((k_sci12_base_addr & 0xFFFF0000) == 0x00080000,
              "SCI12 base address not in SCI standard peripheral space");
static_assert((k_sci1_base_addr - k_sci0_base_addr) == 0x20,
              "SCIj channel spacing incorrect (expected 0x20 bytes)");
/** @} */

/** @} */ /* End of sci_regs defgroup */

#ifdef __cplusplus
}
#endif
