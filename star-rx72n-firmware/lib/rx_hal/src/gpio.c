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

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief GPIO port number constants for switch statement cases */
typedef enum {
  k_gpio_port_0 = 0,    /**< Port 0 */
  k_gpio_port_1 = 1,    /**< Port 1 */
  k_gpio_port_2 = 2,    /**< Port 2 */
  k_gpio_port_3 = 3,    /**< Port 3 */
  k_gpio_port_4 = 4,    /**< Port 4 */
  k_gpio_port_5 = 5,    /**< Port 5 */
  k_gpio_port_6 = 6,    /**< Port 6 */
  k_gpio_port_7 = 7,    /**< Port 7 */
  k_gpio_port_8 = 8,    /**< Port 8 */
  k_gpio_port_9 = 9,    /**< Port 9 */
  k_gpio_port_a = 0x0A, /**< Port A */
  k_gpio_port_b = 0x0B, /**< Port B */
  k_gpio_port_c = 0x0C, /**< Port C */
  k_gpio_port_d = 0x0D, /**< Port D */
  k_gpio_port_e = 0x0E, /**< Port E */
  k_gpio_port_f = 0x0F, /**< Port F */
  k_gpio_port_g = 0x10, /**< Port G */
} gpio_port_number_t;

/** @brief GPIO pin validation constants */
typedef enum {
  k_gpio_max_pin = 7, /**< Maximum valid pin number (pins 0-7) */
} gpio_pin_constants_t;

/** @brief GPIO register bit manipulation constants */
typedef enum {
  k_gpio_bit_set   = 1, /**< Value used for setting a single bit */
  k_gpio_bit_clear = 0, /**< Value for cleared bit comparison */
} gpio_bit_constants_t;

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
static volatile rx_port_regs_t* internal_get_port_base(uint8_t port)
{
  switch (port) {
    case k_gpio_port_0: {
      return PORT0_BASE;
    }
    case k_gpio_port_1: {
      return PORT1_BASE;
    }
    case k_gpio_port_2: {
      return PORT2_BASE;
    }
    case k_gpio_port_3: {
      return PORT3_BASE;
    }
    case k_gpio_port_4: {
      return PORT4_BASE;
    }
    case k_gpio_port_5: {
      return PORT5_BASE;
    }
    case k_gpio_port_6: {
      return PORT6_BASE;
    }
    case k_gpio_port_7: {
      return PORT7_BASE;
    }
    case k_gpio_port_8: {
      return PORT8_BASE;
    }
    case k_gpio_port_9: {
      return PORT9_BASE;
    }
    case k_gpio_port_a: {
      return PORTA_BASE;
    }
    case k_gpio_port_b: {
      return PORTB_BASE;
    }
    case k_gpio_port_c: {
      return PORTC_BASE;
    }
    case k_gpio_port_d: {
      return PORTD_BASE;
    }
    case k_gpio_port_e: {
      return PORTE_BASE;
    }
    case k_gpio_port_f: {
      return PORTF_BASE;
    }
    case k_gpio_port_g: {
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
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);
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
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  /* Set as GPIO mode (not peripheral) */
  port_base->pmr &= ~(k_gpio_bit_set << pin);

  /* Set as output */
  port_base->pdr |= (k_gpio_bit_set << pin);

  rx_log_debug("GPIO", "Pin configured as output");

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
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  /* Set as GPIO mode */
  port_base->pmr &= ~(k_gpio_bit_set << pin);

  /* Set as input */
  port_base->pdr &= ~(k_gpio_bit_set << pin);

  rx_log_debug("GPIO", "Pin configured as input");

  return k_rx_ok;
}

rx_err_t gpio_write_high(uint8_t port, uint8_t pin)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  port_base->podr |= (k_gpio_bit_set << pin);

  return k_rx_ok;
}

rx_err_t gpio_write_low(uint8_t port, uint8_t pin)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  port_base->podr &= ~(k_gpio_bit_set << pin);

  return k_rx_ok;
}

rx_err_t gpio_toggle(uint8_t port, uint8_t pin)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  port_base->podr ^= (k_gpio_bit_set << pin);

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
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  *value = (port_base->pidr & (k_gpio_bit_set << pin)) != k_gpio_bit_clear;

  return k_rx_ok;
}
