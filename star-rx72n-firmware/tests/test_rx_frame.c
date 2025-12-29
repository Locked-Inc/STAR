/**
 * @file test_rx_frame.c
 * @brief Unit Tests for Frame Layer
 *
 * Tests frame encoding/decoding with CRC-32 verification.
 * Verifies bit-exact compatibility with Go implementation.
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "unity.h"
#include "rx_frame.h"
#include <string.h>

/* Test fixtures */
static rx_frame_encoder_t s_encoder;
static rx_frame_decoder_t s_decoder;

void setUp(void) {
    rx_frame_encoder_init(&s_encoder);
    rx_frame_decoder_init(&s_decoder);
}

void tearDown(void) {
    rx_frame_encoder_deinit(&s_encoder);
    rx_frame_decoder_deinit(&s_decoder);
}

/* =============================================================================
 * Encoder Initialization Tests
 * =============================================================================
 */

void test_encoder_init_null(void) {
    rx_err_t err = rx_frame_encoder_init(NULL);
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG, err);
}

void test_encoder_init_success(void) {
    rx_frame_encoder_t enc;
    rx_err_t err = rx_frame_encoder_init(&enc);
    TEST_ASSERT_EQUAL(RX_OK, err);
    TEST_ASSERT_NOT_EQUAL(0, enc.initialized);
}

void test_encoder_deinit_null(void) {
    rx_err_t err = rx_frame_encoder_deinit(NULL);
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG, err);
}

/* =============================================================================
 * Decoder Initialization Tests
 * =============================================================================
 */

void test_decoder_init_null(void) {
    rx_err_t err = rx_frame_decoder_init(NULL);
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG, err);
}

void test_decoder_init_success(void) {
    rx_frame_decoder_t dec;
    rx_err_t err = rx_frame_decoder_init(&dec);
    TEST_ASSERT_EQUAL(RX_OK, err);
    TEST_ASSERT_NOT_EQUAL(0, dec.initialized);
}

/* =============================================================================
 * Encode Tests
 * =============================================================================
 */

void test_encode_null_args(void) {
    rx_frame_t frame = {0};
    uint8_t buffer[64];
    size_t len;

    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG,
                      rx_frame_encode(NULL, &frame, buffer, &len));
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG,
                      rx_frame_encode(&s_encoder, NULL, buffer, &len));
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG,
                      rx_frame_encode(&s_encoder, &frame, NULL, &len));
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG,
                      rx_frame_encode(&s_encoder, &frame, buffer, NULL));
}

void test_encode_uninitialized(void) {
    rx_frame_encoder_t enc = {0};
    rx_frame_t frame       = {0};
    uint8_t buffer[64];
    size_t len;

    rx_err_t err = rx_frame_encode(&enc, &frame, buffer, &len);
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_STATE, err);
}

void test_encode_payload_too_large(void) {
    rx_frame_t frame = {0};
    frame.header.length =
        k_frame_max_payload + 1; /* 1025 bytes (exceeds max) */
    uint8_t buffer[2048];
    size_t len;

    rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_SIZE, err);
}

void test_encode_empty_frame(void) {
    rx_frame_t frame      = {0};
    frame.header.sequence = 1;
    frame.header.length   = 0;
    frame.header.type     = k_frame_type_command;
    frame.header.flags    = k_frame_flag_none;

    uint8_t buffer[64];
    size_t len;

    rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
    TEST_ASSERT_EQUAL(RX_OK, err);
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

void test_encode_with_payload(void) {
    rx_frame_t frame      = {0};
    frame.header.sequence = 0x1234;
    frame.header.length   = 4;
    frame.header.type     = k_frame_type_response;
    frame.header.flags    = k_frame_flag_requires_ack;
    memcpy(frame.payload, "TEST", 4);

    uint8_t buffer[64];
    size_t len;

    rx_err_t err = rx_frame_encode(&s_encoder, &frame, buffer, &len);
    TEST_ASSERT_EQUAL(RX_OK, err);
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

void test_decode_null_args(void) {
    uint8_t data[64]  = {0};
    rx_frame_t frame  = {0};

    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG,
                      rx_frame_decode(NULL, data, 64, &frame));
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG,
                      rx_frame_decode(&s_decoder, NULL, 64, &frame));
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG,
                      rx_frame_decode(&s_decoder, data, 64, NULL));
}

void test_decode_uninitialized(void) {
    rx_frame_decoder_t dec = {0};
    uint8_t data[64]       = {0};
    rx_frame_t frame       = {0};

    rx_err_t err = rx_frame_decode(&dec, data, 64, &frame);
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_STATE, err);
}

void test_decode_too_short(void) {
    uint8_t data[8] = {0};
    rx_frame_t frame;

    rx_err_t err = rx_frame_decode(&s_decoder, data, 8, &frame);
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_SIZE, err);
}

void test_decode_invalid_sync(void) {
    uint8_t data[12] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    rx_frame_t frame;

    rx_err_t err = rx_frame_decode(&s_decoder, data, 12, &frame);
    TEST_ASSERT_EQUAL(RX_ERR_PROTOCOL_ERROR, err);
}

void test_decode_crc_mismatch(void) {
    /* Valid header but incorrect CRC */
    uint8_t data[12] = {
        0x55, 0xAA,       /* SYNC */
        0x00, 0x01,       /* SEQ = 1 */
        0x00, 0x00,       /* LEN = 0 */
        0x01,             /* TYPE = command */
        0x00,             /* FLAGS = none */
        0x00, 0x00, 0x00, 0x00 /* Bad CRC */
    };
    rx_frame_t frame;

    rx_err_t err = rx_frame_decode(&s_decoder, data, 12, &frame);
    TEST_ASSERT_EQUAL(RX_ERR_CRC_MISMATCH, err);
}

/* =============================================================================
 * Encode/Decode Round-Trip Tests
 * =============================================================================
 */

void test_roundtrip_empty_frame(void) {
    rx_frame_t original   = {0};
    original.header.sequence = 42;
    original.header.length   = 0;
    original.header.type     = k_frame_type_ack;
    original.header.flags    = k_frame_flag_none;

    uint8_t buffer[64];
    size_t len;
    TEST_ASSERT_EQUAL(RX_OK,
                      rx_frame_encode(&s_encoder, &original, buffer, &len));

    rx_frame_t decoded;
    TEST_ASSERT_EQUAL(RX_OK,
                      rx_frame_decode(&s_decoder, buffer, len, &decoded));

    TEST_ASSERT_EQUAL(original.header.sequence, decoded.header.sequence);
    TEST_ASSERT_EQUAL(original.header.length, decoded.header.length);
    TEST_ASSERT_EQUAL(original.header.type, decoded.header.type);
    TEST_ASSERT_EQUAL(original.header.flags, decoded.header.flags);
}

void test_roundtrip_with_payload(void) {
    rx_frame_t original      = {0};
    original.header.sequence = 0xBEEF;
    original.header.length   = 8;
    original.header.type     = k_frame_type_command;
    original.header.flags    = k_frame_flag_fec_enabled | k_frame_flag_priority;
    memcpy(original.payload, "DEADBEEF", 8);

    uint8_t buffer[64];
    size_t len;
    TEST_ASSERT_EQUAL(RX_OK,
                      rx_frame_encode(&s_encoder, &original, buffer, &len));

    rx_frame_t decoded;
    TEST_ASSERT_EQUAL(RX_OK,
                      rx_frame_decode(&s_decoder, buffer, len, &decoded));

    TEST_ASSERT_EQUAL(original.header.sequence, decoded.header.sequence);
    TEST_ASSERT_EQUAL(original.header.length, decoded.header.length);
    TEST_ASSERT_EQUAL(original.header.type, decoded.header.type);
    TEST_ASSERT_EQUAL(original.header.flags, decoded.header.flags);
    TEST_ASSERT_EQUAL_MEMORY(original.payload, decoded.payload, 8);
}

void test_roundtrip_max_sequence(void) {
    rx_frame_t original      = {0};
    original.header.sequence = 0xFFFF;
    original.header.length   = 2;
    original.header.type     = k_frame_type_nack;
    original.header.flags    = k_frame_flag_soft_nack;
    original.payload[0]      = 0x12;
    original.payload[1]      = 0x34;

    uint8_t buffer[64];
    size_t len;
    TEST_ASSERT_EQUAL(RX_OK,
                      rx_frame_encode(&s_encoder, &original, buffer, &len));

    rx_frame_t decoded;
    TEST_ASSERT_EQUAL(RX_OK,
                      rx_frame_decode(&s_decoder, buffer, len, &decoded));

    TEST_ASSERT_EQUAL_HEX16(0xFFFF, decoded.header.sequence);
}

void test_roundtrip_large_payload(void) {
    rx_frame_t original = {0};
    original.header.sequence = 100;
    original.header.length   = 256;
    original.header.type     = k_frame_type_response;
    original.header.flags    = k_frame_flag_none;

    /* Fill payload with pattern */
    for (int i = 0; i < 256; i++) {
        original.payload[i] = (uint8_t)(i & 0xFF);
    }

    uint8_t buffer[512];
    size_t len;
    TEST_ASSERT_EQUAL(RX_OK,
                      rx_frame_encode(&s_encoder, &original, buffer, &len));
    TEST_ASSERT_EQUAL(12 + 256, len);

    rx_frame_t decoded;
    TEST_ASSERT_EQUAL(RX_OK,
                      rx_frame_decode(&s_decoder, buffer, len, &decoded));

    TEST_ASSERT_EQUAL(256, decoded.header.length);
    TEST_ASSERT_EQUAL_MEMORY(original.payload, decoded.payload, 256);
}

/* =============================================================================
 * Utility Function Tests
 * =============================================================================
 */

void test_create_ack_null(void) {
    rx_err_t err = rx_frame_create_ack(NULL, 0);
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG, err);
}

void test_create_ack_success(void) {
    rx_frame_t frame;
    rx_err_t err = rx_frame_create_ack(&frame, 42);

    TEST_ASSERT_EQUAL(RX_OK, err);
    TEST_ASSERT_EQUAL(42, frame.header.sequence);
    TEST_ASSERT_EQUAL(0, frame.header.length);
    TEST_ASSERT_EQUAL(k_frame_type_ack, frame.header.type);
    TEST_ASSERT_EQUAL(k_frame_flag_none, frame.header.flags);
}

void test_create_nack_null(void) {
    rx_err_t err = rx_frame_create_nack(NULL, 0, 0);
    TEST_ASSERT_EQUAL(RX_ERR_INVALID_ARG, err);
}

void test_create_nack_with_flags(void) {
    rx_frame_t frame;
    rx_err_t err = rx_frame_create_nack(&frame, 123, k_frame_flag_soft_nack);

    TEST_ASSERT_EQUAL(RX_OK, err);
    TEST_ASSERT_EQUAL(123, frame.header.sequence);
    TEST_ASSERT_EQUAL(0, frame.header.length);
    TEST_ASSERT_EQUAL(k_frame_type_nack, frame.header.type);
    TEST_ASSERT_EQUAL(k_frame_flag_soft_nack, frame.header.flags);
}

void test_encoded_size_calculation(void) {
    TEST_ASSERT_EQUAL(12, rx_frame_encoded_size(0));    /* Min frame */
    TEST_ASSERT_EQUAL(13, rx_frame_encoded_size(1));    /* 1 byte payload */
    TEST_ASSERT_EQUAL(20, rx_frame_encoded_size(8));    /* 8 byte payload */
    TEST_ASSERT_EQUAL(1036, rx_frame_encoded_size(1024)); /* Max payload */
}

void test_frame_type_valid(void) {
    TEST_ASSERT_FALSE(rx_frame_type_valid(0));  /* Unknown */
    TEST_ASSERT_TRUE(rx_frame_type_valid(1));   /* Command */
    TEST_ASSERT_TRUE(rx_frame_type_valid(2));   /* Response */
    TEST_ASSERT_TRUE(rx_frame_type_valid(3));   /* ACK */
    TEST_ASSERT_TRUE(rx_frame_type_valid(4));   /* NACK */
    TEST_ASSERT_FALSE(rx_frame_type_valid(5));  /* Out of range */
    TEST_ASSERT_FALSE(rx_frame_type_valid(255)); /* Way out of range */
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void) {
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

    return UNITY_END();
}
