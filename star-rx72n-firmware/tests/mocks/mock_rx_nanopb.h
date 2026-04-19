/**
 * @file mock_rx_nanopb.h
 * @brief Mock nanopb Protocol Buffers encoding/decoding for comm testing
 *
 * @details
 * Provides test double for nanopb (embedded Protocol Buffers library) to
 * enable unit testing of communication tasks, telemetry encoding, and command
 * decoding without actual protobuf serialization.
 *
 * Enables testing of: Message encoding/decoding logic, Communication protocol
 * error handling, Telemetry data formatting, Command parsing, Buffer overflow
 * protection
 *
 * @par Mock Capabilities: Pre-configured decode results (simulated incoming
 * messages), Encode capture (verify outgoing message content), Error injection
 * (decode failures, buffer overflow), Call tracking
 *
 * @par Usage: tests/test_rx_comm_manager.c, tests/test_telemetry_task.c
 * @see rx_nanopb.h Real nanopb wrapper
 * @see star/v1/ Protocol Buffer schemas (.proto files)
 * @par NASA Power of 10: [OK] Static buffers, bounded message sizes
 * @par SOLID: D - Communication tasks depend on serialization interface
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

#include "rx_err.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum mock_nanopb_constants_t
 * @brief Mock nanopb constants
 */
typedef enum : uint8_t {
  k_mock_nanopb_max_motors   = 4, /**< Number of motors */
  k_mock_nanopb_max_encoders = 4, /**< Number of encoders */
} mock_nanopb_constants_t;

/* =============================================================================
 * Type Definitions (simplified from generated protobuf headers)
 * =============================================================================
 */

/**
 * @struct star_v1_VelocityCommand
 * @brief Velocity command for 4 motors
 */
typedef struct {
  double   front_left_velocity_mps;  /**< Front left motor velocity */
  double   front_right_velocity_mps; /**< Front right motor velocity */
  double   back_left_velocity_mps;   /**< Back left motor velocity */
  double   back_right_velocity_mps;  /**< Back right motor velocity */
  uint32_t sequence;                 /**< Command sequence number */
} star_v1_VelocityCommand;

/**
 * @struct star_v1_SetVelocityRequest
 * @brief Set velocity request message
 */
typedef struct {
  bool                    has_command; /**< Command field present */
  star_v1_VelocityCommand command;     /**< Velocity command */
} star_v1_SetVelocityRequest;

/**
 * @struct star_v1_EmergencyStopRequest
 * @brief Emergency stop request message
 */
typedef struct {
  bool     reason_present; /**< Reason field present */
  uint32_t reason;         /**< E-stop reason code */
} star_v1_EmergencyStopRequest;

/**
 * @struct star_v1_EncoderData
 * @brief Encoder data for telemetry
 */
typedef struct {
  uint32_t motor_id;     /**< Motor index */
  int64_t  ticks;        /**< Encoder tick count */
  double   velocity_mps; /**< Calculated velocity */
  int64_t  timestamp_us; /**< Timestamp */
} star_v1_EncoderData;

/**
 * @struct star_v1_TelemetryData
 * @brief Telemetry data message
 */
typedef struct {
  int64_t  timestamp_us;   /**< System timestamp */
  uint32_t frame_sequence; /**< Telemetry frame sequence */
  bool     emergency_stop; /**< E-stop active flag */
  uint32_t fault_flags;    /**< Fault flags bitfield */

  bool                has_encoder_front_left; /**< Has FL encoder */
  star_v1_EncoderData encoder_front_left;     /**< FL encoder data */

  bool                has_encoder_front_right; /**< Has FR encoder */
  star_v1_EncoderData encoder_front_right;     /**< FR encoder data */

  bool                has_encoder_back_left; /**< Has BL encoder */
  star_v1_EncoderData encoder_back_left;     /**< BL encoder data */

  bool                has_encoder_back_right; /**< Has BR encoder */
  star_v1_EncoderData encoder_back_right;     /**< BR encoder data */

  double temperature_celsius; /**< Ambient temperature (degC) */
} star_v1_TelemetryData;

/**
 * @brief Zero-initializer for TelemetryData
 */
#define star_v1_TelemetryData_init_zero                                                            \
  {0, 0, false, 0, false, {0}, false, {0}, false, {0}, false, {0}, 0.0}

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Reset all mock nanopb state
 *
 * @details
 * Clears all call counts, resets return values to defaults,
 * clears decoded output configuration. Call in test setUp().
 */
void mock_nanopb_reset(void);

/**
 * @brief Set return value for rx_nanopb_init()
 *
 * @param[in] err Error code to return
 */
void mock_nanopb_set_init_return(rx_err_t err);

/**
 * @brief Set return value for rx_nanopb_decode_velocity_request()
 *
 * @param[in] err Error code to return
 */
void mock_nanopb_set_decode_velocity_return(rx_err_t err);

/**
 * @brief Set the velocity values that decode will return
 *
 * @param[in] fl Front left velocity (m/s)
 * @param[in] fr Front right velocity (m/s)
 * @param[in] bl Back left velocity (m/s)
 * @param[in] br Back right velocity (m/s)
 */
void mock_nanopb_set_decoded_velocity(float fl, float fr, float bl, float br);

/**
 * @brief Set return value for rx_nanopb_decode_estop_request()
 *
 * @param[in] err Error code to return
 */
void mock_nanopb_set_decode_estop_return(rx_err_t err);

/**
 * @brief Set return value for rx_nanopb_encode_telemetry()
 *
 * @param[in] err Error code to return
 */
void mock_nanopb_set_encode_telemetry_return(rx_err_t err);

/**
 * @brief Set the encoded length that encode functions will return
 *
 * @param[in] len Encoded length in bytes
 */
void mock_nanopb_set_encode_length(uint32_t len);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

/**
 * @brief Get number of rx_nanopb_init() calls
 *
 * @return uint32_t Call count
 */
uint32_t mock_nanopb_get_init_count(void);

/**
 * @brief Get number of rx_nanopb_decode_velocity_request() calls
 *
 * @return uint32_t Call count
 */
uint32_t mock_nanopb_get_decode_velocity_count(void);

/**
 * @brief Get number of rx_nanopb_decode_estop_request() calls
 *
 * @return uint32_t Call count
 */
uint32_t mock_nanopb_get_decode_estop_count(void);

/**
 * @brief Get number of rx_nanopb_encode_telemetry() calls
 *
 * @return uint32_t Call count
 */
uint32_t mock_nanopb_get_encode_telemetry_count(void);

/**
 * @brief Get the last telemetry data that was encoded
 *
 * @param[out] out_telemetry Pointer to store telemetry data
 *
 * @return bool True if telemetry data was captured
 */
bool mock_nanopb_get_last_telemetry(star_v1_TelemetryData* out_telemetry);

/**
 * @brief Check if nanopb was initialized
 *
 * @return bool True if init was called
 */
bool mock_nanopb_was_initialized(void);

/* =============================================================================
 * Mock nanopb API (replaces real implementation)
 * =============================================================================
 */

/**
 * @brief Mock rx_nanopb_init() - Initialize nanopb wrapper
 *
 * @return rx_err_t Configured return value
 */
rx_err_t rx_nanopb_init(void);

/**
 * @brief Mock rx_nanopb_decode_velocity_request()
 *
 * @param[in]  buffer Input buffer (unused in mock)
 * @param[in]  len    Buffer length (unused in mock)
 * @param[out] msg    Output message structure
 *
 * @return rx_err_t Configured return value
 */
rx_err_t rx_nanopb_decode_velocity_request(const uint8_t*              buffer,
                                           uint32_t                    len,
                                           star_v1_SetVelocityRequest* msg);

/**
 * @brief Mock rx_nanopb_decode_estop_request()
 *
 * @param[in]  buffer Input buffer (unused in mock)
 * @param[in]  len    Buffer length (unused in mock)
 * @param[out] msg    Output message structure
 *
 * @return rx_err_t Configured return value
 */
rx_err_t rx_nanopb_decode_estop_request(const uint8_t*                buffer,
                                        uint32_t                      len,
                                        star_v1_EmergencyStopRequest* msg);

/**
 * @brief Mock rx_nanopb_encode_telemetry()
 *
 * @param[in]  msg         Input telemetry data
 * @param[out] buffer      Output buffer
 * @param[in]  buffer_size Buffer size
 * @param[out] len         Encoded length
 *
 * @return rx_err_t Configured return value
 */
rx_err_t rx_nanopb_encode_telemetry(const star_v1_TelemetryData* msg,
                                    uint8_t*                     buffer,
                                    uint32_t                     buffer_size,
                                    uint32_t*                    len);

/**
 * @brief Mock rx_nanopb_test_reset_state() - Reset for testing
 */
void rx_nanopb_test_reset_state(void);

#ifdef __cplusplus
}
#endif
