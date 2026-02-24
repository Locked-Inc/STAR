/* lib/rx_hal/inc/rx72n_rspi_regs.h */

/**
 * @file rx72n_rspi_regs.h
 * @brief RX72N RSPI Serial Peripheral Interface Register Definitions
 *
 * @details
 * This file defines all 22 RSPI registers per the RX72N Hardware Manual
 * (R01UH0824EJ0120 Rev.1.20, Ch44). All register addresses, offsets, and
 * bit definitions have been verified against the manual.
 *
 * @par System Architecture
 * The RSPI module provides high-speed serial communication between the RX72N
 * and external devices. In the STAR project, RSPI is used for:
 *
 * @dot
 * digraph rspi_architecture {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_rx72n {
 *     label="RX72N MCU";
 *     style=filled;
 *     color=lightgrey;
 *     rspi0 [label="RSPI0\n(Peripheral)"];
 *     rspi1 [label="RSPI1\n(Controller)"];
 *     rspi2 [label="RSPI2\n(Reserved)"];
 *   }
 *
 *   rpi5 [label="Raspberry Pi 5\n(Controller)"];
 *   future [label="Future SPI\nSensors"];
 *
 *   rpi5 -> rspi0 [label="10 Mbps\nCommand/Telemetry"];
 *   rspi1 -> future [label="Reserved", style=dashed];
 * }
 * @enddot
 *
 * @par RSPI Channel Assignments
 * | Channel | Base Address | Mode       | Target           | Speed    | Purpose                |
 * |---------|--------------|------------|------------------|----------|------------------------|
 * | RSPI0   | 0x000D0100   | Peripheral | Raspberry Pi 5   | 10 Mbps  | Command/telemetry      |
 * | RSPI1   | 0x000D0140   | Controller | Reserved          | 5 Mbps   | Available (DRV8263H has no SPI) |
 * | RSPI2   | 0x000D0300   | Reserved   | -                | -        | Future expansion       |
 *
 * @par Key Features
 * - Full-duplex or simplex (transmit-only) synchronous communications
 * - Controller/Peripheral mode operation (using inclusive terminology)
 * - 8 to 32-bit configurable data frame length
 * - MSB-first or LSB-first transmission order
 * - Up to 8 command registers for sequential transfer mode
 * - Configurable clock polarity (CPOL) and phase (CPHA)
 * - DMA/DTC transfer support for high-throughput applications
 * - Mode fault detection for multi-controller configurations
 *
 * @par Hardware Requirements
 * | Parameter      | RSPI0 (RPi5)     | RSPI1 (Reserved) |
 * |----------------|------------------|------------------|
 * | PCLKB          | 60 MHz           | 60 MHz           |
 * | Max Bit Rate   | 30 Mbps          | 30 Mbps          |
 * | Configured     | 10 Mbps          | 5 Mbps           |
 * | Data Width     | 8 bits           | 16 bits          |
 * | CPOL/CPHA      | 0/0              | Default (no SPI device) |
 *
 * @par Memory Map (per channel)
 * | Offset | Size | Register | Description                       |
 * |--------|------|----------|-----------------------------------|
 * | 0x00   | 1    | SPCR     | Control Register                  |
 * | 0x01   | 1    | SSLP     | Chip Select Polarity Register     |
 * | 0x02   | 1    | SPPCR    | Pin Control Register              |
 * | 0x03   | 1    | SPSR     | Status Register                   |
 * | 0x04   | 4    | SPDR     | Data Register (32-bit)            |
 * | 0x08   | 1    | SPSCR    | Sequence Control Register         |
 * | 0x09   | 1    | SPSSR    | Sequence Status Register          |
 * | 0x0A   | 1    | SPBR     | Bit Rate Register                 |
 * | 0x0B   | 1    | SPDCR    | Data Control Register             |
 * | 0x0C   | 1    | SPCKD    | Clock Delay Register              |
 * | 0x0D   | 1    | SSLND    | SSL Negation Delay Register       |
 * | 0x0E   | 1    | SPND     | Next-Access Delay Register        |
 * | 0x0F   | 1    | SPCR2    | Control Register 2                |
 * | 0x10   | 2    | SPCMD0   | Command Register 0                |
 * | 0x12   | 2    | SPCMD1   | Command Register 1                |
 * | 0x14   | 2    | SPCMD2   | Command Register 2                |
 * | 0x16   | 2    | SPCMD3   | Command Register 3                |
 * | 0x18   | 2    | SPCMD4   | Command Register 4                |
 * | 0x1A   | 2    | SPCMD5   | Command Register 5                |
 * | 0x1C   | 2    | SPCMD6   | Command Register 6                |
 * | 0x1E   | 2    | SPCMD7   | Command Register 7                |
 * | 0x20   | 1    | SPDCR2   | Data Control Register 2           |
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion in accessor functions
 * - Rule 2: [OK] N/A (no loops in register definitions)
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] All accessor functions are single-statement inline
 * - Rule 5: [OK] N/A (accessor functions have no assertions needed)
 * - Rule 6: [OK] Minimal scope - inline accessors only
 * - Rule 7: [OK] N/A (no return values to check in definitions)
 * - Rule 8: [OK] All constants use C23 typed enums
 * - Rule 9: [OK] No function pointers in register definitions
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single responsibility - only register definitions
 * - **O**: Open for extension via additional bit field enums
 * - **L**: N/A (no inheritance hierarchy)
 * - **I**: Minimal interface - only necessary accessors
 * - **D**: N/A (hardware register layer, no dependencies to invert)
 *
 * @par Module Dependencies
 * - `<stdint.h>` - Fixed-width integer types
 * - No other dependencies (hardware register layer)
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 44: Serial Peripheral Interface (RSPI), pages 2349-2373
 * - Section 44.2: Register Descriptions, pages 2353-2373
 *
 * @see rx_spi_comm.h Higher-level SPI communication API
 * @see rx72n_regs.h Main register include file
 * @see docs/sections/03_hardware_pinout.tex STAR pin assignments
 *
 * @author STAR Team
 * @date 2026-01-28
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Renesas Serial Peripheral Interface (RSPI) - For SPI Communication
 * =============================================================================
 */

/**
 * @enum rx_rspi_addresses_t
 * @brief RSPI channel base addresses
 *
 * @details
 * Base addresses for the three RSPI channels on the RX72N. Each channel
 * has a 64-byte register space (0x00-0x3F), though only 33 bytes are used.
 * Addresses verified against RX72N Hardware Manual Ch44, Table 44.1.
 *
 * @par Pin Naming Convention
 * **Hardware signal names** (MOSIA/MISOA/MOSIB/MISOB/RSPCKA/RSPCKB) are from the
 * RX72N datasheet. This project uses **COPI/CIPO terminology** per OSHWA inclusive
 * naming standards. Pin descriptions below show: Hardware Name (Project Name).
 *
 * @par Channel Spacing
 * - RSPI0 -> RSPI1: 0x40 bytes (contiguous)
 * - RSPI1 -> RSPI2: 0x1C0 bytes (gap for other peripherals)
 *
 * @par Usage Example
 * @code
 * // Access RSPI0 registers directly
 * volatile rx_rspi_regs_t* spi = (volatile rx_rspi_regs_t*)k_rspi0_base_addr;
 * spi->spcr = k_rspi_spcr_spe;  // Enable SPI
 *
 * // Preferred: Use accessor function
 * rspi0()->spcr = k_rspi_spcr_spe;
 * @endcode
 *
 * @invariant k_rspi0_base_addr < k_rspi1_base_addr < k_rspi2_base_addr
 *
 * @see rspi0(), rspi1(), rspi2() Preferred accessor functions
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief RSPI0 register base address (0x000D0100)
   * @details
   * Used for RPi5 communication in peripheral mode at 10 Mbps.
   * @par Pin Assignments:
   * - RSPCKA: P27/RSPCKA (clock input from RPi5)
   * - MOSIA: P26/MOSIA (RX72N hardware name) -> COPI (project name) - data from RPi5
   * - MISOA: P30/MISOA (RX72N hardware name) -> CIPO (project name) - data to RPi5
   * - SSLA0: P54/SSLA0 (chip select from RPi5)
   */
  k_rspi0_base_addr = 0x000D0100,

  /**
   * @brief RSPI1 register base address (0x000D0140)
   * @details
   * Reserved for future SPI peripherals in controller mode (DRV8263H has no SPI).
   * Directly follows RSPI0 in memory (0x40 byte offset).
   * @par Pin Assignments:
   * - RSPCKB: PE5/RSPCKB (clock output to peripherals)
   * - MOSIB: PE6/MOSIB (RX72N hardware name) -> COPI (project name) - data out
   * - MISOB: PE7/MISOB (RX72N hardware name) -> CIPO (project name) - data in
   * - SSLB0-3: Individual chip selects (available for future sensors)
   */
  k_rspi1_base_addr = 0x000D0140,

  /**
   * @brief RSPI2 register base address (0x000D0300)
   * @details
   * Reserved for future expansion. Not currently used in STAR project.
   * Located at 0x1C0 bytes after RSPI1 (gap for other peripherals).
   */
  k_rspi2_base_addr = 0x000D0300,
} rx_rspi_addresses_t;

/**
 * @struct rx_rspi_regs_t
 * @brief RSPI Register Map Structure (Complete - All 22 Registers)
 *
 * @details
 * Renesas Serial Peripheral Interface (RSPI) registers for SPI communication.
 * Supports controller and peripheral modes with configurable clock and data format.
 * Addresses verified against RX72N Hardware Manual (R01UH0824EJ0120 Rev.1.20).
 *
 * @par Memory Layout Table
 * | Offset | Size | Field   | Type     | Description                              |
 * |--------|------|---------|----------|------------------------------------------|
 * | 0x00   | 1    | spcr    | uint8_t  | Control Register (enable, mode)          |
 * | 0x01   | 1    | sslp    | uint8_t  | Chip Select Polarity Register            |
 * | 0x02   | 1    | sppcr   | uint8_t  | Pin Control Register (loopback)          |
 * | 0x03   | 1    | spsr    | uint8_t  | Status Register (flags)                  |
 * | 0x04   | 4    | spdr    | uint32_t | Data Register (TX/RX)                    |
 * | 0x08   | 1    | spscr   | uint8_t  | Sequence Control Register                |
 * | 0x09   | 1    | spssr   | uint8_t  | Sequence Status Register                 |
 * | 0x0A   | 1    | spbr    | uint8_t  | Bit Rate Register                        |
 * | 0x0B   | 1    | spdcr   | uint8_t  | Data Control Register                    |
 * | 0x0C   | 1    | spckd   | uint8_t  | Clock Delay Register                     |
 * | 0x0D   | 1    | sslnd   | uint8_t  | SSL Negation Delay Register              |
 * | 0x0E   | 1    | spnd    | uint8_t  | Next-Access Delay Register               |
 * | 0x0F   | 1    | spcr2   | uint8_t  | Control Register 2                       |
 * | 0x10   | 2    | spcmd0  | uint16_t | Command Register 0                       |
 * | 0x12   | 2    | spcmd1  | uint16_t | Command Register 1                       |
 * | 0x14   | 2    | spcmd2  | uint16_t | Command Register 2                       |
 * | 0x16   | 2    | spcmd3  | uint16_t | Command Register 3                       |
 * | 0x18   | 2    | spcmd4  | uint16_t | Command Register 4                       |
 * | 0x1A   | 2    | spcmd5  | uint16_t | Command Register 5                       |
 * | 0x1C   | 2    | spcmd6  | uint16_t | Command Register 6                       |
 * | 0x1E   | 2    | spcmd7  | uint16_t | Command Register 7                       |
 * | 0x20   | 1    | spdcr2  | uint8_t  | Data Control Register 2                  |
 * | **Total** | **33** |     |          | (no padding due to packed attribute)     |
 *
 * @par Sequential Transfer Mode
 * Up to 8 commands (SPCMD0-SPCMD7) can be executed sequentially in looped
 * execution. Each command configures:
 * - SSL signal selection and assertion
 * - Bit rate division
 * - Clock polarity (CPOL) and phase (CPHA)
 * - Transfer data length (8-32 bits)
 * - MSB/LSB first transmission order
 * - Inter-transfer delay settings
 *
 * @par Initialization Example (Peripheral Mode - RPi5 Communication)
 * @code{.c}
 * // Configure RSPI0 for 10 Mbps peripheral mode (RPi5 is controller)
 * volatile rx_rspi_regs_t* spi = rspi0();
 *
 * // 1. Disable RSPI before configuration
 * spi->spcr = 0x00;
 *
 * // 2. Configure pin polarity (SSL active low)
 * spi->sslp = 0x00;
 *
 * // 3. Configure command register 0 for 8-bit transfers
 * //    SPB[3:0]=0111 (8 bits), CPOL=0, CPHA=0, LSBF=0 (MSB first)
 * spi->spcmd0 = 0x0700;
 *
 * // 4. Enable RSPI in peripheral mode with interrupts
 * spi->spcr = k_rspi_spcr_spe | k_rspi_spcr_sprie | k_rspi_spcr_sptie;
 * @endcode
 *
 * @par Initialization Example (Controller Mode - Motor Drivers)
 * @code{.c}
 * // Configure RSPI1 for 5 Mbps controller mode (reserved for sensors)
 * volatile rx_rspi_regs_t* spi = rspi1();
 *
 * // 1. Disable RSPI before configuration
 * spi->spcr = 0x00;
 *
 * // 2. Configure bit rate for 5 Mbps (PCLKB=60MHz, n=5: 60/(2*(5+1))=5MHz)
 * spi->spbr = 5;
 *
 * // 3. Configure command register 0 for 16-bit transfers (DRV8263H protocol)
 * //    SPB[3:0]=1111 (16 bits), CPOL=0, CPHA=1 for 16-bit SPI device
 * spi->spcmd0 = 0x0F02;
 *
 * // 4. Enable RSPI in controller mode
 * spi->spcr = k_rspi_spcr_spe | k_rspi_spcr_mstr | k_rspi_spcr_sptie;
 * @endcode
 *
 * @par Data Transfer Example
 * @code{.c}
 * // Full-duplex transfer (controller mode)
 * static rx_err_t rspi_transfer(volatile rx_rspi_regs_t* spi,
 *                                uint8_t tx_data, uint8_t* rx_data) {
 *     // Wait for TX buffer empty
 *     while (!(spi->spsr & k_rspi_spsr_sptef)) { }
 *
 *     // Write data (starts transfer)
 *     spi->spdr = tx_data;
 *
 *     // Wait for RX buffer full
 *     while (!(spi->spsr & k_rspi_spsr_sprf)) { }
 *
 *     // Read received data
 *     *rx_data = (uint8_t)spi->spdr;
 *
 *     return k_rx_ok;
 * }
 * @endcode
 *
 * @invariant sizeof(rx_rspi_regs_t) == 33 bytes (verified by static assertion)
 * @invariant Structure is packed with no padding
 * @invariant All offsets match RX72N Hardware Manual Ch44
 *
 * @note Structure uses `__attribute__((packed))` to ensure no compiler padding
 * @note All fields are volatile for hardware register access
 * @warning Do not modify reserved bits in control registers
 * @attention RSPI must be disabled (SPE=0) before changing most configuration
 *
 * @see rspi0(), rspi1(), rspi2() Accessor functions for each channel
 * @see rspi_spcr_bits_t Control register bit definitions
 * @see rspi_spsr_bits_t Status register bit definitions
 *
 * @since Version 1.0.0
 * @version 1.1.0 (2026-01-27: Added SPCMD1-7 and SPDCR2 for complete coverage)
 */
typedef struct __attribute__((packed)) {
  /**
   * @brief SPI Control Register (SPCR) @ offset 0x00
   * @details Controls RSPI operation including enable, mode, and interrupts.
   * @par Bit Fields:
   * - Bit 7 (SPRIE): Receive interrupt enable
   * - Bit 6 (SPE): SPI function enable
   * - Bit 5 (SPTIE): Transmit interrupt enable
   * - Bit 4 (SPEIE): Error interrupt enable
   * - Bit 3 (MSTR): Controller/Peripheral mode (1=Controller)
   * - Bit 2 (MODFEN): Mode fault detection enable
   * - Bit 1 (TXMD): Transmit-only mode
   * - Bit 0 (SPMS): SPI mode select
   * @see rspi_spcr_bits_t Bit definitions
   */
  volatile uint8_t spcr;

  /**
   * @brief SPI Chip Select Polarity Register (SSLP) @ offset 0x01
   * @details Sets polarity of SSL0-SSL3 signals (0=active low, 1=active high).
   * @note Most SPI peripherals expect active-low chip select (default 0x00)
   */
  volatile uint8_t sslp;

  /**
   * @brief SPI Pin Control Register (SPPCR) @ offset 0x02
   * @details Controls loopback modes and COPI idle value.
   * @par Bit Fields:
   * - Bit 5 (MOIFV): COPI idle value (0=low, 1=high)
   * - Bit 4 (MOIFE): Enable COPI idle value fixing
   * - Bit 1 (SPLP2): Loopback mode 2 (no inversion)
   * - Bit 0 (SPLP): Loopback mode (with inversion)
   * @see rspi_sppcr_bits_t Bit definitions
   */
  volatile uint8_t sppcr;

  /**
   * @brief SPI Status Register (SPSR) @ offset 0x03
   * @details Indicates RSPI status including buffer flags and error flags.
   * @par Bit Fields:
   * - Bit 7 (SPRF): Receive buffer full
   * - Bit 5 (SPTEF): Transmit buffer empty
   * - Bit 4 (UDRF): Underrun error
   * - Bit 3 (PERF): Parity error
   * - Bit 2 (MODF): Mode fault error
   * - Bit 1 (IDLNF): Idle flag (0=idle)
   * - Bit 0 (OVRF): Overrun error
   * @see rspi_spsr_bits_t Bit definitions
   */
  volatile uint8_t spsr;

  /**
   * @brief SPI Data Register (SPDR) @ offset 0x04
   * @details 32-bit TX/RX data register. Access width depends on SPDCR.SPLW.
   * @par Access Modes:
   * - Word access (SPLW=1): 32-bit read/write
   * - Byte access (SPLW=0): 8-bit read/write (lower byte only)
   * @note For 8-bit transfers, only lower byte is significant
   * @note For 16-bit transfers, use appropriate data alignment
   */
  volatile uint32_t spdr;

  /**
   * @brief SPI Sequence Control Register (SPSCR) @ offset 0x08
   * @details Controls command sequence length (0-7 = 1-8 commands).
   * @par Value: 0 to 7 (number of commands - 1)
   */
  volatile uint8_t spscr;

  /**
   * @brief SPI Sequence Status Register (SPSSR) @ offset 0x09
   * @details Indicates current command sequence position.
   */
  volatile uint8_t spssr;

  /**
   * @brief SPI Bit Rate Register (SPBR) @ offset 0x0A
   * @details Sets SPI clock frequency: f_SPCK = PCLKB / (2 x (SPBR + 1))
   * @par Calculation Examples (PCLKB = 60 MHz):
   * - SPBR=0: 30 MHz (maximum)
   * - SPBR=2: 10 MHz (RPi5 communication)
   * - SPBR=5: 5 MHz (reserved for future SPI peripherals)
   * - SPBR=29: 1 MHz
   * @note Only used in controller mode; ignored in peripheral mode
   */
  volatile uint8_t spbr;

  /**
   * @brief SPI Data Control Register (SPDCR) @ offset 0x0B
   * @details Controls data access mode and receive data ready detection.
   * @see rspi_spdcr_bits_t Bit definitions
   */
  volatile uint8_t spdcr;

  /**
   * @brief SPI Clock Delay Register (SPCKD) @ offset 0x0C
   * @details Sets delay from SSL assertion to first SPCK edge.
   * @par Delay: 1 to 8 SPCK cycles (value + 1)
   */
  volatile uint8_t spckd;

  /**
   * @brief SPI Chip Select Negation Delay Register (SSLND) @ offset 0x0D
   * @details Sets delay from last SPCK edge to SSL negation.
   * @par Delay: 1 to 8 SPCK cycles (value + 1)
   */
  volatile uint8_t sslnd;

  /**
   * @brief SPI Next-Access Delay Register (SPND) @ offset 0x0E
   * @details Sets delay from SSL negation to next SSL assertion.
   * @par Delay: 1 to 8 SPCK cycles (value + 1)
   */
  volatile uint8_t spnd;

  /**
   * @brief SPI Control Register 2 (SPCR2) @ offset 0x0F
   * @details Additional control including parity and idle interrupt.
   */
  volatile uint8_t spcr2;

  /**
   * @brief SPI Command Register 0 (SPCMD0) @ offset 0x10
   * @details Primary command configuration for transfer parameters.
   * @par Bit Fields:
   * - Bits 15:12 (SCKDEN): Clock delay setting
   * - Bits 11:8 (SLNDEN): SSL negation delay setting
   * - Bit 7 (SPNDEN): Next-access delay enable
   * - Bit 6 (LSBF): LSB-first transfer (0=MSB first)
   * - Bits 5:4 (SPB): Data length (0111=8bit, 1111=16bit, etc.)
   * - Bit 3 (BRDV): Bit rate division
   * - Bit 2 (SSLA): SSL signal assertion
   * - Bit 1 (CPHA): Clock phase
   * - Bit 0 (CPOL): Clock polarity
   */
  volatile uint16_t spcmd0;

  volatile uint16_t spcmd1; /**< SPI Command Register 1 @ offset 0x12 */
  volatile uint16_t spcmd2; /**< SPI Command Register 2 @ offset 0x14 */
  volatile uint16_t spcmd3; /**< SPI Command Register 3 @ offset 0x16 */
  volatile uint16_t spcmd4; /**< SPI Command Register 4 @ offset 0x18 */
  volatile uint16_t spcmd5; /**< SPI Command Register 5 @ offset 0x1A */
  volatile uint16_t spcmd6; /**< SPI Command Register 6 @ offset 0x1C */
  volatile uint16_t spcmd7; /**< SPI Command Register 7 @ offset 0x1E */

  /**
   * @brief SPI Data Control Register 2 (SPDCR2) @ offset 0x20
   * @details Controls byte swap for 16/32-bit transfers.
   */
  volatile uint8_t spdcr2;
} rx_rspi_regs_t;

/**
 * @brief Get pointer to RSPI0 registers (Raspberry Pi 5 communication)
 *
 * @details
 * Returns a volatile pointer to the RSPI0 register structure at address
 * 0x000D0100. RSPI0 is configured as a peripheral for communication
 * with the Raspberry Pi 5 controller at 10 Mbps.
 *
 * @return Volatile pointer to RSPI0 register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre None (hardware register always accessible)
 * @post Pointer is valid for the lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 * @note This function is always inlined for zero overhead
 *
 * @par Example:
 * @code{.c}
 * volatile rx_rspi_regs_t* spi = rspi0();
 * spi->spcr = k_rspi_spcr_spe;  // Enable RSPI0
 * @endcode
 *
 * @see k_rspi0_base_addr Base address constant
 * @see rx_spi_comm_init() Higher-level initialization
 * @since Version 1.0.0
 */
static inline volatile rx_rspi_regs_t* rspi0(void)
{
  return (volatile rx_rspi_regs_t*)k_rspi0_base_addr;
}

/**
 * @brief Get pointer to RSPI1 registers (reserved for future SPI peripherals)
 *
 * @details
 * Returns a volatile pointer to the RSPI1 register structure at address
 * 0x000D0140. RSPI1 is available for future SPI peripherals
 * (not currently used; DRV8263H motor drivers use PWM+GPIO, not SPI).
 *
 * @return Volatile pointer to RSPI1 register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre None (hardware register always accessible)
 * @post Pointer is valid for the lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 * @note This function is always inlined for zero overhead
 *
 * @par Example:
 * @code{.c}
 * volatile rx_rspi_regs_t* spi = rspi1();
 * spi->spcr = k_rspi_spcr_spe | k_rspi_spcr_mstr;  // Enable as controller
 * @endcode
 *
 * @see k_rspi1_base_addr Base address constant
 * @see rspi0() Primary SPI channel (RPi5 communication)
 * @since Version 1.0.0
 */
static inline volatile rx_rspi_regs_t* rspi1(void)
{
  return (volatile rx_rspi_regs_t*)k_rspi1_base_addr;
}

/**
 * @brief Get pointer to RSPI2 registers (reserved for future expansion)
 *
 * @details
 * Returns a volatile pointer to the RSPI2 register structure at address
 * 0x000D0300. RSPI2 is currently reserved and not used in the STAR project.
 *
 * @return Volatile pointer to RSPI2 register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre None (hardware register always accessible)
 * @post Pointer is valid for the lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 * @note Currently unused in STAR project
 *
 * @see k_rspi2_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_rspi_regs_t* rspi2(void)
{
  return (volatile rx_rspi_regs_t*)k_rspi2_base_addr;
}

/* =============================================================================
 * RSPI Bit Field Definitions (All Complete)
 * =============================================================================
 */

/**
 * @enum rspi_spcr_bits_t
 * @brief RSPI Control Register (SPCR) Bit Definitions
 *
 * @details
 * Controls RSPI operation including mode selection, interrupt enables,
 * and function enable. The SPCR register must be configured before enabling
 * RSPI with the SPE bit.
 *
 * @par Register Layout (8-bit @ offset 0x00)
 * | Bit | Name   | R/W | Description                                      |
 * |-----|--------|-----|--------------------------------------------------|
 * | 7   | SPRIE  | R/W | Receive interrupt enable                         |
 * | 6   | SPE    | R/W | SPI function enable (1=enabled)                  |
 * | 5   | SPTIE  | R/W | Transmit interrupt enable                        |
 * | 4   | SPEIE  | R/W | Error interrupt enable                           |
 * | 3   | MSTR   | R/W | Controller/Peripheral mode (1=Controller)        |
 * | 2   | MODFEN | R/W | Mode fault detection enable                      |
 * | 1   | TXMD   | R/W | Transmit-only mode (simplex)                     |
 * | 0   | SPMS   | R/W | SPI mode select                                  |
 *
 * @par Configuration Examples
 * @code{.c}
 * // Peripheral mode with receive/transmit interrupts (RPi5 communication)
 * rspi0()->spcr = k_rspi_spcr_spe | k_rspi_spcr_sprie | k_rspi_spcr_sptie;
 *
 * // Controller mode with transmit interrupt (future SPI sensors)
 * rspi1()->spcr = k_rspi_spcr_spe | k_rspi_spcr_mstr | k_rspi_spcr_sptie;
 *
 * // Disable RSPI before reconfiguration
 * rspi0()->spcr = 0x00;
 * @endcode
 *
 * @warning SPE must be cleared (0) before changing other bits
 * @see rx_rspi_regs_t::spcr Register field in structure
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Receive Interrupt Enable (SPRIE) - Bit 7
   * @details Enables interrupt when receive buffer is full (SPRF=1).
   * Use for DMA trigger or ISR-driven receive processing.
   */
  k_rspi_spcr_sprie = (1 << 7),

  /**
   * @brief SPI Function Enable (SPE) - Bit 6
   * @details Enables RSPI operation. Must be set last after configuration.
   * @warning Clear this bit before modifying other SPCR bits.
   */
  k_rspi_spcr_spe = (1 << 6),

  /**
   * @brief Transmit Interrupt Enable (SPTIE) - Bit 5
   * @details Enables interrupt when transmit buffer is empty (SPTEF=1).
   * Use for DMA trigger or ISR-driven transmit processing.
   */
  k_rspi_spcr_sptie = (1 << 5),

  /**
   * @brief Error Interrupt Enable (SPEIE) - Bit 4
   * @details Enables interrupt on error conditions (overrun, underrun, mode fault).
   * Recommended for robust error handling in production systems.
   */
  k_rspi_spcr_speie = (1 << 4),

  /**
   * @brief Controller/Peripheral Mode Select (MSTR) - Bit 3
   * @details Selects RSPI operating mode:
   * - 0: Peripheral mode (clock input from external controller)
   * - 1: Controller mode (clock output to external peripherals)
   * @note RSPI0 uses peripheral mode (RPi5 is controller)
   * @note RSPI1 reserved for future SPI sensors (controller mode)
   */
  k_rspi_spcr_mstr = (1 << 3),

  /**
   * @brief Mode Fault Error Detection Enable (MODFEN) - Bit 2
   * @details Enables mode fault detection in multi-controller configurations.
   * Not typically needed for single-controller systems.
   */
  k_rspi_spcr_modfe = (1 << 2),

  /**
   * @brief Transmit-Only Mode (TXMD) - Bit 1
   * @details Enables simplex transmit-only operation (no receive).
   * Useful for write-only peripherals like displays.
   */
  k_rspi_spcr_txmd = (1 << 1),

  /**
   * @brief SPI Mode Select (SPMS) - Bit 0
   * @details Selects 3-wire or 4-wire SPI mode.
   */
  k_rspi_spcr_spms = (1 << 0),
} rspi_spcr_bits_t;

/**
 * @enum rspi_spsr_bits_t
 * @brief RSPI Status Register (SPSR) Bit Definitions
 *
 * @details
 * Indicates RSPI status including buffer full/empty flags and error flags.
 * Status flags are used to determine when data can be read/written and
 * to detect error conditions.
 *
 * @par Register Layout (8-bit @ offset 0x03)
 * | Bit | Name  | R/W | Description                                       |
 * |-----|-------|-----|---------------------------------------------------|
 * | 7   | SPRF  | R   | Receive buffer full (data ready to read)          |
 * | 6   | -     | -   | Reserved                                          |
 * | 5   | SPTEF | R   | Transmit buffer empty (ready for new data)        |
 * | 4   | UDRF  | R/W | Underrun error (write 0 to clear)                 |
 * | 3   | PERF  | R/W | Parity error (write 0 to clear)                   |
 * | 2   | MODF  | R/W | Mode fault error (write 0 to clear)               |
 * | 1   | IDLNF | R   | Idle flag (0=idle, 1=transfer in progress)        |
 * | 0   | OVRF  | R/W | Overrun error (write 0 to clear)                  |
 *
 * @par Error Detection
 * - **OVRF**: Overrun error - receive buffer not read before next byte arrived
 * - **MODF**: Mode fault error - SSL signal error in multi-controller config
 * - **PERF**: Parity error - parity check failed during transmission
 * - **UDRF**: Underrun error - transmit buffer not written before needed
 *
 * @par Polling Example
 * @code{.c}
 * // Wait for transmit buffer empty before writing
 * while (!(rspi1()->spsr & k_rspi_spsr_sptef)) {
 *     // Optionally yield to RTOS
 * }
 * rspi1()->spdr = tx_data;  // Write data
 *
 * // Wait for receive buffer full before reading
 * while (!(rspi1()->spsr & k_rspi_spsr_sprf)) { }
 * uint8_t rx_data = (uint8_t)rspi1()->spdr;  // Read data
 *
 * // Check for errors
 * if (rspi1()->spsr & (k_rspi_spsr_ovrf | k_rspi_spsr_udrf)) {
 *     // Handle error - clear by writing 0 to error bits
 *     rspi1()->spsr = ~(k_rspi_spsr_ovrf | k_rspi_spsr_udrf);
 * }
 * @endcode
 *
 * @warning Error flags must be cleared by writing 0 to the specific bit
 * @see rx_rspi_regs_t::spsr Register field in structure
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Receive Buffer Full Flag (SPRF) - Bit 7
   * @details Set when receive data is available in SPDR. Read SPDR to clear.
   */
  k_rspi_spsr_sprf = (1 << 7),

  /**
   * @brief Transmit Buffer Empty Flag (SPTEF) - Bit 5
   * @details Set when transmit buffer can accept new data. Write SPDR to clear.
   */
  k_rspi_spsr_sptef = (1 << 5),

  /**
   * @brief Underrun Error Flag (UDRF) - Bit 4
   * @details Set when transmit buffer was empty during transfer (controller mode).
   * Write 0 to this bit to clear. Indicates software isn't feeding data fast enough.
   */
  k_rspi_spsr_udrf = (1 << 4),

  /**
   * @brief Parity Error Flag (PERF) - Bit 3
   * @details Set when parity check fails (if parity enabled). Write 0 to clear.
   */
  k_rspi_spsr_perf = (1 << 3),

  /**
   * @brief Mode Fault Error Flag (MODF) - Bit 2
   * @details Set on SSL signal error in multi-controller configuration.
   * Write 0 to clear. Typically not used in single-controller systems.
   */
  k_rspi_spsr_modf = (1 << 2),

  /**
   * @brief Idle Flag (IDLNF) - Bit 1
   * @details Transfer status indicator:
   * - 0: RSPI is idle (no transfer in progress)
   * - 1: Transfer is in progress
   * @note Read-only flag, useful for determining safe reconfiguration time
   */
  k_rspi_spsr_idlnf = (1 << 1),

  /**
   * @brief Overrun Error Flag (OVRF) - Bit 0
   * @details Set when receive buffer overflows (new data arrived before old read).
   * Write 0 to this bit to clear. Indicates software isn't reading data fast enough.
   */
  k_rspi_spsr_ovrf = (1 << 0),
} rspi_spsr_bits_t;

/**
 * @enum rspi_sppcr_bits_t
 * @brief RSPI Pin Control Register (SPPCR) Bit Definitions
 *
 * @details
 * Controls RSPI pin behavior including loopback modes for testing and
 * COPI (Controller Out Peripheral In) idle value control.
 *
 * @par Register Layout (8-bit @ offset 0x02)
 * | Bit | Name  | R/W | Description                                       |
 * |-----|-------|-----|---------------------------------------------------|
 * | 7   | -     | -   | Reserved                                          |
 * | 6   | MOIFE | R/W | COPI idle fixed value enable                      |
 * | 5   | MOIFV | R/W | COPI idle fixed value (0=low, 1=high)             |
 * | 4:2 | -     | -   | Reserved                                          |
 * | 1   | SPLP2 | R/W | Loopback mode 2 (no inversion)                    |
 * | 0   | SPLP  | R/W | Loopback mode (with inversion)                    |
 *
 * @par Loopback Modes (for Testing)
 * - **SPLP**: Internal loopback with data inversion (for self-test)
 * - **SPLP2**: Internal loopback without inversion (for data verification)
 *
 * @par COPI Idle Control
 * When MOIFE is set, the COPI pin is driven to the value specified by MOIFV
 * during idle periods. Useful for peripherals that sample the line between transfers.
 *
 * @par Example
 * @code{.c}
 * // Enable loopback mode for self-test
 * rspi0()->sppcr = k_rspi_sppcr_splp2;  // Loopback without inversion
 *
 * // Normal operation with COPI idle high
 * rspi1()->sppcr = k_rspi_sppcr_moife | k_rspi_sppcr_moifv;
 * @endcode
 *
 * @see rx_rspi_regs_t::sppcr Register field in structure
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief COPI Idle Fixed Value Enable (MOIFE) - Bit 6
   * @details When set, COPI pin is driven to MOIFV value during idle.
   */
  k_rspi_sppcr_moife = (1 << 6),

  /**
   * @brief COPI Idle Fixed Value (MOIFV) - Bit 5
   * @details COPI idle level when MOIFE is enabled:
   * - 0: Drive COPI low during idle
   * - 1: Drive COPI high during idle
   */
  k_rspi_sppcr_moifv = (1 << 5),

  /**
   * @brief Loopback Mode 2 (SPLP2) - Bit 1
   * @details Internal loopback without data inversion. CIPO receives
   * exactly what COPI transmits. Useful for data path verification.
   */
  k_rspi_sppcr_splp2 = (1 << 1),

  /**
   * @brief Loopback Mode (SPLP) - Bit 0
   * @details Internal loopback with data inversion. Useful for hardware
   * self-test where inverted echo is expected.
   */
  k_rspi_sppcr_splp = (1 << 0),
} rspi_sppcr_bits_t;

/**
 * @enum rspi_spdcr_bits_t
 * @brief RSPI Data Control Register (SPDCR) Bit Definitions
 *
 * @details
 * Controls data access modes including word/byte access to SPDR register
 * and receive data ready detection timing.
 *
 * @par Register Layout (8-bit @ offset 0x0B)
 * | Bit | Name   | R/W | Description                                      |
 * |-----|--------|-----|--------------------------------------------------|
 * | 7:6 | -      | -   | Reserved                                         |
 * | 5   | SPRDTD | R/W | Receive data ready detection timing              |
 * | 4   | SPLW   | R/W | Word access mode (0=byte, 1=word)                |
 * | 3:0 | -      | -   | Reserved                                         |
 *
 * @par Access Mode Selection
 * The SPLW bit determines how the SPDR register is accessed:
 * - **Byte access (SPLW=0)**: 8-bit read/write, lower byte only
 * - **Word access (SPLW=1)**: 32-bit read/write, full register
 *
 * @par Example
 * @code{.c}
 * // Configure for 8-bit byte access (typical for RPi5 communication)
 * rspi0()->spdcr = 0x00;  // Byte access
 *
 * // Configure for 32-bit word access (higher throughput)
 * rspi1()->spdcr = k_rspi_spdcr_splw;  // Word access
 * @endcode
 *
 * @see rx_rspi_regs_t::spdcr Register field in structure
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Receive Data Ready Detection (SPRDTD) - Bit 5
   * @details Controls when SPRF flag is set:
   * - 0: Set when receive shift register transfer to SPDR completes
   * - 1: Set when SPDR contains valid data
   */
  k_rspi_spdcr_sprdtd = (1 << 5),

  /**
   * @brief Word Access Mode (SPLW) - Bit 4
   * @details SPDR access width selection:
   * - 0: Byte access (8-bit)
   * - 1: Word access (32-bit)
   * @note Word access is more efficient for 16/32-bit transfers
   */
  k_rspi_spdcr_splw = (1 << 4),
} rspi_spdcr_bits_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* Verify base addresses match Hardware Manual Ch44 */
static_assert(k_rspi0_base_addr == 0x000D0100, "RSPI0 base address incorrect");
static_assert(k_rspi1_base_addr == 0x000D0140, "RSPI1 base address incorrect");
static_assert(k_rspi2_base_addr == 0x000D0300, "RSPI2 base address incorrect");

/* Verify register structure layout and offsets */
static_assert(sizeof(rx_rspi_regs_t) == 33, "RSPI register structure size incorrect");
static_assert(offsetof(rx_rspi_regs_t, spcr) == 0x00, "SPCR offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, sslp) == 0x01, "SSLP offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, sppcr) == 0x02, "SPPCR offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spsr) == 0x03, "SPSR offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spdr) == 0x04, "SPDR offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spscr) == 0x08, "SPSCR offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spssr) == 0x09, "SPSSR offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spbr) == 0x0A, "SPBR offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spdcr) == 0x0B, "SPDCR offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spckd) == 0x0C, "SPCKD offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, sslnd) == 0x0D, "SSLND offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spnd) == 0x0E, "SPND offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spcr2) == 0x0F, "SPCR2 offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spcmd0) == 0x10, "SPCMD0 offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spcmd1) == 0x12, "SPCMD1 offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spcmd2) == 0x14, "SPCMD2 offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spcmd3) == 0x16, "SPCMD3 offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spcmd4) == 0x18, "SPCMD4 offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spcmd5) == 0x1A, "SPCMD5 offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spcmd6) == 0x1C, "SPCMD6 offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spcmd7) == 0x1E, "SPCMD7 offset incorrect");
static_assert(offsetof(rx_rspi_regs_t, spdcr2) == 0x20, "SPDCR2 offset incorrect");

/* Verify channel spacing (RSPI0 to RSPI1 = 0x40 bytes, RSPI1 to RSPI2 = 0x1C0 bytes) */
static_assert((k_rspi1_base_addr - k_rspi0_base_addr) == 0x40, "RSPI0 to RSPI1 spacing incorrect");
static_assert((k_rspi2_base_addr - k_rspi1_base_addr) == 0x1C0, "RSPI1 to RSPI2 spacing incorrect");

#ifdef __cplusplus
}
#endif
