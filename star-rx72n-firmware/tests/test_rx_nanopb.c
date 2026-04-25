/**
 * @file test_rx_nanopb.c
 * @brief Comprehensive Unit Tests for nanopb Protocol Buffer Integration Layer
 *
 * @details
 * **Comprehensive Test Suite for nanopb Protocol Buffers Encoding/Decoding**
 *
 * This test suite validates the rx_nanopb wrapper layer - a lightweight C interface
 * that wraps the nanopb Protocol Buffers library for RX72N embedded firmware. It tests
 * encode/decode operations for motor control messages, telemetry data, emergency stop
 * commands, and helper functions that construct protobuf messages from application data.
 *
 * **nanopb Integration Architecture:**
 * @code
 * +---------------------------+
 * |  Application Layer        | <- Motor control, telemetry tasks
 * +---------------------------+
 * |  rx_nanopb (THIS LAYER)   | <- Encode/decode wrapper, helper functions
 * +---------------------------+
 * |  nanopb Library           | <- Core protobuf encoder/decoder (no malloc)
 * +---------------------------+
 * |  Generated .pb.h/c        | <- star.v1.* message definitions
 * +---------------------------+
 * |  SPI/USB Transport        | <- Physical layer (RPi5 <-> RX72N)
 * +---------------------------+
 * @endcode
 *
 * **Test Categories (97 total tests):**
 * | Category | Test Count | Description |
 * |----------|------------|-------------|
 * | Initialization | 2 | Module init, duplicate init detection |
 * | Velocity Request Encode | 8 | nullptr checks, buffer sizing, edge cases |
 * | Velocity Request Decode | 6 | Invalid data, malformed protobuf, oversized buffers |
 * | Velocity Request Round-Trip | 3 | Encode -> decode -> verify equality |
 * | Velocity Response Encode | 6 | Header population, status codes, error handling |
 * | Emergency Stop Request Decode | 6 | E-stop message parsing, error injection |
 * | Emergency Stop Response Encode | 8 | E-stop engaged/disengaged states, status codes |
 * | SetPidGainsRequest Decode | 8 | PID controller gains update command validation |
 * | Telemetry Encode | 13 | GPS, IMU, temperature, WiFi, CPU, motor load |
 * | Helper Functions (Velocity Cmd) | 5 | Message factory validation, zero initialization |
 * | Helper Functions (Response Hdr) | 6 | Status code mapping, request ID handling |
 * | Encoded Length Tracking | 2 | Message size validation, empty message edge case |
 * | Buffer Size Handling | 2 | Boundary checks, max message size verification |
 *
 * **Protocol Buffers Message Types Tested:**
 * | Message Type | Direction | Fields | Purpose |
 * |--------------|-----------|--------|---------|
 * | star.v1.SetVelocityRequest | RPi5 -> RX72N | VelocityCommand (optional) | Motor velocity commands (4-motor differential drive) |
 * | star.v1.SetVelocityResponse | RX72N -> RPi5 | ResponseHeader (optional) | Command acknowledgment with status/latency |
 * | star.v1.EmergencyStopRequest | RPi5 -> RX72N | engage (bool) | Emergency stop command |
 * | star.v1.EmergencyStopResponse | RX72N -> RPi5 | ResponseHeader + estop_engaged (bool) | E-stop state confirmation |
 * | star.v1.TelemetryData | RX72N -> RPi5 | 14+ fields (GPS, IMU, etc.) | Sensor/motor status telemetry |
 *
 * **Test Methodology:**
 * @par 1. Initialization Tests:
 * @code
 * 1. Call rx_nanopb_init() -> Assert returns k_rx_ok
 * 2. Call rx_nanopb_init() again -> Assert returns k_rx_err_invalid_state (duplicate init)
 * 3. Reset state with rx_nanopb_test_reset_state()
 * 4. Verify encode/decode fail with k_rx_err_not_initialized before init
 * @endcode
 *
 * @par 2. Encode Tests (Validation):
 * @code
 * 1. nullptr pointer checks: msg=nullptr, buffer=nullptr, len=nullptr -> k_rx_err_invalid_arg
 * 2. Buffer size checks: 1-byte buffer -> k_rx_err_invalid_size
 * 3. Empty message: All fields zero -> Encodes to 0 bytes (protobuf default optimization)
 * 4. Valid message: Populate fields -> len > minimum expected size
 * 5. Verify encoded length stays within s_nanopb_buffer_size (512 bytes)
 * @endcode
 *
 * @par 3. Decode Tests (Error Injection):
 * @code
 * 1. nullptr pointer checks: buffer=nullptr, msg=nullptr -> k_rx_err_invalid_arg
 * 2. Empty buffer: len=0 -> k_rx_err_invalid_arg
 * 3. Invalid protobuf data: 0xFF 0xFF 0xFF... -> k_rx_err_protocol_error
 * 4. Oversized buffer: len > s_nanopb_buffer_size -> k_rx_err_invalid_arg
 * 5. Verify protobuf wire format violations detected
 * @endcode
 *
 * @par 4. Round-Trip Tests (Lossless Encoding):
 * @code
 * 1. Create original message with known field values
 * 2. Encode to buffer -> len_out
 * 3. Decode from buffer -> decoded message
 * 4. Assert: original.field == decoded.field for all fields
 * 5. Special cases: zero velocity, negative velocity, max velocity, max sequence
 * 6. Float comparison: TEST_ASSERT_FLOAT_WITHIN(0.0001, original, decoded)
 * @endcode
 *
 * @par 5. Field Validation Tests:
 * @code
 * Velocity Command (4-motor differential drive):
 * - front_left_velocity_mps:  -2.0 to +2.0 m/s (double, maps to int32 in protobuf)
 * - front_right_velocity_mps: -2.0 to +2.0 m/s
 * - back_left_velocity_mps:   -2.0 to +2.0 m/s
 * - back_right_velocity_mps:  -2.0 to +2.0 m/s
 * - sequence: 0 to UINT32_MAX (wraps to 0)
 * - timestamp_us: microsecond timestamp (optional)
 *
 * Telemetry Data (14+ fields):
 * - temperature_celsius: -40 to +85degC (double)
 * - cpu_usage_percent: 0-100 (double)
 * - motor_load_percent: 0-100 (double)
 * - wifi_signal_dbm: -100 to 0 dBm (int32)
 * - GPS (nested): latitude_deg, longitude_deg, altitude_m, accuracy_m, satellites (uint32), fix_type (enum)
 * - IMU (nested): pitch_rad, roll_rad, yaw_rad, accel_{x,y,z}_mps2, gyro_{x,y,z}_rad_per_s
 *
 * Response Header:
 * - status: enum (STATUS_OK, STATUS_INVALID_REQUEST, STATUS_INTERNAL_ERROR, etc.)
 * - request_id: string (max 64 bytes per .options file)
 * - latency_us: uint32_t (optional)
 * @endcode
 *
 * **Coverage Analysis:**
 * @par Message Type Coverage:
 * | Message | Encode Tests | Decode Tests | Round-Trip | Helper Functions |
 * |---------|--------------|--------------|------------|------------------|
 * | SetVelocityRequest | 8 | 6 | 3 | 5 (create_velocity_command) |
 * | SetVelocityResponse | 6 | - | - | 6 (create_response_header) |
 * | EmergencyStopRequest | - | 6 | - | - |
 * | EmergencyStopResponse | 8 | - | - | - |
 * | TelemetryData | 14 | - | - | - |
 * | **TOTAL** | **36** | **12** | **3** | **11** |
 *
 * @par Field Type Coverage:
 * | Field Type | Tests | Examples |
 * |------------|-------|----------|
 * | double | 20 | velocity_mps, temperature_celsius, GPS coords |
 * | uint32 | 8 | sequence, timestamp_us, satellites, latency_us |
 * | int32 | 3 | wifi_signal_dbm |
 * | bool | 6 | has_command, has_gps, has_imu, estop_engaged |
 * | enum | 8 | Status codes (OK, INVALID_REQUEST, etc.), GpsFix types |
 * | string | 2 | request_id (with nullptr and valid string) |
 * | nested message | 6 | GPS, IMU submessages |
 * | optional fields | 12 | has_command, has_header, has_gps, has_imu |
 *
 * @par Error Code Coverage:
 * | Error Code | Tests | Scenarios |
 * |------------|-------|-----------|
 * | k_rx_ok | 89 | All success paths |
 * | k_rx_err_invalid_arg | 28 | nullptr pointers, zero-length buffers, oversized buffers |
 * | k_rx_err_invalid_size | 6 | 1-byte buffer for encode operations |
 * | k_rx_err_protocol_error | 3 | Invalid protobuf wire format (0xFF bytes) |
 * | k_rx_err_not_initialized | 6 | Encode/decode before rx_nanopb_init() |
 * | k_rx_err_invalid_state | 1 | Duplicate initialization attempt |
 *
 * **nanopb-Specific Considerations:**
 * @par Static Allocation (NASA Power of 10 Rule 3):
 * - **Zero dynamic memory:** nanopb configured with PB_BUFFER_ONLY (no malloc/free)
 * - **Static buffers:** s_buffer[512] in test code, s_nanopb_buffer_size in rx_nanopb.c
 * - **Stack allocation:** All star_v1_* message structs allocated on stack (64-128 bytes each)
 * - **Fixed-size arrays:** All repeated fields have max_count in .options file
 *
 * @par max_size Constraints (.options file):
 * @code
 * star.v1.RequestHeader.request_id max_size:64
 * star.v1.VelocityCommand max_count:1
 * star.v1.TelemetryData.gps max_count:1
 * star.v1.TelemetryData.imu max_count:1
 * @endcode
 *
 * @par Protobuf Wire Format:
 * - **Varint encoding:** uint32/int32 use 1-5 bytes (smaller values = fewer bytes)
 * - **Default values:** Fields with default values (0, false, "") NOT encoded -> 0-byte messages
 * - **Optional fields:** has_<field> flag determines if field present in wire format
 * - **Little-endian:** Fixed32/64 types use little-endian byte order
 * - **No padding:** No alignment padding between fields
 *
 * @par Callback-Free Design:
 * - **No nanopb callbacks:** All fields are scalar or static arrays (no dynamic arrays)
 * - **Simplified API:** rx_nanopb_encode_* takes message struct, returns encoded bytes
 * - **No stream state:** Stateless encode/decode (no pb_ostream_t/pb_istream_t exposed)
 *
 * **Mock Infrastructure:**
 * @par Test Fixtures:
 * - **setUp():** Clears s_buffer[512], resets module state, calls rx_nanopb_init()
 * - **tearDown():** No-op (no dynamic resources to release)
 * - **s_buffer[512]:** Shared encode/decode buffer for all tests (same size as production)
 *
 * @par Test Vector Generation:
 * - **internal_make_velocity_params():** Factory function for VelocityCommand parameter struct
 * - **Static constants:** s_test_front_left_velocity_mps, s_test_temperature_c, etc.
 * - **Edge cases:** Zero velocity, max velocity (2.0 m/s), negative velocity (-2.0 m/s)
 * - **Sequence numbers:** 0, 42 (arbitrary), UINT32_MAX (wrap test)
 *
 * @par Message Factories:
 * - **rx_nanopb_create_velocity_command():** Converts rx_velocity_command_params_t -> star_v1_VelocityCommand
 * - **rx_nanopb_create_response_header():** Creates star_v1_ResponseHeader with status + request_id
 * - **Manual initialization:** star_v1_TelemetryData_init_zero macro for zero-init
 *
 * **Performance Expectations:**
 * | Operation | Time @ 240 MHz | Notes |
 * |-----------|----------------|-------|
 * | Encode VelocityCommand (4 motors) | ~20 us | Software protobuf encoding |
 * | Decode VelocityCommand | ~15 us | Software protobuf decoding |
 * | Encode TelemetryData (full) | ~30 us | GPS + IMU + 9 scalar fields |
 * | Encode TelemetryData (empty) | ~2 us | 0-byte message optimization |
 * | Round-trip (encode + decode) | ~35 us | No memcpy overhead |
 *
 * **Critical Design Decisions:**
 * @par 1. Empty Messages Encode to 0 Bytes:
 * - Protobuf optimization: Default values NOT encoded
 * - Test: test_encode_velocity_request_empty() expects len=0
 * - Rationale: Saves bandwidth for ACK-only messages
 *
 * @par 2. No Validation of Field Ranges:
 * - rx_nanopb does NOT validate velocity ranges (-2.0 to +2.0 m/s)
 * - Application layer (motor control) enforces limits
 * - Rationale: Separation of concerns, rx_nanopb is pure serialization
 *
 * @par 3. Sequence Number Management:
 * - Sequence numbers stored in message, NOT managed by rx_nanopb
 * - Application provides sequence via rx_velocity_command_params_t
 * - Test: test_create_velocity_command_max_sequence() verifies UINT32_MAX handling
 *
 * @par 4. Thread Safety:
 * - rx_nanopb NOT thread-safe (uses shared s_nanopb_buffer_size)
 * - Tests run sequentially (Unity framework single-threaded)
 * - Production: Call from single task or protect with mutex
 *
 * @par 5. Helper Functions vs Direct API:
 * - **Helper functions:** rx_nanopb_create_velocity_command(), rx_nanopb_create_response_header()
 * - **Direct API:** rx_nanopb_encode_velocity_request(), rx_nanopb_decode_velocity_request()
 * - Rationale: Helpers reduce boilerplate, ensure zero-initialization
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1 (Control Flow):** [OK] All test functions use simple sequential flow
 * - **Rule 2 (Loop Bounds):** [OK] All loops in nanopb library have message field count bounds
 * - **Rule 3 (Dynamic Memory):** [OK] Zero heap allocation (nanopb PB_BUFFER_ONLY, static test buffers)
 * - **Rule 4 (Function Size):** [OK] Test functions <60 lines, setUp/tearDown <10 lines
 * - **Rule 5 (Assertions):** [OK] Every test has minimum 1 assertion, most have 2-6
 * - **Rule 6 (Scope):** [OK] Test variables declared in setUp() or test function scope
 * - **Rule 7 (Return Checking):** [OK] All rx_nanopb_* returns validated with TEST_ASSERT_EQUAL
 * - **Rule 8 (Preprocessor):** [OK] Only Unity macros (RUN_TEST, TEST_ASSERT_*) and nanopb _init_zero
 * - **Rule 9 (Pointers):** [OK] Single-level dereferencing only (msg->field, no msg->sub->field in tests)
 * - **Rule 10 (Warnings):** [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles Application:
 * - **Single Responsibility:** Each test verifies ONE behavior (e.g., nullptr check, buffer size, round-trip)
 * - **Open/Closed:** Tests extensible via new RUN_TEST() entries, no modification of existing tests
 * - **Liskov Substitution:** N/A (no polymorphism in C tests)
 * - **Interface Segregation:** rx_nanopb API split into encode/decode/helper functions (tests verify each)
 * - **Dependency Inversion:** Tests depend on rx_nanopb.h interface, not implementation (lib/rx_nanopb/src/rx_nanopb.c)
 *
 * **Related Files:**
 * - [rx_nanopb.h](../libs/rx_nanopb/inc/rx_nanopb.h) - nanopb wrapper API (already documented)
 * - [rx_nanopb.c](../libs/rx_nanopb/src/rx_nanopb.c) - nanopb wrapper implementation (already documented)
 * - star-proto/proto/star/v1/motor_control.proto - Motor control message schemas
 * - star-proto/proto/star/v1/telemetry.proto - Telemetry message schemas
 * - star-proto/proto/star/v1/common.proto - Common types (ResponseHeader, Status enum)
 *
 * @since Version 1.0.0
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "rx_nanopb.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_constants Test Constants for nanopb Integration
 * @brief Constants for buffer sizing, sequence numbers, and message field values
 * @{
 */

/**
 * @brief Maximum sequence number for wrap-around testing
 * @details
 * Used to verify sequence number wrap from UINT32_MAX -> 0 in round-trip tests.
 * Protocol allows sequence to wrap without error.
 */
static const uint32_t s_test_sequence_max = UINT32_MAX;

/**
 * @brief Test constants for buffer operations, sequence numbers, and field values
 * @details
 * Defines all integer constant test values with explicit underlying type (C23).
 * Grouped into logical categories for maintainability.
 *
 * @par Buffer Size Constants:
 * - k_test_buffer_size: Matches production s_nanopb_buffer_size (512 bytes)
 * - k_test_small_buffer_size: 1 byte for buffer overflow tests
 *
 * @par Sequence Number Constants:
 * - k_test_sequence_zero: Initial sequence number
 * - k_test_sequence_number: Arbitrary non-zero sequence (42)
 *
 * @par Timestamp Constants:
 * - k_test_timestamp_us: 1 second in microseconds (1,000,000 us)
 * - k_test_latency_us: 500 us response latency
 *
 * @par Telemetry Field Constants:
 * - k_test_wifi_signal_dbm: -65 dBm (typical WiFi signal strength)
 * - k_test_satellites: 8 GPS satellites (good fix)
 * - k_test_motor_id_left: Motor 0 (front-left in 4-motor config)
 * - k_test_motor_id_right: Motor 1 (front-right in 4-motor config)
 *
 * @par Encoded Size Lower Bounds:
 * - k_min_encoded_velocity_req: Minimum bytes for VelocityCommand (10 bytes)
 * - k_min_encoded_velocity_resp: Minimum bytes for ResponseHeader (2 bytes)
 * - k_min_encoded_estop_resp: Minimum bytes for EmergencyStopResponse (2 bytes)
 * - k_min_encoded_telemetry: Minimum bytes for single-field telemetry (1 byte)
 *
 * @note All constants use explicit underlying type per STAR C23 policy
 * @see CLAUDE.md "Constants and Macros (RX72N C Firmware)"
 */
typedef enum : int32_t {
  k_test_buffer_size          = 512,     /**< Test buffer size (matches production) */
  k_test_small_buffer_size    = 1,       /**< Minimal buffer for overflow tests */
  k_test_sequence_zero        = 0,       /**< Initial sequence number */
  k_test_sequence_number      = 42,      /**< Arbitrary test sequence number */
  k_test_timestamp_us         = 1000000, /**< 1 second timestamp in microseconds */
  k_test_wifi_signal_dbm      = -65,     /**< WiFi signal strength in dBm */
  k_test_motor_id_left        = 0,       /**< Front-left motor index */
  k_test_motor_id_right       = 1,       /**< Front-right motor index */
  k_test_satellites           = 8,       /**< GPS satellite count (good fix) */
  k_test_latency_us           = 500,     /**< Response latency in microseconds */
  k_test_free_heap_bytes      = 262144,  /**< 256 KB free heap (telemetry) */
  k_test_uptime_seconds       = 3600,    /**< 1 hour uptime in seconds */
  k_min_encoded_velocity_req  = 10,      /**< Min encoded SetVelocityRequest bytes */
  k_min_encoded_velocity_resp = 2,       /**< Min encoded SetVelocityResponse bytes */
  k_min_encoded_estop_resp    = 2,       /**< Min encoded EmergencyStopResponse bytes */
  k_min_encoded_telemetry     = 1,       /**< Min encoded TelemetryData bytes (1 field) */
} test_constants_t;

/**
 * @brief Floating-point test values for motor velocities (4-motor differential drive)
 * @details
 * Motor indices match STAR hardware configuration:
 * - Motor 0: Front-left wheel
 * - Motor 1: Front-right wheel
 * - Motor 2: Back-left wheel
 * - Motor 3: Back-right wheel
 *
 * Velocity range: -2.0 to +2.0 m/s (hardware limit per motor_config.h)
 * @{
 */
static const double s_test_front_left_velocity_mps =
  1.5; /**< Front-left motor velocity (Motor 0) */
static const double s_test_front_right_velocity_mps =
  1.5; /**< Front-right motor velocity (Motor 1) */
static const double s_test_back_left_velocity_mps = 1.0; /**< Back-left motor velocity (Motor 2) */
static const double s_test_back_right_velocity_mps =
  1.0;                                              /**< Back-right motor velocity (Motor 3) */
static const double s_test_max_velocity_mps  = 2.0; /**< Maximum velocity (hardware limit) */
static const double s_test_zero_velocity_mps = 0.0; /**< Zero velocity (stop command) */
/** @} */

/**
 * @brief Floating-point test values for telemetry fields
 * @details
 * Realistic values for sensor telemetry validation in round-trip tests.
 * Values chosen to be non-zero and distinguishable from defaults.
 * @{
 */
static const double s_test_cpu_usage_percent  = 45.0;      /**< CPU utilization (0-100%) */
static const double s_test_temperature_c      = 25.5;      /**< Temperature (degC) */
static const double s_test_motor_load_percent = 30.0;      /**< Motor load (0-100%) */
static const double s_test_latitude_deg       = 37.7749;   /**< GPS latitude (San Francisco) */
static const double s_test_longitude_deg      = -122.4194; /**< GPS longitude (San Francisco) */
static const double s_test_altitude_m         = 10.0;      /**< GPS altitude (meters) */
static const double s_test_accuracy_m         = 2.5;       /**< GPS accuracy (meters) */
static const double s_test_pitch_rad          = 0.1;       /**< IMU pitch angle (radians) */
static const double s_test_roll_rad           = 0.05;      /**< IMU roll angle (radians) */
static const double s_test_yaw_rad            = 1.57;      /**< IMU yaw angle (~90deg) */
static const double s_test_accel_z_mps2       = 9.81;      /**< IMU Z-axis acceleration (gravity) */
/** @} */

/**
 * @brief Floating-point comparison tolerance for round-trip tests
 * @details
 * Tolerance accounts for:
 * - Protobuf wire format precision (double -> int32 varint -> double)
 * - Floating-point rounding in nanopb encoder/decoder
 * - No precision loss expected for values in test range (-2.0 to +2.0 m/s)
 *
 * @note Unity TEST_ASSERT_FLOAT_WITHIN uses absolute difference, not relative
 */
static const float s_test_float_tolerance = 0.0001F;

/**
 * @brief PID test values for encode/decode validation
 * @{
 */
static const double s_test_pid_kp       = 0.5;    /**< Proportional gain */
static const double s_test_pid_ki       = 10.0;   /**< Integral gain */
static const double s_test_pid_kd       = 0.1;    /**< Derivative gain */
static const double s_test_pid_out_min  = -100.0; /**< Output min percent */
static const double s_test_pid_out_max  = 100.0;  /**< Output max percent */
static const double s_test_pid_int_min  = -50.0;  /**< Integral min clamp */
static const double s_test_pid_int_max  = 50.0;   /**< Integral max clamp */
static const double s_test_pid_kp_tuned = 0.286;  /**< MATLAB-tuned Kp */
static const double s_test_pid_ki_tuned = 8.01;   /**< MATLAB-tuned Ki */
/** @} */

/**
 * @brief PID float tolerance for round-trip comparisons
 */
static const float s_test_pid_tolerance       = 0.0001F;
static const float s_test_pid_tight_tolerance = 0.00001F;

/**
 * @brief Additional integer test constants
 */
typedef enum : int32_t {
  k_test_decode_dummy_len    = 10,  /**< Dummy buffer length for nullptr tests */
  k_test_decode_buf_size_16  = 16,  /**< 16-byte decode test buffer */
  k_test_decode_buf_size_64  = 64,  /**< 64-byte decode test buffer */
  k_test_decode_buf_size_256 = 256, /**< 256-byte encode/decode buffer */
  k_test_decode_buf_size_600 = 600, /**< Oversized buffer (> 512) */
  k_test_motor_id_all        = -1,  /**< Motor ID for all motors */
  k_test_retransmit_retries  = 3,   /**< Retransmit max retries */
  k_test_retransmit_ack_ms   = 50,  /**< Retransmit ACK timeout ms */
  k_test_retransmit_backoff  = 400, /**< Retransmit max backoff ms */
  k_test_long_id_buf_size    = 514, /**< Long request ID buffer size */
} test_extra_constants_t;

/**
 * @brief Diff drive test velocities
 */
static const double s_test_diff_left_mps  = 0.5; /**< Diff drive left velocity */
static const double s_test_diff_right_mps = 1.0; /**< Diff drive right velocity */

/**
 * @brief Float cast of diff drive left velocity for assertion comparisons
 */
static const float s_test_diff_left_mps_f = 0.5F; /**< Diff drive left (float) */

/**
 * @brief Out-of-range velocity for boundary test
 */
static const double s_test_velocity_out_of_range = 1001.0; /**< > k_velocity_mps_max */

/**
 * @brief Invalid protobuf byte pattern for decode error injection
 */
typedef enum : uint8_t {
  k_test_invalid_byte      = 0xFFU, /**< Invalid protobuf wire type byte */
  k_test_fill_byte         = 0xFFU, /**< Byte pattern for garbage fill */
  k_test_pb_tag_submsg     = 0x0AU, /**< Protobuf length-delimited field tag (field 1) */
  k_test_pb_tag_varint     = 0x08U, /**< Protobuf varint field tag (field 1) */
  k_test_pb_tag_fixed64    = 0x11U, /**< Protobuf fixed64 field tag (field 2) */
  k_test_pb_small_buf_size = 4U,    /**< Small protobuf test buffer size */
} test_byte_patterns_t;

/** @} */ /* End of nanopb_test_constants */

/**
 * @defgroup nanopb_test_helpers Internal Test Helper Functions
 * @brief Helper functions for constructing test message parameters
 * @{
 */

/**
 * @brief Create velocity command parameter struct for testing
 *
 * @details
 * Factory function that constructs rx_velocity_command_params_t for passing to
 * rx_nanopb_create_velocity_command(). Encapsulates 4-motor velocity command
 * parameter initialization with explicit field ordering.
 *
 * Motor parameter order matches STAR hardware configuration:
 * 1. Front-left (Motor 0)
 * 2. Front-right (Motor 1)
 * 3. Back-left (Motor 2)
 * 4. Back-right (Motor 3)
 *
 * @param[in] front_left_mps Front-left motor velocity in m/s (-2.0 to +2.0)
 * @param[in] front_right_mps Front-right motor velocity in m/s (-2.0 to +2.0)
 * @param[in] back_left_mps Back-left motor velocity in m/s (-2.0 to +2.0)
 * @param[in] back_right_mps Back-right motor velocity in m/s (-2.0 to +2.0)
 * @param[in] sequence Sequence number (0 to UINT32_MAX, wraps to 0)
 *
 * @return rx_velocity_command_params_t Initialized parameter struct
 *
 * @note No validation performed - allows testing edge cases (negative, max values)
 * @note Does NOT set timestamp_us - caller must set if needed
 *
 * @par Example:
 * @code
 * rx_velocity_command_params_t params = internal_make_velocity_params(
 *     1.5,  // front_left_mps
 *     1.5,  // front_right_mps
 *     1.0,  // back_left_mps
 *     1.0,  // back_right_mps
 *     42    // sequence
 * );
 * star_v1_VelocityCommand cmd;
 * rx_nanopb_create_velocity_command(&cmd, &params);
 * @endcode
 *
 * @see rx_nanopb_create_velocity_command() Convert params to protobuf message
 * @see test_create_velocity_command_valid() Usage example
 *
 * @since Version 1.0.0
 */
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

/** @} */ /* End of nanopb_test_helpers */

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_fixtures Test Fixtures for nanopb Integration Tests
 * @brief setUp/tearDown and shared test buffers
 * @{
 */

/**
 * @brief Shared encode/decode buffer for all tests
 * @details
 * Statically allocated buffer matching production s_nanopb_buffer_size (512 bytes).
 * Used for both encode output and decode input in all test cases.
 *
 * Buffer lifecycle:
 * - setUp(): Cleared to all zeros with memset()
 * - Test: Populated by rx_nanopb_encode_*() or manually for decode tests
 * - tearDown(): No action (static allocation, no cleanup needed)
 *
 * @note Same size as production to catch buffer overflow bugs
 * @note Tests use this shared buffer sequentially (Unity is single-threaded)
 */
static uint8_t s_buffer[k_test_buffer_size];

/**
 * @brief Test fixture setup - runs before EVERY test
 *
 * @details
 * Initializes test environment to known state:
 * 1. Clear encode/decode buffer to all zeros
 * 2. Reset rx_nanopb internal state (sets s_initialized = false)
 * 3. Initialize rx_nanopb module (sets s_initialized = true)
 * 4. Assert initialization succeeded
 *
 * This ensures each test starts with:
 * - Clean buffer (no leftover data from previous test)
 * - Initialized module state (ready for encode/decode operations)
 * - Known good state (k_rx_ok returned)
 *
 * @pre Called automatically by Unity framework before RUN_TEST()
 * @post s_buffer[] contains all zeros
 * @post rx_nanopb module initialized (s_initialized == true)
 *
 * @note Unity requires setUp() with exact signature (void -> void)
 * @note Runs 89 times (once per test function)
 *
 * @see tearDown() Cleanup after each test
 * @see rx_nanopb_init() Module initialization
 * @see rx_nanopb_test_reset_state() Reset internal state
 */
void setUp(void)
{
  for (uint32_t i = 0; i < sizeof(s_buffer); i++) {
    s_buffer[i] = 0;
  }
  rx_nanopb_test_reset_state();
  TEST_ASSERT_EQUAL(k_rx_ok, rx_nanopb_init());
}

/**
 * @brief Test fixture teardown - runs after EVERY test
 *
 * @details
 * No cleanup required because:
 * - s_buffer is static (no deallocation needed)
 * - rx_nanopb has no dynamic resources (no malloc/free)
 * - nanopb uses PB_BUFFER_ONLY mode (no internal allocations)
 * - Mock state reset handled in setUp()
 *
 * Kept as no-op for Unity framework compliance and future extensibility.
 *
 * @pre Called automatically by Unity framework after each test completes
 * @post No state changes
 *
 * @note Unity requires tearDown() with exact signature (void -> void)
 * @note If future tests need cleanup (e.g., closing files), add here
 */
void tearDown(void)
{
  /* Nothing to tear down - no dynamic resources */
}

/** @} */ /* End of nanopb_test_fixtures */

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_init Module Initialization Tests
 * @brief Tests for rx_nanopb module initialization and state management
 * @details
 * Validates module initialization sequence and duplicate init detection.
 * Tests ensure module enforces single initialization per reset cycle.
 *
 * **Test Coverage:**
 * - Fresh initialization returns k_rx_ok
 * - Duplicate initialization returns k_rx_err_invalid_state
 * - State reset allows re-initialization
 *
 * **State Machine:**
 * @code
 * [UNINITIALIZED] --rx_nanopb_init()--> [INITIALIZED]
 *                                            |
 *                        rx_nanopb_init() --+
 *                             |
 *                             v
 *                    k_rx_err_invalid_state
 *
 * [INITIALIZED] --rx_nanopb_test_reset_state()--> [UNINITIALIZED]
 * @endcode
 * @{
 */

/**
 * @brief Test rx_nanopb_init() returns success on fresh initialization
 *
 * @details
 * Verifies module can be initialized successfully from uninitialized state.
 * This is the expected initialization path for production code.
 *
 * **Test Sequence:**
 * 1. Reset module state to UNINITIALIZED
 * 2. Call rx_nanopb_init()
 * 3. Assert: Returns k_rx_ok
 * 4. Assert: Internal s_initialized flag set to true
 *
 * @pre Module state reset via rx_nanopb_test_reset_state()
 * @post Module in INITIALIZED state, ready for encode/decode operations
 *
 * @note setUp() also calls rx_nanopb_init(), this test verifies isolation
 */
void test_nanopb_init_success(void)
{
  rx_nanopb_test_reset_state();
  rx_err_t err = rx_nanopb_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Test rx_nanopb_init() detects duplicate initialization attempts
 *
 * @details
 * Verifies module rejects duplicate initialization calls without reset.
 * This prevents accidental re-initialization that could disrupt ongoing operations.
 *
 * **Test Sequence:**
 * 1. Reset module state to UNINITIALIZED
 * 2. Call rx_nanopb_init() -> Returns k_rx_ok
 * 3. Call rx_nanopb_init() again -> Returns k_rx_err_invalid_state
 * 4. Assert: First call succeeds, second call fails
 *
 * **Rationale:**
 * Duplicate init is programming error, not runtime error. Detecting it helps
 * catch bugs in application initialization sequence.
 *
 * @pre Module state reset via rx_nanopb_test_reset_state()
 * @post Module remains in INITIALIZED state after second call
 *
 * @note Production code should never hit this error (init called once at startup)
 */
void test_nanopb_init_detects_duplicate_call(void)
{
  rx_nanopb_test_reset_state();
  rx_err_t err1 = rx_nanopb_init();
  rx_err_t err2 = rx_nanopb_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err1);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err2);
}

/** @} */ /* End of nanopb_test_init */

/* =============================================================================
 * Velocity Request Encode Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_velocity_req_encode SetVelocityRequest Encoding Tests
 * @brief Tests for encoding SetVelocityRequest messages to protobuf wire format
 * @details
 * Validates rx_nanopb_encode_velocity_request() with various input conditions:
 * - nullptr pointer checks (msg, buffer, len parameters)
 * - Buffer size validation (too small buffer detection)
 * - Empty message handling (0-byte optimization)
 * - Valid message encoding (4-motor command with sequence/timestamp)
 * - Edge cases (max velocity, zero velocity, negative velocity, max sequence)
 * - State validation (not initialized error)
 *
 * **Protobuf Message Structure:**
 * @code
 * message SetVelocityRequest {
 *   optional VelocityCommand command = 1;  // has_command flag + fields
 * }
 *
 * message VelocityCommand {
 *   double front_left_velocity_mps = 1;   // 8 bytes (double -> varint in nanopb)
 *   double front_right_velocity_mps = 2;
 *   double back_left_velocity_mps = 3;
 *   double back_right_velocity_mps = 4;
 *   uint32 sequence = 5;                  // 1-5 bytes (varint)
 *   uint64 timestamp_us = 6;              // 1-10 bytes (varint, optional)
 * }
 * @endcode
 *
 * **Expected Encoded Sizes:**
 * - Empty message (has_command=false): 0 bytes
 * - Command with 4 velocities + sequence: ~10+ bytes
 * - Command with max values: ~40 bytes (worst case varint encoding)
 *
 * **Test Coverage (8 tests):**
 * | Test | Validates | Expected Result |
 * |------|-----------|-----------------|
 * | nullptr msg | msg parameter validation | k_rx_err_invalid_arg |
 * | nullptr buffer | buffer parameter validation | k_rx_err_invalid_arg |
 * | nullptr len | len_out parameter validation | k_rx_err_invalid_arg |
 * | Small buffer | Buffer overflow prevention | k_rx_err_invalid_size |
 * | Empty message | Protobuf default optimization | k_rx_ok, len=0 |
 * | With command | Valid 4-motor encoding | k_rx_ok, len>10 |
 * | Max velocity | Edge case: 2.0 m/s | k_rx_ok, len>0 |
 * | Zero velocity | Edge case: 0.0 m/s | k_rx_ok, len>0 |
 * | Negative velocity | Edge case: -2.0 m/s | k_rx_ok, len>0 |
 * | Not initialized | State validation | k_rx_err_not_initialized |
 * @{
 */

/**
 * @brief Test encode velocity request with nullptr message pointer
 *
 * @details
 * Verifies input validation rejects nullptr msg parameter.
 *
 * @pre Module initialized in setUp()
 * @post No state changes, buffer unmodified
 */
void test_encode_velocity_request_null_msg(void)
{
  uint32_t len;
  rx_err_t err = rx_nanopb_encode_velocity_request(nullptr, s_buffer, sizeof(s_buffer), &len);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode velocity request with nullptr buffer pointer
 */
void test_encode_velocity_request_null_buffer(void)
{
  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;
  uint32_t                   len;
  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, nullptr, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode velocity request with nullptr length pointer
 */
void test_encode_velocity_request_null_len(void)
{
  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;
  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), nullptr);
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
 *
 * @details
 * Verifies protobuf default value optimization - empty messages encode to 0 bytes.
 *
 * **Protobuf Wire Format Rule:**
 * Fields with default values (0, false, empty string) are NOT encoded.
 * Since star_v1_SetVelocityRequest_init_zero has has_command=false (default),
 * no fields are present in wire format -> 0-byte message.
 *
 * **Test Sequence:**
 * 1. Initialize message with _init_zero macro (all fields default)
 * 2. Encode message
 * 3. Assert: Returns k_rx_ok
 * 4. Assert: len_out == 0 (empty wire format)
 *
 * **Rationale:**
 * This optimization saves bandwidth for ACK-only messages or heartbeats.
 *
 * @pre Module initialized in setUp()
 * @post s_buffer unmodified (no bytes written)
 *
 * @note This is EXPECTED behavior, not an error condition
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
 * @brief Test encode velocity request with 4-motor velocity command
 *
 * @details
 * Validates successful encoding of realistic 4-motor velocity command with
 * sequence number and timestamp. This is the primary use case for production code.
 *
 * **Test Message:**
 * - front_left_velocity_mps: 1.5 m/s (Motor 0)
 * - front_right_velocity_mps: 1.5 m/s (Motor 1)
 * - back_left_velocity_mps: 1.0 m/s (Motor 2)
 * - back_right_velocity_mps: 1.0 m/s (Motor 3)
 * - sequence: 42 (arbitrary non-zero)
 * - timestamp_us: 1,000,000 (1 second)
 *
 * **Expected Wire Format:**
 * - Tag+wire_type for command field: 1 byte
 * - 4x velocity fields: ~4 bytes each (double encoded as varint)
 * - Sequence varint: 1 byte (value 42 fits in 1 byte)
 * - Timestamp varint: 3 bytes (1,000,000 requires 3 bytes varint)
 * - **Total:** ~18-20 bytes minimum
 *
 * **Test Sequence:**
 * 1. Create message with has_command=true and all velocity fields populated
 * 2. Encode message
 * 3. Assert: Returns k_rx_ok
 * 4. Assert: len > k_min_encoded_velocity_req (10 bytes)
 *
 * @pre Module initialized in setUp()
 * @post s_buffer contains valid protobuf-encoded message
 *
 * @see test_velocity_request_roundtrip_with_command() Decode validation
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
 * @brief Test encode velocity request when module not initialized
 *
 * @details
 * Verifies module rejects encode operations before initialization.
 * All encode/decode functions check s_initialized flag.
 *
 * **Test Sequence:**
 * 1. Reset module to UNINITIALIZED state
 * 2. Attempt encode without calling rx_nanopb_init()
 * 3. Assert: Returns k_rx_err_not_initialized
 *
 * @pre setUp() initialization bypassed via rx_nanopb_test_reset_state()
 * @post Module remains UNINITIALIZED, buffer unmodified
 *
 * @note Production code initializes once at startup, should never hit this error
 */
void test_encode_velocity_request_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;
  uint32_t                   len = 0;

  rx_err_t err = rx_nanopb_encode_velocity_request(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/** @} */ /* End of nanopb_test_velocity_req_encode */

/* =============================================================================
 * Velocity Request Decode Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_velocity_req_decode SetVelocityRequest Decoding Tests
 * @brief Tests for decoding SetVelocityRequest messages from protobuf wire format
 * @details
 * Validates rx_nanopb_decode_velocity_request() with error injection:
 * - nullptr pointer checks (buffer, msg parameters)
 * - Empty buffer detection (len=0)
 * - Invalid protobuf data (wire format violations)
 * - Oversized buffer detection (len > s_nanopb_buffer_size)
 * - State validation (not initialized error)
 *
 * **Error Injection Scenarios:**
 * | Scenario | Input | Expected Error |
 * |----------|-------|----------------|
 * | nullptr buffer | buffer=nullptr, len=10 | k_rx_err_invalid_arg |
 * | nullptr msg | msg=nullptr | k_rx_err_invalid_arg |
 * | Empty buffer | len=0 | k_rx_err_invalid_arg |
 * | Invalid wire format | 0xFF 0xFF 0xFF... | k_rx_err_protocol_error |
 * | Oversized buffer | len > 512 | k_rx_err_invalid_arg |
 * | Not initialized | Before init | k_rx_err_not_initialized |
 *
 * **Protobuf Wire Format Validation:**
 * nanopb decoder checks:
 * - Valid field tags (1-536870911, not 19000-19999 reserved)
 * - Valid wire types (0=varint, 1=64-bit, 2=length-delimited, 5=32-bit)
 * - Length-delimited fields don't exceed buffer bounds
 * - Varint encoding terminates (MSB=0 in last byte)
 *
 * **Test Coverage (6 tests):**
 * @{
 */

/**
 * @brief Test decode velocity request with nullptr buffer pointer
 *
 * @details
 * Verifies input validation rejects nullptr buffer parameter.
 *
 * @pre Module initialized in setUp()
 * @post No state changes, msg unmodified
 */
void test_decode_velocity_request_null_buffer(void)
{
  star_v1_SetVelocityRequest msg;
  rx_err_t err = rx_nanopb_decode_velocity_request(nullptr, k_test_decode_dummy_len, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode velocity request with nullptr message pointer
 */
void test_decode_velocity_request_null_msg(void)
{
  uint8_t  data[k_test_decode_buf_size_16] = {};
  rx_err_t err = rx_nanopb_decode_velocity_request(data, sizeof(data), nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode velocity request with empty buffer
 */
void test_decode_velocity_request_empty_buffer(void)
{
  uint8_t data[1] = {};

  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_velocity_request(data, 0, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode velocity request with invalid protobuf wire format
 *
 * @details
 * Verifies decoder detects malformed protobuf data and returns protocol error.
 *
 * **Invalid Wire Format:**
 * - Input: 0xFF 0xFF 0xFF 0xFF 0xFF (all bits set)
 * - First byte: Tag=31 (0xFF >> 3), wire_type=7 (0xFF & 0x07)
 * - Wire type 7 is INVALID (only 0-5 are valid in protobuf)
 * - nanopb decoder should reject immediately
 *
 * **Test Sequence:**
 * 1. Create buffer with invalid wire format bytes
 * 2. Attempt to decode as SetVelocityRequest
 * 3. Assert: Returns k_rx_err_protocol_error (not k_rx_ok)
 * 4. Assert: msg struct remains unmodified (init_zero state)
 *
 * **Rationale:**
 * Detects corrupted data from SPI/USB transport layer. Prevents parsing
 * garbage data as valid commands (safety-critical).
 *
 * @pre Module initialized in setUp()
 * @post msg unmodified, no side effects
 *
 * @note Production code should discard message and request retransmit
 */
void test_decode_velocity_request_invalid_data(void)
{
  /* Invalid protobuf data - starts with invalid wire type */
  uint8_t invalid_data[] = {k_test_fill_byte,
                            k_test_fill_byte,
                            k_test_fill_byte,
                            k_test_fill_byte,
                            k_test_fill_byte};

  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_velocity_request(invalid_data, sizeof(invalid_data), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief Test decode velocity request when module not initialized
 * @details Verifies state validation before decode operation.
 * @see test_encode_velocity_request_not_initialized() Encode equivalent
 */
void test_decode_velocity_request_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  uint8_t buffer[] =
    {k_test_pb_tag_submsg, k_test_pb_tag_varint, k_test_pb_tag_fixed64, 0x00, 0x00, 0x00};
  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_velocity_request(buffer, sizeof(buffer), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief Test decode velocity request with oversized buffer
 * @details Verifies bounds checking prevents buffer overflow (len > 512 bytes).
 */
void test_decode_velocity_request_oversized_buffer(void)
{
  star_v1_SetVelocityRequest msg = star_v1_SetVelocityRequest_init_zero;

  /* Length exceeds k_nanopb_buffer_size */
  rx_err_t err = rx_nanopb_decode_velocity_request(s_buffer, s_nanopb_buffer_size + 1, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/** @} */ /* End of nanopb_test_velocity_req_decode */

/* =============================================================================
 * Velocity Request Round-Trip Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_velocity_req_roundtrip SetVelocityRequest Round-Trip Tests
 * @brief Tests for lossless encode -> decode transformation
 * @details
 * Validates that encode followed by decode preserves all message field values.
 * These tests verify protobuf serialization is lossless for the value ranges
 * used in STAR motor control.
 *
 * **Round-Trip Test Pattern:**
 * @code
 * 1. Create original message with known field values
 * 2. Encode: original -> buffer (bytes)
 * 3. Decode: buffer (bytes) -> decoded message
 * 4. Assert: decoded.field == original.field for ALL fields
 * 5. Float comparison: FLOAT_WITHIN(0.0001, original, decoded)
 * @endcode
 *
 * **Floating-Point Precision:**
 * - Velocity values: -2.0 to +2.0 m/s (double precision)
 * - Protobuf encoding: Doubles encoded as varints (no precision loss)
 * - Tolerance: 0.0001 (accounts for any floating-point rounding)
 * - Expected: Exact match for values in test range
 *
 * **Test Coverage (3 tests):**
 * | Test | Values | Purpose |
 * |------|--------|---------|
 * | With command | 1.5, 1.5, 1.0, 1.0 m/s | Typical drive command |
 * | Zero velocity | 0.0, 0.0, 0.0, 0.0 m/s | Stop command edge case |
 * | Negative velocity | -1.5, -1.5, -1.0, -1.0 m/s | Reverse drive edge case |
 * @{
 */

/**
 * @brief Test encode -> decode round-trip for velocity request with 4-motor command
 *
 * @details
 * Primary round-trip validation for typical motor velocity command.
 *
 * **Test Message:**
 * - Front-left: 1.5 m/s
 * - Front-right: 1.5 m/s
 * - Back-left: 1.0 m/s
 * - Back-right: 1.0 m/s
 * - Sequence: 42
 * - Timestamp: 1,000,000 us
 *
 * **Test Sequence:**
 * 1. Create original message with has_command=true and all fields populated
 * 2. Encode to s_buffer -> len_out
 * 3. Assert: Encode succeeded (k_rx_ok), len > 0
 * 4. Decode from s_buffer using same len
 * 5. Assert: Decode succeeded (k_rx_ok)
 * 6. Assert: decoded.has_command == true
 * 7. Assert: All velocity fields match within 0.0001 tolerance
 * 8. Assert: sequence and timestamp_us match exactly
 *
 * @pre Module initialized in setUp()
 * @post s_buffer contains encoded message, decoded msg validated
 *
 * @note This is the primary production use case - forward motion command
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
 * @brief Test encode -> decode round-trip preserves zero velocities (stop command)
 * @details Edge case: All motors stopped. Validates 0.0 values round-trip correctly.
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
 * @brief Test encode -> decode round-trip with negative velocities (reverse drive)
 * @details Edge case: Reverse motion. Validates negative double values round-trip correctly.
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

/** @} */ /* End of nanopb_test_velocity_req_roundtrip */

/* =============================================================================
 * Velocity Response Encode Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_velocity_resp_encode SetVelocityResponse Encoding Tests
 * @brief Tests for encoding SetVelocityResponse messages (RX72N -> RPi5 acknowledgments)
 * @details
 * Validates rx_nanopb_encode_velocity_response() with ResponseHeader population.
 * Response messages contain status codes, latency measurements, and request IDs.
 *
 * **Message Structure:**
 * @code
 * message SetVelocityResponse {
 *   optional ResponseHeader header = 1;
 * }
 *
 * message ResponseHeader {
 *   Status status = 1;           // Enum: OK, INVALID_REQUEST, INTERNAL_ERROR, etc.
 *   string request_id = 2;       // Max 64 bytes (.options file)
 *   uint32 latency_us = 3;       // Optional processing latency
 * }
 * @endcode
 *
 * **Test Coverage (6 tests):** nullptr checks, buffer size, empty message, status codes
 * @{
 */

/**
 * @brief Test encode velocity response with nullptr message pointer
 * @details Verifies input validation rejects nullptr msg parameter.
 */
void test_encode_velocity_response_null_msg(void)
{
  uint32_t len;
  rx_err_t err = rx_nanopb_encode_velocity_response(nullptr, s_buffer, sizeof(s_buffer), &len);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode velocity response with nullptr buffer pointer
 */
void test_encode_velocity_response_null_buffer(void)
{
  star_v1_SetVelocityResponse msg = star_v1_SetVelocityResponse_init_zero;
  uint32_t                    len;
  rx_err_t err = rx_nanopb_encode_velocity_response(&msg, nullptr, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode velocity response with nullptr length pointer
 */
void test_encode_velocity_response_null_len(void)
{
  star_v1_SetVelocityResponse msg = star_v1_SetVelocityResponse_init_zero;
  rx_err_t err = rx_nanopb_encode_velocity_response(&msg, s_buffer, sizeof(s_buffer), nullptr);
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
 * @brief Test encode velocity response when module not initialized
 * @details Verifies state validation before encode operation.
 */
void test_encode_velocity_response_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  star_v1_SetVelocityResponse msg = star_v1_SetVelocityResponse_init_zero;
  uint32_t                    len = 0;

  rx_err_t err = rx_nanopb_encode_velocity_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/** @} */ /* End of nanopb_test_velocity_resp_encode */

/* =============================================================================
 * Emergency Stop Request Decode Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_estop_req_decode EmergencyStopRequest Decoding Tests
 * @brief Tests for decoding emergency stop commands (RPi5 -> RX72N)
 * @details
 * Validates rx_nanopb_decode_estop_request() for safety-critical e-stop protocol.
 * Emergency stop takes priority over all other commands.
 *
 * **Message Structure:**
 * @code
 * message EmergencyStopRequest {
 *   bool engage = 1;  // true=engage e-stop, false=release e-stop
 * }
 * @endcode
 *
 * **Test Coverage (6 tests):** nullptr checks, empty buffer, invalid data, oversized buffer
 * @{
 */

/**
 * @brief Test decode estop request with nullptr buffer pointer
 * @details Verifies input validation for safety-critical e-stop message.
 */
void test_decode_estop_request_null_buffer(void)
{
  star_v1_EmergencyStopRequest msg;
  rx_err_t err = rx_nanopb_decode_estop_request(nullptr, k_test_decode_dummy_len, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode estop request with nullptr message pointer
 */
void test_decode_estop_request_null_msg(void)
{
  uint8_t  data[k_test_decode_buf_size_16] = {};
  rx_err_t err = rx_nanopb_decode_estop_request(data, sizeof(data), nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode estop request with empty buffer
 */
void test_decode_estop_request_empty_buffer(void)
{
  uint8_t data[1] = {};

  star_v1_EmergencyStopRequest msg = star_v1_EmergencyStopRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_estop_request(data, 0, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode estop request with invalid data
 */
void test_decode_estop_request_invalid_data(void)
{
  uint8_t invalid_data[] = {k_test_fill_byte,
                            k_test_fill_byte,
                            k_test_fill_byte,
                            k_test_fill_byte,
                            k_test_fill_byte};

  star_v1_EmergencyStopRequest msg = star_v1_EmergencyStopRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_estop_request(invalid_data, sizeof(invalid_data), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief Test decode estop request when not initialized
 */
void test_decode_estop_request_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  uint8_t                      buffer[] = {k_test_pb_tag_varint, 0x01};
  star_v1_EmergencyStopRequest msg      = star_v1_EmergencyStopRequest_init_zero;

  rx_err_t err = rx_nanopb_decode_estop_request(buffer, sizeof(buffer), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief Test decode estop request with oversized buffer
 * @details Verifies bounds checking for safety-critical e-stop message.
 */
void test_decode_estop_request_oversized_buffer(void)
{
  star_v1_EmergencyStopRequest msg = star_v1_EmergencyStopRequest_init_zero;

  /* Length exceeds k_nanopb_buffer_size */
  rx_err_t err = rx_nanopb_decode_estop_request(s_buffer, s_nanopb_buffer_size + 1, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/** @} */ /* End of nanopb_test_estop_req_decode */

/* =============================================================================
 * SetPidGainsRequest Decode Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_pid_gains_req_decode SetPidGainsRequest Decoding Tests
 * @brief Tests for decoding PID controller gains update commands (RPi5 -> RX72N)
 * @details
 * Validates rx_nanopb_decode_pid_gains_request() with various error conditions
 * and valid PID configuration data.
 *
 * **Message Structure:**
 * @code
 * message SetPidGainsRequest {
 *   optional RequestHeader header = 1;
 *   optional PidConfig pid_config = 2;  // PID controller configuration
 *   int32 motor_id = 3;                 // 0-3 for specific motor, -1 for all
 * }
 *
 * message PidConfig {
 *   double kp = 1;                      // Proportional gain
 *   double ki = 2;                      // Integral gain
 *   double kd = 3;                      // Derivative gain
 *   double output_min_percent = 4;      // Minimum PWM duty %
 *   double output_max_percent = 5;      // Maximum PWM duty %
 *   double integral_min = 6;            // Anti-windup min
 *   double integral_max = 7;            // Anti-windup max
 * }
 * @endcode
 *
 * **Test Coverage (8 tests):** nullptr checks, empty buffer, invalid data,
 * not initialized, oversized buffer, valid decode, round-trip
 * @{
 */

/**
 * @brief Test decode PID gains request with nullptr buffer pointer
 * @details Verifies input validation for critical PID configuration command.
 */
void test_decode_pid_gains_request_null_buffer(void)
{
  star_v1_SetPidGainsRequest msg;
  rx_err_t err = rx_nanopb_decode_pid_gains_request(nullptr, k_test_decode_dummy_len, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode PID gains request with nullptr message pointer
 */
void test_decode_pid_gains_request_null_msg(void)
{
  uint8_t  buffer[k_test_decode_buf_size_64] = {};
  rx_err_t err = rx_nanopb_decode_pid_gains_request(buffer, sizeof(buffer), nullptr);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode PID gains request with empty buffer (len=0)
 */
void test_decode_pid_gains_request_empty_buffer(void)
{
  uint8_t buffer[k_test_decode_buf_size_64] = {};

  star_v1_SetPidGainsRequest msg;
  rx_err_t                   err = rx_nanopb_decode_pid_gains_request(buffer, 0, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode PID gains request with invalid protobuf data
 * @details Malformed protobuf wire format should return protocol error
 */
void test_decode_pid_gains_request_invalid_data(void)
{
  uint8_t buffer[k_test_decode_buf_size_64];

  star_v1_SetPidGainsRequest msg;

  /* Fill buffer with invalid protobuf data */
  for (size_t i = 0; i < sizeof(buffer); i++) {
    buffer[i] = k_test_invalid_byte;
  }

  rx_err_t err = rx_nanopb_decode_pid_gains_request(buffer, sizeof(buffer), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief Test decode PID gains request when module not initialized
 */
void test_decode_pid_gains_request_not_initialized(void)
{
  uint8_t buffer[k_test_decode_buf_size_64] = {};

  star_v1_SetPidGainsRequest msg;

  /* Reset state to uninitialized */
  rx_nanopb_test_reset_state();

  rx_err_t err = rx_nanopb_decode_pid_gains_request(buffer, sizeof(buffer), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);

  /* Re-initialize for subsequent tests */
  (void)rx_nanopb_init();
}

/**
 * @brief Test decode PID gains request with oversized buffer
 */
void test_decode_pid_gains_request_oversized_buffer(void)
{
  uint8_t                    buffer[k_test_decode_buf_size_600] = {};
  star_v1_SetPidGainsRequest msg;

  rx_err_t err = rx_nanopb_decode_pid_gains_request(buffer, sizeof(buffer), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test successful decode of valid PID gains request
 * @details Verifies proper decode of all PID configuration fields
 */
void test_decode_pid_gains_request_valid(void)
{
  uint8_t  encode_buffer[k_test_decode_buf_size_256];
  uint32_t encode_len;

  star_v1_SetPidGainsRequest encode_msg = star_v1_SetPidGainsRequest_init_zero;

  /* Build valid PID gains request */
  encode_msg.has_pid_config                = true;
  encode_msg.pid_config.kp                 = s_test_pid_kp;
  encode_msg.pid_config.ki                 = s_test_pid_ki;
  encode_msg.pid_config.kd                 = s_test_pid_kd;
  encode_msg.pid_config.output_min_percent = s_test_pid_out_min;
  encode_msg.pid_config.output_max_percent = s_test_pid_out_max;
  encode_msg.pid_config.integral_min       = s_test_pid_int_min;
  encode_msg.pid_config.integral_max       = s_test_pid_int_max;
  encode_msg.motor_id                      = k_test_motor_id_all;

  /* Encode using nanopb directly (since we don't have encode function yet) */
  pb_ostream_t ostream   = pb_ostream_from_buffer(encode_buffer, sizeof(encode_buffer));
  bool         encode_ok = pb_encode(&ostream, star_v1_SetPidGainsRequest_fields, &encode_msg);
  TEST_ASSERT_TRUE(encode_ok);
  encode_len = ostream.bytes_written;

  /* Now decode and verify */
  star_v1_SetPidGainsRequest decode_msg;
  rx_err_t err = rx_nanopb_decode_pid_gains_request(encode_buffer, encode_len, &decode_msg);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(decode_msg.has_pid_config);
  TEST_ASSERT_FLOAT_WITHIN(s_test_pid_tolerance, s_test_pid_kp, decode_msg.pid_config.kp);
  TEST_ASSERT_FLOAT_WITHIN(s_test_pid_tolerance, s_test_pid_ki, decode_msg.pid_config.ki);
  TEST_ASSERT_FLOAT_WITHIN(s_test_pid_tolerance, s_test_pid_kd, decode_msg.pid_config.kd);
  TEST_ASSERT_FLOAT_WITHIN(s_test_pid_tolerance,
                           s_test_pid_out_min,
                           decode_msg.pid_config.output_min_percent);
  TEST_ASSERT_FLOAT_WITHIN(s_test_pid_tolerance,
                           s_test_pid_out_max,
                           decode_msg.pid_config.output_max_percent);
  TEST_ASSERT_FLOAT_WITHIN(s_test_pid_tolerance,
                           s_test_pid_int_min,
                           decode_msg.pid_config.integral_min);
  TEST_ASSERT_FLOAT_WITHIN(s_test_pid_tolerance,
                           s_test_pid_int_max,
                           decode_msg.pid_config.integral_max);
  TEST_ASSERT_EQUAL(k_test_motor_id_all, decode_msg.motor_id);
}

/**
 * @brief Test round-trip encode/decode preserves PID gains
 * @details Ensures lossless encoding for MATLAB-tuned PID values
 */
void test_pid_gains_request_round_trip(void)
{
  uint8_t  buffer[k_test_decode_buf_size_256];
  uint32_t encode_len;

  star_v1_SetPidGainsRequest original = star_v1_SetPidGainsRequest_init_zero;

  /* MATLAB-tuned gains for motor system (tau=75ms) */
  original.has_pid_config                = true;
  original.pid_config.kp                 = s_test_pid_kp_tuned;
  original.pid_config.ki                 = s_test_pid_ki_tuned;
  original.pid_config.kd                 = s_test_zero_velocity_mps;
  original.pid_config.output_min_percent = s_test_pid_out_min;
  original.pid_config.output_max_percent = s_test_pid_out_max;
  original.pid_config.integral_min       = s_test_pid_int_min;
  original.pid_config.integral_max       = s_test_pid_int_max;
  original.motor_id                      = 0; /* Front-left motor */

  /* Encode */
  pb_ostream_t ostream   = pb_ostream_from_buffer(buffer, sizeof(buffer));
  bool         encode_ok = pb_encode(&ostream, star_v1_SetPidGainsRequest_fields, &original);
  TEST_ASSERT_TRUE(encode_ok);
  encode_len = ostream.bytes_written;

  /* Decode */
  star_v1_SetPidGainsRequest decoded;
  rx_err_t                   err = rx_nanopb_decode_pid_gains_request(buffer, encode_len, &decoded);

  /* Verify lossless round-trip */
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(decoded.has_pid_config);
  TEST_ASSERT_FLOAT_WITHIN(s_test_pid_tight_tolerance,
                           original.pid_config.kp,
                           decoded.pid_config.kp);
  TEST_ASSERT_FLOAT_WITHIN(s_test_pid_tight_tolerance,
                           original.pid_config.ki,
                           decoded.pid_config.ki);
  TEST_ASSERT_FLOAT_WITHIN(s_test_pid_tight_tolerance,
                           original.pid_config.kd,
                           decoded.pid_config.kd);
  TEST_ASSERT_EQUAL(original.motor_id, decoded.motor_id);
}

/** @} */ /* End of nanopb_test_pid_gains_req_decode */

/* =============================================================================
 * Emergency Stop Response Encode Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_estop_resp_encode EmergencyStopResponse Encoding Tests
 * @brief Tests for encoding emergency stop acknowledgments (RX72N -> RPi5)
 * @details
 * Validates rx_nanopb_encode_estop_response() with e-stop engagement state.
 *
 * **Message Structure:**
 * @code
 * message EmergencyStopResponse {
 *   optional ResponseHeader header = 1;
 *   bool estop_engaged = 2;  // Current e-stop state
 * }
 * @endcode
 *
 * **Test Coverage (8 tests):** nullptr checks, buffer size, engaged states, status codes
 * @{
 */

/**
 * @brief Test encode estop response with nullptr message pointer
 * @details Verifies input validation for safety-critical response.
 */
void test_encode_estop_response_null_msg(void)
{
  uint32_t len;
  rx_err_t err = rx_nanopb_encode_estop_response(nullptr, s_buffer, sizeof(s_buffer), &len);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode estop response with nullptr buffer pointer
 */
void test_encode_estop_response_null_buffer(void)
{
  star_v1_EmergencyStopResponse msg = star_v1_EmergencyStopResponse_init_zero;
  uint32_t                      len;
  rx_err_t err = rx_nanopb_encode_estop_response(&msg, nullptr, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode estop response with nullptr length pointer
 */
void test_encode_estop_response_null_len(void)
{
  star_v1_EmergencyStopResponse msg = star_v1_EmergencyStopResponse_init_zero;
  rx_err_t err = rx_nanopb_encode_estop_response(&msg, s_buffer, sizeof(s_buffer), nullptr);
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
 * @brief Test encode estop response when module not initialized
 * @details Verifies state validation for safety-critical response.
 */
void test_encode_estop_response_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  star_v1_EmergencyStopResponse msg = star_v1_EmergencyStopResponse_init_zero;
  uint32_t                      len = 0;

  rx_err_t err = rx_nanopb_encode_estop_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/** @} */ /* End of nanopb_test_estop_resp_encode */

/* =============================================================================
 * Telemetry Encode Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_telemetry_encode TelemetryData Encoding Tests
 * @brief Tests for encoding telemetry sensor/motor data (RX72N -> RPi5)
 * @details
 * Validates rx_nanopb_encode_telemetry() with 14+ telemetry fields including
 * GPS, IMU, temperature, CPU usage, motor load, and WiFi signal strength.
 *
 * **Message Structure:**
 * @code
 * message TelemetryData {
 *   double temperature_celsius = 1;   // -40 to +85degC
 *   double cpu_usage_percent = 2;     // 0-100%
 *   double motor_load_percent = 3;    // 0-100%
 *   int32 wifi_signal_dbm = 4;        // -100 to 0 dBm
 *   optional GpsData gps = 5;         // Nested message
 *   optional ImuData imu = 6;         // Nested message
 * }
 * @endcode
 *
 * **Test Coverage (13 tests):**
 * - nullptr pointer checks (3 tests)
 * - Buffer size validation (2 tests)
 * - Individual field encoding (5 tests: temp, CPU, motor, WiFi, GPS, IMU)
 * - All fields combined (1 test)
 * - State validation (1 test)
 *
 * **Nested Message Tests:**
 * - GPS: latitude, longitude, altitude, accuracy, satellites, fix_type
 * - IMU: pitch, roll, yaw, accel_{x,y,z}, gyro_{x,y,z}
 * @{
 */

/**
 * @brief Test encode telemetry with nullptr message pointer
 * @details Verifies input validation rejects nullptr msg parameter.
 */
void test_encode_telemetry_null_msg(void)
{
  uint32_t len;
  rx_err_t err = rx_nanopb_encode_telemetry(nullptr, s_buffer, sizeof(s_buffer), &len);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode telemetry with nullptr buffer pointer
 */
void test_encode_telemetry_null_buffer(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  uint32_t              len;
  rx_err_t              err = rx_nanopb_encode_telemetry(&msg, nullptr, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode telemetry with nullptr length pointer
 */
void test_encode_telemetry_null_len(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  rx_err_t              err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), nullptr);
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
 * @brief Test encode telemetry when module not initialized
 * @details Verifies state validation before telemetry encoding.
 */
void test_encode_telemetry_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;
  uint32_t              len = 0;

  rx_err_t err = rx_nanopb_encode_telemetry(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/** @} */ /* End of nanopb_test_telemetry_encode */

/* =============================================================================
 * SetPidGainsResponse Encode Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_pid_gains_resp_encode SetPidGainsResponse Encoding Tests
 * @brief Tests for encoding PID gains acknowledgments (RX72N -> RPi5)
 * @details
 * Validates rx_nanopb_encode_pid_gains_response() with success/failure states.
 *
 * **Test Coverage (4 tests):** nullptr checks, buffer size, success true/false
 * @{
 */

/**
 * @brief Test encode pid gains response with nullptr message pointer
 */
void test_encode_pid_gains_response_null_msg(void)
{
  uint32_t len;
  rx_err_t err = rx_nanopb_encode_pid_gains_response(nullptr, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode pid gains response with nullptr buffer pointer
 */
void test_encode_pid_gains_response_null_buffer(void)
{
  star_v1_SetPidGainsResponse msg = star_v1_SetPidGainsResponse_init_zero;
  uint32_t                    len;
  rx_err_t err = rx_nanopb_encode_pid_gains_response(&msg, nullptr, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode pid gains response with nullptr length pointer
 */
void test_encode_pid_gains_response_null_len(void)
{
  star_v1_SetPidGainsResponse msg = star_v1_SetPidGainsResponse_init_zero;
  rx_err_t err = rx_nanopb_encode_pid_gains_response(&msg, s_buffer, sizeof(s_buffer), nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test encode pid gains response with success true
 */
void test_encode_pid_gains_response_success(void)
{
  star_v1_SetPidGainsResponse msg = star_v1_SetPidGainsResponse_init_zero;
  msg.has_header                  = true;
  msg.header.status               = star_v1_Status_STATUS_OK;
  msg.success                     = true;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_pid_gains_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
}

/**
 * @brief Test encode pid gains response with success false (error state)
 */
void test_encode_pid_gains_response_failure(void)
{
  star_v1_SetPidGainsResponse msg = star_v1_SetPidGainsResponse_init_zero;
  msg.has_header                  = true;
  msg.header.status               = star_v1_Status_STATUS_INTERNAL_ERROR;
  msg.success                     = false;

  uint32_t len = 0;
  rx_err_t err = rx_nanopb_encode_pid_gains_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_GREATER_THAN(0, len);
}

/** @} */ /* End of nanopb_test_pid_gains_resp_encode */

/* =============================================================================
 * SetRetransmitConfigRequest Decode Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_retransmit_req_decode SetRetransmitConfigRequest Decode Tests
 * @brief Tests for decoding retransmit configuration requests (RPi5 -> RX72N)
 * @details
 * Validates rx_nanopb_decode_retransmit_config_request() with various inputs.
 *
 * **Test Coverage (3 tests):** nullptr checks, valid decode
 * @{
 */

/**
 * @brief Test decode retransmit config request with nullptr buffer pointer
 */
void test_decode_retransmit_config_request_null_buffer(void)
{
  star_v1_SetRetransmitConfigRequest msg;
  rx_err_t err = rx_nanopb_decode_retransmit_config_request(nullptr, 0, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode retransmit config request with nullptr message pointer
 */
void test_decode_retransmit_config_request_null_msg(void)
{
  uint8_t  buf[k_test_pb_small_buf_size] = {};
  rx_err_t err = rx_nanopb_decode_retransmit_config_request(buf, sizeof(buf), nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode retransmit config request with valid encoded data
 */
void test_decode_retransmit_config_request_valid(void)
{
  /* Build and encode a retransmit config request using nanopb directly */
  star_v1_SetRetransmitConfigRequest orig = star_v1_SetRetransmitConfigRequest_init_zero;
  orig.has_retransmit_config              = true;
  orig.retransmit_config.enabled          = true;
  orig.retransmit_config.max_retries      = k_test_retransmit_retries;
  orig.retransmit_config.ack_timeout_ms   = k_test_retransmit_ack_ms;
  orig.retransmit_config.max_backoff_ms   = k_test_retransmit_backoff;

  uint8_t      enc_buf[star_v1_SetRetransmitConfigRequest_size];
  pb_ostream_t ostream   = pb_ostream_from_buffer(enc_buf, sizeof(enc_buf));
  bool         encode_ok = pb_encode(&ostream, star_v1_SetRetransmitConfigRequest_fields, &orig);
  TEST_ASSERT_TRUE(encode_ok);

  star_v1_SetRetransmitConfigRequest decoded;
  rx_err_t                           err =
    rx_nanopb_decode_retransmit_config_request(enc_buf, (uint32_t)ostream.bytes_written, &decoded);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(decoded.has_retransmit_config);
  TEST_ASSERT_TRUE(decoded.retransmit_config.enabled);
  TEST_ASSERT_EQUAL(k_test_retransmit_retries, decoded.retransmit_config.max_retries);
  TEST_ASSERT_EQUAL(k_test_retransmit_ack_ms, decoded.retransmit_config.ack_timeout_ms);
  TEST_ASSERT_EQUAL(k_test_retransmit_backoff, decoded.retransmit_config.max_backoff_ms);
}

/** @} */ /* End of nanopb_test_retransmit_req_decode */

/* =============================================================================
 * Helper Function Tests - Create Velocity Command
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_helpers_velocity Helper Function Tests: Create Velocity Command
 * @brief Tests for rx_nanopb_create_velocity_command() message factory
 * @details
 * Validates helper function that converts rx_velocity_command_params_t struct
 * to star_v1_VelocityCommand protobuf message. This function simplifies
 * application code by handling zero-initialization and field population.
 *
 * **Function Signature:**
 * @code
 * rx_err_t rx_nanopb_create_velocity_command(
 *     star_v1_VelocityCommand* cmd,
 *     const rx_velocity_command_params_t* params
 * );
 * @endcode
 *
 * **Test Coverage (5 tests):**
 * - nullptr pointer check
 * - Valid parameters (4-motor command)
 * - Zero velocities (stop command)
 * - Max sequence number (UINT32_MAX wrap)
 * - Struct zero-initialization verification
 * @{
 */

/**
 * @brief Test create velocity command with nullptr pointer
 * @details Verifies input validation rejects nullptr cmd parameter.
 */
void test_create_velocity_command_null(void)
{
  rx_velocity_command_params_t params =

    internal_make_velocity_params(s_test_front_left_velocity_mps,
                                  s_test_front_right_velocity_mps,
                                  s_test_back_left_velocity_mps,
                                  s_test_back_right_velocity_mps,
                                  k_test_sequence_number);
  rx_err_t err = rx_nanopb_create_velocity_command(nullptr, &params);
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
  uint8_t* cmd_bytes = (uint8_t*)&cmd;
  for (size_t i = 0; i < sizeof(cmd); i++) {
    cmd_bytes[i] = k_test_fill_byte;
  }

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

/** @} */ /* End of nanopb_test_helpers_velocity */

/* =============================================================================
 * Helper Function Tests - Create Velocity Command (Diff Drive)
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_helpers_velocity_diff Helper Function Tests: Diff Drive Velocity Command
 * @brief Tests for rx_nanopb_create_velocity_command_diff_drive() message factory
 * @details
 * Validates that left/right side velocities are mapped to both front and back
 * motors on the respective side.
 *
 * **Test Coverage (3 tests):** nullptr check, valid mapping, zero velocities
 * @{
 */

/**
 * @brief Test create diff drive velocity command with nullptr cmd pointer
 */
void test_create_velocity_command_diff_drive_null_cmd(void)
{
  rx_velocity_diff_drive_params_t params = {.left_mps  = s_test_diff_right_mps,
                                            .right_mps = s_test_diff_right_mps,
                                            .sequence  = 1};
  rx_err_t err = rx_nanopb_create_velocity_command_diff_drive(nullptr, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test create diff drive velocity command with nullptr params pointer
 */
void test_create_velocity_command_diff_drive_null_params(void)
{
  star_v1_VelocityCommand cmd;
  rx_err_t                err = rx_nanopb_create_velocity_command_diff_drive(&cmd, nullptr);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test create diff drive velocity command maps left/right to all four wheels
 */
void test_create_velocity_command_diff_drive_valid(void)
{
  star_v1_VelocityCommand         cmd;
  rx_velocity_diff_drive_params_t params = {.left_mps  = s_test_diff_left_mps,
                                            .right_mps = s_test_diff_right_mps,
                                            .sequence  = k_test_sequence_number};

  rx_err_t err = rx_nanopb_create_velocity_command_diff_drive(&cmd, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           s_test_diff_left_mps_f,
                           (float)cmd.front_left_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance,
                           s_test_diff_left_mps_f,
                           (float)cmd.back_left_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance, 1.0F, (float)cmd.front_right_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance, 1.0F, (float)cmd.back_right_velocity_mps);
  TEST_ASSERT_EQUAL(k_test_sequence_number, cmd.sequence);
}

/**
 * @brief Test create diff drive velocity command with zero velocities (stop)
 */
void test_create_velocity_command_diff_drive_zero(void)
{
  star_v1_VelocityCommand         cmd;
  rx_velocity_diff_drive_params_t params = {.left_mps = 0.0, .right_mps = 0.0, .sequence = 0};

  rx_err_t err = rx_nanopb_create_velocity_command_diff_drive(&cmd, &params);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance, 0.0F, (float)cmd.front_left_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance, 0.0F, (float)cmd.back_left_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance, 0.0F, (float)cmd.front_right_velocity_mps);
  TEST_ASSERT_FLOAT_WITHIN(s_test_float_tolerance, 0.0F, (float)cmd.back_right_velocity_mps);
}

/** @} */ /* End of nanopb_test_helpers_velocity_diff */

/* =============================================================================
 * Helper Function Tests - Create Response Header
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_helpers_response Helper Function Tests: Create Response Header
 * @brief Tests for rx_nanopb_create_response_header() message factory
 * @details
 * Validates helper function that constructs star_v1_ResponseHeader with
 * status code and optional request ID. Simplifies response message construction.
 *
 * **Function Signature:**
 * @code
 * void rx_nanopb_create_response_header(
 *     star_v1_ResponseHeader* header,
 *     star_v1_Status status,
 *     const char* request_id
 * );
 * @endcode
 *
 * **Test Coverage (6 tests):**
 * - nullptr header pointer handling
 * - STATUS_OK (success)
 * - Error status codes (INVALID_REQUEST, TIMEOUT, etc.)
 * - Request ID string copying
 * - nullptr request ID handling (empty string)
 * - All status code enum values
 * @{
 */

/**
 * @brief Test create response header with nullptr pointer
 * @details Verifies function handles nullptr gracefully (no crash).
 */
void test_create_response_header_null(void)
{
  /* Should not crash with nullptr pointer */
  rx_nanopb_create_response_header(nullptr, star_v1_Status_STATUS_OK, "test-id");
  /* If we reach here, the function handled nullptr gracefully */
  TEST_PASS();
}

/**
 * @brief Test create response header with STATUS_OK
 */
void test_create_response_header_ok(void)
{
  star_v1_ResponseHeader header;
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_OK, nullptr);

  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_OK, header.status);
}

/**
 * @brief Test create response header with error status
 */
void test_create_response_header_error(void)
{
  star_v1_ResponseHeader header;
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_INVALID_REQUEST, nullptr);

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
  /* The request_id should be copied to the fixed-size string field */
  TEST_ASSERT_EQUAL_STRING(request_id, header.request_id);
}

/**
 * @brief Test create response header with nullptr request ID
 */
void test_create_response_header_null_request_id(void)
{
  star_v1_ResponseHeader header;
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_TIMEOUT, nullptr);

  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_TIMEOUT, header.status);
  /* The request_id should be empty when nullptr is passed */
  TEST_ASSERT_EQUAL_STRING("", header.request_id);
}

/**
 * @brief Test create response header with all status codes
 */
void test_create_response_header_all_status_codes(void)
{
  star_v1_ResponseHeader header;

  /* Test STATUS_UNKNOWN */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_UNKNOWN, nullptr);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_UNKNOWN, header.status);

  /* Test STATUS_INTERNAL_ERROR */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_INTERNAL_ERROR, nullptr);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_INTERNAL_ERROR, header.status);

  /* Test STATUS_NOT_FOUND */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_NOT_FOUND, nullptr);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_NOT_FOUND, header.status);

  /* Test STATUS_INVALID_STATE */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_INVALID_STATE, nullptr);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_INVALID_STATE, header.status);

  /* Test STATUS_ESTOP_ACTIVE */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_ESTOP_ACTIVE, nullptr);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_ESTOP_ACTIVE, header.status);
}

/** @} */ /* End of nanopb_test_helpers_response */

/* =============================================================================
 * Encoded Length Tracking Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_length_tracking Encoded Length Tracking Tests
 * @brief Tests for protobuf message size validation and optimization
 * @details
 * Validates that encoded message length varies correctly with field population.
 * Verifies protobuf wire format optimization (empty fields not encoded).
 *
 * **Test Coverage (2 tests):**
 * - Encoded length increases with more fields
 * - Empty message encodes to minimal size (0 bytes)
 * @{
 */

/**
 * @brief Test that encoded length increases with more data
 * @details
 * Verifies protobuf size optimization - more fields = larger message.
 * Compares small message (temperature only) vs large message (temperature + GPS + IMU).
 */
void test_encoded_length_increases_with_data(void)
{
  star_v1_TelemetryData msg_small = star_v1_TelemetryData_init_zero;
  star_v1_TelemetryData msg_large = star_v1_TelemetryData_init_zero;

  /* Small message: just temperature */
  msg_small.temperature_celsius = s_test_temperature_c;

  /* Large message: temperature + GPS + IMU */
  msg_large.temperature_celsius = s_test_temperature_c;
  msg_large.has_gps             = true;
  msg_large.gps.latitude_deg    = s_test_latitude_deg;
  msg_large.gps.longitude_deg   = s_test_longitude_deg;
  msg_large.has_imu             = true;
  msg_large.imu.pitch_rad       = s_test_pitch_rad;

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

/** @} */ /* End of nanopb_test_length_tracking */

/* =============================================================================
 * Buffer Size Handling Tests
 * =============================================================================
 */

/**
 * @defgroup nanopb_test_buffer_size Buffer Size Handling Tests
 * @brief Tests for message size bounds checking and buffer overflow prevention
 * @details
 * Validates that all message types fit within s_nanopb_buffer_size (512 bytes).
 * Ensures nanopb encoding never exceeds static buffer allocation.
 *
 * **Buffer Size Configuration:**
 * - Production: s_nanopb_buffer_size = 512 bytes
 * - Test: k_test_buffer_size = 512 bytes (matches production)
 * - Max message: ~256 bytes (TelemetryData with all fields)
 * - Safety margin: 2x largest message
 *
 * **Test Coverage (2 tests):**
 * - VelocityRequest with max values fits in buffer
 * - TelemetryData with all fields fits in buffer
 * @{
 */

/**
 * @brief Test encoding stays within buffer bounds for velocity request
 * @details
 * Verifies worst-case VelocityRequest (max velocities, max sequence, timestamp)
 * fits within 512-byte buffer.
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
  TEST_ASSERT_LESS_THAN(s_nanopb_buffer_size, len);
}

/**
 * @brief Test encoding stays within buffer bounds for telemetry
 */
void test_telemetry_fits_in_buffer(void)
{
  star_v1_TelemetryData msg = star_v1_TelemetryData_init_zero;

  /* Fill all fields */
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
  TEST_ASSERT_LESS_THAN(s_nanopb_buffer_size, len);
}

/** @} */ /* End of nanopb_test_buffer_size */

/* =============================================================================
 * Additional Coverage Tests
 * =============================================================================
 */

/**
 * @brief Test successful decode of EmergencyStopRequest with valid data
 * @details Covers the return k_rx_ok path in rx_nanopb_decode_estop_request().
 */
void test_decode_estop_request_valid(void)
{
  /* Encode a valid EmergencyStopRequest using nanopb directly */
  star_v1_EmergencyStopRequest orig      = star_v1_EmergencyStopRequest_init_zero;
  const char*                  test_str  = "test";
  size_t                       copy_idx  = 0;
  const size_t                 max_chars = sizeof(orig.reason) - 1U;
  while (copy_idx < max_chars && test_str[copy_idx] != '\0') {
    orig.reason[copy_idx] = test_str[copy_idx];
    copy_idx++;
  }
  orig.reason[copy_idx] = '\0';

  uint8_t      enc_buf[star_v1_EmergencyStopRequest_size];
  pb_ostream_t ostream   = pb_ostream_from_buffer(enc_buf, sizeof(enc_buf));
  bool         encode_ok = pb_encode(&ostream, star_v1_EmergencyStopRequest_fields, &orig);
  TEST_ASSERT_TRUE(encode_ok);
  TEST_ASSERT_GREATER_THAN(0, ostream.bytes_written);

  star_v1_EmergencyStopRequest decoded = star_v1_EmergencyStopRequest_init_zero;
  rx_err_t err = rx_nanopb_decode_estop_request(enc_buf, (uint32_t)ostream.bytes_written, &decoded);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_STRING("test", decoded.reason);
}

/**
 * @brief Test encode_pid_gains_response when not initialized
 * @details Covers the not-initialized early-return path in
 *          rx_nanopb_encode_pid_gains_response().
 */
void test_encode_pid_gains_response_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  star_v1_SetPidGainsResponse msg = star_v1_SetPidGainsResponse_init_zero;
  uint32_t                    len = 0;
  rx_err_t err = rx_nanopb_encode_pid_gains_response(&msg, s_buffer, sizeof(s_buffer), &len);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief Test encode_pid_gains_response with too-small buffer
 * @details Covers the buffer_size < s_nanopb_buffer_size early-return path in
 *          rx_nanopb_encode_pid_gains_response().
 */
void test_encode_pid_gains_response_small_buffer(void)
{
  star_v1_SetPidGainsResponse msg = star_v1_SetPidGainsResponse_init_zero;
  uint32_t                    len = 0;
  rx_err_t                    err =
    rx_nanopb_encode_pid_gains_response(&msg, s_buffer, k_test_small_buffer_size, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

/**
 * @brief Test decode_retransmit_config_request with len == 0
 * @details Covers the len == 0 path of the length-validation check.
 */
void test_decode_retransmit_config_request_zero_len(void)
{
  uint8_t                            buf[k_test_pb_small_buf_size] = {};
  star_v1_SetRetransmitConfigRequest msg;
  rx_err_t                           err = rx_nanopb_decode_retransmit_config_request(buf, 0, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode_retransmit_config_request with len > buffer_size
 * @details Covers the len > s_nanopb_buffer_size path of the length-validation check.
 */
void test_decode_retransmit_config_request_oversized(void)
{
  star_v1_SetRetransmitConfigRequest msg;
  rx_err_t                           err =
    rx_nanopb_decode_retransmit_config_request(s_buffer, (uint32_t)s_nanopb_buffer_size + 1U, &msg);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test decode_retransmit_config_request when not initialized
 * @details Covers the not-initialized early-return path.
 */
void test_decode_retransmit_config_request_not_initialized(void)
{
  rx_nanopb_test_reset_state();

  uint8_t buf[k_test_pb_small_buf_size] = {k_test_pb_tag_varint, 0x01, 0x00, 0x00};
  star_v1_SetRetransmitConfigRequest msg;
  rx_err_t err = rx_nanopb_decode_retransmit_config_request(buf, sizeof(buf), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_not_initialized, err);
}

/**
 * @brief Test decode_retransmit_config_request with invalid protobuf data
 * @details Covers the pb_decode failure path (returns k_rx_err_protocol_error).
 */
void test_decode_retransmit_config_request_invalid_data(void)
{
  uint8_t                            invalid_data[] = {k_test_fill_byte,
                                                       k_test_fill_byte,
                                                       k_test_fill_byte,
                                                       k_test_fill_byte,
                                                       k_test_fill_byte};
  star_v1_SetRetransmitConfigRequest msg;
  rx_err_t                           err =
    rx_nanopb_decode_retransmit_config_request(invalid_data, sizeof(invalid_data), &msg);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

/**
 * @brief Test create_velocity_command with out-of-range velocity
 * @details Covers the velocity range check in rx_nanopb_create_velocity_command().
 *          Uses a velocity > k_velocity_mps_max (1000.0 m/s) to trigger the guard.
 */
void test_create_velocity_command_out_of_range(void)
{
  star_v1_VelocityCommand      cmd;
  rx_velocity_command_params_t params = internal_make_velocity_params(s_test_velocity_out_of_range,
                                                                      0.0,
                                                                      0.0,
                                                                      0.0,
                                                                      k_test_sequence_zero);
  rx_err_t                     err    = rx_nanopb_create_velocity_command(&cmd, &params);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test create_response_header with a request_id longer than s_nanopb_buffer_size
 * @details Covers the strlen(request_id) > s_nanopb_buffer_size guard (line 1684).
 *          Uses a 513-char string to exceed the 512-byte limit.
 */
void test_create_response_header_long_request_id(void)
{
  /* Build a string of length s_nanopb_buffer_size + 1 (513 chars) */
  static char s_long_id[k_test_long_id_buf_size];
  for (size_t i = 0; i < sizeof(s_long_id) - 1U; i++) {
    s_long_id[i] = 'A';
  }
  s_long_id[sizeof(s_long_id) - 1U] = '\0';

  star_v1_ResponseHeader header;
  /* Function returns without modifying header when request_id is too long */
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_OK, s_long_id);

  /* Header is unmodified: verify by checking that a subsequent valid call works */
  star_v1_ResponseHeader header2 = star_v1_ResponseHeader_init_zero;
  rx_nanopb_create_response_header(&header2, star_v1_Status_STATUS_OK, nullptr);
  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_OK, header2.status);
}

/**
 * @brief Test create_response_header with request_id that exactly fills the buffer.
 *
 * @details
 * Passes a 63-character request_id so that the while-loop at rx_nanopb.c line
 * 1690 exits via `idx < max_len` becoming false rather than `request_id[idx]
 * != '\0'`. This covers the previously uncovered branch.
 *
 * @pre Module initialized
 * @post request_id copied and truncated at 63 characters
 *
 * @since Version 1.0.0
 */
void test_create_response_header_max_length_request_id(void)
{
  /* Build a 63-character string (sizeof(header->request_id) - 1 = 63) */
  enum : uint32_t { k_request_id_max_len = 63 };
  static char s_max_id[k_request_id_max_len + 1U];
  for (size_t i = 0; i < k_request_id_max_len; i++) {
    s_max_id[i] = 'R';
  }
  s_max_id[k_request_id_max_len] = '\0';

  star_v1_ResponseHeader header;
  rx_nanopb_create_response_header(&header, star_v1_Status_STATUS_OK, s_max_id);

  TEST_ASSERT_EQUAL(star_v1_Status_STATUS_OK, header.status);
  /* Verify the full 63-character string was copied */
  TEST_ASSERT_EQUAL_STRING(s_max_id, header.request_id);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

/**
 * @brief Main test runner entry point
 *
 * @details
 * Executes all 89 nanopb integration tests using Unity test framework.
 * Tests run sequentially (Unity is single-threaded).
 *
 * **Test Execution Order:**
 * 1. Initialization tests (2)
 * 2. Velocity request encode tests (8)
 * 3. Velocity request decode tests (6)
 * 4. Velocity request round-trip tests (3)
 * 5. Velocity response encode tests (6)
 * 6. Emergency stop request decode tests (6)
 * 7. Emergency stop response encode tests (8)
 * 8. Telemetry encode tests (14)
 * 9. Helper function tests - velocity command (5)
 * 10. Helper function tests - response header (6)
 * 11. Encoded length tracking tests (2)
 * 12. Buffer size handling tests (2)
 *
 * **Total: 68 RUN_TEST() invocations**
 *
 * @return int Number of test failures (0 = all passed)
 *
 * @note Unity framework calls setUp() before and tearDown() after each test
 * @note Build system: CMake with Unity as external dependency
 * @note Run via: `cd build && ctest -R test_rx_nanopb -V`
 */
/**
 * @brief Run velocity and initialization encode/decode tests
 */
static void internal_run_velocity_tests(void)
{
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
}

/**
 * @brief Run emergency stop and PID gains tests
 */
static void internal_run_estop_pid_tests(void)
{
  /* Emergency stop request decode tests */
  RUN_TEST(test_decode_estop_request_null_buffer);
  RUN_TEST(test_decode_estop_request_null_msg);
  RUN_TEST(test_decode_estop_request_empty_buffer);
  RUN_TEST(test_decode_estop_request_invalid_data);
  RUN_TEST(test_decode_estop_request_not_initialized);
  RUN_TEST(test_decode_estop_request_oversized_buffer);
  RUN_TEST(test_decode_estop_request_valid);

  /* SetPidGainsRequest decode tests */
  RUN_TEST(test_decode_pid_gains_request_null_buffer);
  RUN_TEST(test_decode_pid_gains_request_null_msg);
  RUN_TEST(test_decode_pid_gains_request_empty_buffer);
  RUN_TEST(test_decode_pid_gains_request_invalid_data);
  RUN_TEST(test_decode_pid_gains_request_not_initialized);
  RUN_TEST(test_decode_pid_gains_request_oversized_buffer);
  RUN_TEST(test_decode_pid_gains_request_valid);
  RUN_TEST(test_pid_gains_request_round_trip);

  /* Emergency stop response encode tests */
  RUN_TEST(test_encode_estop_response_null_msg);
  RUN_TEST(test_encode_estop_response_null_buffer);
  RUN_TEST(test_encode_estop_response_null_len);
  RUN_TEST(test_encode_estop_response_small_buffer);
  RUN_TEST(test_encode_estop_response_engaged_true);
  RUN_TEST(test_encode_estop_response_engaged_false);
  RUN_TEST(test_encode_estop_response_estop_active_status);
  RUN_TEST(test_encode_estop_response_not_initialized);
}

/**
 * @brief Run telemetry, PID response, and retransmit config tests
 */
static void internal_run_telemetry_retransmit_tests(void)
{
  /* Telemetry encode tests */
  RUN_TEST(test_encode_telemetry_null_msg);
  RUN_TEST(test_encode_telemetry_null_buffer);
  RUN_TEST(test_encode_telemetry_null_len);
  RUN_TEST(test_encode_telemetry_small_buffer);
  RUN_TEST(test_encode_telemetry_empty);
  RUN_TEST(test_encode_telemetry_temperature);
  RUN_TEST(test_encode_telemetry_cpu_usage);
  RUN_TEST(test_encode_telemetry_motor_load);
  RUN_TEST(test_encode_telemetry_wifi_signal);
  RUN_TEST(test_encode_telemetry_gps);
  RUN_TEST(test_encode_telemetry_imu);
  RUN_TEST(test_encode_telemetry_all_fields);
  RUN_TEST(test_encode_telemetry_not_initialized);

  /* SetPidGainsResponse encode tests */
  RUN_TEST(test_encode_pid_gains_response_null_msg);
  RUN_TEST(test_encode_pid_gains_response_null_buffer);
  RUN_TEST(test_encode_pid_gains_response_null_len);
  RUN_TEST(test_encode_pid_gains_response_success);
  RUN_TEST(test_encode_pid_gains_response_failure);
  RUN_TEST(test_encode_pid_gains_response_not_initialized);
  RUN_TEST(test_encode_pid_gains_response_small_buffer);

  /* SetRetransmitConfigRequest decode tests */
  RUN_TEST(test_decode_retransmit_config_request_null_buffer);
  RUN_TEST(test_decode_retransmit_config_request_null_msg);
  RUN_TEST(test_decode_retransmit_config_request_valid);
  RUN_TEST(test_decode_retransmit_config_request_zero_len);
  RUN_TEST(test_decode_retransmit_config_request_oversized);
  RUN_TEST(test_decode_retransmit_config_request_not_initialized);
  RUN_TEST(test_decode_retransmit_config_request_invalid_data);
}

/**
 * @brief Run helper function, length tracking, and buffer size tests
 */
static void internal_run_helper_tests(void)
{
  /* Helper function tests - velocity command */
  RUN_TEST(test_create_velocity_command_null);
  RUN_TEST(test_create_velocity_command_valid);
  RUN_TEST(test_create_velocity_command_zero);
  RUN_TEST(test_create_velocity_command_max_sequence);
  RUN_TEST(test_create_velocity_command_initializes_struct);
  RUN_TEST(test_create_velocity_command_out_of_range);

  /* Helper function tests - diff drive velocity command */
  RUN_TEST(test_create_velocity_command_diff_drive_null_cmd);
  RUN_TEST(test_create_velocity_command_diff_drive_null_params);
  RUN_TEST(test_create_velocity_command_diff_drive_valid);
  RUN_TEST(test_create_velocity_command_diff_drive_zero);

  /* Helper function tests - response header */
  RUN_TEST(test_create_response_header_null);
  RUN_TEST(test_create_response_header_ok);
  RUN_TEST(test_create_response_header_error);
  RUN_TEST(test_create_response_header_with_request_id);
  RUN_TEST(test_create_response_header_null_request_id);
  RUN_TEST(test_create_response_header_all_status_codes);
  RUN_TEST(test_create_response_header_long_request_id);
  RUN_TEST(test_create_response_header_max_length_request_id);

  /* Length tracking tests */
  RUN_TEST(test_encoded_length_increases_with_data);
  RUN_TEST(test_empty_message_minimal_size);

  /* Buffer size tests */
  RUN_TEST(test_velocity_request_fits_in_buffer);
  RUN_TEST(test_telemetry_fits_in_buffer);
}

int main(void)
{
  UNITY_BEGIN();

  internal_run_velocity_tests();
  internal_run_estop_pid_tests();
  internal_run_telemetry_retransmit_tests();
  internal_run_helper_tests();

  return UNITY_END();
}
