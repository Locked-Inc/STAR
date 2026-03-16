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
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Pull in real types so mock functions have compatible signatures */
#include "rx_comm_manager.h"
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
