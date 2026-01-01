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

/* Port Data Register */
typedef struct {
  volatile uint8_t PDR;  /* Port Direction Register */
  volatile uint8_t PODR; /* Port Output Data Register */
  volatile uint8_t PIDR; /* Port Input Data Register */
  volatile uint8_t PMR;  /* Port Mode Register */
  volatile uint8_t ODR0; /* Open Drain Control Register 0 */
  volatile uint8_t ODR1; /* Open Drain Control Register 1 */
  volatile uint8_t PCR;  /* Pull-up Control Register */
  volatile uint8_t DSCR; /* Drive Capacity Control Register */
} rx_port_regs_t;

/* Port base addresses */
#define PORT0_BASE ((rx_port_regs_t*)0x0008C000)
#define PORT1_BASE ((rx_port_regs_t*)0x0008C008)
#define PORT2_BASE ((rx_port_regs_t*)0x0008C010)
#define PORT3_BASE ((rx_port_regs_t*)0x0008C018)
#define PORT4_BASE ((rx_port_regs_t*)0x0008C020)
#define PORT5_BASE ((rx_port_regs_t*)0x0008C028)
#define PORT6_BASE ((rx_port_regs_t*)0x0008C030)
#define PORT7_BASE ((rx_port_regs_t*)0x0008C038)
#define PORT8_BASE ((rx_port_regs_t*)0x0008C040)
#define PORT9_BASE ((rx_port_regs_t*)0x0008C048)
#define PORTA_BASE ((rx_port_regs_t*)0x0008C050)
#define PORTB_BASE ((rx_port_regs_t*)0x0008C058)
#define PORTC_BASE ((rx_port_regs_t*)0x0008C060)
#define PORTD_BASE ((rx_port_regs_t*)0x0008C068)
#define PORTE_BASE ((rx_port_regs_t*)0x0008C070)
#define PORTF_BASE ((rx_port_regs_t*)0x0008C078)
#define PORTG_BASE ((rx_port_regs_t*)0x0008C080)
#define PORTJ_BASE ((rx_port_regs_t*)0x0008C098)

#define PORT0 (*PORT0_BASE)
#define PORT1 (*PORT1_BASE)
#define PORT2 (*PORT2_BASE)
#define PORT3 (*PORT3_BASE)
#define PORT4 (*PORT4_BASE)
#define PORT5 (*PORT5_BASE)
#define PORT6 (*PORT6_BASE)
#define PORT7 (*PORT7_BASE)
#define PORT8 (*PORT8_BASE)
#define PORT9 (*PORT9_BASE)
#define PORTA (*PORTA_BASE)
#define PORTB (*PORTB_BASE)
#define PORTC (*PORTC_BASE)
#define PORTD (*PORTD_BASE)
#define PORTE (*PORTE_BASE)
#define PORTF (*PORTF_BASE)
#define PORTG (*PORTG_BASE)
#define PORTJ (*PORTJ_BASE)

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_PORT_REGS_H */
