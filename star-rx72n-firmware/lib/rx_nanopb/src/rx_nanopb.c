/* lib/rx_nanopb/src/rx_nanopb.c */

/**
 * @file rx_nanopb.c
 * @brief nanopb Integration Wrapper for RX72N
 * @details
 * Provides simplified encode/decode functions with static buffers
 * for protocol buffer messages used in the STAR project.
 *
 * @note All encoding/decoding uses static buffers - no dynamic allocation.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_nanopb.h"

#include <pb_decode.h>
#include <pb_encode.h>
#include <string.h>

/* =============================================================================
 * Module State
 * =============================================================================
 */

/** Module initialization flag */
static bool s_initialized = false;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Callback for encoding a static string
 *
 * Used for fields that require callbacks but we have static strings.
 *
 * @note The `void* const* arg` parameter is nanopb's callback convention:
 *       - `void*` is the type being pointed to (the user data pointer)
 *       - `const*` means the pointer itself is const (we can't change where it points)
 *       - This is a "pointer to a const pointer to void"
 *       - Read right-to-left: "arg is a pointer to a const pointer to void"
 */
static bool
internal_encode_string_callback(pb_ostream_t* stream, const pb_field_t* field, void* const* arg)
{
  const char* str = (const char*)*arg;
  if (str == NULL) {
    return true; /* Empty string is valid */
  }

  const uint32_t len = strlen(str);
  if (!pb_encode_tag_for_field(stream, field)) {
    return false;
  }
  return pb_encode_string(stream, (const pb_byte_t*)str, len);
}

/* =============================================================================
 * Initialization
 * =============================================================================
 */

rx_err_t rx_nanopb_init(void)
{
  /* Pre-condition: Check not already initialized */
  if (s_initialized) {
    return k_rx_err_invalid_state;
  }

  s_initialized = true;

  /* Post-condition: Verify initialization succeeded */
  if (!s_initialized) {
    return k_rx_fail;
  }

  return k_rx_ok;
}

/**
 * @brief Reset module state for testing
 *
 * @note This function is only for unit testing purposes.
 *       It allows tests to reset the initialization state.
 */
void rx_nanopb_test_reset_state(void)
{
  s_initialized = false;
}

/* =============================================================================
 * SetVelocityRequest Encode/Decode
 * =============================================================================
 */

rx_err_t rx_nanopb_encode_velocity_request(const star_v1_SetVelocityRequest* msg,
                                           uint8_t*                          buffer,
                                           const uint32_t                    buffer_size,
                                           uint32_t*                         len)
{
  /* Pre-condition 1: NULL pointer checks */
  if (msg == NULL || buffer == NULL || len == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Module initialized */
  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }

  /* Pre-condition 3: Buffer size validation (NASA Rule 5 - buffer overflow prevention) */
  if (buffer_size < k_nanopb_buffer_size) {
    return k_rx_err_invalid_size;
  }

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);

  if (!pb_encode(&stream, star_v1_SetVelocityRequest_fields, msg)) {
    return k_rx_err_invalid_size;
  }

  *len = stream.bytes_written;

  /* Post-condition: Encoded length within bounds */
  if (*len > k_nanopb_buffer_size) {
    return k_rx_err_invalid_size;
  }

  return k_rx_ok;
}

rx_err_t rx_nanopb_decode_velocity_request(const uint8_t*              buffer,
                                           const uint32_t              len,
                                           star_v1_SetVelocityRequest* msg)
{
  /* Pre-condition 1: NULL pointer checks */
  if (buffer == NULL || msg == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Length validation */
  if (len == 0 || len > k_nanopb_buffer_size) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 3: Module initialized */
  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }

  /* Initialize message to default values */
  *msg = (star_v1_SetVelocityRequest)star_v1_SetVelocityRequest_init_zero;

  pb_istream_t stream = pb_istream_from_buffer(buffer, len);

  if (!pb_decode(&stream, star_v1_SetVelocityRequest_fields, msg)) {
    return k_rx_err_protocol_error;
  }

  return k_rx_ok;
}

/* =============================================================================
 * SetVelocityResponse Encode/Decode
 * =============================================================================
 */

rx_err_t rx_nanopb_encode_velocity_response(const star_v1_SetVelocityResponse* msg,
                                            uint8_t*                           buffer,
                                            const uint32_t                     buffer_size,
                                            uint32_t*                          len)
{
  /* Pre-condition 1: NULL pointer checks */
  if (msg == NULL || buffer == NULL || len == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Module initialized */
  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }

  /* Pre-condition 3: Buffer size validation (NASA Rule 5 - buffer overflow prevention) */
  if (buffer_size < k_nanopb_buffer_size) {
    return k_rx_err_invalid_size;
  }

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);

  if (!pb_encode(&stream, star_v1_SetVelocityResponse_fields, msg)) {
    return k_rx_err_invalid_size;
  }

  *len = stream.bytes_written;

  /* Post-condition: Encoded length within bounds */
  if (*len > k_nanopb_buffer_size) {
    return k_rx_err_invalid_size;
  }

  return k_rx_ok;
}

/* =============================================================================
 * EmergencyStopRequest Encode/Decode
 * =============================================================================
 */

rx_err_t rx_nanopb_decode_estop_request(const uint8_t*                buffer,
                                        const uint32_t                len,
                                        star_v1_EmergencyStopRequest* msg)
{
  /* Pre-condition 1: NULL pointer checks */
  if (buffer == NULL || msg == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Length validation */
  if (len == 0 || len > k_nanopb_buffer_size) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 3: Module initialized */
  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }

  /* Initialize message to default values */
  *msg = (star_v1_EmergencyStopRequest)star_v1_EmergencyStopRequest_init_zero;

  pb_istream_t stream = pb_istream_from_buffer(buffer, len);

  if (!pb_decode(&stream, star_v1_EmergencyStopRequest_fields, msg)) {
    return k_rx_err_protocol_error;
  }

  return k_rx_ok;
}

rx_err_t rx_nanopb_encode_estop_response(const star_v1_EmergencyStopResponse* msg,
                                         uint8_t*                             buffer,
                                         const uint32_t                       buffer_size,
                                         uint32_t*                            len)
{
  /* Pre-condition 1: NULL pointer checks */
  if (msg == NULL || buffer == NULL || len == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Module initialized */
  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }

  /* Pre-condition 3: Buffer size validation (NASA Rule 5 - buffer overflow prevention) */
  if (buffer_size < k_nanopb_buffer_size) {
    return k_rx_err_invalid_size;
  }

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);

  if (!pb_encode(&stream, star_v1_EmergencyStopResponse_fields, msg)) {
    return k_rx_err_invalid_size;
  }

  *len = stream.bytes_written;

  /* Post-condition: Encoded length within bounds */
  if (*len > k_nanopb_buffer_size) {
    return k_rx_err_invalid_size;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Telemetry Encode/Decode
 * =============================================================================
 */

rx_err_t rx_nanopb_encode_telemetry(const star_v1_TelemetryData* msg,
                                    uint8_t*                     buffer,
                                    const uint32_t               buffer_size,
                                    uint32_t*                    len)
{
  /* Pre-condition 1: NULL pointer checks */
  if (msg == NULL || buffer == NULL || len == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition 2: Module initialized */
  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }

  /* Pre-condition 3: Buffer size validation (NASA Rule 5 - buffer overflow prevention) */
  if (buffer_size < k_nanopb_buffer_size) {
    return k_rx_err_invalid_size;
  }

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);

  if (!pb_encode(&stream, star_v1_TelemetryData_fields, msg)) {
    return k_rx_err_invalid_size;
  }

  *len = stream.bytes_written;

  /* Post-condition: Encoded length within bounds */
  if (*len > k_nanopb_buffer_size) {
    return k_rx_err_invalid_size;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

void rx_nanopb_create_velocity_command(star_v1_VelocityCommand* cmd,
                                       const double             front_left_mps,
                                       const double             front_right_mps,
                                       const double             back_left_mps,
                                       const double             back_right_mps,
                                       const uint32_t           sequence)
{
  if (cmd == NULL) {
    return;
  }

  *cmd                          = (star_v1_VelocityCommand)star_v1_VelocityCommand_init_zero;
  cmd->front_left_velocity_mps  = front_left_mps;
  cmd->front_right_velocity_mps = front_right_mps;
  cmd->back_left_velocity_mps   = back_left_mps;
  cmd->back_right_velocity_mps  = back_right_mps;
  cmd->sequence                 = sequence;
  cmd->timestamp_us             = 0; /* Set by caller if needed */
}

void rx_nanopb_create_velocity_command_diff_drive(star_v1_VelocityCommand* cmd,
                                                  const double             left_mps,
                                                  const double             right_mps,
                                                  const uint32_t           sequence)
{
  /* Differential drive mode: Lock left 2 motors together, right 2 motors together
   * Front Left & Back Left = Left side
   * Front Right & Back Right = Right side */
  rx_nanopb_create_velocity_command(cmd, left_mps, right_mps, left_mps, right_mps, sequence);
}

void rx_nanopb_create_response_header(star_v1_ResponseHeader* header,
                                      const star_v1_Status    status,
                                      const char*             request_id)
{
  if (header == NULL) {
    return;
  }

  *header        = (star_v1_ResponseHeader)star_v1_ResponseHeader_init_zero;
  header->status = status;

  /* Set up callback for request_id if provided */
  if (request_id != NULL) {
    header->request_id.arg          = (void*)request_id;
    header->request_id.funcs.encode = internal_encode_string_callback;
  }
}
