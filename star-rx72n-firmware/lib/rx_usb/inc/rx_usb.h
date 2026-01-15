/* lib/rx_usb/inc/rx_usb.h */

/**
 * @file rx_usb.h
 * @brief USB CDC-ACM Driver for RX72N
 * @details
 * This module provides a USB CDC-ACM (Communications Device Class - Abstract
 * Control Model) driver for the RX72N USB0 peripheral. When connected to a
 * host (e.g., Raspberry Pi 5), the device appears as a virtual serial port
 * (/dev/ttyACM0 on Linux).
 *
 * The driver operates in USB Function (peripheral) mode, allowing the RX72N
 * to be controlled by the Pi5 USB host.
 *
 * Features:
 * - USB 2.0 Full-Speed (12 Mbps)
 * - CDC-ACM class for virtual COM port
 * - Non-blocking read/write operations
 * - Ring buffer for TX/RX data
 * - VBUS detection for cable connect/disconnect
 *
 * Usage:
 * @code
 * rx_usb_config_t config = {0};
 * rx_usb_init(&config);
 *
 * // Wait for host to configure device
 * while (!rx_usb_is_configured()) {
 *   tx_thread_sleep(10);
 * }
 *
 * // Send data
 * uint8_t data[] = "Hello USB!";
 * rx_usb_write(data, sizeof(data));
 *
 * // Receive data
 * uint8_t buf[64];
 * uint32_t len;
 * rx_usb_read(buf, sizeof(buf), &len);
 * @endcode
 *
 * References:
 * - RX72N Group User's Manual: Hardware, Section 32 (USB 2.0 Module)
 * - USB CDC-ACM Specification (USB Class Definitions for Communications)
 * - Renesas RX Family USB Basic Driver Application Note (R01AN2025)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_USB_H
#define STAR_RX_USB_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief USB event types for callback notification
 */
typedef enum {
  k_usb_event_attached   = (0), /**< USB cable attached (VBUS detected) */
  k_usb_event_detached   = (1), /**< USB cable detached */
  k_usb_event_reset      = (2), /**< USB bus reset received */
  k_usb_event_configured = (3), /**< Host configured the device (ready to use) */
  k_usb_event_suspended  = (4), /**< Host suspended the device */
  k_usb_event_resumed    = (5), /**< Host resumed the device */
  k_usb_event_data_rx    = (6), /**< Data received from host */
  k_usb_event_data_tx    = (7), /**< Data transmission complete */
} rx_usb_event_t;

/**
 * @brief USB device state
 */
typedef enum {
  k_usb_state_detached   = (0), /**< No USB cable connected */
  k_usb_state_attached   = (1), /**< Cable connected, waiting for reset */
  k_usb_state_powered    = (2), /**< Powered, waiting for bus reset */
  k_usb_state_default    = (3), /**< Bus reset complete, address not set */
  k_usb_state_addressed  = (4), /**< Address assigned by host */
  k_usb_state_configured = (5), /**< Configuration set, ready to transfer */
  k_usb_state_suspended  = (6), /**< Host suspended the device */
} rx_usb_state_t;

/**
 * @brief USB callback function type
 *
 * @param event The USB event that occurred
 * @param ctx User-provided context pointer
 */
typedef void (*rx_usb_callback_t)(rx_usb_event_t event, void* ctx);

/**
 * @brief USB configuration structure
 */
typedef struct {
  rx_usb_callback_t callback; /**< Event callback function (optional) */
  void*             ctx;      /**< User context for callback */
} rx_usb_config_t;

/**
 * @brief USB CDC line coding structure (baud rate, stop bits, etc.)
 *
 * This structure matches the USB CDC SetLineCoding/GetLineCoding format.
 */
typedef struct {
  uint32_t baud_rate; /**< Data terminal rate in bits per second */
  uint8_t  stop_bits; /**< 0=1 stop bit, 1=1.5 stop bits, 2=2 stop bits */
  uint8_t  parity;    /**< 0=None, 1=Odd, 2=Even, 3=Mark, 4=Space */
  uint8_t  data_bits; /**< Data bits (5, 6, 7, 8, or 16) */
} rx_usb_line_coding_t;

/**
 * @brief USB statistics
 */
typedef struct {
  uint32_t bytes_rx;     /**< Total bytes received from host */
  uint32_t bytes_tx;     /**< Total bytes transmitted to host */
  uint32_t rx_overruns;  /**< RX buffer overrun count */
  uint32_t tx_underruns; /**< TX underrun count (no data to send) */
  uint32_t bus_resets;   /**< USB bus reset count */
  uint32_t suspends;     /**< Suspend count */
} rx_usb_stats_t;

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief USB buffer and endpoint configuration
 */
typedef enum {
  k_usb_rx_buffer_size  = (512), /**< Receive ring buffer size (bytes) */
  k_usb_tx_buffer_size  = (512), /**< Transmit ring buffer size (bytes) */
  k_usb_max_packet_size = (64),  /**< Full-speed bulk max packet size */
} rx_usb_buffer_config_t;

/**
 * @brief Default CDC line coding values
 *
 * These defaults are used during USB initialization before the host
 * sends SET_LINE_CODING. They represent a standard 115200 8N1 configuration.
 */
typedef enum {
  k_usb_default_baud_rate = (115200), /**< Default baud rate: 115200 bps */
  k_usb_default_stop_bits = (0),      /**< Default stop bits: 1 stop bit */
  k_usb_default_parity    = (0),      /**< Default parity: None */
  k_usb_default_data_bits = (8),      /**< Default data bits: 8 bits */
} rx_usb_line_coding_defaults_t;

/* =============================================================================
 * Public API Functions
 * =============================================================================
 */

/**
 * @brief Initialize USB CDC peripheral
 *
 * This function initializes the USB0 peripheral in function (device) mode
 * with CDC-ACM class support. After initialization, the device is ready
 * to be connected to a USB host.
 *
 * @param[in] config Configuration structure (NULL for defaults)
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if already initialized
 */
rx_err_t rx_usb_init(const rx_usb_config_t* config);

/**
 * @brief Deinitialize USB peripheral
 *
 * Stops USB operations and releases resources. The USB cable should be
 * disconnected before calling this function.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_usb_deinit(void);

/**
 * @brief Check if USB is connected and configured by host
 *
 * This function returns true when the USB host has completed enumeration
 * and configuration. Only after this returns true should data be sent.
 *
 * @return true if USB is configured and ready for data transfer
 * @return false if not connected or not yet configured
 */
bool rx_usb_is_configured(void);

/**
 * @brief Get current USB device state
 *
 * @return Current USB state
 */
rx_usb_state_t rx_usb_get_state(void);

/**
 * @brief Write data to USB CDC TX buffer
 *
 * Data is copied to an internal ring buffer and transmitted to the host
 * when the host polls for data. This function is non-blocking.
 *
 * @param[in] data Data buffer to transmit
 * @param[in] len Number of bytes to transmit
 * @return k_rx_ok on success (all data queued)
 * @return k_rx_err_busy if TX buffer is full (partial or no data queued)
 * @return k_rx_err_invalid_state if USB not configured
 * @return k_rx_err_null_ptr if data is NULL
 */
rx_err_t rx_usb_write(const uint8_t* data, uint32_t len);

/**
 * @brief Read data from USB CDC RX buffer
 *
 * Reads available data from the internal ring buffer. This function is
 * non-blocking and returns immediately with available data.
 *
 * @param[out] data Output buffer for received data
 * @param[in] max_len Maximum number of bytes to read
 * @param[out] actual_len Actual number of bytes read (can be 0)
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if data or actual_len is NULL
 */
rx_err_t rx_usb_read(uint8_t* data, uint32_t max_len, uint32_t* actual_len);

/**
 * @brief Check if RX data is available
 *
 * @param[out] available Number of bytes available in RX buffer
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if available is NULL
 */
rx_err_t rx_usb_rx_available(uint32_t* available);

/**
 * @brief Check TX buffer space available
 *
 * @param[out] available Number of bytes available in TX buffer
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if available is NULL
 */
rx_err_t rx_usb_tx_available(uint32_t* available);

/**
 * @brief Flush TX buffer
 *
 * Blocks until all data in the TX buffer has been transmitted to the host.
 *
 * @param[in] timeout_ms Maximum time to wait in milliseconds (0 = no wait)
 * @return k_rx_ok on success
 * @return k_rx_err_timeout if timeout expired before buffer was flushed
 */
rx_err_t rx_usb_flush(uint32_t timeout_ms);

/**
 * @brief Get current CDC line coding settings
 *
 * Returns the line coding (baud rate, stop bits, parity, data bits)
 * set by the host via SET_LINE_CODING request.
 *
 * @param[out] line_coding Line coding structure to fill
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if line_coding is NULL
 */
rx_err_t rx_usb_get_line_coding(rx_usb_line_coding_t* line_coding);

/**
 * @brief Get USB statistics
 *
 * @param[out] stats Statistics structure to fill
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if stats is NULL
 */
rx_err_t rx_usb_get_stats(rx_usb_stats_t* stats);

/**
 * @brief Reset USB statistics
 */
void rx_usb_reset_stats(void);

/**
 * @brief USB interrupt handler
 *
 * This function should be called from the USB interrupt service routine.
 * It handles all USB events (enumeration, data transfer, etc.).
 *
 * @note This is called internally by the ISR; do not call directly.
 */
void rx_usb_isr_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_USB_H */
