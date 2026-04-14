/**
 * @file rx72n_poe3_regs.h
 * @brief RX72N Port Output Enable 3 (POE3) Register Definitions
 *
 * @details
 * Complete register definitions for the RX72N Port Output Enable 3 module.
 * POE3 provides emergency output disable for MTU (Multi-Function Timer Pulse Unit)
 * pins, enabling hardware-level motor protection.
 *
 * @par POE3 Overview
 * - Emergency output disable for MTU0, MTU3/4, MTU6/7 pins
 * - Input signal detection (POE0#, POE4#, POE8#, POE10#, POE11#)
 * - Simultaneous conduction detection (shoot-through protection)
 * - Oscillation stop detection response
 * - Software-triggered high-impedance state
 *
 * @par Target Pins for High-Impedance Control
 * | Timer | Pins Controlled |
 * |-------|-----------------|
 * | MTU0 | MTIOC0A, MTIOC0B, MTIOC0C, MTIOC0D |
 * | MTU3 | MTIOC3B, MTIOC3D |
 * | MTU4 | MTIOC4A, MTIOC4B, MTIOC4C, MTIOC4D |
 * | MTU6 | MTIOC6B, MTIOC6D |
 * | MTU7 | MTIOC7A, MTIOC7B, MTIOC7C, MTIOC7D |
 *
 * @par Register Organization (POE3 base: 0x0008C4C0)
 * | Offset | Size | Register | Description |
 * |--------|------|----------|-------------|
 * | 0x00 | 16 | ICSR1 | Input Level Control/Status 1 (POE0#) |
 * | 0x02 | 16 | OCSR1 | Output Level Control/Status 1 |
 * | 0x04 | 16 | ICSR2 | Input Level Control/Status 2 (POE4#) |
 * | 0x06 | 16 | OCSR2 | Output Level Control/Status 2 |
 * | 0x08 | 16 | ICSR3 | Input Level Control/Status 3 (POE8#) |
 * | 0x0A | 8 | SPOER | Software Port Output Enable |
 * | 0x0B | 8 | POECR1 | Port Output Enable Control 1 |
 * | 0x0C | 16 | POECR2 | Port Output Enable Control 2 (MTU3/4 b10:8, MTU6/7 b2:0) |
 * | 0x10 | 16 | POECR4 | Port Output Enable Control 4 (ICxADD conditions MT34/MT67) |
 * | 0x12 | 16 | POECR5 | Port Output Enable Control 5 (ICxADD conditions MT0) |
 * | 0x16 | 16 | ICSR4 | Input Level Control/Status 4 (POE10#) |
 * | 0x18 | 16 | ICSR5 | Input Level Control/Status 5 (POE11#) |
 * | 0x1A | 16 | ALR1 | Active Level Register 1 |
 * | 0x1C | 16 | ICSR6 | Input Level Control/Status 6 (osc stop) |
 * | 0x24 | 8 | M0SELR1 | MTU0 Pin Select 1 |
 * | 0x25 | 8 | M0SELR2 | MTU0 Pin Select 2 |
 * | 0x26 | 8 | M3SELR | MTU3 Pin Select |
 * | 0x27 | 8 | M4SELR1 | MTU4 Pin Select 1 |
 * | 0x28 | 8 | M4SELR2 | MTU4 Pin Select 2 |
 * | 0x2A | 8 | M6SELR | MTU6 Pin Select |
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
 * - Chapter 25: Port Output Enable 3 (POE3a), pages 1199-1240
 * - Section 25.2: Register Descriptions
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
 * POE3 Base Address and Register Offsets
 * =============================================================================
 */

/**
 * @enum poe3_addresses_t
 * @brief POE3 module addresses
 *
 * @details
 * Base address and register addresses for POE3.
 * All addresses verified against RX72N Hardware Manual Ch25.
 *
 * @note Addresses verified 2026-01-29 against manual section 25.2
 * @since Version 1.0.0
 */
typedef enum : uintptr_t {
  /** @brief POE3 module base address */
  k_poe3_base_addr = 0x0008C4C0U,

  /* Input Level Control/Status Registers */
  /** @brief ICSR1 address - Input Level Control/Status 1 (POE0#) */
  k_poe3_icsr1_addr = 0x0008C4C0U,

  /** @brief ICSR2 address - Input Level Control/Status 2 (POE4#) */
  k_poe3_icsr2_addr = 0x0008C4C4U,

  /** @brief ICSR3 address - Input Level Control/Status 3 (POE8#) */
  k_poe3_icsr3_addr = 0x0008C4C8U,

  /** @brief ICSR4 address - Input Level Control/Status 4 (POE10#) */
  k_poe3_icsr4_addr = 0x0008C4D6U,

  /** @brief ICSR5 address - Input Level Control/Status 5 (POE11#) */
  k_poe3_icsr5_addr = 0x0008C4D8U,

  /** @brief ICSR6 address - Input Level Control/Status 6 (osc stop) */
  k_poe3_icsr6_addr = 0x0008C4DCU,

  /* Output Level Control/Status Registers */
  /** @brief OCSR1 address - Output Level Control/Status 1 (MTU3/4) */
  k_poe3_ocsr1_addr = 0x0008C4C2U,

  /** @brief OCSR2 address - Output Level Control/Status 2 (MTU6/7) */
  k_poe3_ocsr2_addr = 0x0008C4C6U,

  /* Control Registers */
  /** @brief SPOER address - Software Port Output Enable Register */
  k_poe3_spoer_addr = 0x0008C4CAU,

  /** @brief POECR1 address - Port Output Enable Control 1 (MTU0) */
  k_poe3_poecr1_addr = 0x0008C4CBU,

  /** @brief POECR2 address - Port Output Enable Control 2 (MTU3/4) */
  k_poe3_poecr2_addr = 0x0008C4CCU,

  /** @brief POECR4 address - Port Output Enable Control 4 (MTU6/7) */
  k_poe3_poecr4_addr = 0x0008C4D0U,

  /** @brief POECR5 address - Port Output Enable Control 5 (additional) */
  k_poe3_poecr5_addr = 0x0008C4D2U,

  /** @brief ALR1 address - Active Level Register 1 */
  k_poe3_alr1_addr = 0x0008C4DAU,

  /* Pin Select Registers */
  /** @brief M0SELR1 address - MTU0 Pin Select 1 */
  k_poe3_m0selr1_addr = 0x0008C4E4U,

  /** @brief M0SELR2 address - MTU0 Pin Select 2 */
  k_poe3_m0selr2_addr = 0x0008C4E5U,

  /** @brief M3SELR address - MTU3 Pin Select */
  k_poe3_m3selr_addr = 0x0008C4E6U,

  /** @brief M4SELR1 address - MTU4 Pin Select 1 */
  k_poe3_m4selr1_addr = 0x0008C4E7U,

  /** @brief M4SELR2 address - MTU4 Pin Select 2 */
  k_poe3_m4selr2_addr = 0x0008C4E8U,

  /** @brief M6SELR address - MTU6 Pin Select */
  k_poe3_m6selr_addr = 0x0008C4EAU,
} poe3_addresses_t;

/**
 * @enum poe3_offsets_t
 * @brief POE3 register offsets from base address
 *
 * @details
 * Offsets from POE3 base address (0x0008C4C0) for each register.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_poe3_offset_icsr1   = 0x00U, /**< ICSR1 offset (16-bit) */
  k_poe3_offset_ocsr1   = 0x02U, /**< OCSR1 offset (16-bit) */
  k_poe3_offset_icsr2   = 0x04U, /**< ICSR2 offset (16-bit) */
  k_poe3_offset_ocsr2   = 0x06U, /**< OCSR2 offset (16-bit) */
  k_poe3_offset_icsr3   = 0x08U, /**< ICSR3 offset (16-bit) */
  k_poe3_offset_spoer   = 0x0AU, /**< SPOER offset (8-bit) */
  k_poe3_offset_poecr1  = 0x0BU, /**< POECR1 offset (8-bit) */
  k_poe3_offset_poecr2  = 0x0CU, /**< POECR2 offset (8-bit) */
  k_poe3_offset_poecr4  = 0x10U, /**< POECR4 offset (16-bit) */
  k_poe3_offset_poecr5  = 0x12U, /**< POECR5 offset (16-bit) */
  k_poe3_offset_icsr4   = 0x16U, /**< ICSR4 offset (16-bit) */
  k_poe3_offset_icsr5   = 0x18U, /**< ICSR5 offset (16-bit) */
  k_poe3_offset_alr1    = 0x1AU, /**< ALR1 offset (16-bit) */
  k_poe3_offset_icsr6   = 0x1CU, /**< ICSR6 offset (16-bit) */
  k_poe3_offset_m0selr1 = 0x24U, /**< M0SELR1 offset (8-bit) */
  k_poe3_offset_m0selr2 = 0x25U, /**< M0SELR2 offset (8-bit) */
  k_poe3_offset_m3selr  = 0x26U, /**< M3SELR offset (8-bit) */
  k_poe3_offset_m4selr1 = 0x27U, /**< M4SELR1 offset (8-bit) */
  k_poe3_offset_m4selr2 = 0x28U, /**< M4SELR2 offset (8-bit) */
  k_poe3_offset_m6selr  = 0x2AU, /**< M6SELR offset (8-bit) */
} poe3_offsets_t;

/* =============================================================================
 * ICSR1/2/4/5 Register Bits (Input Level Control/Status)
 * =============================================================================
 */

/**
 * @enum icsr_poemode_t
 * @brief POE# Mode Select bits (POE0M/POE4M/POE8M/POE10M/POE11M)
 *
 * @details
 * Selects input detection mode for POE# pins.
 * These bits can only be modified once after reset.
 *
 * @note Manual ref: Ch25 sections 25.2.1-25.2.5
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /** @brief Accept on falling edge of POE# input */
  k_icsr_poemode_falling_edge = 0x0000U,

  /** @brief Low-level sampling: 16 samples at PCLK/8 */
  k_icsr_poemode_lowlevel_pclk8 = 0x0001U,

  /** @brief Low-level sampling: 16 samples at PCLK/16 */
  k_icsr_poemode_lowlevel_pclk16 = 0x0002U,

  /** @brief Low-level sampling: 16 samples at PCLK/128 */
  k_icsr_poemode_lowlevel_pclk128 = 0x0003U,

  /** @brief POEm mode select mask (bits 1:0) */
  k_icsr_poemode_mask = 0x0003U,
} icsr_poemode_t;

/**
 * @enum icsr_bits_t
 * @brief ICSR common bit definitions
 *
 * @par Register Layout (16-bit)
 * | Bits | Field | Description |
 * |------|-------|-------------|
 * | 1:0 | POEmM | POE# mode select |
 * | 7:2 | - | Reserved |
 * | 8 | PIEn | Port interrupt enable |
 * | 9 | POE8E | High-impedance enable (ICSR3 only) |
 * | 11:10 | - | Reserved |
 * | 12 | POEmF | POE# flag |
 * | 15:13 | - | Reserved |
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* Common bits for ICSR1/2/3/4/5 */
  /** @brief Port Interrupt Enable (bit 8) */
  k_icsr_pie = 0x0100U,

  /** @brief POE# Flag (bit 12) - high-impedance request detected */
  k_icsr_poef = 0x1000U,

  /* ICSR3-specific bits */
  /** @brief POE8 High-Impedance Enable (bit 9, ICSR3 only) */
  k_icsr3_poe8e = 0x0200U,
} icsr_bits_t;

/**
 * @enum icsr6_bits_t
 * @brief ICSR6 bit definitions (Oscillation Stop Detection)
 *
 * @par Register Layout (16-bit @ 0x0008C4DC)
 * | Bits | Field | Description |
 * |------|-------|-------------|
 * | 8:0 | - | Reserved |
 * | 9 | OSTSTE | Osc stop high-impedance enable |
 * | 11:10 | - | Reserved |
 * | 12 | OSTSTF | Osc stop flag |
 * | 15:13 | - | Reserved |
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /** @brief Oscillation Stop High-Impedance Enable (bit 9) */
  k_icsr6_ostste = 0x0200U,

  /** @brief Oscillation Stop Flag (bit 12) */
  k_icsr6_oststf = 0x1000U,
} icsr6_bits_t;

/* =============================================================================
 * OCSR1/2 Register Bits (Output Level Control/Status)
 * =============================================================================
 */

/**
 * @enum ocsr_bits_t
 * @brief OCSR bit definitions (simultaneous conduction detection)
 *
 * @par Register Layout (16-bit)
 * | Bits | Field | Description |
 * |------|-------|-------------|
 * | 7:0 | - | Reserved |
 * | 8 | OIEn | Simultaneous conduction interrupt enable |
 * | 9 | OCEn | Simultaneous conduction high-impedance enable |
 * | 14:10 | - | Reserved |
 * | 15 | OSFn | Simultaneous conduction flag |
 *
 * @details
 * Used to detect and respond to simultaneous active-level output on
 * complementary PWM pin pairs (shoot-through detection).
 *
 * @note Manual ref: Ch25 sections 25.2.7, 25.2.8
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /** @brief Simultaneous Conduction Interrupt Enable (bit 8) */
  k_ocsr_oie = 0x0100U,

  /**
   * @brief Simultaneous Conduction High-Impedance Enable (bit 9)
   * @details Can only be modified once after reset.
   */
  k_ocsr_oce = 0x0200U,

  /** @brief Simultaneous Conduction Flag (bit 15) */
  k_ocsr_osf = 0x8000U,
} ocsr_bits_t;

/* =============================================================================
 * SPOER Register Bits (Software Port Output Enable)
 * =============================================================================
 */

/**
 * @enum spoer_bits_t
 * @brief SPOER bit definitions (software high-impedance control)
 *
 * @par Register Layout (8-bit @ 0x0008C4CA)
 * | Bit | Field | Description |
 * |-----|-------|-------------|
 * | 0 | MTUCH34HIZ | MTU3/4 high-impedance enable |
 * | 1 | MTUCH67HIZ | MTU6/7 high-impedance enable |
 * | 2 | MTUCH0HIZ | MTU0 high-impedance enable |
 * | 7:3 | - | Reserved |
 *
 * @details
 * Software-triggered emergency stop for MTU pins.
 * Writing 1 puts outputs in high-impedance state immediately.
 *
 * @note Manual ref: Ch25 section 25.2.10
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief MTU3/4 Pins High-Impedance Enable (bit 0) */
  k_spoer_mtuch34hiz = 0x01U,

  /** @brief MTU6/7 Pins High-Impedance Enable (bit 1) */
  k_spoer_mtuch67hiz = 0x02U,

  /** @brief MTU0 Pins High-Impedance Enable (bit 2) */
  k_spoer_mtuch0hiz = 0x04U,
} spoer_bits_t;

/* =============================================================================
 * POECR1 Register Bits (MTU0 Pin Control)
 * =============================================================================
 */

/**
 * @enum poecr1_bits_t
 * @brief POECR1 bit definitions (MTU0 pin high-impedance enable)
 *
 * @par Register Layout (8-bit @ 0x0008C4CB)
 * | Bit | Field | Description |
 * |-----|-------|-------------|
 * | 0 | MTU0AZE | MTIOC0A high-impedance enable |
 * | 1 | MTU0BZE | MTIOC0B high-impedance enable |
 * | 2 | MTU0CZE | MTIOC0C high-impedance enable |
 * | 3 | MTU0DZE | MTIOC0D high-impedance enable |
 * | 7:4 | - | Reserved |
 *
 * @note These bits can only be modified once after reset.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_poecr1_mtu0aze = 0x01U, /**< MTIOC0A high-impedance enable */
  k_poecr1_mtu0bze = 0x02U, /**< MTIOC0B high-impedance enable */
  k_poecr1_mtu0cze = 0x04U, /**< MTIOC0C high-impedance enable */
  k_poecr1_mtu0dze = 0x08U, /**< MTIOC0D high-impedance enable */
} poecr1_bits_t;

/* =============================================================================
 * POECR2 Register Bits (MTU3/4 Pin Control)
 * =============================================================================
 */

/**
 * @enum poecr2_bits_t
 * @brief POECR2 bit definitions (MTU3/4 and MTU6/7 complementary PWM control)
 *
 * @par Register Layout (16-bit @ 0x0008C4CC)
 * | Bits  | Field     | Description                                |
 * |-------|-----------|--------------------------------------------|
 * | 1:0   | -         | Reserved (MTU7BDZE, MTU7ACZE in b0,b1)     |
 * | 0     | MTU7BDZE  | MTIOC7B/7D high-impedance enable           |
 * | 1     | MTU7ACZE  | MTIOC7A/7C high-impedance enable           |
 * | 2     | MTU6BDZE  | MTIOC6B/6D high-impedance enable           |
 * | 7:3   | -         | Reserved                                   |
 * | 8     | MTU4BDZE  | MTIOC4B/4D high-impedance enable           |
 * | 9     | MTU4ACZE  | MTIOC4A/4C high-impedance enable           |
 * | 10    | MTU3BDZE  | MTIOC3B/3D high-impedance enable           |
 * | 15:11 | -         | Reserved                                   |
 *
 * @details
 * Reset value: b10=1, b9=1, b8=1, b2=1, b1=1, b0=1 (all ZE bits set).
 * Note 2 in the manual: Set MTU6/7 bits (b2:b0) to 0 when MTU6 or MTU7
 * is not used.
 *
 * @note These bits can only be modified once after reset.
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* MTU6/7 hi-Z enable (lower byte, bits 2:0) */
  k_poecr2_mtu7bdze = 0x0001U, /**< MTIOC7B/7D high-impedance enable (bit 0) */
  k_poecr2_mtu7acze = 0x0002U, /**< MTIOC7A/7C high-impedance enable (bit 1) */
  k_poecr2_mtu6bdze = 0x0004U, /**< MTIOC6B/6D high-impedance enable (bit 2) */
  /* MTU3/4 hi-Z enable (upper byte, bits 10:8) */
  k_poecr2_mtu4bdze = 0x0100U, /**< MTIOC4B/4D high-impedance enable (bit 8) */
  k_poecr2_mtu4acze = 0x0200U, /**< MTIOC4A/4C high-impedance enable (bit 9) */
  k_poecr2_mtu3bdze = 0x0400U, /**< MTIOC3B/3D high-impedance enable (bit 10) */
} poecr2_bits_t;

/* =============================================================================
 * POECR4 Register Bits (MTU6/7 Pin Control)
 * =============================================================================
 */

/**
 * @enum poecr4_bits_t
 * @brief POECR4 bit definitions (add POE# flag conditions for MTU3/4 and MTU6/7)
 *
 * @par Register Layout (16-bit @ 0x0008C4D0)
 * | Bits  | Field           | Description                                        |
 * |-------|-----------------|----------------------------------------------------|
 * | 1:0   | -               | Reserved (b0 reads 0, b1 reads 1, write as read)   |
 * | 2     | IC2ADDMT34ZE    | Add POE4F condition to MTU3/4 hi-Z                 |
 * | 3     | IC3ADDMT34ZE    | Add POE8F condition to MTU3/4 hi-Z                 |
 * | 4     | IC4ADDMT34ZE    | Add POE10F condition to MTU3/4 hi-Z                |
 * | 5     | IC5ADDMT34ZE    | Add POE11F condition to MTU3/4 hi-Z                |
 * | 8:6   | -               | Reserved                                           |
 * | 9     | IC1ADDMT67ZE    | Add POE0F condition to MTU6/7 hi-Z                 |
 * | 10    | -               | Reserved (reads 1, write 1)                        |
 * | 11    | IC3ADDMT67ZE    | Add POE8F condition to MTU6/7 hi-Z                 |
 * | 12    | IC4ADDMT67ZE    | Add POE10F condition to MTU6/7 hi-Z                |
 * | 13    | IC5ADDMT67ZE    | Add POE11F condition to MTU6/7 hi-Z                |
 * | 15:14 | -               | Reserved                                           |
 *
 * @details
 * POECR4 extends the high-impedance control conditions for MTU3/4 and MTU6/7
 * pins by adding optional POE# flag triggers.
 *
 * @note These bits can only be modified once after reset.
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* MTU3/4 additional condition bits */
  k_poecr4_ic2addmt34ze = 0x0004U, /**< Add POE4F condition to MTU3/4 hi-Z (bit 2) */
  k_poecr4_ic3addmt34ze = 0x0008U, /**< Add POE8F condition to MTU3/4 hi-Z (bit 3) */
  k_poecr4_ic4addmt34ze = 0x0010U, /**< Add POE10F condition to MTU3/4 hi-Z (bit 4) */
  k_poecr4_ic5addmt34ze = 0x0020U, /**< Add POE11F condition to MTU3/4 hi-Z (bit 5) */
  /* MTU6/7 additional condition bits */
  k_poecr4_ic1addmt67ze = 0x0200U, /**< Add POE0F condition to MTU6/7 hi-Z (bit 9) */
  k_poecr4_ic3addmt67ze = 0x0800U, /**< Add POE8F condition to MTU6/7 hi-Z (bit 11) */
  k_poecr4_ic4addmt67ze = 0x1000U, /**< Add POE10F condition to MTU6/7 hi-Z (bit 12) */
  k_poecr4_ic5addmt67ze = 0x2000U, /**< Add POE11F condition to MTU6/7 hi-Z (bit 13) */
} poecr4_bits_t;

/* =============================================================================
 * POECR5 Register Bits (Additional Pin Control)
 * =============================================================================
 */

/**
 * @enum poecr5_bits_t
 * @brief POECR5 bit definitions (add POE# flag conditions for MTU0 pins)
 *
 * @par Register Layout (16-bit @ 0x0008C4D2)
 * | Bits  | Field          | Description                                       |
 * |-------|----------------|---------------------------------------------------|
 * | 0     | -              | Reserved (reads 0, write 0)                       |
 * | 1     | IC1ADDMT0ZE    | Add POE0F condition to MTU0 hi-Z                  |
 * | 2     | IC2ADDMT0ZE    | Add POE4F condition to MTU0 hi-Z                  |
 * | 3     | -              | Reserved (reads 1, write 1)                       |
 * | 4     | IC4ADDMT0ZE    | Add POE10F condition to MTU0 hi-Z                 |
 * | 5     | IC5ADDMT0ZE    | Add POE11F condition to MTU0 hi-Z                 |
 * | 15:6  | -              | Reserved                                          |
 *
 * @details
 * POECR5 extends the high-impedance control conditions for MTU0 pins by
 * adding optional POE# flag triggers beyond the default POE8F/SPOER conditions.
 *
 * @note These bits can only be modified once after reset.
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_poecr5_ic1addmt0ze = 0x0002U, /**< Add POE0F condition to MTU0 hi-Z (bit 1) */
  k_poecr5_ic2addmt0ze = 0x0004U, /**< Add POE4F condition to MTU0 hi-Z (bit 2) */
  k_poecr5_ic4addmt0ze = 0x0010U, /**< Add POE10F condition to MTU0 hi-Z (bit 4) */
  k_poecr5_ic5addmt0ze = 0x0020U, /**< Add POE11F condition to MTU0 hi-Z (bit 5) */
} poecr5_bits_t;

/* =============================================================================
 * ALR1 Register Bits (Active Level Setting)
 * =============================================================================
 */

/**
 * @enum alr1_bits_t
 * @brief ALR1 bit definitions (active level for shoot-through detection)
 *
 * @par Register Layout (16-bit @ 0x0008C4DA)
 * | Bits | Field | Description |
 * |------|-------|-------------|
 * | 0 | OLSG0A | Active level for MTIOC3B/4A (group 0, positive) |
 * | 1 | OLSG0B | Active level for MTIOC3D/4C (group 0, negative) |
 * | 2 | OLSG1A | Active level for MTIOC3B/4A (group 1, positive) |
 * | 3 | OLSG1B | Active level for MTIOC3D/4C (group 1, negative) |
 * | 4 | OLSG2A | Active level for MTIOC4B (group 2, positive) |
 * | 5 | OLSG2B | Active level for MTIOC4D (group 2, negative) |
 * | 6 | - | Reserved |
 * | 7 | OLSEN | Active level select enable |
 * | 15:8 | - | Reserved |
 *
 * @details
 * When OLSEN=0, active level determined by MTU.TOCR registers.
 * When OLSEN=1, active level determined by OLSGnA/B bits (0=low, 1=high).
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_alr1_olsg0a = 0x0001U, /**< Active level for group 0 positive */
  k_alr1_olsg0b = 0x0002U, /**< Active level for group 0 negative */
  k_alr1_olsg1a = 0x0004U, /**< Active level for group 1 positive */
  k_alr1_olsg1b = 0x0008U, /**< Active level for group 1 negative */
  k_alr1_olsg2a = 0x0010U, /**< Active level for group 2 positive */
  k_alr1_olsg2b = 0x0020U, /**< Active level for group 2 negative */
  k_alr1_olsen  = 0x0080U, /**< Active level select enable */
} alr1_bits_t;

/* =============================================================================
 * Module Stop Control
 * =============================================================================
 */

/**
 * @enum poe3_mstpcr_t
 * @brief POE3 module stop control bits
 *
 * @details
 * POE3 is controlled by MSTPCRA.MSTPA9.
 * Must be cleared (0) before accessing POE3 registers.
 *
 * @note Manual ref: Ch11 (Low Power Consumption) module stop tables
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief MSTPCRA bit for POE3 (bit 9)
   * @details
   * - 0: POE3 module is running
   * - 1: POE3 module is stopped (default)
   */
  k_poe3_mstpcra_bit = (1U << 9U),
} poe3_mstpcr_t;

/* =============================================================================
 * Register Accessor Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to ICSR1 register (POE0# control)
 * @return Volatile pointer to ICSR1 register (16-bit)
 * @pre POE3 module stop bit (MSTPCRA.MSTPA9) must be cleared
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_icsr1_reg(void)
{
  return (volatile uint16_t*)k_poe3_icsr1_addr;
}

/**
 * @brief Get pointer to ICSR2 register (POE4# control)
 * @return Volatile pointer to ICSR2 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_icsr2_reg(void)
{
  return (volatile uint16_t*)k_poe3_icsr2_addr;
}

/**
 * @brief Get pointer to ICSR3 register (POE8# control)
 * @return Volatile pointer to ICSR3 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_icsr3_reg(void)
{
  return (volatile uint16_t*)k_poe3_icsr3_addr;
}

/**
 * @brief Get pointer to ICSR4 register (POE10# control)
 * @return Volatile pointer to ICSR4 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_icsr4_reg(void)
{
  return (volatile uint16_t*)k_poe3_icsr4_addr;
}

/**
 * @brief Get pointer to ICSR5 register (POE11# control)
 * @return Volatile pointer to ICSR5 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_icsr5_reg(void)
{
  return (volatile uint16_t*)k_poe3_icsr5_addr;
}

/**
 * @brief Get pointer to ICSR6 register (oscillation stop)
 * @return Volatile pointer to ICSR6 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_icsr6_reg(void)
{
  return (volatile uint16_t*)k_poe3_icsr6_addr;
}

/**
 * @brief Get pointer to OCSR1 register (MTU3/4 shoot-through)
 * @return Volatile pointer to OCSR1 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_ocsr1_reg(void)
{
  return (volatile uint16_t*)k_poe3_ocsr1_addr;
}

/**
 * @brief Get pointer to OCSR2 register (MTU6/7 shoot-through)
 * @return Volatile pointer to OCSR2 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_ocsr2_reg(void)
{
  return (volatile uint16_t*)k_poe3_ocsr2_addr;
}

/**
 * @brief Get pointer to SPOER register (software emergency stop)
 * @return Volatile pointer to SPOER register (8-bit)
 *
 * @par Example - Emergency stop all MTU outputs:
 * @code{.c}
 * // Immediately disable all MTU complementary PWM outputs
 * *poe3_spoer_reg() = k_spoer_mtuch0hiz | k_spoer_mtuch34hiz | k_spoer_mtuch67hiz;
 * @endcode
 *
 * @since Version 1.0.0
 */
static inline volatile uint8_t* poe3_spoer_reg(void)
{
  return (volatile uint8_t*)k_poe3_spoer_addr;
}

/**
 * @brief Get pointer to POECR1 register (MTU0 pin control)
 * @return Volatile pointer to POECR1 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* poe3_poecr1_reg(void)
{
  return (volatile uint8_t*)k_poe3_poecr1_addr;
}

/**
 * @brief Get pointer to POECR2 register (MTU3/4 and MTU6/7 pin control)
 * @return Volatile pointer to POECR2 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_poecr2_reg(void)
{
  return (volatile uint16_t*)k_poe3_poecr2_addr;
}

/**
 * @brief Get pointer to POECR4 register (MTU3/4 and MTU6/7 additional POE# conditions)
 * @return Volatile pointer to POECR4 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_poecr4_reg(void)
{
  return (volatile uint16_t*)k_poe3_poecr4_addr;
}

/**
 * @brief Get pointer to POECR5 register (MTU0 additional POE# conditions)
 * @return Volatile pointer to POECR5 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_poecr5_reg(void)
{
  return (volatile uint16_t*)k_poe3_poecr5_addr;
}

/**
 * @brief Get pointer to ALR1 register (active level)
 * @return Volatile pointer to ALR1 register (16-bit)
 * @since Version 1.0.0
 */
static inline volatile uint16_t* poe3_alr1_reg(void)
{
  return (volatile uint16_t*)k_poe3_alr1_addr;
}

/**
 * @brief Get pointer to M0SELR1 register (MTU0 pin select 1)
 * @return Volatile pointer to M0SELR1 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* poe3_m0selr1_reg(void)
{
  return (volatile uint8_t*)k_poe3_m0selr1_addr;
}

/**
 * @brief Get pointer to M0SELR2 register (MTU0 pin select 2)
 * @return Volatile pointer to M0SELR2 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* poe3_m0selr2_reg(void)
{
  return (volatile uint8_t*)k_poe3_m0selr2_addr;
}

/**
 * @brief Get pointer to M3SELR register (MTU3 pin select)
 * @return Volatile pointer to M3SELR register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* poe3_m3selr_reg(void)
{
  return (volatile uint8_t*)k_poe3_m3selr_addr;
}

/**
 * @brief Get pointer to M4SELR1 register (MTU4 pin select 1)
 * @return Volatile pointer to M4SELR1 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* poe3_m4selr1_reg(void)
{
  return (volatile uint8_t*)k_poe3_m4selr1_addr;
}

/**
 * @brief Get pointer to M4SELR2 register (MTU4 pin select 2)
 * @return Volatile pointer to M4SELR2 register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* poe3_m4selr2_reg(void)
{
  return (volatile uint8_t*)k_poe3_m4selr2_addr;
}

/**
 * @brief Get pointer to M6SELR register (MTU6 pin select)
 * @return Volatile pointer to M6SELR register (8-bit)
 * @since Version 1.0.0
 */
static inline volatile uint8_t* poe3_m6selr_reg(void)
{
  return (volatile uint8_t*)k_poe3_m6selr_addr;
}

/* =============================================================================
 * Static Assertions - Address Verification
 * =============================================================================
 */

/**
 * @name Address Verification
 * @brief Compile-time verification of register addresses
 * @{
 */

/* Verify base address */
static_assert(k_poe3_base_addr == 0x0008C4C0U,
              "POE3 base address must be 0x0008C4C0 per Ch25 manual");

/* Verify ICSR register addresses */
static_assert(k_poe3_icsr1_addr == 0x0008C4C0U, "ICSR1 address must be 0x0008C4C0 per Ch25 manual");

static_assert(k_poe3_icsr2_addr == 0x0008C4C4U, "ICSR2 address must be 0x0008C4C4 per Ch25 manual");

static_assert(k_poe3_icsr3_addr == 0x0008C4C8U, "ICSR3 address must be 0x0008C4C8 per Ch25 manual");

static_assert(k_poe3_icsr4_addr == 0x0008C4D6U, "ICSR4 address must be 0x0008C4D6 per Ch25 manual");

static_assert(k_poe3_icsr5_addr == 0x0008C4D8U, "ICSR5 address must be 0x0008C4D8 per Ch25 manual");

static_assert(k_poe3_icsr6_addr == 0x0008C4DCU, "ICSR6 address must be 0x0008C4DC per Ch25 manual");

/* Verify OCSR register addresses */
static_assert(k_poe3_ocsr1_addr == 0x0008C4C2U, "OCSR1 address must be 0x0008C4C2 per Ch25 manual");

static_assert(k_poe3_ocsr2_addr == 0x0008C4C6U, "OCSR2 address must be 0x0008C4C6 per Ch25 manual");

/* Verify control register addresses */
static_assert(k_poe3_spoer_addr == 0x0008C4CAU, "SPOER address must be 0x0008C4CA per Ch25 manual");

static_assert(k_poe3_poecr1_addr == 0x0008C4CBU,
              "POECR1 address must be 0x0008C4CB per Ch25 manual");

static_assert(k_poe3_poecr2_addr == 0x0008C4CCU,
              "POECR2 address must be 0x0008C4CC per Ch25 manual");

static_assert(k_poe3_poecr4_addr == 0x0008C4D0U,
              "POECR4 address must be 0x0008C4D0 per Ch25 manual");

static_assert(k_poe3_poecr5_addr == 0x0008C4D2U,
              "POECR5 address must be 0x0008C4D2 per Ch25 manual");

static_assert(k_poe3_alr1_addr == 0x0008C4DAU, "ALR1 address must be 0x0008C4DA per Ch25 manual");

/* Verify pin select register addresses */
static_assert(k_poe3_m0selr1_addr == 0x0008C4E4U,
              "M0SELR1 address must be 0x0008C4E4 per Ch25 manual");

static_assert(k_poe3_m0selr2_addr == 0x0008C4E5U,
              "M0SELR2 address must be 0x0008C4E5 per Ch25 manual");

static_assert(k_poe3_m3selr_addr == 0x0008C4E6U,
              "M3SELR address must be 0x0008C4E6 per Ch25 manual");

static_assert(k_poe3_m4selr1_addr == 0x0008C4E7U,
              "M4SELR1 address must be 0x0008C4E7 per Ch25 manual");

static_assert(k_poe3_m4selr2_addr == 0x0008C4E8U,
              "M4SELR2 address must be 0x0008C4E8 per Ch25 manual");

static_assert(k_poe3_m6selr_addr == 0x0008C4EAU,
              "M6SELR address must be 0x0008C4EA per Ch25 manual");

/* Verify register offsets from base */
static_assert(k_poe3_icsr1_addr - k_poe3_base_addr == k_poe3_offset_icsr1,
              "ICSR1 offset must match");

static_assert(k_poe3_ocsr1_addr - k_poe3_base_addr == k_poe3_offset_ocsr1,
              "OCSR1 offset must match");

static_assert(k_poe3_spoer_addr - k_poe3_base_addr == k_poe3_offset_spoer,
              "SPOER offset must match");

static_assert(k_poe3_alr1_addr - k_poe3_base_addr == k_poe3_offset_alr1, "ALR1 offset must match");

/** @} */

#ifdef __cplusplus
}
#endif
