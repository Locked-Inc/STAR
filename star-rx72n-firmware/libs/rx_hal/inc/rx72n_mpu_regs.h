/**
 * @file rx72n_mpu_regs.h
 * @brief RX72N Memory Protection Unit (MPU) Register Definitions
 *
 * @details
 * Complete register definitions for the RX72N Memory Protection Unit.
 * The MPU provides address-based access control for user-mode programs,
 * protecting code and data regions from unauthorized access.
 *
 * @par MPU Overview
 * - Covers entire 4 GB address space (0x00000000-0xFFFFFFFF)
 * - 8 independent configurable regions (Region 0-7)
 * - 16-byte page granularity (smallest unit of protection)
 * - 3 access types: Execute (X), Write (W), Read (R)
 * - Protection only active in user mode (supervisor mode bypasses)
 * - Background region provides default access for unprotected areas
 * - Overlapping regions use OR of permissions (permission wins)
 *
 * @par Register Organization
 * | Address Range     | Description                              |
 * |-------------------|------------------------------------------|
 * | 0x00086400-0x043F | Region registers (RSPAGE0-7, REPAGE0-7)  |
 * | 0x00086500-0x052F | Control/status registers                 |
 *
 * @par Region Registers (8 regions x 8 bytes each = 64 bytes)
 * | Offset | Register    | Description                            |
 * |--------|-------------|----------------------------------------|
 * | 0x00   | RSPAGEn     | Region n Start Page Number (32-bit)    |
 * | 0x04   | REPAGEn     | Region n End Page Number (32-bit)      |
 *
 * @par Control/Status Registers
 * | Address    | Size | Register | Description                       |
 * |------------|------|----------|-----------------------------------|
 * | 0x00086500 | 32   | MPEN     | Memory Protection Enable          |
 * | 0x00086504 | 32   | MPBAC    | Background Access Control         |
 * | 0x00086508 | 32   | MPECLR   | Error Status Clearing             |
 * | 0x0008650C | 32   | MPESTS   | Error Status                      |
 * | 0x00086514 | 32   | MPDEA    | Data Error Address                |
 * | 0x00086520 | 32   | MPSA     | Region Search Address             |
 * | 0x00086524 | 16   | MPOPS    | Region Search Operation           |
 * | 0x00086526 | 16   | MPOPI    | Region Invalidation Operation     |
 * | 0x00086528 | 32   | MHITI    | Instruction Hit Region            |
 * | 0x0008652C | 32   | MHITD    | Data Hit Region                   |
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
 * - Chapter 17: Memory-Protection Unit (MPU), pages 657-676
 * - Section 17.2: Register Descriptions
 *
 * @author Locked, Inc.
 * @date 2026-01-29
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
 * MPU Base Addresses and Constants
 * =============================================================================
 */

/**
 * @enum mpu_addresses_t
 * @brief MPU module addresses
 *
 * @details
 * Base addresses for MPU registers.
 * All addresses verified against RX72N Hardware Manual Ch17.
 *
 * @note Addresses verified 2026-01-29 against manual section 17.2
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /** @brief MPU region registers base address */
  k_mpu_region_base_addr = 0x00086400U,

  /** @brief MPU control registers base address */
  k_mpu_control_base_addr = 0x00086500U,

  /** @brief Region register block size (8 bytes per region: RSPAGE + REPAGE) */
  k_mpu_region_size = 0x08U,

  /* Region Start Page Number Registers (RSPAGEn) */
  k_mpu_rspage0_addr = 0x00086400U, /**< RSPAGE0 address */
  k_mpu_rspage1_addr = 0x00086408U, /**< RSPAGE1 address */
  k_mpu_rspage2_addr = 0x00086410U, /**< RSPAGE2 address */
  k_mpu_rspage3_addr = 0x00086418U, /**< RSPAGE3 address */
  k_mpu_rspage4_addr = 0x00086420U, /**< RSPAGE4 address */
  k_mpu_rspage5_addr = 0x00086428U, /**< RSPAGE5 address */
  k_mpu_rspage6_addr = 0x00086430U, /**< RSPAGE6 address */
  k_mpu_rspage7_addr = 0x00086438U, /**< RSPAGE7 address */

  /* Region End Page Number Registers (REPAGEn) */
  k_mpu_repage0_addr = 0x00086404U, /**< REPAGE0 address */
  k_mpu_repage1_addr = 0x0008640CU, /**< REPAGE1 address */
  k_mpu_repage2_addr = 0x00086414U, /**< REPAGE2 address */
  k_mpu_repage3_addr = 0x0008641CU, /**< REPAGE3 address */
  k_mpu_repage4_addr = 0x00086424U, /**< REPAGE4 address */
  k_mpu_repage5_addr = 0x0008642CU, /**< REPAGE5 address */
  k_mpu_repage6_addr = 0x00086434U, /**< REPAGE6 address */
  k_mpu_repage7_addr = 0x0008643CU, /**< REPAGE7 address */

  /* Control/Status Registers */
  k_mpu_mpen_addr   = 0x00086500U, /**< Memory Protection Enable */
  k_mpu_mpbac_addr  = 0x00086504U, /**< Background Access Control */
  k_mpu_mpeclr_addr = 0x00086508U, /**< Error Status Clearing */
  k_mpu_mpests_addr = 0x0008650CU, /**< Error Status */
  k_mpu_mpdea_addr  = 0x00086514U, /**< Data Error Address */
  k_mpu_mpsa_addr   = 0x00086520U, /**< Region Search Address */
  k_mpu_mpops_addr  = 0x00086524U, /**< Region Search Operation */
  k_mpu_mpopi_addr  = 0x00086526U, /**< Region Invalidation Operation */
  k_mpu_mhiti_addr  = 0x00086528U, /**< Instruction Hit Region */
  k_mpu_mhitd_addr  = 0x0008652CU, /**< Data Hit Region */
} mpu_addresses_t;

/**
 * @enum mpu_region_t
 * @brief MPU region numbers
 *
 * @details Region indices for use with accessor functions.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mpu_region_0     = 0U, /**< MPU region 0 */
  k_mpu_region_1     = 1U, /**< MPU region 1 */
  k_mpu_region_2     = 2U, /**< MPU region 2 */
  k_mpu_region_3     = 3U, /**< MPU region 3 */
  k_mpu_region_4     = 4U, /**< MPU region 4 */
  k_mpu_region_5     = 5U, /**< MPU region 5 */
  k_mpu_region_6     = 6U, /**< MPU region 6 */
  k_mpu_region_7     = 7U, /**< MPU region 7 */
  k_mpu_region_count = 8U, /**< Total number of MPU regions */
} mpu_region_t;

/**
 * @enum mpu_page_t
 * @brief MPU page configuration constants
 *
 * @details
 * The MPU divides the address space into 16-byte pages.
 * Page number = address[31:4] (upper 28 bits).
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Page size in bytes (16 bytes) */
  k_mpu_page_size = 16U,

  /** @brief Shift amount to convert address to page number */
  k_mpu_page_shift = 4U,

  /** @brief Mask for page number bits [31:4] */
  k_mpu_page_mask = 0xFFFFFFF0U,

  /** @brief Maximum page number (2^28 - 1) */
  k_mpu_page_max = 0x0FFFFFFFU,
} mpu_page_t;

/* =============================================================================
 * RSPAGEn Register (Region Start Page Number)
 * =============================================================================
 */

/**
 * @enum rspage_bits_t
 * @brief Region Start Page Number Register (RSPAGEn) bit definitions
 *
 * @details
 * Specifies the starting page number for a protection region.
 * The page number corresponds to address bits [31:4].
 *
 * @par Register Layout (32-bit @ 0x00086400 + n*8)
 * | Bits  | Field     | Description                       |
 * |-------|-----------|-----------------------------------|
 * | 3:0   | -         | Reserved (read as 0, write 0)     |
 * | 31:4  | RSPN[27:0]| Region Start Page Number          |
 *
 * @note Manual ref: Ch17 section 17.2.1
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Reserved bits mask (bits 3:0) */
  k_rspage_reserved_mask = 0x0000000FU,

  /** @brief Start page number field mask (bits 31:4) */
  k_rspage_rspn_mask = 0xFFFFFFF0U,

  /** @brief Start page number shift amount */
  k_rspage_rspn_shift = 4U,
} rspage_bits_t;

/* =============================================================================
 * REPAGEn Register (Region End Page Number)
 * =============================================================================
 */

/**
 * @enum repage_bits_t
 * @brief Region End Page Number Register (REPAGEn) bit definitions
 *
 * @details
 * Specifies the ending page number and access control for a protection region.
 * The end page is INCLUDED in the protected region.
 *
 * @par Register Layout (32-bit @ 0x00086404 + n*8)
 * | Bits  | Field     | Description                       |
 * |-------|-----------|-----------------------------------|
 * | 0     | V         | Valid (0=invalid, 1=valid)        |
 * | 1     | UAC.R     | Read permission (0=no, 1=yes)     |
 * | 2     | UAC.W     | Write permission (0=no, 1=yes)    |
 * | 3     | UAC.X     | Execute permission (0=no, 1=yes)  |
 * | 31:4  | REPN[27:0]| Region End Page Number            |
 *
 * @note Manual ref: Ch17 section 17.2.2
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Valid bit (bit 0) - Region enabled when set */
  k_repage_v = 0x00000001U,

  /** @brief Read permission (bit 1) - Allow data read */
  k_repage_uac_r = 0x00000002U,

  /** @brief Write permission (bit 2) - Allow data write */
  k_repage_uac_w = 0x00000004U,

  /** @brief Execute permission (bit 3) - Allow instruction fetch */
  k_repage_uac_x = 0x00000008U,

  /** @brief Access control bits mask (UAC[2:0] = bits 3:1) */
  k_repage_uac_mask = 0x0000000EU,

  /** @brief Access control shift amount */
  k_repage_uac_shift = 1U,

  /** @brief End page number field mask (bits 31:4) */
  k_repage_repn_mask = 0xFFFFFFF0U,

  /** @brief End page number shift amount */
  k_repage_repn_shift = 4U,

  /* Common permission combinations */
  /** @brief Read-only region (R) */
  k_repage_uac_ro = k_repage_uac_r,

  /** @brief Read-write region (RW) */
  k_repage_uac_rw = k_repage_uac_r | k_repage_uac_w,

  /** @brief Execute-only region (X) */
  k_repage_uac_xo = k_repage_uac_x,

  /** @brief Read-execute region (RX) */
  k_repage_uac_rx = k_repage_uac_r | k_repage_uac_x,

  /** @brief Full access region (RWX) */
  k_repage_uac_rwx = k_repage_uac_r | k_repage_uac_w | k_repage_uac_x,
} repage_bits_t;

/* =============================================================================
 * MPEN Register (Memory Protection Enable)
 * =============================================================================
 */

/**
 * @enum mpen_bits_t
 * @brief Memory Protection Enable Register (MPEN) bit definitions
 *
 * @details
 * Enables or disables memory protection globally.
 * Protection takes effect when CPU transitions to user mode.
 *
 * @par Register Layout (32-bit @ 0x00086500)
 * | Bits  | Field | Description                         |
 * |-------|-------|-------------------------------------|
 * | 0     | MPEN  | Memory Protection Enable            |
 * | 31:1  | -     | Reserved (read as 0, write 0)       |
 *
 * @note Manual ref: Ch17 section 17.2.3
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Memory Protection Enable bit (bit 0) */
  k_mpen_mpen = 0x00000001U,
} mpen_bits_t;

/* =============================================================================
 * MPBAC Register (Background Access Control)
 * =============================================================================
 */

/**
 * @enum mpbac_bits_t
 * @brief Background Access Control Register (MPBAC) bit definitions
 *
 * @details
 * Sets default access permissions for addresses not covered by any region.
 * The background region covers the entire address space.
 * Default is all access prohibited (0).
 *
 * @par Register Layout (32-bit @ 0x00086504)
 * | Bits  | Field     | Description                       |
 * |-------|-----------|-----------------------------------|
 * | 0     | -         | Reserved (read as 0, write 0)     |
 * | 1     | UBAC.R    | Read permission (0=no, 1=yes)     |
 * | 2     | UBAC.W    | Write permission (0=no, 1=yes)    |
 * | 3     | UBAC.X    | Execute permission (0=no, 1=yes)  |
 * | 31:4  | -         | Reserved (read as 0, write 0)     |
 *
 * @note Manual ref: Ch17 section 17.2.4
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Read permission (bit 1) - Allow data read */
  k_mpbac_ubac_r = 0x00000002U,

  /** @brief Write permission (bit 2) - Allow data write */
  k_mpbac_ubac_w = 0x00000004U,

  /** @brief Execute permission (bit 3) - Allow instruction fetch */
  k_mpbac_ubac_x = 0x00000008U,

  /** @brief Access control bits mask (UBAC[2:0] = bits 3:1) */
  k_mpbac_ubac_mask = 0x0000000EU,

  /** @brief Access control shift amount */
  k_mpbac_ubac_shift = 1U,
} mpbac_bits_t;

/* =============================================================================
 * MPECLR Register (Error Status Clearing)
 * =============================================================================
 */

/**
 * @enum mpeclr_bits_t
 * @brief Error Status Clearing Register (MPECLR) bit definitions
 *
 * @details
 * Writing 1 to CLR clears the IMPER, DMPER, and DRW bits in MPESTS.
 * Always reads as 0.
 *
 * @par Register Layout (32-bit @ 0x00086508)
 * | Bits  | Field | Description                         |
 * |-------|-------|-------------------------------------|
 * | 0     | CLR   | Error Status Clear (write 1 to clr) |
 * | 31:1  | -     | Reserved (read as 0, write 0)       |
 *
 * @note Manual ref: Ch17 section 17.2.5
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Error Status Clear bit (bit 0) - Write 1 to clear MPESTS */
  k_mpeclr_clr = 0x00000001U,
} mpeclr_bits_t;

/* =============================================================================
 * MPESTS Register (Error Status)
 * =============================================================================
 */

/**
 * @enum mpests_bits_t
 * @brief Memory Protection Error Status Register (MPESTS) bit definitions
 *
 * @details
 * Indicates the type of memory protection error that occurred.
 * Bits are cleared by writing 1 to MPECLR.CLR.
 *
 * @par Register Layout (32-bit @ 0x0008650C)
 * | Bits  | Field | Description                           |
 * |-------|-------|---------------------------------------|
 * | 0     | IMPER | Instruction MPE generated (1=error)   |
 * | 1     | DMPER | Data MPE generated (1=error)          |
 * | 2     | DRW   | Data access type (0=read, 1=write)    |
 * | 31:3  | -     | Reserved (read as 0, write 0)         |
 *
 * @note Manual ref: Ch17 section 17.2.6
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Instruction Memory Protection Error (bit 0) */
  k_mpests_imper = 0x00000001U,

  /** @brief Data Memory Protection Error (bit 1) */
  k_mpests_dmper = 0x00000002U,

  /** @brief Data Read/Write indicator (bit 2) - 0=read, 1=write */
  k_mpests_drw = 0x00000004U,
} mpests_bits_t;

/* =============================================================================
 * MPOPS Register (Region Search Operation)
 * =============================================================================
 */

/**
 * @enum mpops_bits_t
 * @brief Region Search Operation Register (MPOPS) bit definitions
 *
 * @details
 * Writing 1 to S initiates a region search using the address in MPSA.
 * Results are stored in MHITD. Always reads as 0.
 *
 * @par Register Layout (16-bit @ 0x00086524)
 * | Bits  | Field | Description                         |
 * |-------|-------|-------------------------------------|
 * | 0     | S     | Region Search Start (write 1)       |
 * | 15:1  | -     | Reserved (read as 0, write 0)       |
 *
 * @note Manual ref: Ch17 section 17.2.9
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /** @brief Region Search Start bit (bit 0) */
  k_mpops_s = 0x0001U,
} mpops_bits_t;

/* =============================================================================
 * MPOPI Register (Region Invalidation Operation)
 * =============================================================================
 */

/**
 * @enum mpopi_bits_t
 * @brief Region Invalidation Operation Register (MPOPI) bit definitions
 *
 * @details
 * Writing 1 to INV clears the V (valid) bit in all REPAGEn registers,
 * effectively disabling all region protections (only background remains).
 * Always reads as 0.
 *
 * @par Register Layout (16-bit @ 0x00086526)
 * | Bits  | Field | Description                         |
 * |-------|-------|-------------------------------------|
 * | 0     | INV   | Region Invalidate Start (write 1)   |
 * | 15:1  | -     | Reserved (read as 0, write 0)       |
 *
 * @note Manual ref: Ch17 section 17.2.10
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /** @brief Region Invalidate Start bit (bit 0) */
  k_mpopi_inv = 0x0001U,
} mpopi_bits_t;

/* =============================================================================
 * MHITI Register (Instruction Hit Region)
 * =============================================================================
 */

/**
 * @enum mhiti_bits_t
 * @brief Instruction Hit Region Register (MHITI) bit definitions
 *
 * @details
 * After an instruction memory protection error, this register indicates
 * which region caused the error and what permissions were set.
 *
 * @par Register Layout (32-bit @ 0x00086528)
 * | Bits   | Field      | Description                        |
 * |--------|------------|------------------------------------|
 * | 0      | -          | Reserved (read as 0)               |
 * | 3:1    | UHACI[2:0] | Hit region access control (RWX)    |
 * | 15:4   | -          | Reserved (read as 0)               |
 * | 23:16  | HITI[7:0]  | Region hit flags (1=hit in region) |
 * | 31:24  | -          | Reserved (read as 0)               |
 *
 * @note HITI[7:0] = 0 means error in background region
 * @note Manual ref: Ch17 section 17.2.11
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Read permission in hit region (bit 1) */
  k_mhiti_uhaci_r = 0x00000002U,

  /** @brief Write permission in hit region (bit 2) */
  k_mhiti_uhaci_w = 0x00000004U,

  /** @brief Execute permission in hit region (bit 3) */
  k_mhiti_uhaci_x = 0x00000008U,

  /** @brief Access control bits mask (bits 3:1) */
  k_mhiti_uhaci_mask = 0x0000000EU,

  /** @brief Region 0 hit flag (bit 16) */
  k_mhiti_hiti0 = 0x00010000U,

  /** @brief Region 1 hit flag (bit 17) */
  k_mhiti_hiti1 = 0x00020000U,

  /** @brief Region 2 hit flag (bit 18) */
  k_mhiti_hiti2 = 0x00040000U,

  /** @brief Region 3 hit flag (bit 19) */
  k_mhiti_hiti3 = 0x00080000U,

  /** @brief Region 4 hit flag (bit 20) */
  k_mhiti_hiti4 = 0x00100000U,

  /** @brief Region 5 hit flag (bit 21) */
  k_mhiti_hiti5 = 0x00200000U,

  /** @brief Region 6 hit flag (bit 22) */
  k_mhiti_hiti6 = 0x00400000U,

  /** @brief Region 7 hit flag (bit 23) */
  k_mhiti_hiti7 = 0x00800000U,

  /** @brief Hit region flags mask (bits 23:16) */
  k_mhiti_hiti_mask = 0x00FF0000U,

  /** @brief Hit region flags shift amount */
  k_mhiti_hiti_shift = 16U,
} mhiti_bits_t;

/* =============================================================================
 * MHITD Register (Data Hit Region)
 * =============================================================================
 */

/**
 * @enum mhitd_bits_t
 * @brief Data Hit Region Register (MHITD) bit definitions
 *
 * @details
 * After a data memory protection error or region search, this register
 * indicates which region was hit and what permissions were set.
 *
 * @par Register Layout (32-bit @ 0x0008652C)
 * | Bits   | Field      | Description                        |
 * |--------|------------|------------------------------------|
 * | 0      | -          | Reserved (read as 0)               |
 * | 3:1    | UHACD[2:0] | Hit region access control (RWX)    |
 * | 15:4   | -          | Reserved (read as 0)               |
 * | 23:16  | HITD[7:0]  | Region hit flags (1=hit in region) |
 * | 31:24  | -          | Reserved (read as 0)               |
 *
 * @note HITD[7:0] = 0 means error in background region
 * @note Manual ref: Ch17 section 17.2.12
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief Read permission in hit region (bit 1) */
  k_mhitd_uhacd_r = 0x00000002U,

  /** @brief Write permission in hit region (bit 2) */
  k_mhitd_uhacd_w = 0x00000004U,

  /** @brief Execute permission in hit region (bit 3) */
  k_mhitd_uhacd_x = 0x00000008U,

  /** @brief Access control bits mask (bits 3:1) */
  k_mhitd_uhacd_mask = 0x0000000EU,

  /** @brief Region 0 hit flag (bit 16) */
  k_mhitd_hitd0 = 0x00010000U,

  /** @brief Region 1 hit flag (bit 17) */
  k_mhitd_hitd1 = 0x00020000U,

  /** @brief Region 2 hit flag (bit 18) */
  k_mhitd_hitd2 = 0x00040000U,

  /** @brief Region 3 hit flag (bit 19) */
  k_mhitd_hitd3 = 0x00080000U,

  /** @brief Region 4 hit flag (bit 20) */
  k_mhitd_hitd4 = 0x00100000U,

  /** @brief Region 5 hit flag (bit 21) */
  k_mhitd_hitd5 = 0x00200000U,

  /** @brief Region 6 hit flag (bit 22) */
  k_mhitd_hitd6 = 0x00400000U,

  /** @brief Region 7 hit flag (bit 23) */
  k_mhitd_hitd7 = 0x00800000U,

  /** @brief Hit region flags mask (bits 23:16) */
  k_mhitd_hitd_mask = 0x00FF0000U,

  /** @brief Hit region flags shift amount */
  k_mhitd_hitd_shift = 16U,
} mhitd_bits_t;

/* =============================================================================
 * MPU Region Register Structure
 * =============================================================================
 */

/**
 * @struct rx_mpu_region_regs_t
 * @brief MPU per-region register pair
 *
 * @details
 * Memory-mapped register layout for one MPU region.
 * Each region has 8 bytes (RSPAGE + REPAGE).
 *
 * @note Structure size verified as 8 bytes.
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed)) {
  /**
   * @brief Region Start Page Number Register (RSPAGEn)
   * @details
   * Bits [31:4] = Start page number (address[31:4])
   * Bits [3:0] = Reserved (write 0)
   */
  volatile uint32_t rspage;

  /**
   * @brief Region End Page Number Register (REPAGEn)
   * @details
   * Bits [31:4] = End page number (inclusive)
   * Bit [3] = Execute permission (UAC.X)
   * Bit [2] = Write permission (UAC.W)
   * Bit [1] = Read permission (UAC.R)
   * Bit [0] = Valid bit (V)
   */
  volatile uint32_t repage;
} rx_mpu_region_regs_t;

/* =============================================================================
 * Register Accessor Functions - Region Registers
 * =============================================================================
 */

/**
 * @brief Get pointer to MPU region register pair
 *
 * @param region Region number (0-7)
 * @return Volatile pointer to region register structure
 *
 * @pre region < k_mpu_region_count (8)
 *
 * @note Thread Safety: Safe - returns constant hardware address
 *
 * @par Example:
 * @code{.c}
 * // Configure region 0 to protect RAM (0x00000000-0x0007FFFF) as read-write
 * volatile rx_mpu_region_regs_t* r0 = mpu_region(k_mpu_region_0);
 * r0->rspage = 0x00000000;  // Start at page 0
 * r0->repage = (0x0007FFF0) | k_repage_uac_rw | k_repage_v;  // End at page 0x7FFF, RW, valid
 * @endcode
 *
 * @since Version 1.0.0
 */
static inline volatile rx_mpu_region_regs_t* mpu_region(uint8_t region)
{
  return (volatile rx_mpu_region_regs_t*)(k_mpu_region_base_addr + (region * k_mpu_region_size));
}

/**
 * @brief Get pointer to RSPAGE register for a region
 *
 * @param region Region number (0-7)
 * @return Volatile pointer to RSPAGE register (32-bit)
 * @since Version 1.0.0
 */
static inline volatile uint32_t* mpu_rspage_reg(uint8_t region)
{
  return (volatile uint32_t*)(k_mpu_region_base_addr + (region * k_mpu_region_size));
}

/**
 * @brief Get pointer to REPAGE register for a region
 *
 * @param region Region number (0-7)
 * @return Volatile pointer to REPAGE register (32-bit)
 * @since Version 1.0.0
 */
static inline volatile uint32_t* mpu_repage_reg(uint8_t region)
{
  return (volatile uint32_t*)(k_mpu_region_base_addr + (region * k_mpu_region_size) + 4U);
}

/* =============================================================================
 * Register Accessor Functions - Control Registers
 * =============================================================================
 */

/**
 * @brief Get pointer to MPEN (Memory Protection Enable) register
 * @return Volatile pointer to MPEN register (32-bit)
 * @since Version 1.0.0
 */
static inline volatile uint32_t* mpu_mpen_reg(void)
{
  return (volatile uint32_t*)k_mpu_mpen_addr;
}

/**
 * @brief Get pointer to MPBAC (Background Access Control) register
 * @return Volatile pointer to MPBAC register (32-bit)
 * @since Version 1.0.0
 */
static inline volatile uint32_t* mpu_mpbac_reg(void)
{
  return (volatile uint32_t*)k_mpu_mpbac_addr;
}

/**
 * @brief Get pointer to MPECLR (Error Status Clearing) register
 * @return Volatile pointer to MPECLR register (32-bit)
 * @since Version 1.0.0
 */
static inline volatile uint32_t* mpu_mpeclr_reg(void)
{
  return (volatile uint32_t*)k_mpu_mpeclr_addr;
}

/**
 * @brief Get pointer to MPESTS (Error Status) register
 * @return Volatile pointer to MPESTS register (32-bit)
 * @since Version 1.0.0
 */
static inline volatile uint32_t* mpu_mpests_reg(void)
{
  return (volatile uint32_t*)k_mpu_mpests_addr;
}

/**
 * @brief Get pointer to MPDEA (Data Error Address) register
 * @return Volatile pointer to MPDEA register (32-bit)
 * @since Version 1.0.0
 */
static inline volatile uint32_t* mpu_mpdea_reg(void)
{
  return (volatile uint32_t*)k_mpu_mpdea_addr;
}

/**
 * @brief Get pointer to MPSA (Region Search Address) register
 * @return Volatile pointer to MPSA register (32-bit)
 * @since Version 1.0.0
 */
static inline volatile uint32_t* mpu_mpsa_reg(void)
{
  return (volatile uint32_t*)k_mpu_mpsa_addr;
}

/**
 * @brief Get pointer to MPOPS (Region Search Operation) register
 * @return Volatile pointer to MPOPS register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* mpu_mpops_reg(void)
{
  return (volatile uint16_t*)k_mpu_mpops_addr;
}

/**
 * @brief Get pointer to MPOPI (Region Invalidation) register
 * @return Volatile pointer to MPOPI register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* mpu_mpopi_reg(void)
{
  return (volatile uint16_t*)k_mpu_mpopi_addr;
}

/**
 * @brief Get pointer to MHITI (Instruction Hit Region) register
 * @return Volatile pointer to MHITI register (32-bit)
 * @since Version 1.0.0
 */
static inline volatile uint32_t* mpu_mhiti_reg(void)
{
  return (volatile uint32_t*)k_mpu_mhiti_addr;
}

/**
 * @brief Get pointer to MHITD (Data Hit Region) register
 * @return Volatile pointer to MHITD register (32-bit)
 * @since Version 1.0.0
 */
static inline volatile uint32_t* mpu_mhitd_reg(void)
{
  return (volatile uint32_t*)k_mpu_mhitd_addr;
}

/* =============================================================================
 * Helper Functions - Page Number Conversion
 * =============================================================================
 */

/**
 * @brief Convert address to page number
 *
 * @param addr Memory address
 * @return Page number (address[31:4])
 *
 * @par Example:
 * @code{.c}
 * uint32_t page = mpu_addr_to_page(0x00012340);
 * // page = 0x00001234
 * @endcode
 *
 * @since Version 1.0.0
 */
static inline uint32_t mpu_addr_to_page(uint32_t addr)
{
  return (addr >> k_mpu_page_shift) & k_mpu_page_max;
}

/**
 * @brief Convert page number to RSPAGE/REPAGE register value
 *
 * @param page Page number (28 bits)
 * @return Register value with page number in bits [31:4]
 *
 * @note For REPAGE, caller must OR in access control and valid bits.
 *
 * @par Example:
 * @code{.c}
 * uint32_t rspage_val = mpu_page_to_reg(0x00001234);
 * uint32_t repage_val = mpu_page_to_reg(0x00001234) | k_repage_uac_rw | k_repage_v;
 * @endcode
 *
 * @since Version 1.0.0
 */
static inline uint32_t mpu_page_to_reg(uint32_t page)
{
  return (page << k_mpu_page_shift) & k_rspage_rspn_mask;
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

/* Verify base addresses */
static_assert(k_mpu_region_base_addr == 0x00086400U,
              "MPU region base address must be 0x00086400 per Ch17 manual");

static_assert(k_mpu_control_base_addr == 0x00086500U,
              "MPU control base address must be 0x00086500 per Ch17 manual");

/* Verify RSPAGE addresses */
static_assert(k_mpu_rspage0_addr == 0x00086400U,
              "RSPAGE0 address must be 0x00086400 per Ch17 manual");

static_assert(k_mpu_rspage1_addr == 0x00086408U,
              "RSPAGE1 address must be 0x00086408 per Ch17 manual");

static_assert(k_mpu_rspage7_addr == 0x00086438U,
              "RSPAGE7 address must be 0x00086438 per Ch17 manual");

/* Verify REPAGE addresses */
static_assert(k_mpu_repage0_addr == 0x00086404U,
              "REPAGE0 address must be 0x00086404 per Ch17 manual");

static_assert(k_mpu_repage1_addr == 0x0008640CU,
              "REPAGE1 address must be 0x0008640C per Ch17 manual");

static_assert(k_mpu_repage7_addr == 0x0008643CU,
              "REPAGE7 address must be 0x0008643C per Ch17 manual");

/* Verify region spacing */
static_assert(k_mpu_region_size == 0x08U, "MPU region spacing must be 8 bytes");

static_assert(k_mpu_rspage1_addr - k_mpu_rspage0_addr == k_mpu_region_size,
              "RSPAGE0 to RSPAGE1 spacing must be 8 bytes");

static_assert(k_mpu_rspage7_addr - k_mpu_rspage0_addr == 7 * k_mpu_region_size,
              "RSPAGE0 to RSPAGE7 spacing must be 56 bytes");

/* Verify control register addresses */
static_assert(k_mpu_mpen_addr == 0x00086500U, "MPEN address must be 0x00086500 per Ch17 manual");

static_assert(k_mpu_mpbac_addr == 0x00086504U, "MPBAC address must be 0x00086504 per Ch17 manual");

static_assert(k_mpu_mpeclr_addr == 0x00086508U,
              "MPECLR address must be 0x00086508 per Ch17 manual");

static_assert(k_mpu_mpests_addr == 0x0008650CU,
              "MPESTS address must be 0x0008650C per Ch17 manual");

static_assert(k_mpu_mpdea_addr == 0x00086514U, "MPDEA address must be 0x00086514 per Ch17 manual");

static_assert(k_mpu_mpsa_addr == 0x00086520U, "MPSA address must be 0x00086520 per Ch17 manual");

static_assert(k_mpu_mpops_addr == 0x00086524U, "MPOPS address must be 0x00086524 per Ch17 manual");

static_assert(k_mpu_mpopi_addr == 0x00086526U, "MPOPI address must be 0x00086526 per Ch17 manual");

static_assert(k_mpu_mhiti_addr == 0x00086528U, "MHITI address must be 0x00086528 per Ch17 manual");

static_assert(k_mpu_mhitd_addr == 0x0008652CU, "MHITD address must be 0x0008652C per Ch17 manual");

/* Verify structure layout */
static_assert(sizeof(rx_mpu_region_regs_t) == k_mpu_region_size,
              "MPU region struct size must be 8 bytes");

static_assert(offsetof(rx_mpu_region_regs_t, rspage) == 0, "RSPAGE offset in struct must be 0");

static_assert(offsetof(rx_mpu_region_regs_t, repage) == 4, "REPAGE offset in struct must be 4");

/* Verify bit definitions */
static_assert(k_repage_v == 0x01U, "REPAGE V bit must be bit 0");

static_assert(k_repage_uac_r == 0x02U,
              "REPAGE UAC.R bit must be bit 1"); /* NOLINT(readability-magic-numbers) */

static_assert(k_repage_uac_w == 0x04U,
              "REPAGE UAC.W bit must be bit 2"); /* NOLINT(readability-magic-numbers) */

static_assert(k_repage_uac_x == 0x08U,
              "REPAGE UAC.X bit must be bit 3"); /* NOLINT(readability-magic-numbers) */

static_assert(k_mpen_mpen == 0x01U, "MPEN.MPEN bit must be bit 0");

static_assert(k_mpests_imper == 0x01U, "MPESTS.IMPER bit must be bit 0");

static_assert(k_mpests_dmper == 0x02U, "MPESTS.DMPER bit must be bit 1");

static_assert(k_mpests_drw == 0x04U, "MPESTS.DRW bit must be bit 2");

/** @} */

#ifdef __cplusplus
}
#endif
