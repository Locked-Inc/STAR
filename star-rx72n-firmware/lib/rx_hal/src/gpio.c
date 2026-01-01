/* src/hardware/gpio.c */

/**
 * @file gpio.c
 * @brief GPIO Driver for RX72N
 *
 * GPIO control with integrated error handling, logging, and pin validation.
 */

#include <stddef.h>

#include "hardware.h"
#include "rx72n_regs.h"

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get PORT base address from port number
 *
 * @param[in] port Port number (0-9, or 0xA-0x10 for A-G)
 *
 * @return Pointer to PORT register base, or NULL if invalid port
 */
static volatile PORT_Type* internal_get_port_base(uint8_t port)
{
  switch (port) {
    case 0: {
      return PORT0_BASE;
    }
    case 1: {
      return PORT1_BASE;
    }
    case 2: {
      return PORT2_BASE;
    }
    case 3: {
      return PORT3_BASE;
    }
    case 4: {
      return PORT4_BASE;
    }
    case 5: {
      return PORT5_BASE;
    }
    case 6: {
      return PORT6_BASE;
    }
    case 7: {
      return PORT7_BASE;
    }
    case 8: {
      return PORT8_BASE;
    }
    case 9: {
      return PORT9_BASE;
    }
    case 0xA: {
      return PORTA_BASE;
    }
    case 0xB: {
      return PORTB_BASE;
    }
    case 0xC: {
      return PORTC_BASE;
    }
    case 0xD: {
      return PORTD_BASE;
    }
    case 0xE: {
      return PORTE_BASE;
    }
    case 0xF: {
      return PORTF_BASE;
    }
    case 0x10: {
      return PORTG_BASE;
    }
    default: {
      return NULL; /* Invalid port */
    }
  }
}

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
  volatile PORT_Type* port_base = internal_get_port_base(port);
  if (port_base == NULL) {
    star_log_error("GPIO", "Invalid port number");
    return k_rx_err_gpio_invalid_port;
  }

  /* Validate pin */
  if (pin > 7) {
    star_log_error("GPIO", "Invalid pin number");
    return k_rx_err_gpio_invalid_pin;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t gpio_set_output(uint8_t port, uint8_t pin)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Reserve pin through global pin validator */
  rx_pin_interface_t* pin_iface = rx_infrastructure_get_pin_interface();
  if (pin_iface != NULL) {
    err = pin_iface->reserve_pin(pin_iface->ctx, port, pin, "GPIO_OUT");
    if (err != k_rx_ok && err != k_rx_err_gpio_conflict) {
      /* Allow conflict (pin already reserved), but fail on other errors */
      RX_RETURN_ON_ERROR(err, "GPIO", "Pin reservation failed");
    }
  }

  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);

  /* Set as GPIO mode (not peripheral) */
  port_base->PMR &= ~(1 << pin);

  /* Set as output */
  port_base->PDR |= (1 << pin);

  star_log_debug("GPIO", "Pin configured as output");

  return k_rx_ok;
}

rx_err_t gpio_set_input(uint8_t port, uint8_t pin)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Reserve pin through global pin validator */
  rx_pin_interface_t* pin_iface = rx_infrastructure_get_pin_interface();
  if (pin_iface != NULL) {
    err = pin_iface->reserve_pin(pin_iface->ctx, port, pin, "GPIO_IN");
    if (err != k_rx_ok && err != k_rx_err_gpio_conflict) {
      /* Allow conflict (pin already reserved), but fail on other errors */
      RX_RETURN_ON_ERROR(err, "GPIO", "Pin reservation failed");
    }
  }

  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);

  /* Set as GPIO mode */
  port_base->PMR &= ~(1 << pin);

  /* Set as input */
  port_base->PDR &= ~(1 << pin);

  star_log_debug("GPIO", "Pin configured as input");

  return k_rx_ok;
}

rx_err_t gpio_write_high(uint8_t port, uint8_t pin)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);

  port_base->PODR |= (1 << pin);

  return k_rx_ok;
}

rx_err_t gpio_write_low(uint8_t port, uint8_t pin)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);

  port_base->PODR &= ~(1 << pin);

  return k_rx_ok;
}

rx_err_t gpio_toggle(uint8_t port, uint8_t pin)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);

  port_base->PODR ^= (1 << pin);

  return k_rx_ok;
}

rx_err_t gpio_read(uint8_t port, uint8_t pin, bool* value)
{
  /* Check null pointer */
  RX_CHECK_NULL_PTR(value, "GPIO", "Value pointer is NULL");

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);

  *value = (port_base->PIDR & (1 << pin)) != 0;

  return k_rx_ok;
}
