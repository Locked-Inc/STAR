/**
 * @file rx72n_doc_regs.h
 * @brief RX72N DOC (Data Operation Circuit) Register Definitions
 *
 * @details
 * Register definitions for the Data Operation Circuit (DOC) peripheral on the
 * RX72N microcontroller. The DOC is a small fixed-function accelerator that
 * performs one of four 16-bit operations on two operands:
 *
 * | Mode (OMS[1:0]) | Operation                               | Flag trigger        |
 * |-----------------|-----------------------------------------|---------------------|
 * | 00 (DCSEL=0)    | 16-bit compare (mismatch detection)     | DODIR != DODSR      |
 * | 00 (DCSEL=1)    | 16-bit compare (match detection)        | DODIR == DODSR      |
 * | 01              | 16-bit addition (DODSR = DODSR + DODIR) | Carry (result > 0xFFFF) |
 * | 10              | 16-bit subtraction (DODSR = DODSR - DODIR) | Borrow (result < 0x0000) |
 * | 11              | Setting prohibited                      | N/A                 |
 *
 * The DOC sets DOPCF whenever the configured condition occurs; DOPCF is read-only
 * and must be cleared by writing 1 to DOPCFCL. An interrupt (GROUPBL0::DOPCI) can
 * optionally be generated via DOPCIE.
 *
 * ## Memory Map
 *
 * | Offset | Size | Register | Access | Description                          |
 * |--------|------|----------|--------|--------------------------------------|
 * | 0x00   | 1    | DOCR     | R/W    | DOC Control Register                 |
 * | 0x01   | 1    | -        | -      | Reserved                             |
 * | 0x02   | 2    | DODIR    | R/W    | Data Input Register                  |
 * | 0x04   | 2    | DODSR    | R/W    | Data Setting Register / Result       |
 *
 * ## DOCR Bit Layout (8-bit)
 *
 * | Bit | Field    | Access | Description                                     |
 * |-----|----------|--------|-------------------------------------------------|
 * | 1:0 | OMS[1:0] | R/W    | Operating Mode Select                           |
 * | 2   | DCSEL    | R/W    | Detection Condition Select (compare mode only)  |
 * | 3   | -        | R      | Reserved (read as 0)                            |
 * | 4   | DOPCIE   | R/W    | DOPCI Interrupt Enable (1=enabled)              |
 * | 5   | DOPCF    | R      | DOPC Flag (1=condition occurred)                |
 * | 6   | DOPCFCL  | W1C    | DOPCF Clear (write 1 to clear DOPCF)            |
 * | 7   | -        | R      | Reserved (read as 0)                            |
 *
 * ## Module Stop Bit
 *
 * DOC is clock-gated via MSTPCRB.MSTPB6. The DOC module stop bit must be
 * cleared (under PRCR.PRC1 unlock) before accessing any DOC register, or writes
 * will be silently ignored.
 *
 * ## Interrupt Source
 *
 * DOC raises one interrupt request: GROUPBL0.DOPCI (vector 110, group bit 29).
 * When DOPCIE=1, setting DOPCF triggers GROUPBL0.DOPCI.
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0111)
 * - Chapter 59: Data Operation Circuit (DOC), pages 2969-2978
 * - Section 11.2.2 MSTPCRB: page 409 (DOC = MSTPB6)
 * - Section 14.4 Group Interrupt Request Sources: GROUPBL0.DOPCI at bit 29
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto / setjmp / recursion (header-only data)
 * - Rule 2: N/A (no loops)
 * - Rule 3: No dynamic allocation
 * - Rule 4: Accessor is a single-statement inline function
 * - Rule 5: N/A (hardware layer; enforced by higher-level API)
 * - Rule 6: Minimal scope (no file-scope variables)
 * - Rule 7: N/A (no return values to check)
 * - Rule 8: All constants use C23 typed enums (uintptr_t for addresses)
 * - Rule 9: No function pointers
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @see rx_doc.h Higher-level DOC API
 * @see rx72n_system_regs.h PRCR and MSTPCRB accessors
 *
 * @author Locked, Inc.
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * DOC Base Address
 * =============================================================================
 */

/**
 * @enum rx_doc_addresses_t
 * @brief DOC register block base address
 *
 * @details
 * Base address for the Data Operation Circuit register block at 0x0008B080.
 * The DOC occupies a 6-byte register window (DOCR, reserved byte, DODIR, DODSR).
 *
 * @par Manual Reference
 * RX72N HW Manual, Chapter 59 (Data Operation Circuit), Register address table,
 * page 2969.
 *
 * @see doc() Accessor function
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /**
   * @brief DOC register base address (0x0008B080)
   * @details Verified against RX72N HW Manual, Chapter 59, page 2969.
   */
  k_doc_base_addr = 0x0008B080,
} rx_doc_addresses_t;

/* =============================================================================
 * DOC Module Stop Bit (MSTPCRB)
 * =============================================================================
 */

/**
 * @enum rx_doc_mstpb_bits_t
 * @brief DOC module stop bit position in MSTPCRB
 *
 * @details
 * The DOC peripheral is clock-gated by bit 6 of MSTPCRB (MSTPB6). Writing 0 to
 * this bit (under PRCR.PRC1 unlock) enables DOC; writing 1 stops the module.
 *
 * @par Manual Reference
 * RX72N HW Manual, Section 11.2.2 MSTPCRB, page 409: MSTPB6 = DOC.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief DOC module stop bit position in MSTPCRB (bit 6) */
  k_doc_mstpb_bit = 6,
} rx_doc_mstpb_bits_t;

/* =============================================================================
 * DOC Reserved Field Sizes
 * =============================================================================
 */

/**
 * @enum rx_doc_reserved_sizes_t
 * @brief DOC register block reserved field sizes
 * @details Number of reserved bytes used to pad the DOC register layout so the
 * compiler-generated struct matches the hardware memory map.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief Reserved byte between DOCR (0x00) and DODIR (0x02) */
  k_doc_reserved_after_docr_bytes = 1,
} rx_doc_reserved_sizes_t;

/* =============================================================================
 * DOC Register Structure
 * =============================================================================
 */

/**
 * @struct rx_doc_regs_t
 * @brief DOC register map structure
 *
 * @details
 * Packed volatile structure mirroring the DOC memory-mapped register layout.
 * Access via the doc() accessor returns a pointer to this structure at
 * k_doc_base_addr.
 *
 * @par Memory Layout (6 bytes total)
 * | Offset | Size | Field     | Type     | Description                    |
 * |--------|------|-----------|----------|--------------------------------|
 * | 0x00   | 1    | docr      | uint8_t  | DOC Control Register           |
 * | 0x01   | 1    | reserved0 | -        | Reserved                       |
 * | 0x02   | 2    | dodir     | uint16_t | Data Input Register            |
 * | 0x04   | 2    | dodsr     | uint16_t | Data Setting / Result Register |
 *
 * @invariant sizeof(rx_doc_regs_t) == 6
 * @invariant All offsets match RX72N HW Manual Chapter 59 register table
 *
 * @see doc() Accessor function
 * @see rx_doc_docr_bits_t DOCR bit field values
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed)) {
  /**
   * @brief DOC Control Register (DOCR) @ offset 0x00
   * @details Selects operating mode, detection condition, enables interrupt,
   * reports and clears the operation completion flag.
   * @see rx_doc_docr_bits_t Bit-field values for this register
   */
  volatile uint8_t docr;

  /** @brief Reserved byte @ offset 0x01 */
  uint8_t reserved0[k_doc_reserved_after_docr_bytes];

  /**
   * @brief Data Input Register (DODIR) @ offset 0x02
   * @details 16-bit operand register. Holds the compare reference, or the
   * second addend (for add mode), or the subtrahend (for subtract mode).
   * @note Writes to DODIR itself do not trigger the DOC operation.
   */
  volatile uint16_t dodir;

  /**
   * @brief Data Setting / Result Register (DODSR) @ offset 0x04
   * @details 16-bit operand register and result register:
   * - Compare mode: holds the comparison value
   * - Add mode: before write = initial accumulator; after write = accumulator + DODIR
   * - Subtract mode: before write = minuend; after write = minuend - DODIR
   * @note Writing DODSR is what triggers a DOC operation to execute.
   */
  volatile uint16_t dodsr;
} rx_doc_regs_t;

/* =============================================================================
 * DOC Register Accessor
 * =============================================================================
 */

/**
 * @brief Get pointer to DOC register block
 *
 * @details
 * Returns a volatile pointer to the DOC register structure mapped at
 * k_doc_base_addr (0x0008B080).
 *
 * @return Volatile pointer to DOC register structure (never nullptr)
 * @retval Non-NULL Always returns valid hardware pointer
 *
 * @pre Caller must have cleared MSTPCRB.MSTPB6 before accessing any register
 * @post Pointer is valid for program lifetime (hardware address is constant)
 *
 * @note Thread Safety: Safe (returns a constant hardware address)
 * @note Always inlined; zero overhead
 *
 * @par Example
 * @code{.c}
 * doc()->docr  = k_doc_oms_compare | k_doc_dcsel_match;
 * doc()->dodir = 0xCAFE;
 * doc()->dodsr = 0xCAFE;  // triggers DOC operation
 * @endcode
 *
 * @see k_doc_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_doc_regs_t* doc(void)
{
  return (volatile rx_doc_regs_t*)k_doc_base_addr;
}

/* =============================================================================
 * DOCR Bit Positions
 * =============================================================================
 */

/**
 * @enum rx_doc_docr_shifts_t
 * @brief DOCR bit-field positions
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_doc_docr_oms_shift     = 0, /**< OMS[1:0] position (bits 1:0) */
  k_doc_docr_dcsel_shift   = 2, /**< DCSEL position (bit 2)       */
  k_doc_docr_dopcie_shift  = 4, /**< DOPCIE position (bit 4)      */
  k_doc_docr_dopcf_shift   = 5, /**< DOPCF position (bit 5)       */
  k_doc_docr_dopcfcl_shift = 6, /**< DOPCFCL position (bit 6)     */
} rx_doc_docr_shifts_t;

/* =============================================================================
 * DOCR Bit Field Values
 * =============================================================================
 */

/**
 * @enum rx_doc_docr_bits_t
 * @brief DOCR bit-field values
 *
 * @details
 * Pre-shifted values for writing DOCR fields. Values may be bitwise-OR'd
 * together except that OMS fields are mutually exclusive.
 *
 * @par DOCR Layout (8-bit)
 * | Bit | Field   | Notes                                            |
 * |-----|---------|--------------------------------------------------|
 * | 1:0 | OMS     | 00=compare, 01=add, 10=subtract, 11=prohibited   |
 * | 2   | DCSEL   | 0=mismatch detect, 1=match detect (compare only) |
 * | 4   | DOPCIE  | 0=interrupt disabled, 1=enabled                  |
 * | 5   | DOPCF   | read-only completion flag                        |
 * | 6   | DOPCFCL | write 1 to clear DOPCF                           |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* OMS[1:0] - Operating Mode Select (bits 1:0) */
  k_doc_oms_compare  = (0x00 << k_doc_docr_oms_shift), /**< 16-bit compare       */
  k_doc_oms_add      = (0x01 << k_doc_docr_oms_shift), /**< 16-bit addition      */
  k_doc_oms_subtract = (0x02 << k_doc_docr_oms_shift), /**< 16-bit subtraction   */

  /* DCSEL - Detection Condition Select (bit 2, compare mode only) */
  k_doc_dcsel_mismatch = (0x00 << k_doc_docr_dcsel_shift), /**< flag on mismatch */
  k_doc_dcsel_match    = (0x01 << k_doc_docr_dcsel_shift), /**< flag on match    */

  /* DOPCIE - Interrupt Enable (bit 4) */
  k_doc_dopcie_disabled = (0x00 << k_doc_docr_dopcie_shift), /**< IRQ disabled */
  k_doc_dopcie_enabled  = (0x01 << k_doc_docr_dopcie_shift), /**< IRQ enabled  */

  /* DOPCF - Completion Flag (bit 5, read-only) */
  k_doc_docr_dopcf_mask = (0x01 << k_doc_docr_dopcf_shift), /**< DOPCF mask */

  /* DOPCFCL - DOPCF Clear (bit 6, write-1-to-clear) */
  k_doc_dopcfcl_clear = (0x01 << k_doc_docr_dopcfcl_shift), /**< write to clear DOPCF */

  /* Field masks for read-back validation */
  k_doc_docr_oms_mask    = (0x03 << k_doc_docr_oms_shift),
  k_doc_docr_dcsel_mask  = (0x01 << k_doc_docr_dcsel_shift),
  k_doc_docr_dopcie_mask = (0x01 << k_doc_docr_dopcie_shift),
} rx_doc_docr_bits_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

static_assert(k_doc_base_addr == 0x0008B080, "DOC base address incorrect (manual p.2969)");
static_assert(sizeof(rx_doc_regs_t) == 6, "DOC register structure size incorrect");
static_assert(offsetof(rx_doc_regs_t, docr) == 0x00, "DOCR offset incorrect");
static_assert(offsetof(rx_doc_regs_t, dodir) == 0x02, "DODIR offset incorrect");
static_assert(offsetof(rx_doc_regs_t, dodsr) == 0x04, "DODSR offset incorrect");
static_assert(k_doc_mstpb_bit == 6, "DOC MSTPCRB bit must be 6 (manual p.409)");

#ifdef __cplusplus
}
#endif
