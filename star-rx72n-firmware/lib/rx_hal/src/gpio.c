/* lib/rx_hal/src/gpio.c */

/**
 * @file gpio.c
 * @brief GPIO Driver for RX72N
 *
 * GPIO control with integrated error handling, logging, and pin validation.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stddef.h>

#include "hardware.h"
#include "rx72n_regs.h"
#include "rx_port_utils.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief GPIO pin validation constants */
typedef enum : uint8_t {
  k_gpio_max_pin = 7, /**< Maximum valid pin number (pins 0-7) */
} gpio_pin_constants_t;

/** @brief GPIO register bit manipulation constants */
typedef enum : uint8_t {
  k_gpio_bit_set   = 1, /**< Value used for setting a single bit */
  k_gpio_bit_clear = 0, /**< Value for cleared bit comparison */
} gpio_bit_constants_t;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Validate port and pin numbers
 *
 * @param[in] port Port number
 * @param[in] pin Pin number
 *
 * @return k_rx_ok if valid, error code otherwise
 */
static rx_err_t internal_validate_port_pin(uint8_t port, uint8_t pin)
{
  /* Validate port */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);
  if (port_base == NULL) {
    rx_log_error("GPIO", "Invalid port number");
    return k_rx_err_gpio_invalid_port;
  }

  /* Validate pin */
  if (pin > k_gpio_max_pin) {
    rx_log_error("GPIO", "Invalid pin number");
    return k_rx_err_gpio_invalid_pin;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t gpio_set_output(rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t       port      = rx_port_from_pin(pin);
  const uint8_t       pin_num   = rx_pin_from_pin(pin);
  rx_pin_interface_t* pin_iface = NULL;

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Reserve pin through global pin validator */
  pin_iface = rx_infrastructure_get_pin_interface();
  if (pin_iface != NULL) {
    err = pin_iface->reserve_pin(pin_iface->ctx, port, pin_num, "GPIO_OUT");
    if (err != k_rx_ok && err != k_rx_err_gpio_conflict) {
      /* Allow conflict (pin already reserved), but fail on other errors */
      RX_RETURN_ON_ERROR(err, "GPIO", "Pin reservation failed");
    }
  }

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  /* Set as GPIO mode (not peripheral) */
  port_base->pmr &= ~(k_gpio_bit_set << pin_num);

  /* Set as output */
  port_base->pdr |= (k_gpio_bit_set << pin_num);

  rx_log_debug("GPIO", "Pin configured as output");

  return k_rx_ok;
}

rx_err_t gpio_set_input(rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t       port      = rx_port_from_pin(pin);
  const uint8_t       pin_num   = rx_pin_from_pin(pin);
  rx_pin_interface_t* pin_iface = NULL;

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Reserve pin through global pin validator */
  pin_iface = rx_infrastructure_get_pin_interface();
  if (pin_iface != NULL) {
    err = pin_iface->reserve_pin(pin_iface->ctx, port, pin_num, "GPIO_IN");
    if (err != k_rx_ok && err != k_rx_err_gpio_conflict) {
      /* Allow conflict (pin already reserved), but fail on other errors */
      RX_RETURN_ON_ERROR(err, "GPIO", "Pin reservation failed");
    }
  }

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  /* Set as GPIO mode */
  port_base->pmr &= ~(k_gpio_bit_set << pin_num);

  /* Set as input */
  port_base->pdr &= ~(k_gpio_bit_set << pin_num);

  rx_log_debug("GPIO", "Pin configured as input");

  return k_rx_ok;
}

rx_err_t gpio_write_high(rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate parameters */
  const rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  port_base->podr |= (k_gpio_bit_set << pin_num);

  return k_rx_ok;
}

rx_err_t gpio_write_low(rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate parameters */
  const rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  port_base->podr &= ~(k_gpio_bit_set << pin_num);

  return k_rx_ok;
}

rx_err_t gpio_toggle(rx_port_pin_t pin)
{
  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate parameters */
  const rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  port_base->podr ^= (k_gpio_bit_set << pin_num);

  return k_rx_ok;
}

rx_err_t gpio_read(rx_port_pin_t pin, bool* value)
{
  /* Check null pointer */
  RX_CHECK_NULL_PTR(value, "GPIO", "Value pointer is NULL");

  /* Extract port and pin number from rx_port_pin_t */
  const uint8_t port    = rx_port_from_pin(pin);
  const uint8_t pin_num = rx_pin_from_pin(pin);

  /* Validate parameters */
  const rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = rx_port_get_base(port);

  *value = (port_base->pidr & (k_gpio_bit_set << pin_num)) != k_gpio_bit_clear;

  return k_rx_ok;
}
