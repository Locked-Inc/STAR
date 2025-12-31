/**
 * @file rx_nanopb.c
 * @brief nanopb Integration Wrapper for RX72N
 *
 * Provides simplified encode/decode functions with static buffers
 * for protocol buffer messages used in the STAR project.
 *
 * @note All encoding/decoding uses static buffers - no dynamic allocation.
 *
 * STAR Project - Texas A&M University
 * December 2025
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

  uint32_t len = strlen(str);
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
  s_initialized = true;
  return RX_OK;
}

/* =============================================================================
 * SetVelocityRequest Encode/Decode
 * =============================================================================
 */

rx_err_t rx_nanopb_encode_velocity_request(const star_v1_SetVelocityRequest* msg,
                                           uint8_t*                          buffer,
                                           uint32_t*                         len)
{
  if (msg == NULL || buffer == NULL || len == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, RX_NANOPB_BUFFER_SIZE);

  if (!pb_encode(&stream, star_v1_SetVelocityRequest_fields, msg)) {
    return RX_ERR_INVALID_SIZE;
  }

  *len = stream.bytes_written;
  return RX_OK;
}

rx_err_t rx_nanopb_decode_velocity_request(const uint8_t*              buffer,
                                           uint32_t                    len,
                                           star_v1_SetVelocityRequest* msg)
{
  if (buffer == NULL || msg == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  /* Initialize message to default values */
  *msg = (star_v1_SetVelocityRequest)star_v1_SetVelocityRequest_init_zero;

  pb_istream_t stream = pb_istream_from_buffer(buffer, len);

  if (!pb_decode(&stream, star_v1_SetVelocityRequest_fields, msg)) {
    return RX_ERR_PROTOCOL_ERROR;
  }

  return RX_OK;
}

/* =============================================================================
 * SetVelocityResponse Encode/Decode
 * =============================================================================
 */

rx_err_t rx_nanopb_encode_velocity_response(const star_v1_SetVelocityResponse* msg,
                                            uint8_t*                           buffer,
                                            uint32_t*                          len)
{
  if (msg == NULL || buffer == NULL || len == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, RX_NANOPB_BUFFER_SIZE);

  if (!pb_encode(&stream, star_v1_SetVelocityResponse_fields, msg)) {
    return RX_ERR_INVALID_SIZE;
  }

  *len = stream.bytes_written;
  return RX_OK;
}

/* =============================================================================
 * EmergencyStopRequest Encode/Decode
 * =============================================================================
 */

rx_err_t rx_nanopb_decode_estop_request(const uint8_t*                buffer,
                                        uint32_t                      len,
                                        star_v1_EmergencyStopRequest* msg)
{
  if (buffer == NULL || msg == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  /* Initialize message to default values */
  *msg = (star_v1_EmergencyStopRequest)star_v1_EmergencyStopRequest_init_zero;

  pb_istream_t stream = pb_istream_from_buffer(buffer, len);

  if (!pb_decode(&stream, star_v1_EmergencyStopRequest_fields, msg)) {
    return RX_ERR_PROTOCOL_ERROR;
  }

  return RX_OK;
}

rx_err_t rx_nanopb_encode_estop_response(const star_v1_EmergencyStopResponse* msg,
                                         uint8_t*                             buffer,
                                         uint32_t*                            len)
{
  if (msg == NULL || buffer == NULL || len == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, RX_NANOPB_BUFFER_SIZE);

  if (!pb_encode(&stream, star_v1_EmergencyStopResponse_fields, msg)) {
    return RX_ERR_INVALID_SIZE;
  }

  *len = stream.bytes_written;
  return RX_OK;
}

/* =============================================================================
 * Telemetry Encode/Decode
 * =============================================================================
 */

rx_err_t
rx_nanopb_encode_telemetry(const star_v1_TelemetryData* msg, uint8_t* buffer, uint32_t* len)
{
  if (msg == NULL || buffer == NULL || len == NULL) {
    return RX_ERR_INVALID_ARG;
  }

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, RX_NANOPB_BUFFER_SIZE);

  if (!pb_encode(&stream, star_v1_TelemetryData_fields, msg)) {
    return RX_ERR_INVALID_SIZE;
  }

  *len = stream.bytes_written;
  return RX_OK;
}

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

void rx_nanopb_create_velocity_command(star_v1_VelocityCommand* cmd,
                                       double                   left_mps,
                                       double                   right_mps,
                                       uint32_t                 sequence)
{
  if (cmd == NULL) {
    return;
  }

  *cmd                    = (star_v1_VelocityCommand)star_v1_VelocityCommand_init_zero;
  cmd->left_velocity_mps  = left_mps;
  cmd->right_velocity_mps = right_mps;
  cmd->sequence           = sequence;
  cmd->timestamp_us       = 0; /* Set by caller if needed */
}

void rx_nanopb_create_response_header(star_v1_ResponseHeader* header,
                                      star_v1_Status          status,
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
