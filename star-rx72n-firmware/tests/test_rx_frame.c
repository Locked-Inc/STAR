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

#include <string.h>

#include "rx_frame.h"
#include "unity.h"

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
  uint8_t    buffer[64];
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
  uint8_t            buffer[64];
  uint32_t           len;

  rx_err_t err = rx_frame_encode(&enc, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_encode_payload_too_large(void)
{
  rx_frame_t frame    = {0};
  frame.header.length = k_frame_max_payload + 1; /* 1025 bytes (exceeds max) */
  uint8_t  buffer[2048];
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

  uint8_t  buffer[64];
  uint32_t len;

  rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_frame_min_size, len); /* 12 bytes: SYNC + Header + CRC */

  /* Verify SYNC word (big-endian) */
  TEST_ASSERT_EQUAL_HEX8(0x55, buffer[0]);
  TEST_ASSERT_EQUAL_HEX8(0xAA, buffer[1]);

  /* Verify SEQ (big-endian) */
  TEST_ASSERT_EQUAL_HEX8(0x00, buffer[2]);
  TEST_ASSERT_EQUAL_HEX8(0x01, buffer[3]);

  /* Verify LEN (big-endian) */
  TEST_ASSERT_EQUAL_HEX8(0x00, buffer[4]);
  TEST_ASSERT_EQUAL_HEX8(0x00, buffer[5]);

  /* Verify TYPE */
  TEST_ASSERT_EQUAL_HEX8(k_frame_type_command, buffer[6]);

  /* Verify FLAGS */
  TEST_ASSERT_EQUAL_HEX8(k_frame_flag_none, buffer[7]);
}

void test_encode_with_payload(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 0x1234;
  frame.header.length   = 4;
  frame.header.type     = k_frame_type_response;
  frame.header.flags    = k_frame_flag_requires_ack;
  memcpy(frame.payload, "TEST", 4);

  uint8_t  buffer[64];
  uint32_t len;

  rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(16, len); /* 12 + 4 payload */

  /* Verify SEQ (big-endian 0x1234) */
  TEST_ASSERT_EQUAL_HEX8(0x12, buffer[2]);
  TEST_ASSERT_EQUAL_HEX8(0x34, buffer[3]);

  /* Verify LEN (big-endian 4) */
  TEST_ASSERT_EQUAL_HEX8(0x00, buffer[4]);
  TEST_ASSERT_EQUAL_HEX8(0x04, buffer[5]);

  /* Verify payload */
  TEST_ASSERT_EQUAL_HEX8('T', buffer[8]);
  TEST_ASSERT_EQUAL_HEX8('E', buffer[9]);
  TEST_ASSERT_EQUAL_HEX8('S', buffer[10]);
  TEST_ASSERT_EQUAL_HEX8('T', buffer[11]);
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
  uint8_t    data[8] = {0};
  rx_frame_t frame;

  rx_err_t err = rx_frame_decode(&s_decoder, data, 8, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_size, err);
}

void test_decode_invalid_sync(void)
{
  uint8_t    data[12] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  rx_frame_t frame;

  rx_err_t err = rx_frame_decode(&s_decoder, data, 12, &frame);
  TEST_ASSERT_EQUAL(k_rx_err_protocol_error, err);
}

void test_decode_crc_mismatch(void)
{
  /* Valid header but incorrect CRC */
  uint8_t data[12] = {
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

  rx_err_t err = rx_frame_decode(&s_decoder, data, 12, &frame);
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

  uint8_t  buffer[64];
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
  rx_frame_t original      = {0};
  original.header.sequence = 0xBEEF;
  original.header.length   = 8;
  original.header.type     = k_frame_type_command;
  original.header.flags    = k_frame_flag_fec_enabled | k_frame_flag_priority;
  memcpy(original.payload, "DEADBEEF", 8);

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL(original.header.sequence, decoded.header.sequence);
  TEST_ASSERT_EQUAL(original.header.length, decoded.header.length);
  TEST_ASSERT_EQUAL(original.header.type, decoded.header.type);
  TEST_ASSERT_EQUAL(original.header.flags, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY(original.payload, decoded.payload, 8);
}

void test_roundtrip_max_sequence(void)
{
  rx_frame_t original      = {0};
  original.header.sequence = 0xFFFF;
  original.header.length   = 2;
  original.header.type     = k_frame_type_nack;
  original.header.flags    = k_frame_flag_soft_nack;
  original.payload[0]      = 0x12;
  original.payload[1]      = 0x34;

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL_HEX16(0xFFFF, decoded.header.sequence);
}

void test_roundtrip_large_payload(void)
{
  rx_frame_t original      = {0};
  original.header.sequence = 100;
  original.header.length   = 256;
  original.header.type     = k_frame_type_response;
  original.header.flags    = k_frame_flag_none;

  /* Fill payload with pattern */
  for (int i = 0; i < 256; i++) {
    original.payload[i] = (uint8_t)(i & 0xFF);
  }

  uint8_t  buffer[512];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &original, buffer, &len));
  TEST_ASSERT_EQUAL(12 + 256, len);

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));

  TEST_ASSERT_EQUAL(256, decoded.header.length);
  TEST_ASSERT_EQUAL_MEMORY(original.payload, decoded.payload, 256);
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
  TEST_ASSERT_EQUAL(12, rx_frame_encoded_size(0));      /* Min frame */
  TEST_ASSERT_EQUAL(13, rx_frame_encoded_size(1));      /* 1 byte payload */
  TEST_ASSERT_EQUAL(20, rx_frame_encoded_size(8));      /* 8 byte payload */
  TEST_ASSERT_EQUAL(1036, rx_frame_encoded_size(1024)); /* Max payload */
}

void test_frame_type_valid(void)
{
  TEST_ASSERT_FALSE(rx_frame_type_valid(0));   /* Unknown */
  TEST_ASSERT_TRUE(rx_frame_type_valid(1));    /* Command */
  TEST_ASSERT_TRUE(rx_frame_type_valid(2));    /* Response */
  TEST_ASSERT_TRUE(rx_frame_type_valid(3));    /* ACK */
  TEST_ASSERT_TRUE(rx_frame_type_valid(4));    /* NACK */
  TEST_ASSERT_FALSE(rx_frame_type_valid(5));   /* Out of range */
  TEST_ASSERT_FALSE(rx_frame_type_valid(255)); /* Way out of range */
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
    original.payload[i] = (uint8_t)(i & 0xFF);
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
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = 4;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack;
  memcpy(frame.payload, "CMD1", 4);

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_command, decoded.header.type);
  TEST_ASSERT_EQUAL_MEMORY("CMD1", decoded.payload, 4);
}

void test_frame_type_response(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 2;
  frame.header.length   = 4;
  frame.header.type     = k_frame_type_response;
  frame.header.flags    = k_frame_flag_none;
  memcpy(frame.payload, "RSP1", 4);

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_type_response, decoded.header.type);
  TEST_ASSERT_EQUAL_MEMORY("RSP1", decoded.payload, 4);
}

void test_frame_type_ack(void)
{
  rx_frame_t frame;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_create_ack(&frame, 100));

  uint8_t  buffer[64];
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

  uint8_t  buffer[64];
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

  uint8_t  buffer[64];
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

  uint8_t  buffer[64];
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

  uint8_t  buffer[64];
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

  uint8_t  buffer[64];
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

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(k_frame_flag_soft_nack, decoded.header.flags);
}

void test_flag_combined(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 6;
  frame.header.length   = 8;
  frame.header.type     = k_frame_type_command;
  frame.header.flags =
    k_frame_flag_requires_ack | k_frame_flag_priority | k_frame_flag_fec_enabled;
  memcpy(frame.payload, "COMBINED", 8);

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(frame.header.flags, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY("COMBINED", decoded.payload, 8);
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

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  /* SYNC word 0x55AA must be big-endian: [0x55, 0xAA] */
  TEST_ASSERT_EQUAL_HEX8(0x55, buffer[0]); /* High byte first */
  TEST_ASSERT_EQUAL_HEX8(0xAA, buffer[1]); /* Low byte second */
}

void test_sequence_big_endian(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 0x1234;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_ack;
  frame.header.flags    = k_frame_flag_none;

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  /* SEQ 0x1234 must be big-endian: [0x12, 0x34] */
  TEST_ASSERT_EQUAL_HEX8(0x12, buffer[2]); /* High byte at offset 2 */
  TEST_ASSERT_EQUAL_HEX8(0x34, buffer[3]); /* Low byte at offset 3 */
}

void test_length_big_endian(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = 0x0100; /* 256 bytes */
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_none;

  /* Fill 256 bytes */
  for (uint32_t i = 0; i < 256; i++) {
    frame.payload[i] = (uint8_t)i;
  }

  uint8_t  buffer[512];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));

  /* LEN 0x0100 must be big-endian: [0x01, 0x00] */
  TEST_ASSERT_EQUAL_HEX8(0x01, buffer[4]); /* High byte at offset 4 */
  TEST_ASSERT_EQUAL_HEX8(0x00, buffer[5]); /* Low byte at offset 5 */
}

void test_crc_little_endian(void)
{
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = 0;
  frame.header.type     = k_frame_type_ack;
  frame.header.flags    = k_frame_flag_none;

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(12, len);

  /* CRC-32 is little-endian (LSB first) at end of frame */
  /* We don't test exact CRC value (that's rx_crc32's job) */
  /* Just verify it's at the correct position (last 4 bytes) */
  uint32_t crc_offset = len - 4;
  TEST_ASSERT_EQUAL(8, crc_offset); /* After SYNC(2) + Header(6) */

  /* Verify CRC is non-zero (frame is valid) */
  uint32_t crc = ((uint32_t)buffer[crc_offset + 0]) | ((uint32_t)buffer[crc_offset + 1] << 8) |
                 ((uint32_t)buffer[crc_offset + 2] << 16) | ((uint32_t)buffer[crc_offset + 3] << 24);
  TEST_ASSERT_NOT_EQUAL(0, crc);
}

/* =============================================================================
 * Go Compatibility Tests (Bit-Exact Verification)
 * =============================================================================
 */

void test_go_compatibility_empty_ack(void)
{
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

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(12, len);

  /* Verify header bytes match Go encoding */
  const uint8_t expected_header[] = {
    0x55, 0xAA, /* SYNC */
    0x00, 0x01, /* SEQ = 1 */
    0x00, 0x00, /* LEN = 0 */
    0x03,       /* TYPE = ACK */
    0x00        /* FLAGS = none */
  };
  TEST_ASSERT_EQUAL_MEMORY(expected_header, buffer, 8);

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
  frame.header.length   = 4;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_requires_ack;
  memcpy(frame.payload, "TEST", 4);

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(16, len); /* 12 + 4 payload */

  /* Verify header and payload match Go encoding */
  const uint8_t expected[] = {
    0x55, 0xAA,       /* SYNC */
    0x00, 0x2A,       /* SEQ = 42 */
    0x00, 0x04,       /* LEN = 4 */
    0x01,             /* TYPE = COMMAND */
    0x01,             /* FLAGS = REQUIRES_ACK */
    'T',  'E', 'S', 'T' /* PAYLOAD */
  };
  TEST_ASSERT_EQUAL_MEMORY(expected, buffer, 12);

  /* Verify round-trip */
  rx_frame_t decoded;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_decode(&s_decoder, buffer, len, &decoded));
  TEST_ASSERT_EQUAL(42, decoded.header.sequence);
  TEST_ASSERT_EQUAL(4, decoded.header.length);
  TEST_ASSERT_EQUAL(k_frame_type_command, decoded.header.type);
  TEST_ASSERT_EQUAL(k_frame_flag_requires_ack, decoded.header.flags);
  TEST_ASSERT_EQUAL_MEMORY("TEST", decoded.payload, 4);
}

/* =============================================================================
 * Edge Case Tests
 * =============================================================================
 */

void test_decode_payload_length_mismatch(void)
{
  /* Create valid frame with 10-byte payload */
  rx_frame_t frame      = {0};
  frame.header.sequence = 1;
  frame.header.length   = 10;
  frame.header.type     = k_frame_type_command;
  frame.header.flags    = k_frame_flag_none;
  memcpy(frame.payload, "0123456789", 10);

  uint8_t  buffer[64];
  uint32_t len;
  TEST_ASSERT_EQUAL(k_rx_ok, rx_frame_encode(&s_encoder, &frame, buffer, &len));
  TEST_ASSERT_EQUAL(22, len); /* 12 + 10 */

  /* Truncate buffer to only 5 bytes of payload (total 17 bytes) */
  /* LEN field still says 10, but only 5 bytes + CRC available */
  rx_frame_t decoded;
  rx_err_t   err = rx_frame_decode(&s_decoder, buffer, 17, &decoded);

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
