/**
 * @file test_frame.c
 * @brief Unit tests for bb_frame one-shot encode/decode.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "bb_frame.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------------------*/

static uint32_t s_failures  = 0U;
static uint32_t s_tests_run = 0U;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            (void)fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            s_failures++; \
        } \
    } while (0)

/* ---------------------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------------------*/

static void test_frame_encode_decode_round_trip(void)
{
    s_tests_run++;
    bb_frame_t original = {0};
    original.header.sequence = 42U;
    original.header.length   = 5U;
    original.header.type     = k_bb_frame_type_command;
    original.header.flags    = k_bb_frame_flag_requires_ack;

    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    (void)memcpy(original.payload, payload, 5U);

    uint8_t wire[1036] = {0};
    uint32_t wire_len = 0U;
    bb_err_t err = bb_frame_encode(&original, wire, sizeof(wire), &wire_len);
    TEST_ASSERT(err == k_bb_err_ok, "encode failed");
    TEST_ASSERT(wire_len == 17U, "encode wrong length"); /* 12 overhead + 5 payload */

    bb_frame_t decoded = {0};
    err = bb_frame_decode(wire, wire_len, &decoded);
    TEST_ASSERT(err == k_bb_err_ok, "decode failed");
    TEST_ASSERT(decoded.header.sequence == 42U, "sequence mismatch");
    TEST_ASSERT(decoded.header.length == 5U, "length mismatch");
    TEST_ASSERT(decoded.header.type == k_bb_frame_type_command, "type mismatch");
    TEST_ASSERT(decoded.header.flags == k_bb_frame_flag_requires_ack, "flags mismatch");
    TEST_ASSERT(memcmp(decoded.payload, payload, 5U) == 0, "payload mismatch");
}

static void test_frame_encode_empty_payload(void)
{
    s_tests_run++;
    bb_frame_t ack = {0};
    (void)bb_frame_build_ack(7U, &ack);

    uint8_t wire[64] = {0};
    uint32_t wire_len = 0U;
    bb_err_t err = bb_frame_encode(&ack, wire, sizeof(wire), &wire_len);
    TEST_ASSERT(err == k_bb_err_ok, "encode ack failed");
    TEST_ASSERT(wire_len == 12U, "ack wrong length"); /* overhead only */

    bb_frame_t decoded = {0};
    err = bb_frame_decode(wire, wire_len, &decoded);
    TEST_ASSERT(err == k_bb_err_ok, "decode ack failed");
    TEST_ASSERT(decoded.header.sequence == 7U, "ack seq mismatch");
    TEST_ASSERT(decoded.header.type == k_bb_frame_type_ack, "ack type mismatch");
    TEST_ASSERT(decoded.header.length == 0U, "ack length mismatch");
}

static void test_frame_decode_crc_mismatch(void)
{
    s_tests_run++;
    bb_frame_t frame = {0};
    frame.header.sequence = 1U;
    frame.header.length   = 3U;
    frame.header.type     = k_bb_frame_type_command;
    frame.payload[0]      = 0xAA;
    frame.payload[1]      = 0xBB;
    frame.payload[2]      = 0xCC;

    uint8_t wire[64] = {0};
    uint32_t wire_len = 0U;
    (void)bb_frame_encode(&frame, wire, sizeof(wire), &wire_len);

    /* Corrupt payload byte */
    wire[8] = 0xFF;

    bb_frame_t decoded = {0};
    bb_err_t err = bb_frame_decode(wire, wire_len, &decoded);
    TEST_ASSERT(err == k_bb_err_crc_mismatch, "expected CRC mismatch");
}

static void test_frame_decode_truncated(void)
{
    s_tests_run++;
    bb_frame_t decoded = {0};
    uint8_t wire[8] = {0};
    bb_err_t err = bb_frame_decode(wire, 8U, &decoded);
    TEST_ASSERT(err == k_bb_err_invalid_arg, "expected invalid_arg for truncated");
}

static void test_frame_null_ptr_guards(void)
{
    s_tests_run++;
    bb_frame_t frame = {0};
    uint8_t wire[64] = {0};
    uint32_t len = 0U;

    TEST_ASSERT(bb_frame_encode(NULL, wire, 64, &len) == k_bb_err_null_ptr,
                "encode null frame");
    TEST_ASSERT(bb_frame_encode(&frame, NULL, 64, &len) == k_bb_err_null_ptr,
                "encode null buf");
    TEST_ASSERT(bb_frame_encode(&frame, wire, 64, NULL) == k_bb_err_null_ptr,
                "encode null len");
    TEST_ASSERT(bb_frame_decode(NULL, 12, &frame) == k_bb_err_null_ptr,
                "decode null wire");
    TEST_ASSERT(bb_frame_decode(wire, 12, NULL) == k_bb_err_null_ptr,
                "decode null frame");
    TEST_ASSERT(bb_frame_build_ack(0, NULL) == k_bb_err_null_ptr,
                "build_ack null");
    TEST_ASSERT(bb_frame_build_nack(0, NULL) == k_bb_err_null_ptr,
                "build_nack null");
}

static void test_frame_endianness(void)
{
    s_tests_run++;
    bb_frame_t frame = {0};
    (void)bb_frame_build_ack(0U, &frame);

    uint8_t wire[64] = {0};
    uint32_t wire_len = 0U;
    (void)bb_frame_encode(&frame, wire, sizeof(wire), &wire_len);

    /* SYNC word 0x55AA should appear as bytes 0xAA, 0x55 (little-endian) */
    TEST_ASSERT(wire[0] == 0xAAU, "SYNC byte 0 should be 0xAA");
    TEST_ASSERT(wire[1] == 0x55U, "SYNC byte 1 should be 0x55");
}

static void test_frame_ack_nack_helpers(void)
{
    s_tests_run++;
    bb_frame_t ack = {0};
    (void)bb_frame_build_ack(100U, &ack);
    TEST_ASSERT(ack.header.sequence == 100U, "ack seq");
    TEST_ASSERT(ack.header.type == k_bb_frame_type_ack, "ack type");
    TEST_ASSERT(ack.header.length == 0U, "ack len");

    bb_frame_t nack = {0};
    (void)bb_frame_build_nack(200U, &nack);
    TEST_ASSERT(nack.header.sequence == 200U, "nack seq");
    TEST_ASSERT(nack.header.type == k_bb_frame_type_nack, "nack type");
    TEST_ASSERT(nack.header.length == 0U, "nack len");
}

static void test_frame_max_payload(void)
{
    s_tests_run++;
    bb_frame_t frame = {0};
    frame.header.sequence = 1U;
    frame.header.length   = 1024U;
    frame.header.type     = k_bb_frame_type_response;

    /* Fill payload with pattern */
    for (uint32_t i = 0U; i < 1024U; i++) {
        frame.payload[i] = (uint8_t)(i & 0xFFU);
    }

    uint8_t wire[1036] = {0};
    uint32_t wire_len = 0U;
    bb_err_t err = bb_frame_encode(&frame, wire, sizeof(wire), &wire_len);
    TEST_ASSERT(err == k_bb_err_ok, "max payload encode");
    TEST_ASSERT(wire_len == 1036U, "max payload wire len");

    bb_frame_t decoded = {0};
    err = bb_frame_decode(wire, wire_len, &decoded);
    TEST_ASSERT(err == k_bb_err_ok, "max payload decode");
    TEST_ASSERT(decoded.header.length == 1024U, "max payload len");
    TEST_ASSERT(memcmp(decoded.payload, frame.payload, 1024U) == 0,
                "max payload data");
}

static void test_frame_encode_payload_too_large(void)
{
    s_tests_run++;
    bb_frame_t frame = {0};
    frame.header.sequence = 1U;
    frame.header.length   = 1025U; /* exceeds k_bb_frame_max_payload (1024) */
    frame.header.type     = k_bb_frame_type_command;

    uint8_t wire[2048] = {0};
    uint32_t wire_len = 0U;
    bb_err_t err = bb_frame_encode(&frame, wire, sizeof(wire), &wire_len);
    TEST_ASSERT(err == k_bb_err_invalid_arg,
                "encode payload>1024 should return invalid_arg");
}

static void test_frame_encode_buffer_too_small(void)
{
    s_tests_run++;
    bb_frame_t frame = {0};
    frame.header.sequence = 1U;
    frame.header.length   = 5U;
    frame.header.type     = k_bb_frame_type_command;

    /* Need 12 + 5 = 17 bytes, provide only 10 */
    uint8_t wire[10] = {0};
    uint32_t wire_len = 0U;
    bb_err_t err = bb_frame_encode(&frame, wire, sizeof(wire), &wire_len);
    TEST_ASSERT(err == k_bb_err_invalid_arg,
                "encode into too-small buffer should return invalid_arg");
}

static void test_frame_decode_wrong_sync(void)
{
    s_tests_run++;
    /* Build a valid frame, then overwrite SYNC with garbage */
    bb_frame_t frame = {0};
    (void)bb_frame_build_ack(1U, &frame);

    uint8_t wire[64] = {0};
    uint32_t wire_len = 0U;
    (void)bb_frame_encode(&frame, wire, sizeof(wire), &wire_len);

    /* Corrupt SYNC word */
    wire[0] = 0x00;
    wire[1] = 0x00;

    bb_frame_t decoded = {0};
    bb_err_t err = bb_frame_decode(wire, wire_len, &decoded);
    TEST_ASSERT(err == k_bb_err_invalid_arg,
                "decode with wrong SYNC should return invalid_arg");
}

/* ---------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------------*/

int main(void)
{
    test_frame_encode_decode_round_trip();
    test_frame_encode_empty_payload();
    test_frame_decode_crc_mismatch();
    test_frame_decode_truncated();
    test_frame_null_ptr_guards();
    test_frame_endianness();
    test_frame_ack_nack_helpers();
    test_frame_max_payload();
    test_frame_encode_payload_too_large();
    test_frame_encode_buffer_too_small();
    test_frame_decode_wrong_sync();

    if (s_failures == 0U) {
        (void)fprintf(stdout, "test_frame: ALL %u TESTS PASSED\n",
                      (unsigned)s_tests_run);
    } else {
        (void)fprintf(stderr, "test_frame: %u FAILURES\n", (unsigned)s_failures);
    }

    return (int)s_failures;
}
