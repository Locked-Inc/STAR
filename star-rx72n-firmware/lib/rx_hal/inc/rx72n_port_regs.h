/* lib/rx_hal/inc/rx72n_port_regs.h */

/**
 * @file rx72n_port_regs.h
 * @brief RX72N PORT (GPIO) Register Definitions
 *
 * General Purpose I/O port registers for digital pin control.
 *
 * IMPORTANT: RX72N PORT registers are organized by REGISTER TYPE, not by PORT!
 * - All PDR registers are contiguous at 0x0008C000-0x0008C017
 * - All PODR registers are contiguous at 0x0008C020-0x0008C037
 * - All PIDR registers are contiguous at 0x0008C040-0x0008C057
 * - etc.
 *
 * This differs from some other MCUs where all registers for a port are grouped together.
 *
 * Package Support:
 * - Only 100-pin LFQFP (R5F572NNHGFP#30) is supported
 * - Available ports: 0-5, A-E, J (some limited)
 * - Ports 6, 7, 8, 9, F, G, H, K, L, M, N, Q are not available on this package
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_PORT_REGS_H
#define STAR_RX72N_PORT_REGS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Port Register Base Addresses
 * =============================================================================
 */

/** @brief PORT register block base addresses (organized by register type) */
typedef enum {
  k_port_pdr_base  = 0x0008C000, /**< PDR (Port Direction) base - all ports contiguous */
  k_port_podr_base = 0x0008C020, /**< PODR (Port Output Data) base */
  k_port_pidr_base = 0x0008C040, /**< PIDR (Port Input Data) base */
  k_port_pmr_base  = 0x0008C060, /**< PMR (Port Mode) base */
  k_port_odr0_base = 0x0008C080, /**< ODR0 (Open Drain 0) base */
  k_port_odr1_base = 0x0008C082, /**< ODR1 (Open Drain 1) base */
  k_port_pcr_base  = 0x0008C0C0, /**< PCR (Pull-up Control) base */
  k_port_dscr_base = 0x0008C0E0, /**< DSCR (Drive Capacity) base */
} rx_port_reg_bases_t;

/** @brief Port number offsets for register access (100-pin LFQFP only) */
typedef enum {
  k_port_offset_0 = 0x00, /**< Port 0 offset (limited: P05, P07 only) */
  k_port_offset_1 = 0x01, /**< Port 1 offset (limited: P12-P17 only) */
  k_port_offset_2 = 0x02, /**< Port 2 offset (full: P20-P27) */
  k_port_offset_3 = 0x03, /**< Port 3 offset (full: P30-P37) */
  k_port_offset_4 = 0x04, /**< Port 4 offset (full: P40-P47) */
  k_port_offset_5 = 0x05, /**< Port 5 offset (limited: P50-P55 only) */
  k_port_offset_a = 0x0A, /**< Port A offset (full: PA0-PA7) */
  k_port_offset_b = 0x0B, /**< Port B offset (full: PB0-PB7) */
  k_port_offset_c = 0x0C, /**< Port C offset (full: PC0-PC7) */
  k_port_offset_d = 0x0D, /**< Port D offset (full: PD0-PD7) */
  k_port_offset_e = 0x0E, /**< Port E offset (full: PE0-PE7) */
  k_port_offset_j = 0x12, /**< Port J offset (limited: PJ3, PJ5 only) */
} rx_port_offsets_t;

/* =============================================================================
 * Port Register Structure
 * =============================================================================
 */

/**
 * @brief Port Register Map (per-port view)
 * @details
 * This structure provides access to all registers for a single port.
 * The hardware organizes registers by TYPE (all PDR together, all PODR together),
 * but this struct provides a logical PORT-centric view with correct spacing.
 *
 * Memory layout (using PORT0 as example):
 * - PDR:  0x0008C000 (base + 0x00)
 * - PODR: 0x0008C020 (base + 0x20)
 * - PIDR: 0x0008C040 (base + 0x40)
 * - PMR:  0x0008C060 (base + 0x60)
 * - ODR0: 0x0008C080 (base + 0x80)
 * - ODR1: 0x0008C082 (base + 0x82) - Note: offset by 2!
 * - PCR:  0x0008C0C0 (base + 0xC0)
 * - DSCR: 0x0008C0E0 (base + 0xE0)
 */
typedef struct __attribute__((packed)) {
  volatile uint8_t  pdr;         /**< +0x00: Port Direction Register (0=input, 1=output) */
  volatile uint8_t  _pad1[0x1F]; /**< +0x01 to +0x1F: Reserved/padding */
  volatile uint8_t  podr;        /**< +0x20: Port Output Data Register (output level) */
  volatile uint8_t  _pad2[0x1F]; /**< +0x21 to +0x3F: Reserved/padding */
  volatile uint8_t  pidr;        /**< +0x40: Port Input Data Register (read pin state) */
  volatile uint8_t  _pad3[0x1F]; /**< +0x41 to +0x5F: Reserved/padding */
  volatile uint8_t  pmr;         /**< +0x60: Port Mode Register (0=GPIO, 1=peripheral) */
  volatile uint8_t  _pad4[0x1F]; /**< +0x61 to +0x7F: Reserved/padding */
  volatile uint16_t odr;         /**< +0x80: Open Drain Control (ODR0 at +0, ODR1 at +1) */
  volatile uint8_t  _pad5[0x3E]; /**< +0x82 to +0xBF: Reserved/padding */
  volatile uint8_t  pcr;         /**< +0xC0: Pull-up Control Register */
  volatile uint8_t  _pad6[0x1F]; /**< +0xC1 to +0xDF: Reserved/padding */
  volatile uint8_t  dscr;        /**< +0xE0: Drive Capacity Control Register */
} rx_port_regs_t;

/* Compile-time verification that struct size matches expected layout */
_Static_assert(sizeof(rx_port_regs_t) == 0xE1, "Port register struct size must be 0xE1 bytes");
_Static_assert(offsetof(rx_port_regs_t, pdr) == 0x00, "PDR must be at offset 0x00");
_Static_assert(offsetof(rx_port_regs_t, podr) == 0x20, "PODR must be at offset 0x20");
_Static_assert(offsetof(rx_port_regs_t, pidr) == 0x40, "PIDR must be at offset 0x40");
_Static_assert(offsetof(rx_port_regs_t, pmr) == 0x60, "PMR must be at offset 0x60");
_Static_assert(offsetof(rx_port_regs_t, odr) == 0x80, "ODR must be at offset 0x80");
_Static_assert(offsetof(rx_port_regs_t, pcr) == 0xC0, "PCR must be at offset 0xC0");
_Static_assert(offsetof(rx_port_regs_t, dscr) == 0xE0, "DSCR must be at offset 0xE0");

/* =============================================================================
 * Port Register Access Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to PORT0 registers
 * @return Volatile pointer to PORT0 register structure
 * @note PORT0 PDR = 0x0008C000, PODR = 0x0008C020, PIDR = 0x0008C040
 * @note 100-pin: Only P05, P07 available
 */
static inline volatile rx_port_regs_t* port0(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_0);
}

/**
 * @brief Get pointer to PORT1 registers
 * @return Volatile pointer to PORT1 register structure
 * @note PORT1 PDR = 0x0008C001, PODR = 0x0008C021, PIDR = 0x0008C041
 * @note 100-pin: Only P12-P17 available
 */
static inline volatile rx_port_regs_t* port1(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_1);
}

/**
 * @brief Get pointer to PORT2 registers
 * @return Volatile pointer to PORT2 register structure
 * @note PORT2 PDR = 0x0008C002, PODR = 0x0008C022, PIDR = 0x0008C042
 * @note 100-pin: P20-P27 available
 */
static inline volatile rx_port_regs_t* port2(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_2);
}

/**
 * @brief Get pointer to PORT3 registers
 * @return Volatile pointer to PORT3 register structure
 * @note PORT3 PDR = 0x0008C003, PODR = 0x0008C023, PIDR = 0x0008C043
 * @note 100-pin: P30-P37 available (P35 is input-only)
 */
static inline volatile rx_port_regs_t* port3(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_3);
}

/**
 * @brief Get pointer to PORT4 registers
 * @return Volatile pointer to PORT4 register structure
 * @note PORT4 PDR = 0x0008C004, PODR = 0x0008C024, PIDR = 0x0008C044
 * @note 100-pin: P40-P47 available
 */
static inline volatile rx_port_regs_t* port4(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_4);
}

/**
 * @brief Get pointer to PORT5 registers
 * @return Volatile pointer to PORT5 register structure
 * @note PORT5 PDR = 0x0008C005, PODR = 0x0008C025, PIDR = 0x0008C045
 * @note 100-pin: Only P50-P55 available
 */
static inline volatile rx_port_regs_t* port5(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_5);
}

/**
 * @brief Get pointer to PORTA registers
 * @return Volatile pointer to PORTA register structure
 * @note PORTA PDR = 0x0008C00A, PODR = 0x0008C02A, PIDR = 0x0008C04A
 * @note 100-pin: PA0-PA7 available
 */
static inline volatile rx_port_regs_t* porta(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_a);
}

/**
 * @brief Get pointer to PORTB registers
 * @return Volatile pointer to PORTB register structure
 * @note PORTB PDR = 0x0008C00B, PODR = 0x0008C02B, PIDR = 0x0008C04B
 * @note 100-pin: PB0-PB7 available
 */
static inline volatile rx_port_regs_t* portb(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_b);
}

/**
 * @brief Get pointer to PORTC registers
 * @return Volatile pointer to PORTC register structure
 * @note PORTC PDR = 0x0008C00C, PODR = 0x0008C02C, PIDR = 0x0008C04C
 * @note 100-pin: PC0-PC7 available
 */
static inline volatile rx_port_regs_t* portc(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_c);
}

/**
 * @brief Get pointer to PORTD registers
 * @return Volatile pointer to PORTD register structure
 * @note PORTD PDR = 0x0008C00D, PODR = 0x0008C02D, PIDR = 0x0008C04D
 * @note 100-pin: PD0-PD7 available
 */
static inline volatile rx_port_regs_t* portd(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_d);
}

/**
 * @brief Get pointer to PORTE registers
 * @return Volatile pointer to PORTE register structure
 * @note PORTE PDR = 0x0008C00E, PODR = 0x0008C02E, PIDR = 0x0008C04E
 * @note 100-pin: PE0-PE7 available
 */
static inline volatile rx_port_regs_t* porte(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_e);
}

/**
 * @brief Get pointer to PORTJ registers
 * @return Volatile pointer to PORTJ register structure
 * @note PORTJ PDR = 0x0008C012, PODR = 0x0008C032, PIDR = 0x0008C052
 * @note 100-pin: Only PJ3, PJ5 available
 */
static inline volatile rx_port_regs_t* portj(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_j);
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_PORT_REGS_H */
