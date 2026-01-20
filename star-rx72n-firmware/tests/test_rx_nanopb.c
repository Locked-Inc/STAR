/* tests/test_rx_nanopb.c */

/**
 * @file test_rx_nanopb.c
 * @brief Unit Tests for nanopb Protocol Buffer Integration
 *
 * Tests the rx_nanopb wrapper for protocol buffer encode/decode
 * operations including velocity commands, emergency stop, and telemetry.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "rx_nanopb.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

static const uint32_t s_test_sequence_max = UINT32_MAX;

/**
 * @brief Test constants for velocity and buffer operations
 */
typedef enum : int32_t {
  k_test_buffer_size          = 512,
  k_test_small_buffer_size    = 1,
  k_test_sequence_zero        = 0,
  k_test_sequence_number      = 42,
  k_test_timestamp_us         = 1000000,
  k_test_wifi_signal_dbm      = -65,
  k_test_motor_id_left        = 0,
  k_test_motor_id_right       = 1,
  k_test_satellites           = 8,
  k_test_latency_us           = 500,
  k_test_free_heap_bytes      = 262144,
  k_test_uptime_seconds       = 3600,
  k_min_encoded_velocity_req  = 10,
  k_min_encoded_velocity_resp = 2,
  k_min_encoded_estop_resp    = 2,
  k_min_encoded_telemetry     = 1,
} test_constants_t;

/**
 * @brief Floating-point test values (4-motor differential drive)
 * Matches motor_config.h motor indices (front_left=0, front_right=1, back_left=2, back_right=3)
 */
static const double s_test_front_left_velocity_mps  = 1.5; /* Motor 0 */
static const double s_test_front_right_velocity_mps = 1.5; /* Motor 1 */
static const double s_test_back_left_velocity_mps   = 1.0; /* Motor 2 */
static const double s_test_back_right_velocity_mps  = 1.0; /* Motor 3 */
static const double s_test_max_velocity_mps         = 2.0;
static const double s_test_zero_velocity_mps        = 0.0;
static const double s_test_battery_percent          = 85.5;
static const double s_test_cpu_usage_percent        = 45.0;
static const double s_test_temperature_c            = 25.5;
static const double s_test_motor_load_percent       = 30.0;
static const double s_test_latitude_deg             = 37.7749;
static const double s_test_longitude_deg            = -122.4194;
static const double s_test_altitude_m               = 10.0;
static const double s_test_accuracy_m               = 2.5;
static const double s_test_pitch_rad                = 0.1;
static const double s_test_roll_rad                 = 0.05;
static const double s_test_yaw_rad                  = 1.57;
static const double s_test_accel_z_mps2             = 9.81;
static const float  s_test_float_tolerance          = 0.0001F;

static rx_velocity_command_params_t internal_make_velocity_params(double   front_left_mps,
                                                                  double   front_right_mps,
                                                                  double   back_left_mps,
                                                                  double   back_right_mps,
                                                                  uint32_t sequence)
{
  rx_velocity_command_params_t params = {
    .front_left_mps  = front_left_mps,
    .front_right_mps = front_right_mps,
    .back_left_mps   = back_left_mps,
    .back_right_mps  = back_right_mps,
    .sequence        = sequence,
  };
  return params;
}

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static uint8_t s_buffer[k_test_buffer_size];

void setUp(void)
{
  memset(s_buffer, 0, sizeof(s_buffer));
  rx_nanopb_test_reset_state();
  rx_nanopb_init();
}

void tearDown(void)
{
  /* Nothing to tear down */
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test rx_nanopb_init returns success
 */
void test_nanopb_init_success(void)
{
  rx_nanopb_test_reset_state();
  rx_err_t err = rx_nanopb_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test rx_nanopb_init detects duplicate initialization attempts
 */
void test_nanopb_init_detects_duplicate_call(void)
{
  rx_nanopb_test_reset_state();
  rx_err_t err1 = rx_nanopb_init();
  rx_err_t err2 = rx_nanopb_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err1);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err2);
}

/* =============================================================================
 * Velocity Request Encode Tests
 * =============================================================================
 */

/**
 * @brief Test encode velocity request with NULL message pointer
 */
void test_encode_velocity_request_null_msg(void)
{
  uint32_t len;
  rx_err_t err = rx_nanopb_encode_velocity_request(NULL, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode velocity request with NULL buffer pointer
 */
void test_encode_velocity_request_null_buffer(void)
{
  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;
  uint32_t                   len;
  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, NULL, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode velocity request with NULL length pointer
 */
void test_encode_velocity_request_null_len(void)
{
  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;
  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode velocity request with a buffer that is too small
 */
void test_encode_velocity_request_small_buffer(void)
{
  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;
  uint32_t                   len = 0;
  uint8_t                    small_buffer[k_test_small_buffer_size];

  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, small_buffer, sizeof(small_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test encode velocity request with empty message
 */
void test_encode_velocity_request_empty(void)
{
  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;
  uint32_t                   len = 0;

  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* Empty message should encode to zero bytes (all default values) */
  TEST_ASSERT_EQUAL(0, len);
}

/**
 * @brief Test encode velocity request with velocity command
 */
void test_encode_velocity_request_with_command(void)
{
  star_v1_SetVelocityRequest msg       = star_v1_SetVelocityRequest_init_zero;
  msg.has_command                      = true;
  msg.command.front_left_velocity_mps  = s_test_front_left_velocity_mps;
  msg.command.front_right_velocity_mps = s_test_front_right_velocity_mps;
  msg.command.back_left_velocity_mps   = s_test_back_left_velocity_mps;
  msg.command.back_right_velocity_mps  = s_test_back_right_velocity_mps;
  msg.command.sequence                 = k_test_sequence_number;
  msg.command.timestamp_us             = k_test_timestamp_us;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(k_min_encoded_velocity_req, len);
}

/**
 * @brief Test encode velocity request with max velocity values
 */
void test_encode_velocity_request_max_velocity(void)
{
  star_v1_SetVelocityRequest msg       = star_v1_SetVelocityRequest_init_zero;
  msg.has_command                      = true;
  msg.command.front_left_velocity_mps  = s_test_max_velocity_mps;
  msg.command.front_right_velocity_mps = s_test_max_velocity_mps;
  msg.command.back_left_velocity_mps   = s_test_max_velocity_mps;
  msg.command.back_right_velocity_mps  = s_test_max_velocity_mps;
  msg.command.sequence                 = s_test_sequence_max;
  msg.command.timestamp_us             = k_test_timestamp_us;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
}

/**
 * @brief Test encode velocity request with zero velocities
 */
void test_encode_velocity_request_zero_velocity(void)
{
  star_v1_SetVelocityRequest msg       = star_v1_SetVelocityRequest_init_zero;
  msg.has_command                      = true;
  msg.command.front_left_velocity_mps  = s_test_zero_velocity_mps;
  msg.command.front_right_velocity_mps = s_test_zero_velocity_mps;
  msg.command.back_left_velocity_mps   = s_test_zero_velocity_mps;
  msg.command.back_right_velocity_mps  = s_test_zero_velocity_mps;
  msg.command.sequence                 = k_test_sequence_number;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test encode velocity request with negative velocities
 */
void test_encode_velocity_request_negative_velocity(void)
{
  star_v1_SetVelocityRequest msg       = star_v1_SetVelocityRequest_init_zero;
  msg.has_command                      = true;
  msg.command.front_left_velocity_mps  = -s_test_max_velocity_mps;
  msg.command.front_right_velocity_mps = -s_test_max_velocity_mps;
  msg.command.back_left_velocity_mps   = -s_test_max_velocity_mps;
  msg.command.back_right_velocity_mps  = -s_test_max_velocity_mps;
  msg.command.sequence                 = k_test_sequence_number;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
}

/**
 * @brief Test encode velocity request when not initialized
 */
void test_encode_velocity_request_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;
  uint32_t                   len = 0;

  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/* =============================================================================
 * Velocity Request Decode Tests
 * =============================================================================
 */

/**
 * @brief Test decode velocity request with NULL buffer pointer
 */
void test_decode_velocity_request_null_buffer(void)
{
  star_v1_SetVelocityRequest msg;
  rx_err_t                   err = rx_nanopb_decode_velocity_request(NULL, 10, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode velocity request with NULL message pointer
 */
void test_decode_velocity_request_null_msg(void)
{
  uint8_t  data[16] = {0};
  rx_err_t err      = rx_nanopb_decode_velocity_request(data, sizeof(data), NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode velocity request with empty buffer
 */
void test_decode_velocity_request_empty_buffer(void)
{
  uint8_t                    data[1] = {0};
  star_v1_SetVelocityRequest msg     = star_v1_SetVelocityRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_velocity_request(data, 0, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode velocity request with invalid data returns protocol error
 */
void test_decode_velocity_request_invalid_data(void)
{
  /* Invalid protobuf data - starts with invalid wire type */
  uint8_t                    invalid_data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  star_v1_SetVelocityRequest msg            = star_v1_SetVelocityRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_velocity_request(invalid_data, sizeof(invalid_data), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief Test decode velocity request when not initialized
 */
void test_decode_velocity_request_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  uint8_t                    buffer[] = {0x0A, 0x08, 0x11, 0x00, 0x00, 0x00};
  star_v1_SetVelocityRequest msg      = star_v1_SetVelocityRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_velocity_request(buffer, sizeof(buffer), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief Test decode velocity request with oversized buffer
 */
void test_decode_velocity_request_oversized_buffer(void)
{
  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;

  /* Length exceeds k_nanopb_buffer_size */
  rx_err_t err = rx_nanopb_decode_velocity_request(s_buffer, k_nanopb_buffer_size + 1, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Velocity Request Round-Trip Tests
 * =============================================================================
 */

/**
 * @brief Test encode/decode round-trip for velocity request with command
 */
void test_velocity_request_roundtrip_with_command(void)
{
  star_v1_SetVelocityRequest original       = star_v1_SetVelocityRequest_init_zero;
  original.has_command                      = true;
  original.command.front_left_velocity_mps  = s_test_front_left_velocity_mps;
  original.command.front_right_velocity_mps = s_test_front_right_velocity_mps;
  original.command.back_left_velocity_mps   = s_test_back_left_velocity_mps;
  original.command.back_right_velocity_mps  = s_test_back_right_velocity_mps;
  original.command.sequence                 = k_test_sequence_number;
  original.command.timestamp_us             = k_test_timestamp_us;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_velocity_request(&original, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);

  star_v1_SetVelocityRequest decoded = star_v1_SetVelocityRequest_init_zero;
  err                                = rx_nanopb_decode_velocity_request(s_buffer, len, &decoded);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_TRUE(decoded.has_command);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)original.command.front_left_velocity_mps,
                           (float)decoded.command.front_left_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)original.command.back_left_velocity_mps,
                           (float)decoded.command.back_left_velocity_mps);
  TEST_ASSERT_EQUAL(original.command.sequence, decoded.command.sequence);
  TEST_ASSERT_EQUAL(original.command.timestamp_us, decoded.command.timestamp_us);
}

/**
 * @brief Test encode/decode round-trip preserves zero velocities
 */
void test_velocity_request_roundtrip_zero_velocity(void)
{
  star_v1_SetVelocityRequest original       = star_v1_SetVelocityRequest_init_zero;
  original.has_command                      = true;
  original.command.front_left_velocity_mps  = s_test_zero_velocity_mps;
  original.command.front_right_velocity_mps = s_test_zero_velocity_mps;
  original.command.back_left_velocity_mps   = s_test_zero_velocity_mps;
  original.command.back_right_velocity_mps  = s_test_zero_velocity_mps;
  original.command.sequence                 = k_test_sequence_number;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_velocity_request(&original, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  star_v1_SetVelocityRequest decoded = star_v1_SetVelocityRequest_init_zero;
  err                                = rx_nanopb_decode_velocity_request(s_buffer, len, &decoded);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_TRUE(decoded.has_command);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)s_test_zero_velocity_mps,
                           (float)decoded.command.front_left_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)s_test_zero_velocity_mps,
                           (float)decoded.command.back_left_velocity_mps);
}

/**
 * @brief Test encode/decode round-trip with negative velocities
 */
void test_velocity_request_roundtrip_negative_velocity(void)
{
  star_v1_SetVelocityRequest original       = star_v1_SetVelocityRequest_init_zero;
  original.has_command                      = true;
  original.command.front_left_velocity_mps  = -s_test_front_left_velocity_mps;
  original.command.front_right_velocity_mps = -s_test_front_right_velocity_mps;
  original.command.back_left_velocity_mps   = -s_test_back_left_velocity_mps;
  original.command.back_right_velocity_mps  = -s_test_back_right_velocity_mps;
  original.command.sequence                 = k_test_sequence_number;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_velocity_request(&original, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  star_v1_SetVelocityRequest decoded = star_v1_SetVelocityRequest_init_zero;
  err                                = rx_nanopb_decode_velocity_request(s_buffer, len, &decoded);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)(-s_test_front_left_velocity_mps),
                           (float)decoded.command.front_left_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)(-s_test_back_left_velocity_mps),
                           (float)decoded.command.back_left_velocity_mps);
}

/* =============================================================================
 * Velocity Response Encode Tests
 * =============================================================================
 */

/**
 * @brief Test encode velocity response with NULL message pointer
 */
void test_encode_velocity_response_null_msg(void)
{
  uint32_t len;
  rx_err_t err = rx_nanopb_encode_velocity_response(NULL, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode velocity response with NULL buffer pointer
 */
void test_encode_velocity_response_null_buffer(void)
{
  star_v1_SetVelocityResponse msg = star_v1_SetVelocityResponse_init_zero;
  uint32_t                    len;
  rx_err_t err = rx_nanopb_encode_velocity_response(&msg, NULL, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode velocity response with NULL length pointer
 */
void test_encode_velocity_response_null_len(void)
{
  star_v1_SetVelocityResponse msg = star_v1_SetVelocityResponse_init_zero;
  rx_err_t err = rx_nanopb_encode_velocity_response(&msg, s_buffer, sizeof(s_buffer), NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode velocity response with a buffer that is too small
 */
void test_encode_velocity_response_small_buffer(void)
{
  star_v1_SetVelocityResponse msg = star_v1_SetVelocityResponse_init_zero;
  uint32_t                    len = 0;
  uint8_t                     small_buffer[k_test_small_buffer_size];

  rx_err_t err = rx_nanopb_encode_velocity_response(&msg, small_buffer, sizeof(small_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test encode velocity response with empty message
 */
void test_encode_velocity_response_empty(void)
{
  star_v1_SetVelocityResponse msg = star_v1_SetVelocityResponse_init_zero;
  uint32_t                    len = 0;

  rx_err_t err = rx_nanopb_encode_velocity_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test encode velocity response with header and status OK
 */
void test_encode_velocity_response_with_header(void)
{
  star_v1_SetVelocityResponse msg = star_v1_SetVelocityResponse_init_zero;
  msg.has_header                  = true;
  msg.header.status               = star_v1_Status_STATUS_OK;
  msg.header.latency_us           = k_test_latency_us;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_velocity_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(k_min_encoded_velocity_resp, len);
}

/**
 * @brief Test encode velocity response with error status
 */
void test_encode_velocity_response_error_status(void)
{
  star_v1_SetVelocityResponse msg = star_v1_SetVelocityResponse_init_zero;
  msg.has_header                  = true;
  msg.header.status               = star_v1_Status_STATUS_INVALID_REQUEST;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_velocity_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
}

/**
 * @brief Test encode velocity response when not initialized
 */
void test_encode_velocity_response_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  star_v1_SetVelocityResponse msg = star_v1_SetVelocityResponse_init_zero;
  uint32_t                    len = 0;

  rx_err_t err = rx_nanopb_encode_velocity_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/* =============================================================================
 * Emergency Stop Request Decode Tests
 * =============================================================================
 */

/**
 * @brief Test decode estop request with NULL buffer pointer
 */
void test_decode_estop_request_null_buffer(void)
{
  star_v1_EmergencyStopRequest msg;
  rx_err_t                     err = rx_nanopb_decode_estop_request(NULL, 10, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode estop request with NULL message pointer
 */
void test_decode_estop_request_null_msg(void)
{
  uint8_t  data[16] = {0};
  rx_err_t err      = rx_nanopb_decode_estop_request(data, sizeof(data), NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode estop request with empty buffer
 */
void test_decode_estop_request_empty_buffer(void)
{
  uint8_t                      data[1] = {0};
  star_v1_EmergencyStopRequest msg     = star_v1_EmergencyStopRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_estop_request(data, 0, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode estop request with invalid data
 */
void test_decode_estop_request_invalid_data(void)
{
  uint8_t                      invalid_data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  star_v1_EmergencyStopRequest msg            = star_v1_EmergencyStopRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_estop_request(invalid_data, sizeof(invalid_data), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief Test decode estop request when not initialized
 */
void test_decode_estop_request_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  uint8_t                      buffer[] = {0x08, 0x01};
  star_v1_EmergencyStopRequest msg      = star_v1_EmergencyStopRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_estop_request(buffer, sizeof(buffer), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief Test decode estop request with oversized buffer
 */
void test_decode_estop_request_oversized_buffer(void)
{
  star_v1_EmergencyStopRequest msg = star_v1_EmergencyStopRequest_init_zero;

  /* Length exceeds k_nanopb_buffer_size */
  rx_err_t err = rx_nanopb_decode_estop_request(s_buffer, k_nanopb_buffer_size + 1, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Emergency Stop Response Encode Tests
 * =============================================================================
 */

/**
 * @brief Test encode estop response with NULL message pointer
 */
void test_encode_estop_response_null_msg(void)
{
  uint32_t len;
  rx_err_t err = rx_nanopb_encode_estop_response(NULL, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode estop response with NULL buffer pointer
 */
void test_encode_estop_response_null_buffer(void)
{
  star_v1_EmergencyStopResponse msg = star_v1_EmergencyStopResponse_init_zero;
  uint32_t                      len;
  rx_err_t err = rx_nanopb_encode_estop_response(&msg, NULL, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode estop response with NULL length pointer
 */
void test_encode_estop_response_null_len(void)
{
  star_v1_EmergencyStopResponse msg = star_v1_EmergencyStopResponse_init_zero;
  rx_err_t err = rx_nanopb_encode_estop_response(&msg, s_buffer, sizeof(s_buffer), NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode estop response with buffer that is too small
 */
void test_encode_estop_response_small_buffer(void)
{
  star_v1_EmergencyStopResponse msg = star_v1_EmergencyStopResponse_init_zero;
  uint32_t                      len = 0;
  uint8_t                       small_buffer[k_test_small_buffer_size];

  rx_err_t err = rx_nanopb_encode_estop_response(&msg, small_buffer, sizeof(small_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test encode estop response with estop engaged true
 */
void test_encode_estop_response_engaged_true(void)
{
  star_v1_EmergencyStopResponse msg = star_v1_EmergencyStopResponse_init_zero;
  msg.has_header                    = true;
  msg.header.status                 = star_v1_Status_STATUS_OK;
  msg.estop_engaged                 = true;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_estop_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(k_min_encoded_estop_resp, len);
}

/**
 * @brief Test encode estop response with estop engaged false
 */
void test_encode_estop_response_engaged_false(void)
{
  star_v1_EmergencyStopResponse msg = star_v1_EmergencyStopResponse_init_zero;
  msg.has_header                    = true;
  msg.header.status                 = star_v1_Status_STATUS_INTERNAL_ERROR;
  msg.estop_engaged                 = false;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_estop_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test encode estop response with estop active status
 */
void test_encode_estop_response_estop_active_status(void)
{
  star_v1_EmergencyStopResponse msg = star_v1_EmergencyStopResponse_init_zero;
  msg.has_header                    = true;
  msg.header.status                 = star_v1_Status_STATUS_ESTOP_ACTIVE;
  msg.estop_engaged                 = true;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_estop_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test encode estop response when not initialized
 */
void test_encode_estop_response_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  star_v1_EmergencyStopResponse msg = star_v1_EmergencyStopResponse_init_zero;
  uint32_t                      len = 0;

  rx_err_t err = rx_nanopb_encode_estop_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/* =============================================================================
 * Telemetry Encode Tests
 * =============================================================================
 */

/**
 * @brief Test encode telemetry with NULL message pointer
 */
void test_encode_telemetry_null_msg(void)
{
  uint32_t len;
  rx_err_t err = rx_nanopb_encode_telemetry(NULL, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode telemetry with NULL buffer pointer
 */
void test_encode_telemetry_null_buffer(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  uint32_t              len;
  rx_err_t              err = rx_nanopb_encode_telemetry(&msg, NULL, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode telemetry with NULL length pointer
 */
void test_encode_telemetry_null_len(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  rx_err_t              err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode telemetry with buffer that is too small
 */
void test_encode_telemetry_small_buffer(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  uint32_t              len = 0;
  uint8_t               small_buffer[k_test_small_buffer_size];

  rx_err_t err = rx_nanopb_encode_telemetry(&msg, small_buffer, sizeof(small_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test encode telemetry with empty message
 */
void test_encode_telemetry_empty(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  uint32_t              len = 0;

  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test encode telemetry with battery data
 */
void test_encode_telemetry_battery(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  msg.battery_percent       = s_test_battery_percent;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(k_min_encoded_telemetry, len);
}

/**
 * @brief Test encode telemetry with temperature data
 */
void test_encode_telemetry_temperature(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  msg.temperature_celsius   = s_test_temperature_c;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
}

/**
 * @brief Test encode telemetry with CPU usage data
 */
void test_encode_telemetry_cpu_usage(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  msg.cpu_usage_percent     = s_test_cpu_usage_percent;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test encode telemetry with motor load data
 */
void test_encode_telemetry_motor_load(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  msg.motor_load_percent    = s_test_motor_load_percent;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test encode telemetry with WiFi signal data
 */
void test_encode_telemetry_wifi_signal(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  msg.wifi_signal_dbm       = k_test_wifi_signal_dbm;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test encode telemetry with GPS data
 */
void test_encode_telemetry_gps(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  msg.has_gps               = true;
  msg.gps.latitude_deg      = s_test_latitude_deg;
  msg.gps.longitude_deg     = s_test_longitude_deg;
  msg.gps.altitude_m        = s_test_altitude_m;
  msg.gps.accuracy_m        = s_test_accuracy_m;
  msg.gps.satellites        = k_test_satellites;
  msg.gps.fix_type          = star_v1_GpsFix_GPS_FIX_3D;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
}

/**
 * @brief Test encode telemetry with IMU data
 */
void test_encode_telemetry_imu(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  msg.has_imu               = true;
  msg.imu.pitch_rad         = s_test_pitch_rad;
  msg.imu.roll_rad          = s_test_roll_rad;
  msg.imu.yaw_rad           = s_test_yaw_rad;
  msg.imu.accel_z_mps2      = s_test_accel_z_mps2;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
}

/**
 * @brief Test encode telemetry with all data fields
 */
void test_encode_telemetry_all_fields(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;

  /* Basic sensor data */
  msg.battery_percent     = s_test_battery_percent;
  msg.temperature_celsius = s_test_temperature_c;
  msg.cpu_usage_percent   = s_test_cpu_usage_percent;
  msg.motor_load_percent  = s_test_motor_load_percent;
  msg.wifi_signal_dbm     = k_test_wifi_signal_dbm;

  /* GPS data */
  msg.has_gps           = true;
  msg.gps.latitude_deg  = s_test_latitude_deg;
  msg.gps.longitude_deg = s_test_longitude_deg;
  msg.gps.altitude_m    = s_test_altitude_m;
  msg.gps.accuracy_m    = s_test_accuracy_m;
  msg.gps.satellites    = k_test_satellites;
  msg.gps.fix_type      = star_v1_GpsFix_GPS_FIX_RTK_FIXED;

  /* IMU data */
  msg.has_imu          = true;
  msg.imu.pitch_rad    = s_test_pitch_rad;
  msg.imu.roll_rad     = s_test_roll_rad;
  msg.imu.yaw_rad      = s_test_yaw_rad;
  msg.imu.accel_z_mps2 = s_test_accel_z_mps2;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_LESS_THAN(k_test_buffer_size, len);
}

/**
 * @brief Test encode telemetry when not initialized
 */
void test_encode_telemetry_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  uint32_t              len = 0;

  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/* =============================================================================
 * Helper Function Tests - Create Velocity Command
 * =============================================================================
 */

/**
 * @brief Test create velocity command with NULL pointer
 */
void test_create_velocity_command_null(void)
{
  rx_velocity_command_params_t params =
    internal_make_velocity_params(s_test_front_left_velocity_mps,
                                  s_test_front_right_velocity_mps,
                                  s_test_back_left_velocity_mps,
                                  s_test_back_right_velocity_mps,
                                  k_test_sequence_number);
  rx_err_t err = rx_nanopb_create_velocity_command(NULL, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test create velocity command with valid parameters
 */
void test_create_velocity_command_valid(void)
{
  star_v1_VelocityCommand      cmd;
  rx_velocity_command_params_t params =
    internal_make_velocity_params(s_test_front_left_velocity_mps,
                                  s_test_front_right_velocity_mps,
                                  s_test_back_left_velocity_mps,
                                  s_test_back_right_velocity_mps,
                                  k_test_sequence_number);
  rx_err_t err = rx_nanopb_create_velocity_command(&cmd, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)s_test_front_left_velocity_mps,
                           (float)cmd.front_left_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)s_test_front_right_velocity_mps,
                           (float)cmd.front_right_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)s_test_back_left_velocity_mps,
                           (float)cmd.back_left_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)s_test_back_right_velocity_mps,
                           (float)cmd.back_right_velocity_mps);
  TEST_ASSERT_EQUAL(k_test_sequence_number, cmd.sequence);
  TEST_ASSERT_EQUAL(0, cmd.timestamp_us); /* Timestamp should be zero by default */
}

/**
 * @brief Test create velocity command with zero velocities
 */
void test_create_velocity_command_zero(void)
{
  star_v1_VelocityCommand      cmd;
  rx_velocity_command_params_t params = internal_make_velocity_params(s_test_zero_velocity_mps,
                                                                      s_test_zero_velocity_mps,
                                                                      s_test_zero_velocity_mps,
                                                                      s_test_zero_velocity_mps,
                                                                      k_test_sequence_zero);
  rx_err_t                     err    = rx_nanopb_create_velocity_command(&cmd, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)s_test_zero_velocity_mps,
                           (float)cmd.front_left_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           (float)s_test_zero_velocity_mps,
                           (float)cmd.back_left_velocity_mps);
  TEST_ASSERT_EQUAL(k_test_sequence_zero, cmd.sequence);
}

/**
 * @brief Test create velocity command with max sequence number
 */
void test_create_velocity_command_max_sequence(void)
{
  star_v1_VelocityCommand      cmd;
  rx_velocity_command_params_t params =
    internal_make_velocity_params(s_test_front_left_velocity_mps,
                                  s_test_front_right_velocity_mps,
                                  s_test_back_left_velocity_mps,
                                  s_test_back_right_velocity_mps,
                                  s_test_sequence_max);
  rx_err_t err = rx_nanopb_create_velocity_command(&cmd, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_EQUAL(s_test_sequence_max, cmd.sequence);
}

/**
 * @brief Test create velocity command initializes struct to zero first
 */
void test_create_velocity_command_initializes_struct(void)
{
  star_v1_VelocityCommand cmd;
  /* Fill with garbage */
  memset(&cmd, 0xFF, sizeof(cmd));

  rx_velocity_command_params_t params =
    internal_make_velocity_params(s_test_front_left_velocity_mps,
                                  s_test_front_right_velocity_mps,
                                  s_test_back_left_velocity_mps,
                                  s_test_back_right_velocity_mps,
                                  k_test_sequence_number);
  rx_err_t err = rx_nanopb_create_velocity_command(&cmd, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* timestamp_us should be explicitly set to 0 */
  TEST_ASSERT_EQUAL(0, cmd.timestamp_us);
}

/* =============================================================================
 * Helper Function Tests - Create Response Header
 * =============================================================================
 */

/**
 * @brief Test create response header with NULL pointer
 */
void test_create_response_header_null(void)
{
  /* Should not crash with NULL pointer */
  rx_nanopb_create_response_header(NULL, star_v1_Status_STATUS_OK, "test-id");
  /* If we reach here, the function handled NULL gracefully */
  TEST_PASS();
}

/**
 * @brief Test create response header with STATUS_OK
 */
void test_create_response_header_ok(void)
{
  star_v1_ResponseHeader header;
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_OK, NULL);

  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_OK, header.status);
}

/**
 * @brief Test create response header with error status
 */
void test_create_response_header_error(void)
{
  star_v1_ResponseHeader header;
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_INVALID_REQUEST, NULL);

  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_INVALID_REQUEST, header.status);
}

/**
 * @brief Test create response header with request ID
 */
void test_create_response_header_with_request_id(void)
{
  star_v1_ResponseHeader header;
  const char*            request_id = "test-request-123";
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_OK, request_id);

  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_OK, header.status);
  /* The request_id callback should be set up */
  TEST_ASSERT_NOT_NULL(header.request_id.funcs.encode);
  TEST_ASSERT_EQUAL(request_id, header.request_id.arg);
}

/**
 * @brief Test create response header with NULL request ID
 */
void test_create_response_header_null_request_id(void)
{
  star_v1_ResponseHeader header;
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_TIMEOUT, NULL);

  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_TIMEOUT, header.status);
  /* The request_id callback should NOT be set */
  TEST_ASSERT_NULL(header.request_id.funcs.encode);
}

/**
 * @brief Test create response header with all status codes
 */
void test_create_response_header_all_status_codes(void)
{
  star_v1_ResponseHeader header;

  /* Test STATUS_UNKNOWN */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_UNKNOWN, NULL);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_UNKNOWN, header.status);

  /* Test STATUS_INTERNAL_ERROR */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_INTERNAL_ERROR, NULL);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_INTERNAL_ERROR, header.status);

  /* Test STATUS_NOT_FOUND */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_NOT_FOUND, NULL);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_NOT_FOUND, header.status);

  /* Test STATUS_INVALID_STATE */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_INVALID_STATE, NULL);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_INVALID_STATE, header.status);

  /* Test STATUS_ESTOP_ACTIVE */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_ESTOP_ACTIVE, NULL);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_ESTOP_ACTIVE, header.status);
}

/* =============================================================================
 * Encoded Length Tracking Tests
 * =============================================================================
 */

/**
 * @brief Test that encoded length increases with more data
 */
void test_encoded_length_increases_with_data(void)
{
  star_v1_TelemetryData msg_small = star_v1_TelemetryData_init_zero;
  star_v1_TelemetryData msg_large = star_v1_TelemetryData_init_zero;

  /* Small message: just battery */
  msg_small.battery_percent = s_test_battery_percent;

  /* Large message: battery + GPS + IMU */
  msg_large.battery_percent   = s_test_battery_percent;
  msg_large.has_gps           = true;
  msg_large.gps.latitude_deg  = s_test_latitude_deg;
  msg_large.gps.longitude_deg = s_test_longitude_deg;
  msg_large.has_imu           = true;
  msg_large.imu.pitch_rad     = s_test_pitch_rad;

  uint32_t len_small = 0;
  uint32_t len_large = 0;

  rx_err_t err_small =
    rx_nanopb_encode_telemetry(&msg_small, s_buffer, sizeof(s_buffer), &len_small);
  rx_err_t err_large =
    rx_nanopb_encode_telemetry(&msg_large, s_buffer, sizeof(s_buffer), &len_large);

  TEST_ASSERT_EQUAL(k_rx_ok, err_small);
  TEST_ASSERT_EQUAL(k_rx_ok, err_large);

  TEST_ASSERT_GREATER_THAN(len_small, len_large);
}

/**
 * @brief Test that empty message encodes to minimal size
 */
void test_empty_message_minimal_size(void)
{
  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;
  uint32_t                   len = 0;

  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Empty protobuf message should encode to 0 bytes (all fields default) */
  TEST_ASSERT_EQUAL(0, len);
}

/* =============================================================================
 * Buffer Size Handling Tests
 * =============================================================================
 */

/**
 * @brief Test encoding stays within buffer bounds for velocity request
 */
void test_velocity_request_fits_in_buffer(void)
{
  star_v1_SetVelocityRequest msg       = star_v1_SetVelocityRequest_init_zero;
  msg.has_command                      = true;
  msg.command.front_left_velocity_mps  = s_test_max_velocity_mps;
  msg.command.front_right_velocity_mps = s_test_max_velocity_mps;
  msg.command.back_left_velocity_mps   = s_test_max_velocity_mps;
  msg.command.back_right_velocity_mps  = s_test_max_velocity_mps;
  msg.command.sequence                 = s_test_sequence_max;
  msg.command.timestamp_us             = k_test_timestamp_us;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), &len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_LESS_THAN(k_nanopb_buffer_size, len);
}

/**
 * @brief Test encoding stays within buffer bounds for telemetry
 */
void test_telemetry_fits_in_buffer(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;

  /* Fill all fields */
  msg.battery_percent     = s_test_battery_percent;
  msg.temperature_celsius = s_test_temperature_c;
  msg.cpu_usage_percent   = s_test_cpu_usage_percent;
  msg.motor_load_percent  = s_test_motor_load_percent;
  msg.wifi_signal_dbm     = k_test_wifi_signal_dbm;

  msg.has_gps           = true;
  msg.gps.latitude_deg  = s_test_latitude_deg;
  msg.gps.longitude_deg = s_test_longitude_deg;
  msg.gps.altitude_m    = s_test_altitude_m;
  msg.gps.accuracy_m    = s_test_accuracy_m;
  msg.gps.satellites    = k_test_satellites;
  msg.gps.fix_type      = star_v1_GpsFix_GPS_FIX_RTK_FIXED;

  msg.has_imu              = true;
  msg.imu.pitch_rad        = s_test_pitch_rad;
  msg.imu.roll_rad         = s_test_roll_rad;
  msg.imu.yaw_rad          = s_test_yaw_rad;
  msg.imu.accel_x_mps2     = s_test_accel_z_mps2;
  msg.imu.accel_y_mps2     = s_test_accel_z_mps2;
  msg.imu.accel_z_mps2     = s_test_accel_z_mps2;
  msg.imu.gyro_x_rad_per_s = s_test_pitch_rad;
  msg.imu.gyro_y_rad_per_s = s_test_roll_rad;
  msg.imu.gyro_z_rad_per_s = s_test_yaw_rad;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_LESS_THAN(k_nanopb_buffer_size, len);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_nanopb_init_success);
  RUN_TEST(test_nanopb_init_detects_duplicate_call);

  /* Velocity request encode tests */
  RUN_TEST(test_encode_velocity_request_null_msg);
  RUN_TEST(test_encode_velocity_request_null_buffer);
  RUN_TEST(test_encode_velocity_request_null_len);
  RUN_TEST(test_encode_velocity_request_small_buffer);
  RUN_TEST(test_encode_velocity_request_empty);
  RUN_TEST(test_encode_velocity_request_with_command);
  RUN_TEST(test_encode_velocity_request_max_velocity);
  RUN_TEST(test_encode_velocity_request_zero_velocity);
  RUN_TEST(test_encode_velocity_request_negative_velocity);
  RUN_TEST(test_encode_velocity_request_not_initialized);

  /* Velocity request decode tests */
  RUN_TEST(test_decode_velocity_request_null_buffer);
  RUN_TEST(test_decode_velocity_request_null_msg);
  RUN_TEST(test_decode_velocity_request_empty_buffer);
  RUN_TEST(test_decode_velocity_request_invalid_data);
  RUN_TEST(test_decode_velocity_request_not_initialized);
  RUN_TEST(test_decode_velocity_request_oversized_buffer);

  /* Velocity request round-trip tests */
  RUN_TEST(test_velocity_request_roundtrip_with_command);
  RUN_TEST(test_velocity_request_roundtrip_zero_velocity);
  RUN_TEST(test_velocity_request_roundtrip_negative_velocity);

  /* Velocity response encode tests */
  RUN_TEST(test_encode_velocity_response_null_msg);
  RUN_TEST(test_encode_velocity_response_null_buffer);
  RUN_TEST(test_encode_velocity_response_null_len);
  RUN_TEST(test_encode_velocity_response_small_buffer);
  RUN_TEST(test_encode_velocity_response_empty);
  RUN_TEST(test_encode_velocity_response_with_header);
  RUN_TEST(test_encode_velocity_response_error_status);
  RUN_TEST(test_encode_velocity_response_not_initialized);

  /* Emergency stop request decode tests */
  RUN_TEST(test_decode_estop_request_null_buffer);
  RUN_TEST(test_decode_estop_request_null_msg);
  RUN_TEST(test_decode_estop_request_empty_buffer);
  RUN_TEST(test_decode_estop_request_invalid_data);
  RUN_TEST(test_decode_estop_request_not_initialized);
  RUN_TEST(test_decode_estop_request_oversized_buffer);

  /* Emergency stop response encode tests */
  RUN_TEST(test_encode_estop_response_null_msg);
  RUN_TEST(test_encode_estop_response_null_buffer);
  RUN_TEST(test_encode_estop_response_null_len);
  RUN_TEST(test_encode_estop_response_small_buffer);
  RUN_TEST(test_encode_estop_response_engaged_true);
  RUN_TEST(test_encode_estop_response_engaged_false);
  RUN_TEST(test_encode_estop_response_estop_active_status);
  RUN_TEST(test_encode_estop_response_not_initialized);

  /* Telemetry encode tests */
  RUN_TEST(test_encode_telemetry_null_msg);
  RUN_TEST(test_encode_telemetry_null_buffer);
  RUN_TEST(test_encode_telemetry_null_len);
  RUN_TEST(test_encode_telemetry_small_buffer);
  RUN_TEST(test_encode_telemetry_empty);
  RUN_TEST(test_encode_telemetry_battery);
  RUN_TEST(test_encode_telemetry_temperature);
  RUN_TEST(test_encode_telemetry_cpu_usage);
  RUN_TEST(test_encode_telemetry_motor_load);
  RUN_TEST(test_encode_telemetry_wifi_signal);
  RUN_TEST(test_encode_telemetry_gps);
  RUN_TEST(test_encode_telemetry_imu);
  RUN_TEST(test_encode_telemetry_all_fields);
  RUN_TEST(test_encode_telemetry_not_initialized);

  /* Helper function tests - velocity command */
  RUN_TEST(test_create_velocity_command_null);
  RUN_TEST(test_create_velocity_command_valid);
  RUN_TEST(test_create_velocity_command_zero);
  RUN_TEST(test_create_velocity_command_max_sequence);
  RUN_TEST(test_create_velocity_command_initializes_struct);

  /* Helper function tests - response header */
  RUN_TEST(test_create_response_header_null);
  RUN_TEST(test_create_response_header_ok);
  RUN_TEST(test_create_response_header_error);
  RUN_TEST(test_create_response_header_with_request_id);
  RUN_TEST(test_create_response_header_null_request_id);
  RUN_TEST(test_create_response_header_all_status_codes);

  /* Length tracking tests */
  RUN_TEST(test_encoded_length_increases_with_data);
  RUN_TEST(test_empty_message_minimal_size);

  /* Buffer size tests */
  RUN_TEST(test_velocity_request_fits_in_buffer);
  RUN_TEST(test_telemetry_fits_in_buffer);

  return UNITY_END();
}
