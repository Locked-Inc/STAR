/**
 * @file test_rx_crc32.c
 * @brief Unit Tests for IEEE 802.3 CRC-32 Implementation
 *
 * Tests the CRC-32 implementation for bit-exact compatibility with
 * Go's crc32.ChecksumIEEE().
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "unity.h"
#include "rx_frame.h"
#include <string.h>

void setUp(void) {
    /* Nothing to set up */
}

void tearDown(void) {
    /* Nothing to tear down */
}

/* =============================================================================
 * Basic CRC-32 Tests
 * =============================================================================
 */

/**
 * @brief Test CRC-32 with empty data returns 0
 */
void test_crc32_empty_data(void) {
    uint32_t crc = rx_crc32_ieee(NULL, 0);
    TEST_ASSERT_EQUAL_HEX32(0x00000000, crc);
}

/**
 * @brief Test CRC-32 with NULL pointer returns 0
 */
void test_crc32_null_pointer(void) {
    uint32_t crc = rx_crc32_ieee(NULL, 10);
    TEST_ASSERT_EQUAL_HEX32(0x00000000, crc);
}

/**
 * @brief Test CRC-32 with zero length returns 0
 */
void test_crc32_zero_length(void) {
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint32_t crc = rx_crc32_ieee(data, 0);
    TEST_ASSERT_EQUAL_HEX32(0x00000000, crc);
}

/* =============================================================================
 * Known Test Vectors
 * CRC values computed with Go: crc32.ChecksumIEEE([]byte{...})
 * =============================================================================
 */

/**
 * @brief Test CRC-32 of "123456789" (standard test vector)
 *
 * This is the canonical IEEE 802.3 CRC-32 test vector.
 * Expected: 0xCBF43926
 */
void test_crc32_standard_vector(void) {
    const uint8_t data[] = "123456789";
    uint32_t crc = rx_crc32_ieee(data, 9); /* "123456789" without null */
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc);
}

/**
 * @brief Test CRC-32 of single byte 0x00
 *
 * Go: crc32.ChecksumIEEE([]byte{0x00}) = 0xD202EF8D
 */
void test_crc32_single_zero(void) {
    uint8_t data[] = {0x00};
    uint32_t crc = rx_crc32_ieee(data, 1);
    TEST_ASSERT_EQUAL_HEX32(0xD202EF8D, crc);
}

/**
 * @brief Test CRC-32 of single byte 0xFF
 *
 * Go: crc32.ChecksumIEEE([]byte{0xFF}) = 0xFF000000
 */
void test_crc32_single_ff(void) {
    uint8_t data[] = {0xFF};
    uint32_t crc = rx_crc32_ieee(data, 1);
    TEST_ASSERT_EQUAL_HEX32(0xFF000000, crc);
}

/**
 * @brief Test CRC-32 of frame sync word 0x55AA (big-endian)
 *
 * Verified against IEEE 802.3 CRC-32 implementation
 */
void test_crc32_sync_word(void) {
    uint8_t data[] = {0x55, 0xAA};
    uint32_t crc = rx_crc32_ieee(data, 2);
    TEST_ASSERT_EQUAL_HEX32(0xB016F118, crc);
}

/**
 * @brief Test CRC-32 of all zeros (8 bytes)
 *
 * Go: crc32.ChecksumIEEE([]byte{0,0,0,0,0,0,0,0}) = 0x6522DF69
 */
void test_crc32_eight_zeros(void) {
    uint8_t data[8] = {0};
    uint32_t crc = rx_crc32_ieee(data, 8);
    TEST_ASSERT_EQUAL_HEX32(0x6522DF69, crc);
}

/**
 * @brief Test CRC-32 of all 0xFF (8 bytes)
 *
 * Verified against IEEE 802.3 CRC-32 implementation
 */
void test_crc32_eight_ff(void) {
    uint8_t data[8];
    memset(data, 0xFF, 8);
    uint32_t crc = rx_crc32_ieee(data, 8);
    TEST_ASSERT_EQUAL_HEX32(0x2144DF1C, crc);
}

/**
 * @brief Test CRC-32 of typical frame header (SYNC + SEQ + LEN + TYPE + FLAGS)
 *
 * Frame header bytes: 0x55 0xAA 0x00 0x01 0x00 0x08 0x01 0x00
 * Verified against IEEE 802.3 CRC-32 implementation
 */
void test_crc32_frame_header(void) {
    uint8_t header[] = {0x55, 0xAA, 0x00, 0x01, 0x00, 0x08, 0x01, 0x00};
    uint32_t crc = rx_crc32_ieee(header, 8);
    TEST_ASSERT_EQUAL_HEX32(0xB157E7A2, crc);
}

/* =============================================================================
 * Incremental CRC-32 Tests
 * =============================================================================
 */

/**
 * @brief Test incremental CRC matches single-shot CRC
 */
void test_crc32_incremental_matches_single(void) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    /* Single-shot CRC */
    uint32_t crc_single = rx_crc32_ieee(data, 8);

    /* Incremental CRC (2 + 3 + 3 bytes) */
    uint32_t crc_incr = rx_crc32_ieee(data, 2);
    crc_incr = rx_crc32_update(crc_incr, data + 2, 3);
    crc_incr = rx_crc32_update(crc_incr, data + 5, 3);

    TEST_ASSERT_EQUAL_HEX32(crc_single, crc_incr);
}

/**
 * @brief Test incremental CRC with single bytes
 */
void test_crc32_incremental_single_bytes(void) {
    const uint8_t data[] = "123456789";

    /* Single-shot CRC */
    uint32_t crc_single = rx_crc32_ieee(data, 9);

    /* Incremental CRC byte by byte */
    uint32_t crc_incr = rx_crc32_ieee(&data[0], 1);
    for (size_t i = 1; i < 9; i++) {
        crc_incr = rx_crc32_update(crc_incr, &data[i], 1);
    }

    TEST_ASSERT_EQUAL_HEX32(crc_single, crc_incr);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void) {
    UNITY_BEGIN();

    /* Basic tests */
    RUN_TEST(test_crc32_empty_data);
    RUN_TEST(test_crc32_null_pointer);
    RUN_TEST(test_crc32_zero_length);

    /* Known test vectors */
    RUN_TEST(test_crc32_standard_vector);
    RUN_TEST(test_crc32_single_zero);
    RUN_TEST(test_crc32_single_ff);
    RUN_TEST(test_crc32_sync_word);
    RUN_TEST(test_crc32_eight_zeros);
    RUN_TEST(test_crc32_eight_ff);
    RUN_TEST(test_crc32_frame_header);

    /* Incremental CRC tests */
    RUN_TEST(test_crc32_incremental_matches_single);
    RUN_TEST(test_crc32_incremental_single_bytes);

    return UNITY_END();
}
