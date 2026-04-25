/**
 * @file rx_nanopb.h
 * @brief nanopb Integration Wrapper for RX72N
 *
 * @details
 * Provides simplified encode/decode functions with static buffers for Protocol
 * Buffer messages used in the STAR project. Wraps the nanopb library with
 * RX72N-friendly APIs that handle memory constraints and error reporting.
 *
 * @par System Architecture
 * @dot
 * digraph nanopb_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_app {
 *     label="Application Layer";
 *     style=filled;
 *     color=lightyellow;
 *     motor [label="Motor Control\nTask"];
 *     telemetry [label="Telemetry\nTask"];
 *   }
 *
 *   subgraph cluster_nanopb {
 *     label="nanopb Wrapper (This Module)";
 *     style=filled;
 *     color=lightblue;
 *     encode [label="rx_nanopb_encode_*()"];
 *     decode [label="rx_nanopb_decode_*()"];
 *     helpers [label="Helper Functions\n(create_velocity_command)"];
 *   }
 *
 *   subgraph cluster_proto {
 *     label="Generated Protobuf Code";
 *     style=filled;
 *     color=lightgreen;
 *     common [label="common.pb.h"];
 *     motor_pb [label="motor_control.pb.h"];
 *     telem_pb [label="telemetry.pb.h"];
 *   }
 *
 *   subgraph cluster_comm {
 *     label="Communication Layer";
 *     style=filled;
 *     color=lightcoral;
 *     spi [label="SPI Comm\n(to RPi5)"];
 *     usb [label="USB CDC\n(debug)"];
 *   }
 *
 *   motor -> encode;
 *   motor -> decode;
 *   telemetry -> encode;
 *   encode -> common;
 *   encode -> motor_pb;
 *   encode -> telem_pb;
 *   decode -> common;
 *   decode -> motor_pb;
 *   helpers -> motor_pb;
 *   encode -> spi;
 *   decode -> spi;
 *   encode -> usb;
 * }
 * @enddot
 *
 * @par Message Flow
 * @msc
 * RPi5, SPI, RX72N_Comm, nanopb, Motor;
 *
 * RPi5 -> SPI [label="SetVelocityRequest (encoded)"];
 * SPI -> RX72N_Comm [label="Raw bytes"];
 * RX72N_Comm -> nanopb [label="rx_nanopb_decode_velocity_request()"];
 * nanopb -> Motor [label="star_v1_SetVelocityRequest"];
 * Motor -> Motor [label="Apply velocity"];
 * Motor -> nanopb [label="star_v1_SetVelocityResponse"];
 * nanopb -> RX72N_Comm [label="rx_nanopb_encode_velocity_response()"];
 * RX72N_Comm -> SPI [label="Raw bytes"];
 * SPI -> RPi5 [label="SetVelocityResponse (encoded)"];
 * @endmsc
 *
 * @par Supported Message Types
 * | Message | Direction | Description |
 * |---------|-----------|-------------|
 * | SetVelocityRequest | RPi5 -> RX72N | Motor velocity commands |
 * | SetVelocityResponse | RX72N -> RPi5 | Command acknowledgment |
 * | EmergencyStopRequest | RPi5 -> RX72N | Emergency stop command |
 * | EmergencyStopResponse | RX72N -> RPi5 | E-stop acknowledgment |
 * | TelemetryData | RX72N -> RPi5 | Sensor/motor telemetry |
 *
 * @par Buffer Configuration
 * | Parameter | Value | Notes |
 * |-----------|-------|-------|
 * | Max buffer size | 512 bytes | Static allocation |
 * | Max message size | ~256 bytes | Largest single message |
 * | Alignment | 4-byte | For DMA compatibility |
 *
 * @par Memory Usage
 * | Component | Size | Notes |
 * |-----------|------|-------|
 * | Code (.text) | ~3 KB | All encode/decode functions |
 * | Static buffer | 512 bytes | Shared encode buffer |
 * | Stack usage | ~128 bytes | Per encode/decode call |
 * | Message struct | 64-128 bytes | On caller stack |
 *
 * @par Performance Characteristics
 * | Operation | Time @ 240 MHz | Notes |
 * |-----------|----------------|-------|
 * | Encode velocity | ~20 us | 4-motor command |
 * | Decode velocity | ~15 us | 4-motor command |
 * | Encode telemetry | ~30 us | Full telemetry packet |
 * | Buffer copy | ~5 us | 256 bytes |
 *
 * @par Wire Format
 * nanopb uses standard Protocol Buffer encoding:
 * - Variable-length integers (varint)
 * - Length-delimited fields for strings/bytes
 * - Little-endian for fixed-width types
 * - No alignment padding
 *
 * @par Error Handling
 * | Error | Cause | Recovery |
 * |-------|-------|----------|
 * | k_rx_err_invalid_size | Buffer too small | Use larger buffer |
 * | k_rx_err_protocol_error | Malformed message | Discard, request retransmit |
 * | k_rx_err_not_initialized | Module not init'd | Call rx_nanopb_init() |
 *
 * @par Thread Safety
 * - Encode/decode functions: **NOT thread-safe** (shared buffer)
 * - Helper functions: Thread-safe (no shared state)
 * - Call from single task or protect with mutex
 *
 * @par Module Dependencies
 * - nanopb: Core protobuf library (git submodule)
 * - gen/star/v1/<*>.pb.h: Generated message headers
 * - rx_err.h: Error code definitions
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] All loops bounded by message field counts
 * - Rule 3: [OK] No dynamic memory (static buffers only)
 * - Rule 4: [OK] Functions under 60 lines
 * - Rule 5: [OK] Input validation on all parameters
 * - Rule 6: [OK] Variables at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Limited preprocessor (nanopb macros only)
 * - Rule 9: [OK] Callbacks only for nanopb field encoding
 * - Rule 10: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single Responsibility - Protobuf encode/decode only
 * - **O**: Open/Closed - Extensible by adding new message types
 * - **L**: Liskov Substitution - N/A (no inheritance)
 * - **I**: Interface Segregation - Separate function per message type
 * - **D**: Dependency Inversion - Depends on generated message headers
 *
 * @see star-proto/proto/star/v1/ Protocol Buffer schema definitions
 * @see https://jpa.kapsi.fi/nanopb/ nanopb documentation
 *
 * @version 1.0.0
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"

/* Include generated protobuf headers */
#include "gen/star/v1/common.pb.h"
#include "gen/star/v1/configuration.pb.h"
#include "gen/star/v1/gateway_service.pb.h"
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
 * @var s_nanopb_buffer_size
 * @brief Maximum encode/decode buffer size in bytes
 *
 * @details
 * Defines the static buffer size used for Protocol Buffer encoding/decoding.
 * All messages must fit within this buffer. Size chosen based on:
 * - Largest message: TelemetryData (~200 bytes encoded)
 * - Margin for future expansion
 * - Memory constraints on RX72N
 *
 * @par Size Rationale
 * | Message Type | Typical Size | Max Size |
 * |--------------|--------------|----------|
 * | SetVelocityRequest | 48 bytes | 64 bytes |
 * | SetVelocityResponse | 24 bytes | 32 bytes |
 * | TelemetryData | 180 bytes | 256 bytes |
 * | EmergencyStop* | 16 bytes | 24 bytes |
 *
 * @note 512 bytes provides 2x margin over largest message
 * @warning Do not reduce below 256 bytes
 *
 * @since Version 1.0.0
 */
static const uint16_t s_nanopb_buffer_size = 512U;

/* =============================================================================
 * Initialization
 * =============================================================================
 */

/**
 * @brief Initialize nanopb wrapper module
 *
 * @details
 * Initializes the nanopb wrapper state and verifies the module is ready
 * for encode/decode operations. Must be called once before any encode/decode
 * function. Typically called during system startup.
 *
 * Initialization steps:
 * 1. Clear internal state
 * 2. Initialize static buffers
 * 3. Verify nanopb library version
 * 4. Set initialized flag
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, module ready for use
 * @retval k_rx_err_invalid_state Already initialized (call rx_nanopb_test_reset_state() first)
 * @retval k_rx_err_generic Internal verification failed
 *
 * @pre None (first function to call)
 * @post Module ready for encode/decode operations
 *
 * @note Thread-safe (initialization is idempotent after first call)
 * @note Safe to call multiple times (returns k_rx_err_invalid_state)
 *
 * @par Example:
 * @code
 * void system_init(void) {
 *     rx_err_t err = rx_nanopb_init();
 *     if (err != k_rx_ok) {
 *         // Handle initialization failure
 *         system_halt("nanopb init failed");
 *     }
 *     // Now safe to use encode/decode functions
 * }
 * @endcode
 *
 * @see rx_nanopb_test_reset_state() Reset for unit testing
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_nanopb_init(void);

/* =============================================================================
 * SetVelocityRequest Encode/Decode
 * =============================================================================
 */

/**
 * @brief Encode SetVelocityRequest message to Protocol Buffer bytes
 *
 * @details
 * Serializes a SetVelocityRequest message containing motor velocity commands
 * into Protocol Buffer wire format. Used when RX72N needs to forward or
 * echo velocity commands.
 *
 * @par Message Structure
 * ```protobuf
 * message SetVelocityRequest {
 *   RequestHeader header = 1;
 *   VelocityCommand command = 2;
 * }
 *
 * message VelocityCommand {
 *   double front_left_mps = 1;
 *   double front_right_mps = 2;
 *   double back_left_mps = 3;
 *   double back_right_mps = 4;
 *   uint32 sequence = 5;
 * }
 * ```
 *
 * @param[in] msg Message to encode
 *   - Must be fully populated
 *   - Velocities in m/s (typically -2.0 to +2.0)
 * @param[out] buffer Output buffer for encoded bytes
 *   - Must be at least s_nanopb_buffer_size bytes
 *   - Receives wire-format data
 * @param[in] buffer_size Size of output buffer in bytes
 *   - Must be >= s_nanopb_buffer_size (512)
 * @param[out] len Actual encoded message length
 *   - Typically 40-60 bytes for velocity command
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, buffer contains encoded message
 * @retval k_rx_err_invalid_arg nullptr in msg, buffer, or len
 * @retval k_rx_err_not_initialized Module not initialized
 * @retval k_rx_err_invalid_size Buffer too small or encoding failed
 *
 * @pre rx_nanopb_init() must be called first
 * @pre msg must be fully populated
 * @pre buffer_size >= s_nanopb_buffer_size
 *
 * @post buffer contains wire-format Protocol Buffer data
 * @post len contains actual encoded length
 *
 * @note Not thread-safe (uses shared internal buffer)
 *
 * @par Performance: ~20 us @ 240 MHz
 *
 * @see rx_nanopb_decode_velocity_request() Decode counterpart
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_nanopb_encode_velocity_request(const star_v1_SetVelocityRequest* msg,
                                                         uint8_t*                          buffer,
                                                         uint32_t  buffer_size,
                                                         uint32_t* len);

/**
 * @brief Decode SetVelocityRequest message from Protocol Buffer bytes
 *
 * @details
 * Deserializes a SetVelocityRequest message received from RPi5 over SPI.
 * This is the primary command message for motor velocity control.
 *
 * @par Decoding Process
 * 1. Validate buffer pointer and length
 * 2. Create nanopb input stream from buffer
 * 3. Decode header and command fields
 * 4. Validate decoded values are within range
 *
 * @param[in] buffer Input buffer containing encoded message
 *   - Wire-format Protocol Buffer data
 *   - Received from SPI or USB
 * @param[in] len Buffer length in bytes
 *   - Must match actual encoded message size
 * @param[out] msg Decoded message structure
 *   - Will be fully populated on success
 *   - Velocities in m/s
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, msg contains decoded values
 * @retval k_rx_err_invalid_arg nullptr or len == 0
 * @retval k_rx_err_not_initialized Module not initialized
 * @retval k_rx_err_protocol_error Malformed message, CRC error, or invalid field
 *
 * @pre rx_nanopb_init() must be called first
 * @pre buffer contains valid wire-format data
 *
 * @post msg fully populated with decoded values
 *
 * @note Not thread-safe (uses shared internal state)
 *
 * @par Performance: ~15 us @ 240 MHz
 *
 * @par Example:
 * @code
 * star_v1_SetVelocityRequest request;
 * rx_err_t err = rx_nanopb_decode_velocity_request(spi_buffer, spi_len, &request);
 * if (err == k_rx_ok) {
 *     // Apply velocities to motors
 *     motor_set_velocity(0, request.command.front_left_mps);
 *     motor_set_velocity(1, request.command.front_right_mps);
 *     // ...
 * } else if (err == k_rx_err_protocol_error) {
 *     // Request retransmit
 *     send_nack();
 * }
 * @endcode
 *
 * @see rx_nanopb_encode_velocity_request() Encode counterpart
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_nanopb_decode_velocity_request(const uint8_t*              buffer,
                                                         uint32_t                    len,
                                                         star_v1_SetVelocityRequest* msg);

/* =============================================================================
 * SetVelocityResponse Encode/Decode
 * =============================================================================
 */

/**
 * @brief Encode SetVelocityResponse message to Protocol Buffer bytes
 *
 * @details
 * Serializes a SetVelocityResponse message as acknowledgment to a velocity
 * command. Sent back to RPi5 after processing SetVelocityRequest.
 *
 * @par Message Structure
 * ```protobuf
 * message SetVelocityResponse {
 *   ResponseHeader header = 1;
 *   // Status in header indicates success/failure
 * }
 * ```
 *
 * @param[in] msg Message to encode
 *   - header.status: k_star_v1_status_ok or error code
 *   - header.request_id: Echo of request ID
 * @param[out] buffer Output buffer for encoded bytes
 * @param[in] buffer_size Size of output buffer (>= s_nanopb_buffer_size)
 * @param[out] len Actual encoded message length (typically 20-30 bytes)
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, buffer contains encoded message
 * @retval k_rx_err_invalid_arg nullptr in parameters
 * @retval k_rx_err_not_initialized Module not initialized
 * @retval k_rx_err_invalid_size Buffer too small
 *
 * @pre rx_nanopb_init() must be called first
 * @post buffer contains wire-format response
 *
 * @par Performance: ~15 us @ 240 MHz
 *
 * @see rx_nanopb_decode_velocity_request() Decode the incoming request
 * @see rx_nanopb_create_response_header() Create header with status
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_nanopb_encode_velocity_response(const star_v1_SetVelocityResponse* msg,
                                                          uint8_t*                           buffer,
                                                          uint32_t  buffer_size,
                                                          uint32_t* len);

/* =============================================================================
 * EmergencyStopRequest Encode/Decode
 * =============================================================================
 */

/**
 * @brief Decode EmergencyStopRequest message from Protocol Buffer bytes
 *
 * @details
 * Deserializes an emergency stop command from RPi5. This is a safety-critical
 * message that must be processed with highest priority.
 *
 * @par E-Stop Processing
 * 1. Decode message (this function)
 * 2. Immediately stop all motors
 * 3. Enter safe state
 * 4. Send EmergencyStopResponse
 *
 * @param[in] buffer Input buffer containing encoded message
 * @param[in] len Buffer length in bytes
 * @param[out] msg Decoded message structure
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, msg contains decoded values
 * @retval k_rx_err_invalid_arg nullptr or len == 0
 * @retval k_rx_err_not_initialized Module not initialized
 * @retval k_rx_err_protocol_error Malformed message
 *
 * @warning E-stop must be processed immediately regardless of return value
 * @note Consider stopping motors first, then decode for response
 *
 * @par Performance: ~10 us @ 240 MHz (minimal message)
 *
 * @see rx_nanopb_encode_estop_response() Send acknowledgment
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_nanopb_decode_estop_request(const uint8_t*                buffer,
                                                      uint32_t                      len,
                                                      star_v1_EmergencyStopRequest* msg);

/**
 * @brief Encode EmergencyStopResponse message to Protocol Buffer bytes
 *
 * @details
 * Serializes an emergency stop response as acknowledgment. Confirms
 * that the RX72N has processed the E-stop and entered safe state.
 *
 * @param[in] msg Message to encode
 *   - header.status: Indicates if E-stop was successful
 *   - Typically always returns success (motors stopped)
 * @param[out] buffer Output buffer for encoded bytes
 * @param[in] buffer_size Size of output buffer (>= s_nanopb_buffer_size)
 * @param[out] len Actual encoded message length
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, buffer contains encoded response
 * @retval k_rx_err_invalid_arg nullptr in parameters
 * @retval k_rx_err_not_initialized Module not initialized
 * @retval k_rx_err_invalid_size Buffer too small
 *
 * @par Performance: ~10 us @ 240 MHz
 *
 * @see rx_nanopb_decode_estop_request() Decode incoming E-stop
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_nanopb_encode_estop_response(const star_v1_EmergencyStopResponse* msg,
                                                       uint8_t*                             buffer,
                                                       uint32_t  buffer_size,
                                                       uint32_t* len);

/* =============================================================================
 * SetPidGainsRequest Encode/Decode
 * =============================================================================
 */

/**
 * @brief Decode SetPidGainsRequest message from Protocol Buffer bytes
 *
 * @details
 * Deserializes a PID gains update command from RPi5 over SPI. This message
 * contains new PID controller gains (Kp, Ki, Kd) and output/integral limits
 * to be applied to one or more motors.
 *
 * @par PID Configuration Structure
 * The decoded message contains:
 * - Kp (proportional gain)
 * - Ki (integral gain)
 * - Kd (derivative gain)
 * - output_min_percent (minimum PWM duty cycle %)
 * - output_max_percent (maximum PWM duty cycle %)
 * - integral_min (anti-windup minimum)
 * - integral_max (anti-windup maximum)
 * - motor_id (0-3 for specific motor, -1 for all motors)
 *
 * @par Decoding Process
 * 1. Validate buffer pointer and length
 * 2. Create nanopb input stream from buffer
 * 3. Decode header and pid_config fields
 * 4. Extract motor_id for targeting
 *
 * @param[in] buffer Input buffer containing encoded message
 *   - Wire-format Protocol Buffer data
 *   - Received from SPI or USB
 * @param[in] len Buffer length in bytes
 *   - Must match actual encoded message size
 * @param[out] msg Decoded message structure
 *   - Will be fully populated on success
 *   - PID gains in standard units
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, msg contains decoded values
 * @retval k_rx_err_invalid_arg nullptr or len == 0
 * @retval k_rx_err_not_initialized Module not initialized
 * @retval k_rx_err_protocol_error Malformed message or invalid field
 *
 * @pre rx_nanopb_init() must be called first
 * @pre buffer contains valid wire-format data
 *
 * @post msg fully populated with decoded PID configuration
 *
 * @note Not thread-safe (uses shared internal state)
 *
 * @par Performance: ~18 us @ 240 MHz
 *
 * @par Example:
 * @code
 * star_v1_SetPidGainsRequest request;
 * rx_err_t err = rx_nanopb_decode_pid_gains_request(spi_buffer, spi_len, &request);
 * if (err == k_rx_ok && request.has_pid_config) {
 *     // Apply gains to motors
 *     pid_gains_t gains = {
 *         .kp = (float)request.pid_config.kp,
 *         .ki = (float)request.pid_config.ki,
 *         .kd = (float)request.pid_config.kd,
 *         .output_min = (float)request.pid_config.output_min_percent,
 *         .output_max = (float)request.pid_config.output_max_percent,
 *         .integral_min = (float)request.pid_config.integral_min,
 *         .integral_max = (float)request.pid_config.integral_max,
 *         .update_pending = true
 *     };
 *     shared_data_set_pid_gains(&gains);
 * } else if (err == k_rx_err_protocol_error) {
 *     // Request retransmit
 *     send_nack();
 * }
 * @endcode
 *
 * @see rx_nanopb_encode_pid_gains_response() Send acknowledgment
 * @see shared_data_set_pid_gains() Apply gains to motor controllers
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_nanopb_decode_pid_gains_request(const uint8_t*              buffer,
                                                          uint32_t                    len,
                                                          star_v1_SetPidGainsRequest* msg);

/**
 * @brief Encode SetPidGainsResponse message to Protocol Buffer bytes
 *
 * @details
 * Serializes a SetPidGainsResponse message as acknowledgment to a PID gains
 * update command. Sent back to RPi5 after processing a SetPidGainsRequest.
 *
 * The response indicates whether the gains were successfully written to
 * shared_data for the Motor Control Task to apply. The optional human-readable
 * message field is left empty (NULL callback); the success boolean is
 * sufficient for programmatic use.
 *
 * @param[in] msg Message to encode (must not be nullptr)
 *   - has_header: must be true for header to be encoded
 *   - header.status: k_star_v1_status_ok or error status
 *   - header.request_id: echoed from the original request
 *   - success: true if gains were written to shared_data
 * @param[out] buffer Output buffer for encoded bytes (must not be nullptr)
 * @param[in] buffer_size Size of output buffer in bytes
 *   - Must be >= s_nanopb_buffer_size (512 bytes)
 * @param[out] len Actual encoded message length in bytes (must not be nullptr)
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, buffer contains wire-format response
 * @retval k_rx_err_invalid_arg nullptr in msg, buffer, or len
 * @retval k_rx_err_not_initialized Module not initialized via rx_nanopb_init()
 * @retval k_rx_err_invalid_size buffer_size too small or encoded length overflow
 *
 * @pre rx_nanopb_init() must be called before this function
 * @pre buffer must point to at least buffer_size bytes of writable memory
 * @post On k_rx_ok: buffer[0..*len-1] contains valid wire-format response
 * @post On k_rx_ok: *len <= s_nanopb_buffer_size
 *
 * @note Not thread-safe; call only from Communication Task context
 *
 * @par Performance: ~15 us @ 240 MHz
 *
 * @see rx_nanopb_decode_pid_gains_request() Decode the incoming request
 * @see rx_nanopb_create_response_header() Create header with status and request_id
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_nanopb_encode_pid_gains_response(const star_v1_SetPidGainsResponse* msg,
                                                           uint8_t*  buffer,
                                                           uint32_t  buffer_size,
                                                           uint32_t* len);

/**
 * @brief Decode SetRetransmitConfigRequest from Protocol Buffer bytes
 *
 * @details
 * Decodes a serialized SetRetransmitConfigRequest message from a byte buffer.
 * Used to process RPi5 commands that configure SPI retransmission parameters.
 *
 * @param[in] buffer Raw protobuf bytes to decode
 *   - **Valid range**: Non-nullptr to protobuf-encoded data
 *   - **Max size**: s_nanopb_buffer_size (1024 bytes)
 *
 * @param[in] len Length of buffer in bytes
 *   - **Valid range**: [1, s_nanopb_buffer_size]
 *
 * @param[out] msg Decoded message output
 *   - **Valid range**: Non-nullptr to star_v1_SetRetransmitConfigRequest
 *   - **Output**: Zero-initialized then populated from buffer
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Decode successful
 * @retval k_rx_err_invalid_arg nullptr or invalid length
 * @retval k_rx_err_not_initialized Module not initialized
 * @retval k_rx_err_protocol_error Protobuf decode failed
 *
 * @pre buffer and msg must be non-NULL
 * @pre rx_nanopb_init() must have been called
 * @post msg populated with decoded fields on success
 *
 * @see rx_spi_comm_set_auto_retransmit() Apply decoded config
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_nanopb_decode_retransmit_config_request(const uint8_t*                      buffer,
                                           uint32_t                            len,
                                           star_v1_SetRetransmitConfigRequest* msg);

/* =============================================================================
 * Telemetry Encode/Decode
 * =============================================================================
 */

/**
 * @brief Encode TelemetryData message to Protocol Buffer bytes
 *
 * @details
 * Serializes telemetry data for transmission to RPi5. Contains motor states,
 * encoder readings, and sensor data. Sent periodically
 * (typically 10-100 Hz).
 *
 * @par Telemetry Contents
 * | Field | Type | Units | Update Rate |
 * |-------|------|-------|-------------|
 * | Motor velocities | double x 4 | m/s | 100 Hz |
 * | Motor currents | double x 4 | A | 100 Hz |
 * | Encoder positions | int32 x 4 | ticks | 100 Hz |
 * | Temperature | double | degC | 1 Hz |
 *
 * @param[in] msg Message to encode (fully populated telemetry)
 * @param[out] buffer Output buffer for encoded bytes
 * @param[in] buffer_size Size of output buffer (>= s_nanopb_buffer_size)
 * @param[out] len Actual encoded message length (typically 150-200 bytes)
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, buffer contains encoded telemetry
 * @retval k_rx_err_invalid_arg nullptr in parameters
 * @retval k_rx_err_not_initialized Module not initialized
 * @retval k_rx_err_invalid_size Buffer too small
 *
 * @par Performance: ~30 us @ 240 MHz (full telemetry)
 *
 * @par Example:
 * @code
 * star_v1_TelemetryData telemetry = {};
 *
 * // Populate motor data
 * telemetry.motor_front_left_velocity_mps = motor_get_velocity(0);
 * telemetry.motor_front_left_current_a = motor_get_current(0);
 * // ... (populate all fields)
 *
 * uint8_t buffer[512];
 * uint32_t len;
 * rx_err_t err = rx_nanopb_encode_telemetry(&telemetry, buffer, sizeof(buffer), &len);
 * if (err == k_rx_ok) {
 *     spi_send(buffer, len);
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_nanopb_encode_telemetry(const star_v1_TelemetryData* msg,
                                                  uint8_t*                     buffer,
                                                  uint32_t                     buffer_size,
                                                  uint32_t*                    len);

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @struct rx_velocity_command_params_t
 * @brief VelocityCommand parameters for 4 independent motors
 *
 * @details
 * Convenience structure for creating velocity commands with all four motors
 * independently controlled. Maps directly to the protobuf VelocityCommand.
 *
 * @par Motor Layout (Top View)
 * @verbatim
 *        FRONT
 *   +-----+-----+
 *   | FL  |  FR |
 *   |     |     |
 *   +-----+-----+
 *   | BL  |  BR |
 *   |     |     |
 *   +-----+-----+
 *        BACK
 * @endverbatim
 *
 * @par Velocity Convention
 * - Positive: Forward motion
 * - Negative: Reverse motion
 * - Range: -2.0 to +2.0 m/s (robot max speed)
 *
 * @see rx_nanopb_create_velocity_command() Create command from params
 * @since Version 1.0.0
 */
typedef struct {
  double   front_left_mps;  /**< Front left motor velocity (m/s), [-2.0, +2.0] */
  double   front_right_mps; /**< Front right motor velocity (m/s), [-2.0, +2.0] */
  double   back_left_mps;   /**< Back left motor velocity (m/s), [-2.0, +2.0] */
  double   back_right_mps;  /**< Back right motor velocity (m/s), [-2.0, +2.0] */
  uint32_t sequence;        /**< Command sequence number (monotonically increasing) */
} rx_velocity_command_params_t;

/**
 * @struct rx_velocity_diff_drive_params_t
 * @brief VelocityCommand parameters for differential drive mode
 *
 * @details
 * Simplified parameters for differential drive robots where left and right
 * sides are controlled together. Front/back motors on same side receive
 * identical velocities.
 *
 * @par Differential Drive Mapping
 * - left_mps -> front_left_mps = back_left_mps
 * - right_mps -> front_right_mps = back_right_mps
 *
 * @par Motion Examples
 * | left_mps | right_mps | Motion |
 * |----------|-----------|--------|
 * | +1.0 | +1.0 | Forward |
 * | -1.0 | -1.0 | Reverse |
 * | +1.0 | -1.0 | Rotate CW |
 * | -1.0 | +1.0 | Rotate CCW |
 * | +1.0 | +0.5 | Turn right |
 *
 * @see rx_nanopb_create_velocity_command_diff_drive() Create diff-drive command
 * @since Version 1.0.0
 */
typedef struct {
  double   left_mps;  /**< Left side velocity (m/s), applied to FL and BL */
  double   right_mps; /**< Right side velocity (m/s), applied to FR and BR */
  uint32_t sequence;  /**< Command sequence number */
} rx_velocity_diff_drive_params_t;

/**
 * @brief Create VelocityCommand for 4 independent motors
 *
 * @details
 * Populates a VelocityCommand structure from parameters. Use for mecanum
 * or omnidirectional robots where each motor can have independent velocity.
 *
 * @param[out] cmd Output command structure to populate
 *   - All velocity fields set from params
 *   - sequence field set from params
 * @param[in] params Velocity command parameters
 *   - All velocities in m/s
 *   - sequence should be monotonically increasing
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, cmd populated
 * @retval k_rx_err_null_ptr nullptr in cmd or params
 *
 * @pre cmd and params must not benullptr
 * @post cmd fully populated with velocity values
 *
 * @note Thread-safe (no shared state)
 *
 * @par Example:
 * @code
 * rx_velocity_command_params_t params = {
 *     .front_left_mps = 1.0,
 *     .front_right_mps = 1.0,
 *     .back_left_mps = 1.0,
 *     .back_right_mps = 1.0,
 *     .sequence = g_command_sequence++,
 * };
 * star_v1_VelocityCommand cmd;
 * rx_nanopb_create_velocity_command(&cmd, &params);
 * @endcode
 *
 * @see rx_velocity_command_params_t Parameter structure
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t
rx_nanopb_create_velocity_command(star_v1_VelocityCommand*            cmd,
                                  const rx_velocity_command_params_t* params);

/**
 * @brief Create VelocityCommand in differential drive mode
 *
 * @details
 * Populates a VelocityCommand for differential drive robots. Left and right
 * parameters are applied to both front and back motors on each side.
 *
 * @par Velocity Mapping
 * - params->left_mps -> cmd->front_left_mps AND cmd->back_left_mps
 * - params->right_mps -> cmd->front_right_mps AND cmd->back_right_mps
 *
 * @param[out] cmd Output command structure to populate
 * @param[in] params Differential drive parameters
 *   - left_mps: Both left motors
 *   - right_mps: Both right motors
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, cmd populated
 * @retval k_rx_err_null_ptr nullptr in cmd or params
 *
 * @pre cmd and params must not benullptr
 * @post cmd populated with left/right velocities duplicated
 *
 * @note Thread-safe (no shared state)
 *
 * @par Example:
 * @code
 * // Turn left: right side faster than left
 * rx_velocity_diff_drive_params_t params = {
 *     .left_mps = 0.5,
 *     .right_mps = 1.0,
 *     .sequence = g_command_sequence++,
 * };
 * star_v1_VelocityCommand cmd;
 * rx_nanopb_create_velocity_command_diff_drive(&cmd, &params);
 * // Result: FL=BL=0.5, FR=BR=1.0
 * @endcode
 *
 * @see rx_velocity_diff_drive_params_t Parameter structure
 * @since Version 1.0.0
 */
rx_err_t
rx_nanopb_create_velocity_command_diff_drive(star_v1_VelocityCommand*               cmd,
                                             const rx_velocity_diff_drive_params_t* params);

/**
 * @brief Create ResponseHeader with status
 *
 * @details
 * Populates a ResponseHeader structure for response messages. Sets status
 * and optionally echoes the request ID from the original request.
 *
 * @param[out] header Output header structure to populate
 * @param[in] status Response status code
 *   - star_v1_Status_STATUS_OK: Success
 *   - star_v1_Status_STATUS_INVALID_ARGUMENT: Bad request
 *   - star_v1_Status_STATUS_INTERNAL_ERROR: Processing error
 * @param[in] request_id Original request ID to echo (can be NULL)
 *   - If NULL, request_id field left empty
 *   - If provided, copied to header.request_id
 *
 * @pre header must not benullptr
 * @post header populated with status and request_id (if provided)
 *
 * @note Thread-safe (no shared state)
 * @note request_id is copied, not referenced
 *
 * @par Example:
 * @code
 * star_v1_SetVelocityResponse response;
 * rx_nanopb_create_response_header(&response.header,
 *                                   star_v1_Status_STATUS_OK,
 *                                   request.header.request_id);
 * @endcode
 *
 * @since Version 1.0.0
 */
void rx_nanopb_create_response_header(star_v1_ResponseHeader* header,
                                      star_v1_Status          status,
                                      const char*             request_id);

/* =============================================================================
 * Test Helpers
 * =============================================================================
 */

/**
 * @brief Reset module state for unit testing
 *
 * @details
 * Resets the nanopb wrapper to uninitialized state. Used in unit tests
 * to verify initialization behavior and ensure clean state between tests.
 *
 * @warning FOR UNIT TESTING ONLY - Do not call in production code
 *
 * @pre None
 * @post Module state reset to uninitialized
 * @post rx_nanopb_init() must be called before encode/decode functions
 *
 * @note This function only exists in test builds
 *
 * @par Example - Test Setup:
 * @code
 * void setUp(void) {
 *     rx_nanopb_test_reset_state();  // Clean slate
 *     rx_nanopb_init();               // Re-initialize
 * }
 *
 * void test_encode_without_init(void) {
 *     rx_nanopb_test_reset_state();  // Don't call init
 *
 *     // Verify encode fails without initialization
 *     rx_err_t err = rx_nanopb_encode_telemetry(&msg, buf, 512, &len);
 *     TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
void rx_nanopb_test_reset_state(void);

#ifdef __cplusplus
}
#endif
