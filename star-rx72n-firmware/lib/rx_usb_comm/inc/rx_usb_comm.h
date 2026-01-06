/* lib/rx_usb_comm/inc/rx_usb_comm.h */

/**
 * @file rx_usb_comm.h
 * @brief High-Level USB CDC Communication Layer for RX72N
 * @details
 * Integrates Frame Layer and USB CDC for reliable communication with the
 * RPi5 controller. Provides the same frame-based protocol as SPI but over
 * USB CDC-ACM virtual serial port.
 *
 * Features:
 * - Frame encoding/decoding with CRC-32
 * - Sequence number tracking
 * - Non-blocking send/receive operations
 * - Same wire protocol as SPI (interchangeable)
 *
 * @note RPi5 sees this as /dev/ttyACM0 virtual serial port.
 *
 * @see rx_frame.h for frame layer
 * @see rx_usb.h for USB CDC driver
 * @see docs/sections/01_nanopb_protocol.tex for protocol specification
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_USB_COMM_H
#define STAR_RX_USB_COMM_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_frame.h"
#include "rx_time_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief USB communication parameters
 */
typedef enum {
  k_usb_comm_rx_buffer_size  = 2048, /**< RX staging buffer size */
  k_usb_comm_tx_buffer_size  = 2048, /**< TX staging buffer size */
  k_usb_comm_default_timeout = 1000, /**< Default timeout in ms */
} rx_usb_comm_constants_t;

/* =============================================================================
 * Handle and Configuration
 * =============================================================================
 */

/**
 * @brief USB communication handle
 *
 * Contains all state for USB communication including frame encoder/decoder,
 * sequence tracking, and staging buffers.
 */
typedef struct {
  rx_frame_encoder_t         encoder;                              /**< Frame encoder */
  rx_frame_decoder_t         decoder;                              /**< Frame decoder */
  uint8_t                    rx_buffer[k_usb_comm_rx_buffer_size]; /**< RX staging buffer */
  uint8_t                    tx_buffer[k_usb_comm_tx_buffer_size]; /**< TX staging buffer */
  uint32_t                   rx_buffer_len;                        /**< Bytes in RX buffer */
  uint32_t                   rx_buffer_pos;                        /**< Current read position */
  uint16_t                   tx_sequence;                          /**< TX sequence counter */
  uint16_t                   rx_sequence;                          /**< Expected RX sequence */
  uint8_t                    fec_enabled;                          /**< FEC enabled flag */
  uint8_t                    initialized;                          /**< Init flag */
  const rx_time_interface_t* time_iface;                           /**< Time interface (optional) */
} rx_usb_comm_handle_t;

/**
 * @brief USB communication configuration
 */
typedef struct {
  uint8_t                    fec_enabled; /**< Enable FEC encoding (0 = disabled) */
  const rx_time_interface_t* time_iface;  /**< Time interface (NULL for default behavior) */
} rx_usb_comm_config_t;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize USB communication handle
 *
 * Sets up frame encoder/decoder and internal buffers.
 * USB driver must be initialized separately via rx_usb_init().
 *
 * @param[out] handle Pointer to handle
 * @param[in]  config Configuration (NULL for defaults)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle is NULL
 */
rx_err_t rx_usb_comm_init(rx_usb_comm_handle_t* handle, const rx_usb_comm_config_t* config);

/**
 * @brief Deinitialize USB communication handle
 *
 * @param[in,out] handle Pointer to handle
 * @return k_rx_ok on success
 */
rx_err_t rx_usb_comm_deinit(rx_usb_comm_handle_t* handle);

/* =============================================================================
 * Send API
 * =============================================================================
 */

/**
 * @brief Send a response frame over USB
 *
 * Encodes payload into a frame with automatic sequence number and CRC.
 * Non-blocking - data is queued to USB TX buffer.
 *
 * @param[in,out] handle Initialized handle
 * @param[in]     type Frame type (typically k_frame_type_response)
 * @param[in]     flags Frame flags
 * @param[in]     payload Payload data
 * @param[in]     payload_len Payload length
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle or payload is NULL
 * @return k_rx_err_invalid_state if not initialized or USB not configured
 * @return k_rx_err_invalid_size if payload exceeds maximum
 * @return k_rx_err_busy if TX buffer is full
 */
rx_err_t rx_usb_comm_send(rx_usb_comm_handle_t* handle,
                          rx_frame_type_t       type,
                          uint8_t               flags,
                          const uint8_t*        payload,
                          uint32_t              payload_len);

/**
 * @brief Send ACK for received frame
 *
 * @param[in,out] handle Initialized handle
 * @param[in]     sequence Sequence number to acknowledge
 *
 * @return k_rx_ok on success
 */
rx_err_t rx_usb_comm_send_ack(rx_usb_comm_handle_t* handle, uint16_t sequence);

/**
 * @brief Send NACK for received frame
 *
 * @param[in,out] handle Initialized handle
 * @param[in]     sequence Sequence number
 * @param[in]     flags Additional flags (e.g., k_frame_flag_soft_nack)
 *
 * @return k_rx_ok on success
 */
rx_err_t rx_usb_comm_send_nack(rx_usb_comm_handle_t* handle, uint16_t sequence, uint8_t flags);

/* =============================================================================
 * Receive API
 * =============================================================================
 */

/**
 * @brief Receive and decode a frame from USB
 *
 * Reads from USB CDC buffer, validates CRC, and decodes frame.
 * Non-blocking if timeout_ms is 0.
 *
 * @param[in,out] handle Initialized handle
 * @param[out]    frame Decoded frame output
 * @param[in]     timeout_ms Timeout in milliseconds (0 = poll once)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle or frame is NULL
 * @return k_rx_err_invalid_state if not initialized or USB not configured
 * @return k_rx_err_timeout if no complete frame received within timeout
 * @return k_rx_err_crc_mismatch if CRC validation fails
 * @return k_rx_err_protocol_error if frame format invalid
 */
rx_err_t rx_usb_comm_receive(rx_usb_comm_handle_t* handle, rx_frame_t* frame, uint32_t timeout_ms);

/**
 * @brief Check if data is available for reading
 *
 * @param[in]  handle Initialized handle
 * @param[out] available True if at least one byte is waiting
 *
 * @return k_rx_ok on success
 */
rx_err_t rx_usb_comm_data_available(rx_usb_comm_handle_t* handle, bool* available);

/**
 * @brief Check if USB is connected and ready
 *
 * @param[in]  handle Initialized handle
 * @param[out] ready True if USB is configured and ready for communication
 *
 * @return k_rx_ok on success
 */
rx_err_t rx_usb_comm_is_ready(rx_usb_comm_handle_t* handle, bool* ready);

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

/**
 * @brief Reset sequence counters
 *
 * @param[in,out] handle Pointer to handle
 */
void rx_usb_comm_reset_sequence(rx_usb_comm_handle_t* handle);

/**
 * @brief Get current TX sequence number
 *
 * @param[in] handle Pointer to handle
 * @return Current TX sequence number
 */
uint16_t rx_usb_comm_get_tx_sequence(const rx_usb_comm_handle_t* handle);

/**
 * @brief Get expected RX sequence number
 *
 * @param[in] handle Pointer to handle
 * @return Expected RX sequence number
 */
uint16_t rx_usb_comm_get_rx_sequence(const rx_usb_comm_handle_t* handle);

/**
 * @brief Flush internal receive buffer
 *
 * Discards any partially received data.
 *
 * @param[in,out] handle Pointer to handle
 */
void rx_usb_comm_flush_rx(rx_usb_comm_handle_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_USB_COMM_H */
