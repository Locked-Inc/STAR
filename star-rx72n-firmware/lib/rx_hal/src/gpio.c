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
#include "rx_port_constants.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

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
    case k_rx_port_0: {
      return port0();
    }
    case k_rx_port_1: {
      return port1();
    }
    case k_rx_port_2: {
      return port2();
    }
    case k_rx_port_3: {
      return port3();
    }
    case k_rx_port_4: {
      return port4();
    }
    case k_rx_port_5: {
      return port5();
    }
    case k_rx_port_6: {
      return port6();
    }
    case k_rx_port_7: {
      return port7();
    }
    case k_rx_port_8: {
      return port8();
    }
    case k_rx_port_9: {
      return port9();
    }
    case k_rx_port_a: {
      return porta();
    }
    case k_rx_port_b: {
      return portb();
    }
    case k_rx_port_c: {
      return portc();
    }
    case k_rx_port_d: {
      return portd();
    }
    case k_rx_port_e: {
      return porte();
    }
    case k_rx_port_f: {
      return portf();
    }
    case k_rx_port_g: {
      return portg();
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

rx_err_t gpio_set_output(gpio_pin_t pin)
{
  /* Extract port and pin number from type-safe enum */
  uint8_t port    = gpio_pin_get_port(pin);
  uint8_t pin_num = gpio_pin_get_pin(pin);

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Reserve pin through global pin validator */
  rx_pin_interface_t* pin_iface = rx_infrastructure_get_pin_interface();
  if (pin_iface != NULL) {
    err = pin_iface->reserve_pin(pin_iface->ctx, port, pin_num, "GPIO_OUT");
    if (err != k_rx_ok && err != k_rx_err_gpio_conflict) {
      /* Allow conflict (pin already reserved), but fail on other errors */
      RX_RETURN_ON_ERROR(err, "GPIO", "Pin reservation failed");
    }
  }

  /* Get port base address */
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  /* Set as GPIO mode (not peripheral) */
  port_base->pmr &= ~(k_gpio_bit_set << pin_num);

  /* Set as output */
  port_base->pdr |= (k_gpio_bit_set << pin_num);

  rx_log_debug("GPIO", "Pin configured as output");

  return k_rx_ok;
}

rx_err_t gpio_set_input(gpio_pin_t pin)
{
  /* Extract port and pin number from type-safe enum */
  uint8_t port    = gpio_pin_get_port(pin);
  uint8_t pin_num = gpio_pin_get_pin(pin);

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Reserve pin through global pin validator */
  rx_pin_interface_t* pin_iface = rx_infrastructure_get_pin_interface();
  if (pin_iface != NULL) {
    err = pin_iface->reserve_pin(pin_iface->ctx, port, pin_num, "GPIO_IN");
    if (err != k_rx_ok && err != k_rx_err_gpio_conflict) {
      /* Allow conflict (pin already reserved), but fail on other errors */
      RX_RETURN_ON_ERROR(err, "GPIO", "Pin reservation failed");
    }
  }

  /* Get port base address */
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  /* Set as GPIO mode */
  port_base->pmr &= ~(k_gpio_bit_set << pin_num);

  /* Set as input */
  port_base->pdr &= ~(k_gpio_bit_set << pin_num);

  rx_log_debug("GPIO", "Pin configured as input");

  return k_rx_ok;
}

rx_err_t gpio_write_high(gpio_pin_t pin)
{
  /* Extract port and pin number from type-safe enum */
  uint8_t port    = gpio_pin_get_port(pin);
  uint8_t pin_num = gpio_pin_get_pin(pin);

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  port_base->podr |= (k_gpio_bit_set << pin_num);

  return k_rx_ok;
}

rx_err_t gpio_write_low(gpio_pin_t pin)
{
  /* Extract port and pin number from type-safe enum */
  uint8_t port    = gpio_pin_get_port(pin);
  uint8_t pin_num = gpio_pin_get_pin(pin);

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  port_base->podr &= ~(k_gpio_bit_set << pin_num);

  return k_rx_ok;
}

rx_err_t gpio_toggle(gpio_pin_t pin)
{
  /* Extract port and pin number from type-safe enum */
  uint8_t port    = gpio_pin_get_port(pin);
  uint8_t pin_num = gpio_pin_get_pin(pin);

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  port_base->podr ^= (k_gpio_bit_set << pin_num);

  return k_rx_ok;
}

rx_err_t gpio_read(gpio_pin_t pin, bool* value)
{
  /* Check null pointer */
  RX_CHECK_NULL_PTR(value, "GPIO", "Value pointer is NULL");

  /* Extract port and pin number from type-safe enum */
  uint8_t port    = gpio_pin_get_port(pin);
  uint8_t pin_num = gpio_pin_get_pin(pin);

  /* Validate parameters */
  rx_err_t err = internal_validate_port_pin(port, pin_num);
  RX_RETURN_ON_ERROR(err, "GPIO", "Port/pin validation failed");

  /* Get port base address */
  volatile rx_port_regs_t* port_base = internal_get_port_base(port);

  *value = (port_base->pidr & (k_gpio_bit_set << pin_num)) != k_gpio_bit_clear;

  return k_rx_ok;
}
