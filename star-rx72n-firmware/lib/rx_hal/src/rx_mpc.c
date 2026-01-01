/* src/drivers/rx_mpc.c */

/**
 * @file rx_mpc.c
 * @brief Multi-Function Pin Controller (MPC) Driver Implementation
 * @details
 * Configures RX72N pin functions for peripherals.
 *
 * The MPC controls pin multiplexing through PFS (Pin Function Select) registers.
 * Each pin can be configured for GPIO or peripheral functions.
 *
 * Write protection sequence:
 * 1. Set PWPR.B0WI = 0 (allow PFSWE write)
 * 2. Set PWPR.PFSWE = 1 (allow PFS write)
 * 3. Write to PFS registers
 * 4. Set PWPR.PFSWE = 0 (protect PFS)
 * 5. Set PWPR.B0WI = 1 (protect PFSWE)
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx_mpc.h"

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "MPC";

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get pointer to PFS register for a pin
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 *
 * @return Pointer to PFS register, or NULL if invalid
 */
static volatile uint8_t* internal_get_pfs_register(uint8_t port, uint8_t pin)
{
  /* Validate pin number */
  if (pin > 7) {
    return NULL;
  }

  /* Calculate PFS register offset
   * PFS registers start at MPC base + 0x21 (offset for P00PFS)
   * Each port has 8 pins, layout: P00-P07, P10-P17, P20-P27, etc.
   */
  volatile uint8_t* pfs_base = (volatile uint8_t*)MPC_BASE + 0x21;

  /* Port offset calculation */
  uint16_t port_offset = 0;

  switch (port) {
    case 0:
      port_offset = 0;
      break;
    case 1:
      port_offset = 8;
      break;
    case 2:
      port_offset = 16;
      break;
    case 3:
      port_offset = 24;
      break;
    case 4:
      port_offset = 29; /* P30-P34 = 5 pins, so P40 starts at offset 29 */
      break;
    case 5:
      port_offset = 37; /* P40-P47 = 8 pins */
      break;
    case 6:
    case 7:
    case 8:
    case 9:
    case 0x0A: /* Port A */
    case 0x0B: /* Port B */
    case 0x0C: /* Port C */
    case 0x0D: /* Port D */
    case 0x0E: /* Port E */
    case 0x0F: /* Port F */
    case 0x10: /* Port G */
    case 0x11: /* Port H */
    case 0x12: /* Port J */
      /* For simplicity, only implement ports 0-5 which cover motor control pins */
      rx_log_error(s_tag, "Error occurred");
      return NULL;
    default:
      rx_log_error(s_tag, "Error occurred");
      return NULL;
  }

  return pfs_base + port_offset + pin;
}

/**
 * @brief Unlock MPC write protection
 */
static void internal_mpc_unlock(void)
{
  /* Unlock sequence:
   * 1. Clear B0WI to allow PFSWE write
   * 2. Set PFSWE to allow PFS write
   */
  MPC.PWPR = 0x00;             /* Clear B0WI */
  MPC.PWPR = k_mpc_pwpr_pfswe; /* Set PFSWE */
}

/**
 * @brief Lock MPC write protection
 */
static void internal_mpc_lock(void)
{
  /* Lock sequence:
   * 1. Clear PFSWE to protect PFS
   * 2. Set B0WI to protect PFSWE
   */
  MPC.PWPR = 0x00;            /* Clear PFSWE */
  MPC.PWPR = k_mpc_pwpr_b0wi; /* Set B0WI */
}

/**
 * @brief Write to PFS register with protection handling
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 * @param[in] value Value to write to PFS register
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_write_pfs(uint8_t port, uint8_t pin, uint8_t value)
{
  volatile uint8_t* pfs_reg = internal_get_pfs_register(port, pin);
  if (pfs_reg == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Unlock, write, lock */
  internal_mpc_unlock();
  *pfs_reg = value;
  internal_mpc_lock();

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_mpc_set_gpio(uint8_t port, uint8_t pin)
{
  /* GPIO mode: PSEL = 0, ISEL = 0, ASEL = 0 */
  return internal_write_pfs(port, pin, 0x00);
}

rx_err_t rx_mpc_set_peripheral(uint8_t port, uint8_t pin, uint8_t psel)
{
  if (psel > 0x1F) {
    rx_log_error(s_tag, "Error occurred");
    return k_rx_err_invalid_arg;
  }

  /* Peripheral mode: PSEL = specified, ISEL = 0, ASEL = 0 */
  return internal_write_pfs(port, pin, psel);
}

rx_err_t rx_mpc_set_mtu_pwm(uint8_t port, uint8_t pin)
{
  /* MTU PWM (MTIOC) pins typically use PSEL = 0x01
   * Common pins:
   * - P14-P17: MTU3 MTIOCA/B/C/D
   * - P24-P27: MTU4 MTIOCA/B/C/D
   */
  return rx_mpc_set_peripheral(port, pin, k_psel_mtu_ioc);
}

rx_err_t rx_mpc_set_mtu_encoder(uint8_t port, uint8_t pin)
{
  /* MTU encoder (MTCLK) pins typically use PSEL = 0x02 or 0x03
   * Common pins:
   * - PC0/PC1: MTCLKA/B
   * - PD0/PD1: MTCLKC/D
   *
   * For phase counting mode, use PSEL = 0x03
   */
  return rx_mpc_set_peripheral(port, pin, k_psel_mtu_phase);
}

rx_err_t rx_mpc_set_adc(uint8_t port, uint8_t pin)
{
  /* ADC mode: Set ASEL = 1 to disable digital I/O
   * Common ADC pins: P40-P47 (AN000-AN007)
   */
  return internal_write_pfs(port, pin, k_pfs_asel);
}

rx_err_t rx_mpc_set_sci(uint8_t port, uint8_t pin, bool is_tx)
{
  /* SCI pins use PSEL = 0x0A
   * TX and RX use the same PSEL code
   */
  (void)is_tx; /* Not used - same PSEL for TX/RX */
  return rx_mpc_set_peripheral(port, pin, k_psel_sci_tx);
}

rx_err_t rx_mpc_set_riic(uint8_t port, uint8_t pin, bool is_scl)
{
  /* RIIC pins use PSEL = 0x0F
   * SCL and SDA use the same PSEL code
   */
  (void)is_scl; /* Not used - same PSEL for SCL/SDA */
  return rx_mpc_set_peripheral(port, pin, k_psel_riic_scl);
}

rx_err_t rx_mpc_set_rspi(uint8_t port, uint8_t pin)
{
  /* RSPI pins use PSEL = 0x0D
   * All RSPI signals (CLK, COPI, CIPO, SSL) use same PSEL
   */
  return rx_mpc_set_peripheral(port, pin, k_psel_rspi_clk);
}
