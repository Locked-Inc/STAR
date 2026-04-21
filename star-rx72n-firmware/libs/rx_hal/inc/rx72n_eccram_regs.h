/**
 * @file rx72n_eccram_regs.h
 * @brief RX72N ECCRAM (ECC-protected RAM) Register Definitions
 *
 * @details
 * Register definitions for the 32 KB ECC-protected RAM region on the RX72N.
 * The ECCRAM supports Single-Error Correction / Double-Error Detection
 * (SEC-DED) Hamming-style ECC over every aligned 32-bit word: one detected
 * single-bit error is corrected on read, while any detected double-bit
 * error raises an interrupt so that software can refuse to act on corrupt
 * state (typically by e-stopping the system).
 *
 * @par Memory Map (RX72N HW Manual Chapter 60, Table 60.1)
 * | Region      | Start       | End         | Size   | Bus          |
 * |-------------|-------------|-------------|--------|--------------|
 * | ECCRAM      | 0x00FF8000  | 0x00FFFFFF  | 32 KB  | Memory Bus 3 |
 *
 * @par Control Register Block (Manual Chapter 60, sections 60.2.9-60.2.17)
 * | Offset | Register     | Size | Description                          |
 * |--------|--------------|------|--------------------------------------|
 * | 0x00   | ECCRAMMODE   | 8    | ECC mode select (disabled/correct/   |
 * |        |              |      | correct-and-detect/detect-only)      |
 * | 0x01   | ECCRAM2STS   | 8    | 2-bit error status flag              |
 * | 0x02   | ECCRAM1STSEN | 8    | 1-bit error info update enable       |
 * | 0x03   | ECCRAM1STS   | 8    | 1-bit error status flag              |
 * | 0x04   | ECCRAMPRCR   | 8    | Protection register (MODE/1STSEN)    |
 * | 0x08   | ECCRAM2ECAD  | 32   | 2-bit error address capture          |
 * | 0x0C   | ECCRAM1ECAD  | 32   | 1-bit error address capture          |
 * | 0x10   | ECCRAMPRCR2  | 8    | Protection register 2 (ETST)         |
 * | 0x14   | ECCRAMETST   | 8    | Test control (ECC decoder bypass)    |
 *
 * @par Initialization Sequence (Manual section 60.3.3)
 * Before enabling error checking, every 32-bit word of ECCRAM must be
 * written so that the ECC syndrome matches the data. Uninitialized ECC
 * bits are random and would falsely flag errors on first read. The driver
 * zeroes the entire region using 32-bit stores while the mode is set to
 * k_rx_eccrammode_ecc_no_check, then switches to the final mode.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 5: Static assertions verify all register offsets and base addresses
 * - Rule 8: All constants use C23 typed enums, no macros for numeric values
 * - Rule 9: Inline accessor (single-level pointer return), no function pointers
 *
 * @par Terminology
 * ECC = Error Correcting Code
 * SEC-DED = Single Error Correction, Double Error Detection
 * PRCR = Protection Register
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
 * Memory Region Addresses (Manual Ch60, Table 60.1, page 2977)
 * =============================================================================
 */

/**
 * @enum rx_eccram_region_addr_t
 * @brief ECCRAM data region start, end, and size
 *
 * @details
 * Physical address range of the ECC-protected RAM region. These are the
 * addresses that application code reads and writes -- the control
 * registers live at a separate base (see rx_eccram_reg_addr_t).
 *
 * @see RX72N HW Manual Chapter 60, Table 60.1 (RAM memory map), page 2977
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /** @brief ECCRAM region start address (inclusive) */
  k_rx_eccram_region_base_addr = 0x00FF8000,
  /** @brief ECCRAM region end address (inclusive) */
  k_rx_eccram_region_end_addr = 0x00FFFFFF,
} rx_eccram_region_addr_t;

/**
 * @enum rx_eccram_region_size_t
 * @brief ECCRAM region size in bytes
 *
 * @details
 * Total usable bytes in the ECC-protected region. The driver's
 * zero-initialization loop iterates over this many bytes in 32-bit
 * steps so that every ECC syndrome matches the data before error
 * checking is enabled.
 *
 * @see RX72N HW Manual Chapter 60, Table 60.1, page 2977
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief ECCRAM size in bytes (32 KB) */
  k_rx_eccram_region_size_bytes = 0x00008000,
} rx_eccram_region_size_t;

/* =============================================================================
 * Control Register Base Address (Manual Ch60, section 60.2, page 2979)
 * =============================================================================
 */

/**
 * @enum rx_eccram_reg_base_t
 * @brief ECCRAM control register block base address
 *
 * @see RX72N HW Manual Chapter 60, section 60.2 (register list), page 2979
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /** @brief ECCRAM control register block base */
  k_rx_eccram_reg_base_addr = 0x000812C0,
} rx_eccram_reg_base_t;

/* =============================================================================
 * Register Offsets (Manual Ch60, sections 60.2.9-60.2.17, pages 2981-2990)
 * =============================================================================
 */

/**
 * @enum rx_eccram_reg_offset_t
 * @brief Offsets of ECCRAM control registers from k_rx_eccram_reg_base_addr
 *
 * @see RX72N HW Manual Chapter 60, sections 60.2.9-60.2.17, pages 2981-2990
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief ECCRAMMODE   @ 0x000812C0 (Ch60, section 60.2.9,  page 2981) */
  k_rx_eccram_offset_eccrammode = 0x00,
  /** @brief ECCRAM2STS   @ 0x000812C1 (Ch60, section 60.2.10, page 2982) */
  k_rx_eccram_offset_eccram2sts = 0x01,
  /** @brief ECCRAM1STSEN @ 0x000812C2 (Ch60, section 60.2.11, page 2983) */
  k_rx_eccram_offset_eccram1stsen = 0x02,
  /** @brief ECCRAM1STS   @ 0x000812C3 (Ch60, section 60.2.12, page 2984) */
  k_rx_eccram_offset_eccram1sts = 0x03,
  /** @brief ECCRAMPRCR   @ 0x000812C4 (Ch60, section 60.2.13, page 2985) */
  k_rx_eccram_offset_eccramprcr = 0x04,
  /** @brief ECCRAM2ECAD  @ 0x000812C8 (Ch60, section 60.2.14, page 2986) */
  k_rx_eccram_offset_eccram2ecad = 0x08,
  /** @brief ECCRAM1ECAD  @ 0x000812CC (Ch60, section 60.2.15, page 2987) */
  k_rx_eccram_offset_eccram1ecad = 0x0C,
  /** @brief ECCRAMPRCR2  @ 0x000812D0 (Ch60, section 60.2.16, page 2989) */
  k_rx_eccram_offset_eccramprcr2 = 0x10,
  /** @brief ECCRAMETST   @ 0x000812D4 (Ch60, section 60.2.17, page 2990) */
  k_rx_eccram_offset_eccrametst = 0x14,
} rx_eccram_reg_offset_t;

/* =============================================================================
 * ECCRAMMODE bit definitions (Manual Ch60, section 60.2.9, page 2981)
 * =============================================================================
 */

/**
 * @enum rx_eccrammode_bits_t
 * @brief ECCRAMMODE operating mode select values (RAMMOD[1:0])
 *
 * @details
 * Selects whether ECC generation and checking are active on ECCRAM
 * accesses. The recommended mode for production is
 * k_rx_eccrammode_ecc_with_check (correct-and-detect).
 *
 * @see RX72N HW Manual Chapter 60, section 60.2.9, page 2981
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief ECC disabled (RAMMOD[1:0] = 00b) */
  k_rx_eccrammode_ecc_disabled = 0x00,
  /** @brief Setting prohibited (RAMMOD[1:0] = 01b) -- do not use */
  k_rx_eccrammode_prohibited = 0x01,
  /** @brief ECC generated, no error checking (RAMMOD[1:0] = 10b) */
  k_rx_eccrammode_ecc_no_check = 0x02,
  /** @brief ECC generated with 1-bit correction and 2-bit detection (RAMMOD[1:0] = 11b) */
  k_rx_eccrammode_ecc_with_check = 0x03,
  /** @brief RAMMOD[1:0] field mask */
  k_rx_eccrammode_rammod_mask = 0x03,
} rx_eccrammode_bits_t;

/* =============================================================================
 * ECCRAM2STS bit definitions (Manual Ch60, section 60.2.10, page 2982)
 * =============================================================================
 */

/**
 * @enum rx_eccram2sts_bits_t
 * @brief ECCRAM2STS 2-bit error flag (ECC2ERR)
 *
 * @details
 * Set to 1 by hardware when a 2-bit error is detected on an ECCRAM read.
 * Cleared by writing 0 to bit 0. While this flag is set, ECCRAM2ECAD
 * holds the failing address.
 *
 * @see RX72N HW Manual Chapter 60, section 60.2.10, page 2982
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief ECC2ERR bit position */
  k_rx_eccram2sts_ecc2err_shift = 0,
  /** @brief ECC2ERR mask */
  k_rx_eccram2sts_ecc2err_mask = 0x01,
  /** @brief Value to clear ECC2ERR (write 0 to bit 0) */
  k_rx_eccram2sts_clear = 0x00,
} rx_eccram2sts_bits_t;

/* =============================================================================
 * ECCRAM1STSEN bit definitions (Manual Ch60, section 60.2.11, page 2983)
 * =============================================================================
 */

/**
 * @enum rx_eccram1stsen_bits_t
 * @brief ECCRAM1STSEN 1-bit error info update enable (ECC1STSEN)
 *
 * @see RX72N HW Manual Chapter 60, section 60.2.11, page 2983
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief ECC1STSEN bit position */
  k_rx_eccram1stsen_ecc1stsen_shift = 0,
  /** @brief ECC1STSEN mask */
  k_rx_eccram1stsen_ecc1stsen_mask = 0x01,
  /** @brief 1-bit error status updates disabled */
  k_rx_eccram1stsen_disable = 0x00,
  /** @brief 1-bit error status updates enabled */
  k_rx_eccram1stsen_enable = 0x01,
} rx_eccram1stsen_bits_t;

/* =============================================================================
 * ECCRAM1STS bit definitions (Manual Ch60, section 60.2.12, page 2984)
 * =============================================================================
 */

/**
 * @enum rx_eccram1sts_bits_t
 * @brief ECCRAM1STS 1-bit error flag (ECC1ERR)
 *
 * @details
 * Set to 1 by hardware when a correctable 1-bit error is detected.
 * Cleared by writing 0 to bit 0. While this flag is set, ECCRAM1ECAD
 * holds the failing address.
 *
 * @see RX72N HW Manual Chapter 60, section 60.2.12, page 2984
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief ECC1ERR bit position */
  k_rx_eccram1sts_ecc1err_shift = 0,
  /** @brief ECC1ERR mask */
  k_rx_eccram1sts_ecc1err_mask = 0x01,
  /** @brief Value to clear ECC1ERR (write 0 to bit 0) */
  k_rx_eccram1sts_clear = 0x00,
} rx_eccram1sts_bits_t;

/* =============================================================================
 * ECCRAMPRCR bit definitions (Manual Ch60, section 60.2.13, page 2985)
 * =============================================================================
 */

/**
 * @enum rx_eccramprcr_bits_t
 * @brief ECCRAMPRCR protection register values
 *
 * @details
 * Write-enables ECCRAMMODE and ECCRAM1STSEN. Bits [7:1] are the key
 * KW[6:0] = 1111000b; bit [0] is PRCR. Write 0xF1 to unlock (key + PRCR=1),
 * 0xF0 to lock (key + PRCR=0). Writes with any other key pattern are
 * ignored.
 *
 * @see RX72N HW Manual Chapter 60, section 60.2.13, page 2985
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief PRCR bit position */
  k_rx_eccramprcr_prcr_shift = 0,
  /** @brief PRCR mask */
  k_rx_eccramprcr_prcr_mask = 0x01,
  /** @brief KW[6:0] = 1111000b in bits [7:1] (mandatory key) */
  k_rx_eccramprcr_key = 0xF0,
  /** @brief Full value to unlock writes to ECCRAMMODE / ECCRAM1STSEN */
  k_rx_eccramprcr_unlock = 0xF1,
  /** @brief Full value to re-lock writes */
  k_rx_eccramprcr_lock = 0xF0,
} rx_eccramprcr_bits_t;

/* =============================================================================
 * ECCRAMPRCR2 bit definitions (Manual Ch60, section 60.2.16, page 2989)
 * =============================================================================
 */

/**
 * @enum rx_eccramprcr2_bits_t
 * @brief ECCRAMPRCR2 protection register 2 values (guards ECCRAMETST)
 *
 * @see RX72N HW Manual Chapter 60, section 60.2.16, page 2989
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief PRCR2 bit position */
  k_rx_eccramprcr2_prcr2_shift = 0,
  /** @brief PRCR2 mask */
  k_rx_eccramprcr2_prcr2_mask = 0x01,
  /** @brief KW2[6:0] = 1111000b in bits [7:1] */
  k_rx_eccramprcr2_key = 0xF0,
  /** @brief Full value to unlock writes to ECCRAMETST */
  k_rx_eccramprcr2_unlock = 0xF1,
  /** @brief Full value to re-lock */
  k_rx_eccramprcr2_lock = 0xF0,
} rx_eccramprcr2_bits_t;

/* =============================================================================
 * ECCRAMETST bit definitions (Manual Ch60, section 60.2.17, page 2990)
 * =============================================================================
 */

/**
 * @enum rx_eccrametst_bits_t
 * @brief ECCRAMETST ECC decoder bypass select (TSTBYP) for test mode
 *
 * @see RX72N HW Manual Chapter 60, section 60.2.17, page 2990
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief TSTBYP bit position */
  k_rx_eccrametst_tstbyp_shift = 0,
  /** @brief TSTBYP mask */
  k_rx_eccrametst_tstbyp_mask = 0x01,
  /** @brief Normal operation (ECC decoder active) */
  k_rx_eccrametst_normal = 0x00,
  /** @brief Bypass ECC decoder (for HW self-test only) */
  k_rx_eccrametst_bypass = 0x01,
} rx_eccrametst_bits_t;

/* =============================================================================
 * MSTPCRC bit for ECCRAM module stop (Manual Ch60, section 60.4.1, page 2994)
 * =============================================================================
 */

/**
 * @enum rx_eccram_mstpcrc_bits_t
 * @brief MSTPCRC.MSTPC6 bit position for ECCRAM module-stop control
 *
 * @details
 * Setting MSTPC6 = 1 stops the clock supply to the ECCRAM control logic
 * and the region becomes inaccessible. It must be cleared (bit = 0)
 * before any ECCRAM access -- including the zero-initialization pass.
 * The write requires the main system PRCR to be in the "PRC1 unlocked"
 * state (see k_rx_prcr_unlock_prc1 in rx_register_protection.h).
 *
 * @see RX72N HW Manual Chapter 60, section 60.4.1 (MSTPCRC), page 2994
 * @see RX72N HW Manual Chapter 11 (Low Power Consumption)
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Bit position of ECCRAM module-stop in MSTPCRC */
  k_rx_eccram_mstpcrc_bit_shift = 6,
  /** @brief Bit mask for MSTPCRC.MSTPC6 */
  k_rx_eccram_mstpcrc_bit_mask = (1U << 6),
} rx_eccram_mstpcrc_bits_t;

/* =============================================================================
 * Register Structure
 * =============================================================================
 */

/**
 * @struct rx_eccram_regs_t
 * @brief ECCRAM control register block
 *
 * @details
 * Memory-mapped layout of the ECCRAM control registers starting at
 * k_rx_eccram_reg_base_addr (0x000812C0). Reserved gaps preserve the
 * hardware layout exactly as described in the manual.
 *
 * @invariant sizeof(rx_eccram_regs_t) == 24 bytes (21 documented + 3 tail padding)
 * @invariant All field offsets match RX72N HW Manual Chapter 60, sections 60.2.9-60.2.17
 *
 * @see eccram_regs() Accessor that returns a pointer to this block at its hardware address
 * @see RX72N HW Manual Chapter 60, sections 60.2.9-60.2.17, pages 2981-2990
 * @since Version 1.0.0
 */
typedef struct {
  /** @brief ECCRAMMODE   @ 0x00 - ECC operating mode select (Ch60, section 60.2.9,  page 2981) */
  volatile uint8_t eccrammode;
  /** @brief ECCRAM2STS   @ 0x01 - 2-bit error status flag   (Ch60, section 60.2.10, page 2982) */
  volatile uint8_t eccram2sts;
  /** @brief ECCRAM1STSEN @ 0x02 - 1-bit info update enable  (Ch60, section 60.2.11, page 2983) */
  volatile uint8_t eccram1stsen;
  /** @brief ECCRAM1STS   @ 0x03 - 1-bit error status flag   (Ch60, section 60.2.12, page 2984) */
  volatile uint8_t eccram1sts;
  /** @brief ECCRAMPRCR   @ 0x04 - Protection register       (Ch60, section 60.2.13, page 2985) */
  volatile uint8_t eccramprcr;
  /** @brief Reserved     @ 0x05-0x07 */
  uint8_t reserved0[3];
  /** @brief ECCRAM2ECAD  @ 0x08 - 2-bit error address       (Ch60, section 60.2.14, page 2986) */
  volatile uint32_t eccram2ecad;
  /** @brief ECCRAM1ECAD  @ 0x0C - 1-bit error address       (Ch60, section 60.2.15, page 2987) */
  volatile uint32_t eccram1ecad;
  /** @brief ECCRAMPRCR2  @ 0x10 - Protection register 2     (Ch60, section 60.2.16, page 2989) */
  volatile uint8_t eccramprcr2;
  /** @brief Reserved     @ 0x11-0x13 */
  uint8_t reserved1[3];
  /** @brief ECCRAMETST   @ 0x14 - Test control              (Ch60, section 60.2.17, page 2990) */
  volatile uint8_t eccrametst;
} rx_eccram_regs_t;

/* =============================================================================
 * Inline Register Accessor
 * =============================================================================
 */

/**
 * @brief Get pointer to the ECCRAM control register block
 *
 * @details
 * Returns a volatile pointer to the ECCRAM control registers at their
 * fixed hardware address. This is the sole, NASA-Rule-8-compliant way
 * to reach the registers -- no macros are used for addresses.
 *
 * @return Volatile pointer to rx_eccram_regs_t
 * @retval Non-NULL Always returns a valid pointer to k_rx_eccram_reg_base_addr
 *
 * @pre None (hardware register address is always mapped)
 * @post Pointer is valid for the lifetime of program execution
 *
 * @note Thread Safety: Safe. Returns a constant hardware address with no
 *       shared state. Concurrent register reads/writes through the pointer
 *       are the caller's concern.
 *
 * @par Example:
 * @code{.c}
 * eccram_regs()->eccramprcr   = k_rx_eccramprcr_unlock;
 * eccram_regs()->eccrammode   = k_rx_eccrammode_ecc_with_check;
 * eccram_regs()->eccramprcr   = k_rx_eccramprcr_lock;
 * @endcode
 *
 * @see k_rx_eccram_reg_base_addr
 * @since Version 1.0.0
 */
static inline volatile rx_eccram_regs_t* eccram_regs(void)
{
  return (volatile rx_eccram_regs_t*)k_rx_eccram_reg_base_addr;
}

/* =============================================================================
 * Static Assertions -- Layout Verification
 * =============================================================================
 */

static_assert(sizeof(rx_eccram_regs_t) == 24,
              "rx_eccram_regs_t must be 24 bytes (21 documented + 3 tail padding)");

static_assert(offsetof(rx_eccram_regs_t, eccrammode) == k_rx_eccram_offset_eccrammode,
              "ECCRAMMODE offset must be 0x00");
static_assert(offsetof(rx_eccram_regs_t, eccram2sts) == k_rx_eccram_offset_eccram2sts,
              "ECCRAM2STS offset must be 0x01");
static_assert(offsetof(rx_eccram_regs_t, eccram1stsen) == k_rx_eccram_offset_eccram1stsen,
              "ECCRAM1STSEN offset must be 0x02");
static_assert(offsetof(rx_eccram_regs_t, eccram1sts) == k_rx_eccram_offset_eccram1sts,
              "ECCRAM1STS offset must be 0x03");
static_assert(offsetof(rx_eccram_regs_t, eccramprcr) == k_rx_eccram_offset_eccramprcr,
              "ECCRAMPRCR offset must be 0x04");
static_assert(offsetof(rx_eccram_regs_t, eccram2ecad) == k_rx_eccram_offset_eccram2ecad,
              "ECCRAM2ECAD offset must be 0x08");
static_assert(offsetof(rx_eccram_regs_t, eccram1ecad) == k_rx_eccram_offset_eccram1ecad,
              "ECCRAM1ECAD offset must be 0x0C");
static_assert(offsetof(rx_eccram_regs_t, eccramprcr2) == k_rx_eccram_offset_eccramprcr2,
              "ECCRAMPRCR2 offset must be 0x10");
static_assert(offsetof(rx_eccram_regs_t, eccrametst) == k_rx_eccram_offset_eccrametst,
              "ECCRAMETST offset must be 0x14");

static_assert(k_rx_eccram_reg_base_addr == 0x000812C0,
              "ECCRAM control register base must be 0x000812C0");
static_assert(k_rx_eccram_region_base_addr == 0x00FF8000, "ECCRAM region base must be 0x00FF8000");
static_assert(k_rx_eccram_region_end_addr == 0x00FFFFFF, "ECCRAM region end must be 0x00FFFFFF");
static_assert(k_rx_eccram_region_size_bytes == 0x00008000,
              "ECCRAM region size must be 32 KB (0x8000)");
static_assert(k_rx_eccramprcr_unlock == 0xF1, "ECCRAMPRCR unlock must be 0xF1");
static_assert(k_rx_eccramprcr2_unlock == 0xF1, "ECCRAMPRCR2 unlock must be 0xF1");
static_assert(k_rx_eccram_mstpcrc_bit_shift == 6, "MSTPCRC.MSTPC6 must be bit 6");

#ifdef __cplusplus
}
#endif
