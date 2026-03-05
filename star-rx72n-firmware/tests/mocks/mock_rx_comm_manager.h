/* star-rx72n-firmware/tests/mocks/mock_rx_comm_manager.h */

/**
 * @file mock_rx_comm_manager.h
 * @brief Mock communication manager for protocol testing without hardware
 *
 * @details
 * Provides test double for communication manager (USB CDC/SPI protocols) to enable
 * unit testing of frame encoding/decoding, protocol error handling, and telemetry
 * without actual USB or SPI hardware.
 *
 * Enables testing of: Frame send/receive logic, Protocol error handling, CRC validation,
 * Frame queue management, Telemetry formatting
 *
 * @par Mock Capabilities: Frame queue simulation (incoming frames), Send tracking
 * (outgoing frames), Error injection, Call tracking
 * @par Usage: tests/test_rx_comm_manager.c, tests/test_telemetry_task.c
 * @see rx_comm_manager.h Real communication manager
 * @see mock_rx_nanopb.h Protocol Buffers mock (used by comm manager)
 * @par NASA Power of 10: [OK] Static allocation, bounded queues
 * @par SOLID: S - Single responsibility (communication only)
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum mock_comm_manager_constants_t
 * @brief Mock communication manager constants
 */
typedef enum : uint16_t {
  k_mock_comm_frame_queue_size = 16,   /**< Maximum queued frames */
  k_mock_comm_max_payload_size = 1024, /**< Maximum payload size */
  k_mock_comm_ascii_buf_size   = 2048, /**< ASCII buffer size */
} mock_comm_manager_constants_t;

/* =============================================================================
 * Type Definitions (mirrors rx_comm_manager.h / rx_frame.h)
 * =============================================================================
 */

/**
 * @enum rx_frame_type_t
 * @brief Frame types for communication protocol
 */
typedef enum : uint8_t {
  k_frame_type_ping      = 0x00, /**< Link health check request */
  k_frame_type_pong      = 0x01, /**< Link health check response */
  k_frame_type_command   = 0x10, /**< Command from controller */
  k_frame_type_response  = 0x11, /**< Response from peripheral */
  k_frame_type_ack       = 0x12, /**< Positive acknowledgment (SPI only) */
  k_frame_type_nack      = 0x13, /**< Negative acknowledgment (SPI only) */
  k_frame_type_reset_ack = 0xFE, /**< Session reset acknowledgment */
  k_frame_type_reset     = 0xFF, /**< Session reset request */
} rx_frame_type_t;

/**
 * @enum rx_comm_channel_t
 * @brief Communication channel identifiers
 */
typedef enum : uint8_t {
  k_comm_channel_usb   = 0, /**< USB CDC channel */
  k_comm_channel_spi   = 1, /**< SPI channel */
  k_comm_channel_count = 2, /**< Total channel count */
} rx_comm_channel_t;

/**
 * @struct rx_frame_header_t
 * @brief Frame header structure
 */
typedef struct {
  uint16_t sequence; /**< Frame sequence number */
  uint16_t length;   /**< Payload length */
  uint8_t  type;     /**< Frame type */
  uint8_t  flags;    /**< Control flags */
} rx_frame_header_t;

/**
 * @struct rx_frame_t
 * @brief Complete frame structure
 */
typedef struct {
  rx_frame_header_t header;                                /**< Frame header */
  uint8_t           payload[k_mock_comm_max_payload_size]; /**< Payload data */
  uint32_t          crc;                                   /**< CRC checksum */
} rx_frame_t;

/* Forward declarations for mock */
struct rx_usb_comm_handle_s;
typedef struct rx_usb_comm_handle_s rx_usb_comm_handle_t;

struct rx_spi_comm_handle_s;
typedef struct rx_spi_comm_handle_s rx_spi_comm_handle_t;

/**
 * @typedef rx_comm_frame_callback_t
 * @brief Frame received callback function type
 */
typedef void (*rx_comm_frame_callback_t)(rx_comm_channel_t channel,
                                         const rx_frame_t* frame,
                                         void*             ctx);

/**
 * @struct rx_comm_manager_config_t
 * @brief Communication manager configuration
 */
typedef struct {
  rx_usb_comm_handle_t*    usb_handle;            /**< USB comm handle */
  rx_spi_comm_handle_t*    spi_handle;            /**< SPI comm handle */
  rx_comm_frame_callback_t callback;              /**< Frame callback */
  void*                    callback_ctx;          /**< Callback context */
  bool                     enable_decoded_output; /**< Enable ASCII output */
} rx_comm_manager_config_t;

/**
 * @struct rx_comm_manager_t
 * @brief Communication manager handle
 */
typedef struct {
  rx_usb_comm_handle_t*    usb_handle;                               /**< USB handle */
  rx_spi_comm_handle_t*    spi_handle;                               /**< SPI handle */
  rx_comm_frame_callback_t callback;                                 /**< Callback */
  void*                    callback_ctx;                             /**< Context */
  char                     ascii_buffer[k_mock_comm_ascii_buf_size]; /**< ASCII buf */
  bool                     enable_decoded_output;                    /**< ASCII output */
  bool                     initialized;                              /**< Init flag */
} rx_comm_manager_t;

/**
 * @struct rx_comm_send_params_t
 * @brief Parameters for send operations
 */
typedef struct {
  rx_comm_channel_t channel;     /**< Channel to send on */
  rx_frame_type_t   type;        /**< Frame type */
  uint8_t           flags;       /**< Frame flags */
  const uint8_t*    payload;     /**< Payload data */
  uint32_t          payload_len; /**< Payload length */
} rx_comm_send_params_t;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Reset all mock communication manager state
 *
 * @details
 * Clears all call counts, resets return values to defaults,
 * clears frame queues. Call in test setUp().
 */
void mock_comm_manager_reset(void);

/**
 * @brief Set return value for rx_comm_manager_init()
 *
 * @param[in] err Error code to return
 */
void mock_comm_manager_set_init_return(rx_err_t err);

/**
 * @brief Set return value for rx_comm_manager_deinit()
 *
 * @param[in] err Error code to return
 */
void mock_comm_manager_set_deinit_return(rx_err_t err);

/**
 * @brief Set return value for rx_comm_manager_poll()
 *
 * @param[in] err Error code to return
 */
void mock_comm_manager_set_poll_return(rx_err_t err);

/**
 * @brief Set return value for rx_comm_manager_send()
 *
 * @param[in] err Error code to return
 */
void mock_comm_manager_set_send_return(rx_err_t err);

/**
 * @brief Queue a frame to be delivered on next poll
 *
 * @param[in] channel  Channel the frame arrives on
 * @param[in] type     Frame type
 * @param[in] payload  Payload data
 * @param[in] len      Payload length
 *
 * @return bool True if frame was queued, false if queue full
 */
bool mock_comm_manager_queue_frame(rx_comm_channel_t channel,
                                   rx_frame_type_t   type,
                                   const uint8_t*    payload,
                                   uint32_t          len);

/**
 * @brief Set channel ready status
 *
 * @param[in] channel Channel to set
 * @param[in] ready   Ready status
 */
void mock_comm_manager_set_channel_ready(rx_comm_channel_t channel, bool ready);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

/**
 * @brief Get number of rx_comm_manager_init() calls
 *
 * @return uint32_t Call count
 */
uint32_t mock_comm_manager_get_init_count(void);

/**
 * @brief Get number of rx_comm_manager_deinit() calls
 *
 * @return uint32_t Call count
 */
uint32_t mock_comm_manager_get_deinit_count(void);

/**
 * @brief Get number of rx_comm_manager_poll() calls
 *
 * @return uint32_t Call count
 */
uint32_t mock_comm_manager_get_poll_count(void);

/**
 * @brief Get number of rx_comm_manager_send() calls
 *
 * @return uint32_t Call count
 */
uint32_t mock_comm_manager_get_send_count(void);

/**
 * @brief Get the last channel sent to
 *
 * @return rx_comm_channel_t Last channel
 */
rx_comm_channel_t mock_comm_manager_get_last_send_channel(void);

/**
 * @brief Get the last frame type sent
 *
 * @return rx_frame_type_t Last frame type
 */
rx_frame_type_t mock_comm_manager_get_last_send_type(void);

/**
 * @brief Get the last payload sent
 *
 * @param[out] out_payload Buffer to copy payload to
 * @param[in]  max_len     Maximum length to copy
 *
 * @return uint32_t Actual length copied
 */
uint32_t mock_comm_manager_get_last_send_payload(uint8_t* out_payload, uint32_t max_len);

/**
 * @brief Check if manager was initialized
 *
 * @return bool True if init was called
 */
bool mock_comm_manager_was_initialized(void);

/**
 * @brief Get number of queued frames remaining
 *
 * @return uint32_t Number of frames in queue
 */
uint32_t mock_comm_manager_get_queue_count(void);

/* =============================================================================
 * Mock Communication Manager API (replaces real implementation)
 * =============================================================================
 */

/**
 * @brief Mock rx_comm_manager_init() - Initialize manager
 *
 * @param[out] mgr Pointer to manager handle
 * @param[in]  cfg Configuration
 *
 * @return rx_err_t Configured return value
 */
rx_err_t rx_comm_manager_init(rx_comm_manager_t* mgr, const rx_comm_manager_config_t* cfg);

/**
 * @brief Mock rx_comm_manager_deinit() - Deinitialize manager
 *
 * @param[in,out] mgr Pointer to manager handle
 *
 * @return rx_err_t Configured return value
 */
rx_err_t rx_comm_manager_deinit(rx_comm_manager_t* mgr);

/**
 * @brief Mock rx_comm_manager_poll() - Poll for incoming frames
 *
 * @param[in,out] mgr Pointer to manager handle
 *
 * @return rx_err_t Configured return value
 */
rx_err_t rx_comm_manager_poll(rx_comm_manager_t* mgr);

/**
 * @brief Mock rx_comm_manager_send() - Send frame
 *
 * @param[in,out] mgr    Pointer to manager handle
 * @param[in]     params Send parameters
 *
 * @return rx_err_t Configured return value
 */
rx_err_t rx_comm_manager_send(rx_comm_manager_t* mgr, const rx_comm_send_params_t* params);

/**
 * @brief Mock rx_comm_manager_respond() - Send response
 *
 * @param[in,out] mgr         Pointer to manager handle
 * @param[in]     channel     Channel to respond on
 * @param[in]     payload     Response payload
 * @param[in]     payload_len Payload length
 *
 * @return rx_err_t Configured return value
 */
rx_err_t rx_comm_manager_respond(rx_comm_manager_t* mgr,
                                 rx_comm_channel_t  channel,
                                 const uint8_t*     payload,
                                 uint32_t           payload_len);

/**
 * @brief Mock rx_comm_manager_channel_ready() - Check channel status
 *
 * @param[in]  mgr     Pointer to manager handle
 * @param[in]  channel Channel to check
 * @param[out] ready   Ready status output
 *
 * @return rx_err_t k_rx_ok on success
 */
rx_err_t
rx_comm_manager_channel_ready(rx_comm_manager_t* mgr, rx_comm_channel_t channel, bool* ready);

/**
 * @brief Mock rx_comm_manager_channel_name() - Get channel name
 *
 * @param[in] channel Channel identifier
 *
 * @return const char* Human-readable name
 */
const char* rx_comm_manager_channel_name(rx_comm_channel_t channel);

#ifdef __cplusplus
}
#endif
