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

/** @brief MPC PFS port offset values
 *
 * PFS registers are contiguous in memory. Each port has 8 register slots,
 * even if not all pins are physically present.
 */
typedef enum {
  k_mpc_port0_offset = 0,   /**< Port 0 offset (P00-P07 = 8 pins) */
  k_mpc_port1_offset = 8,   /**< Port 1 offset (P10-P17 = 8 pins) */
  k_mpc_port2_offset = 16,  /**< Port 2 offset (P20-P27 = 8 pins) */
  k_mpc_port3_offset = 24,  /**< Port 3 offset (P30-P34 = 5 pins) */
  k_mpc_port4_offset = 32,  /**< Port 4 offset (P40-P47 = 8 pins) */
  k_mpc_port5_offset = 40,  /**< Port 5 offset (P50-P57 = 8 pins) */
  k_mpc_port6_offset = 48,  /**< Port 6 offset (P60-P67 = 8 pins) */
  k_mpc_port7_offset = 56,  /**< Port 7 offset (P70-P77 = 8 pins) */
  k_mpc_port8_offset = 64,  /**< Port 8 offset (P80-P87 = 8 pins) */
  k_mpc_port9_offset = 72,  /**< Port 9 offset (P90-P97 = 8 pins) */
  k_mpc_porta_offset = 80,  /**< Port A offset (PA0-PA7 = 8 pins) */
  k_mpc_portb_offset = 88,  /**< Port B offset (PB0-PB7 = 8 pins) */
  k_mpc_portc_offset = 96,  /**< Port C offset (PC0-PC7 = 8 pins) */
  k_mpc_portd_offset = 104, /**< Port D offset (PD0-PD7 = 8 pins) */
  k_mpc_porte_offset = 112, /**< Port E offset (PE0-PE7 = 8 pins) */
  k_mpc_portf_offset = 120, /**< Port F offset (not on 100-pin) */
  k_mpc_portg_offset = 128, /**< Port G offset (not on 100-pin) */
  k_mpc_porth_offset = 136, /**< Port H offset (not on 100-pin) */
  k_mpc_portj_offset = 144, /**< Port J offset (only PJ3 on 100-pin) */
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
      port_offset = k_mpc_port6_offset;
      break;
    case k_mpc_port_7:
      port_offset = k_mpc_port7_offset;
      break;
    case k_mpc_port_8:
      port_offset = k_mpc_port8_offset;
      break;
    case k_mpc_port_9:
      port_offset = k_mpc_port9_offset;
      break;
    case k_mpc_port_a:
      port_offset = k_mpc_porta_offset;
      break;
    case k_mpc_port_b:
      port_offset = k_mpc_portb_offset;
      break;
    case k_mpc_port_c:
      port_offset = k_mpc_portc_offset;
      break;
    case k_mpc_port_d:
      port_offset = k_mpc_portd_offset;
      break;
    case k_mpc_port_e:
      port_offset = k_mpc_porte_offset;
      break;
    case k_mpc_port_f:
    case k_mpc_port_g:
    case k_mpc_port_h:
      /* Ports F, G, H not available on 100-pin LFQFP package */
      rx_log_error(s_tag, "Port not available on this package");
      return NULL;
    case k_mpc_port_j:
      port_offset = k_mpc_portj_offset;
      break;
    default:
      rx_log_error(s_tag, "Invalid port");
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

rx_err_t rx_mpc_set_gpio(rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate the decoded port and pin are in valid range */
  if (port > k_mpc_port_j || pin_num > k_mpc_max_pin) {
    rx_log_error(s_tag, "Invalid port or pin number");
    return k_rx_err_invalid_arg;
  }

  /* GPIO mode: PSEL = 0, ISEL = 0, ASEL = 0 */
  return internal_write_pfs(port, pin_num, k_mpc_pfs_gpio);
}

rx_err_t rx_mpc_set_peripheral(const rx_mpc_peripheral_config_t* config)
{
  if (config == NULL) {
    rx_log_error(s_tag, "Invalid configuration pointer");
    return k_rx_err_invalid_arg;
  }

  if (config->psel > k_mpc_psel_max) {
    rx_log_error(s_tag, "Invalid PSEL value exceeds maximum");
    rx_log_error_val(s_tag, "  psel=", config->psel);
    rx_log_error_val(s_tag, "  k_mpc_psel_max=", (uint8_t)k_mpc_psel_max);
    return k_rx_err_invalid_arg;
  }

  /* Extract port and pin number from rx_port_pin_t */
  uint8_t port    = rx_port_from_pin(config->pin);
  uint8_t pin_num = rx_pin_from_pin(config->pin);

  /* Peripheral mode: PSEL = specified, ISEL = 0, ASEL = 0 */
  return internal_write_pfs(port, pin_num, config->psel);
}

rx_err_t rx_mpc_set_mtu_pwm(rx_port_pin_t pin)
{
  /* MTU PWM (MTIOC) pins typically use PSEL = 0x01
   * Common pins:
   * - P14-P17: MTU3 MTIOCA/B/C/D
   * - P24-P27: MTU4 MTIOCA/B/C/D
   */
  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = k_psel_mtu_ioc};
  return rx_mpc_set_peripheral(&config);
}

rx_err_t rx_mpc_set_mtu_encoder(rx_port_pin_t pin)
{
  /* MTU encoder (MTCLK) pins typically use PSEL = 0x02 or 0x03
   * Common pins:
   * - PC0/PC1: MTCLKA/B
   * - PD0/PD1: MTCLKC/D
   *
   * For phase counting mode, use PSEL = 0x03
   */
  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = k_psel_mtu_phase};
  return rx_mpc_set_peripheral(&config);
}

rx_err_t rx_mpc_set_adc(rx_port_pin_t pin)
{
  /* Extract port and pin number from type-safe enum */
  uint8_t port    = rx_port_from_pin(pin);
  uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate the decoded port and pin are in valid range */
  if (port > k_mpc_port_j || pin_num > k_mpc_max_pin) {
    rx_log_error(s_tag, "Invalid port or pin number");
    return k_rx_err_invalid_arg;
  }

  /* ADC mode: Set ASEL = 1 to disable digital I/O
   * Common ADC pins: P40-P47 (AN000-AN007)
   */
  return internal_write_pfs(port, pin_num, k_pfs_asel);
}

rx_err_t rx_mpc_set_sci(rx_port_pin_t pin, bool is_tx)
{
  /* SCI pins use PSEL = 0x0A
   * TX and RX use the same PSEL code
   */
  (void)is_tx; /* Not used - same PSEL for TX/RX */
  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = k_psel_sci_tx};
  return rx_mpc_set_peripheral(&config);
}

rx_err_t rx_mpc_set_riic(rx_port_pin_t pin)
{
  /* RIIC pins use PSEL = 0x0F
   * SCL and SDA use the same PSEL code
   */
  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = k_psel_riic_scl};
  return rx_mpc_set_peripheral(&config);
}

rx_err_t rx_mpc_set_rspi(rx_port_pin_t pin)
{
  /* RSPI pins use PSEL = 0x0D
   * All RSPI signals (CLK, COPI, CIPO, SSL) use same PSEL
   */
  const rx_mpc_peripheral_config_t config = {.pin = pin, .psel = k_psel_rspi_clk};
  return rx_mpc_set_peripheral(&config);
}
