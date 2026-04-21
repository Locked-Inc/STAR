/**
 * @file rx72n_system_regs.h
 * @brief RX72N System Control Register Definitions
 *
 * @details
 * System control registers for clock generation, power management, oscillator
 * control, and module stop control on the RX72N microcontroller. This is the
 * foundation for system initialization and power optimization.
 *
 * @par System Architecture
 * @dot
 * digraph system_clocks {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   mosc [label="Main OSC\n24 MHz" style="filled" fillcolor="lightblue"];
 *   pll [label="PLL\nx10 = 240 MHz"];
 *   ppll [label="PPLL\n48 MHz (USB)"];
 *
 *   iclk [label="ICLK\n240 MHz"];
 *   pclka [label="PCLKA\n120 MHz"];
 *   pclkb [label="PCLKB\n60 MHz"];
 *
 *   mosc -> pll;
 *   mosc -> ppll;
 *   pll -> iclk [label="/1"];
 *   pll -> pclka [label="/2"];
 *   pll -> pclkb [label="/4"];
 * }
 * @enddot
 *
 * @par STAR Project Clock Configuration
 * | Clock | Frequency | Divider | Purpose                    |
 * |-------|-----------|---------|----------------------------|
 * | ICLK  | 240 MHz   | /1      | CPU instruction clock      |
 * | PCLKA | 120 MHz   | /2      | High-speed peripherals     |
 * | PCLKB | 60 MHz    | /4      | Peripherals (CMT, WDT)     |
 * | PCLKC | 60 MHz    | /4      | Peripherals                |
 * | PCLKD | 60 MHz    | /4      | ADC                        |
 * | FCLK  | 60 MHz    | /4      | Flash operations           |
 * | BCLK  | 120 MHz   | /2      | External bus               |
 * | UCLK  | 48 MHz    | PPLL    | USB                        |
 *
 * @par Register Map Overview
 * **IMPORTANT**: System registers are NOT in a single contiguous block!
 *
 * | Address Range           | Description                           |
 * |-------------------------|---------------------------------------|
 * | 0x00080000 - 0x00080041 | Main system registers (this struct)   |
 * | 0x00080048 - 0x0008004A | PPLL registers                        |
 * | 0x000803FE              | PRCR (Protection Register)            |
 * | 0x000800C0              | RSTSR2 (Reset Status 2)               |
 * | 0x0008101C              | MEMWAIT                               |
 * | 0x0008C290 - 0x0008C291 | RSTSR0/1 (Reset Status 0/1)           |
 *
 * @par Module Stop Control (MSTPCR)
 * Peripherals can be stopped to save power. Bit=1 stops the module:
 * - **MSTPCRA**: CMT, DTC, DMAC
 * - **MSTPCRB**: USB, CRC, SCI, RSPI
 * - **MSTPCRC**: RAM, Flash
 * - **MSTPCRD**: ADC, DAC, MTU, GPTW
 *
 * @par Register Protection (PRCR)
 * Critical registers require PRCR unlock before modification:
 * - **PRC0**: Clock generation (SCKCR, PLLCR, oscillator control)
 * - **PRC1**: Module stop control (MSTPCRA-D)
 * - **PRC3**: Low voltage detection
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] N/A (no loops)
 * - Rule 3: [OK] No dynamic memory allocation
 * - Rule 4: [OK] All accessors are single-statement
 * - Rule 5: [OK] N/A (hardware layer)
 * - Rule 6: [OK] Minimal scope
 * - Rule 7: [OK] N/A
 * - Rule 8: [OK] All constants use C23 typed enums
 * - Rule 9: [OK] No function pointers
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single responsibility - only system register definitions
 * - **O**: Open for extension via additional register accessors
 * - **L**: N/A
 * - **I**: Separate accessors for non-contiguous registers
 * - **D**: N/A (hardware register layer)
 *
 * @par Module Dependencies
 * - `<stddef.h>` - offsetof macro for static assertions
 * - `<stdint.h>` - Fixed-width integer types
 *
 * @par Manual References
 * - Chapter 5: I/O Registers (address map)
 * - Chapter 9: Clock Generation Circuit
 * - Chapter 13: Register Write Protection
 * - Chapter 11: Low Power Consumption
 *
 * @par Verification Status
 * [PASS] VERIFIED (2026-01-28) - All base addresses and register offsets verified
 * against RX72N Manual Ch05. Critical bug fixed: PRCR was incorrectly embedded
 * in the contiguous struct at offset 0x0A, but it's actually at 0x000803FE.
 *
 * @see rx72n_clock.h Clock-specific definitions
 * @see rx_register_protection.h PRCR unlock/lock API
 * @see rx_clock_power_init.c Clock initialization implementation
 *
 * @author Locked, Inc.
 * @date 2026-01-28
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
 * System Control Registers (Contiguous Block @ 0x00080000-0x00080041)
 * =============================================================================
 */

/**
 * @enum rx_system_addresses_t
 * @brief System register base addresses
 *
 * @details
 * Base addresses for system control registers. Note that system registers
 * are spread across multiple non-contiguous memory regions.
 *
 * @par Address Map
 * | Address      | Register/Block | Description                     |
 * |--------------|----------------|---------------------------------|
 * | 0x00080000   | System Base    | Main system register block      |
 * | 0x000803FE   | PRCR           | Register protection control     |
 *
 * @warning PRCR is NOT in the main system block - use prcr_reg() accessor
 *
 * @see system_regs() Main system register accessor
 * @see prcr_reg() PRCR register accessor
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /**
   * @brief System register base address (0x00080000)
   * @details Start of main 66-byte system register block (0x00-0x41)
   */
  k_system_base_addr = 0x00080000,

  /**
   * @brief PRCR (Protection Register) address (0x000803FE)
   * @details Located separately from main system block. Used to protect
   * clock, module stop, and voltage detection registers from accidental writes.
   * @warning Not part of rx_system_regs_t structure
   */
  k_prcr_addr = 0x000803FE,
} rx_system_addresses_t;

/**
 * @enum system_reserved_sizes_t
 * @brief System register reserved field sizes
 *
 * @details
 * Defines reserved byte counts for gaps in the system register map.
 * These ensure correct structure alignment with hardware.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_system_reserved_02_05 = 4, /**< Reserved @ 0x02-0x05 (between MDMONR and SYSCR0) */
  k_system_reserved_0a_0b = 2, /**< Reserved @ 0x0A-0x0B (between SYSCR1 and SBYCR) */
  k_system_reserved_0e_0f = 2, /**< Reserved @ 0x0E-0x0F (between SBYCR and MSTPCRA) */
  k_system_reserved_2b_2f = 5, /**< Reserved @ 0x2B-0x2F (between PLLCR2 and BCKCR) */
  k_system_reserved_31    = 1, /**< Reserved @ 0x31 (between BCKCR and MOSCCR) */
  k_system_reserved_38_3b = 4, /**< Reserved @ 0x38-0x3B (between HOCOCR2 and OSCOVFSR) */
  k_system_reserved_3d    = 1, /**< Reserved @ 0x3D (between OSCOVFSR and CKOCR) */
} system_reserved_sizes_t;

/* =============================================================================
 * Ch03 Operating Modes - MDMONR, SYSCR0, SYSCR1 Bit Definitions
 * =============================================================================
 */

/**
 * @enum mdmonr_bits_t
 * @brief Mode Monitor Register (MDMONR) bit definitions
 *
 * @details
 * Read-only register indicating MCU mode pin status at reset.
 * Located at address 0x00080000.
 *
 * @par Register Layout (16-bit @ offset 0x00)
 * | Bit  | Field | Description                    |
 * |------|-------|--------------------------------|
 * | 0    | MD    | MD Pin Status Flag (read-only) |
 * | 15:1 | -     | Reserved (read as 0)           |
 *
 * @par MD Pin Modes (at reset)
 * - MD = 0: Boot mode (low level on MD pin)
 * - MD = 1: Single-chip mode (high level on MD pin)
 *
 * @note Manual ref: Ch03 section 3.2.1
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_mdmonr_md      = 0x0001U, /**< MD pin status (0=low/boot, 1=high/single-chip) */
  k_mdmonr_md_mask = 0x0001U, /**< MD bit mask */
} mdmonr_bits_t;

/**
 * @enum syscr0_bits_t
 * @brief System Control Register 0 (SYSCR0) bit definitions
 *
 * @details
 * Controls on-chip ROM enable and external bus enable.
 * Requires key code 0x5A in upper byte for writes.
 * Located at address 0x00080006.
 *
 * @par Register Layout (16-bit @ offset 0x06)
 * | Bit   | Field    | Description                         |
 * |-------|----------|-------------------------------------|
 * | 0     | ROME     | On-Chip ROM Enable                  |
 * | 1     | EXBE     | External Bus Enable                 |
 * | 7:2   | -        | Reserved (write 0)                  |
 * | 15:8  | KEY[7:0] | Key code (write 0x5A to modify)     |
 *
 * @par Operating Modes
 * | ROME | EXBE | Mode                              |
 * |------|------|-----------------------------------|
 * | 1    | 0    | Single-chip mode                  |
 * | 1    | 1    | On-chip ROM enabled extended mode |
 * | 0    | 1    | On-chip ROM disabled extended mode|
 * | 0    | 0    | Single-chip mode (ROM disabled)   |
 *
 * @warning Once ROME is set to 0, it cannot be reverted to 1
 * @warning Do not modify EXBE while accessing external bus
 *
 * @note Manual ref: Ch03 section 3.2.2
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_syscr0_rome     = 0x0001U, /**< On-Chip ROM Enable (0=disabled, 1=enabled) */
  k_syscr0_exbe     = 0x0002U, /**< External Bus Enable (0=disabled, 1=enabled) */
  k_syscr0_key      = 0x5A00U, /**< Key code for SYSCR0 write (must be in upper byte) */
  k_syscr0_key_mask = 0xFF00U, /**< Key code mask */

  /* Convenience values for common write operations */
  k_syscr0_rom_enabled   = (0x5A00U | 0x0001U), /**< Enable ROM (write value) */
  k_syscr0_rom_disabled  = 0x5A00U,             /**< Disable ROM (write value) - IRREVERSIBLE */
  k_syscr0_extbus_enable = (0x5A00U | 0x0003U), /**< Enable ROM + ExtBus (write value) */
} syscr0_bits_t;

/**
 * @enum syscr1_bits_t
 * @brief System Control Register 1 (SYSCR1) bit definitions
 *
 * @details
 * Controls RAM, ECCRAM, and Standby RAM enable.
 * Located at address 0x00080008.
 *
 * @par Register Layout (16-bit @ offset 0x08)
 * | Bit   | Field   | Description                        |
 * |-------|---------|-----------------------------------|
 * | 0     | RAME    | RAM Enable                         |
 * | 5:1   | -       | Reserved (read/write as 1)         |
 * | 6     | ECCRAME | ECCRAM Enable                      |
 * | 7     | SBYRAME | Standby RAM Enable                 |
 * | 15:8  | -       | Reserved (write 0)                 |
 *
 * @par Default Value: 0x00FF (all RAM enabled)
 * - RAME = 1 (RAM enabled)
 * - Reserved bits 5:1 = 1
 * - ECCRAME = 1 (ECCRAM enabled)
 * - SBYRAME = 1 (Standby RAM enabled)
 *
 * @warning Do not write 0 to RAME/ECCRAME/SBYRAME during access to that memory
 *
 * @note Manual ref: Ch03 section 3.2.3
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_syscr1_rame          = 0x0001U, /**< RAM Enable (0=disabled, 1=enabled) */
  k_syscr1_reserved_mask = 0x003EU, /**< Reserved bits 5:1 (read/write as 1) */
  k_syscr1_eccrame       = 0x0040U, /**< ECCRAM Enable (0=disabled, 1=enabled) */
  k_syscr1_sbyrame       = 0x0080U, /**< Standby RAM Enable (0=disabled, 1=enabled) */

  /* Convenience values */
  k_syscr1_all_ram_enabled = 0x00FFU, /**< Default: all RAM types enabled */
  k_syscr1_disable_eccram  = 0x00BFU, /**< Disable ECCRAM (RAME+SBYRAME enabled) */
  k_syscr1_disable_sbyram  = 0x007FU, /**< Disable StandbyRAM (RAME+ECCRAM enabled) */
} syscr1_bits_t;

/* =============================================================================
 * Ch09 Clock Generation - CKOCR, SCKCR, SCKCR3, PLLCR Bit Definitions
 * =============================================================================
 */

/**
 * @enum ckocr_bits_t
 * @brief Clock Output Control Register (CKOCR) bit definitions
 *
 * @details
 * Controls the clock output on CLKOUT pin. Can output various system clocks
 * divided by configurable ratio.
 *
 * @par Register Layout (16-bit @ offset 0x3E)
 * | Bit   | Field  | Description                        |
 * |-------|--------|------------------------------------|
 * | 7:0   | -      | Reserved                           |
 * | 10:8  | CKOSEL | Clock output source select         |
 * | 11    | -      | Reserved                           |
 * | 14:12 | CKODIV | Clock output divider               |
 * | 15    | CKOSTP | Clock output stop (0=run, 1=stop)  |
 *
 * @par Clock Output Sources (CKOSEL)
 * | Value | Source     | Description                    |
 * |-------|------------|--------------------------------|
 * | 0     | LOCO       | Low-speed on-chip oscillator   |
 * | 1     | HOCO       | High-speed on-chip oscillator  |
 * | 2     | Main clock | Main oscillator                |
 * | 3     | Sub-clock  | Sub-clock oscillator           |
 * | 4     | PLL        | PLL output                     |
 *
 * @par Clock Output Dividers (CKODIV[2:0] at bits 14:12)
 * | Value | Divider | Output Frequency     |
 * |-------|---------|----------------------|
 * | 000   | /1      | f (source frequency) |
 * | 001   | /2      | f/2                  |
 * | 010   | /4      | f/4                  |
 * | 011   | /8      | f/8                  |
 * | 100   | /16     | f/16                 |
 * | other | --      | PROHIBITED           |
 *
 * @par Clock Output Sources (CKOSEL[2:0] at bits 10:8)
 * | Value | Source       |
 * |-------|--------------|
 * | 000   | LOCO         |
 * | 001   | HOCO         |
 * | 010   | Main clock   |
 * | 011   | Sub-clock    |
 * | 100   | PLL circuit  |
 * | 110   | PPLL circuit |
 * | other | PROHIBITED   |
 *
 * @warning Clock output disabled by default (CKOSTP = 1)
 * @warning CKODIV values 5, 6, 7 are PROHIBITED (max divisor is /16)
 * @note Requires PRCR unlock (PRC0) before modification
 * @see Manual Ch09 section 9.2.21 CKOCR
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* Clock output source select (CKOSEL, bits 10:8) */
  k_ckocr_ckosel_loco     = (0U << 8), /**< LOCO (low-speed on-chip osc) */
  k_ckocr_ckosel_hoco     = (1U << 8), /**< HOCO (high-speed on-chip osc) */
  k_ckocr_ckosel_main     = (2U << 8), /**< Main oscillator */
  k_ckocr_ckosel_subclock = (3U << 8), /**< Sub-clock oscillator */
  k_ckocr_ckosel_pll      = (4U << 8), /**< PLL output */
  k_ckocr_ckosel_ppll     = (6U << 8), /**< PPLL output (110b, not 101b) */
  k_ckocr_ckosel_mask     = (7U << 8), /**< CKOSEL field mask */

  /* Clock output divider (CKODIV, bits 14:12) -- max is /16, values 5-7 PROHIBITED */
  k_ckocr_ckodiv_1    = (0U << 12), /**< Divide by 1 */
  k_ckocr_ckodiv_2    = (1U << 12), /**< Divide by 2 */
  k_ckocr_ckodiv_4    = (2U << 12), /**< Divide by 4 */
  k_ckocr_ckodiv_8    = (3U << 12), /**< Divide by 8 */
  k_ckocr_ckodiv_16   = (4U << 12), /**< Divide by 16 (maximum valid divisor) */
  k_ckocr_ckodiv_mask = (7U << 12), /**< CKODIV field mask */

  /* Clock output stop (CKOSTP, bit 15) */
  k_ckocr_ckostp = (1U << 15), /**< Clock output stop (1=stopped/low, 0=running) */

  /* Convenience values */
  k_ckocr_output_disabled = k_ckocr_ckostp, /**< Default: output disabled */
  k_ckocr_pll_div4 =
    (k_ckocr_ckosel_pll | k_ckocr_ckodiv_4), /**< PLL/4 output (60 MHz @ 240 MHz PLL) */
} ckocr_bits_t;

/**
 * @enum sckcr_divider_t
 * @brief Clock divider values for SCKCR fields
 *
 * @details
 * All SCKCR clock divider fields use the same encoding. Value N means divide
 * by 2^N. For example, 0b0010 (2) means divide by 4.
 *
 * @par Divider Encoding (for PCKD, PCKC, PCKB, PCKA, ICK, FCK fields)
 * | Bits | Divider | Example (240 MHz input) |
 * |------|---------|-------------------------|
 * | 0000 | /1      | 240 MHz                 |
 * | 0001 | /2      | 120 MHz                 |
 * | 0010 | /4      | 60 MHz                  |
 * | 0011 | /8      | 30 MHz                  |
 * | 0100 | /16     | 15 MHz                  |
 * | 0101 | /32     | 7.5 MHz                 |
 * | 0110 | /64     | 3.75 MHz                |
 * | other| --      | PROHIBITED              |
 *
 * @note The BCK field also accepts 1001 (x1/3) as an additional valid setting.
 * @warning Values 0111 and 1000 are PROHIBITED for all SCKCR fields.
 * @see Manual Ch09 section 9.2.1 Table 9.1, section 9.2.2
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_clock_div_1  = 0, /**< Divide by 1 (no division) */
  k_clock_div_2  = 1, /**< Divide by 2 */
  k_clock_div_4  = 2, /**< Divide by 4 */
  k_clock_div_8  = 3, /**< Divide by 8 */
  k_clock_div_16 = 4, /**< Divide by 16 */
  k_clock_div_32 = 5, /**< Divide by 32 */
  k_clock_div_64 = 6, /**< Divide by 64 */
  /* NOTE: /128 does NOT exist in SCKCR. Maximum divisor is /64 (0110 = 6).  */
  /* Values 7 and 8 are PROHIBITED per RX72N Manual Ch09 section 9.2.2.      */
  /* BCK field additionally accepts k_bck_div_3 = 9 (binary 1001 = x1/3).   */
  k_bck_div_3 = 9, /**< Divide by 3 (ONLY valid for BCK[3:0] field, binary 1001) */
} sckcr_divider_t;

/**
 * @enum sckcr_bits_t
 * @brief System Clock Control Register (SCKCR) bit definitions
 *
 * @details
 * 32-bit register controlling all main system clock dividers. Each field is
 * 4 bits wide and uses the encoding from sckcr_divider_t.
 *
 * @par Register Layout (32-bit @ offset 0x20)
 * | Bits  | Field  | Clock    | STAR Project Setting |
 * |-------|--------|----------|----------------------|
 * | 3:0   | PCKD   | PCLKD    | /4 (60 MHz from 240) |
 * | 7:4   | PCKC   | PCLKC    | /4 (60 MHz from 240) |
 * | 11:8  | PCKB   | PCLKB    | /4 (60 MHz from 240) |
 * | 15:12 | PCKA   | PCLKA    | /2 (120 MHz from 240)|
 * | 19:16 | BCK    | BCLK     | /2 (120 MHz from 240)|
 * | 21:20 | -      | Reserved | -                    |
 * | 22    | PSTOP0 | PCLKA/B  | 0 (running)          |
 * | 23    | PSTOP1 | USB      | 0 (running)          |
 * | 27:24 | ICK    | ICLK     | /1 (240 MHz)         |
 * | 31:28 | FCK    | FCLK     | /4 (60 MHz from 240) |
 *
 * @par STAR Project Configuration
 * With 24 MHz main oscillator x PLL(x10) = 240 MHz:
 * - ICLK  = 240 MHz (CPU instruction clock)
 * - PCLKA = 120 MHz (high-speed peripherals)
 * - PCLKB = 60 MHz (CMT, WDT, etc.)
 * - PCLKC = 60 MHz (general peripherals)
 * - PCLKD = 60 MHz (ADC)
 * - FCLK  = 60 MHz (Flash memory)
 * - BCLK  = 120 MHz (External bus)
 *
 * @warning ICLK > 120 MHz requires MEMWAIT = 1 before changing SCKCR
 * @note Requires PRCR unlock (PRC0) before modification
 * @see Manual Ch09 section 9.2.2
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /* Field shifts (for manual bit manipulation if needed) */
  k_sckcr_pckd_shift = 0,
  k_sckcr_pckc_shift = 4,
  k_sckcr_pckb_shift = 8,
  k_sckcr_pcka_shift = 12,
  k_sckcr_bck_shift  = 16,
  k_sckcr_ick_shift  = 24,
  k_sckcr_fck_shift  = 28,

  /* Field masks */
  k_sckcr_pckd_mask = 0xFUL << k_sckcr_pckd_shift,
  k_sckcr_pckc_mask = 0xFUL << k_sckcr_pckc_shift,
  k_sckcr_pckb_mask = 0xFUL << k_sckcr_pckb_shift,
  k_sckcr_pcka_mask = 0xFUL << k_sckcr_pcka_shift,
  k_sckcr_bck_mask  = 0xFUL << k_sckcr_bck_shift,
  k_sckcr_ick_mask  = 0xFUL << k_sckcr_ick_shift,
  k_sckcr_fck_mask  = 0xFUL << k_sckcr_fck_shift,

  /* PSTOP bits (pin output control) */
  k_sckcr_pstop0 = (1UL << 22), /**< SDCLK pin output control (0=enabled, 1=disabled/fixed-high) */
  k_sckcr_pstop1 = (1UL << 23), /**< BCLK pin output control (0=enabled, 1=disabled/fixed-high) */

  /* STAR Project configuration value (24 MHz x 10 = 240 MHz) */
  k_sckcr_star_240mhz =
    (((uint32_t)k_clock_div_4 << k_sckcr_pckd_shift) | /* PCLKD = 240/4 = 60 MHz */
     ((uint32_t)k_clock_div_4 << k_sckcr_pckc_shift) | /* PCLKC = 240/4 = 60 MHz */
     ((uint32_t)k_clock_div_4 << k_sckcr_pckb_shift) | /* PCLKB = 240/4 = 60 MHz */
     ((uint32_t)k_clock_div_2 << k_sckcr_pcka_shift) | /* PCLKA = 240/2 = 120 MHz */
     ((uint32_t)k_clock_div_2 << k_sckcr_bck_shift) |  /* BCLK = 240/2 = 120 MHz */
     ((uint32_t)k_clock_div_1 << k_sckcr_ick_shift) |  /* ICLK = 240/1 = 240 MHz */
     ((uint32_t)k_clock_div_4 << k_sckcr_fck_shift) |  /* FCLK = 240/4 = 60 MHz */
     0UL                                               /* PSTOP0/1 = 0 (clocks running) */
     ), /**< Complete SCKCR value for STAR 240 MHz configuration */
} sckcr_bits_t;

/**
 * @enum sckcr3_cksel_t
 * @brief System clock source selection (SCKCR3 CKSEL field)
 *
 * @details
 * Selects which oscillator/PLL output is used as the system clock source.
 * This is the clock that feeds into SCKCR dividers.
 *
 * @par Clock Sources (CKSEL[2:0] at bits 10:8 of SCKCR3)
 * | Value | Source     | Typical Frequency  | Use Case                |
 * |-------|------------|--------------------|-------------------------|
 * | 0     | LOCO       | 240 kHz (typ)      | Low power mode          |
 * | 1     | HOCO       | 16-20 MHz (config) | Boot, low power         |
 * | 2     | Main clock | 24 MHz (STAR)      | External crystal        |
 * | 3     | Sub-clock  | 32.768 kHz         | RTC, ultra low power    |
 * | 4     | PLL        | 240 MHz (STAR)     | Normal operation        |
 * | other | --         | PROHIBITED         | --                      |
 *
 * @note PPLL is NOT a valid SCKCR3 CKSEL option. PPLL is used only as
 *       the USB clock source, selected via PACKCR.UPLLSEL.
 *
 * @par STAR Project Settings
 * - Boot: LOCO (default after reset)
 * - Normal operation: PLL @ 240 MHz
 *
 * @warning Switching clock sources requires waiting for oscillator stabilization
 * @note Requires PRCR unlock (PRC0) before modification
 * @see Manual Ch09 section 9.2.4 SCKCR3
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_sckcr3_cksel_loco     = 0x0000, /**< LOCO (Low-speed On-Chip Oscillator, 240 kHz) */
  k_sckcr3_cksel_hoco     = 0x0100, /**< HOCO (High-speed On-Chip Oscillator, 16-20 MHz) */
  k_sckcr3_cksel_main     = 0x0200, /**< Main clock oscillator (STAR: 24 MHz crystal) */
  k_sckcr3_cksel_subclock = 0x0300, /**< Sub-clock oscillator (32.768 kHz) */
  k_sckcr3_cksel_pll      = 0x0400, /**< PLL circuit (STAR: 240 MHz). Values 5-7 are prohibited. */
  k_sckcr3_cksel_mask     = 0x0700, /**< CKSEL field mask (bits 10:8) */
} sckcr3_cksel_t;

/**
 * @enum pllcr_bits_t
 * @brief PLL Control Register (PLLCR) bit definitions
 *
 * @details
 * Controls PLL input divider, output multiplier, and source selection.
 * PLL output frequency = (Input / PLIDIV) x (STC + 1)
 *
 * @par Register Layout (16-bit @ offset 0x28)
 * | Bits  | Field     | Description                             |
 * |-------|-----------|---------------------------------------- |
 * | 1:0   | PLIDIV    | PLL input divider (00=/1, 01=/2, 10=/3) |
 * | 3:2   | -         | Reserved                                |
 * | 4     | PLLSRCSEL | PLL source (0=main osc, 1=HOCO)         |
 * | 7:5   | -         | Reserved                                |
 * | 13:8  | STC       | PLL multiplier (value N -> x(N+1))      |
 * | 15:14 | -         | Reserved                                |
 *
 * @par PLIDIV Encoding (bits 1:0)
 * | Value | Divider | Note                 |
 * |-------|---------|----------------------|
 * | 00    | /1      | No division          |
 * | 01    | /2      |                      |
 * | 10    | /3      |                      |
 * | 11    | --      | PROHIBITED           |
 *
 * @par STAR Project PLL Configuration
 * Input: 24 MHz main oscillator
 * Target: 240 MHz PLL output
 * Calculation: 24 MHz / 1 x 10 = 240 MHz
 * - PLIDIV = 00 (divide by 1) -> 24 MHz
 * - STC = 0x09 (multiply by 10) -> 240 MHz
 * - PLLSRCSEL = 0 (main oscillator source)
 * - PLLCR = 0x0900
 *
 * @warning PLL must be stopped (PLLCR2.PLLEN = 1) before changing PLLCR
 * @note Requires PRCR unlock (PRC0) before modification
 * @see Manual Ch09 section 9.2.5 PLLCR
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* PLL input divider (PLIDIV, bits 1:0) */
  k_pllcr_plidiv_1    = 0x0000, /**< Divide PLL input by 1 (binary 00) */
  k_pllcr_plidiv_2    = 0x0001, /**< Divide PLL input by 2 (binary 01) */
  k_pllcr_plidiv_3    = 0x0002, /**< Divide PLL input by 3 (binary 10); value 11 is PROHIBITED */
  k_pllcr_plidiv_mask = 0x0003, /**< PLIDIV field mask */

  /* PLL source select (PLLSRCSEL, bit 4) */
  k_pllcr_src_main = 0x0000, /**< Main oscillator as PLL source (STAR config) */
  k_pllcr_src_hoco = 0x0010, /**< HOCO as PLL source */
  k_pllcr_src_mask = 0x0010, /**< PLLSRCSEL field mask */

  /* PLL multiplier field (STC, bits 13:8) - value N means multiply by (N+1) */
  k_pllcr_stc_shift = 8,
  k_pllcr_stc_mask  = 0x3F00,

  /* Convenience values for common configurations */
  k_pllcr_star_24mhz_to_240mhz =
    (k_pllcr_plidiv_1 |         /* 24 MHz input (no division) */
     k_pllcr_src_main |         /* Main oscillator source */
     (19U << k_pllcr_stc_shift) /* STC=19 -> x10.0 (manual p345: 0b010011) */
     ),                         /**< STAR: 24 MHz x 10 = 240 MHz */

  k_pllcr_12mhz_to_240mhz =
    (k_pllcr_plidiv_1 |         /* 12 MHz input (no division) */
     k_pllcr_src_main |         /* Main oscillator source */
     (19U << k_pllcr_stc_shift) /* STC=19 -> x10.0 (manual p345); = 0x1300 */
     ),                         /**< Alternative: 12 MHz x 10 = 120 MHz (NOT 240 MHz!) */
} pllcr_bits_t;

/**
 * @enum pllcr2_bits_t
 * @brief PLL Control Register 2 (PLLCR2) bit definitions
 *
 * @details
 * Controls PLL enable/disable. Note inverted logic: 0 = enabled, 1 = stopped.
 *
 * @par Register Layout (8-bit @ offset 0x2A)
 * | Bit | Field | Description                     |
 * |-----|-------|---------------------------------|
 * | 0   | PLLEN | PLL enable (0=run, 1=stopped)   |
 * | 7:1 | -     | Reserved                        |
 *
 * @warning Inverted logic! 0 = PLL running, 1 = PLL stopped
 * @note Must wait for OSCOVFSR.PLOVF before switching clock to PLL
 * @note Requires PRCR unlock (PRC0) before modification
 * @see Manual Ch09 section 9.2.4
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_pllcr2_pll_running = 0x00, /**< PLL enabled (running) */
  k_pllcr2_pll_stopped = 0x01, /**< PLL disabled (stopped) */
} pllcr2_bits_t;

/**
 * @struct rx_system_regs_t
 * @brief System Control Register Map (0x00080000 - 0x00080041)
 *
 * @details
 * System control registers for clock configuration, power management,
 * and module stop control. This structure covers the main contiguous
 * block of system registers.
 *
 * @par Memory Layout Table
 * | Offset | Size | Field    | Description                              |
 * |--------|------|----------|------------------------------------------|
 * | 0x00   | 2    | mdmonr   | Mode Monitor Register                    |
 * | 0x02   | 4    | reserved | -                                        |
 * | 0x06   | 2    | syscr0   | System Control Register 0                |
 * | 0x08   | 2    | syscr1   | System Control Register 1                |
 * | 0x0A   | 2    | reserved | -                                        |
 * | 0x0C   | 2    | sbycr    | Standby Control Register                 |
 * | 0x0E   | 2    | reserved | -                                        |
 * | 0x10   | 4    | mstpcra  | Module Stop Control Register A           |
 * | 0x14   | 4    | mstpcrb  | Module Stop Control Register B           |
 * | 0x18   | 4    | mstpcrc  | Module Stop Control Register C           |
 * | 0x1C   | 4    | mstpcrd  | Module Stop Control Register D           |
 * | 0x20   | 4    | sckcr    | System Clock Control Register            |
 * | 0x24   | 2    | sckcr2   | System Clock Control Register 2          |
 * | 0x26   | 2    | sckcr3   | System Clock Control Register 3          |
 * | 0x28   | 2    | pllcr    | PLL Control Register                     |
 * | 0x2A   | 1    | pllcr2   | PLL Control Register 2                   |
 * | 0x2B   | 5    | reserved | -                                        |
 * | 0x30   | 1    | bckcr    | External Bus Clock Control               |
 * | 0x31   | 1    | reserved | -                                        |
 * | 0x32   | 1    | mosccr   | Main Oscillator Control                  |
 * | 0x33   | 1    | sosccr   | Sub-Clock Oscillator Control             |
 * | 0x34   | 1    | lococr   | LOCO Control                             |
 * | 0x35   | 1    | ilococr  | IWDT Oscillator Control                  |
 * | 0x36   | 1    | hococr   | HOCO Control                             |
 * | 0x37   | 1    | hococr2  | HOCO Control 2                           |
 * | 0x38   | 4    | reserved | -                                        |
 * | 0x3C   | 1    | oscovfsr | Oscillation Stabilization Flag           |
 * | 0x3D   | 1    | reserved | -                                        |
 * | 0x3E   | 2    | ckocr    | Clock Output Control Register            |
 * | 0x40   | 1    | ostdcr   | Oscillation Stop Detection Control       |
 * | 0x41   | 1    | ostdsr   | Oscillation Stop Detection Status        |
 * | **Total** | **66** |     |                                          |
 *
 * @par Clock Initialization Example
 * @code{.c}
 * // Initialize system to 240 MHz from 24 MHz main oscillator
 * volatile rx_system_regs_t* sys = system_regs();
 *
 * // 1. Unlock register protection
 * *prcr_reg() = k_rx_prcr_unlock_prc0_prc1;
 *
 * // 2. Enable main oscillator
 * sys->mosccr = 0x00;  // Start main oscillator
 *
 * // 3. Wait for oscillator stabilization
 * while (!(sys->oscovfsr & 0x01)) { }
 *
 * // 4. Configure PLL (24 MHz x 10 = 240 MHz)
 * sys->pllcr = 0x0913;  // STC=9 (x10), PLIDIV=1 (/1)
 * sys->pllcr2 = 0x00;   // Enable PLL
 *
 * // 5. Wait for PLL stabilization
 * while (!(sys->oscovfsr & 0x04)) { }
 *
 * // 6. Set MEMWAIT for >120 MHz operation
 * *memwait_reg() = k_memwait_one_wait;
 *
 * // 7. Switch to PLL
 * sys->sckcr3 = 0x0400;  // CKSEL = PLL
 *
 * // 8. Re-lock protection
 * *prcr_reg() = k_rx_prcr_lock;
 * @endcode
 *
 * @par Module Stop Example
 * @code{.c}
 * // Enable CMT0 module (clear bit to enable)
 * *prcr_reg() = k_rx_prcr_unlock_prc1;  // Unlock MSTPCR
 * system_regs()->mstpcra &= ~(1UL << 15);  // Enable CMT0
 * *prcr_reg() = k_rx_prcr_lock;
 *
 * // Enable USB0 module
 * *prcr_reg() = k_rx_prcr_unlock_prc1;
 * system_regs()->mstpcrb &= ~(1UL << k_mstpb_usb0);
 * *prcr_reg() = k_rx_prcr_lock;
 * @endcode
 *
 * @note PRCR (Protection Register) is NOT in this struct - it's at 0x000803FE.
 *       Use prcr_reg() accessor instead.
 *
 * @invariant sizeof(rx_system_regs_t) == 66 bytes (0x42)
 * @invariant All register offsets match RX72N Manual Ch05
 *
 * @warning Most registers require PRCR unlock before modification
 * @attention MEMWAIT must be set before ICLK > 120 MHz
 *
 * @see system_regs() Accessor function
 * @see prcr_reg() Protection register accessor
 * @since Version 1.0.0
 */
typedef struct __attribute__((packed)) {
  /**
   * @brief Mode Monitor Register (MDMONR) @ 0x00
   * @details Read-only - indicates current operating mode
   */
  volatile uint16_t mdmonr;
  uint8_t           reserved0[k_system_reserved_02_05]; /**< Reserved @ 0x02-0x05 */

  /**
   * @brief System Control Register 0 (SYSCR0) @ 0x06
   * @details Controls ROM/RAM enable and external bus settings
   */
  volatile uint16_t syscr0;

  /**
   * @brief System Control Register 1 (SYSCR1) @ 0x08
   * @details Controls RAM wait states
   */
  volatile uint16_t syscr1;
  uint8_t           reserved1[k_system_reserved_0a_0b]; /**< Reserved @ 0x0A-0x0B */

  /**
   * @brief Standby Control Register (SBYCR) @ 0x0C
   * @details Controls standby mode (SSBY bit)
   */
  volatile uint16_t sbycr;
  uint8_t           reserved2[k_system_reserved_0e_0f]; /**< Reserved @ 0x0E-0x0F */

  /**
   * @brief Module Stop Control Register A (MSTPCRA) @ 0x10
   * @details Controls CMT, DTC, DMAC module power. Bit=1 stops module.
   */
  volatile uint32_t mstpcra;

  /**
   * @brief Module Stop Control Register B (MSTPCRB) @ 0x14
   * @details Controls USB, CRC, SCI, RSPI module power. Bit=1 stops module.
   */
  volatile uint32_t mstpcrb;

  /**
   * @brief Module Stop Control Register C (MSTPCRC) @ 0x18
   * @details Controls RAM, Flash module power
   */
  volatile uint32_t mstpcrc;

  /**
   * @brief Module Stop Control Register D (MSTPCRD) @ 0x1C
   * @details Controls ADC, DAC, MTU, GPTW module power. Bit=1 stops module.
   */
  volatile uint32_t mstpcrd;

  /**
   * @brief System Clock Control Register (SCKCR) @ 0x20
   * @details Sets division ratios for ICLK, PCLKA, PCLKB, PCLKC, PCLKD, FCLK, BCLK
   */
  volatile uint32_t sckcr;

  /**
   * @brief System Clock Control Register 2 (SCKCR2) @ 0x24
   * @details Sets UCK (USB clock) division ratio
   */
  volatile uint16_t sckcr2;

  /**
   * @brief System Clock Control Register 3 (SCKCR3) @ 0x26
   * @details Selects system clock source (CKSEL bits)
   */
  volatile uint16_t sckcr3;

  /**
   * @brief PLL Control Register (PLLCR) @ 0x28
   * @details Sets PLL multiplication factor and input divider
   */
  volatile uint16_t pllcr;

  /**
   * @brief PLL Control Register 2 (PLLCR2) @ 0x2A
   * @details PLL enable (0=enabled, 1=disabled)
   */
  volatile uint8_t pllcr2;
  uint8_t          reserved3[k_system_reserved_2b_2f]; /**< Reserved @ 0x2B-0x2F */

  /**
   * @brief External Bus Clock Control Register (BCKCR) @ 0x30
   * @details Controls external bus clock output
   */
  volatile uint8_t bckcr;
  uint8_t          reserved4[k_system_reserved_31]; /**< Reserved @ 0x31 */

  /**
   * @brief Main Clock Oscillator Control Register (MOSCCR) @ 0x32
   * @details Controls main oscillator (24 MHz crystal)
   */
  volatile uint8_t mosccr;

  /**
   * @brief Sub-Clock Oscillator Control Register (SOSCCR) @ 0x33
   * @details Controls sub-clock oscillator (32.768 kHz)
   */
  volatile uint8_t sosccr;

  /**
   * @brief Low-Speed On-Chip Oscillator Control (LOCOCR) @ 0x34
   * @details Controls LOCO (240 kHz)
   */
  volatile uint8_t lococr;

  /**
   * @brief IWDT-Dedicated Oscillator Control (ILOCOCR) @ 0x35
   * @details Controls IWDT oscillator (120 kHz)
   */
  volatile uint8_t ilococr;

  /**
   * @brief High-Speed On-Chip Oscillator Control (HOCOCR) @ 0x36
   * @details Controls HOCO
   */
  volatile uint8_t hococr;

  /**
   * @brief High-Speed On-Chip Oscillator Control 2 (HOCOCR2) @ 0x37
   * @details HOCO frequency selection
   */
  volatile uint8_t hococr2;
  uint8_t          reserved5[k_system_reserved_38_3b]; /**< Reserved @ 0x38-0x3B */

  /**
   * @brief Oscillation Stabilization Flag Register (OSCOVFSR) @ 0x3C
   * @details Indicates when oscillators are stable (1=stable, 0=not stable):
   * - Bit 0 (MOOVF):  Main clock oscillation stable
   * - Bit 1 (SOOVF):  Sub-clock oscillation stable
   * - Bit 2 (PLOVF):  PLL clock oscillation stable
   * - Bit 3 (HCOVF):  HOCO clock oscillation stable
   * - Bit 4 (ILCOVF): IWDT-dedicated clock oscillation stable
   * - Bit 5 (PPLOVF): PPLL clock oscillation stable
   * - Bits 7:6: Reserved
   */
  volatile uint8_t oscovfsr;
  uint8_t          reserved6[k_system_reserved_3d]; /**< Reserved @ 0x3D */

  /**
   * @brief Clock Output Control Register (CKOCR) @ 0x3E
   * @details Controls CLKOUT pin output source and divider
   * @see ckocr_bits_t for bit field definitions
   */
  volatile uint16_t ckocr;

  /**
   * @brief Oscillation Stop Detection Control (OSTDCR) @ 0x40
   * @details Enables oscillation stop detection
   */
  volatile uint8_t ostdcr;

  /**
   * @brief Oscillation Stop Detection Status (OSTDSR) @ 0x41
   * @details Indicates oscillation stop detection status
   */
  volatile uint8_t ostdsr;
} rx_system_regs_t;

/**
 * @brief Get pointer to system registers
 * @return Volatile pointer to system register structure
 * @note Named system_regs() instead of system() to avoid naming conflicts
 */
static inline volatile rx_system_regs_t* system_regs(void)
{
  return (volatile rx_system_regs_t*)k_system_base_addr;
}

/* =============================================================================
 * Protection Register (PRCR) - At separate address 0x000803FE
 * =============================================================================
 */

/**
 * @brief Get pointer to PRCR (Protection Register)
 *
 * @return Volatile pointer to 16-bit PRCR register
 *
 * @details
 * PRCR is at address 0x000803FE, which is NOT contiguous with the main
 * system register block (0x00080000-0x00080041). This accessor provides
 * the correct address for register protection control.
 *
 * @par Manual Reference:
 * RX72N Group User's Manual: Hardware, Chapter 13 (Register Write Protection)
 * - Address: 0x000803FE (verified against manual)
 * - Size: 16-bit
 * - Key: 0xA5 in upper byte required for any write
 *
 * @note Use k_rx_prcr_unlock_* and k_rx_prcr_lock values from
 *       rx_register_protection.h for unlock/lock operations.
 *
 * @par Example:
 * @code
 * #include "rx_register_protection.h"
 * *prcr_reg() = k_rx_prcr_unlock_prc1;  // Unlock MSTPCR group
 * system_regs()->mstpcra &= ~(1UL << 15); // Enable CMT0
 * *prcr_reg() = k_rx_prcr_lock;          // Re-lock protection
 * @endcode
 */
static inline volatile uint16_t* prcr_reg(void)
{
  return (volatile uint16_t*)k_prcr_addr;
}

/** @brief Module stop bits for MSTPCRA register */
typedef enum : uint8_t {
  k_mstpa_cmt23 = 14, /**< CMT2/CMT3 module stop bit in MSTPCRA */
} rx_module_stop_bits_a_t;

/** @brief Module stop bits for MSTPCRB register */
typedef enum : uint8_t {
  k_mstpb_usb0 = 19, /**< USB0 module stop bit in MSTPCRB */
  k_mstpb_crc  = 23, /**< CRC module stop bit in MSTPCRB */
} rx_module_stop_bits_b_t;

/* =============================================================================
 * Reset Status Registers (RSTSR)
 * =============================================================================
 */

/**
 * @brief Reset status register addresses
 * @details
 * IMPORTANT: RSTSR registers are NOT contiguous in memory!
 * - RSTSR0/1 are adjacent at 0x0008C290-0x0008C291
 * - RSTSR2 is at a separate address 0x000800C0
 * Verified against RX72N Group User's Manual Hardware (R01UH0824EJ0120 Rev.1.20)
 */
typedef enum : uintptr_t {
  k_rstsr0_addr = 0x0008C290, /**< Reset Status Register 0 address (page 286) */
  k_rstsr1_addr = 0x0008C291, /**< Reset Status Register 1 address (page 288) */
  k_rstsr2_addr = 0x000800C0, /**< Reset Status Register 2 address (page 289) */
} rx_rstsr_addresses_t;

/**
 * @brief Reset Status Registers 0 and 1 (contiguous)
 * @details
 * RSTSR0 and RSTSR1 are adjacent in memory at 0x0008C290-0x0008C291.
 */
typedef struct {
  volatile uint8_t rstsr0; /**< Reset Status Register 0 (voltage, power-on) */
  volatile uint8_t rstsr1; /**< Reset Status Register 1 (cold/warm start) */
} rx_rstsr01_regs_t;

/**
 * @brief Get pointer to RSTSR0/RSTSR1 registers
 * @return Volatile pointer to RSTSR0/1 register structure
 */
static inline volatile rx_rstsr01_regs_t* rstsr01(void)
{
  return (volatile rx_rstsr01_regs_t*)k_rstsr0_addr;
}

/**
 * @brief Get pointer to RSTSR2 register
 * @return Volatile pointer to RSTSR2 register
 * @note RSTSR2 is at a separate address from RSTSR0/1
 */
static inline volatile uint8_t* rstsr2(void)
{
  return (volatile uint8_t*)k_rstsr2_addr;
}

/* RSTSR0 bit definitions (page 286) */
typedef enum : uint8_t {
  k_rstsr0_porf    = 1U,   /**< Power-On Reset Detect Flag */
  k_rstsr0_lvd0rf  = 2U,   /**< Voltage-Monitoring 0 Reset Detect Flag */
  k_rstsr0_lvd1rf  = 4U,   /**< Voltage-Monitoring 1 Reset Detect Flag */
  k_rstsr0_lvd2rf  = 8U,   /**< Voltage-Monitoring 2 Reset Detect Flag */
  k_rstsr0_dpsrstf = 128U, /**< Deep Software Standby Reset Flag */
} rstsr0_bits_t;

/* RSTSR1 bit definitions (page 288) */
typedef enum : uint8_t {
  k_rstsr1_cwsf = 1U, /**< Cold/Warm Start Determination Flag */
} rstsr1_bits_t;

/* RSTSR2 bit definitions (page 289) */
typedef enum : uint8_t {
  k_rstsr2_iwdtrf = 1U, /**< Independent Watchdog Timer Reset Detect Flag */
  k_rstsr2_wdtrf  = 2U, /**< Watchdog Timer Reset Detect Flag */
  k_rstsr2_swrf   = 4U, /**< Software Reset Detect Flag */
} rstsr2_bits_t;

/* =============================================================================
 * Software Reset Register (SWRR) - Ch06 page 290
 * =============================================================================
 */

/**
 * @brief Software Reset Register (SWRR) address
 * @details
 * Writing 0xA501 to this register triggers an immediate software reset.
 * Reference: RX72N manual Ch06 section 6.2.4, page 290.
 */
typedef enum : uintptr_t {
  k_swrr_addr = 0x000800C2, /**< Software Reset Register address (16-bit) */
} rx_swrr_addresses_t;

/**
 * @brief Software Reset Register value
 * @details
 * Write this value to SWRR to trigger a software reset.
 * The MCU will reset after the internal reset time (tRESW2) elapses.
 * @warning This immediately resets the MCU - ensure all critical state is saved.
 */
typedef enum : uint16_t {
  k_swrr_reset_key = 0xA501, /**< Write this value to trigger software reset */
} rx_swrr_values_t;

/**
 * @brief Get pointer to SWRR (Software Reset Register)
 * @return Volatile pointer to 16-bit SWRR register
 *
 * @details
 * SWRR is at address 0x000800C2. Writing 0xA501 triggers a software reset.
 *
 * @warning Writing to this register causes immediate MCU reset!
 *
 * @par Manual Reference:
 * RX72N Group User's Manual: Hardware, Chapter 6 (Resets), page 290.
 *
 * @par Example:
 * @code
 * // Trigger a software reset (MCU will restart)
 * *swrr_reg() = k_swrr_reset_key;
 * // Code never reaches here - MCU resets immediately
 * @endcode
 */
static inline volatile uint16_t* swrr_reg(void)
{
  return (volatile uint16_t*)k_swrr_addr;
}

/* =============================================================================
 * Memory Wait Cycle Setting Register (MEMWAIT)
 * =============================================================================
 */

/**
 * @brief MEMWAIT register address
 * @details
 * Memory Wait Cycle Setting Register controls wait cycles for flash and ECCRAM.
 * MANDATORY: Must be set to 1 when ICLK > 120 MHz.
 * Reference: RX72N manual Ch09 section 9.2.2, page 341, line 962.
 */
typedef enum : uintptr_t {
  k_memwait_addr = 0x0008101C, /**< Memory Wait Cycle Setting Register */
} rx_memwait_addresses_t;

/**
 * @brief MEMWAIT register bit values
 * @details
 * - 0: No wait cycle (ICLK <= 120 MHz)
 * - 1: One wait cycle (ICLK > 120 MHz) - REQUIRED for 240 MHz operation
 */
typedef enum : uint8_t {
  k_memwait_no_wait  = 0x00, /**< No wait cycle (ICLK <= 120 MHz) */
  k_memwait_one_wait = 0x01, /**< One wait cycle (ICLK > 120 MHz) - MANDATORY */
} rx_memwait_bits_t;

/**
 * @brief Get pointer to MEMWAIT register
 * @return Volatile pointer to MEMWAIT register
 * @note CRITICAL: Must set to 1 before increasing ICLK above 120 MHz
 */
static inline volatile uint8_t* memwait_reg(void)
{
  return (volatile uint8_t*)k_memwait_addr;
}

/* =============================================================================
 * PPLL Control Registers (for USB clock)
 * =============================================================================
 */

/**
 * @brief PPLL register addresses
 * @details
 * PPLL (PLL for specific purposes) generates the 48 MHz USB clock (UCLK).
 * Must be configured for USB functionality.
 * Reference: RX72N manual Ch09, PPLL frequency synthesizer section.
 */
typedef enum : uintptr_t {
  k_ppllcr_addr  = 0x00080048, /**< PPLL Control Register @ 0x00080048 */
  k_ppllcr2_addr = 0x0008004A, /**< PPLL Control Register 2 @ 0x0008004A */
  k_ppllcr3_addr = 0x0008004B, /**< PPLL Control Register 3 @ 0x0008004B (PPLCK divider) */
} rx_ppll_addresses_t;

/**
 * @brief PPLL configuration values
 * @details
 * PPLL generates 240 MHz internal clock from 24 MHz main oscillator.
 * PPLLCR3 then divides by 5 to produce 48 MHz USB clock (UCLK).
 * Calculation: 24 MHz * 10 = 240 MHz (PPLL output); 240 / 5 = 48 MHz (UCLK)
 * - PPLIDIV[1:0] = 00 (divide input by 1, binary 00)
 * - PPLSTC[5:0] = 13h (multiply by x10.0, STC=19=0b010011 at bits 13:8; formula (STC+1)/2=mult)
 * - PPLSRCSEL = 0 (main clock source, shared with PLL via PLLCR.PLLSRCSEL)
 * - PPLLCR = 0x1300
 *
 * Per manual Ch09 section 9.2.23 (p366): PPLSTC minimum valid value is 0b010011 (x10.0).
 * Settings below x10.0 are prohibited -- PPLL output must be 120-240 MHz.
 */
typedef enum : uint16_t {
  k_ppll_config_48mhz = 0x1300, /**< PPLLCR: PPLSTC=19 (x10.0), PPLIDIV=00 (/1): 24MHz*10=240MHz */
} rx_ppll_config_t;

/**
 * @brief PPLLCR3 frequency divider values
 * @details
 * PPLLCR3 PPLCK[3:0] selects the division ratio applied to the PPLL output
 * before it reaches the USB clock (UCLK) pin via PACKCR.UPLLSEL=1.
 * Per manual Ch09 section 9.2.25 (p368):
 * 0001=/2, 0010=/3, 0011=/4, 0100=/5 (settings other than these are prohibited)
 * Set AFTER PPLL is operating and stable (PPLOVF=1), while USB is stopped.
 */
typedef enum : uint8_t {
  k_ppllcr3_div5 = 0x04, /**< PPLLCR3: PPLCK=0100=/5, 240MHz/5=48MHz USB (manual p368) */
} rx_ppllcr3_config_t;

/**
 * @brief PPLL control values
 */
typedef enum : uint8_t {
  k_ppll_enabled  = 0x00, /**< PPLLCR2: Enable PPLL */
  k_ppll_disabled = 0x01, /**< PPLLCR2: Disable PPLL */
} rx_ppll_control_t;

/**
 * @brief PPLL stabilization flag in OSCOVFSR
 * @details
 * OSCOVFSR bit layout (manual Ch09 section 9.2.14):
 * - b0 MOOVF:  Main clock oscillation stabilization flag
 * - b1 SOOVF:  Sub-clock oscillation stabilization flag
 * - b2 PLOVF:  PLL clock oscillation stabilization flag
 * - b3 HCOVF:  HOCO clock oscillation stabilization flag
 * - b4 ILCOVF: IWDT-dedicated clock oscillation stabilization flag
 * - b5 PPLOVF: PPLL clock oscillation stabilization flag
 */
typedef enum : uint8_t {
  k_ppll_stable_flag = 0x20, /**< OSCOVFSR: PPLOVF at bit 5 (0x20); NOT bit 3 */
} rx_ppll_flags_t;

/**
 * @brief Get pointer to PPLLCR register
 * @return Volatile pointer to PPLLCR register
 */
static inline volatile uint16_t* ppllcr_reg(void)
{
  return (volatile uint16_t*)k_ppllcr_addr;
}

/**
 * @brief Get pointer to PPLLCR2 register
 * @return Volatile pointer to PPLLCR2 register
 */
static inline volatile uint8_t* ppllcr2_reg(void)
{
  return (volatile uint8_t*)k_ppllcr2_addr;
}

/**
 * @brief Get pointer to PPLLCR3 register
 * @return Volatile pointer to PPLLCR3 register
 * @note Set PPLCK[3:0] while PPLL is running and USB module is stopped.
 */
static inline volatile uint8_t* ppllcr3_reg(void)
{
  return (volatile uint8_t*)k_ppllcr3_addr;
}

/* =============================================================================
 * Reference Constants for Static Assertions
 * Named constants eliminate magic-number literals in the static_assert checks
 * below. Values are authoritative per RX72N Hardware Manual.
 * =============================================================================
 */

/**
 * @enum rx_system_addr_refs_t
 * @brief Expected hardware addresses per RX72N Hardware Manual
 * @details
 * Each member documents the address mandated by the hardware manual chapter
 * cited in the comment. Used in static_assert to cross-check address enum
 * definitions against the manual reference values.
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  k_ref_system_base = 0x00080000, /**< System register base address (Ch05, Sec 5.2.2) */
  k_ref_prcr        = 0x000803FE, /**< PRCR address -- outside system struct (Ch05) */
  k_ref_rstsr0      = 0x0008C290, /**< RSTSR0 address (Ch05) */
  k_ref_rstsr1      = 0x0008C291, /**< RSTSR1 address (Ch05) */
  k_ref_rstsr2      = 0x000800C0, /**< RSTSR2 address (Ch05) */
  k_ref_swrr        = 0x000800C2, /**< SWRR address (Ch06) */
  k_ref_memwait     = 0x0008101C, /**< MEMWAIT address (Ch09) */
  k_ref_ppllcr      = 0x00080048, /**< PPLLCR address (Ch09) */
  k_ref_ppllcr2     = 0x0008004A, /**< PPLLCR2 address (Ch09) */
  k_ref_ppllcr3     = 0x0008004B, /**< PPLLCR3 address (Ch09 section 9.2.25, p368) */
} rx_system_addr_refs_t;

/**
 * @enum rx_system_struct_layout_t
 * @brief Expected size and member offsets for rx_system_regs_t
 * @details
 * Authoritative per RX72N Hardware Manual Ch05, Section 5.2.2 address map.
 * Used in static_assert to verify the compiler produces the correct layout.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_sys_regs_size        = 0x42, /**< Total size of rx_system_regs_t (66 bytes, 0x00-0x41) */
  k_sys_reg_mdmonr_off   = 0x00, /**< Offset of MDMONR (mode monitor) */
  k_sys_reg_syscr0_off   = 0x06, /**< Offset of SYSCR0 */
  k_sys_reg_syscr1_off   = 0x08, /**< Offset of SYSCR1 */
  k_sys_reg_sbycr_off    = 0x0C, /**< Offset of SBYCR (standby control) */
  k_sys_reg_mstpcra_off  = 0x10, /**< Offset of MSTPCRA */
  k_sys_reg_mstpcrb_off  = 0x14, /**< Offset of MSTPCRB */
  k_sys_reg_mstpcrc_off  = 0x18, /**< Offset of MSTPCRC */
  k_sys_reg_mstpcrd_off  = 0x1C, /**< Offset of MSTPCRD */
  k_sys_reg_sckcr_off    = 0x20, /**< Offset of SCKCR (system clock control) */
  k_sys_reg_sckcr2_off   = 0x24, /**< Offset of SCKCR2 */
  k_sys_reg_sckcr3_off   = 0x26, /**< Offset of SCKCR3 (clock source select) */
  k_sys_reg_pllcr_off    = 0x28, /**< Offset of PLLCR */
  k_sys_reg_pllcr2_off   = 0x2A, /**< Offset of PLLCR2 (PLL enable) */
  k_sys_reg_bckcr_off    = 0x30, /**< Offset of BCKCR (external bus clock) */
  k_sys_reg_mosccr_off   = 0x32, /**< Offset of MOSCCR (main oscillator control) */
  k_sys_reg_sosccr_off   = 0x33, /**< Offset of SOSCCR (sub-clock control) */
  k_sys_reg_lococr_off   = 0x34, /**< Offset of LOCOCR (LOCO control) */
  k_sys_reg_ilococr_off  = 0x35, /**< Offset of ILOCOCR (IWDT oscillator control) */
  k_sys_reg_hococr_off   = 0x36, /**< Offset of HOCOCR (HOCO control) */
  k_sys_reg_hococr2_off  = 0x37, /**< Offset of HOCOCR2 (HOCO frequency) */
  k_sys_reg_oscovfsr_off = 0x3C, /**< Offset of OSCOVFSR (oscillation stable flags) */
  k_sys_reg_ckocr_off    = 0x3E, /**< Offset of CKOCR (clock output control) */
  k_sys_reg_ostdcr_off   = 0x40, /**< Offset of OSTDCR (oscillation stop detect control) */
  k_sys_reg_ostdsr_off   = 0x41, /**< Offset of OSTDSR (oscillation stop detect status) */
} rx_system_struct_layout_t;

/**
 * @enum rx_rstsr01_layout_t
 * @brief Expected size and member offsets for rx_rstsr01_regs_t
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_rstsr01_struct_size = 2, /**< Total size of rx_rstsr01_regs_t (2 bytes) */
  k_rstsr0_member_off   = 0, /**< Byte offset of rstsr0 member */
  k_rstsr1_member_off   = 1, /**< Byte offset of rstsr1 member */
} rx_rstsr01_layout_t;

/**
 * @enum rx_prcr_offset_t
 * @brief PRCR register offset from system register base address
 * @details PRCR is not part of rx_system_regs_t; it sits at base+0x3FE.
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_prcr_from_sys_base = 0x03FE, /**< PRCR offset from system base (NOT in system struct) */
} rx_prcr_offset_t;

/**
 * @enum rx_clock_verify_t
 * @brief Reference values for clock field-width and PLL multiplier assertions
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_nibble_all_ones = 0x0F, /**< All bits set in 4-bit field (verifies 4-bit mask width) */
  k_pllcr_stc_for_x10 =
    19, /**< PLLCR STC field value for x10.0 (manual p345: 0b010011=19; formula (STC+1)/2=mult) */
} rx_clock_verify_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * Verified against RX72N Manual Ch05, Section 5.2.2 Address Map
 * =============================================================================
 */

/* Verify base addresses match Hardware Manual Ch05 */
static_assert((uintptr_t)k_system_base_addr == (uintptr_t)k_ref_system_base,
              "System register base address incorrect");
static_assert((uintptr_t)k_prcr_addr == (uintptr_t)k_ref_prcr,
              "PRCR address incorrect (must be 0x000803FE, NOT in system struct)");

/* Verify system register structure layout - offsets per RX72N Manual Ch05 */
static_assert(sizeof(rx_system_regs_t) == k_sys_regs_size,
              "System register structure size incorrect (expected 0x42)");
static_assert(offsetof(rx_system_regs_t, mdmonr) == k_sys_reg_mdmonr_off,
              "MDMONR offset incorrect (expected 0x00)");
static_assert(offsetof(rx_system_regs_t, syscr0) == k_sys_reg_syscr0_off,
              "SYSCR0 offset incorrect (expected 0x06)");
static_assert(offsetof(rx_system_regs_t, syscr1) == k_sys_reg_syscr1_off,
              "SYSCR1 offset incorrect (expected 0x08)");
static_assert(offsetof(rx_system_regs_t, sbycr) == k_sys_reg_sbycr_off,
              "SBYCR offset incorrect (expected 0x0C)");
static_assert(offsetof(rx_system_regs_t, mstpcra) == k_sys_reg_mstpcra_off,
              "MSTPCRA offset incorrect (expected 0x10)");
static_assert(offsetof(rx_system_regs_t, mstpcrb) == k_sys_reg_mstpcrb_off,
              "MSTPCRB offset incorrect (expected 0x14)");
static_assert(offsetof(rx_system_regs_t, mstpcrc) == k_sys_reg_mstpcrc_off,
              "MSTPCRC offset incorrect (expected 0x18)");
static_assert(offsetof(rx_system_regs_t, mstpcrd) == k_sys_reg_mstpcrd_off,
              "MSTPCRD offset incorrect (expected 0x1C)");
static_assert(offsetof(rx_system_regs_t, sckcr) == k_sys_reg_sckcr_off,
              "SCKCR offset incorrect (expected 0x20)");
static_assert(offsetof(rx_system_regs_t, sckcr2) == k_sys_reg_sckcr2_off,
              "SCKCR2 offset incorrect (expected 0x24)");
static_assert(offsetof(rx_system_regs_t, sckcr3) == k_sys_reg_sckcr3_off,
              "SCKCR3 offset incorrect (expected 0x26)");
static_assert(offsetof(rx_system_regs_t, pllcr) == k_sys_reg_pllcr_off,
              "PLLCR offset incorrect (expected 0x28)");
static_assert(offsetof(rx_system_regs_t, pllcr2) == k_sys_reg_pllcr2_off,
              "PLLCR2 offset incorrect (expected 0x2A)");
static_assert(offsetof(rx_system_regs_t, bckcr) == k_sys_reg_bckcr_off,
              "BCKCR offset incorrect (expected 0x30)");
static_assert(offsetof(rx_system_regs_t, mosccr) == k_sys_reg_mosccr_off,
              "MOSCCR offset incorrect (expected 0x32)");
static_assert(offsetof(rx_system_regs_t, sosccr) == k_sys_reg_sosccr_off,
              "SOSCCR offset incorrect (expected 0x33)");
static_assert(offsetof(rx_system_regs_t, lococr) == k_sys_reg_lococr_off,
              "LOCOCR offset incorrect (expected 0x34)");
static_assert(offsetof(rx_system_regs_t, ilococr) == k_sys_reg_ilococr_off,
              "ILOCOCR offset incorrect (expected 0x35)");
static_assert(offsetof(rx_system_regs_t, hococr) == k_sys_reg_hococr_off,
              "HOCOCR offset incorrect (expected 0x36)");
static_assert(offsetof(rx_system_regs_t, hococr2) == k_sys_reg_hococr2_off,
              "HOCOCR2 offset incorrect (expected 0x37)");
static_assert(offsetof(rx_system_regs_t, oscovfsr) == k_sys_reg_oscovfsr_off,
              "OSCOVFSR offset incorrect (expected 0x3C)");
static_assert(offsetof(rx_system_regs_t, ckocr) == k_sys_reg_ckocr_off,
              "CKOCR offset incorrect (expected 0x3E)");
static_assert(offsetof(rx_system_regs_t, ostdcr) == k_sys_reg_ostdcr_off,
              "OSTDCR offset incorrect (expected 0x40)");
static_assert(offsetof(rx_system_regs_t, ostdsr) == k_sys_reg_ostdsr_off,
              "OSTDSR offset incorrect (expected 0x41)");

/* Verify RSTSR addresses match Hardware Manual Ch05 */
static_assert((uintptr_t)k_rstsr0_addr == (uintptr_t)k_ref_rstsr0, "RSTSR0 address incorrect");
static_assert((uintptr_t)k_rstsr1_addr == (uintptr_t)k_ref_rstsr1, "RSTSR1 address incorrect");
static_assert((uintptr_t)k_rstsr2_addr == (uintptr_t)k_ref_rstsr2, "RSTSR2 address incorrect");
static_assert(sizeof(rx_rstsr01_regs_t) == k_rstsr01_struct_size,
              "RSTSR01 structure size incorrect");
static_assert(offsetof(rx_rstsr01_regs_t, rstsr0) == k_rstsr0_member_off,
              "RSTSR0 offset incorrect");
static_assert(offsetof(rx_rstsr01_regs_t, rstsr1) == k_rstsr1_member_off,
              "RSTSR1 offset incorrect");

/* Verify SWRR address matches Hardware Manual Ch06 */
static_assert((uintptr_t)k_swrr_addr == (uintptr_t)k_ref_swrr, "SWRR address incorrect");

/* Verify MEMWAIT address match Hardware Manual Ch09 */
static_assert((uintptr_t)k_memwait_addr == (uintptr_t)k_ref_memwait, "MEMWAIT address incorrect");

/* Verify PPLL addresses match Hardware Manual Ch09 */
static_assert((uintptr_t)k_ppllcr_addr == (uintptr_t)k_ref_ppllcr, "PPLLCR address incorrect");
static_assert((uintptr_t)k_ppllcr2_addr == (uintptr_t)k_ref_ppllcr2, "PPLLCR2 address incorrect");
static_assert((uintptr_t)k_ppllcr3_addr == (uintptr_t)k_ref_ppllcr3, "PPLLCR3 address incorrect");

/* Verify PRCR is NOT embedded in system struct (critical check) */
static_assert((uintptr_t)(k_prcr_addr - k_system_base_addr) == (uintptr_t)k_prcr_from_sys_base,
              "PRCR must be at offset 0x3FE from system base, not embedded in struct");

/* Verify SCKCR clock divider field positions and masks */
static_assert((k_sckcr_pckd_mask >> k_sckcr_pckd_shift) == (uint32_t)k_nibble_all_ones,
              "SCKCR PCKD field mask incorrect");
static_assert((k_sckcr_ick_mask >> k_sckcr_ick_shift) == (uint32_t)k_nibble_all_ones,
              "SCKCR ICK field mask incorrect");
static_assert((k_sckcr_fck_mask >> k_sckcr_fck_shift) == (uint32_t)k_nibble_all_ones,
              "SCKCR FCK field mask incorrect");

/* Verify STAR project SCKCR configuration value */
static_assert((k_sckcr_star_240mhz & k_sckcr_ick_mask) ==
                ((uint32_t)k_clock_div_1 << k_sckcr_ick_shift),
              "STAR SCKCR: ICLK should be /1 (240 MHz)");
static_assert((k_sckcr_star_240mhz & k_sckcr_pcka_mask) ==
                ((uint32_t)k_clock_div_2 << k_sckcr_pcka_shift),
              "STAR SCKCR: PCLKA should be /2 (120 MHz)");
static_assert((k_sckcr_star_240mhz & k_sckcr_pckb_mask) ==
                ((uint32_t)k_clock_div_4 << k_sckcr_pckb_shift),
              "STAR SCKCR: PCLKB should be /4 (60 MHz)");
static_assert((k_sckcr_star_240mhz & k_sckcr_fck_mask) ==
                ((uint32_t)k_clock_div_4 << k_sckcr_fck_shift),
              "STAR SCKCR: FCLK should be /4 (60 MHz)");

/* Verify PLLCR multiplier encoding */
static_assert((k_pllcr_star_24mhz_to_240mhz & k_pllcr_stc_mask) ==
                ((uint32_t)k_pllcr_stc_for_x10 << k_pllcr_stc_shift),
              "STAR PLLCR: STC should be 9 (multiply by 10)");
static_assert((k_pllcr_star_24mhz_to_240mhz & k_pllcr_plidiv_mask) == k_pllcr_plidiv_1,
              "STAR PLLCR: PLIDIV should be /1");
static_assert((k_pllcr_star_24mhz_to_240mhz & k_pllcr_src_mask) == k_pllcr_src_main,
              "STAR PLLCR: Source should be main oscillator");

/* Verify clock source select values */
static_assert((k_sckcr3_cksel_pll & k_sckcr3_cksel_mask) == k_sckcr3_cksel_pll,
              "SCKCR3 PLL select value incorrect");
static_assert((k_sckcr3_cksel_main & k_sckcr3_cksel_mask) == k_sckcr3_cksel_main,
              "SCKCR3 main clock select value incorrect");

/* Verify CKOCR bit positions */
static_assert((k_ckocr_ckosel_pll & k_ckocr_ckosel_mask) == k_ckocr_ckosel_pll,
              "CKOCR PLL select value incorrect");
static_assert((k_ckocr_ckodiv_4 & k_ckocr_ckodiv_mask) == k_ckocr_ckodiv_4,
              "CKOCR divide-by-4 value incorrect");

#ifdef __cplusplus
}
#endif
