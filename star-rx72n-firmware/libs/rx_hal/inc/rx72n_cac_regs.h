/**
 * @file rx72n_cac_regs.h
 * @brief RX72N CAC (Clock Frequency Accuracy Measurement Circuit) Register Definitions
 *
 * @details
 * Register definitions for the Clock Frequency Accuracy Measurement Circuit
 * (CAC) module on the RX72N microcontroller. CAC measures the frequency of a
 * target clock source against a reference clock source and raises an interrupt
 * (FERRF / MENDF / OVFF) when the ratio drifts outside configured bounds.
 *
 * @par Feature Summary
 * - 16-bit up-counter clocked by the measurement target clock
 * - Counter is sampled on the edge of the reference clock
 * - Upper-limit (CAULVR) / lower-limit (CALLVR) comparators
 * - FERRF flag + interrupt on frequency-error
 * - MENDF flag + interrupt on measurement-end
 * - OVFF flag + interrupt on counter overflow
 * - Optional digital filter on the external reference clock input (CACREF)
 *
 * @par Use Case in STAR
 * MOSC (24 MHz crystal) is measured against HOCO (16 MHz internal RC).  A
 * drifted crystal (aging, mechanical damage, PCB leakage) is detected before
 * it corrupts motor control timing; the FERRF ISR drives the motor e-stop
 * path.
 *
 * @par Hardware Requirements
 * | Parameter          | Value              | Notes                              |
 * |--------------------|--------------------|-----------------------------------|
 * | Base Address       | 0x0008B000         | 11-byte register block            |
 * | Module Stop Bit    | MSTPCRC.MSTPC19    | 1 = stopped, 0 = active           |
 * | Group Interrupt    | GROUPBL0 (vec 110) | Bits: FERRF=26, MENDF=27, OVFF=28 |
 * | Reset              | PORF               | All registers cleared on reset    |
 *
 * @par Manual Reference
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0111)
 * - Chapter 10: Clock Frequency Accuracy Measurement Circuit (CAC)
 * - Section 10.2: Register Descriptions, pp.392-397
 * - Address summary: p.236
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] N/A (no loops in register definitions)
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] All accessor functions are single-statement
 * - Rule 5: [OK] N/A (hardware layer)
 * - Rule 6: [OK] Minimal scope
 * - Rule 7: [OK] N/A (no return values to check)
 * - Rule 8: [OK] All constants use C23 typed enums
 * - Rule 9: [OK] No function pointers
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @see rx_cac.h Higher-level CAC driver API
 * @see rx72n_system_regs.h MSTPCRC and PRCR accessors
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
 * CAC Base Address (RX72N HW Manual p.236)
 * =============================================================================
 */

/**
 * @enum rx_cac_addresses_t
 * @brief CAC base address constant
 *
 * @details
 * Base address for the Clock Frequency Accuracy Measurement Circuit register
 * block at 0x0008B000 (RX72N HW Manual R01UH0824EJ0111, p.236 address summary).
 *
 * @par Memory Map (RX72N HW Manual Ch10.2, pp.392-397)
 * | Address    | Register | Size | Description                         |
 * |------------|----------|------|-------------------------------------|
 * | 0x0008B000 | CACR0    | 1    | CAC Control Register 0              |
 * | 0x0008B001 | CACR1    | 1    | CAC Control Register 1              |
 * | 0x0008B002 | CACR2    | 1    | CAC Control Register 2              |
 * | 0x0008B003 | CAICR    | 1    | CAC Interrupt Control Register      |
 * | 0x0008B004 | CASTR    | 1    | CAC Status Register                 |
 * | 0x0008B005 | -        | 1    | Reserved                            |
 * | 0x0008B006 | CAULVR   | 2    | CAC Upper-Limit Value Register      |
 * | 0x0008B008 | CALLVR   | 2    | CAC Lower-Limit Value Register      |
 * | 0x0008B00A | CACNTBR  | 2    | CAC Counter Buffer Register (RO)    |
 *
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /**
   * @brief CAC register base address (0x0008B000)
   * @details Verified against RX72N HW Manual R01UH0824EJ0111 p.236.
   */
  k_cac_base_addr = 0x0008B000,
} rx_cac_addresses_t;

/**
 * @enum cac_reserved_sizes_t
 * @brief CAC register reserved field sizes
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_reserved_after_castr_bytes = 1, /**< Reserved byte @ 0x05, between CASTR and CAULVR */
} cac_reserved_sizes_t;

/* =============================================================================
 * CAC Register Map Structure
 * =============================================================================
 */

/**
 * @struct rx_cac_regs_t
 * @brief CAC Register Map Structure
 *
 * @details
 * Clock Frequency Accuracy Measurement Circuit registers, laid out to match
 * the hardware memory map described in the RX72N HW Manual Ch10.2 (pp.392-397).
 * Packed to guarantee byte offsets; all fields volatile because hardware can
 * update them (CASTR, CACNTBR) outside of program control.
 *
 * @par Memory Layout Table
 * | Offset | Size | Field      | Type     | Description                           |
 * |--------|------|------------|----------|---------------------------------------|
 * | 0x00   | 1    | cacr0      | uint8_t  | CAC Control Register 0 (p.393)        |
 * | 0x01   | 1    | cacr1      | uint8_t  | CAC Control Register 1 (p.393)        |
 * | 0x02   | 1    | cacr2      | uint8_t  | CAC Control Register 2 (p.394)        |
 * | 0x03   | 1    | caicr      | uint8_t  | CAC Interrupt Control Register (p.395)|
 * | 0x04   | 1    | castr      | uint8_t  | CAC Status Register (p.395)           |
 * | 0x05   | 1    | reserved0  | -        | Reserved                              |
 * | 0x06   | 2    | caulvr     | uint16_t | CAC Upper-Limit Value Register (p.396)|
 * | 0x08   | 2    | callvr     | uint16_t | CAC Lower-Limit Value Register (p.397)|
 * | 0x0A   | 2    | cacntbr    | uint16_t | CAC Counter Buffer Register (p.397)   |
 * | **Total** | **12** |     |          |                                       |
 *
 * @invariant sizeof(rx_cac_regs_t) == 12 bytes
 * @invariant Offsets match RX72N HW Manual R01UH0824EJ0111 Ch10.2
 *
 * @see cac() Accessor function
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed)) {
  /**
   * @brief CAC Control Register 0 (CACR0) @ offset 0x00
   * @details CFME bit (0) enables/disables frequency measurement (p.393).
   * @see cac_cacr0_bits_t Bit definitions
   */
  volatile uint8_t cacr0;

  /**
   * @brief CAC Control Register 1 (CACR1) @ offset 0x01
   * @details Selects measured-clock source, edge, and enables limit comparison.
   * RX72N HW Manual p.393.
   * @see cac_cacr1_bits_t Bit definitions
   */
  volatile uint8_t cacr1;

  /**
   * @brief CAC Control Register 2 (CACR2) @ offset 0x02
   * @details Reference-clock source, reference-clock divider, and CACREF
   * digital-filter select. RX72N HW Manual p.394.
   * @see cac_cacr2_bits_t Bit definitions
   */
  volatile uint8_t cacr2;

  /**
   * @brief CAC Interrupt Control Register (CAICR) @ offset 0x03
   * @details Individual interrupt enables (FERRIE/MENDIE/OVFIE) plus status-
   * flag clear bits (FERRFCL/MENDFCL/OVFFCL). RX72N HW Manual p.395.
   * @see cac_caicr_bits_t Bit definitions
   */
  volatile uint8_t caicr;

  /**
   * @brief CAC Status Register (CASTR) @ offset 0x04
   * @details Read-only; hardware sets FERRF (bit 0), MENDF (bit 1), OVFF
   * (bit 2). Software clears via CAICR. RX72N HW Manual p.395.
   * @see cac_castr_bits_t Bit definitions
   */
  volatile uint8_t castr;

  uint8_t reserved0[k_cac_reserved_after_castr_bytes]; /**< Reserved @ 0x05 */

  /**
   * @brief CAC Upper-Limit Value Register (CAULVR) @ offset 0x06
   * @details 16-bit upper comparison threshold. When the sampled counter
   * exceeds this value, FERRF is set. RX72N HW Manual p.396.
   * @warning Write only while CACR0.CFME = 0.
   */
  volatile uint16_t caulvr;

  /**
   * @brief CAC Lower-Limit Value Register (CALLVR) @ offset 0x08
   * @details 16-bit lower comparison threshold. When the sampled counter
   * is below this value, FERRF is set. RX72N HW Manual p.397.
   * @warning Write only while CACR0.CFME = 0.
   */
  volatile uint16_t callvr;

  /**
   * @brief CAC Counter Buffer Register (CACNTBR) @ offset 0x0A
   * @details 16-bit read-only snapshot of the last completed measurement.
   * RX72N HW Manual p.397.
   */
  volatile uint16_t cacntbr;
} rx_cac_regs_t;

/**
 * @brief Get pointer to CAC registers
 *
 * @details
 * Returns a volatile pointer to the CAC register structure at
 * k_cac_base_addr (0x0008B000).
 *
 * @return Volatile pointer to CAC register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @note Thread Safety: Safe - returns constant hardware address
 * @note This function is always inlined for zero overhead
 *
 * @see k_cac_base_addr Base address constant
 * @see rx_cac_init() Higher-level initialization
 * @since Version 1.0.0
 */
static inline volatile rx_cac_regs_t* cac(void)
{
  return (volatile rx_cac_regs_t*)k_cac_base_addr;
}

/* =============================================================================
 * CACR0 Bit Definitions (RX72N HW Manual p.393)
 * =============================================================================
 */

/**
 * @enum cac_cacr0_bits_t
 * @brief CAC Control Register 0 (CACR0) bit values
 *
 * @details
 * CACR0 has a single active bit - CFME (Clock Frequency Measurement Enable).
 * All other bits are reserved and must be written 0.
 *
 * @par Register Layout (8-bit)
 * | Bits | Field    | R/W | Description                              |
 * |------|----------|-----|------------------------------------------|
 * | 0    | CFME     | R/W | 0 = measurement disabled, 1 = enabled    |
 * | 7:1  | Reserved | R   | Read as 0, write 0                       |
 *
 * @see rx_cac_regs_t::cacr0
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_cacr0_cfme_pos   = 0,         /**< CFME bit position */
  k_cac_cacr0_cfme_mask  = (1U << 0), /**< CFME bit mask */
  k_cac_cacr0_cfme_stop  = 0x00,      /**< Measurement disabled (CFME=0) */
  k_cac_cacr0_cfme_start = (1U << 0), /**< Measurement enabled (CFME=1) */
} cac_cacr0_bits_t;

/* =============================================================================
 * CACR1 Bit Definitions (RX72N HW Manual p.393)
 * =============================================================================
 */

/**
 * @enum cac_cacr1_shifts_t
 * @brief CAC Control Register 1 (CACR1) field shift positions
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_cacr1_caie_pos    = 0, /**< CAIE  - CAC interrupt enable bit */
  k_cac_cacr1_fmcs_shift  = 1, /**< FMCS[2:0] - Measurement target clock select */
  k_cac_cacr1_tcss_shift  = 4, /**< TCSS[1:0] - Target-clock division select */
  k_cac_cacr1_edges_shift = 6, /**< EDGES[1:0] - Valid edge of CACREF */
} cac_cacr1_shifts_t;

/**
 * @enum cac_cacr1_bits_t
 * @brief CAC Control Register 1 (CACR1) bit values
 *
 * @details
 * Selects the measurement target clock (FMCS), its division (TCSS), the
 * sampling edge of CACREF (EDGES), and the CAC-interrupt enable (CAIE).
 *
 * @par Register Layout (8-bit) - RX72N HW Manual p.393
 * | Bits | Field | R/W | Description                                             |
 * |------|-------|-----|---------------------------------------------------------|
 * | 0    | CAIE  | R/W | 0 = IRQ disabled, 1 = enabled                           |
 * | 3:1  | FMCS  | R/W | Measurement target clock source select                  |
 * | 5:4  | TCSS  | R/W | Target-clock division (div 1/4/8/32)                    |
 * | 7:6  | EDGES | R/W | CACREF edge: rising / falling / both / (reserved)       |
 *
 * @par FMCS[2:0] target-clock options (RX72N HW Manual p.393)
 * | FMCS | Clock source   |
 * |------|---------------|
 * | 000  | Main clock    |
 * | 001  | Sub-clock     |
 * | 010  | HOCO          |
 * | 011  | LOCO          |
 * | 100  | Peripheral B  |
 * | 101  | IWDTCLK       |
 * | 110  | UCLK (USB)    |
 * | 111  | 25 MHz (CLKOUT25M) |
 *
 * @par TCSS[1:0] target-clock divider (RX72N HW Manual p.393)
 * | TCSS | Divider |
 * |------|---------|
 * | 00   | /1      |
 * | 01   | /4      |
 * | 10   | /8      |
 * | 11   | /32     |
 *
 * @par EDGES[1:0] CACREF valid edge (RX72N HW Manual p.393)
 * | EDGES | Edge            |
 * |-------|----------------|
 * | 00    | Rising edge     |
 * | 01    | Falling edge    |
 * | 10    | Both edges      |
 * | 11    | Reserved        |
 *
 * @see rx_cac_regs_t::cacr1
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* CAIE - CAC Interrupt Enable */
  k_cac_cacr1_caie_mask    = (1U << 0),
  k_cac_cacr1_caie_disable = 0x00,
  k_cac_cacr1_caie_enable  = (1U << 0),

  /* FMCS[2:0] - Measurement target clock select */
  k_cac_cacr1_fmcs_mask     = (0x07U << 1),
  k_cac_cacr1_fmcs_main     = (0x00U << 1), /**< Main clock (MOSC) */
  k_cac_cacr1_fmcs_sub      = (0x01U << 1), /**< Sub-clock oscillator */
  k_cac_cacr1_fmcs_hoco     = (0x02U << 1), /**< HOCO */
  k_cac_cacr1_fmcs_loco     = (0x03U << 1), /**< LOCO */
  k_cac_cacr1_fmcs_pclkb    = (0x04U << 1), /**< Peripheral clock B */
  k_cac_cacr1_fmcs_iwdtclk  = (0x05U << 1), /**< IWDT-dedicated clock */
  k_cac_cacr1_fmcs_uclk     = (0x06U << 1), /**< USB clock */
  k_cac_cacr1_fmcs_clkout25 = (0x07U << 1), /**< 25 MHz CLKOUT */

  /* TCSS[1:0] - Target-clock division select */
  k_cac_cacr1_tcss_mask   = (0x03U << 4),
  k_cac_cacr1_tcss_div_1  = (0x00U << 4),
  k_cac_cacr1_tcss_div_4  = (0x01U << 4),
  k_cac_cacr1_tcss_div_8  = (0x02U << 4),
  k_cac_cacr1_tcss_div_32 = (0x03U << 4),

  /* EDGES[1:0] - CACREF valid-edge select */
  k_cac_cacr1_edges_mask    = (0x03U << 6),
  k_cac_cacr1_edges_rising  = (0x00U << 6),
  k_cac_cacr1_edges_falling = (0x01U << 6),
  k_cac_cacr1_edges_both    = (0x02U << 6),
} cac_cacr1_bits_t;

/* =============================================================================
 * CACR2 Bit Definitions (RX72N HW Manual p.394)
 * =============================================================================
 */

/**
 * @enum cac_cacr2_shifts_t
 * @brief CAC Control Register 2 (CACR2) field shift positions
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_cacr2_rps_pos    = 0, /**< RPS   - Reference clock pin select */
  k_cac_cacr2_rscs_shift = 1, /**< RSCS[2:0] - Reference clock source select */
  k_cac_cacr2_rcds_shift = 4, /**< RCDS[1:0] - Reference-clock divider select */
  k_cac_cacr2_dfs_shift  = 6, /**< DFS[1:0]  - Digital filter select (CACREF) */
} cac_cacr2_shifts_t;

/**
 * @enum cac_cacr2_bits_t
 * @brief CAC Control Register 2 (CACR2) bit values
 *
 * @details
 * Selects the reference clock source (RSCS), its divider (RCDS), the
 * digital-filter tap on the external CACREF pin (DFS), and whether the
 * reference clock is taken from the external CACREF pin or the internal
 * source selected by RSCS (RPS).
 *
 * @par Register Layout (8-bit) - RX72N HW Manual p.394
 * | Bits | Field | R/W | Description                              |
 * |------|-------|-----|------------------------------------------|
 * | 0    | RPS   | R/W | 0 = internal RSCS source, 1 = CACREF pin |
 * | 3:1  | RSCS  | R/W | Internal reference source select         |
 * | 5:4  | RCDS  | R/W | Reference-clock divider                  |
 * | 7:6  | DFS   | R/W | Digital-filter enable for CACREF pin     |
 *
 * @par RSCS[2:0] internal reference source (RX72N HW Manual p.394)
 * | RSCS | Source        |
 * |------|--------------|
 * | 000  | Main clock   |
 * | 001  | Sub-clock    |
 * | 010  | HOCO         |
 * | 011  | LOCO         |
 * | 100  | Peripheral B |
 * | 101  | IWDTCLK      |
 *
 * @par RCDS[1:0] reference-clock divider (RX72N HW Manual p.394)
 * | RCDS | Divider |
 * |------|---------|
 * | 00   | /32     |
 * | 01   | /128    |
 * | 10   | /1024   |
 * | 11   | /8192   |
 *
 * @par DFS[1:0] digital-filter sampling (RX72N HW Manual p.394)
 * | DFS | Filter                                    |
 * |-----|-------------------------------------------|
 * | 00  | Filter disabled                           |
 * | 01  | Sample at target-clock rate               |
 * | 10  | Sample at target-clock / 4                |
 * | 11  | Sample at target-clock / 16               |
 *
 * @see rx_cac_regs_t::cacr2
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* RPS - Reference Clock Pin Select */
  k_cac_cacr2_rps_mask     = (1U << 0),
  k_cac_cacr2_rps_internal = 0x00,      /**< Use internal RSCS source */
  k_cac_cacr2_rps_cacref   = (1U << 0), /**< Use external CACREF pin */

  /* RSCS[2:0] - Reference-clock source select (ignored when RPS=1) */
  k_cac_cacr2_rscs_mask    = (0x07U << 1),
  k_cac_cacr2_rscs_main    = (0x00U << 1),
  k_cac_cacr2_rscs_sub     = (0x01U << 1),
  k_cac_cacr2_rscs_hoco    = (0x02U << 1),
  k_cac_cacr2_rscs_loco    = (0x03U << 1),
  k_cac_cacr2_rscs_pclkb   = (0x04U << 1),
  k_cac_cacr2_rscs_iwdtclk = (0x05U << 1),

  /* RCDS[1:0] - Reference-clock divider select (RX72N HW Manual p.394) */
  k_cac_cacr2_rcds_mask     = (0x03U << 4),
  k_cac_cacr2_rcds_div_32   = (0x00U << 4),
  k_cac_cacr2_rcds_div_128  = (0x01U << 4),
  k_cac_cacr2_rcds_div_1024 = (0x02U << 4),
  k_cac_cacr2_rcds_div_8192 = (0x03U << 4),

  /* DFS[1:0] - CACREF digital-filter sampling (RX72N HW Manual p.394) */
  k_cac_cacr2_dfs_mask     = (0x03U << 6),
  k_cac_cacr2_dfs_disabled = (0x00U << 6),
  k_cac_cacr2_dfs_div_1    = (0x01U << 6),
  k_cac_cacr2_dfs_div_4    = (0x02U << 6),
  k_cac_cacr2_dfs_div_16   = (0x03U << 6),
} cac_cacr2_bits_t;

/* =============================================================================
 * CAICR Bit Definitions (RX72N HW Manual p.395)
 * =============================================================================
 */

/**
 * @enum cac_caicr_bits_t
 * @brief CAC Interrupt Control Register (CAICR) bit values
 *
 * @details
 * Enables the individual interrupt sources (FERRIE/MENDIE/OVFIE) and provides
 * write-1-to-clear bits for the status flags in CASTR (FERRFCL/MENDFCL/OVFFCL).
 *
 * @par Register Layout (8-bit) - RX72N HW Manual p.395
 * | Bit | Field    | R/W | Description                                  |
 * |-----|----------|-----|----------------------------------------------|
 * | 0   | FERRIE   | R/W | Frequency-error interrupt enable             |
 * | 1   | MENDIE   | R/W | Measurement-end interrupt enable             |
 * | 2   | OVFIE    | R/W | Counter-overflow interrupt enable            |
 * | 3   | -        | R   | Reserved                                     |
 * | 4   | FERRFCL  | W1C | Write 1 to clear CASTR.FERRF                 |
 * | 5   | MENDFCL  | W1C | Write 1 to clear CASTR.MENDF                 |
 * | 6   | OVFFCL   | W1C | Write 1 to clear CASTR.OVFF                  |
 * | 7   | -        | R   | Reserved                                     |
 *
 * @see rx_cac_regs_t::caicr
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_caicr_ferrie_mask  = (1U << 0), /**< FERRIE - frequency-error IRQ enable */
  k_cac_caicr_mendie_mask  = (1U << 1), /**< MENDIE - measurement-end IRQ enable */
  k_cac_caicr_ovfie_mask   = (1U << 2), /**< OVFIE - overflow IRQ enable */
  k_cac_caicr_ferrfcl_mask = (1U << 4), /**< FERRFCL - write-1-clear FERRF */
  k_cac_caicr_mendfcl_mask = (1U << 5), /**< MENDFCL - write-1-clear MENDF */
  k_cac_caicr_ovffcl_mask  = (1U << 6), /**< OVFFCL  - write-1-clear OVFF */

  /**
   * @brief Mask covering all three write-1-clear bits
   * @details Write this value to CAICR to acknowledge all pending
   * frequency-error / measurement-end / overflow flags at once.
   */
  k_cac_caicr_clear_all_flags = (1U << 4) | (1U << 5) | (1U << 6),
} cac_caicr_bits_t;

/* =============================================================================
 * CASTR Bit Definitions (RX72N HW Manual p.395)
 * =============================================================================
 */

/**
 * @enum cac_castr_bits_t
 * @brief CAC Status Register (CASTR) bit values
 *
 * @details
 * Read-only status flags. Hardware sets each flag on its respective event;
 * software clears them by writing 1 to the corresponding bit in CAICR.
 *
 * @par Register Layout (8-bit) - RX72N HW Manual p.395
 * | Bit | Field | R   | Description                                  |
 * |-----|-------|-----|----------------------------------------------|
 * | 0   | FERRF | R   | 1 = counter outside [CALLVR, CAULVR]         |
 * | 1   | MENDF | R   | 1 = measurement completed                    |
 * | 2   | OVFF  | R   | 1 = counter overflowed (exceeded 0xFFFF)     |
 * | 7:3 | -     | R   | Reserved, read as 0                          |
 *
 * @see rx_cac_regs_t::castr
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_castr_ferrf_mask = (1U << 0), /**< FERRF - frequency-error flag */
  k_cac_castr_mendf_mask = (1U << 1), /**< MENDF - measurement-end flag */
  k_cac_castr_ovff_mask  = (1U << 2), /**< OVFF  - counter overflow flag */
} cac_castr_bits_t;

/* =============================================================================
 * Module Stop + Group Interrupt References (cross-reference only)
 * =============================================================================
 */

/**
 * @enum cac_mstp_info_t
 * @brief Location of CAC module-stop bit in MSTPCRC (RX72N HW Manual pp.410-411)
 *
 * @details
 * The CAC module is gated by MSTPCRC.MSTPC19 (bit 19 of the MSTPCRC register
 * at SYSTEM base + 0x18). Writing 1 stops the module, 0 enables it. Access
 * requires prior PRCR unlock (PRC1).
 *
 * @warning This is informational only - the driver uses the mstpcrc field
 * of rx_system_regs_t directly rather than a dedicated accessor.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_mstpc_bit = 19, /**< CAC is MSTPCRC.MSTPC19 (RX72N HW Manual pp.410-411) */
} cac_mstp_info_t;

/**
 * @enum cac_groupbl0_bits_t
 * @brief CAC interrupt vector bits inside GROUPBL0 (RX72N HW Manual pp.509-519)
 *
 * @details
 * CAC shares GROUPBL0 (interrupt vector 110) with several other peripherals.
 * The CAC-specific bits inside GRPBL0 / GCRBL0 / GENBL0 are:
 *
 * | Bit | Source |
 * |-----|--------|
 * | 26  | FERRF  |
 * | 27  | MENDF  |
 * | 28  | OVFF   |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_cac_groupbl0_vector = 110, /**< GROUPBL0 vector number */
  k_cac_groupbl0_ferrf  = 26,  /**< FERRF bit inside GROUPBL0 */
  k_cac_groupbl0_mendf  = 27,  /**< MENDF bit inside GROUPBL0 */
  k_cac_groupbl0_ovff   = 28,  /**< OVFF  bit inside GROUPBL0 */
} cac_groupbl0_bits_t;

/* =============================================================================
 * Static Assertions - Verify register layout at compile time
 * =============================================================================
 */

/* The k_cac_base_addr enum declaration above is itself the contract for the
 * CAC base address (per Hardware Manual). A static_assert that re-states the
 * same literal here would duplicate the magic number without adding a real
 * cross-check, so we rely on the enum declaration alone. */

static_assert(sizeof(rx_cac_regs_t) == 12, "CAC register structure size incorrect");
static_assert(offsetof(rx_cac_regs_t, cacr0) == 0x00, "CACR0 offset incorrect");
static_assert(offsetof(rx_cac_regs_t, cacr1) == 0x01, "CACR1 offset incorrect");
static_assert(offsetof(rx_cac_regs_t, cacr2) == 0x02, "CACR2 offset incorrect");
static_assert(offsetof(rx_cac_regs_t, caicr) == 0x03, "CAICR offset incorrect");
static_assert(offsetof(rx_cac_regs_t, castr) == 0x04, "CASTR offset incorrect");
static_assert(offsetof(rx_cac_regs_t, caulvr) == 0x06, "CAULVR offset incorrect");
static_assert(offsetof(rx_cac_regs_t, callvr) == 0x08, "CALLVR offset incorrect");
static_assert(offsetof(rx_cac_regs_t, cacntbr) == 0x0A, "CACNTBR offset incorrect");

#ifdef __cplusplus
}
#endif
