/* lib/rx_hal/inc/rx72n_port_regs.h */

/**
 * @file rx72n_port_regs.h
 * @brief RX72N PORT (GPIO) Register Definitions
 *
 * General Purpose I/O port registers for digital pin control.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_PORT_REGS_H
#define STAR_RX72N_PORT_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Port Registers (GPIO)
 * =============================================================================
 */

/** @brief Port base addresses */
typedef enum {
  k_port0_base_addr = 0x0008C000, /**< PORT0 register base address */
  k_port1_base_addr = 0x0008C008, /**< PORT1 register base address */
  k_port2_base_addr = 0x0008C010, /**< PORT2 register base address */
  k_port3_base_addr = 0x0008C018, /**< PORT3 register base address */
  k_port4_base_addr = 0x0008C020, /**< PORT4 register base address */
  k_port5_base_addr = 0x0008C028, /**< PORT5 register base address */
  k_port6_base_addr = 0x0008C030, /**< PORT6 register base address */
  k_port7_base_addr = 0x0008C038, /**< PORT7 register base address */
  k_port8_base_addr = 0x0008C040, /**< PORT8 register base address */
  k_port9_base_addr = 0x0008C048, /**< PORT9 register base address */
  k_porta_base_addr = 0x0008C050, /**< PORTA register base address */
  k_portb_base_addr = 0x0008C058, /**< PORTB register base address */
  k_portc_base_addr = 0x0008C060, /**< PORTC register base address */
  k_portd_base_addr = 0x0008C068, /**< PORTD register base address */
  k_porte_base_addr = 0x0008C070, /**< PORTE register base address */
  k_portf_base_addr = 0x0008C078, /**< PORTF register base address */
  k_portg_base_addr = 0x0008C080, /**< PORTG register base address */
  k_portj_base_addr = 0x0008C098, /**< PORTJ register base address */
} rx_port_addresses_t;

/**
 * @brief Port Register Map
 * @details
 * General Purpose I/O port registers for digital pin control.
 * Each port has 8 pins (bits 0-7) for direction, output, input, and mode control.
 * Base addresses:
 * - PORT0: 0x0008C000
 * - PORT1: 0x0008C008
 * - PORT2: 0x0008C010
 * - ... (offset 0x08 per port)
 */
typedef struct {
  volatile uint8_t pdr;  /**< Port Direction Register (0=input, 1=output) */
  volatile uint8_t podr; /**< Port Output Data Register (output level) */
  volatile uint8_t pidr; /**< Port Input Data Register (read pin state) */
  volatile uint8_t pmr;  /**< Port Mode Register (0=GPIO, 1=peripheral) */
  volatile uint8_t odr0; /**< Open Drain Control Register 0 (pins 0-3) */
  volatile uint8_t odr1; /**< Open Drain Control Register 1 (pins 4-7) */
  volatile uint8_t pcr;  /**< Pull-up Control Register (1=enable pull-up) */
  volatile uint8_t dscr; /**< Drive Capacity Control Register (drive strength) */
} rx_port_regs_t;

/**
 * @brief Get pointer to PORT0 registers
 * @return Volatile pointer to PORT0 register structure
 */
static inline volatile rx_port_regs_t* port0(void)
{
  return (volatile rx_port_regs_t*)k_port0_base_addr;
}

/**
 * @brief Get pointer to PORT1 registers
 * @return Volatile pointer to PORT1 register structure
 */
static inline volatile rx_port_regs_t* port1(void)
{
  return (volatile rx_port_regs_t*)k_port1_base_addr;
}

/**
 * @brief Get pointer to PORT2 registers
 * @return Volatile pointer to PORT2 register structure
 */
static inline volatile rx_port_regs_t* port2(void)
{
  return (volatile rx_port_regs_t*)k_port2_base_addr;
}

/**
 * @brief Get pointer to PORT3 registers
 * @return Volatile pointer to PORT3 register structure
 */
static inline volatile rx_port_regs_t* port3(void)
{
  return (volatile rx_port_regs_t*)k_port3_base_addr;
}

/**
 * @brief Get pointer to PORT4 registers
 * @return Volatile pointer to PORT4 register structure
 */
static inline volatile rx_port_regs_t* port4(void)
{
  return (volatile rx_port_regs_t*)k_port4_base_addr;
}

/**
 * @brief Get pointer to PORT5 registers
 * @return Volatile pointer to PORT5 register structure
 */
static inline volatile rx_port_regs_t* port5(void)
{
  return (volatile rx_port_regs_t*)k_port5_base_addr;
}

/**
 * @brief Get pointer to PORT6 registers
 * @return Volatile pointer to PORT6 register structure
 */
static inline volatile rx_port_regs_t* port6(void)
{
  return (volatile rx_port_regs_t*)k_port6_base_addr;
}

/**
 * @brief Get pointer to PORT7 registers
 * @return Volatile pointer to PORT7 register structure
 */
static inline volatile rx_port_regs_t* port7(void)
{
  return (volatile rx_port_regs_t*)k_port7_base_addr;
}

/**
 * @brief Get pointer to PORT8 registers
 * @return Volatile pointer to PORT8 register structure
 */
static inline volatile rx_port_regs_t* port8(void)
{
  return (volatile rx_port_regs_t*)k_port8_base_addr;
}

/**
 * @brief Get pointer to PORT9 registers
 * @return Volatile pointer to PORT9 register structure
 */
static inline volatile rx_port_regs_t* port9(void)
{
  return (volatile rx_port_regs_t*)k_port9_base_addr;
}

/**
 * @brief Get pointer to PORTA registers
 * @return Volatile pointer to PORTA register structure
 */
static inline volatile rx_port_regs_t* porta(void)
{
  return (volatile rx_port_regs_t*)k_porta_base_addr;
}

/**
 * @brief Get pointer to PORTB registers
 * @return Volatile pointer to PORTB register structure
 */
static inline volatile rx_port_regs_t* portb(void)
{
  return (volatile rx_port_regs_t*)k_portb_base_addr;
}

/**
 * @brief Get pointer to PORTC registers
 * @return Volatile pointer to PORTC register structure
 */
static inline volatile rx_port_regs_t* portc(void)
{
  return (volatile rx_port_regs_t*)k_portc_base_addr;
}

/**
 * @brief Get pointer to PORTD registers
 * @return Volatile pointer to PORTD register structure
 */
static inline volatile rx_port_regs_t* portd(void)
{
  return (volatile rx_port_regs_t*)k_portd_base_addr;
}

/**
 * @brief Get pointer to PORTE registers
 * @return Volatile pointer to PORTE register structure
 */
static inline volatile rx_port_regs_t* porte(void)
{
  return (volatile rx_port_regs_t*)k_porte_base_addr;
}

/**
 * @brief Get pointer to PORTF registers
 * @return Volatile pointer to PORTF register structure
 */
static inline volatile rx_port_regs_t* portf(void)
{
  return (volatile rx_port_regs_t*)k_portf_base_addr;
}

/**
 * @brief Get pointer to PORTG registers
 * @return Volatile pointer to PORTG register structure
 */
static inline volatile rx_port_regs_t* portg(void)
{
  return (volatile rx_port_regs_t*)k_portg_base_addr;
}

/**
 * @brief Get pointer to PORTJ registers
 * @return Volatile pointer to PORTJ register structure
 */
static inline volatile rx_port_regs_t* portj(void)
{
  return (volatile rx_port_regs_t*)k_portj_base_addr;
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_PORT_REGS_H */
