/* SPDX-License-Identifier: MIT */
/**
 * @file rx72n_dmac_regs.h
 * @brief RX72N DMA Controller (DMAC) Register Definitions
 *
 * @details
 * Complete register definitions for the RX72N 8-channel DMA Controller.
 * The DMAC transfers data without CPU intervention, supporting normal,
 * repeat, and block transfer modes.
 *
 * @par DMAC Overview
 * - 8 independent channels (DMAC0-DMAC7)
 * - 512 MB transfer space (0x00000000-0x0FFFFFFF, 0xF0000000-0xFFFFFFFF)
 * - Maximum 64M data transfers (1024 data x 65536 blocks)
 * - 8/16/32-bit transfer sizes
 * - Software or peripheral interrupt triggers
 * - Channel priority: Ch0 > Ch1 > Ch2 > ... > Ch7
 *
 * @par Register Organization
 * | Address       | Size | Description                           |
 * |---------------|------|---------------------------------------|
 * | 0x00082000    | -    | DMAC channel 0 base                   |
 * | 0x00082040    | -    | DMAC channel 1 base                   |
 * | ...           | ...  | (channels spaced 0x40 apart)          |
 * | 0x000821C0    | -    | DMAC channel 7 base                   |
 * | 0x00082200    | 8    | DMAST - Module Start Register         |
 * | 0x00082204    | 8    | DMIST - Interrupt Status Monitor      |
 *
 * @par Per-Channel Register Offsets
 * | Offset | Size | Register | Description                        |
 * |--------|------|----------|------------------------------------|
 * | 0x00   | 32   | DMSAR    | Source Address                     |
 * | 0x04   | 32   | DMDAR    | Destination Address                |
 * | 0x08   | 32   | DMCRA    | Transfer Count A                   |
 * | 0x0C   | 16   | DMCRB    | Transfer Count B                   |
 * | 0x10   | 16   | DMTMD    | Transfer Mode                      |
 * | 0x13   | 8    | DMINT    | Interrupt Setting                  |
 * | 0x14   | 16   | DMAMD    | Address Mode                       |
 * | 0x18   | 32   | DMOFR    | Offset Register (ch0 only!)        |
 * | 0x1C   | 8    | DMCNT    | DMA Transfer Enable                |
 * | 0x1D   | 8    | DMREQ    | DMA Software Start                 |
 * | 0x1E   | 8    | DMSTS    | DMA Status                         |
 * | 0x1F   | 8    | DMCSL    | DMA Channel Select                 |
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
 * - Chapter 18: DMA Controller (DMACAa), pages 677-746
 * - Section 18.2: Register Descriptions
 *
 * @author STAR Team
 * @date 2026-01-29
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
 * DMAC Base Addresses and Channel Configuration
 * =============================================================================
 */

/**
 * @enum dmac_addresses_t
 * @brief DMAC module addresses
 *
 * @details
 * Base addresses for DMAC channels and shared registers.
 * All addresses verified against RX72N Hardware Manual Ch18.
 *
 * @note Addresses verified 2026-01-29 against manual section 18.2
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /** @brief DMAC module base address (channel 0) */
  k_dmac_base_addr = 0x00082000U,

  /** @brief Channel register block size (0x40 = 64 bytes per channel) */
  k_dmac_channel_size = 0x40U,

  /** @brief DMAC channel 0 base address */
  k_dmac0_base_addr = 0x00082000U,

  /** @brief DMAC channel 1 base address */
  k_dmac1_base_addr = 0x00082040U,

  /** @brief DMAC channel 2 base address */
  k_dmac2_base_addr = 0x00082080U,

  /** @brief DMAC channel 3 base address */
  k_dmac3_base_addr = 0x000820C0U,

  /** @brief DMAC channel 4 base address */
  k_dmac4_base_addr = 0x00082100U,

  /** @brief DMAC channel 5 base address */
  k_dmac5_base_addr = 0x00082140U,

  /** @brief DMAC channel 6 base address */
  k_dmac6_base_addr = 0x00082180U,

  /** @brief DMAC channel 7 base address */
  k_dmac7_base_addr = 0x000821C0U,

  /** @brief DMAC Module Start Register (DMAST) address */
  k_dmac_dmast_addr = 0x00082200U,

  /** @brief DMAC74 Interrupt Status Monitor Register (DMIST) address */
  k_dmac_dmist_addr = 0x00082204U,
} dmac_addresses_t;

/**
 * @enum dmac_offsets_t
 * @brief Per-channel register offsets
 *
 * @details
 * Offsets from channel base address for each register.
 * All offsets verified against RX72N Hardware Manual Ch18 section 18.2.
 *
 * @note Offsets verified 2026-01-29 against manual
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmac_offset_dmsar = 0x00U, /**< Source Address Register (32-bit) */
  k_dmac_offset_dmdar = 0x04U, /**< Destination Address Register (32-bit) */
  k_dmac_offset_dmcra = 0x08U, /**< Transfer Count Register A (32-bit) */
  k_dmac_offset_dmcrb = 0x0CU, /**< Transfer Count Register B (16-bit) */
  k_dmac_offset_dmtmd = 0x10U, /**< Transfer Mode Register (16-bit) */
  k_dmac_offset_dmint = 0x13U, /**< Interrupt Setting Register (8-bit) */
  k_dmac_offset_dmamd = 0x14U, /**< Address Mode Register (16-bit) */
  k_dmac_offset_dmofr = 0x18U, /**< Offset Register (32-bit, ch0 only!) */
  k_dmac_offset_dmcnt = 0x1CU, /**< DMA Transfer Enable Register (8-bit) */
  k_dmac_offset_dmreq = 0x1DU, /**< DMA Software Start Register (8-bit) */
  k_dmac_offset_dmsts = 0x1EU, /**< DMA Status Register (8-bit) */
  k_dmac_offset_dmcsl = 0x1FU, /**< DMA Channel Select Register (8-bit) */
} dmac_offsets_t;

/**
 * @enum dmac_channel_t
 * @brief DMAC channel numbers
 *
 * @details Channel indices for use with accessor functions.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmac_channel_0     = 0U, /**< DMAC channel 0 (highest priority) */
  k_dmac_channel_1     = 1U, /**< DMAC channel 1 */
  k_dmac_channel_2     = 2U, /**< DMAC channel 2 */
  k_dmac_channel_3     = 3U, /**< DMAC channel 3 */
  k_dmac_channel_4     = 4U, /**< DMAC channel 4 */
  k_dmac_channel_5     = 5U, /**< DMAC channel 5 */
  k_dmac_channel_6     = 6U, /**< DMAC channel 6 */
  k_dmac_channel_7     = 7U, /**< DMAC channel 7 (lowest priority) */
  k_dmac_channel_count = 8U, /**< Total number of DMAC channels */
} dmac_channel_t;

/* =============================================================================
 * DMAC Register Structures
 * =============================================================================
 */

/**
 * @struct rx_dmac_channel_regs_t
 * @brief DMAC per-channel register structure
 *
 * @details
 * Memory-mapped register layout for one DMAC channel.
 * Each channel has 64 bytes (0x40) of register space.
 *
 * @warning DMOFR register at offset 0x18 is ONLY valid for channel 0!
 *          For channels 1-7, this location is reserved.
 *
 * @note Structure size verified as 0x20 (32 bytes of active registers)
 *       with reserved padding to fill 0x40 channel spacing.
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed)) {
  /**
   * @brief DMA Source Address Register (DMSAR)
   * @details 32-bit source start address. Bits 31:29 extend bit 28.
   *          Valid range: 0x00000000-0x0FFFFFFF, 0xF0000000-0xFFFFFFFF
   */
  volatile uint32_t dmsar;

  /**
   * @brief DMA Destination Address Register (DMDAR)
   * @details 32-bit destination start address. Bits 31:29 extend bit 28.
   *          Valid range: 0x00000000-0x0FFFFFFF, 0xF0000000-0xFFFFFFFF
   */
  volatile uint32_t dmdar;

  /**
   * @brief DMA Transfer Count Register A (DMCRA)
   * @details
   * - Normal mode: bits [15:0] = transfer count, bits [25:16] = block size
   * - Repeat/Block mode: bits [9:0] = repeat/block size
   */
  volatile uint32_t dmcra;

  /**
   * @brief DMA Transfer Count Register B (DMCRB)
   * @details
   * - Normal mode (free-running): Not used
   * - Repeat mode: bits [15:0] = repeat count
   * - Block mode: bits [15:0] = block count
   */
  volatile uint16_t dmcrb;

  /** @brief Reserved (2 bytes padding between DMCRB and DMTMD) */
  volatile uint8_t reserved_0e[2];

  /**
   * @brief DMA Transfer Mode Register (DMTMD)
   * @details
   * Controls transfer mode, data size, repeat area, and trigger source.
   * @see dmtmd_bits_t for bit definitions
   */
  volatile uint16_t dmtmd;

  /** @brief Reserved (1 byte padding between DMTMD and DMINT) */
  volatile uint8_t reserved_12;

  /**
   * @brief DMA Interrupt Setting Register (DMINT)
   * @details
   * Enables/disables various interrupt sources.
   * @see dmint_bits_t for bit definitions
   */
  volatile uint8_t dmint;

  /**
   * @brief DMA Address Mode Register (DMAMD)
   * @details
   * Controls source/destination address update mode and extended repeat.
   * @see dmamd_bits_t for bit definitions
   */
  volatile uint16_t dmamd;

  /** @brief Reserved (2 bytes padding between DMAMD and DMOFR) */
  volatile uint8_t reserved_16[2];

  /**
   * @brief DMA Offset Register (DMOFR)
   * @details
   * 32-bit signed offset value for offset addition mode.
   * @warning ONLY VALID FOR CHANNEL 0! Reserved for channels 1-7.
   */
  volatile uint32_t dmofr;

  /**
   * @brief DMA Transfer Enable Register (DMCNT)
   * @details
   * Enables/disables DMA transfer for this channel.
   * @see dmcnt_bits_t for bit definitions
   */
  volatile uint8_t dmcnt;

  /**
   * @brief DMA Software Start Register (DMREQ)
   * @details
   * Software trigger control and clear settings.
   * @see dmreq_bits_t for bit definitions
   */
  volatile uint8_t dmreq;

  /**
   * @brief DMA Status Register (DMSTS)
   * @details
   * Transfer status flags (active, end, escape end, etc.).
   * @see dmsts_bits_t for bit definitions
   */
  volatile uint8_t dmsts;

  /**
   * @brief DMA Channel Select Register (DMCSL)
   * @details
   * Selects interrupt source channel (used with grouped interrupts).
   * @see dmcsl_bits_t for bit definitions
   */
  volatile uint8_t dmcsl;

  /** @brief Reserved padding to 0x40 (64 bytes) channel spacing */
  volatile uint8_t reserved_20[32];
} rx_dmac_channel_regs_t;

/* =============================================================================
 * DMTMD Register Bits (Transfer Mode)
 * =============================================================================
 */

/**
 * @enum dmtmd_bits_t
 * @brief DMA Transfer Mode Register (DMTMD) bit definitions
 *
 * @details
 * Controls transfer mode, data size, repeat area selection, and trigger source.
 *
 * @par Register Layout (16-bit @ offset 0x10)
 * | Bits   | Field    | Description                    |
 * |--------|----------|--------------------------------|
 * | 1:0    | DCTG     | Transfer Request Source Select |
 * | 7:2    | -        | Reserved                       |
 * | 9:8    | SZ       | Transfer Data Size Select      |
 * | 11:10  | -        | Reserved                       |
 * | 13:12  | DTS      | Repeat Area Select             |
 * | 15:14  | MD       | Transfer Mode Select           |
 *
 * @note Manual ref: Ch18 section 18.2.5
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* DCTG - Transfer Request Source Select (bits 1:0) */
  k_dmtmd_dctg_software  = 0x0000U, /**< Software trigger */
  k_dmtmd_dctg_interrupt = 0x0001U, /**< Peripheral/external interrupt */
  k_dmtmd_dctg_mask      = 0x0003U, /**< DCTG field mask */

  /* SZ - Transfer Data Size Select (bits 9:8) */
  k_dmtmd_sz_8bit  = 0x0000U, /**< 8-bit transfer */
  k_dmtmd_sz_16bit = 0x0100U, /**< 16-bit transfer */
  k_dmtmd_sz_32bit = 0x0200U, /**< 32-bit transfer */
  k_dmtmd_sz_mask  = 0x0300U, /**< SZ field mask */

  /* DTS - Repeat Area Select (bits 13:12) */
  k_dmtmd_dts_dest   = 0x0000U, /**< Destination is repeat/block area */
  k_dmtmd_dts_source = 0x1000U, /**< Source is repeat/block area */
  k_dmtmd_dts_none   = 0x2000U, /**< No repeat/block area */
  k_dmtmd_dts_mask   = 0x3000U, /**< DTS field mask */

  /* MD - Transfer Mode Select (bits 15:14) */
  k_dmtmd_md_normal = 0x0000U, /**< Normal transfer mode */
  k_dmtmd_md_repeat = 0x4000U, /**< Repeat transfer mode */
  k_dmtmd_md_block  = 0x8000U, /**< Block transfer mode */
  k_dmtmd_md_mask   = 0xC000U, /**< MD field mask */
} dmtmd_bits_t;

/* =============================================================================
 * DMINT Register Bits (Interrupt Setting)
 * =============================================================================
 */

/**
 * @enum dmint_bits_t
 * @brief DMA Interrupt Setting Register (DMINT) bit definitions
 *
 * @details
 * Controls interrupt enable for various DMA events.
 *
 * @par Register Layout (8-bit @ offset 0x13)
 * | Bit | Field | Description                                    |
 * |-----|-------|------------------------------------------------|
 * | 0   | DARIE | Dest Address Extended Repeat Overflow IE       |
 * | 1   | SARIE | Source Address Extended Repeat Overflow IE     |
 * | 2   | RPTIE | Repeat Size End Interrupt Enable               |
 * | 3   | ESIE  | Transfer Escape End Interrupt Enable           |
 * | 4   | DTIE  | Transfer End Interrupt Enable                  |
 * | 7:5 | -     | Reserved                                       |
 *
 * @note Manual ref: Ch18 section 18.2.6
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmint_darie = 0x01U, /**< Dest addr extended repeat overflow int enable */
  k_dmint_sarie = 0x02U, /**< Source addr extended repeat overflow int enable */
  k_dmint_rptie = 0x04U, /**< Repeat size end interrupt enable */
  k_dmint_esie  = 0x08U, /**< Transfer escape end interrupt enable */
  k_dmint_dtie  = 0x10U, /**< Transfer end interrupt enable */
} dmint_bits_t;

/* =============================================================================
 * DMAMD Register Bits (Address Mode)
 * =============================================================================
 */

/**
 * @enum dmamd_bits_t
 * @brief DMA Address Mode Register (DMAMD) bit definitions
 *
 * @details
 * Controls source/destination address update mode and extended repeat area.
 *
 * @par Register Layout (16-bit @ offset 0x14)
 * | Bits   | Field | Description                              |
 * |--------|-------|------------------------------------------|
 * | 1:0    | DARA  | Destination Address Extended Repeat Area |
 * | 5:2    | -     | Reserved                                 |
 * | 7:6    | DM    | Destination Address Update Mode          |
 * | 9:8    | SARA  | Source Address Extended Repeat Area      |
 * | 13:10  | -     | Reserved                                 |
 * | 15:14  | SM    | Source Address Update Mode               |
 *
 * @note Manual ref: Ch18 section 18.2.7
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* DARA - Destination Address Extended Repeat Area (bits 1:0) */
  k_dmamd_dara_mask = 0x0003U, /**< DARA field mask */

  /* DM - Destination Address Update Mode (bits 7:6) */
  k_dmamd_dm_fixed     = 0x0000U, /**< Destination address fixed */
  k_dmamd_dm_offset    = 0x0040U, /**< Add offset after transfer (ch0 only) */
  k_dmamd_dm_increment = 0x0080U, /**< Increment after transfer */
  k_dmamd_dm_decrement = 0x00C0U, /**< Decrement after transfer */
  k_dmamd_dm_mask      = 0x00C0U, /**< DM field mask */

  /* SARA - Source Address Extended Repeat Area (bits 9:8) */
  k_dmamd_sara_mask = 0x0300U, /**< SARA field mask */

  /* SM - Source Address Update Mode (bits 15:14) */
  k_dmamd_sm_fixed     = 0x0000U, /**< Source address fixed */
  k_dmamd_sm_offset    = 0x4000U, /**< Add offset after transfer (ch0 only) */
  k_dmamd_sm_increment = 0x8000U, /**< Increment after transfer */
  k_dmamd_sm_decrement = 0xC000U, /**< Decrement after transfer */
  k_dmamd_sm_mask      = 0xC000U, /**< SM field mask */
} dmamd_bits_t;

/* =============================================================================
 * DMCNT Register Bits (Transfer Enable)
 * =============================================================================
 */

/**
 * @enum dmcnt_bits_t
 * @brief DMA Transfer Enable Register (DMCNT) bit definitions
 *
 * @par Register Layout (8-bit @ offset 0x1C)
 * | Bit | Field | Description              |
 * |-----|-------|--------------------------|
 * | 0   | DTE   | DMA Transfer Enable      |
 * | 7:1 | -     | Reserved                 |
 *
 * @note Manual ref: Ch18 section 18.2.9
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmcnt_dte = 0x01U, /**< DMA Transfer Enable (0=disabled, 1=enabled) */
} dmcnt_bits_t;

/* =============================================================================
 * DMREQ Register Bits (Software Start)
 * =============================================================================
 */

/**
 * @enum dmreq_bits_t
 * @brief DMA Software Start Register (DMREQ) bit definitions
 *
 * @par Register Layout (8-bit @ offset 0x1D)
 * | Bit | Field  | Description                    |
 * |-----|--------|--------------------------------|
 * | 0   | SWREQ  | DMA Software Start             |
 * | 3:1 | -      | Reserved                       |
 * | 4   | CLRS   | DMA Software Start Bit Clear   |
 * | 7:5 | -      | Reserved                       |
 *
 * @note Manual ref: Ch18 section 18.2.10
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmreq_swreq = 0x01U, /**< Software start (write 1 to trigger) */
  k_dmreq_clrs  = 0x10U, /**< Auto-clear SWREQ after transfer start */
} dmreq_bits_t;

/* =============================================================================
 * DMSTS Register Bits (Status)
 * =============================================================================
 */

/**
 * @enum dmsts_bits_t
 * @brief DMA Status Register (DMSTS) bit definitions
 *
 * @par Register Layout (8-bit @ offset 0x1E)
 * | Bit | Field | Description                      |
 * |-----|-------|----------------------------------|
 * | 0   | ESIF  | Transfer Escape End Flag         |
 * | 3:1 | -     | Reserved                         |
 * | 4   | DTIF  | Transfer End Flag                |
 * | 6:5 | -     | Reserved                         |
 * | 7   | ACT   | DMA Active Flag                  |
 *
 * @note Manual ref: Ch18 section 18.2.11
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmsts_esif = 0x01U, /**< Transfer escape end flag */
  k_dmsts_dtif = 0x10U, /**< Transfer end flag */
  k_dmsts_act  = 0x80U, /**< DMA active flag (1=transfer in progress) */
} dmsts_bits_t;

/* =============================================================================
 * DMCSL Register Bits (Channel Select)
 * =============================================================================
 */

/**
 * @enum dmcsl_bits_t
 * @brief DMA Channel Select Register (DMCSL) bit definitions
 *
 * @par Register Layout (8-bit @ offset 0x1F)
 * | Bit | Field | Description                |
 * |-----|-------|----------------------------|
 * | 0   | DISEL | Interrupt Select           |
 * | 7:1 | -     | Reserved                   |
 *
 * @note Manual ref: Ch18 section 18.2.12
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmcsl_disel = 0x01U, /**< Interrupt select (0=transfer end, 1=DTIF) */
} dmcsl_bits_t;

/* =============================================================================
 * DMAST Register Bits (Module Start)
 * =============================================================================
 */

/**
 * @enum dmast_bits_t
 * @brief DMAC Module Start Register (DMAST) bit definitions
 *
 * @par Register Layout (8-bit @ 0x00082200)
 * | Bit | Field | Description              |
 * |-----|-------|--------------------------|
 * | 0   | DMST  | DMAC Module Start        |
 * | 7:1 | -     | Reserved                 |
 *
 * @note Manual ref: Ch18 section 18.2.13
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmast_dmst = 0x01U, /**< DMAC module start (0=suspended, 1=operating) */
} dmast_bits_t;

/* =============================================================================
 * DMIST Register Bits (Interrupt Status Monitor)
 * =============================================================================
 */

/**
 * @enum dmist_bits_t
 * @brief DMAC74 Interrupt Status Monitor Register (DMIST) bit definitions
 *
 * @par Register Layout (8-bit @ 0x00082204)
 * | Bit | Field | Description                     |
 * |-----|-------|---------------------------------|
 * | 3:0 | -     | Reserved                        |
 * | 4   | DMIS4 | Channel 4 interrupt status      |
 * | 5   | DMIS5 | Channel 5 interrupt status      |
 * | 6   | DMIS6 | Channel 6 interrupt status      |
 * | 7   | DMIS7 | Channel 7 interrupt status      |
 *
 * @note Manual ref: Ch18 section 18.2.14
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_dmist_dmis4 = 0x10U, /**< Channel 4 interrupt status (bit 4) */
  k_dmist_dmis5 = 0x20U, /**< Channel 5 interrupt status (bit 5) */
  k_dmist_dmis6 = 0x40U, /**< Channel 6 interrupt status (bit 6) */
  k_dmist_dmis7 = 0x80U, /**< Channel 7 interrupt status (bit 7) */
} dmist_bits_t;

/* =============================================================================
 * Module Stop Control
 * =============================================================================
 */

/**
 * @enum dmac_mstpcr_t
 * @brief DMAC module stop control bits
 *
 * @details
 * The DMAC is controlled by MSTPCRA.MSTPA28.
 * Must be cleared (0) before accessing DMAC registers.
 *
 * @note Manual ref: Ch11 (Low Power Consumption) module stop tables
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief MSTPCRA bit for DMAC (bit 28)
   * @details
   * - 0: DMAC module is running
   * - 1: DMAC module is stopped (default)
   */
  k_dmac_mstpcra_bit = (1U << 28U),
} dmac_mstpcr_t;

/* =============================================================================
 * Register Accessor Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to DMAC channel registers
 *
 * @param channel Channel number (0-7)
 * @return Volatile pointer to channel register structure
 *
 * @pre channel < k_dmac_channel_count (8)
 * @pre DMAC module stop bit (MSTPCRA.MSTPA28) must be cleared
 *
 * @note Thread Safety: Safe - returns constant hardware address
 *
 * @par Example:
 * @code{.c}
 * // Configure channel 0 for memory-to-memory transfer
 * volatile rx_dmac_channel_regs_t* ch0 = dmac_channel(k_dmac_channel_0);
 * ch0->dmsar = (uint32_t)src_buffer;
 * ch0->dmdar = (uint32_t)dst_buffer;
 * ch0->dmcra = transfer_count;
 * ch0->dmtmd = k_dmtmd_md_normal | k_dmtmd_sz_32bit | k_dmtmd_dctg_software;
 * ch0->dmamd = k_dmamd_sm_increment | k_dmamd_dm_increment;
 * ch0->dmcnt = k_dmcnt_dte;  // Enable transfer
 * @endcode
 *
 * @since Version 1.0.0
 */
static inline volatile rx_dmac_channel_regs_t* dmac_channel(uint8_t channel)
{
  return (volatile rx_dmac_channel_regs_t*)(k_dmac_base_addr + (channel * k_dmac_channel_size));
}

/**
 * @brief Get pointer to DMAC channel 0 registers
 * @return Volatile pointer to DMAC0 register structure
 * @since Version 1.0.0
 */
static inline volatile rx_dmac_channel_regs_t* dmac0(void)
{
  return (volatile rx_dmac_channel_regs_t*)k_dmac0_base_addr;
}

/**
 * @brief Get pointer to DMAC channel 1 registers
 * @return Volatile pointer to DMAC1 register structure
 * @since Version 1.0.0
 */
static inline volatile rx_dmac_channel_regs_t* dmac1(void)
{
  return (volatile rx_dmac_channel_regs_t*)k_dmac1_base_addr;
}

/**
 * @brief Get pointer to DMAC channel 2 registers
 * @return Volatile pointer to DMAC2 register structure
 * @since Version 1.0.0
 */
static inline volatile rx_dmac_channel_regs_t* dmac2(void)
{
  return (volatile rx_dmac_channel_regs_t*)k_dmac2_base_addr;
}

/**
 * @brief Get pointer to DMAC channel 3 registers
 * @return Volatile pointer to DMAC3 register structure
 * @since Version 1.0.0
 */
static inline volatile rx_dmac_channel_regs_t* dmac3(void)
{
  return (volatile rx_dmac_channel_regs_t*)k_dmac3_base_addr;
}

/**
 * @brief Get pointer to DMAC channel 4 registers
 * @return Volatile pointer to DMAC4 register structure
 * @since Version 1.0.0
 */
static inline volatile rx_dmac_channel_regs_t* dmac4(void)
{
  return (volatile rx_dmac_channel_regs_t*)k_dmac4_base_addr;
}

/**
 * @brief Get pointer to DMAC channel 5 registers
 * @return Volatile pointer to DMAC5 register structure
 * @since Version 1.0.0
 */
static inline volatile rx_dmac_channel_regs_t* dmac5(void)
{
  return (volatile rx_dmac_channel_regs_t*)k_dmac5_base_addr;
}

/**
 * @brief Get pointer to DMAC channel 6 registers
 * @return Volatile pointer to DMAC6 register structure
 * @since Version 1.0.0
 */
static inline volatile rx_dmac_channel_regs_t* dmac6(void)
{
  return (volatile rx_dmac_channel_regs_t*)k_dmac6_base_addr;
}

/**
 * @brief Get pointer to DMAC channel 7 registers
 * @return Volatile pointer to DMAC7 register structure
 * @since Version 1.0.0
 */
static inline volatile rx_dmac_channel_regs_t* dmac7(void)
{
  return (volatile rx_dmac_channel_regs_t*)k_dmac7_base_addr;
}

/**
 * @brief Get pointer to DMAC Module Start Register (DMAST)
 * @return Volatile pointer to DMAST register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* dmac_dmast_reg(void)
{
  return (volatile uint8_t*)k_dmac_dmast_addr;
}

/**
 * @brief Get pointer to DMAC74 Interrupt Status Monitor Register (DMIST)
 * @return Volatile pointer to DMIST register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* dmac_dmist_reg(void)
{
  return (volatile uint8_t*)k_dmac_dmist_addr;
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
static_assert(k_dmac_base_addr == 0x00082000U,
              "DMAC base address must be 0x00082000 per Ch18 manual");

static_assert(k_dmac0_base_addr == 0x00082000U,
              "DMAC0 base address must be 0x00082000 per Ch18 manual");

static_assert(k_dmac1_base_addr == 0x00082040U,
              "DMAC1 base address must be 0x00082040 per Ch18 manual");

static_assert(k_dmac7_base_addr == 0x000821C0U,
              "DMAC7 base address must be 0x000821C0 per Ch18 manual");

/* Verify channel spacing */
static_assert(k_dmac_channel_size == 0x40U, "DMAC channel spacing must be 0x40 (64 bytes)");

static_assert(k_dmac1_base_addr - k_dmac0_base_addr == k_dmac_channel_size,
              "Channel 0 to 1 spacing must be 0x40");

static_assert(k_dmac7_base_addr - k_dmac0_base_addr == 7 * k_dmac_channel_size,
              "Channel 0 to 7 spacing must be 7 * 0x40");

/* Verify shared register addresses */
static_assert(k_dmac_dmast_addr == 0x00082200U, "DMAST address must be 0x00082200 per Ch18 manual");

static_assert(k_dmac_dmist_addr == 0x00082204U, "DMIST address must be 0x00082204 per Ch18 manual");

/* Verify register offsets */
static_assert(k_dmac_offset_dmsar == 0x00U, "DMSAR offset must be 0x00 per Ch18 manual");

static_assert(k_dmac_offset_dmdar == 0x04U, "DMDAR offset must be 0x04 per Ch18 manual");

static_assert(k_dmac_offset_dmcra == 0x08U, "DMCRA offset must be 0x08 per Ch18 manual");

static_assert(k_dmac_offset_dmcrb == 0x0CU, "DMCRB offset must be 0x0C per Ch18 manual");

static_assert(k_dmac_offset_dmtmd == 0x10U, "DMTMD offset must be 0x10 per Ch18 manual");

static_assert(k_dmac_offset_dmint == 0x13U, "DMINT offset must be 0x13 per Ch18 manual");

static_assert(k_dmac_offset_dmamd == 0x14U, "DMAMD offset must be 0x14 per Ch18 manual");

static_assert(k_dmac_offset_dmofr == 0x18U, "DMOFR offset must be 0x18 per Ch18 manual");

static_assert(k_dmac_offset_dmcnt == 0x1CU, "DMCNT offset must be 0x1C per Ch18 manual");

static_assert(k_dmac_offset_dmreq == 0x1DU, "DMREQ offset must be 0x1D per Ch18 manual");

static_assert(k_dmac_offset_dmsts == 0x1EU, "DMSTS offset must be 0x1E per Ch18 manual");

static_assert(k_dmac_offset_dmcsl == 0x1FU, "DMCSL offset must be 0x1F per Ch18 manual");

/* Verify structure layout matches manual offsets */
static_assert(sizeof(rx_dmac_channel_regs_t) == k_dmac_channel_size,
              "DMAC channel struct size must be 0x40 (64 bytes)");

static_assert(offsetof(rx_dmac_channel_regs_t, dmsar) == k_dmac_offset_dmsar,
              "DMSAR struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmdar) == k_dmac_offset_dmdar,
              "DMDAR struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmcra) == k_dmac_offset_dmcra,
              "DMCRA struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmcrb) == k_dmac_offset_dmcrb,
              "DMCRB struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmtmd) == k_dmac_offset_dmtmd,
              "DMTMD struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmint) == k_dmac_offset_dmint,
              "DMINT struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmamd) == k_dmac_offset_dmamd,
              "DMAMD struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmofr) == k_dmac_offset_dmofr,
              "DMOFR struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmcnt) == k_dmac_offset_dmcnt,
              "DMCNT struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmreq) == k_dmac_offset_dmreq,
              "DMREQ struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmsts) == k_dmac_offset_dmsts,
              "DMSTS struct offset must match manual");

static_assert(offsetof(rx_dmac_channel_regs_t, dmcsl) == k_dmac_offset_dmcsl,
              "DMCSL struct offset must match manual");

/** @} */

#ifdef __cplusplus
}
#endif
