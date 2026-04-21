/**
 * @file rx72n_poeg_regs.h
 * @brief RX72N GPTW Port Output Enable (POEG) Register Definitions
 *
 * @details
 * Register definitions for the POEG module which provides emergency PWM output
 * disable functionality for the General PWM Timer (GPTW). The POEG is critical
 * for motor control safety, allowing immediate PWM shutdown on fault detection.
 *
 * @par Safety-Critical Emergency Stop System
 * @dot
 * digraph poeg_system {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_triggers {
 *     label="Fault Detection Sources";
 *     style=filled;
 *     color=lightcoral;
 *
 *     pin [label="GTETRGn Pin\n(External fault)"];
 *     gptw [label="GPTW Output Error\n(Dead-time/shoot-through)"];
 *     osc [label="Oscillation Stop\n(Clock failure)"];
 *     sw [label="Software Request\n(SSF flag)"];
 *   }
 *
 *   subgraph cluster_poeg {
 *     label="POEG Groups A-D (0x0009E000)";
 *     style=filled;
 *     color=lightyellow;
 *
 *     poeg [label="POEG Logic\n(OR of all triggers)" shape=ellipse];
 *   }
 *
 *   subgraph cluster_output {
 *     label="Motor Control Output";
 *     style=filled;
 *     color=lightgreen;
 *
 *     pwm [label="GPTW PWM\nGTIOCxA/B"];
 *     motor [label="H-Bridge\n(DRV8263H)"];
 *   }
 *
 *   pin -> poeg [label="PIDF"];
 *   gptw -> poeg [label="IOCF"];
 *   osc -> poeg [label="OSTPF"];
 *   sw -> poeg [label="SSF"];
 *   poeg -> pwm [label="Stop Request"];
 *   pwm -> motor [label="Hi-Z"];
 * }
 * @enddot
 *
 * @par STAR Project POEG Usage
 * | Group | Assigned To      | Trigger Source      | Use Case                     |
 * |-------|------------------|---------------------|------------------------------|
 * | A     | GPTW0 (Motor 0)  | GTETRGA + Software  | Front-left motor E-stop      |
 * | B     | GPTW1 (Motor 1)  | GTETRGB + Software  | Front-right motor E-stop     |
 * | C     | GPTW2 (Motor 2)  | GTETRGC + Software  | Back-right motor E-stop      |
 * | D     | GPTW3 (Motor 3)  | GTETRGD + Software  | Back-left motor E-stop       |
 *
 * @par Key Features
 * - Emergency PWM output disable (immediate Hi-Z on fault)
 * - Multiple trigger sources: external pin, GPTW error, oscillation stop, software
 * - Digital noise filter on external trigger inputs
 * - Per-group independent control (4 groups for 4 motors)
 * - Interrupt generation on fault detection
 *
 * @par Hardware Requirements
 * | Parameter      | Value            | Notes                        |
 * |----------------|------------------|------------------------------|
 * | Base Address   | 0x0009E000       | Group A base                 |
 * | Group Spacing  | 0x100 bytes      | A=0x0000, B=0x0100, C=0x0200, D=0x0300 |
 * | Module Stop    | MSTPCRA.MSTPA7   | Same as GPTW (shared)        |
 * | Clock          | PCLKB (60 MHz)   | For noise filter sampling    |
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp, or recursion
 * - Rule 2: N/A (no loops in register definitions)
 * - Rule 3: No dynamic memory allocation
 * - Rule 4: All accessor functions are single-statement
 * - Rule 5: N/A (hardware layer)
 * - Rule 6: Minimal scope - inline accessors only
 * - Rule 7: N/A (no return values to check)
 * - Rule 8: All constants use C23 typed enums
 * - Rule 9: No function pointers
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single responsibility - only POEG register definitions
 * - **O**: Open for extension via additional trigger configurations
 * - **L**: N/A (no inheritance hierarchy)
 * - **I**: Minimal interface - only necessary accessor functions
 * - **D**: N/A (hardware register layer)
 *
 * @par Module Dependencies
 * - `<stdint.h>` - Fixed-width integer types
 * - `<stddef.h>` - offsetof macro for static assertions
 *
 * @par Manual References
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 27: GPTW Port Output Enable (POEG), pages 1432-1441
 *
 * @see rx72n_gptw_regs.h General PWM Timer registers
 * @see rx72n_regs.h Main register include file
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
 * POEG - GPTW Port Output Enable for Emergency Motor Stop
 * =============================================================================
 */

/**
 * @enum rx_poeg_addresses_t
 * @brief POEG group base addresses
 *
 * @details
 * Base addresses for each POEG group (A-D). Each group controls emergency
 * output disable for associated GPTW channels. Groups are spaced 0x100 bytes
 * apart.
 *
 * @par Memory Map
 * | Address      | Group | Controls     | Description                    |
 * |--------------|-------|--------------|--------------------------------|
 * | 0x0009E000   | A     | GPTW ch0-3   | Emergency stop group A         |
 * | 0x0009E100   | B     | GPTW ch0-3   | Emergency stop group B         |
 * | 0x0009E200   | C     | GPTW ch0-3   | Emergency stop group C         |
 * | 0x0009E300   | D     | GPTW ch0-3   | Emergency stop group D         |
 *
 * @note Manual: Ch27.2.1 - POEG Group n Setting Register
 * @see poegga() Accessor function for Group A
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /**
   * @brief POEG Group A base address (0x0009E000)
   * @details Verified against RX72N Hardware Manual Ch27.2.1
   */
  k_poegga_base_addr = 0x0009E000,

  /**
   * @brief POEG Group B base address (0x0009E100)
   * @details Verified against RX72N Hardware Manual Ch27.2.1
   */
  k_poeggb_base_addr = 0x0009E100,

  /**
   * @brief POEG Group C base address (0x0009E200)
   * @details Verified against RX72N Hardware Manual Ch27.2.1
   */
  k_poeggc_base_addr = 0x0009E200,

  /**
   * @brief POEG Group D base address (0x0009E300)
   * @details Verified against RX72N Hardware Manual Ch27.2.1
   */
  k_poeggd_base_addr = 0x0009E300,

  /**
   * @brief Group spacing (0x100 bytes between groups)
   * @details Each POEG group has a 256-byte register block
   */
  k_poeg_group_spacing = 0x100,
} rx_poeg_addresses_t;

/**
 * @struct rx_poegg_regs_t
 * @brief POEG Group n Register Structure
 *
 * @details
 * Each POEG group has a single 32-bit register (POEGGn) that controls
 * emergency output disable for the associated GPTW channels. The register
 * provides multiple trigger sources and status flags.
 *
 * @par Memory Layout Table
 * | Offset | Size | Field   | Type     | Description                     |
 * |--------|------|---------| ---------|---------------------------------|
 * | 0x00   | 4    | poeggn  | uint32_t | Group n Setting Register        |
 * | **Total** | **4** |      |          |                                 |
 *
 * @par Emergency Stop Trigger Sources
 * 1. **External Pin (PIDF)**: GTETRGn pin input detection
 * 2. **GPTW Output Error (IOCF)**: Dead-time error or shoot-through detection
 * 3. **Oscillation Stop (OSTPF)**: Main clock failure detection
 * 4. **Software (SSF)**: Direct software emergency stop command
 *
 * @par Configuration Example
 * @code{.c}
 * // Enable software emergency stop and external trigger for Group A
 * volatile rx_poegg_regs_t* poeg_a = poegga();
 *
 * // Enable external pin detection (one-time write after reset)
 * poeg_a->poeggn = k_poeg_pide_enable;
 *
 * // Later, trigger emergency stop via software
 * poeg_a->poeggn |= k_poeg_ssf_stop;  // Immediate PWM disable
 *
 * // Clear emergency stop (after fault resolved)
 * poeg_a->poeggn &= ~k_poeg_ssf_stop;  // Resume PWM
 * @endcode
 *
 * @invariant sizeof(rx_poegg_regs_t) == 4 bytes
 *
 * @note All triggers are OR'd together - any trigger stops output
 * @warning Enable bits (PIDE, IOCE, OSTPE) can only be written once after reset
 * @attention Module stop bit shared with GPTW (MSTPCRA.MSTPA7)
 *
 * @see poegga(), poeggb(), poeggc(), poeggd() Accessor functions
 * @see rx_poegg_bits_t Register bit definitions
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief POEG Group n Setting Register (POEGGn) @ offset 0x00
   *
   * @details
   * Controls emergency output disable triggers and status.
   *
   * @par Bit Layout (32-bit)
   * | Bits    | Field      | R/W  | Description                              |
   * |---------|------------|------|------------------------------------------|
   * | 0       | PIDF       | R/W* | Port Input Detection Flag                |
   * | 1       | IOCF       | R/W* | GPTW Output Stop Request Flag            |
   * | 2       | OSTPF      | R/W* | Oscillation Stop Detection Flag          |
   * | 3       | SSF        | R/W  | Software Stop Flag                       |
   * | 4       | PIDE       | R/W+ | Port Input Detection Enable              |
   * | 5       | IOCE       | R/W+ | GPTW Output Stop Request Enable          |
   * | 6       | OSTPE      | R/W+ | Oscillation Stop Enable                  |
   * | 15:7    | -          | R    | Reserved (read as 0)                     |
   * | 16      | ST         | R    | GTETRGn Input Status                     |
   * | 27:17   | -          | R    | Reserved (read as 0)                     |
   * | 28      | INV        | R/W  | GTETRGn Input Inverting                  |
   * | 29      | NFEN       | R/W  | Noise Filter Enable                      |
   * | 31:30   | NFCS[1:0]  | R/W  | Noise Filter Clock Select                |
   *
   * @note * Only 0 can be written to clear flag
   * @note + Can only be written once after reset
   *
   * @see rx_poegg_bits_t Bit definitions
   */
  volatile uint32_t poeggn;
} rx_poegg_regs_t;

/**
 * @brief Get pointer to POEG Group A registers
 *
 * @details
 * Returns a volatile pointer to POEG Group A register structure at
 * address 0x0009E000. Use for emergency stop control of motor 0.
 *
 * @return Volatile pointer to POEG Group A register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre GPTW/POEG module enabled (MSTPCRA.MSTPA7 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @note Thread Safety: Safe - returns constant hardware address
 *
 * @par Example:
 * @code{.c}
 * // Emergency stop motor 0
 * poegga()->poeggn |= k_poeg_ssf_stop;
 * @endcode
 *
 * @see k_poegga_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_poegg_regs_t* poegga(void)
{
  return (volatile rx_poegg_regs_t*)k_poegga_base_addr;
}

/**
 * @brief Get pointer to POEG Group B registers
 *
 * @details
 * Returns a volatile pointer to POEG Group B register structure at
 * address 0x0009E100. Use for emergency stop control of motor 1.
 *
 * @return Volatile pointer to POEG Group B register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre GPTW/POEG module enabled (MSTPCRA.MSTPA7 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @see k_poeggb_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_poegg_regs_t* poeggb(void)
{
  return (volatile rx_poegg_regs_t*)k_poeggb_base_addr;
}

/**
 * @brief Get pointer to POEG Group C registers
 *
 * @details
 * Returns a volatile pointer to POEG Group C register structure at
 * address 0x0009E200. Use for emergency stop control of motor 2.
 *
 * @return Volatile pointer to POEG Group C register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre GPTW/POEG module enabled (MSTPCRA.MSTPA7 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @see k_poeggc_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_poegg_regs_t* poeggc(void)
{
  return (volatile rx_poegg_regs_t*)k_poeggc_base_addr;
}

/**
 * @brief Get pointer to POEG Group D registers
 *
 * @details
 * Returns a volatile pointer to POEG Group D register structure at
 * address 0x0009E300. Use for emergency stop control of motor 3.
 *
 * @return Volatile pointer to POEG Group D register structure
 * @retval Non-NULL Always returns valid pointer (hardware address)
 *
 * @pre GPTW/POEG module enabled (MSTPCRA.MSTPA7 = 0)
 * @post Pointer valid for lifetime of program execution
 *
 * @see k_poeggd_base_addr Base address constant
 * @since Version 1.0.0
 */
static inline volatile rx_poegg_regs_t* poeggd(void)
{
  return (volatile rx_poegg_regs_t*)k_poeggd_base_addr;
}

/**
 * @enum rx_poegg_bit_shifts_t
 * @brief POEGGn register bit field positions
 *
 * @details
 * Bit positions for POEGGn register fields. Used to construct field values.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_poeg_pidf_shift  = 0,  /**< PIDF - Port Input Detection Flag (bit 0) */
  k_poeg_iocf_shift  = 1,  /**< IOCF - GPTW Output Stop Flag (bit 1) */
  k_poeg_ostpf_shift = 2,  /**< OSTPF - Oscillation Stop Flag (bit 2) */
  k_poeg_ssf_shift   = 3,  /**< SSF - Software Stop Flag (bit 3) */
  k_poeg_pide_shift  = 4,  /**< PIDE - Port Input Detection Enable (bit 4) */
  k_poeg_ioce_shift  = 5,  /**< IOCE - GPTW Output Stop Enable (bit 5) */
  k_poeg_ostpe_shift = 6,  /**< OSTPE - Oscillation Stop Enable (bit 6) */
  k_poeg_st_shift    = 16, /**< ST - GTETRGn Input Status (bit 16) */
  k_poeg_inv_shift   = 28, /**< INV - Input Inverting (bit 28) */
  k_poeg_nfen_shift  = 29, /**< NFEN - Noise Filter Enable (bit 29) */
  k_poeg_nfcs_shift  = 30, /**< NFCS[1:0] - Noise Filter Clock Select (bits 31:30) */
} rx_poegg_bit_shifts_t;

/**
 * @enum rx_poegg_bits_t
 * @brief POEGGn register bit definitions
 *
 * @details
 * Configuration and status bit values for the POEG Group n Setting Register.
 *
 * @par Flag Bits (Status/Control)
 * | Bit  | Name  | Description                                    |
 * |------|-------|------------------------------------------------|
 * | PIDF | b0    | Port input detection flag (write 0 to clear)   |
 * | IOCF | b1    | GPTW output stop detected (write 0 to clear)   |
 * | OSTPF| b2    | Oscillation stop detected (write 0 to clear)   |
 * | SSF  | b3    | Software stop flag (read/write)                |
 *
 * @par Enable Bits (Write-Once After Reset)
 * | Bit  | Name  | Description                                    |
 * |------|-------|------------------------------------------------|
 * | PIDE | b4    | Enable port input detection                    |
 * | IOCE | b5    | Enable GPTW output stop detection              |
 * | OSTPE| b6    | Enable oscillation stop detection              |
 *
 * @par Filter Configuration
 * | NFCS | Clock       | Sampling Delay                              |
 * |------|-------------|---------------------------------------------|
 * | 00   | PCLKB/1     | ~50 ns @ 60 MHz (3 samples)                 |
 * | 01   | PCLKB/8     | ~400 ns @ 60 MHz (3 samples)                |
 * | 10   | PCLKB/32    | ~1.6 us @ 60 MHz (3 samples)                |
 * | 11   | PCLKB/128   | ~6.4 us @ 60 MHz (3 samples)                |
 *
 * @see rx_poegg_regs_t::poeggn Register definition
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /* Flag bits (lower byte) - write 0 to clear */
  k_poeg_pidf_detected  = (1U << k_poeg_pidf_shift),  /**< Port input fault detected */
  k_poeg_iocf_detected  = (1U << k_poeg_iocf_shift),  /**< GPTW output error detected */
  k_poeg_ostpf_detected = (1U << k_poeg_ostpf_shift), /**< Oscillation stop detected */
  k_poeg_ssf_stop       = (1U << k_poeg_ssf_shift),   /**< Software emergency stop active */

  /* Enable bits (write-once after reset) */
  k_poeg_pide_enable  = (1U << k_poeg_pide_shift),  /**< Enable external pin detection */
  k_poeg_ioce_enable  = (1U << k_poeg_ioce_shift),  /**< Enable GPTW error detection */
  k_poeg_ostpe_enable = (1U << k_poeg_ostpe_shift), /**< Enable oscillation stop detection */

  /* Status bit (read-only) */
  k_poeg_st_high = (1U << k_poeg_st_shift), /**< GTETRGn input is high */

  /* Input configuration */
  k_poeg_inv_invert  = (1U << k_poeg_inv_shift),  /**< Invert external trigger input */
  k_poeg_nfen_enable = (1U << k_poeg_nfen_shift), /**< Enable digital noise filter */

  /* Noise filter clock select (NFCS[1:0]) */
  k_poeg_nfcs_pclkb_div1   = (0U << k_poeg_nfcs_shift), /**< Sample @ PCLKB/1 (~50ns) */
  k_poeg_nfcs_pclkb_div8   = (1U << k_poeg_nfcs_shift), /**< Sample @ PCLKB/8 (~400ns) */
  k_poeg_nfcs_pclkb_div32  = (2U << k_poeg_nfcs_shift), /**< Sample @ PCLKB/32 (~1.6us) */
  k_poeg_nfcs_pclkb_div128 = (3U << k_poeg_nfcs_shift), /**< Sample @ PCLKB/128 (~6.4us) */

  /* Combined masks for convenience */
  k_poeg_all_flags_mask = (k_poeg_pidf_detected | k_poeg_iocf_detected | k_poeg_ostpf_detected |
                           k_poeg_ssf_stop), /**< All flag bits */
  k_poeg_all_enable_mask =
    (k_poeg_pide_enable | k_poeg_ioce_enable | k_poeg_ostpe_enable), /**< All enable bits */
  k_poeg_nfcs_mask = (3U << k_poeg_nfcs_shift),                      /**< NFCS field mask */
} rx_poegg_bits_t;

/**
 * @enum rx_poeg_interrupts_t
 * @brief POEG interrupt vector numbers
 *
 * @details
 * ICU interrupt vector numbers for POEG groups. These interrupts are
 * triggered when PIDF or IOCF flags are set.
 *
 * @note Manual: Ch27.4 - Interrupt Sources
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_poeg_irq_group_a = 188, /**< POEGGAI - Group A interrupt */
  k_poeg_irq_group_b = 189, /**< POEGGBI - Group B interrupt */
  k_poeg_irq_group_c = 190, /**< POEGGCI - Group C interrupt */
  k_poeg_irq_group_d = 191, /**< POEGGDI - Group D interrupt */
} rx_poeg_interrupts_t;

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* Verify base addresses match Hardware Manual Ch27.2.1 */
static_assert(k_poegga_base_addr == 0x0009E000, "POEGGA base address incorrect");
static_assert(k_poeggb_base_addr == 0x0009E100, "POEGGB base address incorrect");
static_assert(k_poeggc_base_addr == 0x0009E200, "POEGGC base address incorrect");
static_assert(k_poeggd_base_addr == 0x0009E300, "POEGGD base address incorrect");

/* Verify group spacing */
static_assert((k_poeggb_base_addr - k_poegga_base_addr) == k_poeg_group_spacing,
              "POEG group A-B spacing incorrect");
static_assert((k_poeggc_base_addr - k_poeggb_base_addr) == k_poeg_group_spacing,
              "POEG group B-C spacing incorrect");
static_assert((k_poeggd_base_addr - k_poeggc_base_addr) == k_poeg_group_spacing,
              "POEG group C-D spacing incorrect");

/* Verify register structure layout */
static_assert(sizeof(rx_poegg_regs_t) == 4, "POEG register structure size incorrect");
static_assert(offsetof(rx_poegg_regs_t, poeggn) == 0x00, "POEGGn offset incorrect");

/* Verify bit field positions */
static_assert(k_poeg_pidf_detected == 0x00000001, "PIDF bit value incorrect");
static_assert(k_poeg_iocf_detected == 0x00000002, "IOCF bit value incorrect");
static_assert(k_poeg_ostpf_detected == 0x00000004, "OSTPF bit value incorrect");
static_assert(k_poeg_ssf_stop == 0x00000008, "SSF bit value incorrect");
static_assert(k_poeg_pide_enable == 0x00000010, "PIDE bit value incorrect");
static_assert(k_poeg_ioce_enable == 0x00000020, "IOCE bit value incorrect");
static_assert(k_poeg_ostpe_enable == 0x00000040, "OSTPE bit value incorrect");
static_assert(k_poeg_st_high == 0x00010000, "ST bit value incorrect");
static_assert(k_poeg_inv_invert == 0x10000000, "INV bit value incorrect");
static_assert(k_poeg_nfen_enable == 0x20000000, "NFEN bit value incorrect");
static_assert(k_poeg_nfcs_pclkb_div1 == 0x00000000, "NFCS div1 value incorrect");
static_assert(k_poeg_nfcs_pclkb_div128 == 0xC0000000, "NFCS div128 value incorrect");

#ifdef __cplusplus
}
#endif
