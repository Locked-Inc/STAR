/* src/hardware/gpio.c */

/**
 * @file gpio.c
 * @brief GPIO Driver for RX72N
 *
 * Simple GPIO control for LED and digital I/O.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx72n_regs.h"

/* =============================================================================
 * GPIO Configuration
 * =============================================================================
 */

/**
 * @brief Get PORT base address from port number
 *
 * @param[in] port Port number (0-9, or 0xA-0x10 for A-G)
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
 * @brief Configure GPIO pin as output
 *
 * @param[in] port Port number (0-9, or 0xA-0xG for A-G)
 * @param[in] pin Pin number (0-7)
 */
void gpio_set_output(uint8_t port, uint8_t pin)
{
  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);
  if (port_base == NULL) {
    return; /* Invalid port */
  }

  /* Set as GPIO mode (not peripheral) */
  port_base->PMR &= ~(1 << pin);

  /* Set as output */
  port_base->PDR |= (1 << pin);
}

/**
 * @brief Configure GPIO pin as input
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 */
void gpio_set_input(uint8_t port, uint8_t pin)
{
  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);
  if (port_base == NULL) {
    return; /* Invalid port */
  }

  /* Set as GPIO mode */
  port_base->PMR &= ~(1 << pin);

  /* Set as input */
  port_base->PDR &= ~(1 << pin);
}

/**
 * @brief Set GPIO pin high
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 */
void gpio_write_high(uint8_t port, uint8_t pin)
{
  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);
  if (port_base == NULL) {
    return; /* Invalid port */
  }

  port_base->PODR |= (1 << pin);
}

/**
 * @brief Set GPIO pin low
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 */
void gpio_write_low(uint8_t port, uint8_t pin)
{
  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);
  if (port_base == NULL) {
    return; /* Invalid port */
  }

  port_base->PODR &= ~(1 << pin);
}

/**
 * @brief Toggle GPIO pin
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 */
void gpio_toggle(uint8_t port, uint8_t pin)
{
  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);
  if (port_base == NULL) {
    return; /* Invalid port */
  }

  port_base->PODR ^= (1 << pin);
}

/**
 * @brief Read GPIO pin
 *
 * @param[in] port Port number
 * @param[in] pin Pin number (0-7)
 * @return true if pin is high, false if low
 */
bool gpio_read(uint8_t port, uint8_t pin)
{
  /* Get port base address */
  volatile PORT_Type* port_base = internal_get_port_base(port);
  if (port_base == NULL) {
    return false; /* Invalid port */
  }

  return (port_base->PIDR & (1 << pin)) != 0;
}
