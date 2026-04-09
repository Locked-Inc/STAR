/**
 * @file test_frame_go_compat.c
 * @brief Cross-language frame compatibility tests (C vs Go)
 *
 * @details
 * Encodes frames using the C API and compares the wire bytes against the
 * exact hardcoded vectors from star-gateway/internal/frame/frame_test.go
 * (crossCompatibilityVectors). If C and Go produce different bytes, real
 * SPI/USB communication between the RPi5 gateway and RX72N firmware would
 * fail silently.
 *
 * Test vectors:
 * 1. PING   seq=0, empty payload
 * 2. PONG   seq=0, 4-byte counter=42 (little-endian)
 * 3. COMMAND seq=1, "TEST" payload, FLAGS=REQUIRES_ACK
 * 4. RESPONSE seq=1, "OK" payload
 * 5. ACK    seq=1, empty
 * 6. NACK   seq=1, empty
 * 7. RESET  seq=0, empty
 * 8. RESET_ACK seq=0, empty
 *
 * All expected byte sequences are copied verbatim from the Go test file.
 * Any change to the Go vectors MUST be mirrored here (and vice versa).
 *
 * @see star-gateway/internal/frame/frame_test.go crossCompatibilityVectors()
 * @see test_rx_frame.c Existing C frame tests with the same vectors
 *
 * @author Locked, Inc.
 * @date 2026-04-09
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <stdint.h>
#include <string.h>

#include "rx_frame.h"
#include "unity.h"

/* =============================================================================
 * Constants
 * ============================================================================= */

/**
 * @enum go_compat_wire_sizes_t
 * @brief Expected wire sizes for each cross-compatibility vector
 *
 * @details
 * Wire size = SYNC(2) + Header(6) + Payload(N) + CRC(4) = 12 + N.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_wire_empty   = 12, /**< Frames with no payload: 2+6+0+4 */
  k_wire_4_byte  = 16, /**< Frames with 4-byte payload: 2+6+4+4 */
  k_wire_2_byte  = 14, /**< Frames with 2-byte payload: 2+6+2+4 */
  k_encode_buf   = 64, /**< Encode buffer (generous for any vector) */
} go_compat_wire_sizes_t;

/**
 * @enum go_compat_payload_t
 * @brief Payload-related constants for cross-compatibility vectors
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_payload_len_4   = 4,  /**< 4-byte payload length ("TEST", counter) */
  k_payload_len_2   = 2,  /**< 2-byte payload length ("OK") */
  k_counter_42      = 42, /**< PONG counter value */
  k_idx_0           = 0,  /**< Payload byte index 0 */
  k_idx_1           = 1,  /**< Payload byte index 1 */
  k_idx_2           = 2,  /**< Payload byte index 2 */
  k_idx_3           = 3,  /**< Payload byte index 3 */
} go_compat_payload_t;

/* =============================================================================
 * Fixtures
 * ============================================================================= */

/** @brief Shared encoder for all tests */
static rx_frame_encoder_t s_encoder;

/** @brief Shared decoder for all tests */
static rx_frame_decoder_t s_decoder;

/**
 * @brief Unity setUp - initialize encoder and decoder before each test
 */
void setUp(void)
{
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encoder_init(&s_encoder));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decoder_init(&s_decoder));
}

/**
 * @brief Unity tearDown - deinitialize encoder and decoder after each test
 */
void tearDown(void)
{
  (void)rx_frame_encoder_deinit(&s_encoder);
  (void)rx_frame_decoder_deinit(&s_decoder);
}

/* =============================================================================
 * Vector 1: PING seq=0, empty payload
 * Go CRC-32 = 0x2F42E23D
 * ============================================================================= */

/**
 * @brief PING seq=0 empty - must match Go crossCompatibilityVectors[0]
 *
 * @details
 * Wire bytes from Go:
 *   0xAA 0x55  SYNC (LE)
 *   0x00 0x00  SEQ=0
 *   0x00 0x00  LEN=0
 *   0x00       TYPE=PING
 *   0x00       FLAGS=none
 *   0x3D 0xE2 0x42 0x2F  CRC-32 LE
 *
 * @pre Encoder initialized via setUp()
 * @post Encoded wire bytes match Go vector exactly
 *
 * @since Version 1.0.0
 */
void test_go_compat_ping_seq0_empty(void)
{
  static const uint8_t expected[] = {
    0xAA, 0x55,
    0x00, 0x00,
    0x00, 0x00,
    0x00,
    0x00,
    0x3D, 0xE2, 0x42, 0x2F
  };

  rx_frame_t frame;
  uint8_t    buf[k_encode_buf];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_ping(&frame, 0, NULL, 0));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buf, &len));
  TEST_ASSERT_EQUAL_UINT32(k_wire_empty, len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, k_wire_empty);
}

/* =============================================================================
 * Vector 2: PONG seq=0, 4-byte counter=42 (LE)
 * Go CRC-32 = 0x60647975
 * ============================================================================= */

/**
 * @brief PONG seq=0, counter=42 - must match Go crossCompatibilityVectors[1]
 *
 * @details
 * Wire bytes from Go:
 *   0xAA 0x55  SYNC
 *   0x00 0x00  SEQ=0
 *   0x04 0x00  LEN=4
 *   0x01       TYPE=PONG
 *   0x00       FLAGS=none
 *   0x2A 0x00 0x00 0x00  PAYLOAD (counter=42 LE)
 *   0x75 0x79 0x64 0x60  CRC-32 LE
 *
 * @pre Encoder initialized via setUp()
 * @post Encoded wire bytes match Go vector exactly
 *
 * @since Version 1.0.0
 */
void test_go_compat_pong_seq0_counter42(void)
{
  static const uint8_t expected[] = {
    0xAA, 0x55,
    0x00, 0x00,
    0x04, 0x00,
    0x01,
    0x00,
    0x2A, 0x00, 0x00, 0x00,
    0x75, 0x79, 0x64, 0x60
  };

  uint8_t    payload[k_payload_len_4] = {k_counter_42, 0, 0, 0};
  rx_frame_t frame;
  uint8_t    buf[k_encode_buf];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_pong(&frame, 0, payload, k_payload_len_4));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buf, &len));
  TEST_ASSERT_EQUAL_UINT32(k_wire_4_byte, len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, k_wire_4_byte);
}

/* =============================================================================
 * Vector 3: COMMAND seq=1, payload="TEST", FLAGS=REQUIRES_ACK
 * Go CRC-32 = 0x7A6DE93B
 * ============================================================================= */

/**
 * @brief COMMAND seq=1, "TEST" - must match Go crossCompatibilityVectors[2]
 *
 * @details
 * Wire bytes from Go:
 *   0xAA 0x55  SYNC
 *   0x01 0x00  SEQ=1
 *   0x04 0x00  LEN=4
 *   0x10       TYPE=COMMAND
 *   0x01       FLAGS=REQUIRES_ACK
 *   0x54 0x45 0x53 0x54  PAYLOAD="TEST"
 *   0x3B 0xE9 0x6D 0x7A  CRC-32 LE
 *
 * @pre Encoder initialized via setUp()
 * @post Encoded wire bytes match Go vector exactly
 *
 * @since Version 1.0.0
 */
void test_go_compat_command_seq1_test(void)
{
  static const uint8_t expected[] = {
    0xAA, 0x55,
    0x01, 0x00,
    0x04, 0x00,
    0x10,
    0x01,
    0x54, 0x45, 0x53, 0x54,
    0x3B, 0xE9, 0x6D, 0x7A
  };

  rx_frame_t frame = {};
  uint8_t    buf[k_encode_buf];
  uint32_t   len;

  frame.header.sequence      = 1;
  frame.header.length        = k_payload_len_4;
  frame.header.type          = k_frame_type_command;
  frame.header.flags         = k_frame_flag_requires_ack;
  frame.payload[k_idx_0]     = 'T';
  frame.payload[k_idx_1]     = 'E';
  frame.payload[k_idx_2]     = 'S';
  frame.payload[k_idx_3]     = 'T';

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buf, &len));
  TEST_ASSERT_EQUAL_UINT32(k_wire_4_byte, len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, k_wire_4_byte);
}

/* =============================================================================
 * Vector 4: RESPONSE seq=1, payload="OK"
 * Go CRC-32 = 0x9A1DACEA
 * ============================================================================= */

/**
 * @brief RESPONSE seq=1, "OK" - must match Go crossCompatibilityVectors[3]
 *
 * @details
 * Wire bytes from Go:
 *   0xAA 0x55  SYNC
 *   0x01 0x00  SEQ=1
 *   0x02 0x00  LEN=2
 *   0x11       TYPE=RESPONSE
 *   0x00       FLAGS=none
 *   0x4F 0x4B  PAYLOAD="OK"
 *   0xEA 0xAC 0x1D 0x9A  CRC-32 LE
 *
 * @pre Encoder initialized via setUp()
 * @post Encoded wire bytes match Go vector exactly
 *
 * @since Version 1.0.0
 */
void test_go_compat_response_seq1_ok(void)
{
  static const uint8_t expected[] = {
    0xAA, 0x55,
    0x01, 0x00,
    0x02, 0x00,
    0x11,
    0x00,
    0x4F, 0x4B,
    0xEA, 0xAC, 0x1D, 0x9A
  };

  rx_frame_t frame = {};
  uint8_t    buf[k_encode_buf];
  uint32_t   len;

  frame.header.sequence      = 1;
  frame.header.length        = k_payload_len_2;
  frame.header.type          = k_frame_type_response;
  frame.header.flags         = k_frame_flag_none;
  frame.payload[k_idx_0]     = 'O';
  frame.payload[k_idx_1]     = 'K';

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buf, &len));
  TEST_ASSERT_EQUAL_UINT32(k_wire_2_byte, len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, k_wire_2_byte);
}

/* =============================================================================
 * Vector 5: ACK seq=1, empty
 * Go CRC-32 = 0x9CEA414B
 * ============================================================================= */

/**
 * @brief ACK seq=1, empty - must match Go crossCompatibilityVectors[4]
 *
 * @details
 * Wire bytes from Go:
 *   0xAA 0x55  SYNC
 *   0x01 0x00  SEQ=1
 *   0x00 0x00  LEN=0
 *   0x12       TYPE=ACK
 *   0x00       FLAGS=none
 *   0x4B 0x41 0xEA 0x9C  CRC-32 LE
 *
 * @pre Encoder initialized via setUp()
 * @post Encoded wire bytes match Go vector exactly
 *
 * @since Version 1.0.0
 */
void test_go_compat_ack_seq1_empty(void)
{
  static const uint8_t expected[] = {
    0xAA, 0x55,
    0x01, 0x00,
    0x00, 0x00,
    0x12,
    0x00,
    0x4B, 0x41, 0xEA, 0x9C
  };

  rx_frame_t frame;
  uint8_t    buf[k_encode_buf];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_ack(&frame, 1));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buf, &len));
  TEST_ASSERT_EQUAL_UINT32(k_wire_empty, len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, k_wire_empty);
}

/* =============================================================================
 * Vector 6: NACK seq=1, empty
 * Go CRC-32 = 0x85F1700A
 * ============================================================================= */

/**
 * @brief NACK seq=1, empty - must match Go crossCompatibilityVectors[5]
 *
 * @details
 * Wire bytes from Go:
 *   0xAA 0x55  SYNC
 *   0x01 0x00  SEQ=1
 *   0x00 0x00  LEN=0
 *   0x13       TYPE=NACK
 *   0x00       FLAGS=none
 *   0x0A 0x70 0xF1 0x85  CRC-32 LE
 *
 * @pre Encoder initialized via setUp()
 * @post Encoded wire bytes match Go vector exactly
 *
 * @since Version 1.0.0
 */
void test_go_compat_nack_seq1_empty(void)
{
  static const uint8_t expected[] = {
    0xAA, 0x55,
    0x01, 0x00,
    0x00, 0x00,
    0x13,
    0x00,
    0x0A, 0x70, 0xF1, 0x85
  };

  rx_frame_t frame;
  uint8_t    buf[k_encode_buf];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_nack(&frame, 1, k_frame_flag_none));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buf, &len));
  TEST_ASSERT_EQUAL_UINT32(k_wire_empty, len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, k_wire_empty);
}

/* =============================================================================
 * Vector 7: RESET seq=0, empty
 * Go CRC-32 = 0xBC661F4F
 * ============================================================================= */

/**
 * @brief RESET seq=0, empty - must match Go crossCompatibilityVectors[6]
 *
 * @details
 * Wire bytes from Go:
 *   0xAA 0x55  SYNC
 *   0x00 0x00  SEQ=0
 *   0x00 0x00  LEN=0
 *   0xFF       TYPE=RESET
 *   0x00       FLAGS=none
 *   0x4F 0x1F 0x66 0xBC  CRC-32 LE
 *
 * @pre Encoder initialized via setUp()
 * @post Encoded wire bytes match Go vector exactly
 *
 * @since Version 1.0.0
 */
void test_go_compat_reset_seq0_empty(void)
{
  static const uint8_t expected[] = {
    0xAA, 0x55,
    0x00, 0x00,
    0x00, 0x00,
    0xFF,
    0x00,
    0x4F, 0x1F, 0x66, 0xBC
  };

  rx_frame_t frame;
  uint8_t    buf[k_encode_buf];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_reset(&frame, 0));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buf, &len));
  TEST_ASSERT_EQUAL_UINT32(k_wire_empty, len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, k_wire_empty);
}

/* =============================================================================
 * Vector 8: RESET_ACK seq=0, empty
 * Go CRC-32 = 0xA57D2E0E
 * ============================================================================= */

/**
 * @brief RESET_ACK seq=0, empty - must match Go crossCompatibilityVectors[7]
 *
 * @details
 * Wire bytes from Go:
 *   0xAA 0x55  SYNC
 *   0x00 0x00  SEQ=0
 *   0x00 0x00  LEN=0
 *   0xFE       TYPE=RESET_ACK
 *   0x00       FLAGS=none
 *   0x0E 0x2E 0x7D 0xA5  CRC-32 LE
 *
 * @pre Encoder initialized via setUp()
 * @post Encoded wire bytes match Go vector exactly
 *
 * @since Version 1.0.0
 */
void test_go_compat_reset_ack_seq0_empty(void)
{
  static const uint8_t expected[] = {
    0xAA, 0x55,
    0x00, 0x00,
    0x00, 0x00,
    0xFE,
    0x00,
    0x0E, 0x2E, 0x7D, 0xA5
  };

  rx_frame_t frame;
  uint8_t    buf[k_encode_buf];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_reset_ack(&frame, 0));
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buf, &len));
  TEST_ASSERT_EQUAL_UINT32(k_wire_empty, len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, k_wire_empty);
}

/* =============================================================================
 * Test Runner
 * ============================================================================= */

/**
 * @brief Unity test runner entry point
 *
 * @details
 * Runs all 8 cross-language compatibility tests. Each test encodes a frame
 * with the C API and compares the wire output byte-for-byte against the
 * hardcoded vectors from the Go test suite.
 *
 * @return int Number of test failures (0 = all passed)
 *
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_go_compat_ping_seq0_empty);
  RUN_TEST(test_go_compat_pong_seq0_counter42);
  RUN_TEST(test_go_compat_command_seq1_test);
  RUN_TEST(test_go_compat_response_seq1_ok);
  RUN_TEST(test_go_compat_ack_seq1_empty);
  RUN_TEST(test_go_compat_nack_seq1_empty);
  RUN_TEST(test_go_compat_reset_seq0_empty);
  RUN_TEST(test_go_compat_reset_ack_seq0_empty);

  return UNITY_END();
}
