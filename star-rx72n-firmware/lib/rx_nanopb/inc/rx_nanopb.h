/* lib/rx_nanopb/inc/rx_nanopb.h */

/**
 * @file rx_nanopb.h
 * @brief nanopb Integration Wrapper for RX72N
 * @details
 * Provides simplified encode/decode functions with static buffers
 * for protocol buffer messages used in the STAR project.
 *
 * @see star-proto/proto/star/v1/ for message definitions
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_NANOPB_H
#define STAR_RX_NANOPB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"

/* Include generated protobuf headers */
#include "gen/star/v1/common.pb.h"
#include "gen/star/v1/motor_control.pb.h"
#include "gen/star/v1/telemetry.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Buffer Configuration
 * =============================================================================
 */

/**
 * @brief nanopb buffer configuration
 */
typedef enum {
  k_nanopb_buffer_size = 512, /**< Maximum encode/decode buffer size */
} rx_nanopb_params_t;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize nanopb wrapper
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_state if already initialized
 */
rx_err_t rx_nanopb_init(void);

/* =============================================================================
 * SetVelocityRequest Encode/Decode
 * =============================================================================
 */

/**
 * @brief Encode SetVelocityRequest to bytes
 *
 * @param[in]  msg    Message to encode
 * @param[out] buffer Output buffer (at least k_nanopb_buffer_size bytes)
 * @param[out] len    Actual encoded length
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is NULL
 * @return k_rx_err_invalid_state if rx_nanopb_init() not called
 * @return k_rx_err_invalid_size if encoding fails or length exceeds buffer
 */
rx_err_t rx_nanopb_encode_velocity_request(const star_v1_SetVelocityRequest* msg,
                                           uint8_t*                          buffer,
                                           size_t                            buffer_size,
                                           uint32_t*                         len);

/**
 * @brief Decode SetVelocityRequest from bytes
 *
 * @param[in]  buffer Input buffer
 * @param[in]  len    Buffer length
 * @param[out] msg    Decoded message
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if any pointer is NULL or len is invalid
 * @return k_rx_err_invalid_state if rx_nanopb_init() not called
 * @return k_rx_err_protocol_error if decoding fails
 */
rx_err_t rx_nanopb_decode_velocity_request(const uint8_t*              buffer,
                                           uint32_t                    len,
                                           star_v1_SetVelocityRequest* msg);

/* =============================================================================
 * SetVelocityResponse Encode/Decode
 * =============================================================================
 */

/**
 * @brief Encode SetVelocityResponse to bytes
 *
 * @param[in]  msg    Message to encode
 * @param[out] buffer Output buffer
 * @param[in]  buffer_size Size of the output buffer
 * @param[out] len    Actual encoded length
 *
 * @return k_rx_ok on success
 */
rx_err_t rx_nanopb_encode_velocity_response(const star_v1_SetVelocityResponse* msg,
                                            uint8_t*                           buffer,
                                            size_t                             buffer_size,
                                            uint32_t*                          len);

/* =============================================================================
 * EmergencyStopRequest Encode/Decode
 * =============================================================================
 */

/**
 * @brief Decode EmergencyStopRequest from bytes
 *
 * @param[in]  buffer Input buffer
 * @param[in]  len    Buffer length
 * @param[out] msg    Decoded message
 *
 * @return k_rx_ok on success
 */
rx_err_t rx_nanopb_decode_estop_request(const uint8_t*                buffer,
                                        uint32_t                      len,
                                        star_v1_EmergencyStopRequest* msg);

/**
 * @brief Encode EmergencyStopResponse to bytes
 *
 * @param[in]  msg    Message to encode
 * @param[out] buffer Output buffer
 * @param[in]  buffer_size Size of the output buffer
 * @param[out] len    Actual encoded length
 *
 * @return k_rx_ok on success
 */
rx_err_t rx_nanopb_encode_estop_response(const star_v1_EmergencyStopResponse* msg,
                                         uint8_t*                             buffer,
                                         size_t                               buffer_size,
                                         uint32_t*                            len);

/* =============================================================================
 * Telemetry Encode/Decode
 * =============================================================================
 */

/**
 * @brief Encode TelemetryData to bytes
 *
 * @param[in]  msg    Message to encode
 * @param[out] buffer Output buffer
 * @param[in]  buffer_size Size of the output buffer
 * @param[out] len    Actual encoded length
 *
 * @return k_rx_ok on success
 */
rx_err_t
rx_nanopb_encode_telemetry(const star_v1_TelemetryData* msg, uint8_t* buffer, size_t buffer_size, uint32_t* len);

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @brief Create VelocityCommand with given velocities
 *
 * @param[out] cmd Output command structure
 * @param[in]  left_mps Left wheel velocity (m/s)
 * @param[in]  right_mps Right wheel velocity (m/s)
 * @param[in]  sequence Command sequence number
 */
void rx_nanopb_create_velocity_command(star_v1_VelocityCommand* cmd,
                                       double                   left_mps,
                                       double                   right_mps,
                                       uint32_t                 sequence);

/**
 * @brief Create ResponseHeader with status
 *
 * @param[out] header Output header structure
 * @param[in]  status Response status
 * @param[in]  request_id Original request ID (can be NULL)
 */
void rx_nanopb_create_response_header(star_v1_ResponseHeader* header,
                                      star_v1_Status          status,
                                      const char*             request_id);

#ifdef UNIT_TEST
/**
 * @brief Reset nanopb internal state for unit testing
 */
void rx_nanopb_test_reset_state(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_NANOPB_H */
