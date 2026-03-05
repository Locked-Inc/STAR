/**
 * @file test_serialization.c
 * @brief nanopb C Protobuf Serialization Tests
 *
 * @details
 * Verifies that generated nanopb code serializes/deserializes correctly.
 * Following Boston Dynamics style guide conventions.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Include generated nanopb headers (after buf generate + nanopb)
 * These will be available after running:
 *   pip install nanopb
 *   nanopb_generator --proto-path=proto --output-dir=gen/nanopb proto/star/v1/*.proto
 */
// #include "star/v1/common.pb.h"
// #include "star/v1/motor_control.pb.h"
// #include "star/v1/telemetry.pb.h"
// #include "pb_encode.h"
// #include "pb_decode.h"

void setUp(void) {
    /* Setup before each test */
}

void tearDown(void) {
    /* Cleanup after each test */
}

/**
 * @brief Placeholder test - verifies Unity framework is operational.
 */
void test_placeholder(void) {
    /* This test will be expanded once nanopb generates the C code.
     * For now, verify the test framework is working. */
    TEST_ASSERT_TRUE_MESSAGE(true, "Test framework is operational");
}

/* TODO: Uncomment after nanopb generates C code */
/*
void test_velocity_command_4motors_roundtrip(void) {
    uint8_t buffer[128];
    pb_ostream_t ostream;
    pb_istream_t istream;

    // Create original message with 4 independent motors (MKS units)
    star_v1_VelocityCommand original = star_v1_VelocityCommand_init_zero;
    original.motor_0_velocity_mps = 1.5;   // Motor 0: 1.5 m/s
    original.motor_1_velocity_mps = 1.5;   // Motor 1: 1.5 m/s
    original.motor_2_velocity_mps = 1.0;   // Motor 2: 1.0 m/s
    original.motor_3_velocity_mps = 1.0;   // Motor 3: 1.0 m/s
    original.sequence = 42;
    original.timestamp_us = 1000000;

    // Encode
    ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    bool encode_status = pb_encode(&ostream, star_v1_VelocityCommand_fields, &original);
    TEST_ASSERT_TRUE_MESSAGE(encode_status, "Encoding failed");

    size_t encoded_size = ostream.bytes_written;
    TEST_ASSERT_LESS_THAN_MESSAGE(100, encoded_size, "Encoded size should be compact");

    // Decode
    star_v1_VelocityCommand decoded = star_v1_VelocityCommand_init_zero;
    istream = pb_istream_from_buffer(buffer, encoded_size);
    bool decode_status = pb_decode(&istream, star_v1_VelocityCommand_fields, &decoded);
    TEST_ASSERT_TRUE_MESSAGE(decode_status, "Decoding failed");

    // Verify all 4 motor fields
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.5, decoded.motor_0_velocity_mps);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.5, decoded.motor_1_velocity_mps);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.0, decoded.motor_2_velocity_mps);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.0, decoded.motor_3_velocity_mps);
    TEST_ASSERT_EQUAL_UINT32(42, decoded.sequence);
}

void test_velocity_command_differential_drive(void) {
    uint8_t buffer[128];
    pb_ostream_t ostream;
    pb_istream_t istream;

    // Differential drive mode: Motors 0&1 = left side, Motors 2&3 = right side
    star_v1_VelocityCommand original = star_v1_VelocityCommand_init_zero;
    original.motor_0_velocity_mps = 1.5;   // Left front
    original.motor_1_velocity_mps = 1.5;   // Left rear
    original.motor_2_velocity_mps = -0.5;  // Right front (turn left)
    original.motor_3_velocity_mps = -0.5;  // Right rear (turn left)
    original.sequence = 1;

    // Encode
    ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    bool encode_status = pb_encode(&ostream, star_v1_VelocityCommand_fields, &original);
    TEST_ASSERT_TRUE_MESSAGE(encode_status, "Encoding failed");

    // Decode
    star_v1_VelocityCommand decoded = star_v1_VelocityCommand_init_zero;
    istream = pb_istream_from_buffer(buffer, ostream.bytes_written);
    bool decode_status = pb_decode(&istream, star_v1_VelocityCommand_fields, &decoded);
    TEST_ASSERT_TRUE_MESSAGE(decode_status, "Decoding failed");

    // Verify left motors match (0 & 1)
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.5, decoded.motor_0_velocity_mps);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.5, decoded.motor_1_velocity_mps);

    // Verify right motors match (2 & 3)
    TEST_ASSERT_FLOAT_WITHIN(0.001, -0.5, decoded.motor_2_velocity_mps);
    TEST_ASSERT_FLOAT_WITHIN(0.001, -0.5, decoded.motor_3_velocity_mps);
}

void test_encoder_data_4motors_streaming(void) {
    uint8_t buffer[64];

    // Test all 4 motors (motor IDs 0-3)
    for (int i = 0; i < 12; i++) {
        pb_ostream_t ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));

        star_v1_EncoderData encoder_data = star_v1_EncoderData_init_zero;
        encoder_data.motor_id = i % 4;  // Motor IDs 0, 1, 2, 3
        encoder_data.ticks = i * 1000;
        encoder_data.velocity_mps = i * 0.1f;
        encoder_data.timestamp_us = 1000000 + i * 1000;

        bool encode_status = pb_encode(&ostream, star_v1_EncoderData_fields, &encoder_data);
        TEST_ASSERT_TRUE(encode_status);

        // Decode and verify
        pb_istream_t istream = pb_istream_from_buffer(buffer, ostream.bytes_written);
        star_v1_EncoderData decoded = star_v1_EncoderData_init_zero;
        bool decode_status = pb_decode(&istream, star_v1_EncoderData_fields, &decoded);
        TEST_ASSERT_TRUE(decode_status);

        TEST_ASSERT_EQUAL_INT32(i % 4, decoded.motor_id);
        TEST_ASSERT_EQUAL_INT64(i * 1000, decoded.ticks);
    }
}

void test_enum_zero_values(void) {
    // Verify UNKNOWN is always 0 (Boston Dynamics style)
    TEST_ASSERT_EQUAL(0, star_v1_Status_STATUS_UNKNOWN);
    TEST_ASSERT_EQUAL(0, star_v1_ConnectionStatus_CONNECTION_STATUS_UNKNOWN);
    TEST_ASSERT_EQUAL(0, star_v1_RobotMode_ROBOT_MODE_UNKNOWN);
    TEST_ASSERT_EQUAL(0, star_v1_MotorState_MOTOR_STATE_UNKNOWN);
    TEST_ASSERT_EQUAL(0, star_v1_GpsFix_GPS_FIX_UNKNOWN);
}

void test_telemetry_data_mks_units(void) {
    uint8_t buffer[256];
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    star_v1_TelemetryData telemetry = star_v1_TelemetryData_init_zero;
    telemetry.has_imu = true;
    telemetry.imu.pitch_rad = 0.1;        // radians
    telemetry.imu.roll_rad = -0.05;
    telemetry.imu.yaw_rad = 1.57;
    telemetry.imu.accel_x_mps2 = 0.0;
    telemetry.imu.accel_y_mps2 = 0.0;
    telemetry.imu.accel_z_mps2 = 9.81;    // m/s^2
    telemetry.imu.gyro_x_rad_per_s = 0.01; // rad/s
    telemetry.imu.gyro_y_rad_per_s = 0.0;
    telemetry.imu.gyro_z_rad_per_s = 0.0;
    telemetry.temperature_celsius = 42.0;

    bool encode_status = pb_encode(&ostream, star_v1_TelemetryData_fields, &telemetry);
    TEST_ASSERT_TRUE(encode_status);

    pb_istream_t istream = pb_istream_from_buffer(buffer, ostream.bytes_written);
    star_v1_TelemetryData decoded = star_v1_TelemetryData_init_zero;
    bool decode_status = pb_decode(&istream, star_v1_TelemetryData_fields, &decoded);
    TEST_ASSERT_TRUE(decode_status);

    // Verify MKS units preserved
    TEST_ASSERT_FLOAT_WITHIN(0.001, 9.81, decoded.imu.accel_z_mps2);
    TEST_ASSERT_FLOAT_WITHIN(0.0001, 0.1, decoded.imu.pitch_rad);
}

void test_telemetry_data_rx72n_fields(void) {
    uint8_t buffer[256];
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    // RX72N-specific telemetry with individual encoder fields
    star_v1_TelemetryData telemetry = star_v1_TelemetryData_init_zero;
    telemetry.timestamp_us = 1000000;

    // Encoder data for all 4 motors (individual fields for embedded)
    telemetry.encoder_0.motor_id = 0;
    telemetry.encoder_0.ticks = 1000;
    telemetry.encoder_0.velocity_mps = 1.5;
    telemetry.encoder_0.timestamp_us = 1000000;

    telemetry.encoder_1.motor_id = 1;
    telemetry.encoder_1.ticks = 1050;
    telemetry.encoder_1.velocity_mps = 1.5;
    telemetry.encoder_1.timestamp_us = 1000000;

    telemetry.encoder_2.motor_id = 2;
    telemetry.encoder_2.ticks = 900;
    telemetry.encoder_2.velocity_mps = 1.0;
    telemetry.encoder_2.timestamp_us = 1000000;

    telemetry.encoder_3.motor_id = 3;
    telemetry.encoder_3.ticks = 950;
    telemetry.encoder_3.velocity_mps = 1.0;
    telemetry.encoder_3.timestamp_us = 1000000;

    // Safety state
    telemetry.emergency_stop = false;
    telemetry.fault_flags = 0;

    bool encode_status = pb_encode(&ostream, star_v1_TelemetryData_fields, &telemetry);
    TEST_ASSERT_TRUE_MESSAGE(encode_status, "Encoding failed");

    // Verify compact size for embedded systems
    TEST_ASSERT_LESS_THAN_MESSAGE(256, ostream.bytes_written, "Encoded size should be < 256 bytes");

    // Decode
    pb_istream_t istream = pb_istream_from_buffer(buffer, ostream.bytes_written);
    star_v1_TelemetryData decoded = star_v1_TelemetryData_init_zero;
    bool decode_status = pb_decode(&istream, star_v1_TelemetryData_fields, &decoded);
    TEST_ASSERT_TRUE_MESSAGE(decode_status, "Decoding failed");

    // Verify all 4 encoder fields
    TEST_ASSERT_EQUAL_INT32(0, decoded.encoder_0.motor_id);
    TEST_ASSERT_EQUAL_INT64(1000, decoded.encoder_0.ticks);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.5, decoded.encoder_0.velocity_mps);

    TEST_ASSERT_EQUAL_INT32(1, decoded.encoder_1.motor_id);
    TEST_ASSERT_EQUAL_INT64(1050, decoded.encoder_1.ticks);

    TEST_ASSERT_EQUAL_INT32(2, decoded.encoder_2.motor_id);
    TEST_ASSERT_EQUAL_INT64(900, decoded.encoder_2.ticks);

    TEST_ASSERT_EQUAL_INT32(3, decoded.encoder_3.motor_id);
    TEST_ASSERT_EQUAL_INT64(950, decoded.encoder_3.ticks);

    // Verify safety data
    TEST_ASSERT_FALSE(decoded.emergency_stop);
    TEST_ASSERT_EQUAL_UINT32(0, decoded.fault_flags);
}

void test_request_header_string_limits(void) {
    uint8_t buffer[256];
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    star_v1_RequestHeader header = star_v1_RequestHeader_init_zero;

    // Test within nanopb string limits (max_size:64 for request_id)
    strncpy(header.request_id, "test-request-12345678901234567890", sizeof(header.request_id) - 1);
    strncpy(header.client_version, "1.0.0", sizeof(header.client_version) - 1);

    bool encode_status = pb_encode(&ostream, star_v1_RequestHeader_fields, &header);
    TEST_ASSERT_TRUE(encode_status);

    pb_istream_t istream = pb_istream_from_buffer(buffer, ostream.bytes_written);
    star_v1_RequestHeader decoded = star_v1_RequestHeader_init_zero;
    bool decode_status = pb_decode(&istream, star_v1_RequestHeader_fields, &decoded);
    TEST_ASSERT_TRUE(decode_status);

    TEST_ASSERT_EQUAL_STRING("test-request-12345678901234567890", decoded.request_id);
}
*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_placeholder);

    /* TODO: Uncomment after nanopb generates C code */
    /*
    RUN_TEST(test_velocity_command_4motors_roundtrip);
    RUN_TEST(test_velocity_command_differential_drive);
    RUN_TEST(test_encoder_data_4motors_streaming);
    RUN_TEST(test_enum_zero_values);
    RUN_TEST(test_telemetry_data_mks_units);
    RUN_TEST(test_telemetry_data_rx72n_fields);
    RUN_TEST(test_request_header_string_limits);
    */

    return UNITY_END();
}
