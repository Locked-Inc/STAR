/**
 * @file rx72n_icu_regs.h
 * @brief RX72N ICU Interrupt Controller Unit Register Definitions
 *
 * @details
 * This header provides comprehensive register definitions for the RX72N's
 * Interrupt Controller Unit (ICU). The ICU manages all 256 interrupt sources
 * with configurable priorities, enabling/disabling, and status monitoring.
 *
 * @par System Architecture Context:
 * @verbatim
 * +----------------------------------------------------------------------+
 * |                    RX72N Interrupt Controller (ICU)                  |
 * +----------------------------------------------------------------------+
 * |                                                                      |
 * |  Peripheral --> ICU --> Priority Arbitration --> CPU Interrupt       |
 * |  (CMT, SCI,     |       (0=disabled, 1-15)       (via vector table)  |
 * |   MTU, GPTW,    |                                                    |
 * |   USB, etc.)    v                                                    |
 * |              +-------------------------------------------------+     |
 * |              |  IR[256]    - Interrupt Request Flags           |     |
 * |              |  IER[32]    - Interrupt Enable Registers        |     |
 * |              |  IPR[256]   - Interrupt Priority Registers      |     |
 * |              |  DTCER[256] - DTC Enable Registers              |     |
 * |              |  IRQCR[16]  - External IRQ Control              |     |
 * |              |  NMI Regs   - Non-Maskable Interrupt Control    |     |
 * |              +-------------------------------------------------+     |
 * |                                                                      |
 * +----------------------------------------------------------------------+
 * @endverbatim
 *
 * @par ICU Key Features:
 * - 256 interrupt vectors (16-255 usable, 0-15 reserved)
 * - 16 priority levels (0=disabled, 1=lowest, 15=highest)
 * - Per-interrupt enable/disable control via IER registers
 * - Fast interrupt mode (FIR) for single-cycle response
 * - DTC (Data Transfer Controller) integration
 * - 16 external IRQ pins with edge/level detection and filtering
 * - NMI (Non-Maskable Interrupt) with dedicated registers
 *
 * @par STAR Project Interrupt Usage:
 *
 * | Vector  | Source        | Priority | Description                          |
 * |---------|---------------|----------|--------------------------------------|
 * | 28      | CMT0_CMI0     | 10       | System tick (1 ms, ThreadX)          |
 * | 29      | CMT1_CMI1     | 8        | Motor control timer                  |
 * | 72-75   | IRQ8-IRQ11    | varies   | HC-SR04 echo (4 sensors, both edges) |
 * | 188-191 | INTB188-INTB191 | 14     | POEG fault protection (4 motors)     |
 *
 * @par Note on vectors 188-191 (POEG): these are SLIBR software-configurable
 * interrupt B vectors. The SLIBXR/SLIBR registers (offsets 0x780-0x7CF) must
 * be programmed to route POEGGAI/BI/CI/DI to INTB188-191 respectively.
 * SCI9 (debug UART) is polled, not interrupt-driven; its fixed vectors are
 * 102 (RXI9) and 103 (TXI9). SCI1 fixed vectors are 60 (RXI1) and 61 (TXI1).
 *
 * @par Hardware Requirements:
 * - CPU: Renesas RX72N with 256-vector interrupt controller
 * - ICU base address: 0x00087000 (fixed, non-relocatable)
 * - Register access: 8-bit for IR/IER/IPR, 16-bit for FIR
 *
 * @par Memory Usage:
 * - Register structure size: 0xA02 bytes (2562 bytes)
 * - No dynamic allocation required
 * - Direct memory-mapped register access
 *
 * @see docs/sections/03_hardware_pinout.tex External IRQ pin assignments
 * @see lib/rx_core/inc/rx_error_handler.h Error and fault interrupt handling
 * @see RX72N Group User's Manual: Hardware, Chapter 15 (Interrupt Controller)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, or recursion in accessor functions
 * - Rule 3: [OK] Zero dynamic allocation - all registers statically mapped
 * - Rule 8: [OK] All constants as C23 typed enums (vectors, priorities)
 * - Rule 10: [OK] Compile with -Wall -Wextra -Werror, zero warnings
 *
 * @par SOLID Principles:
 * - S: Single responsibility - only ICU register definitions
 * - O: Open for extension via accessor functions
 * - I: Interface segregation - minimal required registers exposed
 * - D: Dependency inversion - hardware accessed via typed pointers
 *
 * @par Module Dependencies:
 * @dot
 * digraph icu_deps {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   icu_regs [label="rx72n_icu_regs.h"];
 *   stdint [label="<stdint.h>"];
 *   cmt [label="rx_cmt.c"];
 *   uart [label="uart.c"];
 *   usb_isr [label="rx_usb_isr.c"];
 *   error [label="rx_error_handler.c"];
 *   icu_regs -> stdint;
 *   cmt -> icu_regs;
 *   uart -> icu_regs;
 *   usb_isr -> icu_regs;
 *   error -> icu_regs;
 * }
 * @enddot
 *
 * @par Verification Status:
 * [PASS] VERIFIED (2026-01-28) - All register offsets verified against
 * RX72N Manual Ch15 section 15.2. Corrections applied:
 * - NMI register layout corrected (was wrong order)
 * - DMRSR registers fixed: 8-bit regs at 4-byte aligned addresses (0x400/404/408/...)
 *   NOT consecutive bytes as originally implemented
 *
 * @author Locked, Inc.
 * @date 2026-01-28
 * @version 1.0.0
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
 * @defgroup rx_icu_regs RX72N ICU Register Definitions
 * @brief Interrupt Controller Unit hardware register interface
 *
 * @details
 * Provides memory-mapped register access to the RX72N ICU peripheral
 * for interrupt configuration, enabling, and priority management.
 *
 * @par Quick Start Example - Enable CMT0 Interrupt:
 * @code{.c}
 * volatile rx_icu_regs_t* icu_regs = icu();
 *
 * // 1. Clear any pending interrupt request
 * icu_regs->ir[k_vect_cmt0_cmi0] = 0x00;
 *
 * // 2. Set priority (1=lowest, 15=highest, 0=disabled)
 * icu_regs->ipr[k_vect_cmt0_cmi0] = 10;  // Priority 10
 *
 * // 3. Enable interrupt in IER register
 * // IER[3] bit 4 = vector 28 (CMT0_CMI0)
 * // IER index = vector / 8, bit = vector % 8
 * uint8_t ier_index = k_vect_cmt0_cmi0 / 8;  // = 3
 * uint8_t ier_bit = k_vect_cmt0_cmi0 % 8;    // = 4
 * icu_regs->ier[ier_index] |= (1U << ier_bit);
 *
 * // 4. Enable global interrupts (in PSW register, done separately)
 * @endcode
 *
 * @{
 */

/* =============================================================================
 * Interrupt Controller (ICU)
 * =============================================================================
 */

/**
 * @enum rx_icu_addresses_t
 * @brief ICU peripheral base address
 *
 * @details
 * The ICU occupies a contiguous block of memory starting at 0x00087000.
 * All ICU registers are accessed relative to this base address.
 *
 * @par Address Space:
 * | Offset Range | Registers           | Description                         |
 * |--------------|---------------------|-------------------------------------|
 * | 0x000-0x0FF  | IR[256]             | Interrupt Request Flags             |
 * | 0x100-0x1FF  | DTCER[256]          | DTC Enable Registers                |
 * | 0x200-0x21F  | IER[32]             | Interrupt Enable Regs               |
 * | 0x2E0-0x2E1  | SWINTR, SWINT2R     | Software Interrupt                  |
 * | 0x2F0        | FIR                 | Fast Interrupt Register             |
 * | 0x300-0x3FF  | IPR[256]            | Interrupt Priority Regs             |
 * | 0x400-0x41F  | DMRSR[8]            | DMAC Module Start Regs              |
 * | 0x500-0x50F  | IRQCR[16]           | External IRQ Control                |
 * | 0x520-0x52D  | IRQFLTE, IRQFLTC    | IRQ Digital Filter                  |
 * | 0x580-0x594  | NMI Registers       | Non-Maskable Interrupt              |
 * | 0x780-0x78F  | SLIBXRn[16]         | Selectable Int B source (vect 128-143) |
 * | 0x790-0x7CF  | SLIBRn[64]          | Selectable Int B source (vect 144-207) |
 * | 0x9D0-0x9FF  | SLIARn[48]          | Selectable Int A source (vect 208-255) |
 * | 0xA00        | SLIPRCR             | SLIBXR/SLIBR/SLIAR write-protect    |
 * | 0xA01        | SELEXDR             | Selectable ext int direction        |
 *
 * @invariant Base address is fixed and cannot be relocated
 */
typedef enum : uintptr_t {
  k_icu_base_addr       = 0x00087000, /**< ICU register base address (fixed) */
  k_icu_slibr_base_addr = 0x00087700, /**< SELECTB source-select table (n=144..207) */
  k_icu_sliar_base_addr = 0x00087900, /**< SELECTA source-select table (n=208..255) */
} rx_icu_addresses_t;

/**
 * @enum icu_reserved_sizes_t
 * @brief ICU register reserved field sizes for structure padding
 *
 * @details
 * Defines byte sizes for reserved regions in the ICU register map.
 * These ensure proper alignment and offset matching between the C
 * structure and actual hardware register locations.
 *
 * @warning Do not modify these values - they are derived from the
 *          RX72N hardware register layout specification (Ch15).
 */
typedef enum : uint16_t {
  k_icu_reserved_after_ier_bytes      = 192, /**< 0x220-0x2DF: Gap to SWINTR */
  k_icu_reserved_after_swint2r_bytes  = 14,  /**< 0x2E2-0x2EF: Gap to FIR */
  k_icu_reserved_after_fir_bytes      = 14,  /**< 0x2F2-0x2FF: Gap to IPR */
  k_icu_reserved_after_dmrsr_bytes    = 224, /**< 0x420-0x4FF: Gap to IRQCR (was 248, FIXED) */
  k_icu_reserved_after_irqcr_bytes    = 16,  /**< 0x510-0x51F: Gap to IRQFLTE */
  k_icu_reserved_after_irqflte_bytes  = 6,   /**< 0x522-0x527: Gap to IRQFLTC */
  k_icu_reserved_after_irqfltc_bytes  = 84,  /**< 0x52C-0x57F: Gap to NMISR */
  k_icu_reserved_after_exnmiclr_bytes = 9,   /**< 0x587-0x58F: Gap to NMIFLTE */
  k_icu_reserved_after_nmiflte_bytes  = 3,   /**< 0x591-0x593: Gap to NMIFLTC */
  k_icu_reserved_after_nmifltc_bytes  = 491, /**< 0x595-0x77F: Gap to SLIBXRn */
  k_icu_reserved_after_slibr_bytes    = 512, /**< 0x7D0-0x9CF: Gap to SLIARn */
  k_icu_dmrsr_entry_padding           = 3,   /**< DMRSR 8-bit regs at 4-byte boundaries */
} icu_reserved_sizes_t;

/**
 * @enum icu_array_sizes_t
 * @brief ICU register array dimensions
 *
 * @details
 * Defines the sizes of register arrays in the ICU. These values are
 * used to dimension arrays in the rx_icu_regs_t structure.
 *
 * @par Register Array Purpose:
 * - IR[256]: One flag per interrupt vector (vectors 0-255)
 * - DTCER[256]: One enable bit per DTC-capable vector
 * - IER[32]: 8 bits per register, covering 256 vectors (32 x 8 = 256)
 * - IPR[256]: One priority level per interrupt vector
 * - DMRSR[8]: DMAC channel 0-7 start source selection (4-byte aligned)
 * - IRQCR[16]: External IRQ0-15 edge/level configuration
 */
typedef enum : uint16_t {
  k_icu_ir_count         = 256, /**< 256 interrupt request flags (one per vector) */
  k_icu_dtcer_count      = 256, /**< 256 DTC enable flags */
  k_icu_ier_count        = 32,  /**< 32 enable registers x 8 bits = 256 vectors */
  k_icu_ier_bits_per_reg = 8,   /**< Each IER controls 8 interrupt enables */
  k_icu_ipr_count        = 256, /**< 256 priority registers (0-15 each) */
  k_icu_dmrsr_count      = 8,   /**< 8 DMAC channels */
  k_icu_irqcr_count      = 16,  /**< 16 external IRQ pins (IRQ0-15) */
  k_icu_irqflte_count    = 2,   /**< 2 filter enable registers (16 bits total) */
  k_icu_irqfltc_count    = 2,   /**< 2 filter clock registers */
  k_icu_slibxr_count     = 16,  /**< SLIBXRn entries: vectors 128-143 (INTB128-143) */
  k_icu_slibr_count      = 64,  /**< SLIBRn entries: vectors 144-207 (INTB144-207) */
  k_icu_sliar_count      = 48,  /**< SLIARn entries: vectors 208-255 (INTA208-255) */
} icu_array_sizes_t;

/**
 * @struct rx_dmrsr_entry_t
 * @brief DMAC Module Start Register entry with 4-byte alignment
 *
 * @details
 * The DMRSR registers are 8-bit registers but located at 4-byte aligned
 * addresses (0x400, 0x404, 0x408, 0x40C, 0x410, 0x414, 0x418, 0x41C).
 * This structure provides proper padding to match the hardware layout.
 *
 * @par Manual Reference:
 * Ch15 Section 15.2.8 - DMRSR0-7 addresses:
 * - DMRSR0: 0x00087400
 * - DMRSR1: 0x00087404
 * - DMRSR2: 0x00087408
 * - DMRSR3: 0x0008740C
 * - DMRSR4: 0x00087410
 * - DMRSR5: 0x00087414
 * - DMRSR6: 0x00087418
 * - DMRSR7: 0x0008741C
 *
 * @par Usage:
 * @code{.c}
 * // Set DMAC channel 0 trigger to vector 28 (CMT0)
 * icu()->dmrsr[0].reg = 28;
 *
 * // Set DMAC channel 3 trigger to USB vector
 * icu()->dmrsr[3].reg = 102;
 * @endcode
 *
 * @note Each DMRSR register selects the interrupt vector number that
 *       triggers the corresponding DMAC channel transfer.
 *
 * @warning Do not set the same vector number for multiple DMRSR registers.
 * @warning Do not set the same interrupt as both DTC and DMAC trigger.
 */
typedef struct {
  volatile uint8_t reg;                                 /**< 8-bit vector number register */
  uint8_t          reserved[k_icu_dmrsr_entry_padding]; /**< Padding to 4-byte boundary */
} rx_dmrsr_entry_t;

/**
 * @struct rx_icu_regs_t
 * @brief ICU Register Map for Interrupt Controller Unit
 *
 * @details
 * Memory-mapped register structure for the ICU peripheral. This structure
 * provides direct hardware access to interrupt control registers including
 * request flags, enable control, priorities, and external IRQ configuration.
 *
 * @par Interrupt Handling Workflow:
 * 1. Peripheral generates interrupt request -> IR[vector] = 1
 * 2. ICU checks IER[vector/8] bit (vector%8) for enable
 * 3. If enabled, ICU checks IPR[vector] for priority (1-15)
 * 4. Higher priority interrupts preempt lower priority
 * 5. CPU vectors to interrupt handler
 * 6. Handler clears IR[vector] = 0 before returning
 *
 * @par Usage Example - Configure Multiple Interrupts:
 * @code{.c}
 * volatile rx_icu_regs_t* icu_regs = icu();
 *
 * // Disable all interrupts during configuration
 * // (typically done with PSW.I bit, not shown here)
 *
 * // Configure CMT0 (vector 28) - System tick at priority 10
 * icu_regs->ir[28] = 0;      // Clear pending
 * icu_regs->ipr[28] = 10;    // Priority 10
 * icu_regs->ier[3] |= 0x10;  // Enable (IER3 bit 4)
 *
 * // Configure CMT1 (vector 29) - Motor control at priority 12
 * icu_regs->ir[29] = 0;      // Clear pending
 * icu_regs->ipr[29] = 12;    // Priority 12 (higher than tick)
 * icu_regs->ier[3] |= 0x20;  // Enable (IER3 bit 5)
 *
 * // Configure USB (vector 102) - Communication at priority 8
 * icu_regs->ir[102] = 0;     // Clear pending
 * icu_regs->ipr[102] = 8;    // Priority 8
 * icu_regs->ier[12] |= 0x40; // Enable (IER12 bit 6)
 *
 * // Re-enable global interrupts (PSW.I = 1)
 * @endcode
 *
 * @par Memory Layout (Total: ~2562 bytes):
 *
 * | Offset | Size | Field          | Description                          |
 * |--------|------|----------------|--------------------------------------|
 * | 0x000  | 256  | ir[256]        | Interrupt Request flags              |
 * | 0x100  | 256  | dtcer[256]     | DTC Enable registers                 |
 * | 0x200  | 32   | ier[32]        | Interrupt Enable registers           |
 * | 0x220  | 192  | reserved       | Reserved gap                         |
 * | 0x2E0  | 1    | swintr         | Software Interrupt register          |
 * | 0x2E1  | 1    | swint2r        | Software Interrupt 2 register        |
 * | 0x2F0  | 2    | fir            | Fast Interrupt register              |
 * | 0x300  | 256  | ipr[256]       | Interrupt Priority registers         |
 * | 0x400  | 32   | dmrsr[8]       | DMAC Module Start registers          |
 * | 0x500  | 16   | irqcr[16]      | External IRQ Control                 |
 * | 0x520  | 2    | irqflte[2]     | IRQ Filter Enable                    |
 * | 0x528  | 4    | irqfltc[2]     | IRQ Filter Clock Select              |
 * | 0x580  | 4    | nmisr..nmicr   | NMI Status/Enable/Clear/Ctrl         |
 * | 0x584  | 3    | exnmisr..clr   | Extended NMI registers               |
 * | 0x590  | 1    | nmiflte        | NMI Filter Enable                    |
 * | 0x594  | 1    | nmifltc        | NMI Filter Clock                     |
 * | 0x595  | 491  | reserved       | Reserved gap                         |
 * | 0x780  | 16   | slibxr[16]     | Sel. Int B source sel. (vect 128-143)|
 * | 0x790  | 64   | slibr[64]      | Sel. Int B source sel. (vect 144-207)|
 * | 0x7D0  | 512  | reserved       | Reserved gap                         |
 * | 0x9D0  | 48   | sliar[48]      | Sel. Int A source sel. (vect 208-255)|
 * | 0xA00  | 1    | sliprcr        | SLIBXR/SLIBR/SLIAR write-protect     |
 * | 0xA01  | 1    | selexdr        | Selectable ext int direction         |
 *
 * @note Register layout verified against RX72N Manual Ch15, Rev.1.20
 *
 * @warning All registers are memory-mapped and must be accessed as volatile.
 * @warning Some IR/IPR indices (0-15) are reserved and must not be accessed.
 *
 * @invariant IR flags are read-write: write 0 to clear, read to check status
 * @invariant IPR values must be 0-15 (0 disables, 1-15 sets priority)
 * @invariant IER bits: 1=enabled, 0=disabled
 *
 * @see icu() Accessor function
 * @see rx_cmt_interrupt_vector_t Common interrupt vector numbers
 * @see rx_interrupt_priority_t Priority level definitions
 */
typedef struct {
  /* Offset 0x000-0x0FF: Interrupt Request Registers */
  volatile uint8_t ir[k_icu_ir_count]; /**< @0x000 IR016-IR255 */

  /* Offset 0x100-0x1FF: DTC Enable Registers */
  volatile uint8_t dtcer[k_icu_dtcer_count]; /**< @0x100 DTCER026-DTCER255 */

  /* Offset 0x200-0x21F: Interrupt Enable Registers */
  volatile uint8_t ier[k_icu_ier_count];                      /**< @0x200 IER02-IER1F */
  uint8_t          reserved0[k_icu_reserved_after_ier_bytes]; /**< @0x220 Reserved */

  /* Offset 0x2E0-0x2E1: Software Interrupt Registers */
  volatile uint8_t swintr;  /**< @0x2E0 Software Interrupt Register */
  volatile uint8_t swint2r; /**< @0x2E1 Software Interrupt 2 Register */
  uint8_t          reserved1[k_icu_reserved_after_swint2r_bytes]; /**< @0x2E2 Reserved */

  /* Offset 0x2F0: Fast Interrupt Register */
  volatile uint16_t fir; /**< @0x2F0 Fast Interrupt Register */
  uint8_t           reserved2[k_icu_reserved_after_fir_bytes]; /**< @0x2F2 Reserved */

  /* Offset 0x300-0x3FF: Interrupt Priority Registers */
  volatile uint8_t ipr[k_icu_ipr_count]; /**< @0x300 IPR000-IPR255 */

  /* Offset 0x400-0x41F: DMAC Module Start Registers (4-byte aligned 8-bit regs) */
  rx_dmrsr_entry_t dmrsr[k_icu_dmrsr_count]; /**< @0x400 DMRSR0-DMRSR7 (4-byte spacing!) */
  uint8_t reserved3[k_icu_reserved_after_dmrsr_bytes]; /**< @0x420 Reserved (FIXED from 0x408) */

  /* Offset 0x500-0x50F: IRQ Control Registers */
  volatile uint8_t irqcr[k_icu_irqcr_count];                    /**< @0x500 IRQCR0-IRQCR15 */
  uint8_t          reserved4[k_icu_reserved_after_irqcr_bytes]; /**< @0x510 Reserved */

  /* Offset 0x520-0x52D: IRQ Filter Registers */
  volatile uint8_t  irqflte[k_icu_irqflte_count];                  /**< @0x520 IRQFLTE0-IRQFLTE1 */
  uint8_t           reserved5[k_icu_reserved_after_irqflte_bytes]; /**< @0x522 Reserved */
  volatile uint16_t irqfltc[k_icu_irqfltc_count];                  /**< @0x528 IRQFLTC0-IRQFLTC1 */
  uint8_t           reserved6[k_icu_reserved_after_irqfltc_bytes]; /**< @0x52C Reserved */

  /* Offset 0x580-0x597: NMI Registers (CORRECTED ORDER - Manual Ch15 Section 15.2.13-15.2.22) */
  volatile uint8_t nmisr;  /**< @0x580 NMI Status Register */
  volatile uint8_t nmier;  /**< @0x581 NMI Enable Register */
  volatile uint8_t nmiclr; /**< @0x582 NMI Clear Register */
  volatile uint8_t nmicr;  /**< @0x583 NMI Control Register (FIXED: was uint32_t) */

  /* Offset 0x584-0x58F: Extended NMI Registers (ADDED - were missing) */
  volatile uint8_t exnmisr;  /**< @0x584 Extended NMI Status Register */
  volatile uint8_t exnmier;  /**< @0x585 Extended NMI Enable Register */
  volatile uint8_t exnmiclr; /**< @0x586 Extended NMI Clear Register */
  uint8_t          reserved7[k_icu_reserved_after_exnmiclr_bytes]; /**< @0x587 Reserved */

  /* Offset 0x590-0x597: NMI Filter Registers (CORRECTED) */
  volatile uint8_t nmiflte; /**< @0x590 NMI Filter Enable Register (ADDED - was missing) */
  uint8_t          reserved8[k_icu_reserved_after_nmiflte_bytes]; /**< @0x591 Reserved */
  volatile uint8_t nmifltc; /**< @0x594 NMI Filter Clock Select Register (RENAMED from nmiflt) */
  uint8_t          reserved9[k_icu_reserved_after_nmifltc_bytes]; /**< @0x595 Reserved */

  /* Offset 0x780-0x78F: Selectable Interrupt B Source Registers (vectors 128-143) */
  volatile uint8_t slibxr[k_icu_slibxr_count]; /**< @0x780 SLIBXRn: source select for INTB128-143 */

  /* Offset 0x790-0x7CF: Selectable Interrupt B Source Registers (vectors 144-207) */
  volatile uint8_t slibr[k_icu_slibr_count]; /**< @0x790 SLIBRn: source select for INTB144-207 */
  uint8_t          reserved10[k_icu_reserved_after_slibr_bytes]; /**< @0x7D0 Reserved */

  /* Offset 0x9D0-0x9FF: Selectable Interrupt A Source Registers (vectors 208-255) */
  volatile uint8_t sliar[k_icu_sliar_count]; /**< @0x9D0 SLIARn: source select for INTA208-255 */

  /* Offset 0xA00-0xA01: Selectable Interrupt Write-Protect / Direction */
  volatile uint8_t sliprcr; /**< @0xA00 SLIPRCR: write-protect for SLIBXR/SLIBR/SLIAR/SELEXDR */
  volatile uint8_t selexdr; /**< @0xA01 SELEXDR: selectable external interrupt direction */
} rx_icu_regs_t;

/**
 * @brief Get pointer to ICU registers
 *
 * @details
 * Returns a volatile pointer to the ICU register structure,
 * enabling direct hardware register access for interrupt configuration.
 *
 * @return Volatile pointer to ICU register structure at 0x00087000
 *
 * @pre No prerequisites - ICU is always accessible
 *
 * @post Pointer is immediately usable for register access
 * @post No hardware state is modified by this function
 *
 * @note Thread Safety: Function is reentrant (returns constant address)
 * @note Performance: Zero overhead - compiles to single load instruction
 *
 * @warning Interrupt configuration should be done with global interrupts
 *          disabled to prevent race conditions
 *
 * @par Example - Enable/Disable Specific Interrupt:
 * @code{.c}
 * volatile rx_icu_regs_t* icu_regs = icu();
 *
 * // Calculate IER index and bit for vector 28
 * uint8_t vector = 28;
 * uint8_t ier_idx = vector / 8;   // = 3
 * uint8_t ier_bit = vector % 8;   // = 4
 *
 * // Enable interrupt
 * icu_regs->ier[ier_idx] |= (1U << ier_bit);
 *
 * // Disable interrupt
 * icu_regs->ier[ier_idx] &= ~(1U << ier_bit);
 *
 * // Check if interrupt is pending
 * if (icu_regs->ir[vector]) {
 *     // Interrupt is pending
 *     icu_regs->ir[vector] = 0;  // Clear it
 * }
 * @endcode
 *
 * @see rx_icu_regs_t Register structure definition
 */
static inline volatile rx_icu_regs_t* icu(void)
{
  return (volatile rx_icu_regs_t*)k_icu_base_addr;
}

/**
 * @brief Get pointer to ICU SLIBR source-select register for a SELECTB vector.
 *
 * SLIBR is a byte-wide table at 0x00087700 indexed by absolute vector
 * number, covering SELECTB slots 144..207 (RX72N HW manual Ch15 Section
 * 15.2 and hirakuni45/RX RX72N/icu.hpp line 824).  Writing a SELECTB
 * source code (e.g. k_usb0_usbi_sli_src = 62) into SLIBR[vec] routes
 * that peripheral interrupt onto vec.  Vectors outside 144..207 have
 * no backing register -- writes are ignored and reads return 0.
 *
 * @param[in] vec  Absolute vector number in the SELECTB range (144..207)
 * @return volatile uint8_t* Pointer to SLIBR[vec]
 */
static inline volatile uint8_t* icu_slibr(uint8_t vec)
{
  return (volatile uint8_t*)(k_icu_slibr_base_addr + (uintptr_t)vec);
}

/**
 * @enum rx_cmt_interrupt_vector_t
 * @brief CMT (Compare Match Timer) interrupt vector numbers
 *
 * @details
 * Vector numbers for CMT peripheral interrupts. These are used to
 * index into IR[], IER[], and IPR[] arrays.
 *
 * @par IER/Bit Calculation:
 * - IER index = vector / 8
 * - IER bit = vector % 8
 *
 * @par Example: CMT0 (vector 28)
 * - IER[3] (28/8=3), bit 4 (28%8=4)
 * - IPR[28] for priority
 *
 * @see RX72N Manual Ch15 Table 15.4 for complete vector list
 */
typedef enum : uint8_t {
  k_vect_cmt0_cmi0 = 28, /**< CMT0 compare match: IER[3] bit 4, used for 1ms tick */
  k_vect_cmt1_cmi1 = 29, /**< CMT1 compare match: IER[3] bit 5, used for motor control */
} rx_cmt_interrupt_vector_t;

/**
 * @enum rx_sci_interrupt_vector_t
 * @brief SCI (Serial Communications Interface) interrupt vector numbers
 *
 * @details
 * Vector numbers for SCI peripheral receive/transmit interrupts. Used to
 * index into IR[], IER[], and IPR[] arrays per RX72N HUM Table 14.3.
 *
 * @par IER/Bit Calculation:
 * - IER index = vector / 8
 * - IER bit = vector % 8
 *
 * @par Example: SCI9 RXI9 (vector 102)
 * - IER[12] (102/8=12), bit 6 (102%8=6)
 * - IPR[102] for priority
 *
 * @see RX72N HUM Ch14 Table 14.3 for complete vector list
 */
typedef enum : uint8_t {
  k_vect_sci9_rxi9 = 102, /**< SCI9 receive data full: IER[12] bit 6, STAR debug UART RX */
} rx_sci_interrupt_vector_t;

/**
 * @enum rx_interrupt_priority_t
 * @brief Interrupt priority levels for IPR registers
 *
 * @details
 * Priority values for the IPR[n] registers. Higher values mean higher
 * priority. A running interrupt can only be preempted by an interrupt
 * with strictly higher priority.
 *
 * @par Priority Guidelines for STAR Project:
 *
 * | Priority | Usage                    | Example           |
 * |----------|--------------------------|-------------------|
 * | 15       | Reserved for NMI/faults  | Hardware fault    |
 * | 12-14    | Time-critical motor      | Motor PWM sync    |
 * | 9-11     | Real-time control        | CMT tick, encoder |
 * | 6-8      | Communication            | USB, UART RX      |
 * | 3-5      | Background processing    | UART TX, DMA      |
 * | 1-2      | Low priority             | Diagnostics       |
 * | 0        | Disabled                 | Unused interrupts |
 *
 * @warning Setting two interrupts to the same priority is allowed but
 *          undefined which will be serviced first if both are pending
 */
typedef enum : uint8_t {
  k_ipr_level_disable = 0,  /**< Interrupt disabled (will not trigger) */
  k_ipr_level_min     = 1,  /**< Minimum enabled priority (lowest) */
  k_ipr_level_max     = 15, /**< Maximum priority (highest, preempts all) */
} rx_interrupt_priority_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* The k_icu_base_addr enum declaration above is itself the contract for the
 * ICU base address (per Hardware Manual Ch15). A static_assert that re-states
 * the same literal here would duplicate the magic number without adding a
 * cross-check, so we rely on the enum declaration alone. */

/* Verify register structure layout and key offsets (Ch15 Section 15.2) */
static_assert(offsetof(rx_icu_regs_t, ir) == 0x000, "IR offset must be 0x000 (@ 0x00087000)");
static_assert(offsetof(rx_icu_regs_t, dtcer) == 0x100, "DTCER offset must be 0x100 (@ 0x00087100)");
static_assert(offsetof(rx_icu_regs_t, ier) == 0x200, "IER offset must be 0x200 (@ 0x00087200)");
static_assert(offsetof(rx_icu_regs_t, swintr) == 0x2E0,
              "SWINTR offset must be 0x2E0 (@ 0x000872E0)");
static_assert(offsetof(rx_icu_regs_t, swint2r) == 0x2E1,
              "SWINT2R offset must be 0x2E1 (@ 0x000872E1)");
static_assert(offsetof(rx_icu_regs_t, fir) == 0x2F0, "FIR offset must be 0x2F0 (@ 0x000872F0)");
static_assert(offsetof(rx_icu_regs_t, ipr) == 0x300, "IPR offset must be 0x300 (@ 0x00087300)");
static_assert(offsetof(rx_icu_regs_t, dmrsr) == 0x400, "DMRSR offset must be 0x400 (@ 0x00087400)");
static_assert(offsetof(rx_icu_regs_t, irqcr) == 0x500, "IRQCR offset must be 0x500 (@ 0x00087500)");
static_assert(offsetof(rx_icu_regs_t, irqflte) == 0x520,
              "IRQFLTE offset must be 0x520 (@ 0x00087520)");
static_assert(offsetof(rx_icu_regs_t, irqfltc) == 0x528,
              "IRQFLTC offset must be 0x528 (@ 0x00087528)");

/* Verify NMI register offsets (Ch15 Section 15.2.13-15.2.22) - CRITICAL */
static_assert(offsetof(rx_icu_regs_t, nmisr) == 0x580, "NMISR offset must be 0x580 (@ 0x00087580)");
static_assert(offsetof(rx_icu_regs_t, nmier) == 0x581, "NMIER offset must be 0x581 (@ 0x00087581)");
static_assert(offsetof(rx_icu_regs_t, nmiclr) == 0x582,
              "NMICLR offset must be 0x582 (@ 0x00087582)");
static_assert(offsetof(rx_icu_regs_t, nmicr) == 0x583, "NMICR offset must be 0x583 (@ 0x00087583)");
static_assert(offsetof(rx_icu_regs_t, exnmisr) == 0x584,
              "EXNMISR offset must be 0x584 (@ 0x00087584)");
static_assert(offsetof(rx_icu_regs_t, exnmier) == 0x585,
              "EXNMIER offset must be 0x585 (@ 0x00087585)");
static_assert(offsetof(rx_icu_regs_t, exnmiclr) == 0x586,
              "EXNMICLR offset must be 0x586 (@ 0x00087586)");
static_assert(offsetof(rx_icu_regs_t, nmiflte) == 0x590,
              "NMIFLTE offset must be 0x590 (@ 0x00087590)");
static_assert(offsetof(rx_icu_regs_t, nmifltc) == 0x594,
              "NMIFLTC offset must be 0x594 (@ 0x00087594)");

/* Verify register sizes */
static_assert(sizeof(((rx_icu_regs_t*)0)->nmicr) == 1, "NMICR must be 8-bit (1 byte), not 32-bit");

/* Verify selectable interrupt source register offsets (Ch15 Section 15.2.17-15.2.21) */
static_assert(offsetof(rx_icu_regs_t, slibxr) == 0x780,
              "SLIBXRn offset must be 0x780 (@ 0x00087780)");
static_assert(offsetof(rx_icu_regs_t, slibr) == 0x790,
              "SLIBRn offset must be 0x790 (@ 0x00087790)");
static_assert(offsetof(rx_icu_regs_t, sliar) == 0x9D0,
              "SLIARn offset must be 0x9D0 (@ 0x000879D0)");
static_assert(offsetof(rx_icu_regs_t, sliprcr) == 0xA00,
              "SLIPRCR offset must be 0xA00 (@ 0x00087A00)");
static_assert(offsetof(rx_icu_regs_t, selexdr) == 0xA01,
              "SELEXDR offset must be 0xA01 (@ 0x00087A01)");

/* Verify DMRSR entry structure - CRITICAL FIX for 4-byte aligned 8-bit registers */
static_assert(sizeof(rx_dmrsr_entry_t) == 4, "DMRSR entry must be 4 bytes (1 reg + 3 pad)");
static_assert(offsetof(rx_dmrsr_entry_t, reg) == 0, "DMRSR reg offset must be 0");

/* Verify individual DMRSR register addresses (Ch15 Section 15.2.8) */
static_assert(offsetof(rx_icu_regs_t, dmrsr[0]) == 0x400, "DMRSR0 must be at 0x400 (@ 0x00087400)");
static_assert(offsetof(rx_icu_regs_t, dmrsr[1]) == 0x404, "DMRSR1 must be at 0x404 (@ 0x00087404)");
static_assert(offsetof(rx_icu_regs_t, dmrsr[2]) == 0x408, "DMRSR2 must be at 0x408 (@ 0x00087408)");
static_assert(offsetof(rx_icu_regs_t, dmrsr[3]) == 0x40C, "DMRSR3 must be at 0x40C (@ 0x0008740C)");
static_assert(offsetof(rx_icu_regs_t, dmrsr[4]) == 0x410, "DMRSR4 must be at 0x410 (@ 0x00087410)");
static_assert(offsetof(rx_icu_regs_t, dmrsr[5]) == 0x414, "DMRSR5 must be at 0x414 (@ 0x00087414)");
static_assert(offsetof(rx_icu_regs_t, dmrsr[6]) == 0x418, "DMRSR6 must be at 0x418 (@ 0x00087418)");
static_assert(offsetof(rx_icu_regs_t, dmrsr[7]) == 0x41C, "DMRSR7 must be at 0x41C (@ 0x0008741C)");

/** @} */ /* End of rx_icu_regs group */

#ifdef __cplusplus
}
#endif
