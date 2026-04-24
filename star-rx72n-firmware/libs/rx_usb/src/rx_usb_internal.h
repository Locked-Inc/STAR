/**
 * @file rx_usb_internal.h
 * @brief Internal shared definitions for USB CDC driver implementation
 * @details
 * This private header provides internal types and function declarations
 * shared between rx_usb.c and rx_usb_cdc.c. Not for public use.
 *
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

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

/* =============================================================================
 * Hardware Layer Functions (defined in rx_usb_hw.c)
 * =============================================================================
 */

/**
 * @brief Read data from a USB pipe's FIFO
 *
 * @param[in] pipe Pipe number to read from
 * @param[out] data Buffer to store read data
 * @param[in] max_len Maximum bytes to read
 * @return Number of bytes actually read
 */
uint32_t rx_usb_hw_fifo_read(uint8_t pipe, uint8_t* data, uint32_t max_len);

/**
 * @brief Write data to a USB pipe's FIFO
 *
 * @param[in] pipe Pipe number to write to
 * @param[in] data Data to write
 * @param[in] len Number of bytes to write
 * @return Number of bytes actually written
 */
uint32_t rx_usb_hw_fifo_write(uint8_t pipe, const uint8_t* data, uint32_t len);

/**
 * @brief Emit a zero-length packet (ZLP) on a bulk IN pipe
 *
 * Commits an empty packet so hosts observe an explicit end-of-transfer
 * marker.  Required when the previous transmission was exactly one full
 * wMaxPacketSize (64 bytes for FS bulk); without a short packet or ZLP
 * the host read blocks waiting for more data.
 *
 * @param[in] pipe Bulk IN pipe number (1-9; DCP not supported)
 * @return k_rx_ok on success, k_rx_err_busy if pipe still transmitting,
 *         k_rx_err_invalid_arg for invalid pipe, k_rx_err_timeout on FIFO
 *         hang.
 */
rx_err_t rx_usb_hw_fifo_write_zlp(uint8_t pipe);

/**
 * @brief Set USB device address during enumeration
 *
 * @param[in] address USB address assigned by host (0-127)
 */
void rx_usb_hw_set_address(uint8_t address);

/**
 * @brief Configure a USB pipe for bulk/interrupt transfer
 *
 * @param[in] pipe Pipe number (1-9)
 * @param[in] endpoint Endpoint number (0-15)
 * @param[in] is_in True for IN (device to host), false for OUT
 * @param[in] type Pipe type (bulk, interrupt, isochronous)
 * @param[in] max_packet Maximum packet size
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_usb_hw_configure_pipe(uint8_t  pipe,
                                  uint8_t  endpoint,
                                  bool     is_in,
                                  uint16_t type,
                                  uint16_t max_packet);

/**
 * @brief True if the pipe's buffer is drained and ready for a new packet.
 *
 * Reads PIPEnCTR.PBUSY.  Handlers call this BEFORE popping from the
 * TX ring so bytes aren't lost when the hardware is still mid-transmit.
 *
 * @param[in] pipe Pipe number (1..9; pipe 0 / DCP is handled separately).
 * @return true if pipe accepts a packet, false if busy or invalid.
 */
bool rx_usb_hw_pipe_ready_for_write(uint8_t pipe);

/** @brief Initialize USB0 hardware peripheral */
rx_err_t rx_usb_hw_init(void);

/** @brief Deinitialize USB0 hardware peripheral */
rx_err_t rx_usb_hw_deinit(void);

/** @brief Attach to USB bus (enable D+ pull-up) */
rx_err_t rx_usb_hw_attach(void);

/** @brief Detach from USB bus (disable D+ pull-up) */
rx_err_t rx_usb_hw_detach(void);

/** @brief Get current USB bus state from hardware registers */
rx_usb_state_t rx_usb_hw_get_bus_state(void);

/* =============================================================================
 * Shared Functions from rx_usb.c
 * =============================================================================
 */

/** @brief Set global USB device state */
void rx_usb_set_state(rx_usb_state_t state);

/** @brief Set CDC line coding for a port */
void rx_usb_set_line_coding(rx_usb_port_id_t port, const rx_usb_line_coding_t* coding);

/** @brief Push data into a port's RX ring buffer */
uint32_t rx_usb_rx_push(rx_usb_port_id_t port, const uint8_t* data, uint32_t len);

/** @brief Pop data from a port's TX ring buffer */
uint32_t rx_usb_tx_pop(rx_usb_port_id_t port, uint8_t* data, uint32_t max_len);

/**
 * @brief Query whether a port needs a terminating ZLP on its next BEMP
 *
 * Returns true when the last bulk IN packet emitted on the port was a
 * full wMaxPacketSize packet and the TX ring became empty as a result.
 * The ISR-driven bulk-IN handler uses this to drop a zero-length packet
 * so the host's read(2) returns instead of blocking for additional data.
 */
bool rx_usb_tx_zlp_pending(rx_usb_port_id_t port);

/**
 * @brief Update the per-port "last packet was full MPS" marker
 *
 * Called from rx_usb_cdc_handle_bulk_in() so the next BEMP can decide
 * whether to emit a ZLP.  Set true after queueing a max-packet bulk IN
 * with no more data in the ring, false after a short packet or after
 * the terminating ZLP has been emitted.
 */
void rx_usb_tx_set_zlp_pending(rx_usb_port_id_t port, bool full);

/** @brief Increment bus reset counter */
void rx_usb_count_bus_reset(void);

/** @brief Increment suspend counter */
void rx_usb_count_suspend(void);

/** @brief Find port by pipe number */
rx_usb_port_id_t rx_usb_find_port_by_pipe(uint8_t pipe);

/** @brief Find port by interface number */
rx_usb_port_id_t rx_usb_find_port_by_interface(uint8_t interface);

/** @brief Invoke user callback for a port event */
void rx_usb_invoke_callback(rx_usb_port_id_t port, rx_usb_event_t event);

/* =============================================================================
 * Shared Functions from rx_usb_cdc.c
 * =============================================================================
 */

/** @brief Initialize CDC class (descriptors, state) */
rx_err_t rx_usb_cdc_init(void);

/** @brief Handle USB control transfer (SETUP stage) */
void rx_usb_cdc_handle_setup(void);

/** @brief Handle bulk IN transfer completion for a port */
void rx_usb_cdc_handle_bulk_in(rx_usb_port_id_t port);

/** @brief Handle bulk OUT data received for a port */
void rx_usb_cdc_handle_bulk_out(rx_usb_port_id_t port);

#ifdef __cplusplus
}
#endif
