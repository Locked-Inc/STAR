/* lib/rx_hal/inc/rx_port_utils.h */

/**
 * @file rx_port_utils.h
 * @brief PORT Utility Functions for RX72N
 *
 * Shared utility functions for working with PORT registers.
 * Centralizes common PORT operations to avoid code duplication.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_PORT_UTILS_H
#define STAR_RX_PORT_UTILS_H

#include "rx72n_port_regs.h"
#include "rx_port_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get PORT register base address from port number
 *
 * Converts a port number to the corresponding PORT register base address.
 * This is the single source of truth for port-to-address mapping.
 *
 * @param[in] port Port number (use k_rx_port_* constants from rx_port_constants.h)
 *
 * @return Pointer to PORT register base, or NULL if invalid port
 *
 * @note Only ports available on 100-pin LFQFP are supported:
 *       - PORT0-5 (some limited)
 *       - PORTA-E (full)
 *       - PORTJ (limited)
 */
static inline volatile rx_port_regs_t* rx_port_get_base(uint8_t port)
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
    case k_rx_port_j: {
      return portj();
    }
    default: {
      return NULL; /* Invalid port */
    }
  }
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_PORT_UTILS_H */
