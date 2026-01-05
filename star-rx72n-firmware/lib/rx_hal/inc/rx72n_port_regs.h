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
 * Package Configuration:
 * - Define RX72N_PACKAGE_* in rx_package_config.h to specify your hardware
 * - Only ports available on your package variant will compile
 * - Attempting to use unavailable ports generates helpful compile errors
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_PORT_REGS_H
#define STAR_RX72N_PORT_REGS_H

#include <stdint.h>
#include "rx_package_config.h"

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

/** @brief Port number offsets for register access */
typedef enum {
  k_port_offset_0 = 0x00, /**< Port 0 offset */
  k_port_offset_1 = 0x01, /**< Port 1 offset */
  k_port_offset_2 = 0x02, /**< Port 2 offset */
  k_port_offset_3 = 0x03, /**< Port 3 offset */
  k_port_offset_4 = 0x04, /**< Port 4 offset */
  k_port_offset_5 = 0x05, /**< Port 5 offset */
  k_port_offset_6 = 0x06, /**< Port 6 offset */
  k_port_offset_7 = 0x07, /**< Port 7 offset */
  k_port_offset_8 = 0x08, /**< Port 8 offset */
  k_port_offset_9 = 0x09, /**< Port 9 offset */
  k_port_offset_a = 0x0A, /**< Port A offset */
  k_port_offset_b = 0x0B, /**< Port B offset */
  k_port_offset_c = 0x0C, /**< Port C offset */
  k_port_offset_d = 0x0D, /**< Port D offset */
  k_port_offset_e = 0x0E, /**< Port E offset */
  k_port_offset_f = 0x0F, /**< Port F offset */
  k_port_offset_g = 0x10, /**< Port G offset */
  k_port_offset_h = 0x11, /**< Port H offset (224-pin only) */
  k_port_offset_j = 0x12, /**< Port J offset (not contiguous!) */
  k_port_offset_k = 0x13, /**< Port K offset (224-pin only) */
  k_port_offset_l = 0x14, /**< Port L offset (224-pin only) */
  k_port_offset_m = 0x15, /**< Port M offset (224-pin only) */
  k_port_offset_n = 0x16, /**< Port N offset (224-pin only) */
  k_port_offset_q = 0x17, /**< Port Q offset (224-pin only) */
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
typedef struct {
  volatile uint8_t  pdr;          /**< +0x00: Port Direction Register (0=input, 1=output) */
  volatile uint8_t  _pad1[0x1F];  /**< +0x01 to +0x1F: Reserved/padding */
  volatile uint8_t  podr;         /**< +0x20: Port Output Data Register (output level) */
  volatile uint8_t  _pad2[0x1F];  /**< +0x21 to +0x3F: Reserved/padding */
  volatile uint8_t  pidr;         /**< +0x40: Port Input Data Register (read pin state) */
  volatile uint8_t  _pad3[0x1F];  /**< +0x41 to +0x5F: Reserved/padding */
  volatile uint8_t  pmr;          /**< +0x60: Port Mode Register (0=GPIO, 1=peripheral) */
  volatile uint8_t  _pad4[0x1F];  /**< +0x61 to +0x7F: Reserved/padding */
  volatile uint16_t odr;          /**< +0x80: Open Drain Control (ODR0 at +0, ODR1 at +1) */
  volatile uint8_t  _pad5[0x3E];  /**< +0x82 to +0xBF: Reserved/padding */
  volatile uint8_t  pcr;          /**< +0xC0: Pull-up Control Register */
  volatile uint8_t  _pad6[0x1F];  /**< +0xC1 to +0xDF: Reserved/padding */
  volatile uint8_t  dscr;         /**< +0xE0: Drive Capacity Control Register */
} rx_port_regs_t;

/* Compile-time verification that struct size matches expected layout */
_Static_assert(sizeof(rx_port_regs_t) == 0xE1,
               "Port register struct size must be 0xE1 bytes");
_Static_assert(offsetof(rx_port_regs_t, pdr) == 0x00,
               "PDR must be at offset 0x00");
_Static_assert(offsetof(rx_port_regs_t, podr) == 0x20,
               "PODR must be at offset 0x20");
_Static_assert(offsetof(rx_port_regs_t, pidr) == 0x40,
               "PIDR must be at offset 0x40");
_Static_assert(offsetof(rx_port_regs_t, pmr) == 0x60,
               "PMR must be at offset 0x60");
_Static_assert(offsetof(rx_port_regs_t, odr) == 0x80,
               "ODR must be at offset 0x80");
_Static_assert(offsetof(rx_port_regs_t, pcr) == 0xC0,
               "PCR must be at offset 0xC0");
_Static_assert(offsetof(rx_port_regs_t, dscr) == 0xE0,
               "DSCR must be at offset 0xE0");

/* =============================================================================
 * Port Register Access Functions
 * =============================================================================
 */

/* Port 0 - Available on: 100-pin (limited), 144/145-pin (limited), 176-pin, 224-pin */
#ifdef RX72N_PORT0_AVAILABLE
/**
 * @brief Get pointer to PORT0 registers
 * @return Volatile pointer to PORT0 register structure
 * @note PORT0 PDR = 0x0008C000, PODR = 0x0008C020, PIDR = 0x0008C040
 * @note 100-pin: Only P05, P07 available
 * @note 144/145-pin: P00-P03, P05, P07 available
 * @note 176/224-pin: P00-P07 available
 */
static inline volatile rx_port_regs_t* port0(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_0);
}
#endif

/* Port 1 - Available on: 100-pin (limited), 144/145-pin (limited), 176-pin, 224-pin */
#ifdef RX72N_PORT1_AVAILABLE
/**
 * @brief Get pointer to PORT1 registers
 * @return Volatile pointer to PORT1 register structure
 * @note PORT1 PDR = 0x0008C001, PODR = 0x0008C021, PIDR = 0x0008C041
 * @note 100/144/145-pin: Only P12-P17 available
 * @note 176/224-pin: P10-P17 available
 */
static inline volatile rx_port_regs_t* port1(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_1);
}
#endif

/* Port 2 - Available on: ALL packages */
#ifdef RX72N_PORT2_AVAILABLE
/**
 * @brief Get pointer to PORT2 registers
 * @return Volatile pointer to PORT2 register structure
 * @note PORT2 PDR = 0x0008C002, PODR = 0x0008C022, PIDR = 0x0008C042
 * @note All packages: P20-P27 available
 */
static inline volatile rx_port_regs_t* port2(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_2);
}
#endif

/* Port 3 - Available on: ALL packages */
#ifdef RX72N_PORT3_AVAILABLE
/**
 * @brief Get pointer to PORT3 registers
 * @return Volatile pointer to PORT3 register structure
 * @note PORT3 PDR = 0x0008C003, PODR = 0x0008C023, PIDR = 0x0008C043
 * @note All packages: P30-P37 available (P35 is input-only)
 */
static inline volatile rx_port_regs_t* port3(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_3);
}
#endif

/* Port 4 - Available on: ALL packages */
#ifdef RX72N_PORT4_AVAILABLE
/**
 * @brief Get pointer to PORT4 registers
 * @return Volatile pointer to PORT4 register structure
 * @note PORT4 PDR = 0x0008C004, PODR = 0x0008C024, PIDR = 0x0008C044
 * @note All packages: P40-P47 available
 */
static inline volatile rx_port_regs_t* port4(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_4);
}
#endif

/* Port 5 - Available on: ALL packages (some limited) */
#ifdef RX72N_PORT5_AVAILABLE
/**
 * @brief Get pointer to PORT5 registers
 * @return Volatile pointer to PORT5 register structure
 * @note PORT5 PDR = 0x0008C005, PODR = 0x0008C025, PIDR = 0x0008C045
 * @note 100-pin: Only P50-P55 available
 * @note 144/145-pin: P50-P56 available
 * @note 176/224-pin: P50-P57 available
 */
static inline volatile rx_port_regs_t* port5(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_5);
}
#endif

/* Port 6 - Available on: 176-pin, 224-pin ONLY */
#ifdef RX72N_PORT6_AVAILABLE
/**
 * @brief Get pointer to PORT6 registers
 * @return Volatile pointer to PORT6 register structure
 * @note PORT6 PDR = 0x0008C006, PODR = 0x0008C026, PIDR = 0x0008C046
 * @note 176/224-pin: P60-P67 available
 */
static inline volatile rx_port_regs_t* port6(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_6);
}
#endif

/* Port 7 - Available on: 176-pin, 224-pin ONLY */
#ifdef RX72N_PORT7_AVAILABLE
/**
 * @brief Get pointer to PORT7 registers
 * @return Volatile pointer to PORT7 register structure
 * @note PORT7 PDR = 0x0008C007, PODR = 0x0008C027, PIDR = 0x0008C047
 * @note 176/224-pin: P70-P77 available
 */
static inline volatile rx_port_regs_t* port7(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_7);
}
#endif

/* Port 8 - Available on: 144/145-pin (limited), 176-pin, 224-pin */
#ifdef RX72N_PORT8_AVAILABLE
/**
 * @brief Get pointer to PORT8 registers
 * @return Volatile pointer to PORT8 register structure
 * @note PORT8 PDR = 0x0008C008, PODR = 0x0008C028, PIDR = 0x0008C048
 * @note 144/145-pin: Only P80-P83, P86-P87 available
 * @note 176/224-pin: P80-P87 available
 */
static inline volatile rx_port_regs_t* port8(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_8);
}
#endif

/* Port 9 - Available on: 144/145-pin (limited), 176-pin, 224-pin */
#ifdef RX72N_PORT9_AVAILABLE
/**
 * @brief Get pointer to PORT9 registers
 * @return Volatile pointer to PORT9 register structure
 * @note PORT9 PDR = 0x0008C009, PODR = 0x0008C029, PIDR = 0x0008C049
 * @note 144/145-pin: Only P90-P93 available
 * @note 176/224-pin: P90-P97 available
 */
static inline volatile rx_port_regs_t* port9(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_9);
}
#endif

/* Port A - Available on: ALL packages */
#ifdef RX72N_PORTA_AVAILABLE
/**
 * @brief Get pointer to PORTA registers
 * @return Volatile pointer to PORTA register structure
 * @note PORTA PDR = 0x0008C00A, PODR = 0x0008C02A, PIDR = 0x0008C04A
 * @note All packages: PA0-PA7 available
 */
static inline volatile rx_port_regs_t* porta(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_a);
}
#endif

/* Port B - Available on: ALL packages */
#ifdef RX72N_PORTB_AVAILABLE
/**
 * @brief Get pointer to PORTB registers
 * @return Volatile pointer to PORTB register structure
 * @note PORTB PDR = 0x0008C00B, PODR = 0x0008C02B, PIDR = 0x0008C04B
 * @note All packages: PB0-PB7 available
 */
static inline volatile rx_port_regs_t* portb(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_b);
}
#endif

/* Port C - Available on: ALL packages */
#ifdef RX72N_PORTC_AVAILABLE
/**
 * @brief Get pointer to PORTC registers
 * @return Volatile pointer to PORTC register structure
 * @note PORTC PDR = 0x0008C00C, PODR = 0x0008C02C, PIDR = 0x0008C04C
 * @note All packages: PC0-PC7 available
 */
static inline volatile rx_port_regs_t* portc(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_c);
}
#endif

/* Port D - Available on: ALL packages */
#ifdef RX72N_PORTD_AVAILABLE
/**
 * @brief Get pointer to PORTD registers
 * @return Volatile pointer to PORTD register structure
 * @note PORTD PDR = 0x0008C00D, PODR = 0x0008C02D, PIDR = 0x0008C04D
 * @note All packages: PD0-PD7 available
 */
static inline volatile rx_port_regs_t* portd(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_d);
}
#endif

/* Port E - Available on: ALL packages */
#ifdef RX72N_PORTE_AVAILABLE
/**
 * @brief Get pointer to PORTE registers
 * @return Volatile pointer to PORTE register structure
 * @note PORTE PDR = 0x0008C00E, PODR = 0x0008C02E, PIDR = 0x0008C04E
 * @note All packages: PE0-PE7 available
 */
static inline volatile rx_port_regs_t* porte(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_e);
}
#endif

/* Port F - Available on: 144/145-pin (limited), 176-pin, 224-pin */
#ifdef RX72N_PORTF_AVAILABLE
/**
 * @brief Get pointer to PORTF registers
 * @return Volatile pointer to PORTF register structure
 * @note PORTF PDR = 0x0008C00F, PODR = 0x0008C02F, PIDR = 0x0008C04F
 * @note 144/145-pin: Only PF5 available
 * @note 176/224-pin: PF0-PF7 available
 */
static inline volatile rx_port_regs_t* portf(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_f);
}
#endif

/* Port G - Available on: 176-pin, 224-pin */
#ifdef RX72N_PORTG_AVAILABLE
/**
 * @brief Get pointer to PORTG registers
 * @return Volatile pointer to PORTG register structure
 * @note PORTG PDR = 0x0008C010, PODR = 0x0008C030, PIDR = 0x0008C050
 * @note 176/224-pin: PG0-PG7 available
 */
static inline volatile rx_port_regs_t* portg(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_g);
}
#endif

/* Port J - Available on: ALL packages (some limited) */
#ifdef RX72N_PORTJ_AVAILABLE
/**
 * @brief Get pointer to PORTJ registers
 * @return Volatile pointer to PORTJ register structure
 * @note PORTJ PDR = 0x0008C012, PODR = 0x0008C032, PIDR = 0x0008C052
 * @note 100/144/145-pin: Only PJ3, PJ5 available
 * @note 176/224-pin: PJ0-PJ7 available
 */
static inline volatile rx_port_regs_t* portj(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_j);
}
#endif

/* Port H - Available on: 224-pin ONLY */
#ifdef RX72N_PORTH_AVAILABLE
/**
 * @brief Get pointer to PORTH registers
 * @return Volatile pointer to PORTH register structure
 * @note PORTH PDR = 0x0008C011, PODR = 0x0008C031, PIDR = 0x0008C051
 * @note 224-pin: PH0-PH7 available
 */
static inline volatile rx_port_regs_t* porth(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_h);
}
#endif

/* Port K - Available on: 224-pin ONLY */
#ifdef RX72N_PORTK_AVAILABLE
/**
 * @brief Get pointer to PORTK registers
 * @return Volatile pointer to PORTK register structure
 * @note PORTK PDR = 0x0008C013, PODR = 0x0008C033, PIDR = 0x0008C053
 * @note 224-pin: PK0-PK7 available
 */
static inline volatile rx_port_regs_t* portk(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_k);
}
#endif

/* Port L - Available on: 224-pin ONLY */
#ifdef RX72N_PORTL_AVAILABLE
/**
 * @brief Get pointer to PORTL registers
 * @return Volatile pointer to PORTL register structure
 * @note PORTL PDR = 0x0008C014, PODR = 0x0008C034, PIDR = 0x0008C054
 * @note 224-pin: PL0-PL7 available
 */
static inline volatile rx_port_regs_t* portl(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_l);
}
#endif

/* Port M - Available on: 224-pin ONLY */
#ifdef RX72N_PORTM_AVAILABLE
/**
 * @brief Get pointer to PORTM registers
 * @return Volatile pointer to PORTM register structure
 * @note PORTM PDR = 0x0008C015, PODR = 0x0008C035, PIDR = 0x0008C055
 * @note 224-pin: PM0-PM7 available
 */
static inline volatile rx_port_regs_t* portm(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_m);
}
#endif

/* Port N - Available on: 224-pin ONLY */
#ifdef RX72N_PORTN_AVAILABLE
/**
 * @brief Get pointer to PORTN registers
 * @return Volatile pointer to PORTN register structure
 * @note PORTN PDR = 0x0008C016, PODR = 0x0008C036, PIDR = 0x0008C056
 * @note 224-pin: PN0-PN7 available
 */
static inline volatile rx_port_regs_t* portn(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_n);
}
#endif

/* Port Q - Available on: 224-pin ONLY */
#ifdef RX72N_PORTQ_AVAILABLE
/**
 * @brief Get pointer to PORTQ registers
 * @return Volatile pointer to PORTQ register structure
 * @note PORTQ PDR = 0x0008C017, PODR = 0x0008C037, PIDR = 0x0008C057
 * @note 224-pin: PQ0-PQ7 available
 */
static inline volatile rx_port_regs_t* portq(void)
{
  return (volatile rx_port_regs_t*)(k_port_pdr_base + k_port_offset_q);
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_PORT_REGS_H */
