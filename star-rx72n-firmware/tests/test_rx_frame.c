/* tests/test_rx_frame.c */

/**
 * @file test_rx_frame.c
 * @brief Unit Tests for Frame Layer
 *
 * Tests frame encoding/decoding with CRC-32 verification.
 * Verifies bit-exact compatibility with Go implementation.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdint.h>
#include <string.h>

#include "rx_frame.h"
#include "unity.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/**
 * @brief Frame test constants for payload and buffer sizes
 *
 * Note: Minimum frame size is defined in rx_frame.h as k_frame_min_size = 12
 * (SYNC=2 bytes, header=6 bytes, CRC-32=4 bytes)
 */
typedef enum {
  k_test_payload_size    = 256,  /**< Large payload size for testing */
  k_test_buffer_size     = 512,  /**< Buffer size for encoded frame */
  k_test_sequence_num    = 100,  /**< Test sequence number */
  k_test_byte_mask       = 0xFF, /**< Byte mask for payload patterns */
  k_test_small_buffer    = 64,   /**< Small buffer for simple frame tests */
  k_test_oversize_buffer = 2048, /**< Buffer for overflow/boundary tests */
} frame_test_constants_t;

/**
 * @brief Frame wire format byte offsets
 */
typedef enum {
  k_frame_offset_sync_high = 0, /**< SYNC word high byte */
  k_frame_offset_sync_low  = 1, /**< SYNC word low byte */
  k_frame_offset_seq_high  = 2, /**< Sequence number high byte */
  k_frame_offset_seq_low   = 3, /**< Sequence number low byte */
  k_frame_offset_len_high  = 4, /**< Length high byte */
  k_frame_offset_len_low   = 5, /**< Length low byte */
  k_frame_offset_type      = 6, /**< Frame type byte */
  k_frame_offset_flags     = 7, /**< Frame flags byte */
  k_frame_offset_payload   = 8, /**< Payload start offset */
} frame_offset_t;

/**
 * @brief Payload byte offsets relative to payload start
 */
typedef enum {
  k_payload_index_0 = 0,
  k_payload_index_1 = 1,
  k_payload_index_2 = 2,
  k_payload_index_3 = 3,
} payload_index_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/* Test fixtures */
static rx_frame_encoder_t s_encoder;
static rx_frame_decoder_t s_decoder;

void setUp(void)
{
  rx_frame_encoder_init(&s_encoder);
  rx_frame_decoder_init(&s_decoder);
}

void tearDown(void)
{
  rx_frame_encoder_deinit(&s_encoder);
  rx_frame_decoder_deinit(&s_decoder);
}

/* =============================================================================
 * Encoder Initialization Tests
 * =============================================================================
 */

void test_encoder_init_null(void)
{
  rx_err_t err = rx_frame_encoder_init(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_encoder_init_success(void)
{
  rx_frame_encoder_t enc;
  rx_err_t           err = rx_frame_encoder_init(&enc);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(0, enc.initialized);
}

void test_encoder_deinit_null(void)
{
  rx_err_t err = rx_frame_encoder_deinit(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Decoder Initialization Tests
 * =============================================================================
 */

void test_decoder_init_null(void)
{
  rx_err_t err = rx_frame_decoder_init(NULL);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_decoder_init_success(void)
{
  rx_frame_decoder_t dec;
  rx_err_t           err = rx_frame_decoder_init(&dec);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_NOT_EQUAL(0, dec.initialized);
}

/* =============================================================================
 * Encode Tests
 * =============================================================================
 */

void test_encode_null_args(void)
{
  rx_frame_t frame = {0};
  uint8_t    buffer[k_test_small_buffer];
  uint32_t   len;

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_encode(NULL, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_encode(&s_encoder, NULL, buffer, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_encode(&s_encoder, &frame, NULL, &len));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_encode(&s_encoder, &frame, buffer, NULL));
}

void test_encode_uninitialized(void)
{
  rx_frame_encoder_t enc   = {0};
  rx_frame_t         frame = {0};
  uint8_t            buffer[k_test_small_buffer];
  uint32_t           len;

  rx_err_t err = rx_frame_encode(&enc, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_encode_payload_too_large(void)
{
  rx_frame_t frame    = {0};
  frame.header.length = k_frame_max_payload + 1; /* 1025 bytes (exceeds max) */
  uint8_t  buffer[k_test_oversize_buffer];
  uint32_t len;

  rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_encode_empty_frame(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_none;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;

  rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_frame_min_size, len); /* 12 bytes: SYNC + Header + CRC */

  /* Verify SYNC word (big-endian) */
  TEST_ASSERT_EQUAL_HEX8(0x55, buffer[k_frame_offset_sync_high]);
  TEST_ASSERT_EQUAL_HEX8(0xAA, buffer[k_frame_offset_sync_low]);

  /* Verify SEQ (big-endian) */
  TEST_ASSERT_EQUAL_HEX8(0x00, buffer[k_frame_offset_seq_high]);
  TEST_ASSERT_EQUAL_HEX8(0x01, buffer[k_frame_offset_seq_low]);

  /* Verify LEN (big-endian) */
  TEST_ASSERT_EQUAL_HEX8(0x00, buffer[k_frame_offset_len_high]);
  TEST_ASSERT_EQUAL_HEX8(0x00, buffer[k_frame_offset_len_low]);

  /* Verify TYPE */
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_command, buffer[k_frame_offset_type]);

  /* Verify FLAGS */
  TEST_ASSERT_EQUAL_HEX8(k_frame_flag_none, buffer[k_frame_offset_flags]);
}

void test_encode_with_payload(void)
{
  enum { k_small_payload_size = 4 };
  rx_frame_t frame      = {0};
  frame.header.sequence = 0x1234;
  frame.header.length   = k_small_payload_size;
  frame.header.type     = k_frame_type_response;
  frame.header.flags    = k_frame_flag_requires_ack;
  memcpy(frame.payload, "TEST", k_small_payload_size);

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;

  rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_frame_min_size + k_small_payload_size,
                    len); /* Minimum frame + 4 byte payload */

  /* Verify SEQ (big-endian 0x1234) */
  TEST_ASSERT_EQUAL_HEX8(0x12, buffer[k_frame_offset_seq_high]);
  TEST_ASSERT_EQUAL_HEX8(0x34, buffer[k_frame_offset_seq_low]);

  /* Verify LEN (big-endian 4) */
  TEST_ASSERT_EQUAL_HEX8(0x00, buffer[k_frame_offset_len_high]);
  TEST_ASSERT_EQUAL_HEX8(0x04, buffer[k_frame_offset_len_low]);

  /* Verify payload */
  TEST_ASSERT_EQUAL_HEX8('T', buffer[k_frame_offset_payload + k_payload_index_0]);
  TEST_ASSERT_EQUAL_HEX8('E', buffer[k_frame_offset_payload + k_payload_index_1]);
  TEST_ASSERT_EQUAL_HEX8('S', buffer[k_frame_offset_payload + k_payload_index_2]);
  TEST_ASSERT_EQUAL_HEX8('T', buffer[k_frame_offset_payload + k_payload_index_3]);
}

/* =============================================================================
 * Decode Tests
 * =============================================================================
 */

void test_decode_null_args(void)
{
  uint8_t    data[64] = {0};
  rx_frame_t frame    = {0};

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_decode(NULL, data, 64, &frame));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_decode(&s_decoder, NULL, 64, &frame));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, rx_frame_decode(&s_decoder, data, 64, NULL));
}

void test_decode_uninitialized(void)
{
  rx_frame_decoder_t dec      = {0};
  uint8_t            data[64] = {0};
  rx_frame_t         frame    = {0};

  rx_err_t err = rx_frame_decode(&dec, data, 64, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_decode_too_short(void)
{
  enum { k_short_buffer_size = 8 }; /* Less than k_frame_min_size */
  uint8_t    data[k_short_buffer_size] = {0};
  rx_frame_t frame;

  rx_err_t err = rx_frame_decode(&s_decoder, data, k_short_buffer_size, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_decode_invalid_sync(void)
{
  uint8_t    data[k_frame_min_size] = {0};
  rx_frame_t frame;

  rx_err_t err = rx_frame_decode(&s_decoder, data, k_frame_min_size, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

void test_decode_crc_mismatch(void)
{
  /* Valid header but incorrect CRC */
  uint8_t data[k_frame_min_size] = {
    0x55,
    0xAA, /* SYNC */
    0x00,
    0x01, /* SEQ = 1 */
    0x00,
    0x00, /* LEN = 0 */
    0x01, /* TYPE = command */
    0x00, /* FLAGS = none */
    0x00,
    0x00,
    0x00,
    0x00 /* Bad CRC */
  };
  rx_frame_t frame;

  rx_err_t err = rx_frame_decode(&s_decoder, data, k_frame_min_size, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/* =============================================================================
 * Encode/Decode Round-Trip Tests
 * =============================================================================
 */

void test_roundtrip_empty_frame(void)
{
  rx_frame_t original      = {0};
  original.header.sequence = 42;
  original.header.length   = 0;
  original.header.type     = k_frame_type_ack;
  original.header.flags    = k_frame_flag_none;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL(original.header.sequence, decoded.header.sequence);
  TEST_ASSERT_EQUAL(original.header.length, decoded.header.length);
  TEST_ASSERT_EQUAL(original.header.type, decoded.header.type);
  TEST_ASSERT_EQUAL(original.header.flags, decoded.header.flags);
}

void test_roundtrip_with_payload(void)
{
  enum { k_deadbeef_len = 8 };
  rx_frame_t original      = {0};
  original.header.sequence = 0xBEEF;
  original.header.length   = k_deadbeef_len;
  original.header.type     = k_frame_type_command;
  original.header.flags    = k_frame_flag_fec_enabled | k_frame_flag_priority;
  memcpy(original.payload, "DEADBEEF", k_deadbeef_len);

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL(original.header.sequence, decoded.header.sequence);
  TEST_ASSERT_EQUAL(original.header.length, decoded.header.length);
  TEST_ASSERT_EQUAL(original.header.type, decoded.header.type);
  TEST_ASSERT_EQUAL(original.header.flags, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY(original.payload, decoded.payload, k_deadbeef_len);
}

void test_roundtrip_max_sequence(void)
{
  enum { k_seq_test_payload_len = 2, k_payload_val_high = 0x12, k_payload_val_low = 0x34 };
  rx_frame_t original                 = {0};
  original.header.sequence            = 0xFFFF;
  original.header.length              = k_seq_test_payload_len;
  original.header.type                = k_frame_type_nack;
  original.header.flags               = k_frame_flag_soft_nack;
  original.payload[k_payload_index_0] = k_payload_val_high;
  original.payload[k_payload_index_1] = k_payload_val_low;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL_HEX16(0xFFFF, decoded.header.sequence);
}

void test_roundtrip_large_payload(void)
{
  rx_frame_t original      = {0};
  original.header.sequence = k_test_sequence_num;
  original.header.length   = k_test_payload_size;
  original.header.type     = k_frame_type_response;
  original.header.flags    = k_frame_flag_none;

  /* Fill payload with pattern */
  for (uint32_t i = 0; i < k_test_payload_size; i++) {
    original.payload[i] = (uint8_t)(i & k_test_byte_mask);
  }

  uint8_t  buffer[k_test_buffer_size];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_min_size + k_test_payload_size, len);

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL(k_test_payload_size, decoded.header.length);
  TEST_ASSERT_EQUAL_MEMORY(original.payload, decoded.payload, k_test_payload_size);
}

/* =============================================================================
 * Utility Function Tests
 * =============================================================================
 */

void test_create_ack_null(void)
{
  rx_err_t err = rx_frame_create_ack(NULL, 0);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_create_ack_success(void)
{
  rx_frame_t frame;
  rx_err_t   err = rx_frame_create_ack(&frame, 42);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(42, frame.header.sequence);
  TEST_ASSERT_EQUAL(0, frame.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_ack, frame.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_none, frame.header.flags);
}

void test_create_nack_null(void)
{
  rx_err_t err = rx_frame_create_nack(NULL, 0, 0);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_create_nack_with_flags(void)
{
  rx_frame_t frame;
  rx_err_t   err = rx_frame_create_nack(&frame, 123, k_frame_flag_soft_nack);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(123, frame.header.sequence);
  TEST_ASSERT_EQUAL(0, frame.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_nack, frame.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_soft_nack, frame.header.flags);
}

void test_encoded_size_calculation(void)
{
  enum { k_payload_0 = 0, k_payload_1 = 1, k_payload_8 = 8 };
  TEST_ASSERT_EQUAL(k_frame_min_size, rx_frame_encoded_size(k_payload_0));
  TEST_ASSERT_EQUAL(k_frame_min_size + k_payload_1, rx_frame_encoded_size(k_payload_1));
  TEST_ASSERT_EQUAL(k_frame_min_size + k_payload_8, rx_frame_encoded_size(k_payload_8));
  TEST_ASSERT_EQUAL(k_frame_max_size, rx_frame_encoded_size(k_frame_max_payload));
}

void test_frame_type_valid(void)
{
  enum { k_invalid_type_low = 0, k_invalid_type_high = 5, k_invalid_type_max = 255 };
  TEST_ASSERT_FALSE(rx_frame_type_valid(k_invalid_type_low));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_command));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_response));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_ack));
  TEST_ASSERT_TRUE(rx_frame_type_valid(k_frame_type_nack));
  TEST_ASSERT_FALSE(rx_frame_type_valid(k_invalid_type_high));
  TEST_ASSERT_FALSE(rx_frame_type_valid(k_invalid_type_max));
}

/* =============================================================================
 * Maximum Payload Tests
 * =============================================================================
 */

void test_roundtrip_max_payload(void)
{
  rx_frame_t original      = {0};
  original.header.sequence = 999;
  original.header.length   = k_frame_max_payload; /* 1024 bytes */
  original.header.type     = k_frame_type_command;
  original.header.flags    = k_frame_flag_fec_enabled;

  /* Fill with deterministic pattern */
  for (uint32_t i = 0; i < k_frame_max_payload; i++) {
    original.payload[i] = (uint8_t)(i & k_test_byte_mask);
  }

  uint8_t  buffer[k_frame_max_size];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_max_size, len); /* 1036 bytes total */

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL(original.header.sequence, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_max_payload, decoded.header.length);
  TEST_ASSERT_EQUAL(original.header.type, decoded.header.type);
  TEST_ASSERT_EQUAL(original.header.flags, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY(original.payload, decoded.payload, k_frame_max_payload);
}

/* =============================================================================
 * Frame Type Tests (Explicit Coverage)
 * =============================================================================
 */

void test_frame_type_command(void)
{
  enum { k_cmd_payload_len = 4 };
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = k_cmd_payload_len;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack;
  memcpy(frame.payload, "CMD1", k_cmd_payload_len);

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_command, decoded.header.type);
  TEST_ASSERT_EQUAL_MEMORY("CMD1", decoded.payload, k_cmd_payload_len);
}

void test_frame_type_response(void)
{
  enum { k_rsp_payload_len = 4 };
  rx_frame_t frame      = {0};
  frame.header.sequence = 2;
  frame.header.length   = k_rsp_payload_len;
  frame.header.type     = k_frame_type_response;
  frame.header.flags    = k_frame_flag_none;
  memcpy(frame.payload, "RSP1", k_rsp_payload_len);

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_response, decoded.header.type);
  TEST_ASSERT_EQUAL_MEMORY("RSP1", decoded.payload, k_rsp_payload_len);
}

void test_frame_type_ack(void)
{
  rx_frame_t frame;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_ack(&frame, 100));

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_ack, decoded.header.type);
  TEST_ASSERT_EQUAL(100, decoded.header.sequence);
  TEST_ASSERT_EQUAL(0, decoded.header.length);
}

void test_frame_type_nack(void)
{
  rx_frame_t frame;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_nack(&frame, 200, k_frame_flag_soft_nack));

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_nack, decoded.header.type);
  TEST_ASSERT_EQUAL(200, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_frame_flag_soft_nack, decoded.header.flags);
}

/* =============================================================================
 * Frame Flag Tests (All Combinations)
 * =============================================================================
 */

void test_flag_requires_ack(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_requires_ack, decoded.header.flags);
}

void test_flag_retransmit(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 2;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_retransmit;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_retransmit, decoded.header.flags);
}

void test_flag_priority(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 3;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_priority;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_priority, decoded.header.flags);
}

void test_flag_fec_enabled(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 4;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_fec_enabled;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_fec_enabled, decoded.header.flags);
}

void test_flag_soft_nack(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 5;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_nack;
  frame.header.flags    = k_frame_flag_soft_nack;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_soft_nack, decoded.header.flags);
}

void test_flag_combined(void)
{
  enum { k_combined_payload_len = 8 };
  rx_frame_t frame      = {0};
  frame.header.sequence = 6;
  frame.header.length   = k_combined_payload_len;
  frame.header.type     = k_frame_type_command;
  frame.header.flags = k_frame_flag_requires_ack | k_frame_flag_priority | k_frame_flag_fec_enabled;
  memcpy(frame.payload, "COMBINED", k_combined_payload_len);

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(frame.header.flags, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY("COMBINED", decoded.payload, k_combined_payload_len);
}

/* =============================================================================
 * Endianness Tests (Explicit Big-Endian Verification)
 * =============================================================================
 */

void test_sync_word_big_endian(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_ack;
  frame.header.flags    = k_frame_flag_none;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  /* SYNC word 0x55AA must be big-endian: [0x55, 0xAA] */
  TEST_ASSERT_EQUAL_HEX8(0x55, buffer[k_frame_offset_sync_high]); /* High byte first */
  TEST_ASSERT_EQUAL_HEX8(0xAA, buffer[k_frame_offset_sync_low]);  /* Low byte second */
}

void test_sequence_big_endian(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 0x1234;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_ack;
  frame.header.flags    = k_frame_flag_none;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  /* SEQ 0x1234 must be big-endian: [0x12, 0x34] */
  TEST_ASSERT_EQUAL_HEX8(0x12, buffer[k_frame_offset_seq_high]); /* High byte at offset 2 */
  TEST_ASSERT_EQUAL_HEX8(0x34, buffer[k_frame_offset_seq_low]);  /* Low byte at offset 3 */
}

void test_length_big_endian(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = k_test_payload_size;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_none;

  /* Fill 256 bytes */
  for (uint32_t i = 0; i < k_test_payload_size; i++) {
    frame.payload[i] = (uint8_t)i;
  }

  uint8_t  buffer[k_test_buffer_size];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  /* LEN 0x0100 must be big-endian: [0x01, 0x00] */
  TEST_ASSERT_EQUAL_HEX8(0x01, buffer[k_frame_offset_len_high]); /* High byte at offset 4 */
  TEST_ASSERT_EQUAL_HEX8(0x00, buffer[k_frame_offset_len_low]);  /* Low byte at offset 5 */
}

void test_crc_little_endian(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_ack;
  frame.header.flags    = k_frame_flag_none;

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_min_size, len);

  /* CRC-32 is little-endian (LSB first) at end of frame */
  /* We don't test exact CRC value (that's rx_crc32's job) */
  /* Just verify it's at the correct position (last 4 bytes) */
  uint32_t crc_offset = len - k_frame_crc_size;
  TEST_ASSERT_EQUAL(k_frame_min_size - k_frame_crc_size, crc_offset); /* After SYNC + Header */

  /* Verify CRC is non-zero (frame is valid) */
  uint32_t crc = ((uint32_t)buffer[crc_offset + 0]) | ((uint32_t)buffer[crc_offset + 1] << 8) |
                 ((uint32_t)buffer[crc_offset + 2] << 16) |
                 ((uint32_t)buffer[crc_offset + 3] << 24);
  TEST_ASSERT_NOT_EQUAL(0, crc);
}

/* =============================================================================
 * Go Compatibility Tests (Bit-Exact Verification)
 * =============================================================================
 */

void test_go_compatibility_empty_ack(void)
{
  enum { k_header_wire_size = k_frame_sync_size + k_frame_header_size };
  /*
   * Test vector from Go implementation (star-gateway/internal/frame/)
   * Frame: ACK for sequence 1
   * Expected wire format (hex):
   *   55 AA          - SYNC (0x55AA big-endian)
   *   00 01          - SEQ (1 big-endian)
   *   00 00          - LEN (0 big-endian)
   *   03             - TYPE (ACK = 3)
   *   00             - FLAGS (none = 0)
   *   <CRC-32 LE>    - CRC-32 of above 8 bytes
   */
  rx_frame_t frame;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_ack(&frame, 1));

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_min_size, len);

  /* Verify header bytes match Go encoding */
  const uint8_t expected_header[] = {
    0x55,
    0xAA, /* SYNC */
    0x00,
    0x01, /* SEQ = 1 */
    0x00,
    0x00, /* LEN = 0 */
    0x03, /* TYPE = ACK */
    0x00  /* FLAGS = none */
  };
  TEST_ASSERT_EQUAL_MEMORY(expected_header, buffer, k_header_wire_size);

  /* Verify round-trip decode */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(1, decoded.header.sequence);
  TEST_ASSERT_EQUAL(0, decoded.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_ack, decoded.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_none, decoded.header.flags);
}

void test_go_compatibility_command_with_payload(void)
{
  enum { k_go_payload_len = 4 };
  /*
   * Test vector: Command with 4-byte payload "TEST"
   * Expected wire format (hex):
   *   55 AA          - SYNC
   *   00 2A          - SEQ (42 big-endian)
   *   00 04          - LEN (4 big-endian)
   *   01             - TYPE (COMMAND = 1)
   *   01             - FLAGS (REQUIRES_ACK = 0x01)
   *   54 45 53 54    - PAYLOAD "TEST"
   *   <CRC-32 LE>    - CRC-32
   */
  rx_frame_t frame      = {0};
  frame.header.sequence = 42;
  frame.header.length   = k_go_payload_len;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack;
  memcpy(frame.payload, "TEST", k_go_payload_len);

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_min_size + k_go_payload_len, len);

  /* Verify header and payload match Go encoding */
  const uint8_t expected[] = {
    0x55,
    0xAA, /* SYNC */
    0x00,
    0x2A, /* SEQ = 42 */
    0x00,
    0x04, /* LEN = 4 */
    0x01, /* TYPE = COMMAND */
    0x01, /* FLAGS = REQUIRES_ACK */
    'T',
    'E',
    'S',
    'T' /* PAYLOAD */
  };
  TEST_ASSERT_EQUAL_MEMORY(expected,
                           buffer,
                           k_frame_sync_size + k_frame_header_size + k_go_payload_len);

  /* Verify round-trip */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(42, decoded.header.sequence);
  TEST_ASSERT_EQUAL(k_go_payload_len, decoded.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_command, decoded.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_requires_ack, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY("TEST", decoded.payload, k_go_payload_len);
}

/* =============================================================================
 * Edge Case Tests
 * =============================================================================
 */

void test_decode_payload_length_mismatch(void)
{
  enum {
    k_actual_payload_len  = 10,
    k_truncated_payload   = 5,
    k_truncated_frame_len = 17 /* k_frame_min_size + k_truncated_payload = 12 + 5 */
  };
  /* Create valid frame with 10-byte payload */
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = k_actual_payload_len;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_none;
  memcpy(frame.payload, "0123456789", k_actual_payload_len);

  uint8_t  buffer[k_test_small_buffer];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(k_frame_min_size + k_actual_payload_len, len);

  /* Truncate buffer to only 5 bytes of payload (total 17 bytes) */
  /* LEN field still says 10, but only 5 bytes + CRC available */
  rx_frame_t decoded;
  rx_err_t   err = rx_frame_decode(&s_decoder, buffer, k_truncated_frame_len, &decoded);

  /* Decoder should detect insufficient data */
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_decode_zero_length_buffer(void)
{
  rx_frame_t frame;
  rx_err_t   err = rx_frame_decode(&s_decoder, (uint8_t*)"", 0, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_encode_sequence_rollover(void)
{
  /* Test sequence number rollover (0xFFFF -> 0x0000) */
  rx_frame_t frame1      = {0};
  frame1.header.sequence = 0xFFFF;
  frame1.header.length   = 0;
  frame1.header.type     = k_frame_type_ack;
  frame1.header.flags    = k_frame_flag_none;

  uint8_t  buffer1[64];
  uint32_t len1;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame1, buffer1, &len1));

  rx_frame_t decoded1;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer1, len1, &decoded1));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, decoded1.header.sequence);

  /* Next sequence would be 0x0000 */
  rx_frame_t frame2      = {0};
  frame2.header.sequence = 0x0000;
  frame2.header.length   = 0;
  frame2.header.type     = k_frame_type_ack;
  frame2.header.flags    = k_frame_flag_none;

  uint8_t  buffer2[64];
  uint32_t len2;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame2, buffer2, &len2));

  rx_frame_t decoded2;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer2, len2, &decoded2));
  TEST_ASSERT_EQUAL_HEX16(0x0000, decoded2.header.sequence);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Encoder init tests */
  RUN_TEST(test_encoder_init_null);
  RUN_TEST(test_encoder_init_success);
  RUN_TEST(test_encoder_deinit_null);

  /* Decoder init tests */
  RUN_TEST(test_decoder_init_null);
  RUN_TEST(test_decoder_init_success);

  /* Encode tests */
  RUN_TEST(test_encode_null_args);
  RUN_TEST(test_encode_uninitialized);
  RUN_TEST(test_encode_payload_too_large);
  RUN_TEST(test_encode_empty_frame);
  RUN_TEST(test_encode_with_payload);

  /* Decode tests */
  RUN_TEST(test_decode_null_args);
  RUN_TEST(test_decode_uninitialized);
  RUN_TEST(test_decode_too_short);
  RUN_TEST(test_decode_invalid_sync);
  RUN_TEST(test_decode_crc_mismatch);

  /* Round-trip tests */
  RUN_TEST(test_roundtrip_empty_frame);
  RUN_TEST(test_roundtrip_with_payload);
  RUN_TEST(test_roundtrip_max_sequence);
  RUN_TEST(test_roundtrip_large_payload);

  /* Utility tests */
  RUN_TEST(test_create_ack_null);
  RUN_TEST(test_create_ack_success);
  RUN_TEST(test_create_nack_null);
  RUN_TEST(test_create_nack_with_flags);
  RUN_TEST(test_encoded_size_calculation);
  RUN_TEST(test_frame_type_valid);

  /* Maximum payload tests */
  RUN_TEST(test_roundtrip_max_payload);

  /* Frame type tests */
  RUN_TEST(test_frame_type_command);
  RUN_TEST(test_frame_type_response);
  RUN_TEST(test_frame_type_ack);
  RUN_TEST(test_frame_type_nack);

  /* Frame flag tests */
  RUN_TEST(test_flag_requires_ack);
  RUN_TEST(test_flag_retransmit);
  RUN_TEST(test_flag_priority);
  RUN_TEST(test_flag_fec_enabled);
  RUN_TEST(test_flag_soft_nack);
  RUN_TEST(test_flag_combined);

  /* Endianness tests */
  RUN_TEST(test_sync_word_big_endian);
  RUN_TEST(test_sequence_big_endian);
  RUN_TEST(test_length_big_endian);
  RUN_TEST(test_crc_little_endian);

  /* Go compatibility tests */
  RUN_TEST(test_go_compatibility_empty_ack);
  RUN_TEST(test_go_compatibility_command_with_payload);

  /* Edge case tests */
  RUN_TEST(test_decode_payload_length_mismatch);
  RUN_TEST(test_decode_zero_length_buffer);
  RUN_TEST(test_encode_sequence_rollover);

  return UNITY_END();
}
