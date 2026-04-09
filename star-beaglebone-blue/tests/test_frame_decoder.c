/**
 * @file test_frame_decoder.c
 * @brief Unit tests for bb_frame streaming decoder.
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

/**
 * @brief Encode a frame to wire bytes for feeding to the decoder.
 */
static uint32_t helper_encode(const bb_frame_t* frame, uint8_t* wire,
                              uint32_t wire_size)
{
    uint32_t len = 0U;
    (void)bb_frame_encode(frame, wire, wire_size, &len);
    return len;
}

/* ---------------------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------------------*/

static void test_decoder_byte_at_a_time(void)
{
    s_tests_run++;
    bb_frame_t original = {0};
    original.header.sequence = 10U;
    original.header.length   = 4U;
    original.header.type     = k_bb_frame_type_command;
    original.payload[0] = 0xDE;
    original.payload[1] = 0xAD;
    original.payload[2] = 0xBE;
    original.payload[3] = 0xEF;

    uint8_t wire[64] = {0};
    uint32_t wire_len = helper_encode(&original, wire, sizeof(wire));

    bb_frame_decoder_t dec;
    (void)bb_frame_decoder_init(&dec);

    bool got_frame = false;
    for (uint32_t i = 0U; i < wire_len; i++) {
        bool ready = false;
        (void)bb_frame_decoder_feed(&dec, wire[i], &ready);
        if (ready) {
            got_frame = true;
            TEST_ASSERT(i == wire_len - 1U, "frame_ready not on last byte");
        }
    }
    TEST_ASSERT(got_frame, "decoder never signaled frame_ready");

    bb_frame_t decoded = {0};
    (void)bb_frame_decoder_get_frame(&dec, &decoded);
    TEST_ASSERT(decoded.header.sequence == 10U, "decoded seq");
    TEST_ASSERT(decoded.header.length == 4U, "decoded len");
    TEST_ASSERT(decoded.payload[0] == 0xDE, "decoded payload[0]");
    TEST_ASSERT(decoded.payload[3] == 0xEF, "decoded payload[3]");
}

static void test_decoder_resync_after_garbage(void)
{
    s_tests_run++;
    bb_frame_t frame = {0};
    (void)bb_frame_build_ack(99U, &frame);

    uint8_t wire[64] = {0};
    uint32_t wire_len = helper_encode(&frame, wire, sizeof(wire));

    bb_frame_decoder_t dec;
    (void)bb_frame_decoder_init(&dec);

    /* Feed 20 garbage bytes first */
    for (uint32_t i = 0U; i < 20U; i++) {
        bool ready = false;
        (void)bb_frame_decoder_feed(&dec, (uint8_t)(0x37 + i), &ready);
        TEST_ASSERT(!ready, "garbage triggered frame_ready");
    }

    /* Now feed the valid frame */
    bool got_frame = false;
    for (uint32_t i = 0U; i < wire_len; i++) {
        bool ready = false;
        (void)bb_frame_decoder_feed(&dec, wire[i], &ready);
        if (ready) {
            got_frame = true;
        }
    }
    TEST_ASSERT(got_frame, "resync: decoder never found frame");

    bb_frame_t decoded = {0};
    (void)bb_frame_decoder_get_frame(&dec, &decoded);
    TEST_ASSERT(decoded.header.sequence == 99U, "resync: wrong seq");
}

static void test_decoder_resync_on_crc_failure(void)
{
    s_tests_run++;
    /* Build two frames: first with bad CRC, second valid */
    bb_frame_t frame1 = {0};
    frame1.header.sequence = 1U;
    frame1.header.length   = 2U;
    frame1.header.type     = k_bb_frame_type_command;
    frame1.payload[0]      = 0x11;
    frame1.payload[1]      = 0x22;

    uint8_t wire1[64] = {0};
    uint32_t wire1_len = helper_encode(&frame1, wire1, sizeof(wire1));

    /* Corrupt frame1 payload */
    wire1[8] = 0xFF;

    bb_frame_t frame2 = {0};
    (void)bb_frame_build_ack(2U, &frame2);

    uint8_t wire2[64] = {0};
    uint32_t wire2_len = helper_encode(&frame2, wire2, sizeof(wire2));

    bb_frame_decoder_t dec;
    (void)bb_frame_decoder_init(&dec);

    /* Feed corrupted frame -- should not trigger */
    for (uint32_t i = 0U; i < wire1_len; i++) {
        bool ready = false;
        (void)bb_frame_decoder_feed(&dec, wire1[i], &ready);
        TEST_ASSERT(!ready, "corrupted frame triggered ready");
    }

    /* Feed valid frame -- should succeed */
    bool got_frame = false;
    for (uint32_t i = 0U; i < wire2_len; i++) {
        bool ready = false;
        (void)bb_frame_decoder_feed(&dec, wire2[i], &ready);
        if (ready) {
            got_frame = true;
        }
    }
    TEST_ASSERT(got_frame, "crc resync: decoder never found frame2");

    bb_frame_t decoded = {0};
    (void)bb_frame_decoder_get_frame(&dec, &decoded);
    TEST_ASSERT(decoded.header.sequence == 2U, "crc resync: wrong seq");
}

static void test_decoder_reset(void)
{
    s_tests_run++;
    bb_frame_t frame = {0};
    (void)bb_frame_build_ack(5U, &frame);

    uint8_t wire[64] = {0};
    uint32_t wire_len = helper_encode(&frame, wire, sizeof(wire));

    bb_frame_decoder_t dec;
    (void)bb_frame_decoder_init(&dec);

    /* Feed partial frame (first 6 bytes) */
    for (uint32_t i = 0U; i < 6U; i++) {
        bool ready = false;
        (void)bb_frame_decoder_feed(&dec, wire[i], &ready);
    }

    /* Reset */
    (void)bb_frame_decoder_reset(&dec);

    /* Feed complete frame from scratch */
    bool got_frame = false;
    for (uint32_t i = 0U; i < wire_len; i++) {
        bool ready = false;
        (void)bb_frame_decoder_feed(&dec, wire[i], &ready);
        if (ready) {
            got_frame = true;
        }
    }
    TEST_ASSERT(got_frame, "reset: decoder failed after reset");
}

static void test_decoder_back_to_back_frames(void)
{
    s_tests_run++;
    bb_frame_t f1 = {0};
    (void)bb_frame_build_ack(10U, &f1);

    bb_frame_t f2 = {0};
    (void)bb_frame_build_nack(20U, &f2);

    uint8_t wire1[64] = {0};
    uint32_t w1_len = helper_encode(&f1, wire1, sizeof(wire1));

    uint8_t wire2[64] = {0};
    uint32_t w2_len = helper_encode(&f2, wire2, sizeof(wire2));

    bb_frame_decoder_t dec;
    (void)bb_frame_decoder_init(&dec);

    /* Feed frame 1 */
    uint32_t frame_count = 0U;
    for (uint32_t i = 0U; i < w1_len; i++) {
        bool ready = false;
        (void)bb_frame_decoder_feed(&dec, wire1[i], &ready);
        if (ready) {
            frame_count++;
            bb_frame_t decoded = {0};
            (void)bb_frame_decoder_get_frame(&dec, &decoded);
            TEST_ASSERT(decoded.header.sequence == 10U, "b2b: frame1 seq");
            (void)bb_frame_decoder_reset(&dec);
        }
    }

    /* Feed frame 2 */
    for (uint32_t i = 0U; i < w2_len; i++) {
        bool ready = false;
        (void)bb_frame_decoder_feed(&dec, wire2[i], &ready);
        if (ready) {
            frame_count++;
            bb_frame_t decoded = {0};
            (void)bb_frame_decoder_get_frame(&dec, &decoded);
            TEST_ASSERT(decoded.header.sequence == 20U, "b2b: frame2 seq");
            (void)bb_frame_decoder_reset(&dec);
        }
    }

    TEST_ASSERT(frame_count == 2U, "b2b: expected 2 frames");
}

static void test_decoder_null_ptr_guards(void)
{
    s_tests_run++;
    bb_frame_decoder_t dec;
    bool ready = false;
    bb_frame_t frame = {0};

    TEST_ASSERT(bb_frame_decoder_init(NULL) == k_bb_err_null_ptr,
                "init null");
    TEST_ASSERT(bb_frame_decoder_feed(NULL, 0, &ready) == k_bb_err_null_ptr,
                "feed null dec");
    (void)bb_frame_decoder_init(&dec);
    TEST_ASSERT(bb_frame_decoder_feed(&dec, 0, NULL) == k_bb_err_null_ptr,
                "feed null ready");
    TEST_ASSERT(bb_frame_decoder_get_frame(NULL, &frame) == k_bb_err_null_ptr,
                "get_frame null dec");
    TEST_ASSERT(bb_frame_decoder_get_frame(&dec, NULL) == k_bb_err_null_ptr,
                "get_frame null frame");
    TEST_ASSERT(bb_frame_decoder_reset(NULL) == k_bb_err_null_ptr,
                "reset null");
}

static void test_decoder_false_sync_pattern(void)
{
    s_tests_run++;
    /* Feed 0xAA 0xAA 0x55 -- first 0xAA is noise, second is real sync lo */
    bb_frame_t frame = {0};
    (void)bb_frame_build_ack(77U, &frame);

    uint8_t wire[64] = {0};
    uint32_t wire_len = helper_encode(&frame, wire, sizeof(wire));

    bb_frame_decoder_t dec;
    (void)bb_frame_decoder_init(&dec);

    /* Feed a false 0xAA before the real frame (which starts with 0xAA 0x55) */
    bool ready = false;
    (void)bb_frame_decoder_feed(&dec, 0xAAU, &ready);
    TEST_ASSERT(!ready, "false sync: premature ready on first 0xAA");

    /* Now feed another 0xAA -- the decoder saw 0xAA then 0xAA, the second
     * 0xAA should be treated as a potential new sync_lo */
    (void)bb_frame_decoder_feed(&dec, 0xAAU, &ready);
    TEST_ASSERT(!ready, "false sync: premature ready on second 0xAA");

    /* Feed 0x55 to complete the real sync */
    (void)bb_frame_decoder_feed(&dec, 0x55U, &ready);
    TEST_ASSERT(!ready, "false sync: premature ready on 0x55");

    /* Now feed the rest of the valid frame (skip first 2 bytes = SYNC) */
    bool got_frame = false;
    for (uint32_t i = 2U; i < wire_len; i++) {
        (void)bb_frame_decoder_feed(&dec, wire[i], &ready);
        if (ready) {
            got_frame = true;
        }
    }
    TEST_ASSERT(got_frame, "false sync: decoder never found frame");

    bb_frame_t decoded = {0};
    (void)bb_frame_decoder_get_frame(&dec, &decoded);
    TEST_ASSERT(decoded.header.sequence == 77U, "false sync: wrong seq");
}

static void test_decoder_not_initialized(void)
{
    s_tests_run++;
    bb_frame_decoder_t dec;
    (void)memset(&dec, 0, sizeof(dec));
    /* dec.initialized is false since we zeroed it */

    bool ready = false;
    bb_err_t err = bb_frame_decoder_feed(&dec, 0xAA, &ready);
    TEST_ASSERT(err == k_bb_err_not_initialized,
                "feed on uninitialized should return not_initialized");
}

static void test_decoder_get_frame_not_ready(void)
{
    s_tests_run++;
    bb_frame_decoder_t dec;
    (void)bb_frame_decoder_init(&dec);

    /* No frame has been fed -- state is hunt_sync, not complete */
    bb_frame_t frame = {0};
    bb_err_t err = bb_frame_decoder_get_frame(&dec, &frame);
    TEST_ASSERT(err == k_bb_err_invalid_arg,
                "get_frame with no frame ready should return invalid_arg");
}

/* ---------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------------*/

int main(void)
{
    test_decoder_byte_at_a_time();
    test_decoder_resync_after_garbage();
    test_decoder_resync_on_crc_failure();
    test_decoder_reset();
    test_decoder_back_to_back_frames();
    test_decoder_null_ptr_guards();
    test_decoder_false_sync_pattern();
    test_decoder_not_initialized();
    test_decoder_get_frame_not_ready();

    if (s_failures == 0U) {
        (void)fprintf(stdout, "test_frame_decoder: ALL %u TESTS PASSED\n",
                      (unsigned)s_tests_run);
    } else {
        (void)fprintf(stderr, "test_frame_decoder: %u FAILURES\n",
                      (unsigned)s_failures);
    }

    return (int)s_failures;
}
