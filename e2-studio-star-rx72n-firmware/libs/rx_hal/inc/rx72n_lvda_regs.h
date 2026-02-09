/* lib/rx_hal/inc/rx72n_lvda_regs.h */

/**
 * @file rx72n_lvda_regs.h
 * @brief RX72N Voltage Detection Circuit (LVDA) Register Definitions
 *
 * @details
 * Register definitions for the LVDA module which provides brownout protection
 * and voltage monitoring for safety-critical motor control. The LVDA can
 * trigger resets or interrupts when VCC drops below configurable thresholds.
 *
 * @par Voltage Monitoring System
 * @dot
 * digraph lvda_system {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_power {
 *     label="Power Supply";
 *     style=filled;
 *     color=lightblue;
 *
 *     vcc [label="VCC (3.3V)"];
 *   }
 *
 *   subgraph cluster_lvda {
 *     label="LVDA Circuit";
 *     style=filled;
 *     color=lightyellow;
 *
 *     mon0 [label="Voltage Monitor 0\n(Vdet0: OFS1 config)"];
 *     mon1 [label="Voltage Monitor 1\n(Vdet1: 2.85-2.99V)"];
 *     mon2 [label="Voltage Monitor 2\n(Vdet2: 2.85-2.99V)"];
 *   }
 *
 *   subgraph cluster_actions {
 *     label="Actions on Low Voltage";
 *     style=filled;
 *     color=lightcoral;
 *
 *     reset [label="System Reset"];
 *     nmi [label="NMI Interrupt"];
 *     irq [label="Maskable IRQ"];
 *   }
 *
 *   vcc -> mon0;
 *   vcc -> mon1;
 *   vcc -> mon2;
 *   mon0 -> reset [label="Vdet0 > VCC"];
 *   mon1 -> reset [label="LVD1RI=1"];
 *   mon1 -> nmi [label="LVD1IRQSEL=0"];
 *   mon1 -> irq [label="LVD1IRQSEL=1"];
 *   mon2 -> reset [label="LVD2RI=1"];
 *   mon2 -> nmi [label="LVD2IRQSEL=0"];
 *   mon2 -> irq [label="LVD2IRQSEL=1"];
 * }
 * @enddot
 *
 * @par STAR Project LVDA Usage
 * | Monitor   | Threshold | Action    | Use Case                        |
 * |-----------|-----------|-----------|-------------------------------- |
 * | Vdet0     | OFS1 conf | Reset     | Emergency brownout protection   |
 * | Vdet1     | 2.92V     | Interrupt | Low battery warning             |
 * | Vdet2     | 2.85V     | Reset     | Critical brownout shutdown      |
 *
 * @par Voltage Detection Levels
 * | Level     | Voltage (typ) | Hysteresis | Notes                       |
 * |-----------|---------------|------------|------------------------------|
 * | Vdet1_1   | 2.99V         | ~50mV      | Early warning                 |
 * | Vdet1_2   | 2.92V         | ~50mV      | Low battery threshold         |
 * | Vdet1_3   | 2.85V         | ~50mV      | Critical threshold            |
 *
 * @par Hardware Requirements
 * | Parameter      | Value            | Notes                        |
 * |----------------|------------------|------------------------------|
 * | LVD1/2 Regs    | 0x000800E0       | 4-byte block                 |
 * | LVCMPCR etc    | 0x0008C297       | 5-byte block (non-contiguous)|
 * | Enable Time    | td(E-A)          | Wait after enabling LVDnE    |
 * | Clock          | LOCO (240 kHz)   | For digital filter           |
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
 * @par Module Dependencies
 * - `<stdint.h>` - Fixed-width integer types
 *
 * @par Manual References
 * RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 8: Voltage Detection Circuit (LVDA), pages 316-334
 *
 * @see rx72n_regs.h Master register include file
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 * @since Version 1.0.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * LVDA - Voltage Detection Circuit for Brownout Protection
 * =============================================================================
 */

/**
 * @enum rx_lvda_addresses_t
 * @brief LVDA register addresses
 *
 * @details
 * The LVDA registers are located in two non-contiguous memory regions:
 * - Control/Status registers at 0x000800E0-0x000800E3
 * - Configuration registers at 0x0008C297-0x0008C29B
 *
 * @par Memory Map
 * | Address      | Register | Size | Description                       |
 * |--------------|----------|------|-----------------------------------|
 * | 0x000800E0   | LVD1CR1  | 1    | Voltage Monitor 1 Control Reg 1   |
 * | 0x000800E1   | LVD1SR   | 1    | Voltage Monitor 1 Status          |
 * | 0x000800E2   | LVD2CR1  | 1    | Voltage Monitor 2 Control Reg 1   |
 * | 0x000800E3   | LVD2SR   | 1    | Voltage Monitor 2 Status          |
 * | 0x0008C297   | LVCMPCR  | 1    | Voltage Monitor Circuit Control   |
 * | 0x0008C298   | LVDLVLR  | 1    | Voltage Detection Level Select    |
 * | 0x0008C29A   | LVD1CR0  | 1    | Voltage Monitor 1 Control Reg 0   |
 * | 0x0008C29B   | LVD2CR0  | 1    | Voltage Monitor 2 Control Reg 0   |
 *
 * @note Manual: Ch8.2 - Register Descriptions
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /* First register block (0x000800E0-0x000800E3) */
  k_lvd1cr1_addr = 0x000800E0, /**< Voltage Monitor 1 Control Register 1 */
  k_lvd1sr_addr  = 0x000800E1, /**< Voltage Monitor 1 Status Register */
  k_lvd2cr1_addr = 0x000800E2, /**< Voltage Monitor 2 Control Register 1 */
  k_lvd2sr_addr  = 0x000800E3, /**< Voltage Monitor 2 Status Register */

  /* Second register block (0x0008C297-0x0008C29B) */
  k_lvcmpcr_addr  = 0x0008C297, /**< Voltage Monitor Circuit Control Register */
  k_lvdlvlr_addr  = 0x0008C298, /**< Voltage Detection Level Select Register */
  /* Note: 0x0008C299 is reserved */
  k_lvd1cr0_addr  = 0x0008C29A, /**< Voltage Monitor 1 Control Register 0 */
  k_lvd2cr0_addr  = 0x0008C29B, /**< Voltage Monitor 2 Control Register 0 */
} rx_lvda_addresses_t;

/* =============================================================================
 * Register Accessor Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to LVD1CR1 register
 *
 * @details
 * Voltage Monitoring 1 Circuit Control Register 1. Controls interrupt
 * generation condition and interrupt type selection.
 *
 * @return Volatile pointer to LVD1CR1 register
 *
 * @par Register Bits:
 * - b1:0 LVD1IDTSEL[1:0]: Interrupt generation condition select
 * - b2   LVD1IRQSEL: Interrupt type (0=NMI, 1=Maskable)
 *
 * @see rx_lvd1cr1_bits_t Bit definitions
 * @since Version 1.0.0
 */
static inline volatile uint8_t* lvd1cr1_reg(void)
{
  return (volatile uint8_t*)k_lvd1cr1_addr;
}

/**
 * @brief Get pointer to LVD1SR register
 *
 * @details
 * Voltage Monitoring 1 Status Register. Contains detection flag and
 * voltage monitor flag.
 *
 * @return Volatile pointer to LVD1SR register
 *
 * @par Register Bits:
 * - b0 LVD1DET: Voltage change detection flag (write 0 to clear)
 * - b1 LVD1MON: Voltage monitor flag (0=VCC<Vdet1, 1=VCC>=Vdet1)
 *
 * @see rx_lvd1sr_bits_t Bit definitions
 * @since Version 1.0.0
 */
static inline volatile uint8_t* lvd1sr_reg(void)
{
  return (volatile uint8_t*)k_lvd1sr_addr;
}

/**
 * @brief Get pointer to LVD2CR1 register
 *
 * @details
 * Voltage Monitoring 2 Circuit Control Register 1. Controls interrupt
 * generation condition and interrupt type selection.
 *
 * @return Volatile pointer to LVD2CR1 register
 *
 * @see rx_lvd2cr1_bits_t Bit definitions
 * @since Version 1.0.0
 */
static inline volatile uint8_t* lvd2cr1_reg(void)
{
  return (volatile uint8_t*)k_lvd2cr1_addr;
}

/**
 * @brief Get pointer to LVD2SR register
 *
 * @details
 * Voltage Monitoring 2 Status Register. Contains detection flag and
 * voltage monitor flag.
 *
 * @return Volatile pointer to LVD2SR register
 *
 * @see rx_lvd2sr_bits_t Bit definitions
 * @since Version 1.0.0
 */
static inline volatile uint8_t* lvd2sr_reg(void)
{
  return (volatile uint8_t*)k_lvd2sr_addr;
}

/**
 * @brief Get pointer to LVCMPCR register
 *
 * @details
 * Voltage Monitoring Circuit Control Register. Enables/disables voltage
 * detection circuits 1 and 2.
 *
 * @return Volatile pointer to LVCMPCR register
 *
 * @par Register Bits:
 * - b5 LVD1E: Voltage detection 1 circuit enable
 * - b6 LVD2E: Voltage detection 2 circuit enable
 *
 * @see rx_lvcmpcr_bits_t Bit definitions
 * @since Version 1.0.0
 */
static inline volatile uint8_t* lvcmpcr_reg(void)
{
  return (volatile uint8_t*)k_lvcmpcr_addr;
}

/**
 * @brief Get pointer to LVDLVLR register
 *
 * @details
 * Voltage Detection Level Select Register. Sets the detection voltage
 * thresholds for both voltage monitors.
 *
 * @return Volatile pointer to LVDLVLR register
 *
 * @par Register Bits:
 * - b3:0 LVD1LVL[3:0]: Voltage detection 1 level (2.85V/2.92V/2.99V)
 * - b7:4 LVD2LVL[3:0]: Voltage detection 2 level (2.85V/2.92V/2.99V)
 *
 * @see rx_lvdlvlr_bits_t Bit definitions
 * @since Version 1.0.0
 */
static inline volatile uint8_t* lvdlvlr_reg(void)
{
  return (volatile uint8_t*)k_lvdlvlr_addr;
}

/**
 * @brief Get pointer to LVD1CR0 register
 *
 * @details
 * Voltage Monitoring 1 Circuit Control Register 0. Controls interrupt/reset
 * enable, digital filter, and comparison output enable.
 *
 * @return Volatile pointer to LVD1CR0 register
 *
 * @par Register Bits:
 * - b0 LVD1RIE: Interrupt/reset enable
 * - b1 LVD1DFDIS: Digital filter disable (0=enabled, 1=disabled)
 * - b2 LVD1CMPE: Comparison result output enable
 * - b5:4 LVD1FSAMP[1:0]: Digital filter sampling clock select
 * - b6 LVD1RI: Mode select (0=interrupt, 1=reset)
 * - b7 LVD1RN: Reset negate select
 *
 * @see rx_lvd1cr0_bits_t Bit definitions
 * @since Version 1.0.0
 */
static inline volatile uint8_t* lvd1cr0_reg(void)
{
  return (volatile uint8_t*)k_lvd1cr0_addr;
}

/**
 * @brief Get pointer to LVD2CR0 register
 *
 * @details
 * Voltage Monitoring 2 Circuit Control Register 0. Controls interrupt/reset
 * enable, digital filter, and comparison output enable.
 *
 * @return Volatile pointer to LVD2CR0 register
 *
 * @see rx_lvd2cr0_bits_t Bit definitions
 * @since Version 1.0.0
 */
static inline volatile uint8_t* lvd2cr0_reg(void)
{
  return (volatile uint8_t*)k_lvd2cr0_addr;
}

/* =============================================================================
 * Register Bit Definitions
 * =============================================================================
 */

/**
 * @enum rx_lvd_cr1_bits_t
 * @brief LVD1CR1/LVD2CR1 register bit definitions
 *
 * @details
 * Control bits for interrupt generation condition and type selection.
 *
 * @par Interrupt Condition Select (IDTSEL)
 * | Value | Condition                                |
 * |-------|------------------------------------------|
 * | 00    | VCC >= Vdet (rise) detected              |
 * | 01    | VCC < Vdet (drop) detected               |
 * | 10    | Both drop and rise detected              |
 * | 11    | Prohibited                               |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* Interrupt generation condition select (IDTSEL) */
  k_lvd_idtsel_rise = 0x00, /**< Interrupt on VCC >= Vdet (rise) */
  k_lvd_idtsel_drop = 0x01, /**< Interrupt on VCC < Vdet (drop) */
  k_lvd_idtsel_both = 0x02, /**< Interrupt on both rise and drop */

  /* Interrupt type select (IRQSEL) */
  k_lvd_irqsel_nmi     = (0 << 2), /**< Non-maskable interrupt */
  k_lvd_irqsel_maskable = (1 << 2), /**< Maskable interrupt */

  /* Field masks */
  k_lvd_idtsel_mask = 0x03, /**< IDTSEL field mask (bits 1:0) */
  k_lvd_irqsel_mask = 0x04, /**< IRQSEL field mask (bit 2) */
} rx_lvd_cr1_bits_t;

/**
 * @enum rx_lvd_sr_bits_t
 * @brief LVD1SR/LVD2SR register bit definitions
 *
 * @details
 * Status bits for voltage detection and monitoring.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_lvd_det_clear   = 0x00, /**< Clear detection flag (write) */
  k_lvd_det_detected = 0x01, /**< Vdet passage detected (read) */
  k_lvd_mon_below   = 0x00, /**< VCC < Vdet (read) */
  k_lvd_mon_above   = 0x02, /**< VCC >= Vdet or monitor disabled (read) */

  /* Field masks */
  k_lvd_det_mask = 0x01, /**< DET flag mask (bit 0) */
  k_lvd_mon_mask = 0x02, /**< MON flag mask (bit 1) */
} rx_lvd_sr_bits_t;

/**
 * @enum rx_lvcmpcr_bits_t
 * @brief LVCMPCR register bit definitions
 *
 * @details
 * Enable bits for voltage detection circuits 1 and 2.
 *
 * @note VCC must be at least 80mV above detection level when enabling
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_lvd1e_enable  = (1 << 5), /**< Enable voltage detection 1 circuit */
  k_lvd1e_disable = (0 << 5), /**< Disable voltage detection 1 circuit */
  k_lvd2e_enable  = (1 << 6), /**< Enable voltage detection 2 circuit */
  k_lvd2e_disable = (0 << 6), /**< Disable voltage detection 2 circuit */

  /* Field masks */
  k_lvd1e_mask = 0x20, /**< LVD1E field mask (bit 5) */
  k_lvd2e_mask = 0x40, /**< LVD2E field mask (bit 6) */
} rx_lvcmpcr_bits_t;

/**
 * @enum rx_lvdlvlr_bits_t
 * @brief LVDLVLR register bit definitions
 *
 * @details
 * Voltage detection level selection for monitors 1 and 2.
 *
 * @par Voltage Levels (typical values at Ta = 25°C)
 * | Setting | LVD1LVL/LVD2LVL | Voltage  |
 * |---------|-----------------|----------|
 * | _1      | 1001b (0x09)    | 2.99V    |
 * | _2      | 1010b (0x0A)    | 2.92V    |
 * | _3      | 1011b (0x0B)    | 2.85V    |
 *
 * @warning Other values are prohibited
 * @note Can only change when both LVD1E and LVD2E are 0
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* LVD1 voltage levels (bits 3:0) */
  k_lvd1lvl_2v99 = 0x09, /**< Vdet1 = 2.99V (highest) */
  k_lvd1lvl_2v92 = 0x0A, /**< Vdet1 = 2.92V */
  k_lvd1lvl_2v85 = 0x0B, /**< Vdet1 = 2.85V (lowest) */

  /* LVD2 voltage levels (bits 7:4) */
  k_lvd2lvl_2v99 = 0x90, /**< Vdet2 = 2.99V (highest) */
  k_lvd2lvl_2v92 = 0xA0, /**< Vdet2 = 2.92V */
  k_lvd2lvl_2v85 = 0xB0, /**< Vdet2 = 2.85V (lowest) */

  /* Field masks */
  k_lvd1lvl_mask = 0x0F, /**< LVD1LVL field mask (bits 3:0) */
  k_lvd2lvl_mask = 0xF0, /**< LVD2LVL field mask (bits 7:4) */

  /* Default after reset */
  k_lvdlvlr_reset_value = 0xBB, /**< Both at 2.85V after reset */
} rx_lvdlvlr_bits_t;

/**
 * @enum rx_lvd_cr0_bits_t
 * @brief LVD1CR0/LVD2CR0 register bit definitions
 *
 * @details
 * Control bits for interrupt/reset enable, digital filter, and mode selection.
 *
 * @par Digital Filter Sampling Clock (FSAMP)
 * | Value | Clock              | Sample Time   |
 * |-------|--------------------|---------------|
 * | 00    | LOCO/2 (120 kHz)   | ~17 µs        |
 * | 01    | LOCO/4 (60 kHz)    | ~33 µs        |
 * | 10    | LOCO/8 (30 kHz)    | ~67 µs        |
 * | 11    | LOCO/16 (15 kHz)   | ~133 µs       |
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* Interrupt/Reset enable (RIE) */
  k_lvd_rie_disable = (0 << 0), /**< Disable interrupt/reset */
  k_lvd_rie_enable  = (1 << 0), /**< Enable interrupt/reset */

  /* Digital filter disable (DFDIS) */
  k_lvd_dfdis_filter_on  = (0 << 1), /**< Digital filter enabled */
  k_lvd_dfdis_filter_off = (1 << 1), /**< Digital filter disabled */

  /* Comparison result output enable (CMPE) */
  k_lvd_cmpe_disable = (0 << 2), /**< Output disabled */
  k_lvd_cmpe_enable  = (1 << 2), /**< Output enabled */

  /* Digital filter sampling clock select (FSAMP) */
  k_lvd_fsamp_loco_div2  = (0 << 4), /**< LOCO/2 (120 kHz) */
  k_lvd_fsamp_loco_div4  = (1 << 4), /**< LOCO/4 (60 kHz) */
  k_lvd_fsamp_loco_div8  = (2 << 4), /**< LOCO/8 (30 kHz) */
  k_lvd_fsamp_loco_div16 = (3 << 4), /**< LOCO/16 (15 kHz) */

  /* Mode select (RI) */
  k_lvd_ri_interrupt = (0 << 6), /**< Generate interrupt on Vdet passage */
  k_lvd_ri_reset     = (1 << 6), /**< Generate reset when voltage falls below Vdet */

  /* Reset negate select (RN) */
  k_lvd_rn_vcc_above  = (0 << 7), /**< Negate after VCC > Vdet detected */
  k_lvd_rn_after_assert = (1 << 7), /**< Negate after stabilization time from assert */

  /* Field masks */
  k_lvd_rie_mask   = 0x01, /**< RIE field mask (bit 0) */
  k_lvd_dfdis_mask = 0x02, /**< DFDIS field mask (bit 1) */
  k_lvd_cmpe_mask  = 0x04, /**< CMPE field mask (bit 2) */
  k_lvd_fsamp_mask = 0x30, /**< FSAMP field mask (bits 5:4) */
  k_lvd_ri_mask    = 0x40, /**< RI field mask (bit 6) */
  k_lvd_rn_mask    = 0x80, /**< RN field mask (bit 7) */
} rx_lvd_cr0_bits_t;

/**
 * @enum rx_lvd_interrupts_t
 * @brief LVD interrupt vector numbers
 *
 * @details
 * ICU interrupt vector numbers for voltage monitoring. These can be
 * configured as NMI or maskable interrupts.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_lvd1_irq = 88, /**< LVD1 - Voltage monitor 1 interrupt */
  k_lvd2_irq = 89, /**< LVD2 - Voltage monitor 2 interrupt */
} rx_lvd_interrupts_t;

/* =============================================================================
 * Static Assertions - Verify Register Addresses at Compile Time
 * =============================================================================
 */

/* Verify register addresses match Hardware Manual Ch8.2 */
static_assert(k_lvd1cr1_addr == 0x000800E0, "LVD1CR1 address incorrect");
static_assert(k_lvd1sr_addr == 0x000800E1, "LVD1SR address incorrect");
static_assert(k_lvd2cr1_addr == 0x000800E2, "LVD2CR1 address incorrect");
static_assert(k_lvd2sr_addr == 0x000800E3, "LVD2SR address incorrect");
static_assert(k_lvcmpcr_addr == 0x0008C297, "LVCMPCR address incorrect");
static_assert(k_lvdlvlr_addr == 0x0008C298, "LVDLVLR address incorrect");
static_assert(k_lvd1cr0_addr == 0x0008C29A, "LVD1CR0 address incorrect");
static_assert(k_lvd2cr0_addr == 0x0008C29B, "LVD2CR0 address incorrect");

/* Verify bit field values */
static_assert(k_lvd1e_enable == 0x20, "LVD1E bit value incorrect");
static_assert(k_lvd2e_enable == 0x40, "LVD2E bit value incorrect");
static_assert(k_lvd1lvl_2v99 == 0x09, "LVD1LVL 2.99V value incorrect");
static_assert(k_lvd1lvl_2v85 == 0x0B, "LVD1LVL 2.85V value incorrect");
static_assert(k_lvd2lvl_2v99 == 0x90, "LVD2LVL 2.99V value incorrect");
static_assert(k_lvd2lvl_2v85 == 0xB0, "LVD2LVL 2.85V value incorrect");

#ifdef __cplusplus
}
#endif
