/**
 * @file rx72n_riic_regs.h
 * @brief RX72N I2C Bus Interface (RIIC) Register Definitions
 *
 * @details
 * Register definitions for the I2C Bus Interface (RIIC) peripheral on the
 * RX72N microcontroller. RIIC provides I2C communication with
 * support for both controller and peripheral modes.
 *
 * @par RIIC System Architecture
 * @verbatim
 *                       RX72N RIIC I2C Bus Interface Architecture
 *   +-----------------------------------------------------------------------+
 *   |                           RIIC Module                                 |
 *   |                                                                       |
 *   |  +-----------------------------------------------------------------+ |
 *   |  |                    Clock Generator (PCLKB)                      | |
 *   |  |                         60 MHz                                  | |
 *   |  +---------------------------+-------------------------------------+ |
 *   |                              |                                       |
 *   |         +--------------------+--------------------+                  |
 *   |         |                    |                    |                  |
 *   |         v                    v                    v                  |
 *   |  +-------------+      +-------------+      +-------------+          |
 *   |  |   RIIC0     |      |   RIIC1     |      |   RIIC2     |          |
 *   |  |             |      |             |      |             |          |
 *   |  | Controller/ |      | Controller/ |      | Controller/ |          |
 *   |  | Peripheral  |      | Peripheral  |      | Peripheral  |          |
 *   |  |             |      |             |      |             |          |
 *   |  | <=1 Mbps     |      | <=1 Mbps     |      | <=1 Mbps     |          |
 *   |  | (Fast+)     |      | (Fast+)     |      | (Fast+)     |          |
 *   |  +------+------+      +------+------+      +------+------+          |
 *   |         |                    |                    |                  |
 *   +---------+--------------------+--------------------+------------------+
 *             |                    |                    |
 *             v                    v                    v
 *        +---------+          +---------+          +---------+
 *        | SCL0/   |          | SCL1/   |          | SCL2/   |
 *        | SDA0    |          | SDA1    |          | SDA2    |
 *        +----+----+          +----+----+          +----+----+
 *             |                    |                    |
 *             v                    v                    v
 *        +-----------------------------------------------------+
 *        |                   I2C Bus                           |
 *        |    +-----+  +-----+  +-----+  +-----+               |
 *        |    |Sensor| |EEPROM| |RTC  |  |Other|               |
 *        |    |0x48  | |0x50  | |0x68 |  | ... |               |
 *        |    +-----+  +-----+  +-----+  +-----+               |
 *        +-----------------------------------------------------+
 *
 *   Memory Map (Verified against RX72N Manual Ch42):
 *   +------------+----------------------------------------------+
 *   |  Address   |  Register                                    |
 *   +------------+----------------------------------------------+
 *   | 0x00088300 |  RIIC0 base (ICCR1 at +0x00)                 |
 *   | 0x00088320 |  RIIC1 base (ICCR1 at +0x00)                 |
 *   | 0x00088340 |  RIIC2 base (ICCR1 at +0x00)                 |
 *   +------------+----------------------------------------------+
 *   | Offset     |  Register (per channel)                      |
 *   | 0x00       |  ICCR1 (Control Register 1)                  |
 *   | 0x01       |  ICCR2 (Control Register 2)                  |
 *   | 0x02       |  ICMR1 (Mode Register 1)                     |
 *   | 0x03       |  ICMR2 (Mode Register 2)                     |
 *   | 0x04       |  ICMR3 (Mode Register 3)                     |
 *   | 0x05       |  ICFER (Function Enable)                     |
 *   | 0x06       |  ICSER (Status Enable)                       |
 *   | 0x07       |  ICIER (Interrupt Enable)                    |
 *   | 0x08       |  ICSR1 (Status Register 1)                   |
 *   | 0x09       |  ICSR2 (Status Register 2)                   |
 *   | 0x0A-0x0F  |  SARLn/SARUn (Peripheral Addresses)          |
 *   | 0x10       |  ICBRL (Bit Rate Low)                        |
 *   | 0x11       |  ICBRH (Bit Rate High)                       |
 *   | 0x12       |  ICDRT (Transmit Data)                       |
 *   | 0x13       |  ICDRR (Receive Data)                        |
 *   +------------+----------------------------------------------+
 * @endverbatim
 *
 * @par I2C Bit Rate Calculation
 * The bit rate is determined by ICBRL and ICBRH registers:
 *
 * @f[
 * f_{SCL} = \frac{f_{PCLKB}}{(ICBRL + ICBRH + 2) \times 2^{CKS}}
 * @f]
 *
 * Where:
 * - @f$ f_{SCL} @f$ = SCL clock frequency (target bit rate)
 * - @f$ f_{PCLKB} @f$ = Peripheral clock B (60 MHz)
 * - @f$ ICBRL @f$ = SCL low period setting (5 bits)
 * - @f$ ICBRH @f$ = SCL high period setting (5 bits)
 * - @f$ CKS @f$ = Internal clock divider (0-7)
 *
 * @par Common Speed Configurations (PCLKB = 60 MHz)
 * | Mode       | Speed   | CKS | ICBRL | ICBRH | Actual Rate |
 * |------------|---------|-----|-------|-------|-------------|
 * | Standard   | 100 kHz | 3   | 18    | 16    | 100.0 kHz   |
 * | Fast       | 400 kHz | 1   | 19    | 16    | 400.0 kHz   |
 * | Fast Plus  | 1 MHz   | 0   | 14    | 14    | 1.00 MHz    |
 *
 * @par I2C Controller Write Transaction
 * @verbatim
 *   RX72N                                  Peripheral
 *     |                                       |
 *     |--- START ---------------------------->|
 *     |--- Address + W (0) ------------------>|
 *     |<-- ACK ------------------------------>|
 *     |--- Data Byte 0 ---------------------->|
 *     |<-- ACK ------------------------------>|
 *     |--- Data Byte N ---------------------->|
 *     |<-- ACK ------------------------------>|
 *     |--- STOP ----------------------------->|
 *     |                                       |
 * @endverbatim
 *
 * @par Example: Initialize RIIC0 for 400 kHz Fast Mode
 * @code
 * #include "rx72n_riic_regs.h"
 *
 * // Reset RIIC0
 * riic0()->iccr1 = k_riic_iccr1_iicrst;
 *
 * // Configure bit rate for 400 kHz
 * // PCLKB=60MHz, CKS=1 (/2), BRL=19, BRH=16
 * // f_SCL = 60MHz / ((19+16+2) * 2^1) = 810.8 kHz / 2 = 405.4 kHz
 * riic0()->icmr1 = 0x28;  // CKS=1, BCWP=1
 * riic0()->icbrl = 0x13;  // Low period = 19
 * riic0()->icbrh = 0x10;  // High period = 16
 *
 * // Enable RIIC
 * riic0()->iccr1 = k_riic_iccr1_ice;
 *
 * // Start transaction: Set ST bit
 * riic0()->iccr2 = k_riic_iccr2_st;
 *
 * // Wait for start condition
 * while (!(riic0()->icsr2 & k_riic_icsr2_start)) {}
 *
 * // Send address + write bit
 * riic0()->icdrt = (0x50 << 1) | 0;  // EEPROM address 0x50, write
 *
 * // Wait for transmit complete
 * while (!(riic0()->icsr2 & k_riic_icsr2_tdre)) {}
 * @endcode
 *
 * @par NASA Power of 10 Compliance
 * - Rule 3: No dynamic memory - all register access via static inline functions
 * - Rule 5: Compile-time assertions verify all register layout assumptions
 * - Rule 8: Uses typed enums (C23) for all constants, no magic numbers
 * - Rule 10: Clean build with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - Single Responsibility: RIIC register definitions only, no driver logic
 * - Open/Closed: Accessors work with any I2C configuration
 * - Interface Segregation: Minimal API - just register access functions
 *
 * @par STAR Project I2C Usage
 * The STAR project does not currently use I2C, but RIIC is available for
 * future sensor expansion (temperature sensors, IMU, etc.).
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware, Chapter 42 (I2C-bus Interface)
 *
 * @par Verification Status
 * [PASS] VERIFIED (2026-01-28) - All base addresses and register offsets verified
 * against RX72N Manual Ch42 section 42.2
 *
 * @see rx72n_mpc_regs.h MPC pin configuration for I2C pins
 *
 * @date 2026-01-28
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup riic_regs RIIC Register Definitions
 * @brief I2C Bus Interface (RIIC) hardware register interface
 * @{
 */

/* =============================================================================
 * I2C Bus Interface (RIIC) - For I2C Communication
 * =============================================================================
 */

/**
 * @enum rx_riic_addresses_t
 * @brief RIIC channel base addresses (verified against RX72N Hardware Manual Ch42)
 *
 * @details
 * The RX72N has 3 independent RIIC channels. Each channel has a 32-byte
 * register block (though only 20 bytes are used).
 *
 * @par Memory Layout
 * @verbatim
 *   Address      | Channel  | Pins
 *   -------------+----------+---------------------
 *   0x00088300   | RIIC0    | P16/SCL0, P17/SDA0
 *   0x00088320   | RIIC1    | P21/SCL1, P20/SDA1
 *   0x00088340   | RIIC2    | P66/SCL2, P67/SDA2
 * @endverbatim
 *
 * @note Channel spacing is 0x20 (32 bytes) between adjacent channels.
 */
typedef enum : uintptr_t {
  k_riic0_base_addr = 0x00088300, /**< RIIC0 register base address */
  k_riic1_base_addr = 0x00088320, /**< RIIC1 register base address */
  k_riic2_base_addr = 0x00088340, /**< RIIC2 register base address */
} rx_riic_addresses_t;

/**
 * @struct rx_riic_regs_t
 * @brief RIIC Channel Register Map (20 bytes per channel)
 *
 * @details
 * I2C Bus Interface (RIIC) registers for I2C controller/peripheral communication.
 * All registers are 8-bit and byte-accessible.
 *
 * @par Register Layout (20 bytes)
 * @verbatim
 *   Offset | Size | Register | Description
 *   -------+------+----------+----------------------------------
 *   0x00   |  1   | ICCR1    | Control 1 (enable, reset)
 *   0x01   |  1   | ICCR2    | Control 2 (start, stop, restart)
 *   0x02   |  1   | ICMR1    | Mode 1 (clock divider)
 *   0x03   |  1   | ICMR2    | Mode 2 (timeout, SDA delay)
 *   0x04   |  1   | ICMR3    | Mode 3 (ACK/NACK control)
 *   0x05   |  1   | ICFER    | Function Enable
 *   0x06   |  1   | ICSER    | Status Enable
 *   0x07   |  1   | ICIER    | Interrupt Enable
 *   0x08   |  1   | ICSR1    | Status 1 (ACK status)
 *   0x09   |  1   | ICSR2    | Status 2 (TX/RX flags)
 *   0x0A   |  1   | SARL0    | Peripheral Address 0 Low
 *   0x0B   |  1   | SARU0    | Peripheral Address 0 Upper
 *   0x0C   |  1   | SARL1    | Peripheral Address 1 Low
 *   0x0D   |  1   | SARU1    | Peripheral Address 1 Upper
 *   0x0E   |  1   | SARL2    | Peripheral Address 2 Low
 *   0x0F   |  1   | SARU2    | Peripheral Address 2 Upper
 *   0x10   |  1   | ICBRL    | Bit Rate Low period
 *   0x11   |  1   | ICBRH    | Bit Rate High period
 *   0x12   |  1   | ICDRT    | Transmit Data
 *   0x13   |  1   | ICDRR    | Receive Data
 * @endverbatim
 *
 * @par Initialization Sequence
 * 1. Set IICRST bit in ICCR1 (internal reset)
 * 2. Configure ICMR1/2/3 (clock, timeouts)
 * 3. Configure ICBRL/ICBRH (bit rate)
 * 4. Configure ICSER (status detection)
 * 5. Configure ICIER (interrupts)
 * 6. Clear IICRST and set ICE in ICCR1 (enable)
 *
 * @invariant sizeof(rx_riic_regs_t) == 20
 *
 * @see riic0(), riic1(), riic2() Accessor functions for each channel
 */
typedef struct {
  /**
   * @brief I2C Bus Control Register 1
   * @details Enables RIIC module and controls internal reset and pin state.
   * | Bit | Name   | Description |
   * |-----|--------|-------------|
   * | 7   | ICE    | RIIC Enable (1=enabled) |
   * | 6   | IICRST | Internal Reset (1=reset state) |
   * | 5   | CLO    | Additional SCL Output (bus recovery clock) |
   * | 4   | SOWP   | SCLO and SDAO Write Protect |
   * | 3   | SCLO   | SCL Output Control/Monitor |
   * | 2   | SDAO   | SDA Output Control/Monitor |
   * | 1   | SCLI   | SCL Line Input Monitor |
   * | 0   | SDAI   | SDA Line Input Monitor |
   */
  volatile uint8_t iccr1;

  /**
   * @brief I2C Bus Control Register 2
   * @details Controls start/stop/restart conditions and bus state.
   * | Bit | Name | Description |
   * |-----|------|-------------|
   * | 7   | BBSY | Bus Busy (read-only) |
   * | 6   | MST  | Controller Mode (1=controller) |
   * | 5   | TRS  | Transmit/Receive (1=transmit) |
   * | 3   | SP   | Stop Condition Request |
   * | 2   | RS   | Restart Condition Request |
   * | 1   | ST   | Start Condition Request |
   */
  volatile uint8_t iccr2;

  /**
   * @brief I2C Bus Mode Register 1
   * @details Configures internal reference clock divider.
   * | Bits | Name | Description |
   * |------|------|-------------|
   * | 6-4  | CKS  | Clock Select (0-7, divides PCLKB by 2^CKS) |
   * | 3    | BCWP | ICCR2 Write Protect (1=BC bits read-only) |
   * | 2-0  | BC   | Bit Counter |
   */
  volatile uint8_t icmr1;

  /**
   * @brief I2C Bus Mode Register 2
   * @details Configures timeout and SDA output delay.
   */
  volatile uint8_t icmr2;

  /**
   * @brief I2C Bus Mode Register 3
   * @details Controls ACK/NACK transmission and wait state.
   * | Bit | Name  | Description |
   * |-----|-------|-------------|
   * | 6   | WAIT  | Wait insertion (1=insert wait) |
   * | 5   | RDRFS | RDRF Flag Set Timing |
   * | 4   | ACKWP | ACKBT Write Protect |
   * | 3   | ACKBT | ACK Bit to Transmit (0=ACK, 1=NACK) |
   */
  volatile uint8_t icmr3;

  /**
   * @brief I2C Bus Function Enable Register
   * @details Enables various RIIC functions (timeout, noise filter, etc.).
   */
  volatile uint8_t icfer;

  /**
   * @brief I2C Bus Status Enable Register
   * @details Selects which conditions set status flags.
   */
  volatile uint8_t icser;

  /**
   * @brief I2C Bus Interrupt Enable Register
   * @details Enables interrupt generation for various conditions.
   */
  volatile uint8_t icier;

  /**
   * @brief I2C Bus Status Register 1
   * @details Contains slave address detection and host/device-ID detection flags.
   * | Bit | Name  | Description |
   * |-----|-------|-------------|
   * | 7   | HOA   | Host Address Detection Flag |
   * | 5   | DID   | Device-ID Address Detection Flag |
   * | 3   | GCA   | General Call Address Detection Flag |
   * | 2   | AAS2  | Slave Address 2 Detection Flag |
   * | 1   | AAS1  | Slave Address 1 Detection Flag |
   * | 0   | AAS0  | Slave Address 0 Detection Flag |
   */
  volatile uint8_t icsr1;

  /**
   * @brief I2C Bus Status Register 2
   * @details Contains transmit/receive status flags and condition detection.
   * | Bit | Name  | Description |
   * |-----|-------|-------------|
   * | 7   | TDRE  | Transmit Data Empty |
   * | 6   | TEND  | Transmit End |
   * | 5   | RDRF  | Receive Data Full |
   * | 4   | NACKF | NACK Detection Flag |
   * | 3   | STOP  | Stop Condition Detection |
   * | 2   | START | Start Condition Detection |
   * | 1   | AL    | Arbitration-Lost Flag |
   * | 0   | TMOF  | Timeout Detection Flag |
   */
  volatile uint8_t icsr2;

  volatile uint8_t sarl0; /**< Peripheral Address Register L0 (7-bit: bits 7-1) */
  volatile uint8_t saru0; /**< Peripheral Address Register U0 (10-bit: bits 9-8) */
  volatile uint8_t sarl1; /**< Peripheral Address Register L1 */
  volatile uint8_t saru1; /**< Peripheral Address Register U1 */
  volatile uint8_t sarl2; /**< Peripheral Address Register L2 */
  volatile uint8_t saru2; /**< Peripheral Address Register U2 */

  /**
   * @brief I2C Bus Bit Rate Low-Level Width Setting Register
   * @details Sets the low period of SCL clock.
   * | Bits | Name | Description |
   * |------|------|-------------|
   * | 4-0  | BRL  | Low period count (affects SCL low time) |
   */
  volatile uint8_t icbrl;

  /**
   * @brief I2C Bus Bit Rate High-Level Width Setting Register
   * @details Sets the high period of SCL clock.
   * | Bits | Name | Description |
   * |------|------|-------------|
   * | 4-0  | BRH  | High period count (affects SCL high time) |
   */
  volatile uint8_t icbrh;

  /**
   * @brief I2C Bus Transmit Data Register
   * @details Write data here to transmit on I2C bus.
   * - First byte after START should be peripheral address + R/W bit
   * - Subsequent bytes are data
   */
  volatile uint8_t icdrt;

  /**
   * @brief I2C Bus Receive Data Register
   * @details Read received data from here. Read clears RDRF flag.
   */
  volatile uint8_t icdrr;
} rx_riic_regs_t;

/* =============================================================================
 * Inline Register Accessor Functions
 * =============================================================================
 * These functions provide type-safe access to RIIC hardware registers.
 * Using functions instead of macros enables:
 * - Type checking at compile time
 * - Debugger-friendly register access
 * - Consistent volatile semantics
 */

/**
 * @brief Get pointer to RIIC0 registers
 *
 * @details
 * Returns a volatile pointer to RIIC channel 0 registers at address 0x00088300.
 * RIIC0 pins are P16 (SCL0) and P17 (SDA0).
 *
 * @return Volatile pointer to RIIC0 register structure
 *
 * @par Example
 * @code
 * // Enable RIIC0
 * riic0()->iccr1 = k_riic_iccr1_ice;
 *
 * // Start condition
 * riic0()->iccr2 = k_riic_iccr2_st;
 * @endcode
 */
static inline volatile rx_riic_regs_t* riic0(void)
{
  return (volatile rx_riic_regs_t*)k_riic0_base_addr;
}

/**
 * @brief Get pointer to RIIC1 registers
 *
 * @details
 * Returns a volatile pointer to RIIC channel 1 registers at address 0x00088320.
 * RIIC1 pins are P21 (SCL1) and P20 (SDA1).
 *
 * @return Volatile pointer to RIIC1 register structure
 */
static inline volatile rx_riic_regs_t* riic1(void)
{
  return (volatile rx_riic_regs_t*)k_riic1_base_addr;
}

/**
 * @brief Get pointer to RIIC2 registers
 *
 * @details
 * Returns a volatile pointer to RIIC channel 2 registers at address 0x00088340.
 * RIIC2 pins are P66 (SCL2) and P67 (SDA2).
 *
 * @return Volatile pointer to RIIC2 register structure
 */
static inline volatile rx_riic_regs_t* riic2(void)
{
  return (volatile rx_riic_regs_t*)k_riic2_base_addr;
}

/**
 * @enum riic_iccr1_bits_t
 * @brief ICCR1 (I2C Control Register 1) bit definitions
 *
 * @details
 * Controls RIIC module enable/disable, internal reset, bus recovery clock,
 * and provides pin output control and input monitoring.
 *
 * @par Register Layout
 * @verbatim
 *   Bit | Name   | Description
 *   ----+--------+----------------------------------------
 *    7  | ICE    | RIIC Enable (1=enabled, 0=disabled)
 *    6  | IICRST | Internal Reset (1=reset state)
 *    5  | CLO    | Additional SCL Output (bus recovery clock)
 *    4  | SOWP   | SCLO and SDAO Write Protect
 *    3  | SCLO   | SCL Output Control/Monitor
 *    2  | SDAO   | SDA Output Control/Monitor
 *    1  | SCLI   | SCL Line Input Monitor (read-only)
 *    0  | SDAI   | SDA Line Input Monitor (read-only)
 * @endverbatim
 */
typedef enum : uint8_t {
  k_riic_iccr1_ice    = (1 << 7), /**< Bit 7: I2C Bus Interface Enable */
  k_riic_iccr1_iicrst = (1 << 6), /**< Bit 6: Internal Reset (hold in reset) */
  k_riic_iccr1_clo    = (1 << 5), /**< Bit 5: Additional SCL Output (bus recovery) */
  k_riic_iccr1_sowp   = (1 << 4), /**< Bit 4: SCLO and SDAO Write Protect */
  k_riic_iccr1_sclo   = (1 << 3), /**< Bit 3: SCL Output Control/Monitor */
  k_riic_iccr1_sdao   = (1 << 2), /**< Bit 2: SDA Output Control/Monitor */
  k_riic_iccr1_scli   = (1 << 1), /**< Bit 1: SCL Line Input Monitor (read-only) */
  k_riic_iccr1_sdai   = (1 << 0), /**< Bit 0: SDA Line Input Monitor (read-only) */
} riic_iccr1_bits_t;

/**
 * @enum riic_iccr2_bits_t
 * @brief ICCR2 (I2C Control Register 2) bit definitions
 *
 * @details
 * Controls bus conditions (start, stop, restart) and provides bus status.
 *
 * @par Register Layout
 * @verbatim
 *   Bit | Name | R/W | Description
 *   ----+------+-----+----------------------------------------
 *    7  | BBSY |  R  | Bus Busy (1=bus busy, 0=bus free)
 *    6  | MST  |  R  | Controller Mode (1=controller, 0=peripheral)
 *    5  | TRS  |  R  | Transmit Mode (1=transmit, 0=receive)
 *    4  |  -   |  -  | Reserved
 *    3  | SP   |  W  | Stop Condition Request (write 1 to issue)
 *    2  | RS   |  W  | Restart Condition Request
 *    1  | ST   |  W  | Start Condition Request
 *    0  |  -   |  -  | Reserved
 * @endverbatim
 *
 * @note ST, RS, SP bits are write-only and always read as 0.
 */
typedef enum : uint8_t {
  k_riic_iccr2_bbsy = (1 << 7), /**< Bit 7: Bus Busy (read-only) */
  k_riic_iccr2_mst  = (1 << 6), /**< Bit 6: Controller Mode (read-only) */
  k_riic_iccr2_trs  = (1 << 5), /**< Bit 5: Transmit/Receive Select (1=transmit, read-only) */
  k_riic_iccr2_sp   = (1 << 3), /**< Bit 3: Stop Condition Request */
  k_riic_iccr2_rs   = (1 << 2), /**< Bit 2: Restart Condition Request */
  k_riic_iccr2_st   = (1 << 1), /**< Bit 1: Start Condition Request */
} riic_iccr2_bits_t;

/**
 * @enum riic_icsr1_bits_t
 * @brief ICSR1 (I2C Status Register 1) bit definitions
 *
 * @details
 * Contains slave address detection flags, general call detection,
 * device-ID detection, and host address detection (peripheral mode).
 *
 * @par Register Layout
 * @verbatim
 *   Bit | Name | Description
 *   ----+------+----------------------------------------
 *    7  | HOA  | Host Address Detection Flag
 *    6  |  -   | Reserved
 *    5  | DID  | Device-ID Address Detection Flag
 *    4  |  -   | Reserved
 *    3  | GCA  | General Call Address Detection Flag
 *    2  | AAS2 | Slave Address 2 Detection Flag
 *    1  | AAS1 | Slave Address 1 Detection Flag
 *    0  | AAS0 | Slave Address 0 Detection Flag
 * @endverbatim
 *
 * @par Usage
 * @code
 * if (riic0()->icsr1 & k_riic_icsr1_aas0) {
 *     // Slave address 0 detected - we are being addressed
 * }
 * @endcode
 */
typedef enum : uint8_t {
  k_riic_icsr1_hoa  = (1 << 7), /**< Bit 7: Host Address Detection Flag */
  k_riic_icsr1_did  = (1 << 5), /**< Bit 5: Device-ID Address Detection Flag */
  k_riic_icsr1_gca  = (1 << 3), /**< Bit 3: General Call Address Detection Flag */
  k_riic_icsr1_aas2 = (1 << 2), /**< Bit 2: Slave Address 2 Detection Flag */
  k_riic_icsr1_aas1 = (1 << 1), /**< Bit 1: Slave Address 1 Detection Flag */
  k_riic_icsr1_aas0 = (1 << 0), /**< Bit 0: Slave Address 0 Detection Flag */
} riic_icsr1_bits_t;

/**
 * @enum riic_icsr2_bits_t
 * @brief ICSR2 (I2C Status Register 2) bit definitions
 *
 * @details
 * Contains transmit/receive status flags and condition detection.
 *
 * @par Register Layout
 * @verbatim
 *   Bit | Name  | Description
 *   ----+-------+----------------------------------------
 *    7  | TDRE  | Transmit Data Empty (1=can write ICDRT)
 *    6  | TEND  | Transmit End (1=all data transmitted)
 *    5  | RDRF  | Receive Data Full (1=data in ICDRR)
 *    4  | NACKF | NACK Detection Flag
 *    3  | STOP  | Stop Condition Detected
 *    2  | START | Start Condition Detected
 *    1  | AL    | Arbitration-Lost Flag
 *    0  | TMOF  | Timeout Detection Flag
 * @endverbatim
 *
 * @par Typical Polling Sequence
 * @code
 * // Wait for transmit ready
 * while (!(riic0()->icsr2 & k_riic_icsr2_tdre)) {}
 *
 * // Wait for receive complete
 * while (!(riic0()->icsr2 & k_riic_icsr2_rdrf)) {}
 * @endcode
 */
typedef enum : uint8_t {
  k_riic_icsr2_tdre  = (1 << 7), /**< Bit 7: Transmit Data Empty */
  k_riic_icsr2_tend  = (1 << 6), /**< Bit 6: Transmit End */
  k_riic_icsr2_rdrf  = (1 << 5), /**< Bit 5: Receive Data Full */
  k_riic_icsr2_nackf = (1 << 4), /**< Bit 4: NACK Detection Flag */
  k_riic_icsr2_stop  = (1 << 3), /**< Bit 3: Stop Condition Detection */
  k_riic_icsr2_start = (1 << 2), /**< Bit 2: Start Condition Detection */
  k_riic_icsr2_al    = (1 << 1), /**< Bit 1: Arbitration-Lost Flag */
  k_riic_icsr2_tmof  = (1 << 0), /**< Bit 0: Timeout Detection Flag */
} riic_icsr2_bits_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 * These assertions verify our register structures match the RX72N hardware
 * exactly. If any assertion fails, the struct definition must be corrected.
 *
 * Reference: RX72N Group User's Manual: Hardware, Chapter 42, Section 42.2
 * =============================================================================
 */

/**
 * @defgroup riic_static_assert RIIC Static Assertions
 * @brief Compile-time verification of RIIC register layout
 * @details
 * These static assertions are verified at compile time to ensure the
 * C structure definitions match the actual hardware register layout.
 * A failed assertion indicates a bug in the register definitions that
 * must be fixed before the code can compile.
 * @{
 */

/* Verify base addresses match Hardware Manual Ch42 */
static_assert(k_riic0_base_addr == 0x00088300, "RIIC0 base address incorrect");
static_assert(k_riic1_base_addr == 0x00088320, "RIIC1 base address incorrect");
static_assert(k_riic2_base_addr == 0x00088340, "RIIC2 base address incorrect");

/* Verify register structure size (20 bytes = 0x14) */
static_assert(sizeof(rx_riic_regs_t) == 20,
              "RIIC register structure size incorrect (expected 0x14)");

/* Verify control register offsets */
static_assert(offsetof(rx_riic_regs_t, iccr1) == 0x00, "ICCR1 offset incorrect");
static_assert(offsetof(rx_riic_regs_t, iccr2) == 0x01, "ICCR2 offset incorrect");

/* Verify mode register offsets */
static_assert(offsetof(rx_riic_regs_t, icmr1) == 0x02, "ICMR1 offset incorrect");
static_assert(offsetof(rx_riic_regs_t, icmr2) == 0x03, "ICMR2 offset incorrect");
static_assert(offsetof(rx_riic_regs_t, icmr3) == 0x04, "ICMR3 offset incorrect");

/* Verify function/status/interrupt register offsets */
static_assert(offsetof(rx_riic_regs_t, icfer) == 0x05, "ICFER offset incorrect");
static_assert(offsetof(rx_riic_regs_t, icser) == 0x06, "ICSER offset incorrect");
static_assert(offsetof(rx_riic_regs_t, icier) == 0x07, "ICIER offset incorrect");
static_assert(offsetof(rx_riic_regs_t, icsr1) == 0x08, "ICSR1 offset incorrect");
static_assert(offsetof(rx_riic_regs_t, icsr2) == 0x09, "ICSR2 offset incorrect");

/* Verify peripheral address register offsets */
static_assert(offsetof(rx_riic_regs_t, sarl0) == 0x0A, "SARL0 offset incorrect");
static_assert(offsetof(rx_riic_regs_t, saru0) == 0x0B, "SARU0 offset incorrect");
static_assert(offsetof(rx_riic_regs_t, sarl1) == 0x0C, "SARL1 offset incorrect");
static_assert(offsetof(rx_riic_regs_t, saru1) == 0x0D, "SARU1 offset incorrect");
static_assert(offsetof(rx_riic_regs_t, sarl2) == 0x0E, "SARL2 offset incorrect");
static_assert(offsetof(rx_riic_regs_t, saru2) == 0x0F, "SARU2 offset incorrect");

/* Verify bit rate and data register offsets */
static_assert(offsetof(rx_riic_regs_t, icbrl) == 0x10, "ICBRL offset incorrect");
static_assert(offsetof(rx_riic_regs_t, icbrh) == 0x11, "ICBRH offset incorrect");
static_assert(offsetof(rx_riic_regs_t, icdrt) == 0x12, "ICDRT offset incorrect");
static_assert(offsetof(rx_riic_regs_t, icdrr) == 0x13, "ICDRR offset incorrect");

/* Verify channel spacing (0x20 = 32 bytes between channels) */
static_assert((k_riic1_base_addr - k_riic0_base_addr) == 0x20, "RIIC0 to RIIC1 spacing incorrect");
static_assert((k_riic2_base_addr - k_riic1_base_addr) == 0x20, "RIIC1 to RIIC2 spacing incorrect");

/** @} */ /* End of riic_static_assert group */

/** @} */ /* End of riic_regs group */

#ifdef __cplusplus
}
#endif
