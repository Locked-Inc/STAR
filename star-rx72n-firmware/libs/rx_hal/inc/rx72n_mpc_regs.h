/**
 * @file rx72n_mpc_regs.h
 * @brief RX72N Multi-Function Pin Controller (MPC) Register Definitions
 *
 * @details
 * Register definitions for the Multi-Function Pin Controller (MPC) peripheral
 * on the RX72N microcontroller. MPC configures which peripheral function is
 * connected to each GPIO pin (pin multiplexing).
 *
 * @par MPC System Architecture
 * @verbatim
 *                    RX72N Multi-Function Pin Controller Architecture
 *   +-----------------------------------------------------------------------+
 *   |                         MPC Module (0x0008C100)                       |
 *   |                                                                       |
 *   |  +------------+     +------------+     +----------------------------+ |
 *   |  |   PWPR     |     | Bus Ctrl   |     |    PFS Registers           | |
 *   |  | (Protect)  |     | (External) |     |    (Pin Function)          | |
 *   |  +------+-----+     +------------+     +------------+---------------+ |
 *   |         |                                           |                 |
 *   |         | Unlock                                    | PSEL[4:0]       |
 *   |         v                                           v                 |
 *   +---------+-------------------------------------------+-----------------+
 *             |                                           |
 *             |                                           v
 *   +---------+--------------------------------------------------------------+
 *   |                        Pin Multiplexer Matrix                         |
 *   |  +-----------------------------------------------------------------+  |
 *   |  |   Physical Pin  <----- MUX <--- [GPIO, SCI, SPI, I2C, MTU, ...]  |  |
 *   |  +-----------------------------------------------------------------+  |
 *   +------------------------------------------------------------------------+
 *
 *   PFS Write Protection Sequence:
 *   +--------+    +---------+    +---------+    +--------+    +--------+
 *   | PWPR   | ->  | PWPR    | ->  | Write   | ->  | PWPR   | ->  | PWPR   |
 *   | =0x00  |    | =0x40   |    | PnmPFS  |    | =0x00  |    | =0x80  |
 *   |(B0WI=0)|    |(PFSWE=1)|    |         |    |(PFSWE=0|    |(B0WI=1)|
 *   +--------+    +---------+    +---------+    +--------+    +--------+
 *
 *   Memory Map (Verified against RX72N Manual Ch23):
 *   +------------+----------------------------------------------+
 *   |  Address   |  Register                                    |
 *   +------------+----------------------------------------------+
 *   | 0x0008C100 |  PFCSE (CS Output Enable)                    |
 *   | 0x0008C102 |  PFCSS0/1 (CS Output Pin Select)             |
 *   | 0x0008C104 |  PFAOE0/1 (Address Output Enable)            |
 *   | 0x0008C106 |  PFBCR0-3 (External Bus Control)             |
 *   | 0x0008C10E |  PFENET (Ethernet Control)                   |
 *   | 0x0008C11F |  PWPR (Write Protect Register)               |
 *   | 0x0008C128 |  DSCR2[24] (Drive Capacity Control)          |
 *   | 0x0008C140 |  P00PFS - PJ5PFS (Pin Function Select)       |
 *   +------------+----------------------------------------------+
 *
 *   STAR Project Pin Assignments (144-pin LFQFP):
 *   +----------+---------+------+--------------------------------+
 *   |  Pin     |  Port   | PSEL |  Function                      |
 *   +----------+---------+------+--------------------------------+
 *   | USB D+/- | (USB)   |  -   |  USB CDC Debug                 |
 *   | Pin 72   | PB7     | 0x0A |  SCI9-TXD (Debug UART TX)      |
 *   | Pin 74   | PB6     | 0x0A |  SCI9-RXD (Debug UART RX)      |
 *   | Pin 32   | PA0     | 0x0D |  RSPI0-MOSIA (SPI COPI)        |
 *   | Pin 33   | PA1     | 0x0D |  RSPI0-MISOA (SPI CIPO)        |
 *   | Pin 35   | PA3     | 0x0D |  RSPI0-RSPCKA (SPI CLK)        |
 *   | Pin 34   | PA4     | 0x0D |  RSPI0-SSLA0-B (SPI CS)        |
 *   | Pin 97   | PE5     | 0x1E |  GTIOC0A (Motor 0 PWM+)        |
 *   | Pin 96   | PE4     | 0x1E |  GTIOC0B (Motor 0 PWM-)        |
 *   | Pin 60   | P24     | 0x02 |  MTCLKA (Encoder 0 Phase A)    |
 *   | Pin 61   | P25     | 0x02 |  MTCLKB (Encoder 0 Phase B)    |
 *   +----------+---------+------+--------------------------------+
 * @endverbatim
 *
 * @par PFS Register Bit Layout
 * Each pin has a Pin Function Select (PFS) register:
 * @verbatim
 *   Bit 7   | Bit 6   | Bit 5    | Bits 4-0
 *   --------+---------+----------+------------------
 *   ASEL    | ISEL    | Reserved | PSEL[4:0]
 *   (Analog)| (IRQ)   |          | (Peripheral Select)
 * @endverbatim
 *
 * @par Common PSEL Values (varies by pin, check RX72N Manual Appendix)
 * | PSEL | Function Examples |
 * |------|-------------------|
 * | 0x00 | Hi-Z / General Purpose I/O |
 * | 0x02 | MTU3a clock inputs (MTCLKA-D) |
 * | 0x1E | GPTW outputs (GTIOCnA/B) |
 * | 0x0A | SCI (TXD, RXD, SCK, CTS) |
 * | 0x0D | RSPI (COPI, CIPO, RSPCK, SSL) |
 * | 0x0F | RIIC (SDA, SCL) |
 *
 * @par Example: Configure PB7 as SCI9 TXD
 * @code
 * #include "rx72n_mpc_regs.h"
 *
 * // Unlock PFS write protection
 * mpc()->pwpr = 0x00;              // Clear B0WI bit
 * mpc()->pwpr = k_mpc_pwpr_pfswe;  // Set PFSWE to enable PFS writes
 *
 * // Configure PB7 for SCI9 TXD (PSEL = 0x0A)
 * mpc()->pb7pfs = 0x0A;
 *
 * // Re-lock PFS write protection
 * mpc()->pwpr = 0x00;              // Clear PFSWE
 * mpc()->pwpr = k_mpc_pwpr_b0wi;   // Set B0WI to protect PFSWE
 *
 * // Don't forget to also enable peripheral mode in PORT PMR register!
 * @endcode
 *
 * @par NASA Power of 10 Compliance
 * - Rule 3: No dynamic memory - all register access via static inline functions
 * - Rule 5: Compile-time assertions verify all register layout assumptions
 * - Rule 8: Uses typed enums (C23) for all constants, no magic numbers
 * - Rule 10: Clean build with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - Single Responsibility: MPC register definitions only, no driver logic
 * - Open/Closed: Accessors work with any pin configuration
 * - Interface Segregation: Minimal API - just register access functions
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware, Chapter 23 (Multi-Function Pin Controller)
 *
 * @par Verification Status
 * [PASS] VERIFIED (2026-01-28) - All register addresses and offsets verified
 * against RX72N Manual Chapter 23 and Appendix pin function tables.
 *
 * @see rx72n_port_regs.h GPIO port direction and data registers
 * @see rx_mpc.h Higher-level MPC driver with PSEL constants
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
 * @defgroup mpc_regs MPC Register Definitions
 * @brief Multi-Function Pin Controller (MPC) hardware register interface
 * @{
 */

/* =============================================================================
 * Multi-Function Pin Controller (MPC)
 * =============================================================================
 */

/**
 * @enum rx_mpc_addresses_t
 * @brief MPC base address (verified against RX72N Hardware Manual Ch23)
 *
 * @details
 * The MPC module is located at a single base address. All registers are
 * accessed relative to this base.
 */
typedef enum : uintptr_t {
  k_mpc_base_addr = 0x0008C100, /**< MPC register base address */
} rx_mpc_addresses_t;

/**
 * @enum mpc_register_offsets_t
 * @brief Byte offsets of key MPC registers from the MPC base address (0x0008C100)
 *
 * @details
 * These constants are used in static_assert calls to verify the struct layout
 * matches the hardware register map. Each constant encodes the exact byte offset
 * from either the MPC base or the PFS block base (P00PFS), as noted.
 *
 * @note All values verified against RX72N Manual Chapter 23, Section 23.2
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mpc_offset_pwpr      = 0x1F, /**< PWPR offset from MPC base (0x0008C11F) */
  k_mpc_offset_pfs       = 0x40, /**< PFS block start offset from MPC base (P00PFS = 0x0008C140) */
  k_mpc_offset_port5_pfs = 0x28, /**< Port 5 PFS block offset from P00PFS */
  k_mpc_offset_porta_pfs = 0x50, /**< Port A PFS block offset from P00PFS */
  k_mpc_offset_porte_pfs = 0x70, /**< Port E PFS block offset from P00PFS */
  k_mpc_offset_portj_pfs = 0x90, /**< Port J PFS block offset from P00PFS */
  k_mpc_pfs_total_bytes  = 152,  /**< Total size in bytes of the PFS register block (P00PFS through PJ5PFS with reserved padding) */
} mpc_register_offsets_t;

/** @brief MPC register reserved field sizes */
typedef enum : uint8_t {
  k_mpc_reserved1_bytes = 1,  /**< Reserved after PFCSE (0x0008C101) */
  k_mpc_reserved2_bytes = 4,  /**< Reserved after PFBCR3 (0x0008C10A-0x0008C10D) */
  k_mpc_reserved3_bytes = 16, /**< Reserved after PFENET (0x0008C10F-0x0008C11E) */
  k_mpc_reserved4_bytes = 8,  /**< Reserved after PWPR (0x0008C120-0x0008C127) */
  k_mpc_dscr2_count     = 24, /**< DSCR2 registers count (0x0008C128-0x0008C13F) */

  /* Port reserved byte counts (for ports with <8 pins) */
  k_port3_reserved_bytes  = 3, /**< Port 3: P35-P37 (P35 is NMI-only) */
  k_port5_reserved_bytes  = 1, /**< Port 5: P53 (dedicated BCLK) */
  k_port6_reserved_bytes  = 1, /**< Port 6: P65 (dedicated CKE) */
  k_port7_reserved_bytes  = 1, /**< Port 7: P70 (does not exist) */
  k_portf_reserved1_bytes = 2, /**< Port F: PF3-PF4 */
  k_portf_reserved2_bytes = 2, /**< Port F: PF6-PF7 */
  k_portj_reserved1_bytes = 1, /**< Port J: PJ4 */
  k_portj_reserved2_bytes = 2, /**< Port J: PJ6-PJ7 */
} mpc_reserved_sizes_t;

/**
 * @brief Pin Function Select Register (C bitfield)
 * @details
 * Each pin has a PFS register controlling peripheral function selection.
 *
 * This struct uses C bitfields (the `: N` syntax) to pack multiple fields
 * into a single byte, matching the hardware register layout exactly:
 *
 * Bit layout in hardware register (1 byte):
 * ```
 * 7       6       5   4   3   2   1   0
 * +-----------------------------------------+
 * | ASEL | ISEL | Reserved | PSEL[4:0]    |
 * +-----------------------------------------+
 * ```
 *
 * The syntax `volatile uint8_t name : bits` defines a bitfield where:
 * - `name` is the field name (or omitted for padding/reserved bits)
 * - `bits` is the number of bits the field occupies
 *
 * Example: `psel : 5` means psel occupies 5 bits (bits 0-4)
 *
 * This is standard C (not C++) and commonly used in embedded systems
 * for hardware register definitions where bits are packed together.
 */
typedef struct {
  volatile uint8_t psel : 5; /**< Peripheral Select (bits 0-4) */
  uint8_t : 1;               /**< Reserved bit (bit 5) - unnamed bitfield for padding */
  volatile uint8_t isel : 1; /**< Interrupt Input Select (bit 6) */
  volatile uint8_t asel : 1; /**< Analog Input Select (bit 7) */
} rx_pfs_regs_t;

/**
 * @brief MPC Register Map
 * @details
 * Multi-Function Pin Controller (MPC) registers for pin function selection.
 * Controls which peripheral function each GPIO pin is assigned to.
 * Base address: 0x0008C100
 *
 * CRITICAL: This struct matches the exact hardware memory layout!
 * - 0x0008C100: Bus control registers (PFCSE, PFCSS, PFAOE, PFBCR, PFENET)
 * - 0x0008C11F: PWPR (Write Protect Register)
 * - 0x0008C140: PFS registers start (P00PFS)
 *
 * Package Support (144-pin LFQFP R5F572NNHxFB):
 * - Available ports: 0-9 (partial), A-E (full), F (partial), J (partial: PJ3, PJ5)
 * - Not available on 144-pin: G, H
 * - Note: All ports are defined for compatibility with larger packages
 *
 * Complete Memory Layout:
 * - 0x0008C100: PFCSE (CS Output Enable Register)
 * - 0x0008C101: Reserved (1 byte)
 * - 0x0008C102-0x0008C103: PFCSS0, PFCSS1 (CS Output Pin Select)
 * - 0x0008C104-0x0008C105: PFAOE0, PFAOE1 (Address Output Enable)
 * - 0x0008C106-0x0008C109: PFBCR0-3 (External Bus Control)
 * - 0x0008C10A-0x0008C10D: Reserved (4 bytes)
 * - 0x0008C10E: PFENET (Ethernet Control)
 * - 0x0008C10F-0x0008C11E: Reserved (16 bytes)
 * - 0x0008C11F: PWPR (Write Protect Register)
 * - 0x0008C120-0x0008C127: Reserved (8 bytes)
 * - 0x0008C128-0x0008C13F: DSCR2 (Drive Capacity Control, 24 bytes)
 * - 0x0008C140+: PFS registers (P00PFS through PJPFS, 152 bytes)
 */
typedef struct {
  /* Bus Control Registers (0x0008C100 - 0x0008C10E) */
  volatile uint8_t pfcse;                            /**< +0x00: CS Output Enable Register */
  uint8_t          reserved1[k_mpc_reserved1_bytes]; /**< +0x01: Reserved */
  volatile uint8_t pfcss0;                           /**< +0x02: CS Output Pin Select 0 */
  volatile uint8_t pfcss1;                           /**< +0x03: CS Output Pin Select 1 */
  volatile uint8_t pfaoe0;                           /**< +0x04: Address Output Enable 0 */
  volatile uint8_t pfaoe1;                           /**< +0x05: Address Output Enable 1 */
  volatile uint8_t pfbcr0;                           /**< +0x06: External Bus Control 0 */
  volatile uint8_t pfbcr1;                           /**< +0x07: External Bus Control 1 */
  volatile uint8_t pfbcr2;                           /**< +0x08: External Bus Control 2 */
  volatile uint8_t pfbcr3;                           /**< +0x09: External Bus Control 3 */
  uint8_t          reserved2[k_mpc_reserved2_bytes]; /**< +0x0A-0x0D: Reserved */
  volatile uint8_t pfenet;                           /**< +0x0E: Ethernet Control Register */
  uint8_t          reserved3[k_mpc_reserved3_bytes]; /**< +0x0F-0x1E: Reserved */

  /* Write Protection (0x0008C11F) */
  volatile uint8_t pwpr; /**< +0x1F: Write Protect Register (enable PFS writes) */

  /* Reserved and Drive Capacity (0x0008C120 - 0x0008C13F) */
  uint8_t reserved4[k_mpc_reserved4_bytes]; /**< +0x20-0x27: Reserved */
  uint8_t dscr2[k_mpc_dscr2_count];         /**< +0x28-0x3F: Drive Capacity Control 2 */

  /* Pin Function Select Registers start at 0x0008C140 (+0x40 from base) */
  volatile uint8_t p00pfs;                        /**< Port 0 Pin 0 Function Select */
  volatile uint8_t p01pfs;                        /**< Port 0 Pin 1 Function Select */
  volatile uint8_t p02pfs;                        /**< Port 0 Pin 2 Function Select */
  volatile uint8_t p03pfs;                        /**< Port 0 Pin 3 Function Select */
  volatile uint8_t p04pfs;                        /**< Port 0 Pin 4 Function Select */
  volatile uint8_t p05pfs;                        /**< Port 0 Pin 5 Function Select */
  volatile uint8_t p06pfs;                        /**< Port 0 Pin 6 Function Select */
  volatile uint8_t p07pfs;                        /**< Port 0 Pin 7 Function Select */
  volatile uint8_t p10pfs;                        /**< Port 1 Pin 0 Function Select */
  volatile uint8_t p11pfs;                        /**< Port 1 Pin 1 Function Select */
  volatile uint8_t p12pfs;                        /**< Port 1 Pin 2 Function Select */
  volatile uint8_t p13pfs;                        /**< Port 1 Pin 3 Function Select */
  volatile uint8_t p14pfs;                        /**< Port 1 Pin 4 Function Select */
  volatile uint8_t p15pfs;                        /**< Port 1 Pin 5 Function Select */
  volatile uint8_t p16pfs;                        /**< Port 1 Pin 6 Function Select */
  volatile uint8_t p17pfs;                        /**< Port 1 Pin 7 Function Select */
  volatile uint8_t p20pfs;                        /**< Port 2 Pin 0 Function Select */
  volatile uint8_t p21pfs;                        /**< Port 2 Pin 1 Function Select */
  volatile uint8_t p22pfs;                        /**< Port 2 Pin 2 Function Select */
  volatile uint8_t p23pfs;                        /**< Port 2 Pin 3 Function Select */
  volatile uint8_t p24pfs;                        /**< Port 2 Pin 4 Function Select */
  volatile uint8_t p25pfs;                        /**< Port 2 Pin 5 Function Select */
  volatile uint8_t p26pfs;                        /**< Port 2 Pin 6 Function Select */
  volatile uint8_t p27pfs;                        /**< Port 2 Pin 7 Function Select */
  volatile uint8_t p30pfs;                        /**< Port 3 Pin 0 Function Select */
  volatile uint8_t p31pfs;                        /**< Port 3 Pin 1 Function Select */
  volatile uint8_t p32pfs;                        /**< Port 3 Pin 2 Function Select */
  volatile uint8_t p33pfs;                        /**< Port 3 Pin 3 Function Select */
  volatile uint8_t p34pfs;                        /**< Port 3 Pin 4 Function Select */
  uint8_t port3_reserved[k_port3_reserved_bytes]; /**< Port 3 reserved (P35=NMI, P36-P37 N/A) */
  volatile uint8_t p40pfs;                        /**< Port 4 Pin 0 Function Select */
  volatile uint8_t p41pfs;                        /**< Port 4 Pin 1 Function Select */
  volatile uint8_t p42pfs;                        /**< Port 4 Pin 2 Function Select */
  volatile uint8_t p43pfs;                        /**< Port 4 Pin 3 Function Select */
  volatile uint8_t p44pfs;                        /**< Port 4 Pin 4 Function Select */
  volatile uint8_t p45pfs;                        /**< Port 4 Pin 5 Function Select */
  volatile uint8_t p46pfs;                        /**< Port 4 Pin 6 Function Select */
  volatile uint8_t p47pfs;                        /**< Port 4 Pin 7 Function Select */
  volatile uint8_t p50pfs;                        /**< Port 5 Pin 0 Function Select */
  volatile uint8_t p51pfs;                        /**< Port 5 Pin 1 Function Select */
  volatile uint8_t p52pfs;                        /**< Port 5 Pin 2 Function Select */
  uint8_t port5_reserved[k_port5_reserved_bytes]; /**< Port 5 reserved (P53=BCLK dedicated) */
  volatile uint8_t p54pfs;                        /**< Port 5 Pin 4 Function Select */
  volatile uint8_t p55pfs;                        /**< Port 5 Pin 5 Function Select */
  volatile uint8_t p56pfs;                        /**< Port 5 Pin 6 Function Select */
  volatile uint8_t p57pfs;                        /**< Port 5 Pin 7 Function Select */
  volatile uint8_t p60pfs;                        /**< Port 6 Pin 0 Function Select */
  volatile uint8_t p61pfs;                        /**< Port 6 Pin 1 Function Select */
  volatile uint8_t p62pfs;                        /**< Port 6 Pin 2 Function Select */
  volatile uint8_t p63pfs;                        /**< Port 6 Pin 3 Function Select */
  volatile uint8_t p64pfs;                        /**< Port 6 Pin 4 Function Select */
  uint8_t port6_reserved[k_port6_reserved_bytes]; /**< Port 6 reserved (P65=CKE dedicated) */
  volatile uint8_t p66pfs;                        /**< Port 6 Pin 6 Function Select */
  volatile uint8_t p67pfs;                        /**< Port 6 Pin 7 Function Select */
  uint8_t          port7_reserved[k_port7_reserved_bytes];   /**< Port 7 reserved (P70 N/A) */
  volatile uint8_t p71pfs;                                   /**< Port 7 Pin 1 Function Select */
  volatile uint8_t p72pfs;                                   /**< Port 7 Pin 2 Function Select */
  volatile uint8_t p73pfs;                                   /**< Port 7 Pin 3 Function Select */
  volatile uint8_t p74pfs;                                   /**< Port 7 Pin 4 Function Select */
  volatile uint8_t p75pfs;                                   /**< Port 7 Pin 5 Function Select */
  volatile uint8_t p76pfs;                                   /**< Port 7 Pin 6 Function Select */
  volatile uint8_t p77pfs;                                   /**< Port 7 Pin 7 Function Select */
  volatile uint8_t p80pfs;                                   /**< Port 8 Pin 0 Function Select */
  volatile uint8_t p81pfs;                                   /**< Port 8 Pin 1 Function Select */
  volatile uint8_t p82pfs;                                   /**< Port 8 Pin 2 Function Select */
  volatile uint8_t p83pfs;                                   /**< Port 8 Pin 3 Function Select */
  volatile uint8_t p84pfs;                                   /**< Port 8 Pin 4 Function Select */
  volatile uint8_t p85pfs;                                   /**< Port 8 Pin 5 Function Select */
  volatile uint8_t p86pfs;                                   /**< Port 8 Pin 6 Function Select */
  volatile uint8_t p87pfs;                                   /**< Port 8 Pin 7 Function Select */
  volatile uint8_t p90pfs;                                   /**< Port 9 Pin 0 Function Select */
  volatile uint8_t p91pfs;                                   /**< Port 9 Pin 1 Function Select */
  volatile uint8_t p92pfs;                                   /**< Port 9 Pin 2 Function Select */
  volatile uint8_t p93pfs;                                   /**< Port 9 Pin 3 Function Select */
  volatile uint8_t p94pfs;                                   /**< Port 9 Pin 4 Function Select */
  volatile uint8_t p95pfs;                                   /**< Port 9 Pin 5 Function Select */
  volatile uint8_t p96pfs;                                   /**< Port 9 Pin 6 Function Select */
  volatile uint8_t p97pfs;                                   /**< Port 9 Pin 7 Function Select */
  volatile uint8_t pa0pfs;                                   /**< Port A Pin 0 Function Select */
  volatile uint8_t pa1pfs;                                   /**< Port A Pin 1 Function Select */
  volatile uint8_t pa2pfs;                                   /**< Port A Pin 2 Function Select */
  volatile uint8_t pa3pfs;                                   /**< Port A Pin 3 Function Select */
  volatile uint8_t pa4pfs;                                   /**< Port A Pin 4 Function Select */
  volatile uint8_t pa5pfs;                                   /**< Port A Pin 5 Function Select */
  volatile uint8_t pa6pfs;                                   /**< Port A Pin 6 Function Select */
  volatile uint8_t pa7pfs;                                   /**< Port A Pin 7 Function Select */
  volatile uint8_t pb0pfs;                                   /**< Port B Pin 0 Function Select */
  volatile uint8_t pb1pfs;                                   /**< Port B Pin 1 Function Select */
  volatile uint8_t pb2pfs;                                   /**< Port B Pin 2 Function Select */
  volatile uint8_t pb3pfs;                                   /**< Port B Pin 3 Function Select */
  volatile uint8_t pb4pfs;                                   /**< Port B Pin 4 Function Select */
  volatile uint8_t pb5pfs;                                   /**< Port B Pin 5 Function Select */
  volatile uint8_t pb6pfs;                                   /**< Port B Pin 6 Function Select */
  volatile uint8_t pb7pfs;                                   /**< Port B Pin 7 Function Select */
  volatile uint8_t pc0pfs;                                   /**< Port C Pin 0 Function Select */
  volatile uint8_t pc1pfs;                                   /**< Port C Pin 1 Function Select */
  volatile uint8_t pc2pfs;                                   /**< Port C Pin 2 Function Select */
  volatile uint8_t pc3pfs;                                   /**< Port C Pin 3 Function Select */
  volatile uint8_t pc4pfs;                                   /**< Port C Pin 4 Function Select */
  volatile uint8_t pc5pfs;                                   /**< Port C Pin 5 Function Select */
  volatile uint8_t pc6pfs;                                   /**< Port C Pin 6 Function Select */
  volatile uint8_t pc7pfs;                                   /**< Port C Pin 7 Function Select */
  volatile uint8_t pd0pfs;                                   /**< Port D Pin 0 Function Select */
  volatile uint8_t pd1pfs;                                   /**< Port D Pin 1 Function Select */
  volatile uint8_t pd2pfs;                                   /**< Port D Pin 2 Function Select */
  volatile uint8_t pd3pfs;                                   /**< Port D Pin 3 Function Select */
  volatile uint8_t pd4pfs;                                   /**< Port D Pin 4 Function Select */
  volatile uint8_t pd5pfs;                                   /**< Port D Pin 5 Function Select */
  volatile uint8_t pd6pfs;                                   /**< Port D Pin 6 Function Select */
  volatile uint8_t pd7pfs;                                   /**< Port D Pin 7 Function Select */
  volatile uint8_t pe0pfs;                                   /**< Port E Pin 0 Function Select */
  volatile uint8_t pe1pfs;                                   /**< Port E Pin 1 Function Select */
  volatile uint8_t pe2pfs;                                   /**< Port E Pin 2 Function Select */
  volatile uint8_t pe3pfs;                                   /**< Port E Pin 3 Function Select */
  volatile uint8_t pe4pfs;                                   /**< Port E Pin 4 Function Select */
  volatile uint8_t pe5pfs;                                   /**< Port E Pin 5 Function Select */
  volatile uint8_t pe6pfs;                                   /**< Port E Pin 6 Function Select */
  volatile uint8_t pe7pfs;                                   /**< Port E Pin 7 Function Select */
  volatile uint8_t pf0pfs;                                   /**< Port F Pin 0 Function Select */
  volatile uint8_t pf1pfs;                                   /**< Port F Pin 1 Function Select */
  volatile uint8_t pf2pfs;                                   /**< Port F Pin 2 Function Select */
  uint8_t          portf_reserved1[k_portf_reserved1_bytes]; /**< Port F reserved (PF3-PF4 N/A) */
  volatile uint8_t pf5pfs;                                   /**< Port F Pin 5 Function Select */
  uint8_t          portf_reserved2[k_portf_reserved2_bytes]; /**< Port F reserved (PF6-PF7 N/A) */
  volatile uint8_t pg0pfs;                                   /**< Port G Pin 0 Function Select */
  volatile uint8_t pg1pfs;                                   /**< Port G Pin 1 Function Select */
  volatile uint8_t pg2pfs;                                   /**< Port G Pin 2 Function Select */
  volatile uint8_t pg3pfs;                                   /**< Port G Pin 3 Function Select */
  volatile uint8_t pg4pfs;                                   /**< Port G Pin 4 Function Select */
  volatile uint8_t pg5pfs;                                   /**< Port G Pin 5 Function Select */
  volatile uint8_t pg6pfs;                                   /**< Port G Pin 6 Function Select */
  volatile uint8_t pg7pfs;                                   /**< Port G Pin 7 Function Select */
  volatile uint8_t ph0pfs;                                   /**< Port H Pin 0 Function Select */
  volatile uint8_t ph1pfs;                                   /**< Port H Pin 1 Function Select */
  volatile uint8_t ph2pfs;                                   /**< Port H Pin 2 Function Select */
  volatile uint8_t ph3pfs;                                   /**< Port H Pin 3 Function Select */
  volatile uint8_t ph4pfs;                                   /**< Port H Pin 4 Function Select */
  volatile uint8_t ph5pfs;                                   /**< Port H Pin 5 Function Select */
  volatile uint8_t ph6pfs;                                   /**< Port H Pin 6 Function Select */
  volatile uint8_t ph7pfs;                                   /**< Port H Pin 7 Function Select */
  volatile uint8_t pj0pfs;                                   /**< Port J Pin 0 Function Select */
  volatile uint8_t pj1pfs;                                   /**< Port J Pin 1 Function Select */
  volatile uint8_t pj2pfs;                                   /**< Port J Pin 2 Function Select */
  volatile uint8_t pj3pfs;                                   /**< Port J Pin 3 Function Select */
  uint8_t          portj_reserved1[k_portj_reserved1_bytes]; /**< Port J reserved (PJ4 N/A) */
  volatile uint8_t pj5pfs;                                   /**< Port J Pin 5 Function Select */
  uint8_t          portj_reserved2[k_portj_reserved2_bytes]; /**< Port J reserved (PJ6-PJ7 N/A) */
} rx_mpc_regs_t;

/**
 * @brief Get pointer to MPC registers
 *
 * @details
 * Returns a volatile pointer to the MPC register structure at address 0x0008C100.
 * Provides access to all MPC registers including PWPR and PFS registers.
 *
 * @return Volatile pointer to MPC register structure
 *
 * @par Example
 * @code
 * // Unlock, configure pin, re-lock
 * mpc()->pwpr = 0x00;              // Clear B0WI
 * mpc()->pwpr = k_mpc_pwpr_pfswe;  // Enable PFS write
 * mpc()->pb7pfs = 0x0A;            // SCI9 TXD
 * mpc()->pwpr = 0x00;              // Clear PFSWE
 * mpc()->pwpr = k_mpc_pwpr_b0wi;   // Protect PFSWE
 * @endcode
 */
static inline volatile rx_mpc_regs_t* mpc(void)
{
  return (volatile rx_mpc_regs_t*)k_mpc_base_addr;
}

/**
 * @enum mpc_pwpr_bits_t
 * @brief MPC Write Protect Register (PWPR) bit definitions
 *
 * @details
 * The PWPR register protects PFS registers from accidental writes.
 * A specific unlock sequence is required to modify any PFS register.
 *
 * @par PWPR Register Layout
 * @verbatim
 *   Bit | Name  | Description
 *   ----+-------+------------------------------------------------
 *    7  | B0WI  | PFSWE Bit Write Disable (1=PFSWE protected)
 *    6  | PFSWE | PFS Write Enable (1=PFS writes allowed)
 *   5-0 |  -    | Reserved
 * @endverbatim
 *
 * @par Unlock Sequence
 * 1. Write 0x00 to PWPR (clear B0WI)
 * 2. Write 0x40 to PWPR (set PFSWE)
 * 3. Write to desired PFS register(s)
 * 4. Write 0x00 to PWPR (clear PFSWE)
 * 5. Write 0x80 to PWPR (set B0WI)
 *
 * @warning Failing to re-lock after configuration leaves PFS vulnerable!
 */
typedef enum : uint8_t {
  k_mpc_pwpr_pfswe = (1 << 6), /**< Bit 6: PFS Write Enable (1=allow writes) */
  k_mpc_pwpr_b0wi  = (1 << 7), /**< Bit 7: PFSWE Write Disable (1=protect PFSWE) */
} mpc_pwpr_bits_t;

/**
 * @enum pfs_bits_t
 * @brief Pin Function Select (PFS) register bit definitions
 *
 * @details
 * Each GPIO pin has a PFS register controlling its function assignment.
 * The PSEL field selects which peripheral function the pin is connected to.
 *
 * @par PFS Register Layout
 * @verbatim
 *   Bit | Name      | Description
 *   ----+-----------+--------------------------------------------
 *    7  | ASEL      | Analog Input Select (1=analog function)
 *    6  | ISEL      | IRQ Input Select (1=IRQ input enabled)
 *    5  | Reserved  | Always write 0
 *   4-0 | PSEL[4:0] | Peripheral function select (0x00-0x1F)
 * @endverbatim
 *
 * @note PSEL values are pin-specific. Check RX72N Manual Appendix for
 *       the function assignment table for each pin.
 */
typedef enum : uint8_t {
  k_pfs_psel_mask = 0x1F,     /**< Bits 0-4: Peripheral Select mask */
  k_pfs_isel      = (1 << 6), /**< Bit 6: Interrupt Input Select */
  k_pfs_asel      = (1 << 7), /**< Bit 7: Analog Input Select */
} pfs_bits_t;

/* =============================================================================
 * Compile-Time Register Layout Verification
 * =============================================================================
 * These static assertions verify the struct layout matches hardware exactly.
 * If any assertion fails, the struct definition is WRONG and must be fixed!
 *
 * Reference: RX72N Group User's Manual: Hardware, Chapter 23, Section 23.2
 * =============================================================================
 */

/**
 * @defgroup mpc_static_assert MPC Static Assertions
 * @brief Compile-time verification of MPC register layout
 * @details
 * These static assertions are verified at compile time to ensure the
 * C structure definitions match the actual hardware register layout.
 * A failed assertion indicates a bug in the register definitions that
 * must be fixed before the code can compile.
 * @{
 */

/* Verify critical register offsets from base (0x0008C100) */
static_assert(offsetof(rx_mpc_regs_t, pfcse) == 0,
              "PFCSE must be at offset 0x00 (address 0x0008C100)");
static_assert(offsetof(rx_mpc_regs_t, pwpr) == k_mpc_offset_pwpr,
              "PWPR must be at offset 0x1F (address 0x0008C11F)");
static_assert(offsetof(rx_mpc_regs_t, p00pfs) == k_mpc_offset_pfs,
              "P00PFS must be at offset 0x40 (address 0x0008C140)");

/* Verify specific port offsets from P00PFS base */
static_assert(offsetof(rx_mpc_regs_t, p50pfs) == k_mpc_offset_pfs + k_mpc_offset_port5_pfs,
              "P50PFS must be at offset 0x68 (Port 5 base)");
static_assert(offsetof(rx_mpc_regs_t, pa0pfs) == k_mpc_offset_pfs + k_mpc_offset_porta_pfs,
              "PA0PFS must be at offset 0x90 (Port A base)");
static_assert(offsetof(rx_mpc_regs_t, pe0pfs) == k_mpc_offset_pfs + k_mpc_offset_porte_pfs,
              "PE0PFS must be at offset 0xB0 (Port E base)");
static_assert(offsetof(rx_mpc_regs_t, pj0pfs) == k_mpc_offset_pfs + k_mpc_offset_portj_pfs,
              "PJ0PFS must be at offset 0xD0 (Port J base)");

/* Verify struct total size (64 bytes control + 152 bytes PFS = 216 bytes) */
static_assert(sizeof(rx_mpc_regs_t) == k_mpc_offset_pfs + k_mpc_pfs_total_bytes,
              "MPC struct size must be 64 + 152 = 216 bytes total");

/** @} */ /* End of mpc_static_assert group */

/** @} */ /* End of mpc_regs group */

/* =============================================================================
 * Pin Mapping Examples
 * =============================================================================
 * Example MPC configuration for common STAR peripherals.
 * See rx_mpc.h for PSEL constant definitions and helper functions.
 *
 * 1. Configure PB7 as SCI9 TXD (Debug UART):
 *    mpc()->pwpr = 0x00;              // Clear B0WI
 *    mpc()->pwpr = k_mpc_pwpr_pfswe;  // Enable PFS write
 *    mpc()->pb7pfs = 0x0A;            // PSEL = 0x0A for SCI
 *    mpc()->pwpr = 0x00;              // Disable PFS write
 *    mpc()->pwpr = k_mpc_pwpr_b0wi;   // Protect PFSWE
 *
 * 2. Configure PE5 as GTIOC0A (Motor 0 PWM Phase):
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_pfswe;
 *    mpc()->pe5pfs = 0x1E;            // PSEL = 0x1E for GPTW (k_psel_gptw)
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_b0wi;
 *
 * 3. Configure P24 as MTCLKA (Encoder 0 Phase A):
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_pfswe;
 *    mpc()->p24pfs = 0x02;            // PSEL = 0x02 for MTU CLK
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_b0wi;
 *
 * 4. Configure PA4 as SSLA0-B (RPi5 SPI Chip Select):
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_pfswe;
 *    mpc()->pa4pfs = 0x0D;            // PSEL = 0x0D for RSPI
 *    mpc()->pwpr = 0x00;
 *    mpc()->pwpr = k_mpc_pwpr_b0wi;
 *
 * Note: After MPC configuration, also set PMR (Port Mode Register) bit to '1'
 * to enable peripheral mode. See rx_mpc.h for helper functions and named constants.
 */

#ifdef __cplusplus
}
#endif
