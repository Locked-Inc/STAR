/* lib/rx_hal/src/rx_mpc.c */

/**
 * @file rx_mpc.c
 * @brief Multi-Function Pin Controller (MPC) Driver Implementation
 *
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
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_mpc.h"

#include <stddef.h>

#include "rx72n_regs.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "MPC";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief MPC pin validation constants */
typedef enum {
  k_mpc_max_pin = 7, /**< Maximum valid pin number (pins 0-7) */
} mpc_pin_constants_t;

/** @brief MPC PFS register address offset */
typedef enum {
  k_mpc_pfs_base_offset = 0x21, /**< Offset from MPC_BASE to P00PFS register */
} mpc_pfs_offset_t;

/** @brief MPC port number constants */
typedef enum {
  k_mpc_port_0 = 0,    /**< Port 0 */
  k_mpc_port_1 = 1,    /**< Port 1 */
  k_mpc_port_2 = 2,    /**< Port 2 */
  k_mpc_port_3 = 3,    /**< Port 3 */
  k_mpc_port_4 = 4,    /**< Port 4 */
  k_mpc_port_5 = 5,    /**< Port 5 */
  k_mpc_port_6 = 6,    /**< Port 6 */
  k_mpc_port_7 = 7,    /**< Port 7 */
  k_mpc_port_8 = 8,    /**< Port 8 */
  k_mpc_port_9 = 9,    /**< Port 9 */
  k_mpc_port_a = 0x0A, /**< Port A */
  k_mpc_port_b = 0x0B, /**< Port B */
  k_mpc_port_c = 0x0C, /**< Port C */
  k_mpc_port_d = 0x0D, /**< Port D */
  k_mpc_port_e = 0x0E, /**< Port E */
  k_mpc_port_f = 0x0F, /**< Port F */
  k_mpc_port_g = 0x10, /**< Port G */
  k_mpc_port_h = 0x11, /**< Port H */
  k_mpc_port_j = 0x12, /**< Port J */
} mpc_port_number_t;

/** @brief MPC PFS port offset values */
typedef enum {
  k_mpc_port0_offset = 0,  /**< Port 0 offset (P00-P07 = 8 pins) */
  k_mpc_port1_offset = 8,  /**< Port 1 offset (P10-P17 = 8 pins) */
  k_mpc_port2_offset = 16, /**< Port 2 offset (P20-P27 = 8 pins) */
  k_mpc_port3_offset = 24, /**< Port 3 offset (P30-P34 = 5 pins) */
  k_mpc_port4_offset = 29, /**< Port 4 offset (P40-P47 = 8 pins) */
  k_mpc_port5_offset = 37, /**< Port 5 offset */
} mpc_port_offset_t;

/** @brief MPC PSEL maximum value */
typedef enum {
  k_mpc_psel_max = 0x1F, /**< Maximum valid PSEL value (5 bits) */
} mpc_psel_constants_t;

/** @brief MPC PFS register GPIO mode value */
typedef enum {
  k_mpc_pfs_gpio = 0x00, /**< GPIO mode: PSEL = 0, ISEL = 0, ASEL = 0 */
} mpc_pfs_gpio_t;

/** @brief MPC PWPR register clear value */
typedef enum {
  k_mpc_pwpr_clear = 0x00, /**< Clear PWPR register */
} mpc_pwpr_t;

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
  if (pin > k_mpc_max_pin) {
    return NULL;
  }

  /* Calculate PFS register offset
   * PFS registers start at MPC base + 0x21 (offset for P00PFS)
   * Each port has 8 pins, layout: P00-P07, P10-P17, P20-P27, etc.
   */
  volatile uint8_t* pfs_base = (volatile uint8_t*)mpc() + k_mpc_pfs_base_offset;

  /* Port offset calculation */
  uint16_t port_offset = 0;

  switch (port) {
    case k_mpc_port_0:
      port_offset = k_mpc_port0_offset;
      break;
    case k_mpc_port_1:
      port_offset = k_mpc_port1_offset;
      break;
    case k_mpc_port_2:
      port_offset = k_mpc_port2_offset;
      break;
    case k_mpc_port_3:
      port_offset = k_mpc_port3_offset;
      break;
    case k_mpc_port_4:
      port_offset = k_mpc_port4_offset;
      break;
    case k_mpc_port_5:
      port_offset = k_mpc_port5_offset;
      break;
    case k_mpc_port_6:
    case k_mpc_port_7:
    case k_mpc_port_8:
    case k_mpc_port_9:
    case k_mpc_port_a:
    case k_mpc_port_b:
    case k_mpc_port_c:
    case k_mpc_port_d:
    case k_mpc_port_e:
    case k_mpc_port_f:
    case k_mpc_port_g:
    case k_mpc_port_h:
    case k_mpc_port_j:
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
  mpc()->pwpr = k_mpc_pwpr_clear; /* Clear B0WI */
  mpc()->pwpr = k_mpc_pwpr_pfswe; /* Set PFSWE */
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
  mpc()->pwpr = k_mpc_pwpr_clear; /* Clear PFSWE */
  mpc()->pwpr = k_mpc_pwpr_b0wi;  /* Set B0WI */
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
  return internal_write_pfs(port, pin, k_mpc_pfs_gpio);
}

rx_err_t rx_mpc_set_peripheral(uint8_t port, uint8_t pin, uint8_t psel)
{
  if (psel > k_mpc_psel_max) {
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
