/* lib/rx_hal/inc/rx_mpc.h */

/**
 * @file rx_mpc.h
 * @brief Multi-Function Pin Controller (MPC) Driver for RX72N
 * @details
 * Configures pin functions for RX72N peripherals.
 *
 * The MPC allows each pin to be configured for:
 * - GPIO (default)
 * - Peripheral functions (MTU, SCI, RIIC, RSPI, ADC, etc.)
 *
 * Each pin has a PFS (Pin Function Select) register that determines
 * its function. Write protection must be disabled before modifying PFS.
 *
 * Common pin functions for motor control:
 * - MTU PWM outputs (MTIOC3A/B/C/D, MTIOC4A/B/C/D)
 * - MTU encoder inputs (MTCLKA/B/C/D)
 * - GPIO for motor driver control
 * - ADC for current sensing
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_MPC_H
#define STAR_RX72N_MPC_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware_pinout.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Pin Function Codes
 * =============================================================================
 */

/**
 * @brief Pin function select codes
 *
 * These codes are written to PFS registers to select pin functions.
 * Values are device-specific - consult RX72N hardware manual Table 21.5.
 */
typedef enum {
  k_pin_function_gpio   = 0x00, /**< GPIO mode (default) */
  k_pin_function_periph = 0x01, /**< Peripheral function (auto-detect from PSEL) */
} rx_pin_function_t;

/**
 * @brief Peripheral select codes for common motor control functions
 *
 * These are written to the PSEL field of PFS registers.
 * Actual values depend on specific pin - see hardware manual.
 */
typedef enum {
  k_psel_mtu_ioc   = 0x01, /**< MTU I/O compare/PWM output (MTIOC) */
  k_psel_mtu_clk   = 0x02, /**< MTU clock input (MTCLK) */
  k_psel_mtu_phase = 0x03, /**< MTU encoder phase input */
  k_psel_sci_tx    = 0x0A, /**< SCI transmit */
  k_psel_sci_rx    = 0x0A, /**< SCI receive */
  k_psel_riic_scl  = 0x0F, /**< RIIC clock */
  k_psel_riic_sda  = 0x0F, /**< RIIC data */
  k_psel_rspi_clk  = 0x0D, /**< RSPI clock */
  k_psel_rspi_copi = 0x0D, /**< RSPI controller out peripheral in */
  k_psel_rspi_cipo = 0x0D, /**< RSPI controller in peripheral out */
  k_psel_adc       = 0x00, /**< ADC input (disable digital) */
} rx_pin_psel_t;

/**
 * @brief Port numbers for RX72N
 */
typedef enum {
  k_port_0 = 0,
  k_port_1 = 1,
  k_port_2 = 2,
  k_port_3 = 3,
  k_port_4 = 4,
  k_port_5 = 5,
  k_port_6 = 6,
  k_port_7 = 7,
  k_port_8 = 8,
  k_port_9 = 9,
  k_port_a = 0x0A,
  k_port_b = 0x0B,
  k_port_c = 0x0C,
  k_port_d = 0x0D,
  k_port_e = 0x0E,
  k_port_f = 0x0F,
  k_port_g = 0x10,
  k_port_h = 0x11,
  k_port_j = 0x12,
} rx_port_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Configure pin for GPIO function
 *
 * Sets pin to GPIO mode (PFS.PSEL = 0, no peripheral function).
 *
 * @param[in] pin GPIO pin enum (encodes port and pin number)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port or pin is invalid
 */
rx_err_t rx_mpc_set_gpio(gpio_pin_t pin);

/**
 * @brief Configure pin for peripheral function
 *
 * Sets pin to peripheral mode with specified function code.
 *
 * @param[in] pin GPIO pin enum (encodes port and pin number)
 * @param[in] psel Peripheral select code (see hardware manual)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port or pin is invalid
 */
rx_err_t rx_mpc_set_peripheral(gpio_pin_t pin, uint8_t psel);

/**
 * @brief Configure pin for MTU PWM output
 *
 * Convenience function for configuring MTU I/O compare/PWM pins.
 * Common pins: P14/P15/P16/P17 (MTU3), P24/P25/P26/P27 (MTU4)
 *
 * @param[in] pin GPIO pin enum (encodes port and pin number)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port or pin is invalid
 */
rx_err_t rx_mpc_set_mtu_pwm(gpio_pin_t pin);

/**
 * @brief Configure pin for MTU encoder input
 *
 * Convenience function for configuring MTU clock/phase counter inputs.
 * Common pins: PC0/PC1 (MTCLKA/B), PD0/PD1 (MTCLKC/D)
 *
 * @param[in] pin GPIO pin enum (encodes port and pin number)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port or pin is invalid
 */
rx_err_t rx_mpc_set_mtu_encoder(gpio_pin_t pin);

/**
 * @brief Configure pin for ADC input
 *
 * Disables digital input/output for analog operation.
 * Common pins: P40-P47 (AN000-AN007)
 *
 * @param[in] pin GPIO pin enum (encodes port and pin number)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port or pin is invalid
 */
rx_err_t rx_mpc_set_adc(gpio_pin_t pin);

/**
 * @brief Configure pin for SCI UART function
 *
 * Configures pin for SCI transmit or receive.
 *
 * @param[in] pin GPIO pin enum (encodes port and pin number)
 * @param[in] is_tx True for TX, false for RX
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port or pin is invalid
 */
rx_err_t rx_mpc_set_sci(gpio_pin_t pin, bool is_tx);

/**
 * @brief Configure pin for RIIC (I2C) function
 *
 * Configures pin for RIIC SCL or SDA.
 *
 * @param[in] pin GPIO pin enum (encodes port and pin number)
 * @param[in] is_scl True for SCL, false for SDA
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port or pin is invalid
 */
rx_err_t rx_mpc_set_riic(gpio_pin_t pin, bool is_scl);

/**
 * @brief Configure pin for RSPI (SPI) function
 *
 * Configures pin for RSPI clock, COPI, CIPO, or SSL.
 *
 * @param[in] pin GPIO pin enum (encodes port and pin number)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if port or pin is invalid
 */
rx_err_t rx_mpc_set_rspi(gpio_pin_t pin);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MPC_H */
