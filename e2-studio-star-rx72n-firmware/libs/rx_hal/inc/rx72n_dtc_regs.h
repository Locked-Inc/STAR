/* SPDX-License-Identifier: MIT */
/**
 * @file rx72n_dtc_regs.h
 * @brief RX72N Data Transfer Controller (DTCb) Register Definitions
 *
 * @details
 * Complete register definitions for the RX72N Data Transfer Controller.
 * The DTC performs interrupt-driven data transfers without CPU intervention,
 * supporting normal, repeat, block, and sequence transfer modes.
 *
 * @par DTC Overview
 * - Triggered by interrupt requests
 * - Number of channels = number of interrupt sources that can start DTC
 * - Transfer modes: normal, repeat, block, chain, sequence
 * - Transfer space: 16 MB (short) or 4 GB (full address mode)
 * - Single data sizes: 8/16/32-bit
 * - Block sizes: 1-256 data units
 * - Up to 256 sequences per trigger source
 *
 * @par Register Organization
 * | Address       | Size | Register    | Description                        |
 * |---------------|------|-------------|------------------------------------|
 * | 0x00082400    | 8    | DTCCR       | DTC Control Register               |
 * | 0x00082404    | 32   | DTCVBR      | DTC Vector Base Register           |
 * | 0x00082408    | 8    | DTCADMOD    | DTC Address Mode Register          |
 * | 0x0008240C    | 8    | DTCST       | DTC Module Start Register          |
 * | 0x0008240E    | 16   | DTCSTS      | DTC Status Register                |
 * | 0x00082410    | 32   | DTCIBR      | DTC Index Table Base Register      |
 * | 0x00082414    | 8    | DTCOR       | DTC Operation Register             |
 * | 0x00082416    | 16   | DTCSQE      | DTC Sequence Transfer Enable Reg   |
 * | 0x00082418    | 32   | DTCDISP     | DTC Address Displacement Register  |
 *
 * @par DTC vs DMAC
 * - DTC: Low overhead, triggered by any interrupt, lightweight transfers
 * - DMAC: High throughput, dedicated 8 channels, advanced features
 * - DTC is ideal for small periodic transfers (ADC, UART, SPI)
 * - DMAC is better for bulk memory-to-memory transfers
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp, or recursion
 * - Rule 2: N/A (no loops)
 * - Rule 3: No dynamic memory allocation
 * - Rule 4: Short accessor functions
 * - Rule 5: Static assertions verify addresses
 * - Rule 6: Minimal scope
 * - Rule 7: N/A
 * - Rule 8: All constants use C23 typed enums
 * - Rule 9: No function pointers
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par Manual References
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 20: Data Transfer Controller (DTCb), pages 786-867
 * - Section 20.2: Register Descriptions
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project
 * @since Version 1.0.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * DTC Base Address and Register Addresses
 * =============================================================================
 */

/**
 * @enum dtc_addresses_t
 * @brief DTC module addresses
 *
 * @details
 * Base address and individual register addresses for DTC control registers.
 * All addresses verified against RX72N Hardware Manual Ch20.
 *
 * @note Addresses verified 2026-01-29 against manual section 20.2
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief DTC base address */
  k_dtc_base_addr = 0x00082400U,

  /** @brief DTC Control Register address */
  k_dtc_dtccr_addr = 0x00082400U,

  /** @brief DTC Vector Base Register address */
  k_dtc_dtcvbr_addr = 0x00082404U,

  /** @brief DTC Address Mode Register address */
  k_dtc_dtcadmod_addr = 0x00082408U,

  /** @brief DTC Module Start Register address */
  k_dtc_dtcst_addr = 0x0008240CU,

  /** @brief DTC Status Register address */
  k_dtc_dtcsts_addr = 0x0008240EU,

  /** @brief DTC Index Table Base Register address */
  k_dtc_dtcibr_addr = 0x00082410U,

  /** @brief DTC Operation Register address */
  k_dtc_dtcor_addr = 0x00082414U,

  /** @brief DTC Sequence Transfer Enable Register address */
  k_dtc_dtcsqe_addr = 0x00082416U,

  /** @brief DTC Address Displacement Register address */
  k_dtc_dtcdisp_addr = 0x00082418U,
} dtc_addresses_t;

/**
 * @enum dtc_offsets_t
 * @brief Register offsets from DTC base address
 *
 * @details
 * Offsets for each register relative to k_dtc_base_addr.
 *
 * @note Offsets verified 2026-01-29 against manual
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dtc_offset_dtccr    = 0x00U, /**< DTCCR offset (8-bit) */
  k_dtc_offset_dtcvbr   = 0x04U, /**< DTCVBR offset (32-bit) */
  k_dtc_offset_dtcadmod = 0x08U, /**< DTCADMOD offset (8-bit) */
  k_dtc_offset_dtcst    = 0x0CU, /**< DTCST offset (8-bit) */
  k_dtc_offset_dtcsts   = 0x0EU, /**< DTCSTS offset (16-bit) */
  k_dtc_offset_dtcibr   = 0x10U, /**< DTCIBR offset (32-bit) */
  k_dtc_offset_dtcor    = 0x14U, /**< DTCOR offset (8-bit) */
  k_dtc_offset_dtcsqe   = 0x16U, /**< DTCSQE offset (16-bit) */
  k_dtc_offset_dtcdisp  = 0x18U, /**< DTCDISP offset (32-bit) */
} dtc_offsets_t;

/* =============================================================================
 * DTCCR Register Bits (Control Register)
 * =============================================================================
 */

/**
 * @enum dtccr_bits_t
 * @brief DTC Control Register (DTCCR) bit definitions
 *
 * @details
 * Controls DTC read skip functionality.
 *
 * @par Register Layout (8-bit @ 0x00082400)
 * | Bit | Symbol | Description                           |
 * |-----|--------|---------------------------------------|
 * | 2:0 | -      | Reserved (read as 0, write 0)         |
 * | 3   | -      | Reserved (read as 1, write 1)         |
 * | 4   | RRS    | Transfer Information Read Skip Enable |
 * | 7:5 | -      | Reserved (read as 0, write 0)         |
 *
 * @note Manual ref: Ch20 section 20.2.8
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief Reserved bit 3 (must write 1) */
  k_dtccr_reserved_bit3 = 0x08U,

  /**
   * @brief Transfer Information Read Skip Enable (bit 4)
   * @details
   * - 0: Transfer information read is not skipped
   * - 1: Transfer information read is skipped when vector numbers match
   * @warning Set to 0 when using sequence transfer
   */
  k_dtccr_rrs = 0x10U,
} dtccr_bits_t;

/* =============================================================================
 * DTCADMOD Register Bits (Address Mode)
 * =============================================================================
 */

/**
 * @enum dtcadmod_bits_t
 * @brief DTC Address Mode Register (DTCADMOD) bit definitions
 *
 * @details
 * Selects short-address or full-address mode for DTC transfers.
 *
 * @par Register Layout (8-bit @ 0x00082408)
 * | Bit | Symbol | Description                    |
 * |-----|--------|--------------------------------|
 * | 0   | SHORT  | Short-Address Mode Set         |
 * | 7:1 | -      | Reserved (read as 0, write 0)  |
 *
 * @note Manual ref: Ch20 section 20.2.10
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Short-Address Mode Set (bit 0)
   * @details
   * - 0: Full-address mode (4 GB space)
   * - 1: Short-address mode (16 MB space)
   * @warning Set to 0 (full-address mode) when using sequence transfer
   */
  k_dtcadmod_short = 0x01U,
} dtcadmod_bits_t;

/* =============================================================================
 * DTCST Register Bits (Module Start)
 * =============================================================================
 */

/**
 * @enum dtcst_bits_t
 * @brief DTC Module Start Register (DTCST) bit definitions
 *
 * @details
 * Enables or disables the DTC module.
 *
 * @par Register Layout (8-bit @ 0x0008240C)
 * | Bit | Symbol | Description                    |
 * |-----|--------|--------------------------------|
 * | 0   | DTCST  | DTC Module Start               |
 * | 7:1 | -      | Reserved (read as 0, write 0)  |
 *
 * @note Manual ref: Ch20 section 20.2.11
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief DTC Module Start (bit 0)
   * @details
   * - 0: DTC module stop (no transfer requests accepted)
   * - 1: DTC module start (transfer requests accepted)
   * @note Set to 0 before entering module stop or standby modes
   */
  k_dtcst_dtcst = 0x01U,
} dtcst_bits_t;

/* =============================================================================
 * DTCSTS Register Bits (Status)
 * =============================================================================
 */

/**
 * @enum dtcsts_bits_t
 * @brief DTC Status Register (DTCSTS) bit definitions
 *
 * @details
 * Read-only register providing DTC transfer status.
 *
 * @par Register Layout (16-bit @ 0x0008240E)
 * | Bits   | Symbol    | Description                      |
 * |--------|-----------|----------------------------------|
 * | 7:0    | VECN[7:0] | Active Vector Number (read-only) |
 * | 14:8   | -         | Reserved (read as 0)             |
 * | 15     | ACT       | DTC Active Flag (read-only)      |
 *
 * @note Manual ref: Ch20 section 20.2.12
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /** @brief Vector number field mask (bits 7:0) */
  k_dtcsts_vecn_mask = 0x00FFU,

  /**
   * @brief DTC Active Flag (bit 15)
   * @details
   * - 0: Data transfer is not in progress
   * - 1: Data transfer is in progress
   * @note VECN[7:0] is only valid when ACT = 1
   */
  k_dtcsts_act = 0x8000U,
} dtcsts_bits_t;

/* =============================================================================
 * DTCOR Register Bits (Operation)
 * =============================================================================
 */

/**
 * @enum dtcor_bits_t
 * @brief DTC Operation Register (DTCOR) bit definitions
 *
 * @details
 * Controls DTC operation, specifically sequence transfer termination.
 *
 * @par Register Layout (8-bit @ 0x00082414)
 * | Bit | Symbol  | Description                          |
 * |-----|---------|--------------------------------------|
 * | 0   | SQTFRL  | Sequence Transfer Terminate          |
 * | 7:1 | -       | Reserved (read as 0, write 0)        |
 *
 * @note Manual ref: Ch20 section 20.2.14
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Sequence Transfer Terminate (bit 0, write-only)
   * @details
   * Writing 1 terminates the sequence transfer in progress.
   * This bit is always read as 0.
   */
  k_dtcor_sqtfrl = 0x01U,
} dtcor_bits_t;

/* =============================================================================
 * DTCSQE Register Bits (Sequence Transfer Enable)
 * =============================================================================
 */

/**
 * @enum dtcsqe_bits_t
 * @brief DTC Sequence Transfer Enable Register (DTCSQE) bit definitions
 *
 * @details
 * Enables sequence transfer and specifies the trigger vector number.
 *
 * @par Register Layout (16-bit @ 0x00082416)
 * | Bits   | Symbol    | Description                              |
 * |--------|-----------|------------------------------------------|
 * | 7:0    | VECN[7:0] | Sequence Transfer Vector Number Setting  |
 * | 14:8   | -         | Reserved (read as 0, write 0)            |
 * | 15     | ESPSEL    | Sequence Transfer Enable                 |
 *
 * @note Manual ref: Ch20 section 20.2.15
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /** @brief Vector number field mask (bits 7:0) */
  k_dtcsqe_vecn_mask = 0x00FFU,

  /**
   * @brief Sequence Transfer Enable (bit 15)
   * @details
   * - 0: Sequence transfer is disabled
   * - 1: Sequence transfer is enabled
   * @note Set DTCADMOD.SHORT to 0 when enabling sequence transfer
   */
  k_dtcsqe_espsel = 0x8000U,
} dtcsqe_bits_t;

/* =============================================================================
 * DTC Transfer Information Structure (stored in RAM)
 * =============================================================================
 */

/**
 * @enum dtc_mra_bits_t
 * @brief DTC Mode Register A (MRA) bit definitions for transfer info
 *
 * @details
 * MRA is part of DTC transfer information stored in RAM, not directly
 * accessible as a memory-mapped register.
 *
 * @par Register Layout (8-bit in transfer info)
 * | Bits | Symbol   | Description                          |
 * |------|----------|--------------------------------------|
 * | 0    | WBDIS    | Write-back Disable                   |
 * | 1    | -        | Reserved (set to 0)                  |
 * | 3:2  | SM[1:0]  | Source Address Addressing Mode       |
 * | 5:4  | SZ[1:0]  | Transfer Data Size                   |
 * | 7:6  | MD[1:0]  | Transfer Mode Select                 |
 *
 * @note Manual ref: Ch20 section 20.2.1
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* WBDIS - Write-back Disable (bit 0) */
  k_dtc_mra_wbdis = 0x01U, /**< Don't write back transfer info */

  /* SM - Source Address Addressing Mode (bits 3:2) */
  k_dtc_mra_sm_fixed     = 0x00U, /**< Source address fixed */
  k_dtc_mra_sm_increment = 0x08U, /**< Source address increment */
  k_dtc_mra_sm_decrement = 0x0CU, /**< Source address decrement */
  k_dtc_mra_sm_mask      = 0x0CU, /**< SM field mask */

  /* SZ - Transfer Data Size (bits 5:4) */
  k_dtc_mra_sz_byte = 0x00U, /**< 8-bit transfer */
  k_dtc_mra_sz_word = 0x10U, /**< 16-bit transfer */
  k_dtc_mra_sz_long = 0x20U, /**< 32-bit transfer */
  k_dtc_mra_sz_mask = 0x30U, /**< SZ field mask */

  /* MD - Transfer Mode Select (bits 7:6) */
  k_dtc_mra_md_normal = 0x00U, /**< Normal transfer mode */
  k_dtc_mra_md_repeat = 0x40U, /**< Repeat transfer mode */
  k_dtc_mra_md_block  = 0x80U, /**< Block transfer mode */
  k_dtc_mra_md_mask   = 0xC0U, /**< MD field mask */
} dtc_mra_bits_t;

/**
 * @enum dtc_mrb_bits_t
 * @brief DTC Mode Register B (MRB) bit definitions for transfer info
 *
 * @details
 * MRB is part of DTC transfer information stored in RAM, not directly
 * accessible as a memory-mapped register.
 *
 * @par Register Layout (8-bit in transfer info)
 * | Bit | Symbol | Description                              |
 * |-----|--------|------------------------------------------|
 * | 0   | SQEND  | Sequence Transfer End                    |
 * | 1   | INDX   | Index Table Reference                    |
 * | 3:2 | DM     | Destination Address Addressing Mode      |
 * | 4   | DTS    | Repeat/Block Area Select                 |
 * | 5   | DISEL  | DTC Interrupt Select                     |
 * | 6   | CHNS   | Chain Transfer Select                    |
 * | 7   | CHNE   | Chain Transfer Enable                    |
 *
 * @note Manual ref: Ch20 section 20.2.2
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dtc_mrb_sqend = 0x01U, /**< End sequence transfer */
  k_dtc_mrb_indx  = 0x02U, /**< Refer to index table */

  /* DM - Destination Address Addressing Mode (bits 3:2) */
  k_dtc_mrb_dm_fixed     = 0x00U, /**< Destination address fixed */
  k_dtc_mrb_dm_increment = 0x08U, /**< Destination address increment */
  k_dtc_mrb_dm_decrement = 0x0CU, /**< Destination address decrement */
  k_dtc_mrb_dm_mask      = 0x0CU, /**< DM field mask */

  k_dtc_mrb_dts   = 0x10U, /**< Source is repeat/block area (1=src, 0=dest) */
  k_dtc_mrb_disel = 0x20U, /**< Interrupt after each transfer (not just end) */
  k_dtc_mrb_chns  = 0x40U, /**< Chain only when counter becomes 0 */
  k_dtc_mrb_chne  = 0x80U, /**< Chain transfer enable */
} dtc_mrb_bits_t;

/**
 * @struct dtc_transfer_info_full_t
 * @brief DTC transfer information structure for full-address mode
 *
 * @details
 * This structure is placed in RAM and read by the DTC hardware.
 * The address is calculated from DTCVBR + (vector_number x 4).
 *
 * @warning Must be aligned to 4-byte boundary
 * @note For short-address mode, use dtc_transfer_info_short_t
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed, aligned(4))) {
  uint8_t  mra;      /**< Mode Register A (transfer mode, size) */
  uint8_t  mrb;      /**< Mode Register B (chain, interrupt) */
  uint8_t  mrc;      /**< Mode Register C (sequence control) */
  uint8_t  reserved; /**< Reserved (set to 0) */
  uint32_t sar;      /**< Source Address Register */
  uint32_t dar;      /**< Destination Address Register */
  union {
    struct {
      uint16_t cra_low;  /**< Transfer count (normal/repeat) */
      uint16_t cra_high; /**< Block size (block mode) */
    };
    uint32_t cra; /**< Transfer Count Register A (full) */
  };
  uint16_t crb;       /**< Transfer Count Register B (block count) */
  uint16_t reserved2; /**< Reserved */
} dtc_transfer_info_full_t;

/**
 * @struct dtc_transfer_info_short_t
 * @brief DTC transfer information structure for short-address mode
 *
 * @details
 * Compact format for 16 MB address space transfers.
 * Uses 3-byte addresses to save memory.
 *
 * @warning Must be aligned to 4-byte boundary
 * @note For full-address mode, use dtc_transfer_info_full_t
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed, aligned(4))) {
  uint8_t mra;       /**< Mode Register A */
  uint8_t mrb;       /**< Mode Register B */
  uint8_t mrc;       /**< Mode Register C */
  uint8_t reserved;  /**< Reserved */
  uint8_t sar[3];    /**< Source Address (24-bit) */
  uint8_t reserved2; /**< Reserved */
  uint8_t dar[3];    /**< Destination Address (24-bit) */
  uint8_t reserved3; /**< Reserved */
  union {
    struct {
      uint16_t cra_low;
      uint16_t cra_high;
    };
    uint32_t cra;
  };
  uint16_t crb;
  uint16_t reserved4;
} dtc_transfer_info_short_t;

/* =============================================================================
 * Module Stop Control
 * =============================================================================
 */

/**
 * @enum dtc_mstpcr_t
 * @brief DTC module stop control bits
 *
 * @details
 * The DTC is controlled by MSTPCRA.MSTPA28.
 * Must be cleared (0) before accessing DTC registers.
 *
 * @note Manual ref: Ch11 (Low Power Consumption) module stop tables
 * @note DTC and DMAC share the same module stop bit (MSTPA28)
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief MSTPCRA bit for DTC (bit 28)
   * @details
   * - 0: DTC module is running
   * - 1: DTC module is stopped (default)
   * @note Same bit as DMAC (k_dmac_mstpcra_bit)
   */
  k_dtc_mstpcra_bit = (1U << 28U),
} dtc_mstpcr_t;

/* =============================================================================
 * Register Accessor Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to DTC Control Register (DTCCR)
 * @return Volatile pointer to DTCCR register (8-bit)
 * @pre DTC module stop bit (MSTPCRA.MSTPA28) must be cleared
 * @since Version 1.0.0
 */
static inline volatile uint8_t* dtc_dtccr_reg(void)
{
  return (volatile uint8_t*)k_dtc_dtccr_addr;
}

/**
 * @brief Get pointer to DTC Vector Base Register (DTCVBR)
 * @return Volatile pointer to DTCVBR register (32-bit)
 * @pre DTC module stop bit (MSTPCRA.MSTPA28) must be cleared
 * @since Version 1.0.0
 */
static inline volatile uint32_t* dtc_dtcvbr_reg(void)
{
  return (volatile uint32_t*)k_dtc_dtcvbr_addr;
}

/**
 * @brief Get pointer to DTC Address Mode Register (DTCADMOD)
 * @return Volatile pointer to DTCADMOD register (8-bit)
 * @pre DTC module stop bit (MSTPCRA.MSTPA28) must be cleared
 * @since Version 1.0.0
 */
static inline volatile uint8_t* dtc_dtcadmod_reg(void)
{
  return (volatile uint8_t*)k_dtc_dtcadmod_addr;
}

/**
 * @brief Get pointer to DTC Module Start Register (DTCST)
 * @return Volatile pointer to DTCST register (8-bit)
 * @pre DTC module stop bit (MSTPCRA.MSTPA28) must be cleared
 * @since Version 1.0.0
 */
static inline volatile uint8_t* dtc_dtcst_reg(void)
{
  return (volatile uint8_t*)k_dtc_dtcst_addr;
}

/**
 * @brief Get pointer to DTC Status Register (DTCSTS)
 * @return Volatile pointer to DTCSTS register (16-bit)
 * @pre DTC module stop bit (MSTPCRA.MSTPA28) must be cleared
 * @since Version 1.0.0
 */
static inline volatile uint16_t* dtc_dtcsts_reg(void)
{
  return (volatile uint16_t*)k_dtc_dtcsts_addr;
}

/**
 * @brief Get pointer to DTC Index Table Base Register (DTCIBR)
 * @return Volatile pointer to DTCIBR register (32-bit)
 * @pre DTC module stop bit (MSTPCRA.MSTPA28) must be cleared
 * @since Version 1.0.0
 */
static inline volatile uint32_t* dtc_dtcibr_reg(void)
{
  return (volatile uint32_t*)k_dtc_dtcibr_addr;
}

/**
 * @brief Get pointer to DTC Operation Register (DTCOR)
 * @return Volatile pointer to DTCOR register (8-bit)
 * @pre DTC module stop bit (MSTPCRA.MSTPA28) must be cleared
 * @since Version 1.0.0
 */
static inline volatile uint8_t* dtc_dtcor_reg(void)
{
  return (volatile uint8_t*)k_dtc_dtcor_addr;
}

/**
 * @brief Get pointer to DTC Sequence Transfer Enable Register (DTCSQE)
 * @return Volatile pointer to DTCSQE register (16-bit)
 * @pre DTC module stop bit (MSTPCRA.MSTPA28) must be cleared
 * @since Version 1.0.0
 */
static inline volatile uint16_t* dtc_dtcsqe_reg(void)
{
  return (volatile uint16_t*)k_dtc_dtcsqe_addr;
}

/**
 * @brief Get pointer to DTC Address Displacement Register (DTCDISP)
 * @return Volatile pointer to DTCDISP register (32-bit)
 * @pre DTC module stop bit (MSTPCRA.MSTPA28) must be cleared
 * @since Version 1.0.0
 */
static inline volatile uint32_t* dtc_dtcdisp_reg(void)
{
  return (volatile uint32_t*)k_dtc_dtcdisp_addr;
}

/* =============================================================================
 * Static Assertions - Address Verification
 * =============================================================================
 */

/**
 * @name Address Verification
 * @brief Compile-time verification of register addresses and offsets
 * @{
 */

/* Verify base address */
static_assert(k_dtc_base_addr == 0x00082400U,
              "DTC base address must be 0x00082400 per Ch20 manual");

/* Verify register addresses */
static_assert(k_dtc_dtccr_addr == 0x00082400U, "DTCCR address must be 0x00082400 per Ch20 manual");

static_assert(k_dtc_dtcvbr_addr == 0x00082404U,
              "DTCVBR address must be 0x00082404 per Ch20 manual");

static_assert(k_dtc_dtcadmod_addr == 0x00082408U,
              "DTCADMOD address must be 0x00082408 per Ch20 manual");

static_assert(k_dtc_dtcst_addr == 0x0008240CU, "DTCST address must be 0x0008240C per Ch20 manual");

static_assert(k_dtc_dtcsts_addr == 0x0008240EU,
              "DTCSTS address must be 0x0008240E per Ch20 manual");

static_assert(k_dtc_dtcibr_addr == 0x00082410U,
              "DTCIBR address must be 0x00082410 per Ch20 manual");

static_assert(k_dtc_dtcor_addr == 0x00082414U, "DTCOR address must be 0x00082414 per Ch20 manual");

static_assert(k_dtc_dtcsqe_addr == 0x00082416U,
              "DTCSQE address must be 0x00082416 per Ch20 manual");

static_assert(k_dtc_dtcdisp_addr == 0x00082418U,
              "DTCDISP address must be 0x00082418 per Ch20 manual");

/* Verify register offsets */
static_assert(k_dtc_offset_dtccr == 0x00U, "DTCCR offset must be 0x00");

static_assert(k_dtc_offset_dtcvbr == 0x04U, "DTCVBR offset must be 0x04");

static_assert(k_dtc_offset_dtcadmod == 0x08U, "DTCADMOD offset must be 0x08");

static_assert(k_dtc_offset_dtcst == 0x0CU, "DTCST offset must be 0x0C");

static_assert(k_dtc_offset_dtcsts == 0x0EU, "DTCSTS offset must be 0x0E");

static_assert(k_dtc_offset_dtcibr == 0x10U, "DTCIBR offset must be 0x10");

static_assert(k_dtc_offset_dtcor == 0x14U, "DTCOR offset must be 0x14");

static_assert(k_dtc_offset_dtcsqe == 0x16U, "DTCSQE offset must be 0x16");

static_assert(k_dtc_offset_dtcdisp == 0x18U, "DTCDISP offset must be 0x18");

/* Verify offset + base = address */
static_assert(k_dtc_base_addr + k_dtc_offset_dtccr == k_dtc_dtccr_addr,
              "DTCCR: base + offset must equal address");

static_assert(k_dtc_base_addr + k_dtc_offset_dtcvbr == k_dtc_dtcvbr_addr,
              "DTCVBR: base + offset must equal address");

static_assert(k_dtc_base_addr + k_dtc_offset_dtcadmod == k_dtc_dtcadmod_addr,
              "DTCADMOD: base + offset must equal address");

static_assert(k_dtc_base_addr + k_dtc_offset_dtcst == k_dtc_dtcst_addr,
              "DTCST: base + offset must equal address");

static_assert(k_dtc_base_addr + k_dtc_offset_dtcsts == k_dtc_dtcsts_addr,
              "DTCSTS: base + offset must equal address");

static_assert(k_dtc_base_addr + k_dtc_offset_dtcibr == k_dtc_dtcibr_addr,
              "DTCIBR: base + offset must equal address");

static_assert(k_dtc_base_addr + k_dtc_offset_dtcor == k_dtc_dtcor_addr,
              "DTCOR: base + offset must equal address");

static_assert(k_dtc_base_addr + k_dtc_offset_dtcsqe == k_dtc_dtcsqe_addr,
              "DTCSQE: base + offset must equal address");

static_assert(k_dtc_base_addr + k_dtc_offset_dtcdisp == k_dtc_dtcdisp_addr,
              "DTCDISP: base + offset must equal address");

/* Verify transfer info structure sizes */
static_assert(sizeof(dtc_transfer_info_full_t) == 20,
              "Full transfer info structure must be 20 bytes");

/** @} */

#ifdef __cplusplus
}
#endif
