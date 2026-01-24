/* lib/rx_spi_comm/inc/rx_spi_comm.h */

/**
 * @file rx_spi_comm.h
 * @brief High-Level SPI Communication Layer for RX72N
 * @details
 * Integrates Frame Layer, HARQ/FEC, and SPI HAL for reliable communication
 * with the RPi5 controller. Handles:
 * - Frame encoding/decoding with CRC-32
 * - Optional FEC encoding with HARQ retransmission
 * - ACK/NACK protocol management
 * - Sequence number tracking
 *
 * @note RX72N operates as SPI peripheral, RPi5 as SPI controller.
 *
 * @see rx_frame.h for frame layer
 * @see rx_harq.h for HARQ/FEC
 * @see hardware.h for SPI HAL
 * @see docs/sections/01_nanopb_protocol.tex for protocol specification
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_SPI_COMM_H
#define STAR_RX_SPI_COMM_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief SPI communication parameters
 */
typedef enum : uint16_t {
  k_spi_comm_default_channel = 0,    /**< Default RSPI channel */
  k_spi_comm_default_mode    = 0,    /**< SPI mode 0 (CPOL=0, CPHA=0) */
  k_spi_comm_rx_buffer_size  = 2048, /**< RX staging buffer size */
  k_spi_comm_tx_buffer_size  = 2048, /**< TX staging buffer size */
  k_spi_comm_default_timeout = 1000, /**< Default timeout in ms */
} rx_spi_comm_constants_t;

/* =============================================================================
 * Handle and Configuration
 * =============================================================================
 */

/**
 * @brief SPI communication handle
 *
 * Contains all state for SPI communication including frame encoder/decoder,
 * sequence tracking, and staging buffers.
 */
typedef struct {
  rx_frame_encoder_t encoder;                              /**< Frame encoder */
  rx_frame_decoder_t decoder;                              /**< Frame decoder */
  uint8_t            rx_buffer[k_spi_comm_rx_buffer_size]; /**< RX staging buffer */
  uint8_t            tx_buffer[k_spi_comm_tx_buffer_size]; /**< TX staging buffer */
  uint16_t           tx_sequence;                          /**< TX sequence counter */
  uint16_t           rx_sequence;                          /**< Expected RX sequence */
  uint8_t            channel;                              /**< RSPI channel */
  bool               fec_enabled;                          /**< FEC enabled flag */
  bool               initialized;                          /**< Init flag */
} rx_spi_comm_handle_t;

/**
 * @brief SPI communication configuration
 */
typedef struct {
  uint8_t channel;     /**< RSPI channel (0-2, default 0) */
  uint8_t spi_mode;    /**< SPI mode (0-3, default 0) */
  bool    fec_enabled; /**< Enable FEC encoding (false = disabled) */
} rx_spi_comm_config_t;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize SPI communication handle
 *
 * Sets up frame encoder/decoder and optionally HARQ/FEC.
 * Does NOT initialize SPI hardware - call rspi_init_peripheral() first.
 *
 * @param[out] handle Pointer to handle
 * @param[in]  config Configuration (NULL for defaults)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle is NULL
 */
rx_err_t rx_spi_comm_init(rx_spi_comm_handle_t* handle, const rx_spi_comm_config_t* config);

/**
 * @brief Deinitialize SPI communication handle
 *
 * @param[in,out] handle Pointer to handle
 * @return k_rx_ok on success
 */
rx_err_t rx_spi_comm_deinit(rx_spi_comm_handle_t* handle);

/* =============================================================================
 * Send API
 * =============================================================================
 */

/**
 * @brief Send a response frame
 *
 * Encodes payload into a frame with automatic sequence number and CRC.
 * Optionally applies FEC encoding.
 *
 * @param[in,out] handle Initialized handle
 * @param[in]     type Frame type (typically k_frame_type_response)
 * @param[in]     flags Frame flags
 * @param[in]     payload Payload data
 * @param[in]     payload_len Payload length
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle or payload is NULL
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_invalid_size if payload exceeds maximum
 */
rx_err_t rx_spi_comm_send(rx_spi_comm_handle_t* handle,
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
rx_err_t rx_spi_comm_send_ack(rx_spi_comm_handle_t* handle, uint16_t sequence);

/**
 * @brief Send NACK for received frame
 *
 * @param[in,out] handle Initialized handle
 * @param[in]     sequence Sequence number
 * @param[in]     flags Additional flags (e.g., k_frame_flag_soft_nack)
 *
 * @return k_rx_ok on success
 */
rx_err_t rx_spi_comm_send_nack(rx_spi_comm_handle_t* handle, uint16_t sequence, uint8_t flags);

/* =============================================================================
 * Receive API
 * =============================================================================
 */

/**
 * @brief Receive and decode a frame
 *
 * Reads from SPI, validates CRC, and decodes frame.
 * Optionally applies FEC decoding with HARQ combining.
 *
 * @param[in,out] handle Initialized handle
 * @param[out]    frame Decoded frame output
 * @param[in]     timeout_ms Timeout in milliseconds (0 = poll once)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if handle or frame is NULL
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_timeout if no data received within timeout
 * @return k_rx_err_crc_mismatch if CRC validation fails
 * @return k_rx_err_protocol_error if frame format invalid
 */
rx_err_t rx_spi_comm_receive(rx_spi_comm_handle_t* handle, rx_frame_t* frame, uint32_t timeout_ms);

/**
 * @brief Check if data is available for reading
 *
 * @param[in]  handle Initialized handle
 * @param[out] available True if data is waiting
 *
 * @return k_rx_ok on success
 */
rx_err_t rx_spi_comm_data_available(const rx_spi_comm_handle_t* handle, bool* available);

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

/**
 * @brief Reset sequence counters
 *
 * @param[in,out] handle Pointer to handle
 */
void rx_spi_comm_reset_sequence(rx_spi_comm_handle_t* handle);

/**
 * @brief Get current TX sequence number
 *
 * @param[in] handle Pointer to handle
 * @param[out] sequence Output pointer for TX sequence number
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_spi_comm_get_tx_sequence(const rx_spi_comm_handle_t* handle, uint16_t* sequence);

/**
 * @brief Get expected RX sequence number
 *
 * @param[in] handle Pointer to handle
 * @param[out] sequence Output pointer for RX sequence number
 * @return k_rx_ok on success, error code on failure
 */
rx_err_t rx_spi_comm_get_rx_sequence(const rx_spi_comm_handle_t* handle, uint16_t* sequence);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_SPI_COMM_H */
