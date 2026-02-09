/* lib/rx_usb/src/rx_usb_internal.h */

/**
 * @file rx_usb_internal.h
 * @brief Internal shared definitions for USB CDC driver implementation
 * @details
 * This private header provides internal types and function declarations
 * shared between rx_usb.c and rx_usb_cdc.c. Not for public use.
 *
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_USB_INTERNAL_H
#define STAR_RX72N_USB_INTERNAL_H

#include <stdint.h>

#include "rx_usb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Internal Type Definitions
 * =============================================================================
 */

/**
 * @brief USB interface number assignments for CDC composite device
 *
 * Defines the interface numbers for each CDC port in the composite device.
 * Shared between rx_usb.c (port configuration) and rx_usb_cdc.c (descriptor
 * construction).
 */
typedef enum : uint8_t {
  k_intf_port0_control = 0, /**< Port 0 (Protocol) CDC control interface */
  k_intf_port0_data    = 1, /**< Port 0 (Protocol) CDC data interface */
  k_intf_port1_control = 2, /**< Port 1 (Decoded) CDC control interface */
  k_intf_port1_data    = 3, /**< Port 1 (Decoded) CDC data interface */
  k_intf_port2_control = 4, /**< Port 2 (Log) CDC control interface */
  k_intf_port2_data    = 5, /**< Port 2 (Log) CDC data interface */
} usb_interface_number_t;

/**
 * @brief Port hardware configuration structure (internal)
 *
 * Defines the hardware resources (interfaces, endpoints, pipes, and buffer sizes)
 * for a single USB CDC-ACM port. Used internally by rx_usb.c and rx_usb_cdc.c.
 */
typedef struct {
  const char* name;              /**< Port name for logging */
  uint16_t    rx_buffer_size;    /**< RX ring buffer size */
  uint16_t    tx_buffer_size;    /**< TX ring buffer size */
  uint8_t     interface_control; /**< Control interface number */
  uint8_t     interface_data;    /**< Data interface number */
  uint8_t     ep_bulk_in;        /**< Bulk IN endpoint address */
  uint8_t     ep_bulk_out;       /**< Bulk OUT endpoint address */
  uint8_t     ep_interrupt_in;   /**< Interrupt IN endpoint address */
  uint8_t     pipe_bulk_in;      /**< Pipe for Bulk IN */
  uint8_t     pipe_bulk_out;     /**< Pipe for Bulk OUT */
  uint8_t     pipe_interrupt;    /**< Pipe for Interrupt IN */
} rx_usb_port_hw_config_t;

/* =============================================================================
 * Shared-Internal Function Declarations
 *
 * These functions are shared between internal implementation files (rx_usb.c
 * and rx_usb_cdc.c) but are not part of the public API. They use the rx_usb_
 * prefix rather than priv_ to indicate shared-internal scope (multiple
 * implementation files) rather than file-private scope.
 * =============================================================================
 */

/**
 * @brief Get port hardware configuration (shared-internal)
 *
 * Returns the hardware configuration for the specified port.
 * Used by rx_usb_cdc.c to construct descriptors. Defined in rx_usb.c.
 *
 * @note This is a shared-internal API, not part of the public interface.
 *       The rx_usb_ prefix is used for internal functions shared across
 *       multiple implementation files within this module.
 *
 * @param[in] port Port ID
 * @return Pointer to port configuration, or nullptr if port is invalid
 */
const rx_usb_port_hw_config_t* rx_usb_get_port_config(rx_usb_port_id_t port);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_USB_INTERNAL_H */
